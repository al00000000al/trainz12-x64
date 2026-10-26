// Differential test of RenderSystem_7c unit U2b: ExtendedGfxLanguage (ExtendedGfxLanguage.cpp) and the texture
// combiner's parse records, ours against RenderSystem_7c_orig.dll in one process.
//
// As in diff_rs_u2: both modules' imports of Jet::Alloc::New / Delete are hooked (allocations numbered, frees
// logged by name), each scenario runs the same random script on both sides with that side's exports, and the
// logs must be equal. A language object gets a copy of its vftable whose slot 8 (the derived class's Load) logs
// what it is handed; LoadFromFile reads files from %TEMP% through a stand-in ResourceDatabase.
//
// usage: diff_rs_u2b.exe <bin dir with RenderSystem_7c.dll, RenderSystem_7c_orig.dll and the Jet modules> [rounds]
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

static HMODULE mod[2];  // 0 ours, 1 the original
static HMODULE ks;
static unsigned long long checks, bad;
static std::mt19937 rng(9300);

static void* Proc(HMODULE m, const char* name)
{
    void* p = reinterpret_cast<void*>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}
template <class T>
static T As(void* p) { return reinterpret_cast<T>(p); }

// ---- KernelStandard and the heap hooks

using NewFn = void*(__cdecl*)(unsigned);
using DeleteFn = void(__cdecl*)(void*);
static NewFn RealNew;
static DeleteFn RealDelete;
static void* (*CreateNode)(void*, const unsigned char*);
static void (*DestroyNode)(void*, void*);
static void** cache;
static void*(__fastcall* AnsiFromText)(void*, void*, const char*);
static void(__fastcall* AnsiDtor)(void*, void*);
static const char*(__cdecl* AnsiText)(const void*);
static bool(__fastcall* ReleaseRef)(const void*);
static void(__fastcall* AddRef)(const void*);

static int cur = -1;  // the side running
static std::string logText[2];
static std::map<const void*, std::string> names[2];  // pointer -> name, per side
static int allocCount[2];

static void Log(const char* fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (cur >= 0) logText[cur] += buf;
}

static std::string Name(const void* p)
{
    if (!p) return "null";
    auto it = names[cur].find(p);
    if (it != names[cur].end()) return it->second;
    return "?";
}

static void* __cdecl HookNew(unsigned n)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    if (cur >= 0) {
        char b[32];
        std::snprintf(b, sizeof b, "A%d", allocCount[cur]++);
        names[cur][p] = b;
        Log("  new %u -> %s\n", n, b);
    }
    return p;
}

static void __cdecl HookDelete(void* p)
{
    if (cur >= 0 && p) {
        Log("  free %s\n", Name(p).c_str());
        names[cur].erase(p);
    }
    RealDelete(p);
}

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

// a test object: memory from the real heap, filled, named
static void* TestNew(unsigned n, const char* fmt, int k)
{
    void* p = RealNew(n);
    std::memset(p, 0xcd, n);
    char b[32];
    std::snprintf(b, sizeof b, fmt, k);
    names[cur][p] = b;
    return p;
}
static void TestDelete(void* p)
{
    names[cur].erase(p);
    RealDelete(p);
}

// ---- strings: interned names (one reference held for the whole run), AnsiStrings in 4-byte boxes

struct Node {
    void* next;
    void* prev;
    unsigned length, capacity;
    int refs;
    unsigned id;
    char text[1];
};
static const char* kTexts[] = {"vp",  "tex.txt", "a.txt", ".txt", "txt", "x", "c0",   "lightDir",
                               "pos", "ab.txt",  "q.TXT", "",     "z.txtx", "fog", "built-in"};
constexpr int kNames = sizeof kTexts / sizeof *kTexts;
static void* nodes[kNames];
static std::string NodeText(const void* n)
{
    if (!n) return "null";
    const Node* x = static_cast<const Node*>(n);
    char b[64];
    std::snprintf(b, sizeof b, "\"%s\"/%u", x->text, x->length);
    return b;
}
static void* NewAnsi(const char* s)
{
    void* p = RealNew(4);
    AnsiFromText(p, nullptr, s);
    return p;
}
static void FreeAnsi(void* p)
{
    AnsiDtor(p, nullptr);
    RealDelete(p);
}

// ---- per-side entry points

#define F(name) Proc(mod[s], name)
#define EGL "ExtendedGfxLanguage@Jet@@"

