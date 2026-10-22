// Differential test of InputSystem_7c unit U5 (Input) and the InputExtension base: our module against
// InputSystem_7c_orig.dll in one process.
//
// Each module's Input runs against its own set of stand-ins, built the same way for both sides:
//   - driver factories found by name through a patched PluginBase::LookupPluginType ("Win32", "DirectX", a second
//     factory with Win32's id, one whose drivers fail Init, one that creates nothing; other names are not found);
//   - drivers (InputDriver-shaped: vfptr, vbptr, factory, ReferenceCount as a virtual base at +0x10) that report
//     scripted devices from Enumerate and create stand-in devices;
//   - devices (InputDevice-shaped, ReferenceCount at +0xc) whose virtual slots log; the emulated mouse / joystick
//     are the modules' real EmuDevices, playing a stand-in keyboard;
//   - extensions (InputExtension-shaped) registered through the exported Init / RegisterExtension.
// Options (the driver lists), the OS, the window, the crash-handler registry, Profile and Time::Ms are patched in
// both import tables. Random steps: Init / Done / Update / GetDevice / releasing devices / EnumerateDevices /
// extensions / critical shutdown and startup / copy / assignment. Compared after each step: the call logs, the
// Input's memory with all lists (pointers named) and every stand-in's reference count.
//
// usage: diff_is_input.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules> [scenarios]
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

static unsigned long long checks, bad;
static std::mt19937 rng(4242);
static unsigned Pick(unsigned n) { return rng() % n; }
static int side;
static std::vector<std::string> logs[2];

static void Log(const char* fmt, ...)
{
    char buf[512];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    logs[side].push_back(buf);
}

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

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    int n = 0;
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name, name)) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
            ++n;
        }
    }
    (void)n;
}

template <class T>
static T& At(void* p, size_t off)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + off);
}

// ---- KernelStandard pieces used directly
struct Node;
static void** pstringCache;  // &PString::cache
static Node*(__cdecl* cacheCreate)(void*, const unsigned char*);
static void(__cdecl* cacheDestroy)(void*, Node*);
static void(__fastcall* ksAddRef)(void*, void*);
static bool(__fastcall* ksRelease)(void*, void*);
static void*(__cdecl* allocNew)(unsigned);
static void(__cdecl* allocDelete)(void*);
static const char* Text(const void* node) { return node ? static_cast<const char*>(node) + 0x18 : "(null)"; }
static void SetPString(void** s, const char* text)
{
    if (*s) cacheDestroy(*pstringCache, static_cast<Node*>(*s));
    *s = cacheCreate(*pstringCache, reinterpret_cast<const unsigned char*>(text));
}

// ---- names of the objects of the current side
static std::map<uintptr_t, std::string> names[2];
static std::string Name(const void* p)
{
    if (!p) return "0";
    auto it = names[side].find(reinterpret_cast<uintptr_t>(p));
    if (it != names[side].end()) return it->second;
    return "?";
}

// ---- ids
using Id = uint32_t[4];
static Id ids[10];
static int IdIndex(const void* id)
{
    for (int i = 0; i < 10; ++i)
        if (!std::memcmp(id, ids[i], 16)) return i;
    return -1;
}

// ---- devices: InputDevice-shaped stand-ins (0x240 bytes); vtable slots log
constexpr int MAX_DEV = 96;
alignas(8) static unsigned char devices[2][MAX_DEV][0x240];
static int deviceCount[2];
static void* devVt[18];
static void* devRcVt[2];
static void __cdecl DevAddObserver(void* d, void*, int prio) { Log("%s AddObserver %d", Name(d).c_str(), prio); }
static void __cdecl DevRemoveObserver(void* d, void*) { Log("%s RemoveObserver", Name(d).c_str()); }
static void __cdecl DevUpdate(void* d, float dt) { Log("%s Update %.4f", Name(d).c_str(), dt); }
static void __cdecl DevDone(void* d) { Log("%s Done", Name(d).c_str()); }
static void __cdecl DevShutdown(void* d) { Log("%s CriticalShutdown", Name(d).c_str()); }
static void __cdecl DevStartup(void* d) { Log("%s CriticalStartup", Name(d).c_str()); }
static void __cdecl DevOther(void* d) { Log("%s other slot", Name(d).c_str()); }
static unsigned long long deletesRc;
static void __cdecl RcDeleteThis(void* rc)
{
    Log("DeleteThis %s", Name(rc).c_str());
    if (side == 0) ++deletesRc;
}

static void* NewDevice(const void* id)
{
    const int n = deviceCount[side]++;
    if (n >= MAX_DEV) {
        std::printf("too many devices\n");
        std::exit(3);
    }
    unsigned char* d = devices[side][n];
    std::memset(d, 0, 0x240);
    At<void*>(d, 0) = devVt;
    At<void*>(d, 0xc) = devRcVt;
    At<int>(d, 0x10) = 1;
    std::memcpy(d + 0x14, id, 16);
    names[side][reinterpret_cast<uintptr_t>(d)] = "dev" + std::to_string(n);
    names[side][reinterpret_cast<uintptr_t>(d + 0xc)] = "dev" + std::to_string(n) + ".rc";
    return d;
}

