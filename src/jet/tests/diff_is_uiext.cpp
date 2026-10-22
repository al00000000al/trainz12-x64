// Differential test of InputSystem_7c unit U6 (InputUIExtension): our module against InputSystem_7c_orig.dll in one
// process.
//
// Each side builds its module's Input through the exported constructor and fills it in as Init would have left it
// (initialized, a stand-in keyboard and mouse among its devices), then an InputUIExtension through the exported
// constructor (over the same garbage on both sides), Init on that Input, and stand-in observers. Then random steps:
// events through the extension's InputObserver face (focus lost / got for the keyboard and the mouse, keys with
// down / up / repeat and modifier bits, characters, mouse buttons and axes with times around the double-click and
// drag thresholds, mouse position properties, other devices), Update, Activate, Reset, SetExclusive, Done / Init,
// the keyboard's state changing (modifiers), the double-click time changing. Observers log every call with its event
// and may stop the round or remove themselves. Compared: the logs and the extension's memory (pointers named).
//
// usage: diff_is_uiext.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules> [scenarios]
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
static std::mt19937 rng(777);
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

template <class T>
static T& At(void* p, size_t off)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + off);
}

template <class R, class... A>
static R VCall(void* obj, int slot, A... a)
{
    return reinterpret_cast<R(__cdecl*)(void*, A...)>((*static_cast<void***>(obj))[slot])(obj, a...);
}

static std::map<uintptr_t, std::string> names[2];
static std::string Name(const void* p)
{
    if (!p) return "0";
    auto it = names[side].find(reinterpret_cast<uintptr_t>(p));
    return it != names[side].end() ? it->second : "?";
}

static void*(__cdecl* allocNew)(unsigned);

// ---- the stand-in keyboard (0) and mouse (1): InputDevice-shaped, ReferenceCount at +0xc
alignas(8) static unsigned char devices[2][2][0x240];
static void* devVt[18];
static void* rcVt[2];
static int position[3];       // what the mouse's GetProperty(POSITION, i) answers
static bool propertyFails;
static void __cdecl DevAdd(void* d, void* o, int prio) { Log("%s AddObserver %s %d", Name(d).c_str(), Name(o).c_str(), prio); }
static void __cdecl DevRemove(void* d, void* o) { Log("%s RemoveObserver %s", Name(d).c_str(), Name(o).c_str()); }
static bool __cdecl DevGetProperty(void* d, int* p)
{
    const unsigned i = reinterpret_cast<unsigned char*>(p)[4];
    Log("%s GetProperty %d %u", Name(d).c_str(), p[0], i);
    if (propertyFails || p[0] != 5 || i > 2) return false;
    p[2] = position[i];
    return true;
}
static void __cdecl DevOther(void* d) { Log("%s other slot", Name(d).c_str()); }
static void __cdecl DeleteThis(void* rc) { Log("DeleteThis %s", Name(rc).c_str()); }

// ---- observers: InputUIExtensionObserver-shaped (one vptr); slots log, may stop or remove themselves
constexpr int OBS = 4;
alignas(8) static void* observers[2][OBS][2];
static void* obsVt[5];
static unsigned stopMask, removeMask;  // per observer: answer STOP / remove itself during the next call
static void* currentExt[2];
static void(__cdecl* removeObserver[2])(void*, void*);
static int ObsIndex(void* o)
{
    for (int i = 0; i < OBS; ++i)
        if (o == observers[side][i]) return i;
    return -1;
}
static int Answer(void* o)
{
    const int i = ObsIndex(o);
    if (removeMask >> i & 1) {
        removeObserver[side](currentExt[side], o);
        Log("  obs%d removed itself", i);
    }
    return (stopMask >> i & 1) ? 0xff : 0;
}
static void __cdecl ObsGet(void* o) { Log("  obs%d get", ObsIndex(o)); Answer(o); }
static void __cdecl ObsLost(void* o) { Log("  obs%d lost", ObsIndex(o)); Answer(o); }
static int __cdecl ObsKey(void* o, const uint32_t* e)
{
    Log("  obs%d key %08x", ObsIndex(o), e[0]);
    return Answer(o);
}
static int __cdecl ObsChar(void* o, const uint32_t* e)
{
    Log("  obs%d char %08x %04x", ObsIndex(o), e[0], e[1] & 0xffff);
    return Answer(o);
}
static unsigned long long mouseKinds[16];
static int __cdecl ObsMouse(void* o, const int32_t* e)
{
    if (side == 0) ++mouseKinds[(e[0] >> 16) & 15];
    Log("  obs%d mouse %08x b%x %d,%d d%d,%d z%d d%d", ObsIndex(o), e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
    return Answer(o);
}

static unsigned long msNow;
static unsigned long __cdecl FakeMs() { return msNow; }

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        auto* nm = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; nm->u1.AddressOfData; ++nm, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(nm->u1.Ordinal)) continue;
            if (std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nm->u1.AddressOfData)->Name, name)) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            iat->u1.Function = reinterpret_cast<uintptr_t>(fn);
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
        }
    }
}

