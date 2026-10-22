// Differential test of KernelStandard unit U1 (Math.cpp, Eigen3.cpp, OptimalRects.cpp, Barycentric.cpp): our
// KernelStandard_7c.dll against KernelStandard_7c_orig.dll in one 32-bit process, through the raw exports.
// Results are compared as raw bits. The float functions return in st(0): they are called through a pointer
// returning double, which captures st(0) unrounded, exactly what an MSVC8 x87 caller would see.
//
// usage: diff_ks_math.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [quick]
#include <windows.h>

#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static HMODULE ours, orig;
static unsigned long long checks, mismatches;

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

static uint64_t DBits(double d)
{
    uint64_t b;
    std::memcpy(&b, &d, 8);
    return b;
}
static uint32_t FBits(float f)
{
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}
static float FFrom(uint32_t b)
{
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
static double DFrom(uint64_t b)
{
    double d;
    std::memcpy(&d, &b, 8);
    return d;
}
static bool IsNaN(uint64_t b) { return (b & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (b & 0xfffffffffffffull); }

struct Report {
    const char* name;
    unsigned long long n = 0, bad = 0, badNaN = 0;
    void Bad(bool nanInvolved, const std::string& what)
    {
        ++bad;
        if (nanInvolved) ++badNaN;
        if (bad - badNaN <= 5 && !nanInvolved) std::printf("  %s mismatch: %s\n", name, what.c_str());
    }
    void Done()
    {
        checks += n;
        mismatches += bad - badNaN;
        std::printf("%-28s %12llu checks, %llu mismatches", name, n, bad - badNaN);
        if (badNaN) std::printf(" (+%llu where only NaN bits differ)", badNaN);
        std::printf("\n");
    }
};

// ---------------------------------------------------------------------------------------------------------------

static void FloatFunctions(bool quick)
{
    using FF = double(__cdecl*)(float);  // float return in st(0), read unrounded
    const char* names[] = {"?Sqrt@StdMath@Jet@@SAMM@Z", "?Sqrt16@StdMath@Jet@@SAMM@Z", "?InvSqrt@StdMath@Jet@@SAMM@Z",
                           "?InvSqrt16@StdMath@Jet@@SAMM@Z", "?InvSqrtApprox@StdMath@Jet@@SAMM@Z"};
    const uint64_t step = quick ? 4099 : 1;
    for (const char* name : names) {
        Pair<FF> f(name);
        Report r{name};
        for (uint64_t i = 0; i <= 0xffffffffull; i += step) {
            const float x = FFrom(static_cast<uint32_t>(i));
            const uint64_t a = DBits(f.a(x)), b = DBits(f.b(x));
            ++r.n;
            if (a != b) {
                char buf[96];
                std::snprintf(buf, sizeof buf, "x=%08x ours %016llx orig %016llx", static_cast<uint32_t>(i), a, b);
                r.Bad(IsNaN(a) && IsNaN(b), buf);
            }
        }
        r.Done();
    }
    // integer results of a float
    using IF = int(__cdecl*)(float);
    for (const char* name : {"?IntLog2@Jet@@YAHM@Z", "?ILog2@Jet@@YAHM@Z"}) {
        Pair<IF> f(name);
        Report r{name};
        for (uint64_t i = 0; i <= 0xffffffffull; i += step) {
            const float x = FFrom(static_cast<uint32_t>(i));
            ++r.n;
            if (f.a(x) != f.b(x)) r.Bad(false, std::to_string(i));
        }
        r.Done();
    }
}

static void IntFunctions(bool quick)
{
    using II = int(__cdecl*)(int);
    const uint64_t step = quick ? 4099 : 1;
    for (const char* name : {"?ILog2@Jet@@YAHH@Z", "?CeilPow2@Jet@@YAHH@Z", "?FloorPow2@Jet@@YAHH@Z"}) {
        Pair<II> f(name);
        Report r{name};
        for (uint64_t i = 0; i <= 0xffffffffull; i += step) {
            const int x = static_cast<int>(static_cast<uint32_t>(i));
            ++r.n;
            if (f.a(x) != f.b(x)) r.Bad(false, std::to_string(x));
        }
        r.Done();
    }
}

static void DoubleFunctions()
{
    using DD = double(__cdecl*)(double);
    std::mt19937_64 rng(1);
    std::vector<double> in;
    for (uint64_t b : {0ull, 0x8000000000000000ull, 0x3ff0000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull,
                       0x7ff8000000000000ull, 0x0000000000000001ull, 0x000fffffffffffffull, 0x7fefffffffffffffull})
        in.push_back(DFrom(b));
    for (int i = 0; i < 4000000; ++i) {
        switch (i % 4) {
        case 0: in.push_back(DFrom(rng())); break;                                            // any bits
        case 1: in.push_back(std::ldexp(static_cast<double>(rng() >> 11), -53 + static_cast<int>(rng() % 120) - 60)); break;
        case 2: in.push_back(static_cast<double>(FFrom(static_cast<uint32_t>(rng())))); break;  // floats as doubles
        default: in.push_back(static_cast<double>(rng() % 100000) / 7.0);
        }
    }
    for (const char* name : {"?Sqrt@StdMath@Jet@@SANN@Z", "?InvSqrt@StdMath@Jet@@SANN@Z"}) {
        Pair<DD> f(name);
        Report r{name};
        for (double x : in) {
            const uint64_t a = DBits(f.a(x)), b = DBits(f.b(x));
            ++r.n;
            if (a != b) {
                char buf[96];
                std::snprintf(buf, sizeof buf, "x=%016llx ours %016llx orig %016llx", DBits(x), a, b);
                r.Bad(IsNaN(a) && IsNaN(b), buf);
            }
        }
        r.Done();
    }
}

static void CrcAndTables()
{
    using CrcF = unsigned long(__cdecl*)(const void*, unsigned long, unsigned long);
    Pair<CrcF> crc("?Calc@Crc@Jet@@SAKPBXKK@Z");
    Report r{"Crc::Calc"};
    std::mt19937 rng(7);
    std::vector<unsigned char> buf(70000);
    for (auto& b : buf) b = static_cast<unsigned char>(rng());
    for (int i = 0; i < 200000; ++i) {
        const unsigned long size = i < 1000 ? i : rng() % 70000;
        const unsigned long off = rng() % (70000 - size + 1);
        const unsigned long seed = (i % 3 == 0) ? 0 : (i % 3 == 1 ? 0xffffffffu : rng());
        ++r.n;
        if (crc.a(buf.data() + off, size, seed) != crc.b(buf.data() + off, size, seed)) r.Bad(false, std::to_string(i));
    }
    r.Done();

    Report t{"StdMath::s_sqrtLUT"};
    const void* la = Get<const void*>(ours, "?s_sqrtLUT@StdMath@Jet@@0PAIA");
    const void* lb = Get<const void*>(orig, "?s_sqrtLUT@StdMath@Jet@@0PAIA");
    t.n = 512;
    for (int i = 0; i < 512; ++i)
        if (static_cast<const uint32_t*>(la)[i] != static_cast<const uint32_t*>(lb)[i]) t.Bad(false, std::to_string(i));
    t.Done();
    std::printf("  (our table at %p, original at %p)\n", la, lb);
}

// thiscall emulated with __fastcall(this, edx, stack args...)
static void Vectors()
{
    using Ctor = void*(__fastcall*)(void*, void*, const void*);
    const char* names[] = {"??0Vector2@Jet@@QAE@ABVVector3@1@@Z", "??0Vector2@Jet@@QAE@ABVVector4@1@@Z",
                           "??0Vector3@Jet@@QAE@ABVVector2@1@@Z", "??0Vector3@Jet@@QAE@ABVVector4@1@@Z"};
    std::mt19937 rng(3);
    for (const char* name : names) {
        Pair<Ctor> f(name);
        Report r{name};
        for (int i = 0; i < 100000; ++i) {
            uint32_t src[4], a[4], b[4];
            for (auto& s : src) s = rng();
            if (i % 5 == 0) src[0] = 0x7f800001;  // signalling NaN: the original's x87 copy quietens it
            std::memset(a, 0xcd, sizeof a);
            std::memset(b, 0xcd, sizeof b);
            void* ra = f.a(a, nullptr, src);
            void* rb = f.b(b, nullptr, src);
            ++r.n;
            const bool sameRet = (ra == a) == (rb == b);
            if (std::memcmp(a, b, sizeof a) != 0 || !sameRet) {
                // tolerate exactly the SNaN -> QNaN difference
                bool onlySNaN = true;
                for (int k = 0; k < 4; ++k)
                    if (a[k] != b[k] && !((a[k] | 0x00400000u) == b[k] && (a[k] & 0x7f800000u) == 0x7f800000u)) onlySNaN = false;
                r.Bad(onlySNaN && sameRet, name);
            }
        }
        r.Done();
    }
    using Assign = void*(__fastcall*)(void*, void*, const void*);
    Pair<Assign> as("??4StdMath@Jet@@QAEAAV01@ABV01@@Z");
    char x = 0, y = 0;
    Report r{"StdMath::operator="};
    r.n = 1;
    if ((as.a(&x, nullptr, &y) == &x) != (as.b(&x, nullptr, &y) == &x)) r.Bad(false, "return");
    r.Done();
}

// ---------------------------------------------------------------------------------------------------------------
// Eigen3: 15 floats {m[3][3], diag[3], subd[3]}; members are thiscall

struct E3 {
    uint32_t f[15];
};

static float RandFloat(std::mt19937& rng, int kind)
{
    switch (kind) {
    case 0: return static_cast<float>(static_cast<int>(rng() % 21) - 10);                        // small integers
    case 1: return static_cast<float>(static_cast<int>(rng() % 2000001) - 1000000) / 1000.0f;    // decimals
    case 2: return std::ldexp(static_cast<float>(rng() % 1000000) / 1000000.0f - 0.5f, static_cast<int>(rng() % 60) - 30);
    case 3: return FFrom(rng());                                                                 // any bits
    default: return static_cast<float>(rng() % 3);                                               // 0, 1, 2
    }
}

static E3 RandomSymmetric(std::mt19937& rng, int i)
{
    E3 e;
    for (auto& v : e.f) v = rng();  // diag/subd start as garbage: Tridiagonal overwrites them
    const int kind = i % 7 == 6 ? 3 : i % 5;
    float m[3][3];
    for (int r = 0; r < 3; ++r)
        for (int c = r; c < 3; ++c) m[r][c] = m[c][r] = RandFloat(rng, kind);
    if (i % 4 == 1) m[0][2] = m[2][0] = 0.0f;                                // the c == 0 path
    if (i % 9 == 2) m[0][1] = m[1][0] = m[0][2] = m[2][0] = m[1][2] = m[2][1] = 0.0f;  // diagonal
    if (i % 11 == 3) for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) m[r][c] = (r == c) ? 2.5f : 0.0f;
    if (i % 13 == 4) m[0][2] = m[2][0] = -0.0f;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) e.f[r * 3 + c] = FBits(m[r][c]);
    return e;
}

