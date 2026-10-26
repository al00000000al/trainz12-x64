// Unit U14b of KernelStandard_7c, the foundation: Alloc (0x10001030..0x100013d2), PackedHeap / PackedHeapMP
// (0x1001f180..0x1001f2e2, 0x10036cd0..0x10037863, 0x10037460 UnlinkAllocations), PoolBlockAlloc (0x10038760..
// 0x10038a99, 0x1000e850 / 0x1000e870), ReferenceCount (0x1003d3a0..0x1003d402), ConfigData's pool operators
// (0x1000e080, 0x1000e0a0, 0x10013ae0, 0x10013b00), g_jetSTLAllocHeap, and KernelStandardInit with the shutdown
// routine (0x1002ebb0, 0x1002ec50, the table runner 0x10050e20).
//
// The original's static initialisers run in this order (its .CRT$XCU table at 0x10051478): the Alloc instance, the
// free-list heads, the shutdown table runner, KernelStandardInit (setlocale, the main thread id, the PString cache,
// the TLS singleton), the counter frequency, ConfigData's pool, ... g_jetSTLAllocHeap. They are here in a "lib"
// segment, so they come before every other static of the module and go after them at exit.
#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <windows.h>

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <set>

#pragma warning(disable : 4073)  // initializers put in library initialization area
#pragma init_seg(lib)

namespace Jet {

// ---- Alloc

namespace {

struct FreeList {
    unsigned int tag;  // 0x155, never read
    void* head;        // the block headers, chained through the word after the tag
};

FreeList lists[0x80] = {
#define JET_L4 {0x155, nullptr}, {0x155, nullptr}, {0x155, nullptr}, {0x155, nullptr}
#define JET_L32 JET_L4, JET_L4, JET_L4, JET_L4, JET_L4, JET_L4, JET_L4, JET_L4
    JET_L32, JET_L32, JET_L32, JET_L32
#undef JET_L32
#undef JET_L4
};
unsigned int caps[0x80];    // 0x10074ba8: how many blocks a free list keeps
unsigned int counts[0x80];  // 0x10074da8
CRITICAL_SECTION locks[16]; // 0x10074fa8
bool shutDown = true;       // 0x100728d6: no locking before the constructor and after the destructor
unsigned int allocBytes, misses, hits, large, searches, searchMax;  // 0x100adb78..0x100adb8c

}  // namespace

Alloc::Alloc()
{
    for (CRITICAL_SECTION& cs : locks) InitializeCriticalSection(&cs);
    for (unsigned int i = 0; i < 0x80; ++i) {
        caps[i] = i == 0 ? 1 : 0x2000 / (i * 8) + 1;
        counts[i] = 0;
    }
    caps[2] = 0x2000;
    caps[3] = 0x1555;
    caps[4] = 0x1000;
    shutDown = false;
}

Alloc::~Alloc()
{
    shutDown = true;
    for (CRITICAL_SECTION& cs : locks) DeleteCriticalSection(&cs);
}

Alloc& Alloc::operator=(const Alloc&) { return *this; }

// Class c = (size + 0xb) / 8 holds 8c bytes with the tag; classes above 0x7f go straight to malloc. The tag is
// ((c ^ 0x155) & 0x1ff) | c << 9; Delete reads only c.
void* __cdecl Alloc::New(unsigned int size)
{
    unsigned int c, bytes;
    if (size == 0) {
        c = 1;
        bytes = 8;
    } else {
        bytes = (size + 0xb) & ~7u;
        c = (size + 0xb) >> 3;
        if (c > 0x7f) {
            ++large;
            goto fromHeap;
        }
    }
    if (!shutDown) EnterCriticalSection(&locks[c & 0xf]);
    {
        const bool unlocked = shutDown;
        if (void* head = lists[c].head) {
            --counts[c];
            ++hits;
            void** next = reinterpret_cast<void**>(static_cast<char*>(head) + 4);
            lists[c].head = *next;
            if (!unlocked) LeaveCriticalSection(&locks[c & 0xf]);
            return next;
        }
    }
    if (!shutDown) LeaveCriticalSection(&locks[c & 0xf]);
    ++misses;
fromHeap:
    allocBytes += bytes;
    if (unsigned int* p = static_cast<unsigned int*>(Crt80::malloc(bytes))) {
        *p = ((c ^ 0xffffff55u) & 0x1ff) | c << 9;
        return p + 1;
    }
    OutputDebugStringA("Alloc::New> out of memory!\n");
    throw std::bad_alloc();
}

void __cdecl Alloc::Delete(void* p)
{
    unsigned int* const block = static_cast<unsigned int*>(p) - 1;
    const unsigned int c = *block >> 9;
    if (c > 0x7f) {
        allocBytes -= c * 8;
        Crt80::free(block);
        return;
    }
    if (!shutDown) EnterCriticalSection(&locks[c & 0xf]);
    const bool unlocked = shutDown;
    if (counts[c] < caps[c]) {
        *static_cast<void**>(p) = lists[c].head;
        ++counts[c];
        lists[c].head = block;
        if (!unlocked) LeaveCriticalSection(&locks[c & 0xf]);
        return;
    }
    if (!shutDown) LeaveCriticalSection(&locks[c & 0xf]);
    Crt80::free(block);
}

static int Sprintf(char* out, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = Crt80::vsprintf(out, fmt, ap);
    va_end(ap);
    return n;
}

void __cdecl Alloc::Dump()
{
    const unsigned int total = large + misses + hits;
    const float all = static_cast<float>(total);
    const float hitShare = static_cast<float>(hits) * 100.0f / all;
    const float largeShare = static_cast<float>(large) * 100.0f / all;
    char text[0x400];
    Sprintf(text,
            "\nAllocator:\nHits  : %8u - %5.1f%%\nMisses: %8u\nLarge : %8u - %5.1f%%\nSearch: %8u - Maximum %u\n"
            "Alloc : %8u bytes, %u no.\n",
            hits, static_cast<double>(hitShare), misses, large, static_cast<double>(largeShare), searches, searchMax,
            allocBytes, total);
    OutputDebugStringA(text);
}

static Alloc allocInstance;  // 0x100add54

// ---- KernelStandardInit and the shutdown table

namespace {

void(__cdecl* shutdownTable[0x400])();  // 0x100acb70
int shutdownCount;                      // 0x100adc24

// 0x10050e20: the table, newest first, at exit
struct ShutdownRunner {
    ~ShutdownRunner()
    {
        while (shutdownCount > 0) shutdownTable[--shutdownCount]();
    }
} shutdownRunner;

}  // namespace

struct FoundationWork {
    // 0x100390b0
    static void MakeCache()
    {
        if (PString::cache) return;
        void* p = Alloc::New(sizeof(PStringCache));
        PString::cache = p ? ::new (p) PStringCache(0x3e5) : nullptr;
    }

