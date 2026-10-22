// Differential test of InputSystem_7c unit U8 (ForceEffect, ForceResource, the null driver, the plugin table): our
// module against InputSystem_7c_orig.dll in one process.
//
// - Files: Jet::File is patched in both import tables with an in-memory file system (one per side) that keeps the
//   position at +0x1c as the real one does, and can fail writes after a budget. ForceResource::Save writes random
//   effects (with stand-in children); the files of both sides are compared byte for byte, then loaded back through
//   ForceResource::Load, also after random damage (flipped bytes, truncation). Compared: results, effect lists,
//   removed files.
// - ForceEffect: built through the exported constructor on a stand-in device; stand-in child effects (logging,
//   scripted answers) are attached / detached; every forwarding virtual, Invalidate, CriticalShutdown, and
//   Create(name, db) against a stand-in database and resource (with the derived Create(info) answering as scripted).
//   Compared: the logs, the effect's memory and the children's reference counts.
// - JetGetPluginsInputSystem with PluginType / ResourceTypeBase patched to record the factories; the null driver is
//   created through its factory and exercised (Init, Enumerate, CreateDevice, the device's caps).
//
// usage: diff_is_force.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules> [scenarios]
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
static std::mt19937 rng(2024);
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

static void PatchImport(HMODULE m, const char* name, void* fn)
{
    auto* base = reinterpret_cast<unsigned char*>(m);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    bool found = false;
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
            found = true;
        }
    }
    if (!found) std::printf("note: %s not imported by one module\n", name);
}

// ---- PStrings through KernelStandard
static void** pstringCache;
static void*(__cdecl* cacheCreate)(void*, const unsigned char*);
static void(__cdecl* cacheDestroy)(void*, void*);
static bool(__fastcall* ksRelease)(void*, void*);
static void* NewString(const char* t) { return cacheCreate(*pstringCache, reinterpret_cast<const unsigned char*>(t)); }
static const char* Text(const void* node) { return node ? static_cast<const char*>(node) + 0x18 : "(null)"; }

// ---- the in-memory file system
struct MemFile {
    std::string name;
    size_t pos = 0;
    bool open = false;
};
static std::map<std::string, std::vector<uint8_t>> fs[2];
static std::map<void*, MemFile> files[2];
static long writeBudget = -1;  // bytes until writes fail (-1: never)
// reads until reads fail: a damaged LIST size of 0 sends the original's loader back to the same LIST forever
static long readBudget = -1;
static unsigned long long readBudgetHit;
static void SyncPos(void* f) { At<uint32_t>(f, 0x1c) = static_cast<uint32_t>(files[side][f].pos); }
static void* __fastcall FileCtor(void* f, void*)
{
    std::memset(f, 0, 0x3c);
    At<int>(f, 0) = -1;
    files[side][f] = MemFile();
    return f;
}
static void __fastcall FileDtor(void* f, void*)
{
    if (files[side][f].open) Log("file dtor closes %s", files[side][f].name.c_str());
    files[side].erase(f);
}
static int* __cdecl FileOpen(void* f, int* ret, const char* path, unsigned long mode, int share)
{
    MemFile& o = files[side][f];
    Log("Open %s %lx %d", path ? path : "(null)", mode, share);
    if (!path) {
        *ret = 1;
        return ret;
    }
    if (mode == 0x101) {
        fs[side][path].clear();
    } else if (!fs[side].count(path)) {
        *ret = 3;
        return ret;
    }
    o.name = path;
    o.pos = 0;
    o.open = true;
    SyncPos(f);
    *ret = 0;
    return ret;
}
static unsigned long __cdecl FileRead(void* f, void* data, unsigned long size, unsigned long count)
{
    MemFile& o = files[side][f];
    const std::vector<uint8_t>& d = fs[side][o.name];
    const size_t n = size_t(size) * count;
    if (readBudget >= 0 && readBudget-- == 0) {
        if (side == 0) ++readBudgetHit;
        return 0;
    }
    if (!o.open || o.pos + n > d.size()) return 0;
    std::memcpy(data, d.data() + o.pos, n);
    o.pos += n;
    SyncPos(f);
    return count;
}
static unsigned long WriteBytes(void* f, const void* data, size_t n)
{
    MemFile& o = files[side][f];
    if (!o.open) return 0;
    if (writeBudget >= 0) {
        if (static_cast<long>(n) > writeBudget) return 0;
        writeBudget -= static_cast<long>(n);
    }
    std::vector<uint8_t>& d = fs[side][o.name];
    if (d.size() < o.pos + n) d.resize(o.pos + n);
    std::memcpy(d.data() + o.pos, data, n);
    o.pos += n;
    SyncPos(f);
    return 1;
}
static unsigned long __cdecl FileWrite(void* f, const void* data, unsigned long size, unsigned long count)
{
    return WriteBytes(f, data, size_t(size) * count) ? count : 0;
}
static bool __cdecl FileWriteLong(void* f, unsigned long v) { return WriteBytes(f, &v, 4) != 0; }
static unsigned long __cdecl FileSeekSet(void* f, long off)
{
    MemFile& o = files[side][f];
    o.pos = static_cast<size_t>(off);
    SyncPos(f);
    return static_cast<unsigned long>(off);
}
static unsigned long __cdecl FileSeekCur(void* f, long off)
{
    MemFile& o = files[side][f];
    const long p = static_cast<long>(o.pos) + off;
    if (p < 0 || static_cast<size_t>(p) > fs[side][o.name].size()) return 0xffffffff;
    o.pos = static_cast<size_t>(p);
    SyncPos(f);
    return static_cast<unsigned long>(p);
}
static unsigned long __cdecl FileClose(void* f)
{
    MemFile& o = files[side][f];
    Log("Close %s", o.name.c_str());
    o.open = false;
    return 0;
}
static bool __cdecl FileRemove(const char* path)
{
    Log("Remove %s", path);
    return fs[side].erase(path) != 0;
}
static bool __cdecl FileRemove2(void* const* base, const char* path)
{
    Log("Remove %s / %s", Text(*base), path);
    return fs[side].erase(path) != 0;
}

