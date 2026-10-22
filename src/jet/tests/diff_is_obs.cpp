// Differential test of InputSystem_7c unit U1 (ObserverSubject<T> and the observer bases): our module against
// InputSystem_7c_orig.dll in one process. Subjects of both modules (all three exported instantiations) get the same
// random operations: AddObserver with priorities, RemoveObserver, HasObserver, Lock / Unlock with removals while
// locked, Count, the six iterator getters, copy construction, assignment (self-assignment too) and destruction.
// Compared after every step: the return values and the whole list (chunk ring, counts, the used entries, and the
// flag / priority of the unused slots). The observers are fake pointers: the subject never calls them.
//
// usage: diff_is_obs.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules> [steps]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

static HMODULE mods[2];
static unsigned long long checks, bad;
static std::mt19937 rng(4711);
static unsigned Pick(unsigned n) { return rng() % n; }

template <class F>
static F Get(HMODULE m, const std::string& name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name.c_str()));
    if (!p) {
        std::printf("missing %s\n", name.c_str());
        std::exit(2);
    }
    return p;
}

// the mangled names of one instantiation, from the observer class name
struct Api {
    using Ctor = void*(__fastcall*)(void*, void*);
    using Copy = void*(__fastcall*)(void*, void*, const void*);
    using Dtor = void(__fastcall*)(void*);
    using Assign = void*(__fastcall*)(void*, void*, const void*);
    using Add = void(__cdecl*)(void*, void*, int);
    using Remove = void(__cdecl*)(void*, void*);
    using Has = bool(__cdecl*)(const void*, const void*);
    using Op = void(__cdecl*)(void*);
    using CountF = unsigned long(__cdecl*)(void*);
    using Locks = int(__cdecl*)(const void*);
    using Iter = uint32_t*(__cdecl*)(void*, uint32_t*);
    Ctor ctor;
    Copy copy;
    Dtor dtor;
    Assign assign;
    Add add;
    Remove remove;
    Has has;
    Op lock, unlock;
    CountF count;
    Locks locks;
    Iter iters[6];  // First, First const, Last, Last const, End, End const
    void* vtable;

    Api(HMODULE m, const std::string& t)
    {
        const std::string c = "?$ObserverSubject@V" + t + "@Jet@@@Jet@@";
        const std::string arg = "PAV" + t + "@2@";
        ctor = Get<Ctor>(m, "??0" + c + "QAE@XZ");
        copy = Get<Copy>(m, "??0" + c + "QAE@ABV01@@Z");
        dtor = Get<Dtor>(m, "??1" + c + "UAE@XZ");
        assign = Get<Assign>(m, "??4" + c + "QAEAAV01@ABV01@@Z");
        add = Get<Add>(m, "?AddObserver@" + c + "UAAX" + arg + "H@Z");
        remove = Get<Remove>(m, "?RemoveObserver@" + c + "UAAX" + arg + "@Z");
        has = Get<Has>(m, "?HasObserver@" + c + "UBA_NPBV" + t + "@2@@Z");
        lock = Get<Op>(m, "?LockObservers@" + c + "QAAXXZ");
        unlock = Get<Op>(m, "?UnlockObservers@" + c + "QAAXXZ");
        count = Get<CountF>(m, "?Count@" + c + "QAAKXZ");
        locks = Get<Locks>(m, "?GetNumObserverLocks@" + c + "QBAHXZ");
        const char* names[] = {"FirstObserver", "LastObserver", "EndObserver"};
        for (int i = 0; i < 3; ++i) {
            iters[2 * i] = Get<Iter>(m, std::string("?") + names[i] + "@" + c + "QAA?AVObserverIterator@12@XZ");
            iters[2 * i + 1] =
                Get<Iter>(m, std::string("?") + names[i] + "@" + c + "QBA?AVConstObserverIterator@12@XZ");
        }
        vtable = Get<void*>(m, "??_7" + c + "6B@");
    }
};

// the subject as text: vtable tag, chunk ring, entries
static std::string Dump(const uint32_t* s, const Api& api)
{
    std::string out = s[0] == reinterpret_cast<uintptr_t>(api.vtable) ? "vt" : "vt?";
    char buf[96];
    std::snprintf(buf, sizeof(buf), " lock %d:", static_cast<int>(s[2]));
    out += buf;
    const auto* head = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(s[1]));
    if (!head) return out + " empty";
    const uint32_t* c = head;
    int guard = 0;
    do {
        if (reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(c[1]))[0] != reinterpret_cast<uintptr_t>(c))
            out += " BADLINK";
        std::snprintf(buf, sizeof(buf), " [%u", c[2]);
        out += buf;
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t* e = c + 3 + 3 * i;
            if (i < c[2])
                std::snprintf(buf, sizeof(buf), " %u/%x/%d", e[0] & 0xff, e[1], static_cast<int>(e[2]));
            else
                std::snprintf(buf, sizeof(buf), " (%u/%d)", e[0] & 0xff, static_cast<int>(e[2]));
            out += buf;
        }
        out += "]";
        c = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(c[0]));
    } while (c != head && ++guard < 1000);
    return out;
}

