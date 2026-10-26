// Unit U14b of KernelStandard_7c: PStringCache (0x10039830..0x1003a92e, the chain helpers 0x10039a70 / 0x1003a940 /
// 0x1003a9e0, the node makers 0x10039730 / 0x100396f0 / 0x10014170) and PString (0x10039130..0x100396ee).
//
// The cache is a hash table of interned nodes (KernelStandardInit makes it with 997 buckets). One static lock
// (0x100acb58, initialised by the cache's constructor) guards it; other modules take it inline through Lock / Unlock
// to count references. A lookup hashes the last 16 bytes at most (h = h * 8 ^ byte), moves a found node to the front
// of its chain and counts hits and probes. Edits (Ins, Del, ToLower...) take the node out of the cache when it has one
// reference and room (GetUnique), else copy it, and put the result back (Insert), merged with an equal node.
// Node memory: (length + 0x29) & ~0xf bytes from the "#StringData" heap (Alloc above 0xa0).
#include <jet/KernelStandard.h>
#include <jet/PString.h>

#include "Crt80.h"
#include "StringFormat.h"
#include "StringHeap.h"

#include <windows.h>

#include <cstring>

namespace Jet {

PStringCache* PString::cache;  // 0x100adc14
bool PString::isFinished;      // 0x100adbf5

namespace {

CRITICAL_SECTION cacheLock;  // 0x100acb58

using Node = PStringCache::Node;
using Bucket = PStringCache::Bucket;

}  // namespace

struct CacheWork {
    // 0x1003a9e0
    static unsigned int Hash(const unsigned char* text, unsigned int length)
    {
        unsigned int n = length > 0x10 ? 0x10 : length;
        const unsigned char* p = text + length - n;
        unsigned int h = 0;
        for (; n; --n) h = h * 8 ^ *p++;
        return h;
    }
    static Bucket& BucketOf(PStringCache& c, const unsigned char* text, unsigned int length)
    {
        return c.buckets[Hash(text, length) % static_cast<unsigned int>(c.bucketCount)];
    }

    // 0x1003a940
    static void Unlink(Bucket& b, Node* n)
    {
        if (n->prev)
            n->prev->next = n->next;
        else
            b.head = n->next;
        if (n->next)
            n->next->prev = n->prev;
        else
            b.tail = n->prev;
        n->next = nullptr;
        n->prev = nullptr;
        --b.count;
    }
    static void PushFront(Bucket& b, Node* n)
    {
        if (b.head) {
            n->next = b.head;
            b.head->prev = n;
            ++b.count;
            b.head = n;
        } else {
            ++b.count;
            b.tail = n;
            b.head = n;
        }
    }

    // 0x10039a70: the node of this text, moved to the front of its chain; *bucket is the chain either way
    static Node* Lookup(PStringCache& c, const unsigned char* text, unsigned int length, Bucket** bucket)
    {
        Bucket& b = BucketOf(c, text, length);
        *bucket = &b;
        Node* n = b.head;
        for (; n; n = n->next) {
            if (n->length == length && std::strncmp(n->text, reinterpret_cast<const char*>(text), length) == 0) break;
            ++c.probes;
        }
        if (!n) return nullptr;
        if (n->prev) {
            Unlink(b, n);
            PushFront(b, n);
        }
        ++c.hits;
        return n;
    }

    static unsigned int NodeBytes(unsigned int length) { return (length + 0x29) & ~0xfu; }

    static Node* NewNode(unsigned int bytes)
    {
        Node* n = static_cast<Node*>(StringHeap::New(bytes));
        if (n) {
            n->prev = nullptr;
            n->next = nullptr;
            n->id = 0;
        }
        return n;
    }

    // 0x10039730: a node with room for `length`, one reference, empty
    static Node* Empty(unsigned int length)
    {
        const unsigned int bytes = NodeBytes(length);
        Node* n = NewNode(bytes);
        n->length = 0;
        n->capacity = bytes - 0x18;
        n->refs = 1;
        n->text[length] = 0;
        return n;
    }

