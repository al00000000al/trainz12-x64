// The vsnprintf buffer of AnsiString::Add / Set and MultiByteString::Add / Set (0x10003120): 0x400 bytes on the
// stack, then heap buffers of 0x8000, 0x20000, ... bytes until the text fits (empty past 0x7ffffff).
#pragma once

#include <jet/KernelStandard.h>

#include "Crt80.h"

namespace Jet {

struct StringFormat {
    char* text;
    char local[0x400];
    int bytes;
    int count;

    StringFormat(const char* fmt, va_list args)
    {
        int n = Crt80::_vsnprintf(local, 0x3ff, fmt, args);
        if (n >= 0) {
            local[n] = 0;
            text = local;
            bytes = n;
            count = n;
            return;
        }
        unsigned size = 0x8000;
        text = static_cast<char*>(Alloc::New(0x8000));
        n = Crt80::_vsnprintf(text, 0x7fff, fmt, args);
        while (n <= 0) {
            if (text) Alloc::Delete(text);
            size *= 4;
            if (static_cast<int>(size) > 0x7ffffff) {
                text = local;
                local[0] = 0;
                bytes = 0;
                count = 0;
                return;
            }
            text = static_cast<char*>(Alloc::New(size));
            n = Crt80::_vsnprintf(text, size - 1, fmt, args);
        }
        text[n] = 0;
        bytes = n;
        count = n;
    }
    ~StringFormat()
    {
        if (text != local && text) Alloc::Delete(text);
    }
    StringFormat(const StringFormat&) = delete;
};

}  // namespace Jet
