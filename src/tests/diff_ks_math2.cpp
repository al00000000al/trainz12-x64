// Differential test of KernelStandard unit U2 (Math2.cpp): Matrix3, Matrix4 and Quaternion members of our
// KernelStandard_7c.dll against KernelStandard_7c_orig.dll in one process. Every member runs on the same random
// inputs in both modules (angles, matrices, quaternions and vectors from several distributions, with special
// values: +-0, +-1, infinities, NaNs, denormals; plus inputs aimed at each branch) and the whole output object is
// compared as raw bits; GetDeterminant is compared as the unrounded double in st(0).
//
// usage: diff_ks_math2.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [cases per member]
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

static HMODULE ours, orig;
static unsigned long long checks, bad, nanOnly;
static int badHere;

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

static std::mt19937 rng(12345);

static float Uniform(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
static int Pick(int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); }

static const uint32_t kSpecial[] = {
    0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x3f000000, 0x40000000, 0x7f800000, 0xff800000,
    0x7fc00000, 0xffc00000, 0x7fa00001, 0x00000001, 0x80000001, 0x007fffff, 0x7f7fffff, 0xff7fffff,
    0x358637bd, 0x3a83126f, 0x3f7fff58, 0xbf7fff58, 0x3fc90fdb, 0x40490fdb, 0x5f000000, 0x1e3ce508,
};

// one float of the given distribution: 0 [-1,1], 1 [-8,8], 2 [-1000,1000], 3 random bits; 2% special values
static float Value(int mode)
{
    if (Pick(50) == 0) return FromBits(kSpecial[Pick(sizeof kSpecial / 4)]);
    switch (mode) {
    case 0: return Uniform(-1.0f, 1.0f);
    case 1: return Uniform(-8.0f, 8.0f);
    case 2: return Uniform(-1000.0f, 1000.0f);
    default: return FromBits(static_cast<uint32_t>(rng()));
    }
}

static int Mode() { return Pick(10) < 9 ? Pick(3) : 3; }

static void Fill(float* v, int n, int mode)
{
    for (int i = 0; i < n; ++i) v[i] = Value(mode);
}

static void UnitQuat(float* q)
{
    double s = 0;
    for (int i = 0; i < 4; ++i) {
        q[i] = Uniform(-1.0f, 1.0f);
        s += double(q[i]) * q[i];
    }
    const float inv = static_cast<float>(1.0 / std::sqrt(s > 0 ? s : 1.0));
    for (int i = 0; i < 4; ++i) q[i] *= inv;
}

static void Report(const char* what, const float* in, int nIn, const void* a, const void* b, int nOut)
{
    ++bad;
    if (++badHere > 5) return;
    std::printf("  mismatch %s\n    in :", what);
    for (int i = 0; i < nIn; ++i) std::printf(" %08x", Bits(in[i]));
    std::printf("\n    our:");
    for (int i = 0; i < nOut; ++i) std::printf(" %08x", static_cast<const uint32_t*>(a)[i]);
    std::printf("\n    org:");
    for (int i = 0; i < nOut; ++i) std::printf(" %08x", static_cast<const uint32_t*>(b)[i]);
    std::printf("\n");
}