struct Api {
    void*(__fastcall* ctor)(void*, void*, void*, int);
    void(__fastcall* vbaseDtor)(void*);
    bool(__cdecl* supported)(void*, float);
    bool(__cdecl* loadProgram)(void*, const void*, const char*, const void*);
    bool(__cdecl* loadFromFile)(void*, const void*, const void*, const void*, const void*);
    bool(__cdecl* loadNative)(void*, const void*, const char*);
    bool(__cdecl* compile)(void*);
    bool(__cdecl* bind)(const void*);
    bool(__cdecl* end)(const void*);
    void*(__cdecl* getDisplay)(const void*);
    const void*(__cdecl* getSource)(const void*);
    const void*(__cdecl* getFilename)(const void*);
    const void*(__cdecl* getProgramName)(const void*);
    const void*(__cdecl* getRegisteredName)(const void*);
    void(__cdecl* setFilename)(void*, const void*);
    void(__cdecl* setProgramName)(void*, const void*);
    void(__cdecl* setRegisteredName)(void*, const void*);
    float(__cdecl* getGeneration)(const void*);
    void(__cdecl* setGeneration)(void*, float);
    unsigned long(__cdecl* getUserData)(const void*);
    void(__cdecl* setUserData)(const void*, unsigned long);
    const void*(__cdecl* getFallback)(const void*);
    void(__cdecl* setFallback)(const void*, const void*);
    bool(__cdecl* getSymbolId)(const void*, const void*, void*);
    int(__cdecl* getLocalConstant)(const void*, const void*);
    const void*(__fastcall* getSymbolNameList)(const void*);
    void(__cdecl* initInstance)(const void*, void*);
    void(__cdecl* processDefines)(const void*, void*, const void*);
    void(__cdecl* addSymbolName)(void*, const void*, int, bool, int);
    void** vftable;
    void** vftableRc;
    const int* vbtable;
    // texture combiner records
    void*(__fastcall* modCtor)(void*, void*, int);
    void*(__fastcall* modAssign)(void*, void*, const void*);
    void*(__fastcall* regCtor)(void*, void*, int, int);
    void*(__fastcall* regCopy)(void*, void*, const void*);
    void(__fastcall* regDtor)(void*, void*);
    void*(__fastcall* regAssign)(void*, void*, const void*);
    void*(__fastcall* insCtor)(void*, void*, int, void*, void*, int, float, bool);
    void*(__fastcall* insCopy)(void*, void*, const void*);
    void*(__fastcall* insDeep)(void*, void*, const void*);
    void*(__fastcall* insAssign)(void*, void*, const void*);
    void(__fastcall* insDtor)(void*, void*);
    void** insVftable;
} api[2];

