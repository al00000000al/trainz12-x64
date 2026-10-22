// Differential test of KernelStandard unit U9a (Streams.cpp: DynamicBuffer). Each scenario draws one sequence of
// operations on a buffer made by one of the constructors (sizes, increments, external data), then runs it on our
// module's buffer and on the original's: every write and read overload (the strings' through the module's
// BinReader / BinWriter), Seek / SetSize / Rewind / Reset / Clean / ReInit / SetData / AddLength, copies and
// assignment, the getters. After each operation the result, the fields and the written bytes are logged.
//
// Bytes no write has set (a new allocation's tail, a size moved past what was written) are heap garbage that
// differs between the sides: the test keeps which bytes are set and logs the others as "..", and a read of such
// bytes as "(unset)". A string read whose count runs past the data is not drawn (the reader would allocate it).
//
// usage: diff_ks_dynbuf.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
// KDB_SWAP: the sides swapped; KDB_DUMP: every log line.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
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

static int side;
static std::vector<std::string> logs[2];
static void Log(const std::string& s) { logs[side].push_back(s); }
static std::string Num(long long v) { return std::to_string(v); }
static std::string Hex(const void* p, size_t n)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<const unsigned char*>(p)[i];
        s += digits[c >> 4];
        s += digits[c & 15];
    }
    return s;
}

using Node = unsigned char;
static std::string NText(const Node* n) { return n ? "'" + std::string(reinterpret_cast<const char*>(n + 0x18)) + "'" : "(null)"; }
static std::string AText(const void* s)
{
    const unsigned* const b = *static_cast<const unsigned* const*>(s);
    return "[" + std::to_string(b[0] != 0) + " " + std::to_string(b[1]) + " '" + std::string(reinterpret_cast<const char*>(b + 2), b[1]) + "']";
}
static std::string UText(const void* u)
{
    const uint16_t* const t = *static_cast<const uint16_t* const*>(u);
    const unsigned len = static_cast<const unsigned*>(u)[2];
    return "<" + std::to_string(len) + " " + (t ? Hex(t, len * 2) : std::string("null")) + ">";
}

