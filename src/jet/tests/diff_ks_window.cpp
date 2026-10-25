// Differential test of KernelStandard unit U11 (Window.cpp): OS, Window, WindowPlacement. Each scenario is drawn
// once, then run on objects made by our KernelStandard_7c.dll and by KernelStandard_7c_orig.dll (both in this
// process, called through raw exports, one side after the other: both register the window class "JET" on the
// exe's instance); every observable result goes into a log per side and the logs must match.
//
// Everything runs on a thread attached to a private desktop, so no window shows on the user's screen and
// WindowPlacement (which saves and restores every visible top-level window) only sees the test's windows.
// OS: construction with drawn command lines, Init (options through ExceptionHandler::Main), the getters, copies,
// CreateNewWindow / CreateDynamicLibrary. Window: Open with options, the four create calls, top-level or child of
// a test parent, then a drawn script of Show / Move / Resize / CalculateWindowArea / SetHostState / SetTitle /
// SetContextOnly / SetHandleMessages / hooks and dispatchers (logging, swallowing by a hash, hooks clearing
// themselves or adding others) / SendMessage / PostMessage / HandleMessages / focus and capture / message boxes
// (MessageBoxA patched in both modules' import tables) / ExceptionHandler::CriticalShutdown / Close / copies,
// with the state (flags, styles, areas, title, class) logged after each step. Only messages the test sends (and
// the few the window makes from them) are logged with their parameters, so system traffic does not matter.
//
// usage: diff_ks_window.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

struct Rect {
    int x, y, w, h;
};
struct Size {
    int w, h;
};

// ---- logging per side
static int side;
static std::vector<std::string> logs[2];
static void Log(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    static const bool live = getenv("KSW_LIVE") != nullptr;  // every line as it comes (to find a hang)
    if (live) std::printf("%d| %s\n", side, buf);
    logs[side].push_back(buf);
}

static uint32_t Hash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0xC2B2AE3Du;
    return h ^ (h >> 13);
}

