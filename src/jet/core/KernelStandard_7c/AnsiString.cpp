// Unit U14a of KernelStandard_7c: AnsiString and String (0x10001920..0x10003247, 0x100083e0..0x10008451,
// 0x10014090..0x10014d8d, 0x100331e0). Since U14b the state they share is ours too: the shared empty block
// (0x100ae254, a function-local static in the original, guard 0x100ae264) and the "#StringData" PackedHeapMP
// (0x100adc18) that holds every block up to 0xa0 bytes (StringHeap.h); larger blocks come from Alloc.
#include <jet/KernelStandard.h>

#include "Crt80.h"
#include "StringFormat.h"
#include "StringHeap.h"

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {
// {refs 1 (never counted), capacity 0, length 0, ""}: what every empty AnsiString points into. The original makes it
// on first use and registers a destructor that only releases it (a no-op): constant data here.
long nullBlock[4] = {1, 0, 0, 0};
PackedHeapMP* stringHeap;  // 0x100adc18
}  // namespace

void* StringHeap::New(unsigned int size)
{
    size = (size + 7) & ~7u;
    if (!stringHeap) {
        void* p = Alloc::New(sizeof(PackedHeapMP));
        stringHeap = p ? new (p) PackedHeapMP("#StringData") : nullptr;
    }
    if (size > 0xa0) return Alloc::New(size);
    return stringHeap->Alloc(size);
}

PackedHeapMP* StringHeap::Heap() { return stringHeap; }

// a block: {refs, capacity, length, characters}; AnsiString::data points at the capacity
struct AnsiWork {
    static long* NullBlock() { return nullBlock; }
    static PackedHeapMP* Heap() { return stringHeap; }
    static void InitNull() {}
    static void* NullData() { return nullBlock + 1; }

    static unsigned* NewBlock(unsigned size) { return static_cast<unsigned*>(StringHeap::New(size)); }

    // 0x100018a0
    static void Release(void* data)
    {
        long* block = static_cast<long*>(data) - 1;
        if (block == NullBlock()) return;
        if (InterlockedDecrement(block) != 0) return;
        if (((static_cast<unsigned*>(data)[0] + 0x13) & ~7u) <= 0xa0)
            Heap()->Free(reinterpret_cast<unsigned char*>(block));
        else
            Alloc::Delete(block);
    }

    // a fresh block of `cap` characters holding `length` (its characters not yet written)
    static void* Fresh(unsigned cap, unsigned length)
    {
        unsigned* b = NewBlock(cap + 0xc);
        b[0] = 1;
        b[1] = cap;
        b[2] = length;
        reinterpret_cast<char*>(b + 3)[length] = 0;
        return b + 1;
    }

    static unsigned& Cap(const AnsiString& s) { return static_cast<unsigned*>(s.data)[0]; }
    static unsigned& Len(const AnsiString& s) { return static_cast<unsigned*>(s.data)[1]; }
    static char* Chars(const AnsiString& s) { return reinterpret_cast<char*>(static_cast<unsigned*>(s.data) + 2); }
};

namespace {
inline unsigned NewCap(unsigned n) { return (n + 0x11) & ~0xfu; }
inline unsigned& Cap(const AnsiString& s) { return AnsiWork::Cap(s); }
inline unsigned& Len(const AnsiString& s) { return AnsiWork::Len(s); }
inline char* Chars(const AnsiString& s) { return AnsiWork::Chars(s); }
}  // namespace

// ---- the buffer

bool AnsiString::IsBufferUnique() const { return data && static_cast<long*>(data)[-1] == 1; }  // 0x10001920
bool AnsiString::IsBufferNULL() const { return Cap(*this) == 0; }                             // 0x10001940
unsigned int AnsiString::NewSize(unsigned int n) { return NewCap(n); }                         // 0x100331e0

