// Differential test of KernelStandard unit U3 (SmallTypes.cpp): our KernelStandard_7c.dll against
// KernelStandard_7c_orig.dll in one 32-bit process, through the raw exports; results compared as raw bytes.
//
// usage: diff_ks_small.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static HMODULE ours, orig;
static unsigned long long totalChecks, totalBad;

template <class F>
static F Get(HMODULE m, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s in %s\n", name, m == ours ? "ours" : "orig");
        std::exit(2);
    }
    return p;
}

template <class F>
struct Pair {
    F a, b;
    explicit Pair(const char* name) : a(Get<F>(ours, name)), b(Get<F>(orig, name)) {}
};

struct Report {
    const char* name;
    unsigned long long n = 0, bad = 0;
    void Check(bool ok, const std::string& what)
    {
        ++n;
        if (ok) return;
        if (++bad <= 5) std::printf("  %s mismatch: %s\n", name, what.c_str());
    }
    ~Report()
    {
        totalChecks += n;
        totalBad += bad;
        std::printf("%-34s %10llu checks, %llu mismatches\n", name, n, bad);
    }
};

static uint32_t Bits(float f)
{
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}
static float FromBits(uint32_t b)
{
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
static uint64_t DBits(double d)
{
    uint64_t b;
    std::memcpy(&b, &d, 8);
    return b;
}

static float RandFloat(std::mt19937& rng)
{
    switch (rng() % 5) {
    case 0: return static_cast<float>(rng() % 1001) / 1000.0f;  // [0, 1]
    case 1: return static_cast<float>(static_cast<int>(rng() % 4001) - 2000) / 1000.0f;
    case 2: return static_cast<float>(rng() % 7) / 6.0f;
    case 3: return FromBits(rng());
    default: return static_cast<float>(rng() % 3);
    }
}

// thiscall emulated with __fastcall(this, edx, stack args...)
using Ctor0 = void*(__fastcall*)(void*, void*);
using Ctor1 = void*(__fastcall*)(void*, void*, const void*);
using Get0 = void*(__fastcall*)(const void*, void*);

static void Nodes(std::mt19937& rng)
{
    {
        Report r{"node constructors / operator="};
        const struct {
            const char* ctor;
            const char* assign;
            size_t size;
        } kinds[] = {{"??0DLListNode@Jet@@QAE@XZ", "??4DLListNode@Jet@@QAEAAU01@ABU01@@Z", 8},
                     {"??0IListNode@Jet@@QAE@XZ", "??4IListNode@Jet@@QAEAAU01@ABU01@@Z", 8},
                     {"??0ISingleListNode@Jet@@QAE@XZ", "??4ISingleListNode@Jet@@QAEAAU01@ABU01@@Z", 4},
                     {"??0DLTreeNode@Jet@@QAE@XZ", "??4DLTreeNode@Jet@@QAEAAU01@ABU01@@Z", 0x18},
                     {"??0DynamicPtr@Jet@@QAE@XZ", "??4DynamicPtr@Jet@@QAEAAV01@ABV01@@Z", 8},
                     {"??0ProtectedLong@Jet@@QAE@XZ", "??4ProtectedLong@Jet@@QAEAAV01@ABV01@@Z", 4},
                     {"??0ImageProcessing@Jet@@QAE@XZ", "??4ImageProcessing@Jet@@QAEAAV01@ABV01@@Z", 256}};
        for (const auto& k : kinds) {
            Pair<Ctor0> c(k.ctor);
            Pair<Ctor1> as(k.assign);
            for (int i = 0; i < 200; ++i) {
                unsigned char a[260], b[260], src[260];
                for (auto& x : src) x = static_cast<unsigned char>(rng());
                std::memset(a, 0xcd, sizeof a);
                std::memset(b, 0xcd, sizeof b);
                const bool ra = c.a(a, nullptr) == a, rb = c.b(b, nullptr) == b;
                r.Check(ra == rb && std::memcmp(a, b, sizeof a) == 0, k.ctor);
                const bool sa = as.a(a, nullptr, src) == a, sb = as.b(b, nullptr, src) == b;
                r.Check(sa == sb && std::memcmp(a, b, sizeof a) == 0, k.assign);
            }
        }
    }
    {
        // a random forest of DLTreeNodes, every accessor on every node
        Report r{"DLTreeNode / DLListNode accessors"};
        const char* names[] = {"?Count@DLTreeNode@Jet@@QBEIXZ",      "?Next@DLTreeNode@Jet@@QBEPAU12@XZ",
                               "?Prev@DLTreeNode@Jet@@QBEPAU12@XZ",  "?Child@DLTreeNode@Jet@@QBEPAU12@XZ",
                               "?FirstChild@DLTreeNode@Jet@@QBEPAU12@XZ", "?LastChild@DLTreeNode@Jet@@QBEPAU12@XZ",
                               "?Parent@DLTreeNode@Jet@@QBEPAU12@XZ", "?WalkNext@DLTreeNode@Jet@@QBEPAU12@XZ",
                               "?WalkPrev@DLTreeNode@Jet@@QBEPAU12@XZ", "?NextNode@DLTreeNode@Jet@@QBEPAU12@XZ",
                               "?Walk@DLTreeNode@Jet@@QBEPAU12@XZ",  "?Next@DLListNode@Jet@@QBEPAU12@XZ",
                               "?Prev@DLListNode@Jet@@QBEPAU12@XZ",  "?Left@RBTreeNode@Jet@@QBEPAU12@XZ",
                               "?Right@RBTreeNode@Jet@@QBEPAU12@XZ"};
        std::vector<Pair<Get0>> fs;
        for (const char* n : names) fs.emplace_back(n);
        struct T {
            uint32_t count;
            T *next, *prev, *first, *last, *parent;
        };
        for (int run = 0; run < 300; ++run) {
            std::vector<T> nodes(1 + rng() % 60);
            for (auto& n : nodes) n = T{};
            for (size_t i = 1; i < nodes.size(); ++i) {  // attach to a random earlier node
                T* p = &nodes[rng() % i];
                T* c = &nodes[i];
                c->parent = p;
                if (p->last) {
                    p->last->next = c;
                    c->prev = p->last;
                } else {
                    p->first = c;
                }
                p->last = c;
                ++p->count;
            }
            for (auto& n : nodes)
                for (size_t k = 0; k < fs.size(); ++k) r.Check(fs[k].a(&n, nullptr) == fs[k].b(&n, nullptr), names[k]);
        }
    }
    {
        Report r{"RBTreeNode"};
        Pair<Ctor0> ctor("??0RBTreeNode@Jet@@QAE@XZ");
        Pair<Ctor1> copy("??0RBTreeNode@Jet@@QAE@ABU01@@Z"), assign("??4RBTreeNode@Jet@@QAEAAU01@ABU01@@Z");
        Pair<Ctor0> dtor("??1RBTreeNode@Jet@@UAE@XZ");
        const void* vtA = Get<const void*>(ours, "??_7RBTreeNode@Jet@@6B@");
        const void* vtB = Get<const void*>(orig, "??_7RBTreeNode@Jet@@6B@");
        using New = void*(__cdecl*)(unsigned int);
        New alloc = Get<New>(orig, "?New@Alloc@Jet@@SAPAXI@Z");
        for (int i = 0; i < 2000; ++i) {
            uint32_t a[4], b[4], src[4];
            for (auto& x : src) x = rng();
            std::memset(a, 0xcd, sizeof a);
            std::memset(b, 0xcd, sizeof b);
            ctor.a(a, nullptr);
            ctor.b(b, nullptr);
            r.Check(a[0] == reinterpret_cast<uintptr_t>(vtA) && b[0] == reinterpret_cast<uintptr_t>(vtB) &&
                        std::memcmp(a + 1, b + 1, 12) == 0,
                    "ctor");
            assign.a(a, nullptr, src);
            assign.b(b, nullptr, src);
            r.Check(a[0] == reinterpret_cast<uintptr_t>(vtA) && std::memcmp(a + 1, b + 1, 12) == 0, "operator=");
            copy.a(a, nullptr, src);
            copy.b(b, nullptr, src);
            r.Check(a[0] == reinterpret_cast<uintptr_t>(vtA) && std::memcmp(a + 1, b + 1, 12) == 0, "copy ctor");
            dtor.a(a, nullptr);
            dtor.b(b, nullptr);
            r.Check(a[0] == reinterpret_cast<uintptr_t>(vtA) && b[0] == reinterpret_cast<uintptr_t>(vtB), "dtor");
            // scalar deleting destructor through slot 0 of each side's vftable, on Alloc memory
            using Del = void*(__fastcall*)(void*, void*, unsigned int);
            for (int side = 0; side < 2; ++side) {
                void* m = alloc(16);
                (side ? ctor.b : ctor.a)(m, nullptr);
                Del del = static_cast<const Del*>(side ? vtB : vtA)[0];
                r.Check(del(m, nullptr, 1) == m, "deleting dtor");
            }
        }
    }
}

static void Scalars(std::mt19937& rng)
{
    Report r{"ProtectedLong, DynamicPtr, misc"};
    using CtorL = void*(__fastcall*)(void*, void*, long);
    using CdeclL = long(__cdecl*)(void*);
    Pair<CtorL> ctorL("??0ProtectedLong@Jet@@QAE@J@Z");
    Pair<Ctor1> copy("??0ProtectedLong@Jet@@QAE@ABV01@@Z"), dynCtor("??0DynamicPtr@Jet@@QAE@PAX@Z");
    Pair<CdeclL> getData("?GetData@ProtectedLong@Jet@@QBAJXZ"), inc("?Increment@ProtectedLong@Jet@@QAAJXZ"),
        dec("?Decrement@ProtectedLong@Jet@@QAAJXZ");
    Pair<Ctor0> rewind("?Rewind@DynamicPtr@Jet@@QAEXXZ"), dtorPL("??1ProtectedLong@Jet@@QAE@XZ");
    using IncPtr = void*(__fastcall*)(const void*, void*, void*, unsigned long);
    Pair<IncPtr> incPtr("?IncPtr@DynamicPtr@Jet@@IBEPAXPAXK@Z");
    using Static = void(__cdecl*)();
    Pair<Static> init("?Init@ProtectedLong@Jet@@SAXXZ"), done("?Done@ProtectedLong@Jet@@SAXXZ");
    init.a();
    init.b();
    done.a();
    done.b();
    for (int i = 0; i < 100000; ++i) {
        const long v = static_cast<long>(rng());
        long a = 0x55, b = 0x55;
        ctorL.a(&a, nullptr, v);
        ctorL.b(&b, nullptr, v);
        r.Check(a == b, "ProtectedLong(long)");
        r.Check(getData.a(&a) == getData.b(&b), "GetData");
        r.Check(inc.a(&a) == inc.b(&b) && a == b, "Increment");
        r.Check(dec.a(&a) == dec.b(&b) && a == b, "Decrement");
        long c = 0, d = 0;
        copy.a(&c, nullptr, &a);
        copy.b(&d, nullptr, &b);
        r.Check(c == d, "copy");
        dtorPL.a(&c, nullptr);
        dtorPL.b(&d, nullptr);
        r.Check(c == d, "~ProtectedLong");
        uint32_t pa[2] = {1, 2}, pb[2] = {1, 2};
        void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(rng()));
        dynCtor.a(pa, nullptr, p);
        dynCtor.b(pb, nullptr, p);
        r.Check(std::memcmp(pa, pb, 8) == 0, "DynamicPtr(void*)");
        pa[1] = pb[1] = rng();
        rewind.a(pa, nullptr);
        rewind.b(pb, nullptr);
        r.Check(std::memcmp(pa, pb, 8) == 0, "Rewind");
        const unsigned long n = rng();
        r.Check(incPtr.a(pa, nullptr, p, n) == incPtr.b(pb, nullptr, p, n), "IncPtr");
    }
    // empty classes' operator=, statics
    for (const char* name : {"??4BuildVersion@Jet@@QAEAAV01@ABV01@@Z", "??4Date@Jet@@QAEAAV01@ABV01@@Z",
                             "??4MousePointer@Jet@@QAEAAV01@ABV01@@Z"}) {
        Pair<Ctor1> f(name);
        char x = 0, y = 0;
        r.Check((f.a(&x, nullptr, &y) == &x) == (f.b(&x, nullptr, &y) == &x), name);
    }
    using Str = const char*(__cdecl*)();
    for (const char* name : {"?GetJetBuildVersionString@BuildVersion@Jet@@SAPBDXZ", "?GetJetBuildString@BuildVersion@Jet@@SAPBDXZ",
                             "?GetJetBuildChangeNumString@BuildVersion@Jet@@SAPBDXZ"}) {
        Pair<Str> f(name);
        r.Check(std::strcmp(f.a(), f.b()) == 0, name);
    }
    using FloatRet = double(__cdecl*)();  // float in st(0)
    Pair<FloatRet> ver("?GetJetBuildVersion@BuildVersion@Jet@@SAMXZ");
    r.Check(DBits(ver.a()) == DBits(ver.b()), "GetJetBuildVersion");
    using DateF = void(__cdecl*)(char*);
    for (const char* name : {"?Str@Date@Jet@@SAXPAD@Z", "?YYYYMMDD@Date@Jet@@SAXPAD@Z"}) {
        Pair<DateF> f(name);
        char a[32], b[32];
        std::memset(a, 0x7e, sizeof a);
        std::memset(b, 0x7e, sizeof b);
        f.a(a);
        f.b(b);
        r.Check(std::memcmp(a, b, sizeof a) == 0, name);
    }
    Pair<Static> hide("?Hide@MousePointer@Jet@@SAXXZ"), show("?Show@MousePointer@Jet@@SAXXZ");
    const int before = ShowCursor(TRUE) - 1;
    ShowCursor(FALSE);
    hide.a();
    const int afterA = ShowCursor(TRUE) - 1;
    ShowCursor(FALSE);
    show.a();
    hide.b();
    const int afterB = ShowCursor(TRUE) - 1;
    ShowCursor(FALSE);
    show.b();
    r.Check(afterA == before - 1 && afterB == before - 1, "MousePointer Hide/Show");
    for (const char* name : {"?black@PackedColor@Jet@@2U12@B", "?blue@PackedColor@Jet@@2U12@B", "?cyan@PackedColor@Jet@@2U12@B",
                             "?darkGray@PackedColor@Jet@@2U12@B", "?gray@PackedColor@Jet@@2U12@B", "?green@PackedColor@Jet@@2U12@B",
                             "?lightGray@PackedColor@Jet@@2U12@B", "?magenta@PackedColor@Jet@@2U12@B",
                             "?orange@PackedColor@Jet@@2U12@B", "?pink@PackedColor@Jet@@2U12@B", "?red@PackedColor@Jet@@2U12@B",
                             "?white@PackedColor@Jet@@2U12@B", "?yellow@PackedColor@Jet@@2U12@B"})
        r.Check(*Get<const uint32_t*>(ours, name) == *Get<const uint32_t*>(orig, name), name);
}

