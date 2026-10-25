// Differential test of KernelStandard unit U12a (Exception.cpp, Log.cpp): the same scripted scenario runs once
// through KernelStandard_7c_orig.dll's exports and once through ours; every observable result goes into a log and
// the two logs must match. The Log statics are the original's (our module imports them), so each side starts from
// a cleared state (saved and restored around the run) and writes its own log files, whose bytes are compared.
//
// usage: diff_ks_log.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static HMODULE ours, orig;
static std::string tmpDir;

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
static void LogText(const char* s, int line)
{
    if (!s) {
        Log(-1, line);
        return;
    }
    Log(static_cast<long long>(std::strlen(s)), line);
    Log(Hash(s, std::strlen(s)), line);
}
#define LT(s) LogText((s), __LINE__)

template <class F>
static F Slot(void* obj, int n) { return reinterpret_cast<F>((*static_cast<void***>(obj))[n]); }

template <class T>
static T& Data(const char* name) { return *Get<T*>(orig, name); }

using Ctor0 = void*(__fastcall*)(void*, void*);
using CtorFL = void*(__fastcall*)(void*, void*, const char*, int);
using CtorFLM = void*(__fastcall*)(void*, void*, const char*, int, const char*);
using CtorCopy = void*(__fastcall*)(void*, void*, const void*);
using CtorS = void*(__fastcall*)(void*, void*, const char*);
using Dtor = void(__fastcall*)(void*, void*);
using Deleting = void*(__fastcall*)(void*, void*, unsigned int);
using Fmt = void(__cdecl*)(void*, const char*, ...);
using VFmt = void(__cdecl*)(void*, const char*, char*);
using M0 = void(__cdecl*)(void*);
using MB = void(__cdecl*)(void*, bool);
using GetS = const char*(__cdecl*)(const void*);
using GetI = int(__cdecl*)(const void*);
using GetB = bool(__cdecl*)(const void*);
using NewT = void*(__cdecl*)(unsigned int);

// ---- Exception / Debugger