static void LoadApi(int s)
{
    Api& a = api[s];
    a.ctor = As<decltype(a.ctor)>(F("??0" EGL "IAE@PAVDisplay@1@@Z"));
    a.vbaseDtor = As<decltype(a.vbaseDtor)>(F("??_D" EGL "IAEXXZ"));
    a.loadProgram = As<decltype(a.loadProgram)>(F("?LoadProgram@" EGL "UAA_NABVPString@2@PBDPBUPreProcessorArray@2@@Z"));
    a.loadFromFile = As<decltype(a.loadFromFile)>(F("?LoadFromFile@" EGL "UAA_NPBVResourceDatabase@2@ABVPString@2@1PBUPreProcessorArray@2@@Z"));
    a.loadNative = As<decltype(a.loadNative)>(F("?LoadNativeFormat@" EGL "UAA_NABVPString@2@PBD@Z"));
    a.compile = As<decltype(a.compile)>(F("?CompileProgram@" EGL "UAA_NXZ"));
    a.bind = As<decltype(a.bind)>(F("?Bind@" EGL "UBA_NXZ"));
    a.end = As<decltype(a.end)>(F("?End@" EGL "UBA_NXZ"));
    a.getDisplay = As<decltype(a.getDisplay)>(F("?GetDisplay@" EGL "QBAPAVDisplay@2@XZ"));
    a.getSource = As<decltype(a.getSource)>(F("?GetProgramString@" EGL "QBAABVAnsiString@2@XZ"));
    a.getFilename = As<decltype(a.getFilename)>(F("?GetFilename@" EGL "QBAABVPString@2@XZ"));
    a.getProgramName = As<decltype(a.getProgramName)>(F("?GetProgramName@" EGL "QBAABVPString@2@XZ"));
    a.getRegisteredName = As<decltype(a.getRegisteredName)>(F("?GetRegisteredName@" EGL "QBAABVPString@2@XZ"));
    a.setFilename = As<decltype(a.setFilename)>(F("?SetFilename@" EGL "QAAXABVPString@2@@Z"));
    a.setProgramName = As<decltype(a.setProgramName)>(F("?SetProgramName@" EGL "QAAXABVPString@2@@Z"));
    a.setRegisteredName = As<decltype(a.setRegisteredName)>(F("?SetRegisteredName@" EGL "QAAXABVPString@2@@Z"));
    a.getGeneration = As<decltype(a.getGeneration)>(F("?GetGeneration@" EGL "QBAMXZ"));
    a.setGeneration = As<decltype(a.setGeneration)>(F("?SetGeneration@" EGL "QAAXM@Z"));
    a.getUserData = As<decltype(a.getUserData)>(F("?GetUserData@" EGL "QBAKXZ"));
    a.setUserData = As<decltype(a.setUserData)>(F("?SetUserData@" EGL "QBAXK@Z"));
    a.getFallback = As<decltype(a.getFallback)>(F("?GetFallbackProgram@" EGL "QBAPBV12@XZ"));
    a.setFallback = As<decltype(a.setFallback)>(F("?SetFallbackProgram@" EGL "QBAXPBV12@@Z"));
    a.getSymbolId = As<decltype(a.getSymbolId)>(F("?GetSymbolId@" EGL "QBA_NABVPString@2@AAUGfxSymbolId@2@@Z"));
    a.getLocalConstant = As<decltype(a.getLocalConstant)>(F("?GetLocalConstantNumber@" EGL "QBAHABVPString@2@@Z"));
    a.getSymbolNameList = As<decltype(a.getSymbolNameList)>(F("?GetSymbolNameList@" EGL "QBEABV?$Array@USymbolName@ExtendedGfxLanguage@Jet@@$02@2@XZ"));
    a.initInstance = As<decltype(a.initInstance)>(F("?InitInstance@" EGL "IBAXAAV12@@Z"));
    a.processDefines = As<decltype(a.processDefines)>(F("?ProcessDefines@" EGL "IBAXAAVAnsiString@2@PBUPreProcessorArray@2@@Z"));
    a.addSymbolName = As<decltype(a.addSymbolName)>(F("?AddSymbolName@" EGL "IAAXABVPString@2@H_NH@Z"));
    a.vftable = static_cast<void**>(F("??_7" EGL "6B01@@"));
    a.vftableRc = static_cast<void**>(F("??_7" EGL "6BReferenceCount@1@@"));
    a.vbtable = static_cast<const int*>(F("??_8" EGL "7B@"));
    a.supported = As<decltype(a.supported)>(a.vftable[0]);
#define TC "TextureCombiner@Jet@@"
    a.modCtor = As<decltype(a.modCtor)>(F("??0ModifierItem@" TC "QAE@W4ModifierType@12@@Z"));
    a.modAssign = As<decltype(a.modAssign)>(F("??4ModifierItem@" TC "QAEAAU012@ABU012@@Z"));
    a.regCtor = As<decltype(a.regCtor)>(F("??0RegisterItem@" TC "QAE@W4RegType@12@H@Z"));
    a.regCopy = As<decltype(a.regCopy)>(F("??0RegisterItem@" TC "QAE@PBU012@@Z"));
    a.regDtor = As<decltype(a.regDtor)>(F("??1RegisterItem@" TC "QAE@XZ"));
    a.regAssign = As<decltype(a.regAssign)>(F("??4RegisterItem@" TC "QAEAAU012@ABU012@@Z"));
    a.insCtor = As<decltype(a.insCtor)>(F("??0Instruction@" TC "QAE@W4TextureOpcode@12@PAU?$ParseList@URegisterItem@TextureCombiner@Jet@@@2@PAU?$ParseList@UModifierItem@TextureCombiner@Jet@@@2@W4TextureAddressType@12@M_N@Z"));
    a.insCopy = As<decltype(a.insCopy)>(F("??0Instruction@" TC "QAE@ABU012@@Z"));
    a.insDeep = As<decltype(a.insDeep)>(F("??0Instruction@" TC "QAE@PBU012@@Z"));
    a.insAssign = As<decltype(a.insAssign)>(F("??4Instruction@" TC "QAEAAU012@ABU012@@Z"));
    a.insDtor = As<decltype(a.insDtor)>(F("??1Instruction@" TC "UAE@XZ"));
    a.insVftable = static_cast<void**>(F("??_7Instruction@" TC "6B@"));
}

