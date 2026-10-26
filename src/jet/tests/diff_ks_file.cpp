// Differential test of KernelStandard unit U10a (FilePath.cpp, File.cpp, MemoryFile.cpp). Each scenario draws, once,
// path strings and file operation scripts, then runs them on our module's objects and on the original's:
//  * File::Path: every member and static over random paths (separators, drives, dots, wildcards, ';', non-ASCII,
//    lengths around 0x102 / 0x104), the whole 0x104-byte buffer compared after each call;
//  * File::Time and File::Entry;
//  * File: open modes (read, plain / compressed read, create, append, new-only, compressed write, rejected),
//    reads and writes around the 16 KB buffer, seeks (also inside BPE-compressed files), ReadLine / WriteLine in
//    ANSI and UTF-16, blocks, DetectEncoding, Commit, stamps, the errors thrown, the file bytes left on disk;
//  * File through substitutes (each side's own list, U10b): a directory and a new .ja archive written and read
//    back;
//  * MemoryFile: every Open, the typed reads, seeks, Write, Read(PString&).
//
// usage: diff_ks_file.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
// KFD_SWAP: the sides swapped; KFD_DUMP: every log line.
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
static unsigned long Fnv(const void* p, size_t n)
{
    unsigned long h = 2166136261u;
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 16777619u;
    return h;
}

using Node = unsigned char;
#define P "@Path@File@Jet@@"
#define F "@File@Jet@@"
#define M "@MemoryFile@Jet@@"
#define RES "?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@"

struct Api {
    // Path
    void(__fastcall* pCtor)(void*, void*);
    void(__fastcall* pCtorS)(void*, void*, const char*);
    void(__fastcall* pCtorP)(void*, void*, const void*);
    void(__cdecl* pAssignS)(void*, const char*);
    void(__cdecl* pAssignP)(void*, const void*);
    bool(__fastcall* pEq)(const void*, void*, const void*);
    bool(__fastcall* pEqS)(const void*, void*, const char*);
    bool(__fastcall* pNe)(const void*, void*, const void*);
    bool(__fastcall* pNeS)(const void*, void*, const char*);
    bool(__fastcall* pLess)(const void*, void*, const void*);
    bool(__fastcall* pBool)(const void*, void*);
    bool(__fastcall* pNot)(const void*, void*);
    const char*(__cdecl* pChars)(const void*);
    void(__cdecl* pAdd)(void*, const char*);
    void(__cdecl* pAddN)(void*, const char*, int);
    void(__cdecl* pAddPath)(void*, const char*);
    void(__cdecl* pAddPathN)(void*, const char*, int);
    void(__cdecl* pAddSlash)(void*);
    void(__cdecl* pDel)(void*, unsigned, unsigned);
    void(__cdecl* pDelDrive)(void*);
    void(__cdecl* pDelFileName)(void*);
    void(__cdecl* pDelName)(void*, bool);
    void(__cdecl* pDelPath)(void*);
    void(__cdecl* pDelSlash)(void*);
    const char*(__cdecl* pGetExt)(const void*);
    const char*(__cdecl* pGetExtS)(const char*);
    const char*(__cdecl* pGetFileName)(const void*);
    const char*(__cdecl* pGetFileNameS)(const char*);
    const char*(__cdecl* pGetPath)(const void*);
    unsigned(__cdecl* pHash)(const void*);
    bool(__cdecl* pValid)(const void*);
    bool(__cdecl* pIsEqual)(const char*, const char*);
    bool(__cdecl* pIsExt)(const void*, const char*);
    bool(__cdecl* pIsMatch)(const void*, const char*, const char**);
    bool(__cdecl* pIsMatchS)(const char*, const char*, const char**);
    unsigned(__cdecl* pLast)(const void*);
    unsigned(__cdecl* pLength)(const void*);
    void(__cdecl* pMake)(void*, const char*, const char*);
    bool(__cdecl* pRelative)(void*, const char*);
    void(__cdecl* pNormalize)(void*);
    void(__cdecl* pPrevDir)(void*);
    void(__cdecl* pBack)(void*);
    void(__cdecl* pFwd)(void*);
    void(__cdecl* pRootDir)(void*);
    void(__cdecl* pSetExt)(void*, const char*);
    void(__cdecl* pSplit)(const void*, char*, char*, char*, char*);
    void(__cdecl* pLower)(void*);
    void(__cdecl* pUpper)(void*);
    void*(__cdecl* pFirst)(void*, void*);
    // Time, Entry
    bool(__cdecl* tEq)(const void*, const void*);
    bool(__cdecl* tLt)(const void*, const void*);
    bool(__cdecl* tGt)(const void*, const void*);
    bool(__cdecl* tLe)(const void*, const void*);
    bool(__cdecl* tGe)(const void*, const void*);
    void(__cdecl* tSet)(void*, long);
    void(__fastcall* eCtor)(void*, void*);
    void(__fastcall* eCopy)(void*, void*, const void*);
    void*(__cdecl* eAssign)(void*, const void*);
    bool(__cdecl* eDir)(void*);
    bool(__cdecl* eCompressed)(void*);
    bool(__cdecl* eHidden)(void*);
    // File
    void(__fastcall* fCtor)(void*, void*);
    void(__fastcall* fDtor)(void*, void*);
    int*(__cdecl* fOpen)(void*, int*, const char*, unsigned long, int);
    int*(__cdecl* fOpenB)(void*, int*, const Node**, const char*, unsigned long, int);
    unsigned long(__cdecl* fClose)(void*);
    unsigned long(__cdecl* fRead)(void*, void*, unsigned long, unsigned long);
    unsigned long(__cdecl* fWrite)(void*, const void*, unsigned long, unsigned long);
    bool(__cdecl* fRead8)(void*, __int64*);
    bool(__cdecl* fRead32)(void*, int*);
    bool(__cdecl* fReadS)(void*, short*);
    bool(__cdecl* fWrite8)(void*, __int64);
    bool(__cdecl* fWrite32)(void*, int);
    bool(__cdecl* fWriteS)(void*, short);
    bool(__cdecl* fWriteC)(void*, signed char);
    void(__cdecl* fSafeRead)(void*, void*, unsigned long, unsigned long);
    unsigned long(__cdecl* fReadLine)(void*, void*, unsigned long, int);
    unsigned long(__cdecl* fWriteLine)(void*, const char*, int);
    unsigned long(__cdecl* fReadBlock)(void*, const char*, void*, unsigned long);
    unsigned long(__cdecl* fWriteBlock)(void*, const char*, const void*, unsigned long);
    unsigned long(__cdecl* fSeek)(void*, long, int);
    unsigned long(__cdecl* fSeekSet)(void*, long);
    unsigned long(__cdecl* fSeekCur)(void*, long);
    unsigned long(__cdecl* fSeekEnd)(void*, long);
    unsigned long(__cdecl* fTell)(const void*);
    bool(__cdecl* fEof)(void*);
    unsigned long(__cdecl* fSize)(void*);
    void(__cdecl* fCommit)(void*);
    bool(__cdecl* fIsOpen)(void*);
    bool(__cdecl* fIsCompressed)(void*);
    const char*(__cdecl* fName)(const void*);
    void(__cdecl* fDetect)(void*);
    int(__cdecl* fGetEnc)(const void*);
    void(__cdecl* fSetEnc)(void*, int);
    int(__cdecl* fDefEnc)();
    bool(__cdecl* fGetStamp)(void*, long*);
    bool(__cdecl* fReadP)(void*, const Node**);
    bool(__cdecl* fWriteP)(void*, const Node* const*);
    // MemoryFile
    void(__fastcall* mCtor)(void*, void*);
    void(__fastcall* mCtorData)(void*, void*, const void*, unsigned, bool, int);
    void(__fastcall* mCtorFile)(void*, void*, void*, bool, int);
    void(__fastcall* mCtorFileN)(void*, void*, void*, unsigned, bool, int);
    void(__fastcall* mCtorName)(void*, void*, const char*, unsigned);
    void(__fastcall* mDtor)(void*, void*);
    int*(__fastcall* mOpenPath)(void*, void*, int*, const char*, unsigned long, bool, int);
    bool(__fastcall* mOpenData)(void*, void*, const void*, unsigned, bool, int);
    void(__fastcall* mClose)(void*, void*);
    unsigned(__fastcall* mRead)(const void*, void*, void*, unsigned, unsigned);
    void(__fastcall* mReadL)(const void*, void*, long*);
    void(__fastcall* mReadW)(const void*, void*, short*);
    void(__fastcall* mReadB)(const void*, void*, unsigned char*);
    void(__fastcall* mReadF)(const void*, void*, float*);
    void(__fastcall* mReadP)(const void*, void*, const Node**, bool);
    long(__fastcall* mDword)(const void*, void*);
    short(__fastcall* mWord)(const void*, void*);
    signed char(__fastcall* mByte)(const void*, void*);
    unsigned char(__fastcall* mPeek)(const void*, void*);
    void(__fastcall* mWrite)(void*, void*, const void*, unsigned);
    unsigned(__fastcall* mSeek)(const void*, void*, int, int);
    unsigned(__fastcall* mSeekSet)(const void*, void*, int);
    unsigned(__fastcall* mSeekCur)(const void*, void*, int);
    unsigned(__fastcall* mSeekEnd)(const void*, void*, int);
    bool(__fastcall* mEof)(const void*, void*);
    const char*(__fastcall* mName)(const void*, void*);
};
static Api apis[2];
static const Api* origApi;

