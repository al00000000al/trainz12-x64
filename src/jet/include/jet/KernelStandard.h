// Declarations of KernelStandard_7c classes that our modules call. Only what is used so far; members are
// declared exactly as exported so the mangled names match the import library.
#pragma once

#include <jet/PString.h>
#include <jet/Result.h>

#include <cstddef>

struct HWND__;
struct _iobuf;  // the CRT's FILE

namespace Jet {

class UnicodeString;
class AnsiString;
class String;

// What the strings' binary Read / Write go through (DynamicBuffer::Read / Write of a string, Streams.cpp):
// {vftable, DynamicBuffer*}, slot 1 moves the bytes (a count is returned).
class BinReader {
public:
    virtual ~BinReader() {}
    virtual unsigned long Read(void* dst, unsigned long size) = 0;
};
class BinWriter {
public:
    virtual ~BinWriter() {}
    virtual unsigned long Write(const void* src, unsigned long size) = 0;
};

// A growable byte buffer with a cursor (KernelStandard unit U9a, Streams.cpp): {data, position, capacity, size
// (the bytes written), increment (the growth step, 0x200 when 0), external (the data is not owned: no writes, not
// freed)}, 0x18 bytes. Users build it inline.
class DynamicBuffer {
public:
    DynamicBuffer(unsigned long capacity = 0, unsigned long increment = 0);
    DynamicBuffer(void* data, unsigned long size, bool external);
    DynamicBuffer(const DynamicBuffer& o);
    DynamicBuffer& operator=(const DynamicBuffer& o);
    ~DynamicBuffer();
    void Clean();   // = the destructor: data freed, sizes and position 0
    void Reset();   // size and position 0
    void Rewind();  // position 0
    void ReInit(unsigned long capacity, unsigned long increment);
    bool Seek(unsigned long position);  // past the capacity: clipped, false
    bool SetSize(unsigned long size);   // past the capacity: clipped, false
    void SetData(void* data, unsigned long size, bool external);
    bool IsReadOnly();
    unsigned long GetSize() const;
    unsigned long GetLength() const;  // the capacity
    unsigned long GetIncrement() const;
    unsigned long GetCurrentPos() const;
    void* GetData() const;
    void* GetCurrentData() const;
    void AddLength(unsigned long n);  // room for n more bytes at the position

    bool Write(short v);
    bool Write(unsigned short v);
    bool Write(signed char v);
    bool Write(unsigned char v);
    bool Write32(int v);
    bool Write32(unsigned int v);
    bool Write(long v);
    bool Write(unsigned long v);
    bool Write(float v);
    bool Write(bool v);  // 4 bytes
    bool Write(const AnsiString& s);
    bool Write(const String& s);
    bool Write(const UnicodeString& s);
    bool Write(const PString& s);
    unsigned long Write(const void* src, unsigned long size, unsigned long count);  // `size` (not the bytes)

    // a read takes what is left: 1-3 bytes of a 4-byte value succeed and leave the rest as it was
    bool Read(short& v) const;
    bool Read(unsigned short& v) const;
    bool Read(signed char& v) const;
    bool Read(unsigned char& v) const;
    bool Read(long& v) const;
    bool Read(unsigned long& v) const;
    bool Read(float& v) const;
    bool Read(bool& v) const;
    bool Read32(int& v) const;  // (zeroed first)
    bool Read32(unsigned int& v) const;
    bool Read(PString& s) const;
    bool Read(AnsiString& s) const;
    bool Read(String& s) const;
    bool Read(UnicodeString& s) const;
    unsigned long Read(void* dst, unsigned long size, unsigned long count) const;  // the bytes read

    unsigned char* data;     // +0x00
    unsigned long position;  // +0x04
    unsigned long capacity;  // +0x08
    unsigned long size;      // +0x0c
    unsigned long increment; // +0x10
    bool external;           // +0x14
};
JET_LAYOUT32(sizeof(DynamicBuffer) == 0x18);

// ---- random numbers (KernelStandard unit U5, Rand.cpp)
//
// Rand: an R250-style generator: a table of `size` words (>= 250) XORed with the word `lag` places back, seeded
// through an auxiliary table of `auxSize` words (>= 100) and an LCRand-like step. Kernel_7c keeps one static
// instance at 0x10019cf0. Scaled results are computed in x87 double precision; float results come back
// unrounded in st(0), as in the original.
class Rand {
public:
    Rand();  // allocates the tables but does not seed them (call SetSeed or Randomise)
    Rand(int size);
    Rand(int size, int auxSize);
    ~Rand();
    Rand& operator=(const Rand& o);  // a shallow copy: both share the tables

    void Randomise();  // seed from timeGetTime (or the frame-lock time) + 10
    void SetSeed(unsigned int seed);
    unsigned int GetSeed() const;
    unsigned int GetRandom();

    unsigned int operator()(unsigned int n);  // [0, n)
    int operator()(int lo, int hi);           // = NextInt(lo, hi)
    float operator()(float m);                // = NextFloat(m)
    float operator()(float lo, float hi);
    float operator()();
    int NextInt();  // 31 bits
    int NextInt(int n);
    int NextInt(int lo, int hi);  // 0 (not lo) when the generator gives 0
    float NextFloat();
    float NextFloat(float m);
    float NextFloat(float lo, float hi);
    double NextDouble();
    double NextDouble(double m);
    double NextDouble(double lo, double hi);
    bool NextBool();
    float Gaussian(float mean, float sd);  // polar method; the spare value is one static for all generators

protected:
    unsigned int RandLC();
    void Init(int size, int auxSize);

private:
    friend struct RandWork;
    int index;              // +0x00
    int lag;                // +0x04
    int size;               // +0x08
    int auxSize;            // +0x0c
    bool haveGauss;         // +0x10
    unsigned int state;     // +0x14: the LC state while seeding
    unsigned int* aux;      // +0x18
    unsigned int* table;    // +0x1c
};
JET_LAYOUT32(sizeof(Rand) == 0x20);

// LCRand: one 32-bit state, x = 16807 x mod (2^32 - 1) by Schrage's method (with the original's -1 for t <= 0)
class LCRand {
public:
    LCRand();  // seeded like Rand::Randomise
    LCRand(unsigned int seed);
    LCRand& operator=(const LCRand& o);
    void Randomise();
    void SetSeed(unsigned int seed);
    unsigned int GetSeed() const;
    unsigned int GetRandom();
    unsigned int operator()();
    int NextInt();
    int NextInt(int n);
    int NextInt(int lo, int hi);
    float NextFloat();
    float NextFloat(float m);
    float NextFloat(float lo, float hi);
    double NextDouble();
    double NextDouble(double m);
    double NextDouble(double lo, double hi);
    bool NextBool();

private:
    unsigned int state;
};

// The clock (unit U13, Time.cpp). The state stays the original's (forwarded data) until the U14 cut-over: its
// CRT initialiser fills freq and its Log reads it.
class Time {
public:
    static unsigned long __cdecl Ms();  // timeGetTime
    static double __cdecl GetSeconds();
    static void __cdecl SetFrameLockTime(double seconds);  // turns the frame lock on; the time moves on IncFrame
    static void __cdecl IncFrame();
    static void __cdecl Delay(unsigned long ms);
    static unsigned long __cdecl Us();  // the low half of the performance counter
    static unsigned __int64 __cdecl Usq();
    static unsigned __int64 __cdecl GetCounter();
    static unsigned __int64 __cdecl GetCounterFrequency();
    static double __cdecl FrameLockSeconds();
    static unsigned long __cdecl FrameLockMs();
    static unsigned __int64 __cdecl TimeStamp();  // _time64
    static void __cdecl Str(char* out);           // _strtime
    static void __cdecl HHMM(char* out);
    Time& operator=(const Time&);

private:
    static unsigned __int64 __cdecl QueryCounterFrequency();

    friend struct RandWork;
    friend struct TimeTest;
    friend class Profile;  // Display reads the counter frequency
    static JET_KS_FORWARDED bool frameLock;               // ?frameLock@Time@Jet@@0_NA
    static JET_KS_FORWARDED unsigned int frameLockCount;  // ?frameLockCount@Time@Jet@@0IA
    static JET_KS_FORWARDED double frameLockTime;         // ?frameLockTime@Time@Jet@@0NA
    static JET_KS_FORWARDED double frameLockDelta;        // ?frameLockDelta@Time@Jet@@0NA
    static JET_KS_FORWARDED unsigned __int64 freq;        // ?freq@Time@Jet@@0_KA
};

class Console;

// a scope timer on a function's static Info: while that Info's group (`mask`) is enabled, the time spent in the
// scope is added to it (unit U12b, Profile.cpp; the statics stay the original's, the Display timers by address)
class Profile {
public:
    struct Info {
        const char* name;  // "Input::Update"
        const char* file;
        unsigned long count, time;  // +0x08 calls, +0x0c counter ticks (low halves)
        int id;                     // -1 in the static initialisers, then the index in funcs
        unsigned long mask;         // 0x80000000 by default
        unsigned long peek, unknown1c;  // +0x18 the highest average shown
    };
    struct TraceInfo {
        const char* name;
    };
    struct FrameTrace {  // 0x404 bytes
        unsigned int count;
        struct {
            int id;
            unsigned long time;
        } points[0x80];
    };
    enum SortOption { SORT_TOTAL = 0, SORT_AVG = 1, SORT_COUNT = 2 };
    Profile(Info* info);
    ~Profile();
    Profile& operator=(const Profile& o);
    static int __cdecl RegisterTracePoint(const char* name);
    static void __cdecl TracePoint(int id);
    static void __cdecl Enable(unsigned int mask);
    static void __cdecl Disable();
    static void __cdecl ChildTime(bool on);
    static bool __cdecl GetChildTime();
    static void __cdecl SetSortFrequency(float f);
    static float __cdecl GetSortFrequency();
    static void __cdecl SetDisplayFrequency(float f);
    static float __cdecl GetDisplayFrequency();  // exported as a double worker (st(0) unrounded)
    static float __cdecl GetDisplayPeriod();
    static void __cdecl SetPeekResetFrequency(float f);
    static int __cdecl GetFuncsCount();
    static Info** __cdecl GetFuncs();
    static const TraceInfo* __cdecl GetTraceInfo();
    static const FrameTrace* __cdecl GetFrameTraceData();
    static unsigned int __cdecl GetNumTracePoints();
    static unsigned int __cdecl GetTraceFrame();
    static void __cdecl Display(Console& console, int max);
    static void __cdecl Display(float elapsed, Console& console, int max);
    static void __cdecl LogDisplay();
    static void __cdecl Reset();
    static void __cdecl ResetPeeks();
    static void __cdecl Sort();
    static void __cdecl SortBy(SortOption option);

private:
    friend struct ProfileWork;
    friend double __cdecl Profile_GetDisplayFrequency();
    void __cdecl Entry(Info* info);
    void __cdecl Exit(Info* info);
    static int __cdecl SortTotal(const void* a, const void* b);
    static int __cdecl SortAvg(const void* a, const void* b);
    static int __cdecl SortCount(const void* a, const void* b);

    Info* info;
    Profile* parent;
    unsigned long childTime, start;

    static JET_KS_FORWARDED Info* funcs[];
    static JET_KS_FORWARDED int nextid;
    static JET_KS_FORWARDED unsigned int enabled;
    static JET_KS_FORWARDED unsigned int depth;
    static JET_KS_FORWARDED Profile* current;
    static JET_KS_FORWARDED bool childtime;
    static JET_KS_FORWARDED float displayPeriod;
    static JET_KS_FORWARDED float sortFreq;
    static JET_KS_FORWARDED float peekResetFreq;
    static JET_KS_FORWARDED int(__cdecl* sort_func)(const void*, const void*);
    static JET_KS_FORWARDED TraceInfo tracePoint[];
    static JET_KS_FORWARDED unsigned int numTracePoints;
    static JET_KS_FORWARDED unsigned int traceFrame;
    static JET_KS_FORWARDED FrameTrace traceData[];
};

class ProtectedLong;

// a scope timer with per-frame statistics (4 frames in a ring), unit U12b (Profile.cpp)
class QuickTimer {
public:
    struct Stats {  // per frame
        int count;
        unsigned long min, max, total;
    };
    struct Info {
        const char* name;
        const char* file;
        int id;  // -1 until registered, -2 past 0x40 timers
        Stats frames[4];
    };
    struct ThreadInfo {
        QuickTimer* current;
        int depth;
    };
    QuickTimer(Info* info);
    ~QuickTimer();
    QuickTimer& operator=(const QuickTimer& o);
    static int __cdecl GetCurrentWriteFrame();
    static void __cdecl Update();

private:
    friend struct ProfileWork;
    Info* info;
    QuickTimer* parent;
    unsigned __int64 childTime;
    unsigned __int64 start;
    int frame;
    int pad;

