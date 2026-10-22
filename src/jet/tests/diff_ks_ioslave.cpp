// Differential test of KernelStandard unit U9b (IOSlave.cpp, BlockReader.cpp). Each scenario draws, once, data and
// operation sequences, then runs them on our module's objects and on the original's: memory / growing memory / file
// slaves (seeks, tells, sizes, reads with and without a destination, writes, copies), the read cursor over a slave
// (Access at offsets past its buffer, Inc, ++, *, [], AccessUInt32, GetIndex, copies), the write cursor (writes,
// history reads, WriteCopy, IOSlave::Write from a cursor), the scrambled resource stream (reading a stream the
// original wrote; writing with the key word set by the test, the time-based padding not compared), BlockReader over
// a random block text, and BPE Comp / Decomp. Each side logs results and the bytes it produced.
//
// usage: diff_ks_ioslave.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
// KIO_SWAP: the sides swapped; KIO_DUMP: every log line.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
static std::string Hex(const std::string& b) { return Hex(b.data(), b.size()); }

using Node = unsigned char;
static std::string NText(const Node* n) { return n ? "'" + std::string(reinterpret_cast<const char*>(n + 0x18)) + "'" : "(null)"; }

// ---- calls through an object's vftable (slots: dtor, Seek, Tell, GetSize, ReadData, WriteData)
static void** Vt(void* o) { return *static_cast<void***>(o); }
static bool VSeek(void* o, unsigned long p) { return reinterpret_cast<bool(__fastcall*)(void*, void*, unsigned long)>(Vt(o)[1])(o, nullptr, p); }
static unsigned long VTell(void* o) { return reinterpret_cast<unsigned long(__fastcall*)(void*, void*)>(Vt(o)[2])(o, nullptr); }
static unsigned long VSize(void* o) { return reinterpret_cast<unsigned long(__fastcall*)(void*, void*)>(Vt(o)[3])(o, nullptr); }
static unsigned long VRead(void* o, void* d, unsigned long n) { return reinterpret_cast<unsigned long(__fastcall*)(void*, void*, void*, unsigned long)>(Vt(o)[4])(o, nullptr, d, n); }
static unsigned long VWrite(void* o, const void* s, unsigned long n) { return reinterpret_cast<unsigned long(__fastcall*)(void*, void*, const void*, unsigned long)>(Vt(o)[5])(o, nullptr, s, n); }
static void VDelete(void* o, unsigned flags) { reinterpret_cast<void*(__fastcall*)(void*, void*, unsigned)>(Vt(o)[0])(o, nullptr, flags); }

