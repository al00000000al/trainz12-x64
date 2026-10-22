// Jet::UId (Kernel_7c): a 16-byte GUID identifying plugins, plugin types and resource factories.
// Many members of the original are __cdecl (mangled QBA/QAA), not __thiscall: keep them that way.
#pragma once

#include <jet/PString.h>

namespace Jet {

class DynamicBuffer;

class UId {
public:
    unsigned long data1;
    unsigned short data2;
    unsigned short data3;
    unsigned char data4[8];

    UId();                                                                        // ??0UId@Jet@@QAE@XZ (leaves data as is)
    UId(int);                                                                     // ??0UId@Jet@@QAE@H@Z (zero)
    UId(unsigned long d1, unsigned short d2, unsigned short d3);                  // ??0UId@Jet@@QAE@KGG@Z
    UId(unsigned long d1, unsigned short d2, unsigned short d3, const unsigned char* const d4);  // ??0UId@Jet@@QAE@KGGQBE@Z
    UId(const char* text);                                                        // ??0UId@Jet@@QAE@PBD@Z

    UId& operator=(const UId& o);
    void Zero();

    bool __cdecl operator==(const UId& o) const;
    bool __cdecl operator!=(const UId& o) const;
    bool __cdecl operator<(const UId& o) const;
    bool __cdecl operator>(const UId& o) const;
    bool __cdecl operator<=(const UId& o) const;
    bool __cdecl operator>=(const UId& o) const;
    __cdecl operator bool() const;
    bool __cdecl operator!() const;

    unsigned int __cdecl GetHashID() const;

    bool __cdecl Read(DynamicBuffer& buf);
    bool __cdecl Write(DynamicBuffer& buf);
    PString __cdecl GetString() const;  // "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"
    void __cdecl SetRnd();              // CoCreateGuid, or Time::Ms + Rand when that fails
};

static_assert(sizeof(UId) == 16);

extern const UId UIdNull;  // ?UIdNull@Jet@@3VUId@1@B

}  // namespace Jet