// a private block of at least `size` characters; keep copies the text (and keeps the capacity at least)
void AnsiString::MakeBufferUnique(unsigned int size, bool keep)  // 0x10001950
{
    if (!data) {
        if (!size) {
            data = AnsiWork::Fresh(0, 0);  // a 0x10-byte block that claims 4 characters
            Cap(*this) = 4;
            return;
        }
        data = AnsiWork::Fresh(size, 0);
        return;
    }
    if (static_cast<long*>(data)[-1] == 1 && Cap(*this) >= size && Cap(*this) != 0) return;
    unsigned length = 0;
    if (keep) {
        length = Len(*this);
        if (size < Cap(*this)) size = Cap(*this);
    }
    void* fresh = AnsiWork::Fresh(size, length);
    if (keep) std::memcpy(static_cast<unsigned*>(fresh) + 2, Chars(*this), length + 1);
    AnsiWork::Release(data);
    data = fresh;
}

void AnsiString::MakeBufferNULL()  // 0x10001a60
{
    if (data) AnsiWork::Release(data);
    data = AnsiWork::NullData();
}

void AnsiString::MakeBufferEmpty()  // 0x10001ac0
{
    MakeBufferUnique(0x10, false);
    Len(*this) = 0;
    Chars(*this)[0] = 0;
}

void AnsiString::Set(const unsigned char* s, int n, int fallback)  // 0x10001af0
{
    if (!s) {
        MakeBufferNULL();
        return;
    }
    if (n < 0 && (n = fallback) < 0) n = static_cast<int>(std::strlen(reinterpret_cast<const char*>(s)));
    MakeBufferUnique(NewCap(n), false);
    std::memcpy(Chars(*this), s, n);
    Len(*this) = n;
    Chars(*this)[n] = 0;
}

// ---- construction

AnsiString::AnsiString() { data = AnsiWork::NullData(); }  // 0x100031f0

AnsiString::AnsiString(const AnsiString& o)  // 0x10001b60
{
    data = o.data;
    AnsiWork::InitNull();
    long* block = static_cast<long*>(data) - 1;
    if (block != AnsiWork::NullBlock()) InterlockedIncrement(block);
}

AnsiString::AnsiString(const PString& s)  // 0x10001bc0
{
    const PStringCache::Node* node = s.node;
    if (!node) {  // (and &node->text against null)
        data = nullptr;
        MakeBufferNULL();
        return;
    }
    const unsigned n = node->length;
    const unsigned cap = NewCap(n);
    data = AnsiWork::Fresh(cap, n);
    Len(*this) = PString::Cache()->GetANSI(const_cast<PStringCache::Node*>(node), Chars(*this), cap);
    if (static_cast<int>(Len(*this)) == -1) MakeBufferNULL();
}

AnsiString::AnsiString(const UnicodeString& s)  // 0x10001c60
{
    if (!s.text) {
        data = nullptr;
        MakeBufferNULL();
        return;
    }
    const unsigned n = s.length;
    const unsigned cap = NewCap(n);
    data = AnsiWork::Fresh(cap, n);
    char* out = Chars(*this);
    int r = static_cast<int>(Crt80::wcstombs(out, s.text, cap - 1));
    if (r < 0) {
        out[0] = 0;
        r = -1;
    } else {
        out[r] = 0;
    }
    Len(*this) = r;
    if (r == -1) MakeBufferNULL();
}

AnsiString::AnsiString(const char* s)  // 0x10001d00
{
    if (!s) {
        MakeBufferNULL();
        return;
    }
    const unsigned n = static_cast<unsigned>(std::strlen(s));
    MakeBufferUnique(NewCap(n), false);
    std::memcpy(Chars(*this), s, n);
    Len(*this) = n;
    Chars(*this)[n] = 0;
}

AnsiString::AnsiString(const char* s, unsigned int n)  // 0x10001da0: n < 0 counts up to the NUL
{
    if (!s) {
        MakeBufferNULL();
        return;
    }
    if (static_cast<int>(n) < 0) n = static_cast<unsigned>(std::strlen(s));
    MakeBufferUnique(NewCap(n), false);
    std::memcpy(Chars(*this), s, n);
    Len(*this) = n;
    Chars(*this)[n] = 0;
}

AnsiString::AnsiString(const __wchar_t* s) { SetUnicode(s, -1); }                                // 0x10001d80
AnsiString::AnsiString(const __wchar_t* s, unsigned int n) { SetUnicode(s, static_cast<int>(n)); }  // 0x10001e10