// ---- drivers: vfptr +0, vbptr +4, factory +8, vtordisp +0xc, ReferenceCount +0x10 (vptr, count)
constexpr int MAX_DRV = 32;
alignas(8) static unsigned char drivers[2][MAX_DRV][0x18];
static int driverCount[2];
static const int driverVbtable[2] = {-4, 0xc};
static void* drvVt[6];
static void* drvRcVt[2];
struct EnumEntry {
    int id, type, caps;
};
static std::vector<EnumEntry> enumScript[3];  // what a driver reports for each device type bit
static bool createNull;                         // CreateDevice hands out nothing (the original then crashes: unused)

static int FactoryIndex(const void* f);
static bool __cdecl DrvInit(void* d, void* os) {
    const int f = FactoryIndex(At<void*>(d, 8));
    Log("%s Init os %s -> %d", Name(d).c_str(), Name(os).c_str(), f != 2);
    return f != 2;
}
static void __cdecl DrvUpdate(void* d, float dt) { Log("%s Update %.4f", Name(d).c_str(), dt); }
// appends to the Input's DynamicArray<InputDeviceCaps, 6>
static void AddCaps(void** head, const EnumEntry& e, int bit)
{
    unsigned char* tail;
    if (!*head) {
        tail = static_cast<unsigned char*>(allocNew(0x15c));
        At<uint32_t>(tail, 8) = 0;
        At<void*>(tail, 0) = At<void*>(tail, 4) = tail;
        *head = tail;
    } else {
        tail = static_cast<unsigned char*>(At<void*>(*head, 4));
        if (At<uint32_t>(tail, 8) == 6) {
            unsigned char* c = static_cast<unsigned char*>(allocNew(0x15c));
            At<uint32_t>(c, 8) = 0;
            At<void*>(c, 0) = *head;
            At<void*>(c, 4) = tail;
            At<void*>(*head, 4) = c;
            At<void*>(tail, 0) = c;
            tail = c;
        }
    }
    unsigned char* item = tail + 0xc + 0x38 * At<uint32_t>(tail, 8)++;
    std::memset(item, 0, 0x38);
    At<int>(item, 0) = e.type;
    std::memcpy(item + 4, ids[e.id], 16);
    char text[32];
    std::snprintf(text, sizeof(text), "dev id %d bit %d", e.id, bit);
    SetPString(&At<void*>(item, 0x14), text);
    SetPString(&At<void*>(item, 0x18), "description");
    At<uint32_t>(item, 0x1c) = static_cast<uint32_t>(e.caps);
    At<uint32_t>(item, 0x20) = static_cast<uint32_t>(e.id);
}
static void __cdecl DrvEnumerate(void* d, void** list, int type)
{
    Log("%s Enumerate type %d", Name(d).c_str(), type);
    const int bit = type == 1 ? 0 : type == 2 ? 1 : type == 4 ? 2 : -1;
    if (bit < 0) return;
    for (const EnumEntry& e : enumScript[bit]) AddCaps(list, e, type);
}
static void** __cdecl DrvCreateDevice(void* d, void** ret, const void* id)
{
    Log("%s CreateDevice id %d", Name(d).c_str(), IdIndex(id));
    *ret = NewDevice(id);
    return ret;
}
static void __cdecl DrvShutdown(void* d) { Log("%s CriticalShutdown", Name(d).c_str()); }
static void __cdecl DrvStartup(void* d) { Log("%s CriticalStartup", Name(d).c_str()); }

// ---- factories (PluginType-shaped: vptr, UId at +4); 0 Win32, 1 DirectX, 2 failing Init, 3 creates nothing,
// 4 a second Win32 (same id)
alignas(8) static unsigned char factories[2][5][0x20];
static int factoryRefs[2][5];
static const char* const factoryNames[5] = {"Win32", "DirectX", "Fail", "Null", "Win32b"};
static void* facVt[4];
static int FactoryIndex(const void* f)
{
    for (int i = 0; i < 5; ++i)
        if (f == factories[side][i]) return i;
    return -1;
}
static void __cdecl FacAddRef(void* f) { ++factoryRefs[side][FactoryIndex(f)]; }
static void __cdecl FacRelease(void* f) { Log("%s Release", Name(f).c_str()); --factoryRefs[side][FactoryIndex(f)]; }
static void* __cdecl FacCreate(void* f)
{
    const int fi = FactoryIndex(f);
    Log("%s CreateInstance", Name(f).c_str());
    if (fi == 3) return nullptr;
    const int n = driverCount[side]++;
    unsigned char* d = drivers[side][n];
    std::memset(d, 0, sizeof(drivers[side][n]));
    At<void*>(d, 0) = drvVt;
    At<const void*>(d, 4) = driverVbtable;
    At<void*>(d, 8) = f;
    At<void*>(d, 0x10) = drvRcVt;
    At<int>(d, 0x14) = 1;
    names[side][reinterpret_cast<uintptr_t>(d)] = "drv" + std::to_string(n);
    names[side][reinterpret_cast<uintptr_t>(d + 0x10)] = "drv" + std::to_string(n) + ".rc";
    return d;
}

