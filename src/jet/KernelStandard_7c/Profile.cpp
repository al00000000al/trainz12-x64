// Unit U12b of KernelStandard_7c: Profile (0x10014d90..0x10014f65, 0x10038aa0..0x100390ab) and QuickTimer
// (0x1003c0b0..0x1003c36d, 0x1003c830). The statics are ours since U14b (with the three private accumulators of
// Profile::Display(float, ...), 0x100ae210..218). qsort is msvcr80's, so equal keys end up in the original's order.
#include <jet/KernelStandard.h>
#include <jet/SmallTypes.h>

#include "Crt80.h"

#include <windows.h>

#include <cstring>

namespace Jet {

Profile::Info* Profile::funcs[0x1000];         // 0x100a8b58
int Profile::nextid;                           // 0x100adc04
unsigned int Profile::enabled;                 // 0x100adbfc
unsigned int Profile::depth;                   // 0x100adc00
Profile* Profile::current;                     // 0x100adc08
bool Profile::childtime = true;                // 0x10072879
float Profile::displayPeriod = 1.0f / 15.0f;   // 0x10072888 (0x3d888889)
float Profile::sortFreq = 0.5f;                // 0x1007288c
float Profile::peekResetFreq = 2.0f;           // 0x10072890
int(__cdecl* Profile::sort_func)(const void*, const void*) = &Profile::SortTotal;  // 0x10072884
Profile::TraceInfo Profile::tracePoint[0x80];  // 0x100a8958
unsigned int Profile::numTracePoints;          // 0x100adc0c
unsigned int Profile::traceFrame;              // 0x100adc10
Profile::FrameTrace Profile::traceData[0x80];  // 0x10088758

namespace {
QuickTimer::ThreadInfo quickThreadInfo;  // 0x100ae1e4
float displayClock[3];  // 0x100ae210..218: Display(float)'s time since the last display, sort and peak reset
}  // namespace

bool QuickTimer::childtime = true;                          // 0x10072878
int QuickTimer::currentFrame;                               // 0x100adc1c
unsigned long QuickTimer::frameTimes[4];                    // 0x100adc30
QuickTimer::Info* QuickTimer::funcs[0x40];                  // 0x100adc40
unsigned __int64 QuickTimer::lastFrameStart;                // 0x100adc28
int QuickTimer::logEvery = 200;                             // 0x1007287c
ProtectedLong QuickTimer::nextid;                           // 0x100ae1ec
int QuickTimer::numFrames;                                  // 0x100adc20
QuickTimer::ThreadInfo* QuickTimer::threadInfo = &quickThreadInfo;  // 0x10072880

struct ProfileWork {
    static_assert(sizeof(Profile::FrameTrace) == 0x404 && sizeof(Profile::Info) == 0x20);
    static_assert(offsetof(QuickTimer::Info, frames) == 0xc && sizeof(QuickTimer::Stats) == 0x10);