AnsiString::AnsiString(unsigned int ch, unsigned int count)  // 0x10001e40
{
    if (!count) {
        data = AnsiWork::NullData();
        return;
    }
    data = AnsiWork::Fresh(NewCap(count), count);
    std::memset(Chars(*this), static_cast<int>(ch), count);
    Chars(*this)[count] = 0;
}

AnsiString::~AnsiString() { AnsiWork::Release(data); }  // 0x10001ef0

// ---- assignment

const AnsiString& AnsiString::operator=(const AnsiString& o)  // 0x10001f00
{
    void* d = o.data;
    AnsiWork::InitNull();
    long* block = static_cast<long*>(d) - 1;
    if (block != AnsiWork::NullBlock()) InterlockedIncrement(block);
    AnsiWork::Release(data);
    data = o.data;
    return *this;
}

const AnsiString& AnsiString::operator=(const PString& s)  // 0x10001f70
{
    const unsigned cap = NewCap(s.node ? s.node->length : 0);
    MakeBufferUnique(cap, false);
    Len(*this) = PString::Cache()->GetANSI(s.node, Chars(*this), cap);
    if (static_cast<int>(Len(*this)) == -1) MakeBufferNULL();
    return *this;
}

const AnsiString& AnsiString::operator=(const UnicodeString& s)  // 0x10014d50
{
    SetUnicode(reinterpret_cast<const __wchar_t*>(s.text), -1);
    return *this;
}

const AnsiString& AnsiString::operator=(const char* s)  // 0x100021a0
{
    if (!s) {
        MakeBufferNULL();
        return *this;
    }
    const unsigned n = static_cast<unsigned>(std::strlen(s));
    MakeBufferUnique(NewCap(n), false);
    std::memcpy(Chars(*this), s, n);
    Len(*this) = n;
    Chars(*this)[n] = 0;
    return *this;
}

const AnsiString& AnsiString::operator=(const __wchar_t* s)  // 0x10002210: converted into a temporary first
{
    AnsiString t;
    t.SetUnicode(s, -1);
    if (Cap(t) == 0) {
        MakeBufferNULL();
    } else {
        const char* src = Chars(t);
        const unsigned n = static_cast<unsigned>(std::strlen(src));
        MakeBufferUnique(NewCap(n), false);
        std::memcpy(Chars(*this), src, n);
        Len(*this) = n;
        Chars(*this)[n] = 0;
    }
    return *this;
}

bool AnsiString::SetANSI(const char* s, int n)  // 0x10001fd0
{
    Set(reinterpret_cast<const unsigned char*>(s), n, n);
    return true;
}

bool AnsiString::SetUnicode(const __wchar_t* s, int n)  // 0x10002070
{
    if (n < 0) {
        const __wchar_t* p = s;
        while (*p) ++p;
        n = static_cast<int>(p - s);
    }
    const unsigned size = static_cast<unsigned>(n) * 2;
    char local[0x1004];
    char* buf = local;
    if (size > 0xffe) buf = static_cast<char*>(Alloc::New(size));
    const int r = static_cast<int>(Crt80::wcstombs(buf, reinterpret_cast<const wchar_t*>(s), size));
    if (r < 0) {
        MakeBufferNULL();
        if (buf != local && buf) Alloc::Delete(buf);
        return false;
    }
    Set(reinterpret_cast<const unsigned char*>(buf), r, n);
    if (buf != local && buf) Alloc::Delete(buf);
    return true;
}

int AnsiString::GetANSI(char* out, unsigned int size) const  // 0x10001ff0
{
    const unsigned len = Len(*this);
    if (len + 1 <= size) {
        if (len)
            std::strcpy(out, Chars(*this));
        else
            out[0] = 0;
        return static_cast<int>(Len(*this));
    }
    if (!size) return -1;
    const int n = static_cast<int>(size - 1);
    if (n > 0) {
        std::memcpy(out, Chars(*this), n);
        out[n] = 0;
        return n;
    }
    out[0] = 0;
    return 0;
}

