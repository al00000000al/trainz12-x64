// Unit U2 of KernelStandard_7c: Matrix3, Matrix4 and Quaternion rotations (0x1002fd70..0x10031d86,
// 0x1003aa30..0x1003c0a0). Transcribed from the assembly: the float arithmetic is SSE scalar in the original, so
// each expression below keeps the original's association (a*b == b*a bit for bit, but (a+b)+c != a+(b+c)). The
// expressions of Invert, the rotations and the quaternion product were lifted from the listing with
// tools/sse_lift.py (public repo).
//
// x87 parts: sin/cos come from fsincos rounded to float, square roots and the quaternion lengths are x87 at the
// CRT's 53-bit precision (which `double` reproduces), Slerp and GetEulerAngles use fsin/fpatan and msvcr80's
// _CIacos/_CIasin in extended precision. Those run as inline asm on Win32 (calling the msvcr80 of the process,
// the one the original calls); on x64 they fall back to <cmath>.
//
// GetDeterminant returns its double unrounded in st(0); the Win32 export points at Matrix4_Determinant (a
// __cdecl function of the matrix pointer has the ABI of the __cdecl member). See implemented.txt.
#include <jet/Math.h>

#include <windows.h>

#include "Crt80.h"

#include <cmath>

#include "X87.h"