// ---------------------------------------------------------------------------------------------------------------
// languages

constexpr unsigned kLangSize = 0x3c;
#define TAIL " ;;;;;;;;;;;;;;;;"
static void* testVtable[2][9];  // per side: the module's slots, slot 8 our logging Load
static int loadCount;

static std::string MemText(const void* mf)
{
    const char* start = *reinterpret_cast<const char* const*>(static_cast<const char*>(mf) + 4);
    const unsigned size = *reinterpret_cast<const unsigned*>(static_cast<const char*>(mf) + 8);
    if (!start) return "(no data)";
    return std::string(start, start + (size > 300 ? 300 : size)) + "|" + std::to_string(size);
}

static bool __cdecl LoggingLoad(void* self, void* file, const void* name, const void* fileName)
{
    Log("  Load %s [%s] name %s file %s cur %u\n", Name(self).c_str(), MemText(file).c_str(),
        NodeText(*static_cast<void* const*>(name)).c_str(), NodeText(*static_cast<void* const*>(fileName)).c_str(),
        *reinterpret_cast<const char* const*>(file) - *reinterpret_cast<const char* const*>(static_cast<const char*>(file) + 4));
    return ++loadCount % 3 != 0;
}

// the stand-in ResourceDatabase: slot 3 maps a name to a file in %TEMP% (a node the caller owns)
static std::string tempDir;
static void* __cdecl FakeGetFileName(const void* db, void** out, const void* name)
{
    const void* n = *static_cast<void* const*>(name);
    std::string path = tempDir + "rs_u2b_" + (n ? static_cast<const Node*>(n)->text : "null") + ".src";
    Log("  db %s GetFileName %s\n", Name(db).c_str(), NodeText(n).c_str());
    *out = CreateNode(*cache, reinterpret_cast<const unsigned char*>(path.c_str()));
    return out;
}
static void* dbVtable[4] = {nullptr, nullptr, nullptr, reinterpret_cast<void*>(&FakeGetFileName)};
static struct {
    void** vt = dbVtable;
} fakeDb;

static void DumpLang(const char* tag, const void* l)
{
    const auto* b = static_cast<const unsigned char*>(l);
    auto u32 = [&](int off) { return *reinterpret_cast<const unsigned*>(b + off); };
    auto ptr = [&](int off) { return *reinterpret_cast<void* const*>(b + off); };
    float g;
    std::memcpy(&g, b + 0x1c, 4);
    Log("  %s %s: vft %s vbt %s disp %s src \"%s\" file %s prog %s reg %s gen %g user %u fb %s rc %u\n", tag,
        Name(l).c_str(), ptr(0) == testVtable[cur] ? "test" : ptr(0) == api[cur].vftable ? "own" : "?",
        ptr(4) == api[cur].vbtable ? "own" : "?", Name(ptr(8)).c_str(), AnsiText(b + 0xc), NodeText(ptr(0x10)).c_str(),
        NodeText(ptr(0x14)).c_str(), NodeText(ptr(0x18)).c_str(), g, u32(0x2c), Name(ptr(0x30)).c_str(), u32(0x38));
    const unsigned n = u32(0x20), cap = u32(0x24);
    const auto* data = static_cast<const unsigned char*>(ptr(0x28));
    Log("    symbols %u/%u %s:", n, cap, Name(data).c_str());
    for (unsigned i = 0; i < n; ++i) {
        const auto* e = data + 16 * i;
        Log(" %s:%d,%d,%d", NodeText(*reinterpret_cast<void* const*>(e)).c_str(), e[4],
            *reinterpret_cast<const int*>(e + 8), *reinterpret_cast<const int*>(e + 12));
    }
    Log("\n");
}

