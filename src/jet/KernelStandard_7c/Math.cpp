// Unit U1 of KernelStandard_7c: Crc, StdMath, Vector2/3 conversions and the integer log helpers.
// Behaviour follows the TS12 assembly (addresses per function).
//
// StdMath: the original computes in x87 at the CRT's 53-bit precision. Doing every step in double reproduces it
// bit for bit (inputs are floats or doubles, no intermediate leaves the double range for finite inputs). The
// float functions return their unrounded double in st(0) like the original; on Win32 a naked thunk passes the
// double result of the worker straight through.
#include <jet/Math.h>

#include <cstdint>
#include <cstring>

#include "StdMathLUT.h"  // generated at build time from the installed game's KernelStandard_7c.dll
#include "X87.h"

namespace Jet {

// ---------------------------------------------------------------------------------------------------------------
// Crc (0x1001b390). The table (0x10072998) is the standard MSB-first one for 0x04C11DB7.

namespace {
struct CrcTable {
    uint32_t t[256];
    constexpr CrcTable() : t()
    {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i << 24;
            for (int k = 0; k < 8; ++k) c = (c & 0x80000000u) ? (c << 1) ^ 0x04C11DB7u : c << 1;
            t[i] = c;
        }
    }
};
constexpr CrcTable kCrc;
static_assert(kCrc.t[1] == 0x04C11DB7u && kCrc.t[7] == 0x1E475005u);  // as in the original's table
}  // namespace

unsigned long __cdecl Crc::Calc(const void* data, unsigned long size, unsigned long seed)
{
    uint32_t crc = ~static_cast<uint32_t>(seed);
    const auto* p = static_cast<const unsigned char*>(data);
    for (unsigned long i = 0; i < size; ++i) crc = (crc << 8) ^ kCrc.t[(crc >> 24) ^ p[i]];
    return ~crc;
}

// ---------------------------------------------------------------------------------------------------------------
// integer helpers (0x10003e50..0x10003f02)

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

int __cdecl IntLog2(float f) { return static_cast<int>((static_cast<int32_t>(Bits(f)) >> 23) & 0xff) - 0x7f; }
int __cdecl ILog2(float f) { return IntLog2(f); }
int __cdecl ILog2(int i) { return IntLog2(static_cast<float>(i)); }

// smear the highest set bit down (arithmetic shifts: a negative input smears ones)
static int Smear(int v)
{
    v |= v >> 16;
    v |= v >> 8;
    v |= v >> 4;
    v |= v >> 2;
    return v | (v >> 1);
}

int __cdecl CeilPow2(int i) { return Smear(i - 1) + 1; }
int __cdecl FloorPow2(int i) { return i == 0 ? 0 : (Smear(i) >> 1) + 1; }

// ---------------------------------------------------------------------------------------------------------------
// Vector conversions (0x10042070, 0x10042090, 0x100420b0). The original copies through x87 fld/fstp, which
// quietens a signalling NaN; the Win32 build does the same so the bits match.

static void CopyFloats(float* dst, const float* src, int n)
{
    for (int i = 0; i < n; ++i) X87Copy(dst[i], src[i]);
}

Vector2::Vector2(const Vector3& v) { CopyFloats(&x, &v.x, 2); }
Vector2::Vector2(const Vector4& v) { CopyFloats(&x, &v.x, 2); }
Vector3::Vector3(const Vector2& v)
{
    CopyFloats(&x, &v.x, 2);
    z = 0.0f;
}
Vector3::Vector3(const Vector4& v) { CopyFloats(&x, &v.x, 3); }

// ---------------------------------------------------------------------------------------------------------------
// StdMath (0x10014490..0x1001471d)

unsigned int StdMath::s_sqrtLUT[512] = {STDMATH_SQRT_LUT_DATA};

StdMath& StdMath::operator=(const StdMath&) { return *this; }

namespace {

// 1/sqrt seed for the float with these bits: exponent (0x17c - e) / 2, mantissa from the table
double Seed(uint32_t bits, const unsigned int* lut)
{
    const uint32_t e = (bits >> 23) & 0xff;
    const uint32_t y = (((0x17cu - e) << 22) & 0xff800000u) | lut[(bits >> 15) & 0x1ff];
    return FromBits(y);
}

constexpr double kHalf = 0.5, kThreeHalves = 1.5;

}  // namespace