    // 0x1002ec50
    static void __cdecl Shutdown()
    {
        if (PStringCache* c = PString::cache) {
            c->~PStringCache();
            Alloc::Delete(c);
        }
        PString::cache = nullptr;
        PString::isFinished = true;
        Log::Done();
        if (ThreadLocalStorage* t = ThreadLocalStorage::singleton) {
            t->~ThreadLocalStorage();
            Alloc::Delete(t);
        }
        ThreadLocalStorage::singleton = nullptr;
    }

    static void SetMainThread() { Thread::mainThreadId = GetCurrentThreadId(); }
};

}  // namespace Jet

// One instance per source file in the original (a header static); only the first does the work.
struct KernelStandardInit {
    KernelStandardInit();
    KernelStandardInit& operator=(const KernelStandardInit&);
};

KernelStandardInit::KernelStandardInit()
{
    static bool done = false;  // 0x100ae268 bit 0
    if (done) return;
    done = true;
    Jet::Crt80::setlocale(2 /* LC_CTYPE */, ".ACP");
    Jet::FoundationWork::SetMainThread();
    Jet::FoundationWork::MakeCache();
    Jet::ThreadLocalStorage::CreateTLS();
    if (Jet::shutdownCount == 0x400) __debugbreak();
    Jet::shutdownTable[Jet::shutdownCount++] = &Jet::FoundationWork::Shutdown;
}

