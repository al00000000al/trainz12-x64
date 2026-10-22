// Differential test of InputWin32_7c: our module against InputWin32_7c_orig.dll in one process.
//
// Nothing in InputWin32 is exported but the plugin table, so each module's import table is patched instead: every
// Win32 call it makes (user32, winmm, GetVersionExA), the Jet Window / OS / Time / Alloc members, the InputDevice
// constructor, destructor and events, and the plugin type base are replaced by stubs that log their arguments and
// answer from a seeded generator. Each side runs the same scenario from the same seed: the factory from
// JetGetPluginsInputWin32 creates the driver, which enumerates and creates devices; the devices then get random
// property calls, updates, critical shutdown / startup and window messages through the hook they registered. The
// call logs and the objects' memory (pointers mapped to allocation numbers and RTTI names) must be identical.
//
// usage: diff_iw32.exe <dir with InputWin32_7c.dll, InputWin32_7c_orig.dll and the Jet modules> [scenarios]
#include <windows.h>
#include <mmsystem.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

static HMODULE mods[2];
static unsigned long long checks, bad;
static int side;  // 0 ours, 1 original

// ---- per-side record: the call log, the allocations, the registered window hooks
using Hook = bool(__fastcall*)(void*, void*, int&);
struct Alloc {
    uintptr_t at;
    size_t size;
};
struct Record {
    std::vector<std::string> log;
    std::vector<Alloc> allocs;
    std::vector<std::pair<Hook, void*>> hooks;
    std::map<uintptr_t, std::string> vtables;  // RTTI "Class@offset" of the module's vftables
    uintptr_t lo, hi;                          // the module image
} rec[2];

static std::mt19937 srng;  // the stubs' answers; reseeded identically for both sides
static unsigned R() { return srng(); }
static unsigned long msCounter;

static unsigned char fakeOS[0x40];
static unsigned char fakeWindow[0x200];
static void* g_window;
static const HWND kHwnd = reinterpret_cast<HWND>(0x4321);
static int wmEndOfMessages = 0x8101, wmModeChanged = 0x8102;
static unsigned long doubleClickTime;

static std::string Ptr(const void* p)
{
    const auto v = reinterpret_cast<uintptr_t>(p);
    char buf[64];
    if (!v) return "null";
    if (p == fakeOS) return "os";
    if (p == fakeWindow) return "win";
    const Record& r = rec[side];
    for (size_t k = 0; k < r.allocs.size(); ++k)
        if (v >= r.allocs[k].at && v < r.allocs[k].at + r.allocs[k].size) {
            std::snprintf(buf, sizeof(buf), "h%zu+%zx", k, v - r.allocs[k].at);
            return buf;
        }
    // only exact vftable addresses: other data (key bytes) can look like an address in the module
    auto it = r.vtables.find(v);
    if (it != r.vtables.end()) return "vt:" + it->second;
    std::snprintf(buf, sizeof(buf), "%08zx", v);
    return buf;
}

static void Log(const char* fmt, ...)
{
    char buf[512];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    rec[side].log.push_back(buf);
}

// the object's memory as dwords, pointers named
static void Snapshot(const char* what, const void* obj, size_t size)
{
    std::string s = what;
    s += ":";
    const auto* d = static_cast<const uint32_t*>(obj);
    for (size_t i = 0; i < size / 4; ++i) {
        const std::string p = Ptr(reinterpret_cast<const void*>(static_cast<uintptr_t>(d[i])));
        char buf[80];
        if (p.size() == 8 && p.find_first_not_of("0123456789abcdef") == std::string::npos)
            std::snprintf(buf, sizeof(buf), " %x", d[i]);
        else
            std::snprintf(buf, sizeof(buf), " %s", p.c_str());
        s += buf;
    }
    rec[side].log.push_back(s);
}

static const char* Text(const void* pstring)  // a PString's text (node +0x18)
{
    const auto* node = *static_cast<const char* const*>(pstring);
    return node ? node + 0x18 : "(null)";
}