static void RunLanguages(int s, unsigned seed)
{
    std::mt19937 r(seed);
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    Api& a = api[s];
    loadCount = 0;
    std::memcpy(testVtable[s], a.vftable, sizeof testVtable[s]);
    testVtable[s][8] = reinterpret_cast<void*>(&LoggingLoad);
    names[s][&fakeDb] = "db";
    void* display = TestNew(4, "Display%d", 0);
    void* lang[3];
    for (int i = 0; i < 3; ++i) {
        lang[i] = TestNew(kLangSize, "L%d", i);
        a.ctor(lang[i], nullptr, i == 2 ? nullptr : display, 1);
        if (i < 2) *static_cast<void**>(lang[i]) = testVtable[s];
        DumpLang("new", lang[i]);
    }
    auto Rc = [&](void* l) { return static_cast<char*>(l) + a.vbtable[1] + 4; };
    auto AnyName = [&]() { return &nodes[P(0, kNames - 1)]; };
    auto SymName = [&]() { return &nodes[P(4, 8)]; };  // a few names, so lookups find what was added
    // each with a tail longer than any value: ProcessDefines resumes its search one value length past the end
    // of each replacement, which must stay inside the text
    const char* kSources[] = {"mov o0, v0\nfog\n" TAIL, "lightDir c0 x x" TAIL, TAIL, "x" TAIL, "aaaa x aaaa" TAIL,
                              "vp fog vp vp" TAIL, "built-in lightDir" TAIL};
    const char* kDefNames[] = {"x", "fog", "vp", "lightDir", "aa", "c0", "q"};
    const char* kDefValues[] = {"", "y", "longer value", "x", "fog", "c0 c0", "vpvp"};
    // define arrays: {count, capacity, data} with data = AnsiString pairs
    auto NewDefines = [&](int n) {
        auto* arr = static_cast<unsigned*>(TestNew(12, "D%d", n));
        arr[0] = n;
        arr[1] = n + P(0, 2);
        void** data = n ? static_cast<void**>(RealNew(8 * arr[1])) : nullptr;
        for (int i = 0; i < n; ++i) {
            AnsiFromText(&data[2 * i], nullptr, kDefNames[P(0, 6)]);
            AnsiFromText(&data[2 * i + 1], nullptr, kDefValues[P(0, 6)]);
        }
        arr[2] = reinterpret_cast<unsigned>(data);
        return arr;
    };
    auto FreeDefines = [&](unsigned* arr) {
        void** data = reinterpret_cast<void**>(arr[2]);
        for (unsigned i = 0; i < 2 * arr[0]; ++i) AnsiDtor(&data[i], nullptr);
        if (data) RealDelete(data);
        TestDelete(arr);
    };
    for (int step = 0; step < 40; ++step) {
        void* l = lang[P(0, 2)];
        const int op = P(0, 22);
        Log("op %d on %s\n", op, Name(l).c_str());
        switch (op) {
        case 0: a.setFilename(l, AnyName()); break;
        case 1: a.setProgramName(l, AnyName()); break;
        case 2: a.setRegisteredName(l, AnyName()); break;
        case 3: {
            float g = static_cast<float>(P(0, 40)) / 8.0f;
            a.setGeneration(l, g);
            Log("  gen %g\n", a.getGeneration(l));
            break;
        }
        case 4: a.setUserData(l, r()); Log("  user %lu\n", a.getUserData(l)); break;
        case 5: {
            void* f = P(0, 3) ? lang[P(0, 2)] : nullptr;
            a.setFallback(l, f);
            Log("  fallback %s\n", Name(a.getFallback(l)).c_str());
            break;
        }
        case 6:
        case 7: a.addSymbolName(l, SymName(), P(-2, 9), P(0, 1) != 0, P(-1, 3)); break;
        case 8: {
            unsigned char id[12];
            std::memset(id, 0xee, sizeof id);
            bool ok = a.getSymbolId(l, SymName(), id);
            Log("  id %d %u %d %d\n", ok, id[0], *reinterpret_cast<int*>(id + 4), *reinterpret_cast<int*>(id + 8));
            Log("  local %d\n", a.getLocalConstant(l, SymName()));
            break;
        }
        case 9: {
            const void* list = a.getSymbolNameList(l);
            Log("  list +%d\n", static_cast<int>(static_cast<const char*>(list) - static_cast<const char*>(l)));
            Log("  disp %s src %s\n", Name(a.getDisplay(l)).c_str(), AnsiText(a.getSource(l)));
            Log("  names %s %s %s\n", NodeText(*static_cast<void* const*>(a.getFilename(l))).c_str(),
                NodeText(*static_cast<void* const*>(a.getProgramName(l))).c_str(),
                NodeText(*static_cast<void* const*>(a.getRegisteredName(l))).c_str());
            break;
        }
        case 10: {
            void* to = lang[P(0, 2)];
            if (to == l) break;
            a.initInstance(l, to);
            DumpLang("copy", to);
            break;
        }
        case 11:
        case 12: {
            void* text = NewAnsi(kSources[P(0, 6)]);
            unsigned* d = P(0, 4) ? NewDefines(P(0, 4)) : nullptr;
            // shared or not: a second AnsiString on the same block sees the in-place writes
            void* twin = nullptr;
            if (P(0, 1)) {
                twin = RealNew(4);
                std::memcpy(twin, text, 4);
                ++*(*reinterpret_cast<int**>(twin) - 1);
            }
            a.processDefines(l, text, d);
            Log("  defines -> \"%s\"%s%s\n", AnsiText(text), twin ? " twin " : "", twin ? AnsiText(twin) : "");
            if (twin) FreeAnsi(twin);
            FreeAnsi(text);
            if (d) FreeDefines(d);
            break;
        }
        case 13:
        case 14: {
            unsigned* d = P(0, 2) ? NewDefines(P(0, 3)) : nullptr;
            bool ok = a.loadProgram(l, AnyName(), kSources[P(0, 6)], d);
            Log("  loadProgram %d\n", ok);
            if (d) FreeDefines(d);
            break;
        }
        case 15:
        case 16: {
            unsigned* d = P(0, 2) ? NewDefines(P(0, 3)) : nullptr;
            bool ok = a.loadFromFile(l, &fakeDb, AnyName(), AnyName(), d);
            Log("  loadFromFile %d\n", ok);
            if (d) FreeDefines(d);
            break;
        }
        case 17:
            Log("  stubs %d %d %d %d %d\n", a.supported(l, 1.5f), a.loadNative(l, AnyName(), "x"), a.compile(l),
                a.bind(l), a.end(l));
            break;
        case 18: AddRef(Rc(l)); Log("  addref\n"); break;
        case 19: AddRef(Rc(l)); Log("  release %d\n", ReleaseRef(Rc(l))); break;
        default: break;
        }
        DumpLang("L", l);
    }
    // the fallbacks hold references: keep only those to a later object (no cycles), so deleting an object
    // releases its fallback; then delete each through its reference count
    for (int i = 0; i < 3; ++i) {
        const void* f = a.getFallback(lang[i]);
        int j = 0;
        while (j < 3 && lang[j] != f) ++j;
        if (f && j <= i) a.setFallback(lang[i], nullptr);
    }
    for (void* l : lang) {
        const unsigned rc = *reinterpret_cast<unsigned*>(Rc(l) + 4);
        Log("refs %s %u\n", Name(l).c_str(), rc);  // less one for each deleted object it was the fallback of
        for (unsigned i = 1; i < rc; ++i) ReleaseRef(Rc(l));
        Log("final release %s %d\n", Name(l).c_str(), ReleaseRef(Rc(l)));
    }
    TestDelete(display);
}

