// Jet::GetBaryCentricWeights (KernelStandard 0x1003da70): the weights of p in triangle (a, b, c) when p is
// projected along dir (a ray/triangle test without the distance). Transcribed from the assembly: every step is
// an SSE single-precision operation in this order; only the degeneracy test runs in x87 against the double
// constants +-1e-6.
#include <jet/Math.h>

namespace Jet {

Vector3 __cdecl GetBaryCentricWeights(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& p,
                                      const Vector3& dir)
{
    const float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
    const float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
    const float px = dir.y * e2z - dir.z * e2y;  // dir x e2
    const float py = dir.z * e2x - dir.x * e2z;
    const float pz = dir.x * e2y - dir.y * e2x;
    const float det = (py * e1y + pz * e1z) + px * e1x;
    if (-1e-06 < double(det) && double(det) < 1e-06) return Vector3(0.0f, 0.0f, 0.0f);

    const float inv = 1.0f / det;
    const float tx = p.x - a.x, tz = p.z - a.z, ty = p.y - a.y;
    const float u = ((tz * pz + ty * py) + tx * px) * inv;
    const float qx = ty * e1z - tz * e1y;  // t x e1
    const float qy = tz * e1x - tx * e1z;
    const float qz = tx * e1y - ty * e1x;
    const float v = ((dir.x * qx + qz * dir.z) + qy * dir.y) * inv;
    return Vector3((1.0f - u) - v, u, v);
}

}  // namespace Jet