// ---- extensions: InputExtension-shaped (vptr, ReferenceCount +4 {vptr, count}, active, exclusive)
constexpr int MAX_EXT = 48;
alignas(8) static unsigned char extensions[2][MAX_EXT][0x10];
static int extensionCount[2];
static void* extVt[7];
static void* extRcVt[2];
static unsigned long long extDone, deletes;
static void __cdecl ExtDone(void* e)
{
    Log("%s Done", Name(e).c_str());
    if (side == 0) ++extDone;
}
static void __cdecl ExtUpdate(void* e, float dt) { Log("%s Update %.4f", Name(e).c_str(), dt); }
static void __cdecl ExtOther(void* e) { Log("%s other slot", Name(e).c_str()); }

// ---- patched Kernel functions
static unsigned long msNow;
static unsigned long __cdecl FakeMs() { return ++msNow; }
alignas(8) static unsigned char fakeOs[2][0x18], fakeOptions[2][0x10], fakeWindow[2][0x10];
static bool optionsPresent, windowPresent, windowActive;
static void* __cdecl FakeGetOptions(void* os) { return optionsPresent ? fakeOptions[side] : nullptr; }
static void* __cdecl FakeGetWindow(void* os) { return windowPresent ? fakeWindow[side] : nullptr; }
static bool __cdecl IsActiveWnd(void* w)
{
    Log("IsActiveWnd %s", Name(w).c_str());
    return windowActive;
}
static std::map<std::string, std::vector<std::string>> optionValues;
static std::map<std::string, size_t> optionCursor[2];
static void __cdecl SetSeparators(void* o, const char* key, const char* sep)
{
    Log("SetSeparators %s '%s'", key, sep);
    optionCursor[side][key] = 0;
}
static bool __cdecl GetNext(void* o, const char* key, void** value)
{
    auto it = optionValues.find(key);
    size_t& at = optionCursor[side][key];
    if (it == optionValues.end() || at >= it->second.size()) {
        Log("GetNext %s -> none", key);
        return false;
    }
    SetPString(value, it->second[at].c_str());
    Log("GetNext %s -> %s", key, it->second[at++].c_str());
    return true;
}
static int handleNow[2];
static void* callbacks[2][64][2];
static void* __cdecl AddCritical(void* fn, void* arg, const char* what)
{
    const int h = ++handleNow[side];
    callbacks[side][h][0] = fn;
    callbacks[side][h][1] = arg;
    Log("Add%s %s arg %s -> %d", what, Name(fn).c_str(), Name(arg).c_str(), h);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(h));
}
static void* __cdecl AddShutdown(void* fn, void* arg) { return AddCritical(fn, arg, "CriticalShutdown"); }
static void* __cdecl AddStartup(void* fn, void* arg) { return AddCritical(fn, arg, "CriticalStartup"); }
static void __cdecl RemoveShutdown(void* h) { Log("RemoveCriticalShutdown %u", static_cast<unsigned>(reinterpret_cast<uintptr_t>(h))); }
static void __cdecl RemoveStartup(void* h) { Log("RemoveCriticalStartup %u", static_cast<unsigned>(reinterpret_cast<uintptr_t>(h))); }
static const char* expectedGroup = "F3117B18";
static uint32_t groupId[4];
static int* __cdecl LookupPluginType(int* result, void** plugin, const void* parent, void* const* name)
{
    const char* n = Text(*name);
    if (std::memcmp(parent, groupId, 16)) Log("Lookup: wrong group");
    for (int i = 0; i < 5; ++i) {
        if (!std::strcmp(n, factoryNames[i])) {
            *plugin = factories[side][i];
            ++factoryRefs[side][i];
            Log("Lookup %s -> fac%d", n, i);
            *result = 0;
            return result;
        }
    }
    Log("Lookup %s -> none", n);
    *result = 3;
    return result;
}
static void* __fastcall ProfileCtor(void* self, void*, void** info)
{
    Log("Profile %s", static_cast<const char*>(info[0]));
    std::memset(self, 0, 16);
    return self;
}
static void __fastcall ProfileDtor(void*) { Log("~Profile"); }