// ---- exports
#define DB "@DynamicBuffer@Jet@@"
struct Api {
    void(__fastcall* ctorDefault)(void*);
    void*(__fastcall* ctor)(void*, void*, unsigned long, unsigned long);
    void*(__fastcall* ctorData)(void*, void*, void*, unsigned long, bool);
    void*(__fastcall* copy)(void*, void*, const void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    void(__fastcall* dtor)(void*, void*);
    void(__fastcall* clean)(void*, void*);
    void(__fastcall* reset)(void*, void*);
    void(__fastcall* rewind)(void*, void*);
    void(__fastcall* reinit)(void*, void*, unsigned long, unsigned long);
    bool(__fastcall* seek)(void*, void*, unsigned long);
    bool(__fastcall* setSize)(void*, void*, unsigned long);
    void(__fastcall* setData)(void*, void*, void*, unsigned long, bool);
    bool(__fastcall* readOnly)(void*, void*);
    unsigned long(__fastcall* getSize)(const void*, void*);
    unsigned long(__fastcall* getLength)(const void*, void*);
    unsigned long(__fastcall* getIncrement)(const void*, void*);
    unsigned long(__fastcall* getPos)(const void*, void*);
    void*(__fastcall* getData)(const void*, void*);
    void*(__fastcall* getCurrent)(const void*, void*);
    void(__fastcall* addLength)(void*, void*, unsigned long);
    bool(__fastcall* wShort)(void*, void*, short);
    bool(__fastcall* wUShort)(void*, void*, unsigned short);
    bool(__fastcall* wSChar)(void*, void*, signed char);
    bool(__fastcall* wUChar)(void*, void*, unsigned char);
    bool(__fastcall* w32Int)(void*, void*, int);
    bool(__fastcall* w32UInt)(void*, void*, unsigned);
    bool(__fastcall* wLong)(void*, void*, long);
    bool(__fastcall* wULong)(void*, void*, unsigned long);
    bool(__fastcall* wFloat)(void*, void*, float);
    bool(__fastcall* wBool)(void*, void*, bool);
    bool(__fastcall* wAnsi)(void*, void*, const void*);
    bool(__fastcall* wString)(void*, void*, const void*);
    bool(__fastcall* wUnicode)(void*, void*, const void*);
    bool(__fastcall* wPString)(void*, void*, Node* const*);
    unsigned long(__fastcall* wRaw)(void*, void*, const void*, unsigned long, unsigned long);
    bool(__fastcall* rShort)(const void*, void*, void*);  // all the typed reads take a pointer
    bool(__fastcall* rUShort)(const void*, void*, void*);
    bool(__fastcall* rSChar)(const void*, void*, void*);
    bool(__fastcall* rUChar)(const void*, void*, void*);
    bool(__fastcall* rLong)(const void*, void*, void*);
    bool(__fastcall* rULong)(const void*, void*, void*);
    bool(__fastcall* rFloat)(const void*, void*, void*);
    bool(__fastcall* rBool)(const void*, void*, void*);
    bool(__fastcall* r32Int)(const void*, void*, void*);
    bool(__fastcall* r32UInt)(const void*, void*, void*);
    bool(__fastcall* rPString)(const void*, void*, Node**);
    bool(__fastcall* rAnsi)(const void*, void*, void*);
    bool(__fastcall* rString)(const void*, void*, void*);
    bool(__fastcall* rUnicode)(const void*, void*, void*);
    unsigned long(__fastcall* rRaw)(const void*, void*, void*, unsigned long, unsigned long);
};
static Api apis[2];
static void Load(Api& a, HMODULE m)
{
    a.ctorDefault = Get<decltype(a.ctorDefault)>(m, "??_FDynamicBuffer@Jet@@QAEXXZ");
    a.ctor = Get<decltype(a.ctor)>(m, "??0DynamicBuffer@Jet@@QAE@KK@Z");
    a.ctorData = Get<decltype(a.ctorData)>(m, "??0DynamicBuffer@Jet@@QAE@PAXK_N@Z");
    a.copy = Get<decltype(a.copy)>(m, "??0DynamicBuffer@Jet@@QAE@ABV01@@Z");
    a.assign = Get<decltype(a.assign)>(m, "??4DynamicBuffer@Jet@@QAEAAV01@ABV01@@Z");
    a.dtor = Get<decltype(a.dtor)>(m, "??1DynamicBuffer@Jet@@QAE@XZ");
    a.clean = Get<decltype(a.clean)>(m, "?Clean" DB "QAEXXZ");
    a.reset = Get<decltype(a.reset)>(m, "?Reset" DB "QAEXXZ");
    a.rewind = Get<decltype(a.rewind)>(m, "?Rewind" DB "QAEXXZ");
    a.reinit = Get<decltype(a.reinit)>(m, "?ReInit" DB "QAEXKK@Z");
    a.seek = Get<decltype(a.seek)>(m, "?Seek" DB "QAE_NK@Z");
    a.setSize = Get<decltype(a.setSize)>(m, "?SetSize" DB "QAE_NK@Z");
    a.setData = Get<decltype(a.setData)>(m, "?SetData" DB "QAEXPAXK_N@Z");
    a.readOnly = Get<decltype(a.readOnly)>(m, "?IsReadOnly" DB "QAE_NXZ");
    a.getSize = Get<decltype(a.getSize)>(m, "?GetSize" DB "QBEKXZ");
    a.getLength = Get<decltype(a.getLength)>(m, "?GetLength" DB "QBEKXZ");
    a.getIncrement = Get<decltype(a.getIncrement)>(m, "?GetIncrement" DB "QBEKXZ");
    a.getPos = Get<decltype(a.getPos)>(m, "?GetCurrentPos" DB "QBEKXZ");
    a.getData = Get<decltype(a.getData)>(m, "?GetData" DB "QBEPAXXZ");
    a.getCurrent = Get<decltype(a.getCurrent)>(m, "?GetCurrentData" DB "QBEPAXXZ");
    a.addLength = Get<decltype(a.addLength)>(m, "?AddLength" DB "QAEXK@Z");
    a.wShort = Get<decltype(a.wShort)>(m, "?Write" DB "QAE_NF@Z");
    a.wUShort = Get<decltype(a.wUShort)>(m, "?Write" DB "QAE_NG@Z");
    a.wSChar = Get<decltype(a.wSChar)>(m, "?Write" DB "QAE_NC@Z");
    a.wUChar = Get<decltype(a.wUChar)>(m, "?Write" DB "QAE_NE@Z");
    a.w32Int = Get<decltype(a.w32Int)>(m, "?Write32" DB "QAE_NH@Z");
    a.w32UInt = Get<decltype(a.w32UInt)>(m, "?Write32" DB "QAE_NI@Z");
    a.wLong = Get<decltype(a.wLong)>(m, "?Write" DB "QAE_NJ@Z");
    a.wULong = Get<decltype(a.wULong)>(m, "?Write" DB "QAE_NK@Z");
    a.wFloat = Get<decltype(a.wFloat)>(m, "?Write" DB "QAE_NM@Z");
    a.wBool = Get<decltype(a.wBool)>(m, "?Write" DB "QAE_N_N@Z");
    a.wAnsi = Get<decltype(a.wAnsi)>(m, "?Write" DB "QAE_NABVAnsiString@2@@Z");
    a.wString = Get<decltype(a.wString)>(m, "?Write" DB "QAE_NABVString@2@@Z");
    a.wUnicode = Get<decltype(a.wUnicode)>(m, "?Write" DB "QAE_NABVUnicodeString@2@@Z");
    a.wPString = Get<decltype(a.wPString)>(m, "?Write" DB "QAE_NABVPString@2@@Z");
    a.wRaw = Get<decltype(a.wRaw)>(m, "?Write" DB "QAEKPBXKK@Z");
    a.rShort = Get<decltype(a.rShort)>(m, "?Read" DB "QBE_NAAF@Z");
    a.rUShort = Get<decltype(a.rUShort)>(m, "?Read" DB "QBE_NAAG@Z");
    a.rSChar = Get<decltype(a.rSChar)>(m, "?Read" DB "QBE_NAAC@Z");
    a.rUChar = Get<decltype(a.rUChar)>(m, "?Read" DB "QBE_NAAE@Z");
    a.rLong = Get<decltype(a.rLong)>(m, "?Read" DB "QBE_NAAJ@Z");
    a.rULong = Get<decltype(a.rULong)>(m, "?Read" DB "QBE_NAAK@Z");
    a.rFloat = Get<decltype(a.rFloat)>(m, "?Read" DB "QBE_NAAM@Z");
    a.rBool = Get<decltype(a.rBool)>(m, "?Read" DB "QBE_NAA_N@Z");
    a.r32Int = Get<decltype(a.r32Int)>(m, "?Read32" DB "QBE_NAAH@Z");
    a.r32UInt = Get<decltype(a.r32UInt)>(m, "?Read32" DB "QBE_NAAI@Z");
    a.rPString = Get<decltype(a.rPString)>(m, "?Read" DB "QBE_NAAVPString@2@@Z");
    a.rAnsi = Get<decltype(a.rAnsi)>(m, "?Read" DB "QBE_NAAVAnsiString@2@@Z");
    a.rString = Get<decltype(a.rString)>(m, "?Read" DB "QBE_NAAVString@2@@Z");
    a.rUnicode = Get<decltype(a.rUnicode)>(m, "?Read" DB "QBE_NAAVUnicodeString@2@@Z");
    a.rRaw = Get<decltype(a.rRaw)>(m, "?Read" DB "QBEKPAXKK@Z");
}

// the original's own helpers (the same for both sides)
static void** pstringCache;
static Node* (__cdecl* pstringCreate)(void*, const unsigned char*);
static void(__cdecl* pstringDestroy)(void*, Node*);
static void* (__fastcall* asCtor)(void*, void*);
static void* (__fastcall* asCtorText)(void*, void*, const char*);
static void(__fastcall* asDtor)(void*, void*);
static void* (__fastcall* strCtor)(void*, void*);
static void* (__fastcall* strCtorText)(void*, void*, const char*);
static void(__fastcall* strDtor)(void*, void*);
static void* (__fastcall* usCtor)(void*, void*);
static void(__fastcall* usDtor)(void*, void*);
static void(__cdecl* usSet)(void*, const wchar_t*, int, int);
static void Release(Node* n)
{
    if (n) pstringDestroy(*pstringCache, n);
}
static Node* Make(const char* t) { return t ? pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(t)) : nullptr; }

