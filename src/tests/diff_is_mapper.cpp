// Differential test of InputSystem_7c unit U7 (InputAction, InputActionMapper): our module against
// InputSystem_7c_orig.dll in one process.
//
// Each module gets its own mapper and the same random steps:
//   - actions built through the module's own InputAction exports (keys, buttons, axes, povs, flush, names), added,
//     removed (also by an equal copy), all removed;
//   - stand-in devices (InputDevice-shaped: caps, axis properties, observer calls logged) added, removed by id or by
//     device, all removed; Activate / SetExclusive / Reset / Done through the extension's vftable;
//   - action maps loaded from random config trees (KernelStandard's ConfigParser does not parse outside the game, so
//     both modules' ConfigParser / ConfigData imports serve the trees), with and without a map name and a database;
//   - Save, its File / UId imports patched to log the lines written;
//   - device events (key, button, axis, pov, lost / get, property changes), mostly made to match the next stroke of
//     some bound action, at times that sometimes run out; logging observers that sometimes stop the event;
//   - TestDevice, EnumerateDevices, EnumerateActions, and the InputAction calls on a separate action (copy, assign,
//     compare, stroke lists, Reset, EndAdd, Count, TestDevice).
// Compared after every step: the logs, the results, and the mapper's whole state walked through memory (observers,
// the 97 buckets in order with every list and action, every device with its zones and bound actions, references).
//
// usage: diff_is_mapper.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules> [scenarios]
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

static std::mt19937 rng(20261021);
static unsigned Pick(unsigned n) { return n ? rng() % n : 0; }
static int side;
static std::vector<std::string> logs[2];
static unsigned long long checks, bad;
static unsigned long long stats[16];
static std::map<std::string, unsigned long long> coverage;

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
    bool found = false;
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
            found = true;
        }
    }
    if (!found) {
        std::printf("no import %s\n", name);
        std::exit(2);
    }
}

// ---- PString nodes, interned in KernelStandard's cache (text at +0x18)
static void** cache;
static void*(__cdecl* createNode)(void*, const unsigned char*);
static void(__cdecl* destroyNode)(void*, void*);
static std::map<std::string, void*> interned;

static void* Node(const std::string& text)
{
    if (text.empty()) return nullptr;
    auto it = interned.find(text);
    if (it != interned.end()) return it->second;
    return interned[text] = createNode(*cache, reinterpret_cast<const unsigned char*>(text.c_str()));
}
static const char* Text(const void* node) { return node ? static_cast<const char*>(node) + 0x18 : "(null)"; }

// ---- the stand-in ConfigData / ConfigParser
struct FakeData {
    void* name = nullptr;
    int kind = 0;
    const void* data = nullptr;  // a list's {count, ...}, or a single value's PString node
    std::vector<FakeData*> children;
    std::vector<void*> values;
    bool noArray = false;
    std::vector<int> listData;
};
static FakeData unknownItem;
static std::vector<FakeData*> pool;
static FakeData* NewData(const std::string& name)
{
    pool.push_back(new FakeData);
    pool.back()->name = Node(name);
    return pool.back();
}
static const FakeData* parseRoot;
static int parseCode;
static void* __fastcall FakeParserCtor(unsigned char* self, void*)
{
    Log("parser ctor");
    std::memset(self, 0, 0x18);
    return self;
}
static void __fastcall FakeParserDtor(unsigned char*, void*) { Log("parser dtor"); }
static int* __cdecl FakeParse(unsigned char* self, int* result, const char* file)
{
    Log("parse %s", file ? file : "(null)");
    *reinterpret_cast<const FakeData**>(self + 0xc) = parseRoot;
    *result = parseCode;
    return result;
}
static const FakeData* __cdecl FakeByName(const FakeData* self, void* const* key)
{
    for (const FakeData* c : self->children)
        if (c->name == *key) return c;
    return &unknownItem;
}
static const FakeData* __cdecl FakeByIndex(const FakeData* self, int i)
{
    return i >= 0 && i < static_cast<int>(self->children.size()) ? self->children[i] : &unknownItem;
}
static int __cdecl FakeChildCount(const FakeData* self) { return static_cast<int>(self->children.size()); }
static void* const* __cdecl FakeValues(const FakeData* self)
{
    return self->noArray || self->values.empty() ? nullptr : self->values.data();
}

// ---- File (Save) and UId::SetRnd
static int openFails;
static int* __cdecl FakeOpen(void*, int* result, const char* path, unsigned long mode, int share)
{
    Log("open %s %lx %d", path ? path : "(null)", mode, share);
    *result = openFails ? 3 : 0;
    return result;
}
static unsigned long __cdecl FakeWriteLine(void*, const char* text, int flags)
{
    std::string t = text ? text : "(null)";
    for (char& c : t)
        if (c == '\n') c = '|';
    Log("write [%s] %d", t.c_str(), flags);
    return 0;
}
static unsigned long __cdecl FakeClose(void*)
{
    Log("close");
    return 0;
}
static unsigned rndCounter;
static void __cdecl FakeSetRnd(unsigned char* id)
{
    for (int i = 0; i < 16; ++i) id[i] = static_cast<unsigned char>(rndCounter * 37 + i * 11);
    Log("SetRnd");
}

// ---- the stand-in database: GetFileName (slot 3) gives "db/<name>"
static void* dbVtable[8];
static void** dbObject[1] = {dbVtable};
static void** __cdecl FakeGetFileName(void*, void** ret, void* const* name)
{
    Log("GetFileName %s", Text(*name));
    *ret = createNode(*cache, reinterpret_cast<const unsigned char*>((std::string("db/") + Text(*name)).c_str()));
    return ret;
}

// ---- stand-in devices: InputDevice-shaped (ObserverSubject vptr, list, lock; ReferenceCount vptr, refs; id)
struct FakeDevice {
    void** vtable;           // +0x00
    void* list;              // +0x04
    int lock;                // +0x08
    void** rcVtable;         // +0x0c
    int refs;                // +0x10
    unsigned char id[16];    // +0x14
    int tag;
    int type;
    unsigned long buttonMask, axisMask, povMask;
    bool answerRange[8], answerThreshold[8];
    int range[8][2], threshold[8];
};
constexpr int kDevices = 6;
static FakeDevice devices[2][kDevices];
static unsigned char* mapperOf[2];

