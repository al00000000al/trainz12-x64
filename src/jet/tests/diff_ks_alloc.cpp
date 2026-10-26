// Differential test of KernelStandard unit U14b, Memory.cpp: random scripts (made once, before either side runs)
// played through KernelStandard_7c_orig.dll's and our Alloc, PackedHeap / PackedHeapMP, PoolBlockAlloc,
// ConfigData pool operators and ReferenceCount. Addresses differ between the modules, so what is compared is where
// things land relative to each other:
//  - Alloc: the tag word, the alignment, which earlier freed block a New hands back (free-list reuse; blocks freed to
//    msvcr80 may come back in any order and count as "fresh"), and the changes of Dump's counters (its text, caught
//    by patching the module's OutputDebugStringA import).
//  - PackedHeap: the block (by creation order) and offset of every allocation, the bytes before it, the block
//    headers {used, offset, free chain length}, IsEmpty, the unused count.
//  - PoolBlockAlloc: the block and item index of every allocation and the object's counters.
//  - ReferenceCount: which slot of a stand-in vftable each call reaches.
// A thread stress (several threads on Alloc and PackedHeapMP) runs on each side only for crashes and corruption.
//
// usage: diff_ks_alloc.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [steps]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static HMODULE ours, orig, cur;

template <class F>
static F Get(HMODULE m, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}
template <class F>
static F G(const char* name) { return Get<F>(cur, name); }

static std::vector<long long> logv;
static std::vector<int> lines;
static void Log(long long v, int line)
{
    logv.push_back(v);
    lines.push_back(line);
}
#define L(v) Log(static_cast<long long>(v), __LINE__)

static unsigned rngState;
static unsigned Rnd(unsigned n)
{
    rngState = rngState * 1103515245u + 12345u;
    return n ? (rngState >> 8) % n : 0;
}
struct Step {
    int op, obj, a, b, c;
};
static std::vector<Step> MakeScript(int ops, int objs, int steps, unsigned seed)
{
    rngState = seed;
    std::vector<Step> v;
    for (int i = 0; i < steps; ++i)
        v.push_back(Step{static_cast<int>(Rnd(ops)), static_cast<int>(Rnd(objs)), static_cast<int>(Rnd(100000)),
                         static_cast<int>(Rnd(100000)), static_cast<int>(Rnd(1000))});
    return v;
}

// ---- OutputDebugStringA of a module, caught

static std::string debugText;
static void __stdcall CatchDebug(const char* s) { debugText += s; }
static void PatchDebugOutput(HMODULE m)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    void* target = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "OutputDebugStringA"));
    void* target2 = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernelbase.dll"), "OutputDebugStringA"));
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        for (auto* t = reinterpret_cast<void**>(base + d->FirstThunk); *t; ++t) {
            if (*t != target && *t != target2) continue;
            DWORD old;
            VirtualProtect(t, sizeof *t, PAGE_READWRITE, &old);
            *t = reinterpret_cast<void*>(&CatchDebug);
            VirtualProtect(t, sizeof *t, old, &old);
        }
    }
}

// Dump's numbers: hits, misses, large, alloc bytes, count
static std::vector<long long> DumpNumbers()
{
    debugText.clear();
    G<void(__cdecl*)()>("?Dump@Alloc@Jet@@SAXXZ")();
    std::vector<long long> v;
    unsigned u;
    const char* s = debugText.c_str();
    for (const char* key : {"Hits  :", "Misses:", "Large :", "Alloc :"}) {
        const char* p = std::strstr(s, key);
        v.push_back(p && std::sscanf(p + std::strlen(key), "%u", &u) == 1 ? u : -1);
    }
    const char* p = std::strstr(s, "bytes, ");
    v.push_back(p && std::sscanf(p + 7, "%u", &u) == 1 ? u : -1);
    return v;
}

// ---- Alloc

using New = void*(__cdecl*)(unsigned);
using Del = void(__cdecl*)(void*);