// ---- the generator (drawn once per scenario)
static std::mt19937 rng(9393);
static uint32_t R(uint32_t n) { return n ? rng() % n : 0; }
static uint32_t Size()
{
    static const uint32_t special[] = {0, 1, 2, 3, 4, 7, 8, 12, 16, 100, 0x1ff, 0x200, 0x201, 0x1000};
    return R(3) ? special[R(14)] : R(3000);
}
static const char* const kTexts[] = {"", "a", "text", "with space", "\xc9t\xc9", "a much longer string to grow the buffer past one increment"};

enum Make { MAKE_DEFAULT, MAKE_SIZED, MAKE_DATA_COPY, MAKE_DATA_EXTERNAL };
struct Op {
    int kind;
    uint32_t a, b;     // sizes, values
    uint32_t bits;     // 4-byte value / float bits
    bool flag;         // a null pointer / the external flag / self assignment
    int text;          // kTexts index
    std::wstring wide;
};
struct Scenario {
    int make;
    uint32_t a, b;
    std::string external;  // the external / copied data
    std::vector<Op> ops;
};
static Scenario sc;
static const int kOps = 50;  // 42..49: a string written, the position set back, then read as another kind

static void Draw()
{
    sc.make = static_cast<int>(R(4));
    sc.a = Size();
    sc.b = R(3) ? 0 : Size();
    sc.external.assign(R(3) ? R(40) : Size(), '\0');
    for (char& c : sc.external) c = static_cast<char>(rng());
    sc.ops.clear();
    for (uint32_t i = 0, n = 1 + R(60); i < n; ++i) {
        Op o;
        o.kind = static_cast<int>(R(kOps));
        o.a = R(4) ? R(16) : Size();
        o.b = R(4) ? 1 + R(4) : Size() % 9;
        o.bits = R(4) ? rng() : (R(2) ? 0 : 0x7fc00001);
        o.flag = R(6) == 0;
        o.text = static_cast<int>(R(sizeof(kTexts) / sizeof(kTexts[0])));
        o.wide.clear();
        for (uint32_t j = 0, l = R(12); j < l; ++j) o.wide += static_cast<wchar_t>(R(3) ? 'a' + R(26) : 0x400 + R(0x60));
        sc.ops.push_back(o);
    }
}