// ---- exports
struct Api {
    void* vtSlave;
    bool(__fastcall* writeByte)(void*, void*, unsigned char);
    bool(__fastcall* writeU32)(void*, void*, unsigned long);
    bool(__fastcall* readU32)(void*, void*, unsigned long*);
    unsigned long(__fastcall* writeCursor)(void*, void*, void*, unsigned long);
    void*(__fastcall* memCtor)(void*, void*, void*, unsigned long);
    void*(__fastcall* memCopy)(void*, void*, const void*);
    void*(__fastcall* memAssign)(void*, void*, const void*);
    void*(__fastcall* dynCtor)(void*, void*);
    void*(__fastcall* dynCopy)(void*, void*, const void*);
    void*(__fastcall* dynAssign)(void*, void*, const void*);
    const char*(__fastcall* dynData)(const void*, void*);
    void*(__fastcall* fileCtor)(void*, void*, void*);
    void*(__fastcall* fileCopy)(void*, void*, const void*);
    void*(__fastcall* rcCtor)(void*, void*);
    void*(__fastcall* rcCtorSlave)(void*, void*, void*);
    void*(__fastcall* rcCopy)(void*, void*, const void*);
    void(__fastcall* rcAssignSlave)(void*, void*, void*);
    void(__fastcall* rcAssign)(void*, void*, const void*);
    void(__fastcall* rcDtor)(void*, void*);
    void(__fastcall* rcInc)(void*, void*, unsigned long);
    void(__fastcall* rcPlus)(void*, void*, unsigned long);
    void(__fastcall* rcPre)(void*, void*);
    void(__fastcall* rcPost)(void*, void*, int);
    unsigned char(__fastcall* rcDeref)(void*, void*);
    unsigned char(__fastcall* rcIndex)(void*, void*, unsigned long);
    unsigned char(__fastcall* rcByte)(void*, void*, unsigned long);
    unsigned long(__fastcall* rcU32)(void*, void*, unsigned long);
    const void*(__fastcall* rcAccess)(void*, void*, unsigned long, unsigned long);
    int(__fastcall* rcIndexOf)(void*, void*, const void*);
    void*(__fastcall* wcCtor)(void*, void*, void*, unsigned long);
    bool(__fastcall* wcRead)(void*, void*, void*, int, unsigned long);
    unsigned long(__fastcall* wcCopy)(void*, void*, int, unsigned long);
    void*(__fastcall* resCtor)(void*, void*, void*, const void*, bool);
    void*(__fastcall* resCopy)(void*, void*, const void*);
    void*(__fastcall* resAssign)(void*, void*, const void*);
    void*(__fastcall* brCtorFile)(void*, void*, void*);
    void*(__fastcall* brCtor)(void*, void*);
    void(__fastcall* brDtor)(void*, void*);
    void(__cdecl* brInit)(void*, void*);
    void*(__cdecl* brBegin)(void*);
    void(__cdecl* brEnd)(void*);
    bool(__cdecl* brNext)(void*, Node**);
    bool(__cdecl* brReadStr)(void*, char*, int);
    bool(__cdecl* brReadPStr)(void*, Node**);
    int(__cdecl* brReadStrv)(void*, Node**, int);
    bool(__cdecl* brReadInt)(void*, int*);
    int(__cdecl* brReadIntv)(void*, int*, int);
    bool(__cdecl* brReadFloat)(void*, float*);
    int(__cdecl* brReadFloatv)(void*, float*, int);
    bool(__cdecl* brReadBool)(void*, bool*);
    unsigned long(__cdecl* bpeComp)(void*, const void*, unsigned long);
    unsigned long(__cdecl* bpeDecomp)(void*, const void*, unsigned long);
};
static Api apis[2];
#define RC "@IOSlaveReadCursor@Jet@@"
#define BR "@BlockReader@Jet@@"
static void Load(Api& a, HMODULE m)
{
    a.vtSlave = Get<void*>(m, "??_7IOSlave@Jet@@6B@");
    a.writeByte = Get<decltype(a.writeByte)>(m, "?WriteByte@IOSlave@Jet@@QAE_NE@Z");
    a.writeU32 = Get<decltype(a.writeU32)>(m, "?WriteUINT32@IOSlave@Jet@@QAE_NK@Z");
    a.readU32 = Get<decltype(a.readU32)>(m, "?ReadUINT32@IOSlave@Jet@@QAE_NAAK@Z");
    a.writeCursor = Get<decltype(a.writeCursor)>(m, "?Write@IOSlave@Jet@@QAEKAAVIOSlaveReadCursor@2@K@Z");
    a.memCtor = Get<decltype(a.memCtor)>(m, "??0IOSlaveMemory@Jet@@QAE@PAXK@Z");
    a.memCopy = Get<decltype(a.memCopy)>(m, "??0IOSlaveMemory@Jet@@QAE@ABV01@@Z");
    a.memAssign = Get<decltype(a.memAssign)>(m, "??4IOSlaveMemory@Jet@@QAEAAV01@ABV01@@Z");
    a.dynCtor = Get<decltype(a.dynCtor)>(m, "??0IOSlaveMemoryDynamic@Jet@@QAE@XZ");
    a.dynCopy = Get<decltype(a.dynCopy)>(m, "??0IOSlaveMemoryDynamic@Jet@@QAE@ABV01@@Z");
    a.dynAssign = Get<decltype(a.dynAssign)>(m, "??4IOSlaveMemoryDynamic@Jet@@QAEAAV01@ABV01@@Z");
    a.dynData = Get<decltype(a.dynData)>(m, "?GetData@IOSlaveMemoryDynamic@Jet@@QBEPBDXZ");
    a.fileCtor = Get<decltype(a.fileCtor)>(m, "??0IOSlaveFile@Jet@@QAE@PAVFile@1@@Z");
    a.fileCopy = Get<decltype(a.fileCopy)>(m, "??0IOSlaveFile@Jet@@QAE@ABV01@@Z");
    a.rcCtor = Get<decltype(a.rcCtor)>(m, "??0IOSlaveReadCursor@Jet@@QAE@XZ");
    a.rcCtorSlave = Get<decltype(a.rcCtorSlave)>(m, "??0IOSlaveReadCursor@Jet@@QAE@PAVIOSlave@1@@Z");
    a.rcCopy = Get<decltype(a.rcCopy)>(m, "??0IOSlaveReadCursor@Jet@@QAE@ABV01@@Z");
    a.rcAssignSlave = Get<decltype(a.rcAssignSlave)>(m, "??4IOSlaveReadCursor@Jet@@QAEXPAVIOSlave@1@@Z");
    a.rcAssign = Get<decltype(a.rcAssign)>(m, "??4IOSlaveReadCursor@Jet@@QAEXABV01@@Z");
    a.rcDtor = Get<decltype(a.rcDtor)>(m, "??1IOSlaveReadCursor@Jet@@QAE@XZ");
    a.rcInc = Get<decltype(a.rcInc)>(m, "?Inc" RC "QAEXK@Z");
    a.rcPlus = Get<decltype(a.rcPlus)>(m, "??YIOSlaveReadCursor@Jet@@QAEXK@Z");
    a.rcPre = Get<decltype(a.rcPre)>(m, "??EIOSlaveReadCursor@Jet@@QAEXXZ");
    a.rcPost = Get<decltype(a.rcPost)>(m, "??EIOSlaveReadCursor@Jet@@QAEXH@Z");
    a.rcDeref = Get<decltype(a.rcDeref)>(m, "??DIOSlaveReadCursor@Jet@@QAEEXZ");
    a.rcIndex = Get<decltype(a.rcIndex)>(m, "??AIOSlaveReadCursor@Jet@@QAEEK@Z");
    a.rcByte = Get<decltype(a.rcByte)>(m, "?AccessByte" RC "QAEEK@Z");
    a.rcU32 = Get<decltype(a.rcU32)>(m, "?AccessUInt32" RC "QAEKK@Z");
    a.rcAccess = Get<decltype(a.rcAccess)>(m, "?Access" RC "QAEPBXKK@Z");
    a.rcIndexOf = Get<decltype(a.rcIndexOf)>(m, "?GetIndex" RC "QAEHPBX@Z");
    a.wcCtor = Get<decltype(a.wcCtor)>(m, "??0IOSlaveWriteCursor@Jet@@QAE@PAVIOSlave@1@K@Z");
    a.wcRead = Get<decltype(a.wcRead)>(m, "?ReadData@IOSlaveWriteCursor@Jet@@QAE_NPAXHK@Z");
    a.wcCopy = Get<decltype(a.wcCopy)>(m, "?WriteCopy@IOSlaveWriteCursor@Jet@@QAEKHK@Z");
    a.resCtor = Get<decltype(a.resCtor)>(m, "??0IOSlaveResource@Jet@@QAE@PAVIOSlave@1@ABVString@1@_N@Z");
    a.resCopy = Get<decltype(a.resCopy)>(m, "??0IOSlaveResource@Jet@@QAE@ABV01@@Z");
    a.resAssign = Get<decltype(a.resAssign)>(m, "??4IOSlaveResource@Jet@@QAEAAV01@ABV01@@Z");
    a.brCtorFile = Get<decltype(a.brCtorFile)>(m, "??0BlockReader@Jet@@QAE@AAVFile@1@@Z");
    a.brCtor = Get<decltype(a.brCtor)>(m, "??0BlockReader@Jet@@QAE@XZ");
    a.brDtor = Get<decltype(a.brDtor)>(m, "??1BlockReader@Jet@@QAE@XZ");
    a.brInit = Get<decltype(a.brInit)>(m, "?Init" BR "QAAXAAVFile@2@@Z");
    a.brBegin = Get<decltype(a.brBegin)>(m, "?Begin" BR "QAAPAV12@XZ");
    a.brEnd = Get<decltype(a.brEnd)>(m, "?End" BR "QAAXXZ");
    a.brNext = Get<decltype(a.brNext)>(m, "?Next" BR "QAA_NAAVPString@2@@Z");
    a.brReadStr = Get<decltype(a.brReadStr)>(m, "?ReadString" BR "QAA_NPADH@Z");
    a.brReadPStr = Get<decltype(a.brReadPStr)>(m, "?ReadString" BR "QAA_NAAVPString@2@@Z");
    a.brReadStrv = Get<decltype(a.brReadStrv)>(m, "?ReadStringv" BR "QAAHPAVPString@2@H@Z");
    a.brReadInt = Get<decltype(a.brReadInt)>(m, "?ReadInt" BR "QAA_NAAH@Z");
    a.brReadIntv = Get<decltype(a.brReadIntv)>(m, "?ReadIntv" BR "QAAHPAHH@Z");
    a.brReadFloat = Get<decltype(a.brReadFloat)>(m, "?ReadFloat" BR "QAA_NAAM@Z");
    a.brReadFloatv = Get<decltype(a.brReadFloatv)>(m, "?ReadFloatv" BR "QAAHPAMH@Z");
    a.brReadBool = Get<decltype(a.brReadBool)>(m, "?ReadBool" BR "QAA_NAA_N@Z");
    a.bpeComp = Get<decltype(a.bpeComp)>(m, "?Comp@BPE@Jet@@SAKPAXPBXK@Z");
    a.bpeDecomp = Get<decltype(a.bpeDecomp)>(m, "?Decomp@BPE@Jet@@SAKPAXPBXK@Z");
}

