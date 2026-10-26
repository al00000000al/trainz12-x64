// Differential test of KernelStandard unit U6 (Platform.cpp): DynamicLibrary, Events / Events::Event,
// SharedMemory, SharedScratchPad, Job, Jobs. Each scenario is drawn once, then run on objects made by our
// KernelStandard_7c.dll and by KernelStandard_7c_orig.dll (both in this process, called through raw exports);
// every observable result goes into a log per side and the logs must match.
//
// Events: random scripts of Add / Update / Remove(id) / Remove(event) / RemoveEventSafely / GenerateId, on queues
// that own their events or not. The events carry our vftable: Update logs, decides by a hash of the event's tag
// and its update count (the same on both sides) whether it finishes, how its time moves, and whether it removes
// itself or another event, or adds one, from inside the update. After every step the three lists (by tag, with
// times, ids and link consistency) and the counters are logged.
// Jobs: batches of jobs (names, children of a parent added before them, a kick job to release a waiting parent)
// through Init / Add / Update / Done; Process and Dispatch log from our vftable. Job's own members, the copies and
// the default constructor closure are called directly.
//
// usage: diff_ks_platform.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

template <class F>
static F Get(HMODULE m, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}

// ---- logging per side
static int side;
static std::vector<std::string> logs[2];
static void Log(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logs[side].push_back(buf);
}

static uint32_t Hash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0xC2B2AE3Du;
    return h ^ (h >> 13);
}

// ---- the exports of one module
struct Api {
    HMODULE m;
    void*(__cdecl* allocNew)(unsigned int);
    void(__cdecl* allocDelete)(void*);
    // Events
    void*(__fastcall* eventsCtor)(void*, void*, bool);
    void*(__fastcall* eventsCtor0)(void*, void*);
    void(__fastcall* eventsDtor)(void*, void*);
    void*(__fastcall* eventsAssign)(void*, void*, const void*);
    void(__fastcall* generateId)(void*, void*, void*);
    void(__fastcall* add)(void*, void*, void*);
    void(__fastcall* update)(void*, void*, unsigned long);
    bool(__fastcall* removeSafely)(void*, void*, void*);
    bool(__fastcall* removeId)(void*, void*, unsigned long);
    void(__fastcall* removeEvent)(void*, void*, void*);
    // Event
    void*(__fastcall* eventCtor)(void*, void*, unsigned long, unsigned long);
    void*(__fastcall* eventCopy)(void*, void*, const void*);
    void*(__fastcall* eventAssign)(void*, void*, const void*);
    int(__fastcall* eventLess)(const void*, void*, const void*);
    bool(__fastcall* eventUpdate)(void*, void*);
    unsigned long(__fastcall* eventGetId)(const void*, void*);
    unsigned long(__fastcall* eventGetNextTime)(const void*, void*);
    void(__fastcall* eventSetId)(void*, void*, unsigned long);
    void(__fastcall* eventUpdateNext)(void*, void*);
    void* eventVt;
    // DynamicLibrary
    void*(__fastcall* dlCtor)(void*, void*);
    void*(__fastcall* dlCopy)(void*, void*, const void*);
    void*(__fastcall* dlAssign)(void*, void*, const void*);
    void(__fastcall* dlDtor)(void*, void*);
    bool(__cdecl* dlOpen)(void*, const char*);
    void(__cdecl* dlClose)(void*);
    bool(__cdecl* dlImport)(void*, void**, const char*);
    bool(__cdecl* dlModuleName)(void*, void*, void**);
    void* dlVt;
    // SharedMemory
    void*(__fastcall* smCtor)(void*, void*, const char*, unsigned int);
    void(__fastcall* smDtor)(void*, void*);
    void*(__fastcall* smAssign)(void*, void*, const void*);
    bool(__fastcall* smAttach)(void*, void*);
    bool(__fastcall* smDetach)(void*, void*);
    void*(__fastcall* smData)(void*, void*);
    bool(__fastcall* smTry)(void*, void*);
    void(__fastcall* smUnlock)(void*, void*);
    // SharedScratchPad
    void*(__cdecl* padAccess)();
    unsigned char*(__cdecl* padBuffer)(void*, unsigned int);
    bool(__fastcall* release)(const void*, void*);
    void** padArray;
    // Job / Jobs
    void*(__fastcall* jobCtor)(void*, void*, void*);
    void*(__fastcall* jobCopy)(void*, void*, const void*);
    void*(__fastcall* jobAssign)(void*, void*, const void*);
    void(__fastcall* jobDtor)(void*, void*);
    void(__fastcall* jobClosure)(void*, void*);
    const char*(__fastcall* jobName)(const void*, void*);
    void(__cdecl* jobProcess)(void*);
    void(__cdecl* jobDispatch)(void*);
    float(__fastcall* jobProgress)(const void*, void*);
    void(__fastcall* jobSetProgress)(void*, void*, float);
    void(__fastcall* jobLock)(void*, void*);
    void(__fastcall* jobUnlock)(void*, void*);
    bool(__fastcall* jobIsDone)(const void*, void*);
    void(__cdecl* jobStart)(void*);
    void(__cdecl* jobEnd)(void*);
    void* jobVt;
    void*(__fastcall* jobsCtor)(void*, void*);
    void(__cdecl* jobsInit)(void*, void*);
    void(__cdecl* jobsDone)(void*);
    void(__cdecl* jobsUpdate)(void*);
    void(__cdecl* jobsAdd)(void*, void*);
    bool(__fastcall* jobsActive)(const void*, void*);
    void*(__fastcall* optionsCtor)(void*, void*);
};
static Api apis[2];
static const Api* A;