static void Colors(std::mt19937& rng)
{
    Report r{"Color / HSVColor Convert"};
    using ToRgb = void*(__fastcall*)(void*, void*, const void*);
    using ToHsv = void*(__cdecl*)(void*, const void*);
    Pair<ToRgb> toRgb("?Convert@Color@Jet@@QAEAAV12@ABVHSVColor@2@@Z");
    Pair<ToHsv> toHsv("?Convert@HSVColor@Jet@@QAAAAV12@ABVColor@2@@Z");
    for (int i = 0; i < 5000000; ++i) {
        float in[4];
        for (auto& f : in) f = RandFloat(rng);
        if (i % 7 == 0) in[1] = 0.0f;
        if (i % 11 == 0) in[0] = 1.0f;
        if (i % 13 == 0) in[2] = in[0];
        uint32_t a[4], b[4];
        for (int k = 0; k < 4; ++k) a[k] = b[k] = rng();  // untouched fields must stay as they were
        const bool ra = toRgb.a(a, nullptr, in) == a, rb = toRgb.b(b, nullptr, in) == b;
        r.Check(ra == rb && std::memcmp(a, b, sizeof a) == 0, "Color::Convert input " + std::to_string(i));
        for (int k = 0; k < 4; ++k) a[k] = b[k] = rng();
        const bool ha = toHsv.a(a, in) == a, hb = toHsv.b(b, in) == b;
        r.Check(ha == hb && std::memcmp(a, b, sizeof a) == 0, "HSVColor::Convert input " + std::to_string(i));
    }
}

