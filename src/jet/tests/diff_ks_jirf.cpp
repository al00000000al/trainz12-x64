// Differential test of KernelStandard unit U9c (JIRF.cpp). Each scenario draws, once, a chunk tree (headers with
// ids, blocks with data, padded PStrings and Strings, raw words, patched sizes) and a key. Each side writes it to a
// file through its JIRFFile and the size writers (the resource's key word set by the test after OpenJIRF; the
// time-based padding at the end not compared), reads its own file back following the tree, and reads the file the
// original wrote the same way. Reads only go where the tree has data (a short read leaves stack garbage in both).
//
// usage: diff_ks_jirf.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
// KJI_SWAP: the sides swapped; KJI_DUMP: every log line.
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
using Node = unsigned char;
static std::string NText(const Node* n) { return n ? "'" + std::string(reinterpret_cast<const char*>(n + 0x18)) + "'" : "(null)"; }
static std::string AText(const void* s)
{
    const unsigned* const b = *static_cast<const unsigned* const*>(s);
    return "[" + std::to_string(b[0] != 0) + " " + std::to_string(b[1]) + " '" + std::string(reinterpret_cast<const char*>(b + 2), b[1]) + "']";
}

#define RES "?AV?$ResultClass@$1?results@JIRFFile@Jet@@2QBUResultTable@2@BW4Results@JIRFFile@Jet@@@2@"
#define JF "@JIRFFile@Jet@@"
struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void*(__fastcall* copy)(void*, void*, const void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    void(__fastcall* dtor)(void*, void*);
    int*(__cdecl* open)(void*, int*, const char*, const void*, unsigned long);
    int*(__cdecl* openBase)(void*, int*, Node* const*, const char*, const void*, unsigned long);
    void(__cdecl* close)(void*);
    unsigned long(__cdecl* write)(void*, const void*, unsigned long);
    unsigned long(__cdecl* read)(void*, void*, unsigned long);
    bool(__cdecl* read32)(void*, unsigned*);
    unsigned long(__cdecl* seekCur)(void*, long);
    unsigned long(__cdecl* seek)(void*, long);
    unsigned long(__cdecl* tell)(const void*);
    unsigned(__cdecl* writeHeader)(void*, unsigned long);
    unsigned(__cdecl* writeBlock)(void*, unsigned long, unsigned, void*);
    int*(__cdecl* readBlock)(void*, int*, unsigned long, unsigned*, void*);
    int*(__cdecl* readBlockHeader)(void*, int*, unsigned long*, unsigned*);
    int*(__cdecl* readHeader)(void*, int*, unsigned long);
    int*(__cdecl* readHeaderSize)(void*, int*, unsigned long, unsigned*);
    void(__cdecl* writeAt)(void*, unsigned, unsigned long);
    void(__cdecl* writeAtHere)(void*, unsigned);
    void(__cdecl* writeSizeAt)(void*, unsigned);
    void(__cdecl* writePaddedP)(void*, Node* const*);
    void(__cdecl* writePaddedS)(void*, const void*);
    int*(__cdecl* readPaddedP)(void*, int*, Node**);
    int*(__cdecl* readPaddedS)(void*, int*, void*);
    void*(__fastcall* whCtor)(void*, void*, void*, unsigned long);
    void(__fastcall* whDtor)(void*, void*);
    void*(__fastcall* wbCtor)(void*, void*, void*, unsigned long);
    void(__fastcall* wbDtor)(void*, void*);
};
static Api apis[2];
static void Load(Api& a, HMODULE m)
{
    a.ctor = Get<decltype(a.ctor)>(m, "??0JIRFFile@Jet@@QAE@XZ");
    a.copy = Get<decltype(a.copy)>(m, "??0JIRFFile@Jet@@QAE@ABV01@@Z");
    a.assign = Get<decltype(a.assign)>(m, "??4JIRFFile@Jet@@QAEAAV01@ABV01@@Z");
    a.dtor = Get<decltype(a.dtor)>(m, "??1JIRFFile@Jet@@QAE@XZ");
    a.open = Get<decltype(a.open)>(m, "?OpenJIRF" JF "QAA" RES "PBDABVString@2@K@Z");
    a.openBase = Get<decltype(a.openBase)>(m, "?OpenJIRF" JF "QAA" RES "ABVPString@2@PBDABVString@2@K@Z");
    a.close = Get<decltype(a.close)>(m, "?Close" JF "QAAXXZ");
    a.write = Get<decltype(a.write)>(m, "?Write" JF "QAAKPBXK@Z");
    a.read = Get<decltype(a.read)>(m, "?Read" JF "QAAKPAXK@Z");
    a.read32 = Get<decltype(a.read32)>(m, "?Read32" JF "QAA_NAAI@Z");
    a.seekCur = Get<decltype(a.seekCur)>(m, "?SeekCur" JF "QAAKJ@Z");
    a.seek = Get<decltype(a.seek)>(m, "?Seek" JF "QAAKJ@Z");
    a.tell = Get<decltype(a.tell)>(m, "?Tell" JF "QBAKXZ");
    a.writeHeader = Get<decltype(a.writeHeader)>(m, "?WriteHeader" JF "QAAIK@Z");
    a.writeBlock = Get<decltype(a.writeBlock)>(m, "?WriteBlock" JF "QAAIKIPAX@Z");
    a.readBlock = Get<decltype(a.readBlock)>(m, "?ReadBlock" JF "QAA" RES "KAAIPAX@Z");
    a.readBlockHeader = Get<decltype(a.readBlockHeader)>(m, "?ReadBlockHeader" JF "QAA" RES "AAKAAI@Z");
    a.readHeader = Get<decltype(a.readHeader)>(m, "?ReadHeader" JF "QAA" RES "K@Z");
    a.readHeaderSize = Get<decltype(a.readHeaderSize)>(m, "?ReadHeader" JF "QAA" RES "KAAI@Z");
    a.writeAt = Get<decltype(a.writeAt)>(m, "?WriteAt" JF "QAAXIK@Z");
    a.writeAtHere = Get<decltype(a.writeAtHere)>(m, "?WriteAt" JF "QAAXI@Z");
    a.writeSizeAt = Get<decltype(a.writeSizeAt)>(m, "?WriteSizeAt" JF "QAAXI@Z");
    a.writePaddedP = Get<decltype(a.writePaddedP)>(m, "?WritePadded" JF "QAAXABVPString@2@@Z");
    a.writePaddedS = Get<decltype(a.writePaddedS)>(m, "?WritePadded" JF "QAAXABVString@2@@Z");
    a.readPaddedP = Get<decltype(a.readPaddedP)>(m, "?ReadPadded" JF "QAA" RES "AAVPString@2@@Z");
    a.readPaddedS = Get<decltype(a.readPaddedS)>(m, "?ReadPadded" JF "QAA" RES "AAVString@2@@Z");
    a.whCtor = Get<decltype(a.whCtor)>(m, "??0JIRFWriteHeader@Jet@@QAE@AAVJIRFFile@1@K@Z");
    a.whDtor = Get<decltype(a.whDtor)>(m, "??1JIRFWriteHeader@Jet@@QAE@XZ");
    a.wbCtor = Get<decltype(a.wbCtor)>(m, "??0JIRFWriteBlock@Jet@@QAE@AAVJIRFFile@1@K@Z");
    a.wbDtor = Get<decltype(a.wbDtor)>(m, "??1JIRFWriteBlock@Jet@@QAE@XZ");
}