// ---- the stand-in database: Get (slot 1) returns the stand-in resource, GetFileName (slot 3) adds "dir/"
static void* dbVt[4];
alignas(8) static unsigned char db[2][0x20];
alignas(8) static unsigned char resource[2][0x68];
static bool resourceMissing;
static int lockCode;
static void* resVt[2];
static void* __cdecl DbGet(void* d, void* const* name, int flags)
{
    Log("db Get %s %d", Text(*name), flags);
    return resourceMissing ? nullptr : resource[side];
}
static void** __cdecl DbFileName(void* d, void** ret, void* const* name)
{
    *ret = NewString((std::string("dir/") + Text(*name)).c_str());
    return ret;
}
static void __cdecl DbOther(void*) { Log("db other slot"); }
static int* __cdecl ResLock(void* r, int* ret)
{
    Log("resource Lock -> %d", lockCode);
    *ret = lockCode;
    return ret;
}
static void __cdecl ResUnlock(void* r) { Log("resource Unlock"); }
static bool __cdecl ResRelease(const void* r)
{
    Log("Resource::ReleaseReference");
    return false;
}

// ---- the stand-in device (ReferenceCount at +0xc) and child effects (ForceEffect-shaped, 0x88 bytes)
alignas(8) static unsigned char device[2][0x240];
static void* devVt[18];
static void* devRcVt[2];
constexpr int MAX_CHILD = 40;
alignas(8) static unsigned char children[2][MAX_CHILD][0x88];
static int childCount[2];
static unsigned childAnswers[MAX_CHILD];  // bits: IsValid, IsPlaying, Start, Stop, Update, Load, Unload
static bool createNull;
static std::map<uintptr_t, std::string> names[2];
static std::string Name(const void* p)
{
    if (!p) return "0";
    auto it = names[side].find(reinterpret_cast<uintptr_t>(p));
    return it != names[side].end() ? it->second : "?";
}
static int ChildIndex(const void* c)
{
    for (int i = 0; i < childCount[side]; ++i)
        if (c == children[side][i]) return i;
    return -1;
}
static void __cdecl DeleteThis(void* rc) { Log("DeleteThis %s", Name(rc).c_str()); }
static void __cdecl Other(void* o) { Log("%s other slot", Name(o).c_str()); }
static void* childVt[14];
static void* NewChild()
{
    const int n = childCount[side]++;
    unsigned char* c = children[side][n];
    std::memset(c, 0, 0x88);
    At<void*>(c, 0) = childVt;
    At<int>(c, 4) = 1;
    for (int k = 0; k < 0x74 / 4; ++k) At<uint32_t>(c, 0xc + 4 * k) = 0x1000u * n + k;
    At<uint32_t>(c, 0xc + 0x6c) = 1 + n % 12 == 3 ? 4 : 1 + n % 12;  // a type Save can write
    names[side][reinterpret_cast<uintptr_t>(c)] = "child" + std::to_string(n);
    return c;
}
static bool Answer(void* c, int bit, const char* what)
{
    const int i = ChildIndex(c);
    const bool r = i >= 0 && (childAnswers[i] >> bit & 1);
    Log("%s %s -> %d", Name(c).c_str(), what, r);
    return r;
}
static bool __cdecl ChildIsValid(void* c) { return Answer(c, 0, "IsValid"); }
static void __cdecl ChildInvalidate(void* c) { Log("%s Invalidate", Name(c).c_str()); }
static bool __cdecl ChildIsPlaying(void* c) { return Answer(c, 1, "IsPlaying"); }
static bool __cdecl ChildStart(void* c, unsigned long n, bool solo)
{
    Log("  start args %lu %d", n, solo);
    return Answer(c, 2, "Start");
}
static bool __cdecl ChildStop(void* c) { return Answer(c, 3, "Stop"); }
static bool __cdecl ChildUpdate(void* c) { return Answer(c, 4, "Update"); }
static bool __cdecl ChildLoad(void* c) { return Answer(c, 5, "Load"); }
static bool __cdecl ChildUnload(void* c) { return Answer(c, 6, "Unload"); }
static void* __cdecl DevCreateEffect(void* d, const uint32_t* info, unsigned long flags)
{
    Log("device CreateEffect type %u flags %lx", info[0x6c / 4], flags);
    return createNull ? nullptr : NewChild();
}
// the derived Create(info, flags) of the effect under test (slot 12 of its patched vtable)
static unsigned createAnswers;
static int createCalls[2];
static bool __cdecl DerivedCreate(void* e, const uint32_t* info, unsigned long flags)
{
    const bool r = createAnswers >> (createCalls[side]++ & 31) & 1;
    Log("derived Create type %u flags %lx -> %d", info[0x6c / 4], flags, r);
    return r;
}

