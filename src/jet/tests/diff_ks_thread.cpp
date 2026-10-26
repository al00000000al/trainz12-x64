// Differential test of KernelStandard unit U13 (Thread.cpp, Time.cpp): one scripted scenario is run once through
// KernelStandard_7c_orig.dll's exports and once through ours; every observable result goes into a log and the two
// logs must match. Since U14b each module has its own statics (frame lock, mainThreadId, the TLS singleton and
// its indices) and its own heap and PString cache: the helpers use the side's module. The scenario still saves and
// restores those statics and uses its own TLS names on each side. Values that differ by nature (handles, TLS slot numbers, clocks) are logged as relations.
//
// usage: diff_ks_thread.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll>
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

static HMODULE ours, orig;
static HMODULE cur;  // the side running now

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
using ThreadCtor = void*(__fastcall*)(void*, void*, unsigned int(__stdcall*)(void*), void*, bool);
using Dtor = void(__fastcall*)(void*, void*);
using CopyCtor = void*(__fastcall*)(void*, void*, const void*);
using Deleting = void*(__fastcall*)(void*, void*, unsigned int);
using Member0 = bool(__cdecl*)(void*);
using MemberU = unsigned long(__cdecl*)(void*);
using MemberP = void*(__cdecl*)(void*);
using MemberI = int(__cdecl*)(void*);
using SetPrio = void(__cdecl*)(void*, int);
using LockT = bool(__cdecl*)(const void*, unsigned long);
using GetIndexT = int(__cdecl*)(void*, const void*);
using U32 = unsigned long(__cdecl*)();
using U64 = unsigned long long(__cdecl*)();
using F64 = double(__cdecl*)();
using VoidF = void(__cdecl*)();
using VoidD = void(__cdecl*)(double);
using VoidS = void(__cdecl*)(char*);
using VoidU = void(__cdecl*)(unsigned long);
using BoolU = bool(__cdecl*)(unsigned long);
using BoolF = bool(__cdecl*)();
using GetData = void*(__cdecl*)(int);
using SetData = void(__cdecl*)(int, void*);
using TlsRef = void*(__cdecl*)();
using NewT = void*(__cdecl*)(unsigned int);
using CreateT = void*(__cdecl*)(void*, const unsigned char*);

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
#define L(v) Log(static_cast<long long>(v), __LINE__)

static long long Bits(double d)
{
    long long b;
    std::memcpy(&b, &d, 8);
    return b;
}

template <class F>
static F Slot(void* obj, int n) { return reinterpret_cast<F>((*static_cast<void***>(obj))[n]); }

// the side's statics
template <class T>
static T& Data(const char* name) { return *Get<T*>(cur, name); }

struct TlsObj {
    void* lock;
    unsigned int count, capacity;
    struct Entry {
        void* name;
        int index;
    } * entries;
};
struct Node {
    void *next, *prev;
    unsigned int length, capacity;
    int refs;
    unsigned int unknown;
    char text[1];
};

static void* PStr(const char* text)  // an interned node (the caller's reference)
{
    void* cache = Data<void*>("?cache@PString@Jet@@0PAVPStringCache@2@A");
    return Get<CreateT>(cur, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z")(
        cache, reinterpret_cast<const unsigned char*>(text));
}

// ---- thread bodies

struct Body {
    const Side* s;
    void* thread;   // the Thread object running this body
    HANDLE go;      // released by the scenario
    int mode;       // 0: return 7 at once, 1: wait for go then poll IsDone, 2: record the TLS slot value
    int tls;
    void* seen;
    DWORD id;
};

