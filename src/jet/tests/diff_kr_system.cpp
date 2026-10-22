// Differential test of KernelResource_7c (Resource, ResourceDatabase, ResourceInstance, the bins, ResourceSystem):
// our module against KernelResource_7c_orig.dll in one process, each with its own ResourceSystem.
//
// Deterministic: Options (patched) turn "ResourceCaching" off in most scenarios; in the others the loader thread
// exists but is paused from the start, so queued resources wait until this thread claims them. Both modules'
// imports are patched: File::Exists answers from a random file set, File::Subst logs, Time::GetSeconds counts,
// Options::Get gives the scenario's settings, Kernel_7c's Task constructor / destructor do nothing, LCRand's
// constructor takes the scenario's seed (the bin's priorities; the real one reads the clock), NextFloat logs, and
// LookupPluginType hands out stand-in resource types. Those make resources through the module's own Resource
// constructor and give them a vftable whose Load / LoadTxt / LoadComplete / Unload / sizes / SetPriority log and
// answer per name.
//
// Random steps on both sides: databases made (plain and nameless), parents, variants, Get / Exists / GetFileName /
// Remove by name ("a;b" lists too), held references locked / unlocked / released, ResourceInstances (Init, Lock,
// Unlock, Reset, copy, assign), observers, priorities, Cache, LoadAll / IsAllLoaded, Update (the bin, the size
// check), bursts of loaded resources nobody holds (the size limit's LRU pass), DumpUnusedResourcesFromCache,
// databases deleted. Each scenario starts past the last one's clock (the next size check is a static). Compared after each step: the logs, the results, every
// database's table (bucket order, every resource's fields), the system's counters, the bin and the statics.
//
// usage: diff_kr_system.exe <dir with KernelResource_7c.dll, KernelResource_7c_orig.dll and the Jet modules> [scenarios]
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

static std::mt19937 rng(77);
static unsigned Pick(unsigned n) { return n ? rng() % n : 0; }
static int side;
static bool trace;
static std::vector<std::string> logs[2];
static unsigned long long checks, bad;
static std::map<std::string, unsigned long long> coverage;

