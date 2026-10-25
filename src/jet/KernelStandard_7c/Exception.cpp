// Unit U12a of KernelStandard_7c: Exception (0x10008510..0x100085a0, 0x100085e0, 0x10014270..0x10014370,
// 0x1001caa0..0x1001cc3c) and Debugger (0x1001bcb0..0x1001bcf0). HardwareException and ExceptionHandler are U12c.
// The texts are formatted with msvcr80's vsprintf, as in the original (its %f follows that CRT's locale).
#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <windows.h>

#include <cstdarg>
#include <cstring>

namespace Jet {

static const char kNoCallStack[] = "No call stack available\r\n";

Exception::Exception() : hardware(false), line(0), file(nullptr)
{
    description[0] = 0;
    reason[0] = 0;
}

Exception::Exception(const char* file_, int line_) : hardware(false), line(line_), file(file_)
{
    reason[0] = 0;
    std::strcpy(description, kNoCallStack);
}

Exception::Exception(const char* file_, int line_, const char* message) : hardware(false), line(line_), file(file_)
{
    std::strcpy(reason, message);
    std::strcpy(description, kNoCallStack);
}

// copies every byte of both texts, as the original's memcpy / rep movsd
Exception::Exception(const Exception& o) : hardware(o.hardware), line(o.line), file(o.file)
{
    std::memcpy(description, o.description, sizeof description);
    std::memcpy(reason, o.reason, sizeof reason);
}

Exception& Exception::operator=(const Exception& o)
{
    std::memcpy(description, o.description, sizeof description);
    std::memcpy(reason, o.reason, sizeof reason);
    hardware = o.hardware;
    line = o.line;
    file = o.file;
    return *this;
}

Exception::~Exception() {}

const char* __cdecl Exception::Class() const { return "Unknown"; }
bool __cdecl Exception::IsHardwareException() const { return hardware; }
bool __cdecl Exception::HardwareException() const { return hardware; }
const char* __cdecl Exception::Description() const { return description; }
const char* __cdecl Exception::Reason() const { return reason; }
int __cdecl Exception::Line() const { return line; }
const char* __cdecl Exception::File() const { return file; }
void __cdecl Exception::SetHardwareException(bool on) { hardware = on; }
void __cdecl Exception::Log(int) {}

void __cdecl Exception::InitMembers()
{
    description[0] = 0;
    reason[0] = 0;
    file = nullptr;
    line = 0;
    hardware = false;
}

void __cdecl Exception::VSetReasonf(const char* fmt, char* args) { Crt80::vsprintf(reason, fmt, args); }

void __cdecl Exception::SetReasonf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    Crt80::vsprintf(reason, fmt, ap);
    va_end(ap);
}

void __cdecl Exception::AddReasonf(const char* fmt, ...)
{
    std::strcat(reason, "  -  ");
    va_list ap;
    va_start(ap, fmt);
    Crt80::vsprintf(reason + std::strlen(reason), fmt, ap);
    va_end(ap);
}

void __cdecl Exception::VSetDescriptionf(const char* fmt, char* args) { Crt80::vsprintf(description, fmt, args); }

void __cdecl Exception::SetDescriptionf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    Crt80::vsprintf(description, fmt, ap);
    va_end(ap);
}

// ---- Debugger

void __cdecl Debugger::Output(Level, const char* text) { OutputDebugStringA(text); }
bool __cdecl Debugger::IsReadValid(void* p, unsigned int size) { return IsBadReadPtr(p, size) == 0; }
bool __cdecl Debugger::IsDebuggerAttached() { return IsDebuggerPresent() == 1; }
void __cdecl Debugger::Break() {}
void __cdecl Debugger::LogCallStack() {}
Debugger& Debugger::operator=(const Debugger&) { return *this; }

}  // namespace Jet