// ---- one side
struct Buf {  // the buffer as raw memory
    unsigned char* data;
    unsigned long position, capacity, size, increment;
    bool external;
};
static std::vector<char> set;  // which bytes of the current data have been written (sized to the capacity)

static std::string State(const Buf* b)
{
    std::string s = "pos " + Num(b->position) + " cap " + Num(b->capacity) + " size " + Num(b->size) + " inc " +
                    Num(b->increment) + " ext " + Num(b->external) + (b->data ? " data" : " null");
    if (b->data) {
        s += " [";
        for (unsigned long i = 0; i < b->size && i < 4096; ++i)
            s += i < set.size() && set[i] ? Hex(b->data + i, 1) : std::string("..");
        s += "]";
    }
    return s;
}
static bool AllSet(unsigned long from, unsigned long to)
{
    for (unsigned long i = from; i < to; ++i)
        if (i >= set.size() || !set[i]) return false;
    return true;
}
// after an operation: the set bytes follow the capacity (new room is unset); a write marks [from, position)
static void Track(const Buf* b, unsigned long writeFrom = ~0ul)
{
    set.resize(b->capacity, 0);
    if (writeFrom != ~0ul)
        for (unsigned long i = writeFrom; i < b->position && i < set.size(); ++i) set[i] = 1;
}
static std::string ReadOut(const Buf* b, unsigned long from, const void* v, size_t n)
{
    return AllSet(from, b->position) ? Hex(v, n) : std::string("(unset)");
}