// ---- Jet stubs
static void* __cdecl S_GetWindow(void*) { return g_window; }
static void __cdecl S_RegisterHook(void* w, Hook h, void* ctx)
{
    Log("RegisterWindowHook %s %s", Ptr(w).c_str(), Ptr(ctx).c_str());
    rec[side].hooks.push_back({h, ctx});
}
static void __cdecl S_ClearHook(void* w, void* ctx)
{
    Log("ClearWindowHook %s %s", Ptr(w).c_str(), Ptr(ctx).c_str());
    auto& hooks = rec[side].hooks;
    for (size_t i = 0; i < hooks.size(); ++i)
        if (hooks[i].second == ctx) {
            hooks.erase(hooks.begin() + i);
            break;
        }
}
struct JRect {
    int x, y, w, h;
};
static JRect* __cdecl S_GetClientArea(void* w, JRect* r)
{
    r->x = R() % 3 ? 0 : static_cast<int>(R() % 50);
    r->y = R() % 3 ? 0 : static_cast<int>(R() % 50);
    r->w = static_cast<int>(R() % 2000) - 2;
    r->h = static_cast<int>(R() % 1500) - 2;
    Log("GetClientArea %s -> %d %d %d %d", Ptr(w).c_str(), r->x, r->y, r->w, r->h);
    return r;
}
static void __cdecl S_LocalToGlobal(void* w, int* p)
{
    Log("LocalToGlobal %s %d %d", Ptr(w).c_str(), p[0], p[1]);
    p[0] += static_cast<int>(R() % 400) - 50;
    p[1] += static_cast<int>(R() % 300) - 50;
}
static bool __cdecl S_IsIconic(void*)
{
    const bool r = R() % 6 == 0;
    Log("IsIconic -> %d", r);
    return r;
}
static bool __cdecl S_IsActiveApp(void*)
{
    const bool r = R() % 4 != 0;
    Log("IsActiveApp -> %d", r);
    return r;
}
static void __cdecl S_SetCapture(void* w, bool on) { Log("SetCapture %s %d", Ptr(w).c_str(), on); }
static unsigned long __cdecl S_Ms() { return ++msCounter; }
static void* __cdecl S_New(unsigned int size)
{
    auto* p = static_cast<unsigned char*>(std::malloc(size));
    std::memset(p, 0xa5, size);
    rec[side].allocs.push_back({reinterpret_cast<uintptr_t>(p), size});
    Log("New %x -> %s", size, Ptr(p).c_str());
    return p;
}
static void __cdecl S_Delete(void* p) { Log("Delete %s", Ptr(p).c_str()); }  // kept: the numbering stays

// InputDevice / InputDriver / PluginType (thiscall members as fastcall with a dummy edx)
static void* __fastcall S_DeviceCtor(unsigned char* self, void*, void* driver)
{
    Log("InputDevice() %s %s", Ptr(self).c_str(), Ptr(driver).c_str());
    std::memset(self + 4, 0, 0x238 - 4);
    *reinterpret_cast<int*>(self + 0x10) = 1;  // the reference count
    *reinterpret_cast<void**>(self + 0x22c) = driver;
    self[0x234] = 1;
    return self;
}
static void __fastcall S_DeviceDtor(void* self) { Log("~InputDevice %s", Ptr(self).c_str()); }
static void* __fastcall S_DriverCtor(unsigned char* self, void*, int mostDerived)
{
    Log("InputDriver() %s %d", Ptr(self).c_str(), mostDerived);
    *reinterpret_cast<void**>(self + 8) = nullptr;  // PluginInstance::pluginFactory
    return self;
}
static void __fastcall S_DriverDtor(void*) {}  // ours only: the original inlines it (empty)
static void* __fastcall S_TypeCtor(unsigned char* self, void*, const void* name, const uint32_t* id, const uint32_t* parent)
{
    Log("PluginType() %s '%s' %08x-%08x-%08x-%08x parent %08x-%08x-%08x-%08x", Ptr(self).c_str(), Text(name), id[0],
        id[1], id[2], id[3], parent[0], parent[1], parent[2], parent[3]);
    std::memset(self + 4, 0, 0x1c);
    return self;
}
static void __cdecl S_TypeAddRef(const void* t) { Log("PluginType::AddReference %s", Ptr(t).c_str()); }
static void __cdecl S_TypeRelease(const void* t) { Log("PluginType::ReleaseReference %s", Ptr(t).c_str()); }

static void __cdecl S_KeyEvent(void* d, unsigned long key, int flags, unsigned long t)
{
    Log("KeyEvent %s %lx %x %lu", Ptr(d).c_str(), key, flags, t);
}
static void __cdecl S_CharEvent(void* d, unsigned short ch, int flags, unsigned long t)
{
    Log("CharEvent %s %x %x %lu", Ptr(d).c_str(), ch, flags, t);
}
static void __cdecl S_ButtonEvent(void* d, int b, int flags, unsigned long t)
{
    Log("ButtonEvent %s %d %x %lu", Ptr(d).c_str(), b, flags, t);
}
static void __cdecl S_AxisEvent(void* d, int axis, int v, int delta, unsigned long t)
{
    Log("AxisEvent %s %d %d %d %lu", Ptr(d).c_str(), axis, v, delta, t);
}
static void __cdecl S_PovEvent(void* d, int pov, int v, unsigned long t)
{
    Log("PovEvent %s %d %d %lu", Ptr(d).c_str(), pov, v, t);
}
static void __cdecl S_PropertyEvent(void* d, int id, unsigned long t)
{
    Log("PropertyChangedEvent %s %d %lu", Ptr(d).c_str(), id, t);
}
static void __cdecl S_LostEvent(void* d, unsigned long t) { Log("InputLostEvent %s %lu", Ptr(d).c_str(), t); }
static void __cdecl S_GetEvent(void* d, unsigned long t) { Log("InputGetEvent %s %lu", Ptr(d).c_str(), t); }

