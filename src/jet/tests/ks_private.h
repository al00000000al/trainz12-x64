// Private statics of our KernelStandard_7c.dll for the diff tests: the original's are at fixed offsets from its base;
// ours are looked up by name in the linker map (KernelStandard_7c.map, copied next to the DLL). nullptr when the map
// or the symbol is missing: the caller then leaves that static out of the comparison.
#pragma once

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

// `piece` is a unique part of the mangled name, e.g. "logFlush@" (anonymous-namespace names carry a hash)
inline void* OurPrivate(const char* dir, HMODULE ours, const char* piece)
{
    FILE* f = std::fopen((std::string(dir) + "\\KernelStandard_7c.map").c_str(), "r");
    if (!f) return nullptr;
    char line[1024];
    unsigned long va = 0;
    while (std::fgets(line, sizeof line, f)) {
        if (!std::strstr(line, piece)) continue;
        char sym[512];
        unsigned sec, off;
        unsigned long v;
        if (std::sscanf(line, " %x:%x %511s %lx", &sec, &off, sym, &v) == 4) {
            va = v;
            break;
        }
    }
    std::fclose(f);
    return va ? reinterpret_cast<char*>(ours) + (va - 0x10000000) : nullptr;
}
