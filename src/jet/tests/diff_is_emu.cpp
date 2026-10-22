// Differential test of InputSystem_7c unit U4 (EmuDevice without Init / Load): our module against
// InputSystem_7c_orig.dll in one process.
//
// An EmuDevice of each module is built through the exported constructor and set up as Init would leave it (type,
// ranges, valid, a stand-in keyboard as the source). Bindings come from LoadDefaults or random Bind calls. Then both
// get the same random steps: keyboard events through the device's InputObserver face (bound keys with modifiers,
// presses and releases, repeats, lost / get, floods that overflow the 256-event queue), Update with random frame
// times, UpdateAxis, InitAxis, GetCaps, Get / SetProperty, and finally Done and the destructor. A logging observer
// on the device sees every event it sends. Compared: the logs, the device's memory (pointers named), the binding
// list and the queued events.
//
// usage: diff_is_emu.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules> [scenarios]
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static unsigned long long checks, bad;
static std::mt19937 rng(31337);
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

// ---- the stand-in keyboard: InputDevice-shaped, its observer slots log; ReferenceCount at +0xc
static void* sourceVtable[18];
static void __cdecl SourceAdd(void*, void* o, int prio) { Log("source AddObserver %d", prio); }
static void __cdecl SourceRemove(void*, void*) { Log("source RemoveObserver"); }
alignas(8) static unsigned char source[2][0x240];

// ---- an observer on the emulated device
static unsigned long long kinds[16];
static int __cdecl OnEvent(void*, const void*, const uint32_t* e)
{
    const int kind = e[0] & 0xf0;
    ++kinds[kind >> 4];
    char what[80] = "";
    if (kind == 0x30) std::snprintf(what, sizeof(what), "button %u down %u", e[1] & 0xff, (e[1] >> 8) & 0xff);
    if (kind == 0x50) std::snprintf(what, sizeof(what), "axis %u %d %d", e[1] & 0xff, e[2], e[3]);
    if (kind == 0x60) std::snprintf(what, sizeof(what), "pov %u %d", e[1] & 0xff, e[2]);
    if (kind == 0x70) std::snprintf(what, sizeof(what), "property %u", e[1]);
    Log("  event %x %s t%u", e[0], what, e[4]);
    return 0;
}
static void* watcherVtable[1] = {reinterpret_cast<void*>(&OnEvent)};
static void* watcher[1] = {watcherVtable};

// Time::Ms through both modules' import tables: a counter restarted for each side
static unsigned long msNow;
static unsigned long __cdecl FakeMs() { return ++msNow; }

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
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
        }
    }
}

struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void(__fastcall* dtor)(void*);
    void(__cdecl* bind)(void*, unsigned, bool, bool, bool, unsigned, unsigned, unsigned long);
    void(__cdecl* initAxis)(void*, unsigned, int);
    void(__cdecl* updateAxis)(void*, unsigned, float, unsigned long);
    void(__cdecl* loadDefaults)(void*);
    void(__cdecl* addObserver)(void*, void*, int);
    void* vt[3];
    explicit Api(HMODULE m)
    {
        ctor = Get<decltype(ctor)>(m, "??0EmuDevice@Jet@@AAE@XZ");
        dtor = Get<decltype(dtor)>(m, "??1EmuDevice@Jet@@UAE@XZ");
        bind = Get<decltype(bind)>(m, "?Bind@EmuDevice@Jet@@AAAXE_N00EEK@Z");
        initAxis = Get<decltype(initAxis)>(m, "?InitAxis@EmuDevice@Jet@@AAAXEH@Z");
        updateAxis = Get<decltype(updateAxis)>(m, "?UpdateAxis@EmuDevice@Jet@@AAAXEMK@Z");
        loadDefaults = Get<decltype(loadDefaults)>(m, "?LoadDefaults@EmuDevice@Jet@@QAAXXZ");
        addObserver = Get<decltype(addObserver)>(m, "?AddObserver@?$ObserverSubject@VInputObserver@Jet@@@Jet@@UAAXPAVInputObserver@2@H@Z");
        vt[0] = Get<void*>(m, "??_7EmuDevice@Jet@@6B?$ObserverSubject@VInputObserver@Jet@@@1@@");
        vt[1] = Get<void*>(m, "??_7EmuDevice@Jet@@6BReferenceCount@1@@");
        vt[2] = Get<void*>(m, "??_7EmuDevice@Jet@@6B@");
    }
};

template <class R, class... A>
static R VCall(void* obj, int slot, A... a)
{
    return reinterpret_cast<R(__cdecl*)(void*, A...)>((*static_cast<void***>(obj))[slot])(obj, a...);
}