static const char* ObserverName(const void* o)
{
    static char buf[32];
    if (o == mapperOf[side] + 0xc) return "mapper";
    std::snprintf(buf, sizeof(buf), "%p?", o);
    return buf;
}
static void __cdecl DevAddObserver(FakeDevice* d, void* o, int prio) { Log("dev%d AddObserver %s %d", d->tag, ObserverName(o), prio); }
static void __cdecl DevRemoveObserver(FakeDevice* d, void* o) { Log("dev%d RemoveObserver %s", d->tag, ObserverName(o)); }
static void __cdecl DevGetCaps(FakeDevice* d, unsigned char* caps)
{
    Log("dev%d GetCaps", d->tag);
    *reinterpret_cast<int*>(caps) = d->type;
    *reinterpret_cast<unsigned long*>(caps + 0x24) = d->buttonMask;
    *reinterpret_cast<unsigned long*>(caps + 0x2c) = d->axisMask;
    *reinterpret_cast<unsigned long*>(caps + 0x34) = d->povMask;
}
static bool __cdecl DevGetProperty(FakeDevice* d, int* p)
{
    const int index = reinterpret_cast<unsigned char*>(p)[4];
    Log("dev%d GetProperty %d %d", d->tag, p[0], index);
    if (index >= 8) return false;
    if (p[0] == 3 && d->answerRange[index]) {
        p[2] = d->range[index][0];
        p[3] = d->range[index][1];
        return true;
    }
    if (p[0] == 4 && d->answerThreshold[index]) {
        p[2] = d->threshold[index];
        return true;
    }
    return false;
}
static void __cdecl DevTrap(FakeDevice* d) { Log("dev%d unexpected call", d->tag); }
static void* devVtable[18];
static void* devRcVtable[2];

// ---- observers on the mapper
struct FakeObserver {
    void** vtable;
    int tag;
    int stopEvery;  // 0 never
    int calls;
};
static FakeObserver observers[2][3];
static int __cdecl ObsOnAction(FakeObserver* o, const unsigned char* id, void* const* name, const uint32_t* e)
{
    ++o->calls;
    ++stats[0];
    const bool stop = o->stopEvery && o->calls % o->stopEvery == 0;
    Log("obs%d action %s on %02x event %x %x %d %d t%u -> %s", o->tag, Text(*name), id[0], e[0], e[1], e[2], e[3], e[4],
        stop ? "STOP" : "go");
    return stop ? 0xff : 0;
}
static void* obsVtable[1] = {reinterpret_cast<void*>(&ObsOnAction)};