static void** pstringCache;
static Node* (__cdecl* pstringCreate)(void*, const unsigned char*);
static void(__cdecl* pstringDestroy)(void*, Node*);
static void* (__fastcall* strCtor)(void*, void*);
static void* (__fastcall* strCtorText)(void*, void*, const char*);
static void(__fastcall* strDtor)(void*, void*);
static void Release(Node* n)
{
    if (n) pstringDestroy(*pstringCache, n);
}
static Node* Make(const char* t) { return t ? pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(t)) : nullptr; }

// ---- the generator
static std::mt19937 rng(6161);
static uint32_t R(uint32_t n) { return n ? rng() % n : 0; }

// a chunk tree as a list of steps
enum Kind { HEADER_OPEN, HEADER_CLOSE, BLOCK, BLOCK_OBJ, PSTRING, STRING, WORD32, RAW };
struct Step {
    Kind kind;
    uint32_t id;
    std::string data;  // block data, string text
    int text;          // 0 none (PString), 1 empty, 2 text
    bool byObject;     // the header written through JIRFWriteHeader (else WriteHeader + WriteSizeAt)
};
struct Scenario {
    std::vector<Step> steps;
    std::string key;
    uint32_t keyWord;
    std::string origFile;  // the original's file (before the padding)
};
static Scenario sc;