static unsigned int __stdcall Run(void* arg)
{
    Body* b = static_cast<Body*>(arg);
    b->id = GetCurrentThreadId();
    if (b->mode == 0) return 7;
    if (b->mode == 2) {
        const auto set = b->s->f<SetData>("?SetThreadData@ThreadLocalStorage@Jet@@SAXHPAX@Z");
        const auto get = b->s->f<GetData>("?GetThreadData@ThreadLocalStorage@Jet@@SAPAXH@Z");
        b->seen = get(b->tls);  // a new thread starts with null
        set(b->tls, reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + b->id % 97)));
        WaitForSingleObject(b->go, INFINITE);
        return get(b->tls) == reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + b->id % 97)) ? 11 : 12;
    }
    WaitForSingleObject(b->go, INFINITE);
    const auto isDone = b->s->f<Member0>("?IsDone@Thread@Jet@@QAA_NXZ");
    while (!isDone(b->thread)) Sleep(1);
    return 42;
}

static void TimeScenario(const Side& s)
{
    bool& lock = Data<bool>("?frameLock@Time@Jet@@0_NA");
    unsigned int& count = Data<unsigned int>("?frameLockCount@Time@Jet@@0IA");
    double& time = Data<double>("?frameLockTime@Time@Jet@@0NA");
    double& delta = Data<double>("?frameLockDelta@Time@Jet@@0NA");
    const bool lock0 = lock;
    const unsigned int count0 = count;
    const double time0 = time, delta0 = delta;

    const auto setLock = s.f<VoidD>("?SetFrameLockTime@Time@Jet@@SAXN@Z");
    const auto inc = s.f<VoidF>("?IncFrame@Time@Jet@@SAXXZ");
    const auto ms = s.f<U32>("?FrameLockMs@Time@Jet@@SAKXZ");
    const auto sec = s.f<F64>("?FrameLockSeconds@Time@Jet@@SANXZ");
    const double deltas[] = {1000.0 / 60, 33.333333333333336, 1.0, 0.1, 1e-300, 1e300, -5.25, 4.9e-324, 1e15,
                             std::nan(""), 2048.0 / 3, -0.0};
    const unsigned int counts[] = {0, 1, 59, 0x7ffffffe, 0xfffffffe, 123456789};
    for (double d : deltas)
        for (unsigned int c : counts) {
            lock = false;
            count = c;
            time = 0.5;
            setLock(d);
            L(lock);
            L(Bits(delta));
            L(count);
            L(Bits(sec()));
            L(ms());
            for (int k = 0; k < 4; ++k) {
                inc();
                L(count);
                L(Bits(time));
                L(Bits(sec()));
                L(ms());
            }
        }
    // FrameLockMs over the truncation edges
    const double times[] = {0, 999.999, 1000, -999.9, -1000, 4294967295999.0, 4294967296000.0, 9.3e21, -9.3e21,
                            1e40, std::nan(""), 1.5e3, -1.5e3, 2.5e3};
    for (double t : times) {
        lock = true;
        time = t;
        L(ms());
        L(Bits(sec()));
    }
    // unlocked: the live clocks
    lock = false;
    const DWORD t0 = timeGetTime();
    const unsigned long m = ms();
    L(m - t0 < 1000);
    const double g = sec();
    const auto seconds = s.f<F64>("?GetSeconds@Time@Jet@@SANXZ");
    const double g2 = seconds();
    L(g2 >= g && g2 - g < 1.0);
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    L(s.f<U64>("?GetCounterFrequency@Time@Jet@@SA_KXZ")() == static_cast<unsigned long long>(f.QuadPart));
    L(s.f<U64>("?QueryCounterFrequency@Time@Jet@@CA_KXZ")() == static_cast<unsigned long long>(f.QuadPart));
    QueryPerformanceCounter(&c);
    const unsigned long long c1 = s.f<U64>("?GetCounter@Time@Jet@@SA_KXZ")();
    const unsigned long long c2 = s.f<U64>("?Usq@Time@Jet@@SA_KXZ")();
    const unsigned long u = s.f<U32>("?Us@Time@Jet@@SAKXZ")();
    L(c1 >= static_cast<unsigned long long>(c.QuadPart) && c2 >= c1 && u - static_cast<unsigned long>(c2) < 0x10000000);
    L(std::fabs(seconds() - static_cast<double>(c2) / static_cast<double>(f.QuadPart)) < 0.5);
    L(s.f<U32>("?Ms@Time@Jet@@SAKXZ")() - timeGetTime() + 1000 < 2000);
    const unsigned long long stamp = s.f<U64>("?TimeStamp@Time@Jet@@SA_KXZ")();
    L(stamp - static_cast<unsigned long long>(_time64(nullptr)) + 2 < 4);
    const DWORD d0 = timeGetTime();
    s.f<VoidU>("?Delay@Time@Jet@@SAXK@Z")(30);
    L(timeGetTime() - d0 >= 25);
    for (int k = 0; k < 3; ++k) {  // Str / HHMM against _strtime (retried across a second boundary)
        char a[32] = {}, h[32] = {}, b[32] = {};
        std::memset(h, 'x', sizeof h);
        _strtime(a);
        s.f<VoidS>("?Str@Time@Jet@@SAXPAD@Z")(b);
        s.f<VoidS>("?HHMM@Time@Jet@@SAXPAD@Z")(h);
        const bool same = std::strcmp(a, b) == 0 && h[0] == a[0] && h[1] == a[1] && h[2] == a[3] && h[3] == a[4];
        if (!same && k < 2) continue;
        L(same);
        L(std::strlen(b));
        L(h[4] == 0 && h[5] == 'x');
        break;
    }
    lock = lock0;
    count = count0;
    time = time0;
    delta = delta0;
}