KernelStandardInit& KernelStandardInit::operator=(const KernelStandardInit&) { return *this; }

static KernelStandardInit kernelStandardInit;

namespace Jet {

// ---- PackedHeap

class PackedHeapBlock {
public:
    int used;                // allocations in use
    unsigned int offset;     // bytes handed out from data
    unsigned int* freeHead;  // freed allocations of this block
    unsigned char data[1];
};

namespace {

constexpr unsigned int kBlockLimit = 0xffff5;  // offset + 4 + size stays below this

// {PackedHeapBlock* array, count, capacity}, ascending addresses (our CRT's heap)
struct BlockList {
    PackedHeapBlock** items;
    unsigned int count;
    unsigned int capacity;
};
static_assert(sizeof(BlockList) == 3 * sizeof(void*));

using SizeMap = std::map<unsigned int, unsigned int*>;  // size -> the newest freed allocation of that size

// every heap, by address (0x100adbf8)
std::set<PackedHeap*>* allHeaps;
bool leaked;  // 0x100ae20f: a block still in use when its heap went

}  // namespace

struct HeapWork {
    static BlockList& Blocks(PackedHeap& h) { return *reinterpret_cast<BlockList*>(h.blocks); }
    static const BlockList& Blocks(const PackedHeap& h) { return *reinterpret_cast<const BlockList*>(h.blocks); }
    static SizeMap*& Sizes(PackedHeap& h) { return *reinterpret_cast<SizeMap**>(h.bySize); }

    static void InitContainers(PackedHeap& h)
    {
        Blocks(h) = BlockList{nullptr, 0, 0};
        Sizes(h) = new SizeMap;
        h.bySize[1] = h.bySize[2] = nullptr;
    }
    static void ClearContainers(PackedHeap& h)
    {
        Blocks(h).count = 0;
        Sizes(h)->clear();
    }
    static void CopyContainers(PackedHeap& h, const PackedHeap& o)
    {
        const BlockList& from = Blocks(o);
        BlockList& to = Blocks(h);
        for (unsigned int i = 0; i < from.count; ++i) Insert(to, from.items[i]);
        *Sizes(h) = *Sizes(const_cast<PackedHeap&>(o));
    }
    static void FreeContainers(PackedHeap& h)
    {
        std::free(Blocks(h).items);
        Blocks(h) = BlockList{nullptr, 0, 0};
        delete Sizes(h);
        Sizes(h) = nullptr;
    }