namespace Jet {

extern "C" double __cdecl Jet_SqrtF(float x);     // Math.cpp
extern "C" double __cdecl Jet_InvSqrtF(float x);  // Math.cpp

namespace {

// ---- x87 helpers

// 0x10031d70: fsincos, both results rounded to float
void SinCos(float a, float& s, float& c)
{
#if defined(_M_IX86)
    float* ps = &s;
    float* pc = &c;
    __asm {
        fld dword ptr [a]
        fsincos
        mov eax, pc
        fstp dword ptr [eax]
        mov eax, ps
        fstp dword ptr [eax]
    }
#else
    s = static_cast<float>(std::sin(static_cast<double>(a)));
    c = static_cast<float>(std::cos(static_cast<double>(a)));
#endif
}

// the callers of Rotate skip the call for a zero angle (so -0 gives +0)
void SinCosNonZero(float a, float& s, float& c)
{
    if (a == 0.0f) {
        s = 0.0f;
        c = 1.0f;
    } else {
        SinCos(a, s, c);
    }
}

#if defined(_M_IX86)
// msvcr80's x87 intrinsics (argument and result in st(0)), as the original imports them
void* CrtTrig(const char* name) { return Crt80::Proc(name); }

void* CIacos()
{
    static void* const f = CrtTrig("_CIacos");
    return f;
}

void* CIasin()
{
    static void* const f = CrtTrig("_CIasin");
    return f;
}

float CallCrt(void* fn, float x)  // fld x, call, fstp float
{
    float r;
    __asm {
        fld dword ptr [x]
        call dword ptr [fn]
        fstp dword ptr [r]
    }
    return r;
}
#endif

float Acos(float x)
{
#if defined(_M_IX86)
    if (void* f = CIacos()) return CallCrt(f, x);
#endif
    return static_cast<float>(std::acos(static_cast<double>(x)));
}

float Asin(float x)
{
#if defined(_M_IX86)
    if (void* f = CIasin()) return CallCrt(f, x);
#endif
    return static_cast<float>(std::asin(static_cast<double>(x)));
}

// fpatan of two 53-bit values: atan2(y, x) in extended precision, rounded to float
float Atan2(double y, double x)
{
#if defined(_M_IX86)
    float r;
    __asm {
        fld qword ptr [y]
        fld qword ptr [x]
        fpatan
        fstp dword ptr [r]
    }
    return r;
#else
    return static_cast<float>(std::atan2(y, x));
#endif
}

// Slerp's weights (0x1003b784): a = acos(dot), s0 = sin((1 - t) a) / sin a, s1 = sin(a t) / sin a, all in x87
void SlerpWeights(float dot, float t, float& s0, float& s1)
{
#if defined(_M_IX86)
    void* f = CIacos();
    const double a = f ? 0.0 : std::acos(static_cast<double>(dot));
    float r0, r1;
    __asm {
        mov eax, f
        test eax, eax
        jz from_double
        fld dword ptr [dot]
        call eax
        jmp have_angle
    from_double:
        fld qword ptr [a]
    have_angle:
        fld st(0)
        fsin
        fld1
        fld st(0)
        fdivrp st(2), st
        fld dword ptr [t]
        fsub st(1), st
        fxch st(1)
        fmul st, st(3)
        fsin
        fmul st, st(2)
        fstp dword ptr [r0]
        fmulp st(2), st
        fxch st(1)
        fsin
        fmulp st(1), st
        fstp dword ptr [r1]
    }
    s0 = r0;
    s1 = r1;
#else
    const double a = std::acos(static_cast<double>(dot));
    const double inv = 1.0 / std::sin(a);
    const double td = t;
    s0 = static_cast<float>(std::sin((1.0 - td) * a) * inv);
    s1 = static_cast<float>(std::sin(a * td) * inv);
#endif
}

// ---- vector helpers (0x1003aa30, 0x1003aad0, 0x1003ab90)

float Len2(const float* v) { return (v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]; }

void Normalize(const float* v, float* out)  // no zero check
{
    const float inv = static_cast<float>(Jet_InvSqrtF(Len2(v)));
    const float x = v[0], y = v[1], z = v[2];
    out[0] = x * inv;
    out[1] = y * inv;
    out[2] = z * inv;
}

// 3x3 product out = a * b (0x1003ac50)
void Mul3(const float* a, const float* b, float* out)
{
    float r[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r[i * 3 + j] = (a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j]) + a[i * 3 + 2] * b[6 + j];
    for (int k = 0; k < 9; ++k) out[k] = r[k];
}

// Rotate(x, y, z) on rows 0-2 of a matrix with the given row stride and column count (0x1002fd70, 0x100310a0)
void RotateRows(float* m, int stride, int cols, float ax, float ay, float az)
{
    float sa, ca, sb, cb, sc, cc;
    SinCosNonZero(ax, sa, ca);
    SinCosNonZero(ay, sb, cb);
    SinCosNonZero(az, sc, cc);
    const float f9 = cb * ca, f10 = cb * sa, f11 = cc * cb, f6 = sc * cb;
    const float ss = sc * sb, cs = cc * sb;
    const float f14 = sc * sa - cs * ca;
    const float f15 = ss * ca + cc * sa;
    const float f5 = cs * sa + sc * ca;
    const float f8 = cc * ca - ss * sa;
    float r[12];
    for (int j = 0; j < cols; ++j) {
        const float m0 = m[j], m1 = m[stride + j], m2 = m[2 * stride + j];
        r[j] = (m2 * f14 + m1 * f5) + m0 * f11;
        r[cols + j] = (m1 * f8 - m0 * f6) + m2 * f15;
        r[2 * cols + j] = (m0 * sb - m1 * f10) + m2 * f9;
    }
    for (int j = 0; j < cols; ++j) {
        m[j] = r[j];
        m[stride + j] = r[cols + j];
        m[2 * stride + j] = r[2 * cols + j];
    }
}

// rotation of rows a and b by (s, c) over the first three columns: a' = a c + b s, b' = -0 - a s + b c
void RotatePair(float* a, float* b, float s, float c)
{
    for (int j = 0; j < 3; ++j) {
        const float x = a[j], y = b[j];
        a[j] = x * c + y * s;
        b[j] = (-0.0f - x * s) + y * c;
    }
}

// the rotation part of SetRotFromQuaternion / SetFromPosRot (0x10031ab0, 0x10031c10)
void QuaternionRows(float* m, const Quaternion& q)
{
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    const float z2 = z * 2.0f, x2 = x * 2.0f, y2 = y * 2.0f;
    const float xx = x * x2, zz = z * z2, yz = y * z2, xz = x * z2;
    const float wx = w * x2, wz = w * z2, xy = x * y2, yy = y * y2, wy = w * y2;
    m[0] = 1.0f - (zz + yy);
    m[1] = wz + xy;
    m[2] = xz - wy;
    m[4] = xy - wz;
    m[5] = 1.0f - (zz + xx);
    m[6] = wx + yz;
    m[8] = wy + xz;
    m[9] = yz - wx;
    m[10] = 1.0f - (yy + xx);
}

// the four cofactors of row 0, shared by Invert and GetDeterminant (0x10030590, 0x10030e00)
struct Cofactors {
    float p10, p14, p18, p1c, p20, p24, p28, p2c, p30, p34, p38, p3c;  // pair products of rows 2 and 3
    float c0, c1, c2, c3;