// ---- Win32 stubs
static SHORT WINAPI S_GetKeyState(int vk)
{
    const SHORT r = static_cast<SHORT>(R());
    Log("GetKeyState %x -> %x", vk, static_cast<unsigned short>(r));
    return r;
}
static BOOL WINAPI S_GetKeyboardState(PBYTE kb)
{
    for (int i = 0; i < 256; ++i) kb[i] = static_cast<BYTE>(R());
    const BOOL r = R() % 10 != 0;
    Log("GetKeyboardState -> %d", r);
    return r;
}
static int WINAPI S_GetSystemMetrics(int i)
{
    const int r = static_cast<int>(R() % 3);
    Log("GetSystemMetrics %d -> %d", i, r);
    return r;
}
static UINT WINAPI S_RegisterWindowMessageA(LPCSTR s)
{
    const UINT r = R() % 5 == 0 ? 0 : 0xC100 + R() % 4;
    Log("RegisterWindowMessage %s -> %x", s, r);
    return r;
}
static HWND WINAPI S_FindWindowA(LPCSTR c, LPCSTR n)
{
    const HWND r = R() % 2 ? reinterpret_cast<HWND>(0x5678) : nullptr;
    Log("FindWindow %s %s -> %p", c, n, r);
    return r;
}
static LRESULT WINAPI S_SendMessageA(HWND h, UINT m, WPARAM w, LPARAM l)
{
    const LRESULT r = R() % 3;
    Log("SendMessage %p %x %x %x -> %d", h, m, static_cast<unsigned>(w), static_cast<unsigned>(l), static_cast<int>(r));
    return r;
}
static UINT WINAPI S_GetDoubleClickTime()
{
    const UINT r = R() % 1000;
    Log("GetDoubleClickTime -> %u", r);
    return r;
}
static BOOL WINAPI S_ClipCursor(const RECT* r)
{
    if (r)
        Log("ClipCursor %ld %ld %ld %ld", r->left, r->top, r->right, r->bottom);
    else
        Log("ClipCursor null");
    return TRUE;
}
static BOOL WINAPI S_SetCursorPos(int x, int y)
{
    Log("SetCursorPos %d %d", x, y);
    return TRUE;
}
static HCURSOR WINAPI S_SetCursor(HCURSOR c)
{
    const HCURSOR r = R() % 4 ? reinterpret_cast<HCURSOR>(static_cast<uintptr_t>(0x9000 + R() % 16)) : nullptr;
    Log("SetCursor %p -> %p", c, r);
    return r;
}
static HCURSOR WINAPI S_LoadCursorA(HINSTANCE i, LPCSTR n)
{
    Log("LoadCursor %p %p", i, n);
    return reinterpret_cast<HCURSOR>(0x7f00aa);
}
static BOOL WINAPI S_GetCursorPos(POINT* p)
{
    p->x = static_cast<LONG>(R() % 1400) - 100;
    p->y = static_cast<LONG>(R() % 1000) - 100;
    Log("GetCursorPos -> %ld %ld", p->x, p->y);
    return TRUE;
}
static BOOL WINAPI S_GetWindowRect(HWND h, RECT* r)
{
    r->left = static_cast<LONG>(R() % 300);
    r->top = static_cast<LONG>(R() % 300);
    r->right = r->left + static_cast<LONG>(R() % 1200);
    r->bottom = r->top + static_cast<LONG>(R() % 900);
    Log("GetWindowRect %p -> %ld %ld %ld %ld", h, r->left, r->top, r->right, r->bottom);
    return TRUE;
}
static BOOL WINAPI S_ReleaseCapture()
{
    Log("ReleaseCapture");
    return TRUE;
}
static UINT WINAPI S_SendInput(UINT n, LPINPUT in, int size)
{
    Log("SendInput %u %d", n, size);
    for (UINT i = 0; i < n; ++i)
        Log("  input %lu vk %x scan %x flags %lx time %lx extra %zx", in[i].type, in[i].ki.wVk, in[i].ki.wScan,
            in[i].ki.dwFlags, in[i].ki.time, static_cast<size_t>(in[i].ki.dwExtraInfo));
    return n;
}
static BOOL WINAPI S_GetVersionExA(OSVERSIONINFOA* v)
{
    Log("GetVersionEx size %lx", v->dwOSVersionInfoSize);
    v->dwMajorVersion = 3 + R() % 5;
    v->dwMinorVersion = R() % 3;
    v->dwBuildNumber = R() % 10000;
    v->dwPlatformId = R() % 3;
    const BOOL r = R() % 8 != 0;
    Log("  -> %d %lu.%lu platform %lu", r, v->dwMajorVersion, v->dwMinorVersion, v->dwPlatformId);
    return r;
}
static DWORD WINAPI S_timeGetTime()
{
    const DWORD r = R();
    Log("timeGetTime -> %lx", r);
    return r;
}
// a joystick is there or not for a whole scenario, with the same caps every time (as a real one)
static unsigned scenarioSeed;
static MMRESULT WINAPI S_joyGetDevCapsA(UINT_PTR id, JOYCAPSA* c, UINT size)
{
    Log("joyGetDevCaps %u %u", static_cast<unsigned>(id), size);
    std::mt19937 own(scenarioSeed ^ static_cast<unsigned>(id * 0x9e3779b9u));
    auto R = [&own] { return static_cast<unsigned>(own()); };
    if (R() % 4 == 0) return JOYERR_UNPLUGGED;
    std::memset(c, 0, sizeof(*c));
    c->wMid = static_cast<WORD>(R());
    c->wPid = static_cast<WORD>(R());
    std::snprintf(c->szPname, sizeof(c->szPname), "Pad %u", R() % 100);
    UINT* range = &c->wXmin;  // x, y, z min/max
    for (int i = 0; i < 6; i += 2) {
        range[i] = R() % 4 == 0 ? 0 : R() % 1000;
        range[i + 1] = R() % 6 == 0 ? range[i] : range[i] + R() % 70000;
    }
    c->wNumButtons = R() % 33;
    c->wPeriodMin = R() % 20;
    c->wPeriodMax = R() % 2000;
    UINT* range2 = &c->wRmin;  // r, u, v min/max
    for (int i = 0; i < 6; i += 2) {
        range2[i] = R() % 1000;
        range2[i + 1] = R() % 6 == 0 ? range2[i] : range2[i] + R() % 70000;
    }
    c->wCaps = R() & 0x7f;
    c->wMaxAxes = 6;
    c->wNumAxes = R() % 7;
    c->wMaxButtons = 32;
    return JOYERR_NOERROR;
}
static MMRESULT WINAPI S_joyGetPos(UINT id, JOYINFO* info)
{
    std::memset(info, 0, sizeof(*info));
    const MMRESULT r = R() % 3 == 0 ? JOYERR_UNPLUGGED : JOYERR_NOERROR;
    Log("joyGetPos %u -> %u", id, r);
    return r;
}
static MMRESULT WINAPI S_joyGetPosEx(UINT id, JOYINFOEX* info)
{
    Log("joyGetPosEx %u size %lx flags %lx", id, info->dwSize, info->dwFlags);
    if (R() % 6 == 0) return JOYERR_UNPLUGGED;
    DWORD* axes = &info->dwXpos;
    for (int i = 0; i < 6; ++i) axes[i] = R() % 3 == 0 ? 0 : R() % 71000;
    info->dwButtons = R() % 3 == 0 ? 0 : R();
    static const DWORD povs[] = {0xffff, 0, 9000, 18000, 27000, 0x12345, 4500};
    info->dwPOV = povs[R() % 7];
    return JOYERR_NOERROR;
}
static MMRESULT WINAPI S_joyGetThreshold(UINT id, UINT* t)
{
    *t = R() % 5000;
    const MMRESULT r = R() % 4 == 0 ? JOYERR_PARMS : JOYERR_NOERROR;
    Log("joyGetThreshold %u -> %u %u", id, r, *t);
    return r;
}
static MMRESULT WINAPI S_joySetThreshold(UINT id, UINT t)
{
    const MMRESULT r = R() % 4 == 0 ? JOYERR_PARMS : JOYERR_NOERROR;
    Log("joySetThreshold %u %u -> %u", id, t, r);
    return r;
}