    static JET_KS_FORWARDED bool childtime;
    static JET_KS_FORWARDED int currentFrame;
    static JET_KS_FORWARDED unsigned long frameTimes[];
    static JET_KS_FORWARDED Info* funcs[];
    static JET_KS_FORWARDED unsigned __int64 lastFrameStart;
    static JET_KS_FORWARDED int logEvery;
    static JET_KS_FORWARDED ProtectedLong nextid;
    static JET_KS_FORWARDED int numFrames;
    static JET_KS_FORWARDED ThreadInfo* threadInfo;
};
JET_LAYOUT32(sizeof(QuickTimer) == 0x20);

// Jet's allocator (unit U14b, Memory.cpp): msvcr80 malloc blocks behind a 4-byte tag, with free lists for the 127
// size classes of 8 bytes up to 0x3f8 (classes pick one of 16 locks by their low 4 bits). One static instance.
class Alloc {
public:
    Alloc();
    ~Alloc();
    Alloc& operator=(const Alloc&);
    static void* __cdecl New(unsigned int size);
    static void __cdecl Delete(void* p);
    static void __cdecl Dump();  // the statistics to OutputDebugString
};

// objects of classes using this live in Jet's heap, so any module can free them
#define JET_HEAP_OBJECT                                                    \
    static void* operator new(size_t n) { return Alloc::New(static_cast<unsigned int>(n)); } \
    static void operator delete(void* p) { Alloc::Delete(p); }

// ---- strings (KernelStandard, unit U14a, AnsiString.cpp). AnsiString is one pointer to a shared, reference-counted
// block {refs, capacity, length, characters}, pointing at the capacity. Empty strings share one static block
// (capacity 0, still the original's); a capacity of 0 reads as "null". String derives from AnsiString.
class AnsiString {
public:
    AnsiString();  // the shared empty buffer
    AnsiString(const AnsiString& o);
    AnsiString(const PString& s);
    AnsiString(const UnicodeString& s);
    AnsiString(unsigned int ch, unsigned int count);
    AnsiString(const char* s);
    AnsiString(const char* s, unsigned int n);
    AnsiString(const __wchar_t* s);
    AnsiString(const __wchar_t* s, unsigned int n);
    ~AnsiString();

    const AnsiString& __cdecl operator=(const AnsiString& o);
    const AnsiString& __cdecl operator=(const PString& s);
    const AnsiString& __cdecl operator=(const UnicodeString& s);
    const AnsiString& __cdecl operator=(const char* s);
    const AnsiString& __cdecl operator=(const __wchar_t* s);
    bool __cdecl operator==(const AnsiString& o) const;
    bool __cdecl operator==(const char* s) const;
    bool __cdecl operator!=(const AnsiString& o) const;
    bool __cdecl operator!=(const char* s) const;
    bool __cdecl operator<(const AnsiString& o) const;
    bool __cdecl operator<(const char* s) const;
    bool __cdecl operator<=(const AnsiString& o) const;
    bool __cdecl operator<=(const char* s) const;
    bool __cdecl operator>(const AnsiString& o) const;
    bool __cdecl operator>(const char* s) const;
    bool __cdecl operator>=(const AnsiString& o) const;
    bool __cdecl operator>=(const char* s) const;
    char& __cdecl operator[](int i);
    char& __cdecl operator[](unsigned int i);
    unsigned int __cdecl operator[](int i) const;
    unsigned int __cdecl operator[](unsigned int i) const;
    unsigned int __cdecl operator[](__int64 i) const;
    unsigned int __cdecl operator[](unsigned __int64 i) const;
    __cdecl operator const char*() const;
    AnsiString& __cdecl operator+=(const AnsiString& o);
    AnsiString& __cdecl operator+=(const PString& s);
    AnsiString& __cdecl operator+=(const char* s);
    AnsiString& __cdecl operator+=(const __wchar_t* s);

    int __cdecl Add(const char* fmt, char* args);  // vsprintf appended
    int __cdecl Addf(const char* fmt, ...);
    unsigned int __cdecl AllocSize() const;
    int __cdecl Compare(const AnsiString& o) const;
    int __cdecl Compare(const char* s) const;
    int __cdecl CompareIgnoreCase(const AnsiString& o) const;
    int __cdecl CompareIgnoreCase(const char* s) const;
    void __cdecl Del(unsigned int pos, unsigned int count);
    bool __cdecl EqualsIgnoreCase(const AnsiString& o) const;
    bool __cdecl EqualsIgnoreCase(const char* s) const;
    int __cdecl Find(unsigned int ch, unsigned int start) const;
    int __cdecl Find(const char* s, unsigned int start) const;
    int __cdecl FindIgnoreCase(unsigned int ch, unsigned int start) const;
    int __cdecl FindIgnoreCase(const char* s, unsigned int start) const;
    static int __cdecl FindIgnoreCase(const char* text, unsigned int ch, unsigned int start);
    static int __cdecl FindIgnoreCase(const char* text, unsigned int length, const char* s, unsigned int start);
    int __cdecl GetANSI(char* out, unsigned int size) const;
    double __cdecl GetDouble() const;
    float __cdecl GetFloat() const;
    int __cdecl GetInt(int base) const;
    unsigned int __cdecl GetUInt(int base) const;
    int __cdecl GetUnicode(__wchar_t* out, unsigned int size) const;  // characters written
    void __cdecl Ins(unsigned int ch, unsigned int pos, unsigned int count);  // count copies of ch at pos
    void __cdecl Ins(const char* s, unsigned int pos);                      // s inserted at pos
    void __cdecl Ins(const __wchar_t* s, unsigned int pos);
    void __cdecl Left(unsigned int n);
    unsigned int __cdecl Length() const;
    AnsiString Mid(unsigned int pos, unsigned int n) const;
    bool __cdecl Read(BinReader& r);
    void __cdecl Right(unsigned int n);
    void __cdecl Set(int value, unsigned int radix);
    void __cdecl Set(const char* fmt, char* args);  // vsprintf
    bool __cdecl SetANSI(const char* s, int length);
    void __cdecl SetLength(unsigned int n);
    bool __cdecl SetUnicode(const __wchar_t* s, int length);
    void __cdecl Setf(const char* fmt, ...);
    void __cdecl Setn(const char* s, unsigned int n);  // the first n characters (clipped to s)
    unsigned int __cdecl Size() const;
    void __cdecl Strip(const char* chars);
    void __cdecl ToLower();
    void __cdecl ToUpper();
    AnsiString __cdecl Token(const char* delimiters);  // cuts and returns the first token
    bool __cdecl Write(BinWriter& w) const;

private:
    friend class Options;
    friend class ConfigData;
    friend class StringTablesBase;
    friend class IOSlaveResource;
    friend class UnicodeString;
    friend class String;
    friend struct LogWork;
    friend struct ConsoleWork;
    friend struct HandlerWork;
    friend struct AnsiWork;
    enum CompareResult { Less, Equal, Greater };
    CompareResult __cdecl CompareWith(const AnsiString& o) const;
    CompareResult __cdecl CompareWith(const char* s) const;
    CompareResult __cdecl CompareWithMatchCase(const AnsiString& o) const;
    CompareResult __cdecl CompareWithMatchCase(const char* s) const;
    bool IsBufferNULL() const;
    bool IsBufferUnique() const;
    void MakeBufferEmpty();
    void MakeBufferNULL();
    void MakeBufferUnique(unsigned int size, bool keep);
    unsigned int __cdecl NewSize(unsigned int length);
    void __cdecl Set(const unsigned char* s, int length, int fallback);

protected:
    void* data = nullptr;
};

class String : public AnsiString {
public:
    String();
    String(const String& o);
    String(const String& o, unsigned int pos, int n);
    String(const AnsiString& o);
    String(const PString& s);
    String(const UnicodeString& s);
    String(unsigned int ch, unsigned int count);
    String(const char* s);
    String(const char* s, unsigned int n);
    ~String();
    __cdecl operator const char*() const;
    const String& __cdecl operator=(const String& o);
    const String& __cdecl operator=(const AnsiString& o);
    const String& __cdecl operator=(const PString& s);
    const String& __cdecl operator=(const UnicodeString& s);
    const String& __cdecl operator=(const char* s);
};

// UTF-16 string {text, capacity (bytes), length (characters)}, an owned buffer from Alloc (unit U14a,
// UnicodeString.cpp). The capacity is (2 length + 0x12) & ~0xf.
class UnicodeString {
public:
    UnicodeString();
    UnicodeString(const UnicodeString& o);
    UnicodeString(const AnsiString& s);
    UnicodeString(const String& s);
    UnicodeString(const PString& s);
    UnicodeString(unsigned int ch, unsigned int count);
    UnicodeString(const char* s);
    UnicodeString(const char* s, unsigned int n);
    UnicodeString(const __wchar_t* s);
    UnicodeString(const __wchar_t* s, unsigned int n);
    ~UnicodeString();

    const UnicodeString& __cdecl operator=(const UnicodeString& o);
    const UnicodeString& __cdecl operator=(const AnsiString& s);
    const UnicodeString& __cdecl operator=(const PString& s);
    const UnicodeString& __cdecl operator=(const String& s);
    const UnicodeString& __cdecl operator=(const __wchar_t* s);
    bool __cdecl operator==(const UnicodeString& o) const;
    bool __cdecl operator==(const __wchar_t* s) const;
    bool __cdecl operator!=(const UnicodeString& o) const;
    bool __cdecl operator!=(const __wchar_t* s) const;
    bool __cdecl operator<(const UnicodeString& o) const;
    bool __cdecl operator<(const __wchar_t* s) const;
    bool __cdecl operator<=(const UnicodeString& o) const;
    bool __cdecl operator<=(const __wchar_t* s) const;
    bool __cdecl operator>(const UnicodeString& o) const;
    bool __cdecl operator>(const __wchar_t* s) const;
    bool __cdecl operator>=(const UnicodeString& o) const;
    bool __cdecl operator>=(const __wchar_t* s) const;
    __wchar_t& __cdecl operator[](int i);
    __wchar_t& __cdecl operator[](unsigned int i);
    unsigned int __cdecl operator[](int i) const;
    unsigned int __cdecl operator[](unsigned int i) const;
    __cdecl operator const __wchar_t*() const;
    UnicodeString& __cdecl operator+=(const UnicodeString& o);
    UnicodeString& __cdecl operator+=(const __wchar_t* s);

