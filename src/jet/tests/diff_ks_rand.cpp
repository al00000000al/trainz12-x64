// Differential test of KernelStandard unit U5 (Rand.cpp): Rand and LCRand of our KernelStandard_7c.dll against
// KernelStandard_7c_orig.dll in one process. After seeding, the whole generator state (fields and both tables)
// is compared; then random call sequences run on both, comparing every result as raw bits (float results are read
// from st(0) unrounded) and the state after every call. Randomise is made deterministic by setting each module's
// exported Time::frameLock / frameLockTime (its own since U14b).
//
// usage: diff_ks_rand.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [seeds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

static HMODULE ours, orig;

// Time::frameLock / frameLockTime of both modules, set alike
struct TimeLock {
    bool* lock[2];
    double* time[2];
    TimeLock();
    void Set(bool on) { *lock[0] = *lock[1] = on; }
    void Time(double t) { *time[0] = *time[1] = t; }
};
static unsigned long long checks, bad;

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

static uint64_t DBits(double d)
{
    uint64_t b;
    std::memcpy(&b, &d, 8);
    return b;
}

static void Check(bool ok, const char* what, int seed, int step)
{
    ++checks;
    if (!ok && ++bad <= 10) std::printf("  mismatch %s seed %d step %d\n", what, seed, step);
}

struct RandObj {  // the 0x20-byte layout
    int index, lag, size, auxSize;
    uint32_t gaussFlag;  // bool + padding
    uint32_t state;
    uint32_t* aux;
    uint32_t* table;
};

static bool SameState(const RandObj& a, const RandObj& b)
{
    if (a.index != b.index || a.lag != b.lag || a.size != b.size || a.auxSize != b.auxSize ||
        (a.gaussFlag & 0xff) != (b.gaussFlag & 0xff) || a.state != b.state)
        return false;
    return std::memcmp(a.table, b.table, a.size * 4u) == 0 && std::memcmp(a.aux, b.aux, a.auxSize * 4u) == 0;
}

// thiscall emulated with __fastcall(this, edx, stack args)
using Ctor0 = void*(__fastcall*)(void*, void*);
using CtorI = void*(__fastcall*)(void*, void*, int);
using CtorII = void*(__fastcall*)(void*, void*, int, int);
using VoidU = void(__fastcall*)(void*, void*, unsigned int);
using Void0 = void(__fastcall*)(void*, void*);
using U0 = unsigned int(__fastcall*)(void*, void*);
using UU = unsigned int(__fastcall*)(void*, void*, unsigned int);
using I0 = int(__fastcall*)(void*, void*);
using II = int(__fastcall*)(void*, void*, int);
using III = int(__fastcall*)(void*, void*, int, int);
using F0 = double(__fastcall*)(void*, void*);
using FF = double(__fastcall*)(void*, void*, float);
using FFF = double(__fastcall*)(void*, void*, float, float);
using DD = double(__fastcall*)(void*, void*, double);
using DDD = double(__fastcall*)(void*, void*, double, double);
using B0 = bool(__fastcall*)(void*, void*);
using Assign = void*(__fastcall*)(void*, void*, const void*);

static float RandomArg(std::mt19937& rng)
{
    switch (rng() % 4) {
    case 0: return static_cast<float>(rng() % 1000);
    case 1: return static_cast<float>(static_cast<int>(rng() % 2001) - 1000) / 7.0f;
    case 2: {
        uint32_t b = rng();
        float f;
        std::memcpy(&f, &b, 4);
        return f;
    }
    default: return 1.0f;
    }
}

TimeLock::TimeLock()
{
    HMODULE m[2] = {ours, orig};
    for (int k = 0; k < 2; ++k) {
        lock[k] = Get<bool*>(m[k], "?frameLock@Time@Jet@@0_NA");
        time[k] = Get<double*>(m[k], "?frameLockTime@Time@Jet@@0NA");
    }
}