static void Load(Api& a, HMODULE m, HMODULE orig)
{
    a.m = m;
    a.allocNew = Get<decltype(a.allocNew)>(orig, "?New@Alloc@Jet@@SAPAXI@Z");
    a.allocDelete = Get<decltype(a.allocDelete)>(orig, "?Delete@Alloc@Jet@@SAXPAX@Z");
    a.eventsCtor = Get<decltype(a.eventsCtor)>(m, "??0Events@Jet@@QAE@_N@Z");
    a.eventsCtor0 = Get<decltype(a.eventsCtor0)>(m, "??0Events@Jet@@QAE@XZ");
    a.eventsDtor = Get<decltype(a.eventsDtor)>(m, "??1Events@Jet@@QAE@XZ");
    a.eventsAssign = Get<decltype(a.eventsAssign)>(m, "??4Events@Jet@@QAEAAV01@ABV01@@Z");
    a.generateId = Get<decltype(a.generateId)>(m, "?GenerateId@Events@Jet@@QAEXPAVEvent@12@@Z");
    a.add = Get<decltype(a.add)>(m, "?Add@Events@Jet@@QAEXPAVEvent@12@@Z");
    a.update = Get<decltype(a.update)>(m, "?Update@Events@Jet@@QAEXK@Z");
    a.removeSafely = Get<decltype(a.removeSafely)>(m, "?RemoveEventSafely@Events@Jet@@QAE_NPAVEvent@12@@Z");
    a.removeId = Get<decltype(a.removeId)>(m, "?Remove@Events@Jet@@QAE_NK@Z");
    a.removeEvent = Get<decltype(a.removeEvent)>(m, "?Remove@Events@Jet@@QAEXPAVEvent@12@@Z");
    a.eventCtor = Get<decltype(a.eventCtor)>(m, "??0Event@Events@Jet@@QAE@KK@Z");
    a.eventCopy = Get<decltype(a.eventCopy)>(m, "??0Event@Events@Jet@@QAE@ABV012@@Z");
    a.eventAssign = Get<decltype(a.eventAssign)>(m, "??4Event@Events@Jet@@QAEAAV012@ABV012@@Z");
    a.eventLess = Get<decltype(a.eventLess)>(m, "??MEvent@Events@Jet@@QBEHABV012@@Z");
    a.eventUpdate = Get<decltype(a.eventUpdate)>(m, "?Update@Event@Events@Jet@@UAE_NXZ");
    a.eventGetId = Get<decltype(a.eventGetId)>(m, "?GetId@Event@Events@Jet@@UBEKXZ");
    a.eventGetNextTime = Get<decltype(a.eventGetNextTime)>(m, "?GetNextTime@Event@Events@Jet@@QBEKXZ");
    a.eventSetId = Get<decltype(a.eventSetId)>(m, "?SetId@Event@Events@Jet@@QAEXK@Z");
    a.eventUpdateNext = Get<decltype(a.eventUpdateNext)>(m, "?UpdateNext@Event@Events@Jet@@IAEXXZ");
    a.eventVt = Get<void*>(m, "??_7Event@Events@Jet@@6B@");
    a.dlCtor = Get<decltype(a.dlCtor)>(m, "??0DynamicLibrary@Jet@@QAE@XZ");
    a.dlCopy = Get<decltype(a.dlCopy)>(m, "??0DynamicLibrary@Jet@@QAE@ABV01@@Z");
    a.dlAssign = Get<decltype(a.dlAssign)>(m, "??4DynamicLibrary@Jet@@QAEAAV01@ABV01@@Z");
    a.dlDtor = Get<decltype(a.dlDtor)>(m, "??1DynamicLibrary@Jet@@UAE@XZ");
    a.dlOpen = Get<decltype(a.dlOpen)>(m, "?Open@DynamicLibrary@Jet@@UAA_NPBD@Z");
    a.dlClose = Get<decltype(a.dlClose)>(m, "?Close@DynamicLibrary@Jet@@UAAXXZ");
    a.dlImport = Get<decltype(a.dlImport)>(m, "?Import@DynamicLibrary@Jet@@UAA_NPAPAXPBD@Z");
    a.dlModuleName = Get<decltype(a.dlModuleName)>(m, "?GetModuleNameAndAddress@DynamicLibrary@Jet@@SA_NPAXAAVString@2@AAPAX@Z");
    a.dlVt = Get<void*>(m, "??_7DynamicLibrary@Jet@@6B@");
    a.smCtor = Get<decltype(a.smCtor)>(m, "??0SharedMemory@Jet@@QAE@PBDI@Z");
    a.smDtor = Get<decltype(a.smDtor)>(m, "??1SharedMemory@Jet@@QAE@XZ");
    a.smAssign = Get<decltype(a.smAssign)>(m, "??4SharedMemory@Jet@@QAEAAV01@ABV01@@Z");
    a.smAttach = Get<decltype(a.smAttach)>(m, "?ProcessAttach@SharedMemory@Jet@@QAE_NXZ");
    a.smDetach = Get<decltype(a.smDetach)>(m, "?ProcessDetach@SharedMemory@Jet@@QAE_NXZ");
    a.smData = Get<decltype(a.smData)>(m, "?Data@SharedMemory@Jet@@QAEPAXXZ");
    a.smTry = Get<decltype(a.smTry)>(m, "?TryExclusiveLock@SharedMemory@Jet@@QAE_NXZ");
    a.smUnlock = Get<decltype(a.smUnlock)>(m, "?Unlock@SharedMemory@Jet@@QAEXXZ");
    a.padAccess = Get<decltype(a.padAccess)>(m, "?GetAccess@SharedScratchPad@Jet@@SAPAV12@XZ");
    a.padBuffer = Get<decltype(a.padBuffer)>(m, "?GetBuffer@SharedScratchPad@Jet@@QAAPAEI@Z");
    a.release = Get<decltype(a.release)>(orig, "?ReleaseReference@ReferenceCount@Jet@@QBE_NXZ");
    a.padArray = Get<void**>(m, "?scratchPadArray@SharedScratchPad@Jet@@1V?$FastDynamicArray@PAVSharedScratchPad@Jet@@$0N@U?$Less@PAVSharedScratchPad@Jet@@PAV12@@2@@2@A");
    a.jobCtor = Get<decltype(a.jobCtor)>(m, "??0Job@Jet@@QAE@PAV01@@Z");
    a.jobCopy = Get<decltype(a.jobCopy)>(m, "??0Job@Jet@@QAE@ABV01@@Z");
    a.jobAssign = Get<decltype(a.jobAssign)>(m, "??4Job@Jet@@QAEAAV01@ABV01@@Z");
    a.jobDtor = Get<decltype(a.jobDtor)>(m, "??1Job@Jet@@UAE@XZ");
    a.jobClosure = Get<decltype(a.jobClosure)>(m, "??_FJob@Jet@@QAEXXZ");
    a.jobName = Get<decltype(a.jobName)>(m, "?GetName@Job@Jet@@UBEPBDXZ");
    a.jobProcess = Get<decltype(a.jobProcess)>(m, "?Process@Job@Jet@@MAAXXZ");
    a.jobDispatch = Get<decltype(a.jobDispatch)>(m, "?Dispatch@Job@Jet@@MAAXXZ");
    a.jobProgress = Get<decltype(a.jobProgress)>(m, "?GetProgress@Job@Jet@@QBEMXZ");
    a.jobSetProgress = Get<decltype(a.jobSetProgress)>(m, "?SetProgress@Job@Jet@@AAEXM@Z");
    a.jobLock = Get<decltype(a.jobLock)>(m, "?Lock@Job@Jet@@AAEXXZ");
    a.jobUnlock = Get<decltype(a.jobUnlock)>(m, "?Unlock@Job@Jet@@AAEXXZ");
    a.jobIsDone = Get<decltype(a.jobIsDone)>(m, "?IsDone@Job@Jet@@ABE_NXZ");
    a.jobStart = Get<decltype(a.jobStart)>(m, "?StartProcessing@Job@Jet@@AAAXXZ");
    a.jobEnd = Get<decltype(a.jobEnd)>(m, "?EndProcessing@Job@Jet@@AAAXXZ");
    a.jobVt = Get<void*>(m, "??_7Job@Jet@@6B@");
    a.jobsCtor = Get<decltype(a.jobsCtor)>(m, "??0Jobs@Jet@@QAE@XZ");
    a.jobsInit = Get<decltype(a.jobsInit)>(m, "?Init@Jobs@Jet@@QAAXPAVOptions@2@@Z");
    a.jobsDone = Get<decltype(a.jobsDone)>(m, "?Done@Jobs@Jet@@QAAXXZ");
    a.jobsUpdate = Get<decltype(a.jobsUpdate)>(m, "?Update@Jobs@Jet@@QAAXXZ");
    a.jobsAdd = Get<decltype(a.jobsAdd)>(m, "?Add@Jobs@Jet@@QAAXPAVJob@2@@Z");
    a.jobsActive = Get<decltype(a.jobsActive)>(m, "?IsActive@Jobs@Jet@@QBE_NXZ");
    a.optionsCtor = Get<decltype(a.optionsCtor)>(m, "??0Options@Jet@@QAE@XZ");
}