// the original's own helpers (the same for both sides)
static void** pstringCache;
static void(__cdecl* pstringDestroy)(void*, Node*);
static void* (__fastcall* strCtorText)(void*, void*, const char*);
static void(__fastcall* strDtor)(void*, void*);
static void* (__fastcall* fCtor)(void*, void*);
static void(__fastcall* fDtor)(void*, void*);
static int* (__cdecl* fOpen)(void*, int*, const char*, unsigned long, int);
static unsigned long(__cdecl* fClose)(void*);
static void Release(Node* n)
{
    if (n) pstringDestroy(*pstringCache, n);
}

// ---- the generator (drawn once per scenario)
static std::mt19937 rng(4242);
static uint32_t R(uint32_t n) { return n ? rng() % n : 0; }
static uint32_t Len()
{
    static const uint32_t special[] = {0, 1, 2, 3, 4, 5, 15, 16, 17, 255, 256, 0x1000, 0x1001, 0x2000};
    return R(3) ? special[R(14)] : R(5000);
}
static std::string Bytes(uint32_t n, int kind)
{
    std::string s(n, '\0');
    for (uint32_t i = 0; i < n; ++i) {
        switch (kind) {
        case 0: s[i] = static_cast<char>(rng()); break;                  // noise
        case 1: s[i] = static_cast<char>("abcab"[R(5)]); break;         // few symbols (pairs repeat)
        case 2: s[i] = static_cast<char>(i % 7 == 0 ? R(256) : 'x'); break;
        default: s[i] = static_cast<char>((i * 31) >> 3); break;
        }
    }
    return s;
}