static const std::map<std::string, void*> kStubs = {
    {"?GetWindow@OS@Jet@@QBAPAVWindow@2@XZ", &S_GetWindow},
    {"?RegisterWindowHook@Window@Jet@@QAAXP6I_NPAX0AAH@Z0@Z", &S_RegisterHook},
    {"?ClearWindowHook@Window@Jet@@QAAXPAX@Z", &S_ClearHook},
    {"?GetClientArea@Window@Jet@@QBA?AURect@2@XZ", &S_GetClientArea},
    {"?LocalToGlobal@Window@Jet@@QBAXAAUPoint@2@@Z", &S_LocalToGlobal},
    {"?IsIconic@Window@Jet@@QBA_NXZ", &S_IsIconic},
    {"?IsActiveApp@Window@Jet@@QAA_NXZ", &S_IsActiveApp},
    {"?SetCapture@Window@Jet@@QBAX_N@Z", &S_SetCapture},
    {"?WM_END_OF_MESSAGES@Window@Jet@@2HB", &wmEndOfMessages},
    {"?WM_MODE_CHANGED@Window@Jet@@2HB", &wmModeChanged},
    {"?Ms@Time@Jet@@SAKXZ", &S_Ms},
    {"?New@Alloc@Jet@@SAPAXI@Z", &S_New},
    {"?Delete@Alloc@Jet@@SAXPAX@Z", &S_Delete},
    {"??0InputDevice@Jet@@QAE@PAVInputDriver@1@@Z", &S_DeviceCtor},
    {"??1InputDevice@Jet@@UAE@XZ", &S_DeviceDtor},
    {"??0InputDriver@Jet@@QAE@XZ", &S_DriverCtor},
    {"??1InputDriver@Jet@@UAE@XZ", &S_DriverDtor},
    {"??0PluginType@Jet@@QAE@ABVPString@1@ABVUId@1@1@Z", &S_TypeCtor},
    {"?AddReference@PluginType@Jet@@UBAXXZ", &S_TypeAddRef},
    {"?ReleaseReference@PluginType@Jet@@UBAXXZ", &S_TypeRelease},
    {"?KeyEvent@InputDevice@Jet@@IAAXKHK@Z", &S_KeyEvent},
    {"?CharEvent@InputDevice@Jet@@IAAXGHK@Z", &S_CharEvent},
    {"?ButtonEvent@InputDevice@Jet@@IAAXHHK@Z", &S_ButtonEvent},
    {"?AxisEvent@InputDevice@Jet@@IAAXHHHK@Z", &S_AxisEvent},
    {"?PovEvent@InputDevice@Jet@@IAAXHHK@Z", &S_PovEvent},
    {"?PropertyChangedEvent@InputDevice@Jet@@IAAXW4DevicePropertyID@2@K@Z", &S_PropertyEvent},
    {"?InputLostEvent@InputDevice@Jet@@IAAXK@Z", &S_LostEvent},
    {"?InputGetEvent@InputDevice@Jet@@IAAXK@Z", &S_GetEvent},
    {"?doubleClickTime@InputUIExtension@Jet@@2KA", &doubleClickTime},
    {"GetKeyState", &S_GetKeyState},
    {"GetKeyboardState", &S_GetKeyboardState},
    {"GetSystemMetrics", &S_GetSystemMetrics},
    {"RegisterWindowMessageA", &S_RegisterWindowMessageA},
    {"FindWindowA", &S_FindWindowA},
    {"SendMessageA", &S_SendMessageA},
    {"GetDoubleClickTime", &S_GetDoubleClickTime},
    {"ClipCursor", &S_ClipCursor},
    {"SetCursorPos", &S_SetCursorPos},
    {"SetCursor", &S_SetCursor},
    {"LoadCursorA", &S_LoadCursorA},
    {"GetCursorPos", &S_GetCursorPos},
    {"GetWindowRect", &S_GetWindowRect},
    {"ReleaseCapture", &S_ReleaseCapture},
    {"SendInput", &S_SendInput},
    {"GetVersionExA", &S_GetVersionExA},
    {"timeGetTime", &S_timeGetTime},
    {"joyGetDevCapsA", &S_joyGetDevCapsA},
    {"joyGetPos", &S_joyGetPos},
    {"joyGetPosEx", &S_joyGetPosEx},
    {"joyGetThreshold", &S_joyGetThreshold},
    {"joySetThreshold", &S_joySetThreshold},
};