static void Eigen()
{
    using Void = void(__fastcall*)(void*, void*);
    using Bool = bool(__fastcall*)(void*, void*);
    Pair<Void> sortAll("?IncreasingSort@Eigen3@Jet@@QAEXXZ");
    Pair<Void> tri("?Tridiagonal@Eigen3@Jet@@AAEXXZ");
    Pair<Bool> ql("?QLAlgorithm@Eigen3@Jet@@AAE_NXZ");
    Pair<Void> sortEigen("?IncreasingSortEigen@Eigen3@Jet@@AAEXXZ");
    std::mt19937 rng(11);
    Report r{"Eigen3 (all steps)"};
    unsigned long qlFailed = 0;
    for (int i = 0; i < 2000000; ++i) {
        const E3 in = RandomSymmetric(rng, i);
        E3 a = in, b = in;
        ++r.n;
        if (i % 2) {
            sortAll.a(&a, nullptr);
            sortAll.b(&b, nullptr);
        } else {  // step by step: also compares the intermediate states and QL's result
            tri.a(&a, nullptr);
            tri.b(&b, nullptr);
            const bool sameTri = std::memcmp(&a, &b, sizeof a) == 0;
            const E3 afterTri = b;
            const bool qa = ql.a(&a, nullptr), qb = ql.b(&b, nullptr);
            const bool same = sameTri && qa == qb && std::memcmp(&a, &b, sizeof a) == 0;
            if (!same && r.bad < 3) {
                std::printf("  input %d kind %d, differs after %s (ql %d/%d)\n   in :", i, i % 7 == 6 ? 3 : i % 5,
                            sameTri ? "QLAlgorithm" : "Tridiagonal", qa, qb);
                for (uint32_t v : (sameTri ? afterTri : in).f) std::printf(" %08x", v);
                std::printf("\n   our:");
                for (uint32_t v : a.f) std::printf(" %08x", v);
                std::printf("\n   org:");
                for (uint32_t v : b.f) std::printf(" %08x", v);
                std::printf("\n");
            }
            if (!qb) ++qlFailed;
            sortEigen.a(&a, nullptr);
            sortEigen.b(&b, nullptr);
            if (!same) {
                bool nanOnly = qa == qb;
                for (int k = 0; k < 15; ++k)
                    if (a.f[k] != b.f[k] && !(IsNaN(DBits(FFrom(a.f[k]))) && IsNaN(DBits(FFrom(b.f[k]))))) nanOnly = false;
                r.Bad(nanOnly, "intermediate state, input " + std::to_string(i));
                continue;
            }
        }
        if (std::memcmp(&a, &b, sizeof a) != 0) {
            bool nanOnly = true;
            for (int k = 0; k < 15; ++k)
                if (a.f[k] != b.f[k] && !(IsNaN(DBits(FFrom(a.f[k]))) && IsNaN(DBits(FFrom(b.f[k]))))) nanOnly = false;
            r.Bad(nanOnly, "result, input " + std::to_string(i));
        }
    }
    r.Done();
    std::printf("  (QL gave up after 32 iterations on %lu inputs, on both sides)\n", qlFailed);

    // accessors and assignment
    using Ref2 = float*(__fastcall*)(void*, void*, int, int);
    using Val2 = double(__fastcall*)(const void*, void*, int, int);
    using Ref1 = float*(__fastcall*)(void*, void*, int);
    using Val1 = double(__fastcall*)(const void*, void*, int);
    using Assign = void*(__fastcall*)(void*, void*, const void*);
    Pair<Ref2> ref2("??REigen3@Jet@@QAEAAMHH@Z"), matrix("?Matrix@Eigen3@Jet@@AAEAAMHH@Z");
    Pair<Val2> val2("??REigen3@Jet@@QBEMHH@Z");
    Pair<Ref1> ref1("??AEigen3@Jet@@QAEAAMH@Z");
    Pair<Val1> val1("??AEigen3@Jet@@QBEMH@Z");
    Pair<Assign> assign("??4Eigen3@Jet@@QAEAAV01@ABV01@@Z");
    Report acc{"Eigen3 accessors, operator="};
    for (int i = 0; i < 20000; ++i) {
        E3 e = RandomSymmetric(rng, i);
        for (int k = 0; k < 15; ++k) e.f[k] = (i % 5 == 0 && k == 4) ? 0x7f800001u : e.f[k];
        const int row = static_cast<int>(rng() % 3), col = static_cast<int>(rng() % 3), idx = static_cast<int>(rng() % 15);
        acc.n += 6;
        if (ref2.a(&e, nullptr, row, col) != ref2.b(&e, nullptr, row, col)) acc.Bad(false, "operator()");
        if (matrix.a(&e, nullptr, row, col) != matrix.b(&e, nullptr, row, col)) acc.Bad(false, "Matrix");
        if (ref1.a(&e, nullptr, idx) != ref1.b(&e, nullptr, idx)) acc.Bad(false, "operator[]");
        if (DBits(val2.a(&e, nullptr, row, col)) != DBits(val2.b(&e, nullptr, row, col))) acc.Bad(false, "operator() const");
        if (DBits(val1.a(&e, nullptr, idx)) != DBits(val1.b(&e, nullptr, idx))) acc.Bad(false, "operator[] const");
        E3 x{}, y{};
        const bool ra = assign.a(&x, nullptr, &e) == &x, rb = assign.b(&y, nullptr, &e) == &y;
        if (ra != rb || std::memcmp(&x, &y, sizeof x) != 0) acc.Bad(false, "operator=");
    }
    acc.Done();
}