// ---------------------------------------------------------------------------------------------------------------
// texture combiner records

static void DumpMods(const void* list)
{
    if (!list) {
        Log(" -");
        return;
    }
    const auto* l = static_cast<void* const*>(list);
    Log(" [%s %s/%s #%d:", Name(list).c_str(), Name(l[0]).c_str(), Name(l[1]).c_str(), reinterpret_cast<const int*>(l)[2]);
    for (auto* m = static_cast<const int*>(l[0]); m; m = reinterpret_cast<const int*>(m[1]))
        Log(" %s=%d", Name(m).c_str(), m[0]);
    Log("]");
}
static void DumpReg(const void* reg)
{
    const auto* r = static_cast<const int*>(reg);
    Log(" {%s %d %d", Name(reg).c_str(), r[0], r[1]);
    DumpMods(reinterpret_cast<void*>(r[2]));
    DumpMods(reinterpret_cast<void*>(r[3]));
    Log(" next %s}", Name(reinterpret_cast<void*>(r[4])).c_str());
}
static void DumpIns(const char* tag, const void* ins)
{
    const auto* w = static_cast<const int*>(ins);
    float v;
    std::memcpy(&v, w + 10, 4);
    Log("  %s %s vft %s op %d addr %d flag %d f18 %x %x %x %x value %g\n   regs", tag, Name(ins).c_str(),
        reinterpret_cast<void* const*>(ins)[0] == api[cur].insVftable ? "own" : "?", w[1], w[4],
        static_cast<const unsigned char*>(ins)[0x14], w[6], w[7], w[8], w[9], v);
    if (const auto* l = reinterpret_cast<void* const*>(w[2])) {
        Log(" [%s #%d:", Name(l).c_str(), reinterpret_cast<const int*>(l)[2]);
        for (const void* r = l[0]; r; r = reinterpret_cast<void* const*>(r)[4]) DumpReg(r);
        Log("]");
    } else {
        Log(" -");
    }
    Log("\n   mods");
    DumpMods(reinterpret_cast<void*>(w[3]));
    Log("\n");
}