// the original's MakeRelativeTo reads address 0 for some inputs (the same directory and a base with no name): tried
// first on a copy, under SEH
static bool OrigRelativeSafe(const unsigned char* p, const char* base)
{
    alignas(16) static unsigned char copy[0x400];
    std::memcpy(copy, p, 0x104);
    __try {
        origApi->pRelative(copy, base);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void Load(Api& a, HMODULE m)
{
#define G(f, n) a.f = Get<decltype(a.f)>(m, n)
    G(pCtor, "??0Path@File@Jet@@QAE@XZ");
    G(pCtorS, "??0Path@File@Jet@@QAE@PBD@Z");
    G(pCtorP, "??0Path@File@Jet@@QAE@ABU012@@Z");
    G(pAssignS, "??4Path@File@Jet@@QAAXPBD@Z");
    G(pAssignP, "??4Path@File@Jet@@QAAXABU012@@Z");
    G(pEq, "??8Path@File@Jet@@QBE_NABU012@@Z");
    G(pEqS, "??8Path@File@Jet@@QBE_NPBD@Z");
    G(pNe, "??9Path@File@Jet@@QBE_NABU012@@Z");
    G(pNeS, "??9Path@File@Jet@@QBE_NPBD@Z");
    G(pLess, "??MPath@File@Jet@@QBE_NABU012@@Z");
    G(pBool, "??BPath@File@Jet@@ABE_NXZ");
    G(pNot, "??7Path@File@Jet@@ABE_NXZ");
    G(pChars, "??BPath@File@Jet@@QBAPBDXZ");
    G(pAdd, "?Add" P "QAAXPBD@Z");
    G(pAddN, "?Add" P "QAAXPBDH@Z");
    G(pAddPath, "?AddPath" P "QAAXPBD@Z");
    G(pAddPathN, "?AddPath" P "QAAXPBDH@Z");
    G(pAddSlash, "?AddSlash" P "QAAXXZ");
    G(pDel, "?Del" P "QAAXII@Z");
    G(pDelDrive, "?DelDrive" P "QAAXXZ");
    G(pDelFileName, "?DelFileName" P "QAAXXZ");
    G(pDelName, "?DelName" P "QAAX_N@Z");
    G(pDelPath, "?DelPath" P "QAAXXZ");
    G(pDelSlash, "?DelSlash" P "QAAXXZ");
    G(pGetExt, "?GetExt" P "QBAPBDXZ");
    G(pGetExtS, "?GetExt" P "SAPBDPBD@Z");
    G(pGetFileName, "?GetFileName" P "QBAPBDXZ");
    G(pGetFileNameS, "?GetFileName" P "SAPBDPBD@Z");
    G(pGetPath, "?GetPath" P "QBAPBDXZ");
    G(pHash, "?GetFilePathHash" P "QBAIXZ");
    G(pValid, "?HasValidData" P "QBA_NXZ");
    G(pIsEqual, "?IsEqual" P "SA_NPBD0@Z");
    G(pIsExt, "?IsExt" P "QBA_NPBD@Z");
    G(pIsMatch, "?IsMatch" P "QBA_NPBDPAPBD@Z");
    G(pIsMatchS, "?IsMatch" P "SA_NPBD0PAPBD@Z");
    G(pLast, "?Last" P "QBAIXZ");
    G(pLength, "?Length" P "QBAIXZ");
    G(pMake, "?Make" P "QAAXPBD0@Z");
    G(pRelative, "?MakeRelativeTo" P "QAA_NPBD@Z");
    G(pNormalize, "?Normalize" P "QAAXXZ");
    G(pPrevDir, "?PrevDir" P "QAAXXZ");
    G(pBack, "?ReplaceBackSlashes" P "QAAXXZ");
    G(pFwd, "?ReplaceForwardSlashes" P "QAAXXZ");
    G(pRootDir, "?RootDir" P "QAAXXZ");
    G(pSetExt, "?SetExt" P "QAAXPBD@Z");
    G(pSplit, "?SplitPath" P "QBAXPAD000@Z");
    G(pLower, "?ToLower" P "QAAXXZ");
    G(pUpper, "?ToUpper" P "QAAXXZ");
    G(pFirst, "?ExtractFirstPath" P "QAA?AU123@XZ");
    G(tEq, "??8Time@File@Jet@@QBA_NABT012@@Z");
    G(tLt, "??MTime@File@Jet@@QBA_NABT012@@Z");
    G(tGt, "??OTime@File@Jet@@QBA_NABT012@@Z");
    G(tLe, "??NTime@File@Jet@@QBA_NABT012@@Z");
    G(tGe, "??PTime@File@Jet@@QBA_NABT012@@Z");
    G(tSet, "?SetSeconds@Time@File@Jet@@QAAXJ@Z");
    G(eCtor, "??0Entry@File@Jet@@QAE@XZ");
    G(eCopy, "??0Entry@File@Jet@@QAE@ABU012@@Z");
    G(eAssign, "??4Entry@File@Jet@@QAAAAU012@ABU012@@Z");
    G(eDir, "?IsDir@Entry@File@Jet@@QAA_NXZ");
    G(eCompressed, "?IsCompressed@Entry@File@Jet@@QAA_NXZ");
    G(eHidden, "?IsHidden@Entry@File@Jet@@QAA_NXZ");
    G(fCtor, "??0File@Jet@@QAE@XZ");
    G(fDtor, "??1File@Jet@@QAE@XZ");
    G(fOpen, "?Open" F "QAA" RES "PBDKH@Z");
    G(fOpenB, "?Open" F "QAA" RES "ABVPString@2@PBDKH@Z");
    G(fClose, "?Close" F "QAAKXZ");
    G(fRead, "?Read" F "QAAKPAXKK@Z");
    G(fWrite, "?Write" F "QAAKPBXKK@Z");
    G(fRead8, "?Read" F "QAA_NAA_J@Z");
    G(fRead32, "?Read32" F "QAA_NAAH@Z");
    G(fReadS, "?Read" F "QAA_NAAF@Z");
    G(fWrite8, "?Write" F "QAA_N_J@Z");
    G(fWrite32, "?Write32" F "QAA_NH@Z");
    G(fWriteS, "?Write" F "QAA_NF@Z");
    G(fWriteC, "?Write" F "QAA_NC@Z");
    G(fSafeRead, "?SafeRead" F "QAAXPAXKK@Z");
    G(fReadLine, "?ReadLine" F "QAAKPAXKH@Z");
    G(fWriteLine, "?WriteLine" F "QAAKPBDH@Z");
    G(fReadBlock, "?ReadBlock" F "QAAKPBDPAXK@Z");
    G(fWriteBlock, "?WriteBlock" F "QAAKPBDPBXK@Z");
    G(fSeek, "?Seek" F "QAAKJW4FileSeek@2@@Z");
    G(fSeekSet, "?SeekSet" F "QAAKJ@Z");
    G(fSeekCur, "?SeekCur" F "QAAKJ@Z");
    G(fSeekEnd, "?SeekEnd" F "QAAKJ@Z");
    G(fTell, "?Tell" F "QBAKXZ");
    G(fEof, "?Eof" F "QAA_NXZ");
    G(fSize, "?Size" F "QAAKXZ");
    G(fCommit, "?Commit" F "QAAXXZ");
    G(fIsOpen, "?IsOpen" F "QAA_NXZ");
    G(fIsCompressed, "?IsCompressed" F "QAA_NXZ");
    G(fName, "?Name" F "QBAPBDXZ");
    G(fDetect, "?DetectEncoding" F "QAAXXZ");
    G(fGetEnc, "?GetEncoding" F "QBAHXZ");
    G(fSetEnc, "?SetEncoding" F "QAAXH@Z");
    G(fDefEnc, "?GetDefaultEncoding" F "SAHXZ");
    G(fGetStamp, "?GetStamp" F "QAA_NAATTime@12@@Z");
    G(fReadP, "?Read" F "QAA_NAAVPString@2@@Z");
    G(fWriteP, "?Write" F "QAA_NABVPString@2@@Z");
    G(mCtor, "??0MemoryFile@Jet@@QAE@XZ");
    G(mCtorData, "??0MemoryFile@Jet@@QAE@PBXI_NH@Z");
    G(mCtorFile, "??0MemoryFile@Jet@@QAE@AAVFile@1@_NH@Z");
    G(mCtorFileN, "??0MemoryFile@Jet@@QAE@AAVFile@1@I_NH@Z");
    G(mCtorName, "??0MemoryFile@Jet@@QAE@PBDI@Z");
    G(mDtor, "??1MemoryFile@Jet@@QAE@XZ");
    G(mOpenPath, "?Open" M "QAE" RES "PBDK_NH@Z");
    G(mOpenData, "?Open" M "QAE_NPBXI_NH@Z");
    G(mClose, "?Close" M "QAEXXZ");
    G(mRead, "?Read" M "QBEIPAXII@Z");
    G(mReadL, "?Read" M "QBEXAAJ@Z");
    G(mReadW, "?Read" M "QBEXAAF@Z");
    G(mReadB, "?Read" M "QBEXAAE@Z");
    G(mReadF, "?Read" M "QBEXAAM@Z");
    G(mReadP, "?Read" M "QBEXAAVPString@2@_N@Z");
    G(mDword, "?Readsdword" M "QBEJXZ");
    G(mWord, "?Readsword" M "QBEFXZ");
    G(mByte, "?Readsbyte" M "QBECXZ");
    G(mPeek, "?Peek" M "QBEEXZ");
    G(mWrite, "?Write" M "QAEXPBXI@Z");
    G(mSeek, "?Seek" M "QBEIHW4FileSeek@2@@Z");
    G(mSeekSet, "?SeekSet" M "QBEIH@Z");
    G(mSeekCur, "?SeekCur" M "QBEIH@Z");
    G(mSeekEnd, "?SeekEnd" M "QBEIH@Z");
    G(mEof, "?Eof" M "QBE_NXZ");
    G(mName, "?Name" M "QBEPBDXZ");
#undef G
}

// each side's PString cache (U14b, UseHelpers) and File::Subst (since U10b the substitute lists are separate)
static void** pstringCache;
static Node*(__cdecl* pstringCreate)(void*, const unsigned char*);
static void(__cdecl* pstringDestroy)(void*, Node*);
static void(__cdecl* fileSubsts[2])(const Node**, const char*, bool);
static std::string tmpDir;

static std::mt19937 rng;
static unsigned Pick(unsigned n) { return n ? static_cast<unsigned>(rng() % n) : 0; }

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    std::printf("crash %08lx at %p (side %d) ours %p orig %p\n", e->ExceptionRecord->ExceptionCode,
                e->ExceptionRecord->ExceptionAddress, side, GetModuleHandleA("KernelStandard_7c.dll"),
                GetModuleHandleA("KernelStandard_7c_orig.dll"));
    const DWORD* sp = reinterpret_cast<const DWORD*>(e->ContextRecord->Esp);
    for (int i = 0; i < 48; ++i) std::printf("%08lx%s", sp[i], i % 8 == 7 ? "\n" : " ");
    std::exit(3);
}

// ---- paths

static std::string RandomPath()
{
    static const char* const parts[] = {"a",  "B",    "dir", "Data", ".",   "..",  "x.txt", "y.TGA", "name.ext.gz",
                                        "*",  "*.t*", "?",   "a?c",  "",   ";",   "c:",  "Base", "ts12:",
                                        "\x8e\xe9", "trainz.exe", "..x", ".hidden", "W*D", "ab;cd", "Z:", "~"};
    static const char* const seps[] = {"/", "\\", "", "//", ":"};
    std::string s;
    if (Pick(4) == 0) s += Pick(2) ? "/" : "\\";
    if (Pick(5) == 0) s += Pick(2) ? "c:/" : "D:\\";
    if (Pick(6) == 0) s += "./";
    const unsigned n = Pick(7);
    for (unsigned i = 0; i < n; ++i) {
        s += parts[Pick(sizeof(parts) / sizeof(*parts))];
        if (i + 1 < n || Pick(4) == 0) s += seps[Pick(Pick(3) ? 2 : 5)];
    }
    if (Pick(12) == 0) {
        const unsigned target = 0xf8 + Pick(0x14);  // around the 0x102 / 0x104 limits
        while (s.size() < target) s += static_cast<char>('a' + Pick(26)), s += Pick(9) ? "" : "/";
        s.resize(target);
    }
    return s;
}

struct PathCase {
    std::string a, b, c;
    unsigned op;
    unsigned x, y;
};
static std::vector<PathCase> pathCases;

static std::string Off(const char* p, const char* base)
{
    if (!p) return "null";
    return Num(p - base);
}

static void PathBuf(unsigned char* buf, const std::string& s)
{
    std::memset(buf, 0xcd, 0x104);
    std::strncpy(reinterpret_cast<char*>(buf), s.c_str(), 0x103);
    buf[0x103] = 0;
}

static void RunPaths()
{
    const Api& a = apis[side];
    for (const PathCase& c : pathCases) {
        // inside one zeroed area: some members read past the 0x104 bytes (DelPath's memmove copies len + 1 bytes
        // from the name), which must read the same on both sides
        alignas(16) static unsigned char area[0xc00];
        std::memset(area, 0, sizeof(area));
        unsigned char* const p = area;
        unsigned char* const q = area + 0x400;
        unsigned char* const r = area + 0x800;
        PathBuf(p, c.a);
        PathBuf(q, c.b);
        std::string line = Num(c.op) + " ";
        const char* const pc = reinterpret_cast<const char*>(p);
        switch (c.op) {
        case 0: a.pCtor(p, nullptr); break;
        case 1: a.pCtorS(p, nullptr, c.b.c_str()); break;
        case 2: a.pCtorP(p, nullptr, q); break;
        case 3: a.pAssignS(p, c.b.c_str()); break;
        case 4: a.pAssignP(p, q); break;
        case 5:
            line += Num(a.pEq(p, nullptr, q)) + Num(a.pEqS(p, nullptr, c.b.c_str())) + Num(a.pNe(p, nullptr, q)) +
                    Num(a.pNeS(p, nullptr, c.b.c_str())) + Num(a.pLess(p, nullptr, q)) + Num(a.pBool(p, nullptr)) +
                    Num(a.pNot(p, nullptr)) + Off(a.pChars(p), pc);
            break;
        case 6: a.pAdd(p, c.b.size() < 0x80 && c.a.size() < 0x80 ? c.b.c_str() : ""); break;
        case 7: a.pAddN(p, c.b.c_str(), c.a.size() + c.x % 0x40 < 0xff ? c.x % 0x40 : 0); break;
        case 8: a.pAddPath(p, c.b.c_str()); break;
        case 9: a.pAddPathN(p, c.b.c_str(), static_cast<int>(c.x % 0x30)); break;
        case 10: a.pAddSlash(p); break;
        case 11: a.pDel(p, c.x % 0x20, c.y % 0x20); break;
        case 12: a.pDelDrive(p); break;
        case 13: a.pDelFileName(p); break;
        case 14: a.pDelName(p, c.x & 1); break;
        case 15: a.pDelPath(p); break;
        case 16: a.pDelSlash(p); break;
        case 17:
            line += Off(a.pGetExt(p), pc) + " " + Off(a.pGetExtS(c.b.c_str()), c.b.c_str()) + " " +
                    Off(a.pGetFileName(p), pc) + " " + Off(a.pGetFileNameS(c.b.c_str()), c.b.c_str()) + " " +
                    Off(a.pGetPath(p), pc) + " " + Num(a.pHash(p)) + " " + Num(a.pValid(p)) + " " +
                    Num(a.pLast(p)) + " " + Num(a.pLength(p));
            break;
        case 18: line += Num(a.pIsEqual(c.a.c_str(), c.b.c_str())); break;
        case 19: line += Num(a.pIsExt(p, c.x & 1 ? c.c.c_str() : nullptr)); break;
        case 20: {
            const char* rest = reinterpret_cast<const char*>(1);
            line += Num(a.pIsMatch(p, c.b.c_str(), &rest)) + " " + Off(rest, pc);
            const char* rest2 = nullptr;
            line += " " + Num(a.pIsMatchS(c.a.c_str(), c.b.c_str(), c.x & 1 ? &rest2 : nullptr)) + " " +
                    Off(rest2, c.a.c_str());
            break;
        }
        case 21: a.pMake(p, c.b.c_str(), c.c.c_str()); break;
        case 22:
            if (OrigRelativeSafe(p, c.b.c_str()))
                line += Num(a.pRelative(p, c.b.c_str()));
            else
                line += "(the original faults)";
            break;
        case 23:
            if (!c.a.empty()) a.pNormalize(p);  // (the original reads address 0 for an empty path)
            break;
        case 24: a.pPrevDir(p); break;
        case 25: a.pBack(p); break;
        case 26: a.pFwd(p); break;
        case 27: a.pRootDir(p); break;
        case 28:
            if (c.a.size() < 0xf0) a.pSetExt(p, c.x & 1 ? c.c.c_str() : nullptr);  // (no length check)
            break;
        case 29: {
            char d[0x104], dir[0x104], n[0x104], e[0x104];
            a.pSplit(p, d, dir, n, e);
            line += std::string(d) + "|" + dir + "|" + n + "|" + e;
            break;
        }
        case 30: a.pLower(p); break;
        case 31: a.pUpper(p); break;
        case 32:
            std::memset(r, 0xcd, 0x104);
            line += Off(static_cast<const char*>(a.pFirst(p, r)), reinterpret_cast<const char*>(r)) + " " + Hex(r, 0x104);
            break;
        }
        line += " " + Hex(p, 0x104);
        Log(line);
    }
}

// ---- Time, Entry

static void RunTimeEntry(unsigned seed)
{
    const Api& a = apis[side];
    std::mt19937 r(seed);
    for (int i = 0; i < 20; ++i) {
        long x[2] = {static_cast<long>(r()), static_cast<long>(r() % 3) - 1};
        if (r() & 1) x[1] = x[0];
        Log(Num(a.tEq(&x[0], &x[1])) + Num(a.tLt(&x[0], &x[1])) + Num(a.tGt(&x[0], &x[1])) +
            Num(a.tLe(&x[0], &x[1])) + Num(a.tGe(&x[0], &x[1])));
        a.tSet(&x[0], static_cast<long>(r()));
        Log(Num(x[0]));
    }
    alignas(4) unsigned char e[0x120], f[0x120];
    std::memset(e, 0xab, sizeof(e));
    a.eCtor(e, nullptr);
    Log(Hex(e, sizeof(e)));
    for (int i = 0; i < 8; ++i) {
        for (auto& b : e) b = static_cast<unsigned char>(r());
        e[r() % 0x104] = 0;
        std::memset(f, 0x5a, sizeof(f));
        a.eCopy(f, nullptr, e);
        Log(Hex(f, sizeof(f)));
        std::memset(f, 0x77, sizeof(f));
        f[0] = 0;
        Log(Num(a.eAssign(f, e) == f) + Hex(f, sizeof(f)));
        Log(Num(a.eDir(e)) + Num(a.eCompressed(e)) + Num(a.eHidden(e)));
    }
}

// ---- files

struct FileOp {
    unsigned kind;
    unsigned long a, b;
    std::string text;
};
struct FileScript {
    std::string name;  // relative to the temp dir
    std::vector<unsigned char> initial;  // written to the file before the script (empty: no file)
    bool exists;
    std::vector<FileOp> ops;
};
static std::vector<FileScript> scripts;

static std::vector<unsigned char> RandomBytes(size_t n, unsigned kind, unsigned seed = 0)
{
    std::mt19937 local(seed);
    std::vector<unsigned char> v(n);
    for (size_t i = 0; i < n; ++i) {
        switch (kind) {
        case 0: v[i] = static_cast<unsigned char>(seed ? local() : rng()); break;  // incompressible
        case 1: v[i] = static_cast<unsigned char>("abcab\n"[seed ? local() % 6 : Pick(6)]); break;  // compressible text
        default: v[i] = static_cast<unsigned char>(i % 7 == 6 ? '\n' : 'A' + (i * 13 + kind) % 26); break;
        }
    }
    return v;
}

static const unsigned long kModes[] = {0x10, 0x20, 0x30, 0x11, 0x101, 0x111, 0x201, 0x211, 0x301, 0x102,
                                       0x112, 0x1010, 0x21, 0x0, 0x110};

static void DrawFile(unsigned n)
{
    FileScript s;
    s.name = "kfd_" + Num(n % 4) + ".bin";
    s.exists = Pick(5) != 0;
    if (s.exists) {
        const unsigned sizes[] = {0, 1, 2, 11, 12, 100, 0x3fff, 0x4000, 0x4001, 0x8003, 50000};
        s.initial = RandomBytes(sizes[Pick(11)], Pick(3));
        if (Pick(3) == 0 && s.initial.size() >= 2) s.initial[0] = 0xff, s.initial[1] = 0xfe;  // a UTF-16 mark
    }
    // a compressed source (written by the original's own File as mode 0x102 in the setup) one time in three
    const unsigned ops = 4 + Pick(30);
    s.ops.push_back({0, kModes[Pick(sizeof(kModes) / sizeof(*kModes))], Pick(3), ""});  // Open(mode, encoding)
    for (unsigned i = 0; i < ops; ++i) {
        FileOp o{1 + Pick(18), 0, 0, ""};
        switch (o.kind) {
        case 1: o.a = Pick(4) ? Pick(0x80) : Pick(0x3000); o.b = 1 + Pick(3); break;  // Read(size, count)
        case 2: o.a = Pick(4) ? Pick(0x80) : Pick(0x9000); break;                     // Write(bytes)
        case 3: o.a = Pick(3); o.b = Pick(4) ? Pick(0x100) : Pick(0x10000); break;     // Seek(whence, offset)
        case 4: o.a = 2 + Pick(Pick(2) ? 0x40 : 0x1100); o.b = Pick(3); break;         // ReadLine(size, enc)
        case 5: o.text = "line " + Num(Pick(1000)) + (Pick(2) ? "\n" : "\xe9\n"); o.b = Pick(3); break;  // WriteLine
        case 13: o.text = Pick(2) ? "INFO" : "DATA"; o.a = Pick(0x30); break;          // WriteBlock / ReadBlock
        default: break;
        }
        s.ops.push_back(o);
    }
    scripts.push_back(s);
}

static std::vector<unsigned char> ReadDisk(const std::string& path)
{
    std::vector<unsigned char> v;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return v;
    unsigned char b[4096];
    size_t n;
    while ((n = std::fread(b, 1, sizeof(b), f)) > 0) v.insert(v.end(), b, b + n);
    std::fclose(f);
    return v;
}

static void WriteDisk(const std::string& path, const std::vector<unsigned char>& v)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    if (!v.empty()) std::fwrite(v.data(), 1, v.size(), f);
    std::fclose(f);
}