// points the module's import table entries named in kStubs at the stubs; returns the names it found
static std::vector<std::string> PatchImports(HMODULE m)
{
    std::vector<std::string> found;
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk
                                                                                       : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            const char* name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name;
            auto it = kStubs.find(name);
            if (it == kStubs.end()) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(it->second);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
            found.push_back(name);
        }
    }
    return found;
}

// every vftable of the module by its RTTI complete object locator: address -> "Class@offset"
static void FindVtables(HMODULE m, Record& r)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const size_t size = nt->OptionalHeader.SizeOfImage;
    r.lo = reinterpret_cast<uintptr_t>(base);
    r.hi = r.lo + size;
    std::map<uintptr_t, std::string> cols;
    for (size_t j = 0; j + 20 <= size; j += 4) {
        const auto* col = reinterpret_cast<const uint32_t*>(base + j);
        if (col[0] != 0 || col[3] < r.lo || col[3] >= r.hi - 12) continue;
        const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(col[3]) + 8);
        if (std::memcmp(name, ".?A", 3) != 0) continue;
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%s@%x", name, col[1]);
        cols[reinterpret_cast<uintptr_t>(col)] = buf;
    }
    for (size_t k = 0; k + 4 <= size; k += 4) {
        auto it = cols.find(*reinterpret_cast<const uint32_t*>(base + k));
        if (it != cols.end()) r.vtables[reinterpret_cast<uintptr_t>(base + k + 4)] = it->second;
    }
}