    // the first block at or above p
    static unsigned int LowerBound(const BlockList& l, const void* p)
    {
        unsigned int lo = 0, hi = l.count;
        while (lo < hi) {
            const unsigned int mid = (lo + hi) / 2;
            if (reinterpret_cast<const unsigned char*>(l.items[mid]) < p)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo;
    }
    static void Insert(BlockList& l, PackedHeapBlock* b)
    {
        const unsigned int at = LowerBound(l, b);
        if (at < l.count && l.items[at] == b) return;
        if (l.count == l.capacity) {
            l.capacity = l.capacity ? l.capacity * 2 : 8;
            l.items = static_cast<PackedHeapBlock**>(std::realloc(l.items, l.capacity * sizeof(PackedHeapBlock*)));
        }
        std::memmove(l.items + at + 1, l.items + at, (l.count - at) * sizeof(PackedHeapBlock*));
        l.items[at] = b;
        ++l.count;
    }
    static void Erase(BlockList& l, PackedHeapBlock* b)
    {
        const unsigned int at = LowerBound(l, b);
        if (at == l.count || l.items[at] != b) return;
        std::memmove(l.items + at, l.items + at + 1, (l.count - at - 1) * sizeof(PackedHeapBlock*));
        --l.count;
    }

    // 0x10037460 PackedHeapBlock::UnlinkAllocations: the block's freed allocations off their size chains
    static void UnlinkAllocations(PackedHeapBlock* b)
    {
        unsigned char* const base = reinterpret_cast<unsigned char*>(b);
        for (unsigned int* c = b->freeHead; c;) {
            if (reinterpret_cast<unsigned char*>(c) < base + 0xc ||
                reinterpret_cast<unsigned char*>(c) >= base + b->offset + 0x20) {
                OutputDebugStringA("PackedHeapBlock::UnlinkAllocations> corrupt heap\n");
                break;
            }
            *reinterpret_cast<unsigned int**>(c[3]) = reinterpret_cast<unsigned int*>(c[4]);
            if (c[4]) reinterpret_cast<unsigned int*>(c[4])[3] = c[3];
            c = reinterpret_cast<unsigned int*>(c[2]);
        }
        b->freeHead = nullptr;
        b->offset = 0;
    }

    static unsigned char* Bump(PackedHeapBlock* b, unsigned int size)
    {
        const unsigned int at = b->offset;
        if (at + 4 + size >= kBlockLimit) return nullptr;
        b->offset = at + 4 + size;
        *reinterpret_cast<unsigned int*>(b->data + at) = size;
        ++b->used;
        return b->data + at + 4;
    }

    static void Throw()
    {
        throw std::bad_alloc();
    }

    // 0x10036cd0: one line on the "PackedHeap" channel
    static void Say(const char* fmt, ...)
    {
        static LogClient* const client = Log::GetClient("PackedHeap");  // 0x100ae1e0 (0x100509d0)
        struct Text : AnsiString {
            const int* Data() const { return static_cast<const int*>(data); }
        } text;
        va_list ap;
        va_start(ap, fmt);
        text.Add(fmt, ap);
        va_end(ap);
        client->kind = 2;
        const int* d = text.Data();
        client->OutputString("%s", d[0] ? reinterpret_cast<const char*>(d + 2) : nullptr);
    }
};

PackedHeap::PackedHeap(const char* n) : name(n), unused(0)
{
    HeapWork::InitContainers(*this);
    if (!allHeaps) allHeaps = new std::set<PackedHeap*>;
    allHeaps->insert(this);
}

// Copies take both containers as they are: the same blocks, and size chains whose links still point into the
// source's map (as the original's set / map copies do). A copy is not in the list of heaps.
PackedHeap::PackedHeap(const PackedHeap& o) : name(o.name), unused(o.unused)
{
    HeapWork::InitContainers(*this);
    HeapWork::CopyContainers(*this, o);
}

PackedHeap& PackedHeap::operator=(const PackedHeap& o)
{
    name = o.name;
    if (this != &o) {
        HeapWork::ClearContainers(*this);
        HeapWork::CopyContainers(*this, o);
    }
    unused = o.unused;
    return *this;
}

PackedHeap::~PackedHeap()
{
    if (allHeaps) {
        allHeaps->erase(this);
        if (allHeaps->empty()) {
            delete allHeaps;
            allHeaps = nullptr;
        }
    }
    BlockList& l = HeapWork::Blocks(*this);
    while (l.count) {
        PackedHeapBlock* const b = l.items[0];
        HeapWork::Erase(l, b);
        if (b->used == 0) {
            HeapWork::UnlinkAllocations(b);
            VirtualFree(b, 0, MEM_RELEASE);
        } else {
            leaked = true;
        }
    }
    HeapWork::FreeContainers(*this);
}

unsigned char* PackedHeap::AllocFromUnused(unsigned int size)
{
    unsigned int*& slot = (*HeapWork::Sizes(*this))[size];
    unsigned int* const c = slot;
    if (!c) return nullptr;
    *reinterpret_cast<unsigned int**>(c[3]) = reinterpret_cast<unsigned int*>(c[4]);
    if (c[4]) reinterpret_cast<unsigned int*>(c[4])[3] = c[3];
    *reinterpret_cast<unsigned int**>(c[1]) = reinterpret_cast<unsigned int*>(c[2]);
    if (c[2]) reinterpret_cast<unsigned int*>(c[2])[1] = c[1];
    PackedHeapBlock* const b = FindBlockForAllocation(reinterpret_cast<unsigned char*>(c + 1));
    if (++b->used == 1) --unused;
    c[0] = size;
    return reinterpret_cast<unsigned char*>(c + 1);
}

unsigned char* PackedHeap::Alloc(unsigned int size)
{
    if (size + 4 < 0x14) size = 0x10;
    const unsigned int s = (size + 3) & ~3u;
    if (s + 4 < 0x14 || s + 4 > 0xffff4) HeapWork::Throw();
    if (unsigned char* p = AllocFromUnused(s)) return p;
    // the blocks from the highest address down (a reverse walk of the original's set)
    const BlockList& l = HeapWork::Blocks(*this);
    for (unsigned int i = l.count; i-- > 0;) {
        if (unsigned char* p = HeapWork::Bump(l.items[i], s)) {
            if (l.items[i]->used == 1) --unused;
            return p;
        }
    }
    if (unsigned char* p = HeapWork::Bump(AllocBlock(), s)) return p;
    HeapWork::Throw();
    return nullptr;
}

bool PackedHeap::Free(PackedHeapBlock* b, unsigned char* p)
{
    unsigned char* const base = reinterpret_cast<unsigned char*>(b);
    if (p < base + 0x10 || p >= base + b->offset + 0xc || (reinterpret_cast<uintptr_t>(p) & 3)) return false;
    unsigned int* const c = reinterpret_cast<unsigned int*>(p) - 1;
    const unsigned int size = c[0];
    if (size >= b->offset || (size & 3)) return false;
    unsigned int*& slot = (*HeapWork::Sizes(*this))[size];
    c[3] = reinterpret_cast<unsigned int>(&slot);
    c[4] = reinterpret_cast<unsigned int>(slot);
    if (slot) slot[3] = reinterpret_cast<unsigned int>(&c[4]);
    slot = c;
    c[1] = reinterpret_cast<unsigned int>(&b->freeHead);
    c[2] = reinterpret_cast<unsigned int>(b->freeHead);
    if (b->freeHead) b->freeHead[1] = reinterpret_cast<unsigned int>(&c[2]);
    b->freeHead = c;
    if (--b->used == 0) ++unused;
    return true;
}

void PackedHeap::Free(unsigned char* p)
{
    if (!p) return;
    PackedHeapBlock* const b = FindBlockForAllocation(p);
    if (!b || !Free(b, p) || b->used != 0) return;
    if (unused > 1)
        FreeBlock(b);
    else
        HeapWork::UnlinkAllocations(b);
}

// the last block below p (no bounds check; Free checks)
PackedHeapBlock* PackedHeap::FindBlockForAllocation(unsigned char* p)
{
    const BlockList& l = HeapWork::Blocks(*this);
    const unsigned int at = HeapWork::LowerBound(l, p);
    return at ? l.items[at - 1] : nullptr;
}

PackedHeapBlock* PackedHeap::AllocBlock()
{
    auto* b = static_cast<PackedHeapBlock*>(
        VirtualAlloc(nullptr, 0x100000, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE));
    if (!b) {
        OutputDebugStringA("PackedHeap::AllocBlock> out of memory\n");
        HeapWork::Throw();
    }
    b->used = 0;
    b->offset = 0;
    b->freeHead = nullptr;
    HeapWork::Insert(HeapWork::Blocks(*this), b);
    return b;
}

// (the unused count is not lowered, as in the original)
void PackedHeap::FreeBlock(PackedHeapBlock* b)
{
    HeapWork::Erase(HeapWork::Blocks(*this), b);
    if (b->used != 0) {
        leaked = true;
        return;
    }
    HeapWork::UnlinkAllocations(b);
    VirtualFree(b, 0, MEM_RELEASE);
}

bool PackedHeap::IsEmpty() const
{
    const BlockList& l = HeapWork::Blocks(*this);
    for (unsigned int i = 0; i < l.count; ++i)
        if (l.items[i]->used != 0) return false;
    return true;
}

// The totals in KB, given to the format in the order free, used, unused (its labels say unused, used, free).
void PackedHeap::DebugPrint()
{
    unsigned int freeBytes = 0, usedBytes = 0, unusedBytes = 0;
    const BlockList& l = HeapWork::Blocks(*this);
    for (unsigned int i = 0; i < l.count; ++i) {
        const PackedHeapBlock* b = l.items[i];
        unsigned int f = 0;
        for (const unsigned int* c = b->freeHead; c; c = reinterpret_cast<const unsigned int*>(c[2])) f += c[0] + 4;
        freeBytes += f;
        usedBytes += b->offset - f;
        unusedBytes += 0xffff4 - b->offset;
    }
    HeapWork::Say("%s - %luk unused, %luk used, %luk free", name, freeBytes >> 10, usedBytes >> 10, unusedBytes >> 10);
}

void __cdecl PackedHeap::DebugPrintAll()
{
    HeapWork::Say("** Packed Heap Summary **");
    if (allHeaps)
        for (PackedHeap* h : *allHeaps) h->DebugPrint();
    HeapWork::Say("** - **");
}

PackedHeapMP::PackedHeapMP(const char* n) : PackedHeap(n), lock(nullptr) {}
PackedHeapMP::~PackedHeapMP() {}

unsigned char* PackedHeapMP::Alloc(unsigned int size)
{
    const bool locked = lock.Lock(INFINITE);
    unsigned char* p;
    try {
        p = PackedHeap::Alloc(size);
    } catch (...) {
        if (locked) lock.Unlock();
        throw;
    }
    if (locked) lock.Unlock();
    return p;
}

void PackedHeapMP::Free(unsigned char* p)
{
    const bool locked = lock.Lock(INFINITE);
    PackedHeap::Free(p);
    if (locked) lock.Unlock();
}

bool PackedHeapMP::IsEmpty() const
{
    const bool locked = lock.Lock(INFINITE);
    const bool r = PackedHeap::IsEmpty();
    if (locked) lock.Unlock();
    return r;
}

// ---- PoolBlockAlloc. A block is {next, prev, offset back to its Jet::Alloc block, free item chain, free items}
// at 0x14 bytes before the first item; each item is {block, data...} with the data aligned.

struct PoolWork {
    struct Block {
        Block* next;
        Block* prev;
        int rawOffset;
        void** freeHead;
        int freeItems;
    };
    static void Release(Block* b) { Alloc::Delete(reinterpret_cast<char*>(b) + b->rawOffset); }
};

PoolBlockAlloc::PoolBlockAlloc(unsigned int size, unsigned int n, unsigned int a) : lock(nullptr)
{
    count = static_cast<int>(n);
    align = static_cast<int>(a);
    if (align < 1 || (a & (~(a - 1) * 2 | (a - 1))) != 0) align = 8;
    itemSize = static_cast<int>((align + 3 + size) & ~(static_cast<unsigned int>(align) - 1));
    if (itemSize < align) itemSize = align;
    full = nullptr;
    partial = nullptr;
    freeItems = 0;
    uncarved = 0;
    carving = nullptr;
    blockBytes = itemSize * static_cast<int>(n) + 0x14;
}

PoolBlockAlloc::~PoolBlockAlloc() { ReleaseMemory(); }

void __cdecl PoolBlockAlloc::Lock() { lock.Lock(INFINITE); }
void __cdecl PoolBlockAlloc::Unlock() { lock.Unlock(); }

void __cdecl PoolBlockAlloc::ReleaseMemory()
{
    using Block = PoolWork::Block;
    Lock();
    while (full) {
        Block* b = static_cast<Block*>(full);
        full = b->next;
        PoolWork::Release(b);
    }
    while (partial) {
        Block* b = static_cast<Block*>(partial);
        partial = b->next;
        PoolWork::Release(b);
    }
    freeItems = 0;
    uncarved = 0;
    carving = nullptr;
    Unlock();
}

void* __cdecl PoolBlockAlloc::Alloc()
{
    using Block = PoolWork::Block;
    Lock();
    void** item;
    if (uncarved < 1) {
        if (freeItems > 0) {
            Block* const b = static_cast<Block*>(partial);
            item = b->freeHead;
            b->freeHead = static_cast<void**>(*item);
            *item = b;
            --b->freeItems;
            --freeItems;
            if (b->freeItems < 1) {  // no free item left: to the full list
                partial = b->next;
                if (partial) static_cast<Block*>(partial)->prev = nullptr;
                b->next = static_cast<Block*>(full);
                b->prev = nullptr;
                if (full) static_cast<Block*>(full)->prev = b;
                full = b;
            }
            Unlock();
            return item + 1;
        }
        char* const raw = static_cast<char*>(Jet::Alloc::New(static_cast<unsigned int>(blockBytes - 1 + align)));
        const uintptr_t first = (static_cast<uintptr_t>(align) + 0x17 + reinterpret_cast<uintptr_t>(raw)) &
                                ~(static_cast<uintptr_t>(align) - 1);
        Block* const b = reinterpret_cast<Block*>(first - 0x18);
        b->rawOffset = static_cast<int>(raw - reinterpret_cast<char*>(b));
        b->freeHead = nullptr;
        b->freeItems = 0;
        b->next = static_cast<Block*>(full);
        b->prev = nullptr;
        if (full) static_cast<Block*>(full)->prev = b;
        full = b;
        uncarved = count;
        carving = b;
    }
    item = reinterpret_cast<void**>(static_cast<char*>(carving) + 0x14 + (count - uncarved) * itemSize);
    --uncarved;
    *item = carving;
    if (uncarved < 1) carving = nullptr;
    Unlock();
    return item + 1;
}

void __cdecl PoolBlockAlloc::Free(void* p)
{
    using Block = PoolWork::Block;
    if (!p) return;
    Lock();
    void** const item = static_cast<void**>(p) - 1;
    Block* const b = static_cast<Block*>(*item);
    *item = b->freeHead;
    ++b->freeItems;
    b->freeHead = item;
    ++freeItems;
    if (b->freeItems < count) {
        if (b->freeItems == 1) {  // the first free item: from the full list to the partial one
            if (!b->prev)
                full = b->next;
            else
                b->prev->next = b->next;
            if (b->next) b->next->prev = b->prev;
            b->next = static_cast<Block*>(partial);
            b->prev = nullptr;
            if (partial) static_cast<Block*>(partial)->prev = b;
            partial = b;
        }
    } else {  // every item free: the block goes
        if (b->prev) b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
        if (b == full) {
            full = b->next;
            if (full) static_cast<Block*>(full)->prev = nullptr;
        } else if (b == partial) {
            partial = b->next;
            if (partial) static_cast<Block*>(partial)->prev = nullptr;
        }
        freeItems -= count;
        PoolWork::Release(b);
    }
    Unlock();
}

// ---- ConfigData's pool (0x10074198: 0x10-byte nodes in 0x14-byte items, 0x200 a block, 4-aligned)

static PoolBlockAlloc configDataPool(0x10, 0x200, 4);

void* __cdecl ConfigData::operator new(size_t) { return configDataPool.Alloc(); }
void* __cdecl ConfigData::operator new(size_t, void* at) { return at; }

void __cdecl ConfigData::operator delete(void* p)
{
    if (p) configDataPool.Free(p);
}

void __cdecl ConfigData::operator delete(void* p, void*)
{
    if (p) configDataPool.Free(p);
}

// ---- ReferenceCount (each module keeps its own vftable; these are the out-of-line members)

ReferenceCount::~ReferenceCount() {}

void __cdecl ReferenceCount::DeleteThis() const { delete this; }  // the deleting destructor (slot 1) with 1

void ReferenceCount::AddReference() const { InterlockedIncrement(reinterpret_cast<volatile LONG*>(&referenceCount)); }

bool ReferenceCount::ReleaseReference() const
{
    if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&referenceCount)) == 0) {
        DeleteThis();
        return true;
    }
    return false;
}

}  // namespace Jet

// ---- the archive records' heap (0x100ae180, FileSubArchive.cpp) and the one the STL containers of other modules
// use (0x100ae1ac). DebugPrintAll lists heaps by address and the original's archive heap comes first: the two are
// put in one section in that order (the linker sorts $-suffixed sections by name).

#pragma section(".jheap$a", read, write)
#pragma section(".jheap$b", read, write)
namespace Jet {
__declspec(allocate(".jheap$a")) PackedHeapMP fileSubArchiveHeap("FileSubArchive");
}  // namespace Jet
__declspec(allocate(".jheap$b")) Jet::PackedHeapMP g_jetSTLAllocHeap("#jetstd::");
