// Differential test of KernelStandard unit U12b (Console.cpp, Profile.cpp): the same scripted scenario runs once
// through KernelStandard_7c_orig.dll's exports and once through ours; every observable result goes into a log and the
// two logs must match. Console is fully deterministic (random operation scripts over several sizes). Profile and
// QuickTimer share the original's statics (saved, cleared and restored around each side); their clock readings are
// replaced by fixed values before anything that prints or sorts them, and are otherwise logged only as relations.
//
// usage: diff_ks_profile.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scripts]
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
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

static long long Hash(const void* p, size_t n)
{
    unsigned long long h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 1099511628211ull;
    return static_cast<long long>(h);
}

template <class T>
static T& Data(const char* name) { return *Get<T*>(orig, name); }

using Ctor0 = void*(__fastcall*)(void*, void*);
using CtorP = void*(__fastcall*)(void*, void*, const void*);
using Dtor = void(__fastcall*)(void*, void*);
using NewT = void*(__cdecl*)(unsigned int);

// ---- Console

struct Con {
    int width, height;
    unsigned char* buffer;
    int top;
    struct Chunk {
        Chunk *next, *prev;
        unsigned int count;
        struct {
            unsigned char type, pad[3];
            int pos;
            unsigned long rgb;
        } spans[13];
    } * spans;
    unsigned char* cursor;
    unsigned char* lineEnd;
};

static void LogConsole(const Con& c, int line)
{
    Log(c.width, line);
    Log(c.height, line);
    Log(c.top, line);
    if (c.buffer) {
        Log(Hash(c.buffer, static_cast<size_t>(c.width) * c.height), line);
        Log(c.cursor - c.buffer, line);
        Log(c.lineEnd - c.buffer, line);
    }
    if (!c.spans) {
        Log(-1, line);
        return;
    }
    const Con::Chunk* k = c.spans;
    int chunks = 0;
    do {
        Log(k->count, line);
        for (unsigned int i = 0; i < k->count && i < 13; ++i) {
            Log(k->spans[i].type, line);
            Log(k->spans[i].pos, line);
            Log(k->spans[i].rgb, line);
        }
        Log(k->next->prev == k, line);
        k = k->next;
    } while (k != c.spans && ++chunks < 100);
}

static const char* const kTexts[] = {
    "", "hello", "a\nb\nc", "<color:FF8000>red</color> done", "<color:#12ab34>x</color>", "<color:0x7fA1b2C3>y",
    "</color>", "<other>tag", "<color>no value", "<<color:00FF00>double", "unterminated <color:00ff00", "<>",
    "x<color:1>a<color:12345678901>b</colour>c</COLOR>d", "<color:-ff>neg", "<color:+0x10>pos", "<a/b>slash",
    "<color:zz>bad", "line\n\n\nend\n", "<color:abcdef><color:fedcba><color:123456><color:654321><color:111111>"
    "<color:222222><color:333333><color:444444><color:555555><color:666666><color:777777><color:888888>"
    "<color:999999><color:aaaaaa><color:bbbbbb></color></color>13+",
    "0123456789012345678901234567890123456789012345678901234567890123456789", "<color::ff>", "<:color>",
    "tab\there <color:00ff00>", "<color:00FF00/>", "<color/x:00ff00>"};