// bytes from a seed (no generator draw: used inside the per-side runs)
static std::string Pattern(uint32_t n, int seed)
{
    std::string s(n, '\0');
    uint32_t x = static_cast<uint32_t>(seed) * 2654435761u + n;
    for (uint32_t i = 0; i < n; ++i) {
        x = x * 1103515245u + 12345u;
        s[i] = static_cast<char>(seed & 1 ? x >> 24 : "abcab"[(x >> 16) % 5]);
    }
    return s;
}

struct Op {
    int kind;
    uint32_t a, b;
    int c;
    bool flag;
};
static std::vector<Op> Ops(int kinds, uint32_t n)
{
    std::vector<Op> v;
    for (uint32_t i = 0; i < n; ++i) v.push_back({static_cast<int>(R(kinds)), Len(), R(4) ? R(64) : Len(), static_cast<int>(R(8000)) - 6000, R(5) == 0});
    return v;
}

struct Scenario {
    std::string data;  // the memory / file content
    std::vector<Op> slaveOps, dynOps, cursorOps, writeOps;
    uint32_t chunk;
    std::string key;
    uint32_t keyWord;
    std::string resStream;  // a stream the original wrote (with keyWord set)
    std::string resPlain;
    std::string blockText;
    std::vector<Op> blockOps;
    std::string bpeData;
};
static Scenario sc;

static std::string tmpDir;
static std::string FileBytes(const std::string& path)
{
    std::string b;
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) b.append(buf, n);
        std::fclose(f);
    }
    return b;
}
static void PutFile(const std::string& path, const std::string& bytes)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}

// a scrambled stream written by a resource slave of `a` over growing memory, its key word set (+0x18) after the
// header; the bytes before the padding
static std::string WriteResource(const Api& a, const std::string& key, uint32_t keyWord, const std::string& plain,
                                 unsigned long* padding)
{
    alignas(8) unsigned char dyn[0x40], res[0x40], k[4];
    a.dynCtor(dyn, nullptr);
    strCtorText(k, nullptr, key.c_str());
    a.resCtor(res, nullptr, dyn, k, true);
    strDtor(k, nullptr);
    *reinterpret_cast<uint32_t*>(res + 0x18) = keyWord;
    size_t at = 0;
    while (at < plain.size()) {
        const size_t n = std::min<size_t>(plain.size() - at, 1 + (at * 7919 + plain.size()) % 3000);
        VWrite(res, plain.data() + at, static_cast<unsigned long>(n));
        at += n;
    }
    VDelete(res, 0);
    const char* const d = a.dynData(dyn, nullptr);
    std::string out = d ? std::string(d, VSize(dyn)) : std::string();
    VDelete(dyn, 0);
    // {version, key word ^ crc ^ size, size}, then the bytes, then the time-based padding
    const size_t body = 12 + plain.size();
    *padding = static_cast<unsigned long>(out.size() > body ? out.size() - body : 0);
    if (out.size() > body) out.resize(body);
    return out;
}

static std::string BlockText()
{
    static const char* const words[] = {"name", "kind", "value", "\"quoted string\"", "\"un", "42", "-7", "0x1f", "3.5", "1e3",
                                        "abc", "//c", "{", "}", "true", "0"};
    std::string t = "first 1 2\r\n";  // (a reader's buffers are unset heap until a line is read)
    int depth = 0;
    for (uint32_t i = 0, n = 1 + R(40); i < n; ++i) {
        switch (R(8)) {
        case 0: t += "\r\n"; break;
        case 1: t += "// comment\r\n"; break;
        case 2:
            t += "{\r\n";
            ++depth;
            break;
        case 3:
            if (depth || R(4) == 0) {
                t += "}\r\n";
                --depth;
            }
            break;
        default: {
            t += std::string(R(3), ' ') + (R(2) ? "\t" : "");
            for (uint32_t w = 0, k = 1 + R(5); w < k; ++w) t += std::string(w ? " " : "") + words[R(sizeof(words) / sizeof(words[0]))];
            t += R(6) ? "\r\n" : "\n";
            break;
        }
        }
    }
    return t;
}

