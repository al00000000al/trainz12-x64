// Differential test of KernelStandard unit U14a (UnicodeString.cpp, MultiByteString.cpp, AnsiString.cpp): one
// random operation script per class (made once, before either side runs) is played through
// KernelStandard_7c_orig.dll's exports and through ours; after every step each touched object is dumped (counts,
// capacity, references, the text) and every result is logged. Both modules share the original's Alloc, string
// heap and empty AnsiString block, so AnsiStrings are also handed across: made by one module, changed and freed by
// the other. Inputs that crash the original (a null text into the MBCS/wide converters) are left out.
//
// usage: diff_ks_string.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [steps]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static HMODULE ours, orig;

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
static void LogBits(double d, int line)
{
    long long b;
    std::memcpy(&b, &d, 8);
    Log(b, line);
}

// ---- inputs

static std::vector<std::string> texts;
static std::vector<std::wstring> wtexts;
static void MakeCorpus()
{
    texts = {"", "a", "Z", "Hello World", "hello world", "HELLO world", "  tok1, tok2 ;tok3  ", ",;, ", "abcabcABC",
             "123", "-42", "0x1f", "  7.5", "3.25e2", "4294967295", "1e400", "nan", "\xe9t\xc9 \xc0la", "\xff\xfe\x80",
             "mixed Case\tTab", "a,b,,c;d", "xyzzy", "tail "};
    for (int n : {0x93, 0x94, 0x95, 0xa0, 0xa1, 0xa8, 300, 1100}) {
        std::string s;
        for (int i = 0; i < n; ++i) s += static_cast<char>('a' + (i * 7) % 26);
        texts.push_back(s);
    }
    for (const auto& t : texts) {
        std::wstring w;
        for (unsigned char c : t) w += static_cast<wchar_t>(c);
        wtexts.push_back(w);
    }
    wtexts.push_back(L"\x041f\x0440\x0438\x0432\x0435\x0442");  // not in the ANSI code page: conversions fail
    wtexts.push_back(L"\x20ac uro \x2013 dash");                  // in CP1252 (multi-byte elsewhere)
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
    for (int i = 0; i < steps; ++i) {
        Step s;
        s.op = Rnd(ops);
        s.obj = Rnd(6);
        s.o2 = Rnd(6);
        s.t = Rnd(static_cast<unsigned>(texts.size()));
        s.a = Rnd(1000);
        s.b = Rnd(1000);
        s.c = Rnd(1000);
        v.push_back(s);
    }
    return v;
}

// ---- reader / writer stand-ins (the BinReader / BinWriter slots: deleting destructor, then the transfer)