static void AllocScenario(const std::vector<Step>& script)
{
    const auto aNew = G<New>("?New@Alloc@Jet@@SAPAXI@Z");
    const auto aDel = G<Del>("?Delete@Alloc@Jet@@SAXPAX@Z");
    const std::vector<long long> base = DumpNumbers();
    std::vector<void*> live(64, nullptr);
    std::vector<unsigned> size(64, 0);
    std::map<void*, int> freedAt;  // blocks freed so far, by the step that freed them
    int stepNo = 0;
    for (const Step& st : script) {
        ++stepNo;
        void*& p = live[st.obj];
        L(st.op);
        switch (st.op) {
        case 0:
        case 1:
        case 2: {
            if (p) {
                aDel(p);
                freedAt[p] = stepNo;
                p = nullptr;
            }
            // sizes around the class bounds, mostly small
            unsigned n;
            const unsigned k = st.a % 10;
            if (k < 5) n = st.b % 0x100;
            else if (k < 7) n = 0x3e0 + st.b % 0x30;  // the last classes and the first large sizes
            else if (k < 9) n = st.b % 0x400;
            else n = 0x400 + st.b % 0x4000;
            if (st.c % 50 == 0) n = 0;
            const long long hitsBefore = st.c % 8 == 0 ? DumpNumbers()[0] : 0;
            p = aNew(n);
            size[st.obj] = n;
            const unsigned tag = static_cast<unsigned*>(p)[-1];
            L(tag);
            L(reinterpret_cast<uintptr_t>(p) & 7);
            auto it = freedAt.find(p);
            // a free-list hit (Dump's count) hands back a block freed before: which one is exact. A block from
            // msvcr80 may have any earlier address.
            if (st.c % 8 == 0) {
                const bool hit = DumpNumbers()[0] != hitsBefore;
                L(hit);
                L(hit && it != freedAt.end() ? stepNo - it->second : -1);
            }
            if (it != freedAt.end()) freedAt.erase(it);
            std::memset(p, 0x40 + st.obj, n);
            break;
        }
        case 3:
            if (p) {
                const unsigned char* b = static_cast<unsigned char*>(p);
                bool ok = true;
                for (unsigned i = 0; i < size[st.obj]; ++i) ok &= b[i] == 0x40 + st.obj;
                L(ok);
                aDel(p);
                freedAt[p] = stepNo;
                p = nullptr;
            }
            break;
        case 4: {
            const std::vector<long long> d = DumpNumbers();
            for (size_t i = 0; i < d.size(); ++i) L(d[i] - base[i]);
            break;
        }
        }
    }
    for (void*& p : live)
        if (p) aDel(p), p = nullptr;
    const std::vector<long long> d = DumpNumbers();
    for (size_t i = 0; i < d.size(); ++i) L(d[i] - base[i]);
}

// How many freed blocks a class keeps: its free list is drained first (the module's own frees leave some there),
// then n blocks are made and freed, and made again: the hits are the list's capacity (when n is larger).
static void FreeListScenario()
{
    const auto aNew = G<New>("?New@Alloc@Jet@@SAPAXI@Z");
    const auto aDel = G<Del>("?Delete@Alloc@Jet@@SAXPAX@Z");
    for (unsigned size : {0x7cu, 0x1fcu, 0x3f0u}) {
        const unsigned c = (size + 0xb) >> 3;
        const unsigned n = 0x2000 / (8 * c) + 12;
        std::vector<void*> drain, made;
        for (int i = 0; i < 0x400; ++i) drain.push_back(aNew(size));
        for (unsigned i = 0; i < n; ++i) made.push_back(aNew(size));
        for (void* p : made) aDel(p);
        const long long before = DumpNumbers()[0];
        for (void*& p : made) p = aNew(size);
        L(DumpNumbers()[0] - before);
        for (void* p : made) aDel(p);
        for (void* p : drain) aDel(p);
    }
}

// ---- PackedHeap / PackedHeapMP

struct Heap {
    alignas(8) unsigned char raw[0x2c];
    bool mp;
};