static void Draw()
{
    sc.data = Bytes(Len() + 1, static_cast<int>(R(4)));
    sc.slaveOps = Ops(8, 1 + R(30));
    sc.dynOps = Ops(8, 1 + R(30));
    sc.cursorOps = Ops(12, 1 + R(40));
    sc.writeOps = Ops(5, 1 + R(30));
    static const uint32_t chunks[] = {0, 1, 7, 64, 0x1000};
    sc.chunk = chunks[R(5)];
    static const char* const keys[] = {"", "k", "key text", "a much longer resource key"};
    sc.key = keys[R(4)];
    sc.keyWord = rng();
    sc.resPlain = Bytes(R(3) ? R(600) : Len(), static_cast<int>(R(4)));
    unsigned long pad;
    sc.resStream = WriteResource(apis[1], sc.key, sc.keyWord, sc.resPlain, &pad);  // (apis[1] is the original unless swapped)
    sc.blockText = BlockText();
    sc.blockOps = Ops(12, 1 + R(40));
    sc.blockOps[0].kind = 0;  // (Next first: the line buffer is unset before)
    sc.bpeData = Bytes(R(3) ? R(3000) : Len(), static_cast<int>(R(4)));
}

// ---- one side
static std::string SlaveState(void* o) { return "tell " + Num(VTell(o)) + " size " + Num(VSize(o)); }

// the common slave operations (0..7)
static std::string SlaveOp(const Api& a, void* o, const Op& op, bool canWriteEmpty)
{
    std::string s = "op " + Num(op.kind) + " ";
    switch (op.kind) {
    case 0: s += Num(VSeek(o, op.a % (VSize(o) + 3))); break;
    case 1: {
        std::string d(op.b + 4, '\x5a');
        const unsigned long r = VRead(o, op.flag ? nullptr : &d[0], op.b);
        s += Num(r) + " " + Hex(d.data(), op.flag ? 0 : std::min<size_t>(r, d.size()));
        break;
    }
    case 2: {
        const std::string d = Pattern(op.b, op.c);
        if (!canWriteEmpty && op.b == 0) {
            s += "(no empty write)";
            break;
        }
        s += Num(VWrite(o, d.data(), op.b));
        break;
    }
    case 3: s += Num(a.writeByte(o, nullptr, static_cast<unsigned char>(op.c))); break;
    case 4: s += Num(a.writeU32(o, nullptr, static_cast<unsigned long>(op.c))); break;
    case 5: {
        unsigned long v = 0xeeeeeeee;
        const bool r = a.readU32(o, nullptr, &v);
        s += Num(r) + " " + Num(v);
        break;
    }
    case 6: s += Num(VSeek(o, 0)); break;
    default: s += Num(VSeek(o, VSize(o))); break;
    }
    return s + " | " + SlaveState(o);
}