static std::string Dump(const unsigned char* d, const Api& api)
{
    std::string s;
    char buf[32];
    for (size_t i = 0; i < 0x360; i += 4) {
        uint32_t v;
        std::memcpy(&v, d + i, 4);
        if (v == reinterpret_cast<uintptr_t>(api.vt[0])) s += " vt0";
        else if (v == reinterpret_cast<uintptr_t>(api.vt[1])) s += " vt1";
        else if (v == reinterpret_cast<uintptr_t>(api.vt[2])) s += " vt2";
        else if (i == 4 || i == 0x250 || i == 0x254) s += v ? " ptr" : " 0";  // observer list, queue, bindings
        else if (i == 0x23c) s += v == reinterpret_cast<uintptr_t>(source[side]) ? " src" : (v ? " ?" : " 0");
        else {
            std::snprintf(buf, sizeof(buf), " %x", v);
            s += buf;
        }
    }
    // bindings
    s += " |";
    const auto* head = *reinterpret_cast<const uint32_t* const*>(d + 0x254);
    if (head) {
        const uint32_t* c = head;
        do {
            std::snprintf(buf, sizeof(buf), " [%u", c[2]);
            s += buf;
            for (uint32_t i = 0; i < c[2]; ++i) {
                std::snprintf(buf, sizeof(buf), " %x", c[3 + i]);
                s += buf;
            }
            s += "]";
            c = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(c[0]));
        } while (c != head);
    }
    // queued events: type, code, time
    s += " | q";
    const uint32_t* q = reinterpret_cast<const uint32_t*>(d + 0x240);
    const auto* data = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(q[4]));
    for (uint32_t k = 0, at = q[0]; k < q[3]; ++k, at = at == q[2] ? 0 : at + 1) {
        // (the lost / got markers leave their other fields as stack contents)
        const bool key = (data[at * 5] & 0xf0) == 0x30;
        std::snprintf(buf, sizeof(buf), " %x/%x/%u", data[at * 5], key ? data[at * 5 + 1] & 0xff : 0, data[at * 5 + 4]);
        s += buf;
    }
    return s;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_emu <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 3000;
    SetDllDirectoryA(dir.c_str());
    for (const char* dep : {"KernelStandard_7c", "Kernel_7c", "KernelResource_7c"})
        if (!LoadLibraryA((dir + "\\" + dep + ".dll").c_str())) std::printf("cannot load %s\n", dep);
    HMODULE mods[2] = {LoadLibraryA((dir + "\\InputSystem_7c.dll").c_str()),
                       LoadLibraryA((dir + "\\InputSystem_7c_orig.dll").c_str())};
    if (!mods[0] || !mods[1]) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    const Api api[2] = {Api(mods[0]), Api(mods[1])};
    for (HMODULE m : mods) PatchImport(m, "?Ms@Time@Jet@@SAKXZ", reinterpret_cast<void*>(&FakeMs));
    sourceVtable[1] = reinterpret_cast<void*>(&SourceAdd);
    sourceVtable[2] = reinterpret_cast<void*>(&SourceRemove);

    alignas(8) unsigned char emu[2][0x360];
    for (int sc = 0; sc < scenarios; ++sc) {
        const int type = Pick(5) ? (Pick(2) ? 2 : 4) : static_cast<int>(Pick(3));
        const bool defaults = Pick(3) != 0;
        struct BindArgs {
            unsigned key, shift, ctrl, alt, kind, target;
            unsigned long dir;
        };
        std::vector<BindArgs> binds(defaults ? Pick(3) : 1 + Pick(25));
        for (auto& b : binds)
            b = {Pick(4) ? 0x5c + Pick(12) : Pick(256), Pick(2), Pick(2), Pick(2), Pick(4), Pick(10), Pick(5) ? Pick(4) : rng()};
        const unsigned char fill = static_cast<unsigned char>(rng());
        int range[8][2];
        for (auto& r : range) {
            r[0] = static_cast<int>(Pick(200)) - 100;
            r[1] = r[0] + static_cast<int>(Pick(3) ? 0xffff : Pick(300));
        }
        for (side = 0; side < 2; ++side) {
            unsigned char* const me = emu[side];
            std::memset(me, fill, sizeof(emu[side]));
            std::memset(source[side], 0, sizeof(source[side]));
            *reinterpret_cast<void**>(source[side]) = sourceVtable;
            *reinterpret_cast<int*>(source[side] + 0x10) = 1000;  // the reference count
            api[side].ctor(me, nullptr);
            // as Init leaves it
            *reinterpret_cast<int*>(me + 0x24) = type;
            *reinterpret_cast<int*>(me + 0x128) = type;
            for (int i = 0; i < 8; ++i) {
                *reinterpret_cast<int*>(me + 0x304 + 12 * i) = range[i][0];
                *reinterpret_cast<int*>(me + 0x308 + 12 * i) = range[i][1];
            }
            for (int i = 0; i < 3; ++i) *reinterpret_cast<int*>(me + 0x260 + 20 * i) = type == 2 ? 0 : 0x7fff;
            *reinterpret_cast<void**>(me + 0x23c) = source[side];
            me[0x234] = 1;
            if (defaults) api[side].loadDefaults(me);
            for (auto& b : binds) api[side].bind(me, b.key, b.shift, b.ctrl, b.alt, b.kind, b.target, b.dir);
            api[side].addObserver(me, watcher, 0);
            Log("built%s", Dump(me, api[side]).c_str());
        }
        const int steps = 1 + Pick(120);
        for (int st = 0; st < steps; ++st) {
            const unsigned op = Pick(40);
            const unsigned a = rng(), b = rng(), c = rng(), d = rng();
            const float dt = static_cast<float>(Pick(1000)) / 997.0f * (Pick(4) ? 0.05f : 1.0f);
            const unsigned flood = 200 + Pick(120);
            for (side = 0; side < 2; ++side) {
                msNow = 1000u * static_cast<unsigned>(st);
                unsigned char* const me = emu[side];
                void* const obs = me + 0x238;
                uint32_t e[5];
                const unsigned key = (a % 4 ? 0x5c + a % 12 : a % 256);
                if (op < 18) {  // a key event
                    e[0] = 0x31 | (b & 0x38000000) | (c % 3 == 0 ? 0x20000 : 0x10000) | (d % 9 == 0 ? 0x100000 : 0);
                    e[1] = key;
                    e[2] = e[3] = 0;
                    e[4] = d;
                    VCall<int>(obs, 0, static_cast<const void*>(source[side] + 0x14), e);
                } else if (op < 20) {  // lost / get / other kinds
                    e[0] = (a % 3 == 0 ? 0x10 : a % 3 == 1 ? 0x20 : 0x50) | 1;
                    e[1] = e[2] = e[3] = 0;
                    e[4] = d;
                    VCall<int>(obs, 0, static_cast<const void*>(source[side] + 0x14), e);
                } else if (op < 21) {  // a flood
                    for (unsigned k = 0; k < flood; ++k) {
                        e[0] = 0x31 | ((k * 7) % 3 == 0 ? 0x20000 : 0x10000);
                        e[1] = 0x5c + k % 12;
                        e[2] = e[3] = 0;
                        e[4] = k;
                        VCall<int>(obs, 0, static_cast<const void*>(source[side] + 0x14), e);
                    }
                } else if (op < 32) {
                    VCall<void>(me, 14, dt);
                } else if (op < 33) {
                    api[side].updateAxis(me, a % 8, dt, b);  // (8, 9 read ranges past the object)
                } else if (op < 34) {
                    api[side].initAxis(me, a % 8, static_cast<int>(b % 2000) - 100);
                } else if (op < 35) {
                    unsigned char caps[0x38] = {};
                    VCall<void>(me, 4, caps);
                    const auto* u = reinterpret_cast<const uint32_t*>(caps);
                    const auto* n = reinterpret_cast<const char* const*>(caps + 0x14);
                    Log("caps %x %x-%x %s %x b%u %x a%u %x p%u %x", u[0], u[1], u[4], n[0] ? n[0] + 0x18 : "-", u[7],
                        u[8], u[9], u[10], u[11], u[12], u[13]);
                } else if (op < 38) {
                    uint32_t p[4] = {a % 10, b, static_cast<uint32_t>(static_cast<int>(c % 200000) - 50000), d % 70000};
                    reinterpret_cast<unsigned char*>(p)[4] = static_cast<unsigned char>(b % 12);
                    const bool r = op < 36 ? VCall<bool>(me, 5, p) : VCall<bool>(me, 6, p);
                    Log("%s -> %d %x %x %x %x", op < 36 ? "get" : "set", r, p[0], p[1] & 0xff, p[2], p[3]);
                } else if (op < 39) {
                    me[0x234] = static_cast<unsigned char>(a % 4 ? 1 : 0);
                } else {
                    VCall<void>(me, 15);  // Done
                }
                Log("dev%s", Dump(me, api[side]).c_str());
            }
            const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
            for (size_t i = 0; i < n; ++i) {
                ++checks;
                const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
                const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
                if (x != y) {
                    if (++bad <= 6) std::printf("scenario %d step %d op %u type %d line %zu:\n  ours %s\n  orig %s\n", sc, st, op, type, i, x.c_str(), y.c_str());
                    break;
                }
            }
            logs[0].clear();
            logs[1].clear();
        }
        for (side = 0; side < 2; ++side) api[side].dtor(emu[side]);
    }
    std::printf("events sent (both sides): button %llu, axis %llu, pov %llu, property %llu, get %llu, lost %llu\n",
                kinds[3], kinds[5], kinds[6], kinds[7], kinds[2], kinds[1]);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