    static void Free(Node* n)
    {
        if (((n->capacity + 0x1f) & ~7u) < 0xa1)
            StringHeap::Heap()->Free(reinterpret_cast<unsigned char*>(n));
        else
            Alloc::Delete(n);
    }

    // the release every member inlines
    static void Release(PStringCache& c, Node* n)
    {
        EnterCriticalSection(&cacheLock);
        if (n->refs == 1) --c.usedNodes;
        --c.liveRefs;
        --n->refs;
        LeaveCriticalSection(&cacheLock);
    }

    static unsigned int Length(const char* s) { return static_cast<unsigned int>(std::strlen(s)); }
};

// ---- construction

PStringCache::PStringCache(unsigned int n)
{
    InitializeCriticalSection(&cacheLock);
    bucketCount = static_cast<int>(n);
    const unsigned long long bytes = static_cast<unsigned long long>(n) * sizeof(Bucket);
    const unsigned int total = bytes > 0xffffffffull || bytes > 0xfffffffbull ? 0xffffffffu : static_cast<unsigned int>(bytes) + 4;
    unsigned int* block = static_cast<unsigned int*>(Jet::Alloc::New(total));
    Bucket* b = nullptr;
    if (block) {
        block[0] = n;
        b = reinterpret_cast<Bucket*>(block + 1);
        for (unsigned int i = 0; i < n; ++i) b[i] = Bucket{nullptr, nullptr, 0};
    }
    buckets = b;
    unused14 = 0;
    unused18 = 0;
    probes = 0;
    liveRefs = 0;
    usedNodes = 0;
    hits = 0;
    nodes = 0;
    gcBucket = 0;
    walkBucket = 0;
    walkNode = nullptr;
}

PStringCache::~PStringCache()
{
    for (int i = 0; i < bucketCount; ++i) {
        Bucket& b = buckets[i];
        while (Node* n = b.head) {
            CacheWork::Unlink(b, n);
            CacheWork::Free(n);
        }
    }
    if (buckets) Jet::Alloc::Delete(reinterpret_cast<unsigned int*>(buckets) - 1);
    DeleteCriticalSection(&cacheLock);
}

void __cdecl PStringCache::Lock() { EnterCriticalSection(&cacheLock); }
void __cdecl PStringCache::Unlock() { LeaveCriticalSection(&cacheLock); }

// ---- nodes

Node* __cdecl PStringCache::Alloc(unsigned int length) { return CacheWork::NewNode(length + 0x18); }

// a node of `length` the caller may write: this one out of the cache when it is the only reference and has room
// (the caller holds the lock), else a fresh copy (one reference of `node` dropped)
Node* __cdecl PStringCache::GetUnique(Node* node, unsigned int length)
{
    const unsigned int bytes = CacheWork::NodeBytes(length);
    const unsigned int cap = bytes - 0x18;
    if (!node) {
        Node* n = CacheWork::NewNode(bytes);
        n->length = 0;
        n->capacity = cap;
        n->refs = 1;
        n->text[length] = 0;
        return n;
    }
    if (static_cast<unsigned int>(node->refs) < 2 && cap <= node->capacity) {
        CacheWork::Unlink(CacheWork::BucketOf(*this, reinterpret_cast<const unsigned char*>(node->text), node->length),
                          node);
        --liveRefs;
        --usedNodes;
        return node;
    }
    Node* n = CacheWork::NewNode(bytes);
    n->length = node->length;
    n->capacity = cap;
    n->refs = 1;
    std::memcpy(n->text, node->text, node->length + 1);
    CacheWork::Release(*this, node);
    return n;
}

Node* __cdecl PStringCache::Insert(Node* node)
{
    ++liveRefs;
    Bucket* b;
    Node* found = CacheWork::Lookup(*this, reinterpret_cast<const unsigned char*>(node->text), node->length, &b);
    if (!found) {
        ++nodes;
        ++usedNodes;
        CacheWork::PushFront(*b, node);
        return node;
    }
    if (found->refs == 0) ++usedNodes;
    ++found->refs;
    CacheWork::Free(node);
    return found;
}

// 0x10039cc0 (the middle argument is not used)
Node* __cdecl PStringCache::Create(const unsigned char* text, unsigned int, unsigned int length)
{
    ++liveRefs;
    EnterCriticalSection(&cacheLock);
    Bucket* b;
    if (Node* found = CacheWork::Lookup(*this, text, length, &b)) {
        if (found->refs == 0) ++usedNodes;
        ++found->refs;
        LeaveCriticalSection(&cacheLock);
        return found;
    }
    const unsigned int bytes = CacheWork::NodeBytes(length);
    Node* n = CacheWork::NewNode(bytes);
    n->length = length;
    n->capacity = bytes - 0x18;
    n->refs = 1;
    std::memcpy(n->text, text, length);
    n->text[length] = 0;
    ++nodes;
    ++usedNodes;
    CacheWork::PushFront(*b, n);
    LeaveCriticalSection(&cacheLock);
    return n;
}

Node* __cdecl PStringCache::Create(const unsigned char* text, unsigned int length)
{
    if (!text && length == 0) return nullptr;
    return Create(text, length, length);
}

Node* __cdecl PStringCache::Create(const unsigned char* text)
{
    if (!text) return nullptr;
    const unsigned int n = CacheWork::Length(reinterpret_cast<const char*>(text));
    return Create(text, n, n);
}

// length < 0: up to the NUL; else at most `length` (the text is measured either way)
Node* __cdecl PStringCache::CreateANSI(const unsigned char* text, int length)
{
    if (length < 0) {
        if (!text) return nullptr;
        const unsigned int n = CacheWork::Length(reinterpret_cast<const char*>(text));
        return Create(text, n, n);
    }
    const int n = static_cast<int>(CacheWork::Length(reinterpret_cast<const char*>(text)));
    if (n < length) length = n;
    if (!text && length == 0) return nullptr;
    return Create(text, static_cast<unsigned int>(length), static_cast<unsigned int>(length));
}

// msvcr80 wcstombs; the node gets the bytes it wrote
Node* __cdecl PStringCache::CreateUnicode(const __wchar_t* text, int length)
{
    if (!text) return nullptr;
    if (length < 0) {
        const __wchar_t* p = text;
        while (*p) ++p;
        length = static_cast<int>(p - text);
    }
    char local[0x1004];
    const unsigned int bytes = static_cast<unsigned int>(length) * 2;
    char* out = bytes > 0xffe ? static_cast<char*>(Jet::Alloc::New(bytes + 2)) : local;
    Node* n = nullptr;
    const int written = static_cast<int>(Crt80::wcstombs(out, reinterpret_cast<const wchar_t*>(text), bytes));
    if (written >= 0)
        n = Create(reinterpret_cast<const unsigned char*>(out), static_cast<unsigned int>(length),
                   static_cast<unsigned int>(written));
    if (out != local && out) Jet::Alloc::Delete(out);
    return n;
}

// `count` copies of ch
Node* __cdecl PStringCache::Create(unsigned int ch, unsigned int count)
{
    if (count == 0) return nullptr;
    EnterCriticalSection(&cacheLock);
    Node* n = CacheWork::Empty(count);
    std::memset(n->text, static_cast<int>(ch), count);
    n->length = count;
    n->text[count] = 0;
    Node* r = Insert(n);
    LeaveCriticalSection(&cacheLock);
    return r;
}

void __cdecl PStringCache::Destroy(Node* node) { CacheWork::Release(*this, node); }

// ---- reading

int __cdecl PStringCache::GetANSI(Node* node, char* out, unsigned int size) const
{
    if (size < 2 || !node) {
        out[0] = 0;
        return 0;
    }
    if (node->length <= size - 1) {
        std::strcpy(out, node->text);
        return static_cast<int>(node->length);
    }
    const int n = static_cast<int>(size - 1);
    if (n > 0) {
        std::memcpy(out, node->text, static_cast<size_t>(n));
        out[n] = 0;
        return n;
    }
    out[0] = 0;
    return 0;
}

int __cdecl PStringCache::GetUnicode(Node* node, __wchar_t* out, unsigned int size) const
{
    if (size < 2 || !node) {
        out[0] = 0;
        return 0;
    }
    size_t n = node->length;
    if (static_cast<int>(size - 1) < static_cast<int>(n)) n = size - 1;
    const size_t r = Crt80::mbstowcs(reinterpret_cast<wchar_t*>(out), node->text, n);
    if (static_cast<int>(r) < 0) return -1;
    out[r] = 0;
    return static_cast<int>(r);
}

unsigned int __cdecl PStringCache::GetHash(const unsigned char* text, unsigned int length) const
{
    return CacheWork::Hash(text, length);
}

int __cdecl PStringCache::Find(Node* node, unsigned int ch, unsigned int start) const
{
    if (node->length != 0) {
        if (const char* p = std::strchr(node->text + start, static_cast<int>(ch))) return static_cast<int>(p - node->text);
    }
    return -1;
}

int __cdecl PStringCache::Find(Node* node, const unsigned char* text, unsigned int start) const
{
    if (node->length != 0) {
        if (const char* p = std::strstr(node->text + start, reinterpret_cast<const char*>(text)))
            return static_cast<int>(p - node->text);
    }
    return -1;
}

// ---- walking

Node* __cdecl PStringCache::Next()
{
    do {
        if (!walkNode) {
            if (++walkBucket == bucketCount) return nullptr;
            walkNode = buckets[walkBucket].head;
        } else {
            walkNode = walkNode->next;
        }
    } while (!walkNode);
    return walkNode;
}

void __cdecl PStringCache::ClearIDs()
{
    EnterCriticalSection(&cacheLock);
    walkBucket = -1;
    walkNode = nullptr;
    for (Node* n = Next(); n; n = Next()) n->id = 0;
    LeaveCriticalSection(&cacheLock);
}

// one bucket a call, round robin, swept from the tail
void __cdecl PStringCache::CollectGarbage()
{
    EnterCriticalSection(&cacheLock);
    gcBucket = gcBucket < bucketCount - 1 ? gcBucket + 1 : 0;
    Bucket& b = buckets[gcBucket];
    for (Node* n = b.tail; n;) {
        Node* const prev = n->prev;
        if (n->refs == 0) {
            CacheWork::Unlink(b, n);
            CacheWork::Free(n);
        }
        n = prev;
    }
    LeaveCriticalSection(&cacheLock);
}

void __cdecl PStringCache::Dump()  // walks every chain and prints nothing
{
    EnterCriticalSection(&cacheLock);
    LeaveCriticalSection(&cacheLock);
}

// ---- edits

Node* __cdecl PStringCache::Ins(Node* node, unsigned int ch, unsigned int pos, unsigned int count)
{
    if (!node) return Create(ch, count);
    if (count == 0 || pos > node->length) return node;
    EnterCriticalSection(&cacheLock);
    const unsigned int length = node->length;
    Node* n = GetUnique(node, length + count);
    std::memmove(n->text + count + pos, n->text + pos, length - pos + 1);
    std::memset(n->text + pos, static_cast<int>(ch), count);
    n->length += count;
    Node* r = Insert(n);
    LeaveCriticalSection(&cacheLock);
    return r;
}

Node* __cdecl PStringCache::Ins(Node* node, const unsigned char* text, unsigned int pos)
{
    if (!node) {
        if (!text) return nullptr;
        const unsigned int n = CacheWork::Length(reinterpret_cast<const char*>(text));
        return Create(text, n, n);
    }
    if (!text) return node;
    const unsigned int count = CacheWork::Length(reinterpret_cast<const char*>(text));
    if (count == 0 || pos > node->length) return node;
    EnterCriticalSection(&cacheLock);
    const unsigned int length = node->length;
    Node* n = GetUnique(node, length + count);
    std::memmove(n->text + pos + count, n->text + pos, length - pos + 1);
    std::memcpy(n->text + pos, text, count);
    n->length += count;
    Node* r = Insert(n);
    LeaveCriticalSection(&cacheLock);
    return r;
}

Node* __cdecl PStringCache::Del(Node* node, unsigned int pos, unsigned int count)
{
    if (!node || count == 0 || pos >= node->length) return node;
    EnterCriticalSection(&cacheLock);
    Node* n = GetUnique(node, node->length);
    if (n->length < pos + count) {
        n->text[pos] = 0;
        n->length = pos;
    } else {
        std::memmove(n->text + pos, n->text + count + pos, n->length - count - pos + 1);
        n->length -= count;
    }
    Node* r = Insert(n);
    LeaveCriticalSection(&cacheLock);
    return r;
}

// the formatted text (StringFormat's buffers); nothing formatted gives no node
Node* __cdecl PStringCache::Set(Node* node, const unsigned char* fmt, char* args)
{
    if (node) CacheWork::Release(*this, node);
    StringFormat f(reinterpret_cast<const char*>(fmt), args);
    if (f.bytes == 0) return nullptr;
    return Create(reinterpret_cast<const unsigned char*>(f.text), static_cast<unsigned int>(f.count),
                  static_cast<unsigned int>(f.count));
}

Node* __cdecl PStringCache::Set(Node* node, int value, unsigned int radix)
{
    char text[0x44];
    Crt80::_itoa(value, text, static_cast<int>(radix));
    if (node) CacheWork::Release(*this, node);
    const unsigned int n = CacheWork::Length(text);
    return Create(reinterpret_cast<const unsigned char*>(text), n, n);
}

Node* __cdecl PStringCache::ToLower(Node* node)
{
    if (!node || node->length == 0) return node;
    EnterCriticalSection(&cacheLock);
    Node* n = GetUnique(node, node->length);
    Crt80::_strlwr(n->text);
    node = Insert(n);
    LeaveCriticalSection(&cacheLock);
    return node;
}

Node* __cdecl PStringCache::ToUpper(Node* node)
{
    if (!node || node->length == 0) return node;
    EnterCriticalSection(&cacheLock);
    Node* n = GetUnique(node, node->length);
    Crt80::_strupr(n->text);
    node = Insert(n);
    LeaveCriticalSection(&cacheLock);
    return node;
}

// the text without any of `chars`; unchanged text keeps the node
Node* __cdecl PStringCache::Strip(Node* node, const unsigned char* chars)
{
    if (!node || node->length == 0) return node;
    char local[0x2000];
    char* out = node->length > 0x1fff ? static_cast<char*>(Jet::Alloc::New(node->length + 1)) : local;
    unsigned int n = 0;
    for (unsigned int i = 0; i < node->length; ++i)
        if (!std::strchr(reinterpret_cast<const char*>(chars), static_cast<unsigned char>(node->text[i])))
            out[n++] = node->text[i];
    Node* r = node;
    if (n != node->length) {
        out[n] = 0;
        CacheWork::Release(*this, node);
        r = Create(reinterpret_cast<const unsigned char*>(out), n, n);
    }
    if (out != local && out) Jet::Alloc::Delete(out);
    return r;
}

// ---- PString

void __cdecl PString::Setf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    node = cache->Set(node, reinterpret_cast<const unsigned char*>(fmt), ap);
    va_end(ap);
}