static void Run()
{
    const Api& a = apis[side];
    // memory slave over a copy of the data
    {
        std::string data = sc.data;
        alignas(8) unsigned char m[0x40], c[0x40];
        a.memCtor(m, nullptr, &data[0], static_cast<unsigned long>(data.size()));
        Log("mem " + SlaveState(m));
        for (const Op& op : sc.slaveOps) Log("mem " + SlaveOp(a, m, op, true));
        a.memCopy(c, nullptr, m);
        Log("mem copy " + SlaveState(c) + " " + Num(Vt(c) == Vt(m)));
        a.memAssign(c, nullptr, m);
        VDelete(c, 0);
        VDelete(m, 0);
        Log("mem data " + Hex(data));
        alignas(8) unsigned char n[0x40];
        a.memCtor(n, nullptr, nullptr, 50);
        Log("mem null " + SlaveState(n) + " " + Num(VSeek(n, 1)));
        VDelete(n, 0);
    }
    // growing memory
    {
        alignas(8) unsigned char d[0x40], c[0x40], e[0x40];
        a.dynCtor(d, nullptr);
        Log("dyn " + SlaveState(d) + " " + Num(a.dynData(d, nullptr) != nullptr));
        for (const Op& op : sc.dynOps) {
            // (an empty write where the position is at the end is an invalid parameter in both)
            const bool atEnd = VTell(d) >= VSize(d);
            Log("dyn " + SlaveOp(a, d, op, !atEnd));
        }
        const char* p = a.dynData(d, nullptr);
        Log("dyn data " + (p ? Hex(p, VSize(d)) : std::string("(null)")));
        a.dynCopy(c, nullptr, d);
        p = a.dynData(c, nullptr);
        Log("dyn copy " + SlaveState(c) + " " + (p ? Hex(p, VSize(c)) : std::string("(null)")));
        a.dynCtor(e, nullptr);
        a.dynAssign(e, nullptr, d);
        p = a.dynData(e, nullptr);
        Log("dyn assigned " + SlaveState(e) + " " + (p ? Hex(p, VSize(e)) : std::string("(null)")));
        a.dynAssign(e, nullptr, e);
        VDelete(e, 0);
        VDelete(c, 0);
        VDelete(d, 0);
        // the deleting destructor through Jet::Alloc
        void* const h = a.dynCtor(Get<void* (__cdecl*)(unsigned)>(GetModuleHandleA("KernelStandard_7c_orig.dll"), "?New@Alloc@Jet@@SAPAXI@Z")(0x18), nullptr);
        VDelete(h, 1);
    }
    // file slave over a temporary file
    {
        const std::string path = tmpDir + "kio_file.bin";
        PutFile(path, sc.data);
        alignas(8) unsigned char f[0x200], s[0x40], c[0x40];
        fCtor(f, nullptr);
        int r = -7;
        fOpen(f, &r, path.c_str(), 0x10, 0);  // (read)
        a.fileCtor(s, nullptr, f);
        Log("file " + Num(r) + " " + SlaveState(s));
        for (const Op& op : sc.slaveOps)
            if (op.kind != 2 && op.kind != 3 && op.kind != 4) Log("file " + SlaveOp(a, s, op, true));
        a.fileCopy(c, nullptr, s);
        Log("file copy " + SlaveState(c));
        VDelete(c, 0);
        VDelete(s, 0);
        fClose(f);
        fDtor(f, nullptr);
        alignas(8) unsigned char z[0x40];
        a.fileCtor(z, nullptr, nullptr);
        Log("file none " + SlaveState(z) + " " + Num(VRead(z, nullptr, 5)) + " " + Num(VSeek(z, 0)));
        VDelete(z, 0);
        // writing
        fCtor(f, nullptr);
        DeleteFileA(path.c_str());
        fOpen(f, &r, path.c_str(), 0x101, 0);
        a.fileCtor(s, nullptr, f);
        for (const Op& op : sc.writeOps) {
            const std::string d = Pattern(op.b, op.c);
            Log("file write " + Num(VWrite(s, d.data(), op.b)) + " " + SlaveState(s));
        }
        VDelete(s, 0);
        fClose(f);
        fDtor(f, nullptr);
        Log("file written " + Hex(FileBytes(path)));
    }
    // read cursor over a memory slave
    {
        std::string data = sc.data;
        alignas(8) unsigned char m[0x40], rc[0x40], rc2[0x40];
        a.memCtor(m, nullptr, &data[0], static_cast<unsigned long>(data.size()));
        a.rcCtorSlave(rc, nullptr, m);
        const void* last = nullptr;
        for (const Op& op : sc.cursorOps) {
            std::string s = "cur " + Num(op.kind) + " ";
            switch (op.kind) {
            case 0: {
                const unsigned long off = op.a % 300, n = op.b;
                const void* p = a.rcAccess(rc, nullptr, off, n);
                s += p ? Hex(p, n) : std::string("(null)");
                last = p;
                break;
            }
            case 1: a.rcInc(rc, nullptr, op.b); break;
            case 2: a.rcPlus(rc, nullptr, op.b); break;
            case 3: a.rcPre(rc, nullptr); break;
            case 4: a.rcPost(rc, nullptr, 0); break;
            case 5: s += Num(a.rcDeref(rc, nullptr)); break;
            case 6: s += Num(a.rcIndex(rc, nullptr, op.a % 100)); break;
            case 7: s += Num(a.rcByte(rc, nullptr, op.a % 100)); break;
            case 8: s += Num(a.rcU32(rc, nullptr, op.a % 100)); break;
            case 9: s += Num(last ? a.rcIndexOf(rc, nullptr, last) : -1); break;
            case 10: {
                a.rcCopy(rc2, nullptr, rc);
                const void* p = a.rcAccess(rc2, nullptr, 0, 3);
                s += "copy " + (p ? Hex(p, 3) : std::string("(null)"));
                a.rcAssign(rc2, nullptr, rc);
                a.rcDtor(rc2, nullptr);
                break;
            }
            default: {  // the cursor's bytes into a growing slave
                alignas(8) unsigned char d[0x40];
                a.dynCtor(d, nullptr);
                const unsigned long w = a.writeCursor(d, nullptr, rc, op.b);
                const char* p = a.dynData(d, nullptr);
                s += "write " + Num(w) + " " + (p ? Hex(p, VSize(d)) : std::string("(null)"));
                VDelete(d, 0);
                last = nullptr;
                break;
            }
            }
            const unsigned* const f = reinterpret_cast<const unsigned*>(rc);
            Log(s + " | slave " + Num(f[0] != 0) + " pos " + Num(f[1]) + " off " + Num(f[3]) + " filled " + Num(f[4]) + " cap " + Num(f[5]) + " " +
                SlaveState(m));
        }
        a.rcAssignSlave(rc, nullptr, nullptr);  // (leaks the buffer, as in the original)
        a.rcDtor(rc, nullptr);
        VDelete(m, 0);
        a.rcCtor(rc2, nullptr);
        a.rcDtor(rc2, nullptr);
    }
    // write cursor over a growing slave
    {
        alignas(8) unsigned char d[0x40], wc[0x40];
        a.dynCtor(d, nullptr);
        a.wcCtor(wc, nullptr, d, sc.chunk);
        for (const Op& op : sc.writeOps) {
            std::string s = "wc " + Num(op.kind) + " ";
            switch (op.kind) {
            case 0: case 1: {
                if (!op.b) {  // (an empty write reaches the growing slave at its end: an invalid parameter in both)
                    s += "(no empty write)";
                    break;
                }
                const std::string b = Pattern(op.b, op.c);
                s += Num(VWrite(wc, b.data(), op.b));
                break;
            }
            case 2: {
                std::string out(op.b + 2, '\x33');
                const int at = op.c % 5000;
                const bool r = a.wcRead(wc, nullptr, &out[0], at, op.b);
                s += Num(r) + " " + (r ? Hex(out) : std::string());
                break;
            }
            case 3: s += Num(a.wcCopy(wc, nullptr, -static_cast<int>(1 + op.a % 300), op.b)); break;
            default: s += Num(a.wcCopy(wc, nullptr, op.c % 3000, op.b % 600)); break;
            }
            const unsigned* const f = reinterpret_cast<const unsigned*>(wc);
            Log(s + " | fill " + Num(f[5]) + " " + Num(f[6]) + " cur " + Num(f[7]) + " " + SlaveState(d) + " " + SlaveState(wc));
        }
        VDelete(wc, 0);
        const char* p = a.dynData(d, nullptr);
        Log("wc out " + (p ? Hex(p, VSize(d)) : std::string("(null)")));
        VDelete(d, 0);
    }
    // the resource stream: reading the original's, writing with the scenario's key word
    {
        std::string stream = sc.resStream;
        alignas(8) unsigned char m[0x40], res[0x40], k[4], c[0x40];
        a.memCtor(m, nullptr, &stream[0], static_cast<unsigned long>(stream.size()));
        strCtorText(k, nullptr, sc.key.c_str());
        a.resCtor(res, nullptr, m, k, false);
        strDtor(k, nullptr);
        const unsigned* const f = reinterpret_cast<const unsigned*>(res);
        Log("res read slave " + Num(f[1] != 0) + " base " + Num(f[2]) + " ver " + Num(f[5]) + " word " + Num(f[6]) + " " + SlaveState(res));
        std::string got;
        for (uint32_t i = 0; i < 64; ++i) {
            char buf[700];
            const unsigned long n = VRead(res, buf, 1 + (i * 97 + sc.resPlain.size()) % 700);
            got.append(buf, n);
            if (!n) break;
            if (i % 9 == 4) Log("res seek " + Num(VSeek(res, (i * 53) % (VSize(res) + 10))) + " " + SlaveState(res));
        }
        Log("res got " + Hex(got));
        {
            const bool past = VSeek(res, VSize(res) + 5);
            const unsigned long at = VTell(res);
            const bool end = VSeek(res, VSize(res));
            Log("res seek past " + Num(past) + " " + Num(at) + " end " + Num(end) + " " + SlaveState(res));
        }
        a.resCopy(c, nullptr, res);
        Log("res copy " + SlaveState(c));
        a.resAssign(c, nullptr, res);
        VDelete(c, 0);
        VDelete(res, 0);
        VDelete(m, 0);
        unsigned long pad = 0;
        const std::string mine = WriteResource(a, sc.key, sc.keyWord, sc.resPlain, &pad);
        Log("res written " + Hex(mine) + " pad " + Num(pad));
    }
    // BlockReader over the block text
    {
        const std::string path = tmpDir + "kio_block.txt";
        PutFile(path, sc.blockText);
        alignas(8) unsigned char f[0x200], br[0x40];
        fCtor(f, nullptr);
        int r = -7;
        fOpen(f, &r, path.c_str(), 0x10, 0);
        if (sc.blockOps.size() % 2) {
            a.brCtorFile(br, nullptr, f);
        } else {
            a.brCtor(br, nullptr);
            a.brInit(br, f);
        }
        // File::ReadLine puts no NUL after the line, so the scans run on into what earlier lines left, and past
        // them into the buffers' unset heap: zeroed here, the same on both sides
        std::memset(*reinterpret_cast<char**>(br + 4), 0, 0x200);
        std::memset(*reinterpret_cast<char**>(br + 8), 0, 0x200);
        std::vector<void*> stack{br};
        for (const Op& op : sc.blockOps) {
            void* const cur = stack.back();
            std::string s = "br " + Num(op.kind) + " ";
            switch (op.kind) {
            case 0: case 1: case 2: {
                Node* name = nullptr;
                const bool ok = a.brNext(cur, &name);
                s += Num(ok) + " " + NText(name);
                Release(name);
                break;
            }
            case 3: {
                void* const ch = a.brBegin(cur);
                s += Num(ch != nullptr);
                if (ch) stack.push_back(ch);
                break;
            }
            case 4:
                if (stack.size() > 1) {
                    stack.pop_back();
                    a.brEnd(stack.back());
                    s += "end";
                }
                break;
            case 5: {
                char out[32];
                std::memset(out, 0x5a, sizeof(out));
                const int size = static_cast<int>(op.a % 33);
                const bool ok = a.brReadStr(cur, out, size);
                s += Num(ok) + " " + (ok ? std::string(out) : std::string());
                break;
            }
            case 6: {
                Node* p = nullptr;
                const bool ok = a.brReadPStr(cur, &p);
                s += Num(ok) + " " + NText(p);
                Release(p);
                break;
            }
            case 7: {
                Node* v[6] = {};
                const int n = a.brReadStrv(cur, v, static_cast<int>(op.a % 7));
                s += Num(n);
                for (int i = 0; i < 6; ++i) {
                    s += " " + NText(v[i]);
                    Release(v[i]);
                }
                break;
            }
            case 8: {
                int v = -12345;
                const bool ok = a.brReadInt(cur, &v);
                s += Num(ok) + " " + Num(v);
                break;
            }
            case 9: {
                int v[6] = {-1, -1, -1, -1, -1, -1};
                const int n = a.brReadIntv(cur, v, static_cast<int>(op.a % 7));
                s += Num(n);
                for (int i = 0; i < 6; ++i) s += " " + Num(v[i]);
                break;
            }
            case 10: {
                float v[4] = {-1, -1, -1, -1};
                const bool ok = a.brReadFloat(cur, &v[0]);
                const int n = a.brReadFloatv(cur, v + 1, 3);
                s += Num(ok) + " " + Num(n) + " " + Hex(v, sizeof(v));
                break;
            }
            default: {
                bool v = true;
                const bool ok = a.brReadBool(cur, &v);
                s += Num(ok) + " " + Num(v);
                break;
            }
            }
            const unsigned char* const b = static_cast<const unsigned char*>(cur);
            Log(s + " | ended " + Num(b[0x20]) + " pending " + Num(b[0x21]) + " depth " + Num(stack.size()));
        }
        while (stack.size() > 1) {
            stack.pop_back();
            a.brEnd(stack.back());
        }
        a.brDtor(br, nullptr);
        fClose(f);
        fDtor(f, nullptr);
    }
    // BPE
    {
        const std::string& in = sc.bpeData;
        std::string out(in.size() + 0x1000, '\0');
        const unsigned long n = a.bpeComp(&out[0], in.data(), static_cast<unsigned long>(in.size()));
        Log("bpe comp " + Num(n) + " " + Hex(out.data(), n));
        if (n < in.size()) {
            std::string back(in.size() + 0x1000, '\0');
            const unsigned long m = a.bpeDecomp(&back[0], out.data(), n);
            Log("bpe decomp " + Num(m) + " " + Num(m == in.size() && std::memcmp(back.data(), in.data(), m) == 0) + " " + Hex(back.data(), m));
        }
    }
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
        std::printf("usage: diff_ks_ioslave <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 2000;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    HMODULE orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    const bool swap = getenv("KIO_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours);
    Load(apis[1], swap ? ours : orig);
    pstringCache = Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringDestroy = Get<decltype(pstringDestroy)>(orig, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    strCtorText = Get<decltype(strCtorText)>(orig, "??0String@Jet@@QAE@PBD@Z");
    strDtor = Get<decltype(strDtor)>(orig, "??1String@Jet@@QAE@XZ");
    fCtor = Get<decltype(fCtor)>(orig, "??0File@Jet@@QAE@XZ");
    fDtor = Get<decltype(fDtor)>(orig, "??1File@Jet@@QAE@XZ");
    fOpen = Get<decltype(fOpen)>(orig, "?Open@File@Jet@@QAA?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@PBDKH@Z");
    fClose = Get<decltype(fClose)>(orig, "?Close@File@Jet@@QAAKXZ");
    {
        char t[MAX_PATH];
        GetTempPathA(MAX_PATH, t);
        tmpDir = t;
        SetCurrentDirectoryA(tmpDir.c_str());
    }
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
                    std::printf("scenario %d line %zu:\n  ours %s\n  orig %s\n", n, i, x.substr(0, 1200).c_str(), y.substr(0, 1200).c_str());
                break;
            }
        }
        if (getenv("KIO_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", n, l.substr(0, 300).c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
