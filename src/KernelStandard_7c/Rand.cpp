// Unit U5 of KernelStandard_7c: Rand (an R250-style table generator) and LCRand (16807 x mod 2^32-1)
// (0x1003c370..0x1003d256, 0x10001860). Transcribed from the assembly: every scaled result is x87 arithmetic at
// the CRT's 53-bit precision, which `double` reproduces; integer results are truncated (fistp with chop / _ftol2).
//
// The original returns float results unrounded in st(0). The Win32 exports of the float members therefore point
// at the *Raw workers below (a __fastcall function taking (this, unused edx, args...) has exactly the thiscall
// ABI and returns its double in st(0)); see implemented.txt. The members themselves round to float as usual.
#include <jet/KernelStandard.h>

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>

#pragma comment(lib, "winmm.lib")

namespace Jet {

namespace {

double FromBits(uint64_t b)
{
    double d;
    std::memcpy(&d, &b, 8);
    return d;
}

const double kScale = FromBits(0x3DF0000000100000ull);  // 1 / (2^32 - 1), as in the original (0x100575c0)

double U(unsigned int x) { return static_cast<double>(x); }

// _ftol2_sse (0x1004de90): cvttsd2si, so out-of-range values and NaN give 0x80000000
int Chop(double x) { return _mm_cvttsd_si32(_mm_set_sd(x)); }

// 16807 x mod (2^32 - 1), Schrage: q = 255546, r = 5673; t <= 0 becomes t - 1 (that is t + m mod 2^32)
unsigned int LC(unsigned int s)
{
    if (s <= 0x3E63Au) return (s * 16807u) % 0xFFFFFFFFu;
    const unsigned int q = s / 0x3E63Au, r = s % 0x3E63Au;
    int t = static_cast<int>(r * 16807u - q * 5673u);
    if (t <= 0) t -= 1;
    return static_cast<unsigned int>(t);
}

// natural log as the x87 fyl2x gives it (rounded to double, which the following fmul would do anyway)
double X87Ln(double s)
{
#if defined(_M_IX86)
    double r;
    __asm {
        fldln2
        fld qword ptr [s]
        fyl2x
        fstp qword ptr [r]
    }
    return r;
#else
    return std::log(s);
#endif
}

float g_gaussSpare;  // one static for all generators (0x100ae250)

}  // namespace

// private access for the free functions below
struct RandWork {
    static unsigned int Step(Rand& r)  // the table step every member inlines (GetRandom, 0x1003d020)
    {
        const int i = r.index;
        const int k = (i < r.lag) ? r.size - r.lag + i : i - r.lag;
        const unsigned int v = r.table[k] ^ r.table[i];
        r.table[i] = v;
        r.index = (i >= r.size - 1) ? 0 : i + 1;
        return v;
    }

    static unsigned int RandomiseSeed()  // 0x1003c370 / 0x1003c850
    {
        if (Time::frameLock) return static_cast<unsigned int>(static_cast<long long>(Time::frameLockTime * 0.001)) + 10;
        return timeGetTime() + 10;
    }