// ---- the plugin table: PluginType / ResourceTypeBase constructors record what is registered
struct Registered {
    void* object;
    std::string name;
    uint32_t id[4], parent[4];
};
static std::vector<Registered> registered[2];
static void* __fastcall PluginTypeCtor(void* self, void*, void* const* name, const uint32_t* id, const uint32_t* parent)
{
    std::memset(self, 0, 0x20);
    Registered r{self, Text(*name)};
    std::memcpy(r.id, id, 16);
    std::memcpy(r.parent, parent, 16);
    std::memcpy(static_cast<unsigned char*>(self) + 4, id, 16);
    static unsigned char fakeLibrary[2][0x200];  // PluginType::AddReference counts on its library (+0x18)
    At<void*>(self, 0x18) = fakeLibrary[side];
    registered[side].push_back(r);
    return self;
}

struct Api {
    void*(__fastcall* effectCtor)(void*, void*, void*);
    void(__fastcall* effectDtor)(void*, void*);
    bool(__cdecl* attach)(void*, void*);
    bool(__cdecl* detach)(void*, void*);
    void*(__cdecl* enumerate)(void*);
    void*(__cdecl* getInfo)(void*);
    bool(__cdecl* create)(void*, void* const*, void*, unsigned long);
    int*(__cdecl* load)(void*, int*, const char*);
    bool(__cdecl* save)(void*, const char*, void*);
    void*(__cdecl* effectList)(void*);
    void(__cdecl* plugins)();
    void* effectVt;
    explicit Api(HMODULE m)
    {
        effectCtor = Get<decltype(effectCtor)>(m, "??0ForceEffect@Jet@@QAE@PAVInputDevice@1@@Z");
        effectDtor = Get<decltype(effectDtor)>(m, "??1ForceEffect@Jet@@MAE@XZ");
        attach = Get<decltype(attach)>(m, "?Attach@ForceEffect@Jet@@QAA_NPAV12@@Z");
        detach = Get<decltype(detach)>(m, "?Detach@ForceEffect@Jet@@QAA_NPAV12@@Z");
        enumerate = Get<decltype(enumerate)>(m, "?EnumerateAttachedEffects@ForceEffect@Jet@@QAAABV?$DynamicArray@PAVForceEffect@Jet@@$0N@U?$Less@PAVForceEffect@Jet@@PAV12@@2@@2@XZ");
        getInfo = Get<decltype(getInfo)>(m, "?GetInfo@ForceEffect@Jet@@QAAAAVForceInfo@2@XZ");
        create = Get<decltype(create)>(m, "?Create@ForceEffect@Jet@@UAA_NABVPString@2@PBVResourceDatabase@2@K@Z");
        load = Get<decltype(load)>(m, "?Load@ForceResource@Jet@@MAA?AV?$ResultClass@$1?results@Resource@Jet@@2QBUResultTable@2@BW4Results@Resource@Jet@@@2@PBD@Z");
        save = Get<decltype(save)>(m, "?Save@ForceResource@Jet@@SA_NABVForceEffect@2@PBDPBVResourceDatabase@2@@Z");
        effectList = Get<decltype(effectList)>(m, "?GetEffectList@ForceResource@Jet@@QBAABV?$Array@VForceInfo@Jet@@$00@2@XZ");
        plugins = Get<decltype(plugins)>(m, "JetGetPluginsInputSystem");
        effectVt = Get<void*>(m, "??_7ForceEffect@Jet@@6B@");
    }
};