    explicit Cofactors(const float* m)
    {
        p10 = m[15] * m[10];
        p14 = m[11] * m[14];
        p18 = m[6] * m[15];
        p1c = m[7] * m[14];
        p20 = m[6] * m[11];
        p24 = m[7] * m[10];
        p28 = m[2] * m[15];
        p2c = m[3] * m[14];
        p30 = m[2] * m[11];
        p34 = m[3] * m[10];
        p38 = m[2] * m[7];
        p3c = m[3] * m[6];
        c0 = ((m[13] * p20 + m[9] * p1c) + m[5] * p10) - ((m[13] * p24 + m[9] * p18) + m[5] * p14);
        c1 = ((m[13] * p34 + m[9] * p28) + m[1] * p14) - ((m[13] * p30 + m[9] * p2c) + m[1] * p10);
        c2 = ((m[13] * p38 + m[5] * p2c) + m[1] * p18) - ((m[13] * p3c + m[5] * p28) + m[1] * p1c);
        c3 = ((m[9] * p3c + m[5] * p30) + m[1] * p24) - ((m[9] * p38 + m[5] * p34) + m[1] * p20);
    }
};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Matrix3 (thiscall)

void Matrix3::Rotate(float x, float y, float z) { RotateRows(m, 3, 3, x, y, z); }

void Matrix3::RotateX(float a)  // 0x10030110
{
    float s, c;
    SinCos(a, s, c);
    RotatePair(m + 3, m + 6, s, c);
}

void Matrix3::RotateY(float a)  // 0x10030220
{
    float s, c;
    SinCos(a, s, c);
    RotatePair(m + 6, m + 0, s, c);
}

void Matrix3::RotateZ(float a)  // 0x10030330
{
    float s, c;
    SinCos(a, s, c);
    RotatePair(m + 0, m + 3, s, c);
}

void Matrix3::SetRotateX(float a)  // 0x10030440
{
    float s, c;
    SinCos(a, s, c);
    m[0] = 1.0f, m[1] = 0.0f, m[2] = 0.0f;
    m[3] = 0.0f, m[4] = c, m[5] = s;
    m[6] = 0.0f, m[7] = -0.0f - s, m[8] = c;
}

void Matrix3::SetRotateY(float a)  // 0x100304b0
{
    float s, c;
    SinCos(a, s, c);
    m[0] = c, m[1] = 0.0f, m[2] = -0.0f - s;
    m[3] = 0.0f, m[4] = 1.0f, m[5] = 0.0f;
    m[6] = s, m[7] = 0.0f, m[8] = c;
}

void Matrix3::SetRotateZ(float a)  // 0x10030520
{
    float s, c;
    SinCos(a, s, c);
    m[0] = c, m[1] = s, m[2] = 0.0f;
    m[3] = -0.0f - s, m[4] = c, m[5] = 0.0f;
    m[6] = 0.0f, m[7] = 0.0f, m[8] = 1.0f;
}

// ---------------------------------------------------------------------------------------------------------------
// Matrix4 (__cdecl members)

Matrix4& __cdecl Matrix4::Invert()  // 0x10030590: adjugate / determinant, no singularity check
{
    const float m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3], m4 = m[4], m5 = m[5], m6 = m[6], m7 = m[7];
    const float m8 = m[8], m9 = m[9], m10 = m[10], m11 = m[11], m12 = m[12], m13 = m[13], m14 = m[14], m15 = m[15];
    const Cofactors k(m);
    float r[16];
    r[0] = k.c0;
    r[1] = k.c1;
    r[2] = k.c2;
    r[3] = k.c3;
    r[4] = ((m12 * k.p24 + m8 * k.p18) + m4 * k.p14) - ((m12 * k.p20 + m8 * k.p1c) + m4 * k.p10);
    r[5] = ((m0 * k.p10 + m12 * k.p30) + m8 * k.p2c) - ((m0 * k.p14 + m12 * k.p34) + m8 * k.p28);
    r[6] = ((m0 * k.p1c + m12 * k.p3c) + m4 * k.p28) - ((m0 * k.p18 + m12 * k.p38) + m4 * k.p2c);
    r[7] = ((m0 * k.p20 + m8 * k.p38) + m4 * k.p34) - ((m0 * k.p24 + m8 * k.p3c) + m4 * k.p30);
    // pair products of rows 0 and 1
    const float q4_13 = m4 * m13, q12_5 = m12 * m5, q4_9 = m4 * m9, q8_5 = m8 * m5, q0_13 = m0 * m13;
    const float q12_1 = m12 * m1, q12_9 = m12 * m9, q0_9 = m0 * m9, q8_1 = m8 * m1, q8_13 = m8 * m13;
    const float q0_5 = m0 * m5, q4_1 = m4 * m1;
    r[8] = ((q12_5 * m11 + m7 * q8_13) + q4_9 * m15) - ((m7 * q12_9 + q4_13 * m11) + q8_5 * m15);
    r[9] = ((m3 * q12_9 + q0_13 * m11) + q8_1 * m15) - ((q12_1 * m11 + m3 * q8_13) + q0_9 * m15);
    r[10] = ((q12_1 * m7 + m3 * q4_13) + q0_5 * m15) - ((m3 * q12_5 + q0_13 * m7) + q4_1 * m15);
    r[11] = ((m3 * q8_5 + q0_9 * m7) + q4_1 * m11) - ((m3 * q4_9 + q8_1 * m7) + q0_5 * m11);
    r[12] = ((q8_5 * m14 + m6 * q12_9) + q4_13 * m10) - ((q4_9 * m14 + m6 * q8_13) + q12_5 * m10);
    r[13] = ((q0_9 * m14 + m2 * q8_13) + q12_1 * m10) - ((q8_1 * m14 + m2 * q12_9) + q0_13 * m10);
    r[14] = ((m2 * q12_5 + q0_13 * m6) + q4_1 * m14) - ((q12_1 * m6 + m2 * q4_13) + q0_5 * m14);
    r[15] = ((m2 * q4_9 + q8_1 * m6) + q0_5 * m10) - ((m2 * q8_5 + q0_9 * m6) + q4_1 * m10);
    const float inv = 1.0f / (((k.c2 * m8 + m0 * k.c0) + k.c3 * m12) + k.c1 * m4);
    for (int i = 0; i < 16; ++i) m[i] = r[i] * inv;
    return *this;
}

// 0x10030e00: the expansion along row 0 in x87 (the cofactors are SSE floats)
double __cdecl Matrix4_Determinant(const Matrix4* mat)
{
    const float* m = mat->m;
    const Cofactors k(m);
    return ((double(m[12]) * k.c3 + double(m[8]) * k.c2) + double(m[4]) * k.c1) + double(m[0]) * k.c0;
}

float __cdecl Matrix4::GetDeterminant() const { return static_cast<float>(Matrix4_Determinant(this)); }

void __cdecl Matrix4::Rotate(float x, float y, float z) { RotateRows(m, 4, 4, x, y, z); }

void __cdecl Matrix4::SetRotAxisAngle(const Vector3& axis, float a)  // 0x10031480
{
    float s, c;
    SinCos(a, s, c);
    const float x = axis.x, y = axis.y, z = axis.z;
    const float t = 1.0f - c;
    const float xy = (x * y) * t, xz = (x * z) * t, yz = (z * y) * t;
    m[0] = (x * x) * t + c;
    m[1] = z * s + xy;
    m[2] = xz - y * s;
    m[4] = xy - z * s;
    m[5] = (y * y) * t + c;
    m[6] = x * s + yz;
    m[8] = y * s + xz;
    m[9] = yz - x * s;
    m[10] = (z * z) * t + c;
}

void __cdecl Matrix4::RotateX(float a)  // 0x100315a0
{
    float s, c;
    SinCos(a, s, c);
    RotatePair(m + 4, m + 8, s, c);
}

void __cdecl Matrix4::RotateY(float a)  // 0x100316b0
{
    float s, c;
    SinCos(a, s, c);
    RotatePair(m + 8, m + 0, s, c);
}

void __cdecl Matrix4::RotateZ(float a)  // 0x100317c0
{
    float s, c;
    SinCos(a, s, c);
    RotatePair(m + 0, m + 4, s, c);
}

namespace {
void ClearRest(float* m)  // column 3 and row 3 of the identity
{
    m[3] = m[7] = m[11] = 0.0f;
    m[12] = m[13] = m[14] = 0.0f;
    m[15] = 1.0f;
}
}  // namespace

void __cdecl Matrix4::SetRotateX(float a, bool keepRest)  // 0x100318d0
{
    float s, c;
    SinCos(a, s, c);
    m[0] = 1.0f, m[1] = 0.0f, m[2] = 0.0f;
    m[4] = 0.0f, m[5] = c, m[6] = s;
    m[8] = 0.0f, m[9] = -0.0f - s, m[10] = c;
    if (!keepRest) ClearRest(m);
}

void __cdecl Matrix4::SetRotateY(float a, bool keepRest)  // 0x10031970
{
    float s, c;
    SinCos(a, s, c);
    m[0] = c, m[1] = 0.0f, m[2] = -0.0f - s;
    m[4] = 0.0f, m[5] = 1.0f, m[6] = 0.0f;
    m[8] = s, m[9] = 0.0f, m[10] = c;
    if (!keepRest) ClearRest(m);
}

void __cdecl Matrix4::SetRotateZ(float a, bool keepRest)  // 0x10031a10
{
    float s, c;
    SinCos(a, s, c);
    m[0] = c, m[1] = s, m[2] = 0.0f;
    m[4] = -0.0f - s, m[5] = c, m[6] = 0.0f;
    m[8] = 0.0f, m[9] = 0.0f, m[10] = 1.0f;
    if (!keepRest) ClearRest(m);
}

void __cdecl Matrix4::SetRotFromQuaternion(const Quaternion& q)  // 0x10031ab0
{
    QuaternionRows(m, q);
    ClearRest(m);
}

void __cdecl Matrix4::SetFromPosRot(const Vector3& pos, const Quaternion& q)  // 0x10031c10
{
    QuaternionRows(m, q);
    X87Copy(m[12], pos.x);
    m[13] = pos.y;
    m[14] = pos.z;
    m[3] = m[7] = m[11] = 0.0f;
    m[15] = 1.0f;
}

// ---------------------------------------------------------------------------------------------------------------
// Quaternion (thiscall)

namespace {

// the normalisation that ends both Set(Matrix) (0x1003b38f): the length in x87, the scaling in SSE
void Normalize4(Quaternion& q)
{
    const double l2 = ((double(q.x) * q.x + double(q.y) * q.y) + double(q.z) * q.z) + double(q.w) * q.w;
    const float inv = 1.0f / static_cast<float>(std::sqrt(l2));
    q.x = inv * q.x;
    q.y = inv * q.y;
    q.z = inv * q.z;
    q.w = inv * q.w;
}

}  // namespace

void Quaternion::Set(float ax, float ay, float az)  // 0x1003afd0: from Rx * (Ry * Rz)
{
    Matrix3 rx = {{1, 0, 0, 0, 1, 0, 0, 0, 1}}, ry = rx, rz = rx;
    rx.Rotate(ax, 0.0f, 0.0f);
    ry.Rotate(0.0f, ay, 0.0f);
    rz.Rotate(0.0f, 0.0f, az);
    Matrix3 t;
    Mul3(ry.m, rz.m, t.m);
    Mul3(rx.m, t.m, t.m);
    Set(t);
}

void Quaternion::Set(const Matrix3& mat)  // 0x1003b180
{
    const float* m = mat.m;
    const float m0 = m[0], m4 = m[4], m8 = m[8];
    const float d48 = m4 + m8;
    const float tr = m0 + d48;
    if (tr >= 0.0f) {  // NaN takes the other branch
        const double r = std::sqrt(double(tr) + 1.0);
        const float h = 0.5f / static_cast<float>(r);
        w = static_cast<float>(r * 0.5);
        x = (m[5] - m[7]) * h;
        y = (m[6] - m[2]) * h;
        z = (m[1] - m[3]) * h;
    } else {
        int i = (m4 > m0) ? 1 : 0;
        if (m8 > m[i * 4]) i = 2;
        if (i == 0) {
            const double r = std::sqrt((double(m0) - double(d48)) + 1.0);
            const float h = 0.5f / static_cast<float>(r);
            x = static_cast<float>(r * 0.5);
            y = (m[3] + m[1]) * h;
            z = (m[6] + m[2]) * h;
            w = (m[5] - m[7]) * h;
        } else if (i == 1) {
            const double r = std::sqrt((double(m4) - (double(m0) + double(m8))) + 1.0);
            const float h = 0.5f / static_cast<float>(r);
            y = static_cast<float>(r * 0.5);
            z = (m[7] + m[5]) * h;
            x = (m[3] + m[1]) * h;
            w = (m[6] - m[2]) * h;
        } else {
            const double r = std::sqrt((double(m8) - (double(m0) + double(m4))) + 1.0);
            const float h = 0.5f / static_cast<float>(r);
            z = static_cast<float>(r * 0.5);
            x = (m[6] + m[2]) * h;
            y = (m[7] + m[5]) * h;
            w = (m[1] - m[3]) * h;
        }
    }
    Normalize4(*this);
}

void Quaternion::Set(const Matrix4& mat)  // 0x1003b420
{
    const float* m = mat.m;
    const float tr = (m[0] + m[5]) + m[10];
    if (tr > 1e-6f) {
        const double r = std::sqrt(double(tr) + 1.0);
        const float h = 0.5f / static_cast<float>(r);
        w = static_cast<float>(r * 0.5);
        x = (m[6] - m[9]) * h;
        y = (m[8] - m[2]) * h;
        z = (m[1] - m[4]) * h;
    } else {
        static const int next[3] = {1, 2, 0};
        int i = (m[5] > m[0]) ? 1 : 0;
        if (m[10] > m[i * 5]) i = 2;
        const int j = next[i], k = next[j];
        const double r = std::sqrt((double(m[i * 5]) - (double(m[k * 5]) + double(m[j * 5]))) + 1.0);
        const float rf = static_cast<float>(r);
        const float h = (std::fabs(r) > double(1e-6f)) ? 0.5f / rf : rf;
        float q[4];
        q[i] = static_cast<float>(0.5 * r);
        q[3] = (m[j * 4 + k] - m[k * 4 + j]) * h;
        q[j] = (m[j * 4 + i] + m[i * 4 + j]) * h;
        q[k] = (m[k * 4 + i] + m[i * 4 + k]) * h;
        x = q[0];
        y = q[1];
        z = q[2];
        w = q[3];
    }
    Normalize4(*this);
}

Quaternion& Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t)  // 0x1003b650
{
    if (a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z) {
        x = a.x;
        X87Copy(y, a.y);
        X87Copy(z, a.z);
        X87Copy(w, a.w);
        return *this;
    }
    const float ax = a.x;
    float bx = b.x, by = b.y, bz = b.z, bw = b.w;
    float dot = ((bx * ax + bw * a.w) + a.z * bz) + by * a.y;
    if (0.0f > dot) {
        dot = -0.0f - dot;
        bx = -0.0f - bx;
        by = -0.0f - by;
        bz = -0.0f - bz;
        bw = -0.0f - bw;
    }
    float s0, s1;
    if (1.0f - dot > 0.001f) {
        SlerpWeights(dot, t, s0, s1);
    } else {
        s1 = t;
        s0 = 1.0f - t;
    }
    x = ax * s0 + s1 * bx;
    y = s1 * by + s0 * a.y;
    z = a.z * s0 + s1 * bz;
    w = s1 * bw + s0 * a.w;
    return *this;
}