// ---- the exports of one side, by mangled name (Alloc, AnsiString and ExceptionHandler are the original's)
static HMODULE mods[2], origMod;
static std::map<std::string, void*> fns[2];
static void* Fn(const char* name)
{
    auto it = fns[side].find(name);
    if (it != fns[side].end()) return it->second;
    void* p = reinterpret_cast<void*>(GetProcAddress(mods[side], name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return fns[side][name] = p;
}
static void* OrigFn(const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(origMod, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}
template <class R, class... A>
static R Cd(const char* name, A... a)  // __cdecl (members get the object first)
{
    return reinterpret_cast<R(__cdecl*)(A...)>(Fn(name))(a...);
}
template <class R, class... A>
static R Th(const char* name, void* self, A... a)  // __thiscall through __fastcall
{
    return reinterpret_cast<R(__fastcall*)(void*, void*, A...)>(Fn(name))(self, nullptr, a...);
}
static void* AllocNew(unsigned n) { return reinterpret_cast<void*(__cdecl*)(unsigned)>(OrigFn("?New@Alloc@Jet@@SAPAXI@Z"))(n); }
static void AllocDelete(void* p) { reinterpret_cast<void(__cdecl*)(void*)>(OrigFn("?Delete@Alloc@Jet@@SAXPAX@Z"))(p); }

// ---- the current window and its parent (names for logging)
static void* win;    // the Window object
static HWND parentWnd;
static bool posKnown;  // false after CreateDefault without a parent (cascaded positions differ between runs)
static HWND Handle() { return win ? *static_cast<HWND*>(win) : nullptr; }
static const char* Role(HWND h)
{
    if (!h) return "0";
    if (h == Handle()) return "self";
    if (h == parentWnd) return "parent";
    return "other";
}

// The messages logged with their parameters. What the test sends carries 0x5aNN in lParam (WM_SIZE excepted);
// the same ids from the system (activation, paint, input) depend on the desktop's history and are not logged.
static const unsigned kMagic = 0x5a00;
static bool Watched(unsigned m, LPARAM lp)
{
    switch (m) {
    case WM_DESTROY: case WM_SIZE: case WM_CLOSE: case WM_USER + 2:
        return true;
    case WM_USER + 1:  // HandleMessages posts it and dispatches one message: which one depends on the queue
        return false;
    case WM_ACTIVATE: case WM_PAINT: case WM_ACTIVATEAPP: case WM_SYSCOMMAND: case 0x100: case 0x101: case 0x102:
    case 0x109: case 0x10a: case 0x1ff: case 0x200: case 0x201: case 0x20d: case 0x20e: case WM_USER + 7:
        return (static_cast<unsigned>(lp) & 0xffffff00) == kMagic;
    }
    return false;
}

static unsigned LP(unsigned, LPARAM lp) { return static_cast<unsigned>(lp); }

// ---- hooks and dispatchers: contexts 1..6
static unsigned hookCalls[2], dispCalls[2];
static const char kRegHook[] = "?RegisterWindowHook@Window@Jet@@QAAXP6I_NPAX0AAH@Z0@Z";
static const char kClearHook[] = "?ClearWindowHook@Window@Jet@@QAAXPAX@Z";
static const char kRegDisp[] = "?RegisterWindowDispatcher@Window@Jet@@QAAXP6I_NPAX0@Z0@Z";
static const char kClearDisp[] = "?ClearWindowDispatcher@Window@Jet@@QAAXPAX@Z";

struct HookMessage {
    HWND window;
    unsigned id;
    WPARAM wp;
    LPARAM lp;
};
static bool __fastcall TestHook(void* ctx, void* message, int& result);
static bool __fastcall TestHook(void* ctx, void* message, int& result)
{
    const HookMessage* m = static_cast<const HookMessage*>(message);
    static const bool all = getenv("KSW_ALL") != nullptr;  // every message a hook sees, on stdout only
    if (all) std::printf("%d raw %p %x %x %x\n", side, m->window, m->id, static_cast<unsigned>(m->wp), LP(m->id, m->lp));
    if (!Watched(m->id, m->lp)) return false;
    const unsigned n = hookCalls[side]++;
    const uint32_t h = Hash(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx)) * 131 + m->id, n);
    Log("hook %u %s %x %x %x res=%d", static_cast<unsigned>(reinterpret_cast<uintptr_t>(ctx)), Role(m->window), m->id,
        static_cast<unsigned>(m->wp), LP(m->id, m->lp), result);
    switch (h % 10) {
    case 0:
    case 1:
        result = static_cast<int>(h >> 8);
        return true;  // swallowed
    case 2:
        Cd<void>(kClearHook, win, ctx);  // clears itself (the walk ends)
        return false;
    case 3:
        Cd<void>(kRegHook, win, &TestHook, reinterpret_cast<void*>(static_cast<uintptr_t>(1 + h % 6)));
        return false;
    }
    return false;
}

static bool __fastcall TestDispatcher(void* ctx, void* msg)
{
    const MSG* m = static_cast<const MSG*>(msg);
    if (!Watched(m->message, m->lParam)) return false;
    const unsigned n = dispCalls[side]++;
    const uint32_t h = Hash(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx)) * 977 + m->message, n);
    Log("disp %u %s %x %x %x", static_cast<unsigned>(reinterpret_cast<uintptr_t>(ctx)), Role(m->hwnd), m->message,
        static_cast<unsigned>(m->wParam), LP(m->message, m->lParam));
    return h % 4 == 0;  // taken: not dispatched
}

// ---- the parent window (a test class) logs what reaches it
static LRESULT CALLBACK ParentProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (Watched(m, lp)) Log("parent got %x %x %x", m, static_cast<unsigned>(wp), LP(m, lp));
    if (m == WM_DESTROY) return 0;  // never quits
    return DefWindowProcA(h, m, wp, lp);
}

// ---- MessageBoxA, patched in both modules' import tables
static int boxAnswer;
static int WINAPI FakeMessageBox(HWND h, LPCSTR text, LPCSTR caption, UINT type)
{
    Log("MessageBox %s [%s] [%s] %x", Role(h), text ? text : "(null)", caption ? caption : "(null)", type);
    return boxAnswer;
}
static void PatchImport(HMODULE m, const char* dll, const char* fn, void* to)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        if (_stricmp(reinterpret_cast<char*>(base + d->Name), dll) != 0) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<char*>(ibn->Name), fn) != 0) continue;
            DWORD old;
            VirtualProtect(&slots->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            slots->u1.Function = reinterpret_cast<uintptr_t>(to);
            VirtualProtect(&slots->u1.Function, sizeof(void*), old, &old);
            return;
        }
    }
    std::printf("import %s!%s not found\n", dll, fn);
    std::exit(2);
}

// ---- a scenario
struct Op {
    int kind;
    uint32_t a, b, c;
};
struct Scenario {
    std::string cmdLine;
    std::vector<std::string> keys;  // asked through Options::Get
    bool withParent;
    bool open;
    bool optionsToOpen;
    int create;  // 0 windowed, 1 2D, 2 default, 3 full screen, 4 none
    int x, y, w, h;
    bool hooksFirst;  // a hook before the window exists (sees the creation messages)
    bool defaultCtor;
    std::string title;
    std::vector<Op> ops;
};