    static unsigned long Counter()
    {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return c.LowPart;
    }
    static void Line(Console& c)  // '\n' written at the cursor, then the usual wrap
    {
        *c.cursor = '\n';
        ++c.cursor;
        if (c.cursor >= c.lineEnd) {
            --c.cursor;
            c.NewLine();
        }
    }
};

// ---- Profile

Profile::Profile(Info* info_) : info(info_), parent(nullptr)
{
    if (info_->mask & enabled) {
        ++depth;
        parent = current;
        current = this;
        Entry(info_);
    }
}

Profile::~Profile()
{
    Info* const i = info;
    if (i->mask & enabled) {
        const unsigned long now = ProfileWork::Counter();
        const unsigned long spent = now - start;
        i->time += spent;
        i->time -= childTime;
        if (parent && childtime) parent->childTime += spent;
        current = parent;
        --depth;
    }
}

Profile& Profile::operator=(const Profile& o)
{
    info = o.info;
    parent = o.parent;
    childTime = o.childTime;
    start = o.start;
    return *this;
}

void __cdecl Profile::Entry(Info* i)
{
    ++i->count;
    if (i->id == -1 && nextid < 0x1000) {
        i->id = nextid;
        funcs[nextid] = i;
        ++nextid;
    }
    childTime = 0;
    start = ProfileWork::Counter();
}

void __cdecl Profile::Exit(Info* i)
{
    const unsigned long spent = ProfileWork::Counter() - start;
    i->time += spent;
    i->time -= childTime;
    if (parent && childtime) parent->childTime += spent;
}

void __cdecl Profile::Enable(unsigned int mask) { enabled = mask; }
void __cdecl Profile::Disable() { enabled = 0; }
void __cdecl Profile::ChildTime(bool on) { childtime = on; }
bool __cdecl Profile::GetChildTime() { return childtime; }
void __cdecl Profile::SetSortFrequency(float f) { sortFreq = f; }
float __cdecl Profile::GetSortFrequency() { return sortFreq; }
void __cdecl Profile::SetDisplayFrequency(float f) { displayPeriod = 1.0f / f; }
// exported as ?GetDisplayFrequency@Profile@Jet@@SAMXZ: the original returns fld1 / fdiv unrounded in st(0)
double __cdecl Profile_GetDisplayFrequency() { return 1.0 / static_cast<double>(Profile::displayPeriod); }
float __cdecl Profile::GetDisplayPeriod() { return displayPeriod; }
void __cdecl Profile::SetPeekResetFrequency(float f) { peekResetFreq = f; }
int __cdecl Profile::GetFuncsCount() { return nextid; }
Profile::Info** __cdecl Profile::GetFuncs() { return funcs; }
const Profile::TraceInfo* __cdecl Profile::GetTraceInfo() { return tracePoint; }
const Profile::FrameTrace* __cdecl Profile::GetFrameTraceData() { return traceData; }
unsigned int __cdecl Profile::GetNumTracePoints() { return numTracePoints; }
unsigned int __cdecl Profile::GetTraceFrame() { return traceFrame; }

int __cdecl Profile::SortTotal(const void* a, const void* b)
{
    return static_cast<int>((*static_cast<Info* const*>(b))->time - (*static_cast<Info* const*>(a))->time);
}

int __cdecl Profile::SortAvg(const void* a, const void* b)
{
    const Info* x = *static_cast<Info* const*>(a);
    const Info* y = *static_cast<Info* const*>(b);
    const unsigned long ax = x->count ? x->time / x->count : 0;
    if (y->count) return static_cast<int>(y->time / y->count - ax);
    return static_cast<int>(0 - ax);
}

int __cdecl Profile::SortCount(const void* a, const void* b)
{
    return static_cast<int>((*static_cast<Info* const*>(b))->count - (*static_cast<Info* const*>(a))->count);
}

void __cdecl Profile::SortBy(SortOption option)
{
    if (option == SORT_TOTAL)
        sort_func = &SortTotal;
    else if (option == SORT_AVG)
        sort_func = &SortAvg;
    else if (option == SORT_COUNT)
        sort_func = &SortCount;
}

void __cdecl Profile::Sort() { Crt80::qsort(funcs, nextid, 4, sort_func); }

void __cdecl Profile::Reset()
{
    for (int i = 0; i < nextid; ++i) {
        funcs[i]->time = 0;
        funcs[i]->count = 0;
    }
}

void __cdecl Profile::ResetPeeks()
{
    for (int i = 0; i < nextid; ++i) funcs[i]->peek = 0;
}

// the logging of the sorted table is compiled out of the original: only the sort is left
void __cdecl Profile::LogDisplay()
{
    if (enabled) Crt80::qsort(funcs, nextid, 4, sort_func);
}

// The table of the first `max` functions with at least 64 ticks (in the current order): calls, total and average
// in ms, the peak average, a bar of the share of the total; then the counters start again.
void __cdecl Profile::Display(Console& console, int max)
{
    if (!enabled) return;
    const unsigned int perMs = static_cast<unsigned int>(Time::freq) / 1000;
    unsigned int sum = 0;
    for (int i = 0; i < nextid; ++i) {
        if (i >= max) break;
        Info* f = funcs[i];
        if (!(f->time & 0xffffffc0u)) break;
        const unsigned long avg = f->time * 1000 / f->count / perMs;
        if (avg > f->peek) f->peek = avg;
        sum += funcs[i]->time >> 6;
    }
    console.Printf("Function:                       Count    Total    Avg  Total: %lu\n", sum * 64000 / perMs);
    ProfileWork::Line(console);
    if (sum) {
        for (int i = 0; i < nextid; ++i) {
            if (i >= max) break;
            Info* f = funcs[i];
            if (!(f->time & 0xffffffc0u)) break;
            const unsigned long ms1000 = f->time * 1000;
            const unsigned long count = f->count;
            console.Printf("%-28.28s %6u %8lu %6lu %6u ", f->name, count, ms1000 / perMs, ms1000 / count / perMs,
                           f->peek);
            const unsigned int bar = funcs[i]->time * 36 / sum >> 6;
            if (bar - 1 <= 0x22) console.Bar(static_cast<int>(bar), 0x12);
            ProfileWork::Line(console);
        }
    }
    for (int i = 0; i < nextid; ++i) {
        funcs[i]->time = 0;
        funcs[i]->count = 0;
    }
}

// every displayPeriod seconds of `elapsed`: sorted every sortFreq seconds, shown, the peaks reset every peekResetFreq
void __cdecl Profile::Display(float elapsed, Console& console, int max)
{
    if (!(elapsed > 0.0f)) return;
    float& shown = displayClock[0];
    float& sorted = displayClock[1];
    float& peeks = displayClock[2];
    peeks += elapsed;
    shown += elapsed;
    sorted += elapsed;
    if (!(shown > displayPeriod)) return;
    shown -= displayPeriod;
    if (!(displayPeriod > shown)) shown = 0.0f;
    if (sorted > sortFreq) {
        Crt80::qsort(funcs, nextid, 4, sort_func);
        sorted = 0.0f;
    }
    console.Clear();
    Display(console, max);
    if (peeks > peekResetFreq) {
        ResetPeeks();
        peeks = 0.0f;
    }
}

// "START" takes slot 0; the others the next of 128 slots (-0x151 when full)
int __cdecl Profile::RegisterTracePoint(const char* name)
{
    if (std::strcmp(name, "START") == 0) {
        tracePoint[0].name = name;
        if (numTracePoints == 0) numTracePoints = 1;
        return 0;
    }
    const unsigned int n = numTracePoints;
    if (n >= 0x80) return -0x151;
    if (n != 0) {
        tracePoint[n].name = name;
        numTracePoints = n + 1;
        return static_cast<int>(n);
    }
    numTracePoints = 2;
    tracePoint[1].name = name;
    return 1;
}

// id 0 starts the next of 128 frames; each frame keeps up to 128 {id, counter} points
void __cdecl Profile::TracePoint(int id)
{
    if (id == -0x151) return;
    unsigned int f = traceFrame;
    if (id == 0) {
        f = (f + 1) & 0x7f;
        traceFrame = f;
        traceData[f].count = 0;
    }
    FrameTrace& t = traceData[f];
    const unsigned int n = t.count;
    if (n < 0x80) {
        t.count = n + 1;
        t.points[n].id = id;
        t.points[n].time = ProfileWork::Counter();
    }
}

// ---- QuickTimer

QuickTimer::QuickTimer(Info* info_)
{
    frame = currentFrame;
    info = info_;
    ++threadInfo->depth;
    parent = threadInfo->current;
    threadInfo->current = this;
    if (info_->id == -1) {
        const int id = InterlockedIncrement(reinterpret_cast<volatile long*>(&nextid)) - 1;
        info_->id = id;
        if (id < 0x40)
            funcs[id] = info_;
        else
            info_->id = -2;
    }
    childTime = 0;
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    start = static_cast<unsigned __int64>(c.QuadPart);
}

QuickTimer::~QuickTimer()
{
    Stats& s = info->frames[frame];
    ++s.count;
    const unsigned long spent = ProfileWork::Counter() - static_cast<unsigned long>(start);
    s.total += spent;
    s.total -= static_cast<unsigned long>(childTime);
    if (s.count > 1) {
        if (spent < s.min)
            s.min = spent;
        else if (spent > s.max)
            s.max = spent;
    } else {
        s.max = spent;
        s.min = spent;
    }
    if (parent && childtime) parent->childTime += spent;
    threadInfo->current = parent;
    --threadInfo->depth;
}

QuickTimer& QuickTimer::operator=(const QuickTimer& o)
{
    std::memcpy(this, &o, sizeof *this);
    return *this;
}

int __cdecl QuickTimer::GetCurrentWriteFrame() { return currentFrame; }

// a frame ends: its length is kept, the next frame's slot (two ahead in the ring of 4) is emptied; the report every
// logEvery frames is compiled out of the original (it formats into an empty log call)
void __cdecl QuickTimer::Update()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const unsigned long spent = now.LowPart - static_cast<unsigned long>(lastFrameStart);
    lastFrameStart = static_cast<unsigned __int64>(now.QuadPart);
    const int was = currentFrame;
    const int next = (was + 1) & 3;
    const unsigned int clear = (next - 2) & 3;
    currentFrame = next;
    int n = *reinterpret_cast<volatile long*>(&nextid);
    frameTimes[was] = spent;
    if (n > 0x40) n = 0x40;
    const int frames = numFrames;
    volatile int report = frames % logEvery;  // as the original: no frames between reports traps
    (void)report;
    numFrames = frames + 1;
    for (int i = 0; i < n; ++i)
        if (Info* f = funcs[i]) f->frames[clear] = Stats{0, 0, 0, 0};
}

}  // namespace Jet