// ---------------------------------------------------------------------------------------------------------------

static void Barycentric()
{
    using Bary = void*(__cdecl*)(void*, const void*, const void*, const void*, const void*, const void*);
    Pair<Bary> f("?GetBaryCentricWeights@Jet@@YA?AVVector3@1@ABV21@0000@Z");
    std::mt19937 rng(5);
    Report r{"GetBaryCentricWeights"};
    unsigned long degenerate = 0;
    for (int i = 0; i < 3000000; ++i) {
        float v[5][3];
        const int kind = i % 7 == 6 ? 3 : i % 3;
        for (auto& p : v)
            for (auto& c : p) c = RandFloat(rng, kind);
        if (i % 10 == 1)  // collinear triangle
            for (int c = 0; c < 3; ++c) v[2][c] = v[0][c] + (v[1][c] - v[0][c]) * 2.0f;
        if (i % 10 == 2)  // direction in the triangle's plane
            for (int c = 0; c < 3; ++c) v[4][c] = v[1][c] - v[0][c];
        if (i % 10 == 3)  // tiny triangle: determinant near +-1e-6
            for (int c = 0; c < 3; ++c) {
                v[1][c] = v[0][c] + RandFloat(rng, 2) * 1e-3f;
                v[2][c] = v[0][c] + RandFloat(rng, 2) * 1e-3f;
            }
        uint32_t a[3], b[3];
        std::memset(a, 0xcd, sizeof a);
        std::memset(b, 0xcd, sizeof b);
        void* ra = f.a(a, v[0], v[1], v[2], v[3], v[4]);
        void* rb = f.b(b, v[0], v[1], v[2], v[3], v[4]);
        ++r.n;
        if (b[0] == 0 && b[1] == 0 && b[2] == 0) ++degenerate;
        if ((ra == a) != (rb == b) || std::memcmp(a, b, sizeof a) != 0) {
            bool nanOnly = true;
            for (int k = 0; k < 3; ++k)
                if (a[k] != b[k] && !(IsNaN(DBits(FFrom(a[k]))) && IsNaN(DBits(FFrom(b[k]))))) nanOnly = false;
            char buf[128];
            std::snprintf(buf, sizeof buf, "input %d: ours %08x %08x %08x orig %08x %08x %08x", i, a[0], a[1], a[2], b[0], b[1], b[2]);
            r.Bad(nanOnly, buf);
        }
    }
    r.Done();
    std::printf("  (%lu inputs took the degenerate {0,0,0} path)\n", degenerate);
}