static void RunRecords(int s, unsigned seed)
{
    std::mt19937 r(seed);
    auto P = [&](int lo, int hi) { return lo + static_cast<int>(r() % static_cast<unsigned>(hi - lo + 1)); };
    Api& a = api[s];
    int k = 0;
    auto List = [&]() {
        auto* l = static_cast<void**>(TestNew(12, "List%d", k++));
        l[0] = l[1] = nullptr;
        l[2] = nullptr;
        return l;
    };
    auto Push = [&](void** l, void* item, int nextOff) {
        if (!l[1])
            l[0] = item;
        else
            *reinterpret_cast<void**>(static_cast<char*>(l[1]) + nextOff) = item;
        ++reinterpret_cast<int*>(l)[2];
        l[1] = item;
    };
    auto Mods = [&]() -> void** {
        if (!P(0, 2)) return nullptr;
        void** l = List();
        for (int n = P(0, 3); n; --n) {
            void* m = TestNew(8, "M%d", k++);
            a.modCtor(m, nullptr, P(0, 6));
            Push(l, m, 4);
        }
        return l;
    };
    auto Reg = [&]() {
        void* x = TestNew(0x14, "R%d", k++);
        a.regCtor(x, nullptr, P(0, 9), P(-1, 7));
        reinterpret_cast<void**>(x)[2] = Mods();
        reinterpret_cast<void**>(x)[3] = Mods();
        return x;
    };
    for (int round = 0; round < 6; ++round) {
        void** regs = P(0, 3) ? List() : nullptr;
        if (regs)
            for (int n = P(0, 4); n; --n) Push(regs, Reg(), 0x10);
        void* ins = TestNew(0x2c, "I%d", k++);
        a.insCtor(ins, nullptr, P(0, 30), regs, Mods(), P(0, 4), static_cast<float>(P(-8, 8)) / 4, P(0, 1) != 0);
        DumpIns("ins", ins);
        // deep and shallow copies
        void* deep = TestNew(0x2c, "I%d", k++);
        a.insDeep(deep, nullptr, ins);
        DumpIns("deep", deep);
        void* shallow = TestNew(0x2c, "I%d", k++);
        a.insCopy(shallow, nullptr, deep);
        DumpIns("shallow", shallow);
        void* assigned = TestNew(0x2c, "I%d", k++);
        std::memset(assigned, 0x5a, 0x2c);
        a.insAssign(assigned, nullptr, ins);
        DumpIns("assigned", assigned);
        TestDelete(assigned);
        TestDelete(shallow);
        // register copies
        if (regs && regs[0]) {
            void* rc = TestNew(0x14, "R%d", k++);
            a.regCopy(rc, nullptr, regs[0]);
            Log("  regcopy");
            DumpReg(rc);
            Log("\n");
            void* ra = TestNew(0x14, "R%d", k++);
            a.regAssign(ra, nullptr, rc);
            Log("  regassign");
            DumpReg(ra);
            Log("\n");
            TestDelete(ra);
            a.regDtor(rc, nullptr);
            TestDelete(rc);
            void* m1 = TestNew(8, "M%d", k++);
            void* m2 = TestNew(8, "M%d", k++);
            a.modCtor(m1, nullptr, 5);
            a.modCtor(m2, nullptr, 2);
            reinterpret_cast<void**>(m2)[1] = m1;
            a.modAssign(m1, nullptr, m2);
            Log("  modassign %d %s\n", static_cast<int*>(m1)[0], Name(reinterpret_cast<void**>(m1)[1]).c_str());
            TestDelete(m1);
            TestDelete(m2);
        }
        // destruction: the deep copy through its deleting destructor, the original in place
        if (P(0, 1)) {
            using Del = void*(__fastcall*)(void*, void*, unsigned);
            reinterpret_cast<Del>((*static_cast<void***>(deep))[0])(deep, nullptr, 1);
        } else {
            a.insDtor(deep, nullptr);
            DumpIns("dead", deep);
            TestDelete(deep);
        }
        a.insDtor(ins, nullptr);
        DumpIns("dead", ins);
        TestDelete(ins);
    }
}