static void Run()
{
    const Api& a = apis[side];
    alignas(8) unsigned char mem[0x40];
    alignas(8) unsigned char other[0x40];
    Buf* const b = reinterpret_cast<Buf*>(mem);
    std::string external = sc.external;
    std::memset(mem, 0xcd, sizeof(mem));
    switch (sc.make) {
    case MAKE_DEFAULT: a.ctorDefault(b); break;
    case MAKE_SIZED: a.ctor(b, nullptr, sc.a, sc.b); break;
    default: a.ctorData(b, nullptr, external.empty() ? nullptr : &external[0], static_cast<unsigned long>(external.size()), sc.make == MAKE_DATA_EXTERNAL); break;
    }
    set.assign(b->capacity, sc.make >= MAKE_DATA_COPY ? 1 : 0);
    Log("made " + State(b));
    std::function<void(const Op&, std::string&)> step = [&](const Op& o, std::string& s) {
        const unsigned long from = b->position;
        unsigned char v[8];
        std::memset(v, 0xee, sizeof(v));
        float f;
        std::memcpy(&f, &o.bits, 4);
        switch (o.kind) {
        case 0: s += Num(a.wShort(b, nullptr, static_cast<short>(o.bits))); Track(b, from); break;
        case 1: s += Num(a.wUShort(b, nullptr, static_cast<unsigned short>(o.bits))); Track(b, from); break;
        case 2: s += Num(a.wSChar(b, nullptr, static_cast<signed char>(o.bits))); Track(b, from); break;
        case 3: s += Num(a.wUChar(b, nullptr, static_cast<unsigned char>(o.bits))); Track(b, from); break;
        case 4: s += Num(a.w32Int(b, nullptr, static_cast<int>(o.bits))); Track(b, from); break;
        case 5: s += Num(a.w32UInt(b, nullptr, o.bits)); Track(b, from); break;
        case 6: s += Num(a.wLong(b, nullptr, static_cast<long>(o.bits))); Track(b, from); break;
        case 7: s += Num(a.wULong(b, nullptr, o.bits)); Track(b, from); break;
        case 8: s += Num(a.wFloat(b, nullptr, f)); Track(b, from); break;
        case 9: s += Num(a.wBool(b, nullptr, (o.bits & 3) != 0)); Track(b, from); break;
        case 10: {
            std::string bytes(o.a * o.b + 1, '\0');
            for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<char>(o.bits + i * 7);
            const unsigned long r = a.wRaw(b, nullptr, o.flag ? nullptr : bytes.data(), o.a, o.b);
            s += Num(r);
            Track(b, from);
            break;
        }
        case 11: {
            alignas(4) unsigned char t[4];
            asCtorText(t, nullptr, kTexts[o.text]);
            const bool r = a.wAnsi(b, nullptr, t);
            asDtor(t, nullptr);
            s += Num(r);
            Track(b, from);
            break;
        }
        case 12: {
            alignas(4) unsigned char t[4];
            strCtorText(t, nullptr, kTexts[o.text]);
            const bool r = a.wString(b, nullptr, t);
            strDtor(t, nullptr);
            s += Num(r);
            Track(b, from);
            break;
        }
        case 13: {
            alignas(4) unsigned char u[12];
            usCtor(u, nullptr);
            if (!o.flag) usSet(u, o.wide.c_str(), static_cast<int>(o.wide.size()), static_cast<int>(o.wide.size()));
            const bool r = a.wUnicode(b, nullptr, u);
            usDtor(u, nullptr);
            s += Num(r);
            Track(b, from);
            break;
        }
        case 14: {
            Node* p = o.flag ? nullptr : Make(kTexts[o.text]);
            const bool r = a.wPString(b, nullptr, &p);
            Release(p);
            s += Num(r);
            Track(b, from);
            break;
        }
        case 15: { const bool r = a.rShort(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 2); break; }
        case 16: { const bool r = a.rUShort(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 2); break; }
        case 17: { const bool r = a.rSChar(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 1); break; }
        case 18: { const bool r = a.rUChar(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 1); break; }
        case 19: { const bool r = a.rLong(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 4); break; }
        case 20: { const bool r = a.rULong(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 4); break; }
        case 21: { const bool r = a.rFloat(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 4); break; }
        case 22: { const bool r = a.rBool(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 1); break; }
        case 23: { const bool r = a.r32Int(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 4); break; }
        case 24: { const bool r = a.r32UInt(b, nullptr, v); s += Num(r) + " " + ReadOut(b, from, v, 4); break; }
        case 25: {
            std::string dst(o.a * o.b + 4, '\x5a');
            const unsigned long r = a.rRaw(b, nullptr, o.flag ? nullptr : &dst[0], o.a, o.b);
            s += Num(r) + " " + (AllSet(from, b->position) ? Hex(dst.data(), dst.size()) : std::string("(unset)"));
            break;
        }
        case 42: case 43: case 44: case 45: case 46: case 47: case 48: case 49: {
            static const int writes[] = {11, 12, 13, 14};
            static const int reads[] = {26, 27, 28, 29};
            Op w = o;
            w.kind = writes[(o.kind - 42) & 3];
            w.flag = false;
            Op r = o;
            r.kind = reads[(o.bits >> 3) & 3];
            r.flag = false;
            // (two operations of their own, logged on one line)
            s += "pair " + Num(w.kind) + "/" + Num(r.kind) + " ";
            step(w, s);
            s += " seek " + Num(a.seek(b, nullptr, from)) + " ";
            step(r, s);
            break;
        }
        case 26: case 27: case 28: case 29: {
            // the count must be set bytes and fit what is left, or the reader allocates it
            uint32_t head = 0;
            const bool fits = b->data && b->position + 4 <= b->size && AllSet(b->position, b->position + 4) &&
                              (std::memcpy(&head, b->data + b->position, 4), (head & 0xbfffffffu) <= b->size - b->position - 4) &&
                              AllSet(b->position, b->position + 4 + (head & 0xbfffffffu));
            if (!fits) {
                s += "(not read)";
                break;
            }
            if (o.kind == 26) {
                Node* p = Make("old");
                const bool r = a.rPString(b, nullptr, &p);
                s += Num(r) + " " + NText(p);
                Release(p);
            } else if (o.kind == 29) {
                alignas(4) unsigned char u[12];
                usCtor(u, nullptr);
                const bool r = a.rUnicode(b, nullptr, u);
                s += Num(r) + " " + UText(u);
                usDtor(u, nullptr);
            } else {
                alignas(4) unsigned char t[4];
                if (o.kind == 27) asCtor(t, nullptr);
                else strCtor(t, nullptr);
                const bool r = o.kind == 27 ? a.rAnsi(b, nullptr, t) : a.rString(b, nullptr, t);
                s += Num(r) + " " + AText(t);
                if (o.kind == 27) asDtor(t, nullptr);
                else strDtor(t, nullptr);
            }
            break;
        }
        case 30: s += Num(a.seek(b, nullptr, o.a * o.b)); break;
        case 31: s += Num(a.setSize(b, nullptr, o.a * o.b)); break;
        case 32: a.rewind(b, nullptr); break;
        case 33: a.reset(b, nullptr); break;
        case 34: a.clean(b, nullptr); set.clear(); break;
        case 35: a.reinit(b, nullptr, o.a, o.flag ? 0 : o.b * 3); set.assign(b->capacity, 0); break;
        case 36: case 37: {
            a.setData(b, nullptr, external.empty() || o.flag ? nullptr : &external[0], static_cast<unsigned long>(external.size()), o.kind == 37);
            set.assign(b->capacity, 1);
            break;
        }
        case 38: a.addLength(b, nullptr, o.a * o.b); Track(b); break;
        case 39: {  // a copy, which then goes on as the buffer
            std::memset(other, 0xcd, sizeof(other));
            a.copy(other, nullptr, b);
            s += "copy " + State(reinterpret_cast<Buf*>(other)) + " shared " + Num(reinterpret_cast<Buf*>(other)->data == b->data);
            a.dtor(b, nullptr);
            std::memcpy(mem, other, sizeof(mem));
            break;
        }
        case 40: {  // assignment from a fresh buffer over the data, or onto itself
            if (o.flag) {
                a.assign(b, nullptr, b);
            } else {
                std::memset(other, 0xcd, sizeof(other));
                a.ctorData(other, nullptr, external.empty() ? nullptr : &external[0], static_cast<unsigned long>(external.size()), (o.bits & 1) != 0);
                a.assign(b, nullptr, other);
                a.dtor(other, nullptr);
                set.assign(b->capacity, 1);
            }
            break;
        }
        default: {
            s += "get " + Num(a.getSize(b, nullptr)) + " " + Num(a.getLength(b, nullptr)) + " " + Num(a.getIncrement(b, nullptr)) +
                 " " + Num(a.getPos(b, nullptr)) + " " + Num(a.getData(b, nullptr) == b->data) + " " +
                 Num(static_cast<unsigned char*>(a.getCurrent(b, nullptr)) - b->data) + " " + Num(a.readOnly(b, nullptr));
            break;
        }
        }
        if (!b->data) std::fill(set.begin(), set.end(), 0);  // (no data: a later allocation starts unset)
    };
    for (const Op& o : sc.ops) {
        std::string s = "op " + Num(o.kind) + " ";
        step(o, s);
        Log(s + " | " + State(b));
    }
    a.dtor(b, nullptr);
    Log("destroyed " + State(b));
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    std::printf("crash %08lx at %p side %d\n", e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress, side);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_ks_dynbuf <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 20000;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    HMODULE orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    const bool swap = getenv("KDB_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours);
    Load(apis[1], swap ? ours : orig);
    pstringCache = Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringCreate = Get<decltype(pstringCreate)>(orig, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringDestroy = Get<decltype(pstringDestroy)>(orig, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    asCtor = Get<decltype(asCtor)>(orig, "??0AnsiString@Jet@@QAE@XZ");
    asCtorText = Get<decltype(asCtorText)>(orig, "??0AnsiString@Jet@@QAE@PBD@Z");
    asDtor = Get<decltype(asDtor)>(orig, "??1AnsiString@Jet@@QAE@XZ");
    strCtor = Get<decltype(strCtor)>(orig, "??0String@Jet@@QAE@XZ");
    strCtorText = Get<decltype(strCtorText)>(orig, "??0String@Jet@@QAE@PBD@Z");
    strDtor = Get<decltype(strDtor)>(orig, "??1String@Jet@@QAE@XZ");
    usCtor = Get<decltype(usCtor)>(orig, "??0UnicodeString@Jet@@QAE@XZ");
    usDtor = Get<decltype(usDtor)>(orig, "??1UnicodeString@Jet@@QAE@XZ");
    usSet = Get<decltype(usSet)>(orig, "?Set@UnicodeString@Jet@@AAAXPB_WHH@Z");

    unsigned long long checks = 0, bad = 0;
    for (int n = 0; n < scenarios; ++n) {
        Draw();
        for (side = 0; side < 2; ++side) Run();
        const size_t lines = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < lines; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 8)
                    std::printf("scenario %d line %zu:\n  ours %s\n  orig %s\n", n, i, x.substr(0, 1500).c_str(), y.substr(0, 1500).c_str());
                break;
            }
        }
        if (getenv("KDB_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", n, l.substr(0, 400).c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