static void ConsoleScenario(const Side& s, uint32_t seed)
{
    const auto ctor = s.f<Ctor0>("??0Console@Jet@@QAE@XZ");
    const auto copy = s.f<CtorP>("??0Console@Jet@@QAE@ABV01@@Z");
    const auto assign = s.f<CtorP>("??4Console@Jet@@QAEAAV01@ABV01@@Z");
    const auto dtor = s.f<Dtor>("??1Console@Jet@@QAE@XZ");
    const auto init = s.f<void(__cdecl*)(void*, const int*)>("?Init@Console@Jet@@QAAXABUSize@2@@Z");
    const auto done = s.f<void(__cdecl*)(void*)>("?Done@Console@Jet@@QAAXXZ");
    const auto clear = s.f<void(__cdecl*)(void*)>("?Clear@Console@Jet@@QAAXXZ");
    const auto print = s.f<void(__cdecl*)(void*, const char*)>("?Print@Console@Jet@@QAAXPBD@Z");
    const auto printf_ = s.f<void(__cdecl*)(void*, const char*, ...)>("?Printf@Console@Jet@@QAAXPBDZZ");
    const auto printCh = s.f<void(__cdecl*)(void*, char, int)>("?Print@Console@Jet@@QAAXDH@Z");
    const auto printInt = s.f<void(__cdecl*)(void*, int, int)>("?Print@Console@Jet@@QAAXHH@Z");
    const auto bar = s.f<void(__cdecl*)(void*, int, int)>("?Bar@Console@Jet@@QAAXHH@Z");
    const auto go = s.f<void(__cdecl*)(void*, int, int)>("?Goto@Console@Jet@@QAAXHH@Z");
    const auto blit = s.f<void(__cdecl*)(const void*, unsigned char*)>("?BlitToMono@Console@Jet@@QBAXPAE@Z");
    const auto blit44 =
        s.f<void(__cdecl*)(const void*, int, int, unsigned char*)>("?BlitToMono44@Console@Jet@@QBAXHHPAE@Z");
    const auto hex = s.f<int(__cdecl*)(const char*, int)>("?StringToHex@Console@Jet@@SAHPBDH@Z");

    for (const char* t : {"ff", "0x1F", "#abc", "-10", "+ff", "-0x8", "12345678", "fffffffff", "g1", "1g", "", "0X",
                          "-", "#", "aBcDeF09"})
        for (int n = 0; n <= static_cast<int>(std::strlen(t)) + 1; ++n) L(hex(t, n));

    std::mt19937 rng(seed);
    auto R = [&](uint32_t n) { return rng() % n; };
    static unsigned char screen[0x10000];
    const int sizes[][2] = {{40, 10}, {7, 3}, {1, 1}, {13, 5}, {25, 10}, {3, 8}};
    for (const auto& sz : sizes) {
        Con c;
        std::memset(&c, 0xcd, sizeof c);
        ctor(&c, nullptr);
        L(c.buffer == nullptr && c.spans == nullptr);
        init(&c, sz);
        LogConsole(c, __LINE__);
        for (int step = 0; step < 60; ++step) {
            switch (R(9)) {
            case 0:
            case 1: print(&c, kTexts[R(sizeof kTexts / sizeof *kTexts)]); break;
            case 2: printf_(&c, "%d|%s|%5.2f|%x\n<color:%06X>z", static_cast<int>(R(2000)) - 1000, "fmt", R(100) / 7.0,
                            R(65536), R(0x1000000)); break;
            case 3: printCh(&c, static_cast<char>('A' + R(26)), static_cast<int>(R(sz[0] * 2 + 2))); break;
            case 4: {
                const int w = std::min(static_cast<int>(R(6)), sz[0] - 1 - static_cast<int>(c.cursor - c.buffer) % sz[0]);
                if (w >= 0) printInt(&c, static_cast<int>(R(200000)) - 100000 * static_cast<int>(R(2)), w);
                break;
            }
            case 5: {  // a width below the bar's length would make the padding count negative (a 4G loop)
                const int half = static_cast<int>(R(12));
                bar(&c, half, half / 2 + (half & 1) + static_cast<int>(R(6)));
                break;
            }
            case 6: go(&c, static_cast<int>(R(sz[0])), static_cast<int>(R(sz[1]))); break;  // top + y < 2 * height: one wrap
            case 7: if (R(4) == 0) clear(&c); break;
            case 8: {
                Con d;
                std::memset(&d, 0xab, sizeof d);
                copy(&d, nullptr, &c);
                LogConsole(d, __LINE__);
                Con e;
                std::memset(&e, 0, sizeof e);
                ctor(&e, nullptr);
                assign(&e, nullptr, &d);
                LogConsole(e, __LINE__);
                assign(&e, nullptr, &c);  // replaces its own spans
                LogConsole(e, __LINE__);
                d.buffer = nullptr;  // they share c's buffer
                e.buffer = nullptr;
                dtor(&d, nullptr);
                dtor(&e, nullptr);
                L(d.spans == nullptr && e.spans == nullptr);
                break;
            }
            }
            LogConsole(c, __LINE__);
        }
        std::memset(screen, 0x55, sizeof screen);
        blit(&c, screen);
        L(Hash(screen, sizeof screen));
        std::memset(screen, 0x66, sizeof screen);
        blit44(&c, 3, 2, screen);
        L(Hash(screen, sizeof screen));
        done(&c);
        L(c.buffer == nullptr);
        done(&c);
        print(&c, "<color:00ff00>");  // spans only (no buffer is touched: no characters)
        dtor(&c, nullptr);
        L(c.spans == nullptr);
    }
}