    int __cdecl Add(const __wchar_t* fmt, char* args);
    int __cdecl Addf(const __wchar_t* fmt, ...);
    unsigned int __cdecl AllocSize() const;
    int __cdecl Compare(const UnicodeString& o) const;
    int __cdecl Compare(const __wchar_t* s) const;
    int __cdecl CompareIgnoreCase(const UnicodeString& o) const;
    int __cdecl CompareIgnoreCase(const __wchar_t* s) const;
    void __cdecl Del(unsigned int pos, unsigned int count);
    bool __cdecl EqualsIgnoreCase(const UnicodeString& o) const;
    bool __cdecl EqualsIgnoreCase(const __wchar_t* s) const;
    int __cdecl Find(unsigned int ch, unsigned int start) const;
    int __cdecl Find(const __wchar_t* s, unsigned int start) const;
    int __cdecl FindIgnoreCase(unsigned int ch, unsigned int start) const;
    int __cdecl FindIgnoreCase(const __wchar_t* s, unsigned int start) const;
    static int __cdecl FindIgnoreCase(const __wchar_t* text, unsigned int ch, unsigned int start);
    static int __cdecl FindIgnoreCase(const __wchar_t* text, unsigned int length, const __wchar_t* s,
                                      unsigned int start);
    int __cdecl GetANSI(char* out, unsigned int size) const;
    double __cdecl GetDouble() const;
    float __cdecl GetFloat() const;
    int __cdecl GetInt(int base) const;
    unsigned int __cdecl GetUInt(int base) const;
    int __cdecl GetUnicode(__wchar_t* out, unsigned int size) const;
    void __cdecl Ins(unsigned int ch, unsigned int pos, unsigned int count);
    void __cdecl Ins(const __wchar_t* s, unsigned int pos);
    void __cdecl Left(unsigned int n);
    unsigned int __cdecl Length() const;
    bool __cdecl Read(BinReader& r);
    void __cdecl Right(unsigned int n);
    void __cdecl Set(int value, unsigned int radix);
    void __cdecl Set(const __wchar_t* fmt, char* args);
    bool __cdecl SetANSI(const char* s, int length);
    void __cdecl SetLength(unsigned int n);
    bool __cdecl SetUnicode(const __wchar_t* s, int length);
    void __cdecl Setf(const __wchar_t* fmt, ...);
    void __cdecl Setn(const __wchar_t* s, unsigned int n);
    unsigned int __cdecl Size() const;
    void __cdecl Strip(const __wchar_t* chars);
    void __cdecl ToLower();
    void __cdecl ToUpper();
    UnicodeString __cdecl Token(const __wchar_t* delimiters);
    bool __cdecl Write(BinWriter& w) const;

private:
    friend class ConfigData;
    friend class StringTablesBase;
    friend class IOSlaveResource;
    friend class AnsiString;
    friend class MultiByteString;
    enum CompareResult { Less, Equal, Greater };
    CompareResult __cdecl CompareWith(const UnicodeString& o) const;
    CompareResult __cdecl CompareWith(const __wchar_t* s) const;
    CompareResult __cdecl CompareWithMatchCase(const UnicodeString& o) const;
    CompareResult __cdecl CompareWithMatchCase(const __wchar_t* s) const;
    __wchar_t* __cdecl Alloc(unsigned int size);
    unsigned int __cdecl NewSize(unsigned int length);
    void __cdecl Set(const __wchar_t* s, int length, int fallback);  // length < 0: fallback, < 0: up to the NUL

    wchar_t* text = nullptr;
    unsigned int capacity = 0;
    unsigned int length = 0;
};
JET_LAYOUT32(sizeof(UnicodeString) == 0xc);

// MBCS string {data, capacity, bytes, characters} (unit U14a, MultiByteString.cpp); no module imports it. Several
// members count characters as bytes (the copies, Set, SetLength, Ins, Del): kept as in the original.
class MultiByteString {
public:
    MultiByteString();
    MultiByteString(const MultiByteString& o);
    MultiByteString(const MultiByteString& o, unsigned int pos, int count);
    MultiByteString(unsigned int ch, unsigned int count);
    MultiByteString(const char* s);
    MultiByteString(const char* s, unsigned int n);
    ~MultiByteString();

    const MultiByteString& __cdecl operator=(const MultiByteString& o);
    const MultiByteString& __cdecl operator=(const char* s);
    bool __cdecl operator==(const MultiByteString& o) const;
    bool __cdecl operator==(const char* s) const;
    bool __cdecl operator!=(const MultiByteString& o) const;
    bool __cdecl operator!=(const char* s) const;
    bool __cdecl operator<(const MultiByteString& o) const;
    bool __cdecl operator<(const char* s) const;
    bool __cdecl operator<=(const MultiByteString& o) const;
    bool __cdecl operator<=(const char* s) const;
    bool __cdecl operator>(const MultiByteString& o) const;
    bool __cdecl operator>(const char* s) const;
    bool __cdecl operator>=(const MultiByteString& o) const;
    bool __cdecl operator>=(const char* s) const;
    char& __cdecl operator[](int i);
    char& __cdecl operator[](unsigned int i);
    unsigned int __cdecl operator[](int i) const;
    unsigned int __cdecl operator[](unsigned int i) const;
    __cdecl operator const char*() const;
    MultiByteString& __cdecl operator+=(const MultiByteString& o);
    MultiByteString& __cdecl operator+=(const char* s);

    int __cdecl Add(const char* fmt, char* args);
    int __cdecl Addf(const char* fmt, ...);
    unsigned int __cdecl AllocSize() const;
    const char* __cdecl CStr() const;
    int __cdecl Compare(const MultiByteString& o) const;
    int __cdecl Compare(const char* s) const;
    int __cdecl CompareIgnoreCase(const MultiByteString& o) const;
    int __cdecl CompareIgnoreCase(const char* s) const;
    void __cdecl Del(unsigned int pos, unsigned int count);
    bool __cdecl EqualsIgnoreCase(const MultiByteString& o) const;
    bool __cdecl EqualsIgnoreCase(const char* s) const;
    int __cdecl Find(unsigned int ch, unsigned int start) const;
    int __cdecl Find(const char* s, unsigned int start) const;
    int __cdecl FindIgnoreCase(unsigned int ch, unsigned int start) const;
    int __cdecl FindIgnoreCase(const char* s, unsigned int start) const;
    static int __cdecl FindIgnoreCase(const char* text, unsigned int ch, unsigned int start);
    static int __cdecl FindIgnoreCase(const char* text, unsigned int length, const char* s, unsigned int start);
    int __cdecl GetANSI(char* out, unsigned int size) const;
    double __cdecl GetDouble() const;
    float __cdecl GetFloat() const;
    int __cdecl GetInt(int base) const;
    unsigned int __cdecl GetUInt(int base) const;
    int __cdecl GetUnicode(__wchar_t* out, unsigned int size) const;
    void __cdecl Ins(unsigned int ch, unsigned int pos, unsigned int count);
    void __cdecl Ins(const char* s, unsigned int pos);
    void __cdecl Left(unsigned int n);
    unsigned int __cdecl Length() const;
    bool __cdecl Read(BinReader& r);
    void __cdecl Right(unsigned int n);
    void __cdecl Set(int value, unsigned int radix);
    void __cdecl Set(const char* fmt, char* args);
    bool __cdecl SetANSI(const char* s, int length);
    void __cdecl SetLength(unsigned int n);
    bool __cdecl SetUnicode(const __wchar_t* s, int length);
    void __cdecl Setf(const char* fmt, ...);
    void __cdecl Setn(const char* s, unsigned int n);
    unsigned int __cdecl Size() const;
    void __cdecl Strip(const char* chars);
    void __cdecl ToLower();
    void __cdecl ToUpper();
    MultiByteString __cdecl Token(const char* delimiters);
    bool __cdecl Write(BinWriter& w) const;

private:
    enum CompareResult { Less, Equal, Greater };
    CompareResult __cdecl CompareWith(const MultiByteString& o) const;
    CompareResult __cdecl CompareWith(const char* s) const;
    CompareResult __cdecl CompareWithMatchCase(const MultiByteString& o) const;
    CompareResult __cdecl CompareWithMatchCase(const char* s) const;
    unsigned char* __cdecl Alloc(unsigned int size);
    unsigned int __cdecl NewSize(unsigned int bytes);
    static unsigned int __cdecl GetLengthAndSize(const unsigned char* s, unsigned int& size);
    void __cdecl Set(const unsigned char* s, int length, int fallback);

    unsigned char* data = nullptr;
    unsigned int capacity = 0;
    unsigned int bytes = 0;
    unsigned int chars = 0;
};
JET_LAYOUT32(sizeof(MultiByteString) == 0x10);

// ---- options: a list of {key, value, ...} entries read from option files or raw lines
class Vector2;
class Vector3;
class Vector4;
class Quaternion;
class Color;
template <class T, unsigned int SIZ, class L>
class DynamicArray;
template <class A, class B>
struct Less;

// a pair of values written "a:b"
template <class T>
struct Range {
    T min;
    T max;
};

// The options of the command line, an options file, the environment: one entry per key (lower case), the value a
// string; typed reads parse it, GetNext walks it in separator-delimited runs.
class Options {
public:
    Options();
    // The original never exports a destructor: every user inlines it (Kernel_7c 0x1000c280).
    ~Options()
    {
        while (Node* n = head) {
            if (!n->prev)
                head = n->next;
            else
                n->prev->next = n->next;
            if (!n->next)
                tail = n->prev;
            else
                n->next->prev = n->prev;
            n->next = n->prev = nullptr;
            --count;
            n->separators.~String();
            n->value.~String();
            n->key.~String();
            Alloc::Delete(n);
        }
    }
    void __cdecl Init(const char* appName, int argc, char* argv[]);
    void __cdecl Init(const char* appName, const char* commandLine);
    void __cdecl ProcessCmdLine(int argc, char* argv[]);
    void __cdecl ProcessFile(const char* path);
    void __cdecl ProcessEnv(const char* variable);
    void __cdecl ProcessRaw(const char* line);
    void __cdecl Dump();
    void __cdecl SetSeparators(const char* key, const char* separators) const;
    bool __cdecl GetNext(const char* key, String& value) const;
    bool __cdecl GetNext(const char* key, PString& value) const;
    bool __cdecl GetNext(const char* key, bool& value) const;
    bool __cdecl GetNext(const char* key, unsigned int& value) const;
    bool __cdecl GetNext(const char* key, unsigned long& value) const;
    bool __cdecl GetNext(const char* key, int& value) const;
    bool __cdecl GetNext(const char* key, long& value) const;
    bool __cdecl GetNext(const char* key, float& value) const;
    bool __cdecl GetNext(const char* key, Vector2& value) const;
    bool __cdecl GetNext(const char* key, Vector3& value) const;
    bool __cdecl GetNext(const char* key, Vector4& value) const;
    bool __cdecl GetNext(const char* key, Quaternion& value) const;
    bool __cdecl GetNext(const char* key, Color& value) const;
    bool __cdecl Get(const char* key) const;
    bool __cdecl Get(const char* key, PString& value) const;
    bool __cdecl Get(const char* key, String& value) const;
    bool __cdecl Get(const char* key, PString& value, const PString& fallback) const;
    bool __cdecl Get(const char* key, String& value, const String& fallback) const;
    bool __cdecl Get(const char* key, bool& value, bool fallback) const;
    bool __cdecl Get(const char* key, int& value, int fallback) const;
    bool __cdecl Get(const char* key, unsigned int& value, unsigned int fallback) const;
    bool __cdecl Get(const char* key, long& value, long fallback) const;
    bool __cdecl Get(const char* key, unsigned long& value, unsigned long fallback) const;
    bool __cdecl Get(const char* key, float& value, float fallback) const;
    bool __cdecl Get(const char* key, Vector2& value, const Vector2& fallback) const;
    bool __cdecl Get(const char* key, Vector3& value, const Vector3& fallback) const;
    bool __cdecl Get(const char* key, Vector4& value, const Vector4& fallback) const;
    bool __cdecl Get(const char* key, Quaternion& value, const Quaternion& fallback) const;
    bool __cdecl Get(const char* key, Color& value, const Color& fallback) const;
    bool __cdecl Get(const char* key, DynamicArray<String, 41, Less<String, String>>& values, const char* separators) const;
    bool __cdecl Get(const char* key, Range<float>& value) const;
    bool __cdecl Get(const char* key, Range<int>& value) const;

private:
    // 0x18 bytes from Jet::Alloc
    struct Node {
        Node* next;
        Node* prev;
        String key;          // +0x08 lower case
        String value;        // +0x0c
        String separators;   // +0x10 for GetNext
        const char* cursor;  // +0x14 into value, where GetNext goes on
    };
    friend struct LogWork;  // Log::Init resets the "Log" entry's separators and cursor
    Node* Find(const char* key) const;
    Node* head;
    Node* tail;
    int count;
};

class DynamicLibrary;
class Window;

// The platform layer: options, window, application name. 0x18 bytes, built by App::Init in Jet's heap. Layout
// (unit U11, Window.cpp): {instance, Window*, commandLine, Options*, parent HWND, AnsiString appName}; the x64
// stand-in keeps its own state in the block.
class OS {
public:
    OS(const char* appName, const char* commandLine, void* instance, HWND__* window);
    OS(const OS& o);
    OS& operator=(const OS& o);
    ~OS();
    bool Init();
    Options* __cdecl GetOptions() const;
    const char* __cdecl GetAppName() const;
    DynamicLibrary* __cdecl CreateDynamicLibrary() const;
    Window* __cdecl GetWindow() const;
    void __cdecl SetWindow(Window* window);
    Window* __cdecl CreateNewWindow() const;
    const char* __cdecl GetName() const;
    const char* __cdecl GetCmdLine() const;
    void* __cdecl GetInstance() const;
    HWND__* __cdecl GetParent() const;
    void __cdecl SetParent(HWND__* parent);
    static const char* __cdecl GetDefaultOptions();

private:
    static int __cdecl InitCallBack(void* self);
#if defined(JET_KS_BUILD)
    static const char* const defaultOptions;
#endif
    unsigned char opaque[JET_OPAQUE_SIZE(0x18, 0x30)];
};

class Thread;  // (declared with the synchronisation objects)

class Exception;

// Runs a callback under Jet's crash handler (unit U12c, ExceptionHandler.cpp): an assertion or a Jet::Exception is
// reported and ends the callback with 0; a hardware fault goes to the crash report (ERRORLOG.TXT, CRASH.DMP,
// XCrashReport.exe); critical-shutdown callbacks run when it catches one. The report writers and the x86 stack walk
// are still the original's, called by address; the statics are the original's.
class ExceptionHandler {
public:
    using CallBack = int(__cdecl*)(void*);
    class Shutdown;  // {arg, fn, next}, 12 bytes from Jet::Alloc
    static int __cdecl Main(CallBack fn, void* arg, Thread* thread);
    static void* __cdecl AddCriticalShutdown(CallBack fn, void* arg);
    static void __cdecl RemoveCriticalShutdown(void* handle);
    static void* __cdecl AddCriticalStartup(CallBack fn, void* arg);
    static void __cdecl RemoveCriticalStartup(void* handle);
    static void __cdecl CriticalShutdown();
    static void __cdecl CriticalStartup();
    static bool __cdecl Assert(const char* fmt, ...);
    static void __cdecl AssertInit(const char* file, int line, const char* reason);
    static void __cdecl SetAssertions(bool prompt);
    static void __cdecl Init(const Options* options);
    static void __cdecl InitExceptionDialog(void* dialog, Exception& e);
    static void __cdecl LogLine(void* dialog, void* dc, const char* fmt, ...);
    static void __cdecl Break();  // does nothing
    ExceptionHandler& operator=(const ExceptionHandler&);

private:
    friend struct HandlerWork;
    static int __cdecl MainTryCatch(CallBack fn, void* arg);
    static int __cdecl ProcessException(Exception& e);
    static int __cdecl DoFilterException(void* pointers);
    static int __cdecl FilterException(void* pointers);