// ---------------------------------------------------------------------------------------------------------------
// OptimalRects: {set head, tail, count, pending head, tail, count}; nodes {x, y, w, h, next link, prev link}

struct ORect {
    int x, y, w, h;
};
struct ONode {
    ORect r;
    ONode* next;  // points at the next node's link (node + 0x10)
    ONode* prev;
};
struct ORects {
    ONode* head;
    ONode* tail;
    int count;
    ONode* pHead;
    ONode* pTail;
    int pCount;
};

static std::vector<int> Dump(const ORects& o)
{
    std::vector<int> v{o.count, o.pCount};
    for (ONode* n = o.head; n; n = n->next ? reinterpret_cast<ONode*>(reinterpret_cast<char*>(n->next) - 0x10) : nullptr) {
        v.insert(v.end(), {n->r.x, n->r.y, n->r.w, n->r.h});
        if (v.size() > 400000) break;
    }
    v.push_back(-9999);
    for (ONode* n = o.pHead; n; n = n->next ? reinterpret_cast<ONode*>(reinterpret_cast<char*>(n->next) - 0x10) : nullptr) {
        v.insert(v.end(), {n->r.x, n->r.y, n->r.w, n->r.h});
        if (v.size() > 800000) break;
    }
    return v;
}

static void Rects()
{
    using Inv = void(__cdecl*)(void*, const ORect*);
    using Proc = void(__cdecl*)(void*);
    using Comb = ORect*(__cdecl*)(void*, ORect*);
    using New = void*(__cdecl*)(unsigned int);
    Pair<Inv> inv("?Invalidate@OptimalRects@Jet@@QAAXABURect@2@@Z"), val("?Validate@OptimalRects@Jet@@QAAXABURect@2@@Z");
    Pair<Proc> proc("?Process@OptimalRects@Jet@@QAAXXZ");
    Pair<Comb> comb("?Combine@OptimalRects@Jet@@QAA?AURect@2@XZ");
    New alloc = Get<New>(orig, "?New@Alloc@Jet@@SAPAXI@Z");
    std::mt19937 rng(17);
    Report r{"OptimalRects"};
    unsigned long long maxSet = 0;
    for (int run = 0; run < 3000; ++run) {
        ORects a{}, b{};
        const int grid = run % 3 == 0 ? 16 : (run % 3 == 1 ? 64 : 400);
        const int steps = 5 + static_cast<int>(rng() % 120);
        for (int s = 0; s < steps; ++s) {
            ORect q{static_cast<int>(rng() % grid) - (rng() % 20 == 0 ? grid / 2 : 0), static_cast<int>(rng() % grid),
                    1 + static_cast<int>(rng() % (grid / 2 + 1)), 1 + static_cast<int>(rng() % (grid / 2 + 1))};
            if (rng() % 25 == 0) q.w = -q.w;
            const unsigned op = rng() % 20;
            if (op == 0) {  // pending rectangles queued by hand, then Process
                const int k = 1 + static_cast<int>(rng() % 4);
                for (int j = 0; j < k; ++j) {
                    ORect p{static_cast<int>(rng() % grid), static_cast<int>(rng() % grid), 1 + static_cast<int>(rng() % (grid / 2 + 1)),
                            1 + static_cast<int>(rng() % (grid / 2 + 1))};
                    for (ORects* o : {&a, &b}) {
                        auto* n = static_cast<ONode*>(alloc(sizeof(ONode)));
                        *n = ONode{p, nullptr, o->pTail ? reinterpret_cast<ONode*>(reinterpret_cast<char*>(o->pTail) + 0x10) : nullptr};
                        if (o->pTail)
                            o->pTail->next = reinterpret_cast<ONode*>(reinterpret_cast<char*>(n) + 0x10);
                        else
                            o->pHead = n;
                        o->pTail = n;
                        ++o->pCount;
                    }
                }
                proc.a(&a);
                proc.b(&b);
            } else if (op == 1) {
                val.a(&a, &q);
                val.b(&b, &q);
            } else {
                inv.a(&a, &q);
                inv.b(&b, &q);
            }
            ORect ca{}, cb{};
            ORect* ra = comb.a(&a, &ca);
            ORect* rb = comb.b(&b, &cb);
            ++r.n;
            if (Dump(a) != Dump(b) || std::memcmp(&ca, &cb, sizeof ca) != 0 || (ra == &ca) != (rb == &cb)) {
                r.Bad(false, "run " + std::to_string(run) + " step " + std::to_string(s));
                break;
            }
            if (static_cast<unsigned long long>(b.count) > maxSet) maxSet = b.count;
        }
        // the nodes stay allocated (both sides' lists are left as they are)
    }
    r.Done();
    std::printf("  (largest set: %llu rectangles)\n", maxSet);
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    const bool quick = argc > 2 && std::strcmp(argv[2], "quick") == 0;
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) {
        std::printf("load failed: ours=%p orig=%p err=%lu\n", static_cast<void*>(ours), static_cast<void*>(orig), GetLastError());
        return 2;
    }
    unsigned int cw = 0;
    __control87_2(0, 0, &cw, nullptr);
    std::printf("x87 control word %04x (precision %s)\n", cw,
                (cw & _MCW_PC) == _PC_53 ? "53-bit" : (cw & _MCW_PC) == _PC_64 ? "64-bit" : "24-bit");
    CrcAndTables();
    Vectors();
    Rects();
    Barycentric();
    Eigen();
    DoubleFunctions();
    IntFunctions(quick);
    FloatFunctions(quick);
    std::printf("%llu checks, %llu mismatches\n", checks, mismatches);
    return mismatches ? 1 : 0;
}