// ---- Profile / QuickTimer

struct Info {  // Profile::Info
    const char* name;
    const char* file;
    unsigned long count, time;
    int id;
    unsigned long mask, peek, u1c;
};
struct QInfo {
    const char* name;
    const char* file;
    int id;
    struct {
        int count;
        unsigned long min, max, total;
    } frames[4];
};

struct ProfileState {
    unsigned int* enabled;
    int* nextid;
    Info** funcs;
    unsigned int* depth;
    void** current;
    bool* childtime;
    float* displayPeriod;
    float* sortFreq;
    float* peekResetFreq;
    void** sortFunc;
    const char** tracePoint;
    unsigned int* numTracePoints;
    unsigned int* traceFrame;
    unsigned char* traceData;
    float* acc;  // the three Display(float) accumulators
    // QuickTimer
    bool* qChildtime;
    int* qCurrentFrame;
    unsigned long* qFrameTimes;
    QInfo** qFuncs;
    unsigned long long* qLastFrameStart;
    int* qLogEvery;
    long* qNextid;
    int* qNumFrames;
    void** qThreadInfo;
};
static ProfileState ps;

struct Saved {
    std::vector<unsigned char> blob;
};
static std::vector<std::pair<void*, std::vector<unsigned char>>> saved;
static void Save(void* p, size_t n)
{
    saved.push_back({p, std::vector<unsigned char>(static_cast<unsigned char*>(p), static_cast<unsigned char*>(p) + n)});
}
static void RestoreAll()
{
    for (auto& s : saved) std::memcpy(s.first, s.second.data(), s.second.size());
}