static void ThreadScenario(const Side& s)
{
    const auto ctor = s.f<ThreadCtor>("??0Thread@Jet@@QAE@P6GIPAX@Z0_N@Z");
    const auto dtor = s.f<Dtor>("??1Thread@Jet@@UAE@XZ");
    const auto vft = s.f<void*>("??_7Thread@Jet@@6B@");
    const auto vftEvent = s.f<void*>("??_7WEvent@Jet@@6B@");
    const auto resume = s.f<Member0>("?Resume@Thread@Jet@@QAA_NXZ");
    const auto suspend = s.f<Member0>("?Suspend@Thread@Jet@@QAA_NXZ");
    const auto setPrio = s.f<SetPrio>("?SetPriority@Thread@Jet@@QAAXW4Priority@12@@Z");
    const auto getPrio = s.f<MemberI>("?GetPriority@Thread@Jet@@QBA?AW4Priority@12@XZ");
    const auto startedOk = s.f<Member0>("?StartedOk@Thread@Jet@@QBA_NXZ");
    const auto isAlive = s.f<Member0>("?IsAlive@Thread@Jet@@QBA_NXZ");
    const auto isDone = s.f<Member0>("?IsDone@Thread@Jet@@QAA_NXZ");
    const auto setDone = s.f<Member0>("?SetDone@Thread@Jet@@QAAXXZ");
    const auto terminate = s.f<MemberU>("?Terminate@Thread@Jet@@QAAKXZ");
    const auto threadId = s.f<MemberU>("?GetThreadID@Thread@Jet@@QAAKXZ");
    const auto opVoid = s.f<MemberP>("??BThread@Jet@@UAAPAXXZ");
    const NewT alloc = Get<NewT>(cur, "?New@Alloc@Jet@@SAPAXI@Z");

    // 1. created suspended, the state words, suspend/resume, priorities, done flag, Terminate
    for (int prio : {0, 1, 2, -1, -2, 15, -15}) {
        Body b = {&s, nullptr, CreateEventA(nullptr, TRUE, FALSE, nullptr), 1, 0, nullptr, 0};
        uint32_t* t = static_cast<uint32_t*>(alloc(0x20));
        b.thread = t;
        ctor(t, nullptr, &Run, &b, false);
        L(t[0] == reinterpret_cast<uintptr_t>(vft));
        L(t[2] == reinterpret_cast<uintptr_t>(vftEvent));
        L(t[1] != 0 && t[3] != 0);
        L(static_cast<uint8_t>(t[4]));
        L(t[5] == reinterpret_cast<uintptr_t>(&Run));
        L(t[6] == reinterpret_cast<uintptr_t>(&b));
        L(t[7] == GetThreadId(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(t[1]))));
        L(threadId(t) == t[7]);
        L(startedOk(t));
        L(isAlive(t));
        L(opVoid(t) == reinterpret_cast<void*>(static_cast<uintptr_t>(t[3])));
        L(Slot<MemberP>(t, 1)(t) == reinterpret_cast<void*>(static_cast<uintptr_t>(t[3])));
        L(Slot<LockT>(t, 2)(t, 0));  // a suspended thread is not signalled
        L(isDone(t));
        setPrio(t, prio);
        L(getPrio(t));
        L(suspend(t));
        L(resume(t));
        L(resume(t));  // the create suspension
        L(resume(t));  // running: count 0, still true
        L(isDone(t));
        setDone(t);
        L(isDone(t));
        L(isDone(t));  // auto-reset: consumed
        Sleep(2);
        L(b.id == t[7]);
        SetEvent(b.go);
        L(terminate(t));  // sets done; the body sees it and returns 42
        L(isAlive(t));
        L(Slot<LockT>(t, 2)(t, 0));
        L(terminate(t));  // already finished
        L(startedOk(t));
        Slot<Deleting>(t, 0)(t, nullptr, 1);
        CloseHandle(b.go);
    }
    // 2. a body that returns at once, destroyed in place (dtor only); the handle is dropped, not closed
    for (int k = 0; k < 3; ++k) {
        Body b = {&s, nullptr, nullptr, 0, 0, nullptr, 0};
        uint32_t t[8];
        std::memset(t, 0xcd, sizeof t);
        b.thread = t;
        ctor(t, nullptr, &Run, &b, true);
        const HANDLE h = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(t[1]));
        WaitForSingleObject(h, INFINITE);
        L(isAlive(t));
        DWORD code = 0;
        GetExitCodeThread(h, &code);
        L(code);
        L(isDone(t));
        if (k == 1) setDone(t);
        dtor(t, nullptr);
        L(t[0] == reinterpret_cast<uintptr_t>(s.f<void*>("??_7SyncObject@Jet@@6B@")));
        L(t[1]);
        L(t[3]);
        L(GetExitCodeThread(h, &code) != 0);  // still a valid handle
        CloseHandle(h);
    }
    // 3. destroying a running thread waits for it (the destructor sets done)
    {
        Body b = {&s, nullptr, CreateEventA(nullptr, TRUE, TRUE, nullptr), 1, 0, nullptr, 0};
        uint32_t* t = static_cast<uint32_t*>(alloc(0x20));
        b.thread = t;
        ctor(t, nullptr, &Run, &b, true);
        Sleep(5);
        const HANDLE h = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(t[1]));
        Slot<Deleting>(t, 0)(t, nullptr, 1);
        DWORD code = 0;
        L(GetExitCodeThread(h, &code) != 0);
        L(code);
        CloseHandle(h);
        CloseHandle(b.go);
    }
    // 4. the statics and process helpers
    unsigned int& mainId = Data<unsigned int>("?mainThreadId@Thread@Jet@@0IA");
    const unsigned int main0 = mainId;
    L(s.f<U32>("?GetMainThreadId@Thread@Jet@@SAKXZ")() == main0);
    L(s.f<U32>("?GetCurrentId@Thread@Jet@@SAKXZ")() == GetCurrentThreadId());
    L(s.f<U32>("?GetProcessId@Thread@Jet@@SAKXZ")() == GetCurrentProcessId());
    struct G {
        const Side* s;
        DWORD id;
    } g = {&s, 0};
    HANDLE h = CreateThread(
        nullptr, 0,
        [](void* a) -> DWORD {
            G* g = static_cast<G*>(a);
            g->id = GetCurrentThreadId();
            g->s->f<VoidF>("?GlobalInit@Thread@Jet@@SAXXZ")();
            return 0;
        },
        &g, 0, nullptr);
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    L(mainId == g.id);
    L(s.f<U32>("?GetMainThreadId@Thread@Jet@@SAKXZ")() == g.id);
    mainId = main0;
    char name[0x200], want[0x200];
    std::memset(name, 'x', sizeof name);
    s.f<VoidS>("?GetProcessName@Thread@Jet@@SAXPAD@Z")(name);
    GetModuleFileNameA(nullptr, want, 0x100);
    L(std::strcmp(name, want) == 0);
    const auto alive = s.f<BoolU>("?IsProcessAlive@Thread@Jet@@SA_NK@Z");
    L(alive(GetCurrentProcessId()));
    L(alive(0));
    L(alive(0xfffffff0));
    s.f<VoidU>("?TerminateProcess@Thread@Jet@@SAXK@Z")(0);
    STARTUPINFOA si = {sizeof si};
    PROCESS_INFORMATION pi;
    char cmd[] = "cmd.exe /c ping -n 30 127.0.0.1 >nul";
    if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        L(alive(pi.dwProcessId));
        s.f<VoidU>("?TerminateProcess@Thread@Jet@@SAXK@Z")(pi.dwProcessId);
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        L(code);
        L(alive(pi.dwProcessId));
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        L(-1);
    }
}

