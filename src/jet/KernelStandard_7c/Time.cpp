// Unit U13 of KernelStandard_7c: Time (0x1000db20..0x1000ddd7). The counter frequency and the frame-lock state are
// ours since U14b; freq is filled by a "lib" initialiser (the original's 0x100506c0 runs right after
// KernelStandardInit), before any other static of the module can read the clock.
#include <jet/KernelStandard.h>

#include <windows.h>
#include <mmsystem.h>

#include <ctime>

#pragma comment(lib, "winmm.lib")

#pragma warning(disable : 4073)  // initializers put in library initialization area
#pragma init_seg(lib)

namespace Jet {

bool Time::frameLock;              // 0x100adb90
unsigned int Time::frameLockCount; // 0x100adb94
double Time::frameLockTime;        // 0x100adb98
double Time::frameLockDelta;       // 0x100adba0
unsigned __int64 Time::freq = Time::QueryCounterFrequency();  // 0x100ae160

Time& Time::operator=(const Time&) { return *this; }  // 0x1000db10 (shared with the other empty classes)

static unsigned __int64 Counter()
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<unsigned __int64>(c.QuadPart);
}

// the original converts both as unsigned 64-bit values (x87, 53-bit precision)
double __cdecl Time::GetSeconds() { return static_cast<double>(Counter()) / static_cast<double>(freq); }
unsigned __int64 __cdecl Time::GetCounterFrequency() { return freq; }
unsigned __int64 __cdecl Time::GetCounter() { return Counter(); }
unsigned __int64 __cdecl Time::Usq() { return Counter(); }
unsigned long __cdecl Time::Us() { return static_cast<unsigned long>(Counter()); }

unsigned __int64 __cdecl Time::QueryCounterFrequency()
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return static_cast<unsigned __int64>(f.QuadPart);
}

double __cdecl Time::FrameLockSeconds() { return frameLock ? frameLockTime : GetSeconds(); }

// frameLockTime * 0.001 truncated through a 64-bit fistp; the low half is returned
unsigned long __cdecl Time::FrameLockMs()
{
    if (frameLock) return static_cast<unsigned long>(static_cast<long long>(frameLockTime * 0.001));
    return timeGetTime();
}

unsigned long __cdecl Time::Ms() { return timeGetTime(); }
void __cdecl Time::Delay(unsigned long ms) { Sleep(ms); }

unsigned __int64 __cdecl Time::TimeStamp()
{
    __time64_t t;
    _time64(&t);
    return static_cast<unsigned __int64>(t);
}

void __cdecl Time::Str(char* out) { _strtime(out); }

void __cdecl Time::HHMM(char* out)  // "HH:MM:SS" -> "HHMM"
{
    char t[16];
    _strtime(t);
    out[0] = t[0];
    out[3] = t[4];
    out[1] = t[1];
    out[2] = t[3];
    out[4] = '\0';
}

void __cdecl Time::SetFrameLockTime(double seconds)
{
    frameLock = true;
    frameLockDelta = seconds;
}

// the time of the frame being entered is the count before the increment
void __cdecl Time::IncFrame()
{
    const double n = static_cast<double>(frameLockCount);
    ++frameLockCount;
    frameLockTime = n * frameLockDelta;
}

}  // namespace Jet