static void Log(const char* fmt, ...)
{
    char buf[600];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    logs[side].push_back(buf);
    if (trace) std::fprintf(stderr, "  L%d %.300s\n", side, buf), std::fflush(stderr);
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

// ---- PString nodes (text at +0x18)
static void** cache;
static void*(__cdecl* createNode)(void*, const unsigned char*);
static void(__cdecl* destroyNode)(void*, void*);
static void*(__cdecl* allocNew)(unsigned);
static void(__cdecl* allocDelete)(void*);
static std::map<std::string, void*> interned;
static void* Node(const std::string& text)
{
    if (text.empty()) return nullptr;
    auto it = interned.find(text);
    if (it != interned.end()) return it->second;
    return interned[text] = createNode(*cache, reinterpret_cast<const unsigned char*>(text.c_str()));
}
static std::string Text(const void* node) { return node ? static_cast<const char*>(node) + 0x18 : "(null)"; }
static unsigned Hash(const std::string& s)
{
    unsigned h = 2166136261u;
    for (char c : s) h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
    return h;
}

// ---- the scenario's world
static std::set<std::string> files;  // "<drive>:<path>" that exist
static bool instantBin;
// with caching on, the loader thread exists but is paused from the start (PauseProcessing holds its work mutex), so
// queued resources stay queued until this thread claims them: the queue, Claim and Unqueue run deterministically
static bool caching;
static float timeFloor;
static double secondsNow;
static unsigned binMaxDeleted;
static float binProcessTime;

static int* __cdecl FakeExists(int* result, const char* path)
{
    const bool there = path && files.count(path);
    Log("Exists %s -> %d", path ? path : "(null)", there);
    ++coverage[there ? "exists yes" : "exists no"];
    *result = there ? 0 : 2;
    return result;
}
static void __cdecl FakeSubst(void* const* drive, const char* path, bool add) { Log("Subst %s %s %d", Text(*drive).c_str(), path ? path : "(null)", add); }
static double __cdecl FakeSeconds() { return secondsNow += 0.25; }
static bool __cdecl FakeGetBool(const void*, const char* key, bool* value, bool fallback)
{
    Log("Options %s", key);
    *value = std::strcmp(key, "ResourceCaching") ? fallback : caching;
    return true;
}
static bool __cdecl FakeGetPlain(const void*, const char* key)
{
    Log("Options %s", key);
    return std::strcmp(key, "ResourceInstantBin") == 0 && instantBin;
}
static bool __cdecl FakeGetFloat(const void*, const char* key, float* value, float fallback)
{
    Log("Options %s", key);
    *value = std::strcmp(key, "ResourceBinProcessTime") == 0 ? binProcessTime : fallback;
    return true;
}
static bool __cdecl FakeGetUint(const void*, const char* key, unsigned* value, unsigned fallback)
{
    Log("Options %s", key);
    *value = std::strcmp(key, "ResourceBinMaxDeleted") == 0 ? binMaxDeleted : fallback;
    return true;
}
static void* __fastcall FakeTaskCtor(unsigned char* self, void*, const void*, float, bool)
{
    std::memset(self, 0, 0x28);
    return self;
}
static void __fastcall FakeTaskDtor(void*, void*) {}

// ---- stand-in resource types and resources
struct Api;
static const Api* apis[2];
struct FakeType {
    void** vtable;
    int refs;
    std::string ext;
};
static std::map<std::string, FakeType*> types[2];
static void** resourceVt[2];  // per side: the module's Resource vftable with our slots

static std::string ResName(const unsigned char* r) { return Text(*reinterpret_cast<void* const*>(r + 4)); }

static void __cdecl TypeAddRef(FakeType* t) { ++t->refs; }
static void __cdecl TypeRelease(FakeType* t) { --t->refs; }
static void* __fastcall TypeDtor(FakeType* t, void*, unsigned) { return t; }
static void* MakeResource(FakeType* t, void* const* name, const void* owner);
static void* __cdecl TypeCreate(FakeType* t, void* const* name, const void* owner)
{
    Log("type %s Create %s", t->ext.c_str(), Text(*name).c_str());
    return MakeResource(t, name, owner);
}
static unsigned __cdecl TypeSize(FakeType*) { return 0x60; }
static void* __cdecl TypeOnTheFly(FakeType* t, void* const* name, const void* owner)
{
    const bool make = t->ext == "fly" && Hash(Text(*name)) % 3 != 0;
    Log("type %s OnTheFly %s -> %d", t->ext.c_str(), Text(*name).c_str(), make);
    return make ? MakeResource(t, name, owner) : nullptr;
}
static void* typeVt[6] = {reinterpret_cast<void*>(&TypeAddRef), reinterpret_cast<void*>(&TypeRelease),
                          reinterpret_cast<void*>(&TypeDtor), reinterpret_cast<void*>(&TypeCreate),
                          reinterpret_cast<void*>(&TypeSize), reinterpret_cast<void*>(&TypeOnTheFly)};

static int* __cdecl FakeLookup(int* result, void** out, const void*, void* const* name)
{
    auto& map = types[side];
    auto it = map.find(Text(*name));
    Log("Lookup %s -> %d", Text(*name).c_str(), it != map.end());
    if (it == map.end()) {
        *result = 1;
        return result;
    }
    ++it->second->refs;
    *out = it->second;
    *result = 0;
    return result;
}

// the resource's slots
static int loadsLeft;  // (unused)
static int* __cdecl ResLoad(unsigned char* r, int* result, const char* file)
{
    const std::string n = ResName(r);
    const int code = Hash(n + "load") % 7 == 0 ? 1 : 0;
    Log("Load %s file %s -> %d", n.c_str(), file ? file : "(null)", code);
    ++coverage[code ? "load fails" : "load"];
    *result = code;
    return result;
}
static int* __cdecl ResLoadTxt(unsigned char* r, int* result, const char* file)
{
    const std::string n = ResName(r);
    const int code = Hash(n + "txt") % 9 == 0 ? 2 : 0;
    Log("LoadTxt %s file %s -> %d", n.c_str(), file ? file : "(null)", code);
    ++coverage["loadtxt"];
    *result = code;
    return result;
}
static int* __cdecl ResLoadComplete(unsigned char* r, int* result)
{
    const std::string n = ResName(r);
    const int code = Hash(n + "complete") % 11 == 0 ? 3 : 0;
    Log("LoadComplete %s -> %d", n.c_str(), code);
    *result = code;
    return result;
}
static bool __cdecl ResUnload(unsigned char* r)
{
    Log("Unload %s", ResName(r).c_str());
    return false;
}
static unsigned __cdecl ResSize(const unsigned char* r)
{
    const unsigned h = Hash(ResName(r) + "size");
    return h % 13 == 0 ? ~0u : 2000000 + h % 18000000;
}
static unsigned __cdecl ResDataSize(const unsigned char* r)
{
    const unsigned h = Hash(ResName(r) + "data");
    return h % 5 == 0 ? ~0u : h % 3000000;
}
// (not with the loader in use: a queued resource reset to Ready on release gets queued twice, see ClaimQueued)
// the bins set a resource's priority as they take it: logged, so the order of a batch shows
// the bin's random draws (LCRand::NextFloat, __thiscall), logged with the generator's state
static float(__fastcall* nextFloat)(void*, void*, float);
static float __fastcall FakeNextFloat(void* self, void*, float range)
{
    const unsigned before = *static_cast<const unsigned*>(self);
    const float f = nextFloat(self, nullptr, range);
    Log("rand %08x %g -> %g", before, range, f);
    return f;
}
// the bin's generator is seeded from the clock: the same seed for both sides instead
static unsigned randSeed;
static void* __fastcall FakeLCRandCtor(unsigned* self)
{
    *self = randSeed;
    return self;
}
static void* setPriority[2];  // the module's own
static void __cdecl ResSetPriority(const unsigned char* r, float p)
{
    Log("priority %s %g", ResName(r).c_str(), p);
    reinterpret_cast<void(__cdecl*)(const void*, float)>(setPriority[side])(r, p);
}
static bool __cdecl ResReset(const unsigned char* r) { return !caching && Hash(ResName(r) + "reset") % 6 == 0; }

struct Api {
    HMODULE module;
    void*(__fastcall* resourceCtor)(void*, void*, void* const*, const void*);
    void*(__fastcall* systemCtor)(void*, void*);
    void(__fastcall* systemDtor)(void*, void*);
    void(__cdecl* init)(void*, const void*);
    void(__cdecl* update)(void*, float);
    void(__fastcall* systemDump)(void*, void*);
    void*(__fastcall* dbCtor)(void*, void*, void* const*, void* const*);
    void*(__fastcall* namelessCtor)(void*, void*, void* const*, void* const*);
    void(__cdecl* addParentName)(void*, void* const*);
    void(__cdecl* addParent)(void*, void*);
    void(__cdecl* addVariant)(void*, void* const*);
    void(__cdecl* removeName)(const void*, void* const*);
    void(__cdecl* removeParent)(void*, void*);
    void(__fastcall* dbDump)(void*, void*);
    bool(__cdecl* release)(const void*);
    void(__cdecl* addRef)(const void*);
    void(__cdecl* touch)(const void*);
    unsigned(__cdecl* getSize)(const void*);
    void*(__fastcall* instCtor)(void*, void*);
    void*(__fastcall* instCopy)(void*, void*, const void*);
    void(__fastcall* instDtor)(void*, void*);
    void*(__cdecl* instAssign)(void*, const void*);
    void(__cdecl* instInit)(void*, const void*, void* const*);
    void(__cdecl* instInit3)(void*, const void*, void* const*, bool);
    void(__cdecl* instReset)(void*);
    int*(__cdecl* instLock)(const void*, int*);
    void(__cdecl* instUnlock)(const void*);
    bool(__cdecl* instAvailable)(const void*);
    void(__cdecl* pause)(void*);
    void(__cdecl* resume)(void*);
    void(__cdecl* cache)(const void*, bool);
    void* resourceVftable;
    unsigned* totals;  // totalResources .. totalClashes
};

static void* MakeResource(FakeType* t, void* const* name, const void* owner)
{
    unsigned char* const r = static_cast<unsigned char*>(allocNew(0x60));
    apis[side]->resourceCtor(r, nullptr, name, owner);
    *reinterpret_cast<void***>(r) = resourceVt[side];
    (void)t;
    return r;
}

// ---- observers
struct FakeObserver {
    void** vtable;
    int tag;
};
static void __cdecl ObsLoaded(FakeObserver* o, const unsigned char* r) { Log("obs%d loaded %s", o->tag, ResName(r).c_str()); }
static void* obsVt[1] = {reinterpret_cast<void*>(&ObsLoaded)};
static FakeObserver observers[3] = {{obsVt, 0}, {obsVt, 1}, {obsVt, 2}};

template <class R, class... A>
static R VCall(const void* obj, int slot, A... a)
{
    return reinterpret_cast<R(__cdecl*)(const void*, A...)>((*static_cast<void* const* const*>(obj))[slot])(obj, a...);
}

// ---- dumps
struct DbInfo {
    unsigned char* db[2];
    std::string name;
};
static std::vector<DbInfo> dbs;
// the parent links (by name): a cycle would make the lookups recurse without end, in both modules
static std::map<std::string, std::set<std::string>> parentsOf;
static bool Reaches(const std::string& from, const std::string& to)
{
    if (from == to) return true;
    for (const std::string& p : parentsOf[from])
        if (Reaches(p, to)) return true;
    return false;
}

static std::string DbName(const void* db)
{
    if (!db) return "-";
    for (const auto& d : dbs)
        if (d.db[side] == db) return d.name;
    return "?";
}

static std::string DumpResource(const unsigned char* r)
{
    char buf[400];
    auto I = [&](int off) { return *reinterpret_cast<const int*>(r + off); };
    auto F = [&](int off) { return *reinterpret_cast<const float*>(r + off); };
    const void* const vt = *reinterpret_cast<void* const*>(r);
    const char* kind = vt == resourceVt[side] ? "R" : vt == apis[side]->resourceVftable ? "r" : "D";
    std::snprintf(buf, sizeof(buf), "{%s %s load %s st%d refs%d in%d hits%d miss%d locks%u fl%x pr%g hp%g use%g ct%g sz%x/%x own %s orig %s",
                  kind, ResName(r).c_str(), Text(*reinterpret_cast<void* const*>(r + 0x38)).c_str(), I(0x28), I(0x1c), I(0x24),
                  I(0x40), I(0x44), static_cast<unsigned>(I(0x48)), r[0x58] & 0xf, F(0x14), F(0x18), F(0x20), F(0x10),
                  I(0x50), I(0x54), DbName(*reinterpret_cast<void* const*>(r + 8)).c_str(),
                  DbName(*reinterpret_cast<void* const*>(r + 0xc)).c_str());
    std::string s = buf;
    if (side == 0) ++coverage["status " + std::to_string(I(0x28)) + (kind[0] == 'D' ? " redir" : "")];
    if (const auto* type = *reinterpret_cast<FakeType* const*>(r + 0x34)) s += " t:" + type->ext;
    if (kind[0] == 'D') {
        const unsigned char* target = *reinterpret_cast<unsigned char* const*>(r + 0x5c);
        if (trace) std::fprintf(stderr, "    redir %s target %p\n", s.c_str(), static_cast<const void*>(target)), std::fflush(stderr);
        s += " -> " + (target ? ResName(target) + "@" + DbName(*reinterpret_cast<void* const*>(target + 8)) : std::string("null"));
    }
    // observers and redirections: FDA<ptr,13> {next, prev, count, data}
    for (int off : {0x2c, 0x4c}) {
        const auto* head = *reinterpret_cast<const unsigned char* const*>(r + off);
        if (!head) continue;
        s += off == 0x2c ? " obs" : " redirs";
        const unsigned char* c = head;
        do {
            const unsigned cnt = *reinterpret_cast<const unsigned*>(c + 8);
            for (unsigned i = 0; i < cnt; ++i) {
                const void* p = *reinterpret_cast<void* const*>(c + 0xc + 4 * i);
                if (off == 0x2c)
                    s += " " + std::to_string(static_cast<const FakeObserver*>(p)->tag);
                else
                    s += " " + ResName(static_cast<const unsigned char*>(p)) + "@" + DbName(*reinterpret_cast<void* const*>(static_cast<const unsigned char*>(p) + 8));
            }
            c = *reinterpret_cast<const unsigned char* const*>(c);
        } while (c != head);
    }
    return s + "}";
}

// The resources of the database's table waiting in the (paused) loader's queue (status Cached) are claimed: locked
// (which takes them out of the queue and loads them here) and unlocked. A queued resource must not outlive its
// database or lose the table's reference while queued: the loader's later claim or exit deletes it under the table
// or after its system, in both modules. Nor may one be queued twice (Cache forced, or reset to Ready on release,
// while queued): the claim takes out one entry, and the database's destructor deletes the resource under the
// other, which the loader's exit then deletes again.
static void ClaimQueued(const unsigned char* db)
{
    std::vector<const unsigned char*> queued;
    const auto* buckets = *reinterpret_cast<const unsigned char* const* const*>(db + 0x10);
    for (unsigned b = 0; b < *reinterpret_cast<const unsigned*>(db + 0x18); ++b) {
        const unsigned char* const head = buckets[b];
        if (!head) continue;
        const unsigned char* c = head;
        do {
            for (unsigned i = 0; i < *reinterpret_cast<const unsigned*>(c + 8); ++i) {
                const unsigned char* r = *reinterpret_cast<const unsigned char* const*>(c + 0xc + 4 * i);
                if (*reinterpret_cast<const int*>(r + 0x28) == 2) queued.push_back(r);
            }
            c = *reinterpret_cast<const unsigned char* const*>(c);
        } while (c != head);
    }
    for (const unsigned char* r : queued) {
        int res = -1;
        VCall<int*>(r, 0, &res);
        if (res == 0) VCall<void>(r, 1);
        Log("claimed %s -> %d", ResName(r).c_str(), res);
    }
}

static std::string DumpDb(const unsigned char* db)
{
    std::string s = "db " + DbName(db) + " name " + Text(*reinterpret_cast<void* const*>(db + 0xc));
    if (trace) std::fprintf(stderr, "   table of %s: count %u\n", s.c_str(), *reinterpret_cast<const unsigned*>(db + 0x14)), std::fflush(stderr);
    char buf[96];
    std::snprintf(buf, sizeof(buf), " n%u look%d req%d inv%d fl%x", *reinterpret_cast<const unsigned*>(db + 0x14),
                  *reinterpret_cast<const int*>(db + 0x2c), *reinterpret_cast<const int*>(db + 0x30),
                  *reinterpret_cast<const int*>(db + 0x34), db[0x38] & 1);
    s += buf;
    // parents FDA<db*,5>, variants list
    if (const auto* head = *reinterpret_cast<const unsigned char* const*>(db + 0x1c)) {
        s += " parents";
        const unsigned char* c = head;
        do {
            for (unsigned i = 0; i < *reinterpret_cast<const unsigned*>(c + 8); ++i) s += " " + DbName(*reinterpret_cast<void* const*>(c + 0xc + 4 * i));
            c = *reinterpret_cast<const unsigned char* const*>(c);
        } while (c != head);
    }
    s += " variants";
    for (const unsigned char* v = *reinterpret_cast<const unsigned char* const*>(db + 0x20); v != db + 0x20;
         v = *reinterpret_cast<const unsigned char* const*>(v))
        s += " " + Text(*reinterpret_cast<void* const*>(v + 8));
    const auto* buckets = *reinterpret_cast<const unsigned char* const* const*>(db + 0x10);
    for (unsigned b = 0; b < *reinterpret_cast<const unsigned*>(db + 0x18); ++b) {
        const unsigned char* head = buckets[b];
        if (!head) continue;
        s += " b" + std::to_string(b) + ":";
        const unsigned char* c = head;
        do {
            for (unsigned i = 0; i < *reinterpret_cast<const unsigned*>(c + 8); ++i)
            {
                const unsigned char* r = *reinterpret_cast<const unsigned char* const*>(c + 0xc + 4 * i);
                if (trace)
                    std::fprintf(stderr, "   dump %p bucket %u node %p %s\n", static_cast<const void*>(r), b,
                                 *reinterpret_cast<void* const*>(r + 4), DumpResource(r).substr(0, 200).c_str()),
                        std::fflush(stderr);
                s += " " + DumpResource(r);
            }
            c = *reinterpret_cast<const unsigned char* const*>(c);
        } while (c != head);
    }
    return s;
}

static std::string DumpSystem(const unsigned char* rs)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), "rs fl%x time%g res%u loadedRes%u binned%u size%u data%u bin%u max%u paused%d dump%d",
                  rs[0x10] & 1, *reinterpret_cast<const float*>(rs + 0xdc), *reinterpret_cast<const unsigned*>(rs + 0xe0),
                  *reinterpret_cast<const unsigned*>(rs + 0xe4), *reinterpret_cast<const unsigned*>(rs + 0xe8),
                  *reinterpret_cast<const unsigned*>(rs + 0xec), *reinterpret_cast<const unsigned*>(rs + 0xf0),
                  *reinterpret_cast<const unsigned*>(rs + 0xf4), *reinterpret_cast<const unsigned*>(rs + 0xf8),
                  *reinterpret_cast<const int*>(rs + 0xfc), rs[0x100]);
    std::string s = buf;
    if (*reinterpret_cast<void* const*>(rs + 0x18)) s += " LOADED-QUEUE";
    const unsigned char* bin = *reinterpret_cast<const unsigned char* const*>(rs + 8);
    if (bin && !instantBin) {
        std::snprintf(buf, sizeof(buf), " bin n%u t%g last%g", *reinterpret_cast<const unsigned*>(bin + 0x10),
                      *reinterpret_cast<const float*>(bin + 0x1c), *reinterpret_cast<const float*>(bin + 0x20));
        s += buf;
        const auto* data = *reinterpret_cast<const unsigned char* const* const*>(bin + 0x18);
        for (unsigned i = 0; i < *reinterpret_cast<const unsigned*>(bin + 0x10); ++i) s += " " + ResName(data[i]);
        if (side == 0 && *reinterpret_cast<const unsigned*>(bin + 0x10)) ++coverage["bin holds some"];
    }
    const unsigned* t = apis[side]->totals;
    std::snprintf(buf, sizeof(buf), " totals %u %u %u %u %u %u", t[0], t[1], t[2], t[3], t[4], t[5]);
    s += buf;
    for (auto& [ext, type] : types[side]) s += " " + ext + ":" + std::to_string(type->refs);
    return s;
}

