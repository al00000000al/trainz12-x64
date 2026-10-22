// Differential test of InputSystem_7c's EmuDevice::Load (0x10003c50): our module against InputSystem_7c_orig.dll in
// one process.
//
// KernelStandard's ConfigParser does not parse outside the game (every non-empty text ends in its "Syntax error."),
// so both modules' imports of ConfigParser (ctor, dtor, Parse) and ConfigData (operator(), operator[],
// GetChildCount, GetPStringArray) are patched to stand-ins serving a random tree: a root with "MouseEmulator" and /
// or "JoystickEmulator" sections (or neither), entries named by buttons, axes, povs (from the module's own name
// table), cut names and junk, their values keys with +shift / +ctrl / +alt, speeds, inc / dec, pov directions as
// numbers or N..NW, held as a single value, a typed value or a list (counted, empty, or with no array at all).
// Parse also fails now and then, or leaves no root. Each tree is loaded into a fresh EmuDevice of each module
// (mouse, joystick or neither). Compared: the result, the parser calls, and the device's memory (bindings, axes, and
// what an unknown axis' speed writes past the object).
//
// usage: diff_is_emuload.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules> [trees]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

static std::mt19937 rng(4711);
static unsigned Pick(unsigned n) { return rng() % n; }
static int side;
static std::vector<std::string> logs[2];
static void Log0(const std::string& s) { logs[side].push_back(s); }

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
static std::map<std::string, void*> interned;

static void* Node(const std::string& text)
{
    if (text.empty()) return nullptr;
    auto it = interned.find(text);
    if (it != interned.end()) return it->second;
    return interned[text] = createNode(*cache, reinterpret_cast<const unsigned char*>(text.c_str()));
}

static const char* Text(const void* node) { return node ? static_cast<const char*>(node) + 0x18 : ""; }

// ---- the stand-in ConfigData: what the modules read inline first ({name, kind, data}), then the tree
struct FakeData {
    void* name = nullptr;
    int kind = 0;
    const int* data = nullptr;
    std::vector<FakeData*> children;
    std::vector<void*> values;  // PString nodes
    bool noArray = false;       // GetPStringArray gives null
    std::vector<int> listData;  // for kind >= 4: the value count first
};

static FakeData unknownItem;  // what a missing key or index gives
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
    Log0("parser ctor");
    std::memset(self, 0, 0x18);
    return self;
}
static void __fastcall FakeParserDtor(unsigned char*, void*) { Log0("parser dtor"); }
static int* __cdecl FakeParse(unsigned char* self, int* result, const char* file)
{
    Log0(std::string("parse ") + file);
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


struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void(__fastcall* dtor)(void*);
    bool(__cdecl* load)(void*, const char*, const void*);
    void* vt[3];
    explicit Api(HMODULE m)
    {
        ctor = Get<decltype(ctor)>(m, "??0EmuDevice@Jet@@AAE@XZ");
        dtor = Get<decltype(dtor)>(m, "??1EmuDevice@Jet@@UAE@XZ");
        load = Get<decltype(load)>(m, "?Load@EmuDevice@Jet@@QAA_NPBDPBVResourceDatabase@2@@Z");
        vt[0] = Get<void*>(m, "??_7EmuDevice@Jet@@6B?$ObserverSubject@VInputObserver@Jet@@@1@@");
        vt[1] = Get<void*>(m, "??_7EmuDevice@Jet@@6BReferenceCount@1@@");
        vt[2] = Get<void*>(m, "??_7EmuDevice@Jet@@6B@");
    }
};

// the object and what lies past it: an unknown axis (index 0xff) with a speed writes at 0x260 + 0x14 * 0xff
constexpr size_t kSpan = 0x1800;

static std::string Dump(const unsigned char* d, const Api& api)
{
    std::string s;
    char buf[32];
    for (size_t i = 0; i < kSpan; i += 4) {
        uint32_t v;
        std::memcpy(&v, d + i, 4);
        if (v == reinterpret_cast<uintptr_t>(api.vt[0])) s += " vt0";
        else if (v == reinterpret_cast<uintptr_t>(api.vt[1])) s += " vt1";
        else if (v == reinterpret_cast<uintptr_t>(api.vt[2])) s += " vt2";
        else if (i == 4 || i == 0x250 || i == 0x254) s += v ? " ptr" : " 0";  // observer list, queue, bindings
        else {
            std::snprintf(buf, sizeof(buf), " %x", v);
            s += buf;
        }
    }
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
    return s;
}