static void ExceptionScenario(const Side& s)
{
    const NewT alloc = Get<NewT>(orig, "?New@Alloc@Jet@@SAPAXI@Z");
    const auto vft = s.f<void*>("??_7Exception@Jet@@6B@");
    const auto setReason = s.f<Fmt>("?SetReasonf@Exception@Jet@@QAAXPBDZZ");
    const auto addReason = s.f<Fmt>("?AddReasonf@Exception@Jet@@QAAXPBDZZ");
    const auto setDesc = s.f<Fmt>("?SetDescriptionf@Exception@Jet@@QAAXPBDZZ");
    const auto vsetReason = s.f<VFmt>("?VSetReasonf@Exception@Jet@@QAAXPBDPAD@Z");
    const auto vsetDesc = s.f<VFmt>("?VSetDescriptionf@Exception@Jet@@QAAXPBDPAD@Z");
    auto dump = [&](const unsigned char* e, int line) {
        Log(*reinterpret_cast<void* const*>(e) == vft, line);
        Log(Hash(e + 4, 0x110c), line);
    };
    unsigned char a[0x1110], b[0x1110];
    for (int k = 0; k < 4; ++k) {
        std::memset(a, 0xcd + k, sizeof a);
        if (k == 0) s.f<Ctor0>("??0Exception@Jet@@QAE@XZ")(a, nullptr);
        if (k == 1) s.f<CtorFL>("??0Exception@Jet@@QAE@PBDH@Z")(a, nullptr, "file.cpp", 42);
        if (k == 2) s.f<CtorFLM>("??0Exception@Jet@@QAE@PBDH0@Z")(a, nullptr, "f2.cpp", -7, "the reason");
        if (k == 3) s.f<CtorFLM>("??0Exception@Jet@@QAE@PBDH0@Z")(a, nullptr, nullptr, 0, "");
        dump(a, __LINE__);
        L(s.f<GetI>("?Line@Exception@Jet@@QBAHXZ")(a));
        L(s.f<GetS>("?File@Exception@Jet@@QBAPBDXZ")(a) == *reinterpret_cast<const char**>(a + 0x110c));
        LT(Slot<GetS>(a, 1)(a));
        L(Slot<GetB>(a, 2)(a));
        L(Slot<GetS>(a, 3)(a) == reinterpret_cast<const char*>(a + 4));
        L(Slot<GetS>(a, 4)(a) == reinterpret_cast<const char*>(a + 0x1004));
        LT(s.f<GetS>("?Description@Exception@Jet@@UBAPBDXZ")(a));
        LT(s.f<GetS>("?Reason@Exception@Jet@@UBAPBDXZ")(a));
        s.f<MB>("?SetHardwareException@Exception@Jet@@QAAX_N@Z")(a, k & 1);
        L(s.f<GetB>("?HardwareException@Exception@Jet@@QBA_NXZ")(a));
        L(s.f<GetB>("?IsHardwareException@Exception@Jet@@UBA_NXZ")(a));
        setReason(a, "%s %d %5.2f %x %c", "abc", -12, 3.14159, 0xbeef, 'Z');
        LT(reinterpret_cast<char*>(a + 0x1004));
        addReason(a, "more %u", 7u);
        addReason(a, "%s", "");
        LT(reinterpret_cast<char*>(a + 0x1004));
        setDesc(a, "desc %08X %.3e", 0x1234u, 1.0 / 3);
        LT(reinterpret_cast<char*>(a + 4));
        dump(a, __LINE__);
        {
            int args[3] = {5, 6, 7};
            vsetReason(a, "%d-%d-%d", reinterpret_cast<char*>(args));
            vsetDesc(a, "<%d>", reinterpret_cast<char*>(args + 2));
        }
        dump(a, __LINE__);
        std::memset(b, 0x11, sizeof b);
        s.f<CtorCopy>("??0Exception@Jet@@QAE@ABV01@@Z")(b, nullptr, a);
        dump(b, __LINE__);
        std::memset(b + 4, 0x22, sizeof b - 4);
        s.f<CtorCopy>("??4Exception@Jet@@QAEAAV01@ABV01@@Z")(b, nullptr, a);
        dump(b, __LINE__);
        s.f<M0>("?InitMembers@Exception@Jet@@QAAXXZ")(b);
        dump(b, __LINE__);
        s.f<void(__cdecl*)(void*, int)>("?Log@Exception@Jet@@QAAXH@Z")(b, 3);
        s.f<Dtor>("??1Exception@Jet@@UAE@XZ")(b, nullptr);
        dump(b, __LINE__);
        Slot<Deleting>(a, 0)(a, nullptr, 0);  // the destructor only
        dump(a, __LINE__);
        unsigned char* h = static_cast<unsigned char*>(alloc(0x1110));
        s.f<CtorFL>("??0Exception@Jet@@QAE@PBDH@Z")(h, nullptr, "heap", k);
        Slot<Deleting>(h, 0)(h, nullptr, 1);  // frees through Jet::Alloc
    }
    // Debugger
    L(s.f<bool(__cdecl*)()>("?IsDebuggerAttached@Debugger@Jet@@SA_NXZ")());
    const auto readValid = s.f<bool(__cdecl*)(void*, unsigned int)>("?IsReadValid@Debugger@Jet@@SA_NPAXI@Z");
    L(readValid(a, sizeof a));
    L(readValid(nullptr, 4));
    L(readValid(reinterpret_cast<void*>(0x10), 0));
    s.f<void(__cdecl*)(int, const char*)>("?Output@Debugger@Jet@@SAXW4Level@12@PBD@Z")(1, "diff_ks_log\n");
    s.f<void(__cdecl*)()>("?Break@Debugger@Jet@@SAXXZ")();
    s.f<void(__cdecl*)()>("?LogCallStack@Debugger@Jet@@SAXXZ")();
}

// ---- Log

struct Statics {
    void** clientList;
    void** logFile;
    void** stderrFile;
    double* startTime;
    void** writeLock;
    bool* initialized;
    bool* displayModuleName;
    bool* disabled;
    bool* flush;
};
static Statics st;

static void Clear()
{
    *st.clientList = nullptr;
    *st.logFile = nullptr;
    *st.stderrFile = nullptr;
    *st.startTime = 0;
    *st.writeLock = nullptr;
    *st.initialized = false;
    *st.displayModuleName = false;
    *st.disabled = false;
    *st.flush = false;
}

static std::string ReadFile(const std::string& path)
{
    std::string r;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "<none>";
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) r.append(buf, n);
    std::fclose(f);
    return r;
}

struct Client {
    void *next, *prev;
    int* name;  // AnsiString data {capacity, length, text}
    int* unused;
    int level, kind;
    unsigned char busy, pad[3];
    unsigned char outputs, pad1[3];
    unsigned char format, pad2[3];
};

static void LogClientState(void* c, int line)
{
    Client* k = static_cast<Client*>(c);
    Log(k->level, line);
    Log(k->busy, line);
    Log(k->outputs & 0x0f, line);
    Log(k->format & 0x03, line);
    Log(k->name[1], line);
    if (k->name[0]) LogText(reinterpret_cast<char*>(k->name + 2), line);
}