static uint32_t U32(const void* p, int off) { return *reinterpret_cast<const uint32_t*>(static_cast<const char*>(p) + off); }
static void* Ptr(const void* p, int off) { return *reinterpret_cast<void* const*>(static_cast<const char*>(p) + off); }

// ================= Events
// our events: 0x18 bytes of Event, then the tag (+0x18)
static constexpr int kTag = 0x18;
static unsigned char* curEvents;
static std::map<uint32_t, unsigned char*> evByTag[2];  // live events, listed or detached
static std::set<uint32_t> detachedNow[2];               // taken out by our callback during an Update
static std::map<uint32_t, uint32_t> updates[2];         // update count per tag
static uint32_t nextTag;

static uint32_t TagOf(const void* e) { return U32(e, kTag); }

static unsigned char* NewEvent(uint32_t tag, unsigned long interval, unsigned long next);

static bool __fastcall EvUpdate(unsigned char* e, void*)
{
    const uint32_t tag = TagOf(e);
    const uint32_t n = updates[side][tag]++;
    const uint32_t h = Hash(tag, n);
    Log("update %u #%u next %lu", tag, n, U32(e, 0x14));
    // the time moves: the base update (adds the interval), UpdateNext, or a new time
    if (h % 3 == 0)
        A->eventUpdate(e, nullptr);
    else if (h % 3 == 1)
        A->eventUpdateNext(e, nullptr);
    else
        *reinterpret_cast<uint32_t*>(e + 0x14) += 1 + (h >> 8) % 40;
    const bool finished = (h >> 4) % 4 == 0;
    if ((h >> 12) % 9 == 0) {  // removes itself safely
        const bool r = A->removeSafely(curEvents, nullptr, e);
        Log("  self safe -> %d", r);
        if (r) detachedNow[side].insert(tag);
    }
    if ((h >> 16) % 7 == 0 && !evByTag[side].empty()) {  // removes another by id (the tag picks it)
        auto it = evByTag[side].lower_bound(Hash(h, 1) % (nextTag + 1));
        if (it == evByTag[side].end()) it = evByTag[side].begin();
        const unsigned long id = U32(it->second, 0x10);
        const bool r = A->removeId(curEvents, nullptr, id);
        Log("  remove id %lu (tag %u) -> %d", id, it->first, r);
    }
    if ((h >> 20) % 8 == 0) {  // adds one
        const uint32_t t = ++nextTag;
        unsigned char* ne = NewEvent(t, 1 + Hash(h, 2) % 30, U32(e, 0x14) + Hash(h, 3) % 20);
        A->generateId(curEvents, nullptr, ne);
        A->add(curEvents, nullptr, ne);
        Log("  add %u", t);
    }
    Log("  -> %d", finished);
    return finished;
}