Quaternion& Quaternion::Update(const Vector3& v, float dt)  // 0x1003b850: q += (q * (v, 0)) * 0.5 * dt
{
    const float a0 = x, a1 = y, a2 = z, a3 = w;
    const float b0 = v.x, b1 = v.y, b2 = v.z, b3 = 0.0f;
    // the quaternion product (0x1003ae10)
    const float o0 = (((b1 * a2) - (b2 * a1)) + (a0 * b3)) + (b0 * a3);
    const float o1 = (((a0 * b2) - (b0 * a2)) + (a1 * b3)) + (a3 * b1);
    const float o2 = (((b0 * a1) - (a0 * b1)) + (a2 * b3)) + (b2 * a3);
    const float o3 = (b3 * a3) - (((a1 * b1) + (a2 * b2)) + (a0 * b0));
    x = a0 + (o0 * 0.5f) * dt;
    y = a1 + (o1 * 0.5f) * dt;
    z = a2 + (o2 * 0.5f) * dt;
    w = a3 + (o3 * 0.5f) * dt;
    return *this;
}

Vector3 Quaternion::GetEulerAngles() const  // 0x1003b920
{
    const double l2 = ((double(x) * x + double(y) * y) + double(w) * w) + double(z) * z;
    const float inv = 1.0f / static_cast<float>(std::sqrt(l2));
    const float nx = inv * x, ny = inv * y, nz = inv * z, nw = inv * w;
    const float y2 = ny * 2.0f, z2 = nz * 2.0f;
    const float sp = -0.0f - (z2 * nx - y2 * nw);
    Vector3 out;
    if (!(-0.99999f > sp) && !(sp > 0.99999f)) {  // NaN takes this branch
        out.y = Asin(sp);
        const double nx2 = double(nx) + nx;
        out.x = Atan2(double(ny) * z2 + nx2 * nw, 1.0 - (nx2 * nx + double(y2) * ny));
        out.z = Atan2(double(nx) * y2 + double(z2) * nw, 1.0 - (double(y2) * ny + double(z2) * nz));
    } else {
        const float c = 1.0f - ((nx * 2.0f) * nx + z2 * nz);
        out.x = 0.0f;
        out.y = 1.5707964f;
        out.z = Acos(!(c > -1.0f) ? -1.0f : (!(c > 1.0f) ? c : 1.0f));
    }
    return out;
}

