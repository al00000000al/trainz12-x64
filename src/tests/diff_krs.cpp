// Differential test of KernelResources_7c: our module against KernelResources_7c_orig.dll in one process.
//
// The bitmap resource code is driven through its exports with a stand-in resource (a fake object whose vtable
// records every call) and real KernelStandard bitmaps: random areas, clips, justifications and repeat limits go to
// the three BitmapBaseResource::Render variants and the BitmapBaseInstance members of both modules, and the
// destination pixels, the results and the call logs are compared. BitmapResource is built through its exported
// constructor and loads generated TGA files through a stand-in database (with texture streaming limits that force
// the halving loop).
//
// usage: diff_krs.exe <dir with KernelResources_7c.dll, KernelResources_7c_orig.dll and the Jet modules> [cases]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static HMODULE ours, orig, ks;
static unsigned long long checks, bad;
static std::mt19937 rng(777);
static int Pick(int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); }

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

template <class F>
struct Pair {
    F a, b;
    explicit Pair(const char* name) : a(Get<F>(ours, name)), b(Get<F>(orig, name)) {}
};

static std::vector<std::string>* dumpA;
static std::vector<std::string>* dumpB;
static void Check(bool ok, const char* what, int c)
{
    if (!ok && bad < 3 && dumpA) {
        std::printf("  ours:");
        for (auto& s : *dumpA) std::printf(" [%s]", s.c_str());
        std::printf("\n  orig:");
        for (auto& s : *dumpB) std::printf(" [%s]", s.c_str());
        std::printf("\n");
    }
    ++checks;
    if (!ok && ++bad <= 20) std::printf("  mismatch %s case %d\n", what, c);
}

struct Rect {
    int x, y, w, h;
};
struct Point {
    int x, y;
};
struct Size {
    int w, h;
};

// ---- KernelStandard: PStrings and bitmaps

static void* pstringCache;
using CreateFn = void*(__cdecl*)(void* cache, const unsigned char* text);
static CreateFn createNode;

struct PString {
    void* node;
};

static PString MakePString(const char* s) { return PString{createNode(pstringCache, reinterpret_cast<const unsigned char*>(s))}; }

using BitmapCtor = void*(__fastcall*)(void* self, void*, const Size* size, int format, unsigned char* data);
using BitmapDtor = void(__fastcall*)(void* self, void*);
static BitmapCtor bitmapCtor;
static BitmapDtor bitmapDtor;
static int format32 = -1;

struct BitmapMem {  // the 0x44-byte KernelStandard bitmap
    int width, height;
    unsigned char unknown08[4];
    unsigned char* data;  // +0x0c
    int pitch;            // +0x10
    int bpp;              // +0x14
    unsigned char rest[0x44 - 0x18];
};
static_assert(sizeof(BitmapMem) == 0x44);

static BitmapMem* NewBitmap(int w, int h, bool randomFill)
{
    auto* b = static_cast<BitmapMem*>(std::calloc(1, sizeof(BitmapMem)));
    const Size s = {w, h};
    bitmapCtor(b, nullptr, &s, format32, nullptr);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w * b->bpp; ++x) b->data[y * b->pitch + x] = randomFill ? static_cast<unsigned char>(rng()) : 0;
    return b;
}

static void FreeBitmap(BitmapMem* b)
{
    bitmapDtor(b, nullptr);
    std::free(b);
}

static bool SamePixels(const BitmapMem* a, const BitmapMem* b)
{
    if (a->width != b->width || a->height != b->height) return false;
    for (int y = 0; y < a->height; ++y)
        if (std::memcmp(a->data + y * a->pitch, b->data + y * b->pitch, a->width * a->bpp) != 0) return false;
    return true;
}