int AnsiString::GetUnicode(__wchar_t* out, unsigned int size) const  // 0x10002150
{
    unsigned max = Len(*this);
    if (size >> 1 <= max) max = (size >> 1) - 1;
    const int r = static_cast<int>(Crt80::mbstowcs(reinterpret_cast<wchar_t*>(out), Chars(*this), max));
    if (r >= 0) {
        out[r] = 0;
        return r;
    }
    out[0] = 0;
    return -1;
}

// ---- comparison: a capacity of 0 is "null"; Less 0, Equal 1, Greater 2

AnsiString::CompareResult AnsiString::CompareWith(const char* s) const  // 0x10002670
{
    if (Cap(*this) == 0) return s == nullptr ? Equal : Less;
    if (!s) return Greater;
    const int r = Crt80::_stricmp(Chars(*this), s);
    if (r < 0) return Less;
    return r > 0 ? Greater : Equal;
}

AnsiString::CompareResult AnsiString::CompareWith(const AnsiString& o) const  // 0x100026c0
{
    if (Cap(*this) == 0) return Cap(o) == 0 ? Equal : Less;
    if (Cap(o) == 0) return Greater;
    const int r = Crt80::_stricmp(Chars(*this), Chars(o));
    if (r < 0) return Less;
    return r > 0 ? Greater : Equal;
}

namespace {
inline int ByteCompare(const char* a, const char* b)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(a);
    const unsigned char* q = reinterpret_cast<const unsigned char*>(b);
    for (;; ++p, ++q) {
        if (*p != *q) return *p < *q ? -1 : 1;
        if (!*p) return 0;
    }
}
}  // namespace

AnsiString::CompareResult AnsiString::CompareWithMatchCase(const char* s) const  // 0x10002710
{
    if (Cap(*this) == 0) return s == nullptr ? Equal : Less;
    if (!s) return Greater;
    const int r = ByteCompare(Chars(*this), s);
    if (r < 0) return Less;
    return r > 0 ? Greater : Equal;
}

AnsiString::CompareResult AnsiString::CompareWithMatchCase(const AnsiString& o) const  // 0x10002770
{
    if (Cap(*this) == 0) return Cap(o) == 0 ? Equal : Less;
    if (Cap(o) == 0) return Greater;
    const int r = ByteCompare(Chars(*this), Chars(o));
    if (r < 0) return Less;
    return r > 0 ? Greater : Equal;
}

// the null cases return 1 / 0 / 2 like CompareWith, the others -1 / 0 / 1
int AnsiString::Compare(const AnsiString& o) const  // 0x100027e0
{
    if (Cap(*this) == 0) return Cap(o) == 0;
    if (Cap(o) == 0) return 2;
    return ByteCompare(Chars(*this), Chars(o));
}

int AnsiString::Compare(const char* s) const  // 0x10002840
{
    if (Cap(*this) == 0) return s == nullptr;
    if (!s) return 2;
    return ByteCompare(Chars(*this), s);
}

int AnsiString::CompareIgnoreCase(const AnsiString& o) const  // 0x10002890
{
    if (Cap(*this) == 0) return Cap(o) == 0;
    if (Cap(o) == 0) return 2;
    return Crt80::_stricmp(Chars(*this), Chars(o));
}

int AnsiString::CompareIgnoreCase(const char* s) const  // 0x100028d0
{
    if (Cap(*this) == 0) return s == nullptr;
    if (!s) return 2;
    return Crt80::_stricmp(Chars(*this), s);
}

bool AnsiString::operator<(const char* s) const { return CompareWithMatchCase(s) == Less; }  // 0x10014790..
bool AnsiString::operator>(const char* s) const { return CompareWithMatchCase(s) == Greater; }
bool AnsiString::operator==(const char* s) const { return CompareWithMatchCase(s) == Equal; }
bool AnsiString::operator!=(const char* s) const { return CompareWithMatchCase(s) != Equal; }
bool AnsiString::operator<=(const char* s) const { return CompareWithMatchCase(s) != Greater; }
bool AnsiString::operator>=(const char* s) const { return CompareWithMatchCase(s) != Less; }
bool AnsiString::operator<(const AnsiString& o) const { return CompareWithMatchCase(o) == Less; }
bool AnsiString::operator>(const AnsiString& o) const { return CompareWithMatchCase(o) == Greater; }
bool AnsiString::operator==(const AnsiString& o) const { return CompareWithMatchCase(o) == Equal; }
bool AnsiString::operator!=(const AnsiString& o) const { return CompareWithMatchCase(o) != Equal; }
bool AnsiString::operator<=(const AnsiString& o) const { return CompareWithMatchCase(o) != Greater; }
bool AnsiString::operator>=(const AnsiString& o) const { return CompareWithMatchCase(o) != Less; }

