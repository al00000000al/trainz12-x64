// KernelStandard functions whose exported signature is awkward to declare (template result types),
// bound by mangled name at run time.
#include <jet/KernelStandard.h>

#include <windows.h>

#if !defined(JET_KS_STATIC)  // the x64 harness links a stand-in that defines File::Exists directly
namespace Jet {

// static ResultClass<&File::results, File::Results> File::Exists(const char*): the result (one int, 0 = ok) is
// returned in memory through a hidden first argument.
bool File::Exists(const char* path)
{
    using Fn = int*(__cdecl*)(int* result, const char* path);
    static Fn fn = reinterpret_cast<Fn>(GetProcAddress(
        GetModuleHandleA("KernelStandard_7c.dll"),
        "?Exists@File@Jet@@SA?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@PBD@Z"));
    int result = -1;
    if (fn) fn(&result, path);
    return result == 0;
}

}  // namespace Jet
#endif