// the File's fields that do not depend on addresses
static std::string State(const unsigned char* f)
{
    auto u = [&](int o) { return *reinterpret_cast<const unsigned long*>(f + o); };
    const long fd = static_cast<long>(u(0));
    return "fd" + Num(fd != -1) + " at" + Num(u(4) - u(8)) + " n" + Num(u(0xc)) + " m" + Num(u(0x10)) + " sz" +
           Num(static_cast<long>(u(0x14))) + " ds" + Num(u(0x18)) + " p" + Num(u(0x1c)) + " r" + Num(u(0x20)) +
           " d" + Num(u(0x24)) + " fl" + Num(f[0x28] & 3) + " e" + Num(u(0x34)) + " '" +
           reinterpret_cast<const char*>(f + 0x38) + "'";
}

template <class Fn>
static std::string Try(Fn fn)
{
    try {
        return fn();
    } catch (...) {
        return "threw";
    }
}

static void RunFiles()
{
    const Api& a = apis[side];
    for (const FileScript& s : scripts) {
        const std::string path = tmpDir + s.name;
        DeleteFileA(path.c_str());
        if (s.exists) WriteDisk(path, s.initial);
        alignas(8) unsigned char f[0x200];
        std::memset(f, 0, sizeof(f));
        a.fCtor(f, nullptr);
        Log("ctor " + State(f));
        static unsigned char data[0x9100];
        bool stale = false;  // Open failed with 5 or 6: the descriptor was closed but is still in the object
        for (const FileOp& o : s.ops) {
            if (stale) break;
            if (getenv("KFD_TRACE")) std::printf("  op %u %lu %lu %s\n", o.kind, o.a, o.b, State(f).c_str());
            // what reaches msvcr80 with no descriptor check ends the process in both modules (invalid parameter)
            const bool open = *reinterpret_cast<const long*>(f) != -1;
            const bool raw = o.kind == 3 || o.kind == 4 || o.kind == 6 || o.kind == 7 || o.kind == 13 || o.kind == 14 ||
                             o.kind == 17 || o.kind == 18 || o.kind > 17;
            if (!open && raw) continue;
            std::string line = Num(o.kind) + " ";
            switch (o.kind) {
            case 0: {
                int r = -1;
                line += Try([&] { return Num(*a.fOpen(f, &r, s.name.c_str(), o.a, o.b)); });
                stale = r == 5 || r == 6;
                break;
            }
            case 1:
                std::memset(data, 0xee, sizeof(data));
                line += Try([&] {
                    const unsigned long n = a.fRead(f, data, o.a, o.b);
                    return Num(n) + " " + Num(Fnv(data, o.a * o.b < sizeof(data) ? o.a * o.b : sizeof(data)));
                });
                break;
            case 2: {
                const std::vector<unsigned char> v = RandomBytes(o.a, o.a & 3, o.a * 31 + o.b + 1);
                line += Try([&] { return Num(a.fWrite(f, v.data(), 1, static_cast<unsigned long>(v.size()))); });
                break;
            }
            case 3:
                line += Try([&] {
                    const long off = o.a == 2 ? static_cast<long>(o.b % 0x200) : static_cast<long>(o.b);
                    return Num(static_cast<long>(a.fSeek(f, off, static_cast<int>(o.a))));
                });
                break;
            case 4:
                std::memset(data, 0xee, 0x1200);
                line += Try([&] {
                    const unsigned long n = a.fReadLine(f, data, o.a, static_cast<int>(o.b));
                    return Num(n) + " " + Hex(data, o.a < 0x1200 ? o.a : 0x1200);
                });
                break;
            case 5: line += Try([&] { return Num(a.fWriteLine(f, o.text.c_str(), static_cast<int>(o.b))); }); break;
            case 6: line += Try([&] { return Num(a.fSize(f)); }); break;
            case 7: line += Try([&] { return Num(a.fEof(f)) + Num(a.fTell(f)) + Num(a.fIsOpen(f)) + Num(a.fIsCompressed(f)); }); break;
            case 8: line += Try([&] { a.fCommit(f); return std::string("ok"); }); break;
            case 9: line += Try([&] { a.fDetect(f); return Num(a.fGetEnc(f)); }); break;
            case 10: {
                __int64 v = 0x1122334455667788ll;
                int w = 0;
                short h = 0;
                line += Try([&] { return Num(a.fRead8(f, &v)) + Num(v); });
                line += Try([&] { return Num(a.fRead32(f, &w)) + Num(w) + Num(a.fReadS(f, &h)) + Num(h); });
                break;
            }
            case 11:
                line += Try([&] {
                    return Num(a.fWrite8(f, 0x0102030405060708ll)) + Num(a.fWrite32(f, -7)) + Num(a.fWriteS(f, 300)) +
                           Num(a.fWriteC(f, -1));
                });
                break;
            case 12:
                line += Try([&] {
                    a.fSafeRead(f, data, 3, 2);
                    return std::string("read");
                });
                break;
            case 13: {
                const std::vector<unsigned char> v = RandomBytes(o.a, 2, o.a + 7);
                line += Try([&] { return Num(a.fWriteBlock(f, o.text.c_str(), v.data(), static_cast<unsigned long>(v.size()))); });
                break;
            }
            case 14:
                std::memset(data, 0xee, 0x40);
                line += Try([&] { return Num(a.fReadBlock(f, "INFO", data, 0x10)) + Hex(data, 0x10); });
                break;
            case 15: line += Try([&] { return Num(a.fClose(f)); }); break;
            case 16: {
                // a PString written then read back
                Node* n = pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>("pstr-test"));
                line += Try([&] { return Num(a.fWriteP(f, &n)); });
                pstringDestroy(*pstringCache, n);
                const Node* got = nullptr;
                line += Try([&] { return Num(a.fReadP(f, &got)) + (got ? std::string(reinterpret_cast<const char*>(got + 0x18)) : "-"); });
                break;
            }
            case 17: {
                long t = 0;
                line += Try([&] { return Num(a.fGetStamp(f, &t)) + Num(t != 0); });
                line += " " + Num(a.fDefEnc()) + Off(a.fName(f), reinterpret_cast<const char*>(f + 0x38));
                break;
            }
            default: line += Try([&] { return Num(static_cast<long>(a.fSeekSet(f, 0))) + Num(static_cast<long>(a.fSeekCur(f, 5))) + Num(static_cast<long>(a.fSeekEnd(f, 3))); }); break;
            }
            Log(line + " | " + State(f));
        }
        if (stale) *reinterpret_cast<long*>(f) = -1;  // (the destructor would close it again)
        Try([&] { a.fClose(f); return std::string(); });
        a.fDtor(f, nullptr);
        const std::vector<unsigned char> disk = ReadDisk(path);
        Log("disk " + Num(disk.size()) + " " + Num(Fnv(disk.data(), disk.size())));
        DeleteFileA(path.c_str());
    }
}

