// Differential test of KernelStandard unit U14b, PString.cpp: one random operation script (made once, before either
// side runs) is played through KernelStandard_7c_orig.dll's PStringCache / PString exports and through ours, each on
// its own live cache (PString::cache; a second cache cannot be made, the constructor resets the module's one lock).
// After every step the touched nodes are dumped: which of the test's slots hold the node (so "the same node"
// compares), length, capacity, references, id and the text, and the node's place among the test's nodes in its chain. The cache's
// counters are compared as changes from the start (usedNodes, liveRefs, hits, nodes); probes and absolute counts
// depend on the nodes the module made for itself and are left out.
// Left out as in the original they corrupt memory: an ANSI Read cut short for a node from the string heap (length
// below 0x87: it is freed with Jet::Alloc::Delete), Find on a null node.
//
// usage: diff_ks_pstring.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [steps]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static HMODULE ours, orig, cur;

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
template <class F>
static F G(const char* name) { return Get<F>(cur, name); }

static std::vector<long long> logv;
static std::vector<int> lines;
static void Log(long long v, int line)
{
    logv.push_back(v);
    lines.push_back(line);
}
#define L(v) Log(static_cast<long long>(v), __LINE__)

static long long Hash(const void* p, size_t n)
{
    unsigned long long h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 1099511628211ull;
    return static_cast<long long>(h);
}

// ---- inputs

static std::vector<std::string> texts;
static std::vector<std::wstring> wtexts;
static void MakeCorpus()
{
    texts = {"", "a", "Z", "Hello World", "hello world", "HELLO world", "  tok1, tok2 ;tok3  ", ",;, ", "abcabcABC",
             "123", "-42", "path/to/file.txt", "C:\\dir\\x.cfg", "\xe9t\xc9 \xc0la", "\xff\xfe\x80", "mixed Case\tTab",
             "a,b,,c;d", "xyzzy", "tail ", "u14b_probe", "kind", "trainz-build", "0123456789abcdefXYZ",
             "0123456789abcdefxyz", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaz"};
    for (int n : {0x70, 0x77, 0x78, 0x79, 0x87, 0x88, 300, 1100, 0x2100}) {
        std::string s;
        for (int i = 0; i < n; ++i) s += static_cast<char>('a' + (i * 7 + n) % 26);
        texts.push_back(s);
    }
    for (const auto& t : texts) {
        std::wstring w;
        for (unsigned char c : t) w += static_cast<wchar_t>(c);
        wtexts.push_back(w);
    }
    wtexts.push_back(L"\x041f\x0440\x0438\x0432\x0435\x0442");  // not in the ANSI code page
    wtexts.push_back(L"\x20ac uro \x2013 dash");
    wtexts.push_back(L"\xdf\xe4\xf6\xfc\xc4\xd6\xdc");
}

static unsigned rngState;
static unsigned Rnd(unsigned n)
{
    rngState = rngState * 1103515245u + 12345u;
    return n ? (rngState >> 8) % n : 0;
}

struct Step {
    int op, obj, o2, t, a, b, c;
};
static std::vector<Step> MakeScript(int ops, int steps, unsigned seed)
{
    rngState = seed;
    std::vector<Step> v;
    for (int i = 0; i < steps; ++i)
        v.push_back(Step{static_cast<int>(Rnd(ops)), static_cast<int>(Rnd(8)), static_cast<int>(Rnd(8)),
                         static_cast<int>(Rnd(static_cast<unsigned>(texts.size()))), static_cast<int>(Rnd(1000)),
                         static_cast<int>(Rnd(1000)), static_cast<int>(Rnd(1000))});
    return v;
}

struct Writer {
    std::vector<unsigned char> bytes;
    unsigned long limit = 0xffffffff;
    virtual ~Writer() {}
    virtual unsigned long Write(const void* p, unsigned long n)
    {
        unsigned long k = n;
        if (bytes.size() + k > limit) k = limit > bytes.size() ? limit - static_cast<unsigned long>(bytes.size()) : 0;
        bytes.insert(bytes.end(), static_cast<const unsigned char*>(p), static_cast<const unsigned char*>(p) + k);
        return k;
    }
};
struct Reader {
    std::vector<unsigned char> bytes;
    size_t pos = 0;
    virtual ~Reader() {}
    virtual unsigned long Read(void* p, unsigned long n)
    {
        unsigned long k = n;
        if (pos + k > bytes.size()) k = static_cast<unsigned long>(bytes.size() - pos);
        std::memcpy(p, bytes.data() + pos, k);
        pos += k;
        return k;
    }
};

// ---- the cache

struct Node {
    Node* next;
    Node* prev;
    unsigned length, capacity;
    int refs;
    unsigned id;
    char text[1];
};
struct Bucket {
    Node* head;
    Node* tail;
    int count;
};
struct Cache {
    int usedNodes, liveRefs, hits, nodes, probes, u14, u18, bucketCount, gcBucket;
    Bucket* buckets;
    int walkBucket;
    Node* walkNode;
};

static Cache* TheCache() { return *G<Cache**>("?cache@PString@Jet@@0PAVPStringCache@2@A"); }

// The test's slots. A node's identity is which slots hold it (addresses are not compared: each module's heap reuses
// freed nodes in its own order).
static Node** slots;
static int Holders(Node* n)
{
    if (!n) return -1;
    int m = 0;
    for (int i = 0; i < 8; ++i)
        if (slots[i] == n) m |= 1 << i;
    return m;
}
static bool IsSlot(Node* n)
{
    for (int i = 0; i < 8; ++i)
        if (slots[i] == n) return true;
    return false;
}

static Cache start;
static void DumpCache(int line)
{
    const Cache* c = TheCache();
    Log(c->usedNodes - start.usedNodes, line);
    Log(c->liveRefs - start.liveRefs, line);
    Log(c->hits - start.hits, line);
    Log(c->nodes - start.nodes, line);
}

// the node, and where it sits among the test's nodes of its chain (-1: not in a chain)
static void Dump(Node* n, int line)
{
    Log(Holders(n), line);
    if (!n) return;
    Log(n->length, line);
    Log(n->capacity, line);
    Log(n->refs, line);
    Log(n->id, line);
    Log(Hash(n->text, n->length + 1), line);
    const Cache* c = TheCache();
    unsigned h = 0;
    {
        unsigned k = n->length > 0x10 ? 0x10 : n->length;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(n->text) + n->length - k; k; --k) h = h * 8 ^ *p++;
    }
    const Bucket& b = c->buckets[h % static_cast<unsigned>(c->bucketCount)];
    int place = 0, found = -1;
    for (Node* m = b.head; m; m = m->next) {
        if (m == n) {
            found = place;
            break;
        }
        if (IsSlot(m)) ++place;
    }
    Log(found, line);
}