static void Draw()
{
    sc.steps.clear();
    int open = 0;
    for (uint32_t i = 0, n = 1 + R(25); i < n; ++i) {
        Step s{};
        s.id = R(3) ? 0x41000000u + R(26) : rng();
        switch (R(9)) {
        case 0: case 1:
            s.kind = HEADER_OPEN;
            s.byObject = R(2) != 0;
            ++open;
            break;
        case 2:
            if (!open) continue;
            s.kind = HEADER_CLOSE;
            --open;
            break;
        case 3: case 4: {
            s.kind = R(3) ? BLOCK : BLOCK_OBJ;
            const uint32_t len = R(4) ? R(64) : R(3000);
            for (uint32_t k = 0; k < len; ++k) s.data += static_cast<char>(rng());
            break;
        }
        case 5: case 6: {
            s.kind = R(2) ? PSTRING : STRING;
            s.text = static_cast<int>(R(3));
            static const char* const texts[] = {"a", "abc", "four", "a longer padded string", "x y z"};
            if (s.text == 2) s.data = texts[R(5)];
            break;
        }
        case 7: s.kind = WORD32; break;
        default:
            s.kind = RAW;
            for (uint32_t k = 0, len = 1 + R(9); k < len; ++k) s.data += static_cast<char>(rng());
            break;
        }
        sc.steps.push_back(s);
    }
    while (open--) {
        Step s{};
        s.kind = HEADER_CLOSE;
        sc.steps.push_back(s);
    }
    static const char* const keys[] = {"", "key", "another key"};
    sc.key = keys[R(3)];
    sc.keyWord = rng();
}

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