int __cdecl PString::Add(const char* fmt, char* args)
{
    StringFormat f(fmt, args);
    node = cache->Ins(node, reinterpret_cast<const unsigned char*>(f.text), node ? node->length : 0);
    return f.count;
}

int __cdecl PString::Addf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int r = Add(fmt, ap);
    va_end(ap);
    return r;
}

namespace {
inline int Sign(int c) { return c < 0 ? -1 : c > 0 ? 1 : 0; }
}  // namespace

bool __cdecl PString::operator<(const PString& o) const
{
    if (!node) return o.node != nullptr;
    if (!o.node) return false;
    return std::strcmp(node->text, o.node->text) < 0;
}

bool __cdecl PString::operator>(const PString& o) const
{
    if (!node) return false;
    if (!o.node) return true;
    return std::strcmp(node->text, o.node->text) > 0;
}

int __cdecl PString::Compare(const PString& o) const
{
    if (node == o.node) return 0;
    if (!node) return o.node->text[0] != 0 ? -1 : 0;
    if (!o.node) return node->text[0] != 0 ? 1 : 0;
    return Sign(std::strcmp(node->text, o.node->text));
}

int __cdecl PString::Compare(const char* text) const
{
    if (!node) return text && *text ? -1 : 0;
    if (!text) return node->text[0] != 0 ? 1 : 0;
    return Sign(std::strcmp(node->text, text));
}