bool AnsiString::EqualsIgnoreCase(const AnsiString& o) const  // 0x10014910
{
    if (Cap(*this) == 0) return Cap(o) == 0;
    if (Cap(o) == 0) return false;
    return Crt80::_stricmp(Chars(*this), Chars(o)) == 0;
}

bool AnsiString::EqualsIgnoreCase(const char* s) const  // 0x10014990
{
    if (Cap(*this) == 0) return s == nullptr;
    if (!s) return false;
    return Crt80::_stricmp(Chars(*this), s) == 0;
}

// ---- characters and search

AnsiString::operator const char*() const { return Cap(*this) ? Chars(*this) : nullptr; }  // 0x10008440
char& AnsiString::operator[](int i) { return Chars(*this)[i]; }                          // 0x100140b0
char& AnsiString::operator[](unsigned int i) { return Chars(*this)[i]; }
unsigned int AnsiString::operator[](int i) const { return static_cast<unsigned char>(Chars(*this)[i]); }  // 0x10014a00
unsigned int AnsiString::operator[](unsigned int i) const { return static_cast<unsigned char>(Chars(*this)[i]); }
unsigned int AnsiString::operator[](__int64 i) const  // (the low half is the index)
{
    return static_cast<unsigned char>(Chars(*this)[static_cast<int>(i)]);
}
unsigned int AnsiString::operator[](unsigned __int64 i) const
{
    return static_cast<unsigned char>(Chars(*this)[static_cast<int>(i)]);
}
unsigned int AnsiString::AllocSize() const { return Cap(*this); }           // 0x10014ad0
unsigned int AnsiString::Length() const { return Len(*this); }              // 0x10003240
unsigned int AnsiString::Size() const { return Cap(*this) ? Len(*this) + 1 : 0; }  // 0x10014100

int AnsiString::Find(unsigned int ch, unsigned int start) const  // 0x10002910
{
    if (!Cap(*this)) return -1;
    const char* p = std::strchr(Chars(*this) + start, static_cast<int>(ch));
    return p ? static_cast<int>(p - Chars(*this)) : -1;
}

int AnsiString::Find(const char* s, unsigned int start) const  // 0x10002950
{
    if (!Cap(*this)) return -1;
    const char* p = std::strstr(Chars(*this) + start, s);
    return p ? static_cast<int>(p - Chars(*this)) : -1;
}

namespace {
inline unsigned Lower(unsigned c)
{
    if (c < 0x80) return c - 0x41 < 0x1a ? (c | 0x20) : c;
    return static_cast<unsigned>(Crt80::tolower(static_cast<int>(c)));
}
inline unsigned Upper(unsigned c)
{
    if (c < 0x80) return c - 0x61 < 0x1a ? (c & ~0x20u) : c;
    return static_cast<unsigned>(Crt80::toupper(static_cast<int>(c)));
}
}  // namespace

int AnsiString::FindIgnoreCase(const char* t, unsigned int ch, unsigned int start)  // 0x10002d60
{
    if (!t) return -1;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(t) + start;
    const unsigned lo = Lower(ch), hi = Upper(ch);
    for (; *p; ++p)
        if (*p == lo || *p == hi) return static_cast<int>(reinterpret_cast<const char*>(p) - t);
    return -1;
}