// ---- random trees
static std::vector<std::string> names;  // the original's name table: keys, buttons, axes, povs, pov directions

static std::string Name(unsigned from, unsigned count)
{
    std::string n = names[from + Pick(count)];
    if (n.empty() || Pick(25) == 0) n = Pick(2) ? "KC_Nothing" : "BC_33";
    if (Pick(30) == 0) n.resize(Pick(static_cast<unsigned>(n.size()) + 1));
    return n;
}

static std::string Key() { return Pick(15) ? Name(1, 0xff) : Name(0, 0x100); }

static std::vector<std::string> Parts(int what)  // 0 button, 1 axis, 2 pov
{
    std::vector<std::string> parts;
    if (Pick(12)) parts.push_back(Key());
    for (const char* m : {"+shift", "+ctrl", "+alt"})
        if (Pick(4) == 0) parts.push_back(m);
    if (what == 1) {
        if (Pick(3)) parts.push_back(Pick(2) ? "inc" : "dec");
        if (Pick(2)) {
            char b[32];
            std::snprintf(b, sizeof(b), Pick(4) ? "%u" : "0x%x", Pick(5) ? Pick(1000) : static_cast<unsigned>(rng()));
            parts.push_back(b);
        }
    } else if (what == 2) {
        if (Pick(2)) {
            parts.push_back(Name(0x131, 9));
        } else {
            char b[32];
            std::snprintf(b, sizeof(b), "%u", Pick(3) ? 4500 * Pick(9) : Pick(40000));
            parts.push_back(b);
        }
    }
    if (Pick(15) == 0) parts.push_back(Pick(2) ? "pressed" : "1.5");
    for (size_t i = parts.size(); i > 1; --i) std::swap(parts[i - 1], parts[Pick(static_cast<unsigned>(i))]);
    return parts;
}