static const char* kTokens[] = {"-HideWindow", "+HideWindow", "-ShowFPS-", "+Render=OpenGL", "-Sound=None",
                                "+Width=640", "-Log=File", "+InputMouse=Win32", "-Fullscreen", "+Fullscreen=1",
                                "-x", "+mode=a,b,c", "+ShowFPS"};
static const char* kKeys[] = {"HideWindow", "ShowFPS", "Render", "Sound", "Width", "Log", "InputMouse",
                              "Fullscreen", "x", "mode", "Input", "Network", "LogFlush", "StencilBits", "missing"};
static const unsigned kMsgs[] = {WM_PAINT, WM_ACTIVATE, WM_ACTIVATEAPP, WM_SIZE, WM_SYSCOMMAND, 0x100, 0x101, 0x102,
                                 0x109, 0x10a, 0x1ff, 0x200, 0x201, 0x20d, 0x20e, WM_USER + 7};
// SC_SCREENSAVE / SC_KEYMENU / SC_MONITORPOWER only to a top-level window, which swallows them (DefWindowProc
// would start the screen saver, a menu loop or turn the monitor off); no SC_SIZE / SC_MOVE (modal loops)
static const unsigned kSysCommands[] = {SC_CLOSE, SC_MINIMIZE, SC_RESTORE, SC_MAXIMIZE,
                                        SC_SCREENSAVE, SC_KEYMENU, SC_MONITORPOWER};

static Scenario Draw(std::mt19937& rng)
{
    Scenario s;
    const int nt = rng() % 5;
    for (int i = 0; i < nt; ++i) {
        if (!s.cmdLine.empty()) s.cmdLine += ' ';
        s.cmdLine += kTokens[rng() % (sizeof kTokens / sizeof *kTokens)];
    }
    for (const char* k : kKeys)
        if (rng() % 2) s.keys.push_back(k);
    s.withParent = rng() % 3 == 0;
    s.open = rng() % 12 != 0;
    s.optionsToOpen = rng() % 4 != 0;
    s.create = rng() % 9;
    s.create = s.create < 3 ? 0 : s.create < 5 ? 1 : s.create < 6 ? 2 : s.create < 8 ? 3 : 4;
    s.x = static_cast<int>(rng() % 400) - 50;
    s.y = static_cast<int>(rng() % 300) - 50;
    s.w = 1 + static_cast<int>(rng() % 700);
    s.h = 1 + static_cast<int>(rng() % 500);
    s.hooksFirst = rng() % 3 == 0;
    s.defaultCtor = rng() % 5 == 0;
    const int tl = rng() % 4 == 0 ? static_cast<int>(rng() % 0x110) : static_cast<int>(rng() % 20);
    for (int i = 0; i < tl; ++i) s.title += static_cast<char>('a' + rng() % 26);
    s.ops.resize(rng() % 60);
    for (Op& op : s.ops) {
        op.kind = rng() % 26;
        op.a = rng();
        op.b = rng();
        op.c = rng();
    }
    return s;
}