// compressed files: written by one side's File (mode 0x102), read and sought by the other side's
static void RunCompressed(unsigned seed)
{
    const Api& w = apis[side];
    const Api& rd = apis[side ^ 1];
    std::mt19937 r(seed);
    const std::string name = "kfd_c.bin";
    const std::string path = tmpDir + name;
    DeleteFileA(path.c_str());
    const size_t total = r() % 3 == 0 ? r() % 0x50 : r() % 0x14000;
    std::vector<unsigned char> v(total);
    for (size_t i = 0; i < total; ++i) v[i] = static_cast<unsigned char>(r() % 4 ? "trainz "[i % 7] : r());
    alignas(8) unsigned char f[0x200];
    w.fCtor(f, nullptr);
    int res = -1;
    w.fOpen(f, &res, name.c_str(), 0x102, 0);
    for (size_t at = 0; at < total;) {
        const size_t n = std::min<size_t>(1 + r() % 0x5000, total - at);
        w.fWrite(f, v.data() + at, 1, static_cast<unsigned long>(n));
        at += n;
    }
    w.fClose(f);
    w.fDtor(f, nullptr);
    const std::vector<unsigned char> disk = ReadDisk(path);
    Log("packed " + Num(res) + " " + Num(disk.size()) + " " + Num(Fnv(disk.data(), disk.size())));
    // read through both modules (the reading side's log is compared)
    for (int pass = 0; pass < 2; ++pass) {
        const Api& a = pass ? rd : w;
        a.fCtor(f, nullptr);
        a.fOpen(f, &res, name.c_str(), pass ? 0x30 : 0x10, 0);
        std::string line = "open " + Num(res) + " " + State(f);
        std::mt19937 r2(seed * 7 + 1);
        std::vector<unsigned char> b(0x6000);
        for (int i = 0; i < 25; ++i) {
            const unsigned k = r2() % 4;
            if (k == 0) {
                const long off = static_cast<long>(r2() % (total + 0x100));
                line += " s" + Try([&] { return Num(static_cast<long>(a.fSeek(f, off, 0))); });
            } else if (k == 1) {
                const long off = static_cast<long>(r2() % 0x800) - 0x400;
                line += " c" + Try([&] { return Num(static_cast<long>(a.fSeek(f, off, 1))); });
            } else {
                const unsigned long n = r2() % 3 ? r2() % 0x100 : r2() % 0x6000;
                line += " r" + Try([&] { return Num(a.fRead(f, b.data(), 1, n)) + ":" + Num(Fnv(b.data(), n)); });
            }
        }
        a.fClose(f);
        a.fDtor(f, nullptr);
        if (pass == 0) Log(line);  // compared side by side (each side reads its own writer's file)
    }
    DeleteFileA(path.c_str());
}