// StdMath::s_sqrtLUT is private: one friend accessor
struct StdMathWork {
    static const unsigned int* L() { return StdMath::s_sqrtLUT; }
};

static const unsigned int* L() { return StdMathWork::L(); }

// The workers, one per export, with C names so the Win32 thunks can jump to them.
extern "C" {

    // 0x10014490: the seed comes from the LOW 32 bits of the double (a bug in the original, kept)
    double __cdecl Jet_InvSqrtD(double x)
    {
        uint64_t raw;
        std::memcpy(&raw, &x, 8);
        const double h = x * kHalf;
        double y = Seed(static_cast<uint32_t>(raw), L());
        y = y * (kThreeHalves - (y * y) * h);
        y = y * (kThreeHalves - (y * y) * h);
        return y * (kThreeHalves - h * (y * y));
    }

    // 0x10014500
    double __cdecl Jet_InvSqrtF(float x)
    {
        const double h = static_cast<double>(x) * 0.5;
        double y = Seed(Bits(x), L());
        y = y * (kThreeHalves - (y * y) * h);
        return y * (kThreeHalves - h * (y * y));
    }

    // 0x10014560
    double __cdecl Jet_InvSqrt16F(float x)
    {
        const double y = Seed(Bits(x), L());
        const double h = static_cast<double>(x) * 0.5;
        return y * (kThreeHalves - (h * y) * y);
    }

    // 0x100145b0: the seed comes from x rounded to float
    double __cdecl Jet_SqrtD(double x)
    {
        const float xf = static_cast<float>(x);
        const double h = x * 0.5;
        double y = Seed(Bits(xf), L());
        y = y * (kThreeHalves - (y * y) * h);
        y = y * (kThreeHalves - h * (y * y));
        return x * y;
    }

    // 0x10014620
    double __cdecl Jet_SqrtF(float x)
    {
        const double xd = x;
        const double h = 0.5 * xd;
        double y = Seed(Bits(x), L());
        y = y * (kThreeHalves - (y * y) * h);
        y = y * (kThreeHalves - h * (y * y));
        return xd * y;
    }

    // 0x10014690
    double __cdecl Jet_Sqrt16F(float x)
    {
        const double y = Seed(Bits(x), L());
        const double xd = x;
        const double t = ((0.5 * xd) * y) * y;
        return xd * (y * (kThreeHalves - t));
    }

    // 0x100146f0
    double __cdecl Jet_InvSqrtApproxF(float x)
    {
        const double h = static_cast<double>(x) * 0.5;
        const double y = FromBits(0x5f3759dfu - (Bits(x) >> 1));
        return y * (kThreeHalves - (h * y) * y);
    }
}  // extern "C"

double __cdecl StdMath::Sqrt(double x) { return Jet_SqrtD(x); }
double __cdecl StdMath::InvSqrt(double x) { return Jet_InvSqrtD(x); }

#if defined(_M_IX86)
// return the worker's double unrounded in st(0): same stack frame, so a jump is enough
__declspec(naked) float __cdecl StdMath::Sqrt(float) { __asm jmp Jet_SqrtF }
__declspec(naked) float __cdecl StdMath::Sqrt16(float) { __asm jmp Jet_Sqrt16F }
__declspec(naked) float __cdecl StdMath::InvSqrt(float) { __asm jmp Jet_InvSqrtF }
__declspec(naked) float __cdecl StdMath::InvSqrt16(float) { __asm jmp Jet_InvSqrt16F }
__declspec(naked) float __cdecl StdMath::InvSqrtApprox(float) { __asm jmp Jet_InvSqrtApproxF }
#else
float __cdecl StdMath::Sqrt(float x) { return static_cast<float>(Jet_SqrtF(x)); }
float __cdecl StdMath::Sqrt16(float x) { return static_cast<float>(Jet_Sqrt16F(x)); }
float __cdecl StdMath::InvSqrt(float x) { return static_cast<float>(Jet_InvSqrtF(x)); }
float __cdecl StdMath::InvSqrt16(float x) { return static_cast<float>(Jet_InvSqrt16F(x)); }
float __cdecl StdMath::InvSqrtApprox(float x) { return static_cast<float>(Jet_InvSqrtApproxF(x)); }
#endif

}  // namespace Jet