static void LogState(const char* when)
{
    const unsigned char* b = static_cast<const unsigned char*>(win);
    HWND h = Handle();
    Log("%s: flags %02x active %x handle %s visible %d host %d hookcnt %d dispcnt %d bpp0 %d shutdown %d", when,
        b[0x11c] & 0x3f, b[0x105] & 3, h ? (IsWindow(h) ? "live" : "dead") : "0",
        Cd<bool>("?GetVisible@Window@Jet@@QBA_NXZ", win), Cd<bool>("?GetHostState@Window@Jet@@QBA_NXZ", win),
        *reinterpret_cast<const int*>(b + 0x10c), *reinterpret_cast<const int*>(b + 0x114),
        *reinterpret_cast<const int*>(b + 0x12c) == 0, *reinterpret_cast<void* const*>(b + 0x130) != nullptr);
    Log("  title [%s] internal [%s] parent %s main %s", Cd<const char*>("?GetTitle@Window@Jet@@QAAPBDXZ", win),
        Cd<const char*>("?GetInternalName@Window@Jet@@QAAPBDXZ", win),
        Role(Cd<HWND>("?GetParent@Window@Jet@@QBAPAUHWND__@@XZ", win)),
        Cd<void*>("?GetMainWindow@Window@Jet@@SAPAV12@XZ") == win ? "this" : "other");
    WNDCLASSEXA wc = {sizeof wc};
    Log("  class %d", GetClassInfoExA(GetModuleHandleA(nullptr), "JET", &wc) != 0);
    if (h && IsWindow(h)) {
        char text[512] = {};
        GetWindowTextA(h, text, sizeof text);
        if (b[0x11c] & 1) std::strcpy(text, "(fps)");  // ShowFPS: the title depends on the clock
        Log("  style %08x ex %08x text [%s] obj %d iconic %d", GetWindowLongA(h, GWL_STYLE),
            GetWindowLongA(h, GWL_EXSTYLE) & ~WS_EX_WINDOWEDGE, text,
            reinterpret_cast<void*>(GetWindowLongA(h, 0)) == win, Cd<bool>("?IsIconic@Window@Jet@@QBA_NXZ", win));
        Rect r;
        Cd<Rect*>("?GetClientArea@Window@Jet@@QBA?AURect@2@XZ", win, &r);
        Log("  client %d %d %d %d", r.x, r.y, r.w, r.h);
        Cd<Rect*>("?GetArea@Window@Jet@@QBA?AURect@2@XZ", win, &r);
        if (posKnown)
            Log("  area %d %d %d %d", r.x, r.y, r.w, r.h);
        else
            Log("  area size %d %d", r.w, r.h);
        Cd<Rect*>("?GetParentArea@Window@Jet@@QBA?AURect@2@XZ", win, &r);
        Log("  parent area %d %d %d %d", r.x, r.y, r.w, r.h);
        POINT p = {5, 7};
        Cd<void>("?LocalToGlobal@Window@Jet@@QBAXAAUPoint@2@@Z", win, &p);
        if (posKnown) Log("  local (5,7) -> %d %d", p.x, p.y);
    } else {
        Rect r;
        Cd<Rect*>("?GetClientArea@Window@Jet@@QBA?AURect@2@XZ", win, &r);
        Log("  client %d %d %d %d", r.x, r.y, r.w, r.h);
        Cd<Rect*>("?GetParentArea@Window@Jet@@QBA?AURect@2@XZ", win, &r);
        Log("  parent area %d %d %d %d", r.x, r.y, r.w, r.h);
    }
}

