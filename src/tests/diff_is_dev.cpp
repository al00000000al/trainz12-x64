// Differential test of InputSystem_7c unit U2 (InputDevice, Keyboard / Mouse / JoystickDevice, InputDriver):
// our module against InputSystem_7c_orig.dll in one process.
//
// Devices of all four kinds are built through the exported constructors around a stand-in driver (an
// InputDriver-shaped block whose reference count KernelStandard's real AddReference / ReleaseReference move).
// Stand-in observers log every event they get and, from inside the notification, return STOP or remove themselves
// or add another observer (the subject is locked then). Both sides run the same random steps: state bytes, the
// accessors, all eight event functions, NotifyObservers, Done / CriticalShutdown / CriticalStartup, the base
// virtuals, MouseDevice::SetPosition / SetBoundings (their SetProperty calls are caught by a patched vtable slot)
// and the destructor. Compared: the logs, the return values and the device's memory (vtables by name).
//
// usage: diff_is_dev.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules> [steps]
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static HMODULE mods[2];
static unsigned long long checks, bad;
static std::mt19937 rng(9001);
static unsigned Pick(unsigned n) { return rng() % n; }
static int side;
static std::vector<std::string> logs[2];

static void Log(const char* fmt, ...)
{
    char buf[256];
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

// ---- the stand-in driver: {vfptr, vbptr, factory, vtordisp, ReferenceCount{vptr, count}}
static int driverVbtable[2] = {0, 12};
static void* rcVtable[2];
alignas(8) static uint32_t driverBlock[2][6];

// ---- stand-in observers: slot 0 OnInputEvent(this, const UId*, InputEvent*)
struct Observer {
    void** vtable;
    int id;
};
static Observer observers[2][6];
static unsigned char* currentDevice[2];
struct Plan {  // what observer `id` does on its n-th call, drawn before both sides run
    std::vector<unsigned> actions;
} plans[6];
static size_t calls[2][6];

using AddFn = void(__cdecl*)(void*, void*, int);
using RemoveFn = void(__cdecl*)(void*, void*);
static AddFn addObs[2];
static RemoveFn removeObs[2];

static unsigned long long observerCalls;
static int __cdecl OnInputEvent(Observer* self, const void* uid, const uint32_t* e)
{
    ++observerCalls;
    const int kind = e[0] & 0xf0;
    char what[96];
    switch (kind) {
    case 0x30: std::snprintf(what, sizeof(what), "code %u down %u", e[1] & 0xff, (e[1] >> 8) & 0xff); break;
    case 0x40: std::snprintf(what, sizeof(what), "ch %x", e[1] & 0xffff); break;
    case 0x50: std::snprintf(what, sizeof(what), "axis %u %d %d", e[1] & 0xff, e[2], e[3]); break;
    case 0x60: std::snprintf(what, sizeof(what), "pov %u %d", e[1] & 0xff, e[2]); break;
    case 0x70: std::snprintf(what, sizeof(what), "property %u", e[1]); break;
    default: what[0] = 0;
    }
    const long uidOff = static_cast<long>(static_cast<const unsigned char*>(uid) - currentDevice[side]);
    Log("  obs %d: type %x %s time %u uid +%lx", self->id, e[0], what, e[4], uidOff);
    const std::vector<unsigned>& p = plans[self->id].actions;
    const unsigned a = p.empty() ? 0 : p[calls[side][self->id]++ % p.size()];
    if (a % 7 == 1) removeObs[side](currentDevice[side], self);
    if (a % 7 == 2) addObs[side](currentDevice[side], &observers[side][(a >> 4) % 6], static_cast<int>(a >> 8) % 5 - 2);
    return a % 3 == 0 ? 0xff : 0;
}
static void* observerVtable[1] = {reinterpret_cast<void*>(&OnInputEvent)};

// ---- SetProperty catcher for MouseDevice::SetPosition / SetBoundings
static bool __cdecl CatchSetProperty(void*, const uint32_t* p)
{
    // SetPosition never sets `b` (stack contents in both modules)
    Log("  SetProperty id %u index %u a %d b %d", p[0], p[1] & 0xff, static_cast<int>(p[2]), p[0] == 3 ? static_cast<int>(p[3]) : 0);
    return false;
}

struct Api {
    using Ctor = void*(__fastcall*)(void*, void*, void*);
    using Dtor = void(__fastcall*)(void*);
    Ctor ctors[4];  // InputDevice, Keyboard, Mouse, Joystick
    Dtor dtors[4];
    void* vtables[4][2];
    bool(__cdecl* isValid)(const void*);
    const void*(__cdecl* instanceId)(const void*);
    unsigned long(__cdecl* typeId)(const void*);
    const void*(__cdecl* getState)(const void*);
    const void*(__cdecl* getPrevState)(const void*);
    void*(__cdecl* getHandle)(void*);
    void(__cdecl* key)(void*, unsigned long, int, unsigned long);
    void(__cdecl* charE)(void*, unsigned short, int, unsigned long);
    void(__cdecl* button)(void*, int, int, unsigned long);
    void(__cdecl* axis)(void*, int, int, int, unsigned long);
    void(__cdecl* pov)(void*, int, int, unsigned long);
    void(__cdecl* property)(void*, int, unsigned long);
    void(__cdecl* lost)(void*, unsigned long);
    void(__cdecl* got)(void*, unsigned long);
    void(__cdecl* notify)(void*, uint32_t*, bool);
    void(__cdecl* done)(void*);
    void(__cdecl* shutdown)(void*);
    void(__cdecl* startup)(void*);
    bool(__cdecl* keyPressed)(const void*, unsigned char);
    bool(__cdecl* keyToggled)(const void*, unsigned char);
    int(__cdecl* mouseInt[6])(const void*);
    bool(__cdecl* mouseButton)(const void*, unsigned char);
    void(__cdecl* setPosition)(void*, const int*);
    void(__cdecl* setBoundings)(void*, const int*);
    int(__cdecl* joyInt[16])(const void*);
    bool(__cdecl* joyButton)(const void*, unsigned char);
    unsigned char(__cdecl* joyPressure)(const void*, unsigned char);
    int(__cdecl* joyPov)(const void*, unsigned char);

    explicit Api(HMODULE m)
    {
        const char* cls[4] = {"InputDevice", "KeyboardDevice", "MouseDevice", "JoystickDevice"};
        for (int i = 0; i < 4; ++i) {
            ctors[i] = Get<Ctor>(m, (std::string("??0") + cls[i] + "@Jet@@QAE@PAVInputDriver@1@@Z").c_str());
            dtors[i] = Get<Dtor>(m, (std::string("??1") + cls[i] + "@Jet@@UAE@XZ").c_str());
            vtables[i][0] = Get<void*>(
                m, (std::string("??_7") + cls[i] + "@Jet@@6B?$ObserverSubject@VInputObserver@Jet@@@1@@").c_str());
            vtables[i][1] = Get<void*>(m, (std::string("??_7") + cls[i] + "@Jet@@6BReferenceCount@1@@").c_str());
        }
        isValid = Get<decltype(isValid)>(m, "?IsValid@InputDevice@Jet@@QBA_NXZ");
        instanceId = Get<decltype(instanceId)>(m, "?GetInstanceId@InputDevice@Jet@@QBAABVUId@2@XZ");
        typeId = Get<decltype(typeId)>(m, "?GetTypeId@InputDevice@Jet@@QBAKXZ");
        getState = Get<decltype(getState)>(m, "?GetState@InputDevice@Jet@@QBAABTInputData@2@XZ");
        getPrevState = Get<decltype(getPrevState)>(m, "?GetPrevState@InputDevice@Jet@@QBAABTInputData@2@XZ");
        getHandle = Get<decltype(getHandle)>(m, "?GetHandle@InputDevice@Jet@@QAAPAXXZ");
        key = Get<decltype(key)>(m, "?KeyEvent@InputDevice@Jet@@IAAXKHK@Z");
        charE = Get<decltype(charE)>(m, "?CharEvent@InputDevice@Jet@@IAAXGHK@Z");
        button = Get<decltype(button)>(m, "?ButtonEvent@InputDevice@Jet@@IAAXHHK@Z");
        axis = Get<decltype(axis)>(m, "?AxisEvent@InputDevice@Jet@@IAAXHHHK@Z");
        pov = Get<decltype(pov)>(m, "?PovEvent@InputDevice@Jet@@IAAXHHK@Z");
        property = Get<decltype(property)>(m, "?PropertyChangedEvent@InputDevice@Jet@@IAAXW4DevicePropertyID@2@K@Z");
        lost = Get<decltype(lost)>(m, "?InputLostEvent@InputDevice@Jet@@IAAXK@Z");
        got = Get<decltype(got)>(m, "?InputGetEvent@InputDevice@Jet@@IAAXK@Z");
        notify = Get<decltype(notify)>(m, "?NotifyObservers@InputDevice@Jet@@IAAXAAUInputEvent@2@_N@Z");
        done = Get<decltype(done)>(m, "?Done@InputDevice@Jet@@MAAXXZ");
        shutdown = Get<decltype(shutdown)>(m, "?CriticalShutdown@InputDevice@Jet@@MAAXXZ");
        startup = Get<decltype(startup)>(m, "?CriticalStartup@InputDevice@Jet@@MAAXXZ");
        keyPressed = Get<decltype(keyPressed)>(m, "?IsKeyPressed@KeyboardDevice@Jet@@QBA_NE@Z");
        keyToggled = Get<decltype(keyToggled)>(m, "?IsKeyToggled@KeyboardDevice@Jet@@QBA_NE@Z");
        const char* mi[6] = {"GetRelX", "GetRelY", "GetRelZ", "GetAbsX", "GetAbsY", "GetAbsZ"};
        for (int i = 0; i < 6; ++i)
            mouseInt[i] = Get<int(__cdecl*)(const void*)>(m, (std::string("?") + mi[i] + "@MouseDevice@Jet@@QBAHXZ").c_str());
        mouseButton = Get<decltype(mouseButton)>(m, "?IsButtonPressed@MouseDevice@Jet@@QBA_NE@Z");
        setPosition = Get<decltype(setPosition)>(m, "?SetPosition@MouseDevice@Jet@@QAAXABUPoint@2@@Z");
        setBoundings = Get<decltype(setBoundings)>(m, "?SetBoundings@MouseDevice@Jet@@QAAXABURect@2@@Z");
        const char* ji[8] = {"X", "Y", "Z", "RX", "RY", "RZ", "U", "V"};
        for (int i = 0; i < 16; ++i)
            joyInt[i] = Get<int(__cdecl*)(const void*)>(
                m, (std::string("?Get") + (i < 8 ? "Rel" : "Abs") + ji[i % 8] + "@JoystickDevice@Jet@@QBAHXZ").c_str());
        joyButton = Get<decltype(joyButton)>(m, "?IsButtonPressed@JoystickDevice@Jet@@QBA_NE@Z");
        joyPressure = Get<decltype(joyPressure)>(m, "?GetButtonPressure@JoystickDevice@Jet@@QBAEE@Z");
        joyPov = Get<decltype(joyPov)>(m, "?GetPov@JoystickDevice@Jet@@QBAHE@Z");
    }
};

static std::string Words(const unsigned char* p, size_t n, const Api& api, int kind)
{
    std::string s;
    char buf[24];
    for (size_t i = 0; i < n; i += 4) {
        uint32_t v;
        std::memcpy(&v, p + i, 4);
        if (v == reinterpret_cast<uintptr_t>(api.vtables[kind][0]))
            s += " vt0";
        else if (v == reinterpret_cast<uintptr_t>(api.vtables[kind][1]))
            s += " vt1";
        else if (i == 4 || i == 0x22c) {  // the observer list head (a heap pointer) and the driver
            s += v ? (i == 0x22c ? (v == reinterpret_cast<uintptr_t>(driverBlock[side]) ? " drv" : " ?") : " list")
                   : " 0";
        } else {
            std::snprintf(buf, sizeof(buf), " %x", v);
            s += buf;
        }
    }
    return s;
}

// the observer list: priorities and which stand-in, chunk by chunk
static std::string ListDump(const unsigned char* dev)
{
    std::string s;
    char buf[48];
    const auto* head = *reinterpret_cast<const uint32_t* const*>(dev + 4);
    if (!head) return " none";
    const uint32_t* c = head;
    do {
        std::snprintf(buf, sizeof(buf), " [%u", c[2]);
        s += buf;
        for (uint32_t i = 0; i < c[2]; ++i) {
            const uint32_t* e = c + 3 + 3 * i;
            const auto* o = reinterpret_cast<const Observer*>(static_cast<uintptr_t>(e[1]));
            std::snprintf(buf, sizeof(buf), " %u/%d/%d", e[0] & 0xff, o ? o->id : -1, static_cast<int>(e[2]));
            s += buf;
        }
        s += "]";
        c = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(c[0]));
    } while (c != head);
    return s;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_dev <dir> [steps]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int steps = argc > 2 ? std::atoi(argv[2]) : 100000;
    SetDllDirectoryA(dir.c_str());
    for (const char* dep : {"KernelStandard_7c", "Kernel_7c", "KernelResource_7c"})
        if (!LoadLibraryA((dir + "\\" + dep + ".dll").c_str())) std::printf("cannot load %s\n", dep);
    mods[0] = LoadLibraryA((dir + "\\InputSystem_7c.dll").c_str());
    mods[1] = LoadLibraryA((dir + "\\InputSystem_7c_orig.dll").c_str());
    if (!mods[0] || !mods[1]) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    const Api api[2] = {Api(mods[0]), Api(mods[1])};
    for (int s = 0; s < 2; ++s) {
        addObs[s] = Get<AddFn>(mods[s], "?AddObserver@?$ObserverSubject@VInputObserver@Jet@@@Jet@@UAAXPAVInputObserver@2@H@Z");
        removeObs[s] = Get<RemoveFn>(mods[s], "?RemoveObserver@?$ObserverSubject@VInputObserver@Jet@@@Jet@@UAAXPAVInputObserver@2@@Z");
        for (int i = 0; i < 6; ++i) observers[s][i] = {observerVtable, i};
        // the driver's ReferenceCount: a real one (KernelStandard's vftable copy from the device's RC vtable)
        rcVtable[s] = nullptr;
        driverBlock[s][1] = reinterpret_cast<uintptr_t>(driverVbtable);
        driverBlock[s][4] = *reinterpret_cast<uint32_t*>(api[s].vtables[0][1]) ? reinterpret_cast<uintptr_t>(api[s].vtables[0][1]) : 0;
        driverBlock[s][5] = 1;
    }

    alignas(8) unsigned char dev[2][0x240];
    void* patched[2][18];
    int kind = -1;
    for (int step = 0; step < steps; ++step) {
        // a new device every so often, with new observer plans
        if (kind < 0 || Pick(400) == 0) {
            if (kind >= 0)
                for (side = 0; side < 2; ++side) {
                    api[side].dtors[kind](dev[side]);
                    Log("destroyed: driver refs %u", driverBlock[side][5]);
                }
            kind = static_cast<int>(Pick(4));
            for (auto& p : plans) {
                p.actions.resize(Pick(6));
                for (auto& a : p.actions) a = rng();
            }
            const unsigned char fill = static_cast<unsigned char>(rng());
            const unsigned withDriver = Pick(5);
            for (side = 0; side < 2; ++side) {
                std::memset(dev[side], fill, sizeof(dev[side]));
                std::memset(calls[side], 0, sizeof(calls[side]));
                currentDevice[side] = dev[side];
                api[side].ctors[kind](dev[side], nullptr, withDriver ? driverBlock[side] : nullptr);
                Log("built kind %d: driver refs %u", kind, driverBlock[side][5]);
                // a copy of the primary vtable with SetProperty (slot 6) caught
                std::memcpy(patched[side], api[side].vtables[kind][0], sizeof(patched[side]));
                patched[side][6] = reinterpret_cast<void*>(&CatchSetProperty);
            }
        }
        const unsigned op = Pick(30);
        const unsigned a = rng(), b = rng(), c = rng(), d = rng();
        unsigned char bytes[0x104];
        for (auto& x : bytes) x = static_cast<unsigned char>(rng());
        for (side = 0; side < 2; ++side) {
            unsigned char* const me = dev[side];
            const Api& f = api[side];
            switch (op) {
            case 0: addObs[side](me, &observers[side][a % 6], static_cast<int>(b % 5) - 2); break;
            case 1: removeObs[side](me, &observers[side][a % 6]); break;
            case 2:  // new state bytes (type kept in the low bits sometimes)
                std::memcpy(me + 0x24, bytes, 0x104);
                if (a % 2) *reinterpret_cast<int*>(me + 0x24) = static_cast<int>(b % 6);
                std::memcpy(me + 0x128, bytes + 7, 0x104 - 7);
                break;
            case 3: me[0x234] = static_cast<unsigned char>(a % 3 ? 1 : a % 7); break;
            case 4: f.key(me, a % 300, static_cast<int>(b & 0x7f0ff0f), c); break;
            case 5: f.charE(me, static_cast<unsigned short>(a), static_cast<int>(b & 0x7f0ff0f), c); break;
            case 6: f.button(me, static_cast<int>(a % 40), static_cast<int>(b & 0x7f0ff0f), c); break;
            case 7: f.axis(me, static_cast<int>(a % 10), static_cast<int>(b), static_cast<int>(c), d); break;
            case 8: f.pov(me, static_cast<int>(a % 5), static_cast<int>(b), c); break;
            case 9: f.property(me, static_cast<int>(a % 12), b); break;
            case 10: f.lost(me, a); break;
            case 11: f.got(me, a); break;
            case 12: {
                uint32_t e[5] = {a & 0x7f0fff, b, c, d, a ^ b};
                f.notify(me, e, (c & 1) != 0);
                Log("notify -> type %x", e[0]);
                break;
            }
            case 13: f.done(me); break;
            case 14: f.shutdown(me); break;
            case 15: f.startup(me); break;
            case 16:
                Log("valid %d id +%ld type %lu state +%ld prev +%ld handle %p", f.isValid(me),
                    static_cast<long>(static_cast<const unsigned char*>(f.instanceId(me)) - me), f.typeId(me),
                    static_cast<long>(static_cast<const unsigned char*>(f.getState(me)) - me),
                    static_cast<long>(static_cast<const unsigned char*>(f.getPrevState(me)) - me), f.getHandle(me));
                break;
            case 17: Log("key %d %d", f.keyPressed(me, static_cast<unsigned char>(a)), f.keyToggled(me, static_cast<unsigned char>(a))); break;
            case 18: {
                std::string s = "mouse";
                for (auto g : f.mouseInt) s += " " + std::to_string(g(me));
                s += " b" + std::to_string(f.mouseButton(me, static_cast<unsigned char>(a % 12)));
                Log("%s", s.c_str());
                break;
            }
            case 19: {
                std::string s = "joy";
                for (auto g : f.joyInt) s += " " + std::to_string(g(me));
                s += " b" + std::to_string(f.joyButton(me, static_cast<unsigned char>(a % 40)));
                s += " p" + std::to_string(f.joyPressure(me, static_cast<unsigned char>(b % 40)));
                s += " pov" + std::to_string(f.joyPov(me, static_cast<unsigned char>(c % 6)));
                Log("%s", s.c_str());
                break;
            }
            case 20:
            case 21: {  // through the patched vtable, then back
                void* const saved = *reinterpret_cast<void**>(me);
                *reinterpret_cast<void**>(me) = patched[side];
                const int r[4] = {static_cast<int>(a % 2000) - 100, static_cast<int>(b % 2000) - 100,
                                  static_cast<int>(c % 3000), static_cast<int>(d % 3000)};
                if (op == 20)
                    f.setPosition(me, r);
                else
                    f.setBoundings(me, r);
                *reinterpret_cast<void**>(me) = saved;
                break;
            }
            case 22: {  // the base virtuals through the device's own vtable
                void** vt = *reinterpret_cast<void***>(me);
                unsigned char caps[0x38] = {};
                uint32_t prop[4] = {a % 10, b, c, d};
                reinterpret_cast<void(__cdecl*)(void*, void*)>(vt[4])(me, caps);
                const bool g = reinterpret_cast<bool(__cdecl*)(void*, void*)>(vt[5])(me, prop);
                const bool s = reinterpret_cast<bool(__cdecl*)(void*, void*)>(vt[6])(me, prop);
                const void* e = reinterpret_cast<void*(__cdecl*)(void*)>(vt[7])(me);
                reinterpret_cast<void(__cdecl*)(void*)>(vt[8])(me);
                reinterpret_cast<void(__cdecl*)(void*)>(vt[9])(me);
                reinterpret_cast<void(__cdecl*)(void*, bool)>(vt[10])(me, true);
                reinterpret_cast<void(__cdecl*)(void*, bool)>(vt[11])(me, false);
                reinterpret_cast<void(__cdecl*)(void*, float)>(vt[14])(me, 0.5f);
                Log("virtuals %d %d %p", g, s, e);
                break;
            }
            default: f.key(me, a % 256, static_cast<int>(b & 0x30000), c); break;
            }
            Log("dev%s |%s", Words(me, 0x238, f, kind).c_str(), ListDump(me).c_str());
        }
        const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < n; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 10) std::printf("step %d op %u kind %d:\n  ours %s\n  orig %s\n", step, op, kind, x.c_str(), y.c_str());
                break;
            }
        }
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu observer calls\n", observerCalls / 2);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