// ---- names
static const char* const kBaseNames[] = {"a.tga", "b.tga", "c.wav", "d.obj", "e.fly", "f.tga", "g", "h.fly", "dir/i.tga", "j.obj"};
static std::string RandomName()
{
    std::string n = kBaseNames[Pick(10)];
    if (Pick(8) == 0) n += std::string(";") + kBaseNames[Pick(10)];
    if (Pick(20) == 0) n = ";" + n;
    return n;
}

static LONG WINAPI Crashed(EXCEPTION_POINTERS* e)
{
    const uintptr_t at = e->ContextRecord->Eip;
    HMODULE m = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<const char*>(at), &m);
    char name[MAX_PATH] = "?";
    if (m) GetModuleFileNameA(m, name, MAX_PATH);
    std::fprintf(stderr, "crash %08lx at %08x (%s +%x), data %08x\n", e->ExceptionRecord->ExceptionCode,
                 static_cast<unsigned>(at), name, static_cast<unsigned>(at - reinterpret_cast<uintptr_t>(m)),
                 static_cast<unsigned>(e->ExceptionRecord->ExceptionInformation[1]));
    // the stack's return addresses that fall in a module
    const uintptr_t* sp = reinterpret_cast<const uintptr_t*>(e->ContextRecord->Esp);
    for (int i = 0; i < 400; ++i) {
        HMODULE mod = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<const char*>(sp[i]), &mod) || !mod)
            continue;
        char mn[MAX_PATH];
        GetModuleFileNameA(mod, mn, MAX_PATH);
        const char* base = std::strrchr(mn, '\\');
        std::fprintf(stderr, "  [%d] %s +%x\n", i, base ? base + 1 : mn, static_cast<unsigned>(sp[i] - reinterpret_cast<uintptr_t>(mod)));
    }
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(Crashed);
    if (argc < 2) {
        std::printf("usage: diff_kr_system <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 300;
    trace = argc > 3;
    SetDllDirectoryA(dir.c_str());
    HMODULE ks = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    nextFloat = Get<decltype(nextFloat)>(ks, "?NextFloat@LCRand@Jet@@QAEMM@Z");
    LoadLibraryA((dir + "\\Kernel_7c.dll").c_str());
    HMODULE mods[2] = {LoadLibraryA((dir + "\\KernelResource_7c.dll").c_str()),
                       LoadLibraryA((dir + "\\KernelResource_7c_orig.dll").c_str())};
    if (getenv("KR_SWAP")) std::swap(mods[0], mods[1]);  // the original first (to see which one fails)
    if (!ks || !mods[0] || !mods[1]) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    cache = Get<void**>(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    createNode = Get<decltype(createNode)>(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    destroyNode = Get<decltype(destroyNode)>(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    allocNew = Get<decltype(allocNew)>(ks, "?New@Alloc@Jet@@SAPAXI@Z");
    allocDelete = Get<decltype(allocDelete)>(ks, "?Delete@Alloc@Jet@@SAXPAX@Z");
    static Api api[2];
    for (int s = 0; s < 2; ++s) {
        HMODULE m = mods[s];
        Api& a = api[s];
        a.module = m;
        a.resourceCtor = Get<decltype(a.resourceCtor)>(m, "??0Resource@Jet@@QAE@ABVPString@1@PBVResourceDatabase@1@@Z");
        a.systemCtor = Get<decltype(a.systemCtor)>(m, "??0ResourceSystem@Jet@@QAE@XZ");
        a.systemDtor = Get<decltype(a.systemDtor)>(m, "??1ResourceSystem@Jet@@QAE@XZ");
        a.init = Get<decltype(a.init)>(m, "?Init@ResourceSystem@Jet@@QAAXPBVOptions@2@@Z");
        a.update = Get<decltype(a.update)>(m, "?Update@ResourceSystem@Jet@@QAAXM@Z");
        a.systemDump = Get<decltype(a.systemDump)>(m, "?DumpUnusedResourcesFromCache@ResourceSystem@Jet@@QAEXXZ");
        a.dbCtor = Get<decltype(a.dbCtor)>(m, "??0ResourceDatabase@Jet@@QAE@ABVPString@1@0@Z");
        a.namelessCtor = Get<decltype(a.namelessCtor)>(m, "??0ResourceNamelessDatabase@Jet@@QAE@ABVPString@1@0@Z");
        a.addParentName = Get<decltype(a.addParentName)>(m, "?AddParent@ResourceDatabase@Jet@@QAAXABVPString@2@@Z");
        a.addParent = Get<decltype(a.addParent)>(m, "?AddParent@ResourceDatabase@Jet@@QAAXPAV12@@Z");
        a.addVariant = Get<decltype(a.addVariant)>(m, "?AddVariant@ResourceDatabase@Jet@@QAAXABVPString@2@@Z");
        a.removeName = Get<decltype(a.removeName)>(m, "?Remove@ResourceDatabase@Jet@@QBAXABVPString@2@@Z");
        a.removeParent = Get<decltype(a.removeParent)>(m, "?RemoveParent@ResourceDatabase@Jet@@QAAXPAV12@@Z");
        a.dbDump = Get<decltype(a.dbDump)>(m, "?DumpUnusedResourcesFromCache@ResourceDatabase@Jet@@QAEXXZ");
        a.release = Get<decltype(a.release)>(m, "?ReleaseReference@Resource@Jet@@QBA_NXZ");
        a.addRef = Get<decltype(a.addRef)>(m, "?AddReference@Resource@Jet@@QBAXXZ");
        a.touch = Get<decltype(a.touch)>(m, "?Touch@Resource@Jet@@QBAXXZ");
        a.getSize = Get<decltype(a.getSize)>(m, "?GetSize@Resource@Jet@@QBAIXZ");
        a.instCtor = Get<decltype(a.instCtor)>(m, "??0ResourceInstance@Jet@@QAE@XZ");
        a.instCopy = Get<decltype(a.instCopy)>(m, "??0ResourceInstance@Jet@@QAE@ABV01@@Z");
        a.instDtor = Get<decltype(a.instDtor)>(m, "??1ResourceInstance@Jet@@QAE@XZ");
        a.instAssign = Get<decltype(a.instAssign)>(m, "??4ResourceInstance@Jet@@QAAAAV01@ABV01@@Z");
        a.instInit = Get<decltype(a.instInit)>(m, "?Init@ResourceInstance@Jet@@QAAXPBVResourceDatabase@2@ABVPString@2@@Z");
        a.instInit3 = Get<decltype(a.instInit3)>(m, "?Init@ResourceInstance@Jet@@QAAXPBVResourceDatabase@2@ABVPString@2@_N@Z");
        a.instReset = Get<decltype(a.instReset)>(m, "?Reset@ResourceInstance@Jet@@QAAXXZ");
        a.instLock = Get<decltype(a.instLock)>(m, "?Lock@ResourceInstance@Jet@@QBA?AV?$ResultClass@$1?results@Resource@Jet@@2QBUResultTable@2@BW4Results@Resource@Jet@@@2@XZ");
        a.instUnlock = Get<decltype(a.instUnlock)>(m, "?Unlock@ResourceInstance@Jet@@QBAXXZ");
        a.instAvailable = Get<decltype(a.instAvailable)>(m, "?IsAvailable@ResourceInstance@Jet@@QBA_NXZ");
        a.pause = Get<decltype(a.pause)>(m, "?PauseProcessing@ResourceSystem@Jet@@QAAXXZ");
        a.resume = Get<decltype(a.resume)>(m, "?ResumeProcessing@ResourceSystem@Jet@@QAAXXZ");
        a.cache = Get<decltype(a.cache)>(m, "?Cache@Resource@Jet@@QBAX_N@Z");
        a.resourceVftable = Get<void*>(m, "??_7Resource@Jet@@6B@");
        static unsigned* totals[2];
        a.totals = Get<unsigned*>(m, "?totalResources@Resource@Jet@@0IA");
        (void)totals;
        apis[s] = &a;
        // the resource vftable with our slots
        resourceVt[s] = static_cast<void**>(std::malloc(18 * sizeof(void*)));
        std::memcpy(resourceVt[s], a.resourceVftable, 18 * sizeof(void*));
        resourceVt[s][7] = reinterpret_cast<void*>(&ResReset);
        resourceVt[s][10] = reinterpret_cast<void*>(&ResLoad);
        resourceVt[s][11] = reinterpret_cast<void*>(&ResLoadTxt);
        resourceVt[s][12] = reinterpret_cast<void*>(&ResLoadComplete);
        resourceVt[s][14] = reinterpret_cast<void*>(&ResUnload);
        resourceVt[s][15] = reinterpret_cast<void*>(&ResSize);
        resourceVt[s][16] = reinterpret_cast<void*>(&ResDataSize);
        setPriority[s] = resourceVt[s][4];
        resourceVt[s][4] = reinterpret_cast<void*>(&ResSetPriority);
        PatchImport(m, "?Exists@File@Jet@@SA?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@PBD@Z", reinterpret_cast<void*>(&FakeExists));
        PatchImport(m, "?Subst@File@Jet@@SAXABVPString@2@PBD_N@Z", reinterpret_cast<void*>(&FakeSubst));
        PatchImport(m, "?GetSeconds@Time@Jet@@SANXZ", reinterpret_cast<void*>(&FakeSeconds));
        PatchImport(m, "?Get@Options@Jet@@QBA_NPBDAA_N_N@Z", reinterpret_cast<void*>(&FakeGetBool));
        PatchImport(m, "?Get@Options@Jet@@QBA_NPBD@Z", reinterpret_cast<void*>(&FakeGetPlain));
        PatchImport(m, "?Get@Options@Jet@@QBA_NPBDAAMM@Z", reinterpret_cast<void*>(&FakeGetFloat));
        PatchImport(m, "?Get@Options@Jet@@QBA_NPBDAAII@Z", reinterpret_cast<void*>(&FakeGetUint));
        PatchImport(m, "??0Task@Jet@@QAE@ABVPString@1@M_N@Z", reinterpret_cast<void*>(&FakeTaskCtor));
        PatchImport(m, "??1Task@Jet@@UAE@XZ", reinterpret_cast<void*>(&FakeTaskDtor));
        PatchImport(m, "?NextFloat@LCRand@Jet@@QAEMM@Z", reinterpret_cast<void*>(&FakeNextFloat));
        PatchImport(m, "??0LCRand@Jet@@QAE@XZ", reinterpret_cast<void*>(&FakeLCRandCtor));
        PatchImport(m, "?LookupPluginType@PluginBase@Jet@@KA?AVResult@2@PAPAVPluginType@2@ABVUId@2@ABVPString@2@@Z", reinterpret_cast<void*>(&FakeLookup));
        for (const char* ext : {"tga", "wav", "obj.txt", "fly", "g.txt"}) types[s][ext] = new FakeType{typeVt, 0, ext};
    }

    static unsigned char systems[2][0x110];
    unsigned char options[16] = {};
    const char* const drives[] = {"base", "game", "mods", "extra"};
    for (int sc = 0; sc < scenarios; ++sc) {
        files.clear();
        for (const char* drive : drives)
            for (const char* variant : {"", "hi/", "lo/"})
                for (const char* n : kBaseNames) {
                    const std::string f = std::string(drive) + ":" + variant + n;
                    if (Pick(4) == 0) files.insert(f);
                    if (Pick(5) == 0) files.insert(f + ".txt");
                }
        instantBin = Pick(5) == 0;
        caching = Pick(8) == 0;
        randSeed = rng() | 1;
        binMaxDeleted = 1 + Pick(6);
        binProcessTime = Pick(2) ? 0.5f : 0.1f;
        const unsigned seed = rng();
        dbs.clear();
        parentsOf.clear();
        // per side: held resources and instances
        std::vector<const unsigned char*> held[2];
        std::vector<int> heldDb;
        static unsigned char instances[2][4][0xc];
        for (side = 0; side < 2; ++side) {
            secondsNow = 100.0;
            apis[side]->systemCtor(systems[side], nullptr);
            apis[side]->init(systems[side], options);
            if (caching) apis[side]->pause(systems[side]);
            for (auto& inst : instances[side]) apis[side]->instCtor(inst, nullptr);
            // the next size check is a static of the module: past the last scenario's time, so it can come again
            if (timeFloor > 0) apis[side]->update(systems[side], timeFloor);
            Log("%s", DumpSystem(systems[side]).c_str());
        }
        (void)seed;
        const int steps = 1 + static_cast<int>(Pick(80));
        for (int st = 0; st < steps; ++st) {
            const unsigned op = Pick(100);
            const unsigned a = rng(), b = rng(), c = rng();
            const std::string name = RandomName();
            const std::string name2 = RandomName();
            const float dt = static_cast<float>(Pick(Pick(3) ? 30 : 120)) / 10.0f;
            const int dbIndex = dbs.empty() ? -1 : static_cast<int>(a % dbs.size());
            const int dbIndex2 = dbs.empty() ? -1 : static_cast<int>(b % dbs.size());
            // pick a held resource
            const int hold = held[0].empty() ? -1 : static_cast<int>(c % held[0].size());
            std::string newDbName;
            const bool makeDb = op < 8 || dbs.empty();
            if (makeDb) {
                newDbName = drives[Pick(4)];
                for (const auto& d : dbs)
                    if (d.name == newDbName) newDbName.clear();
            }
            bool removedDb = false;
            // a parent link that would close a cycle is not made
            const bool cyclic = op >= 52 && op < 53 && !dbs.empty() && Reaches(dbs[dbIndex2].name, dbs[dbIndex].name);
            if (op >= 52 && op < 53 && !dbs.empty() && !cyclic) parentsOf[dbs[dbIndex].name].insert(dbs[dbIndex2].name);
            if (op >= 53 && op < 54 && !dbs.empty()) parentsOf[dbs[dbIndex].name].erase(dbs[dbIndex2].name);
            for (side = 0; side < 2; ++side) {
                const Api& A = *apis[side];
                unsigned char* const rs = systems[side];
                secondsNow = 1000.0 + st;
                if (trace) std::fprintf(stderr, "sc %d st %d op %u side %d\n", sc, st, op, side), std::fflush(stderr);
                std::string what;
                if (makeDb) {  // a database
                    if (!newDbName.empty()) {
                        unsigned char* db = static_cast<unsigned char*>(allocNew(0x48));
                        void* n = Node(newDbName);
                        void* path = Node("C:/game/data");
                        if (a % 3)
                            A.dbCtor(db, nullptr, &n, &path);
                        else
                            A.namelessCtor(db, nullptr, &n, &path);
                        if (side == 0) dbs.push_back({{db, nullptr}, newDbName});
                        else dbs.back().db[1] = db;
                        what = "new db " + newDbName;
                    }
                } else if (op < 40) {  // Get
                    unsigned char* db = dbs[dbIndex].db[side];
                    void* n = Node(name);
                    const int flags = static_cast<int>(b % 8);
                    const auto* r = VCall<const unsigned char*>(db, 1, static_cast<void* const*>(&n), flags);
                    what = "Get " + name + " " + std::to_string(flags) + " -> " + (r ? DumpResource(r) : "null");
                    ++coverage[r ? "get found" : "get none"];
                    // keep the reference when it gained one (found in this database's table or made here)
                    if (r && side == 0) heldDb.push_back(dbIndex);
                    if (r) held[side].push_back(r);
                } else if (op < 48) {  // Exists / GetFileName
                    unsigned char* db = dbs[dbIndex].db[side];
                    void* n = Node(name);
                    const bool e = VCall<bool>(db, 2, static_cast<void* const*>(&n));
                    void* out = nullptr;
                    VCall<void**>(db, 3, &out, static_cast<void* const*>(&n));
                    what = "Exists " + name + " " + std::to_string(e) + " file " + Text(out);
                    if (out) destroyNode(*cache, out);
                } else if (op < 52) {  // Remove by name (it deletes the resource even when held: let go of it first)
                    for (auto& inst : instances[side]) {
                        const unsigned char* r = *reinterpret_cast<const unsigned char* const*>(inst);
                        if (r && ResName(r) == name) A.instReset(inst);
                    }
                    for (size_t i = held[side].size(); i-- > 0;) {
                        if (ResName(held[side][i]) != name) continue;
                        A.release(held[side][i]);
                        held[side].erase(held[side].begin() + static_cast<long>(i));
                        if (side == 1) heldDb.erase(heldDb.begin() + static_cast<long>(i));
                    }
                    if (caching) ClaimQueued(dbs[dbIndex].db[side]);
                    void* n = Node(name);
                    A.removeName(dbs[dbIndex].db[side], &n);
                    what = "Remove " + name;
                } else if (op < 56) {  // parents and variants
                    unsigned char* db = dbs[dbIndex].db[side];
                    if (op < 53) {
                        if (!cyclic) {
                            void* n = Node(dbs[dbIndex2].name);
                            A.addParentName(db, &n);
                        }
                        what = "AddParent " + dbs[dbIndex2].name + (cyclic ? " (skipped)" : "");
                    } else if (op < 54) {
                        A.removeParent(db, dbs[dbIndex2].db[side]);
                        what = "RemoveParent " + dbs[dbIndex2].name;
                    } else {
                        void* v = Node(a % 2 ? "hi" : "lo");
                        A.addVariant(db, &v);
                        what = std::string("AddVariant ") + (a % 2 ? "hi" : "lo");
                    }
                } else if (op < 72 && hold >= 0) {  // a held resource
                    const unsigned char* r = held[side][hold];
                    const unsigned k = b % 9;
                    if (k < 3) {
                        int res = -1;
                        VCall<int*>(r, 0, &res);
                        if (res == 0 && (c & 4)) VCall<void>(r, 1);  // mostly let go at once
                        what = "Lock " + ResName(r) + " -> " + std::to_string(res) + ((res == 0 && (c & 4)) ? " unlocked" : "");
                        ++coverage["lock " + std::to_string(res)];
                    } else if (k < 4) {
                        VCall<void>(r, 1);
                        what = "Unlock " + ResName(r);
                    } else if (k < 6) {
                        const bool last = A.release(r);
                        what = "Release " + ResName(r) + " -> " + std::to_string(last);
                        held[side].erase(held[side].begin() + hold);
                        if (side == 1) heldDb.erase(heldDb.begin() + hold);
                    } else if (k < 7) {
                        VCall<void>(r, 4, static_cast<float>(c % 50));
                        A.touch(r);
                        what = "SetPriority " + ResName(r);
                        // (forced, a queued resource would be queued twice, see ClaimQueued)
                        const bool force = (c & 32) && *reinterpret_cast<const int*>(r + 0x28) != 2;
                        if (c & 16) {
                            A.cache(r, force);
                            what += force ? " Cache force" : " Cache";
                        }
                    } else if (k < 8) {
                        if (c % 2)
                            VCall<void>(r, 5, static_cast<const void*>(&observers[c % 3]));
                        else
                            VCall<void>(r, 6, static_cast<const void*>(&observers[c % 3]));
                        what = std::string(c % 2 ? "AddObserver " : "RemoveObserver ") + ResName(r);
                    } else {
                        int res = -1;
                        const bool all = VCall<bool>(r, 2);
                        VCall<int*>(r, 3, &res);
                        what = "LoadAll " + ResName(r) + " " + std::to_string(all) + " " + std::to_string(res) + " size " + std::to_string(A.getSize(r));
                    }
                } else if (op < 82) {  // instances
                    unsigned char* inst = instances[side][a % 4];
                    const unsigned k = b % 7;
                    if (k < 2) {
                        void* n = Node(name);
                        if (k == 0)
                            A.instInit(inst, dbs[dbIndex].db[side], &n);
                        else
                            A.instInit3(inst, dbs[dbIndex].db[side], &n, (c & 1) != 0);
                        what = "Init " + name;
                    } else if (k < 4) {
                        int res = -1;
                        A.instLock(inst, &res);
                        what = "ILock -> " + std::to_string(res);
                    } else if (k < 5) {
                        A.instUnlock(inst);
                        what = "IUnlock";
                    } else if (k < 6) {
                        A.instReset(inst);
                        what = "IReset";
                    } else {
                        A.instAssign(inst, instances[side][c % 4]);
                        what = "IAssign";
                    }
                    const unsigned char* r = *reinterpret_cast<const unsigned char* const*>(inst);
                    what += " -> " + (r ? ResName(r) : std::string("none")) + " fl" + std::to_string(inst[8] & 1) + " av" + std::to_string(A.instAvailable(inst));
                } else if (op < 91) {  // time passes
                    A.update(rs, dt);
                    what = "Update";
                } else if (op < 94) {
                    // a burst: names got, loaded and let go in every database (loaded resources nobody holds, for
                    // the size limit's LRU pass over several databases), then time passes
                    what = "Burst";
                    for (size_t k = 0; k < dbs.size(); ++k)
                        for (unsigned i = 0; i < 7; ++i) {
                            const char* const burstName = kBaseNames[(a / (i + 1) + i * 7 + k) % 10];
                            void* n = Node(burstName);
                            const auto* r = VCall<const unsigned char*>(dbs[k].db[side], 1, static_cast<void* const*>(&n), 0);
                            if (!r) continue;
                            int res = -1;
                            VCall<int*>(r, 0, &res);
                            if (res == 0) VCall<void>(r, 1);
                            A.release(r);
                            what += std::string(" ") + burstName + ":" + std::to_string(res);
                        }
                    A.update(rs, 0.5f);
                } else if (op < 96) {
                    if (!instantBin) {
                        A.systemDump(rs, nullptr);
                        what = "DumpUnused";
                    } else {
                        A.dbDump(dbs[dbIndex].db[side], nullptr);
                        what = "DbDumpUnused";
                    }
                } else if (op < 98 && dbs.size() > 1) {  // a database goes (after its holders let go)
                    const int k = dbIndex;
                    for (auto& inst : instances[side]) {
                        const unsigned char* r = *reinterpret_cast<const unsigned char* const*>(inst);
                        if (r && *reinterpret_cast<void* const*>(r + 8) == dbs[k].db[side]) A.instReset(inst);
                    }
                    for (size_t i = held[side].size(); i-- > 0;) {
                        const unsigned char* r = held[side][i];
                        if (*reinterpret_cast<void* const*>(r + 8) != dbs[k].db[side] && heldDb[i] != k) continue;
                        A.release(r);
                        held[side].erase(held[side].begin() + static_cast<long>(i));
                        if (side == 1) heldDb.erase(heldDb.begin() + static_cast<long>(i));
                    }
                    if (caching) ClaimQueued(dbs[k].db[side]);
                    reinterpret_cast<void(__fastcall*)(void*, void*, unsigned)>((*reinterpret_cast<void***>(dbs[k].db[side]))[0])(dbs[k].db[side], nullptr, 1);
                    what = "delete db " + dbs[k].name;
                    removedDb = true;
                }
                if (trace) std::fprintf(stderr, " did %s\n", what.c_str()), std::fflush(stderr);
                Log("%s", what.c_str());
                Log("%s", DumpSystem(rs).c_str());
                for (const auto& d : dbs)
                    if (d.db[side] && !(removedDb && d.db[side] == dbs[dbIndex].db[side])) Log("%s", DumpDb(d.db[side]).c_str());
            }
            if (removedDb) {
                parentsOf.erase(dbs[dbIndex].name);
                for (auto& [k, v] : parentsOf) v.erase(dbs[dbIndex].name);
                dbs.erase(dbs.begin() + dbIndex);
            }
            const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
            for (size_t i = 0; i < n; ++i) {
                ++checks;
                const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
                const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
                if (x != y) {
                    if (++bad <= 5) {
                        size_t at = 0;
                        while (at < x.size() && at < y.size() && x[at] == y[at]) ++at;
                        const size_t from = at > 100 ? at - 100 : 0;
                        std::printf("scenario %d step %d op %u line %zu:\n  ours ...%s\n  orig ...%s\n", sc, st, op, i,
                                    x.substr(from, 260).c_str(), y.substr(from, 260).c_str());
                    }
                    break;
                }
            }
            logs[0].clear();
            logs[1].clear();
            if (bad > 5) break;
        }
        // the end: let go of everything, then the databases and the system
        timeFloor = *reinterpret_cast<const float*>(systems[0] + 0xdc) + 1.0f;
        for (side = 0; side < 2; ++side) {
            const Api& A = *apis[side];
            auto traceDbs = [&](const char* when) {
                if (!trace) return;
                std::fprintf(stderr, "teardown %d %s\n", side, when);
                for (auto& d : dbs) std::fprintf(stderr, "  %s\n", DumpDb(d.db[side]).substr(0, 1500).c_str());
                if (const unsigned char* loader = *reinterpret_cast<const unsigned char* const*>(systems[side] + 0x14)) {
                    const unsigned n = *reinterpret_cast<const unsigned*>(loader + 8);
                    const auto* data = *reinterpret_cast<const unsigned char* const* const*>(loader + 0x10);
                    std::fprintf(stderr, "  loader heap %u:", n);
                    for (unsigned i = 0; i < n; ++i) std::fprintf(stderr, " %p", static_cast<const void*>(data[i]));
                    std::fprintf(stderr, "\n");
                }
                std::fflush(stderr);
            };
            traceDbs("start");
            for (auto& inst : instances[side]) A.instDtor(inst, nullptr);
            traceDbs("instances");
            for (const unsigned char* r : held[side]) A.release(r);
            traceDbs("released");
            if (caching)
                for (auto& d : dbs) ClaimQueued(d.db[side]);
            for (auto& d : dbs) reinterpret_cast<void(__fastcall*)(void*, void*, unsigned)>((*reinterpret_cast<void***>(d.db[side]))[0])(d.db[side], nullptr, 1);
            // what the loader still holds now it deletes on its exit (on its thread, and after the databases)
            if (const unsigned char* loader = *reinterpret_cast<const unsigned char* const*>(systems[side] + 0x14))
                if (*reinterpret_cast<const unsigned*>(loader + 8)) ++coverage["loader heap left"];
            A.systemDtor(systems[side], nullptr);
            Log("end %s", DumpSystem(systems[side]).c_str());
        }
        for (size_t i = 0; i < logs[0].size() || i < logs[1].size(); ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 5) std::printf("scenario %d end line %zu:\n  ours %s\n  orig %s\n", sc, i, x.c_str(), y.c_str());
                break;
            }
        }
        logs[0].clear();
        logs[1].clear();
        if (bad > 5) break;
    }
    for (const auto& [k, v] : coverage) std::printf("  [%s] %llu\n", k.c_str(), v);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