// substitutes: a FileSubPath (a directory) and a FileSubArchive (a new .ja archive), each side its own (its module's
// File::Subst) under its own names
static void RunSubst(unsigned seed)
{
    const Api& a = apis[side];
    auto const fileSubst = fileSubsts[side];
    std::mt19937 r(seed);
    const std::string dirName = "kfdsub" + Num(side);
    const std::string arcName = "kfdarc" + Num(side);
    const std::string dirPath = tmpDir + dirName + "\\";
    const std::string arcPath = tmpDir + dirName + ".ja";
    CreateDirectoryA(dirPath.c_str(), nullptr);
    DeleteFileA(arcPath.c_str());
    const Node* dn = pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(dirName.c_str()));
    const Node* an = pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(arcName.c_str()));
    fileSubst(&dn, dirPath.c_str(), false);
    fileSubst(&an, arcPath.c_str(), false);
    if (getenv("KFD_TRACE")) std::printf("  substs set\n");
    for (int k = 0; k < 2; ++k) {
        const std::string base = k ? arcName : dirName;
        for (int i = 0; i < 3; ++i) {
            const std::string file = base + ":f" + Num(i) + ".txt";
            const size_t n = r() % 3 ? r() % 0x200 : r() % 0x9000;
            std::vector<unsigned char> v(n);
            for (auto& c : v) c = static_cast<unsigned char>(r() % 3 ? 'a' + r() % 4 : r());
            alignas(8) unsigned char f[0x200];
            a.fCtor(f, nullptr);
            int res = -1;
            a.fOpen(f, &res, file.c_str(), i == 2 ? 0x102 : 0x101, 0);
            std::string line = file.substr(file.find(':')) + " w" + Num(res);
            if (res == 0) {
                line += " " + Num(a.fWrite(f, v.data(), 1, static_cast<unsigned long>(n)));
                a.fClose(f);
            }
            a.fOpen(f, &res, file.c_str(), 0x10, 0);
            line += " r" + Num(res);
            if (res == 0) {
                std::vector<unsigned char> b(n + 16);
                line += " " + Try([&] { return Num(a.fSize(f)) + " " + Num(a.fRead(f, b.data(), 1, static_cast<unsigned long>(n + 16))); });
                line += " " + Num(std::memcmp(b.data(), v.data(), n) == 0);
                line += " " + Try([&] { return Num(static_cast<long>(a.fSeek(f, static_cast<long>(n / 2), 0))) + " " + Num(a.fRead(f, b.data(), 1, 8)); });
                a.fClose(f);
            }
            a.fDtor(f, nullptr);
            if (getenv("KFD_TRACE")) std::printf("  %s\n", line.c_str());
            Log(line);
        }
    }
    if (getenv("KFD_TRACE")) std::printf("  unsubst\n");
    fileSubst(&dn, nullptr, false);
    fileSubst(&an, nullptr, false);
    pstringDestroy(*pstringCache, const_cast<Node*>(dn));
    pstringDestroy(*pstringCache, const_cast<Node*>(an));
}