    static JET_KS_FORWARDED char corePrefix[];
    static JET_KS_FORWARDED Shutdown* criticalShutdown;
    static JET_KS_FORWARDED Shutdown* criticalStartup;
    static JET_KS_FORWARDED bool doCoreDump;
    static JET_KS_FORWARDED int dumpOptions;
    alignas(8) static JET_KS_FORWARDED char hardwareExceptionBlock[];  // room for a HardwareException
    static JET_KS_FORWARDED bool ignore;
    static JET_KS_FORWARDED bool isPromptAssertions;
    static JET_KS_FORWARDED int line;
    // file and reason (`char const * const`, which VS2005 mangled 0PBDB) are found by name at run time
};

// An intrusive list of T linked through T's ListLink at +4 (Events::Event, Job): {head, tail, count}. The links
// point at the neighbours' links, not at the objects.
template <class T>
struct ListOps;  // (core/KernelStandard_7c/Platform.cpp)
struct ListLink {
    ListLink* next;
    ListLink* prev;
};
template <class T>
struct LinkedList {
    T* head;
    T* tail;
    unsigned long count;
};

// Jet's timed event queue (0x2c bytes); the kernel keeps one as the TimeEventSystem. Events due by a time are
// updated in time order; a finished one is deleted when the queue owns its events, a repeating one is queued again.
class Events {
public:
    class Event {
    public:
        Event(unsigned long interval, unsigned long nextTime);
        Event(const Event& o);
        Event& operator=(const Event& o);
        int operator<(const Event& o) const;
        virtual bool Update();  // true: finished (the base moves its time on and finishes)
        virtual unsigned long GetId() const;
        unsigned long GetNextTime() const;
        void SetId(unsigned long id);
        JET_HEAP_OBJECT

    protected:
        void UpdateNext();

    private:
        friend class Events;
        template <class>
        friend struct ListOps;
        ListLink link;            // +0x04
        unsigned long interval;   // +0x0c
        unsigned long id;         // +0x10
        unsigned long nextTime;   // +0x14
    };

    Events();
    explicit Events(bool ownsEvents);
    ~Events();
    Events& operator=(const Events& o);
    void GenerateId(Event* e);
    void Add(Event* e);
    void Update(unsigned long time);
    bool RemoveEventSafely(Event* e);
    bool Remove(unsigned long id);
    void Remove(Event* e);

private:
    bool ownsEvents;               // +0x00
    unsigned long lastId;          // +0x04
    LinkedList<Event> removed;     // +0x08 removed by id, deleted on the next Update
    LinkedList<Event> queue;       // +0x14 by next time
    LinkedList<Event> requeue;     // +0x20 updated, not finished: back into the queue after the pass
};
JET_LAYOUT32(sizeof(Events) == 0x2c && sizeof(Events::Event) == 0x18);

template <class T>
class IList;
class CriticalSection;
class File;

// A named log channel (0x24 bytes from Jet::Alloc, unit U12a, Log.cpp): {next, prev (a ring through
// Log::clientList), AnsiString name +8, AnsiString +0xc (unused), level +0x10 (0 None, 1 Err, 2 Warn, 3 Diag,
// 4 Prog), message kind +0x14 (an index into " !?+-", set by the callers), busy +0x18, outputs +0x1c (1 file,
// 2 debugger, 8 window), format +0x20 (1 line break, 2 time prefix)}.
class LogClient {
public:
    void __cdecl OutputString(const char* fmt, ...);
    void __cdecl OutputStringV(const char* fmt, char* args);

private:
    friend class Log;
    friend struct LogWork;
    friend struct HandlerWork;
    friend struct HeapWork;  // PackedHeap::DebugPrint sets the kind
    LogClient(const char* name);
#if defined(JET_KS_BUILD)
    LogClient* next;
    LogClient* prev;
    AnsiString name;
    AnsiString unused;
    int level;
    int kind;
    bool busy, pad19[3];
    unsigned char outputs, pad1d[3];
    unsigned char format, pad21[3];
#endif
    static JET_KS_FORWARDED bool disabled;  // ?disabled@LogClient@Jet@@0_NA
};
#if defined(JET_KS_BUILD)
JET_LAYOUT32(sizeof(LogClient) == 0x24);
#endif

// JetLog.txt and the log channels (unit U12a, Log.cpp; the statics are ours since U14b).
class Log {
public:
    static void __cdecl Init(Options* options);
    static void __cdecl Done();
    static void __cdecl Shutdown(bool quiet);  // quiet: no "Shutdown." line
    static LogClient* __cdecl GetClient(const char* name);
    static void __cdecl SetLogToFile(::_iobuf* file);
    static ::_iobuf* __cdecl GetLogToFile();
    Log& operator=(const Log&);

private:
    friend class LogClient;
    friend struct LogWork;
    friend struct HandlerWork;
    static LogClient* __cdecl CreateClient(const char* name);
    static LogClient* __cdecl SearchClientList(const char* name);
    static void __cdecl Process(String& line);

    static JET_KS_FORWARDED IList<LogClient>* clientList;
    static JET_KS_FORWARDED File* logFile;
    static JET_KS_FORWARDED ::_iobuf* stderrFile;
    static JET_KS_FORWARDED double startTime;
    static JET_KS_FORWARDED CriticalSection* writeLock;
    static JET_KS_FORWARDED bool initialized;
    static JET_KS_FORWARDED bool displayModuleName;
};

// Debugger helpers (unit U12a, Exception.cpp)
class Debugger {
public:
    enum Level {};
    static void __cdecl Output(Level level, const char* text);  // OutputDebugStringA
    static bool __cdecl IsReadValid(void* p, unsigned int size);
    static bool __cdecl IsDebuggerAttached();
    static void __cdecl Break();         // does nothing
    static void __cdecl LogCallStack();  // does nothing
    Debugger& operator=(const Debugger&);
};

// ---- files
enum FileSeek { FILE_SEEK_SET = 0, FILE_SEEK_CUR = 1, FILE_SEEK_END = 2 };

class FileSubstitute;
class FileSub;

class File {
public:
    // a file time: seconds since 1970 (st_mtime), compared signed
    union Time {
        Time& operator=(const Time& o);
        bool __cdecl operator==(const Time& o) const;
        bool __cdecl operator<(const Time& o) const;
        bool __cdecl operator>(const Time& o) const;
        bool __cdecl operator<=(const Time& o) const;
        bool __cdecl operator>=(const Time& o) const;
        void __cdecl SetSeconds(long s);
        long seconds;
    };
    // one directory entry (0x120 bytes): a find result or an archive directory record
    struct Entry {
        Entry();
        Entry(const Entry& o);
        Entry& __cdecl operator=(const Entry& o);
        bool __cdecl IsDir();         // attrib bit 0x10
        bool __cdecl IsCompressed();  // attrib bit 0x80
        bool __cdecl IsHidden();      // attrib bit 0x02
        void __cdecl LittleEndian();  // does nothing
        char name[0x104];        // +0x000
        unsigned long size;      // +0x104 (an archive's stored size)
        unsigned long length;    // +0x108 (an archive's data length)
        unsigned long crc;       // +0x10c
        unsigned long u110;      // +0x110
        unsigned long u114;      // +0x114
        unsigned long time;      // +0x118
        unsigned short attrib;   // +0x11c _finddata attributes (0x80: compressed)
        unsigned short u11e;     // +0x11e
    };
    // a fixed MAX_PATH buffer with path helpers. Text past 0x102 characters leaves it empty and sets a
    // write-only error byte (0x100adb92 in the original).
    struct Path {
        Path();  // empty
        Path(const char* path);
        Path(const Path& o);
        void __cdecl operator=(const char* path);
        void __cdecl operator=(const Path& o);
        bool operator==(const Path& o) const;  // case-sensitive
        bool operator==(const char* o) const;
        bool operator!=(const Path& o) const;
        bool operator!=(const char* o) const;
        bool operator<(const Path& o) const;
        __cdecl operator const char*() const;
        void __cdecl Normalize();
        void __cdecl DelPath();
        void __cdecl DelFileName();
        void __cdecl Del(unsigned int pos, unsigned int count);
        void __cdecl DelDrive();
        void __cdecl DelSlash();
        void __cdecl AddSlash();
        void __cdecl ToLower();
        void __cdecl ToUpper();
        unsigned int __cdecl Length() const;
        unsigned int __cdecl Last() const;  // the last character
        unsigned int __cdecl GetFilePathHash() const;
        bool __cdecl HasValidData() const;  // NUL inside the buffer
        void __cdecl DelName(bool delSlash);  // true: the separator goes too
        void __cdecl AddPath(const char* path);
        void __cdecl AddPath(const char* path, int count);
        void __cdecl Add(const char* text);
        void __cdecl Add(const char* text, int count);
        bool __cdecl IsMatch(const char* other, const char** rest) const;
        static bool __cdecl IsMatch(const char* path, const char* pattern, const char** rest);
        static bool __cdecl IsEqual(const char* a, const char* b);
        bool __cdecl IsExt(const char* ext) const;
        void __cdecl SetExt(const char* ext);
        const char* __cdecl GetFileName() const;
        static const char* __cdecl GetFileName(const char* path);
        const char* __cdecl GetPath() const;  // past the drive colon
        static const char* __cdecl GetPath(const char* path);
        const char* __cdecl GetExt() const;
        static const char* __cdecl GetExt(const char* path);
        void __cdecl GetCwd();
        void __cdecl Make(const char* dir, const char* name);
        void __cdecl PrevDir();
        void __cdecl RootDir();
        void __cdecl ReplaceBackSlashes();
        void __cdecl ReplaceForwardSlashes();
        void __cdecl SplitPath(char* drive, char* dir, char* name, char* ext) const;
        bool __cdecl MakeRelativeTo(const char* base);
        Path __cdecl ExtractFirstPath();  // cuts and returns the first component
        FileSubstitute* __cdecl GetSubst() const;  // the substitute named before a ':' (not a drive letter)
        static FileSubstitute* __cdecl GetSubst(const char* path);
        char text[0x104];