// ---- the modules' exports
struct Api {
    // InputAction
    void*(__fastcall* actionCtor)(void*, void*);
    void*(__fastcall* actionCopy)(void*, void*, const void*);
    void(__fastcall* actionDtor)(void*, void*);
    void*(__cdecl* actionAssign)(void*, const void*);
    bool(__cdecl* actionEqual)(const void*, const void*);
    void(__cdecl* setName)(void*, const char*);
    void**(__cdecl* getName)(const void*, void**);
    void(__cdecl* reset)(void*);
    unsigned(__cdecl* count)(void*);
    void(__cdecl* beginAdd)(void*);
    void(__cdecl* endAdd)(void*);
    void(__cdecl* setFlush)(void*, bool);
    void(__cdecl* addKey)(void*, int, bool, bool, bool, bool, float);
    void(__cdecl* addButton)(void*, int, bool, float);
    void(__cdecl* addAxis)(void*, int, int, int, float);
    void(__cdecl* addPov)(void*, int, int, float);
    uint32_t*(__cdecl* getStrokes)(const void*);
    void(__cdecl* setStrokes)(void*, const uint32_t*);
    bool(__cdecl* testCaps)(const void*, const void*);
    bool(__cdecl* testDev)(const void*, const void*);
    void* actionVt;
    // InputActionMapper
    void*(__fastcall* ctor)(void*, void*);
    void(__fastcall* dtor)(void*, void*);
    int*(__cdecl* addDevice)(void*, int*, void*);
    void(__cdecl* removeById)(void*, const void*);
    void(__cdecl* removeDevice)(void*, void*);
    void(__cdecl* removeAll)(void*);
    bool(__cdecl* testDevice)(const void*, void*);
    bool(__cdecl* testDeviceName)(const void*, void*, void* const*);
    int*(__cdecl* load)(void*, int*, const char*, const void*, const char*);
    int*(__cdecl* save)(void*, int*, const char*, const void*, const char*);
    int*(__cdecl* addAction)(void*, int*, const void*);
    void(__cdecl* removeAction)(void*, const void*);
    void(__cdecl* removeAllActions)(void*);
    void*(__cdecl* enumDevices)(void*);
    void*(__cdecl* enumActions)(void*);
    void* vt[3];
    explicit Api(HMODULE m)
    {
        actionCtor = Get<decltype(actionCtor)>(m, "??0InputAction@Jet@@QAE@XZ");
        actionCopy = Get<decltype(actionCopy)>(m, "??0InputAction@Jet@@QAE@ABV01@@Z");
        actionDtor = Get<decltype(actionDtor)>(m, "??1InputAction@Jet@@UAE@XZ");
        actionAssign = Get<decltype(actionAssign)>(m, "??4InputAction@Jet@@QAAAAV01@ABV01@@Z");
        actionEqual = Get<decltype(actionEqual)>(m, "??8InputAction@Jet@@QBA_NABV01@@Z");
        setName = Get<decltype(setName)>(m, "?SetName@InputAction@Jet@@QAAXPBD@Z");
        getName = Get<decltype(getName)>(m, "?GetName@InputAction@Jet@@QBA?AVPString@2@XZ");
        reset = Get<decltype(reset)>(m, "?Reset@InputAction@Jet@@QAAXXZ");
        count = Get<decltype(count)>(m, "?Count@InputAction@Jet@@QAAIXZ");
        beginAdd = Get<decltype(beginAdd)>(m, "?BeginAdd@InputAction@Jet@@QAAXXZ");
        endAdd = Get<decltype(endAdd)>(m, "?EndAdd@InputAction@Jet@@QAAXXZ");
        setFlush = Get<decltype(setFlush)>(m, "?SetFlush@InputAction@Jet@@QAAX_N@Z");
        addKey = Get<decltype(addKey)>(m, "?AddKey@InputAction@Jet@@QAAXW4KeyCode@2@_N111M@Z");
        addButton = Get<decltype(addButton)>(m, "?AddButton@InputAction@Jet@@QAAXW4ButtonCode@2@_NM@Z");
        addAxis = Get<decltype(addAxis)>(m, "?AddAxis@InputAction@Jet@@QAAXW4AxisCode@2@W4AxisModification@2@W4AxisDir@2@M@Z");
        addPov = Get<decltype(addPov)>(m, "?AddPov@InputAction@Jet@@QAAXW4PovCode@2@W4PovDir@2@M@Z");
        getStrokes = Get<decltype(getStrokes)>(m, "?GetStrokeList@InputAction@Jet@@QBAPAV?$Array@UStrokeDef@Jet@@$01@2@XZ");
        setStrokes = Get<decltype(setStrokes)>(m, "?SetStrokeList@InputAction@Jet@@QAAXABV?$Array@UStrokeDef@Jet@@$01@2@@Z");
        testCaps = Get<decltype(testCaps)>(m, "?TestDevice@InputAction@Jet@@QBA_NABVInputDeviceCaps@2@@Z");
        testDev = Get<decltype(testDev)>(m, "?TestDevice@InputAction@Jet@@QBA_NPBVInputDevice@2@@Z");
        actionVt = Get<void*>(m, "??_7InputAction@Jet@@6B@");
        ctor = Get<decltype(ctor)>(m, "??0InputActionMapper@Jet@@QAE@XZ");
        dtor = Get<decltype(dtor)>(m, "??1InputActionMapper@Jet@@UAE@XZ");
        addDevice = Get<decltype(addDevice)>(m, "?AddDevice@InputActionMapper@Jet@@QAA?AV?$ResultClass@$1?results@InputActionMapper@Jet@@2QBUResultTable@2@BW4Results@InputActionMapper@Jet@@@2@PAVInputDevice@2@@Z");
        removeById = Get<decltype(removeById)>(m, "?RemoveDevice@InputActionMapper@Jet@@QAAXABVUId@2@@Z");
        removeDevice = Get<decltype(removeDevice)>(m, "?RemoveDevice@InputActionMapper@Jet@@QAAXPAVInputDevice@2@@Z");
        removeAll = Get<decltype(removeAll)>(m, "?RemoveAllDevices@InputActionMapper@Jet@@QAAXXZ");
        testDevice = Get<decltype(testDevice)>(m, "?TestDevice@InputActionMapper@Jet@@QBA_NPAVInputDevice@2@@Z");
        testDeviceName = Get<decltype(testDeviceName)>(m, "?TestDevice@InputActionMapper@Jet@@QBA_NPAVInputDevice@2@ABVPString@2@@Z");
        load = Get<decltype(load)>(m, "?Load@InputActionMapper@Jet@@QAA?AV?$ResultClass@$1?results@InputActionMapper@Jet@@2QBUResultTable@2@BW4Results@InputActionMapper@Jet@@@2@PBDPBVResourceDatabase@2@0@Z");
        save = Get<decltype(save)>(m, "?Save@InputActionMapper@Jet@@QAA?AV?$ResultClass@$1?results@InputActionMapper@Jet@@2QBUResultTable@2@BW4Results@InputActionMapper@Jet@@@2@PBDPBVResourceDatabase@2@0@Z");
        addAction = Get<decltype(addAction)>(m, "?AddAction@InputActionMapper@Jet@@QAA?AV?$ResultClass@$1?results@InputActionMapper@Jet@@2QBUResultTable@2@BW4Results@InputActionMapper@Jet@@@2@ABVInputAction@2@@Z");
        removeAction = Get<decltype(removeAction)>(m, "?RemoveAction@InputActionMapper@Jet@@QAAXABVInputAction@2@@Z");
        removeAllActions = Get<decltype(removeAllActions)>(m, "?RemoveAllActions@InputActionMapper@Jet@@QAAXXZ");
        enumDevices = Get<decltype(enumDevices)>(m, "?EnumerateDevices@InputActionMapper@Jet@@QAAPAV?$DynamicArray@VInputDeviceCaps@Jet@@$05U?$Less@VInputDeviceCaps@Jet@@V12@@2@@2@XZ");
        enumActions = Get<decltype(enumActions)>(m, "?EnumerateActions@InputActionMapper@Jet@@QAAPAV?$IList@VInputAction@Jet@@@2@XZ");
        vt[0] = Get<void*>(m, "??_7InputActionMapper@Jet@@6B@");
        vt[1] = Get<void*>(m, "??_7InputActionMapper@Jet@@6BInputObserver@1@@");
        vt[2] = Get<void*>(m, "??_7InputActionMapper@Jet@@6BReferenceCount@1@@");
    }
};
static const Api* apis[2];

template <class R, class... A>
static R VCall(void* obj, int slot, A... a)
{
    return reinterpret_cast<R(__cdecl*)(void*, A...)>((*static_cast<void***>(obj))[slot])(obj, a...);
}

// ---- dumps
static std::string DumpAction(const unsigned char* a, bool bound)
{
    std::string s;
    char buf[96];
    const void* vt = *reinterpret_cast<void* const*>(a);
    std::snprintf(buf, sizeof(buf), "{%s %s %s n%u/%u", vt == apis[side]->actionVt ? "A" : "D",
                  Text(*reinterpret_cast<void* const*>(a + 0xc)), a[0x10] ? "flush" : "-",
                  *reinterpret_cast<const unsigned*>(a + 0x14), *reinterpret_cast<const unsigned*>(a + 0x18));
    s += buf;
    const auto* data = *reinterpret_cast<const uint32_t* const*>(a + 0x1c);
    for (unsigned i = 0; i < *reinterpret_cast<const unsigned*>(a + 0x14); ++i) {
        std::snprintf(buf, sizeof(buf), " %x/%u", data[2 * i], data[2 * i + 1]);
        s += buf;
    }
    if (bound) {
        std::snprintf(buf, sizeof(buf), " t%u p%u", *reinterpret_cast<const unsigned*>(a + 0x20), *reinterpret_cast<const unsigned*>(a + 0x24));
        s += buf;
    }
    return s + "}";
}

// an IList through its head: the actions, and the ring checked both ways
static std::string DumpList(const unsigned char* head, bool bound)
{
    std::string s;
    char buf[48];
    const unsigned count = *reinterpret_cast<const unsigned*>(head + 8);
    std::snprintf(buf, sizeof(buf), "[%u", count);
    s += buf;
    const unsigned char* link = *reinterpret_cast<unsigned char* const*>(head);
    const unsigned char* prev = head;
    unsigned n = 0;
    while (link != head && n < 1000) {
        if (*reinterpret_cast<unsigned char* const*>(link + 4) != prev) s += " BADPREV";
        s += " " + DumpAction(link - 4, bound);
        prev = link;
        link = *reinterpret_cast<unsigned char* const*>(link);
        ++n;
    }
    if (n != count) s += " BADCOUNT";
    return s + "]";
}