int AnsiString::FindIgnoreCase(const char* t, unsigned int len, const char* s, unsigned int start)  // 0x10002e00
{
    if (!t) return -1;
    const unsigned n = static_cast<unsigned>(std::strlen(s));
    if (!n) return static_cast<int>(start);
    const unsigned first = static_cast<unsigned char>(s[0]);
    const unsigned lo = Lower(first), hi = Upper(first);
    const unsigned char* p = reinterpret_cast<const unsigned char*>(t) + start;
    const auto left = [&] { return len - static_cast<unsigned>(reinterpret_cast<const char*>(p) - t); };
    if (left() < n) return -1;
    while (*p) {
        while (*p != lo && *p != hi) {
            ++p;
            if (!*p) return -1;
        }
        if (Crt80::_strnicmp(reinterpret_cast<const char*>(p), s, n) == 0)
            return static_cast<int>(reinterpret_cast<const char*>(p) - t);
        ++p;
        if (left() < n) return -1;
    }
    return -1;
}

int AnsiString::FindIgnoreCase(unsigned int ch, unsigned int start) const  // 0x10014ae0
{
    return FindIgnoreCase(Chars(*this), ch, start);
}

int AnsiString::FindIgnoreCase(const char* s, unsigned int start) const  // 0x10014b00
{
    return FindIgnoreCase(Chars(*this), Len(*this), s, start);
}

// ---- numbers (no null checks: the empty block reads as "")

double __cdecl AnsiString_GetDouble(const AnsiString* s) { return Crt80::atof(AnsiWork::Chars(*s)); }  // 0x10014770
double AnsiString::GetDouble() const { return AnsiString_GetDouble(this); }
float AnsiString::GetFloat() const { return static_cast<float>(AnsiString_GetDouble(this)); }
int AnsiString::GetInt(int base) const { return Crt80::strtol(Chars(*this), nullptr, base); }        // 0x10014730
unsigned int AnsiString::GetUInt(int base) const { return Crt80::strtoul(Chars(*this), nullptr, base); }  // 0x10014750

void AnsiString::Set(int value, unsigned int radix)  // 0x100025e0
{
    char buf[68];
    Crt80::_itoa(value, buf, static_cast<int>(radix));
    const unsigned n = static_cast<unsigned>(std::strlen(buf));
    MakeBufferUnique(NewCap(n), false);
    std::memcpy(Chars(*this), buf, n);
    Len(*this) = n;
    Chars(*this)[n] = 0;
}

// ---- editing

void AnsiString::Setn(const char* s, unsigned int n)  // 0x10002580
{
    const unsigned len = static_cast<unsigned>(std::strlen(s));
    if (len < n) n = len;
    MakeBufferUnique(NewCap(n), false);
    std::memcpy(Chars(*this), s, n);
    Len(*this) = n;
    Chars(*this)[n] = 0;
}

void AnsiString::Strip(const char* set)  // 0x100022f0
{
    MakeBufferUnique(0, true);
    unsigned out = 0;
    for (unsigned i = 0; i < Len(*this); ++i)
        if (!std::strchr(set, static_cast<unsigned char>(Chars(*this)[i]))) Chars(*this)[out++] = Chars(*this)[i];
    Len(*this) = out;
    Chars(*this)[out] = 0;
}

void AnsiString::Left(unsigned int n)  // 0x10002990
{
    if (n < Len(*this)) {
        MakeBufferUnique(0, true);
        Len(*this) = n;
        Chars(*this)[n] = 0;
    }
}

void AnsiString::Right(unsigned int n)  // 0x10014a10
{
    if (n < Len(*this)) Del(0, Len(*this) - n);
}

void AnsiString::SetLength(unsigned int n)  // 0x100029c0: grows the buffer, shortens the text
{
    MakeBufferUnique(NewCap(n), true);
    if (n < Len(*this)) Len(*this) = n;
    Chars(*this)[n] = 0;
}

void AnsiString::Del(unsigned int pos, unsigned int count)  // 0x10002bd0
{
    if (!count || pos >= Len(*this)) return;
    MakeBufferUnique(0, true);
    char* c = Chars(*this);
    if (Len(*this) < pos + count) {
        Len(*this) = pos;
        c[pos] = 0;
        return;
    }
    std::memmove(c + pos, c + pos + count, ((Len(*this) - pos) - count) + 1);
    Len(*this) -= count;
}