// the fields Load always sets (and the parameters the type has)
static std::string InfoText(const uint32_t* i)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), "{%x d%d sp%u g%u dir%d,%d tb%d tr%u sd%u env%u %u %u %u %u t%u c%u |", i[0], i[1],
                  i[2], i[3], i[4], i[5], i[6], i[7], i[8], i[9] & 0xff, i[10], i[11], i[12], i[13], i[0x1b], i[0x1c]);
    std::string s = buf;
    const unsigned t = i[0x1b];
    const int words = t == 1 ? 1 : t == 2 ? 2 : (t >= 4 && t <= 8) ? 4 : (t >= 9 && t <= 12) ? 12 : 0;
    for (int k = 0; k < words; ++k) {
        std::snprintf(buf, sizeof(buf), " %x", i[0xf + k]);
        s += buf;
    }
    return s + "}";
}

static void RandomInfo(uint32_t* i)
{
    for (int k = 0; k < 0x1d; ++k) i[k] = rng();
    i[0] = 0;
    i[9] = Pick(2);  // hasEnvelope (+3 bytes padding)
    const unsigned types[] = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    i[0x1b] = Pick(20) ? types[Pick(11)] : Pick(15);
    i[0x1c] = Pick(2);
    i[6] = Pick(3) ? static_cast<uint32_t>(-1) : (Pick(5) ? Pick(32) : Pick(80));  // trigger button
    i[1] = Pick(3) ? static_cast<uint32_t>(-1) : rng() % 5000000;
}