static std::string DumpMapper(const unsigned char* m)
{
    std::string s;
    char buf[128];
    const Api& api = *apis[side];
    for (int i = 0; i < 3; ++i)
        if (*reinterpret_cast<void* const*>(m + (i == 0 ? 0 : i == 1 ? 0xc : 0x10)) != api.vt[i]) s += " BADVT";
    // observers: FDA of {valid, observer, priority}
    s += " obs";
    if (const auto* head = *reinterpret_cast<const unsigned char* const*>(m + 4)) {
        const unsigned char* c = head;
        do {
            const unsigned cnt = *reinterpret_cast<const unsigned*>(c + 8);
            for (unsigned i = 0; i < cnt; ++i) {
                const unsigned char* e = c + 0xc + 12 * i;
                const auto* o = *reinterpret_cast<FakeObserver* const*>(e + 4);
                std::snprintf(buf, sizeof(buf), " %s%d/%d", e[0] ? "" : "!", o->tag, *reinterpret_cast<const int*>(e + 8));
                s += buf;
            }
            c = *reinterpret_cast<const unsigned char* const*>(c);
        } while (c != head);
    }
    std::snprintf(buf, sizeof(buf), " lock%d refs%d act%d ex%d table%u/%u", *reinterpret_cast<const int*>(m + 8),
                  *reinterpret_cast<const int*>(m + 0x14), m[0x18], m[0x19], *reinterpret_cast<const unsigned*>(m + 0x20),
                  *reinterpret_cast<const unsigned*>(m + 0x24));
    s += buf;
    const auto* buckets = *reinterpret_cast<unsigned char* const* const*>(m + 0x1c);
    for (unsigned b = 0; b < *reinterpret_cast<const unsigned*>(m + 0x24); ++b) {
        const unsigned char* head = buckets[b];
        if (!head) continue;
        std::snprintf(buf, sizeof(buf), " b%u:", b);
        s += buf;
        const unsigned char* c = head;
        do {
            const unsigned cnt = *reinterpret_cast<const unsigned*>(c + 8);
            std::snprintf(buf, sizeof(buf), "<%u>", cnt);
            s += buf;
            for (unsigned i = 0; i < cnt; ++i) {
                const unsigned char* list = *reinterpret_cast<unsigned char* const*>(c + 0xc + 4 * i);
                s += std::string(" ") + Text(*reinterpret_cast<void* const*>(list)) + DumpList(list + 4, false);
            }
            c = *reinterpret_cast<const unsigned char* const*>(c);
        } while (c != head);
    }
    // devices
    const unsigned char* const dhead = m + 0x28;
    std::snprintf(buf, sizeof(buf), " devices%u", *reinterpret_cast<const unsigned*>(m + 0x30));
    s += buf;
    const unsigned char* d = *reinterpret_cast<unsigned char* const*>(dhead);
    const unsigned char* prev = dhead;
    for (int n = 0; d != dhead && n < 100; ++n) {
        if (*reinterpret_cast<unsigned char* const*>(d + 4) != prev) s += " BADPREV";
        const auto* dev = *reinterpret_cast<const FakeDevice* const*>(d + 8);
        std::snprintf(buf, sizeof(buf), " (dev%d refs%d z", dev ? dev->tag : -1, dev ? dev->refs : 0);
        s += buf;
        for (int z = 0; z < 8; ++z) {
            std::snprintf(buf, sizeof(buf), " %d,%d", *reinterpret_cast<const int*>(d + 0x18 + 8 * z),
                          *reinterpret_cast<const int*>(d + 0x1c + 8 * z));
            s += buf;
        }
        s += " " + DumpList(d + 0xc, true) + ")";
        prev = d;
        d = *reinterpret_cast<unsigned char* const*>(d);
    }
    return s;
}

// ---- random actions, described once and built on each side through its exports
struct Stroke {
    int kind;  // 0 key, 1 button, 2 axis, 3 pov
    int code, a, b, c, d;
    float time;
};
struct ActionSpec {
    std::string name;
    bool flush;
    std::vector<Stroke> strokes;
};
// (the long ones differ only before their last 16 characters: the hash sees the tail)
static const char* const actionNames[] = {"Jump", "Fire", "Brake", "Horn", "Look", "jump", "A", "Throttle_Up",
                                          "Camera_Rotate_Left_Fast", "Xamera_Rotate_Left_Fast"};
constexpr unsigned kNames = sizeof(actionNames) / sizeof(actionNames[0]);

static float RandomTime()
{
    static const float times[] = {0.5f, 0.25f, 1.0f, 0.0015f, 0.1234f, 2.5f, 0.0f, 0.0005f, 0.0025f};
    return Pick(3) ? times[Pick(9)] : static_cast<float>(Pick(3000)) / 1000.0f;
}

static ActionSpec RandomAction()
{
    ActionSpec a;
    a.name = actionNames[Pick(kNames)];
    a.flush = Pick(4) == 0;
    const unsigned n = 1 + Pick(Pick(3) ? 3 : 6);
    const int family = static_cast<int>(Pick(5));  // mostly one kind per action, so some devices fit
    for (unsigned i = 0; i < n; ++i) {
        Stroke s{};
        s.kind = family < 4 && Pick(5) ? family : static_cast<int>(Pick(4));
        switch (s.kind) {
        case 0: s.code = Pick(4) ? 0x10 + static_cast<int>(Pick(6)) : static_cast<int>(Pick(256)); break;
        case 1: s.code = Pick(4) ? static_cast<int>(Pick(6)) : static_cast<int>(Pick(40)); break;
        case 2: s.code = static_cast<int>(Pick(8)); break;
        case 3: s.code = static_cast<int>(Pick(4)); break;
        }
        s.a = Pick(3) == 0, s.b = Pick(3) == 0, s.c = Pick(3) == 0, s.d = Pick(3) != 0;
        if (s.kind == 2) s.a = static_cast<int>(Pick(4)), s.b = static_cast<int>(Pick(4));
        if (s.kind == 3) s.a = static_cast<int>(Pick(10));
        s.time = RandomTime();
        a.strokes.push_back(s);
    }
    return a;
}