static unsigned long __fastcall EvGetId(const unsigned char* e, void*) { return U32(e, 0x10); }
static void* evVt[2] = {reinterpret_cast<void*>(&EvUpdate), reinterpret_cast<void*>(&EvGetId)};

static unsigned char* NewEvent(uint32_t tag, unsigned long interval, unsigned long next)
{
    auto* e = static_cast<unsigned char*>(A->allocNew(0x1c));
    A->eventCtor(e, nullptr, interval, next);
    *reinterpret_cast<void**>(e) = evVt;
    *reinterpret_cast<uint32_t*>(e + kTag) = tag;
    evByTag[side][tag] = e;
    return e;
}

// the three lists by tag, with link checks; returns the tags listed
static std::set<uint32_t> DumpEvents(const unsigned char* ev)
{
    std::set<uint32_t> listed;
    std::string s = "events own" + std::to_string(ev[0]) + " last" + std::to_string(U32(ev, 4));
    static const char* names[3] = {" removed", " queue", " requeue"};
    for (int l = 0; l < 3; ++l) {
        const int off = 8 + 0xc * l;
        s += names[l];
        s += "(" + std::to_string(U32(ev, off + 8)) + "):";
        const unsigned char* prev = nullptr;
        unsigned n = 0;
        for (auto* e = static_cast<const unsigned char*>(Ptr(ev, off)); e;) {
            const void* back = Ptr(e, 8);
            const bool ok = back == (prev ? prev + 4 : nullptr);
            s += " " + std::to_string(TagOf(e)) + "@" + std::to_string(U32(e, 0x14)) + "/" + std::to_string(U32(e, 0x10)) + "+" +
                 std::to_string(U32(e, 0xc)) + (ok ? "" : "!");
            listed.insert(TagOf(e));
            prev = e;
            const auto* next = static_cast<const unsigned char*>(Ptr(e, 4));
            e = next ? next - 4 : nullptr;
            if (++n > 10000) break;
        }
        if (Ptr(ev, off + 4) != prev) s += " tail!";
    }
    Log("%s", s.c_str());
    return listed;
}

struct EvOp {
    int kind;
    uint32_t a, b, c;
};