// the scenario's file written by `a`; the bytes before the resource's padding
static std::string WriteFile(const Api& a, const std::string& path)
{
    DeleteFileA(path.c_str());
    alignas(8) unsigned char f[0x200], k[4];
    a.ctor(f, nullptr);
    strCtorText(k, nullptr, sc.key.c_str());
    int r = -7;
    a.open(f, &r, path.c_str(), k, 0x101);
    strDtor(k, nullptr);
    Log("open write " + Num(r));
    unsigned char* const res = *reinterpret_cast<unsigned char**>(f + 0x140);
    *reinterpret_cast<uint32_t*>(res + 0x18) = sc.keyWord;
    struct Open {
        bool byObject;
        unsigned pos;
        alignas(8) unsigned char obj[16];
    };
    std::vector<Open> stack;
    for (const Step& s : sc.steps) {
        std::string l = "w " + Num(s.kind) + " ";
        switch (s.kind) {
        case HEADER_OPEN: {
            Open o{s.byObject, 0, {}};
            if (s.byObject) a.whCtor(o.obj, nullptr, f, s.id);
            else o.pos = a.writeHeader(f, s.id);
            l += Num(o.byObject ? *reinterpret_cast<unsigned*>(o.obj + 4) : o.pos);
            stack.push_back(o);
            break;
        }
        case HEADER_CLOSE: {
            Open& o = stack.back();
            if (o.byObject) a.whDtor(o.obj, nullptr);
            else a.writeSizeAt(f, o.pos);
            stack.pop_back();
            break;
        }
        case BLOCK: {
            std::string d = s.data;
            l += Num(a.writeBlock(f, s.id, static_cast<unsigned>(d.size()), d.empty() ? nullptr : &d[0]));
            break;
        }
        case BLOCK_OBJ: {
            alignas(8) unsigned char o[16];
            a.wbCtor(o, nullptr, f, s.id);
            l += Num(a.write(f, s.data.data(), static_cast<unsigned long>(s.data.size())));
            a.wbDtor(o, nullptr);
            break;
        }
        case PSTRING: {
            Node* p = s.text == 0 ? nullptr : Make(s.text == 1 ? "" : s.data.c_str());
            a.writePaddedP(f, &p);
            Release(p);
            break;
        }
        case STRING: {
            alignas(4) unsigned char t[4];
            if (s.text == 0) strCtor(t, nullptr);
            else strCtorText(t, nullptr, s.text == 1 ? "" : s.data.c_str());
            a.writePaddedS(f, t);
            strDtor(t, nullptr);
            break;
        }
        case WORD32: {
            const unsigned at = static_cast<unsigned>(a.tell(f));
            unsigned long w = s.id;
            a.write(f, &w, 4);
            a.writeAt(f, at, s.id ^ 0x5a5a5a5au);
            break;
        }
        default: l += Num(a.write(f, s.data.data(), static_cast<unsigned long>(s.data.size()))); break;
        }
        Log(l + " tell " + Num(a.tell(f)));
    }
    const unsigned long size = *reinterpret_cast<uint32_t*>(res + 0x20);
    a.close(f);
    a.dtor(f, nullptr);
    std::string b = FileBytes(path);
    if (!sc.key.empty() && b.size() > 12 + size) b.resize(12 + size);
    return b;
}

// the file read back along the steps
static void ReadFile(const Api& a, const std::string& path, const char* what)
{
    alignas(8) unsigned char f[0x200], k[4];
    a.ctor(f, nullptr);
    strCtorText(k, nullptr, sc.key.c_str());
    int r = -7;
    if (path.find('\\') != std::string::npos && what[0] == 'b') {
        Node* base = Make("kji");
        a.openBase(f, &r, &base, path.substr(path.rfind('\\') + 1).c_str(), k, 0x10);
        Release(base);
    } else {
        a.open(f, &r, path.c_str(), k, 0x10);
    }
    strDtor(k, nullptr);
    Log(std::string(what) + " open " + Num(r) + " tell " + Num(a.tell(f)));
    int depth = 0;
    for (const Step& s : sc.steps) {
        std::string l = std::string(what) + " " + Num(s.kind) + " ";
        int res = -7;
        switch (s.kind) {
        case HEADER_OPEN: {
            unsigned size = 0xeeee;
            if (depth++ & 1) a.readHeader(f, &res, s.id);
            else a.readHeaderSize(f, &res, s.id ^ (s.byObject ? 0 : 1), &size);
            l += Num(res) + " " + Num(size);
            break;
        }
        case HEADER_CLOSE: --depth; break;
        case BLOCK: case BLOCK_OBJ: {
            if (s.kind == BLOCK_OBJ && s.data.size() % 3 == 0) {
                unsigned long id = 0;
                unsigned size = 0;
                a.readBlockHeader(f, &res, &id, &size);
                l += Num(res) + " " + Num(id) + " " + Num(size) + " skip " + Num(a.seekCur(f, static_cast<long>(size)));
                break;
            }
            std::string d(s.data.size() + 8, '\x11');
            unsigned size = static_cast<unsigned>(s.data.size() / 2 + (s.data.size() % 2) * 3);
            a.readBlock(f, &res, s.id ^ (s.data.size() % 5 == 1 ? 1 : 0), &size, s.data.size() % 4 == 2 ? nullptr : &d[0]);
            l += Num(res) + " " + Num(size) + " " + Hex(d.data(), d.size());
            // (a wrong id, or no buffer, leaves the data unread)
            if (res == 3 || s.data.size() % 4 == 2) l += " skip " + Num(a.seekCur(f, static_cast<long>(size)));
            break;
        }
        case PSTRING: {
            Node* p = Make("old");
            a.readPaddedP(f, &res, &p);
            l += Num(res) + " " + NText(p);
            Release(p);
            break;
        }
        case STRING: {
            alignas(4) unsigned char t[4];
            strCtorText(t, nullptr, "old");
            a.readPaddedS(f, &res, t);
            l += Num(res) + " " + AText(t);
            strDtor(t, nullptr);
            break;
        }
        case WORD32: {
            unsigned v = 0;
            l += Num(a.read32(f, &v)) + " " + Num(v);
            break;
        }
        default: {
            std::string d(s.data.size(), '\0');
            l += Num(a.read(f, &d[0], static_cast<unsigned long>(d.size()))) + " " + Hex(d.data(), d.size());
            break;
        }
        }
        Log(l + " tell " + Num(a.tell(f)));
    }
    Log(std::string(what) + " seek " + Num(a.seek(f, 4)) + " " + Num(a.seek(f, 0x7fffffff)) + " " + Num(a.seekCur(f, -3)));
    // a copy shares the slaves (it is let go without its destructor, which would close them twice)
    alignas(8) unsigned char c[0x200];
    a.copy(c, nullptr, f);
    Log(std::string(what) + " copy " + Num(a.tell(c)) + " " + Num(std::memcmp(c, f, 0x28) == 0 && ((c[0x28] ^ f[0x28]) & 3) == 0 && std::memcmp(c + 0x2c, f + 0x2c, 12) == 0) + " " + Num(std::strcmp(reinterpret_cast<char*>(c + 0x38), reinterpret_cast<char*>(f + 0x38)) == 0));
    alignas(8) unsigned char e[0x200];
    a.ctor(e, nullptr);
    a.assign(e, nullptr, f);
    Log(std::string(what) + " assigned " + Num(a.tell(e)));
    a.close(f);
    a.dtor(f, nullptr);
}