namespace {

// the shared part of the Set*Axis functions (0x1003bb00..0x1003c0a0): n = |axis|, c = a cross product with up
// (made unit), t = a cross product of n and c; false when |c| <= 0.0001 (the callers then use defaults)
bool UnitCross(float* c)
{
    if (!(Jet_SqrtF(Len2(c)) > 0.0001)) return false;
    Normalize(c, c);
    return true;
}

// c = n x up and t = c x n, the variant of SetYAxis, SetZAxis and SetViewVector
bool CrossNU(const float* n, const Vector3& up, float* c, float* t)
{
    c[0] = up.z * n[1] - up.y * n[2];
    c[1] = up.x * n[2] - up.z * n[0];
    c[2] = up.y * n[0] - up.x * n[1];
    if (!UnitCross(c)) return false;
    t[0] = n[2] * c[1] - n[1] * c[2];
    t[1] = n[0] * c[2] - n[2] * c[0];
    t[2] = n[1] * c[0] - n[0] * c[1];
    return true;
}

void Rows(Matrix3& m, const float* r0, const float* r1, const float* r2)
{
    for (int j = 0; j < 3; ++j) {
        m.m[j] = r0[j];
        m.m[3 + j] = r1[j];
        m.m[6 + j] = r2[j];
    }
}

const float kX[3] = {1.0f, 0.0f, 0.0f};
const float kY[3] = {0.0f, 1.0f, 0.0f};

}  // namespace