static void RunEvents(const std::vector<EvOp>& ops, bool owns, bool defaultCtor)
{
    const Api& a = *A;
    alignas(8) unsigned char ev[0x2c];
    std::memset(ev, 0xcd, sizeof(ev));
    if (defaultCtor)
        a.eventsCtor0(ev, nullptr);
    else
        a.eventsCtor(ev, nullptr, owns);
    curEvents = ev;
    evByTag[side].clear();
    detachedNow[side].clear();
    updates[side].clear();
    nextTag = 0;
    std::set<uint32_t> listed = DumpEvents(ev);
    unsigned long time = 0;
    for (const EvOp& op : ops) {
        if (getenv("KSP_TRACE")) std::fprintf(stderr, "side %d op %d\n", side, op.kind);
        const uint32_t tagBefore = op.kind == 0 ? nextTag + 1 : nextTag;  // (the op's own new event is not listed yet)
        std::set<uint32_t> detached;  // what this op hands back to us
        switch (op.kind) {
        case 0: {  // a new event, maybe with a generated id
            const uint32_t t = ++nextTag;
            unsigned char* e = NewEvent(t, op.a % 25, time + op.b % 60);
            if (op.c & 1) a.generateId(ev, nullptr, e);
            else a.eventSetId(e, nullptr, op.c % 50);
            a.add(ev, nullptr, e);
            Log("add %u", t);
            break;
        }
        case 1: {  // a detached one again
            for (auto& [t, e] : evByTag[side])
                if (!listed.count(t) && Hash(t, op.a) % 2) {
                    a.add(ev, nullptr, e);
                    Log("re-add %u", t);
                    break;
                }
            break;
        }
        case 2:
            time += op.a % 30;
            Log("Update %lu", time);
            detachedNow[side].clear();
            a.update(ev, nullptr, time);
            break;
        case 3: {  // remove by id (an id that may or may not exist)
            unsigned long id = op.a % 60;
            if ((op.b & 3) && !evByTag[side].empty()) {
                auto it = evByTag[side].lower_bound(op.c % (nextTag + 1));
                if (it == evByTag[side].end()) it = evByTag[side].begin();
                id = U32(it->second, 0x10);
            }
            const bool r = a.removeId(ev, nullptr, id);
            Log("Remove id %lu -> %d", id, r);
            break;
        }
        case 4:
        case 5: {  // Remove(event) / RemoveEventSafely of a live event
            if (evByTag[side].empty()) break;
            auto it = evByTag[side].lower_bound(op.a % (nextTag + 1));
            if (it == evByTag[side].end()) it = evByTag[side].begin();
            if (op.kind == 4) {
                a.removeEvent(ev, nullptr, it->second);
                Log("Remove event %u", it->first);
            } else {
                const bool r = a.removeSafely(ev, nullptr, it->second);
                Log("RemoveEventSafely %u -> %d", it->first, r);
            }
            break;
        }
        case 6: {  // the Event members on a copy
            if (evByTag[side].empty()) break;
            auto it = evByTag[side].lower_bound(op.a % (nextTag + 1));
            if (it == evByTag[side].end()) it = evByTag[side].begin();
            const unsigned char* e = it->second;
            alignas(8) unsigned char c1[0x18], c2[0x18];
            a.eventCopy(c1, nullptr, e);
            std::memset(c2, 0x5a, sizeof(c2));
            a.eventAssign(c2, nullptr, e);
            const bool same1 = std::memcmp(c1 + 4, e + 4, 0x14) == 0 && Ptr(c1, 0) == a.eventVt;
            const bool same2 = std::memcmp(c2 + 4, e + 4, 0x14) == 0 && Ptr(c2, 0) == Ptr(c2, 0);
            Log("copy %u %d %d less %d %d getid %lu next %lu", it->first, same1, same2, a.eventLess(c1, nullptr, e),
                a.eventLess(e, nullptr, c2), a.eventGetId(c1, nullptr), a.eventGetNextTime(c1, nullptr));
            break;
        }
        }
        const std::set<uint32_t> now = DumpEvents(ev);
        // listed before (or made during the op), gone now: handed back to us, or deleted by the queue
        for (auto& [t, e] : evByTag[side])
            if (t > tagBefore) listed.insert(t);
        for (uint32_t t : listed)
            if (!now.count(t) && evByTag[side].count(t)) {
                const bool back = op.kind == 4 || op.kind == 5 || detachedNow[side].count(t) || !ev[0];
                if (!back) evByTag[side].erase(t);
                Log("  %u %s", t, back ? "detached" : "deleted");
            }
        listed = now;
    }
    // the destructor deletes the listed ones; we free the rest
    a.eventsDtor(ev, nullptr);
    for (auto& [t, e] : evByTag[side])
        if (!listed.count(t)) a.allocDelete(e);
    evByTag[side].clear();
}