// ---- MemoryFile

static std::string MState(const unsigned char* m, const Api& a)
{
    auto u = [&](int o) { return *reinterpret_cast<const unsigned long*>(m + o); };
    const unsigned long size = u(8);
    std::string s = "at" + Num(static_cast<long>(u(0) - u(4))) + " n" + Num(size) + " own" + Num(m[0xc]);
    const char* const name = a.mName(m, nullptr);
    s += " '" + std::string(name ? name : "(null)") + "'";
    if (u(4) && size < 0x100000) s += " " + Num(Fnv(reinterpret_cast<const void*>(u(4)), size));
    return s;
}

static void RunMemory(unsigned seed)
{
    const Api& a = apis[side];
    std::mt19937 r(seed);
    const std::string name = "kfd_m.txt";
    const std::string path = tmpDir + name;
    for (int round = 0; round < 6; ++round) {
        // the source: ANSI or UTF-16 (with or without a mark)
        const unsigned kind = r() % 4;
        std::vector<unsigned char> v;
        if (kind == 0 || kind == 1) {
            if (kind == 1) v = {0xff, 0xfe};
            const unsigned n = r() % 0x3000;
            for (unsigned i = 0; i < n; ++i) {
                const unsigned short c = static_cast<unsigned short>(i % 9 == 8 ? '\n' : 'a' + r() % 26);
                v.push_back(static_cast<unsigned char>(c));
                v.push_back(static_cast<unsigned char>(c >> 8));
            }
        } else {
            const unsigned n = r() % 3 ? r() % 0x200 : r() % 0x9000;
            for (unsigned i = 0; i < n; ++i) v.push_back(static_cast<unsigned char>(r() % 2 ? r() : 'a' + i % 26));
            if (v.size() >= 4 && r() % 2) *reinterpret_cast<unsigned long*>(v.data()) = static_cast<unsigned long>(r() % 0x30) | (r() % 2 ? 0x40000000 : 0);
        }
        WriteDisk(path, v);
        const bool unicode = r() % 2 != 0;
        const int encoding = static_cast<int>(r() % 3);
        alignas(8) unsigned char m[0x40], f[0x200];
        std::memset(m, 0, sizeof(m));
        const unsigned how = r() % 5;
        switch (how) {
        case 0: a.mCtorData(m, nullptr, v.data(), static_cast<unsigned>(v.size()), unicode, encoding); break;
        case 1: {
            a.mCtor(m, nullptr);
            int res = -1;
            a.mOpenPath(m, nullptr, &res, name.c_str(), 0x10, unicode, encoding);
            Log("open " + Num(res));
            break;
        }
        case 2:
        case 3: {
            a.fCtor(f, nullptr);
            int res = -1;
            a.fOpen(f, &res, name.c_str(), 0x10, 0);
            if (res == 0 && v.size() > 8 && r() % 2) a.fSeek(f, 3, 0);
            if (how == 2)
                a.mCtorFile(m, nullptr, f, unicode, encoding);
            else
                a.mCtorFileN(m, nullptr, f, static_cast<unsigned>(v.size() / 2), unicode, encoding);
            Log("file " + Num(res) + " " + State(f));
            a.fClose(f);
            a.fDtor(f, nullptr);
            break;
        }
        default:
            a.mCtorName(m, nullptr, "named", static_cast<unsigned>(r() % 0x40));
            if (*reinterpret_cast<void**>(m + 4)) std::memset(*reinterpret_cast<void**>(m + 4), 0, *reinterpret_cast<unsigned*>(m + 8));  // (not initialised)
            break;
        }
        Log("mf " + Num(how) + " " + MState(m, a));
        for (int i = 0; i < 30; ++i) {
            unsigned k = r() % 12;
            // with no data a negative seek wraps the cursor below 0, where the bounds check overflows (both modules)
            if (!*reinterpret_cast<void**>(m + 4) && k >= 3 && k <= 6) k = 7;
            if (getenv("KFD_TRACE")) std::printf("  m %u cur %p start %p n %u %s\n", k, *reinterpret_cast<void**>(m), *reinterpret_cast<void**>(m + 4), *reinterpret_cast<unsigned*>(m + 8), MState(m, a).c_str());
            std::string line = Num(k) + " ";
            unsigned char b[0x100];
            switch (k) {
            case 0: {
                const unsigned n = r() % 0x60;
                std::memset(b, 0xee, sizeof(b));
                line += Num(a.mRead(m, nullptr, b, n, 1)) + Hex(b, n);
                break;
            }
            case 1: {
                long l = 5;
                short s = 5;
                unsigned char c = 5;
                float fl = 5;
                a.mReadL(m, nullptr, &l);
                a.mReadW(m, nullptr, &s);
                a.mReadB(m, nullptr, &c);
                a.mReadF(m, nullptr, &fl);
                line += Num(l) + "," + Num(s) + "," + Num(c) + "," + Hex(&fl, 4);
                break;
            }
            case 2: line += Num(a.mDword(m, nullptr)) + "," + Num(a.mWord(m, nullptr)) + "," + Num(a.mByte(m, nullptr)) + "," + Num(a.mPeek(m, nullptr)); break;
            case 3: line += Num(a.mSeek(m, nullptr, static_cast<int>(r() % 0x100) - 0x40, static_cast<int>(r() % 4))); break;
            case 4: line += Num(a.mSeekSet(m, nullptr, static_cast<int>(r() % 0x200) - 0x20)); break;
            case 5: line += Num(a.mSeekCur(m, nullptr, static_cast<int>(r() % 0x80) - 0x40)); break;
            case 6: line += Num(a.mSeekEnd(m, nullptr, static_cast<int>(r() % 0x80) - 0x10)); break;
            case 7: line += Num(a.mEof(m, nullptr)); break;
            case 8: {
                const unsigned n = r() % 0x20;
                for (unsigned j = 0; j < n; ++j) b[j] = static_cast<unsigned char>(j * 7);
                if (m[0xc]) a.mWrite(m, nullptr, b, n);  // (only into owned blocks)
                break;
            }
            case 9: {
                // a length past the data leaves the text buffer as Alloc gave it (garbage): only the length logged
                const unsigned char* const cur = *reinterpret_cast<unsigned char* const*>(m);
                const unsigned char* const end = *reinterpret_cast<unsigned char* const*>(m + 4) + *reinterpret_cast<unsigned*>(m + 8);
                bool copied = false;
                if (cur && cur + 4 <= end) {
                    const unsigned len = *reinterpret_cast<const unsigned*>(cur) & 0xbfffffff;
                    copied = len <= static_cast<unsigned>(end - cur - 4);
                }
                const Node* s = nullptr;
                a.mReadP(m, nullptr, &s, false);
                line += !copied ? "(garbage)" : !s ? "-" : "'" + std::string(reinterpret_cast<const char*>(s + 0x18)) + "'";
                if (s) pstringDestroy(*pstringCache, const_cast<Node*>(s));
                break;
            }
            case 10: {
                const unsigned n = r() % 0x40;
                line += Num(a.mOpenData(m, nullptr, v.data(), static_cast<unsigned>(v.size() < n ? v.size() : n), r() % 2 != 0, static_cast<int>(r() % 3)));
                break;
            }
            default: a.mClose(m, nullptr); break;
            }
            Log(line + " | " + MState(m, a));
        }
        a.mDtor(m, nullptr);
        DeleteFileA(path.c_str());
    }
}