void AnsiString::Ins(const char* s, unsigned int pos)  // 0x10002a00
{
    if (!s) return;
    const unsigned n = static_cast<unsigned>(std::strlen(s));
    if (!n || pos > Len(*this)) return;
    const unsigned old = Len(*this);
    const unsigned len = old + n;
    MakeBufferUnique(NewCap(len), true);
    if (len < Len(*this)) Len(*this) = len;
    Chars(*this)[len] = 0;
    Len(*this) = len;
    std::memmove(Chars(*this) + n + pos, Chars(*this) + pos, (old - pos) + 1);
    std::memcpy(Chars(*this) + pos, s, n);
}

void AnsiString::Ins(unsigned int ch, unsigned int pos, unsigned int count)  // 0x10002b40
{
    if (!count || pos > Len(*this)) return;
    const unsigned old = Len(*this);
    const unsigned len = old + count;
    MakeBufferUnique(NewCap(len), true);
    if (len < Len(*this)) Len(*this) = len;
    Chars(*this)[len] = 0;
    Len(*this) = len;
    std::memmove(Chars(*this) + pos + count, Chars(*this) + pos, (old - pos) + 1);
    std::memset(Chars(*this) + pos, static_cast<int>(ch), count);
}

void AnsiString::Ins(const __wchar_t* s, unsigned int pos)  // 0x10002ab0
{
    AnsiString t;
    t.SetUnicode(s, -1);
    Ins(Cap(t) ? Chars(t) : nullptr, pos);
}

AnsiString& AnsiString::operator+=(const AnsiString& o)  // 0x10014b50
{
    Ins(Cap(o) ? Chars(o) : nullptr, Len(*this));
    return *this;
}

AnsiString& AnsiString::operator+=(const PString& s)  // 0x100030e0
{
    Ins(s.node ? s.node->text : nullptr, Len(*this));
    return *this;
}

AnsiString& AnsiString::operator+=(const char* s)  // 0x10014090
{
    Ins(s, Len(*this));
    return *this;
}

AnsiString& AnsiString::operator+=(const __wchar_t* s)  // 0x10014b30
{
    Ins(s, Len(*this));
    return *this;
}

void AnsiString::ToLower()  // 0x10014b90
{
    if (!Len(*this)) return;
    MakeBufferUnique(0, true);
    Crt80::_strlwr(Chars(*this));
}

void AnsiString::ToUpper()  // 0x10014bc0
{
    if (!Len(*this)) return;
    MakeBufferUnique(0, true);
    Crt80::_strupr(Chars(*this));
}

// the first run of characters not in `delimiters` (after leading delimiters); everything up to its end is cut
AnsiString AnsiString::Token(const char* delimiters)  // 0x10002c40
{
    AnsiString r;
    unsigned start = 0;
    while (start < Len(*this) && std::strchr(delimiters, static_cast<unsigned char>(Chars(*this)[start]))) ++start;
    unsigned end = start;
    while (end < Len(*this) && !std::strchr(delimiters, static_cast<unsigned char>(Chars(*this)[end]))) ++end;
    if (end != start) r.Setn(Chars(*this) + start, end - start);
    if (end) Del(0, end);
    return r;
}

AnsiString AnsiString::Mid(unsigned int pos, unsigned int n) const  // 0x10014a40
{
    AnsiString r(*this);
    r.Del(0, pos);
    if (n < Len(r)) {
        r.MakeBufferUnique(0, true);
        Len(r) = n;
        Chars(r)[n] = 0;
    }
    return r;
}

// ---- formatting

int AnsiString::Add(const char* fmt, char* args)  // 0x10002350
{
    StringFormat f(fmt, args);
    if (f.bytes > 0) {
        const unsigned old = Len(*this);
        const unsigned len = f.bytes + old;
        MakeBufferUnique(NewCap(len), true);
        if (len < Len(*this)) Len(*this) = len;
        Chars(*this)[len] = 0;
        std::memcpy(Chars(*this) + old, f.text, f.bytes);
        Len(*this) = len;
    }
    return f.count;
}