// ================= DynamicLibrary, SharedMemory, SharedScratchPad
static void RunMisc(uint32_t seed)
{
    const Api& a = *A;
    // DynamicLibrary
    alignas(8) unsigned char dl[8], dl2[8];
    a.dlCtor(dl, nullptr);
    Log("dl vt %d handle %u", Ptr(dl, 0) == a.dlVt, U32(dl, 4));
    const char* const libs[] = {"kernel32.dll", "no_such_library_xyz.dll", "user32.dll", ""};
    const char* const procs[] = {"GetTickCount", "NoSuchProc", "MessageBoxA", "Sleep"};
    for (int i = 0; i < 6; ++i) {
        const char* lib = libs[Hash(seed, i) % 4];
        const bool o = a.dlOpen(dl, lib);
        void* p = reinterpret_cast<void*>(1);
        const char* pn = procs[Hash(seed, i + 10) % 4];
        const bool im = a.dlImport(dl, &p, pn);
        Log("dl open %s -> %d handle %d import %s -> %d %p", lib, o, U32(dl, 4) != 0, pn, im,
            p ? reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA(lib), pn)) == p : 0);
        if (Hash(seed, i + 20) % 3 == 0) {
            a.dlClose(dl);
            Log("dl close handle %u", U32(dl, 4));
        }
    }
    a.dlCopy(dl2, nullptr, dl);
    Log("dl copy %d %d", Ptr(dl2, 0) == a.dlVt, U32(dl2, 4) == U32(dl, 4));
    *reinterpret_cast<uint32_t*>(dl2 + 4) = 0;
    a.dlAssign(dl2, nullptr, dl);
    Log("dl assign %d", U32(dl2, 4) == U32(dl, 4));
    *reinterpret_cast<uint32_t*>(dl2 + 4) = 0;  // (one owner of the handle)
    a.dlDtor(dl2, nullptr);
    a.dlDtor(dl, nullptr);
    Log("dl dtor handle %u", U32(dl, 4));
    void* base = reinterpret_cast<void*>(7);
    Log("dl modname %d %p", a.dlModuleName(nullptr, nullptr, &base), base);

    // SharedMemory (the name per side, so the two do not share a mapping)
    char name[64];
    std::snprintf(name, sizeof(name), "diff_ks_platform_%lu_%d_%u", GetCurrentProcessId(), side, seed);
    const unsigned size = 1 + Hash(seed, 99) % 100000;
    alignas(8) unsigned char sm[0x10], sm2[0x10];
    a.smCtor(sm, nullptr, name, size);
    Log("sm ctor data %d size %u name %d map %d", Ptr(sm, 0) != nullptr, U32(sm, 8), Ptr(sm, 4) == name, Ptr(sm, 0xc) != nullptr);
    Log("sm data before %d try %d", a.smData(sm, nullptr) != nullptr, a.smTry(sm, nullptr));
    a.smUnlock(sm, nullptr);
    const bool at = a.smAttach(sm, nullptr);
    auto* d = static_cast<unsigned char*>(a.smData(sm, nullptr));
    Log("sm attach %d data %d map %d", at, d != nullptr, Ptr(sm, 0xc) != nullptr);
    if (d) {
        d[0] = 0x11;
        d[size - 1] = 0x22;
        Log("sm rw %d %d", d[0], d[size - 1]);
    }
    std::memset(sm2, 0, sizeof(sm2));
    a.smAssign(sm2, nullptr, sm);
    Log("sm assign %d", std::memcmp(sm, sm2, 0x10) == 0);
    Log("sm detach %d data %d map %d", a.smDetach(sm, nullptr), Ptr(sm, 0) != nullptr, Ptr(sm, 0xc) != nullptr);
    Log("sm detach again %d", a.smDetach(sm, nullptr));
    a.smDtor(sm, nullptr);
    Log("sm dtor data %d map %d", Ptr(sm, 0) != nullptr, Ptr(sm, 0xc) != nullptr);

    // SharedScratchPad: this thread's pad, its growth; another thread's
    auto ArrayCount = [&]() -> unsigned {
        const void* head = *a.padArray;
        if (!head) return 0;
        unsigned n = 0;
        const void* c = head;
        do {
            n += U32(c, 8);
            c = Ptr(c, 0);
        } while (c != head);
        return n;
    };
    void* pad = a.padAccess();
    void* pad2 = a.padAccess();
    Log("pad same %d refs %u tid %d buf %d cap %u array %u", pad == pad2, U32(pad, 4), U32(pad, 8) == GetCurrentThreadId(),
        Ptr(pad, 0xc) != nullptr, U32(pad, 0x10), ArrayCount());
    unsigned char* last = nullptr;
    for (int i = 0; i < 8; ++i) {
        // (0 first, and sometimes exactly the size reached: the boundary of growing)
        const unsigned want = i == 0 ? 0 : Hash(seed, 300 + i) % 3 == 0 ? U32(pad, 0x10) : Hash(seed, 200 + i) % 5000;
        unsigned char* b = a.padBuffer(pad, want);
        Log("pad buffer %u -> %d same %d cap %u", want, b != nullptr, b == last, U32(pad, 0x10));
        last = b;
    }
    static const Api* threadApi;
    static void* threadPad;
    threadApi = A;
    HANDLE th = CreateThread(nullptr, 0, [](void*) -> DWORD {
        threadPad = threadApi->padAccess();
        return 0;
    }, nullptr, 0, nullptr);
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);
    Log("pad other thread differs %d tid %d array %u", threadPad != pad, U32(threadPad, 8) != GetCurrentThreadId(), ArrayCount());
    Log("release other %d array %u", a.release(threadPad, nullptr), ArrayCount());
    Log("release %d refs %u", a.release(pad2, nullptr), U32(pad, 4));
    Log("release last %d array %u", a.release(pad, nullptr), ArrayCount());
}

// ================= Job / Jobs
// our jobs: 0x28 bytes of Job, then the tag (+0x28)
static CRITICAL_SECTION jobLog;
static std::vector<std::string> processed[2];
static std::vector<uint32_t> dispatched[2];
static unsigned deleted[2];

static uint32_t JobTag(const void* j) { return U32(j, 0x28); }

