// KernelStandard_7c math leaves (unit U1, notes/KernelStandard.md): Crc, StdMath, Vector2/3 conversions, the
// integer log/power-of-two helpers, barycentric weights, Eigen3 (3x3 symmetric eigen solver) and OptimalRects
// (a dirty-rectangle set). Declarations match the exported names; layouts are the TS12 ones.
#pragma once

#include <jet/Platform.h>

namespace Jet {

class Vector3;
class Vector4;

class Vector2 {
public:
    Vector2() = default;
    Vector2(float x_, float y_) : x(x_), y(y_) {}
    Vector2(const Vector3& v);
    Vector2(const Vector4& v);
    float x, y;
};

class Vector3 {
public:
    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vector3(const Vector2& v);  // z = 0
    Vector3(const Vector4& v);
    float x, y, z;
};

class Vector4 {
public:
    float x, y, z, w;
};

struct Rect {
    int x, y, w, h;
};

struct Point {
    int x, y;
};

struct Size {
    int w, h;
};

class Crc {
public:
    // MSB-first CRC with polynomial 0x04C11DB7; the seed is complemented on the way in and on the way out
    static unsigned long __cdecl Calc(const void* data, unsigned long size, unsigned long seed);
};

// exponent of the float (biased exponent - 127); ILog2(int) converts the int to float first
int __cdecl IntLog2(float f);
int __cdecl ILog2(float f);
int __cdecl ILog2(int i);
int __cdecl CeilPow2(int i);
int __cdecl FloorPow2(int i);

// weights of `p` in triangle (a, b, c) along `dir` (projection), {0,0,0} when degenerate
Vector3 __cdecl GetBaryCentricWeights(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& p,
                                      const Vector3& dir);

// ---- unit U2 (Math2.cpp): rotations. Matrices are row-major; Matrix3 members are __thiscall, Matrix4 members
// __cdecl (as exported). Angles in radians; sin/cos come from the x87 fsincos rounded to float.
class Quaternion;

class Matrix3 {
public:
    void Rotate(float x, float y, float z);  // this = R(x, y, z) applied to the rows (see Math2.cpp)
    void RotateX(float a);
    void RotateY(float a);
    void RotateZ(float a);
    void SetRotateX(float a);
    void SetRotateY(float a);
    void SetRotateZ(float a);
    float m[9];
};
JET_LAYOUT32(sizeof(Matrix3) == 0x24);

class Matrix4 {
public:
    Matrix4& __cdecl Invert();
    float __cdecl GetDeterminant() const;  // returned unrounded in st(0) on Win32, like the original
    void __cdecl Rotate(float x, float y, float z);  // rows 0-2
    void __cdecl SetRotAxisAngle(const Vector3& axis, float a);  // the upper 3x3 only
    void __cdecl RotateX(float a);
    void __cdecl RotateY(float a);
    void __cdecl RotateZ(float a);
    void __cdecl SetRotateX(float a, bool keepRest);  // keepRest: leave column 3 and row 3 alone
    void __cdecl SetRotateY(float a, bool keepRest);
    void __cdecl SetRotateZ(float a, bool keepRest);
    void __cdecl SetRotFromQuaternion(const Quaternion& q);
    void __cdecl SetFromPosRot(const Vector3& pos, const Quaternion& q);
    float m[16];
};
JET_LAYOUT32(sizeof(Matrix4) == 0x40);

class Quaternion {
public:
    void Set(float x, float y, float z);  // from Euler angles: Rx * (Ry * Rz)
    void Set(const Matrix3& m);
    void Set(const Matrix4& m);
    Quaternion& Slerp(const Quaternion& a, const Quaternion& b, float t);
    Quaternion& Update(const Vector3& w, float dt);  // q += (q * (w, 0)) * 0.5 * dt
    Vector3 GetEulerAngles() const;
    Quaternion& SetXAxis(const Vector3& axis, const Vector3& up);
    Quaternion& SetYAxis(const Vector3& axis, const Vector3& up);
    Quaternion& SetZAxis(const Vector3& axis, const Vector3& up);
    Quaternion& SetViewVector(const Vector3& view, const Vector3& up);
    float x, y, z, w;
};
JET_LAYOUT32(sizeof(Quaternion) == 0x10);

// Table-driven square roots: a seed from s_sqrtLUT and the float's exponent, then Newton steps (x87 in the
// original, at double precision). The float versions return the unrounded double in st(0), like the original.
class StdMath {
public:
    StdMath& operator=(const StdMath&);
    static float __cdecl Sqrt(float x);
    static double __cdecl Sqrt(double x);
    static float __cdecl Sqrt16(float x);  // one Newton step
    static float __cdecl InvSqrt(float x);
    static double __cdecl InvSqrt(double x);
    static float __cdecl InvSqrt16(float x);
    static float __cdecl InvSqrtApprox(float x);  // 0x5f3759df and one Newton step

private:
    friend struct StdMathWork;
    static unsigned int s_sqrtLUT[512];  // read inline by 10 modules: exported data
};

// Eigen3: eigenvalues/vectors of a symmetric 3x3 matrix (Householder tridiagonalisation, then QL with implicit
// shifts, then an increasing sort). 0x3c bytes: matrix (rows of 3) +0, diagonal +0x24, subdiagonal +0x30.
class Eigen3 {
public:
    Eigen3& operator=(const Eigen3& o);
    float& operator()(int row, int col);
    float operator()(int row, int col) const;
    float& operator[](int i);  // eigenvalue i (diagonal after solving)
    float operator[](int i) const;
    void IncreasingSort();

private:
    float& Matrix(int row, int col);
    void Tridiagonal();
    bool QLAlgorithm();
    void IncreasingSortEigen();

    float m[3][3];  // +0x00: the input, then the eigenvectors (columns)
    float diag[3];  // +0x24
    float subd[3];  // +0x30
};
JET_LAYOUT32(sizeof(Eigen3) == 0x3c);

// A set of non-overlapping rectangles: Invalidate adds a rectangle and splits it against those already in the
// set, Combine returns their bounding box. Rect = {x, y, w, h}; w and h must be positive.
class OptimalRects {
public:
    void __cdecl Invalidate(const Rect& r);
    void __cdecl Validate(const Rect& r);  // empty in TS12
    void __cdecl Process();
    Rect __cdecl Combine();

    struct Node;  // {Rect, link}: defined in OptimalRects.cpp

private:
    Node* head;         // +0x00 the set
    Node* tail;         // +0x04
    int count;          // +0x08
    Node* pendingHead;  // +0x0c rectangles still to split in
    Node* pendingTail;  // +0x10
    int pendingCount;   // +0x14
};

}  // namespace Jet