// ---- the modules' exports
struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void*(__fastcall* copy)(void*, void*, const void*);
    void(__fastcall* dtor)(void*, void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    bool(__cdecl* init)(void*, void*);
    void(__cdecl* done)(void*);
    bool(__cdecl* update)(void*, float);
    bool(__cdecl* isInit)(const void*);
    bool(__cdecl* isActiveWnd)(const void*);
    void*(__cdecl* enumerate)(void*, int, unsigned long);
    void**(__cdecl* getDevice)(void*, void**, const void*);
    void(__cdecl* registerExt)(void*, void*);
    void(__cdecl* enumerateAll)(void*);
    int(__cdecl* findType)(void*, const void*);
    int(__cdecl* shutdownCb)(void*);
    int(__cdecl* startupCb)(void*);
    void*(__fastcall* extCtor)(void*, void*);
    void(__fastcall* extDtor)(void*, void*);
    bool(__cdecl* extInit)(void*, void*);
    bool(__fastcall* extRelease)(void*, void*);
    void(__cdecl* extActivate)(void*, bool);
    bool(__cdecl* extIsActive)(const void*);
    void(__cdecl* extSetExclusive)(void*, bool);
    bool(__cdecl* extGetExclusive)(const void*);
    void* extVt[2];
    bool* isInstance;
    void* emuVt;
    explicit Api(HMODULE m)
    {
        ctor = Get<decltype(ctor)>(m, "??0Input@Jet@@QAE@XZ");
        copy = Get<decltype(copy)>(m, "??0Input@Jet@@QAE@ABV01@@Z");
        dtor = Get<decltype(dtor)>(m, "??1Input@Jet@@QAE@XZ");
        assign = Get<decltype(assign)>(m, "??4Input@Jet@@QAEAAV01@ABV01@@Z");
        init = Get<decltype(init)>(m, "?Init@Input@Jet@@QAA_NPAVOS@2@@Z");
        done = Get<decltype(done)>(m, "?Done@Input@Jet@@QAAXXZ");
        update = Get<decltype(update)>(m, "?Update@Input@Jet@@QAA_NM@Z");
        isInit = Get<decltype(isInit)>(m, "?IsInitialized@Input@Jet@@QBA_NXZ");
        isActiveWnd = Get<decltype(isActiveWnd)>(m, "?IsActiveWnd@Input@Jet@@QBA_NXZ");
        enumerate = Get<decltype(enumerate)>(m, "?EnumerateDevices@Input@Jet@@QAAPAV?$DynamicArray@VInputDeviceCaps@Jet@@$05U?$Less@VInputDeviceCaps@Jet@@V12@@2@@2@HK@Z");
        getDevice = Get<decltype(getDevice)>(m, "?GetDevice@Input@Jet@@QAA?AV?$AutoReference@VInputDevice@Jet@@@2@ABVUId@2@@Z");
        registerExt = Get<decltype(registerExt)>(m, "?RegisterExtension@Input@Jet@@AAAXPAVInputExtension@2@@Z");
        enumerateAll = Get<decltype(enumerateAll)>(m, "?EnumerateAll@Input@Jet@@AAAXXZ");
        findType = Get<decltype(findType)>(m, "?FindDeviceType@Input@Jet@@AAAHABVUId@2@@Z");
        shutdownCb = Get<decltype(shutdownCb)>(m, "?CriticalShutdown@Input@Jet@@CAHPAX@Z");
        startupCb = Get<decltype(startupCb)>(m, "?CriticalStartup@Input@Jet@@CAHPAX@Z");
        extCtor = Get<decltype(extCtor)>(m, "??0InputExtension@Jet@@QAE@XZ");
        extDtor = Get<decltype(extDtor)>(m, "??1InputExtension@Jet@@UAE@XZ");
        extInit = Get<decltype(extInit)>(m, "?Init@InputExtension@Jet@@UAA_NAAVInput@2@@Z");
        extRelease = Get<decltype(extRelease)>(m, "?ReleaseReference@InputExtension@Jet@@QAE_NXZ");
        extActivate = Get<decltype(extActivate)>(m, "?Activate@InputExtension@Jet@@UAAX_N@Z");
        extIsActive = Get<decltype(extIsActive)>(m, "?IsActive@InputExtension@Jet@@QBA_NXZ");
        extSetExclusive = Get<decltype(extSetExclusive)>(m, "?SetExclusive@InputExtension@Jet@@UAAX_N@Z");
        extGetExclusive = Get<decltype(extGetExclusive)>(m, "?GetExclusive@InputExtension@Jet@@QBA_NXZ");
        extVt[0] = Get<void*>(m, "??_7InputExtension@Jet@@6BInputObserver@1@@");
        extVt[1] = Get<void*>(m, "??_7InputExtension@Jet@@6BReferenceCount@1@@");
        isInstance = Get<bool*>(m, "?isInstance@Input@Jet@@0_NA");
        emuVt = Get<void*>(m, "??_7EmuDevice@Jet@@6B?$ObserverSubject@VInputObserver@Jet@@@1@@");
    }
};

// ---- dumps
static std::string DumpList(const void* head, unsigned stride, unsigned items, bool caps)
{
    std::string s;
    char buf[160];
    if (!head) return " -";
    const unsigned char* c = static_cast<const unsigned char*>(head);
    do {
        const uint32_t n = At<uint32_t>(const_cast<unsigned char*>(c), 8);
        std::snprintf(buf, sizeof(buf), " [%u:", n);
        s += buf;
        if (n > items) s += "!";
        for (uint32_t i = 0; i < n && i < items; ++i) {
            const unsigned char* it = c + 0xc + stride * i;
            if (caps) {
                std::snprintf(buf, sizeof(buf), " {t%d id%d %s %s %x %u %x %u %x %u %x}", At<int>(const_cast<unsigned char*>(it), 0),
                              IdIndex(it + 4), Text(At<void*>(const_cast<unsigned char*>(it), 0x14)),
                              Text(At<void*>(const_cast<unsigned char*>(it), 0x18)),
                              At<uint32_t>(const_cast<unsigned char*>(it), 0x1c), At<uint32_t>(const_cast<unsigned char*>(it), 0x20),
                              At<uint32_t>(const_cast<unsigned char*>(it), 0x24), At<uint32_t>(const_cast<unsigned char*>(it), 0x28),
                              At<uint32_t>(const_cast<unsigned char*>(it), 0x2c), At<uint32_t>(const_cast<unsigned char*>(it), 0x30),
                              At<uint32_t>(const_cast<unsigned char*>(it), 0x34));
                s += buf;
            } else {
                s += " " + Name(At<void*>(const_cast<unsigned char*>(it), 0));
            }
        }
        s += "]";
        c = At<const unsigned char*>(const_cast<unsigned char*>(c), 0);
    } while (c != head);
    return s;
}

