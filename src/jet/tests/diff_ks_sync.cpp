// Differential test of KernelStandard unit U4 (SyncObjects.cpp): one scripted scenario is run once on objects made
// by our KernelStandard_7c.dll and once on objects made by KernelStandard_7c_orig.dll (both loaded in this
// process, called through raw exports); every observable result goes into a log and the two logs must match.
// Handles differ in value, so only their null-ness is logged.
//
// usage: diff_ks_sync.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static HMODULE ours, orig;

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

// thiscall emulated with __fastcall(this, edx, args...); __cdecl members take this on the stack
using Ctor0 = void*(__fastcall*)(void*, void*);
using CtorBS = void*(__fastcall*)(void*, void*, bool, const char*);
using CtorBBS = void*(__fastcall*)(void*, void*, bool, bool, const char*);
using CtorIIS = void*(__fastcall*)(void*, void*, int, int, const char*);
using CtorS = void*(__fastcall*)(void*, void*, const char*);
using CtorLock = void*(__fastcall*)(void*, void*, const void*, bool);
using CtorMulti = void*(__fastcall*)(void*, void*, void**, unsigned long, bool);
using Dtor = void(__fastcall*)(void*, void*);
using LockT = bool(__cdecl*)(const void*, unsigned long);
using Unlock0 = bool(__cdecl*)(const void*);
using UnlockN = bool(__cdecl*)(const void*, unsigned long);
using Handle = void*(__cdecl*)(void*);
using Act = void(__cdecl*)(void*);
using MultiLockT = int(__cdecl*)(const void*, unsigned long, bool);
using MultiIs = bool(__cdecl*)(const void*, unsigned long);
using Swmr = bool(__cdecl*)(void*, unsigned long);
using SwmrDone = void(__fastcall*)(void*, void*);
using Deleting = void*(__fastcall*)(void*, void*, unsigned int);
using NewT = void*(__cdecl*)(unsigned int);

struct Side {
    HMODULE m;
    const char* tag;
    template <class F>
    F f(const char* name) const { return Get<F>(m, name); }
};

static std::vector<long long> logv;
static std::vector<int> lines;
static void Log(long long v, int line)
{
    logv.push_back(v);
    lines.push_back(line);
}
#define L(v) Log((v), __LINE__)

// vftable slot n of an object
template <class F>
static F Slot(void* obj, int n) { return reinterpret_cast<F>((*static_cast<void***>(obj))[n]); }

static DWORD WINAPI HoldMutex(void* arg)  // takes the mutex, signals, waits for the release request, exits
{
    auto* p = static_cast<HANDLE*>(arg);
    WaitForSingleObject(p[0], INFINITE);
    SetEvent(p[1]);
    WaitForSingleObject(p[2], INFINITE);
    if (p[3]) ReleaseMutex(p[0]);  // p[3] == 0: exit while owning it (abandoned)
    return 0;
}