static void SetValues(FakeData* e, const std::vector<std::string>& parts)
{
    switch (Pick(4)) {
    case 0: {  // one value holding all the parts
        std::string v;
        for (size_t i = 0; i < parts.size(); ++i) v += (i ? " " : "") + parts[i];
        e->kind = 0;
        if (!v.empty() || Pick(2)) e->values.push_back(Node(v));
        break;
    }
    case 1:  // a typed single value: the first part
        e->kind = 1 + static_cast<int>(Pick(3));
        if (!parts.empty()) e->values.push_back(Node(parts[0]));
        break;
    default: {  // a list, its count first in the data
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

static FakeData* Section(const char* title)
{
    FakeData* s = NewData(title);
    s->kind = 4;
    const unsigned entries = Pick(12);
    for (unsigned i = 0; i < entries; ++i) {
        const unsigned what = Pick(10);
        std::string key;
        int kind;
        if (what < 4) key = Name(0x100, 0x25), kind = 0;
        else if (what < 7) key = Name(0x125, 8), kind = 1;
        else if (what < 9) key = Name(0x12d, 4), kind = 2;
        else key = Pick(2) ? Name(0, 0x100) : "something", kind = static_cast<int>(Pick(3));
        if (Pick(20) == 0) key += Pick(2) ? "_" : " AC_Y";  // two names in one key
        FakeData* e = NewData(key);
        SetValues(e, Parts(kind));
        s->children.push_back(e);
    }
    // the usual complete set now and then
    if (Pick(3) == 0) {
        for (auto [k, v] : {std::pair<const char*, const char*>{"BC_Left", "KC_LeftCtrl"}, {"AC_X", "KC_Left dec 600"},
                            {"AC_X", "KC_Right inc"}, {"AC_Y", "KC_Up dec"}, {"AC_Y", "KC_Down inc 400"}}) {
            FakeData* e = NewData(k);
            e->values.push_back(Node(v));
            s->children.push_back(e);
        }
    }
    return s;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_emuload <dir> [trees]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int trees = argc > 2 ? std::atoi(argv[2]) : 3000;
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
    const Api api[2] = {Api(mods[0]), Api(mods[1])};
    for (HMODULE m : mods) {
        PatchImport(m, "??0ConfigParser@Jet@@QAE@XZ", reinterpret_cast<void*>(&FakeParserCtor));
        PatchImport(m, "??1ConfigParser@Jet@@UAE@XZ", reinterpret_cast<void*>(&FakeParserDtor));
        PatchImport(m, "?Parse@ConfigParser@Jet@@QAA?AV?$ResultClass@$1?results@ConfigParser@Jet@@2QBUResultTable@2@BW4Results@ConfigParser@Jet@@@2@PBD@Z",
                    reinterpret_cast<void*>(&FakeParse));
        PatchImport(m, "??RConfigData@Jet@@QBAABV01@ABVPString@1@@Z", reinterpret_cast<void*>(&FakeByName));
        PatchImport(m, "??AConfigData@Jet@@QBAABV01@H@Z", reinterpret_cast<void*>(&FakeByIndex));
        PatchImport(m, "?GetChildCount@ConfigData@Jet@@QBAHXZ", reinterpret_cast<void*>(&FakeChildCount));
        PatchImport(m, "?GetPStringArray@ConfigData@Jet@@QBAPBVPString@2@XZ", reinterpret_cast<void*>(&FakeValues));
    }
    {
        auto get = Get<const void*(__cdecl*)()>(mods[1], "?Get@InputStrings@Jet@@SAABV12@XZ");
        const auto* nodes = static_cast<void* const*>(get());
        for (int i = 0; i < 0x13a; ++i) names.push_back(Text(nodes[i]));
    }

    unsigned long long checks = 0, bad = 0, complete = 0, withBindings = 0;
    static unsigned char emu[2][kSpan];
    for (int t = 0; t < trees; ++t) {
        for (FakeData* d : pool) delete d;
        pool.clear();
        FakeData* root = NewData("");
        root->kind = 4;
        if (Pick(10) == 0) root->children.push_back(NewData("junk"));
        const bool mouseFirst = Pick(2) != 0;
        if (Pick(6)) root->children.push_back(Section(mouseFirst ? "MouseEmulator" : "JoystickEmulator"));
        if (Pick(2)) root->children.push_back(Section(mouseFirst ? "JoystickEmulator" : "MouseEmulator"));
        parseRoot = Pick(40) ? root : nullptr;
        parseCode = Pick(30) ? 0 : static_cast<int>(1 + Pick(3));
        const int type = Pick(6) ? (Pick(2) ? 2 : 4) : static_cast<int>(Pick(3));
        const unsigned char fill = static_cast<unsigned char>(rng());
        bool result[2] = {};
        for (side = 0; side < 2; ++side) {
            unsigned char* const me = emu[side];
            std::memset(me, fill, kSpan);
            api[side].ctor(me, nullptr);
            *reinterpret_cast<int*>(me + 0x24) = type;
            *reinterpret_cast<int*>(me + 0x128) = type;
            result[side] = api[side].load(me, "emu.cfg", nullptr);
            Log0(std::string("result ") + (result[side] ? "1" : "0") + Dump(me, api[side]));
            api[side].dtor(me);
        }
        ++checks;
        bool same = logs[0] == logs[1];
        if (!same && ++bad <= 4) {
            for (size_t i = 0; i < logs[0].size() || i < logs[1].size(); ++i) {
                const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
                const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
                if (x == y) continue;
                size_t at = 0;
                while (at < x.size() && at < y.size() && x[at] == y[at]) ++at;
                const size_t from = at > 60 ? at - 60 : 0;
                std::printf("tree %d type %d line %zu:\n  ours ...%s\n  orig ...%s\n", t, type, i,
                            x.substr(from, 160).c_str(), y.substr(from, 160).c_str());
                break;
            }
            for (const FakeData* s : root->children) {
                std::printf("  [%s]\n", Text(s->name));
                for (const FakeData* e : s->children) {
                    std::printf("    %s (kind %d%s):", Text(e->name), e->kind, e->noArray ? ", no array" : "");
                    for (void* v : e->values) std::printf(" \"%s\"", Text(v));
                    if (e->data) std::printf(" count %d", e->data[0]);
                    std::printf("\n");
                }
            }
        }
        if (same) {
            if (result[0]) ++complete;
            if (logs[0].back().find('[') != std::string::npos) ++withBindings;
        }
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("trees with bindings %llu, complete %llu\n", withBindings, complete);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