static void* __fastcall JobDelete(unsigned char* j, void*, unsigned flags)
{
    ++deleted[side];
    A->jobDtor(j, nullptr);
    if (flags & 1) A->allocDelete(j);
    return j;
}
static const char* __fastcall JobGetName(const unsigned char* j, void*)
{
    static char names[2][4096][16];
    const uint32_t t = JobTag(j) % 4096;
    std::snprintf(names[side][t], 16, "job%u", JobTag(j));
    return names[side][t];
}
static void __cdecl JobProcessFn(unsigned char* j)  // on the jobs thread
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "process %u locks %u parent %d", JobTag(j), U32(j, 0x14), Ptr(j, 0x10) != nullptr);
    EnterCriticalSection(&jobLog);
    processed[side].push_back(buf);
    LeaveCriticalSection(&jobLog);
    if (Hash(JobTag(j), 5) % 4 == 0) Sleep(1);
}
static void __cdecl JobDispatchFn(unsigned char* j)
{
    const unsigned char* p = static_cast<const unsigned char*>(Ptr(j, 0x10));
    A->jobDispatch(j);
    Log("dispatch %u parent %u locks %u", JobTag(j), p ? JobTag(p) : 0, p ? U32(p, 0x14) : 0);
    dispatched[side].push_back(JobTag(j));
}
static void* jobVt[4] = {reinterpret_cast<void*>(&JobDelete), reinterpret_cast<void*>(&JobGetName),
                         reinterpret_cast<void*>(&JobProcessFn), reinterpret_cast<void*>(&JobDispatchFn)};

static unsigned char* NewJob(uint32_t tag, void* parent)
{
    auto* j = static_cast<unsigned char*>(A->allocNew(0x2c));
    A->jobCtor(j, nullptr, parent);
    *reinterpret_cast<void**>(j) = jobVt;
    *reinterpret_cast<uint32_t*>(j + 0x28) = tag;
    return j;
}

static void LogJob(const char* what, const unsigned char* j)
{
    Log("%s links %d %d progress %g parent %d locks %u times %d %d %d %d", what, Ptr(j, 4) != nullptr, Ptr(j, 8) != nullptr,
        *reinterpret_cast<const float*>(j + 0xc), Ptr(j, 0x10) != nullptr, U32(j, 0x14), U32(j, 0x18) != 0,
        U32(j, 0x1c) == U32(j, 0x18), U32(j, 0x20) == U32(j, 0x18), U32(j, 0x24) == U32(j, 0x18));
}

static bool WaitDispatched(unsigned char* jobs, size_t count)
{
    for (int i = 0; i < 5000; ++i) {
        A->jobsUpdate(jobs);
        if (dispatched[side].size() >= count) return true;
        Sleep(1);
    }
    Log("timeout waiting for %zu (have %zu)", count, dispatched[side].size());
    return false;
}

// the finished list (+0x2c) reaching a count, with no Update meanwhile (so nothing is dispatched)
static void WaitFinished(const unsigned char* jobs, unsigned count)
{
    for (int i = 0; i < 5000 && U32(jobs, 0x2c) < count; ++i) Sleep(1);
    Sleep(5);  // (the thread's pass over the waiting jobs)
}

