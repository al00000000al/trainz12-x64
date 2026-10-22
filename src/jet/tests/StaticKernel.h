// The x64 harness links Kernel_7c statically (JET_KS_STATIC), so nothing attaches it the way loading the DLL does.
// JET_STATIC_KERNEL at the top of main attaches it for the run; in the 32-bit build it expands to nothing.
#pragma once

#if defined(JET_KS_STATIC)
namespace Jet {
void StaticModuleAttach();
void StaticModuleDetach();
}  // namespace Jet

struct StaticKernel {
    StaticKernel() { Jet::StaticModuleAttach(); }
    ~StaticKernel() { Jet::StaticModuleDetach(); }
};
#define JET_STATIC_KERNEL StaticKernel staticKernel
#else
#define JET_STATIC_KERNEL static_assert(true)
#endif