using Create1 = Node*(__cdecl*)(Cache*, const char*);
using Create2 = Node*(__cdecl*)(Cache*, const char*, unsigned);
using CreateCh = Node*(__cdecl*)(Cache*, unsigned, unsigned);
using CreateA = Node*(__cdecl*)(Cache*, const char*, int);
using CreateU = Node*(__cdecl*)(Cache*, const wchar_t*, int);
using InsCh = Node*(__cdecl*)(Cache*, Node*, unsigned, unsigned, unsigned);
using InsT = Node*(__cdecl*)(Cache*, Node*, const char*, unsigned);
using DelF = Node*(__cdecl*)(Cache*, Node*, unsigned, unsigned);
using SetI = Node*(__cdecl*)(Cache*, Node*, int, unsigned);
using SetF = Node*(__cdecl*)(Cache*, Node*, const char*, va_list);
using FindCh = int(__cdecl*)(const Cache*, Node*, unsigned, unsigned);
using FindT = int(__cdecl*)(const Cache*, Node*, const char*, unsigned);
using NodeOp = Node*(__cdecl*)(Cache*, Node*);
using StripF = Node*(__cdecl*)(Cache*, Node*, const char*);
using Destroy = void(__cdecl*)(Cache*, Node*);
using GetA = int(__cdecl*)(const Cache*, Node*, char*, unsigned);
using GetW = int(__cdecl*)(const Cache*, Node*, wchar_t*, unsigned);
using HashF = unsigned(__cdecl*)(const Cache*, const char*, unsigned);
using Unique = Node*(__cdecl*)(Cache*, Node*, unsigned);
using CacheOp = void(__cdecl*)(Cache*);
using NextF = Node*(__cdecl*)(Cache*);
using PsFmt = void(__cdecl*)(Node**, const char*, ...);
using PsAddf = int(__cdecl*)(Node**, const char*, ...);
using PsCmp = int(__cdecl*)(Node* const*, Node* const*);
using PsCmpT = int(__cdecl*)(Node* const*, const char*);
using PsBool = bool(__cdecl*)(Node* const*, Node* const*);
using PsSort = int(__cdecl*)(const void*, const void*);
using PsRead = bool(__cdecl*)(Node**, Reader*);
using PsWrite = bool(__cdecl*)(Node* const*, Writer*);