static void Build(const Api& api, void* obj, const ActionSpec& a)
{
    api.actionCtor(obj, nullptr);
    api.setName(obj, a.name.c_str());
    api.setFlush(obj, a.flush);
    api.beginAdd(obj);
    for (const Stroke& s : a.strokes) {
        switch (s.kind) {
        case 0: api.addKey(obj, s.code, s.a != 0, s.b != 0, s.c != 0, s.d != 0, s.time); break;
        case 1: api.addButton(obj, s.code, s.d != 0, s.time); break;
        case 2: api.addAxis(obj, s.code, s.a, s.b, s.time); break;
        case 3: api.addPov(obj, s.code, s.a, s.time); break;
        }
    }
}

// ---- random action maps (the trees the stand-in parser serves)
static std::vector<std::string> names;  // the original's name table

static std::string RandomPart(int what)  // 0 key, 1 button, 2 axis, 3 pov, 4 flush
{
    static const char* const common[] = {"0.25", "1.5", "0.0015", "2", "0x10", "pressed", "released", "junk"};
    if (Pick(6) == 0) return common[Pick(8)];
    switch (what) {
    case 0: {
        static const char* const m[] = {"+shift", "+ctrl", "+alt", "pressed", "released"};
        return Pick(2) ? names[0x10 + Pick(8)] : m[Pick(5)];
    }
    case 1: return Pick(2) ? names[0x100 + Pick(Pick(3) ? 6 : 0x25)] : (Pick(2) ? "pressed" : "released");
    case 2: {
        static const char* const m[] = {"inc", "dec", "changed", "positive", "negative", "increase", "decrease"};
        return Pick(2) ? names[0x125 + Pick(8)] : m[Pick(7)];
    }
    case 3: {
        static const char* const m[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW", "C", "center"};
        return Pick(2) ? names[0x12d + Pick(4)] : m[Pick(10)];
    }
    default: return Pick(2) ? "true" : "false";
    }
}

static void SetValues(FakeData* e, const std::vector<std::string>& parts)
{
    switch (Pick(4)) {
    case 0: {
        std::string v;
        for (size_t i = 0; i < parts.size(); ++i) v += (i ? " " : "") + parts[i];
        e->kind = 0;
        if (!v.empty() || Pick(2)) e->values.push_back(Node(v));
        break;
    }
    case 1:
        e->kind = 1 + static_cast<int>(Pick(3));
        if (!parts.empty()) e->values.push_back(Node(parts[0]));
        break;
    default: {
        e->kind = 4 + static_cast<int>(Pick(3));
        for (const auto& p : parts) e->values.push_back(Node(p));
        int count = static_cast<int>(e->values.size());
        if (Pick(10) == 0 && count) count = static_cast<int>(Pick(static_cast<unsigned>(count)));
        e->listData.push_back(count);
        e->data = e->listData.data();
        break;
    }
    }
    if (Pick(30) == 0) e->noArray = true;
}

static FakeData* RandomMap(const std::string& mapName)
{
    static const char* const keys[] = {"key", "button", "axis", "pov", "flush"};
    FakeData* map = NewData("actionmap");
    map->data = Node(mapName);
    const unsigned actions = Pick(6);
    for (unsigned i = 0; i < actions; ++i) {
        FakeData* a = NewData(Pick(15) ? "action" : "acton");
        a->data = Pick(12) ? Node(actionNames[Pick(kNames)]) : (Pick(2) ? Node("") : nullptr);
        const unsigned variants = Pick(4);
        for (unsigned j = 0; j < variants; ++j) {
            FakeData* v = NewData(Pick(12) ? "variant" : "flush");
            const unsigned lines = Pick(6);
            const int family = static_cast<int>(Pick(4));
            for (unsigned k = 0; k < lines; ++k) {
                const int what = Pick(4) ? (Pick(6) ? family : 4) : static_cast<int>(Pick(5));
                FakeData* e = NewData(Pick(20) ? keys[what] : "keys");
                std::vector<std::string> parts;
                for (unsigned p = 1 + Pick(4); p; --p) parts.push_back(RandomPart(what));
                if (what < 4 && Pick(3)) parts.push_back(RandomPart(what));
                SetValues(e, parts);
                v->children.push_back(e);
            }
            a->children.push_back(v);
        }
        map->children.push_back(a);
    }
    return map;
}

// ---- events made to fit the next stroke of some bound action
static void MakeEvent(uint32_t* e, const unsigned char* m, int& deviceIndex, unsigned long now)
{
    e[0] = e[1] = e[2] = e[3] = 0;
    e[4] = static_cast<uint32_t>(now);
    // a bound action of side 0's mapper to aim at
    struct Target {
        int device;
        uint32_t code;
        unsigned long deadline;  // the last time + the stroke's time, when under way
    };
    std::vector<Target> targets;
    const unsigned char* const dhead = m + 0x28;
    for (const unsigned char* d = *reinterpret_cast<unsigned char* const*>(dhead); d != dhead;
         d = *reinterpret_cast<unsigned char* const*>(d)) {
        const auto* dev = *reinterpret_cast<const FakeDevice* const*>(d + 8);
        const unsigned char* head = d + 0xc;
        for (const unsigned char* l = *reinterpret_cast<unsigned char* const*>(head); l != head;
             l = *reinterpret_cast<unsigned char* const*>(l)) {
            const unsigned char* a = l - 4;
            const unsigned count = *reinterpret_cast<const unsigned*>(a + 0x14);
            const unsigned progress = *reinterpret_cast<const unsigned*>(a + 0x24);
            const uint32_t* const data = *reinterpret_cast<const uint32_t* const*>(a + 0x1c);
            if (progress < count)
                targets.push_back({dev->tag, data[2 * progress],
                                   progress ? *reinterpret_cast<const uint32_t*>(a + 0x20) + data[2 * progress + 1] : 0});
        }
    }
    const unsigned what = Pick(20);
    if (what < 1) {
        e[0] = Pick(2) ? 0x10 : 0x20;
    } else if (what < 2) {
        e[0] = 0x70;
        e[1] = Pick(3) ? 3 + Pick(2) : Pick(10);
    } else if (!targets.empty() && what < 16) {
        const Target t = targets[Pick(static_cast<unsigned>(targets.size()))];
        deviceIndex = t.device;
        const uint32_t code = t.code;
        if (t.deadline && Pick(3) == 0) e[4] = static_cast<uint32_t>(t.deadline + Pick(3) - 1);
        e[1] = (code >> 16) & 0xff;
        switch (code >> 24) {
        case 0:
        case 1:
            e[0] = 0x30 | (Pick(4) ? ((code >> 24) ? 4 : 1) : Pick(5));
            if (code & 0x80)
                e[0] |= Pick(5) ? 0x10000 : 0x20000;
            else
                e[0] |= Pick(5) ? 0x20000 : 0x10000;
            if (Pick(8) == 0) e[0] |= 0x100000;
            e[0] |= (code & 4 ? 0x8000000 : 0) | (code & 2 ? 0x10000000 : 0) | (code & 1 ? 0x20000000 : 0);
            if (Pick(6) == 0) e[0] ^= 0x8000000u << Pick(3);
            break;
        case 2:
            e[0] = 0x50 | 4;
            e[2] = static_cast<uint32_t>(static_cast<int>(Pick(70000)) - 2000);
            e[3] = static_cast<uint32_t>(static_cast<int>(Pick(400)) - 200);
            break;
        case 3: {
            e[0] = 0x60 | 4;
            const unsigned dir = (code >> 12) & 0xf;
            e[2] = Pick(5) ? (dir == 8 ? 0xffffffffu : 4500u * dir) : (Pick(2) ? 0xffffffffu : 4500u * Pick(9));
            break;
        }
        default: e[0] = 0x30 | 1; break;
        }
        return;
    } else {
        e[0] = (Pick(4) == 0 ? 0x50 : Pick(3) == 0 ? 0x60 : 0x30) | Pick(5) | (Pick(2) ? 0x10000 : 0x20000);
        e[1] = Pick(2) ? Pick(8) : Pick(256);
        if ((e[0] & 0xf0) == 0x50) e[1] = Pick(8);
        e[2] = static_cast<uint32_t>(static_cast<int>(Pick(70000)) - 2000);
        e[3] = static_cast<uint32_t>(static_cast<int>(Pick(400)) - 200);
    }
    deviceIndex = static_cast<int>(Pick(kDevices + 1));
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_mapper <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 2000;
    SetDllDirectoryA(dir.c_str());
    HMODULE ks = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    for (const char* dep : {"Kernel_7c", "KernelResource_7c"})
        if (!LoadLibraryA((dir + "\\" + dep + ".dll").c_str())) std::printf("cannot load %s\n", dep);
    HMODULE mods[2] = {LoadLibraryA((dir + "\\InputSystem_7c.dll").c_str()),
                       LoadLibraryA((dir + "\\InputSystem_7c_orig.dll").c_str())};
    if (!ks || !mods[0] || !mods[1]) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    cache = Get<void**>(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    createNode = Get<decltype(createNode)>(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    destroyNode = Get<decltype(destroyNode)>(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    auto allocDelete = Get<void(__cdecl*)(void*)>(ks, "?Delete@Alloc@Jet@@SAXPAX@Z");
    const Api api0(mods[0]), api1(mods[1]);
    apis[0] = &api0;
    apis[1] = &api1;
    for (HMODULE m : mods) {
        PatchImport(m, "??0ConfigParser@Jet@@QAE@XZ", reinterpret_cast<void*>(&FakeParserCtor));
        PatchImport(m, "??1ConfigParser@Jet@@UAE@XZ", reinterpret_cast<void*>(&FakeParserDtor));
        PatchImport(m, "?Parse@ConfigParser@Jet@@QAA?AV?$ResultClass@$1?results@ConfigParser@Jet@@2QBUResultTable@2@BW4Results@ConfigParser@Jet@@@2@PBD@Z",
                    reinterpret_cast<void*>(&FakeParse));
        PatchImport(m, "??RConfigData@Jet@@QBAABV01@ABVPString@1@@Z", reinterpret_cast<void*>(&FakeByName));
        PatchImport(m, "??AConfigData@Jet@@QBAABV01@H@Z", reinterpret_cast<void*>(&FakeByIndex));
        PatchImport(m, "?GetChildCount@ConfigData@Jet@@QBAHXZ", reinterpret_cast<void*>(&FakeChildCount));
        PatchImport(m, "?GetPStringArray@ConfigData@Jet@@QBAPBVPString@2@XZ", reinterpret_cast<void*>(&FakeValues));
        PatchImport(m, "?Open@File@Jet@@QAA?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@PBDKH@Z",
                    reinterpret_cast<void*>(&FakeOpen));
        PatchImport(m, "?WriteLine@File@Jet@@QAAKPBDH@Z", reinterpret_cast<void*>(&FakeWriteLine));
        PatchImport(m, "?Close@File@Jet@@QAAKXZ", reinterpret_cast<void*>(&FakeClose));
        PatchImport(m, "?SetRnd@UId@Jet@@QAAXXZ", reinterpret_cast<void*>(&FakeSetRnd));
    }
    {
        auto get = Get<const void*(__cdecl*)()>(mods[1], "?Get@InputStrings@Jet@@SAABV12@XZ");
        const auto* nodes = static_cast<void* const*>(get());
        for (int i = 0; i < 0x13a; ++i) names.push_back(nodes[i] ? Text(nodes[i]) : "KC_None");
    }
    dbVtable[3] = reinterpret_cast<void*>(&FakeGetFileName);
    for (void*& v : devVtable) v = reinterpret_cast<void*>(&DevTrap);
    devVtable[1] = reinterpret_cast<void*>(&DevAddObserver);
    devVtable[2] = reinterpret_cast<void*>(&DevRemoveObserver);
    devVtable[4] = reinterpret_cast<void*>(&DevGetCaps);
    devVtable[5] = reinterpret_cast<void*>(&DevGetProperty);
    for (void*& v : devRcVtable) v = reinterpret_cast<void*>(&DevTrap);

    alignas(8) static unsigned char mapper[2][0x40];
    alignas(8) static unsigned char action[2][0x20], other[2][0x20];
    for (int sc = 0; sc < scenarios; ++sc) {
        // devices: types and abilities shared by both sides
        FakeDevice proto[kDevices];
        for (int i = 0; i < kDevices; ++i) {
            FakeDevice& d = proto[i];
            std::memset(&d, 0, sizeof(d));
            d.tag = i;
            d.type = i == 0 ? 1 : i == 1 ? 2 : static_cast<int>(1u << Pick(3));
            d.buttonMask = Pick(3) ? 0x3f : rng();
            d.axisMask = Pick(3) ? 0xff : rng();
            d.povMask = Pick(3) ? 0xf : rng();
            for (int z = 0; z < 8; ++z) {
                d.answerRange[z] = Pick(5) != 0;
                d.answerThreshold[z] = Pick(4) != 0;
                d.range[z][0] = Pick(3) ? 0 : static_cast<int>(Pick(1000)) - 500;
                d.range[z][1] = Pick(3) ? 0xffff : static_cast<int>(Pick(70000));
                d.threshold[z] = static_cast<int>(Pick(5000));
            }
            for (int b = 0; b < 16; ++b) d.id[b] = static_cast<unsigned char>(i == 5 && b ? 0x11 : 0x40 + i);
            if (i == 5) d.id[0] = 0x44;  // the same id as device 4 from the second byte on: a different id
        }
        for (side = 0; side < 2; ++side) {
            for (int i = 0; i < kDevices; ++i) {
                devices[side][i] = proto[i];
                devices[side][i].vtable = devVtable;
                devices[side][i].rcVtable = devRcVtable;
                devices[side][i].refs = 1000;
            }
            for (int i = 0; i < 3; ++i) observers[side][i] = {obsVtable, i, i == 0 ? 0 : static_cast<int>(Pick(1) + 3 + i), 0};
            std::memset(mapper[side], 0xcd, sizeof(mapper[side]));
            apis[side]->ctor(mapper[side], nullptr);
            mapperOf[side] = mapper[side];
        }
        // the same random choices for both sides: draw them once per step
        const int steps = 1 + static_cast<int>(Pick(70));
        unsigned long now = 1000;
        for (int st = 0; st < steps; ++st) {
            const unsigned op = Pick(100);
            const ActionSpec spec = RandomAction(), spec2 = Pick(3) ? spec : RandomAction();
            const unsigned a = rng(), b = rng(), c = rng();
            for (FakeData* d : pool) delete d;
            pool.clear();
            FakeData* root = NewData("");
            const std::string mapName = Pick(2) ? "Main" : "Other";
            for (unsigned k = Pick(4); k; --k) root->children.push_back(Pick(10) ? RandomMap(Pick(3) ? mapName : "Main") : NewData("junk"));
            const FakeData* rootChoice = Pick(25) ? root : nullptr;
            const int codeChoice = Pick(25) ? 0 : 1 + static_cast<int>(Pick(3));
            const bool withDb = Pick(4) == 0;
            const unsigned nameChoice = Pick(3);
            const int failOpen = Pick(15) == 0;
            uint32_t ev[5];
            int evDevice = 0;
            now += Pick(4) ? Pick(400) : Pick(5000);
            MakeEvent(ev, mapper[0], evDevice, now);
            ++stats[1 + (op % 10)];
            for (side = 0; side < 2; ++side) {
                const Api& api = *apis[side];
                unsigned char* const m = mapper[side];
                FakeDevice* const devs = devices[side];
                void* const obs = m + 0xc;
                int result[1] = {-1};
                rndCounter = static_cast<unsigned>(st);
                parseRoot = rootChoice;
                parseCode = codeChoice;
                openFails = failOpen;
                const char* const mapArg = nameChoice == 0 ? nullptr : nameChoice == 1 ? mapName.c_str() : "Missing";
                if (op < 15) {  // add an action
                    Build(api, action[side], spec);
                    api.addAction(m, result, action[side]);
                    Log("AddAction -> %d", result[0]);
                    api.actionDtor(action[side], nullptr);
                } else if (op < 22) {  // add a device
                    FakeDevice* d = a % 12 == 0 ? nullptr : &devs[a % kDevices];
                    api.addDevice(m, result, d);
                    Log("AddDevice %d -> %d", d ? d->tag : -1, result[0]);
                } else if (op < 25) {  // remove a device
                    FakeDevice* d = &devs[a % kDevices];
                    if (b % 2)
                        api.removeById(m, d->id);
                    else
                        api.removeDevice(m, d);
                    Log("RemoveDevice %d", d->tag);
                } else if (op < 26) {
                    api.removeAll(m);
                    Log("RemoveAllDevices");
                } else if (op < 30) {  // remove an action: one like an added one, or another
                    Build(api, action[side], spec2);
                    api.removeAction(m, action[side]);
                    Log("RemoveAction");
                    api.actionDtor(action[side], nullptr);
                } else if (op < 31) {
                    api.removeAllActions(m);
                    Log("RemoveAllActions");
                } else if (op < 37) {  // load
                    api.load(m, result, a % 10 || withDb ? "maps.cfg" : nullptr, withDb ? dbObject : nullptr, mapArg);
                    Log("Load -> %d", result[0]);
                } else if (op < 40) {  // save
                    api.save(m, result, a % 15 ? "out.cfg" : nullptr, withDb ? dbObject : nullptr, nameChoice ? mapName.c_str() : nullptr);
                    Log("Save -> %d", result[0]);
                } else if (op < 75) {  // an event
                    const unsigned char* id = evDevice < kDevices ? devs[evDevice].id : reinterpret_cast<const unsigned char*>("nobody-at-all..");
                    const int r = VCall<int>(obs, 0, static_cast<const void*>(id), static_cast<const void*>(ev));
                    Log("event dev%d %x %x %d %d t%u -> %x", evDevice, ev[0], ev[1], ev[2], ev[3], ev[4], r);
                } else if (op < 79) {  // observers
                    FakeObserver* o = &observers[side][a % 3];
                    if (b % 3)
                        VCall<void>(m, 1, static_cast<void*>(o), static_cast<int>(c % 5) - 2);
                    else
                        VCall<void>(m, 2, static_cast<void*>(o));
                    Log("observer %d %s", o->tag, b % 3 ? "added" : "removed");
                } else if (op < 82) {
                    VCall<void>(obs, 2, (a & 1) != 0);  // Activate
                    Log("Activate %u", a & 1);
                } else if (op < 84) {
                    VCall<void>(obs, 3, (a & 1) != 0);  // SetExclusive
                    Log("SetExclusive %u", a & 1);
                } else if (op < 86) {
                    VCall<void>(obs, 4);  // Reset
                    Log("Reset");
                } else if (op < 87) {
                    VCall<void>(obs, 5);  // Done
                    Log("Done");
                } else if (op < 90) {  // tests
                    FakeDevice* d = &devs[a % kDevices];
                    const bool all = api.testDevice(m, d);
                    void* node = Node(actionNames[b % kNames]);
                    const bool one = api.testDeviceName(m, d, &node);
                    Log("TestDevice %d -> %d %d", d->tag, all, one);
                } else if (op < 92) {  // enumerations
                    if (auto* list = static_cast<unsigned char**>(api.enumDevices(m))) {
                        std::string s = "devices";
                        if (unsigned char* head = *list) {
                            unsigned char* ch = head;
                            do {
                                const unsigned cnt = *reinterpret_cast<unsigned*>(ch + 8);
                                for (unsigned i = 0; i < cnt; ++i) {
                                    char buf[64];
                                    const unsigned char* caps = ch + 0xc + 0x38 * i;
                                    std::snprintf(buf, sizeof(buf), " %d/%lx/%lx/%lx", *reinterpret_cast<const int*>(caps),
                                                  *reinterpret_cast<const unsigned long*>(caps + 0x24),
                                                  *reinterpret_cast<const unsigned long*>(caps + 0x2c),
                                                  *reinterpret_cast<const unsigned long*>(caps + 0x34));
                                    s += buf;
                                }
                                ch = *reinterpret_cast<unsigned char**>(ch);
                            } while (ch != head);
                        }
                        Log("%s", s.c_str());
                    } else {
                        Log("devices none");
                    }
                    if (auto* list = static_cast<unsigned char*>(api.enumActions(m))) {
                        Log("actions %s", DumpList(list, false).c_str());
                        // free the copies and the list
                        unsigned char* l = *reinterpret_cast<unsigned char**>(list);
                        while (l != list) {
                            unsigned char* next = *reinterpret_cast<unsigned char**>(l);
                            // the deleting destructor (thiscall: this in ecx, the flag on the stack)
                            reinterpret_cast<void*(__fastcall*)(void*, void*, unsigned)>((*reinterpret_cast<void***>(l - 4))[0])(l - 4, nullptr, 1);
                            l = next;
                        }
                        allocDelete(list);
                    } else {
                        Log("actions none");
                    }
                } else {  // InputAction alone
                    Build(api, action[side], spec);
                    Build(api, other[side], spec2);
                    const unsigned what = a % 8;
                    if (what == 0) {
                        Log("equal %d %d", api.actionEqual(action[side], other[side]), api.actionEqual(other[side], action[side]));
                    } else if (what == 1) {
                        api.actionAssign(action[side], other[side]);
                        Log("assigned %s", DumpAction(action[side], false).c_str());
                    } else if (what == 2) {
                        uint32_t* list = api.getStrokes(action[side]);
                        api.setStrokes(other[side], list);
                        Log("strokes %u %u -> %s", list[0], list[1], DumpAction(other[side], false).c_str());
                        if (list[2]) allocDelete(reinterpret_cast<void*>(static_cast<uintptr_t>(list[2])));
                        allocDelete(list);
                    } else if (what == 3) {
                        api.endAdd(action[side]);
                        Log("EndAdd %s count %u", DumpAction(action[side], false).c_str(), api.count(action[side]));
                    } else if (what == 4) {
                        api.reset(action[side]);
                        void* name = nullptr;
                        api.getName(action[side], &name);
                        Log("Reset %s name %s", DumpAction(action[side], false).c_str(), Text(name));
                        if (name) destroyNode(*cache, name);
                    } else if (what == 5) {
                        unsigned char caps[0x38] = {};
                        *reinterpret_cast<int*>(caps) = static_cast<int>(1u << (b % 3));
                        *reinterpret_cast<unsigned long*>(caps + 0x24) = c & 0x100 ? 0x3f : b;
                        *reinterpret_cast<unsigned long*>(caps + 0x2c) = c & 0x200 ? 0xff : c;
                        *reinterpret_cast<unsigned long*>(caps + 0x34) = c & 0x400 ? 0xf : a;
                        Log("TestCaps %d / dev %d", api.testCaps(action[side], caps), api.testDev(action[side], &devs[b % kDevices]));
                    } else if (what == 6) {
                        alignas(8) unsigned char copy[0x20];
                        api.actionCopy(copy, nullptr, action[side]);
                        Log("copy %s equal %d", DumpAction(copy, false).c_str(), api.actionEqual(copy, action[side]));
                        api.actionDtor(copy, nullptr);
                    } else {
                        Log("null dev %d", api.testDev(action[side], nullptr));
                    }
                    api.actionDtor(action[side], nullptr);
                    api.actionDtor(other[side], nullptr);
                }
                Log("state%s", DumpMapper(m).c_str());
                for (int i = 0; i < kDevices; ++i) Log("dev%d refs %d", i, devs[i].refs);
            }
            for (const std::string& l : logs[0]) {  // coverage
                for (const char* key : {"Load -> 0", "Load -> 3", "Load -> 4", "Load -> 5", "-> 2",
                                        "AddAction -> 0", "AddAction -> 3", "AddAction -> 4", "Save -> 0", "Save -> 5",
                                        "write [", "-> STOP", "-> ff", "flush n", "GetFileName", "SetRnd", "TestDevice"})
                    if (l.find(key) != std::string::npos) ++coverage[key];
                if (l.rfind("AddDevice", 0) == 0) ++coverage["AddDevice" + l.substr(l.size() - 5)];
            }
            const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
            for (size_t i = 0; i < n; ++i) {
                ++checks;
                const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
                const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
                if (x != y) {
                    if (++bad <= 6) {
                        size_t at = 0;
                        while (at < x.size() && at < y.size() && x[at] == y[at]) ++at;
                        const size_t from = at > 80 ? at - 80 : 0;
                        std::printf("scenario %d step %d op %u line %zu:\n  ours ...%s\n  orig ...%s\n", sc, st, op, i,
                                    x.substr(from, 240).c_str(), y.substr(from, 240).c_str());
                    }
                    break;
                }
            }
            logs[0].clear();
            logs[1].clear();
        }
        for (side = 0; side < 2; ++side) {
            VCall<void>(mapper[side] + 0xc, 5);  // Done
            apis[side]->dtor(mapper[side], nullptr);
        }
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("observer notifications %llu; ops:", stats[0]);
    for (int i = 1; i <= 10; ++i) std::printf(" %llu", stats[i]);
    std::printf("\n");
    for (const auto& [k, v] : coverage) std::printf("  [%s] %llu\n", k.c_str(), v);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