static std::string Dump(void* in, const Api& api)
{
    std::string s;
    char buf[128];
    for (int i = 0; i < 3; ++i) s += std::string(" ") + At<const char*>(in, 4 * i);
    for (int i = 0; i < 6; ++i) s += " " + Name(At<void*>(in, 0xc + 4 * i));
    std::snprintf(buf, sizeof(buf), " n%d i%d sh%u st%u os %s inst%d", At<int>(in, 0x24), At<unsigned char>(in, 0x28),
                  At<unsigned>(in, 0x38), At<unsigned>(in, 0x3c), Name(At<void*>(in, 0x40)).c_str(), *api.isInstance);
    s += buf;
    s += " | dev" + DumpList(At<void*>(in, 0x2c), 4, 13, false);
    s += " | caps" + DumpList(At<void*>(in, 0x30), 0x38, 6, true);
    s += " | ext" + DumpList(At<void*>(in, 0x34), 4, 13, false);
    return s;
}

static std::string Refs()
{
    std::string s = "refs";
    char buf[48];
    for (int i = 0; i < deviceCount[side]; ++i) {
        std::snprintf(buf, sizeof(buf), " d%d:%d", i, At<int>(devices[side][i], 0x10));
        s += buf;
    }
    for (int i = 0; i < driverCount[side]; ++i) {
        std::snprintf(buf, sizeof(buf), " r%d:%d", i, At<int>(drivers[side][i], 0x14));
        s += buf;
    }
    for (int i = 0; i < 5; ++i) {
        std::snprintf(buf, sizeof(buf), " f%d:%d", i, factoryRefs[side][i]);
        s += buf;
    }
    for (int i = 0; i < extensionCount[side]; ++i) {
        std::snprintf(buf, sizeof(buf), " e%d:%d/%d%d", i, At<int>(extensions[side][i], 8), extensions[side][i][0xc],
                      extensions[side][i][0xd]);
        s += buf;
    }
    return s;
}