static void ProfileScenario(const Side& s, uint32_t seed)
{
    RestoreAll();
    *ps.enabled = 0;
    *ps.nextid = 0;
    *ps.depth = 0;
    *ps.current = nullptr;
    *ps.childtime = true;
    *ps.displayPeriod = 0.5f;
    *ps.sortFreq = 1.0f;
    *ps.peekResetFreq = 3.0f;
    *ps.numTracePoints = 0;
    *ps.traceFrame = 0;
    ps.acc[0] = ps.acc[1] = ps.acc[2] = 0;
    const auto pctor = s.f<CtorP>("??0Profile@Jet@@QAE@PAUInfo@01@@Z");
    const auto pdtor = s.f<Dtor>("??1Profile@Jet@@QAE@XZ");
    const auto enable = s.f<void(__cdecl*)(unsigned int)>("?Enable@Profile@Jet@@SAXI@Z");
    const auto disable = s.f<void(__cdecl*)()>("?Disable@Profile@Jet@@SAXXZ");
    const auto childTime = s.f<void(__cdecl*)(bool)>("?ChildTime@Profile@Jet@@SAX_N@Z");
    const auto entry = s.f<void(__cdecl*)(void*, Info*)>("?Entry@Profile@Jet@@AAAXPAUInfo@12@@Z");
    const auto exit_ = s.f<void(__cdecl*)(void*, Info*)>("?Exit@Profile@Jet@@AAAXPAUInfo@12@@Z");
    const auto sortBy = s.f<void(__cdecl*)(int)>("?SortBy@Profile@Jet@@SAXW4SortOption@12@@Z");
    const auto sort = s.f<void(__cdecl*)()>("?Sort@Profile@Jet@@SAXXZ");
    const auto display = s.f<void(__cdecl*)(void*, int)>("?Display@Profile@Jet@@SAXAAVConsole@2@H@Z");
    const auto displayF = s.f<void(__cdecl*)(float, void*, int)>("?Display@Profile@Jet@@SAXMAAVConsole@2@H@Z");
    const auto logDisplay = s.f<void(__cdecl*)()>("?LogDisplay@Profile@Jet@@SAXXZ");
    const auto reset = s.f<void(__cdecl*)()>("?Reset@Profile@Jet@@SAXXZ");
    const auto resetPeeks = s.f<void(__cdecl*)()>("?ResetPeeks@Profile@Jet@@SAXXZ");
    const auto reg = s.f<int(__cdecl*)(const char*)>("?RegisterTracePoint@Profile@Jet@@SAHPBD@Z");
    const auto trace = s.f<void(__cdecl*)(int)>("?TracePoint@Profile@Jet@@SAXH@Z");

    // getters and setters
    s.f<void(__cdecl*)(float)>("?SetSortFrequency@Profile@Jet@@SAXM@Z")(2.5f);
    L(*ps.sortFreq == 2.5f);
    L(s.f<float(__cdecl*)()>("?GetSortFrequency@Profile@Jet@@SAMXZ")() == 2.5f);
    for (float f : {3.0f, 7.0f, 0.3f, 60.0f, 1e-30f, 0.0f}) {
        s.f<void(__cdecl*)(float)>("?SetDisplayFrequency@Profile@Jet@@SAXM@Z")(f);
        float p = *ps.displayPeriod;
        L(Hash(&p, 4));
        const double g = s.f<double(__cdecl*)()>("?GetDisplayFrequency@Profile@Jet@@SAMXZ")();  // st(0) as is
        L(Hash(&g, 8));
        p = s.f<float(__cdecl*)()>("?GetDisplayPeriod@Profile@Jet@@SAMXZ")();
        L(Hash(&p, 4));
    }
    *ps.displayPeriod = 0.5f;
    s.f<void(__cdecl*)(float)>("?SetPeekResetFrequency@Profile@Jet@@SAXM@Z")(3.0f);
    childTime(false);
    L(s.f<bool(__cdecl*)()>("?GetChildTime@Profile@Jet@@SA_NXZ")());
    childTime(true);
    L(s.f<bool(__cdecl*)()>("?GetChildTime@Profile@Jet@@SA_NXZ")());
    L(s.f<void*(__cdecl*)()>("?GetFuncs@Profile@Jet@@SAPAPAUInfo@12@XZ")() == ps.funcs);
    L(s.f<void*(__cdecl*)()>("?GetTraceInfo@Profile@Jet@@SAPBUTraceInfo@12@XZ")() == ps.tracePoint);
    L(s.f<void*(__cdecl*)()>("?GetFrameTraceData@Profile@Jet@@SAPBUFrameTrace@12@XZ")() == ps.traceData);

    // scopes: registration, nesting, mask
    static Info infos[40];
    static char names[40][32];
    for (int i = 0; i < 40; ++i) {
        std::snprintf(names[i], sizeof names[i], "Func%02d::Update", i);
        infos[i] = {names[i], "file.cpp", 0, 0, -1, i % 5 == 0 ? 2u : 0x80000000u, 0, 0};
    }
    enable(0x80000000u);
    struct Scope {
        void* info;
        void* parent;
        unsigned long childTime, start;
    };
    Scope sc[4];
    for (int round = 0; round < 3; ++round)
        for (int i = 0; i < 40; i += 3) {
            pctor(&sc[0], nullptr, &infos[i]);
            pctor(&sc[1], nullptr, &infos[(i + 1) % 40]);
            L(*ps.depth);
            L(*ps.current == &sc[1] || *ps.current == &sc[0] || *ps.current == nullptr);
            L(sc[1].parent == &sc[0]);
            pctor(&sc[2], nullptr, &infos[(i + 2) % 40]);
            pdtor(&sc[2], nullptr);
            pdtor(&sc[1], nullptr);
            pdtor(&sc[0], nullptr);
            L(*ps.depth);
            L(*ps.current == nullptr);
        }
    L(*ps.nextid);
    for (int i = 0; i < 40; ++i) {
        L(infos[i].count);
        L(infos[i].id);
    }
    for (int i = 0; i < *ps.nextid; ++i) L(ps.funcs[i] - infos);
    {
        Scope e = {nullptr, nullptr, 5, 0};
        entry(&e, &infos[1]);
        L(e.childTime);
        L(infos[1].count);
        exit_(&e, &infos[1]);
    }
    disable();
    L(*ps.enabled);

    // sorting and the table, on fixed numbers
    std::mt19937 rng(seed);
    auto R = [&](uint32_t n) { return rng() % n; };
    *(reinterpret_cast<unsigned long long*>(Get<void*>(orig, "?freq@Time@Jet@@0_KA"))) = 3579545;
    static unsigned char buf[60 * 30];
    Con c = {60, 30, buf, 0, nullptr, buf, buf};
    s.f<void(__cdecl*)(void*)>("?Clear@Console@Jet@@QAAXXZ")(&c);
    for (int round = 0; round < 12; ++round) {
        for (int i = 0; i < *ps.nextid; ++i) {
            Info* f = ps.funcs[i];
            f->time = R(4) ? R(50) * 3579 + R(64) : R(64);
            f->count = 1 + R(3) * R(20);
            if (R(5) == 0) f->time = ps.funcs[0]->time;  // equal keys
        }
        sortBy(static_cast<int>(R(4)));
        sort();
        for (int i = 0; i < *ps.nextid; ++i) L(ps.funcs[i] - infos);
        enable(R(3) ? 0x80000000u : 0);
        display(&c, static_cast<int>(R(40)));
        LogConsole(c, __LINE__);
        for (int i = 0; i < *ps.nextid; ++i) {
            L(ps.funcs[i]->peek);
            L(ps.funcs[i]->time);
        }
        for (int i = 0; i < *ps.nextid; ++i) {
            ps.funcs[i]->time = 1000 + i;
            ps.funcs[i]->count = 1;
        }
        for (int k = 0; k < 6; ++k) {
            const float dt = R(4) == 0 ? -0.1f : R(100) / 97.0f;
            displayF(dt, &c, 30);
            L(Hash(ps.acc, 12));
            L(Hash(buf, sizeof buf));
        }
        logDisplay();
        if (R(3) == 0) resetPeeks();
        if (R(4) == 0) reset();
        for (int i = 0; i < *ps.nextid; ++i) L(ps.funcs[i]->peek + 7 * ps.funcs[i]->time);
    }

    // trace points
    const char* tps[] = {"A", "START", "B", "START", "C"};
    for (int i = 0; i < 140; ++i) {
        L(reg(tps[i % 5]));
        L(*ps.numTracePoints);
    }
    for (int i = 0; i < 128; ++i) L(ps.tracePoint[i] ? (ps.tracePoint[i] - tps[0]) : -1);
    for (int i = 0; i < 2000; ++i) {
        const int id = R(10) == 0 ? 0 : R(30) == 0 ? -0x151 : static_cast<int>(R(200));
        trace(id);
    }
    L(*ps.traceFrame);
    for (int f = 0; f < 128; ++f) {
        const unsigned int* t = reinterpret_cast<const unsigned int*>(ps.traceData + f * 0x404);
        L(t[0]);
        for (unsigned int k = 0; k < t[0] && k < 0x80; ++k) L(t[1 + 2 * k]);
    }

    // QuickTimer
    struct ThreadInfo {
        void* current;
        int depth;
    } ti = {nullptr, 0};
    *ps.qThreadInfo = &ti;
    *ps.qNextid = 0;
    *ps.qCurrentFrame = 0;
    *ps.qNumFrames = 0;
    *ps.qLogEvery = 7;
    *ps.qChildtime = true;
    const auto qctor = s.f<CtorP>("??0QuickTimer@Jet@@QAE@PAUInfo@01@@Z");
    const auto qdtor = s.f<Dtor>("??1QuickTimer@Jet@@QAE@XZ");
    const auto update = s.f<void(__cdecl*)()>("?Update@QuickTimer@Jet@@SAXXZ");
    static QInfo qi[70];
    for (int i = 0; i < 70; ++i) {
        std::memset(&qi[i], 0, sizeof qi[i]);
        qi[i].name = names[i % 40];
        qi[i].id = -1;
    }
    struct QT {
        void* info;
        void* parent;
        unsigned long long child, start;
        int frame, pad;
    } q[3];
    for (int round = 0; round < 30; ++round) {
        for (int i = 0; i < 70; i += 7) {
            qctor(&q[0], nullptr, &qi[(i + round) % 70]);
            qctor(&q[1], nullptr, &qi[(i + round * 3 + 1) % 70]);
            L(ti.depth);
            L(ti.current == &q[1]);
            L(q[1].parent == &q[0]);
            L(q[0].frame);
            qdtor(&q[1], nullptr);
            L(ti.current == &q[0]);
            qdtor(&q[0], nullptr);
            L(ti.depth);
        }
        L(s.f<int(__cdecl*)()>("?GetCurrentWriteFrame@QuickTimer@Jet@@SAHXZ")());
        *ps.qLastFrameStart = 0x1234567800000000ull;
        const unsigned long sentinel = 0xdeadbeef;
        for (int k = 0; k < 4; ++k) ps.qFrameTimes[k] = sentinel;
        update();
        L(*ps.qCurrentFrame);
        L(*ps.qNumFrames);
        for (int k = 0; k < 4; ++k) L(ps.qFrameTimes[k] == sentinel);
        L(*ps.qLastFrameStart != 0x1234567800000000ull);
    }
    L(*ps.qNextid);
    for (int i = 0; i < 70; ++i) {
        L(qi[i].id);
        for (int k = 0; k < 4; ++k) {
            L(qi[i].frames[k].count);
            L(qi[i].frames[k].min <= qi[i].frames[k].max);
        }
    }
    for (int i = 0; i < 64; ++i) L(ps.qFuncs[i] ? ps.qFuncs[i] - qi : -1);
    {
        unsigned char a[0x20], b[0x20];
        for (int i = 0; i < 0x20; ++i) a[i] = static_cast<unsigned char>(i * 7), b[i] = 0;
        s.f<CtorP>("??4QuickTimer@Jet@@QAEAAV01@ABV01@@Z")(b, nullptr, a);
        L(std::memcmp(a, b, 0x20));
        Scope pa = {&infos[3], &sc[0], 9, 8}, pb = {};
        s.f<CtorP>("??4Profile@Jet@@QAEAAV01@ABV01@@Z")(&pb, nullptr, &pa);
        L(std::memcmp(&pa, &pb, sizeof pa));
    }
    RestoreAll();
}