int __cdecl PString::CompareIgnoreCase(const PString& o) const
{
    if (node == o.node) return 0;
    if (!node) return o.node->text[0] != 0 ? -1 : 0;
    if (!o.node) return node->text[0] != 0 ? 1 : 0;
    return Crt80::_stricmp(node->text, o.node->text);
}

int __cdecl PString::CompareIgnoreCase(const char* text) const
{
    if (!node) return text && *text ? -1 : 0;
    if (!text) return node->text[0] != 0 ? 1 : 0;
    return Crt80::_stricmp(node->text, text);
}

bool __cdecl PString::EqualsIgnoreCase(const PString& o) const
{
    if (!node) return !o.node;
    if (!o.node) return false;
    if (node == o.node) return true;
    return Crt80::_stricmp(node->text, o.node->text) == 0;
}

int __cdecl PString::SortPtr(const void* a, const void* b)
{
    const unsigned int x = *static_cast<const unsigned int*>(a), y = *static_cast<const unsigned int*>(b);
    return x < y ? -1 : y < x ? 1 : 0;
}

// The header: bit 30 set = a UTF-16 byte count, else the length. An ANSI text cut short frees its node with
// Jet::Alloc::Delete whatever heap it came from, as in the original.
bool __cdecl PString::Read(BinReader& r)
{
    if (node) CacheWork::Release(*cache, node);
    node = nullptr;
    unsigned int header = 0;
    if (r.Read(&header, 4) != 4) return false;
    const bool wide = (header & 0x40000000) != 0;
    unsigned int length = header;
    if (wide) {
        header &= ~0x40000000u;
        length = header >> 1;
    }
    if (length == 0) return true;
    if (wide) {
        unsigned short local[0x200];
        unsigned short* text = header + 2 > 0x400 ? static_cast<unsigned short*>(Alloc::New(header + 2)) : local;
        bool ok = false;
        if (r.Read(text, header) == header) {
            text[length] = 0;
            node = cache->CreateUnicode(reinterpret_cast<const __wchar_t*>(text), static_cast<int>(length));  // 0x10014170
            ok = node != nullptr;
        }
        if (text != local && text) Alloc::Delete(text);
        return ok;
    }
    Node* n = CacheWork::Empty(length);
    if (r.Read(n->text, header) != header) {
        if (n) Alloc::Delete(n);
        return false;
    }
    n->text[length] = 0;
    n->length = length;
    EnterCriticalSection(&cacheLock);  // 0x100396f0
    node = cache->Insert(n);
    LeaveCriticalSection(&cacheLock);
    return true;
}

bool __cdecl PString::Write(BinWriter& w) const
{
    unsigned int length = node ? node->length : 0;
    if (w.Write(&length, 4) != 4) return false;
    length &= ~0x40000000u;
    if (length == 0) return true;
    return w.Write(node->text, length) == length;
}

}  // namespace Jet