static std::vector<unsigned char*> blocks;  // by creation order (found through VirtualQuery)
static int BlockOf(const void* p, unsigned* offset)
{
    MEMORY_BASIC_INFORMATION mi;
    VirtualQuery(p, &mi, sizeof mi);
    auto* b = static_cast<unsigned char*>(mi.AllocationBase);
    *offset = static_cast<unsigned>(static_cast<const unsigned char*>(p) - b);
    for (size_t i = 0; i < blocks.size(); ++i)
        if (blocks[i] == b) return static_cast<int>(i);
    blocks.push_back(b);
    return static_cast<int>(blocks.size() - 1);
}
static void DumpBlocks(int line)
{
    for (unsigned char* b : blocks) {
        MEMORY_BASIC_INFORMATION mi;
        VirtualQuery(b, &mi, sizeof mi);
        Log(mi.State == MEM_COMMIT, line);
        if (mi.State != MEM_COMMIT) continue;
        const unsigned* h = reinterpret_cast<const unsigned*>(b);
        Log(h[0], line);  // allocations in use
        Log(h[1], line);  // bytes handed out
        int n = 0;
        for (const unsigned* c = reinterpret_cast<const unsigned*>(h[2]); c && n < 100000; c = reinterpret_cast<const unsigned*>(c[2]))
            ++n;
        Log(n, line);
    }
}

using HeapCtor = void*(__fastcall*)(void*, void*, const char*);
using HeapDtor = void(__fastcall*)(void*, void*);
using HeapAlloc_ = unsigned char*(__fastcall*)(void*, void*, unsigned);
using HeapFree_ = void(__fastcall*)(void*, void*, unsigned char*);
using HeapEmpty = bool(__fastcall*)(const void*, void*);
using HeapPrint = void(__fastcall*)(void*, void*);

static void HeapScenario(const std::vector<Step>& script, bool mp)
{
    const char* const ctorName = mp ? "??0PackedHeapMP@Jet@@QAE@PBD@Z" : "??0PackedHeap@Jet@@QAE@PBD@Z";
    const char* const dtorName = mp ? "??1PackedHeapMP@Jet@@QAE@XZ" : "??1PackedHeap@Jet@@QAE@XZ";
    const auto hAlloc = G<HeapAlloc_>(mp ? "?Alloc@PackedHeapMP@Jet@@QAEPAEI@Z" : "?Alloc@PackedHeap@Jet@@QAEPAEI@Z");
    const auto hFree = G<HeapFree_>(mp ? "?Free@PackedHeapMP@Jet@@QAEXPAE@Z" : "?Free@PackedHeap@Jet@@QAEXPAE@Z");
    const auto hEmpty = G<HeapEmpty>(mp ? "?IsEmpty@PackedHeapMP@Jet@@QBE_NXZ" : "?IsEmpty@PackedHeap@Jet@@QBE_NXZ");
    Heap h{};
    G<HeapCtor>(ctorName)(h.raw, nullptr, "diff heap");
    blocks.clear();
    std::vector<unsigned char*> live(200, nullptr);
    std::vector<unsigned> size(200, 0);
    for (const Step& st : script) {
        unsigned char*& p = live[st.obj];
        L(st.op);
        switch (st.op) {
        case 0:
        case 1:
        case 2: {
            if (p) {
                hFree(h.raw, nullptr, p);
                p = nullptr;
            }
            unsigned n;
            const unsigned k = st.a % 10;
            if (k < 6) n = st.b % 0xa8;
            else if (k < 9) n = st.b % 0x2000;
            else n = st.b % 0x30000;  // a few blocks' worth of big ones
            if (st.c % 40 == 0) n = 0;
            p = hAlloc(h.raw, nullptr, n);
            size[st.obj] = n;
            unsigned off;
            L(BlockOf(p, &off));
            L(off);
            L(reinterpret_cast<unsigned*>(p)[-1]);
            std::memset(p, 0x30 + st.obj % 50, n < 0x10 ? 0x10 : n);
            break;
        }
        case 3:
        case 4:
            if (p) {
                bool ok = true;
                for (unsigned i = 0; i < size[st.obj]; ++i) ok &= p[i] == 0x30 + st.obj % 50;
                L(ok);
                hFree(h.raw, nullptr, p);
                p = nullptr;
            }
            break;
        case 5: {  // pointers that are not allocations: misaligned, below every block, the heap object itself
            // (one past a block could land on another heap's allocation: a double free in both modules)
            if (!p) break;
            static unsigned char outside[64];
            unsigned char* bad[3] = {p + 1 + st.b % 3, outside + 16, reinterpret_cast<unsigned char*>(&h)};
            hFree(h.raw, nullptr, bad[st.a % 3]);
            hFree(h.raw, nullptr, nullptr);
            break;
        }
        case 6:
            L(hEmpty(h.raw, nullptr));
            L(reinterpret_cast<unsigned*>(h.raw)[7]);  // blocks with nothing in use
            DumpBlocks(__LINE__);
            break;
        case 7:
            if (!mp) G<HeapPrint>("?DebugPrint@PackedHeap@Jet@@QAEXXZ")(h.raw, nullptr);
            else G<void(__cdecl*)()>("?DebugPrintAll@PackedHeap@Jet@@SAXXZ")();
            break;
        }
    }
    DumpBlocks(__LINE__);
    rngState = 99;  // the same frees on both sides
    for (unsigned char*& p : live)
        if (p && Rnd(2)) hFree(h.raw, nullptr, p), p = nullptr;
    DumpBlocks(__LINE__);
    L(hEmpty(h.raw, nullptr));
    G<HeapDtor>(dtorName)(h.raw, nullptr);  // blocks still in use stay (leaked) as in the original
    DumpBlocks(__LINE__);
}