// ---------------------------------------------------------------------------------------------------------------

static void Compare(const char* part, int round)
{
    ++checks;
    if (logText[0] == logText[1]) return;
    if (++bad > 8) return;
    const std::string &x = logText[0], &y = logText[1];
    size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) ++i;
    size_t from = x.rfind("op ", i);
    if (from == std::string::npos || i - from > 2000) from = i > 400 ? i - 400 : 0;
    std::printf("MISMATCH %s round %d\n-- ours:\n%s\n-- original:\n%s\n", part, round,
                x.substr(from, i - from + 400).c_str(), y.substr(from, i - from + 400).c_str());
}

static void WriteSources()
{
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    tempDir = tmp;
    // files for half the names (the rest fail to open)
    const char* bodies[] = {"lightDir fog x\n" TAIL, TAIL, "vp vp\r\nc0" TAIL, "aaaa" TAIL,
                            "x fog lightDir vp aa q c0 xx" TAIL};
    for (int i = 0; i < kNames; i += 2) {
        std::string path = tempDir + "rs_u2b_" + kTexts[i] + ".src";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) continue;
        std::fputs(bodies[i % 5], f);
        std::fclose(f);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_rs_u2b <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 400;
    SetDllDirectoryA(argv[1]);
    std::string dir = argv[1];
    mod[0] = LoadLibraryA((dir + "\\RenderSystem_7c.dll").c_str());
    mod[1] = GetModuleHandleA("RenderSystem_7c_orig.dll");
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!mod[0] || !mod[1] || !ks) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RealNew = static_cast<NewFn>(Proc(ks, "?New@Alloc@Jet@@SAPAXI@Z"));
    RealDelete = static_cast<DeleteFn>(Proc(ks, "?Delete@Alloc@Jet@@SAXPAX@Z"));
    CreateNode = As<decltype(CreateNode)>(Proc(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    DestroyNode = As<decltype(DestroyNode)>(Proc(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    cache = static_cast<void**>(Proc(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    AnsiFromText = As<decltype(AnsiFromText)>(Proc(ks, "??0AnsiString@Jet@@QAE@PBD@Z"));
    AnsiDtor = As<decltype(AnsiDtor)>(Proc(ks, "??1AnsiString@Jet@@QAE@XZ"));
    AnsiText = As<decltype(AnsiText)>(Proc(ks, "??BAnsiString@Jet@@QBAPBDXZ"));
    ReleaseRef = As<decltype(ReleaseRef)>(Proc(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ"));
    AddRef = As<decltype(AddRef)>(Proc(ks, "?AddReference@ReferenceCount@Jet@@QBEXXZ"));
    for (int i = 0; i < kNames; ++i)
        nodes[i] = *kTexts[i] ? CreateNode(*cache, reinterpret_cast<const unsigned char*>(kTexts[i])) : nullptr;
    WriteSources();
    for (int s = 0; s < 2; ++s) {
        LoadApi(s);
        PatchImport(mod[s], "?New@Alloc@Jet@@SAPAXI@Z", reinterpret_cast<void*>(&HookNew));
        PatchImport(mod[s], "?Delete@Alloc@Jet@@SAXPAX@Z", reinterpret_cast<void*>(&HookDelete));
    }
    struct {
        const char* name;
        void (*run)(int, unsigned);
        int scale;
    } parts[] = {{"languages", RunLanguages, 4}, {"records", RunRecords, 2}};
    for (auto& p : parts) {
        for (int round = 0; round < rounds * p.scale / 4 + 1; ++round) {
            const unsigned seed = rng();
            for (int s = 0; s < 2; ++s) {
                cur = s;
                logText[s].clear();
                names[s].clear();
                allocCount[s] = 0;
                p.run(s, seed);
                cur = -1;
            }
            Compare(p.name, round);
        }
        std::printf("-- %s done\n", p.name);
    }
    std::printf("%llu scenarios, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