// ---- calls through the vftables (__cdecl members)
template <class Ret, class... A>
static Ret VCall(void* obj, int slot, A... a)
{
    using F = Ret(__cdecl*)(void*, A...);
    return reinterpret_cast<F>((*static_cast<void***>(obj))[slot])(obj, a...);
}

struct Uid {
    uint32_t d[4];
};
static Uid uidKeyboard, uidMouse, uidJoy1, uidJoy2;

static void DumpCaps(const unsigned char* c)
{
    const auto* u = reinterpret_cast<const uint32_t*>(c);
    Log("caps type %x id %08x-%08x-%08x-%08x '%s' '%s' caps %x buttons %u %x axes %u %x %x %x", u[0], u[1], u[2], u[3],
        u[4], Text(c + 0x14), Text(c + 0x18), u[7], u[8], u[9], u[10], u[11], u[12], u[13]);
}

static void DumpList(void* head)
{
    if (!head) {
        Log("list empty");
        return;
    }
    auto* chunk = static_cast<unsigned char*>(head);
    do {
        const uint32_t count = *reinterpret_cast<uint32_t*>(chunk + 8);
        Log("chunk %s count %u", Ptr(chunk).c_str(), count);
        for (uint32_t i = 0; i < count; ++i) DumpCaps(chunk + 0xc + 0x38 * i);
        chunk = *reinterpret_cast<unsigned char**>(chunk);
    } while (chunk != head);
}

// ---- one scenario: the actions are drawn before the sides run, the stubs reseed per side
struct Action {
    int kind;
    unsigned a, b, c, d;
    float dt;
};

static const unsigned kMessages[] = {
    WM_ACTIVATEAPP, WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_DEADCHAR, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_IME_CHAR,
    WM_SYSCOMMAND, WM_DESTROY, WM_MOVE, WM_SIZE, 4, 0, 1, 0x1b, 0x1d, 0x111, 0x113, WM_MOUSEMOVE, WM_MOUSEMOVE,
    WM_MOUSEMOVE, 0x201, 0x202, 0x203, 0x204, 0x205, 0x206, 0x207, 0x208, 0x209, WM_MOUSEWHEEL, WM_MOUSEWHEEL,
    WM_XBUTTONDOWN, WM_XBUTTONUP, 0x20d, 0x1ff, 0x8101, 0x8101, 0x8102, 0xC100, 0xC101, 0xC102, 0xC103};

static void RunDevice(unsigned char* dev, const std::vector<Action>& actions, size_t size)
{
    Snapshot("device", dev, size);
    for (const Action& act : actions) {
        dev[0x234] = act.d % 8 != 0;  // valid
        fakeWindow[0x11c] = static_cast<unsigned char>(act.d >> 8);
        switch (act.kind) {
        case 0: {  // GetCaps
            unsigned char caps[0x38] = {};
            VCall<void>(dev, 4, caps);
            DumpCaps(caps);
            break;
        }
        case 1:
        case 2: {  // GetProperty / SetProperty
            static const unsigned char indices[] = {0, 1, 2, 3, 7, 8, 0x3b, 0x46, 0x47, 0x3b, 0x46, 0x47};
            uint32_t p[4];
            p[0] = act.a % 11;
            p[1] = act.b;
            reinterpret_cast<unsigned char*>(p)[4] = act.b % 2 ? indices[act.c % 12] : static_cast<unsigned char>(act.c);
            reinterpret_cast<unsigned char*>(p)[5] = static_cast<unsigned char>(act.c >> 8) % 3;
            p[2] = act.c % 3 ? act.d % 70000 - 1000 : act.d;
            p[3] = act.a;
            const bool r = act.kind == 1 ? VCall<bool>(dev, 5, p) : VCall<bool>(dev, 6, p);
            Log("%s -> %d: %x %x %x %x", act.kind == 1 ? "GetProperty" : "SetProperty", r, p[0], p[1], p[2], p[3]);
            break;
        }
        case 3:
            VCall<void>(dev, 14, act.dt);
            break;
        case 4:
            VCall<void>(dev, 16);
            Log("CriticalShutdown");
            break;
        case 5:
            VCall<void>(dev, 17);
            Log("CriticalStartup");
            break;
        default: {  // a window message through the device's hook
            Hook h = nullptr;
            for (auto& e : rec[side].hooks)
                if (e.second == dev) h = e.first;
            if (!h) {
                Log("no hook");
                break;
            }
            struct {
                HWND hwnd;
                unsigned id;
                uintptr_t w;
                intptr_t l;
            } m = {kHwnd, kMessages[act.a % (sizeof(kMessages) / sizeof(kMessages[0]))], act.b, static_cast<intptr_t>(act.c)};
            if (act.d % 2 == 0) m.w = act.b % 256;                  // a virtual key / small value
            if (act.d % 7 == 0) m.w = SC_CLOSE;
            if (act.d % 11 == 0) m.w = static_cast<uintptr_t>(static_cast<short>(act.b) & 0xffff0000);
            if (m.id == WM_MOUSEMOVE && act.d % 2) m.l = static_cast<intptr_t>(((act.c % 1600) << 16) | (act.b % 2000));
            int result = 0;
            const bool r = h(dev, &m, result);
            Log("hook %x %x %x -> %d %d", m.id, static_cast<unsigned>(m.w), static_cast<unsigned>(m.l), r, result);
            break;
        }
        }
        Snapshot("device", dev, size);
    }
}