// an iterator {node, head, chunk, list} as offsets from the subject's own data
static std::string DumpIter(const uint32_t* it, const uint32_t* s)
{
    char buf[128];
    const uint32_t head = s[1];
    std::snprintf(buf, sizeof(buf), "node %s head %s chunk %s list %s",
                  it[0] ? (it[2] ? std::to_string(static_cast<int>(it[0] - it[2])).c_str() : "?") : "0",
                  it[1] == head ? "h" : (it[1] ? "?" : "0"), it[2] ? (it[2] == head ? "h" : "c") : "0",
                  it[3] == reinterpret_cast<uintptr_t>(s + 1) ? "ok" : "?");
    return buf;
}

static void Compare(const std::string& what, const std::string& a, const std::string& b)
{
    ++checks;
    if (a != b && ++bad <= 20) std::printf("%s:\n  ours %s\n  orig %s\n", what.c_str(), a.c_str(), b.c_str());
}

static void Run(const char* type, int steps)
{
    const Api api[2] = {Api(mods[0], type), Api(mods[1], type)};
    // up to 3 live subjects per side
    alignas(8) uint32_t subj[2][3][4] = {};
    bool live[3] = {};
    for (int step = 0; step < steps; ++step) {
        const unsigned k = Pick(3);
        const unsigned op = live[k] ? Pick(100) : 0;
        void* const obs = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + 0x10 * Pick(12)));
        const int prio = static_cast<int>(Pick(7)) - 3;
        const unsigned other = Pick(3);
        const bool copyFrom = live[other] && other != k && Pick(2) == 0;
        std::string res[2];
        for (int s = 0; s < 2; ++s) {
            const Api& f = api[s];
            uint32_t* me = subj[s][k];
            char buf[64] = "";
            if (!live[k]) {
                if (copyFrom)
                    f.copy(me, nullptr, subj[s][other]);
                else
                    f.ctor(me, nullptr);
            } else if (op < 40) {
                f.add(me, obs, prio);
            } else if (op < 60) {
                f.remove(me, obs);
            } else if (op < 66) {
                std::snprintf(buf, sizeof(buf), "has %d", f.has(me, obs));
            } else if (op < 74) {
                f.lock(me);
            } else if (op < 82) {
                if (static_cast<int>(me[2]) > 0) f.unlock(me);  // never below 0
            } else if (op < 86) {
                std::snprintf(buf, sizeof(buf), "count %lu locks %d", f.count(me), f.locks(me));
            } else if (op < 92) {
                uint32_t it[4];
                const unsigned which = op % 6;
                f.iters[which](me, it);
                std::snprintf(buf, sizeof(buf), "iter %u: ", which);
                res[s] = buf + DumpIter(it, me);
                buf[0] = 0;
            } else if (op < 96) {
                if (live[other]) f.assign(me, nullptr, subj[s][other]);  // other == k: self-assignment
            } else {
                f.dtor(me);
            }
            res[s] += buf;
            res[s] += " | " + Dump(me, f);
        }
        if (!live[k])
            live[k] = true;
        else if (op >= 96)
            live[k] = false;
        Compare(std::string(type) + " step " + std::to_string(step) + " op " + std::to_string(op), res[0], res[1]);
        for (int j = 0; j < 3; ++j)
            if (live[j] && j != static_cast<int>(k))
                Compare("other subject", Dump(subj[0][j], api[0]), Dump(subj[1][j], api[1]));
    }
    for (int j = 0; j < 3; ++j)
        if (live[j])
            for (int s = 0; s < 2; ++s) api[s].dtor(subj[s][j]);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_obs <dir> [steps]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int steps = argc > 2 ? std::atoi(argv[2]) : 200000;
    SetDllDirectoryA(dir.c_str());
    for (const char* dep : {"KernelStandard_7c", "Kernel_7c", "KernelResource_7c"})
        if (!LoadLibraryA((dir + "\\" + dep + ".dll").c_str())) std::printf("cannot load %s\n", dep);
    mods[0] = LoadLibraryA((dir + "\\InputSystem_7c.dll").c_str());
    mods[1] = LoadLibraryA((dir + "\\InputSystem_7c_orig.dll").c_str());
    if (!mods[0] || !mods[1] || mods[0] == mods[1]) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    for (const char* t : {"InputObserver", "InputActionObserver", "InputUIExtensionObserver"}) Run(t, steps);
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