// copies: the name, the unused count and the same blocks (shared with the source)
static void HeapCopyScenario()
{
    unsigned char a[0x20], b[0x20], c[0x20];
    G<HeapCtor>("??0PackedHeap@Jet@@QAE@PBD@Z")(a, nullptr, "first");
    const auto hAlloc = G<HeapAlloc_>("?Alloc@PackedHeap@Jet@@QAEPAEI@Z");
    unsigned char* p = hAlloc(a, nullptr, 40);
    unsigned char* q = hAlloc(a, nullptr, 60);
    G<HeapFree_>("?Free@PackedHeap@Jet@@QAEXPAE@Z")(a, nullptr, q);
    G<void*(__fastcall*)(void*, void*, const void*)>("??0PackedHeap@Jet@@QAE@ABV01@@Z")(b, nullptr, a);
    G<HeapCtor>("??0PackedHeap@Jet@@QAE@PBD@Z")(c, nullptr, "third");
    G<void*(__fastcall*)(void*, void*, const void*)>("??4PackedHeap@Jet@@QAEAAV01@ABV01@@Z")(c, nullptr, a);
    for (unsigned char* h : {a, b, c}) {
        L(std::strcmp(*reinterpret_cast<const char**>(h), "first") == 0);
        L(reinterpret_cast<unsigned*>(h)[7]);
        L(G<HeapEmpty>("?IsEmpty@PackedHeap@Jet@@QBE_NXZ")(h, nullptr));
    }
    unsigned char* r = hAlloc(b, nullptr, 60);
    unsigned off;
    blocks.clear();
    L(BlockOf(p, &off));
    L(BlockOf(r, &off));
    G<HeapFree_>("?Free@PackedHeap@Jet@@QAEXPAE@Z")(a, nullptr, p);
    G<HeapFree_>("?Free@PackedHeap@Jet@@QAEXPAE@Z")(b, nullptr, r);
    L(G<HeapEmpty>("?IsEmpty@PackedHeap@Jet@@QBE_NXZ")(a, nullptr));
    // the copies share a's blocks: destroying more than one frees them twice (in both modules), so b and c leak
    G<HeapDtor>("??1PackedHeap@Jet@@QAE@XZ")(a, nullptr);
}

// ---- PoolBlockAlloc and ConfigData's pool

using PoolCtor = void*(__fastcall*)(void*, void*, unsigned, unsigned, unsigned);
using PoolAlloc = void*(__cdecl*)(void*);
using PoolFree = void(__cdecl*)(void*, void*);