static void Images(std::mt19937& rng)
{
    Report r{"ImageProcessing"};
    using SetF = void(__fastcall*)(void*, void*, float);
    using Proc = bool(__fastcall*)(void*, void*, void*);
    using Adjust = bool(__fastcall*)(void*, void*, float, void*);
    Pair<Ctor0> ctor("??0ImageProcessing@Jet@@QAE@XZ"), ident("?SetupIdentity@ImageProcessing@Jet@@QAEXXZ"),
        dtor("??1ImageProcessing@Jet@@QAE@XZ");
    Pair<SetF> bright("?SetupBrightness@ImageProcessing@Jet@@QAEXM@Z"), contrast("?SetupContrast@ImageProcessing@Jet@@QAEXM@Z");
    Pair<Proc> process("?Process@ImageProcessing@Jet@@QAE_NAAVBitmap@2@@Z");
    Pair<Adjust> adjust("?AdjustColor@ImageProcessing@Jet@@QAE_NMAAVBitmap@2@@Z");
    struct Bm {
        uint32_t w, h, pad8;
        unsigned char* data;
        uint32_t pitch, pad14;
        int format;
    };
    for (int i = 0; i < 20000; ++i) {
        unsigned char ta[256], tb[256];
        ctor.a(ta, nullptr);
        ctor.b(tb, nullptr);
        for (int s = 0; s < 3; ++s) {
            const float v = (rng() % 4 == 0) ? FromBits(rng()) : static_cast<float>(static_cast<int>(rng() % 3001) - 500) / 1000.0f;
            if (rng() % 2) {
                bright.a(ta, nullptr, v);
                bright.b(tb, nullptr, v);
            } else {
                contrast.a(ta, nullptr, v);
                contrast.b(tb, nullptr, v);
            }
            r.Check(std::memcmp(ta, tb, 256) == 0, "Setup");
        }
        const uint32_t w = 1 + rng() % 9, h = 1 + rng() % 5, pitch = w * 4 + (rng() % 3) * 4;
        std::vector<unsigned char> pa(pitch * h), pb;
        for (auto& x : pa) x = static_cast<unsigned char>(rng());
        pb = pa;
        Bm ba{w, h, 0, pa.data(), pitch, 0, (rng() % 6) ? 2 : 1}, bb = ba;
        bb.data = pb.data();
        if (rng() % 2) {
            r.Check(process.a(ta, nullptr, &ba) == process.b(tb, nullptr, &bb) && pa == pb, "Process");
        } else {
            const float sat = (rng() % 5 == 0) ? FromBits(rng()) : static_cast<float>(static_cast<int>(rng() % 4001) - 1000) / 1000.0f;
            r.Check(adjust.a(ta, nullptr, sat, &ba) == adjust.b(tb, nullptr, sat, &bb) && pa == pb, "AdjustColor");
        }
        ident.a(ta, nullptr);
        ident.b(tb, nullptr);
        dtor.a(ta, nullptr);
        dtor.b(tb, nullptr);
        r.Check(std::memcmp(ta, tb, 256) == 0, "SetupIdentity");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) {
        std::printf("load failed\n");
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::mt19937 rng(42);
    Nodes(rng);
    Scalars(rng);
    Colors(rng);
    Images(rng);
    std::printf("%llu checks, %llu mismatches\n", totalChecks, totalBad);
    return totalBad ? 1 : 0;
}
