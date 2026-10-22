// Build switches shared by the Jet headers.
//
// JET_KS_IMPORT marks KernelStandard_7c members that the original modules import through their import table. The
// 32-bit drop-in build links the game's KernelStandard_7c.dll; the x64 harness defines JET_KS_STATIC and links its
// stand-in (tests/x64/KernelStandardStub.cpp) into the same image, so the marker becomes empty there.
//
// JET_LAYOUT32 checks a size or offset of the 32-bit TS12 binaries. These layouts are the ABI other 32-bit modules
// see; an x64 build has its own (pointers are 8 bytes) and prints them instead (tests/golden_kernel.cpp).
#pragma once

#if defined(JET_KS_STATIC) || defined(JET_KS_BUILD)  // JET_KS_BUILD: compiling KernelStandard_7c itself
#define JET_KS_IMPORT
#else
#define JET_KS_IMPORT __declspec(dllimport)
#endif

// JET_KS_FORWARDED marks KernelStandard data that is still the original's. Every 32-bit build imports it (our
// KernelStandard_7c imports it from KernelStandard_7c_orig.dll); the x64 stand-in defines it.
#if defined(JET_KS_STATIC)
#define JET_KS_FORWARDED
#else
#define JET_KS_FORWARDED __declspec(dllimport)
#endif

#if defined(_WIN64)
#define JET_LAYOUT32(...) static_assert(true)
#else
#define JET_LAYOUT32(...) static_assert(__VA_ARGS__)
#endif

// Size of a KernelStandard object that our code embeds or allocates without seeing its members: the TS12 size
// on 32-bit, and room for an x64 KernelStandard on x64 (provisional until that module exists; its stand-in checks
// that its state fits).
#if defined(_WIN64)
#define JET_OPAQUE_SIZE(size32, size64) (size64)
#else
#define JET_OPAQUE_SIZE(size32, size64) (size32)
#endif