static void FindFormat()
{
    for (int f = 0; f < 32 && format32 < 0; ++f) {
        BitmapMem b = {};
        const Size s = {4, 4};
        __try {
            bitmapCtor(&b, nullptr, &s, f, nullptr);
            if (b.bpp == 4 && b.data && b.pitch >= 16) format32 = f;
            bitmapDtor(&b, nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    if (format32 < 0) {
        std::printf("no 32-bit bitmap format found\n");
        std::exit(2);
    }
}

// ---- a stand-in resource: vtable slots log their calls

struct Fake;
static std::vector<std::string> callLog;
static char logLine[256];
static void Log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(logLine, sizeof logLine, fmt, ap);
    va_end(ap);
    callLog.emplace_back(logLine);
}

struct Fake {
    void** vt;
    unsigned char pad[0x24];
    int status;  // +0x28
    unsigned char pad2[0x80];
    int lockResult;
    const void* bitmap;
    Rect area;
    Point offset;
};

static int* __cdecl FLock(Fake* f, int* r)
{
    Log("Lock");
    *r = f->lockResult;
    return r;
}
static void __cdecl FUnlock(Fake*) { Log("Unlock"); }
static int* __cdecl FIsAllLoaded(Fake*, int* r)
{
    Log("IsAllLoaded");
    *r = 3;
    return r;
}
static const void* __cdecl FGetBitmap(Fake* f)
{
    Log("GetBitmap");
    return f->bitmap;
}
static Rect* __cdecl FGetArea(Fake* f, Rect* r)
{
    Log("GetArea");
    *r = f->area;
    return r;
}
static Point* __cdecl FGetOffset(Fake* f, Point* r)
{
    Log("GetOffset");
    *r = f->offset;
    return r;
}
static void __cdecl FRender18(Fake*, const void* db, void* dest, const Rect* area, unsigned just, const Rect* clip)
{
    Log("Render18 %p %d,%d,%d,%d %u %d,%d,%d,%d", db, area->x, area->y, area->w, area->h, just, clip->x,
        clip->y, clip->w, clip->h);
}
static void __cdecl FRender19(Fake*, const void* db, void* dest, const Rect* area, const Rect* clip, unsigned just,
                              const Size* rep)
{
    Log("Render19 %p %d,%d,%d,%d %d,%d,%d,%d %u %d,%d", db, area->x, area->y, area->w, area->h, clip->x,
        clip->y, clip->w, clip->h, just, rep->w, rep->h);
}
static void __cdecl FRender20(Fake*, const void* db, void* dest, const Rect* area, const Rect* clip)
{
    Log("Render20 %p %d,%d,%d,%d %d,%d,%d,%d", db, area->x, area->y, area->w, area->h, clip->x, clip->y,
        clip->w, clip->h);
}
static void __cdecl FTrap() { Log("unexpected slot"); }

static void* fakeVt[32];

static void InitFake(Fake& f)
{
    std::memset(&f, 0, sizeof f);
    f.vt = fakeVt;
}

static Rect RandRect(int lo, int hi, int wlo, int whi) { return Rect{Pick(lo, hi), Pick(lo, hi), Pick(wlo, whi), Pick(wlo, whi)}; }

// ---- tests

using Render6 = void(__cdecl*)(Fake*, const void*, BitmapMem*, const Rect*, const Rect*, unsigned, const Size*);
using Render5 = void(__cdecl*)(Fake*, const void*, BitmapMem*, const Rect*, unsigned, const Rect*);
using Render4 = void(__cdecl*)(Fake*, const void*, BitmapMem*, const Rect*, const Rect*);

static void TestRender(int cases)
{
    const Pair<Render6> r6("?Render@BitmapBaseResource@Jet@@UAAXPBVResourceDatabase@2@AAVBitmap@2@ABURect@2@2IABUSize@2@@Z");
    const Pair<Render5> r5("?Render@BitmapBaseResource@Jet@@UAAXPBVResourceDatabase@2@AAVBitmap@2@ABURect@2@I2@Z");
    const Pair<Render4> r4("?Render@BitmapBaseResource@Jet@@UAAXPBVResourceDatabase@2@AAVBitmap@2@ABURect@2@2@Z");
    BitmapMem* src = NewBitmap(40, 30, true);
    for (int c = 0; c < cases; ++c) {
        Fake f;
        InitFake(f);
        f.lockResult = Pick(0, 9) == 0 ? 4 : 0;
        f.bitmap = src;
        f.area = Rect{Pick(0, 12), Pick(0, 10), Pick(-1, 28), Pick(-1, 20)};
        const Rect area = RandRect(-20, 60, -2, 70);
        const Rect clip = Pick(0, 3) ? RandRect(-10, 50, -2, 70) : Rect{0, 0, 64, 48};
        const unsigned just = static_cast<unsigned>(Pick(0, 15));
        const Size rep = {Pick(0, 3) ? 0 : Pick(1, 4), Pick(0, 3) ? 0 : Pick(1, 4)};
        const int which = Pick(0, 2);
        BitmapMem* da = NewBitmap(64, 48, true);
        BitmapMem* db = NewBitmap(64, 48, false);
        for (int y = 0; y < 48; ++y) std::memcpy(db->data + y * db->pitch, da->data + y * da->pitch, 64 * 4);
        std::vector<std::string> la, lb;
        callLog.clear();
        if (which == 0) r6.a(&f, nullptr, da, &area, &clip, just, &rep);
        if (which == 1) r5.a(&f, nullptr, da, &area, just, &clip);
        if (which == 2) r4.a(&f, nullptr, da, &area, &clip);
        la.swap(callLog);
        if (which == 0) r6.b(&f, nullptr, db, &area, &clip, just, &rep);
        if (which == 1) r5.b(&f, nullptr, db, &area, just, &clip);
        if (which == 2) r4.b(&f, nullptr, db, &area, &clip);
        lb.swap(callLog);
        static const char* names[] = {"Render tiled", "Render justified", "Render (4 args)"};
        Check(SamePixels(da, db), names[which], c);
        dumpA = &la, dumpB = &lb;
        Check(la == lb, "Render call log", c);
        FreeBitmap(da);
        FreeBitmap(db);
    }
    FreeBitmap(src);
}

struct Instance {
    Fake* resource;
    const void* orig;
    unsigned char flags, pad[3];
};

using IRender3 = void(__cdecl*)(const Instance*, BitmapMem*, const Rect*, const Rect*);
using IRender5 = void(__cdecl*)(const Instance*, BitmapMem*, const Rect*, const Rect*, unsigned, const Size*);
using IRender4 = void(__cdecl*)(const Instance*, BitmapMem*, const Rect*, unsigned, const Rect*);
using IGetBitmap = const void*(__cdecl*)(const Instance*);
using IGetArea = Rect*(__cdecl*)(const Instance*, Rect*);
using IGetOffset = Point*(__cdecl*)(const Instance*, Point*);
using IBool = bool(__cdecl*)(const Instance*);
using ICtor = Instance*(__fastcall*)(Instance*, void*);
using ICopy = Instance*(__fastcall*)(Instance*, void*, const Instance*);

static void TestInstances(int cases)
{
    const Pair<IRender3> r3("?Render@BitmapBaseInstance@Jet@@QBAXAAVBitmap@2@ABURect@2@1@Z");
    const Pair<IRender5> r5("?Render@BitmapBaseInstance@Jet@@QBAXAAVBitmap@2@ABURect@2@1IABUSize@2@@Z");
    const Pair<IRender4> r4("?Render@BitmapBaseInstance@Jet@@QBAXAAVBitmap@2@ABURect@2@I1@Z");
    const Pair<IGetBitmap> gb("?GetBitmap@BitmapBaseInstance@Jet@@QBAPBVBitmap@2@XZ");
    const Pair<IGetArea> ga("?GetArea@BitmapBaseInstance@Jet@@QBA?BURect@2@XZ");
    const Pair<IGetOffset> go("?GetOffset@BitmapBaseInstance@Jet@@QBA?BUPoint@2@XZ");
    const Pair<IBool> av("?IsAvailable@BitmapBaseInstance@Jet@@QBA_NXZ");
    const Pair<IGetBitmap> gr("?GetBitmapBaseResource@BitmapBaseInstance@Jet@@ABAPAVBitmapBaseResource@2@XZ");
    const Pair<ICtor> ctor("??0BitmapInstance@Jet@@QAE@XZ");
    const Pair<ICopy> copy("??0BitmapBaseInstance@Jet@@QAE@ABV01@@Z");
    const Pair<ICopy> assign("??4BitmapFrameBaseInstance@Jet@@QAEAAV01@ABV01@@Z");
    for (int c = 0; c < cases; ++c) {
        Fake f;
        InitFake(f);
        f.lockResult = Pick(0, 4) == 0 ? 4 : 0;
        f.bitmap = reinterpret_cast<void*>(static_cast<uintptr_t>(rng()));
        f.area = RandRect(-5, 5, -5, 5);
        f.offset = Point{Pick(-9, 9), Pick(-9, 9)};
        f.status = Pick(4, 7);
        const Instance init = {Pick(0, 6) ? &f : nullptr, reinterpret_cast<void*>(static_cast<uintptr_t>(rng())),
                               static_cast<unsigned char>(Pick(0, 3)), {}};
        Instance a = init, b = init;
        const Rect area = RandRect(-9, 9, -9, 9), clip = RandRect(-9, 9, -9, 9);
        const unsigned just = static_cast<unsigned>(Pick(0, 15));
        const Size rep = {Pick(0, 3), Pick(0, 3)};
        BitmapMem* dest = reinterpret_cast<BitmapMem*>(0x1234);
        std::vector<std::string> la, lb;
        auto both = [&](auto&& fa, auto&& fb, const char* what) {
            callLog.clear();
            const auto ra = fa();
            la = std::move(callLog);
            callLog.clear();
            const auto rb = fb();
            lb = std::move(callLog);
            callLog.clear();
            dumpA = &la, dumpB = &lb;
            Check(ra == rb && la == lb && std::memcmp(&a, &b, sizeof a) == 0, what, c);
        };
        both([&] { r3.a(&a, dest, &area, &clip); return 0; }, [&] { r3.b(&b, dest, &area, &clip); return 0; }, "Instance::Render 3");
        both([&] { r5.a(&a, dest, &area, &clip, just, &rep); return 0; }, [&] { r5.b(&b, dest, &area, &clip, just, &rep); return 0; }, "Instance::Render 5");
        both([&] { r4.a(&a, dest, &area, just, &clip); return 0; }, [&] { r4.b(&b, dest, &area, just, &clip); return 0; }, "Instance::Render 4");
        if (init.resource) {
            both([&] { return gb.a(&a); }, [&] { return gb.b(&b); }, "Instance::GetBitmap");
            both([&] { Rect r; ga.a(&a, &r); return std::to_string(r.x) + "," + std::to_string(r.y) + "," + std::to_string(r.w) + "," + std::to_string(r.h); },
                 [&] { Rect r; ga.b(&b, &r); return std::to_string(r.x) + "," + std::to_string(r.y) + "," + std::to_string(r.w) + "," + std::to_string(r.h); }, "Instance::GetArea");
            both([&] { Point p; go.a(&a, &p); return p.x * 1000 + p.y; }, [&] { Point p; go.b(&b, &p); return p.x * 1000 + p.y; }, "Instance::GetOffset");
        }
        both([&] { return av.a(&a); }, [&] { return av.b(&b); }, "Instance::IsAvailable");
        both([&] { return gr.a(&a); }, [&] { return gr.b(&b); }, "Instance::GetBitmapBaseResource");
        Instance ca, cb;
        std::memset(&ca, 0xcc, sizeof ca);
        std::memset(&cb, 0xcc, sizeof cb);
        ctor.a(&ca, nullptr);
        ctor.b(&cb, nullptr);
        Check(ca.resource == cb.resource && ca.orig == cb.orig && (ca.flags & 1) == (cb.flags & 1), "Instance ctor", c);
    }
}

// ---- BitmapResource through a stand-in database

static std::string tgaPath;
static PString* __cdecl DbGetFileName(void*, PString* r, const PString*)
{
    *r = MakePString(tgaPath.c_str());
    return r;
}
static void* dbVt[8];
struct FakeDb {
    void** vt;
};

static void WriteTga(const char* path, int w, int h)
{
    std::vector<unsigned char> f(18 + w * h * 4);
    f[2] = 2;
    f[12] = static_cast<unsigned char>(w), f[13] = static_cast<unsigned char>(w >> 8);
    f[14] = static_cast<unsigned char>(h), f[15] = static_cast<unsigned char>(h >> 8);
    f[16] = 32;
    f[17] = 8;
    for (size_t i = 18; i < f.size(); ++i) f[i] = static_cast<unsigned char>(rng());
    FILE* fp = std::fopen(path, "wb");
    std::fwrite(f.data(), 1, f.size(), fp);
    std::fclose(fp);
}

using BrCtor = void*(__fastcall*)(void*, void*, const PString*, const void* db);
using BrLoad = int*(__cdecl*)(void*, int*, const char*);
using BrSize = unsigned(__cdecl*)(const void*);
using BrBool = bool(__cdecl*)(void*);
using BrGetBitmap = BitmapMem*(__cdecl*)(const void*);
using BrGetArea = Rect*(__cdecl*)(const void*, Rect*);
using BrSetBitmap = void(__cdecl*)(void*, const BitmapMem*);
using BrSetMax = void(__cdecl*)(int);

static void TestBitmapResource(int cases)
{
    const Pair<BrCtor> ctor("??0BitmapResource@Jet@@QAE@ABVPString@1@PBVResourceDatabase@1@@Z");
    const Pair<BrLoad> load("?Load@BitmapResource@Jet@@MAA?AV?$ResultClass@$1?results@Resource@Jet@@2QBUResultTable@2@BW4Results@Resource@Jet@@@2@PBD@Z");
    const Pair<BrSize> size("?CalculateSize@BitmapResource@Jet@@MBAIXZ");
    const Pair<BrSize> dataSize("?CalculateDataSize@BitmapResource@Jet@@MBAIXZ");
    const Pair<BrBool> unload("?Unload@BitmapResource@Jet@@MAA_NXZ");
    const Pair<BrGetBitmap> getBitmap("?GetBitmap@BitmapResource@Jet@@MBAPBVBitmap@2@XZ");
    const Pair<BrGetArea> getArea("?GetArea@BitmapResource@Jet@@MBA?BURect@2@XZ");
    const Pair<BrSetBitmap> setBitmap("?SetBitmap@BitmapResource@Jet@@MAAXPBVBitmap@2@@Z");
    const Pair<BrSetMax> setMax("?SetMaxSizeTextureStreaming@BitmapResource@Jet@@SAXH@Z");
    // Resource's constructor reads its resource system (+0xdc): give it a zeroed stand-in
    static unsigned char fakeSystem[0x400];
    HMODULE kr = GetModuleHandleA("KernelResource_7c_orig.dll");
    void** rs = kr ? reinterpret_cast<void**>(GetProcAddress(kr, "?resourceSystem@Resource@Jet@@0PAVResourceSystem@2@A")) : nullptr;
    if (!rs) {
        std::printf("no Resource::resourceSystem\n");
        return;
    }
    if (!*rs) *rs = fakeSystem;
    dbVt[3] = reinterpret_cast<void*>(&DbGetFileName);
    FakeDb db = {dbVt};
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    tgaPath = std::string(tmp) + "diff_krs.tga";
    const PString name = MakePString("test.tga");
    int loaded = 0, scaled = 0, scaledAway = 0;
    for (int c = 0; c < cases; ++c) {
        alignas(8) unsigned char ma[0x60] = {}, mb[0x60] = {};
        ctor.a(ma, nullptr, &name, &db);
        ctor.b(mb, nullptr, &name, &db);
        Check(size.a(ma) == size.b(mb), "CalculateSize", c);
        Check(dataSize.a(ma) == dataSize.b(mb), "CalculateDataSize (empty)", c);
        const int w = Pick(1, 70), h = Pick(1, 70);
        const bool missing = Pick(0, 9) == 0;
        if (missing)
            DeleteFileA(tgaPath.c_str());
        else
            WriteTga(tgaPath.c_str(), w, h);
        const int limit = Pick(0, 2) ? Pick(1, 40) : 2048;
        setMax.a(limit);
        setMax.b(limit);
        int ra = -1, rb = -1;
        load.a(ma, &ra, "test.tga");
        load.b(mb, &rb, "test.tga");
        Check(ra == rb, "Load result", c);
        BitmapMem* ba = getBitmap.a(ma);
        BitmapMem* bb = getBitmap.b(mb);
        loaded += ra == 0;
        // Bitmap::Scale gives null for this format, so a scaled load ends without a bitmap (in both modules)
        if (!ba && ra == 0) ++scaled;
        if (ra != 0 && !missing) ++scaledAway;
        Check((ba == nullptr) == (bb == nullptr), "Load bitmap present", c);
        if (ba && bb) {
            Check(SamePixels(ba, bb), "Load pixels", c);
            Check(dataSize.a(ma) == dataSize.b(mb), "CalculateDataSize", c);
            Rect a, b;
            getArea.a(ma, &a);
            getArea.b(mb, &b);
            Check(std::memcmp(&a, &b, sizeof a) == 0, "GetArea", c);
            BitmapMem* src = NewBitmap(Pick(1, 30), Pick(1, 30), true);
            setBitmap.a(ma, src);
            setBitmap.b(mb, src);
            Check(SamePixels(getBitmap.a(ma), getBitmap.b(mb)), "SetBitmap", c);
            FreeBitmap(src);
        }
        Check(unload.a(ma) == unload.b(mb), "Unload", c);
        Check(getBitmap.a(ma) == nullptr && getBitmap.b(mb) == nullptr, "Unload clears", c);
    }
    std::printf("BitmapResource: %d loads, %d loaded, %d scaled (Scale gave null), %d halved to nothing\n", cases, loaded, scaled,
                scaledAway);
    setMax.a(2048);
    setMax.b(2048);
    DeleteFileA(tgaPath.c_str());
}

static void TestBase()
{
    using GA = Rect*(__cdecl*)(const void*, Rect*);
    using GO = Point*(__cdecl*)(const void*, Point*);
    using GB = const void*(__cdecl*)(const void*);
    const Pair<GA> ga("?GetArea@BitmapBaseResource@Jet@@UBA?BURect@2@XZ");
    const Pair<GO> go("?GetOffset@BitmapBaseResource@Jet@@UBA?BUPoint@2@XZ");
    const Pair<GB> gb("?GetBitmap@BitmapBaseResource@Jet@@UBAPBVBitmap@2@XZ");
    Rect a = {1, 2, 3, 4}, b = {5, 6, 7, 8};
    ga.a(nullptr, &a);
    ga.b(nullptr, &b);
    Check(std::memcmp(&a, &b, sizeof a) == 0, "Base GetArea", 0);
    Point p = {1, 2}, q = {3, 4};
    go.a(nullptr, &p);
    go.b(nullptr, &q);
    Check(p.x == q.x && p.y == q.y, "Base GetOffset", 0);
    bool ta = false, tb = false;
    try {
        gb.a(nullptr);
    } catch (...) {
        ta = true;
    }
    try {
        gb.b(nullptr);
    } catch (...) {
        tb = true;
    }
    Check(ta && tb, "Base GetBitmap throws", 0);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_krs <dir> [cases]\n");
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string dir = argv[1];
    const int cases = argc > 2 ? std::atoi(argv[2]) : 20000;
    SetDllDirectoryA(dir.c_str());
    ks = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((dir + "\\KernelResources_7c_orig.dll").c_str());
    ours = LoadLibraryA((dir + "\\KernelResources_7c.dll").c_str());
    if (!ks || !ours || !orig) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    pstringCache = *Get<void**>(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    createNode = Get<CreateFn>(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    bitmapCtor = Get<BitmapCtor>(ks, "??0Bitmap@Jet@@QAE@ABUSize@1@W4Format@01@PAE@Z");
    bitmapDtor = Get<BitmapDtor>(ks, "??1Bitmap@Jet@@QAE@XZ");
    if (!pstringCache) {
        std::printf("PString cache not initialised\n");
        return 2;
    }
    FindFormat();
    for (void*& s : fakeVt) s = reinterpret_cast<void*>(&FTrap);
    fakeVt[0] = reinterpret_cast<void*>(&FLock);
    fakeVt[1] = reinterpret_cast<void*>(&FUnlock);
    fakeVt[3] = reinterpret_cast<void*>(&FIsAllLoaded);
    fakeVt[18] = reinterpret_cast<void*>(&FRender18);
    fakeVt[19] = reinterpret_cast<void*>(&FRender19);
    fakeVt[20] = reinterpret_cast<void*>(&FRender20);
    fakeVt[21] = reinterpret_cast<void*>(&FGetBitmap);
    fakeVt[22] = reinterpret_cast<void*>(&FGetArea);
    fakeVt[23] = reinterpret_cast<void*>(&FGetOffset);
    std::printf("32-bit bitmap format %d\n", format32);
    TestBase();
    TestRender(cases);
    std::printf("render: %llu checks, %llu mismatches\n", checks, bad);
    TestInstances(cases);
    std::printf("instances: %llu checks, %llu mismatches\n", checks, bad);
    TestBitmapResource(cases / 20);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
