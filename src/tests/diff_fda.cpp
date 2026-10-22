// Differential test: FastDynamicArray<PluginConfig*,13,Less<...>> exported by our Kernel_7c.dll vs the original.
// Each side runs the same random operation sequence on its own array through its own exports; after every
// step the whole chunk ring (count per chunk + items) and the position of any returned iterator must match.
// usage: diff_fda.exe <our Kernel_7c.dll> <game bin dir holding Kernel_7c_orig.dll>
#include <windows.h>

#include <cstdio>
#include <random>
#include <string>
#include <vector>

struct Chunk {
    Chunk* next;
    Chunk* prev;
    unsigned count;
    void* data[13];
};
struct Array {
    Chunk* head;
};
struct Iter {
    void** node;
    Chunk* head;
    Chunk* chunk;
};

#define FDA "?$FastDynamicArray@PAVPluginConfig@Jet@@$0N@U?$Less@PAVPluginConfig@Jet@@PAV12@@2@@Jet@@"
using VoidT = void(__fastcall*)(Array*, void*, void* const*);
using IterT = Iter*(__fastcall*)(Array*, void*, Iter*, void* const*);
using IterIt = Iter*(__fastcall*)(Array*, void*, Iter*, const Iter*);
using IterItT = Iter*(__fastcall*)(Array*, void*, Iter*, const Iter*, void* const*);
using IterN = Iter*(__fastcall*)(Array*, void*, Iter*, unsigned long);
using IterV = Iter*(__fastcall*)(Array*, void*, Iter*);
using TV = void*(__fastcall*)(Array*, void*);
using VoidN = void(__fastcall*)(Array*, void*, unsigned long);
using CountT = unsigned long(__fastcall*)(const Array*, void*);
using IndexIt = unsigned long(__fastcall*)(const Array*, void*, Iter);
using RefN = void**(__fastcall*)(Array*, void*, unsigned long);
using SwapT = void(__fastcall*)(Array*, void*, Iter*, Iter*);
using ExistsT = bool(__fastcall*)(const Array*, void*, const Iter*);
using SortT = void(__fastcall*)(Array*, void*);

struct Side {
    HMODULE m;
    template <class F>
    F f(const std::string& name)
    {
        auto p = reinterpret_cast<F>(GetProcAddress(m, name.c_str()));
        if (!p) {
            std::printf("missing %s\n", name.c_str());
            std::exit(2);
        }
        return p;
    }
};

static std::vector<std::vector<void*>> Shape(const Array& a)
{
    std::vector<std::vector<void*>> s;
    if (Chunk* c = a.head) {
        do {
            s.emplace_back(c->data, c->data + c->count);
            if (c->next->prev != c) s.push_back({reinterpret_cast<void*>(-1)});  // broken ring marker
            c = c->next;
        } while (c != a.head && s.size() < 100000);
    }
    return s;
}

static long Pos(const Array& a, const Iter& it)
{
    if (!it.node) return -1;
    long n = 0;
    if (Chunk* c = a.head) {
        do {
            for (unsigned i = 0; i < c->count; ++i, ++n)
                if (&c->data[i] == it.node) return (c == it.chunk && it.head == a.head) ? n : -2;
            c = c->next;
        } while (c != a.head);
    }
    return -3;  // points outside the array
}