static std::vector<void*> poolBlocks;
static int PoolBlockOf(void* item)
{
    void* b = static_cast<void**>(item)[-1];
    for (size_t i = 0; i < poolBlocks.size(); ++i)
        if (poolBlocks[i] == b) return static_cast<int>(i);
    poolBlocks.push_back(b);
    return static_cast<int>(poolBlocks.size() - 1);
}
static int Rank(void* b)
{
    if (!b) return -1;
    for (size_t i = 0; i < poolBlocks.size(); ++i)
        if (poolBlocks[i] == b) return static_cast<int>(i);
    return -2;
}

static void PoolScenario(const std::vector<Step>& script, unsigned itemSize, unsigned count, unsigned align)
{
    alignas(8) unsigned char pool[0x30];
    G<PoolCtor>("??0PoolBlockAlloc@Jet@@QAE@III@Z")(pool, nullptr, itemSize, count, align);
    const int* f = reinterpret_cast<const int*>(pool);
    for (int i = 2; i <= 6; ++i) L(f[i]);  // align, item size, count, block bytes, free items
    const auto pAlloc = G<PoolAlloc>("?Alloc@PoolBlockAlloc@Jet@@QAAPAXXZ");
    const auto pFree = G<PoolFree>("?Free@PoolBlockAlloc@Jet@@QAAXPAX@Z");
    poolBlocks.clear();
    std::vector<void*> live(300, nullptr);
    for (const Step& st : script) {
        void*& p = live[st.obj % 300];
        L(st.op);
        if (st.op < 3) {
            if (p) pFree(pool, p), p = nullptr;
            p = pAlloc(pool);
            const int b = PoolBlockOf(p);
            L(b);
            const uintptr_t first = reinterpret_cast<uintptr_t>(poolBlocks[b]) + 0x18;
            L((reinterpret_cast<uintptr_t>(p) - first) / static_cast<unsigned>(f[3]));
            L((reinterpret_cast<uintptr_t>(p) - first) % static_cast<unsigned>(f[3]));
            L(reinterpret_cast<uintptr_t>(p) & (f[2] - 1));
            std::memset(p, 0x77, itemSize);
        } else if (st.op < 5) {
            if (p) pFree(pool, p), p = nullptr;
            pFree(pool, nullptr);
        } else if (st.op == 5) {
            G<void(__cdecl*)(void*)>("?ReleaseMemory@PoolBlockAlloc@Jet@@QAAXXZ")(pool);
            for (void*& q : live) q = nullptr;
            poolBlocks.clear();
        }
        L(f[6]);
        L(f[8]);
        L(Rank(reinterpret_cast<void* const*>(pool)[7]));
        L(reinterpret_cast<void* const*>(pool)[0] == nullptr);
        L(reinterpret_cast<void* const*>(pool)[1] == nullptr);
    }
    G<HeapDtor>("??1PoolBlockAlloc@Jet@@QAE@XZ")(pool, nullptr);
}

static void ConfigPoolScenario(const std::vector<Step>& script)
{
    const auto cNew = G<void*(__cdecl*)(size_t)>("??2ConfigData@Jet@@SAPAXI@Z");
    const auto cDel = G<void(__cdecl*)(void*)>("??3ConfigData@Jet@@SAXPAX@Z");
    const auto cNewAt = G<void*(__cdecl*)(size_t, void*)>("??2ConfigData@Jet@@SAPAXIPAX@Z");
    const auto cDelAt = G<void(__cdecl*)(void*, void*)>("??3ConfigData@Jet@@SAXPAX0@Z");
    poolBlocks.clear();
    std::vector<void*> live(700, nullptr);
    char at[16];
    L(cNewAt(16, at) == at);
    for (const Step& st : script) {
        void*& p = live[st.obj % 700];
        if (p) {
            if (st.c & 1) cDel(p); else cDelAt(p, at);
            p = nullptr;
        }
        if (st.op & 1) {
            p = cNew(16);
            L(PoolBlockOf(p));
        }
    }
    for (void*& p : live)
        if (p) cDel(p), p = nullptr;
    cDel(nullptr);
}

// ---- ReferenceCount: a stand-in object {vptr, count} with our own slots

