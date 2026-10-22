// The game's C runtime (msvcr80.dll, loaded with the original KernelStandard): the locale- and rounding-dependent
// functions our units must share with the original code still running (atof's rounding, the ctype tables a
// setlocale changes, wcstombs's code page, the stdout buffer of printf). Resolved by name on first use.
#pragma once

#include <windows.h>

#include <psapi.h>

#include <cstdarg>
#include <cstddef>

namespace Jet::Crt80 {

// The module is found among the loaded ones by its base name: loaded side-by-side (Microsoft.VC80.CRT), it is not
// found by GetModuleHandle outside the activation context of the module that loaded it.
inline HMODULE Module()
{
    static const HMODULE crt = [] {
        HMODULE mods[1024];
        DWORD bytes = 0;
        if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &bytes)) return HMODULE(nullptr);
        for (DWORD i = 0; i < bytes / sizeof(HMODULE); ++i) {
            char name[MAX_PATH];
            if (K32GetModuleBaseNameA(GetCurrentProcess(), mods[i], name, MAX_PATH) && lstrcmpiA(name, "msvcr80.dll") == 0)
                return mods[i];
        }
        return HMODULE(nullptr);
    }();
    return crt;
}

inline void* Proc(const char* name)
{
    const HMODULE crt = Module();
    return crt ? reinterpret_cast<void*>(GetProcAddress(crt, name)) : nullptr;
}

#define JET_CRT80(ret, name, params, args)                                                      \
    inline ret name params                                                                       \
    {                                                                                            \
        static auto const f = reinterpret_cast<ret(__cdecl*) params>(Proc(#name));              \
        return f args;                                                                           \
    }

JET_CRT80(int, isspace, (int c), (c))
JET_CRT80(int, atoi, (const char* s), (s))
JET_CRT80(double, atof, (const char* s), (s))
JET_CRT80(long, strtol, (const char* s, char** end, int base), (s, end, base))
JET_CRT80(unsigned long, strtoul, (const char* s, char** end, int base), (s, end, base))
JET_CRT80(char*, _strlwr, (char* s), (s))
JET_CRT80(int, _stricmp, (const char* a, const char* b), (a, b))
JET_CRT80(size_t, wcstombs, (char* dst, const wchar_t* src, size_t n), (dst, src, n))
JET_CRT80(size_t, mbstowcs, (wchar_t* dst, const char* src, size_t n), (dst, src, n))
JET_CRT80(char*, strncpy, (char* dst, const char* src, size_t n), (dst, src, n))
JET_CRT80(char*, getenv, (const char* name), (name))
JET_CRT80(int, vprintf, (const char* format, va_list ap), (format, ap))
JET_CRT80(double, strtod, (const char* s, char** end), (s, end))
JET_CRT80(void, _invalid_parameter_noinfo, (), ())
#undef JET_CRT80

// sscanf of one "%s" (the only form KernelStandard uses)
inline int sscanf_s1(const char* s, const char* format, char* out)
{
    static auto const f = reinterpret_cast<int(__cdecl*)(const char*, const char*, ...)>(Proc("sscanf"));
    return f(s, format, out);
}

// msvcr80's operator new / delete (what the original's std::allocator<char> used)
inline void* operator_new(size_t n)
{
    static auto const f = reinterpret_cast<void*(__cdecl*)(size_t)>(Proc("??2@YAPAXI@Z"));
    return f(n);
}
inline void operator_delete(void* p)
{
    static auto const f = reinterpret_cast<void(__cdecl*)(void*)>(Proc("??3@YAXPAX@Z"));
    f(p);
}

inline int printf(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    const int r = vprintf(format, ap);
    va_end(ap);
    return r;
}

}  // namespace Jet::Crt80