static void Run()
{
    const Api& a = apis[side];
    const std::string path = tmpDir + "kji_" + Num(side) + ".jirf";
    const std::string mine = WriteFile(a, path);
    Log("file " + Hex(mine.data(), mine.size()));
    ReadFile(a, path, "own");
    const std::string orig = tmpDir + "kji_orig.jirf";
    ReadFile(a, orig, "orig");
    ReadFile(a, orig, "base");
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
        std::printf("usage: diff_ks_jirf <dir> [scenarios]\n");
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
    const bool swap = getenv("KJI_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours);
    Load(apis[1], swap ? ours : orig);
    pstringCache = Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringCreate = Get<decltype(pstringCreate)>(orig, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringDestroy = Get<decltype(pstringDestroy)>(orig, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    strCtor = Get<decltype(strCtor)>(orig, "??0String@Jet@@QAE@XZ");
    strCtorText = Get<decltype(strCtorText)>(orig, "??0String@Jet@@QAE@PBD@Z");
    strDtor = Get<decltype(strDtor)>(orig, "??1String@Jet@@QAE@XZ");
    Api& origApi = swap ? apis[0] : apis[1];
    {
        char t[MAX_PATH];
        GetTempPathA(MAX_PATH, t);
        tmpDir = t;
        SetCurrentDirectoryA(tmpDir.c_str());
        Node* drive = Make("kji");
        Get<void(__cdecl*)(Node* const*, const char*, bool)>(orig, "?Subst@File@Jet@@SAXABVPString@2@PBD_N@Z")(&drive, tmpDir.c_str(), true);
        Release(drive);
    }
    unsigned long long checks = 0, bad = 0;
    for (int n = 0; n < scenarios; ++n) {
        Draw();
        side = 0;
        WriteFile(origApi, tmpDir + "kji_orig.jirf");  // (its log lines are dropped)
        logs[0].clear();
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
        if (getenv("KJI_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", n, l.substr(0, 300).c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