int AnsiString::Addf(const char* fmt, ...)  // 0x10002450
{
    va_list ap;
    va_start(ap, fmt);
    const int r = Add(fmt, ap);
    va_end(ap);
    return r;
}

void AnsiString::Set(const char* fmt, char* args)  // 0x10002470
{
    StringFormat f(fmt, args);
    if (f.bytes == 0) {
        MakeBufferNULL();
        return;
    }
    MakeBufferUnique(NewCap(f.bytes), false);
    std::memcpy(Chars(*this), f.text, f.bytes);
    Len(*this) = f.bytes;
    Chars(*this)[f.bytes] = 0;
}

void AnsiString::Setf(const char* fmt, ...)  // 0x10002560
{
    va_list ap;
    va_start(ap, fmt);
    Set(fmt, ap);
    va_end(ap);
}

// ---- binary form: a 4-byte header, bit 30 set for UTF-16 (then the byte count), else the length

bool AnsiString::Read(BinReader& r)  // 0x10002f00
{
    unsigned header = 0;
    if (r.Read(&header, 4) != 4) {
        MakeBufferNULL();
        return false;
    }
    const bool wide = (header & 0x40000000) != 0;
    unsigned bytes = header, n = header;
    if (wide) {
        bytes = header & 0xbfffffff;
        n = bytes >> 1;
    }
    if (!n) {
        MakeBufferEmpty();
        return true;
    }
    bool ok;
    if (!wide) {
        SetLength(n);
        ok = r.Read(Chars(*this), bytes) == bytes;
        if (ok) Len(*this) = n;
    } else {
        char local[0x400];
        char* buf = local;
        if (bytes + 2 > 0x400) buf = static_cast<char*>(Alloc::New(bytes + 2));
        ok = r.Read(buf, bytes) == bytes;
        if (ok) {
            reinterpret_cast<__wchar_t*>(buf)[n] = 0;
            ok = SetUnicode(reinterpret_cast<const __wchar_t*>(buf), n);
        }
        if (buf != local && buf) Alloc::Delete(buf);
    }
    if (!ok) MakeBufferNULL();
    return ok;
}

bool AnsiString::Write(BinWriter& w) const  // 0x10003080
{
    unsigned header = Len(*this);
    if (w.Write(&header, 4) != 4) return false;
    if (!header) return true;
    return w.Write(Chars(*this), header) == header;
}

// ---- String: AnsiString under another name (most members are the same code)

String::String() {}
String::String(const String& o) : AnsiString(o) {}  // 0x100083e0
String::String(const AnsiString& o) : AnsiString(o) {}
String::String(const PString& s) : AnsiString(s) {}                                   // 0x10014c10
String::String(const UnicodeString& s) : AnsiString(s) {}                             // 0x10014bf0
String::String(unsigned int ch, unsigned int count) : AnsiString(ch, count) {}        // 0x10014d30
String::String(const char* s) : AnsiString(s) {}                                      // 0x10014120
String::String(const char* s, unsigned int n) : AnsiString(s, n) {}                   // 0x10014d10
String::~String() {}

String::String(const String& o, unsigned int pos, int n)  // 0x10014c30
{
    const unsigned len = AnsiWork::Len(o);
    if (pos >= len) return;
    const unsigned rest = len - pos;
    if (n < 0 || rest < static_cast<unsigned>(n)) n = static_cast<int>(rest);
    const char* src = AnsiWork::Cap(o) ? AnsiWork::Chars(o) : nullptr;
    AnsiString t(src + pos, static_cast<unsigned>(n));
    AnsiString::operator=(t);
}

String::operator const char*() const { return AnsiString::operator const char*(); }
const String& String::operator=(const String& o) { AnsiString::operator=(o); return *this; }      // 0x10008400
const String& String::operator=(const AnsiString& o) { AnsiString::operator=(o); return *this; }
const String& String::operator=(const PString& s) { AnsiString::operator=(s); return *this; }     // 0x10014d70
const String& String::operator=(const UnicodeString& s) { AnsiString::operator=(s); return *this; }
const String& String::operator=(const char* s) { AnsiString::operator=(s); return *this; }        // 0x10008420

}  // namespace Jet