static void Compare(int sc, const char* where)
{
    const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
    for (size_t i = 0; i < n; ++i) {
        ++checks;
        const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
        const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
        if (x != y) {
            if (++bad <= 8) std::printf("scenario %d %s line %zu:\n  ours %s\n  orig %s\n", sc, where, i, x.c_str(), y.c_str());
            break;
        }
    }
    logs[0].clear();
    logs[1].clear();
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_force <dir> [scenarios]\n");
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
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
    pstringCache = Get<void**>(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    cacheCreate = Get<decltype(cacheCreate)>(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    cacheDestroy = Get<decltype(cacheDestroy)>(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    ksRelease = Get<decltype(ksRelease)>(ks, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ");
    const Api api[2] = {Api(mods[0]), Api(mods[1])};
    for (HMODULE m : mods) {
        PatchImport(m, "??0File@Jet@@QAE@XZ", reinterpret_cast<void*>(&FileCtor));
        PatchImport(m, "??1File@Jet@@QAE@XZ", reinterpret_cast<void*>(&FileDtor));
        PatchImport(m, "?Open@File@Jet@@QAA?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@PBDKH@Z", reinterpret_cast<void*>(&FileOpen));
        PatchImport(m, "?Read@File@Jet@@QAAKPAXKK@Z", reinterpret_cast<void*>(&FileRead));
        PatchImport(m, "?Write@File@Jet@@QAAKPBXKK@Z", reinterpret_cast<void*>(&FileWrite));
        PatchImport(m, "?Write@File@Jet@@QAA_NK@Z", reinterpret_cast<void*>(&FileWriteLong));
        PatchImport(m, "?SeekSet@File@Jet@@QAAKJ@Z", reinterpret_cast<void*>(&FileSeekSet));
        PatchImport(m, "?SeekCur@File@Jet@@QAAKJ@Z", reinterpret_cast<void*>(&FileSeekCur));
        PatchImport(m, "?Close@File@Jet@@QAAKXZ", reinterpret_cast<void*>(&FileClose));
        PatchImport(m, "?Remove@File@Jet@@SA_NPBD@Z", reinterpret_cast<void*>(&FileRemove));
        PatchImport(m, "?Remove@File@Jet@@SA_NABVPString@2@PBD@Z", reinterpret_cast<void*>(&FileRemove2));
        PatchImport(m, "?ReleaseReference@Resource@Jet@@QBA_NXZ", reinterpret_cast<void*>(&ResRelease));
        PatchImport(m, "??0PluginType@Jet@@QAE@ABVPString@1@ABVUId@1@1@Z", reinterpret_cast<void*>(&PluginTypeCtor));
        PatchImport(m, "??0ResourceTypeBase@Jet@@QAE@ABVPString@1@ABVUId@1@1@Z", reinterpret_cast<void*>(&PluginTypeCtor));
    }
    dbVt[0] = dbVt[2] = reinterpret_cast<void*>(&DbOther);
    dbVt[1] = reinterpret_cast<void*>(&DbGet);
    dbVt[3] = reinterpret_cast<void*>(&DbFileName);
    resVt[0] = reinterpret_cast<void*>(&ResLock);
    resVt[1] = reinterpret_cast<void*>(&ResUnlock);
    for (void*& p : devVt) p = reinterpret_cast<void*>(&Other);
    devVt[13] = reinterpret_cast<void*>(&DevCreateEffect);
    devRcVt[0] = reinterpret_cast<void*>(&DeleteThis);
    devRcVt[1] = reinterpret_cast<void*>(&Other);
    for (void*& p : childVt) p = reinterpret_cast<void*>(&Other);
    childVt[0] = reinterpret_cast<void*>(&DeleteThis);
    childVt[2] = reinterpret_cast<void*>(&ChildIsValid);
    childVt[3] = reinterpret_cast<void*>(&ChildInvalidate);
    childVt[4] = reinterpret_cast<void*>(&ChildIsPlaying);
    childVt[6] = reinterpret_cast<void*>(&ChildStart);
    childVt[7] = reinterpret_cast<void*>(&ChildStop);
    childVt[8] = reinterpret_cast<void*>(&ChildUpdate);
    childVt[9] = reinterpret_cast<void*>(&ChildLoad);
    childVt[10] = reinterpret_cast<void*>(&ChildUnload);
    void* effectVt[2][14];

    // ---- the plugin table and the null driver
    if (std::getenv("IS_TRACE")) std::printf("plugins\n");
    for (side = 0; side < 2; ++side) {
        api[side].plugins();
        for (const Registered& r : registered[side]) {
            Log("registered %s id %08x parent %08x vt-slot4 %u", r.name.c_str(), r.id[0], r.parent[0],
                r.name == "ffe" ? VCall<unsigned>(r.object, 4) : 0u);
            if (r.name != "None") continue;
            void* const drv = VCall<void*>(r.object, 3);  // CreateInstance
            Log("factory of the driver %s", At<void*>(drv, 8) == r.object ? "set" : "?");
            Log("Init -> %d", VCall<bool>(drv, 0, static_cast<void*>(nullptr)));
            void* list = nullptr;
            VCall<void>(drv, 2, static_cast<void*>(&list), 4);
            VCall<void>(drv, 2, static_cast<void*>(&list), 1);
            const unsigned char* c = static_cast<const unsigned char*>(list);
            Log("enumerated %u: t%d %s %s caps %x %x", At<uint32_t>(list, 8), At<int>(list, 0xc),
                Text(At<void*>(list, 0xc + 0x14)), Text(At<void*>(list, 0xc + 0x18)), At<uint32_t>(list, 0xc + 0x1c),
                At<uint32_t>(list, 0xc + 0x20));
            (void)c;
            void* dev = nullptr;
            uint32_t anyId[4] = {1, 2, 3, 4};
            VCall<void**>(drv, 3, &dev, static_cast<const void*>(anyId));
            Log("device rc %d valid %d id %08x", At<int>(dev, 0x10), At<unsigned char>(dev, 0x234), At<uint32_t>(dev, 0x14));
            unsigned char caps[0x38] = {};
            VCall<void>(dev, 4, static_cast<void*>(caps));
            Log("device caps t%d %s %s %x", At<int>(caps, 0), Text(At<void*>(caps, 0x14)), Text(At<void*>(caps, 0x18)),
                At<uint32_t>(caps, 0x1c));
            Log("device released -> %d", ksRelease(static_cast<unsigned char*>(dev) + 0xc, nullptr));
        }
    }
    Compare(-1, "plugins");

    unsigned long long saved = 0, loadedEffects = 0, skipped = 0;
    for (int sc = 0; sc < scenarios; ++sc) {
        if (std::getenv("IS_TRACE")) std::printf("scenario %d\n", sc);
        // ---- an effect with children
        const int nchildren = static_cast<int>(Pick(5));
        uint32_t mainInfo[0x1d];
        RandomInfo(mainInfo);
        std::vector<std::vector<uint32_t>> childInfos(static_cast<size_t>(nchildren), std::vector<uint32_t>(0x1d));
        for (auto& ci : childInfos) RandomInfo(ci.data());
        for (unsigned& a : childAnswers) a = rng() | (Pick(3) ? 0x7f : 0);
        createAnswers = rng();
        const unsigned steps = 1 + Pick(25);
        alignas(8) unsigned char effect[2][0x88];
        for (side = 0; side < 2; ++side) {
            names[side].clear();
            childCount[side] = 0;
            createCalls[side] = 0;
            unsigned char* dev = device[side];
            std::memset(dev, 0, sizeof(device[side]));
            At<void*>(dev, 0) = devVt;
            At<void*>(dev, 0xc) = devRcVt;
            At<int>(dev, 0x10) = 5;
            names[side][reinterpret_cast<uintptr_t>(dev)] = "device";
            names[side][reinterpret_cast<uintptr_t>(dev + 0xc)] = "device.rc";
            std::memset(effect[side], 0xab, 0x88);
            api[side].effectCtor(effect[side], nullptr, dev);
            std::memcpy(effectVt[side], At<void**>(effect[side], 0), sizeof(effectVt[side]));
            effectVt[side][12] = reinterpret_cast<void*>(&DerivedCreate);
            At<void**>(effect[side], 0) = effectVt[side];
            names[side][reinterpret_cast<uintptr_t>(effect[side])] = "effect";
            std::memcpy(static_cast<unsigned char*>(api[side].getInfo(effect[side])), mainInfo, 0x74);
            for (int i = 0; i < nchildren; ++i) {
                void* c = NewChild();
                std::memcpy(static_cast<unsigned char*>(c) + 0xc, childInfos[static_cast<size_t>(i)].data(), 0x74);
                Log("Attach child%d -> %d", i, api[side].attach(effect[side], c));
            }
        }
        Compare(sc, "build");
        for (unsigned st = 0; st < steps; ++st) {
            const unsigned op = Pick(16);
            const unsigned a = rng(), b = rng();
            lockCode = Pick(5) ? 0 : 3;
            resourceMissing = Pick(8) == 0;
            createNull = Pick(6) == 0;
            // the resource's effect list
            const unsigned nres = Pick(5);
            std::vector<uint32_t> resInfos(nres * 0x1d);
            for (unsigned i = 0; i < nres; ++i) RandomInfo(&resInfos[i * 0x1d]);
            for (side = 0; side < 2; ++side) {
                unsigned char* const e = effect[side];
                const Api& f = api[side];
                if (std::getenv("IS_TRACE")) std::printf("  step %u op %u side %d\n", st, op, side);
                switch (op) {
                case 0: Log("IsValid -> %d", VCall<bool>(e, 2)); break;
                case 1: VCall<void>(e, 3); Log("Invalidate"); break;
                case 2: Log("IsPlaying -> %d", VCall<bool>(e, 4)); break;
                case 3: Log("IsLoaded -> %d", VCall<bool>(e, 5)); break;
                case 4: Log("Start -> %d", VCall<bool>(e, 6, static_cast<unsigned long>(a % 5), (b & 1) != 0)); break;
                case 5: Log("Stop -> %d", VCall<bool>(e, 7)); break;
                case 6: Log("Update -> %d", VCall<bool>(e, 8)); break;
                case 7: Log("Load -> %d", VCall<bool>(e, 9)); break;
                case 8: Log("Unload -> %d", VCall<bool>(e, 10)); break;
                case 9: VCall<void>(e, 13); Log("CriticalShutdown"); break;
                case 10: {  // attach a new or an existing child
                    void* c = childCount[side] && a % 3 == 0 ? children[side][b % childCount[side]]
                                                              : (childCount[side] < MAX_CHILD - 8 ? NewChild() : nullptr);
                    Log("Attach %s -> %d", Name(c).c_str(), f.attach(e, c));
                    break;
                }
                case 11: {
                    void* c = childCount[side] && a % 5 ? children[side][b % childCount[side]] : nullptr;
                    Log("Detach %s -> %d", Name(c).c_str(), f.detach(e, c));
                    break;
                }
                case 12:
                case 13: {  // Create from the stand-in resource
                    unsigned char* r = resource[side];
                    std::memset(r, 0, sizeof(resource[side]));
                    At<void*>(r, 0) = resVt;
                    At<uint32_t>(r, 0x5c) = nres;
                    At<uint32_t>(r, 0x60) = nres;
                    At<void*>(r, 0x64) = resInfos.data();
                    std::memset(db[side], 0, sizeof(db[side]));
                    At<void*>(db[side], 0) = dbVt;
                    void* name = NewString("effect.ffe");
                    // (without a device the original calls CreateEffect through null for the second effect)
                    if (childCount[side] >= MAX_CHILD - 8 || !At<void*>(e, 8)) break;
                    Log("Create -> %d", f.create(e, &name, a % 9 ? db[side] : nullptr, b));
                    cacheDestroy(*pstringCache, name);
                    break;
                }
                default: {  // Save the effect, then load the file back
                    std::memset(db[side], 0, sizeof(db[side]));
                    At<void*>(db[side], 0) = dbVt;
                    At<void*>(db[side], 0xc) = NewString("base");
                    writeBudget = a % 4 ? -1 : static_cast<long>(b % 900);
                    const bool viaDb = a % 3 != 0;
                    const char* fname = b % 13 ? "out.ffe" : nullptr;
                    const bool ok = f.save(e, fname, viaDb ? db[side] : nullptr);
                    writeBudget = -1;
                    Log("Save -> %d", ok);
                    if (side == 0 && ok) ++saved;
                    break;
                }
                }
                const auto* list = static_cast<const uint32_t*>(f.enumerate(e));
                std::string s;
                if (const auto* head = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(list[0]))) {
                    const uint32_t* c = head;
                    do {
                        s += " [";
                        for (uint32_t i = 0; i < c[2]; ++i) s += " " + Name(reinterpret_cast<void*>(static_cast<uintptr_t>(c[3 + i])));
                        s += "]";
                        c = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(c[0]));
                    } while (c != head);
                }
                char buf[96];
                std::snprintf(buf, sizeof(buf), " | rc %d dev %s valid %d devrc %d |", At<int>(e, 4), Name(At<void*>(e, 8)).c_str(),
                              e[0x84], At<int>(device[side], 0x10));
                s += buf;
                for (int i = 0; i < childCount[side]; ++i) s += " " + std::to_string(At<int>(children[side][i], 4));
                Log("effect%s %s", s.c_str(), InfoText(static_cast<const uint32_t*>(f.getInfo(e))).c_str());
            }
            Compare(sc, "step");
            // a saved file: the two sides' bytes must match; then both load it, also damaged
            const bool have0 = fs[0].count("dir/out.ffe") || fs[0].count("out.ffe");
            const bool have1 = fs[1].count("dir/out.ffe") || fs[1].count("out.ffe");
            ++checks;
            if (have0 != have1 || (have0 && fs[0] != fs[1])) {
                if (++bad <= 8) {
                    std::printf("scenario %d step %u: the saved files differ (%d %d)\n", sc, st, have0, have1);
                    for (const auto& kv : fs[0]) {
                        const std::vector<uint8_t>& x = kv.second;
                        const std::vector<uint8_t>& y = fs[1][kv.first];
                        size_t i = 0;
                        while (i < x.size() && i < y.size() && x[i] == y[i]) ++i;
                        std::printf("  %s sizes %zu %zu, first difference at %zu: %02x %02x\n", kv.first.c_str(), x.size(),
                                    y.size(), i, i < x.size() ? x[i] : 0, i < y.size() ? y[i] : 0);
                    }
                }
            }
            if (!have0 || !have1) continue;
            const std::string path = fs[0].count("dir/out.ffe") ? "dir/out.ffe" : "out.ffe";
            std::vector<uint8_t> data = fs[0][path];
            if (Pick(3) == 0 && !data.empty()) {  // damage
                const unsigned n = 1 + Pick(4);
                for (unsigned k = 0; k < n; ++k) data[Pick(static_cast<unsigned>(data.size()))] ^= static_cast<uint8_t>(1u << Pick(8));
                if (Pick(3) == 0) data.resize(Pick(static_cast<unsigned>(data.size())));
            }
            for (side = 0; side < 2; ++side) {
                fs[side].clear();
                fs[side]["dir/in.ffe"] = data;
                alignas(8) unsigned char res[0x68] = {};
                At<void*>(res, 8) = db[side];
                int result = -1;
                readBudget = 20000;
                api[side].load(res, &result, "in.ffe");
                readBudget = -1;
                const auto* list = static_cast<const uint32_t*>(api[side].effectList(res));
                Log("Load -> %d count %u", result, list[0]);
                for (uint32_t i = 0; i < list[0]; ++i) {
                    const uint32_t* inf = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(list[2])) + 0x1d * i;
                    Log("  %s", InfoText(inf).c_str());
                }
                if (side == 0) loadedEffects += list[0];
                fs[side].clear();
                cacheDestroy(*pstringCache, At<void*>(db[side], 0xc));
                At<void*>(db[side], 0xc) = nullptr;
            }
            Compare(sc, "load");
        }
        for (side = 0; side < 2; ++side) {
            api[side].effectDtor(effect[side], nullptr);
            std::string s = "destroyed: device rc " + std::to_string(At<int>(device[side], 0x10)) + " children";
            for (int i = 0; i < childCount[side]; ++i) s += " " + std::to_string(At<int>(children[side][i], 4));
            Log("%s", s.c_str());
            fs[side].clear();
        }
        Compare(sc, "end");
    }
    (void)skipped;
    std::printf("saved %llu files, loaded %llu effects, loader loops cut %llu\n", saved, loadedEffects, readBudgetHit);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