static void LogRing(int line)
{
    char* r = static_cast<char*>(*st.clientList);
    if (!r) {
        Log(-1, line);
        return;
    }
    Log(*reinterpret_cast<int*>(r + 8), line);
    for (char* c = *reinterpret_cast<char**>(r); c != r; c = *reinterpret_cast<char**>(c)) {
        LogClientState(c, line);
        Log(*reinterpret_cast<char**>(*reinterpret_cast<char**>(c) + 4) == c, line);  // the next one's prev
    }
}

static void LogScenario(const Side& s)
{
    const auto optCtor = s.f<Ctor0>("??0Options@Jet@@QAE@XZ");
    const auto raw = s.f<void(__cdecl*)(void*, const char*)>("?ProcessRaw@Options@Jet@@QAAXPBD@Z");
    const auto init = s.f<void(__cdecl*)(void*)>("?Init@Log@Jet@@SAXPAVOptions@2@@Z");
    const auto done = s.f<void(__cdecl*)()>("?Done@Log@Jet@@SAXXZ");
    const auto shutdown = s.f<void(__cdecl*)(bool)>("?Shutdown@Log@Jet@@SAX_N@Z");
    const auto getClient = s.f<void*(__cdecl*)(const char*)>("?GetClient@Log@Jet@@SAPAVLogClient@2@PBD@Z");
    const auto search = s.f<void*(__cdecl*)(const char*)>("?SearchClientList@Log@Jet@@CAPAVLogClient@2@PBD@Z");
    const auto process = s.f<void(__cdecl*)(void*)>("?Process@Log@Jet@@CAXAAVString@2@@Z");
    const auto out = s.f<Fmt>("?OutputString@LogClient@Jet@@QAAXPBDZZ");
    const auto outV = s.f<VFmt>("?OutputStringV@LogClient@Jet@@QAAXPBDPAD@Z");
    const auto setLogTo = s.f<void(__cdecl*)(void*)>("?SetLogToFile@Log@Jet@@SAXPAU_iobuf@@@Z");
    const auto getLogTo = s.f<void*(__cdecl*)()>("?GetLogToFile@Log@Jet@@SAPAU_iobuf@@XZ");
    const auto strCtor = Get<CtorS>(orig, "??0String@Jet@@QAE@PBD@Z");
    const auto strDtor = Get<Dtor>(orig, "??1String@Jet@@QAE@XZ");

    struct Case {
        const char* options;
        int finish;  // 0 Done, 1 Shutdown(false) then Done, 2 Shutdown(true) then Done
    };
    const Case cases[] = {
        {"-LogFile=%s -Log=Alpha:Warn:mono=off,Gamma:Diag:file=off:window", 0},
        {"-LogFile=%s -LogFlush -LogDisplayModuleName=1 -Log=beta:Prog:newlines=off:displaymodulename=on", 1},
        {"-LogFile=%s -LogAnsi -Log=Alpha:None,Alpha:err:file=OFF:file=on", 2},
        {"-LogFile=%s -NoLog -Log=Delta:Err", 0},
        {"-LogFile= -Log=Alpha:Diag", 1},
        {"-LogFile=no_such_dir_ksl\\x.txt", 0},
        {"-LogDisplayModuleName=0 -LogFlush=0 -LogFile=%s", 1},
    };
    int k = 0;
    for (const Case& c : cases) {
        Clear();
        const std::string path = tmpDir + "diff_ks_log_" + s.tag + std::to_string(k) + ".txt";
        DeleteFileA(path.c_str());
        char line[512];
        std::snprintf(line, sizeof line, c.options, path.c_str());
        void* clients[4];
        clients[0] = getClient("Alpha");
        clients[1] = getClient("beta");
        L(getClient("ALPHA") == clients[0]);
        L(search("alpha") == clients[0]);
        clients[2] = search("Gamma");
        LogRing(__LINE__);
        unsigned char opts[0xc];
        optCtor(opts, nullptr);
        raw(opts, line);
        init(opts);
        L(*st.initialized);
        L(*st.writeLock != nullptr);
        L(*st.logFile != nullptr);
        L(*st.disabled);
        L(*st.displayModuleName);
        L(*st.flush);
        L(*st.stderrFile != nullptr);
        LogRing(__LINE__);
        init(opts);  // a second Init does nothing
        LogRing(__LINE__);
        clients[3] = getClient("Delta");
        *st.startTime = 0;  // no clock in the prefix
        for (int i = 0; i < 4; ++i) {
            Client* cl = static_cast<Client*>(clients[i]);
            for (int kind = 0; kind < 5; ++kind) {
                cl->kind = kind;
                const unsigned char saveFormat = cl->format, saveOutputs = cl->outputs;
                cl->format = static_cast<unsigned char>((cl->format & ~3) | ((kind + i) & 3));
                cl->outputs &= ~2;  // keep the debugger quiet
                out(cl, "line %d of %s: %5.3f %s", kind, s.tag[0] == 'o' ? "x" : "x", kind / 7.0, "end");
                int args[2] = {kind, i};
                outV(cl, "[%d,%d]", reinterpret_cast<char*>(args));
                cl->busy = 1;
                out(cl, "dropped while busy");
                cl->busy = 0;
                cl->format = saveFormat;
                cl->outputs = saveOutputs;
            }
        }
        // the option-line parser
        const char* settings[] = {"", ":", "Alpha", "Eps:Err:err:ERR", "Eps:=on", "Eps:mono=on:mono=off:file=junk",
                                  "Zeta:window=:displaymodulename=OFF", "Zeta:None:Prog", "beta:file", "beta:newlines=on",
                                  "Eta::Warn", "Theta:Diag:mono:window=off:newlines:file=off"};
        for (const char* t : settings) {
            unsigned char str[4];
            strCtor(str, nullptr, t);
            process(str);
            strDtor(str, nullptr);
        }
        LogRing(__LINE__);
        L(getLogTo() == *st.stderrFile);
        if (c.finish == 1) shutdown(false);
        if (c.finish == 2) shutdown(true);
        if (c.finish) {
            L(*st.logFile == nullptr);
            shutdown(false);  // nothing open: nothing happens
        }
        done();
        L(*st.initialized);
        L(*st.logFile == nullptr && *st.writeLock == nullptr && *st.clientList == nullptr);
        const std::string text = ReadFile(path);
        L(text.size());
        L(Hash(text.data(), text.size()));
        if (std::getenv("KSL_DUMP")) std::printf("--- %s %d\n%s\n", s.tag, k, text.c_str());
        DeleteFileA(path.c_str());
        ++k;
    }
    // SetLogToFile / GetLogToFile only (no output through the stream)
    Clear();
    *st.disabled = true;
    setLogTo(nullptr);
    L(*st.disabled);
    setLogTo(reinterpret_cast<void*>(0x1234));
    L(*st.disabled);
    L(getLogTo() == reinterpret_cast<void*>(0x1234));
    Clear();
}