struct Api {
    void*(__fastcall* inputCtor)(void*, void*);
    void(__fastcall* inputDtor)(void*, void*);
    void*(__fastcall* ctor)(void*, void*);
    void(__fastcall* dtor)(void*, void*);
    void*(__cdecl* getKeyboard)(void*);
    void*(__cdecl* getMouse)(void*);
    void(__cdecl* addObserver)(void*, void*, int);
    unsigned long* doubleClickTime;
    void* vt[3];
    explicit Api(HMODULE m)
    {
        inputCtor = Get<decltype(inputCtor)>(m, "??0Input@Jet@@QAE@XZ");
        inputDtor = Get<decltype(inputDtor)>(m, "??1Input@Jet@@QAE@XZ");
        ctor = Get<decltype(ctor)>(m, "??0InputUIExtension@Jet@@QAE@XZ");
        dtor = Get<decltype(dtor)>(m, "??1InputUIExtension@Jet@@UAE@XZ");
        getKeyboard = Get<decltype(getKeyboard)>(m, "?GetKeyboard@InputUIExtension@Jet@@QAAPAVKeyboardDevice@2@XZ");
        getMouse = Get<decltype(getMouse)>(m, "?GetMouse@InputUIExtension@Jet@@QAAPAVMouseDevice@2@XZ");
        addObserver = Get<decltype(addObserver)>(m, "?AddObserver@?$ObserverSubject@VInputUIExtensionObserver@Jet@@@Jet@@UAAXPAVInputUIExtensionObserver@2@H@Z");
        doubleClickTime = Get<unsigned long*>(m, "?doubleClickTime@InputUIExtension@Jet@@2KA");
        vt[0] = Get<void*>(m, "??_7InputUIExtension@Jet@@6B@");
        vt[1] = Get<void*>(m, "??_7InputUIExtension@Jet@@6BInputObserver@1@@");
        vt[2] = Get<void*>(m, "??_7InputUIExtension@Jet@@6BReferenceCount@1@@");
    }
};