static void Scenario(const Side& s)
{
    logv.clear();
    lines.clear();
    const auto mutexCtor = s.f<CtorBS>("??0Mutex@Jet@@QAE@_NPBD@Z");
    const auto semCtor = s.f<CtorIIS>("??0Semaphore@Jet@@QAE@HHPBD@Z");
    const auto evCtor = s.f<CtorBBS>("??0WEvent@Jet@@QAE@_N0PBD@Z");
    const auto csCtor = s.f<CtorS>("??0CriticalSection@Jet@@QAE@PBD@Z");
    const auto lock = s.f<LockT>("?Lock@SyncObject@Jet@@UBA_NK@Z");
    const auto isLocked = s.f<Unlock0>("?IsLocked@SyncObject@Jet@@QAA_NXZ");
    const auto signal = s.f<Unlock0>("?Signal@SyncObject@Jet@@QAA_NXZ");
    const auto wait = s.f<LockT>("?Wait@SyncObject@Jet@@QBA_NK@Z");
    const auto handle = s.f<Handle>("??BSyncObject@Jet@@UAAPAXXZ");
    const auto vtSync = s.f<void*>("??_7SyncObject@Jet@@6B@");
    const NewT alloc = Get<NewT>(orig, "?New@Alloc@Jet@@SAPAXI@Z");

    // construction: vftables, handles, the default constructor closures
    struct {
        const char* closure;
        const char* vft;
        size_t size;
    } kinds[] = {{"??_FMutex@Jet@@QAEXXZ", "??_7Mutex@Jet@@6B@", 8},
                 {"??_FSemaphore@Jet@@QAEXXZ", "??_7Semaphore@Jet@@6B@", 8},
                 {"??_FWEvent@Jet@@QAEXXZ", "??_7WEvent@Jet@@6B@", 8},
                 {"??_FCriticalSection@Jet@@QAEXXZ", "??_7CriticalSection@Jet@@6B@", 12}};
    for (const auto& k : kinds) {
        uint32_t o[3] = {0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd};
        s.f<Ctor0>(k.closure)(o, nullptr);
        L(o[0] == reinterpret_cast<uintptr_t>(s.f<void*>(k.vft)));
        L(o[1] != 0);
        L(k.size == 12 ? (o[1] == 0) * 2 + (o[2] != 0) : o[2] == 0xcdcdcdcd);
        L(Slot<Unlock0>(o, 4)(o));  // Unlock() of a fresh object
        L(Slot<UnlockN>(o, 3)(o, 2));
        L(isLocked(o));
        L(Slot<LockT>(o, 2)(o, 0));
        L(Slot<Unlock0>(o, 4)(o));
        void* h = Slot<Handle>(o, 1)(o);
        L(h == reinterpret_cast<void*>(static_cast<uintptr_t>(o[1])));
        Slot<Deleting>(o, 0)(o, nullptr, 0);  // destructor only
        L(o[0] == reinterpret_cast<uintptr_t>(k.size == 12 ? vtSync : vtSync));
        L(o[1] == 0);
        // the scalar deleting destructor frees through Jet::Alloc
        void* m = alloc(static_cast<unsigned int>(k.size));
        s.f<Ctor0>(k.closure)(m, nullptr);
        L(Slot<Deleting>(m, 0)(m, nullptr, 1) == m);
    }

    // mutex: recursion, unlock past the count, held by another thread, abandoned
    {
        uint32_t mx[2];
        mutexCtor(mx, nullptr, true, nullptr);
        for (int i = 0; i < 3; ++i) L(lock(mx, 0));
        for (int i = 0; i < 6; ++i) L(Slot<Unlock0>(mx, 4)(mx));
        L(isLocked(mx));
        L(wait(mx, 0));
        L(signal(mx));
        L(Slot<UnlockN>(mx, 3)(mx, 5));
        for (int abandon = 0; abandon < 2; ++abandon) {
            HANDLE p[4] = {handle(mx), CreateEventA(nullptr, FALSE, FALSE, nullptr), CreateEventA(nullptr, FALSE, FALSE, nullptr),
                           reinterpret_cast<HANDLE>(static_cast<uintptr_t>(!abandon))};
            HANDLE t = CreateThread(nullptr, 0, HoldMutex, p, 0, nullptr);
            WaitForSingleObject(p[1], INFINITE);
            L(lock(mx, 0));
            L(isLocked(mx));
            L(Slot<Unlock0>(mx, 4)(mx));
            SetEvent(p[2]);
            WaitForSingleObject(t, INFINITE);
            L(lock(mx, 100));  // after a release: true; abandoned: WAIT_ABANDONED, so false
            L(Slot<Unlock0>(mx, 4)(mx));
            CloseHandle(t);
            CloseHandle(p[1]);
            CloseHandle(p[2]);
        }
        Slot<Deleting>(mx, 0)(mx, nullptr, 0);
        uint32_t named[2], same[2];
        mutexCtor(named, nullptr, false, s.tag);
        mutexCtor(same, nullptr, false, s.tag);
        L(named[1] != 0 && same[1] != 0);
        L(lock(named, 0));
        L(lock(same, 0));  // the same kernel object, owned by this thread
        L(Slot<Unlock0>(same, 4)(same));
        L(Slot<Unlock0>(named, 4)(named));
        Slot<Deleting>(named, 0)(named, nullptr, 0);
        Slot<Deleting>(same, 0)(same, nullptr, 0);
    }

    // semaphore: counts and the maximum
    for (int init = 0; init < 3; ++init)
        for (int max = 1; max < 4; ++max) {
            uint32_t sm[2];
            semCtor(sm, nullptr, init, max, nullptr);
            L(sm[1] != 0);
            for (int i = 0; i < 4; ++i) L(lock(sm, 0));
            L(Slot<UnlockN>(sm, 3)(sm, 2));
            L(Slot<Unlock0>(sm, 4)(sm));
            L(Slot<UnlockN>(sm, 3)(sm, 5));
            for (int i = 0; i < 4; ++i) L(lock(sm, 0));
            L(isLocked(sm));
            Slot<Deleting>(sm, 0)(sm, nullptr, 0);
        }

    // events
    const auto set = s.f<Act>("?Set@WEvent@Jet@@QAAXXZ");
    const auto pulse = s.f<Act>("?Pulse@WEvent@Jet@@QAAXXZ");
    const auto reset = s.f<Act>("?Reset@WEvent@Jet@@QAAXXZ");
    for (int manual = 0; manual < 2; ++manual)
        for (int initial = 0; initial < 2; ++initial) {
            uint32_t ev[2];
            evCtor(ev, nullptr, manual != 0, initial != 0, nullptr);
            L(lock(ev, 0));
            L(lock(ev, 0));
            set(ev);
            L(lock(ev, 0));
            L(lock(ev, 0));
            reset(ev);
            L(lock(ev, 0));
            pulse(ev);
            L(lock(ev, 0));
            set(ev);
            L(isLocked(ev));
            L(lock(ev, 0));
            L(Slot<Unlock0>(ev, 4)(ev));
            L(Slot<UnlockN>(ev, 3)(ev, 1));
            Slot<Deleting>(ev, 0)(ev, nullptr, 0);
        }

    // critical section
    {
        uint32_t cs[3];
        csCtor(cs, nullptr, "ignored");
        L(cs[1] == 0 && cs[2] != 0);
        L(Slot<LockT>(cs, 2)(cs, 0));
        L(Slot<LockT>(cs, 2)(cs, INFINITE));
        L(Slot<Unlock0>(cs, 4)(cs));
        L(Slot<Unlock0>(cs, 4)(cs));
        L(Slot<UnlockN>(cs, 3)(cs, 1));
        L(isLocked(cs));
        Slot<Deleting>(cs, 0)(cs, nullptr, 0);
        L(cs[2] == 0);
        uint32_t empty[3] = {cs[0], 0, 0};  // a section pointer of 0: Lock and Unlock still succeed
        L(Slot<LockT>(cs, 2)(empty, 0));
        L(Slot<Unlock0>(cs, 4)(empty));
    }

    // SingleLock
    {
        const auto ctor = s.f<CtorLock>("??0SingleLock@Jet@@QAE@PBVSyncObject@1@_N@Z");
        const auto dtor = s.f<Dtor>("??1SingleLock@Jet@@QAE@XZ");
        const auto slLock = s.f<LockT>("?Lock@SingleLock@Jet@@QBA_NK@Z");
        const auto slUnlock = s.f<Unlock0>("?Unlock@SingleLock@Jet@@QBA_NXZ");
        const auto slIs = s.f<Unlock0>("?IsLocked@SingleLock@Jet@@QBA_NXZ");
        const auto assign = s.f<CtorS>("??4SingleLock@Jet@@QAEAAV01@ABV01@@Z");
        uint32_t sm[2];
        semCtor(sm, nullptr, 1, 1, nullptr);
        uint32_t a[2], b[2];
        ctor(a, nullptr, sm, true);
        L(static_cast<uint8_t>(a[1]));
        ctor(b, nullptr, sm, false);  // the semaphore is taken: lockNow would block for good
        L(slLock(b, 0));
        dtor(b, nullptr);
        (void)b;
        L(slIs(a));
        L(slLock(a, 0));
        L(slUnlock(a));
        L(slIs(a));
        L(slUnlock(a));
        L(slLock(a, 0));
        L(slLock(a, 0));
        uint32_t c[2] = {0, 0};
        L(assign(c, nullptr, reinterpret_cast<const char*>(a)) == reinterpret_cast<void*>(c) && std::memcmp(a, c, 8) == 0);
        dtor(a, nullptr);
        L(static_cast<uint8_t>(a[1]));
        L(lock(sm, 0));  // released by the destructor
        Slot<Deleting>(sm, 0)(sm, nullptr, 0);
    }

    // MultiLock
    {
        const auto ctor = s.f<CtorMulti>("??0MultiLock@Jet@@QAE@PAPAVSyncObject@1@K_N@Z");
        const auto dtor = s.f<Dtor>("??1MultiLock@Jet@@QAE@XZ");
        const auto mlLock = s.f<MultiLockT>("?Lock@MultiLock@Jet@@QBA?AW4Result@12@K_N@Z");
        const auto mlUnlock = s.f<Unlock0>("?Unlock@MultiLock@Jet@@QBA_NXZ");
        const auto mlUnlockN = s.f<UnlockN>("?Unlock@MultiLock@Jet@@QBA_NK@Z");
        const auto mlIs = s.f<MultiIs>("?IsLocked@MultiLock@Jet@@QBA_NK@Z");
        uint32_t e1[2], e2[2], sm[2], mx[2];
        evCtor(e1, nullptr, true, false, nullptr);
        evCtor(e2, nullptr, false, true, nullptr);
        semCtor(sm, nullptr, 0, 2, nullptr);
        mutexCtor(mx, nullptr, false, nullptr);
        void* objs[4] = {e1, e2, sm, mx};
        for (int now = 0; now < 2; ++now) {
            uint32_t ml[4];
            ctor(ml, nullptr, objs, 4, now != 0);
            for (unsigned long i = 0; i < 4; ++i) L(mlIs(ml, i));
            L(mlLock(ml, 0, false));
            L(mlLock(ml, 0, true));
            set(e1);
            Slot<UnlockN>(sm, 3)(sm, 1);
            L(mlLock(ml, 0, false));
            for (unsigned long i = 0; i < 4; ++i) L(mlIs(ml, i));
            L(mlLock(ml, 0, true));
            set(e2);
            L(mlLock(ml, 0, true));
            for (unsigned long i = 0; i < 4; ++i) L(mlIs(ml, i));
            L(mlUnlockN(ml, 1));
            L(mlUnlock(ml));
            for (unsigned long i = 0; i < 4; ++i) L(mlIs(ml, i));
            reset(e1);
            L(mlLock(ml, 10, true));
            dtor(ml, nullptr);
            L(ml[0] == 0 && ml[1] == 0);
        }
        // abandoned mutex through MultiLock (a fresh one: this thread may own mx several times over now)
        uint32_t mx2[2];
        mutexCtor(mx2, nullptr, false, nullptr);
        HANDLE p[4] = {handle(mx2), CreateEventA(nullptr, FALSE, FALSE, nullptr), CreateEventA(nullptr, FALSE, FALSE, nullptr), nullptr};
        HANDLE t = CreateThread(nullptr, 0, HoldMutex, p, 0, nullptr);
        WaitForSingleObject(p[1], INFINITE);
        SetEvent(p[2]);
        WaitForSingleObject(t, INFINITE);
        uint32_t ml[4];
        reset(e1);
        void* objs2[2] = {e1, mx2};
        ctor(ml, nullptr, objs2, 2, false);
        L(mlLock(ml, 0, false));  // 0x20000000 + 1: abandoned
        L(mlIs(ml, 1));
        dtor(ml, nullptr);
        CloseHandle(t);
        CloseHandle(p[1]);
        CloseHandle(p[2]);
        for (void* o : objs) Slot<Deleting>(o, 0)(o, nullptr, 0);
        Slot<Deleting>(mx2, 0)(mx2, nullptr, 0);
    }

    // SWMRGuard: a reader on another thread keeps writers out
    {
        const auto ctor = s.f<Ctor0>("??0SWMRGuard@Jet@@QAE@XZ");
        const auto dtor = s.f<Dtor>("??1SWMRGuard@Jet@@QAE@XZ");
        const auto toRead = s.f<Swmr>("?WaitToRead@SWMRGuard@Jet@@QAA_NK@Z");
        const auto toWrite = s.f<Swmr>("?WaitToWrite@SWMRGuard@Jet@@QAA_NK@Z");
        const auto doneRead = s.f<SwmrDone>("?DoneReading@SWMRGuard@Jet@@QAEXXZ");
        const auto doneWrite = s.f<SwmrDone>("?DoneWriting@SWMRGuard@Jet@@QAEXXZ");
        uint32_t g[8];
        ctor(g, nullptr);
        L(g[4] == reinterpret_cast<uintptr_t>(g + 2) && g[5] == reinterpret_cast<uintptr_t>(g) && g[6] != 0 && g[7] == 0);
        L(toRead(g, 0));
        L(toRead(g, 0));
        L(g[7]);
        L(toWrite(g, 0));  // a reader is in: the event is reset
        doneRead(g, nullptr);
        L(toWrite(g, 0));
        doneRead(g, nullptr);
        L(g[7]);
        L(toWrite(g, 0));
        doneWrite(g, nullptr);
        doneRead(g, nullptr);  // no readers left: stays 0
        L(g[7]);
        struct Ctx {
            uint32_t* g;
            Swmr toRead;
            SwmrDone doneRead;
            HANDLE in, out;
        } ctx{g, toRead, doneRead, CreateEventA(nullptr, FALSE, FALSE, nullptr), CreateEventA(nullptr, FALSE, FALSE, nullptr)};
        HANDLE t = CreateThread(
            nullptr, 0,
            [](void* a) -> DWORD {
                auto* c = static_cast<Ctx*>(a);
                c->toRead(c->g, INFINITE);
                SetEvent(c->in);
                WaitForSingleObject(c->out, INFINITE);
                c->doneRead(c->g, nullptr);
                return 0;
            },
            &ctx, 0, nullptr);
        WaitForSingleObject(ctx.in, INFINITE);
        L(toWrite(g, 0));
        L(toRead(g, 0));
        doneRead(g, nullptr);
        SetEvent(ctx.out);
        WaitForSingleObject(t, INFINITE);
        L(toWrite(g, 200));
        doneWrite(g, nullptr);
        dtor(g, nullptr);
        CloseHandle(t);
        CloseHandle(ctx.in);
        CloseHandle(ctx.out);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) return 2;
    Scenario({orig, "Local\\JetSyncProbeOrig"});
    const std::vector<long long> a = logv;
    Scenario({ours, "Local\\JetSyncProbeOurs"});
    const std::vector<long long>& b = logv;
    size_t bad = 0;
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const long long x = i < a.size() ? a[i] : -999, y = i < b.size() ? b[i] : -999;
        if (x != y && ++bad <= 10)
            std::printf("  step %zu (line %d): orig %lld ours %lld\n", i, i < lines.size() ? lines[i] : 0, x, y);
    }
    std::printf("sync scenario: %zu results each, %zu mismatches\n", a.size(), bad);
    return bad ? 1 : 0;
}