// every helper from the side's own module (U14b: each module has its own heap, PString cache and statics)
static HMODULE sideModule[2];
static void UseHelpers(int sd)
{
    const HMODULE m = sideModule[sd];
    pstringCache = Get<void**>(m, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringCreate = Get<decltype(pstringCreate)>(m, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringDestroy = Get<decltype(pstringDestroy)>(m, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_ks_file <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 300;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    HMODULE orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    const bool swap = getenv("KFD_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours);
    Load(apis[1], swap ? ours : orig);
    sideModule[0] = swap ? orig : ours;
    sideModule[1] = swap ? ours : orig;
    UseHelpers(1);  // the setup below (as before U14b, the original's)
    origApi = &apis[swap ? 0 : 1];
    fileSubsts[0] = Get<void(__cdecl*)(const Node**, const char*, bool)>(swap ? orig : ours, "?Subst@File@Jet@@SAXABVPString@2@PBD_N@Z");
    fileSubsts[1] = Get<void(__cdecl*)(const Node**, const char*, bool)>(swap ? ours : orig, "?Subst@File@Jet@@SAXABVPString@2@PBD_N@Z");
    {
        char t[MAX_PATH];
        GetTempPathA(MAX_PATH, t);
        tmpDir = std::string(t) + "kfd\\";
        CreateDirectoryA(tmpDir.c_str(), nullptr);
        SetCurrentDirectoryA(tmpDir.c_str());
    }
    unsigned long long checks = 0, bad = 0;
    for (int n = 0; n < scenarios; ++n) {
        rng.seed(0x5eed0000u + n);
        pathCases.clear();
        scripts.clear();
        for (int i = 0; i < 60; ++i) {
            const unsigned op = Pick(33);
            pathCases.push_back({RandomPath(), RandomPath(), Pick(2) ? "txt" : ".Gz", op, rng(), rng()});
        }
        for (unsigned i = 0; i < 3; ++i) DrawFile(n * 3 + i);
        const unsigned seed = rng();
        const bool subst = n % 10 == 0;
        const bool trace = getenv("KFD_TRACE") != nullptr;
        for (side = 0; side < 2; ++side) {
            UseHelpers(side);
            if (trace) std::printf("%d/%d paths\n", n, side);
            RunPaths();
            if (trace) std::printf("time\n");
            RunTimeEntry(seed);
            if (trace) std::printf("files\n");
            RunFiles();
            if (trace) std::printf("compressed\n");
            RunCompressed(seed);
            if (trace) std::printf("memory\n");
            RunMemory(seed);
            if (trace) std::printf("subst\n");
            if (subst) RunSubst(seed);
        }
        const size_t lines = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < lines; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 8) {
                    if (getenv("KFD_CONTEXT"))
                        for (size_t k = i > 40 ? i - 40 : 0; k < i; ++k) std::printf("   . %s\n", logs[0][k].substr(0, 200).c_str());
                    std::printf("scenario %d line %zu:\n  ours %s\n  orig %s\n", n, i, x.substr(0, 1500).c_str(), y.substr(0, 1500).c_str());
                }
                break;
            }
        }
        if (getenv("KFD_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", n, l.substr(0, 300).c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