static void RandTest(int seeds)
{
    Pair<Ctor0> ctor0("??0Rand@Jet@@QAE@XZ");
    Pair<CtorI> ctor1("??0Rand@Jet@@QAE@H@Z");
    Pair<CtorII> ctor2("??0Rand@Jet@@QAE@HH@Z");
    Pair<Void0> dtor("??1Rand@Jet@@QAE@XZ"), randomise("?Randomise@Rand@Jet@@QAEXXZ");
    Pair<VoidU> setSeed("?SetSeed@Rand@Jet@@QAEXI@Z");
    Pair<U0> getSeed("?GetSeed@Rand@Jet@@QBEIXZ"), getRandom("?GetRandom@Rand@Jet@@QAEIXZ"), randLC("?RandLC@Rand@Jet@@IAEIXZ");
    Pair<UU> opU("??RRand@Jet@@QAEII@Z");
    Pair<III> opII("??RRand@Jet@@QAEHHH@Z"), nextIntII("?NextInt@Rand@Jet@@QAEHHH@Z");
    Pair<I0> nextInt0("?NextInt@Rand@Jet@@QAEHXZ");
    Pair<II> nextInt1("?NextInt@Rand@Jet@@QAEHH@Z");
    Pair<F0> op0("??RRand@Jet@@QAEMXZ"), nextFloat0("?NextFloat@Rand@Jet@@QAEMXZ"), nextDouble0("?NextDouble@Rand@Jet@@QAENXZ");
    Pair<FF> opF("??RRand@Jet@@QAEMM@Z"), nextFloat1("?NextFloat@Rand@Jet@@QAEMM@Z");
    Pair<FFF> opFF("??RRand@Jet@@QAEMMM@Z"), nextFloat2("?NextFloat@Rand@Jet@@QAEMMM@Z"), gauss("?Gaussian@Rand@Jet@@QAEMMM@Z");
    Pair<DD> nextDouble1("?NextDouble@Rand@Jet@@QAENN@Z");
    Pair<DDD> nextDouble2("?NextDouble@Rand@Jet@@QAENNN@Z");
    Pair<B0> nextBool("?NextBool@Rand@Jet@@QAE_NXZ");
    Pair<Assign> assign("??4Rand@Jet@@QAEAAV01@ABV01@@Z");
    Pair<CtorII> init("?Init@Rand@Jet@@IAEXHH@Z");
    TimeLock frameLock;

    std::mt19937 rng(321);
    for (int seed = 0; seed < seeds; ++seed) {
        RandObj a{}, b{};
        const int kind = seed % 4;
        int n = 0, m = 0;
        if (kind == 0) {
            ctor0.a(&a, nullptr);
            ctor0.b(&b, nullptr);
        } else if (kind == 1) {
            n = static_cast<int>(rng() % 600);
            ctor1.a(&a, nullptr, n);
            ctor1.b(&b, nullptr, n);
        } else {
            n = static_cast<int>(rng() % 600);
            m = static_cast<int>(rng() % 300);
            ctor2.a(&a, nullptr, n, m);
            ctor2.b(&b, nullptr, n, m);
        }
        Check(a.index == b.index && a.lag == b.lag && a.size == b.size && a.auxSize == b.auxSize &&
                  (a.gaussFlag & 0xff) == (b.gaussFlag & 0xff) && a.state == b.state,
              "construction", seed, 0);
        if (kind == 3) {
            frameLock.Set(true);
            frameLock.Time(static_cast<double>(rng()) * 1.37);
            randomise.a(&a, nullptr);
            randomise.b(&b, nullptr);
            frameLock.Set(false);
        } else {
            const unsigned int s = (seed % 9 == 0) ? 0u : (seed % 9 == 1 ? 0xFFFFFFFFu : rng());
            setSeed.a(&a, nullptr, s);
            setSeed.b(&b, nullptr, s);
        }
        Check(SameState(a, b), "seeded state", seed, 0);
        for (int step = 0; step < 4000; ++step) {
            const unsigned op = rng() % 22;
            const float f1 = RandomArg(rng), f2 = RandomArg(rng);
            const int i1 = static_cast<int>(rng() % 3 ? rng() % 2000 : rng()), i2 = static_cast<int>(rng());
            uint64_t ra = 0, rb = 0;
            switch (op) {
            case 0: ra = getRandom.a(&a, nullptr); rb = getRandom.b(&b, nullptr); break;
            case 1: ra = opU.a(&a, nullptr, i2); rb = opU.b(&b, nullptr, i2); break;
            case 2: ra = static_cast<uint32_t>(opII.a(&a, nullptr, i1, i2)); rb = static_cast<uint32_t>(opII.b(&b, nullptr, i1, i2)); break;
            case 3: ra = static_cast<uint32_t>(nextIntII.a(&a, nullptr, -i1, i1)); rb = static_cast<uint32_t>(nextIntII.b(&b, nullptr, -i1, i1)); break;
            case 4: ra = static_cast<uint32_t>(nextInt0.a(&a, nullptr)); rb = static_cast<uint32_t>(nextInt0.b(&b, nullptr)); break;
            case 5: ra = static_cast<uint32_t>(nextInt1.a(&a, nullptr, i1)); rb = static_cast<uint32_t>(nextInt1.b(&b, nullptr, i1)); break;
            case 6: ra = DBits(op0.a(&a, nullptr)); rb = DBits(op0.b(&b, nullptr)); break;
            case 7: ra = DBits(nextFloat0.a(&a, nullptr)); rb = DBits(nextFloat0.b(&b, nullptr)); break;
            case 8: ra = DBits(nextDouble0.a(&a, nullptr)); rb = DBits(nextDouble0.b(&b, nullptr)); break;
            case 9: ra = DBits(opF.a(&a, nullptr, f1)); rb = DBits(opF.b(&b, nullptr, f1)); break;
            case 10: ra = DBits(nextFloat1.a(&a, nullptr, f1)); rb = DBits(nextFloat1.b(&b, nullptr, f1)); break;
            case 11: ra = DBits(opFF.a(&a, nullptr, f1, f2)); rb = DBits(opFF.b(&b, nullptr, f1, f2)); break;
            case 12: ra = DBits(nextFloat2.a(&a, nullptr, f1, f2)); rb = DBits(nextFloat2.b(&b, nullptr, f1, f2)); break;
            case 13: ra = DBits(nextDouble1.a(&a, nullptr, f1 * 1.5)); rb = DBits(nextDouble1.b(&b, nullptr, f1 * 1.5)); break;
            case 14: ra = DBits(nextDouble2.a(&a, nullptr, f1, f2 * 3.0)); rb = DBits(nextDouble2.b(&b, nullptr, f1, f2 * 3.0)); break;
            case 15: ra = nextBool.a(&a, nullptr); rb = nextBool.b(&b, nullptr); break;
            case 16: case 17: ra = DBits(gauss.a(&a, nullptr, f1, f2)); rb = DBits(gauss.b(&b, nullptr, f1, f2)); break;
            case 18: ra = randLC.a(&a, nullptr); rb = randLC.b(&b, nullptr); break;
            case 19: ra = getSeed.a(&a, nullptr); rb = getSeed.b(&b, nullptr); break;
            case 20: {  // assignment: a shallow copy of all eight dwords
                RandObj ca{}, cb{};
                const bool sa = assign.a(&ca, nullptr, &a) == &ca, sb = assign.b(&cb, nullptr, &b) == &cb;
                ra = sa && std::memcmp(&ca, &a, sizeof a) == 0;
                rb = sb && std::memcmp(&cb, &b, sizeof b) == 0;
                break;
            }
            default:
                if (rng() % 50 == 0) {  // occasionally re-seed
                    const unsigned int s = rng();
                    setSeed.a(&a, nullptr, s);
                    setSeed.b(&b, nullptr, s);
                }
            }
            if (ra != rb && bad < 12)
                std::printf("  op %u args %08x %08x %d %d: ours %016llx orig %016llx\n", op, *reinterpret_cast<const uint32_t*>(&f1),
                            *reinterpret_cast<const uint32_t*>(&f2), i1, i2, ra, rb);
            Check(ra == rb, "result", seed, step);
            Check(SameState(a, b), "state", seed, step);
        }
        // re-initialise to a new size (frees the old tables)
        const int n2 = static_cast<int>(rng() % 700), m2 = static_cast<int>(rng() % 250);
        init.a(&a, nullptr, n2, m2);
        init.b(&b, nullptr, n2, m2);
        Check(a.index == b.index && a.lag == b.lag && a.size == b.size && a.auxSize == b.auxSize && a.state == b.state,
              "Init", seed, -1);
        dtor.a(&a, nullptr);
        dtor.b(&b, nullptr);
    }
}