int main(int argc, char** argv)
{
    if (argc < 3) return 2;
    SetDllDirectoryA(argv[2]);
    Side ours{LoadLibraryA(argv[1])};
    std::string origPath = std::string(argv[2]) + "\\Kernel_7c_orig.dll";
    Side orig{LoadLibraryA(origPath.c_str())};
    if (!ours.m || !orig.m || ours.m == orig.m) return 2;

    const std::string Q = std::string("@") + FDA;
    std::mt19937 rng(99);
    int failures = 0, checks = 0;
    const char* names[] = {"Add", "AddPack", "AddUnique", "Push", "PushUnique", "Insert", "InsertUnique",
                           "Insert(it)", "InsertAfter", "InsertBefore", "Pop", "Remove()", "Remove(it)",
                           "Remove(n)", "RemoveLast", "RemovePack", "Index", "[]", "Swap", "Exists", "Sort",
                           "First/Last/End", "Index(it)"};
    for (int run = 0; run < 400; ++run) {
        Array A{nullptr}, B{nullptr};
        const int steps = 50 + rng() % 400;
        const unsigned spread = 1 + rng() % 60;  // small spread -> many duplicates
        for (int s = 0; s < steps; ++s) {
            Side* sides[2] = {&ours, &orig};
            Array* arrs[2] = {&A, &B};
            const int op = static_cast<int>(rng() % 23);
            void* v = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + 4 * (rng() % spread)));
            const unsigned long cnt = ours.f<CountT>("?Count" + Q + "QBEKXZ")(&A, nullptr);
            const unsigned long n1 = cnt ? rng() % (cnt + 1) : 0, n2 = cnt ? rng() % (cnt + 1) : 0;
            long ret[2] = {-9, -9};
            void* val[2] = {nullptr, nullptr};
            for (int k = 0; k < 2; ++k) {
                Side& S = *sides[k];
                Array* a = arrs[k];
                Iter r{}, it{}, it2{};
                S.f<IterN>("?Index" + Q + "QAE?AVIterator@12@K@Z")(a, nullptr, &it, n1);
                S.f<IterN>("?Index" + Q + "QAE?AVIterator@12@K@Z")(a, nullptr, &it2, n2);
                switch (op) {
                case 0: S.f<VoidT>("?Add" + Q + "QAEXABQAVPluginConfig@2@@Z")(a, nullptr, &v); break;
                case 1: S.f<VoidT>("?AddPack" + Q + "QAEXABQAVPluginConfig@2@@Z")(a, nullptr, &v); break;
                case 2: S.f<VoidT>("?AddUnique" + Q + "QAEXABQAVPluginConfig@2@@Z")(a, nullptr, &v); break;
                case 3: S.f<VoidT>("?Push" + Q + "QAEXABQAVPluginConfig@2@@Z")(a, nullptr, &v); break;
                case 4: S.f<VoidT>("?PushUnique" + Q + "QAEXABQAVPluginConfig@2@@Z")(a, nullptr, &v); break;
                case 5: ret[k] = Pos(*a, *S.f<IterT>("?Insert" + Q + "QAE?AVIterator@12@ABQAVPluginConfig@2@@Z")(a, nullptr, &r, &v)); break;
                case 6: ret[k] = Pos(*a, *S.f<IterT>("?InsertUnique" + Q + "QAE?AVIterator@12@ABQAVPluginConfig@2@@Z")(a, nullptr, &r, &v)); break;
                case 7: ret[k] = Pos(*a, *S.f<IterItT>("?Insert" + Q + "QAE?AVIterator@12@ABV312@ABQAVPluginConfig@2@@Z")(a, nullptr, &r, &it, &v)); break;
                case 8: if (it.node) ret[k] = Pos(*a, *S.f<IterItT>("?InsertAfter" + Q + "QAE?AVIterator@12@ABV312@ABQAVPluginConfig@2@@Z")(a, nullptr, &r, &it, &v)); break;
                case 9: ret[k] = Pos(*a, *S.f<IterItT>("?InsertBefore" + Q + "QAE?AVIterator@12@ABV312@ABQAVPluginConfig@2@@Z")(a, nullptr, &r, &it, &v)); break;
                case 10: if (cnt) val[k] = S.f<TV>("?Pop" + Q + "QAEPAVPluginConfig@2@XZ")(a, nullptr); break;
                case 11: if (cnt) val[k] = S.f<TV>("?Remove" + Q + "QAEPAVPluginConfig@2@XZ")(a, nullptr); break;
                case 12: ret[k] = Pos(*a, *S.f<IterIt>("?Remove" + Q + "QAE?AVIterator@12@ABV312@@Z")(a, nullptr, &r, &it)); break;
                case 13: if (n1 < cnt) ret[k] = Pos(*a, *S.f<IterN>("?Remove" + Q + "QAE?AVIterator@12@K@Z")(a, nullptr, &r, n1)); break;
                case 14: S.f<VoidN>("?RemoveLast" + Q + "QAEXK@Z")(a, nullptr, n1 % 30); break;
                case 15: ret[k] = Pos(*a, *S.f<IterIt>("?RemovePack" + Q + "QAE?AVIterator@12@ABV312@@Z")(a, nullptr, &r, &it)); break;
                case 16: ret[k] = Pos(*a, it); break;
                case 17: if (n1 < cnt) val[k] = *S.f<RefN>(std::string("??A") + FDA + "QAEAAPAVPluginConfig@1@K@Z")(a, nullptr, n1); break;
                case 18: S.f<SwapT>("?Swap" + Q + "QAEXAAVIterator@12@0@Z")(a, nullptr, &it, &it2); break;
                case 19: ret[k] = S.f<ExistsT>("?Exists" + Q + "QBE_NABVIterator@12@@Z")(a, nullptr, &it); break;
                case 20: S.f<SortT>("?Sort" + Q + "QAEXXZ")(a, nullptr); break;
                case 21: {
                    Iter f{}, l{}, e{};
                    S.f<IterV>("?First" + Q + "QAE?AVIterator@12@XZ")(a, nullptr, &f);
                    S.f<IterV>("?Last" + Q + "QAE?AVIterator@12@XZ")(a, nullptr, &l);
                    S.f<IterV>("?End" + Q + "QAE?AVIterator@12@XZ")(a, nullptr, &e);
                    ret[k] = Pos(*a, f) * 1000000 + Pos(*a, l) * 1000 + Pos(*a, e);
                    break;
                }
                case 22: {
                    long r22 = static_cast<long>(S.f<IndexIt>("?Index" + Q + "QBEKVIterator@12@@Z")(a, nullptr, it));
                    // a past-the-end iterator {null, head, head} gives an address-based value in the original too:
                    // compare it with the address part taken out
                    if (!it.node && it.chunk) r22 -= static_cast<long>((0 - reinterpret_cast<intptr_t>(it.chunk) - 0xc) >> 2);
                    ret[k] = r22;
                    break;
                }
                }
            }
            ++checks;
            if (ret[0] != ret[1] || val[0] != val[1] || Shape(A) != Shape(B)) {
                if (failures++ < 15)
                    std::printf("MISMATCH run %d step %d op %s (n1=%lu n2=%lu count=%lu): ret %ld/%ld val %p/%p shape %s\n",
                                run, s, names[op], n1, n2, cnt, ret[0], ret[1], val[0], val[1],
                                Shape(A) == Shape(B) ? "same" : "differs");
                break;  // the two arrays diverged; start a new run
            }
        }
        // clear both
        ours.f<VoidN>("?RemoveLast" + Q + "QAEXK@Z")(&A, nullptr, 0xffffffff);
        orig.f<VoidN>("?RemoveLast" + Q + "QAEXK@Z")(&B, nullptr, 0xffffffff);
    }
    std::printf("%d checks, %d mismatches\n", checks, failures);
    return failures ? 1 : 0;
}