// a device reference the test holds, by index (the same on both sides)
static std::vector<void*> held[2];
static unsigned long long devMade, emuMade;

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_input <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 2000;
    SetDllDirectoryA(dir.c_str());
    HMODULE ks = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    HMODULE kernel = LoadLibraryA((dir + "\\Kernel_7c.dll").c_str());
    LoadLibraryA((dir + "\\KernelResource_7c.dll").c_str());
    HMODULE mods[2] = {LoadLibraryA((dir + "\\InputSystem_7c.dll").c_str()),
                       LoadLibraryA((dir + "\\InputSystem_7c_orig.dll").c_str())};
    if (!ks || !kernel || !mods[0] || !mods[1]) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    pstringCache = Get<void**>(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    cacheCreate = Get<decltype(cacheCreate)>(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    cacheDestroy = Get<decltype(cacheDestroy)>(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    ksAddRef = Get<decltype(ksAddRef)>(ks, "?AddReference@ReferenceCount@Jet@@QBEXXZ");
    ksRelease = Get<decltype(ksRelease)>(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ");
    allocNew = Get<decltype(allocNew)>(ks, "?New@Alloc@Jet@@SAPAXI@Z");
    allocDelete = Get<decltype(allocDelete)>(ks, "?Delete@Alloc@Jet@@SAXPAX@Z");
    auto uidFromText = Get<void*(__fastcall*)(void*, void*, const char*)>(kernel, "??0UId@Jet@@QAE@PBD@Z");
    uidFromText(groupId, nullptr, "F3117B18-F364-41a1-8993-966236A35B5E");
    (void)expectedGroup;

    const Api api[2] = {Api(mods[0]), Api(mods[1])};
    const char* idNames[6] = {"?UIdKeyboard@Jet@@3VUId@1@B", "?UIdMouse@Jet@@3VUId@1@B", "?UIdJoystick1@Jet@@3VUId@1@B",
                              "?UIdJoystick2@Jet@@3VUId@1@B", "?UIdMouseEmu@Jet@@3VUId@1@B", "?UIdJoystickEmu@Jet@@3VUId@1@B"};
    for (int i = 0; i < 6; ++i) std::memcpy(ids[i], Get<void*>(mods[1], idNames[i]), 16);
    for (int i = 6; i < 10; ++i)
        for (int k = 0; k < 4; ++k) ids[i][k] = 0x1000u * i + k;

    for (HMODULE m : mods) {
        PatchImport(m, "?Ms@Time@Jet@@SAKXZ", reinterpret_cast<void*>(&FakeMs));
        PatchImport(m, "?GetOptions@OS@Jet@@QBAPAVOptions@2@XZ", reinterpret_cast<void*>(&FakeGetOptions));
        PatchImport(m, "?GetWindow@OS@Jet@@QBAPAVWindow@2@XZ", reinterpret_cast<void*>(&FakeGetWindow));
        PatchImport(m, "?IsActiveWnd@Window@Jet@@QAA_NXZ", reinterpret_cast<void*>(&IsActiveWnd));
        PatchImport(m, "?SetSeparators@Options@Jet@@QBAXPBD0@Z", reinterpret_cast<void*>(&SetSeparators));
        PatchImport(m, "?GetNext@Options@Jet@@QBA_NPBDAAVPString@2@@Z", reinterpret_cast<void*>(&GetNext));
        PatchImport(m, "?AddCriticalShutdown@ExceptionHandler@Jet@@SAPAXP6AHPAX@Z0@Z", reinterpret_cast<void*>(&AddShutdown));
        PatchImport(m, "?AddCriticalStartup@ExceptionHandler@Jet@@SAPAXP6AHPAX@Z0@Z", reinterpret_cast<void*>(&AddStartup));
        PatchImport(m, "?RemoveCriticalShutdown@ExceptionHandler@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&RemoveShutdown));
        PatchImport(m, "?RemoveCriticalStartup@ExceptionHandler@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&RemoveStartup));
        PatchImport(m, "?LookupPluginType@PluginBase@Jet@@KA?AVResult@2@PAPAVPluginType@2@ABVUId@2@ABVPString@2@@Z",
                    reinterpret_cast<void*>(&LookupPluginType));
        PatchImport(m, "??0Profile@Jet@@QAE@PAUInfo@01@@Z", reinterpret_cast<void*>(&ProfileCtor));
        PatchImport(m, "??1Profile@Jet@@QAE@XZ", reinterpret_cast<void*>(&ProfileDtor));
    }
    for (void*& p : devVt) p = reinterpret_cast<void*>(&DevOther);
    devVt[1] = reinterpret_cast<void*>(&DevAddObserver);
    devVt[2] = reinterpret_cast<void*>(&DevRemoveObserver);
    devVt[14] = reinterpret_cast<void*>(&DevUpdate);
    devVt[15] = reinterpret_cast<void*>(&DevDone);
    devVt[16] = reinterpret_cast<void*>(&DevShutdown);
    devVt[17] = reinterpret_cast<void*>(&DevStartup);
    devRcVt[0] = drvRcVt[0] = extRcVt[0] = reinterpret_cast<void*>(&RcDeleteThis);
    devRcVt[1] = drvRcVt[1] = extRcVt[1] = reinterpret_cast<void*>(&DevOther);
    drvVt[0] = reinterpret_cast<void*>(&DrvInit);
    drvVt[1] = reinterpret_cast<void*>(&DrvUpdate);
    drvVt[2] = reinterpret_cast<void*>(&DrvEnumerate);
    drvVt[3] = reinterpret_cast<void*>(&DrvCreateDevice);
    drvVt[4] = reinterpret_cast<void*>(&DrvShutdown);
    drvVt[5] = reinterpret_cast<void*>(&DrvStartup);
    facVt[0] = reinterpret_cast<void*>(&FacAddRef);
    facVt[1] = reinterpret_cast<void*>(&FacRelease);
    facVt[2] = reinterpret_cast<void*>(&DevOther);
    facVt[3] = reinterpret_cast<void*>(&FacCreate);
    for (void*& p : extVt) p = reinterpret_cast<void*>(&ExtOther);
    extVt[5] = reinterpret_cast<void*>(&ExtDone);
    extVt[6] = reinterpret_cast<void*>(&ExtUpdate);

    const char* const choices[] = {"Default", "Win32", "DirectX", "Fail", "Null", "Win32b", "Nothing"};
    alignas(8) unsigned char input[2][0x44], other[2][0x44];
    unsigned long long ops[16] = {};
    for (int sc = 0; sc < scenarios; ++sc) {
        // the option lists and what the drivers report, shared by both sides
        optionValues.clear();
        for (const char* key : {"InputKeyboard", "InputMouse", "InputJoystick"}) {
            std::vector<std::string>& v = optionValues[key];
            const unsigned n = Pick(12) ? 1 + Pick(3) : 0;
            for (unsigned i = 0; i < n; ++i) v.push_back(choices[Pick(5) ? Pick(3) : Pick(7)]);
        }
        // a driver reports devices of its own type (or of types no driver has: 0, 3, 5, 8), so a reported type
        // always has its driver (the original calls a missing one through null)
        static const int odd[4] = {0, 3, 5, 8};
        for (int k = 0; k < 3; ++k) {
            std::vector<EnumEntry>& script = enumScript[k];
            script.clear();
            const unsigned n = 1 + Pick(4);
            for (unsigned i = 0; i < n; ++i)
                script.push_back({static_cast<int>(Pick(4) ? Pick(4) : 6 + Pick(4)),
                                  Pick(5) ? 1 << k : odd[Pick(4)], static_cast<int>(Pick(8))});
        }
        optionsPresent = Pick(12) != 0;
        createNull = false;
        for (side = 0; side < 2; ++side) {
            names[side].clear();
            deviceCount[side] = driverCount[side] = extensionCount[side] = 0;
            handleNow[side] = 0;
            held[side].clear();
            for (int i = 0; i < 5; ++i) {
                std::memset(factories[side][i], 0, 0x20);
                At<void*>(factories[side][i], 0) = facVt;
                const uint32_t fid = i == 4 ? 0 : static_cast<uint32_t>(i);
                for (int k = 0; k < 4; ++k) At<uint32_t>(factories[side][i], 4 + 4 * k) = 0xabc0 + fid + k;
                factoryRefs[side][i] = 1;
                names[side][reinterpret_cast<uintptr_t>(factories[side][i])] = "fac" + std::to_string(i);
            }
            names[side][reinterpret_cast<uintptr_t>(input[side])] = "input";
            names[side][reinterpret_cast<uintptr_t>(fakeOs[side])] = "os";
            names[side][reinterpret_cast<uintptr_t>(fakeWindow[side])] = "window";
            names[side][reinterpret_cast<uintptr_t>(api[side].shutdownCb)] = "Input::CriticalShutdown";
            names[side][reinterpret_cast<uintptr_t>(api[side].startupCb)] = "Input::CriticalStartup";
            std::memset(input[side], 0xcd, sizeof(input[side]));
            api[side].ctor(input[side], nullptr);
            Log("built%s", Dump(input[side], api[side]).c_str());
        }
        const int steps = 1 + Pick(60);
        for (int st = 0; st < steps; ++st) {
            const unsigned op = st == 0 && Pick(6) ? 0 : Pick(16);
            ++ops[op];
            const unsigned a = rng(), b = rng(), c = rng();
            const float dt = static_cast<float>(Pick(1000)) / 977.0f;
            windowPresent = (b & 3) != 0;
            windowActive = (b & 4) != 0;
            for (side = 0; side < 2; ++side) {
                msNow = 1000u * static_cast<unsigned>(st);
                void* const in = input[side];
                const Api& f = api[side];
                switch (op) {
                case 0:
                    Log("Init -> %d", f.init(in, a % 9 ? fakeOs[side] : nullptr));
                    break;
                case 1:
                    if (a % 3 == 0) {
                        f.done(in);
                        Log("Done");
                    } else {
                        Log("Update -> %d", f.update(in, dt));
                    }
                    break;
                case 2:
                case 3:
                case 4: {  // GetDevice: the drivers' ids mostly, sometimes the emulated devices
                    if (!f.isInit(in)) break;  // (uninitialised: no drivers, GetDevice of a known type crashes)
                    int id = a % 5 ? static_cast<int>(a % 4) : (a % 3 ? 4 + static_cast<int>(b % 2) : 6 + static_cast<int>(b % 4));
                    const int type = f.findType(in, ids[id]);
                    const int need = type == 1 ? 0 : type == 2 ? 1 : type == 4 ? 2 : -1;
                    if (need >= 0 && !At<void*>(in, 0x18 + 4 * need)) break;
                    void* ref = nullptr;
                    f.getDevice(in, &ref, ids[id]);
                    if (ref && !names[side].count(reinterpret_cast<uintptr_t>(ref))) {
                        const bool emu = At<void*>(ref, 0) == f.emuVt;
                        const std::string n = (emu ? "emu" : "?dev") + std::to_string(held[side].size());
                        names[side][reinterpret_cast<uintptr_t>(ref)] = n;
                        names[side][reinterpret_cast<uintptr_t>(ref) + 0xc] = n + ".rc";
                    }
                    Log("GetDevice id %d -> %s rc %d", id, Name(ref).c_str(), ref ? At<int>(ref, 0x10) : -1);
                    if (ref) held[side].push_back(ref);
                    if (ref && side == 0) ++(At<void*>(ref, 0) == f.emuVt ? emuMade : devMade);
                    break;
                }
                case 5:  // release a held device
                    if (held[side].empty()) break;
                    {
                        const size_t k = a % held[side].size();
                        void* d = held[side][k];
                        held[side].erase(held[side].begin() + static_cast<long>(k));
                        const bool zero = ksRelease(static_cast<unsigned char*>(d) + 0xc, nullptr);
                        Log("release %s -> %d", Name(d).c_str(), zero);
                    }
                    break;
                case 6: {  // EnumerateDevices
                    const int mask = a % 4 ? static_cast<int>(1u << (b % 3)) | static_cast<int>(c % 8) : -1;
                    const unsigned long capsMask = a % 3 ? 0 : b % 8;
                    void* list = f.enumerate(in, mask, capsMask);
                    std::string s = list ? DumpList(At<void*>(list, 0), 0x38, 6, true) : " null";
                    Log("EnumerateDevices %d %lu ->%s", mask, capsMask, s.c_str());
                    if (list) {  // free it as the caller would (DynamicArray::RemoveAll + Delete)
                        unsigned char* head = static_cast<unsigned char*>(At<void*>(list, 0));
                        unsigned char* ch = head;
                        do {
                            unsigned char* next = At<unsigned char*>(ch, 0);
                            for (uint32_t i = 0; i < At<uint32_t>(ch, 8); ++i) {
                                unsigned char* item = ch + 0xc + 0x38 * i;
                                if (At<Node*>(item, 0x14)) cacheDestroy(*pstringCache, At<Node*>(item, 0x14));
                                if (At<Node*>(item, 0x18)) cacheDestroy(*pstringCache, At<Node*>(item, 0x18));
                            }
                            allocDelete(ch);
                            ch = next;
                        } while (ch != head);
                        allocDelete(list);
                    }
                    break;
                }
                case 7:
                case 8: {  // a new extension, registered through InputExtension::Init or directly
                    if (extensionCount[side] >= MAX_EXT) break;
                    const int n = extensionCount[side]++;
                    unsigned char* e = extensions[side][n];
                    std::memset(e, 0xee, 0x10);
                    f.extCtor(e, nullptr);
                    Log("ext%d built %s %s %d %d %d", n, At<void*>(e, 0) == f.extVt[0] ? "vt0" : "?",
                        At<void*>(e, 4) == f.extVt[1] ? "vt1" : "?", At<int>(e, 8), e[0xc], e[0xd]);
                    At<void*>(e, 0) = extVt;
                    At<void*>(e, 4) = extRcVt;
                    names[side][reinterpret_cast<uintptr_t>(e)] = "ext" + std::to_string(n);
                    names[side][reinterpret_cast<uintptr_t>(e + 4)] = "ext" + std::to_string(n) + ".rc";
                    if (a % 2)
                        Log("ext%d Init -> %d", n, f.extInit(e, in));
                    else
                        f.registerExt(in, e);
                    if (b % 3 == 0 && n > 0) {  // registering one twice
                        f.registerExt(in, extensions[side][b % n]);
                        Log("registered ext%u again", b % n);
                    }
                    break;
                }
                case 9: {  // the test's own reference to an extension goes, or one is added
                    if (!extensionCount[side]) break;
                    unsigned char* e = extensions[side][a % extensionCount[side]];
                    if (At<int>(e, 8) <= 0) break;
                    if (b % 3 == 0) {
                        ksAddRef(e + 4, nullptr);
                        Log("%s AddRef", Name(e).c_str());
                    } else if (b % 3 == 1 && At<int>(e, 8) > 1) {
                        Log("%s release -> %d", Name(e).c_str(), ksRelease(e + 4, nullptr));
                    } else {
                        Log("%s InputExtension::ReleaseReference -> %d", Name(e).c_str(), f.extRelease(e, nullptr));
                    }
                    break;
                }
                case 10: {  // the extension accessors
                    if (!extensionCount[side]) break;
                    unsigned char* e = extensions[side][a % extensionCount[side]];
                    f.extActivate(e, (b & 1) != 0);
                    f.extSetExclusive(e, (b & 2) != 0);
                    Log("%s active %d exclusive %d", Name(e).c_str(), f.extIsActive(e), f.extGetExclusive(e));
                    break;
                }
                case 11:  // the crash handler calls back
                    for (int h = 1; h <= handleNow[side]; ++h) {
                        auto fn = reinterpret_cast<int(__cdecl*)(void*)>(callbacks[side][h][0]);
                        if ((h % 2 == 1) == (a % 2 == 0)) Log("callback %d -> %d", h, fn(callbacks[side][h][1]));
                    }
                    break;
                case 12:
                    Log("IsActiveWnd -> %d IsInitialized %d", f.isActiveWnd(in), f.isInit(in));
                    break;
                case 13: {  // EnumerateAll / FindDeviceType
                    if (a % 2) f.enumerateAll(in);
                    Log("FindDeviceType %d -> %d", b % 10, f.findType(in, ids[b % 10]));
                    break;
                }
                case 14: {  // a copy, or an assignment to a fresh Input; dumped, then emptied without Done
                    std::memset(other[side], 0xcd, sizeof(other[side]));
                    if (a % 2) {
                        f.copy(other[side], nullptr, in);
                    } else {
                        f.ctor(other[side], nullptr);
                        f.assign(other[side], nullptr, in);
                    }
                    names[side][reinterpret_cast<uintptr_t>(other[side])] = "other";
                    Log("copy%s", Dump(other[side], f).c_str());
                    other[side][0x28] = 0;
                    f.dtor(other[side], nullptr);
                    Log("copy gone inst%d %s", *f.isInstance, Refs().c_str());
                    *f.isInstance = true;  // (the copy's destructor cleared it)
                    break;
                }
                default:
                    Log("Update -> %d", f.update(in, dt));
                    break;
                }
                Log("input%s", Dump(in, f).c_str());
                Log("%s", Refs().c_str());
            }
            if (std::getenv("IS_DUMP") && sc == std::atoi(std::getenv("IS_DUMP")))
                for (const std::string& l : logs[0]) std::printf("%s\n", l.c_str());
            const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
            for (size_t i = 0; i < n; ++i) {
                ++checks;
                const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
                const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
                if (x != y) {
                    if (++bad <= 6) std::printf("scenario %d step %d op %u line %zu:\n  ours %s\n  orig %s\n", sc, st, op, i, x.c_str(), y.c_str());
                    break;
                }
            }
            logs[0].clear();
            logs[1].clear();
        }
        // the end: the destructor (Done included); the test's references go after it
        for (side = 0; side < 2; ++side) {
            api[side].dtor(input[side], nullptr);
            Log("destroyed inst%d %s", *api[side].isInstance, Refs().c_str());
            for (void* d : held[side]) Log("release %s -> %d", Name(d).c_str(), ksRelease(static_cast<unsigned char*>(d) + 0xc, nullptr));
            Log("%s", Refs().c_str());
        }
        const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < n; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 6) std::printf("scenario %d end line %zu:\n  ours %s\n  orig %s\n", sc, i, x.c_str(), y.c_str());
                break;
            }
        }
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("ops:");
    for (unsigned long long o : ops) std::printf(" %llu", o);
    std::printf("\ndevices %llu, emulated %llu, extension Done %llu, stand-ins deleted %llu\n", devMade, emuMade,
                extDone, deletesRc);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