static void Release(Node*& n)
{
    if (n) G<Destroy>("?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z")(TheCache(), n);
    n = nullptr;
}

static const bool traceSteps = getenv("KS_STEP") != nullptr;

static void Scenario(const std::vector<Step>& script)
{
    Cache* c = TheCache();
    start = *c;
    Node* o[8] = {};
    slots = o;
    for (const Step& st : script) {
        Node*& x = o[st.obj];
        Node*& y = o[st.o2];
        const std::string& t = texts[st.t];
        const std::wstring& w = wtexts[(st.t + st.c) % wtexts.size()];
        if (traceSteps) std::printf("op %d obj %d t %d a %d b %d c %d\n", st.op, st.obj, st.t, st.a, st.b, st.c);
        L(st.op);
        switch (st.op) {
        case 0:
            Release(x);
            x = G<Create1>("?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z")(c, st.a % 13 ? t.c_str() : nullptr);
            break;
        case 1: {
            Release(x);
            const unsigned n = st.a % 7 == 0 ? 0 : st.a % (t.size() + 1);
            x = G<Create2>("?Create@PStringCache@Jet@@QAAPAUNode@12@PBEI@Z")(c, st.b % 11 || n ? t.c_str() : nullptr, n);
            break;
        }
        case 2:
            Release(x);
            x = G<CreateCh>("?Create@PStringCache@Jet@@QAAPAUNode@12@II@Z")(c, 0x20 + st.a % 0x60, st.b % 0x90);
            break;
        case 3:
            Release(x);
            x = G<CreateA>("?CreateANSI@PStringCache@Jet@@QAAPAUNode@12@PBEH@Z")(
                c, t.c_str(), st.a % 3 ? static_cast<int>(st.b % (t.size() + 3)) - 1 : -1);
            break;
        case 4:
            Release(x);
            x = G<CreateU>("?CreateUnicode@PStringCache@Jet@@QAAPAUNode@12@PB_WH@Z")(
                c, st.a % 9 ? w.c_str() : nullptr, st.a % 3 ? static_cast<int>(st.b % (w.size() + 1)) : -1);
            break;
        case 5: {
            const unsigned len = x ? x->length : 0;
            x = G<InsCh>("?Ins@PStringCache@Jet@@QAAPAUNode@12@PAU312@III@Z")(c, x, 0x30 + st.a % 0x40, st.b % (len + 3),
                                                                             st.c % 12);
            break;
        }
        case 6: {
            const unsigned len = x ? x->length : 0;
            x = G<InsT>("?Ins@PStringCache@Jet@@QAAPAUNode@12@PAU312@PBEI@Z")(c, x, st.c % 17 ? t.c_str() : nullptr,
                                                                             st.b % (len + 3));
            break;
        }
        case 7: {
            const unsigned len = x ? x->length : 0;
            x = G<DelF>("?Del@PStringCache@Jet@@QAAPAUNode@12@PAU312@II@Z")(c, x, st.a % (len + 3), st.b % 20);
            break;
        }
        case 8:
            x = G<SetI>("?Set@PStringCache@Jet@@QAAPAUNode@12@PAU312@HI@Z")(c, x, st.a * 37 - 9000,
                                                                          st.b % 3 == 0 ? 16 : st.b % 3 == 1 ? 10 : 2 + st.c % 35);
            break;
        case 9:
            if (st.a % 4 == 0)
                G<PsFmt>("?Setf@PString@Jet@@QAAXPBDZZ")(&x, "%s-%d-%5.2f|", t.c_str(), st.b, st.c / 7.0);
            else if (st.a % 4 == 1)
                G<PsFmt>("?Setf@PString@Jet@@QAAXPBDZZ")(&x, "");
            else
                L(G<PsAddf>("?Addf@PString@Jet@@QAAHPBDZZ")(&x, "<%s:%x>", t.c_str(), st.c));
            break;
        case 10:
            if (x) {
                L(G<FindCh>("?Find@PStringCache@Jet@@QBAHPAUNode@12@II@Z")(c, x, t.empty() ? 'a' : t[st.a % t.size()],
                                                                          st.b % (x->length + 1)));
                L(G<FindT>("?Find@PStringCache@Jet@@QBAHPAUNode@12@PBEI@Z")(c, x, t.substr(0, st.c % 4).c_str(),
                                                                           st.b % (x->length + 1)));
            }
            break;
        case 11:
            x = G<StripF>("?Strip@PStringCache@Jet@@QAAPAUNode@12@PAU312@PBE@Z")(c, x, st.a & 1 ? " ,;" : "aeiou");
            break;
        case 12:
            x = G<NodeOp>(st.a & 1 ? "?ToLower@PStringCache@Jet@@QAAPAUNode@12@PAU312@@Z"
                                   : "?ToUpper@PStringCache@Jet@@QAAPAUNode@12@PAU312@@Z")(c, x);
            break;
        case 13: {
            char a[0x300];
            std::memset(a, 0x5a, sizeof a);
            L(G<GetA>("?GetANSI@PStringCache@Jet@@QBAHPAUNode@12@PADI@Z")(c, x, a, st.a % 0x300));
            L(Hash(a, sizeof a));
            wchar_t b[0x300];
            std::memset(b, 0x5a, sizeof b);
            L(G<GetW>("?GetUnicode@PStringCache@Jet@@QBAHPAUNode@12@PA_WI@Z")(c, x, b, st.b % 0x300));
            L(Hash(b, sizeof b));
            break;
        }
        case 14:
            L(G<HashF>("?GetHash@PStringCache@Jet@@QBAIPBEI@Z")(c, t.c_str(), static_cast<unsigned>(t.size())));
            break;
        case 15:  // a copy (another reference), then a private edit of it through GetUnique / Insert
            if (x && &x != &y) {
                Release(y);
                G<CacheOp>("?Lock@PStringCache@Jet@@QAAXXZ")(c);  // the copy every module inlines
                ++x->refs;
                ++c->liveRefs;
                G<CacheOp>("?Unlock@PStringCache@Jet@@QAAXXZ")(c);
                y = x;
            }
            break;
        case 16:
            Release(x);
            break;
        case 17:
            G<CacheOp>("?CollectGarbage@PStringCache@Jet@@QAAXXZ")(c);
            L(c->gcBucket);
            break;
        case 18: {  // the id walk: the test's nodes in walk order
            G<CacheOp>("?ClearIDs@PStringCache@Jet@@QAAXXZ")(c);
            L(c->walkBucket);
            L(c->walkNode == nullptr);
            break;
        }
        case 19: {
            Node* const* px = &x;
            Node* const* py = &y;
            L(G<PsCmp>("?Compare@PString@Jet@@QBAHABV12@@Z")(px, py));
            L(G<PsCmpT>("?Compare@PString@Jet@@QBAHPBD@Z")(px, st.a % 5 ? t.c_str() : nullptr));
            L(G<PsCmp>("?CompareIgnoreCase@PString@Jet@@QBAHABV12@@Z")(px, py) < 0);
            L(G<PsCmp>("?CompareIgnoreCase@PString@Jet@@QBAHABV12@@Z")(px, py) > 0);
            const int ci = G<PsCmpT>("?CompareIgnoreCase@PString@Jet@@QBAHPBD@Z")(px, st.a % 5 ? t.c_str() : nullptr);
            L(ci < 0);
            L(ci > 0);
            L(G<PsBool>("?EqualsIgnoreCase@PString@Jet@@QBA_NABV12@@Z")(px, py));
            L(G<PsBool>("??MPString@Jet@@QBA_NABV01@@Z")(px, py));
            L(G<PsBool>("??OPString@Jet@@QBA_NABV01@@Z")(px, py));
            L(G<PsSort>("?SortPtr@PString@Jet@@SAHPBX0@Z")(px, py) == 0);
            break;
        }
        case 20: {
            Writer wr;
            if (st.a % 7 == 0) wr.limit = st.b % 12;
            L(G<PsWrite>("?Write@PString@Jet@@QBA_NAAVBinWriter@2@@Z")(&x, &wr));
            L(Hash(wr.bytes.data(), wr.bytes.size()));
            Reader rd;
            rd.bytes = wr.bytes;
            // cut short only where the original frees with Alloc: a node above 0xa0 bytes, (length + 0x29) & ~0xf
            if (st.b % 5 == 0 && rd.bytes.size() > 4 && x && x->length >= 0x87) rd.bytes.resize(4 + x->length / 2);
            const unsigned len = x ? x->length : 0;
            const bool cut = rd.bytes.size() >= 4 && rd.bytes.size() < 4 + len;
            if (&x != &y && (!cut || len >= 0x87)) {
                L(G<PsRead>("?Read@PString@Jet@@QAA_NAAVBinReader@2@@Z")(&y, &rd));
                Dump(y, __LINE__);
            }
            break;
        }
        case 21: {  // UTF-16 records
            Reader rd;
            const unsigned n = static_cast<unsigned>(w.size() * 2);
            const unsigned header = n | 0x40000000;
            rd.bytes.resize(4 + n);
            std::memcpy(rd.bytes.data(), &header, 4);
            if (n) std::memcpy(rd.bytes.data() + 4, w.data(), n);
            if (st.b % 5 == 0 && rd.bytes.size() > 4) rd.bytes.resize(4 + n / 2);
            L(G<PsRead>("?Read@PString@Jet@@QAA_NAAVBinReader@2@@Z")(&x, &rd));
            break;
        }
        case 22: {  // GetUnique + Insert under the lock, as the members do
            if (!x) break;
            G<CacheOp>("?Lock@PStringCache@Jet@@QAAXXZ")(c);
            Node* u = G<Unique>("?GetUnique@PStringCache@Jet@@QAAPAUNode@12@PAU312@I@Z")(c, x, x->length + st.a % 40);
            Dump(u, __LINE__);
            if (u->length) u->text[st.b % u->length] = static_cast<char>('A' + st.c % 26);
            x = G<NodeOp>("?Insert@PStringCache@Jet@@QAAPAUNode@12@PAU312@@Z")(c, u);
            G<CacheOp>("?Unlock@PStringCache@Jet@@QAAXXZ")(c);
            break;
        }
        }
        Dump(x, __LINE__);
        DumpCache(__LINE__);
    }
    for (auto& n : o) Release(n);
    DumpCache(__LINE__);
}