    private:
        operator bool() const;  // exported, private: true
        bool operator!() const;  // exported, private: false
    };
    // A directory search (0x440 bytes): the entry found, then plain directories through _findfirst and a
    // substitute's directories and archives one after the other, each name given once (case-insensitive).
    // Unit U10b (FileFind.cpp).
    class Find : public Entry {
    public:
        Find();
        Find(const Find& o);
        ~Find();
        Find& operator=(const Find& o);
        bool __cdecl Open(const char* pattern);
        bool __cdecl Open(const char* pattern, const PString& base);
        bool __cdecl Next();
        void __cdecl Close();
        void __cdecl Dump();
        Path __cdecl GetNameAndPath() const;

    private:
        friend class FindImpl;
        bool __cdecl CheckFiles();
        bool __cdecl FirstDirect(const char* pattern);
        bool __cdecl NextDirect();
        bool __cdecl NextFile();
        bool __cdecl NextSearch();
        void __cdecl ReduceWildcard(const char* pattern);
        static const long ClosedHandle;  // -1 (exported, unused)

        long handle;             // +0x120 _findfirst handle, or an archive's read cursor; -1 none
        Path dir;                 // +0x124 the pattern's directory (a substitute's name taken off)
        Path current;             // +0x228 the directory of the entry found
        Path wildcard;            // +0x32c the pattern's name part
        struct SubChunk* subs;    // +0x430 the FileSubs still to search, taken from the end
        struct NameNode* names;   // +0x434 the names given so far (lower case, sorted): a ring through this field
        unsigned long nameCount;  // +0x438
        FileSub* archive;         // +0x43c the archive being searched, or null
    };
    // File::Exists returns a ResultClass by value; outside KernelStandard it is bound at run time (see
    // core/Kernel_7c/KernelStandardRuntime.cpp)
#if !defined(JET_KS_IMPLEMENTATION)
    static bool Exists(const char* path);
#endif
    static bool __cdecl Md(const char* path);     // makes the directory (and its parents)
    static void __cdecl Subst(const PString& drive, const char* path, bool add);  // null path: removes it
    static bool __cdecl Prune(const char* path);  // deletes the directory tree; the path must not be null

    // A buffered file over the CRT's low-level I/O (msvcr80's file descriptors: the original's FileSub code
    // opens files into these objects). Mode bits: 0x0f write (2: BPE-compressed "CFIL" output), 0xf0 read
    // (0x10 plain or compressed, 0x20 plain, 0x30 compressed), 0xf00 create (0x100 truncate, 0x200 append,
    // 0x300 new only), 0x1000 rejected. Unit U10a (File.cpp).
    enum Results {};
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;
    enum Mode { READ = 0x10, CREATE_WRITE = 0x101 };  // the modes InputSystem uses
    File();
    ~File();
    Result __cdecl Open(const char* path, unsigned long mode, int encoding);  // "x:rest": x is a subst name
    Result __cdecl Open(const PString& base, const char* path, unsigned long mode, int encoding);
    unsigned long __cdecl Read(void* data, unsigned long size, unsigned long count);  // items read
    bool __cdecl Read(signed char& v);
    bool __cdecl Read(unsigned char& v);
    bool __cdecl Read(short& v);
    bool __cdecl Read(unsigned short& v);
    bool __cdecl Read(long& v);
    bool __cdecl Read(unsigned long& v);
    bool __cdecl Read(float& v);
    bool __cdecl Read(__int64& v);
    bool __cdecl Read(unsigned __int64& v);
    bool __cdecl Read32(int& v);
    bool __cdecl Read32(unsigned int& v);
    bool __cdecl Read(PString& s);
    bool __cdecl Read(AnsiString& s);
    bool __cdecl Read(String& s);
    bool __cdecl Read(UnicodeString& s);
    unsigned long __cdecl Write(const void* data, unsigned long size, unsigned long count);
    bool __cdecl Write(signed char v);
    bool __cdecl Write(unsigned char v);
    bool __cdecl Write(short v);
    bool __cdecl Write(unsigned short v);
    bool __cdecl Write(long v);
    bool __cdecl Write(unsigned long value);
    bool __cdecl Write(float v);
    bool __cdecl Write(__int64 v);
    bool __cdecl Write(unsigned __int64 v);
    bool __cdecl Write32(int v);
    bool __cdecl Write32(unsigned int v);
    bool __cdecl Write(const PString& s);
    bool __cdecl Write(const AnsiString& s);
    bool __cdecl Write(const String& s);
    bool __cdecl Write(const UnicodeString& s);
    void __cdecl SafeRead(void* data, unsigned long size, unsigned long count);         // throws FileEx short
    void __cdecl SafeWrite(const void* data, unsigned long size, unsigned long count);  // throws FileEx short
    unsigned long __cdecl WriteLine(const char* text, int encoding);
    unsigned long __cdecl ReadLine(void* buffer, unsigned long size, int encoding);  // 0 at the end
    unsigned long __cdecl ReadBlock(const char* id, void* data, unsigned long size);
    unsigned long __cdecl WriteBlock(const char* id, const void* data, unsigned long size);
    unsigned long __cdecl Seek(long offset, FileSeek whence);
    unsigned long __cdecl SeekSet(long offset);
    unsigned long __cdecl SeekCur(long offset);
    unsigned long __cdecl SeekEnd(long offset);
    unsigned long __cdecl Tell() const;
    bool __cdecl Eof();
    unsigned long __cdecl Close();
    void __cdecl Commit();
    unsigned long __cdecl Size();
    void __cdecl SetBuffer(void* buffer);  // a 16 KB buffer the file does not own
    bool __cdecl IsOpen();
    bool __cdecl IsCompressed();
    const char* __cdecl Name() const;
    bool __cdecl GetStamp(Time& t);
    bool __cdecl SetStamp(Time& t);
    void __cdecl DetectEncoding();
    int __cdecl GetEncoding() const;  // 1 ANSI, 2 UTF-16
    void __cdecl SetEncoding(int encoding);
    static int __cdecl GetDefaultEncoding();  // 1
    static void __cdecl SetCallBack(bool(__cdecl* progress)(unsigned long done, unsigned long written));
    static void __cdecl SetLog(bool on);  // does nothing
    static bool __cdecl Cd(const char* path);
    static bool __cdecl GetCwd(Path& path);
    static bool __cdecl Rd(const char* path);
    static void __cdecl SetStamp(Time& t, const char* path);  // throws FileEx
    static bool __cdecl Remove(const char* path);
    static bool __cdecl Remove(const PString& base, const char* path);
#if defined(JET_KS_IMPLEMENTATION)
    // the substitute statics (unit U10b, FileSub.cpp): "name:rest" paths go to the substitute's FileSubs
    static Result __cdecl Exists(const char* path);  // 0 there, 2 not, 7 no such substitute
    static Result __cdecl Exists(PString& base, const char* path);  // base: the substitute it was found in
    static Result __cdecl ExistsIgnoreCase(const char* path, Path& found);
    static Result __cdecl ExistsIgnoreCase(PString& base, const char* path, Path& found);
    static Result __cdecl CheckWriteMode(const char* path);  // 0 writable, 2 missing, 9 read-only
    static Result __cdecl SubstExists(const PString& name);
    static void __cdecl SubstLink(const PString& name, const PString& other);
    static unsigned long __cdecl GetSubstBuildVersion(const PString& name);
    static unsigned long __cdecl GetSubstProductCode(const PString& name);
    static bool __cdecl IsDirectory(const char* path);
    static bool __cdecl IsDirectory(const PString& base, const char* path);
    static bool __cdecl Unlink(const char* path);
    static bool __cdecl Unlink(const PString& base, const char* path);
    static bool __cdecl Rename(const char* from, const char* to);
    static bool __cdecl Rename(const PString& base, const char* from, const char* to);
    static bool __cdecl GetStamp(Time& t, const char* path);
    static void __cdecl SetReadonly(const char* path, bool on);
    static void __cdecl SetReadonly(const PString& base, const char* path, bool on);
    static bool __cdecl TrueName(char* path);  // a substitute's path becomes the real one
#endif
    // the current position, read inline by the original's callers (+0x1c; x64 offset provisional)
    long Position() const { return static_cast<long>(position); }
    int Handle() const { return fd; }  // -1 when closed
    // the text encoding field (+0x34: 1 ANSI, 2 UTF-16), read inline by ConfigData's file writer
    int EncodingField() const { return encoding; }

    static const PString Sep;

private:
    friend class JIRFFile;
    friend class FileImpl;
    friend class FileSubPath;
    friend class FileSubArchive;
    static bool(__cdecl* callback)(unsigned long, unsigned long);  // progress of an archive write (FileSubArchive)

    int fd;                    // +0x00 msvcr80 descriptor, -1 when closed
    char* cur;                 // +0x04 the next byte in the buffer
    char* buffer;              // +0x08 16 KB
    unsigned long count;       // +0x0c valid (read) or pending (write) bytes in the buffer
    unsigned long mode;        // +0x10
    unsigned long size;        // +0x14 the data size, -1 until known (0 after a failed read)
    unsigned long dataStart;   // +0x18 the "CFIL" header's length
    unsigned long position;    // +0x1c in the data
    unsigned long rawPos;      // +0x20 the next raw file offset
    unsigned long dirty;       // +0x24 written since the last flush
    unsigned char flags;       // +0x28 bit 0 owns the buffer, bit 1 the descriptor belongs to a FileSub
    FileSubstitute* subst;     // +0x2c the substitute it was opened through
    FileSub* sub;              // +0x30 the FileSub object holding it
    int encoding;              // +0x34
    Path path;                 // +0x38
};
JET_LAYOUT32(sizeof(File) == 0x13c);  // (the offsets are checked in File.cpp)

// A file's contents in memory: {current, start, size, owned, AnsiString name}. Unit U10a (MemoryFile.cpp).
class MemoryFile {
public:
    MemoryFile();
    MemoryFile(const char* name, unsigned int size);  // an owned empty block of size bytes
    MemoryFile(const void* data, unsigned int size, bool unicode, int encoding);
    MemoryFile(File& file, unsigned int size, bool unicode, int encoding);
    MemoryFile(File& file, bool unicode, int encoding);  // the rest of the file
    MemoryFile(File& file, const String& key);
    MemoryFile(const MemoryFile& o);  // shallow
    MemoryFile& operator=(const MemoryFile& o);
    ~MemoryFile();
    File::Result Open(const char* path, unsigned long mode, bool unicode, int encoding);
    File::Result Open(const PString& base, const char* path, unsigned long mode, bool unicode, int encoding);
    bool Open(File& file, unsigned int size, bool unicode, int encoding);
    bool Open(File& file, bool unicode, int encoding);
    bool Open(File& file, const String& key);
    bool Open(const void* data, unsigned int size, bool unicode, int encoding);
    void Open(unsigned int size);
    void Close();  // frees owned data, empties
    unsigned int Read(void* dst, unsigned int size, unsigned int count) const;  // short reads read nothing
    void Read(signed char& v) const;
    void Read(unsigned char& v) const;
    void Read(short& v) const;
    void Read(unsigned short& v) const;
    void Read(long& v) const;
    void Read(unsigned long& v) const;
    void Read(float& v) const;
    void Read(PString& s, bool unused) const;  // u32 length (bit 30: UTF-16), then the text
    signed char Readsbyte() const;
    unsigned char Readubyte() const;
    short Readsword() const;
    unsigned short Readuword() const;
    long Readsdword() const;
    unsigned long Readudword() const;
    float Readfloat() const;
    unsigned char Peek() const;
    void Write(const void* src, unsigned int size);
    unsigned int Seek(int offset, FileSeek whence) const;
    unsigned int SeekSet(int offset) const;
    unsigned int SeekCur(int offset) const;
    unsigned int SeekEnd(int offset) const;
    unsigned int Tell() const;
    unsigned int Size() const;
    bool Eof() const;
    const void* Data() const;
    const void* CurrentPosition() const;
    const char* Name() const;
    void SetName(const char* name);
    void SetName(const String& name);