Quaternion& Quaternion::SetXAxis(const Vector3& axis, const Vector3& up)  // 0x1003bb00: rows (n, c, t)
{
    float n[3], c[3], t[3];
    Normalize(&axis.x, n);
    c[0] = up.y * n[2] - up.z * n[1];
    c[1] = up.z * n[0] - up.x * n[2];
    c[2] = up.x * n[1] - up.y * n[0];
    Matrix3 m;
    if (UnitCross(c)) {
        t[0] = c[2] * n[1] - c[1] * n[2];
        t[1] = c[0] * n[2] - c[2] * n[0];
        t[2] = c[1] * n[0] - c[0] * n[1];
        Rows(m, n, c, t);
    } else {
        Rows(m, n, kY, kX);
    }
    Set(m);
    return *this;
}

Quaternion& Quaternion::SetYAxis(const Vector3& axis, const Vector3& up)  // 0x1003bc70: rows (c, n, t)
{
    float n[3], c[3], t[3];
    Normalize(&axis.x, n);
    Matrix3 m;
    if (CrossNU(n, up, c, t))
        Rows(m, c, n, t);
    else
        Rows(m, kX, n, kY);
    Set(m);
    return *this;
}

Quaternion& Quaternion::SetZAxis(const Vector3& axis, const Vector3& up)  // 0x1003bdd0: rows (t, c, n)
{
    float n[3], c[3], t[3];
    Normalize(&axis.x, n);
    Matrix3 m;
    if (CrossNU(n, up, c, t))
        Rows(m, t, c, n);
    else
        Rows(m, kX, kY, n);
    Set(m);
    return *this;
}

Quaternion& Quaternion::SetViewVector(const Vector3& view, const Vector3& up)  // 0x1003bf30: rows (c, t, -n)
{
    float n[3], c[3], t[3];
    Normalize(&view.x, n);
    const float back[3] = {-0.0f - n[0], -0.0f - n[1], -0.0f - n[2]};
    Matrix3 m;
    if (CrossNU(n, up, c, t))
        Rows(m, c, t, back);
    else
        Rows(m, kX, kY, back);
    Set(m);
    return *this;
}

}  // namespace Jet