static void LCRandTest(int seeds)
{
    Pair<Ctor0> ctor0("??0LCRand@Jet@@QAE@XZ");
    Pair<CtorI> ctor1("??0LCRand@Jet@@QAE@I@Z");
    Pair<Void0> randomise("?Randomise@LCRand@Jet@@QAEXXZ");
    Pair<VoidU> setSeed("?SetSeed@LCRand@Jet@@QAEXI@Z");
    Pair<U0> getSeed("?GetSeed@LCRand@Jet@@QBEIXZ"), getRandom("?GetRandom@LCRand@Jet@@QAEIXZ"), op0("??RLCRand@Jet@@QAEIXZ");
    Pair<I0> nextInt0("?NextInt@LCRand@Jet@@QAEHXZ");
    Pair<II> nextInt1("?NextInt@LCRand@Jet@@QAEHH@Z");
    Pair<III> nextInt2("?NextInt@LCRand@Jet@@QAEHHH@Z");
    Pair<F0> nextFloat0("?NextFloat@LCRand@Jet@@QAEMXZ"), nextDouble0("?NextDouble@LCRand@Jet@@QAENXZ");
    Pair<FF> nextFloat1("?NextFloat@LCRand@Jet@@QAEMM@Z");
    Pair<FFF> nextFloat2("?NextFloat@LCRand@Jet@@QAEMMM@Z");
    Pair<DD> nextDouble1("?NextDouble@LCRand@Jet@@QAENN@Z");
    Pair<DDD> nextDouble2("?NextDouble@LCRand@Jet@@QAENNN@Z");
    Pair<B0> nextBool("?NextBool@LCRand@Jet@@QAE_NXZ");
    Pair<Assign> assign("??4LCRand@Jet@@QAEAAV01@ABV01@@Z");
    TimeLock frameLock;
    std::mt19937 rng(654);
    for (int seed = 0; seed < seeds; ++seed) {
        uint32_t a = 0, b = 0;
        frameLock.Set(true);
        frameLock.Time(static_cast<double>(rng()) * 0.73);
        if (seed % 3 == 0) {
            ctor0.a(&a, nullptr);
            ctor0.b(&b, nullptr);
        } else if (seed % 3 == 1) {
            randomise.a(&a, nullptr);
            randomise.b(&b, nullptr);
        } else {
            const unsigned int s = (seed % 7 == 0) ? 0x3E63Au + static_cast<unsigned int>(seed % 3) : rng();
            ctor1.a(&a, nullptr, static_cast<int>(s));
            ctor1.b(&b, nullptr, static_cast<int>(s));
        }
        frameLock.Set(false);
        Check(a == b, "LCRand seed", seed, 0);
        for (int step = 0; step < 4000; ++step) {
            const unsigned op = rng() % 15;
            const float f1 = RandomArg(rng), f2 = RandomArg(rng);
            const int i1 = static_cast<int>(rng() % 3 ? rng() % 2000 : rng());
            uint64_t ra = 0, rb = 0;
            switch (op) {
            case 0: ra = getRandom.a(&a, nullptr); rb = getRandom.b(&b, nullptr); break;
            case 1: ra = op0.a(&a, nullptr); rb = op0.b(&b, nullptr); break;
            case 2: ra = static_cast<uint32_t>(nextInt0.a(&a, nullptr)); rb = static_cast<uint32_t>(nextInt0.b(&b, nullptr)); break;
            case 3: ra = static_cast<uint32_t>(nextInt1.a(&a, nullptr, i1)); rb = static_cast<uint32_t>(nextInt1.b(&b, nullptr, i1)); break;
            case 4: ra = static_cast<uint32_t>(nextInt2.a(&a, nullptr, -i1, i1 * 3)); rb = static_cast<uint32_t>(nextInt2.b(&b, nullptr, -i1, i1 * 3)); break;
            case 5: ra = DBits(nextFloat0.a(&a, nullptr)); rb = DBits(nextFloat0.b(&b, nullptr)); break;
            case 6: ra = DBits(nextDouble0.a(&a, nullptr)); rb = DBits(nextDouble0.b(&b, nullptr)); break;
            case 7: ra = DBits(nextFloat1.a(&a, nullptr, f1)); rb = DBits(nextFloat1.b(&b, nullptr, f1)); break;
            case 8: ra = DBits(nextFloat2.a(&a, nullptr, f1, f2)); rb = DBits(nextFloat2.b(&b, nullptr, f1, f2)); break;
            case 9: ra = DBits(nextDouble1.a(&a, nullptr, f1 * 2.0)); rb = DBits(nextDouble1.b(&b, nullptr, f1 * 2.0)); break;
            case 10: ra = DBits(nextDouble2.a(&a, nullptr, f1, f2 * 5.0)); rb = DBits(nextDouble2.b(&b, nullptr, f1, f2 * 5.0)); break;
            case 11: ra = nextBool.a(&a, nullptr); rb = nextBool.b(&b, nullptr); break;
            case 12: ra = getSeed.a(&a, nullptr); rb = getSeed.b(&b, nullptr); break;
            case 13: {
                uint32_t ca = 0, cb = 0;
                ra = assign.a(&ca, nullptr, &a) == &ca && ca == a;
                rb = assign.b(&cb, nullptr, &b) == &cb && cb == b;
                break;
            }
            default: {
                const unsigned int s = rng() % 4 ? rng() : 0x3E63Au - 1 + rng() % 3;
                setSeed.a(&a, nullptr, s);
                setSeed.b(&b, nullptr, s);
            }
            }
            if (ra != rb && bad < 24)
                std::printf("  lc op %u args %08x %08x %d: ours %016llx orig %016llx\n", op, *reinterpret_cast<const uint32_t*>(&f1),
                            *reinterpret_cast<const uint32_t*>(&f2), i1, ra, rb);
            Check(ra == rb && a == b, "LCRand step", seed, step);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const int seeds = argc > 2 ? std::atoi(argv[2]) : 2000;
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) return 2;
    RandTest(seeds);
    std::printf("Rand:   %llu checks, %llu mismatches\n", checks, bad);
    const unsigned long long c1 = checks, b1 = bad;
    LCRandTest(seeds);
    std::printf("LCRand: %llu checks, %llu mismatches\n", checks - c1, bad - b1);
    return bad ? 1 : 0;
}