static void TlsScenario(const Side& s)
{
    TlsObj*& singleton = Data<TlsObj*>("?singleton@ThreadLocalStorage@Jet@@0PAV12@A");
    int* idx[4] = {&Data<int>("?ThreadTlsIndex@ThreadLocalStorage@Jet@@2HA"),
                   &Data<int>("?ProfilePtTlsIndex@ThreadLocalStorage@Jet@@2HA"),
                   &Data<int>("?ProfileTraceTlsIndex@ThreadLocalStorage@Jet@@2HA"),
                   &Data<int>("?ProfileRenderTlsIndex@ThreadLocalStorage@Jet@@2HA")};
    int idx0[4];
    for (int k = 0; k < 4; ++k) idx0[k] = *idx[k];
    TlsObj* const single0 = singleton;
    L(single0 != nullptr);
    L(s.f<TlsRef>("?GetTLS@Jet@@YAAAVThreadLocalStorage@1@XZ")() == single0);

    const auto getIndex = s.f<GetIndexT>("?GetIndex@ThreadLocalStorage@Jet@@QAAHABVPString@2@@Z");
    // the singleton: the original's own names first
    static const char* const own[4] = {"ThreadIndex", "ProfilePtIndex", "ProfileTraceIndex", "ProfileRenderIndex"};
    for (int k = 0; k < 4; ++k) {
        void* node = PStr(own[k]);
        const int refs = static_cast<Node*>(node)->refs;
        const unsigned int count = single0->count;
        const int i = getIndex(single0, &node);
        L(i == *idx[k]);
        L(single0->count - count);
        L(static_cast<Node*>(node)->refs - refs);
    }
    // new names, growth past 128 entries, repeats: on a fresh instance of the side
    TlsObj* const grow = static_cast<TlsObj*>(Get<NewT>(cur, "?New@Alloc@Jet@@SAPAXI@Z")(16));
    s.f<Dtor>("??0ThreadLocalStorage@Jet@@AAE@XZ")(grow, nullptr);
    std::vector<int> got;
    const unsigned int count0 = grow->count;
    const unsigned int cap0 = grow->capacity;
    L(cap0 % 128);
    for (int k = 0; k < 140; ++k) {
        char n[64];
        std::snprintf(n, sizeof n, "%s.slot%d", s.tag, k);
        void* node = PStr(n);
        const int refs = static_cast<Node*>(node)->refs;
        const void* entries = grow->entries;
        const int i = getIndex(grow, &node);
        got.push_back(i);
        L(i >= 0 && i < 1088);
        L(grow->count - count0);
        L(grow->capacity);
        L(grow->entries != entries);
        L(static_cast<Node*>(node)->refs - refs);
        L(grow->entries[grow->count - 1].name == node && grow->entries[grow->count - 1].index == i);
        if (k % 7 == 3) {
            L(getIndex(grow, &node) == i);
            L(static_cast<Node*>(node)->refs - refs);
        }
        if (grow->count + 2 > 1000) break;  // keep to the process's TLS slots
    }
    bool distinct = true;
    for (size_t a = 0; a < got.size(); ++a)
        for (size_t b = a + 1; b < got.size(); ++b) distinct &= got[a] != got[b];
    L(distinct);
    // null name: an entry with no node
    {
        void* none = nullptr;
        const unsigned int c = grow->count;
        const int i = getIndex(grow, &none);
        L(grow->count - c);
        L(getIndex(grow, &none) == i);
    }
    s.f<Dtor>("??1ThreadLocalStorage@Jet@@AAE@XZ")(grow, nullptr);
    for (int k = 0; k < 4; ++k) *idx[k] = idx0[k];

    // Get/SetThreadData on 4 threads
    {
        void* node = PStr((std::string(s.tag) + ".data").c_str());
        const int slot = getIndex(single0, &node);
        const auto set = s.f<SetData>("?SetThreadData@ThreadLocalStorage@Jet@@SAXHPAX@Z");
        const auto get = s.f<GetData>("?GetThreadData@ThreadLocalStorage@Jet@@SAPAXH@Z");
        set(slot, reinterpret_cast<void*>(0x5150));
        L(reinterpret_cast<uintptr_t>(get(slot)));
        HANDLE go = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        Body b[4];
        HANDLE th[4];
        for (int k = 0; k < 4; ++k) {
            b[k] = {&s, nullptr, go, 2, slot, reinterpret_cast<void*>(1), 0};
            th[k] = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(&Run), &b[k], 0, nullptr);
        }
        Sleep(20);
        SetEvent(go);
        WaitForMultipleObjects(4, th, TRUE, INFINITE);
        for (int k = 0; k < 4; ++k) {
            DWORD code;
            GetExitCodeThread(th[k], &code);
            L(code);
            L(b[k].seen == nullptr);
            CloseHandle(th[k]);
        }
        L(reinterpret_cast<uintptr_t>(get(slot)));
        CloseHandle(go);
    }

    // copies share the mutex and copy the entries (never destroyed: their destructor frees the slots and the
    // shared mutex)
    {
        TlsObj* c = static_cast<TlsObj*>(Get<NewT>(cur, "?New@Alloc@Jet@@SAPAXI@Z")(16));
        std::memset(c, 0xcd, 16);
        Node* first = static_cast<Node*>(single0->entries[0].name);
        const int refs = first->refs;
        s.f<CopyCtor>("??0ThreadLocalStorage@Jet@@QAE@ABV01@@Z")(c, nullptr, single0);
        L(c->lock == single0->lock);
        L(c->count == single0->count);
        L(c->capacity == single0->capacity);
        L(c->entries != single0->entries);
        L(std::memcmp(c->entries, single0->entries, single0->count * 8) == 0);
        L(first->refs - refs);
        TlsObj d = {nullptr, 0, 0, nullptr};  // operator= keeps a larger own capacity
        TlsObj* e = &d;
        s.f<CopyCtor>("??4ThreadLocalStorage@Jet@@QAEAAV01@ABV01@@Z")(e, nullptr, c);
        L(d.lock == c->lock);
        L(d.count == c->count);
        L(d.capacity == c->capacity);
        L(first->refs - refs);
        const unsigned int bigCap = d.capacity;
        TlsObj few = {nullptr, 2, 0, static_cast<TlsObj::Entry*>(Get<NewT>(cur, "?New@Alloc@Jet@@SAPAXI@Z")(16))};
        few.entries[0] = {nullptr, 1};
        few.entries[1] = {nullptr, 2};
        few.capacity = 2;
        s.f<CopyCtor>("??4ThreadLocalStorage@Jet@@QAEAAV01@ABV01@@Z")(&d, nullptr, &few);
        L(d.count);
        L(d.capacity == bigCap);
        L(d.entries[1].index);
        L(first->refs - refs);  // the release of the old entries
        TlsObj empty = {nullptr, 0, 0, nullptr};
        s.f<CopyCtor>("??4ThreadLocalStorage@Jet@@QAEAAV01@ABV01@@Z")(&d, nullptr, &empty);
        L(d.count);
        L(d.lock == nullptr);
    }

    // a private instance: its own mutex, the four names (the index statics move to it), then its destructor
    {
        TlsObj* t = static_cast<TlsObj*>(Get<NewT>(cur, "?New@Alloc@Jet@@SAPAXI@Z")(16));
        s.f<Dtor>("??0ThreadLocalStorage@Jet@@AAE@XZ")(t, nullptr);
        L(t->lock != nullptr && t->lock != single0->lock);
        L(*static_cast<void**>(t->lock) == s.f<void*>("??_7Mutex@Jet@@6B@"));
        L(t->count);
        L(t->capacity);
        for (unsigned int k = 0; k < t->count; ++k) {
            L(std::strcmp(static_cast<Node*>(t->entries[k].name)->text, own[k]) == 0);
            L(t->entries[k].index == *idx[k]);
            L(*idx[k] != idx0[k]);
        }
        s.f<Dtor>("??1ThreadLocalStorage@Jet@@AAE@XZ")(t, nullptr);
        L(t->lock == nullptr);
        L(t->count);
        for (int k = 0; k < 4; ++k) *idx[k] = idx0[k];
    }

    // CreateTLS / DestroyTLS on the singleton slot (the original's one is put back)
    {
        L(s.f<BoolF>("?CreateTLS@ThreadLocalStorage@Jet@@SA_NXZ")());
        TlsObj* made = singleton;
        L(made != single0 && made != nullptr);
        L(made->count);
        L(s.f<TlsRef>("?GetTLS@Jet@@YAAAVThreadLocalStorage@1@XZ")() == made);
        s.f<VoidF>("?DestroyTLS@ThreadLocalStorage@Jet@@SAXXZ")();
        L(singleton == nullptr);
        s.f<VoidF>("?DestroyTLS@ThreadLocalStorage@Jet@@SAXXZ")();  // twice: nothing
        L(singleton == nullptr);
        singleton = single0;
        for (int k = 0; k < 4; ++k) *idx[k] = idx0[k];
    }
}

static void Scenario(const Side& s)
{
    cur = s.m;
    logv.clear();
    lines.clear();
    std::printf("%s: time\n", s.tag);
    TimeScenario(s);
    std::printf("%s: thread\n", s.tag);
    ThreadScenario(s);
    std::printf("%s: tls\n", s.tag);
    TlsScenario(s);
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e) -> LONG {
        std::printf("crash %08lx at %p after line %d\n", e->ExceptionRecord->ExceptionCode,
                    e->ExceptionRecord->ExceptionAddress, lines.empty() ? 0 : lines.back());
        return EXCEPTION_EXECUTE_HANDLER;
    });
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) return 2;
    Scenario({orig, "orig"});
    const std::vector<long long> a = logv;
    Scenario({ours, "ours"});
    const std::vector<long long>& b = logv;
    size_t bad = 0;
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const long long x = i < a.size() ? a[i] : -999, y = i < b.size() ? b[i] : -999;
        if (x != y && ++bad <= 10)
            std::printf("  step %zu (line %d): orig %lld ours %lld\n", i, i < lines.size() ? lines[i] : 0, x, y);
    }
    std::printf("thread/time/tls scenario: %zu results each, %zu mismatches\n", a.size(), bad);
    return bad ? 1 : 0;
}