static void Scenario(const Side& s)
{
    logv.clear();
    lines.clear();
    std::printf("%s: exception\n", s.tag);
    ExceptionScenario(s);
    std::printf("%s: log\n", s.tag);
    LogScenario(s);
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
    char t[MAX_PATH];
    GetTempPathA(MAX_PATH, t);
    tmpDir = t;
    SetCurrentDirectoryA(t);
    st = {&Data<void*>("?clientList@Log@Jet@@0PAV?$IList@VLogClient@Jet@@@2@A"),
          &Data<void*>("?logFile@Log@Jet@@0PAVFile@2@A"),
          &Data<void*>("?stderrFile@Log@Jet@@0PAU_iobuf@@A"),
          &Data<double>("?startTime@Log@Jet@@0NA"),
          &Data<void*>("?writeLock@Log@Jet@@0PAVCriticalSection@2@A"),
          &Data<bool>("?initialized@Log@Jet@@0_NA"),
          &Data<bool>("?displayModuleName@Log@Jet@@0_NA"),
          &Data<bool>("?disabled@LogClient@Jet@@0_NA"),
          reinterpret_cast<bool*>(reinterpret_cast<char*>(orig) + 0x7287b)};
    const Statics saved = st;
    void* const v[] = {*st.clientList, *st.logFile, *st.stderrFile, *st.writeLock};
    const double start = *st.startTime;
    const bool b[] = {*st.initialized, *st.displayModuleName, *st.disabled, *st.flush};
    Scenario({orig, "orig"});
    const std::vector<long long> a = logv;
    const std::vector<int> la = lines;
    Scenario({ours, "ours"});
    const std::vector<long long>& r = logv;
    *saved.clientList = v[0];
    *saved.logFile = v[1];
    *saved.stderrFile = v[2];
    *saved.writeLock = v[3];
    *saved.startTime = start;
    *saved.initialized = b[0];
    *saved.displayModuleName = b[1];
    *saved.disabled = b[2];
    *saved.flush = b[3];
    size_t bad = 0;
    for (size_t i = 0; i < a.size() || i < r.size(); ++i) {
        const long long x = i < a.size() ? a[i] : -999, y = i < r.size() ? r[i] : -999;
        if (x != y && ++bad <= 10)
            std::printf("  step %zu (line %d): orig %lld ours %lld\n", i, i < la.size() ? la[i] : 0, x, y);
    }
    std::printf("exception/log scenario: %zu results each, %zu mismatches\n", a.size(), bad);
    return bad ? 1 : 0;
}
