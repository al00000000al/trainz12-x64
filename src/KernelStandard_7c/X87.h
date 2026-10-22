// Helpers for matching the original's mixed x87/SSE float code bit for bit.
//
// MSVC 2005 compiled many float copies as x87 fld/fstp, which turns a signalling NaN into a quiet one; X87Copy
// does the same on Win32 (and is a plain copy on x64, where the whole program is rebuilt).
#pragma once

namespace Jet {

inline void X87Copy(float& dst, const float& src)
{
#if defined(_M_IX86)
    const float* s = &src;
    float* d = &dst;
    __asm {
        mov ecx, s
        mov eax, d
        fld dword ptr [ecx]
        fstp dword ptr [eax]
    }
#else
    dst = src;
#endif
}

}  // namespace Jet