static void DrainQueue()
{
    MSG m;
    while (PeekMessageA(&m, nullptr, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT) {
            Log("drained WM_QUIT %d", static_cast<int>(m.wParam));
            continue;
        }
        if (Watched(m.message, m.lParam)) Log("drained %s %x", Role(m.hwnd), m.message);
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
}

// an OS with the scenario's command line; returns it (the Window test uses its options)
static void* MakeOS(const Scenario& s, const char* app)
{
    void* os = AllocNew(0x18);
    Th<void*>("??0OS@Jet@@QAE@PBD0PAXPAUHWND__@@@Z", os, app, s.cmdLine.c_str(), reinterpret_cast<void*>(0x4321),
              parentWnd);
    return os;
}

static void RunOS(const Scenario& s, uint32_t seed)
{
    const std::string app = "app" + std::to_string(seed % 1000);
    void* os = MakeOS(s, seed % 7 == 0 ? nullptr : app.c_str());
    auto GetAppName = [&](void* o) { return Cd<const char*>("?GetAppName@OS@Jet@@QBAPBDXZ", o); };
    const char* an = GetAppName(os);
    Log("os app [%s] cmd same %d inst %p parent %s name [%s]", an ? an : "(null)",
        Cd<const char*>("?GetCmdLine@OS@Jet@@QBAPBDXZ", os) == s.cmdLine.c_str(),
        Cd<void*>("?GetInstance@OS@Jet@@QBAPAXXZ", os), Role(Cd<HWND>("?GetParent@OS@Jet@@QBAPAUHWND__@@XZ", os)),
        Cd<const char*>("?GetName@OS@Jet@@QBAPBDXZ", os));
    const char* defs = Cd<const char*>("?GetDefaultOptions@OS@Jet@@SAPBDXZ");
    const char* const* data = static_cast<const char* const*>(Fn("?defaultOptions@OS@Jet@@0PBDB"));
    Log("defaults same %d [%s]", defs == *data, defs);
    Log("init %d", Th<bool>("?Init@OS@Jet@@QAE_NXZ", os));
    void* opts = Cd<void*>("?GetOptions@OS@Jet@@QBAPAVOptions@2@XZ", os);
    std::string has;
    for (const std::string& k : s.keys)
        has += Cd<bool>("?Get@Options@Jet@@QBA_NPBD@Z", opts, k.c_str()) ? '1' : '0';
    Log("options %s", has.c_str());
    // the InitCallBack directly (processes the defaults and the command line again)
    if (seed % 5 == 0) Log("callback %d", Cd<int>("?InitCallBack@OS@Jet@@CAHPAX@Z", os));
    Log("window %p", Cd<void*>("?GetWindow@OS@Jet@@QBAPAVWindow@2@XZ", os));
    Cd<void>("?SetWindow@OS@Jet@@QAAXPAVWindow@2@@Z", os, reinterpret_cast<void*>(0x1230));
    Cd<void>("?SetParent@OS@Jet@@QAAXPAUHWND__@@@Z", os, reinterpret_cast<HWND>(static_cast<uintptr_t>(seed % 3)));
    Log("window %p parent %p", Cd<void*>("?GetWindow@OS@Jet@@QBAPAVWindow@2@XZ", os),
        Cd<HWND>("?GetParent@OS@Jet@@QBAPAUHWND__@@XZ", os));
    Cd<void>("?SetParent@OS@Jet@@QAAXPAUHWND__@@@Z", os, parentWnd);
    // CreateNewWindow: a Window of the OS's parent
    void* w = Cd<void*>("?CreateNewWindow@OS@Jet@@QBAPAVWindow@2@XZ", os);
    void* const saved = win;
    win = w;
    LogState("new window");
    win = saved;
    Th<void>("??1Window@Jet@@QAE@XZ", w);
    AllocDelete(w);
    // CreateDynamicLibrary: this module's vftable, no handle
    void** dl = Cd<void**>("?CreateDynamicLibrary@OS@Jet@@QBAPAVDynamicLibrary@2@XZ", os);
    Log("dynlib vft %d handle %p", dl[0] == Fn("??_7DynamicLibrary@Jet@@6B@"), dl[1]);
    reinterpret_cast<void*(__fastcall*)(void*, void*, unsigned)>(static_cast<void**>(dl[0])[0])(dl, nullptr, 1);
    // copies share the options: only their names are destroyed
    void* copy = AllocNew(0x18);
    Th<void*>("??0OS@Jet@@QAE@ABV01@@Z", copy, os);
    const char* ca = GetAppName(copy);
    Log("copy app [%s] opts same %d cmd same %d", ca ? ca : "(null)",
        Cd<void*>("?GetOptions@OS@Jet@@QBAPAVOptions@2@XZ", copy) == opts,
        Cd<const char*>("?GetCmdLine@OS@Jet@@QBAPBDXZ", copy) == s.cmdLine.c_str());
    Scenario other = s;
    void* target = MakeOS(other, "other");
    void* targetOpts = Cd<void*>("?GetOptions@OS@Jet@@QBAPAVOptions@2@XZ", target);
    Th<void*>("??4OS@Jet@@QAEAAV01@ABV01@@Z", target, os);
    const char* ta = GetAppName(target);
    Log("assigned app [%s] opts same %d", ta ? ta : "(null)",
        Cd<void*>("?GetOptions@OS@Jet@@QBAPAVOptions@2@XZ", target) == opts);
    auto DropName = [](void* o) {
        reinterpret_cast<void(__fastcall*)(void*, void*)>(OrigFn("??1AnsiString@Jet@@QAE@XZ"))(static_cast<char*>(o) + 0x14,
                                                                                               nullptr);
        AllocDelete(o);
    };
    DropName(copy);
    DropName(target);
    (void)targetOpts;  // leaked, as with the original's operator=
    Th<void>("??1OS@Jet@@QAE@XZ", os);
    AllocDelete(os);
}

static void RunWindow(const Scenario& s)
{
    hookCalls[side] = dispCalls[side] = 0;
    if (s.withParent) {
        parentWnd = CreateWindowExA(0, "KSWParent", "parent", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 30, 40, 500, 400,
                                    nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
        DrainQueue();
    }
    void* os = MakeOS(s, "win");
    Th<bool>("?Init@OS@Jet@@QAE_NXZ", os);
    void* opts = Cd<void*>("?GetOptions@OS@Jet@@QBAPAVOptions@2@XZ", os);

    win = AllocNew(0x134);
    std::memset(win, s.title.size() & 1 ? 0xff : 0, 0x134);  // the bytes the constructor leaves
    if (s.defaultCtor)
        Th<void>("??_FWindow@Jet@@QAEXXZ", win);
    else
        Th<void*>("??0Window@Jet@@QAE@PAUHWND__@@@Z", win, parentWnd);
    posKnown = true;
    LogState("made");
    if (s.hooksFirst) Cd<void>(kRegHook, win, &TestHook, reinterpret_cast<void*>(1));
    if (s.open) {
        Log("open %d", Cd<bool>("?Open@Window@Jet@@QAA_NPBDPBVOptions@2@@Z", win, s.title.c_str(),
                                s.optionsToOpen ? opts : nullptr));
        LogState("opened");
    }
    bool made = false;
    const HWND p = s.defaultCtor ? nullptr : parentWnd;
    switch (s.create) {
    case 0:
        made = Cd<bool>("?CreateWindowedHost@Window@Jet@@QAA_NHHHHPAX@Z", win, s.x, s.y, s.w, s.h, nullptr);
        Log("windowed %d", made);
        break;
    case 1:
        made = Cd<bool>("?Create2DWindow@Window@Jet@@QAA_NHHHHPAX@Z", win, s.x, s.y, s.w, s.h, nullptr);
        Log("2d %d", made);
        break;
    case 2:
        posKnown = p != nullptr;
        made = Cd<bool>("?CreateDefault@Window@Jet@@QAA_NXZ", win);
        Log("default %d", made);
        break;
    case 3:
        made = Cd<bool>("?CreateFullscreenHost@Window@Jet@@QAA_NHHPAX@Z", win, s.w, s.h, nullptr);
        Log("fullscreen %d", made);
        break;
    }
    LogState("created");

    for (const Op& op : s.ops) {
        const bool b = op.a & 1;
        switch (op.kind) {
        case 0:
            Cd<void>("?Show@Window@Jet@@QAAX_N@Z", win, b);
            Log("show %d", b);
            break;
        case 1: {
            const Rect r = {static_cast<int>(op.b % 300) - 20, static_cast<int>(op.c % 200) - 20,
                            static_cast<int>(op.b >> 20) % 600, static_cast<int>(op.c >> 20) % 400};
            Log("move %d", Cd<bool>("?Move@Window@Jet@@QAA_NABURect@2@_N@Z", win, &r, b));
            break;
        }
        case 2: {
            const Size z = {static_cast<int>(op.b % 600), static_cast<int>(op.c % 400)};
            Log("resize %d", Cd<bool>("?Resize@Window@Jet@@QAA_NABUSize@2@_N@Z", win, &z, b));
            break;
        }
        case 3: {
            const Size z = {static_cast<int>(op.b % 900) - 100, static_cast<int>(op.c % 700) - 100};
            Size out;
            Cd<Size*>("?CalculateWindowArea@Window@Jet@@QBA?AUSize@2@ABU32@_N@Z", win, &out, &z, b);
            Log("calc %d %d %d -> %d %d", z.w, z.h, b, out.w, out.h);
            break;
        }
        case 4:
            Log("hoststate %d -> %d", b, Cd<bool>("?SetHostState@Window@Jet@@QAA_N_N@Z", win, b));
            // the frame follows the new style only on the next frame change: forced here, or the client area
            // depends on what the desktop did before
            if (Handle() && IsWindow(Handle()))
                SetWindowPos(Handle(), nullptr, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            break;
        case 5: {
            std::string t;
            const unsigned n = op.b % 4 == 0 ? 0xf8 + op.c % 16 : op.c % 30;
            for (unsigned i = 0; i < n; ++i) t += static_cast<char>('A' + (op.c >> (i % 24)) % 26);
            Log("settitle %u -> %d", n, Cd<bool>("?SetTitle@Window@Jet@@QAA_NPBD@Z", win, t.c_str()));
            break;
        }
        case 6:
            Cd<void>("?SetContextOnly@Window@Jet@@QAAX_N@Z", win, b);
            Log("contextonly %d", b);
            break;
        case 7:
            Cd<void>("?SetHandleMessages@Window@Jet@@QAAX_N@Z", win, b);
            Log("handlemessages %d", b);
            break;
        case 8:
        case 9:
            Cd<void>(kRegHook, win, &TestHook, reinterpret_cast<void*>(static_cast<uintptr_t>(1 + op.b % 6)));
            Log("reghook %u", 1 + op.b % 6);
            break;
        case 10:
            Cd<void>(kClearHook, win, reinterpret_cast<void*>(static_cast<uintptr_t>(1 + op.b % 7)));
            Log("clearhook %u", 1 + op.b % 7);
            break;
        case 11:
            Cd<void>(kRegDisp, win, &TestDispatcher, reinterpret_cast<void*>(static_cast<uintptr_t>(1 + op.b % 6)));
            Log("regdisp %u", 1 + op.b % 6);
            break;
        case 12:
            Cd<void>(kClearDisp, win, reinterpret_cast<void*>(static_cast<uintptr_t>(1 + op.b % 7)));
            Log("cleardisp %u", 1 + op.b % 7);
            break;
        case 13:
        case 14:
        case 15: {  // SendMessage to the window
            const unsigned m = kMsgs[op.b % (sizeof kMsgs / sizeof *kMsgs)];
            WPARAM wp = op.c % 4;
            const bool child = *reinterpret_cast<HWND*>(static_cast<char*>(win) + 0x118) != nullptr;
            if (m == WM_SYSCOMMAND) wp = kSysCommands[op.c % (child ? 4 : 7)];
            if (m == WM_ACTIVATE) wp = (op.c & 0x30000) | (op.c % 3);
            const LPARAM lp = static_cast<LPARAM>(m == WM_SIZE ? (op.a & 0x01ff01ff) : kMagic | (op.a & 0xff));
            if (Handle() && IsWindow(Handle()))
                Log("send %x %x %x -> %d", m, static_cast<unsigned>(wp), static_cast<unsigned>(lp),
                    static_cast<int>(SendMessageA(Handle(), m, wp, lp)));
            break;
        }
        case 16:
        case 17: {  // posted to the window or the parent, handled by the next HandleMessages
            const unsigned m = kMsgs[op.b % (sizeof kMsgs / sizeof *kMsgs)];
            const HWND to = op.c % 4 == 0 && parentWnd ? parentWnd : Handle();
            if (to && IsWindow(to)) {
                PostMessageA(to, m, m == WM_SYSCOMMAND ? SC_RESTORE : op.c % 4,
                             static_cast<LPARAM>(m == WM_SIZE ? (op.a & 0x00ff00ff) : kMagic | (op.a & 0xff)));
                Log("post %s %x", Role(to), m);
            }
            break;
        }
        case 18:
        case 19:
            Log("handle -> %d", Cd<bool>("?HandleMessages@Window@Jet@@QAA_NXZ", win));
            break;
        case 20:
            if (Handle() && IsWindow(Handle())) {
                Cd<void>("?SetFocus@Window@Jet@@QBAXXZ", win);
                Cd<void>("?SetCapture@Window@Jet@@QBAX_N@Z", win, b);
                Log("focus %d capture %d", Cd<bool>("?HasFocus@Window@Jet@@QBA_NXZ", win), GetCapture() == Handle());
                Cd<void>("?SetCapture@Window@Jet@@QBAX_N@Z", win, false);
            }
            break;
        case 21:
            boxAnswer = op.b % 2 ? IDYES : IDNO;
            Cd<void>("?Message@Window@Jet@@QAAXPBD@Z", win, "hello");
            Log("confirm -> %d", Cd<bool>("?MessageConfirm@Window@Jet@@QAA_NPBD@Z", win, "sure?"));
            break;
        case 22:  // the crash handler's shutdown callbacks (Window's unregisters the class)
            reinterpret_cast<void(__cdecl*)()>(OrigFn("?CriticalShutdown@ExceptionHandler@Jet@@SAXXZ"))();
            Log("criticalshutdown");
            break;
        case 23:
            Cd<void>("?Close@Window@Jet@@QAAXXZ", win);
            Log("close");
            break;
        case 24: {
            Log("active app %d wnd %d bpp>0 %d", Cd<bool>("?IsActiveApp@Window@Jet@@QAA_NXZ", win),
                Cd<bool>("?IsActiveWnd@Window@Jet@@QAA_NXZ", win),
                Cd<int>("?GetHostBitsPerPixel@Window@Jet@@QAAHXZ", win) > 0);
            void* h = Cd<void*>("?GetHandle@Window@Jet@@QBAPAXXZ", win);
            Cd<void>("?SetHandle@Window@Jet@@QAAXPAX@Z", win, reinterpret_cast<void*>(0x77));
            Log("sethandle %d", *static_cast<void**>(win) == reinterpret_cast<void*>(0x77));
            Cd<void>("?SetHandle@Window@Jet@@QAAXPAX@Z", win, h);
            Cd<void>("?Error@Window@Jet@@QAAXPAD@Z", win, const_cast<char*>("test"));
            break;
        }
        case 25: {  // operator=: a plain copy
            void* copy = AllocNew(0x134);
            Th<void*>("??4Window@Jet@@QAEAAV01@ABV01@@Z", copy, win);
            Log("assign same %d", std::memcmp(copy, win, 0x134) == 0);
            AllocDelete(copy);
            break;
        }
        }
        LogState("op");
    }
    Th<void>("??1Window@Jet@@QAE@XZ", win);
    Log("destroyed main %d", Cd<void*>("?GetMainWindow@Window@Jet@@SAPAV12@XZ") == nullptr);
    WNDCLASSEXA wc = {sizeof wc};
    Log("class after %d", GetClassInfoExA(GetModuleHandleA(nullptr), "JET", &wc) != 0);
    AllocDelete(win);
    win = nullptr;
    // the class can outlive the window (Open without Close's flag, or a failed Unregister): never into the next side
    UnregisterClassA("JET", GetModuleHandleA(nullptr));
    Th<void>("??1OS@Jet@@QAE@XZ", os);
    AllocDelete(os);
    if (parentWnd) DestroyWindow(parentWnd);
    parentWnd = nullptr;
    DrainQueue();
}

// WindowPlacement: three windows saved, moved and restored (a hidden one is not saved)
static void RunPlacement()
{
    HWND w[4];
    for (int i = 0; i < 4; ++i) {
        w[i] = CreateWindowExA(0, "KSWParent", "placed", WS_OVERLAPPEDWINDOW | (i == 3 ? 0 : WS_VISIBLE), 20 + 70 * i,
                               30 + 50 * i, 200 + 10 * i, 150, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    }
    ShowWindow(w[2], SW_MINIMIZE);
    DrainQueue();
    Cd<void>("?SaveWindowPositions@WindowPlacement@Jet@@SAXXZ");
    for (int i = 0; i < 4; ++i) SetWindowPos(w[i], nullptr, 300 - 30 * i, 200, 100, 90, SWP_NOZORDER);
    ShowWindow(w[2], SW_RESTORE);
    ShowWindow(w[3], SW_SHOW);
    DrainQueue();
    Cd<void>("?RestoreWindowPositions@WindowPlacement@Jet@@SAXXZ");
    DrainQueue();
    for (int i = 0; i < 4; ++i) {
        RECT r;
        GetWindowRect(w[i], &r);
        WINDOWPLACEMENT pl = {sizeof pl};
        GetWindowPlacement(w[i], &pl);
        Log("placed %d: %d %d %d %d show %u normal %d %d", i, r.left, r.top, r.right, r.bottom, pl.showCmd,
            pl.rcNormalPosition.left, pl.rcNormalPosition.top);
    }
    for (HWND h : w) DestroyWindow(h);
    DrainQueue();
}

static int scenarios;
static unsigned long long checks, bad;

static void Compare(const char* what, int sc)
{
    if (getenv("KSW_DUMP"))
        for (const std::string& l : logs[0]) std::printf("%s %d| %s\n", what, sc, l.c_str());
    const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
    for (size_t i = 0; i < n; ++i) {
        ++checks;
        const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
        const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
        if (x != y) {
            if (++bad <= 10) std::printf("%s scenario %d line %zu:\n  ours %s\n  orig %s\n", what, sc, i, x.c_str(), y.c_str());
            break;
        }
    }
    logs[0].clear();
    logs[1].clear();
}

static DWORD WINAPI Worker(void* desk)
{
    if (!SetThreadDesktop(static_cast<HDESK>(desk))) {
        std::printf("SetThreadDesktop failed %lu\n", GetLastError());
        return 2;
    }
    WNDCLASSEXA wc = {sizeof wc};
    wc.lpfnWndProc = ParentProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "KSWParent";
    RegisterClassExA(&wc);

    for (side = 0; side < 2; ++side) RunPlacement();
    Compare("placement", 0);

    std::mt19937 rng(4321);
    const bool trace = getenv("KSW_TRACE") != nullptr;
    const int only = getenv("KSW_ONLY") ? std::atoi(getenv("KSW_ONLY")) : -1;  // replay one scenario
    for (int sc = 0; sc < scenarios; ++sc) {
        const Scenario s = Draw(rng);
        const uint32_t seed = rng();
        if (only >= 0 && sc != only) continue;
        if (trace) std::printf("sc %d\n", sc);
        for (side = 0; side < 2; ++side) RunOS(s, seed);
        Compare("os", sc);
        for (side = 0; side < 2; ++side) RunWindow(s);
        Compare("window", sc);
    }
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_ks_window <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    scenarios = argc > 2 ? std::atoi(argv[2]) : 500;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    origMod = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!origMod || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    mods[0] = getenv("KSW_SWAP") ? origMod : ours;
    mods[1] = getenv("KSW_SWAP") ? ours : origMod;
    PatchImport(ours, "USER32.dll", "MessageBoxA", reinterpret_cast<void*>(&FakeMessageBox));
    PatchImport(origMod, "USER32.dll", "MessageBoxA", reinterpret_cast<void*>(&FakeMessageBox));

    HDESK desk = CreateDesktopA("diff_ks_window", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desk) {
        std::printf("CreateDesktop failed %lu\n", GetLastError());
        return 2;
    }
    HANDLE t = CreateThread(nullptr, 0, Worker, desk, 0, nullptr);
    WaitForSingleObject(t, INFINITE);
    DWORD rc = 0;
    GetExitCodeThread(t, &rc);
    CloseHandle(t);
    CloseDesktop(desk);
    if (rc) return static_cast<int>(rc);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