    static double Gaussian(Rand& r, float mean, float sd)  // 0x1003cec0: the polar method
    {
        if (r.haveGauss) {
            r.haveGauss = false;
            return double(g_gaussSpare) * double(sd) + double(mean);
        }
        double u, v;
        float s;
        do {
            u = (U(Step(r)) * kScale) * 2.0 - 1.0;
            const float uf = static_cast<float>(u);
            v = (U(Step(r)) * kScale) * 2.0 - 1.0;
            const float vf = static_cast<float>(v);
            s = vf * vf + uf * uf;
        } while (s >= 1.0f);
        r.haveGauss = true;
        const double f = std::sqrt((X87Ln(s) * -2.0) / double(s));
        g_gaussSpare = static_cast<float>(v * f);
        return (u * f) * double(sd) + double(mean);
    }
};

// ---- Rand: the double-valued workers (also the Win32 entry points of the float members)

double __fastcall Rand_NextFloat0(Rand* r, int) { return U(RandWork::Step(*r)) * kScale; }  // 0x1003c6a0

double __fastcall Rand_NextFloat1(Rand* r, int, float m)  // 0x1003c500
{
    const unsigned int v = RandWork::Step(*r);
    return v ? (U(v) * double(m)) * kScale : 0.0;
}

double __fastcall Rand_NextFloat2(Rand* r, int, float lo, float hi)  // 0x1003c630
{
    const unsigned int v = RandWork::Step(*r);
    return v ? ((double(hi) - double(lo)) * U(v)) * kScale + double(lo) : 0.0;
}

double __fastcall Rand_Gaussian(Rand* r, int, float mean, float sd) { return RandWork::Gaussian(*r, mean, sd); }

// ---- Rand

void Rand::Init(int n, int auxN)  // 0x1003d0b0
{
    index = 0;
    state = 0;
    if (n < 250) n = 250;
    if (auxN < 100) auxN = 100;
    size = n;
    auxSize = auxN;
    if (table) Alloc::Delete(table);
    if (aux) Alloc::Delete(aux);
    table = static_cast<unsigned int*>(Alloc::New(static_cast<unsigned int>(n) * 4));
    aux = static_cast<unsigned int*>(Alloc::New(static_cast<unsigned int>(auxN) * 4));
    lag = n / 2 + 22;
}

Rand::Rand()  // 0x1003d160: the tables are allocated, not seeded
    : index(0), lag(147), size(250), auxSize(100), haveGauss(false), state(0), aux(nullptr), table(nullptr)
{
    table = static_cast<unsigned int*>(Alloc::New(250 * 4));
    aux = static_cast<unsigned int*>(Alloc::New(100 * 4));
}

Rand::Rand(int n) : haveGauss(false), aux(nullptr), table(nullptr) { Init(n, 100); }
Rand::Rand(int n, int auxN) : haveGauss(false), aux(nullptr), table(nullptr) { Init(n, auxN); }

Rand::~Rand()
{
    if (table) Alloc::Delete(table);
    if (aux) Alloc::Delete(aux);
}

Rand& Rand::operator=(const Rand& o)  // 0x1003c830: eight dwords
{
    std::memcpy(static_cast<void*>(this), &o, sizeof *this);
    return *this;
}

void Rand::Randomise() { SetSeed(RandWork::RandomiseSeed()); }

void Rand::SetSeed(unsigned int seed)  // 0x1003cca0
{
    state = seed;
    index = 0;
    for (int i = 0; i < auxSize; ++i) {
        state = LC(state);
        aux[i] = state;
    }
    state = LC(state);
    for (int i = 0; i < size; ++i) {
        // index into the auxiliary table: computed in x87, stored as float, then rounded to nearest
        const float x = static_cast<float>(U(state) * (1.0 / 4294967296.0) * double(auxSize - 1) - 0.5);
        const int j = static_cast<int>(std::nearbyint(x));
        table[i] = aux[j];
        state = LC(state);
        aux[j] = state;
    }
    for (int i = 0; i < size; ++i) {
        state = LC(state);
        if (state > 0x40000000u) table[i] |= 0x80000000u;
    }
    unsigned int mask = 0xFFFFFFFFu, bit = 0x80000000u;  // a diagonal of 32 bits makes the table independent
    for (int k = 0; k < 32; ++k, mask >>= 1, bit >>= 1) {
        table[3 + 7 * k] &= mask;
        table[3 + 7 * k] |= bit;
    }
}

unsigned int Rand::GetSeed() const { return state; }
unsigned int Rand::GetRandom() { return RandWork::Step(*this); }

unsigned int Rand::RandLC()  // 0x1003d060
{
    state = LC(state);
    return state;
}

unsigned int Rand::operator()(unsigned int n)  // 0x1003c3e0
{
    const unsigned int v = RandWork::Step(*this);
    return v ? static_cast<unsigned int>(static_cast<long long>((U(v) * U(n)) * kScale)) : 0;
}

int Rand::NextInt(int lo, int hi)  // 0x1003c480
{
    const unsigned int v = RandWork::Step(*this);
    if (!v) return 0;
    const int d = static_cast<int>(static_cast<unsigned int>(hi) - static_cast<unsigned int>(lo));
    return Chop((double(d) * U(v)) * kScale + double(lo));
}

int Rand::operator()(int lo, int hi) { return NextInt(lo, hi); }
int Rand::NextInt() { return static_cast<int>(RandWork::Step(*this) & 0x7FFFFFFFu); }

int Rand::NextInt(int n)  // 0x1003c5c0
{
    const unsigned int v = RandWork::Step(*this);
    return v ? Chop((U(v) * double(n)) * kScale) : 0;
}

float Rand::NextFloat() { return static_cast<float>(Rand_NextFloat0(this, 0)); }
float Rand::NextFloat(float m) { return static_cast<float>(Rand_NextFloat1(this, 0, m)); }
float Rand::NextFloat(float lo, float hi) { return static_cast<float>(Rand_NextFloat2(this, 0, lo, hi)); }
float Rand::operator()() { return NextFloat(); }
float Rand::operator()(float m) { return NextFloat(m); }
float Rand::operator()(float lo, float hi) { return NextFloat(lo, hi); }
double Rand::NextDouble() { return Rand_NextFloat0(this, 0); }

double Rand::NextDouble(double m)  // 0x1003c700
{
    const unsigned int v = RandWork::Step(*this);
    return v ? (U(v) * m) * kScale : 0.0;
}

double Rand::NextDouble(double lo, double hi)  // 0x1003c770
{
    const unsigned int v = RandWork::Step(*this);
    return v ? ((hi - lo) * U(v)) * kScale + lo : 0.0;
}

bool Rand::NextBool() { return (RandWork::Step(*this) & 1) != 0; }
float Rand::Gaussian(float mean, float sd) { return static_cast<float>(RandWork::Gaussian(*this, mean, sd)); }

// ---- LCRand (0x1003c850..0x1003cc92, 0x10001860, 0x1003d250); no zero checks here

double __fastcall LCRand_NextFloat0(LCRand* r, int) { return U(r->GetRandom()) * kScale; }
double __fastcall LCRand_NextFloat1(LCRand* r, int, float m) { return (U(r->GetRandom()) * double(m)) * kScale; }
double __fastcall LCRand_NextFloat2(LCRand* r, int, float lo, float hi)
{
    const unsigned int v = r->GetRandom();
    return ((double(hi) - double(lo)) * U(v)) * kScale + double(lo);
}

LCRand::LCRand() : state(RandWork::RandomiseSeed()) {}
LCRand::LCRand(unsigned int seed) : state(seed) {}
LCRand& LCRand::operator=(const LCRand& o)
{
    state = o.state;
    return *this;
}
void LCRand::Randomise() { state = RandWork::RandomiseSeed(); }
void LCRand::SetSeed(unsigned int seed) { state = seed; }
unsigned int LCRand::GetSeed() const { return state; }

unsigned int LCRand::GetRandom()
{
    state = LC(state);
    return state;
}

unsigned int LCRand::operator()() { return GetRandom(); }
int LCRand::NextInt() { return static_cast<int>(GetRandom() & 0x7FFFFFFFu); }
int LCRand::NextInt(int n) { return Chop((U(GetRandom()) * double(n)) * kScale); }

int LCRand::NextInt(int lo, int hi)
{
    const unsigned int v = GetRandom();
    const int d = static_cast<int>(static_cast<unsigned int>(hi) - static_cast<unsigned int>(lo));
    return Chop((double(d) * U(v)) * kScale + double(lo));
}

float LCRand::NextFloat() { return static_cast<float>(LCRand_NextFloat0(this, 0)); }
float LCRand::NextFloat(float m) { return static_cast<float>(LCRand_NextFloat1(this, 0, m)); }
float LCRand::NextFloat(float lo, float hi) { return static_cast<float>(LCRand_NextFloat2(this, 0, lo, hi)); }
double LCRand::NextDouble() { return LCRand_NextFloat0(this, 0); }
double LCRand::NextDouble(double m) { return (U(GetRandom()) * m) * kScale; }

double LCRand::NextDouble(double lo, double hi)
{
    const unsigned int v = GetRandom();
    return ((hi - lo) * U(v)) * kScale + lo;
}

bool LCRand::NextBool() { return (GetRandom() & 1) != 0; }

}  // namespace Jet