    mutable const char* cur;  // +0x00
    const char* start;        // +0x04
    unsigned int size;        // +0x08
    bool owned;               // +0x0c
    AnsiString name;          // +0x10

protected:
    bool __cdecl ReadData(File& file, unsigned int size, bool unicode, int encoding);
    bool __cdecl ReadData(File& file, const String& key);
};
JET_LAYOUT32(sizeof(MemoryFile) == 0x14);

// ---- synchronisation objects (KernelStandard unit U4, SyncObjects.cpp). SyncObject {vptr, HANDLE}; vftable slots:
// deleting dtor, operator void*, Lock(timeout), Unlock(count), Unlock() (MSVC puts overloads in reverse order).
// Other modules import the constructors, so their objects carry KernelStandard's vftables. All are allocated
// through Jet::Alloc (their deleting destructors free with Alloc::Delete).
#define JET_HEAP_ARRAYS                                                                     \
    static void* operator new[](size_t n) { return Alloc::New(static_cast<unsigned int>(n)); } \
    static void operator delete[](void* p) { Alloc::Delete(p); }

class SyncObject {
public:
    JET_KS_IMPORT virtual ~SyncObject();
    JET_KS_IMPORT virtual __cdecl operator void*();
    JET_KS_IMPORT virtual bool __cdecl Lock(unsigned long timeout) const;  // true only for WAIT_OBJECT_0
    JET_KS_IMPORT virtual bool __cdecl Unlock() const;                     // base: false
    JET_KS_IMPORT virtual bool __cdecl Unlock(unsigned long count) const;  // base: false
    // tries Lock(0): true when that fails (a mutex owned by this thread counts as not locked)
    JET_KS_IMPORT bool __cdecl IsLocked();
    JET_KS_IMPORT bool __cdecl Signal();                       // Unlock()
    JET_KS_IMPORT bool __cdecl Wait(unsigned long timeout) const;  // Lock(timeout)
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
#if defined(JET_KS_IMPLEMENTATION)  // only KernelStandard itself (or the x64 stand-in) builds these objects
    JET_KS_IMPORT SyncObject();
#else
    SyncObject() = delete;
#endif
    void* handle;

private:
    JET_KS_IMPORT SyncObject(const SyncObject&);         // sets the vftable only; the handle is not copied
    JET_KS_IMPORT void operator=(const SyncObject&);     // does nothing
};

class Mutex : public SyncObject {
public:
    JET_KS_IMPORT Mutex(bool initiallyOwned = false, const char* name = nullptr);
    JET_KS_IMPORT virtual ~Mutex();
    JET_KS_IMPORT bool __cdecl Unlock() const override;                     // ReleaseMutex
    JET_KS_IMPORT bool __cdecl Unlock(unsigned long count) const override;  // the same
};

class Semaphore : public SyncObject {
public:
    JET_KS_IMPORT Semaphore(int initialCount = 0, int maxCount = 1, const char* name = nullptr);
    JET_KS_IMPORT virtual ~Semaphore();
    JET_KS_IMPORT bool __cdecl Unlock() const override;                     // release 1
    JET_KS_IMPORT bool __cdecl Unlock(unsigned long count) const override;  // release count
};

class WEvent : public SyncObject {
public:
    JET_KS_IMPORT WEvent(bool manualReset = false, bool initialState = false, const char* name = nullptr);
    JET_KS_IMPORT virtual ~WEvent();
    JET_KS_IMPORT bool __cdecl Unlock() const override;  // true, does nothing
    JET_KS_IMPORT void __cdecl Set();
    JET_KS_IMPORT void __cdecl Pulse();
    JET_KS_IMPORT void __cdecl Reset();
};

// {vptr, unused handle, CRITICAL_SECTION* from Jet::Alloc}; the name is ignored
class CriticalSection : public SyncObject {
public:
    JET_KS_IMPORT CriticalSection(const char* name = nullptr);
    JET_KS_IMPORT virtual ~CriticalSection();
    JET_KS_IMPORT bool __cdecl Lock(unsigned long timeout) const override;  // INFINITE: Enter, else TryEnter
    JET_KS_IMPORT bool __cdecl Unlock() const override;

private:
    void* section;  // +8
};

JET_LAYOUT32(sizeof(Mutex) == 8);
JET_LAYOUT32(sizeof(CriticalSection) == 0xc);

// ---- heaps (unit U14b, Memory.cpp)

// 1 MB of VirtualAlloc: {allocations in use, bytes handed out, free chain}, then the allocations from +0xc, each
// {size, data...}. A freed one is {size, prev's next field, next (block chain), prev's next field, next (size chain)}.
class PackedHeapBlock;

// A heap of small allocations (4-byte aligned, 0x10..0xffff0 bytes) in PackedHeapBlocks, with the freed ones
// reused by exact size. The blocks are ordered by address; every heap is in one global list (DebugPrintAll).
// sizeof is the original's (0x20): other modules allocate these. The two MSVC8 trees of the original are our own
// containers here, behind the same 12 bytes each.
class PackedHeap {
public:
    PackedHeap(const char* name);
    PackedHeap(const PackedHeap& o);  // the same blocks and chains as o (shared), not in the list of heaps
    ~PackedHeap();                    // frees the blocks with nothing left in them
    PackedHeap& operator=(const PackedHeap& o);
    unsigned char* Alloc(unsigned int size);
    void Free(unsigned char* p);
    bool IsEmpty() const;
    void DebugPrint();
    static void __cdecl DebugPrintAll();

private:
    friend struct HeapWork;
    unsigned char* AllocFromUnused(unsigned int size);
    bool Free(PackedHeapBlock* block, unsigned char* p);
    PackedHeapBlock* FindBlockForAllocation(unsigned char* p);
    PackedHeapBlock* AllocBlock();
    void FreeBlock(PackedHeapBlock* block);

    const char* name;     // +0x00
    void* blocks[3];      // +0x04  {PackedHeapBlock* array, count, capacity}, by address
    void* bySize[3];      // +0x10  {map of size -> free chain head, -, -}
    unsigned int unused;  // +0x1c  blocks with nothing in use
};

// PackedHeap under a CriticalSection
class PackedHeapMP : public PackedHeap {
public:
    PackedHeapMP(const char* name);
    ~PackedHeapMP();
    unsigned char* Alloc(unsigned int size);
    void Free(unsigned char* p);
    bool IsEmpty() const;

private:
    CriticalSection lock;  // +0x20
};

JET_LAYOUT32(sizeof(PackedHeap) == 0x20);
JET_LAYOUT32(sizeof(PackedHeapMP) == 0x2c);

// Fixed-size items in blocks of `count` (one Jet::Alloc each). Blocks with free items are on the second list; a
// block whose items are all free again is released, unless it is the one being carved. ConfigData nodes use one.
class PoolBlockAlloc {
public:
    PoolBlockAlloc(unsigned int itemSize, unsigned int count, unsigned int align);
    ~PoolBlockAlloc();
    void* __cdecl Alloc();
    void __cdecl Free(void* p);
    void __cdecl ReleaseMemory();

protected:
    void __cdecl Lock();
    void __cdecl Unlock();

private:
    friend struct PoolWork;
    void* full;           // +0x00  blocks without free items
    void* partial;        // +0x04  blocks with free items
    int align;            // +0x08  a power of 2 (8 otherwise)
    int itemSize;         // +0x0c  with its 4-byte block pointer, rounded to align
    int count;            // +0x10
    int blockBytes;       // +0x14
    int freeItems;        // +0x18
    void* carving;        // +0x1c  the newest block, while it has uncarved items
    int uncarved;         // +0x20
    CriticalSection lock; // +0x24
};

JET_LAYOUT32(sizeof(PoolBlockAlloc) == 0x30);

// locks a SyncObject (Lock(INFINITE)) for its lifetime: {SyncObject*, bool locked}
class SingleLock {
public:
    JET_KS_IMPORT SingleLock(const SyncObject* object, bool lockNow);
    JET_KS_IMPORT ~SingleLock();
    JET_KS_IMPORT SingleLock& operator=(const SingleLock& o);
    JET_KS_IMPORT bool __cdecl Lock(unsigned long timeout) const;
    JET_KS_IMPORT bool __cdecl Unlock() const;
    JET_KS_IMPORT bool __cdecl IsLocked() const;

private:
    const SyncObject* object;
    mutable bool locked;
};

// WaitForMultipleObjects over SyncObjects: {HANDLE* handles, bool* locked, count, SyncObject** objects}
class MultiLock {
public:
    // what Lock returns: 1 timeout, 0x10000000 + index acquired, 0x20000000 + index abandoned, 0 otherwise
    enum Result { FAILED = 0, TIMEOUT = 1, ACQUIRED = 0x10000000, ABANDONED = 0x20000000 };
    JET_KS_IMPORT MultiLock(SyncObject** objects, unsigned long count, bool lockNow);
    JET_KS_IMPORT ~MultiLock();
    JET_KS_IMPORT MultiLock& operator=(const MultiLock& o);
    JET_KS_IMPORT Result __cdecl Lock(unsigned long timeout, bool waitAll) const;
    JET_KS_IMPORT bool __cdecl Unlock() const;
    JET_KS_IMPORT bool __cdecl Unlock(unsigned long index) const;  // false, does nothing
    JET_KS_IMPORT bool __cdecl IsLocked(unsigned long index) const;
    JET_HEAP_OBJECT

private:
    void** handles;
    bool* locked;
    unsigned long count;
    SyncObject** objects;
};

// single writer, many readers: {WEvent noReaders, Mutex writer, SyncObject* both[2], MultiLock*, int readers}
class SWMRGuard {
public:
    JET_KS_IMPORT SWMRGuard();
    JET_KS_IMPORT ~SWMRGuard();
    JET_KS_IMPORT bool __cdecl WaitToWrite(unsigned long timeout);
    JET_KS_IMPORT void DoneWriting();
    JET_KS_IMPORT bool __cdecl WaitToRead(unsigned long timeout);
    JET_KS_IMPORT void DoneReading();

private:
    WEvent noReaders;         // +0x00
    Mutex writer;             // +0x08
    SyncObject* both[2];      // +0x10: writer, noReaders
    MultiLock* lock;          // +0x18
    int readers;              // +0x1c
};
JET_LAYOUT32(sizeof(SWMRGuard) == 0x20);

// a Win32 thread (a SyncObject: waiting on it waits for the thread to end); unit U13, Thread.cpp.
// {vptr, thread handle, WEvent done @8, alive @0x10, fn @0x14, arg @0x18, thread id @0x1c}
class Thread : public SyncObject {
public:
    enum Priority { PRIORITY_NORMAL = 0, PRIORITY_1 = 1, PRIORITY_2 = 2 };  // only the values used are named
    using Function = unsigned int(__stdcall*)(void*);
    JET_KS_IMPORT Thread(Function fn, void* arg, bool start);  // start false: created suspended
    JET_KS_IMPORT virtual ~Thread();
    JET_KS_IMPORT virtual __cdecl operator void*();  // the done event's handle
    JET_KS_IMPORT bool __cdecl Resume();
    JET_KS_IMPORT bool __cdecl Suspend();
    JET_KS_IMPORT void __cdecl SetPriority(Priority p);
    JET_KS_IMPORT Priority __cdecl GetPriority() const;
    JET_KS_IMPORT bool __cdecl StartedOk() const;
    JET_KS_IMPORT bool __cdecl IsAlive() const;
    JET_KS_IMPORT bool __cdecl IsDone();  // the done event was set (an auto-reset event: it is consumed)
    JET_KS_IMPORT void __cdecl SetDone();
    JET_KS_IMPORT unsigned long __cdecl Terminate();  // sets done, waits for the end, returns the exit code
    JET_KS_IMPORT unsigned long __cdecl GetThreadID();
    JET_KS_IMPORT static unsigned long __cdecl GetCurrentId();
    JET_KS_IMPORT static unsigned long __cdecl GetProcessId();
    JET_KS_IMPORT static unsigned long __cdecl GetMainThreadId();
    static void __cdecl GetProcessName(char* out);
    JET_KS_IMPORT static bool __cdecl IsProcessAlive(unsigned long processId);
    JET_KS_IMPORT static void __cdecl TerminateProcess(unsigned long processId);
    JET_KS_IMPORT static void __cdecl GlobalInit();

private:
    static unsigned long __stdcall Start(void* thread);
    static int __cdecl SafeStart(void* thread);