static std::vector<int> calls;
struct RefObj {
    void** vptr;
    long count;
};
static void __cdecl SlotDeleteThis(const RefObj* o)  // slot 0, __cdecl with this on the stack
{
    calls.push_back(100 + static_cast<int>(o->count));
    // the original's DeleteThis would call slot 1; ours is what is tested through the export below
}
static void* __fastcall SlotDtor(RefObj* o, void*, int flag)  // slot 1, the deleting destructor
{
    calls.push_back(200 + flag);
    return o;
}
static void* refTable[2] = {reinterpret_cast<void*>(&SlotDeleteThis), reinterpret_cast<void*>(&SlotDtor)};

static void RefScenario(const std::vector<Step>& script)
{
    RefObj o{refTable, 1};
    const auto add = G<void(__fastcall*)(const RefObj*, void*)>("?AddReference@ReferenceCount@Jet@@QBEXXZ");
    const auto rel = G<bool(__fastcall*)(const RefObj*, void*)>("?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ");
    const auto del = G<void(__cdecl*)(const RefObj*)>("?DeleteThis@ReferenceCount@Jet@@MBAXXZ");
    for (const Step& st : script) {
        calls.clear();
        if (st.op % 3 == 0) add(&o, nullptr);
        else if (st.op % 3 == 1) L(rel(&o, nullptr));
        else del(&o);
        L(o.count);
        L(o.vptr == refTable);
        for (int c : calls) L(c);
        if (o.count <= 0) o.count = 1 + st.a % 3;
    }
    // the destructor puts the module's own vftable back
    G<void(__fastcall*)(RefObj*, void*)>("??1ReferenceCount@Jet@@MAE@XZ")(&o, nullptr);
    MEMORY_BASIC_INFORMATION mi;
    VirtualQuery(o.vptr, &mi, sizeof mi);
    L(mi.AllocationBase == cur);
}

// ---- threads (each side alone: no comparison, only crashes and corrupted blocks)

static unsigned char sharedHeap[0x2c];
static long threadErrors;
static DWORD WINAPI Stress(void* arg)
{
    const auto aNew = G<New>("?New@Alloc@Jet@@SAPAXI@Z");
    const auto aDel = G<Del>("?Delete@Alloc@Jet@@SAXPAX@Z");
    const auto hAlloc = G<HeapAlloc_>("?Alloc@PackedHeapMP@Jet@@QAEPAEI@Z");
    const auto hFree = G<HeapFree_>("?Free@PackedHeapMP@Jet@@QAEXPAE@Z");
    unsigned seed = static_cast<unsigned>(reinterpret_cast<uintptr_t>(arg));
    void* a[32] = {};
    unsigned char* h[32] = {};
    unsigned char tag[32] = {};
    for (int i = 0; i < 200000; ++i) {
        seed = seed * 1103515245u + 12345u;
        const unsigned k = (seed >> 8) % 32, n = (seed >> 13) % 0x500;
        const unsigned char v = static_cast<unsigned char>(seed >> 24);
        if (a[k]) {
            if (*static_cast<unsigned char*>(a[k]) != tag[k]) InterlockedIncrement(&threadErrors);
            aDel(a[k]);
        }
        a[k] = aNew(n + 1);
        *static_cast<unsigned char*>(a[k]) = v;
        if (h[k]) {
            if (*h[k] != static_cast<unsigned char>(tag[k] ^ 0x5a)) InterlockedIncrement(&threadErrors);
            hFree(sharedHeap, nullptr, h[k]);
        }
        h[k] = hAlloc(sharedHeap, nullptr, n % 0x100 + 1);
        *h[k] = static_cast<unsigned char>(v ^ 0x5a);
        tag[k] = v;
    }
    for (int k = 0; k < 32; ++k) {
        if (a[k]) aDel(a[k]);
        if (h[k]) hFree(sharedHeap, nullptr, h[k]);
    }
    return 0;
}
static bool StressSide(HMODULE m, const char* tag)
{
    cur = m;
    threadErrors = 0;
    G<HeapCtor>("??0PackedHeapMP@Jet@@QAE@PBD@Z")(sharedHeap, nullptr, "stress");
    HANDLE t[6];
    for (int i = 0; i < 6; ++i) t[i] = CreateThread(nullptr, 0, Stress, reinterpret_cast<void*>(static_cast<uintptr_t>(i * 7919 + 1)), 0, nullptr);
    WaitForMultipleObjects(6, t, TRUE, INFINITE);
    for (HANDLE x : t) CloseHandle(x);
    const bool empty = G<HeapEmpty>("?IsEmpty@PackedHeapMP@Jet@@QBE_NXZ")(sharedHeap, nullptr);
    G<HeapDtor>("??1PackedHeapMP@Jet@@QAE@XZ")(sharedHeap, nullptr);
    std::printf("threads (%s): %ld corrupted, heap %s\n", tag, threadErrors, empty ? "empty" : "NOT EMPTY");
    return threadErrors == 0 && empty;
}