static std::string Dump(unsigned char* e, const Api& api)
{
    std::string s;
    char buf[32];
    for (size_t i = 0; i < 0x148; i += 4) {
        const uint32_t v = At<uint32_t>(e, i);
        if (i == 0) s += v == reinterpret_cast<uintptr_t>(api.vt[0]) ? " vt0" : " ?";
        else if (i == 0xc) s += v == reinterpret_cast<uintptr_t>(api.vt[1]) ? " vt1" : " ?";
        else if (i == 0x10) s += v == reinterpret_cast<uintptr_t>(api.vt[2]) ? " vt2" : " ?";
        else if (i == 4) s += v ? " list" : " 0";
        else if (i == 0xfc || i == 0x100) s += " " + Name(reinterpret_cast<void*>(static_cast<uintptr_t>(v)));
        else {
            std::snprintf(buf, sizeof(buf), " %x", v);
            s += buf;
        }
    }
    // the observer list: valid flag and observer of each entry
    s += " |";
    const auto* head = At<const uint32_t*>(e, 4);
    if (head) {
        const uint32_t* c = head;
        do {
            std::snprintf(buf, sizeof(buf), " [%u", c[2]);
            s += buf;
            for (uint32_t i = 0; i < c[2]; ++i) {
                const uint32_t* entry = c + 3 + 3 * i;
                std::snprintf(buf, sizeof(buf), " %u:%d:%d", entry[0] & 0xff, ObsIndex(reinterpret_cast<void*>(static_cast<uintptr_t>(entry[1]))), entry[2]);
                s += buf;
            }
            s += "]";
            c = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(c[0]));
        } while (c != head);
    }
    return s;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_uiext <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 2000;
    SetDllDirectoryA(dir.c_str());
    HMODULE ks = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    LoadLibraryA((dir + "\\Kernel_7c.dll").c_str());
    LoadLibraryA((dir + "\\KernelResource_7c.dll").c_str());
    HMODULE mods[2] = {LoadLibraryA((dir + "\\InputSystem_7c.dll").c_str()),
                       LoadLibraryA((dir + "\\InputSystem_7c_orig.dll").c_str())};
    if (!ks || !mods[0] || !mods[1]) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    allocNew = Get<decltype(allocNew)>(ks, "?New@Alloc@Jet@@SAPAXI@Z");
    const Api api[2] = {Api(mods[0]), Api(mods[1])};
    for (int s = 0; s < 2; ++s) {
        PatchImport(mods[s], "?Ms@Time@Jet@@SAKXZ", reinterpret_cast<void*>(&FakeMs));
        removeObserver[s] = Get<void(__cdecl*)(void*, void*)>(mods[s], "?RemoveObserver@?$ObserverSubject@VInputUIExtensionObserver@Jet@@@Jet@@UAAXPAVInputUIExtensionObserver@2@@Z");
    }
    uint32_t idKeyboard[4], idMouse[4];
    std::memcpy(idKeyboard, Get<void*>(mods[1], "?UIdKeyboard@Jet@@3VUId@1@B"), 16);
    std::memcpy(idMouse, Get<void*>(mods[1], "?UIdMouse@Jet@@3VUId@1@B"), 16);
    for (void*& p : devVt) p = reinterpret_cast<void*>(&DevOther);
    devVt[1] = reinterpret_cast<void*>(&DevAdd);
    devVt[2] = reinterpret_cast<void*>(&DevRemove);
    devVt[5] = reinterpret_cast<void*>(&DevGetProperty);
    rcVt[0] = reinterpret_cast<void*>(&DeleteThis);
    rcVt[1] = reinterpret_cast<void*>(&DevOther);
    obsVt[0] = reinterpret_cast<void*>(&ObsGet);
    obsVt[1] = reinterpret_cast<void*>(&ObsLost);
    obsVt[2] = reinterpret_cast<void*>(&ObsKey);
    obsVt[3] = reinterpret_cast<void*>(&ObsChar);
    obsVt[4] = reinterpret_cast<void*>(&ObsMouse);

    alignas(8) unsigned char input[2][0x44], ext[2][0x148];
    unsigned long long kinds[32] = {};
    for (int sc = 0; sc < scenarios; ++sc) {
        const unsigned char fill = static_cast<unsigned char>(rng());
        unsigned char keyState[0x100];
        for (auto& k : keyState) k = static_cast<unsigned char>(Pick(4) ? 0 : rng());
        const unsigned long dct = Pick(3) ? 250 : 50 + Pick(400);
        const bool withMouse = Pick(8) != 0, withKeyboard = Pick(8) != 0;
        const unsigned nobs = Pick(OBS + 1);
        int prio[OBS];
        for (int& p : prio) p = static_cast<int>(Pick(5)) - 2;
        for (side = 0; side < 2; ++side) {
            const Api& f = api[side];
            names[side].clear();
            *f.doubleClickTime = dct;
            for (int d = 0; d < 2; ++d) {
                unsigned char* dev = devices[side][d];
                std::memset(dev, 0, 0x240);
                At<void*>(dev, 0) = devVt;
                At<void*>(dev, 0xc) = rcVt;
                At<int>(dev, 0x10) = 1;
                std::memcpy(dev + 0x14, d ? idMouse : idKeyboard, 16);
                At<int>(dev, 0x24) = d ? 2 : 1;
                if (!d) std::memcpy(dev + 0x28, keyState, 0x100);
                names[side][reinterpret_cast<uintptr_t>(dev)] = d ? "mouse" : "keyboard";
                names[side][reinterpret_cast<uintptr_t>(dev + 0xc)] = d ? "mouse.rc" : "keyboard.rc";
            }
            // the Input as Init leaves it, with the devices in its list (a DynamicArray chunk of AutoReferences)
            f.inputCtor(input[side], nullptr);
            input[side][0x28] = 1;
            unsigned char* chunk = static_cast<unsigned char*>(allocNew(0x40));
            At<void*>(chunk, 0) = At<void*>(chunk, 4) = chunk;
            uint32_t n = 0;
            if (withKeyboard) At<void*>(chunk, 0xc + 4 * n++) = devices[side][0];
            if (withMouse) At<void*>(chunk, 0xc + 4 * n++) = devices[side][1];
            for (uint32_t i = 0; i < n; ++i) ++At<int>(At<void*>(chunk, 0xc + 4 * i), 0x10);
            At<uint32_t>(chunk, 8) = n;
            At<void*>(input[side], 0x2c) = n ? chunk : nullptr;
            unsigned char* const e = ext[side];
            std::memset(e, fill, sizeof(ext[side]));
            f.ctor(e, nullptr);
            currentExt[side] = e;
            names[side][reinterpret_cast<uintptr_t>(e + 0xc)] = "ext";
            for (int i = 0; i < OBS; ++i) {
                observers[side][i][0] = obsVt;
                names[side][reinterpret_cast<uintptr_t>(observers[side][i])] = "obs" + std::to_string(i);
            }
            for (unsigned i = 0; i < nobs; ++i) f.addObserver(e, observers[side][i], prio[i]);
            Log("built%s", Dump(e, f).c_str());
            Log("Init -> %d", VCall<bool>(e + 0xc, 1, static_cast<void*>(input[side])));
            Log("init%s", Dump(e, f).c_str());
        }
        int mx = 300, my = 200;
        unsigned long t = 1000;
        const int steps = 1 + Pick(150);
        for (int st = 0; st < steps; ++st) {
            const unsigned op = Pick(64);
            const unsigned a = rng(), b = rng(), c = rng();
            t += Pick(4) ? Pick(120) : Pick(600);
            stopMask = Pick(6) ? 0 : rng() & 15;
            removeMask = Pick(25) ? 0 : 1u << Pick(OBS);
            uint32_t ev[5] = {0, 0, 0, 0, static_cast<uint32_t>(t)};
            int kind = -1;
            if (op < 6) {  // focus lost / got
                ev[0] = (a % 2 ? 0x10 : 0x20) | (b % 5 ? 1 + b % 2 : b % 8);
                kind = 0;
            } else if (op < 18) {  // a key
                ev[0] = 0x31 | (a % 3 == 0 ? 0x20000 : 0x10000) | (a % 7 == 0 ? 0x100000 : 0) | (b & 0x3f000000);
                ev[1] = c % 4 ? 0x10 + c % 12 : c % 256;
                kind = 1;
            } else if (op < 24) {  // a character
                ev[0] = 0x41 | (b & 0x3f100000) | (a & 0x30000);
                ev[1] = c & 0xffff;
                kind = 2;
            } else if (op < 34) {  // a mouse button
                ev[0] = 0x32 | (a % 2 ? 0x20000 : 0x10000) | (b & 0x3f000000);
                ev[1] = c % 5 ? c % 2 : c % 8;
                kind = 3;
            } else if (op < 50) {  // a mouse axis
                const unsigned axis = a % 7 ? a % 2 : a % 4;
                const int step = static_cast<int>(b % 9) - 4 + (b % 11 == 0 ? static_cast<int>(c % 120) - 60 : 0);
                int v = 0;
                if (axis == 0) v = mx += step;
                if (axis == 1) v = my += step;
                if (axis >= 2) v = static_cast<int>(c % 1000);
                ev[0] = 0x52 | (c % 9 == 0 ? 1 : 0) * 3;
                ev[1] = axis;
                ev[2] = static_cast<uint32_t>(v);
                ev[3] = static_cast<uint32_t>(step);
                kind = 4;
            } else if (op < 53) {  // a property
                ev[0] = 0x70 | (a % 4 ? 2 : 1);
                ev[1] = b % 4 ? 5 : b % 10;
                position[0] = mx = static_cast<int>(c % 800);
                position[1] = my = static_cast<int>(b % 600);
                position[2] = static_cast<int>(a % 100);
                propertyFails = false;
                kind = 5;
            }
            for (side = 0; side < 2; ++side) {
                const Api& f = api[side];
                unsigned char* const e = ext[side];
                void* const obs = e + 0xc;
                msNow = t;
                if (kind >= 0) {
                    ++kinds[kind];
                    Log("event %x %x %d %d t%u", ev[0], ev[1], ev[2], ev[3], ev[4]);
                    const int r = VCall<int>(obs, 0, static_cast<const void*>(idKeyboard), static_cast<const void*>(ev));
                    Log("-> %d", r);
                } else if (op < 55) {  // a burst on one button: down, up, down, up (a double click, or two clicks)
                    const unsigned button = a % 3;
                    const unsigned gap = b % 4 ? 5 + b % 40 : 100 + b % 200;
                    for (int k = 0; k < 4; ++k) {
                        msNow = t + gap * static_cast<unsigned>(k);
                        uint32_t be[5] = {0x32u | (k % 2 ? 0x20000u : 0x10000u), button, 0, 0, msNow};
                        if (k == 2 && c % 3 == 0) {  // the mouse moves between the clicks
                            uint32_t mv[5] = {0x52, c % 2, static_cast<uint32_t>(c % 2 ? my + 3 + c % 12 : mx + 3 + c % 12),
                                              3 + c % 12, msNow};
                            VCall<int>(obs, 0, static_cast<const void*>(idMouse), static_cast<const void*>(mv));
                        }
                        Log("burst %x %u t%u", be[0], button, be[4]);
                        VCall<int>(obs, 0, static_cast<const void*>(idMouse), static_cast<const void*>(be));
                    }
                } else if (op < 57) {
                    VCall<void>(obs, 6, static_cast<float>(a % 100) / 100.0f);
                    Log("Update");
                } else if (op < 59) {
                    VCall<void>(obs, 2, (b & 1) != 0);
                    Log("Activate %d", b & 1);
                } else if (op < 60) {
                    VCall<void>(obs, 4);
                    Log("Reset");
                } else if (op < 61) {
                    VCall<void>(obs, 3, (b & 1) != 0);
                    Log("SetExclusive %d", b & 1);
                } else if (op < 62) {  // the keyboard's state changes (modifiers)
                    for (int k = 0; k < 8; ++k) devices[side][0][0x28 + (a >> (k * 3) & 0xff)] = static_cast<unsigned char>(b >> k);
                    Log("keys changed");
                } else if (op < 63) {
                    if (a % 3) {
                        VCall<void>(obs, 5);
                        Log("Done");
                    } else {
                        Log("Init again -> %d", VCall<bool>(obs, 1, static_cast<void*>(input[side])));
                    }
                } else {
                    Log("GetKeyboard %s GetMouse %s", Name(f.getKeyboard(e)).c_str(), Name(f.getMouse(e)).c_str());
                }
                Log("ext%s", Dump(e, f).c_str());
                Log("refs %d %d", At<int>(devices[side][0], 0x10), At<int>(devices[side][1], 0x10));
            }
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
            if (std::getenv("IS_DUMP") && sc == std::atoi(std::getenv("IS_DUMP")))
                for (const std::string& l : logs[0]) std::printf("%s\n", l.c_str());
            logs[0].clear();
            logs[1].clear();
        }
        for (side = 0; side < 2; ++side) {
            api[side].dtor(ext[side], nullptr);
            Log("ext destroyed refs %d %d", At<int>(devices[side][0], 0x10), At<int>(devices[side][1], 0x10));
            input[side][0x28] = 0;  // (no Done: the lists go with the destructor)
            api[side].inputDtor(input[side], nullptr);
            Log("input destroyed refs %d %d", At<int>(devices[side][0], 0x10), At<int>(devices[side][1], 0x10));
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
    std::printf("events: focus %llu, key %llu, char %llu, button %llu, axis %llu, property %llu\n", kinds[0], kinds[1],
                kinds[2], kinds[3], kinds[4], kinds[5]);
    std::printf("mouse events by kind:");
    for (int k = 1; k <= 8; ++k) std::printf(" %d:%llu", k, mouseKinds[k]);
    std::printf("\n%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