static size_t Compare(const char* name, const std::vector<Step>& script)
{
    logv.clear();
    lines.clear();
    std::printf("%s: orig\n", name);
    cur = orig;
    Scenario(script);
    const std::vector<long long> a = logv;
    const std::vector<int> la = lines;
    logv.clear();
    lines.clear();
    std::printf("%s: ours\n", name);
    cur = ours;
    Scenario(script);
    size_t bad = 0;
    for (size_t i = 0; i < a.size() || i < logv.size(); ++i) {
        const long long x = i < a.size() ? a[i] : -999, y = i < logv.size() ? logv[i] : -999;
        if (x != y && ++bad <= 10) {
            std::printf("  %s step %zu (line %d): orig %lld ours %lld\n", name, i, i < la.size() ? la[i] : 0, x, y);
            if (getenv("KS_TRACE"))
                for (size_t k = i > 30 ? i - 30 : 0; k <= i && k < la.size() && k < logv.size(); ++k)
                    std::printf("    [%zu] line %d: %lld / %lld\n", k, la[k], a[k], logv[k]);
        }
    }
    std::printf("%s: %zu results each, %zu mismatches\n", name, a.size(), bad);
    return bad;
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    const int steps = argc > 2 ? std::atoi(argv[2]) : 30000;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e) -> LONG {
        std::printf("crash %08lx at %p after line %d\n", e->ExceptionRecord->ExceptionCode,
                    e->ExceptionRecord->ExceptionAddress, lines.empty() ? 0 : lines.back());
        ExitProcess(3);
    });
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) return 2;
    using SetLoc = char*(__cdecl*)(int, const char*);
    if (HMODULE crt = GetModuleHandleA("msvcr80.dll")) reinterpret_cast<SetLoc>(GetProcAddress(crt, "setlocale"))(0, ".ACP");
    MakeCorpus();
    size_t bad = 0;
    bad += Compare("PStringCache", MakeScript(23, steps, 7));
    bad += Compare("PStringCache (long)", MakeScript(23, steps * 3, 11));
    std::printf("total mismatches: %zu\n", bad);
    return bad ? 1 : 0;
}
