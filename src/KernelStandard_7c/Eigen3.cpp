// Jet::Eigen3 (KernelStandard 0x1001be40..0x1001c489): eigen decomposition of a symmetric 3x3 matrix.
//
// Transcribed from the assembly, not from the textbook algorithm it follows: the original mixes SSE single
// precision with x87 double precision (53-bit control word) operation by operation, and convergence depends on
// it. Here `float` expressions are its SSE operations and `double` expressions its x87 ones. The 4x unrolled
// scan loops of the original only run for counts above 3, so they are dead for a 3x3 matrix and left out.
#include <jet/Math.h>

#include <cmath>
#include <cstring>

#include "X87.h"

namespace Jet {

Eigen3& Eigen3::operator=(const Eigen3& o)  // 0x1001be40: 15 dwords
{
    std::memcpy(this, &o, sizeof *this);
    return *this;
}

// 0x1001be60/70: the object as a flat float array (index 9-11 are the eigenvalues)
float& Eigen3::operator[](int i) { return reinterpret_cast<float*>(this)[i]; }
float Eigen3::operator[](int i) const { return reinterpret_cast<const float*>(this)[i]; }
float& Eigen3::operator()(int row, int col) { return m[row][col]; }  // 0x1001bea0 (also Matrix)
float Eigen3::operator()(int row, int col) const { return m[row][col]; }
float& Eigen3::Matrix(int row, int col) { return m[row][col]; }

void Eigen3::IncreasingSort()  // 0x1001c390
{
    Tridiagonal();
    QLAlgorithm();
    IncreasingSortEigen();
}

// 0x1001bec0: Householder reduction to tridiagonal form; the matrix becomes the rotation
void Eigen3::Tridiagonal()
{
    const float b = m[0][1], c = m[0][2], d = m[1][1], e = m[1][2], f = m[2][2];
    X87Copy(diag[0], m[0][0]);
    subd[2] = 0.0f;
    float sub1 = e, m22;
    if (c == 0.0f) {
        diag[1] = d;
        diag[2] = f;
        subd[0] = b;
        m[0][0] = 1.0f;
        m[1][1] = 1.0f;
        m[1][2] = 0.0f;
        m[2][1] = 0.0f;
        m22 = 1.0f;
    } else {
        const float length = static_cast<float>(std::sqrt(double(c) * double(c) + double(b) * double(b)));
        subd[0] = length;
        const float inv = 1.0f / length;
        const float cc = inv * c;
        const float bb = inv * b;
        const float q = (f - d) * cc + (e * bb) * 2.0f;
        const float qc = q * cc;
        sub1 = e - q * bb;
        m[0][0] = 1.0f;
        diag[1] = qc + d;
        diag[2] = f - qc;
        m[1][1] = bb;
        m[1][2] = cc;
        m[2][1] = cc;
        m22 = -0.0f - bb;
    }
    m[2][2] = m22;
    m[2][0] = 0.0f;
    m[1][0] = 0.0f;
    m[0][2] = 0.0f;
    m[0][1] = 0.0f;
    subd[1] = sub1;
}

// 0x1001c000: QL with implicit shifts, at most 32 iterations per eigenvalue
bool Eigen3::QLAlgorithm()
{
    for (int l = 0; l < 3; ++l) {
        int iter = 0;
        for (; iter < 32; ++iter) {
            int mm = l;
            for (; mm <= 1; ++mm) {  // x87: the sums are doubles
                const double dd = double(std::fabs(diag[mm])) + double(std::fabs(diag[mm + 1]));
                if (double(std::fabs(subd[mm])) + dd == dd) break;
            }
            if (mm == l) break;

            float g = (diag[l + 1] - diag[l]) / (subd[l] * 2.0f);
            const float r = static_cast<float>(std::sqrt(double(g) * double(g) + 1.0));
            if (g < 0.0f)
                g = subd[l] / (g - r);
            else  // also for NaN, as comiss + jbe
                g = subd[l] / (r + g);
            g = g + (diag[mm] - diag[l]);

            float s = 1.0f, c = 1.0f, p = 0.0f;
            for (int i = mm - 1; i >= l; --i) {
                const float f = subd[i] * s;
                const float b = subd[i] * c;
                double root;  // kept unrounded for subd[i + 1], as on the x87 stack
                float factor;
                if (std::fabs(f) < std::fabs(g) || std::isnan(f) || std::isnan(g)) {  // fcomip + jb
                    const float t = f / g;
                    root = std::sqrt(double(t) * double(t) + 1.0);
                    c = 1.0f / static_cast<float>(root);
                    s = c * t;
                    factor = g;
                } else {
                    const float t = g / f;
                    root = std::sqrt(double(t) * double(t) + 1.0);
                    s = 1.0f / static_cast<float>(root);
                    c = t * s;
                    factor = f;
                }
                subd[i + 1] = static_cast<float>(root * double(factor));
                g = diag[i + 1] - p;
                const float r2 = (diag[i] - g) * s + (b * c) * 2.0f;
                p = s * r2;
                diag[i + 1] = p + g;
                g = c * r2 - b;
                for (int k = 0; k < 3; ++k) {
                    const float a0 = m[k][i], a1 = m[k][i + 1];
                    m[k][i + 1] = a0 * s + a1 * c;
                    m[k][i] = a0 * c - a1 * s;
                }
            }
            diag[l] = diag[l] - p;
            subd[l] = g;
            subd[mm] = 0.0f;
        }
        if (iter == 32) return false;
    }
    return true;
}

// 0x1001c3a0: selection sort of the eigenvalues, swapping the eigenvector columns with them
void Eigen3::IncreasingSortEigen()
{
    for (int i = 0; i < 2; ++i) {
        int idx = i;
        float mn = diag[i];
        for (int k = i + 1; k < 3; ++k)
            if (mn > diag[k]) {
                idx = k;
                mn = diag[k];
            }
        if (idx == i) continue;
        X87Copy(diag[idx], diag[i]);
        diag[i] = mn;
        for (int r = 0; r < 3; ++r) {
            const float t = m[r][i];
            X87Copy(m[r][i], m[r][idx]);
            m[r][idx] = t;
        }
    }
}

}  // namespace Jet