static void RunJobs(uint32_t seed)
{
    const Api& a = *A;
    processed[side].clear();
    dispatched[side].clear();
    deleted[side] = 0;
    // Job's own members on plain jobs (the module's vftable)
    {
        alignas(8) unsigned char p[0x28], c[0x28], c2[0x28], d[0x28];
        a.jobCtor(p, nullptr, nullptr);
        LogJob("job", p);
        a.jobCtor(c, nullptr, p);
        LogJob("child", c);
        LogJob("parent", p);
        Log("name %s vt %d", a.jobName(p, nullptr), Ptr(p, 0) == a.jobVt);
        a.jobSetProgress(p, nullptr, 0.25f + static_cast<float>(seed % 7));
        a.jobLock(p, nullptr);
        Log("progress %g done %d", a.jobProgress(p, nullptr), a.jobIsDone(p, nullptr));
        a.jobUnlock(p, nullptr);
        a.jobUnlock(p, nullptr);
        Log("done %d locks %u", a.jobIsDone(p, nullptr), U32(p, 0x14));
        a.jobLock(p, nullptr);
        a.jobProcess(c);
        a.jobDispatch(c);
        Log("after dispatch locks %u", U32(p, 0x14));
        a.jobDispatch(p);
        a.jobCopy(c2, nullptr, c);
        Log("copy %d vt %d", std::memcmp(c2 + 4, c + 4, 0x24) == 0, Ptr(c2, 0) == a.jobVt);
        std::memset(d, 0x77, sizeof(d));
        a.jobAssign(d, nullptr, c);
        Log("assign %d vt %u", std::memcmp(d + 4, c + 4, 0x24) == 0, U32(d, 0));
        a.jobStart(c);
        a.jobEnd(c);
        Log("times %d", U32(c, 0x20) >= U32(c, 0x1c));
        std::memset(d, 0x66, sizeof(d));
        a.jobClosure(d, nullptr);
        LogJob("closure", d);
        Log("closure vt %d", Ptr(d, 0) == a.jobVt);
        a.jobDtor(d, nullptr);
        Log("dtor vt %d", Ptr(d, 0) == a.jobVt);
    }
    // the thread
    alignas(8) unsigned char options[0x40];
    a.optionsCtor(options, nullptr);
    alignas(8) unsigned char jobs[0xa8];
    std::memset(jobs, 0xcd, sizeof(jobs));
    a.jobsCtor(jobs, nullptr);
    Log("jobs ctor %u %u lists %u %u %u console %d %d", U32(jobs, 0), U32(jobs, 4), U32(jobs, 0x10), U32(jobs, 0x2c), U32(jobs, 0x48),
        jobs[0xa4], jobs[0xa5]);
    a.jobsInit(jobs, options);
    Log("init thread %d console %d", Ptr(jobs, 0x4c) != nullptr, jobs[0xa4]);
    uint32_t tag = 0;
    size_t expected = 0;
    // plain jobs
    const int n = 1 + Hash(seed, 1) % 12;
    for (int i = 0; i < n; ++i) a.jobsAdd(jobs, NewJob(++tag, nullptr));
    expected += n;
    WaitDispatched(jobs, expected);
    // a parent with children: the parent waits until they are dispatched and another job is processed
    if (Hash(seed, 2) % 2) {
        unsigned char* parent = NewJob(++tag, nullptr);
        const uint32_t ptag = tag;
        const int k = 1 + Hash(seed, 3) % 4;
        std::vector<unsigned char*> kids;
        for (int i = 0; i < k; ++i) kids.push_back(NewJob(++tag, parent));
        Log("parent %u locks %u", ptag, U32(parent, 0x14));
        a.jobsAdd(jobs, parent);
        for (unsigned char* c : kids) a.jobsAdd(jobs, c);
        expected += k;
        WaitFinished(jobs, k);  // none dispatched before the thread is past them: the parent stays waiting
        WaitDispatched(jobs, expected);
        Sleep(5);
        a.jobsUpdate(jobs);
        Log("children dispatched %zu parent locks %u", dispatched[side].size(), U32(parent, 0x14));
        a.jobsAdd(jobs, NewJob(++tag, nullptr));  // the kick
        expected += 2;
        WaitDispatched(jobs, expected);
    }
    Sleep(2);
    a.jobsUpdate(jobs);
    a.jobsDone(jobs);
    Log("done lists %u %u %u active %d thread %d deleted %u", U32(jobs, 0x10), U32(jobs, 0x2c), U32(jobs, 0x48),
        a.jobsActive(jobs, nullptr), Ptr(jobs, 0x4c) != nullptr, deleted[side]);
    for (const std::string& p : processed[side]) Log("%s", p.c_str());
    std::string order = "dispatch order";
    for (uint32_t t : dispatched[side]) order += " " + std::to_string(t);
    Log("%s", order.c_str());
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_ks_platform <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 2000;
    const bool trace = argc > 3;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    HMODULE orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    // each side's own Alloc and ReferenceCount (U14b)
    Load(apis[0], getenv("KSP_SWAP") ? orig : ours, getenv("KSP_SWAP") ? orig : ours);
    Load(apis[1], getenv("KSP_SWAP") ? ours : orig, getenv("KSP_SWAP") ? ours : orig);
    InitializeCriticalSection(&jobLog);
    std::mt19937 rng(1234);
    unsigned long long checks = 0, bad = 0;
    auto Compare = [&](const char* what, int sc) {
        if (getenv("KSP_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%s %d| %s\n", what, sc, l.c_str());
        const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < n; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 10) std::printf("%s scenario %d line %zu:\n  ours %s\n  orig %s\n", what, sc, i, x.c_str(), y.c_str());
                break;
            }
        }
        logs[0].clear();
        logs[1].clear();
    };
    for (int sc = 0; sc < scenarios; ++sc) {
        // events: the script is drawn once
        std::vector<EvOp> ops(1 + rng() % 120);
        for (EvOp& op : ops) {
            const uint32_t k = rng() % 100;
            op.kind = k < 30 ? 0 : k < 36 ? 1 : k < 64 ? 2 : k < 76 ? 3 : k < 84 ? 4 : k < 92 ? 5 : 6;
            op.a = rng();
            op.b = rng();
            op.c = rng();
        }
        const bool owns = rng() % 3 != 0;
        const bool defaultCtor = rng() % 5 == 0;
        if (trace) std::printf("sc %d events\n", sc);
        for (side = 0; side < 2; ++side) {
            A = &apis[side];
            RunEvents(ops, defaultCtor || owns, defaultCtor);
        }
        Compare("events", sc);
        if (sc % 10 == 0) {
            const uint32_t seed = rng();
            if (trace) std::printf("sc %d misc\n", sc);
            for (side = 0; side < 2; ++side) {
                A = &apis[side];
                RunMisc(seed);
            }
            Compare("misc", sc);
        }
        if (sc % 20 == 0) {
            const uint32_t seed = rng();
            if (trace) std::printf("sc %d jobs\n", sc);
            for (side = 0; side < 2; ++side) {
                A = &apis[side];
                if (trace) std::printf(" side %d\n", side);
                RunJobs(seed);
            }
            Compare("jobs", sc);
        }
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