static void Scenario(const Side& s, int scripts)
{
    logv.clear();
    lines.clear();
    std::printf("%s\n", s.tag);
    for (int k = 0; k < scripts; ++k) ConsoleScenario(s, 1000 + k);
    for (int k = 0; k < 3; ++k) ProfileScenario(s, 77 + k);
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e) -> LONG {
        HMODULE m = nullptr;
        char mod[MAX_PATH] = "?";
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(e->ExceptionRecord->ExceptionAddress), &m);
        if (m) GetModuleFileNameA(m, mod, MAX_PATH);
        std::printf("crash %08lx at %p (%s +%lx) after line %d, %zu results\n", e->ExceptionRecord->ExceptionCode,
                    e->ExceptionRecord->ExceptionAddress, mod,
                    static_cast<unsigned long>(static_cast<char*>(e->ExceptionRecord->ExceptionAddress) - reinterpret_cast<char*>(m)),
                    lines.empty() ? 0 : lines.back(), logv.size());
        return EXCEPTION_EXECUTE_HANDLER;
    });
    const int scripts = argc > 2 ? std::atoi(argv[2]) : 20;
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) return 2;
    char* const base = reinterpret_cast<char*>(orig);
    ps = {&Data<unsigned int>("?enabled@Profile@Jet@@0IA"),
          &Data<int>("?nextid@Profile@Jet@@0HA"),
          Get<Info**>(orig, "?funcs@Profile@Jet@@0PAPAUInfo@12@A"),
          &Data<unsigned int>("?depth@Profile@Jet@@0IA"),
          &Data<void*>("?current@Profile@Jet@@0PAV12@A"),
          &Data<bool>("?childtime@Profile@Jet@@0_NA"),
          &Data<float>("?displayPeriod@Profile@Jet@@0MA"),
          &Data<float>("?sortFreq@Profile@Jet@@0MA"),
          &Data<float>("?peekResetFreq@Profile@Jet@@0MA"),
          &Data<void*>("?sort_func@Profile@Jet@@0P6AHPBX0@ZA"),
          Get<const char**>(orig, "?tracePoint@Profile@Jet@@0PAUTraceInfo@12@A"),
          &Data<unsigned int>("?numTracePoints@Profile@Jet@@0IA"),
          &Data<unsigned int>("?traceFrame@Profile@Jet@@0IA"),
          Get<unsigned char*>(orig, "?traceData@Profile@Jet@@0PAUFrameTrace@12@A"),
          reinterpret_cast<float*>(base + 0xae210),
          &Data<bool>("?childtime@QuickTimer@Jet@@0_NA"),
          &Data<int>("?currentFrame@QuickTimer@Jet@@0HA"),
          Get<unsigned long*>(orig, "?frameTimes@QuickTimer@Jet@@0PAKA"),
          Get<QInfo**>(orig, "?funcs@QuickTimer@Jet@@0PAPAUInfo@12@A"),
          &Data<unsigned long long>("?lastFrameStart@QuickTimer@Jet@@0_KA"),
          &Data<int>("?logEvery@QuickTimer@Jet@@0HA"),
          &Data<long>("?nextid@QuickTimer@Jet@@0VProtectedLong@2@A"),
          &Data<int>("?numFrames@QuickTimer@Jet@@0HA"),
          &Data<void*>("?threadInfo@QuickTimer@Jet@@0PAUThreadInfo@12@A")};
    Save(ps.enabled, 4);
    Save(ps.nextid, 4);
    Save(ps.funcs, 0x1000 * 4);
    Save(ps.depth, 4);
    Save(ps.current, 4);
    Save(ps.childtime, 1);
    Save(ps.displayPeriod, 4);
    Save(ps.sortFreq, 4);
    Save(ps.peekResetFreq, 4);
    Save(ps.sortFunc, 4);
    Save(ps.tracePoint, 0x80 * 4);
    Save(ps.numTracePoints, 4);
    Save(ps.traceFrame, 4);
    Save(ps.traceData, 0x404 * 128);
    Save(ps.acc, 12);
    Save(ps.qChildtime, 1);
    Save(ps.qCurrentFrame, 4);
    Save(ps.qFrameTimes, 16);
    Save(ps.qFuncs, 0x40 * 4);
    Save(ps.qLastFrameStart, 8);
    Save(ps.qLogEvery, 4);
    Save(ps.qNextid, 4);
    Save(ps.qNumFrames, 4);
    Save(ps.qThreadInfo, 4);
    Save(Get<void*>(orig, "?freq@Time@Jet@@0_KA"), 8);
    Scenario({orig, "orig"}, scripts);
    const std::vector<long long> a = logv;
    const std::vector<int> la = lines;
    Scenario({ours, "ours"}, scripts);
    const std::vector<long long>& r = logv;
    size_t bad = 0;
    for (size_t i = 0; i < a.size() || i < r.size(); ++i) {
        const long long x = i < a.size() ? a[i] : -999, y = i < r.size() ? r[i] : -999;
        if (x != y && ++bad <= 10)
            std::printf("  step %zu (line %d): orig %lld ours %lld\n", i, i < la.size() ? la[i] : 0, x, y);
    }
    std::printf("console/profile scenario: %zu results each, %zu mismatches\n", a.size(), bad);
    return bad ? 1 : 0;
}