static void Scenario(unsigned seed, int devicePick, const std::vector<Action>& actions, int windowMode,
                     const std::vector<int>& enumTypes)
{
    using GetPlugins = void(__cdecl*)();
    using ReleaseRef = bool(__fastcall*)(void*);
    static ReleaseRef release = reinterpret_cast<ReleaseRef>(
        GetProcAddress(GetModuleHandleA("KernelStandard_7c.dll"), "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ"));
    for (side = 0; side < 2; ++side) {
        Record& r = rec[side];
        r.log.clear();
        r.allocs.clear();
        r.hooks.clear();
        srng.seed(seed);
        scenarioSeed = seed;
        msCounter = 0;
        std::memset(fakeWindow, 0, sizeof(fakeWindow));
        *reinterpret_cast<HWND*>(fakeWindow) = windowMode == 2 ? nullptr : kHwnd;
        g_window = windowMode == 0 ? nullptr : fakeWindow;

        reinterpret_cast<GetPlugins>(GetProcAddress(mods[side], "JetGetPluginsInputWin32"))();
        if (r.allocs.empty()) {
            Log("no factory");
            continue;
        }
        void* factory = reinterpret_cast<void*>(r.allocs.back().at);
        Snapshot("factory", factory, 0x20);
        auto* driver = VCall<unsigned char*>(factory, 3);
        r.vtables[*reinterpret_cast<uint32_t*>(driver + 4)] = "InputDriverWin32 vbtable";  // no RTTI for it
        Snapshot("driver", driver, 0x1c);
        Log("Init -> %d", VCall<bool>(driver, 0, static_cast<void*>(fakeOS)));
        Snapshot("driver", driver, 0x1c);
        for (int type : enumTypes) {
            void* head = nullptr;
            VCall<void>(driver, 2, &head, type);
            Log("Enumerate %d", type);
            DumpList(head);
        }
        static const Uid* uids[] = {&uidKeyboard, &uidMouse, &uidJoy1, &uidJoy2};
        Uid other = {{0x12345678, 1, 2, 3}};
        const Uid* id = devicePick < 4 ? uids[devicePick] : &other;
        void* ref[1] = {nullptr};
        if (devicePick == 1 && !g_window)
            Log("no mouse without a window (Init fails; both read the null window's flags)");
        else
            VCall<void*>(driver, 3, ref, id);
        auto* dev = static_cast<unsigned char*>(ref[0]);
        Log("CreateDevice %d -> %s", devicePick, Ptr(dev).c_str());
        if (dev) {
            static const size_t sizes[] = {0x240, 0x294, 0x540, 0x540};
            const size_t size = r.allocs.size() && reinterpret_cast<void*>(r.allocs.back().at) == dev
                                    ? r.allocs.back().size
                                    : sizes[devicePick < 4 ? devicePick : 0];
            RunDevice(dev, actions, size);
            Log("release device -> %d", release(dev + 0xc));
        }
        VCall<void>(driver, 4);
        Log("driver CriticalShutdown");
        VCall<void>(driver, 5);
        Log("driver CriticalStartup");
        Log("release driver -> %d", release(driver + 0x14));
        Log("doubleClickTime %lu", doubleClickTime);
    }
    const auto& a = rec[0].log;
    const auto& b = rec[1].log;
    const size_t n = a.size() > b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i) {
        ++checks;
        const std::string& x = i < a.size() ? a[i] : std::string("(end)");
        const std::string& y = i < b.size() ? b[i] : std::string("(end)");
        if (x != y) {
            if (++bad <= 20) {
                std::printf("seed %u device %d line %zu:\n  ours %s\n  orig %s\n", seed, devicePick, i, x.c_str(),
                            y.c_str());
                if (i > 0) std::printf("  after: %s\n", a[i - 1].c_str());
                // for snapshots: the words that differ (index = dword offset in the object)
                std::vector<std::string> wx, wy;
                for (auto* v : {&wx, &wy}) {
                    const std::string& src = v == &wx ? x : y;
                    size_t p = 0;
                    while (p < src.size()) {
                        size_t q = src.find(' ', p);
                        if (q == std::string::npos) q = src.size();
                        v->push_back(src.substr(p, q - p));
                        p = q + 1;
                    }
                }
                for (size_t k = 0; k < wx.size() && k < wy.size(); ++k)
                    if (wx[k] != wy[k])
                        std::printf("  word %zu (+%zx): ours %s orig %s\n", k, (k - 1) * 4, wx[k].c_str(), wy[k].c_str());
            }
            break;  // one report per scenario
        }
    }
    if (std::getenv("DIFF_TRACE"))
        for (const auto& l : a) std::printf("  %s\n", l.c_str());
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(e->ExceptionRecord->ExceptionAddress), &m);
    if (m) GetModuleFileNameA(m, name, MAX_PATH);
    std::printf("crash %08lx at %p (%s +%zx, reading %zx) (side %d), last log: %s\n", e->ExceptionRecord->ExceptionCode,
                e->ExceptionRecord->ExceptionAddress, name,
                static_cast<size_t>(static_cast<char*>(e->ExceptionRecord->ExceptionAddress) - reinterpret_cast<char*>(m)),
                static_cast<size_t>(e->ExceptionRecord->ExceptionInformation[1]), side,
                rec[side].log.empty() ? "-" : rec[side].log.back().c_str());
    std::fflush(stdout);
    ExitProcess(3);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_iw32 <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 2000;
    SetUnhandledExceptionFilter(&Crash);
    SetDllDirectoryA(dir.c_str());
    for (const char* dep : {"KernelStandard_7c", "Kernel_7c", "KernelResource_7c", "InputSystem_7c"})
        if (!LoadLibraryA((dir + "\\" + dep + ".dll").c_str()))
            std::printf("cannot load %s (%lu)\n", dep, GetLastError());
    mods[0] = LoadLibraryA((dir + "\\InputWin32_7c.dll").c_str());
    mods[1] = LoadLibraryA((dir + "\\InputWin32_7c_orig.dll").c_str());
    if (!mods[0] || !mods[1]) {
        std::printf("cannot load the modules (ours %p, orig %p, error %lu)\n", static_cast<void*>(mods[0]),
                    static_cast<void*>(mods[1]), GetLastError());
        return 2;
    }
    for (int s = 0; s < 2; ++s) {
        const auto found = PatchImports(mods[s]);
        std::printf("%s: %zu imports stubbed\n", s ? "orig" : "ours", found.size());
        FindVtables(mods[s], rec[s]);
    }
    HMODULE is = GetModuleHandleA("InputSystem_7c.dll");
    uidKeyboard = *reinterpret_cast<Uid*>(GetProcAddress(is, "?UIdKeyboard@Jet@@3VUId@1@B"));
    uidMouse = *reinterpret_cast<Uid*>(GetProcAddress(is, "?UIdMouse@Jet@@3VUId@1@B"));
    uidJoy1 = *reinterpret_cast<Uid*>(GetProcAddress(is, "?UIdJoystick1@Jet@@3VUId@1@B"));
    uidJoy2 = *reinterpret_cast<Uid*>(GetProcAddress(is, "?UIdJoystick2@Jet@@3VUId@1@B"));

    std::mt19937 gen(777);
    for (int i = 0; i < scenarios; ++i) {
        const unsigned seed = gen();
        const int device = static_cast<int>(gen() % 9) % 5;  // keyboard and mouse more often
        const int windowMode = gen() % 8 == 0 ? static_cast<int>(gen() % 3) : 1;
        std::vector<int> types;
        for (unsigned k = gen() % 4; k; --k) types.push_back(static_cast<int>(gen() % 6));
        std::vector<Action> actions(gen() % 80);
        for (Action& a : actions) {
            const unsigned k = gen() % 32;  // mostly window messages
            a.kind = k < 1 ? 0 : k < 3 ? 1 : k < 5 ? 2 : k < 9 ? 3 : k < 10 ? 4 : k < 12 ? 5 : 6;
            a.a = gen();
            a.b = gen();
            a.c = gen();
            a.d = gen();
            a.dt = std::uniform_real_distribution<float>(0.0f, 0.5f)(gen);
        }
        Scenario(seed, device, actions, windowMode, types);
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