// ----

template <class F>
static size_t Compare(const char* name, F f)
{
    std::printf("%s ...\n", name);
    logv.clear();
    lines.clear();
    cur = orig;
    f();
    const std::vector<long long> a = logv;
    const std::vector<int> la = lines;
    logv.clear();
    lines.clear();
    cur = ours;
    f();
    size_t bad = 0;
    for (size_t i = 0; i < a.size() || i < logv.size(); ++i) {
        const long long x = i < a.size() ? a[i] : -999, y = i < logv.size() ? logv[i] : -999;
        if (x != y && ++bad <= 10) {
            std::printf("  %s step %zu (line %d): orig %lld ours %lld\n", name, i, i < la.size() ? la[i] : 0, x, y);
            if (getenv("KS_TRACE"))
                for (size_t k = i > 30 ? i - 30 : 0; k <= i && k < la.size() && k < logv.size(); ++k)
                    std::printf("    [%zu] line %d: %lld / %lld\n", k, la[k], a[k], logv[k]);
        }
    }
    std::printf("%s: %zu results each, %zu mismatches\n", name, a.size(), bad);
    return bad;
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    const int steps = argc > 2 ? std::atoi(argv[2]) : 40000;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e) -> LONG {
        std::printf("crash %08lx at %p after line %d\n", e->ExceptionRecord->ExceptionCode,
                    e->ExceptionRecord->ExceptionAddress, lines.empty() ? 0 : lines.back());
        ExitProcess(3);
    });
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) return 2;
    PatchDebugOutput(ours);
    PatchDebugOutput(orig);
    size_t bad = 0;
    const auto alloc = MakeScript(5, 64, steps, 3);
    bad += Compare("Alloc", [&] { AllocScenario(alloc); });
    bad += Compare("Alloc free lists", [&] { FreeListScenario(); });
    const auto heap = MakeScript(8, 200, steps, 5);
    bad += Compare("PackedHeap", [&] { HeapScenario(heap, false); });
    bad += Compare("PackedHeapMP", [&] { HeapScenario(heap, true); });
    bad += Compare("PackedHeap copies", [&] { HeapCopyScenario(); });
    const auto pool = MakeScript(6, 300, steps / 2, 9);
    bad += Compare("PoolBlockAlloc 0x10/0x200/4", [&] { PoolScenario(pool, 0x10, 0x200, 4); });
    bad += Compare("PoolBlockAlloc 0x33/7/16", [&] { PoolScenario(pool, 0x33, 7, 16); });
    bad += Compare("PoolBlockAlloc 5/1/3", [&] { PoolScenario(pool, 5, 1, 3); });
    bad += Compare("PoolBlockAlloc 0x100/64/0", [&] { PoolScenario(pool, 0x100, 64, 0); });
    bad += Compare("ConfigData pool", [&] { ConfigPoolScenario(MakeScript(2, 700, steps, 13)); });
    bad += Compare("ReferenceCount", [&] { RefScenario(MakeScript(3, 1, 3000, 17)); });
    const bool so = StressSide(orig, "orig"), su = StressSide(ours, "ours");
    if (!su) ++bad;
    if (!so) std::printf("(the original failed its own thread stress)\n");
    std::printf("total mismatches: %zu\n", bad);
    return bad ? 1 : 0;
}