    friend class ResourceSystem;  // compares the caller with the main thread
    friend struct FoundationWork;  // KernelStandardInit sets it
    static JET_KS_FORWARDED unsigned int mainThreadId;
#if defined(JET_KS_BUILD)
    WEvent done;        // +0x08
    bool alive;         // +0x10
    Function fn;        // +0x14
    void* arg;          // +0x18
    unsigned long id;   // +0x1c
#else
    unsigned char opaque[JET_OPAQUE_SIZE(0x18, 0x28)];
#endif
};
JET_LAYOUT32(sizeof(Thread) == 0x20);

// Named TLS slots (unit U13, Thread.cpp): {Mutex*, count, capacity, entries {PString name, TlsAlloc index}}.
// KernelStandardInit creates the singleton and its shutdown routine destroys it (Memory.cpp).
class ThreadLocalStorage {
public:
    JET_KS_IMPORT ThreadLocalStorage(const ThreadLocalStorage& o);  // shares the mutex, copies the entries
    JET_KS_IMPORT ThreadLocalStorage& operator=(const ThreadLocalStorage& o);
    JET_KS_IMPORT int __cdecl GetIndex(const PString& name);  // a new TlsAlloc slot for a name not seen yet
    JET_KS_IMPORT static bool __cdecl CreateTLS();
    JET_KS_IMPORT static void __cdecl DestroyTLS();
    JET_KS_IMPORT static void* __cdecl GetThreadData(int index);
    JET_KS_IMPORT static void __cdecl SetThreadData(int index, void* data);

    static JET_KS_FORWARDED int ThreadTlsIndex;
    static JET_KS_FORWARDED int ProfilePtTlsIndex;
    static JET_KS_FORWARDED int ProfileTraceTlsIndex;
    static JET_KS_FORWARDED int ProfileRenderTlsIndex;

private:
    JET_KS_IMPORT ThreadLocalStorage();
    JET_KS_IMPORT ~ThreadLocalStorage();
    friend JET_KS_IMPORT ThreadLocalStorage& GetTLS();
    friend struct TlsWork;
    friend struct FoundationWork;

    struct Entry {
        PStringCache::Node* name;
        int index;
    };
    Mutex* lock;
    unsigned int count, capacity;
    Entry* entries;

    static JET_KS_FORWARDED ThreadLocalStorage* singleton;
};
JET_LAYOUT32(sizeof(ThreadLocalStorage) == 0x10);
JET_KS_IMPORT ThreadLocalStorage& GetTLS();

// A dynamically loaded module: {vptr, handle}. Users construct and destroy it inline (Kernel_7c 0x1000afd0:
// handle = 0; at the end "if (handle) Close()"), so only the virtual members are imported; we call them by
// qualified name and keep our own vftable copy, like the original.
// Slot order: deleting destructor, Open, Close, Import.
class DynamicLibrary {
public:
#if defined(JET_KS_BUILD)
    DynamicLibrary();
    DynamicLibrary(const DynamicLibrary& o);
    DynamicLibrary& operator=(const DynamicLibrary& o);
    virtual ~DynamicLibrary();
#else
    DynamicLibrary() : handle(nullptr) {}
    virtual ~DynamicLibrary()
    {
        if (handle) DynamicLibrary::Close();
    }
#endif
    JET_KS_IMPORT virtual bool __cdecl Open(const char* path);
    JET_KS_IMPORT virtual void __cdecl Close();
    JET_KS_IMPORT virtual bool __cdecl Import(void** proc, const char* name);
    static bool __cdecl GetModuleNameAndAddress(void* address, String& name, void*& base);  // always false
    JET_HEAP_OBJECT

private:
    void* handle;
};
JET_LAYOUT32(sizeof(DynamicLibrary) == 8);

// A named page-file mapping shared between processes (0x10 bytes): {data, name, size, mapping}.
class SharedMemory {
public:
    SharedMemory(const char* name, unsigned int size);
    ~SharedMemory();
    SharedMemory& operator=(const SharedMemory& o);
    bool ProcessAttach();  // creates or opens the mapping and maps it
    bool ProcessDetach();
    void* Data();
    bool TryExclusiveLock();  // always false
    void Unlock();            // does nothing

private:
    void* data;
    const char* name;
    unsigned int size;
    void* mapping;
};
JET_LAYOUT32(sizeof(SharedMemory) == 0x10);

struct Size;

// Jet's text console (0x1c bytes, unit U12b, Console.cpp): a width x height character ring scrolled by `top`, a
// cursor and the end of its line, and the colour spans of the last Print ("<color:RRGGBB>" ... "</color>").
class Console {
public:
    struct Span {  // 12 bytes
        unsigned char type;  // 1 colour on, 0 off
        int pos;             // the character index in the printed text
        unsigned long rgb;   // 0x00BBGGRR (the hex value with red and blue swapped)
    };
    struct Chunk {  // 0xa8 bytes from Jet::Alloc, a ring of chunks of 13 spans
        Chunk* next;
        Chunk* prev;
        unsigned int count;
        Span spans[13];
    };
    Console();
    Console(const Console& o);  // shares the buffer, copies the spans
    ~Console();
    Console& operator=(const Console& o);
    void __cdecl Init(const Size& size);
    void __cdecl Done();
    void __cdecl Clear();
    void __cdecl Printf(const char* format, ...);
    void __cdecl Print(const char* text);
    void __cdecl Print(char ch, int count);
    void __cdecl Print(int value, int width);  // right-aligned after the cursor; the cursor stays
    void __cdecl Bar(int halfSteps, int width);
    void __cdecl Goto(int x, int y);
    void __cdecl BlitToMono(unsigned char* screen) const;
    void __cdecl BlitToMono44(int x, int y, unsigned char* screen) const;
    static int __cdecl StringToHex(const char* text, int length);

    int width;               // +0x00
    int height;              // +0x04
    unsigned char* buffer;   // +0x08
    int top;                 // +0x0c the row shown first
    Chunk* spans;            // +0x10
    unsigned char* cursor;   // +0x14
    unsigned char* lineEnd;  // +0x18

private:
    friend struct ConsoleWork;
    friend struct ProfileWork;  // Profile::Display's line breaks
    void __cdecl NewLine();
    static JET_KS_FORWARDED unsigned char f50_font[];  // 8 bytes a character
};
JET_LAYOUT32(sizeof(Console) == 0x1c);

class Jobs;

// A unit of work for the Jobs thread (0x28 bytes). Process runs on that thread, Dispatch on the thread calling
// Jobs::Update once it is done. A job made with a parent holds the parent until its own Dispatch.
class Job {
public:
    explicit Job(Job* parent = nullptr);
    Job(const Job& o);
    Job& operator=(const Job& o);
    virtual ~Job();
    virtual const char* GetName() const;  // "Unknown"
    float GetProgress() const;
    JET_HEAP_OBJECT

protected:
    virtual void __cdecl Process();
    virtual void __cdecl Dispatch();

private:
    friend class Jobs;
    template <class>
    friend struct ListOps;
    void SetProgress(float p);
    void Lock();
    void Unlock();
    bool IsDone() const;
    void __cdecl StartProcessing();
    void __cdecl EndProcessing();

    ListLink link;              // +0x04
    float progress;             // +0x0c
    Job* parent;                // +0x10
    unsigned long locks;        // +0x14 children not dispatched yet
    unsigned long created;      // +0x18 timeGetTime
    unsigned long started;      // +0x1c
    unsigned long ended;        // +0x20
    unsigned long unknown24;    // +0x24 (set with the others, never read)
};
JET_LAYOUT32(sizeof(Job) == 0x28);

// The job thread (0xa8 bytes). Add queues a job; the thread processes jobs in order; a job with children still
// pending waits until they are dispatched; Update (main thread) dispatches and deletes the finished ones.
class Jobs {
public:
    Jobs();
    void __cdecl Init(Options* options);  // option "JobsConsole": text consoles drawn into 0xb0000
    void __cdecl Done();
    void __cdecl Update();
    void __cdecl Add(Job* job);
    bool IsActive() const;

private:
    static unsigned int __stdcall Main(void* jobs);
    void Run();
    void Finish(Job* job);
    void ShowFinished();
    void ShowQueue();

    unsigned long unknown00;          // +0x00
    unsigned long unknown04;          // +0x04
    LinkedList<Job> queue;            // +0x08
    Mutex queueLock;                  // +0x14
    Semaphore queued;                 // +0x1c
    LinkedList<Job> finished;         // +0x24
    Mutex finishedLock;               // +0x30
    Semaphore finishedCount;          // +0x38
    LinkedList<Job> pending;          // +0x40 processed, waiting for children (thread only)
    Thread* thread;                   // +0x4c
    Console console;                  // +0x50 status
    Console queueConsole;             // +0x6c
    Console finishedConsole;          // +0x88
    bool consoleOn;                   // +0xa4
    bool active;                      // +0xa5
};
JET_LAYOUT32(sizeof(Jobs) == 0xa8);

// ---- I/O slaves (byte streams; KernelStandard unit U9b, IOSlave.cpp). vftable: deleting dtor, Seek, Tell, GetSize,
// ReadData, WriteData. Deleting destructors free with Jet::Alloc.
class IOSlaveReadCursor;
class IOSlave {
public:
    JET_KS_IMPORT IOSlave();
    JET_KS_IMPORT IOSlave(const IOSlave& o);
    JET_KS_IMPORT IOSlave& operator=(const IOSlave& o);
    JET_KS_IMPORT virtual ~IOSlave();
    virtual bool Seek(unsigned long pos) = 0;
    virtual unsigned long Tell() = 0;
    JET_KS_IMPORT virtual unsigned long GetSize();                                   // 0
    JET_KS_IMPORT virtual unsigned long ReadData(void* dst, unsigned long size);         // 0
    JET_KS_IMPORT virtual unsigned long WriteData(const void* src, unsigned long size);  // 0 (= ReadData's code)
    JET_KS_IMPORT bool WriteByte(unsigned char v);
    JET_KS_IMPORT bool WriteUINT32(unsigned long v);
    JET_KS_IMPORT bool ReadUINT32(unsigned long& v);
    JET_KS_IMPORT unsigned long Write(IOSlaveReadCursor& src, unsigned long size);  // in 4 KB pieces
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS
};

// a fixed memory block {data, position, size} (no data: size 0)
class IOSlaveMemory : public IOSlave {
public:
    JET_KS_IMPORT IOSlaveMemory(void* data, unsigned long size);
    JET_KS_IMPORT IOSlaveMemory(const IOSlaveMemory& o);
    JET_KS_IMPORT IOSlaveMemory& operator=(const IOSlaveMemory& o);
    JET_KS_IMPORT virtual ~IOSlaveMemory();
    JET_KS_IMPORT bool Seek(unsigned long pos) override;
    JET_KS_IMPORT unsigned long Tell() override;
    JET_KS_IMPORT unsigned long GetSize() override;
    JET_KS_IMPORT unsigned long ReadData(void* dst, unsigned long size) override;  // a null dst skips
    JET_KS_IMPORT unsigned long WriteData(const void* src, unsigned long size) override;
    IOSlave* AsSlave() { return this; }

private:
    unsigned char* data;     // +0x04
    unsigned long position;  // +0x08
    unsigned long size;      // +0x0c
};
JET_LAYOUT32(sizeof(IOSlaveMemory) == 0x10);

// a growing memory stream over an MSVC8 std::vector<char> {allocator, first, last, end} and a position
class IOSlaveMemoryDynamic : public IOSlave {
public:
    IOSlaveMemoryDynamic();
    IOSlaveMemoryDynamic(const IOSlaveMemoryDynamic& o);
    IOSlaveMemoryDynamic& operator=(const IOSlaveMemoryDynamic& o);
    virtual ~IOSlaveMemoryDynamic();
    bool Seek(unsigned long pos) override;
    unsigned long Tell() override;
    unsigned long GetSize() override;
    unsigned long ReadData(void* dst, unsigned long size) override;
    unsigned long WriteData(const void* src, unsigned long size) override;
    const char* GetData() const;  // null when empty