static bool IsNaN(uint32_t b) { return (b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0; }

// a difference only in which NaN comes out: when two NaNs meet, x87 and SSE (and the operand order of a
// commutative SSE operation) pick different payloads. Counted apart and not reported. For the double of
// GetDeterminant (nOut == 2) the halves are not floats: compared as a whole.
static bool NaNOnly(const void* a, const void* b, int nOut)
{
    if (nOut == 2) {
        double x, y;
        std::memcpy(&x, a, 8);
        std::memcpy(&y, b, 8);
        return x != x && y != y;
    }
    for (int i = 0; i < nOut; ++i) {
        const uint32_t x = static_cast<const uint32_t*>(a)[i], y = static_cast<const uint32_t*>(b)[i];
        if (x != y && !(IsNaN(x) && IsNaN(y))) return false;
    }
    return true;
}

static void Compare(const char* what, const float* in, int nIn, const void* a, const void* b, int nOut)
{
    ++checks;
    if (std::memcmp(a, b, nOut * 4) == 0) return;
    if (NaNOnly(a, b, nOut)) {
        ++nanOnly;
        return;
    }
    Report(what, in, nIn, a, b, nOut);
}

// thiscall emulated with __fastcall(this, edx, stack args); Matrix4 members are __cdecl(this, args)
using T_F = void(__fastcall*)(void*, void*, float);
using T_FFF = void(__fastcall*)(void*, void*, float, float, float);
using T_P = void(__fastcall*)(void*, void*, const void*);
using T_PF = void*(__fastcall*)(void*, void*, const void*, float);
using T_PP = void*(__fastcall*)(void*, void*, const void*, const void*);
using T_PPF = void*(__fastcall*)(void*, void*, const void*, const void*, float);
using T_Ret = void*(__fastcall*)(const void*, void*, void*);
using C_0 = void*(__cdecl*)(void*);
using C_D = double(__cdecl*)(const void*);
using C_F = void(__cdecl*)(void*, float);
using C_FB = void(__cdecl*)(void*, float, bool);
using C_FFF = void(__cdecl*)(void*, float, float, float);
using C_PF = void(__cdecl*)(void*, const void*, float);
using C_P = void(__cdecl*)(void*, const void*);
using C_PP = void(__cdecl*)(void*, const void*, const void*);

static int cases = 200000;

static void Begin(const char* name)
{
    badHere = 0;
    std::printf("%s\n", name);
}

// a matrix: random entries, or a rotation (from the original's SetRotFromQuaternion) with some noise
static void Matrix(float* m, int n, int mode)
{
    static const Pair<C_P> fromQ("?SetRotFromQuaternion@Matrix4@Jet@@QAAXABVQuaternion@2@@Z");
    if (Pick(3) == 0) {
        float q[4], m4[16];
        UnitQuat(q);
        fromQ.b(m4, q);
        for (int i = 0; i < n; ++i) m[i] = n == 9 ? m4[(i / 3) * 4 + i % 3] : m4[i];
        if (Pick(2)) m[Pick(n)] += Uniform(-1e-3f, 1e-3f);
        if (Pick(4) == 0) m[Pick(n)] = Value(mode);
    } else {
        Fill(m, n, mode);
    }
}

static void TestMatrix3()
{
    struct One {
        const char* name;
        Pair<T_F> p;
    };
    One ones[] = {{"Matrix3::RotateX", Pair<T_F>("?RotateX@Matrix3@Jet@@QAEXM@Z")},
                  {"Matrix3::RotateY", Pair<T_F>("?RotateY@Matrix3@Jet@@QAEXM@Z")},
                  {"Matrix3::RotateZ", Pair<T_F>("?RotateZ@Matrix3@Jet@@QAEXM@Z")},
                  {"Matrix3::SetRotateX", Pair<T_F>("?SetRotateX@Matrix3@Jet@@QAEXM@Z")},
                  {"Matrix3::SetRotateY", Pair<T_F>("?SetRotateY@Matrix3@Jet@@QAEXM@Z")},
                  {"Matrix3::SetRotateZ", Pair<T_F>("?SetRotateZ@Matrix3@Jet@@QAEXM@Z")}};
    for (One& o : ones) {
        Begin(o.name);
        for (int c = 0; c < cases; ++c) {
            float in[10], a[9], b[9];
            const int mode = Mode();
            Matrix(in, 9, mode);
            in[9] = Value(Pick(4));
            std::memcpy(a, in, 36);
            std::memcpy(b, in, 36);
            o.p.a(a, nullptr, in[9]);
            o.p.b(b, nullptr, in[9]);
            Compare(o.name, in, 10, a, b, 9);
        }
    }
    Begin("Matrix3::Rotate");
    const Pair<T_FFF> rot("?Rotate@Matrix3@Jet@@QAEXMMM@Z");
    for (int c = 0; c < cases; ++c) {
        float in[12], a[9], b[9];
        const int mode = Mode();
        Matrix(in, 9, mode);
        for (int i = 9; i < 12; ++i) in[i] = Pick(4) == 0 ? (Pick(2) ? 0.0f : -0.0f) : Value(Pick(4));
        std::memcpy(a, in, 36);
        std::memcpy(b, in, 36);
        rot.a(a, nullptr, in[9], in[10], in[11]);
        rot.b(b, nullptr, in[9], in[10], in[11]);
        Compare("Matrix3::Rotate", in, 12, a, b, 9);
    }
}

static void TestMatrix4()
{
    {
        Begin("Matrix4::Invert");
        const Pair<C_0> inv("?Invert@Matrix4@Jet@@QAAAAV12@XZ");
        for (int c = 0; c < cases; ++c) {
            float in[16], a[16], b[16];
            Matrix(in, 16, Mode());
            std::memcpy(a, in, 64);
            std::memcpy(b, in, 64);
            void* ra = inv.a(a);
            void* rb = inv.b(b);
            Compare("Matrix4::Invert", in, 16, a, b, 16);
            if (ra != a || rb != b) Report("Matrix4::Invert return", in, 0, &ra, &rb, 1);
        }
    }
    {
        Begin("Matrix4::GetDeterminant");
        const Pair<C_D> det("?GetDeterminant@Matrix4@Jet@@QBAMXZ");
        for (int c = 0; c < cases; ++c) {
            float in[16];
            Matrix(in, 16, Mode());
            const double a = det.a(in), b = det.b(in);
            Compare("Matrix4::GetDeterminant", in, 16, &a, &b, 2);
        }
    }
    struct One {
        const char* name;
        Pair<C_F> p;
    };
    One ones[] = {{"Matrix4::RotateX", Pair<C_F>("?RotateX@Matrix4@Jet@@QAAXM@Z")},
                  {"Matrix4::RotateY", Pair<C_F>("?RotateY@Matrix4@Jet@@QAAXM@Z")},
                  {"Matrix4::RotateZ", Pair<C_F>("?RotateZ@Matrix4@Jet@@QAAXM@Z")}};
    for (One& o : ones) {
        Begin(o.name);
        for (int c = 0; c < cases; ++c) {
            float in[17], a[16], b[16];
            Matrix(in, 16, Mode());
            in[16] = Value(Pick(4));
            std::memcpy(a, in, 64);
            std::memcpy(b, in, 64);
            o.p.a(a, in[16]);
            o.p.b(b, in[16]);
            Compare(o.name, in, 17, a, b, 16);
        }
    }
    struct Set {
        const char* name;
        Pair<C_FB> p;
    };
    Set sets[] = {{"Matrix4::SetRotateX", Pair<C_FB>("?SetRotateX@Matrix4@Jet@@QAAXM_N@Z")},
                  {"Matrix4::SetRotateY", Pair<C_FB>("?SetRotateY@Matrix4@Jet@@QAAXM_N@Z")},
                  {"Matrix4::SetRotateZ", Pair<C_FB>("?SetRotateZ@Matrix4@Jet@@QAAXM_N@Z")}};
    for (Set& o : sets) {
        Begin(o.name);
        for (int c = 0; c < cases; ++c) {
            float in[17], a[16], b[16];
            Fill(in, 16, 3);
            in[16] = Value(Pick(4));
            const bool keep = Pick(2) != 0;
            std::memcpy(a, in, 64);
            std::memcpy(b, in, 64);
            o.p.a(a, in[16], keep);
            o.p.b(b, in[16], keep);
            Compare(o.name, in, 17, a, b, 16);
        }
    }
    {
        Begin("Matrix4::Rotate");
        const Pair<C_FFF> rot("?Rotate@Matrix4@Jet@@QAAXMMM@Z");
        for (int c = 0; c < cases; ++c) {
            float in[19], a[16], b[16];
            Matrix(in, 16, Mode());
            for (int i = 16; i < 19; ++i) in[i] = Pick(4) == 0 ? (Pick(2) ? 0.0f : -0.0f) : Value(Pick(4));
            std::memcpy(a, in, 64);
            std::memcpy(b, in, 64);
            rot.a(a, in[16], in[17], in[18]);
            rot.b(b, in[16], in[17], in[18]);
            Compare("Matrix4::Rotate", in, 19, a, b, 16);
        }
    }
    {
        Begin("Matrix4::SetRotAxisAngle");
        const Pair<C_PF> f("?SetRotAxisAngle@Matrix4@Jet@@QAAXABVVector3@2@M@Z");
        for (int c = 0; c < cases; ++c) {
            float in[20], a[16], b[16];
            Fill(in, 16, 3);
            Fill(in + 16, 4, Mode());
            std::memcpy(a, in, 64);
            std::memcpy(b, in, 64);
            f.a(a, in + 16, in[19]);
            f.b(b, in + 16, in[19]);
            Compare("Matrix4::SetRotAxisAngle", in, 20, a, b, 16);
        }
    }
    {
        Begin("Matrix4::SetRotFromQuaternion / SetFromPosRot");
        const Pair<C_P> fq("?SetRotFromQuaternion@Matrix4@Jet@@QAAXABVQuaternion@2@@Z");
        const Pair<C_PP> fp("?SetFromPosRot@Matrix4@Jet@@QAAXABVVector3@2@ABVQuaternion@2@@Z");
        for (int c = 0; c < cases; ++c) {
            float in[23], a[16], b[16];
            Fill(in, 16, 3);
            const int mode = Mode();
            if (Pick(2)) UnitQuat(in + 16);
            else Fill(in + 16, 4, mode);
            Fill(in + 20, 3, mode);
            std::memcpy(a, in, 64);
            std::memcpy(b, in, 64);
            fq.a(a, in + 16);
            fq.b(b, in + 16);
            Compare("Matrix4::SetRotFromQuaternion", in, 20, a, b, 16);
            fp.a(a, in + 20, in + 16);
            fp.b(b, in + 20, in + 16);
            Compare("Matrix4::SetFromPosRot", in, 23, a, b, 16);
        }
    }
}

static void Quat(float* q, int mode)
{
    if (Pick(2)) UnitQuat(q);
    else Fill(q, 4, mode);
}

static void TestQuaternion()
{
    {
        Begin("Quaternion::Set(float, float, float)");
        const Pair<T_FFF> f("?Set@Quaternion@Jet@@QAEXMMM@Z");
        for (int c = 0; c < cases; ++c) {
            float in[7], a[4], b[4];
            Fill(in, 4, 3);
            for (int i = 4; i < 7; ++i) in[i] = Pick(5) == 0 ? 0.0f : Value(Pick(4));
            std::memcpy(a, in, 16);
            std::memcpy(b, in, 16);
            f.a(a, nullptr, in[4], in[5], in[6]);
            f.b(b, nullptr, in[4], in[5], in[6]);
            Compare("Quaternion::Set(xyz)", in, 7, a, b, 4);
        }
    }
    {
        Begin("Quaternion::Set(Matrix3) / Set(Matrix4)");
        const Pair<T_P> f3("?Set@Quaternion@Jet@@QAEXABVMatrix3@2@@Z");
        const Pair<T_P> f4("?Set@Quaternion@Jet@@QAEXABVMatrix4@2@@Z");
        for (int c = 0; c < cases; ++c) {
            float in[20], a[4], b[4];
            Fill(in, 4, 3);
            Matrix(in + 4, 16, Mode());
            std::memcpy(a, in, 16);
            std::memcpy(b, in, 16);
            f3.a(a, nullptr, in + 4);
            f3.b(b, nullptr, in + 4);
            Compare("Quaternion::Set(Matrix3)", in, 13, a, b, 4);
            f4.a(a, nullptr, in + 4);
            f4.b(b, nullptr, in + 4);
            Compare("Quaternion::Set(Matrix4)", in, 20, a, b, 4);
        }
    }
    {
        Begin("Quaternion::Slerp");
        const Pair<T_PPF> f("?Slerp@Quaternion@Jet@@QAEAAV12@ABV12@0M@Z");
        for (int c = 0; c < cases; ++c) {
            float in[13], a[4], b[4];
            const int mode = Mode();
            Quat(in + 4, mode);
            switch (Pick(4)) {
            case 0: std::memcpy(in + 8, in + 4, 16); break;  // equal
            case 1:                                           // close (the linear branch)
                for (int i = 0; i < 4; ++i) in[8 + i] = in[4 + i] * (Pick(2) ? 1.0f : -1.0f) + Uniform(-1e-3f, 1e-3f);
                break;
            default: Quat(in + 8, mode); break;
            }
            in[12] = Pick(3) ? Uniform(0.0f, 1.0f) : Value(Pick(4));
            Fill(in, 4, 3);
            const bool alias = Pick(4) == 0;  // this == a
            std::memcpy(a, alias ? in + 4 : in, 16);
            std::memcpy(b, alias ? in + 4 : in, 16);
            float a1[4], b1[4];
            std::memcpy(a1, in + 4, 16);
            std::memcpy(b1, in + 4, 16);
            void* ra = f.a(a, nullptr, alias ? a : a1, in + 8, in[12]);
            void* rb = f.b(b, nullptr, alias ? b : b1, in + 8, in[12]);
            Compare("Quaternion::Slerp", in, 13, a, b, 4);
            if (ra != a || rb != b) Report("Quaternion::Slerp return", in, 0, &ra, &rb, 1);
        }
    }
    {
        Begin("Quaternion::Update");
        const Pair<T_PF> f("?Update@Quaternion@Jet@@QAEAAV12@ABVVector3@2@M@Z");
        for (int c = 0; c < cases; ++c) {
            float in[8], a[4], b[4];
            const int mode = Mode();
            Quat(in, mode);
            Fill(in + 4, 4, mode);
            std::memcpy(a, in, 16);
            std::memcpy(b, in, 16);
            void* ra = f.a(a, nullptr, in + 4, in[7]);
            void* rb = f.b(b, nullptr, in + 4, in[7]);
            Compare("Quaternion::Update", in, 8, a, b, 4);
            if (ra != a || rb != b) Report("Quaternion::Update return", in, 0, &ra, &rb, 1);
        }
    }
    {
        Begin("Quaternion::GetEulerAngles");
        const Pair<T_Ret> f("?GetEulerAngles@Quaternion@Jet@@QBE?AVVector3@2@XZ");
        for (int c = 0; c < cases; ++c) {
            float in[4], a[3] = {1, 2, 3}, b[3] = {1, 2, 3};
            const int mode = Mode();
            if (Pick(3) == 0) {  // near the poles: a rotation of +-90 degrees about y with noise
                const float s = 0.70710678f * (Pick(2) ? 1.0f : -1.0f);
                in[0] = Uniform(-1e-3f, 1e-3f);
                in[1] = s + Uniform(-1e-3f, 1e-3f);
                in[2] = Uniform(-1e-3f, 1e-3f);
                in[3] = 0.70710678f + Uniform(-1e-3f, 1e-3f);
                if (Pick(2)) std::swap(in[0], in[2]);
            } else {
                Quat(in, mode);
            }
            void* ra = f.a(in, nullptr, a);
            void* rb = f.b(in, nullptr, b);
            Compare("Quaternion::GetEulerAngles", in, 4, a, b, 3);
            if (ra != a || rb != b) Report("Quaternion::GetEulerAngles return", in, 0, &ra, &rb, 1);
        }
    }
    struct Axis {
        const char* name;
        Pair<T_PP> p;
    };
    Axis axes[] = {
        {"Quaternion::SetXAxis", Pair<T_PP>("?SetXAxis@Quaternion@Jet@@QAEAAV12@ABVVector3@2@0@Z")},
        {"Quaternion::SetYAxis", Pair<T_PP>("?SetYAxis@Quaternion@Jet@@QAEAAV12@ABVVector3@2@0@Z")},
        {"Quaternion::SetZAxis", Pair<T_PP>("?SetZAxis@Quaternion@Jet@@QAEAAV12@ABVVector3@2@0@Z")},
        {"Quaternion::SetViewVector", Pair<T_PP>("?SetViewVector@Quaternion@Jet@@QAEAAV12@ABVVector3@2@0@Z")}};
    for (Axis& o : axes) {
        Begin(o.name);
        for (int c = 0; c < cases; ++c) {
            float in[10], a[4], b[4];
            const int mode = Mode();
            Fill(in, 4, 3);
            Fill(in + 4, 3, mode);
            if (Pick(4) == 0) {  // up parallel to the axis: the default branch
                const float k = Uniform(-3.0f, 3.0f);
                for (int i = 0; i < 3; ++i) in[7 + i] = in[4 + i] * k + (Pick(2) ? 0.0f : Uniform(-1e-5f, 1e-5f));
            } else {
                Fill(in + 7, 3, mode);
            }
            std::memcpy(a, in, 16);
            std::memcpy(b, in, 16);
            void* ra = o.p.a(a, nullptr, in + 4, in + 7);
            void* rb = o.p.b(b, nullptr, in + 4, in + 7);
            Compare(o.name, in, 10, a, b, 4);
            if (ra != a || rb != b) Report("SetAxis return", in, 0, &ra, &rb, 1);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_ks_math2 <dir> [cases per member]\n");
        return 2;
    }
    const std::string dir = argv[1];
    if (argc > 2) cases = std::atoi(argv[2]);
    SetDllDirectoryA(dir.c_str());
    orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!ours || !orig) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    TestMatrix3();
    TestMatrix4();
    TestQuaternion();
    std::printf("%llu checks, %llu mismatches, %llu NaN-payload-only differences\n", checks, bad, nanOnly);
    return bad ? 1 : 0;
}