struct Writer {
    std::vector<unsigned char> bytes;
    unsigned long limit = 0xffffffff;  // a short write after this many bytes
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
static Reader Crafted(unsigned header, const void* body, size_t n)
{
    Reader r;
    r.bytes.resize(4 + n);
    std::memcpy(r.bytes.data(), &header, 4);
    if (n) std::memcpy(r.bytes.data() + 4, body, n);
    return r;
}

using C0 = void*(__fastcall*)(void*, void*);
using CP = void*(__fastcall*)(void*, void*, const void*);
using CPI = void*(__fastcall*)(void*, void*, const void*, unsigned);
using CPII = void*(__fastcall*)(void*, void*, const void*, unsigned, int);
using CII = void*(__fastcall*)(void*, void*, unsigned, unsigned);
using Dt = void(__fastcall*)(void*, void*);
using MP = void*(__cdecl*)(void*, const void*);
using MPI = int(__cdecl*)(void*, const void*, int);
using MI = void(__cdecl*)(void*, unsigned);
using MII = void(__cdecl*)(void*, unsigned, unsigned);
using MIII = void(__cdecl*)(void*, unsigned, unsigned, unsigned);
using M0 = void(__cdecl*)(void*);
using QP = int(__cdecl*)(const void*, const void*);
using QPI = int(__cdecl*)(const void*, const void*, unsigned);
using QII = int(__cdecl*)(const void*, unsigned, unsigned);
using QI = int(__cdecl*)(const void*, int);
using Q0 = unsigned(__cdecl*)(const void*);
using QD = double(__cdecl*)(const void*);
using BP = bool(__cdecl*)(const void*, const void*);
using Fmt = int(__cdecl*)(void*, const void*, ...);
using Tok = void*(__cdecl*)(void*, void*, const void*);
using SII = int(__cdecl*)(const void*, unsigned, unsigned);
using SIPI = int(__cdecl*)(const void*, unsigned, const void*, unsigned);
using RW = bool(__cdecl*)(void*, void*);

struct Side {
    HMODULE m;
    const char* tag;
    template <class F>
    F f(const char* name) const { return Get<F>(m, name); }
};

static void* PStringNode(const char* text)  // a node of the live cache (the original's), one reference
{
    void* cache = *Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    return Get<void*(__cdecl*)(void*, const unsigned char*)>(orig, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z")(
        cache, reinterpret_cast<const unsigned char*>(text));
}
static void PStringRelease(void* node)
{
    void* cache = *Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    if (node) Get<void(__cdecl*)(void*, void*)>(orig, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z")(cache, node);
}

// an AnsiString made by the original from text (the input of the conversions)
struct OrigAnsi {
    void* data;
    OrigAnsi(const char* s) { Get<CP>(orig, "??0AnsiString@Jet@@QAE@PBD@Z")(this, nullptr, s); }
    ~OrigAnsi() { Get<Dt>(orig, "??1AnsiString@Jet@@QAE@XZ")(this, nullptr); }
};

// ---- UnicodeString {text, capacity, length}

struct U {
    unsigned short* text;
    unsigned cap, len;
};
static void Dump(const U& u, int line)
{
    Log(u.text != nullptr, line);
    Log(u.cap, line);
    Log(u.len, line);
    if (u.text) Log(Hash(u.text, (u.len + 1) * 2), line);
}

static void UnicodeScenario(const Side& s, const std::vector<Step>& script)
{
    U o[6];
    const auto dtor = s.f<Dt>("??1UnicodeString@Jet@@QAE@XZ");
    for (auto& x : o) s.f<C0>("??0UnicodeString@Jet@@QAE@XZ")(&x, nullptr);
    const wchar_t* delim = L" ,;";
    for (const Step& st : script) {
        U& x = o[st.obj];
        U& y = o[st.o2];
        const std::string& t = texts[st.t];
        const std::wstring& w = wtexts[(st.t + st.c) % wtexts.size()];
        const unsigned len = x.len;
        L(st.op);
        switch (st.op) {
        case 0: dtor(&x, nullptr); s.f<C0>("??0UnicodeString@Jet@@QAE@XZ")(&x, nullptr); break;
        case 1:
            if (&x == &y) break;
            dtor(&x, nullptr);
            s.f<CP>("??0UnicodeString@Jet@@QAE@ABV01@@Z")(&x, nullptr, &y);
            break;
        case 2: dtor(&x, nullptr); s.f<CP>("??0UnicodeString@Jet@@QAE@PBD@Z")(&x, nullptr, t.c_str()); break;
        case 3: dtor(&x, nullptr); s.f<CPI>("??0UnicodeString@Jet@@QAE@PBDI@Z")(&x, nullptr, t.c_str(), st.a % (t.size() + 1)); break;
        case 4: dtor(&x, nullptr); s.f<CP>("??0UnicodeString@Jet@@QAE@PB_W@Z")(&x, nullptr, w.c_str()); break;
        case 5: dtor(&x, nullptr); s.f<CPI>("??0UnicodeString@Jet@@QAE@PB_WI@Z")(&x, nullptr, w.c_str(), st.a % (w.size() + 1)); break;
        case 6: dtor(&x, nullptr); s.f<CII>("??0UnicodeString@Jet@@QAE@II@Z")(&x, nullptr, 0x41 + st.a % 300, st.b % 40); break;
        case 7: L(s.f<MP>("??4UnicodeString@Jet@@QAAABV01@ABV01@@Z")(&x, &y) == &x); break;
        case 8: L(s.f<MP>("??4UnicodeString@Jet@@QAAABV01@PB_W@Z")(&x, st.a & 1 ? w.c_str() : nullptr) == &x); break;
        case 9: L(s.f<MPI>("?SetANSI@UnicodeString@Jet@@QAA_NPBDH@Z")(&x, t.c_str(), st.a & 1 ? -1 : int(st.a % (t.size() + 1))) & 0xff); break;
        case 10: L(s.f<MPI>("?SetUnicode@UnicodeString@Jet@@QAA_NPB_WH@Z")(&x, w.c_str(), st.a & 1 ? -1 : int(st.a % (w.size() + 1))) & 0xff); break;
        case 11: s.f<MII>("?Set@UnicodeString@Jet@@QAAXHI@Z")(&x, st.a * 7919 - 400000, 2 + st.b % 35); break;
        case 12: s.f<Fmt>("?Setf@UnicodeString@Jet@@QAAXPB_WZZ")(&x, L"%s|%d|%x", w.c_str(), st.a - 500, st.b); break;
        case 13: L(s.f<Fmt>("?Addf@UnicodeString@Jet@@QAAHPB_WZZ")(&x, st.a & 1 ? L"%s." : L"", w.c_str())); break;
        case 14: s.f<MPI>("?Setn@UnicodeString@Jet@@QAAXPB_WI@Z")(&x, w.c_str(), st.a % 50); break;
        case 15: s.f<MPI>("?Ins@UnicodeString@Jet@@QAAXPB_WI@Z")(&x, w.c_str(), st.a % (len + 2)); break;
        case 16: s.f<MIII>("?Ins@UnicodeString@Jet@@QAAXIII@Z")(&x, 0x30 + st.a % 400, st.b % (len + 2), st.c % 9); break;
        case 17: s.f<MII>("?Del@UnicodeString@Jet@@QAAXII@Z")(&x, st.a % (len + 2), st.b % 12); break;
        case 18: s.f<MI>("?Left@UnicodeString@Jet@@QAAXI@Z")(&x, st.a % (len + 2)); break;
        case 19: s.f<MI>("?Right@UnicodeString@Jet@@QAAXI@Z")(&x, st.a % (len + 2)); break;
        case 20: s.f<MI>("?SetLength@UnicodeString@Jet@@QAAXI@Z")(&x, st.a % (len + 40)); break;
        case 21: if (x.text) s.f<MP>("?Strip@UnicodeString@Jet@@QAAXPB_W@Z")(&x, delim); break;
        case 22: s.f<M0>(st.a & 1 ? "?ToLower@UnicodeString@Jet@@QAAXXZ" : "?ToUpper@UnicodeString@Jet@@QAAXXZ")(&x); break;
        case 23: {
            U r;
            L(s.f<Tok>("?Token@UnicodeString@Jet@@QAA?AV12@PB_W@Z")(&x, &r, delim) == &r);
            Dump(r, __LINE__);
            dtor(&r, nullptr);
            break;
        }
        case 24: {
            const char* ops[] = {"??8UnicodeString@Jet@@QBA_NABV01@@Z", "??9UnicodeString@Jet@@QBA_NABV01@@Z",
                                 "??MUnicodeString@Jet@@QBA_NABV01@@Z", "??NUnicodeString@Jet@@QBA_NABV01@@Z",
                                 "??OUnicodeString@Jet@@QBA_NABV01@@Z", "??PUnicodeString@Jet@@QBA_NABV01@@Z",
                                 "?EqualsIgnoreCase@UnicodeString@Jet@@QBA_NABV12@@Z"};
            for (const char* n : ops) L(s.f<BP>(n)(&x, &y));
            L(s.f<QP>("?Compare@UnicodeString@Jet@@QBAHABV12@@Z")(&x, &y));
            L(s.f<QP>("?CompareIgnoreCase@UnicodeString@Jet@@QBAHABV12@@Z")(&x, &y));
            const wchar_t* p = st.a % 5 ? w.c_str() : nullptr;
            const char* pops[] = {"??8UnicodeString@Jet@@QBA_NPB_W@Z", "??9UnicodeString@Jet@@QBA_NPB_W@Z",
                                  "??MUnicodeString@Jet@@QBA_NPB_W@Z", "??NUnicodeString@Jet@@QBA_NPB_W@Z",
                                  "??OUnicodeString@Jet@@QBA_NPB_W@Z", "??PUnicodeString@Jet@@QBA_NPB_W@Z",
                                  "?EqualsIgnoreCase@UnicodeString@Jet@@QBA_NPB_W@Z"};
            for (const char* n : pops) L(s.f<BP>(n)(&x, p));
            L(s.f<QP>("?Compare@UnicodeString@Jet@@QBAHPB_W@Z")(&x, p));
            L(s.f<QP>("?CompareIgnoreCase@UnicodeString@Jet@@QBAHPB_W@Z")(&x, p));
            break;
        }
        case 25: {
            const unsigned start = st.a % (len + 1);
            const unsigned ch = w.empty() ? 'a' : w[st.b % w.size()];
            L(s.f<QII>("?Find@UnicodeString@Jet@@QBAHII@Z")(&x, ch, start));
            L(s.f<QII>("?FindIgnoreCase@UnicodeString@Jet@@QBAHII@Z")(&x, ch ^ (st.c & 0x20), start));
            const std::wstring sub = w.substr(st.b % (w.size() + 1), st.c % 4);
            L(s.f<QPI>("?Find@UnicodeString@Jet@@QBAHPB_WI@Z")(&x, sub.c_str(), start));
            if (x.text) L(s.f<QPI>("?FindIgnoreCase@UnicodeString@Jet@@QBAHPB_WI@Z")(&x, sub.c_str(), start));
            break;
        }
        case 26: {
            L(s.f<QI>("?GetInt@UnicodeString@Jet@@QBAHH@Z")(&x, st.a % 3 ? 10 : (st.a % 2 ? 16 : 0)));
            L(s.f<QI>("?GetUInt@UnicodeString@Jet@@QBAIH@Z")(&x, 10));
            LogBits(s.f<QD>("?GetFloat@UnicodeString@Jet@@QBAMXZ")(&x), __LINE__);
            LogBits(s.f<QD>("?GetDouble@UnicodeString@Jet@@QBANXZ")(&x), __LINE__);
            L(s.f<Q0>("?Size@UnicodeString@Jet@@QBAIXZ")(&x));
            L(s.f<Q0>("?Length@UnicodeString@Jet@@QBAIXZ")(&x));
            L(s.f<Q0>("?AllocSize@UnicodeString@Jet@@QBAIXZ")(&x));
            if (x.text && len) {
                L(s.f<QI>("??AUnicodeString@Jet@@QBAIH@Z")(&x, st.b % len));
                L(*reinterpret_cast<unsigned short*>(s.f<MP>("??AUnicodeString@Jet@@QAAAA_WI@Z")(&x, reinterpret_cast<void*>(st.b % len))));
            }
            break;
        }
        case 27: {
            char a[0x600];
            std::memset(a, 0x5a, sizeof a);
            const unsigned size = 1 + st.a % 0x400;
            if (x.text) {
                L(s.f<QPI>("?GetANSI@UnicodeString@Jet@@QBAHPADI@Z")(&x, a, size));
                L(Hash(a, sizeof a));
            }
            unsigned short b[0x600];
            std::memset(b, 0x5a, sizeof b);
            L(s.f<QPI>("?GetUnicode@UnicodeString@Jet@@QBAHPA_WI@Z")(&x, b, st.b % 0x800));
            L(Hash(b, sizeof b));
            break;
        }
        case 28: {
            Writer wr;
            if (st.a % 7 == 0) wr.limit = st.b % 12;
            L(s.f<RW>("?Write@UnicodeString@Jet@@QBA_NAAVBinWriter@2@@Z")(&x, &wr));
            L(Hash(wr.bytes.data(), wr.bytes.size()));
            Reader rd;
            rd.bytes = wr.bytes;
            L(s.f<RW>("?Read@UnicodeString@Jet@@QAA_NAAVBinReader@2@@Z")(&y, &rd));
            Dump(y, __LINE__);
            break;
        }
        case 29: {  // crafted input: ANSI counts, UTF-16 byte counts, short bodies
            const unsigned n = static_cast<unsigned>(st.a % 2 ? t.size() : w.size() * 2);
            const unsigned header = st.a % 2 ? n : (n | 0x40000000);
            Reader rd = st.a % 2 ? Crafted(header, t.data(), t.size()) : Crafted(header, w.data(), w.size() * 2);
            if (st.b % 5 == 0 && rd.bytes.size() > 4) rd.bytes.resize(4 + (rd.bytes.size() - 4) / 2);
            L(s.f<RW>("?Read@UnicodeString@Jet@@QAA_NAAVBinReader@2@@Z")(&x, &rd));
            break;
        }
        case 30: {
            OrigAnsi a(t.c_str());
            if (st.a & 1) {
                dtor(&x, nullptr);
                s.f<CP>("??0UnicodeString@Jet@@QAE@ABVAnsiString@1@@Z")(&x, nullptr, &a);
            } else if (t.size()) {  // an empty AnsiString makes the original call _mbslen(null)
                L(s.f<MP>("??4UnicodeString@Jet@@QAAABV01@ABVAnsiString@1@@Z")(&x, &a) == &x);
            }
            L(s.f<MP>("??4UnicodeString@Jet@@QAAABV01@ABVString@1@@Z")(&y, &a) == &y);
            Dump(y, __LINE__);
            break;
        }
        case 31: {
            void* node = st.a % 4 ? PStringNode(t.c_str()) : nullptr;
            if (st.b & 1) {
                dtor(&x, nullptr);
                s.f<CP>("??0UnicodeString@Jet@@QAE@ABVPString@1@@Z")(&x, nullptr, &node);
            } else {
                L(s.f<MP>("??4UnicodeString@Jet@@QAAABV01@ABVPString@1@@Z")(&x, &node) == &x);
            }
            PStringRelease(node);
            break;
        }
        }
        Dump(x, __LINE__);
    }
    for (auto& x : o) dtor(&x, nullptr);
}

// ---- MultiByteString {data, capacity, bytes, characters}

struct M {
    char* data;
    unsigned cap, bytes, chars;
};
static void Dump(const M& m, int line)
{
    Log(m.data != nullptr, line);
    Log(m.cap, line);
    Log(m.bytes, line);
    Log(m.chars, line);
    if (m.data) Log(Hash(m.data, m.bytes + 1), line);
}

static void MultiByteScenario(const Side& s, const std::vector<Step>& script)
{
    M o[6];
    const auto dtor = s.f<Dt>("??1MultiByteString@Jet@@QAE@XZ");
    for (auto& x : o) s.f<C0>("??0MultiByteString@Jet@@QAE@XZ")(&x, nullptr);
    for (const Step& st : script) {
        M& x = o[st.obj];
        M& y = o[st.o2];
        const std::string& t = texts[st.t];
        const std::wstring& w = wtexts[(st.t + st.c) % wtexts.size()];
        const unsigned len = x.chars;
        L(st.op);
        if (getenv("KS_TRACE"))
            std::printf("%zu op %d  cap %u bytes %u chars %u  a %d b %d\n", &st - script.data(), st.op, x.cap, x.bytes,
                        x.chars, st.a, st.b);
        switch (st.op) {
        case 0: dtor(&x, nullptr); s.f<C0>("??0MultiByteString@Jet@@QAE@XZ")(&x, nullptr); break;
        case 1:
            if (&x == &y) break;
            dtor(&x, nullptr);
            if (st.a & 1)
                s.f<CP>("??0MultiByteString@Jet@@QAE@ABV01@@Z")(&x, nullptr, &y);
            else
                s.f<CPII>("??0MultiByteString@Jet@@QAE@ABV01@IH@Z")(&x, nullptr, &y, st.b % (y.chars + 2), int(st.c % 20) - 3);
            break;
        case 2: dtor(&x, nullptr); s.f<CP>("??0MultiByteString@Jet@@QAE@PBD@Z")(&x, nullptr, st.a % 9 ? t.c_str() : nullptr); break;
        case 3: dtor(&x, nullptr); s.f<CPI>("??0MultiByteString@Jet@@QAE@PBDI@Z")(&x, nullptr, t.c_str(), st.a % (t.size() + 1)); break;
        case 4: dtor(&x, nullptr); s.f<CII>("??0MultiByteString@Jet@@QAE@II@Z")(&x, nullptr, 0x41 + st.b % 60, st.c % 30); break;
            // (a double-byte ch leaves bytes != characters, after which the original's Ins / Del / SetLength
            // write past the buffer; covered once below on a fresh object only)
        case 5: L(s.f<MP>("??4MultiByteString@Jet@@QAAABV01@ABV01@@Z")(&x, &y) == &x); break;
        case 6: L(s.f<MP>("??4MultiByteString@Jet@@QAAABV01@PBD@Z")(&x, st.a % 9 ? t.c_str() : nullptr) == &x); break;
        case 7: L(s.f<MPI>("?SetANSI@MultiByteString@Jet@@QAA_NPBDH@Z")(&x, t.c_str(), st.a & 1 ? -1 : int(st.a % (t.size() + 1))) & 0xff); break;
        case 8:
            if (!(s.f<MPI>("?SetUnicode@MultiByteString@Jet@@QAA_NPB_WH@Z")(&x, w.c_str(), st.a & 1 ? -1 : int(st.a % (w.size() + 1))) & 0xff)) {
                // a failed conversion keeps the character count with a null buffer; the original's next Del or Ins
                // then reads address 0, so the object is made again after its state is logged
                L(0);
                Dump(x, __LINE__);
                dtor(&x, nullptr);
                s.f<C0>("??0MultiByteString@Jet@@QAE@XZ")(&x, nullptr);
            }
            break;
        case 9: s.f<MII>("?Set@MultiByteString@Jet@@QAAXHI@Z")(&x, st.a * 7919 - 400000, 2 + st.b % 35); break;
        case 10: s.f<Fmt>("?Setf@MultiByteString@Jet@@QAAXPBDZZ")(&x, st.a & 1 ? "%s|%d|%x" : "", t.c_str(), st.a - 500, st.b); break;
        case 11: L(s.f<Fmt>("?Addf@MultiByteString@Jet@@QAAHPBDZZ")(&x, st.a & 1 ? "%s." : "", t.c_str())); break;
        case 12: s.f<MPI>("?Setn@MultiByteString@Jet@@QAAXPBDI@Z")(&x, t.c_str(), st.a % 50); break;
        case 13: s.f<MPI>("?Ins@MultiByteString@Jet@@QAAXPBDI@Z")(&x, t.c_str(), st.a % (len + 2)); break;
        case 14: s.f<MIII>("?Ins@MultiByteString@Jet@@QAAXIII@Z")(&x, 0x30 + st.a % 200, st.b % (len + 2), st.c % 9); break;
        case 15: s.f<MII>("?Del@MultiByteString@Jet@@QAAXII@Z")(&x, st.a % (len + 2), st.b % 12); break;
        case 16: s.f<MI>("?Left@MultiByteString@Jet@@QAAXI@Z")(&x, st.a % (len + 2)); break;
        case 17: s.f<MI>("?Right@MultiByteString@Jet@@QAAXI@Z")(&x, st.a % (len + 2)); break;
        case 18: s.f<MI>("?SetLength@MultiByteString@Jet@@QAAXI@Z")(&x, st.a % (len + 40)); break;
        case 19: s.f<MP>("?Strip@MultiByteString@Jet@@QAAXPBD@Z")(&x, " ,;"); break;
        case 20: s.f<M0>(st.a & 1 ? "?ToLower@MultiByteString@Jet@@QAAXXZ" : "?ToUpper@MultiByteString@Jet@@QAAXXZ")(&x); break;
        case 21: {
            M r;
            L(s.f<Tok>("?Token@MultiByteString@Jet@@QAA?AV12@PBD@Z")(&x, &r, " ,;") == &r);
            Dump(r, __LINE__);
            dtor(&r, nullptr);
            break;
        }
        case 22: {
            const char* ops[] = {"??8MultiByteString@Jet@@QBA_NABV01@@Z", "??9MultiByteString@Jet@@QBA_NABV01@@Z",
                                 "??MMultiByteString@Jet@@QBA_NABV01@@Z", "??NMultiByteString@Jet@@QBA_NABV01@@Z",
                                 "??OMultiByteString@Jet@@QBA_NABV01@@Z", "??PMultiByteString@Jet@@QBA_NABV01@@Z",
                                 "?EqualsIgnoreCase@MultiByteString@Jet@@QBA_NABV12@@Z"};
            for (const char* n : ops) L(s.f<BP>(n)(&x, &y));
            L(s.f<QP>("?Compare@MultiByteString@Jet@@QBAHABV12@@Z")(&x, &y));
            L(s.f<QP>("?CompareIgnoreCase@MultiByteString@Jet@@QBAHABV12@@Z")(&x, &y));
            const char* p = st.a % 5 ? t.c_str() : nullptr;
            const char* pops[] = {"??8MultiByteString@Jet@@QBA_NPBD@Z", "??9MultiByteString@Jet@@QBA_NPBD@Z",
                                  "??MMultiByteString@Jet@@QBA_NPBD@Z", "??NMultiByteString@Jet@@QBA_NPBD@Z",
                                  "??OMultiByteString@Jet@@QBA_NPBD@Z", "??PMultiByteString@Jet@@QBA_NPBD@Z",
                                  "?EqualsIgnoreCase@MultiByteString@Jet@@QBA_NPBD@Z"};
            for (const char* n : pops) L(s.f<BP>(n)(&x, p));
            L(s.f<QP>("?Compare@MultiByteString@Jet@@QBAHPBD@Z")(&x, p));
            L(s.f<QP>("?CompareIgnoreCase@MultiByteString@Jet@@QBAHPBD@Z")(&x, p));
            break;
        }
        case 23: {
            const unsigned start = st.a % (x.bytes + 1);
            const unsigned ch = t.empty() ? 'a' : static_cast<unsigned char>(t[st.b % t.size()]);
            L(s.f<QII>("?Find@MultiByteString@Jet@@QBAHII@Z")(&x, ch, start));
            L(s.f<QII>("?FindIgnoreCase@MultiByteString@Jet@@QBAHII@Z")(&x, ch ^ (st.c & 0x20), start));
            const std::string sub = t.substr(st.b % (t.size() + 1), st.c % 4);
            L(s.f<QPI>("?Find@MultiByteString@Jet@@QBAHPBDI@Z")(&x, sub.c_str(), start));
            L(s.f<QPI>("?FindIgnoreCase@MultiByteString@Jet@@QBAHPBDI@Z")(&x, sub.c_str(), start));
            L(s.f<SIPI>("?FindIgnoreCase@MultiByteString@Jet@@SAHPBDI0I@Z")(t.c_str(), static_cast<unsigned>(t.size()), sub.c_str(), st.a % (t.size() + 1)));
            L(s.f<SII>("?FindIgnoreCase@MultiByteString@Jet@@SAHPBDII@Z")(t.c_str(), ch, st.a % (t.size() + 1)));
            break;
        }
        case 24: {
            L(s.f<QI>("?GetInt@MultiByteString@Jet@@QBAHH@Z")(&x, st.a % 3 ? 10 : (st.a % 2 ? 16 : 0)));
            L(s.f<QI>("?GetUInt@MultiByteString@Jet@@QBAIH@Z")(&x, 10));
            LogBits(s.f<QD>("?GetFloat@MultiByteString@Jet@@QBAMXZ")(&x), __LINE__);
            LogBits(s.f<QD>("?GetDouble@MultiByteString@Jet@@QBANXZ")(&x), __LINE__);
            L(s.f<Q0>("?Size@MultiByteString@Jet@@QBAIXZ")(&x));
            L(s.f<Q0>("?Length@MultiByteString@Jet@@QBAIXZ")(&x));
            L(s.f<Q0>("?AllocSize@MultiByteString@Jet@@QBAIXZ")(&x));
            L(s.f<Q0>("?CStr@MultiByteString@Jet@@QBAPBDXZ")(&x) == reinterpret_cast<uintptr_t>(x.data));
            if (x.data && x.bytes) L(s.f<QI>("??AMultiByteString@Jet@@QBAIH@Z")(&x, st.b % x.bytes));
            break;
        }
        case 25: {
            if (!x.data) break;
            char a[0x600];
            std::memset(a, 0x5a, sizeof a);
            L(s.f<QPI>("?GetANSI@MultiByteString@Jet@@QBAHPADI@Z")(&x, a, 1 + st.a % 0x400));
            L(Hash(a, sizeof a));
            unsigned short b[0x600];
            std::memset(b, 0x5a, sizeof b);
            L(s.f<QPI>("?GetUnicode@MultiByteString@Jet@@QBAHPA_WI@Z")(&x, b, 1 + st.b % 0x500));
            L(Hash(b, sizeof b));
            break;
        }
        case 26: {
            Writer wr;
            if (st.a % 7 == 0) wr.limit = st.b % 12;
            L(s.f<RW>("?Write@MultiByteString@Jet@@QBA_NAAVBinWriter@2@@Z")(&x, &wr));
            L(Hash(wr.bytes.data(), wr.bytes.size()));
            Reader rd;
            rd.bytes = wr.bytes;
            // (a UTF-16 header makes the original read 2n bytes into room for n: not tried)
            L(s.f<RW>("?Read@MultiByteString@Jet@@QAA_NAAVBinReader@2@@Z")(&y, &rd));
            Dump(y, __LINE__);
            break;
        }
        case 27: {  // a double-byte fill, only looked at
            M r;
            s.f<CII>("??0MultiByteString@Jet@@QAE@II@Z")(&r, nullptr, 0x8140 + st.b, st.c % 30);
            Dump(r, __LINE__);
            L(s.f<Q0>("?Size@MultiByteString@Jet@@QBAIXZ")(&r));
            dtor(&r, nullptr);
            break;
        }
        }
        Dump(x, __LINE__);
    }
    for (auto& x : o) dtor(&x, nullptr);
}

// ---- AnsiString: data -> {refs @-4, capacity, length, characters}

static void* nullData;  // the empty block (the original's, shared by both modules)
static void Dump(void* const& a, int line)
{
    const unsigned* d = static_cast<const unsigned*>(a);
    Log(a == nullData, line);
    Log(static_cast<const long*>(a)[-1], line);
    Log(d[0], line);
    Log(d[1], line);
    Log(Hash(d + 2, d[1] + 1), line);
}

static void AnsiScenario(const Side& s, const std::vector<Step>& script)
{
    void* o[6];
    const Side& other = s.m == ours ? Side{orig, "orig"} : Side{ours, "ours"};
    const char* const dtorName = "??1AnsiString@Jet@@QAE@XZ";
    for (auto& x : o) s.f<C0>("??0AnsiString@Jet@@QAE@XZ")(&x, nullptr);
    for (const Step& st : script) {
        void*& x = o[st.obj];
        void*& y = o[st.o2];
        const std::string& t = texts[st.t];
        const std::wstring& w = wtexts[(st.t + st.c) % wtexts.size()];
        const unsigned len = static_cast<unsigned*>(x)[1];
        // every fifth step frees and makes objects through the other module
        const Side& life = st.c % 5 == 0 ? other : s;
        const auto dtor = life.f<Dt>(dtorName);
        L(st.op);
        switch (st.op) {
        case 0: dtor(&x, nullptr); life.f<C0>("??0AnsiString@Jet@@QAE@XZ")(&x, nullptr); break;
        case 1:
            if (&x == &y) break;
            dtor(&x, nullptr);
            life.f<CP>("??0AnsiString@Jet@@QAE@ABV01@@Z")(&x, nullptr, &y);
            break;
        case 2: dtor(&x, nullptr); life.f<CP>("??0AnsiString@Jet@@QAE@PBD@Z")(&x, nullptr, st.a % 9 ? t.c_str() : nullptr); break;
        case 3: dtor(&x, nullptr); life.f<CPI>("??0AnsiString@Jet@@QAE@PBDI@Z")(&x, nullptr, st.a % 9 ? t.c_str() : nullptr, st.a % 4 ? st.a % (t.size() + 1) : 0xffffffffu); break;
        case 4: dtor(&x, nullptr); s.f<CP>("??0AnsiString@Jet@@QAE@PB_W@Z")(&x, nullptr, w.c_str()); break;
        case 5: dtor(&x, nullptr); s.f<CPI>("??0AnsiString@Jet@@QAE@PB_WI@Z")(&x, nullptr, w.c_str(), st.a % (w.size() + 1)); break;
        case 6: dtor(&x, nullptr); s.f<CII>("??0AnsiString@Jet@@QAE@II@Z")(&x, nullptr, 0x41 + st.a % 300, st.b % 200); break;
        case 7: L(s.f<MP>("??4AnsiString@Jet@@QAAABV01@ABV01@@Z")(&x, &y) == &x); break;
        case 8: L(s.f<MP>("??4AnsiString@Jet@@QAAABV01@PBD@Z")(&x, st.a % 9 ? t.c_str() : nullptr) == &x); break;
        case 9: L(s.f<MP>("??4AnsiString@Jet@@QAAABV01@PB_W@Z")(&x, w.c_str()) == &x); break;
        case 10: L(s.f<MPI>("?SetANSI@AnsiString@Jet@@QAA_NPBDH@Z")(&x, st.a % 9 ? t.c_str() : nullptr, st.a & 1 ? -1 : int(st.a % (t.size() + 1))) & 0xff); break;
        case 11: L(s.f<MPI>("?SetUnicode@AnsiString@Jet@@QAA_NPB_WH@Z")(&x, w.c_str(), st.a & 1 ? -1 : int(st.a % (w.size() + 1))) & 0xff); break;
        case 12: s.f<MII>("?Set@AnsiString@Jet@@QAAXHI@Z")(&x, st.a * 7919 - 400000, 2 + st.b % 35); break;
        case 13: s.f<Fmt>("?Setf@AnsiString@Jet@@QAAXPBDZZ")(&x, st.a & 1 ? "%s|%d|%x" : "", t.c_str(), st.a - 500, st.b); break;
        case 14: L(s.f<Fmt>("?Addf@AnsiString@Jet@@QAAHPBDZZ")(&x, st.a & 1 ? "%s." : "", t.c_str())); break;
        case 15: s.f<MPI>("?Setn@AnsiString@Jet@@QAAXPBDI@Z")(&x, t.c_str(), st.a % 50); break;
        case 16: s.f<MPI>("?Ins@AnsiString@Jet@@QAAXPBDI@Z")(&x, st.a % 9 ? t.c_str() : nullptr, st.a % (len + 2)); break;
        case 17: s.f<MPI>("?Ins@AnsiString@Jet@@QAAXPB_WI@Z")(&x, w.c_str(), st.a % (len + 2)); break;
        case 18: s.f<MIII>("?Ins@AnsiString@Jet@@QAAXIII@Z")(&x, 0x30 + st.a % 200, st.b % (len + 2), st.c % 9); break;
        case 19: s.f<MII>("?Del@AnsiString@Jet@@QAAXII@Z")(&x, st.a % (len + 2), st.b % 12); break;
        case 20: s.f<MI>("?Left@AnsiString@Jet@@QAAXI@Z")(&x, st.a % (len + 2)); break;
        case 21: s.f<MI>("?Right@AnsiString@Jet@@QAAXI@Z")(&x, st.a % (len + 2)); break;
        case 22: s.f<MI>("?SetLength@AnsiString@Jet@@QAAXI@Z")(&x, st.a % (len + 40)); break;
        case 23: s.f<MP>("?Strip@AnsiString@Jet@@QAAXPBD@Z")(&x, " ,;"); break;
        case 24: s.f<M0>(st.a & 1 ? "?ToLower@AnsiString@Jet@@QAAXXZ" : "?ToUpper@AnsiString@Jet@@QAAXXZ")(&x); break;
        case 25: {
            void* r;
            L(s.f<Tok>("?Token@AnsiString@Jet@@QAA?AV12@PBD@Z")(&x, &r, " ,;") == &r);
            Dump(r, __LINE__);
            other.f<Dt>(dtorName)(&r, nullptr);
            break;
        }
        case 26: {
            void* r;
            using Mid = void*(__fastcall*)(const void*, void*, void*, unsigned, unsigned);
            L(s.f<Mid>("?Mid@AnsiString@Jet@@QBE?AV12@II@Z")(&x, nullptr, &r, st.a % (len + 2), st.b % 20) == &r);
            Dump(r, __LINE__);
            s.f<Dt>(dtorName)(&r, nullptr);
            break;
        }
        case 27: {
            const char* ops[] = {"??8AnsiString@Jet@@QBA_NABV01@@Z", "??9AnsiString@Jet@@QBA_NABV01@@Z",
                                 "??MAnsiString@Jet@@QBA_NABV01@@Z", "??NAnsiString@Jet@@QBA_NABV01@@Z",
                                 "??OAnsiString@Jet@@QBA_NABV01@@Z", "??PAnsiString@Jet@@QBA_NABV01@@Z",
                                 "?EqualsIgnoreCase@AnsiString@Jet@@QBA_NABV12@@Z"};
            for (const char* n : ops) L(s.f<BP>(n)(&x, &y));
            L(s.f<QP>("?Compare@AnsiString@Jet@@QBAHABV12@@Z")(&x, &y));
            L(s.f<QP>("?CompareIgnoreCase@AnsiString@Jet@@QBAHABV12@@Z")(&x, &y));
            const char* p = st.a % 5 ? t.c_str() : nullptr;
            const char* pops[] = {"??8AnsiString@Jet@@QBA_NPBD@Z", "??9AnsiString@Jet@@QBA_NPBD@Z",
                                  "??MAnsiString@Jet@@QBA_NPBD@Z", "??NAnsiString@Jet@@QBA_NPBD@Z",
                                  "??OAnsiString@Jet@@QBA_NPBD@Z", "??PAnsiString@Jet@@QBA_NPBD@Z",
                                  "?EqualsIgnoreCase@AnsiString@Jet@@QBA_NPBD@Z"};
            for (const char* n : pops) L(s.f<BP>(n)(&x, p));
            L(s.f<QP>("?Compare@AnsiString@Jet@@QBAHPBD@Z")(&x, p));
            L(s.f<QP>("?CompareIgnoreCase@AnsiString@Jet@@QBAHPBD@Z")(&x, p));
            break;
        }
        case 28: {
            const unsigned start = st.a % (len + 1);
            const unsigned ch = t.empty() ? 'a' : static_cast<unsigned char>(t[st.b % t.size()]);
            L(s.f<QII>("?Find@AnsiString@Jet@@QBAHII@Z")(&x, ch, start));
            L(s.f<QII>("?FindIgnoreCase@AnsiString@Jet@@QBAHII@Z")(&x, ch ^ (st.c & 0x20), start));
            const std::string sub = t.substr(st.b % (t.size() + 1), st.c % 4);
            L(s.f<QPI>("?Find@AnsiString@Jet@@QBAHPBDI@Z")(&x, sub.c_str(), start));
            L(s.f<QPI>("?FindIgnoreCase@AnsiString@Jet@@QBAHPBDI@Z")(&x, sub.c_str(), start));
            L(s.f<SIPI>("?FindIgnoreCase@AnsiString@Jet@@SAHPBDI0I@Z")(t.c_str(), static_cast<unsigned>(t.size()), sub.c_str(), st.a % (t.size() + 1)));
            L(s.f<SII>("?FindIgnoreCase@AnsiString@Jet@@SAHPBDII@Z")(t.c_str(), ch, st.a % (t.size() + 1)));
            break;
        }
        case 29: {
            L(s.f<QI>("?GetInt@AnsiString@Jet@@QBAHH@Z")(&x, st.a % 3 ? 10 : (st.a % 2 ? 16 : 0)));
            L(s.f<QI>("?GetUInt@AnsiString@Jet@@QBAIH@Z")(&x, 10));
            LogBits(s.f<QD>("?GetFloat@AnsiString@Jet@@QBAMXZ")(&x), __LINE__);
            LogBits(s.f<QD>("?GetDouble@AnsiString@Jet@@QBANXZ")(&x), __LINE__);
            L(s.f<Q0>("?Size@AnsiString@Jet@@QBAIXZ")(&x));
            L(s.f<Q0>("?Length@AnsiString@Jet@@QBAIXZ")(&x));
            L(s.f<Q0>("?AllocSize@AnsiString@Jet@@QBAIXZ")(&x));
            const uintptr_t c = s.f<Q0>("??BAnsiString@Jet@@QBAPBDXZ")(&x);
            L(c ? c - reinterpret_cast<uintptr_t>(x) : 0);
            L(s.f<Q0>("??BString@Jet@@QBAPBDXZ")(&x) == c);
            if (len) {
                L(s.f<QI>("??AAnsiString@Jet@@QBAIH@Z")(&x, st.b % len));
                L(Get<unsigned(__cdecl*)(const void*, long long)>(s.m, "??AAnsiString@Jet@@QBAI_J@Z")(&x, st.b % len));
            }
            using IsB = bool(__fastcall*)(const void*, void*);
            L(s.f<IsB>("?IsBufferNULL@AnsiString@Jet@@ABE_NXZ")(&x, nullptr));
            L(s.f<IsB>("?IsBufferUnique@AnsiString@Jet@@ABE_NXZ")(&x, nullptr));
            break;
        }
        case 30: {
            char a[0x600];
            std::memset(a, 0x5a, sizeof a);
            L(s.f<QPI>("?GetANSI@AnsiString@Jet@@QBAHPADI@Z")(&x, a, st.a % 0x400));
            L(Hash(a, sizeof a));
            unsigned short b[0x600];
            std::memset(b, 0x5a, sizeof b);
            L(s.f<QPI>("?GetUnicode@AnsiString@Jet@@QBAHPA_WI@Z")(&x, b, 2 + st.b % 0x500));
            L(Hash(b, sizeof b));
            break;
        }
        case 31: {
            Writer wr;
            if (st.a % 7 == 0) wr.limit = st.b % 12;
            L(s.f<RW>("?Write@AnsiString@Jet@@QBA_NAAVBinWriter@2@@Z")(&x, &wr));
            L(Hash(wr.bytes.data(), wr.bytes.size()));
            Reader rd;
            rd.bytes = wr.bytes;
            L(s.f<RW>("?Read@AnsiString@Jet@@QAA_NAAVBinReader@2@@Z")(&y, &rd));
            Dump(y, __LINE__);
            const unsigned n = static_cast<unsigned>(w.size() * 2);
            Reader r2 = Crafted(st.b & 1 ? (n | 0x40000000) : 0x40000000, w.data(), n);
            if (st.b % 5 == 0 && r2.bytes.size() > 4) r2.bytes.resize(4 + n / 2);
            L(s.f<RW>("?Read@AnsiString@Jet@@QAA_NAAVBinReader@2@@Z")(&x, &r2));
            break;
        }
        case 32: {  // conversions: UnicodeString / PString in and out
            unsigned short* ut[3];
            Get<CP>(orig, "??0UnicodeString@Jet@@QAE@PB_W@Z")(ut, nullptr, w.c_str());
            if (st.a & 1) {
                dtor(&x, nullptr);
                life.f<CP>(st.b & 1 ? "??0AnsiString@Jet@@QAE@ABVUnicodeString@1@@Z" : "??0String@Jet@@QAE@ABVUnicodeString@1@@Z")(&x, nullptr, ut);
            } else {
                L(s.f<MP>(st.b & 1 ? "??4AnsiString@Jet@@QAAABV01@ABVUnicodeString@1@@Z" : "??4String@Jet@@QAAABV01@ABVUnicodeString@1@@Z")(&x, ut) == &x);
            }
            Get<Dt>(orig, "??1UnicodeString@Jet@@QAE@XZ")(ut, nullptr);
            void* node = st.c % 4 ? PStringNode(t.c_str()) : nullptr;
            if (st.c & 1) {
                dtor(&y, nullptr);
                life.f<CP>(st.b & 2 ? "??0AnsiString@Jet@@QAE@ABVPString@1@@Z" : "??0String@Jet@@QAE@ABVPString@1@@Z")(&y, nullptr, &node);
            } else {
                L(s.f<MP>(st.b & 2 ? "??4AnsiString@Jet@@QAAABV01@ABVPString@1@@Z" : "??4String@Jet@@QAAABV01@ABVPString@1@@Z")(&y, &node) == &y);
                s.f<MP>("??YAnsiString@Jet@@QAAAAV01@ABVPString@1@@Z")(&y, &node);
            }
            PStringRelease(node);
            Dump(y, __LINE__);
            break;
        }
        case 33: {  // appends and String's own members
            // (x += x copies from the block it just released when the buffer grows: freed memory, not compared)
            if (&x != &y) s.f<MP>("??YAnsiString@Jet@@QAAAAV01@ABV01@@Z")(&x, &y);
            s.f<MP>("??YAnsiString@Jet@@QAAAAV01@PBD@Z")(&x, t.c_str());
            s.f<MP>("??YAnsiString@Jet@@QAAAAV01@PB_W@Z")(&x, w.c_str());
            if (&x != &y) {
                dtor(&y, nullptr);
                life.f<CPII>("??0String@Jet@@QAE@ABV01@IH@Z")(&y, nullptr, &x, st.a % (len + 3), int(st.b % 30) - 5);
                Dump(y, __LINE__);
            }
            L(s.f<MP>("??4String@Jet@@QAAABV01@PBD@Z")(&o[(st.obj + 1) % 6], t.c_str()) == &o[(st.obj + 1) % 6]);
            L(s.f<MP>("??4String@Jet@@QAAABV01@ABV01@@Z")(&o[(st.obj + 2) % 6], &x) == &o[(st.obj + 2) % 6]);
            Dump(o[(st.obj + 2) % 6], __LINE__);
            break;
        }
        case 34: {
            using V = void(__fastcall*)(void*, void*);
            using VU = void(__fastcall*)(void*, void*, unsigned, bool);
            if (st.a & 1)
                s.f<V>("?MakeBufferEmpty@AnsiString@Jet@@AAEXXZ")(&x, nullptr);
            else if (st.a & 2)
                s.f<V>("?MakeBufferNULL@AnsiString@Jet@@AAEXXZ")(&x, nullptr);
            else
                s.f<VU>("?MakeBufferUnique@AnsiString@Jet@@AAEXI_N@Z")(&x, nullptr, st.b % 300, (st.c & 1) != 0);
            break;
        }
        }
        Dump(x, __LINE__);
    }
    for (auto& x : o) s.f<Dt>(dtorName)(&x, nullptr);
}

using Scen = void (*)(const Side&, const std::vector<Step>&);
static size_t Compare(const char* name, Scen f, const std::vector<Step>& script)
{
    logv.clear();
    lines.clear();
    std::printf("%s: orig\n", name);
    f({orig, "orig"}, script);
    const std::vector<long long> a = logv;
    const std::vector<int> la = lines;
    logv.clear();
    lines.clear();
    std::printf("%s: ours\n", name);
    f({ours, "ours"}, script);
    size_t bad = 0;
    for (size_t i = 0; i < a.size() || i < logv.size(); ++i) {
        const long long x = i < a.size() ? a[i] : -999, y = i < logv.size() ? logv[i] : -999;
        if (x != y && ++bad <= 10)
            std::printf("  %s step %zu (line %d): orig %lld ours %lld\n", name, i, i < la.size() ? la[i] : 0, x, y);
    }
    std::printf("%s: %zu results each, %zu mismatches\n", name, a.size(), bad);
    return bad;
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    const int steps = argc > 2 ? std::atoi(argv[2]) : 20000;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e) -> LONG {
        std::printf("crash %08lx at %p after line %d\n", e->ExceptionRecord->ExceptionCode,
                    e->ExceptionRecord->ExceptionAddress, lines.empty() ? 0 : lines.back());
        return EXCEPTION_EXECUTE_HANDLER;
    });
    SetDllDirectoryA(argv[1]);
    ours = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c.dll").c_str());
    orig = LoadLibraryA((std::string(argv[1]) + "\\KernelStandard_7c_orig.dll").c_str());
    if (!ours || !orig || ours == orig) return 2;
    // the game sets the locale like this (KernelStandardInit); the original's own init already ran
    using SetLoc = char*(__cdecl*)(int, const char*);
    HMODULE crt = GetModuleHandleA("msvcr80.dll");
    if (crt) reinterpret_cast<SetLoc>(GetProcAddress(crt, "setlocale"))(0, ".ACP");
    {
        void* a;
        Get<C0>(orig, "??0AnsiString@Jet@@QAE@XZ")(&a, nullptr);
        nullData = a;
    }
    MakeCorpus();
    size_t bad = 0;
    bad += Compare("UnicodeString", UnicodeScenario, MakeScript(32, steps, 1));
    bad += Compare("MultiByteString", MultiByteScenario, MakeScript(28, steps, 2));
    bad += Compare("AnsiString", AnsiScenario, MakeScript(35, steps, 3));
    std::printf("total mismatches: %zu\n", bad);
    return bad ? 1 : 0;
}