    struct Vector {
        unsigned char allocator[4];  // (empty std::allocator<char>)
        char* first;
        char* last;
        char* end;
    };

private:
    Vector bytes;            // +0x04
    unsigned long position;  // +0x14
};
JET_LAYOUT32(sizeof(IOSlaveMemoryDynamic) == 0x18);

class File;
// an open File (a closed one counts as none) and its size when made (grown by writes)
class IOSlaveFile : public IOSlave {
public:
    explicit IOSlaveFile(File* file);
    IOSlaveFile(const IOSlaveFile& o);
    IOSlaveFile& operator=(const IOSlaveFile& o);
    virtual ~IOSlaveFile();
    bool Seek(unsigned long pos) override;
    unsigned long Tell() override;
    unsigned long GetSize() override;
    unsigned long ReadData(void* dst, unsigned long size) override;  // a null dst skips
    unsigned long WriteData(const void* src, unsigned long size) override;

private:
    File* file;          // +0x04
    unsigned long size;  // +0x08
};
JET_LAYOUT32(sizeof(IOSlaveFile) == 0xc);

// Reads a slave through a buffer {slave, stream position of the buffer, buffer, offset in it, bytes in it,
// capacity}; Access(offset, n) makes n bytes at the cursor + offset contiguous (null at the end, which also lets
// the slave go).
class IOSlaveReadCursor {
public:
    IOSlaveReadCursor();
    explicit IOSlaveReadCursor(IOSlave* slave);
    IOSlaveReadCursor(const IOSlaveReadCursor& o);  // the same slave at the same place, no buffer
    ~IOSlaveReadCursor();
    void operator=(IOSlave* slave);
    void operator=(const IOSlaveReadCursor& o);
    void operator++();
    void operator++(int);
    void operator+=(unsigned long n);
    void Inc(unsigned long n);
    unsigned char operator*();
    unsigned char operator[](unsigned long offset);
    unsigned char AccessByte(unsigned long offset);
    unsigned long AccessUInt32(unsigned long offset);
    const void* Access(unsigned long offset, unsigned long n);
    int GetIndex(const void* p);  // the offset of a pointer into the buffer from the cursor

    IOSlave* slave;           // +0x00
    unsigned long position;   // +0x04
    unsigned char* buffer;    // +0x08
    unsigned long offset;     // +0x0c
    unsigned long filled;     // +0x10
    unsigned long capacity;   // +0x14
};
JET_LAYOUT32(sizeof(IOSlaveReadCursor) == 0x18);

// Writes through to a slave, keeping the last 2 x chunk bytes written in two buffers for WriteCopy (an LZ copy
// from distance -offset).
class IOSlaveWriteCursor : public IOSlave {
public:
    IOSlaveWriteCursor(IOSlave* slave, unsigned long chunk);
    virtual ~IOSlaveWriteCursor();
    unsigned long WriteData(const void* src, unsigned long size) override;
    bool ReadData(void* dst, int offset, unsigned long size);  // from the history, else from the slave
    unsigned long WriteCopy(int offset, unsigned long size);

protected:
    IOSlaveWriteCursor(const IOSlaveWriteCursor& o);  // (the vftable only)
    void operator=(const IOSlaveWriteCursor& o);    // (nothing)
    bool Seek(unsigned long pos) override;            // false
    unsigned long Tell() override;                    // 0
    unsigned long ReadData(void* dst, unsigned long size) override;  // 0

private:
    IOSlave* slave;           // +0x04
    unsigned char* buffers[2];  // +0x08
    unsigned long chunk;      // +0x10
    unsigned long fill[2];    // +0x14
    unsigned long current;    // +0x1c
};
JET_LAYOUT32(sizeof(IOSlaveWriteCursor) == 0x20);

// A scrambled stream inside another slave: a 12-byte header {version (1), key word, size} then the bytes, each
// scrambled by its position, the key word and the key text (CheckRead / CheckWrite). Written ones get their header
// and 4 KB padding when destroyed. An empty key passes everything through.
class IOSlaveResource : public IOSlave {
public:
    IOSlaveResource(IOSlave* slave, const String& key, bool write);
    IOSlaveResource(const IOSlaveResource& o);
    IOSlaveResource& operator=(const IOSlaveResource& o);
    virtual ~IOSlaveResource();
    bool Seek(unsigned long pos) override;
    unsigned long Tell() override;
    unsigned long GetSize() override;
    unsigned long ReadData(void* dst, unsigned long size) override;
    unsigned long WriteData(const void* src, unsigned long size) override;

protected:
    unsigned char CheckWrite(unsigned long pos, unsigned char v);
    unsigned char CheckRead(unsigned long pos, unsigned char v);

private:
    IOSlave* slave;           // +0x04 (null when the header failed)
    unsigned long base;       // +0x08 the slave position after the header
    AnsiString key;           // +0x0c
    bool write;               // +0x10
    unsigned long version;    // +0x14
    unsigned long keyWord;    // +0x18
    unsigned long position;   // +0x1c
    unsigned long size;       // +0x20
};
JET_LAYOUT32(sizeof(IOSlaveResource) == 0x24);

// A reader of "name { ... }" block text files, line by line through File::ReadLine {file, line buffer, token buffer,
// cursor, buffer size, PString name, parent, child, end, block pending}; children share the parent's buffers.
class BlockReader {
public:
    BlockReader();
    explicit BlockReader(File& file);
    BlockReader(const BlockReader& o);
    BlockReader& operator=(const BlockReader& o);
    ~BlockReader();
    void __cdecl Init(File& file);
    BlockReader* __cdecl Begin();
    void __cdecl End();
    bool __cdecl Next(PString& name);
    bool __cdecl ReadString(char* out, int size);
    bool __cdecl ReadString(PString& out);
    int __cdecl ReadStringv(PString* out, int count);
    bool __cdecl ReadInt(int& v);
    int __cdecl ReadIntv(int* out, int count);
    bool __cdecl ReadFloat(float& v);
    int __cdecl ReadFloatv(float* out, int count);
    bool __cdecl ReadBool(bool& v);

protected:
    explicit BlockReader(BlockReader* parent);
    bool __cdecl SkipBlock();
    bool __cdecl ParseEntry();
    bool __cdecl ParseString();
    bool __cdecl IsBlockStart();
    static bool __cdecl SkipToToken(const char*& p);
    static bool __cdecl SkipWhiteSpace(const char*& p);
    static bool __cdecl SkipNonWhiteSpace(const char*& p);
    static bool __cdecl IsEol(const char* p);
    static bool __cdecl IsComment(const char* p);

private:
    File* file;            // +0x00
    char* line;            // +0x04
    char* token;           // +0x08
    const char* cursor;    // +0x0c
    unsigned long bufferSize;  // +0x10
    PString name;          // +0x14
    BlockReader* parent;   // +0x18
    BlockReader* child;    // +0x1c
    bool ended;            // +0x20
    bool pending;          // +0x21
};
JET_LAYOUT32(sizeof(BlockReader) == 0x24);

// Byte-pair encoding (Philip Gage's, blocks of up to 2 KB / 210 distinct bytes); Comp falls back to a copy when
// the output reaches the input's size.
class BPE {
public:
    static unsigned long __cdecl Comp(void* dst, const void* src, unsigned long size);
    static unsigned long __cdecl Decomp(void* dst, const void* src, unsigned long size);
};

// JIRF chunk files (KernelStandard unit U9c, JIRF.cpp): a File read and written through an IOSlaveFile and an
// IOSlaveResource (scrambled when a key is given). Header {"JIRF", size, id}, blocks {id, size, data}; sizes are
// patched in when the writer objects go.
class JIRFFile : public File {
public:
    enum Results {};
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;
    JIRFFile();
    JIRFFile(const JIRFFile& o);  // (shares the slaves)
    JIRFFile& operator=(const JIRFFile& o);
    ~JIRFFile();
    Result __cdecl OpenJIRF(const char* path, const String& key, unsigned long mode);
    Result __cdecl OpenJIRF(const PString& base, const char* path, const String& key, unsigned long mode);
    void __cdecl Close();
    unsigned long __cdecl Write(const void* src, unsigned long size);
    unsigned long __cdecl Read(void* dst, unsigned long size);
    bool __cdecl Read32(unsigned int& v);  // (any byte read counts)
    unsigned long __cdecl SeekCur(long offset);  // -1 on failure
    unsigned long __cdecl Seek(long position);
    unsigned long __cdecl Tell() const;
    unsigned int __cdecl WriteHeader(unsigned long id);  // the size field's position
    unsigned int __cdecl WriteBlock(unsigned long id, unsigned int size, void* data);
    Result __cdecl ReadBlock(unsigned long id, unsigned int& size, void* data);
    Result __cdecl ReadBlockHeader(unsigned long& id, unsigned int& size);
    Result __cdecl ReadHeader(unsigned long id);
    Result __cdecl ReadHeader(unsigned long id, unsigned int& size);
    void __cdecl WriteAt(unsigned int position, unsigned long value);
    void __cdecl WriteAt(unsigned int position);  // the size from there to here
    void __cdecl WriteSizeAt(unsigned int position);
    void __cdecl WritePadded(const PString& s);
    void __cdecl WritePadded(const String& s);
    Result __cdecl ReadPadded(PString& s);
    Result __cdecl ReadPadded(String& s);

private:
    IOSlaveFile* slaveFile;     // +0x13c
    IOSlaveResource* resource;  // +0x140
};
JET_LAYOUT32(sizeof(JIRFFile) == 0x144);

// writes the chunk size at `position` when it goes
class JIRFWriteSize {
protected:
    explicit JIRFWriteSize(JIRFFile& file);
    ~JIRFWriteSize();
    JIRFFile* file;
    unsigned int position;

private:
    const JIRFWriteSize& __cdecl operator=(const JIRFWriteSize& o);  // (nothing)
};
class JIRFWriteHeader : public JIRFWriteSize {
public:
    JIRFWriteHeader(JIRFFile& file, unsigned long id);
    ~JIRFWriteHeader();
};
class JIRFWriteBlock : public JIRFWriteSize {
public:
    JIRFWriteBlock(JIRFFile& file, unsigned long id);
    ~JIRFWriteBlock();
};

// ---- reference counting (KernelStandard). Only the out-of-line members are imported; like the original
// Kernel_7c, our module keeps its own copy of the vftable.
class ReferenceCount {
public:
    ReferenceCount() : referenceCount(1) {}
    JET_KS_IMPORT void AddReference() const;
    JET_KS_IMPORT bool ReleaseReference() const;
    int GetReferenceCount() const { return referenceCount; }

protected:
    JET_KS_IMPORT virtual void __cdecl DeleteThis() const;
    JET_KS_IMPORT virtual ~ReferenceCount();

private:
    mutable int referenceCount;
};

// (FastDynamicArray.h includes this header)
template <class A, class B>
struct Less;
template <class T, unsigned int SIZ, class L>
class FastDynamicArray;

// A per-thread scratch buffer (0x14 bytes) that grows to the largest size asked; GetAccess hands out the calling
// thread's (with a reference), making it on first use.
class SharedScratchPad : public ReferenceCount {
public:
    static SharedScratchPad* __cdecl GetAccess();
    unsigned char* __cdecl GetBuffer(unsigned int size);
    JET_HEAP_OBJECT

protected:
    explicit SharedScratchPad(unsigned long threadId);
    ~SharedScratchPad() override;

private:
    unsigned long threadId;  // +0x08
    unsigned char* buffer;   // +0x0c
    unsigned int capacity;   // +0x10

protected:
    static CriticalSection lock;
    static FastDynamicArray<SharedScratchPad*, 13, Less<SharedScratchPad*, SharedScratchPad*>> scratchPadArray;
};

}  // namespace Jet
