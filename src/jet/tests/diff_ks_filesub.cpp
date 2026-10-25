// Differential test of KernelStandard unit U10b (FileSub.cpp, FileFind.cpp, FileSubArchive.cpp): the substitute
// list, its statics, FileSubPath / FileSubLink / FileSubArchive and File::Find, our module against the original.
// Each side builds its own substitutes (the lists are separate):
//  * read: a substitute of a directory, an archive (written once per scenario through the original) and a link to a
//    second substitute; Exists / ExistsIgnoreCase / IsDirectory / GetStamp / TrueName / CheckWriteMode /
//    SubstExists / GetSubstBuildVersion / ProductCode / Path::GetSubst over drawn names, then File::Find over drawn
//    patterns (every entry's 0x120 bytes and its path), then the same after the linked substitute is removed;
//  * write: each side its own copy of a small tree under writable substitutes (and a link): files written plain
//    and compressed through them, Md, Rename, Unlink, SetReadonly, plain Md / Prune / Rename / Unlink, then the
//    whole tree (names, sizes, read-only bits, content hashes);
//  * archive: each side its own .ja, new or built here (versions, magics, with / without / with a wrong directory,
//    "FIL!" and foreign chunks, raw-chunk "CFIL" files, short and foreign files), mounted writable or read-only;
//    files written plain and compressed around the 16 KB edges (File::SetCallBack progress logged), read alone and
//    two at once, Exists / IsDirectory / GetStamp / TrueName / Unlink / Rename / Md, searches (one with a file
//    written in the middle), build version / product code (SetDefault* drawn), remounts, and the archive file
//    itself chunk by chunk after each unmount (names up to their NUL: the original writes leftovers after them);
//  * real archives (first): Base.ja and Data.ja from <bin>\.. mounted on both sides, every entry listed by Find
//    and read through File (KFS_BUILTIN=1 adds Builtin\ts12a..h.ja, about 10 GB).
//
// usage: diff_ks_filesub.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
// KFS_SWAP: the sides swapped; KFS_DUMP: every log line; KFS_TRACE: log lines as they come; KFS_NOREAL: skip the
// real archives; KFS_FROM: the first scenario; KFS_APROBE: one file written to a new archive and read back.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

// Jet's exceptions, as far as a catch needs them (caught by type name across the modules)
namespace Jet {
class Exception {
public:
    virtual ~Exception();
    virtual const char* __cdecl Class() const;
    virtual bool __cdecl IsHardwareException() const;
    virtual const char* __cdecl Description() const;
    virtual const char* __cdecl Reason() const;
};
}  // namespace Jet

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
static void Log(const std::string& s)
{
    static const bool trace = getenv("KFS_TRACE") != nullptr;
    if (trace) std::printf("%d| %s\n", side, s.substr(0, 300).c_str());
    logs[side].push_back(s);
}
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
static unsigned long Fnv(const void* p, size_t n, unsigned long h = 2166136261u)
{
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 16777619u;
    return h;
}

using Node = unsigned char;
using PS = const Node*;  // a PString: one node pointer
#define F "@File@Jet@@"
#define FI "@Find@File@Jet@@"
#define RES "?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@"

struct Api {
    void(__cdecl* subst)(const PS*, const char*, bool);
    int*(__cdecl* substExists)(int*, const PS*);
    void(__cdecl* substLink)(const PS*, const PS*);
    unsigned long(__cdecl* buildVersion)(const PS*);
    unsigned long(__cdecl* productCode)(const PS*);
    int*(__cdecl* exists)(int*, const char*);
    int*(__cdecl* existsB)(int*, PS*, const char*);
    int*(__cdecl* existsIC)(int*, const char*, void*);
    int*(__cdecl* existsICB)(int*, PS*, const char*, void*);
    int*(__cdecl* checkWrite)(int*, const char*);
    bool(__cdecl* isDir)(const char*);
    bool(__cdecl* isDirB)(const PS*, const char*);
    bool(__cdecl* unlink)(const char*);
    bool(__cdecl* unlinkB)(const PS*, const char*);
    bool(__cdecl* remove)(const char*);
    bool(__cdecl* removeB)(const PS*, const char*);
    bool(__cdecl* rename)(const char*, const char*);
    bool(__cdecl* renameB)(const PS*, const char*, const char*);
    bool(__cdecl* md)(const char*);
    bool(__cdecl* prune)(const char*);
    bool(__cdecl* getStamp)(long*, const char*);
    void(__cdecl* setReadonly)(const char*, bool);
    void(__cdecl* setReadonlyB)(const PS*, const char*, bool);
    bool(__cdecl* trueName)(char*);
    void*(__cdecl* getSubstS)(const char*);
    void*(__cdecl* getSubstM)(const void*);
    // Find
    void(__fastcall* findCtor)(void*, void*);
    void(__fastcall* findCopy)(void*, void*, const void*);
    void(__fastcall* findDtor)(void*, void*);
    void*(__fastcall* findAssign)(void*, void*, const void*);
    bool(__cdecl* findOpen)(void*, const char*);
    bool(__cdecl* findOpenB)(void*, const char*, const PS*);
    bool(__cdecl* findNext)(void*);
    void(__cdecl* findClose)(void*);
    void*(__cdecl* findPath)(const void*, void*);
    // File
    void(__fastcall* fCtor)(void*, void*);
    void(__fastcall* fDtor)(void*, void*);
    int*(__cdecl* fOpen)(void*, int*, const char*, unsigned long, int);
    unsigned long(__cdecl* fClose)(void*);
    unsigned long(__cdecl* fRead)(void*, void*, unsigned long, unsigned long);
    unsigned long(__cdecl* fWrite)(void*, const void*, unsigned long, unsigned long);
    unsigned long(__cdecl* fSize)(void*);
    // archives
    void(__cdecl* setCallBack)(bool(__cdecl*)(unsigned long, unsigned long));
    void(__cdecl* setDefBuild)(unsigned long);
    void(__cdecl* setDefProduct)(unsigned long);
};
static Api apis[2];

static void Load(Api& a, HMODULE m)
{
#define G(field, name) a.field = Get<decltype(a.field)>(m, name)
    G(subst, "?Subst" F "SAXABVPString@2@PBD_N@Z");
    G(substExists, "?SubstExists" F "SA" RES "ABVPString@2@@Z");
    G(substLink, "?SubstLink" F "SAXABVPString@2@0@Z");
    G(buildVersion, "?GetSubstBuildVersion" F "SAKABVPString@2@@Z");
    G(productCode, "?GetSubstProductCode" F "SAKABVPString@2@@Z");
    G(exists, "?Exists" F "SA" RES "PBD@Z");
    G(existsB, "?Exists" F "SA" RES "AAVPString@2@PBD@Z");
    G(existsIC, "?ExistsIgnoreCase" F "SA" RES "PBDAAUPath@12@@Z");
    G(existsICB, "?ExistsIgnoreCase" F "SA" RES "AAVPString@2@PBDAAUPath@12@@Z");
    G(checkWrite, "?CheckWriteMode" F "SA" RES "PBD@Z");
    G(isDir, "?IsDirectory" F "SA_NPBD@Z");
    G(isDirB, "?IsDirectory" F "SA_NABVPString@2@PBD@Z");
    G(unlink, "?Unlink" F "SA_NPBD@Z");
    G(unlinkB, "?Unlink" F "SA_NABVPString@2@PBD@Z");
    G(remove, "?Remove" F "SA_NPBD@Z");
    G(removeB, "?Remove" F "SA_NABVPString@2@PBD@Z");
    G(rename, "?Rename" F "SA_NPBD0@Z");
    G(renameB, "?Rename" F "SA_NABVPString@2@PBD1@Z");
    G(md, "?Md" F "SA_NPBD@Z");
    G(prune, "?Prune" F "SA_NPBD@Z");
    G(getStamp, "?GetStamp" F "SA_NAATTime@12@PBD@Z");
    G(setReadonly, "?SetReadonly" F "SAXPBD_N@Z");
    G(setReadonlyB, "?SetReadonly" F "SAXABVPString@2@PBD_N@Z");
    G(trueName, "?TrueName" F "SA_NPAD@Z");
    G(getSubstS, "?GetSubst@Path" F "SAPAVFileSubstitute@3@PBD@Z");
    G(getSubstM, "?GetSubst@Path" F "QBAPAVFileSubstitute@3@XZ");
    G(findCtor, "??0Find" F "QAE@XZ");
    G(findCopy, "??0Find" F "QAE@ABV012@@Z");
    G(findDtor, "??1Find" F "QAE@XZ");
    G(findAssign, "??4Find" F "QAEAAV012@ABV012@@Z");
    G(findOpen, "?Open" FI "QAA_NPBD@Z");
    G(findOpenB, "?Open" FI "QAA_NPBDABVPString@3@@Z");
    G(findNext, "?Next" FI "QAA_NXZ");
    G(findClose, "?Close" FI "QAAXXZ");
    G(findPath, "?GetNameAndPath" FI "QBA?AUPath@23@XZ");
    G(fCtor, "??0File@Jet@@QAE@XZ");
    G(fDtor, "??1File@Jet@@QAE@XZ");
    G(fOpen, "?Open" F "QAA" RES "PBDKH@Z");
    G(fClose, "?Close" F "QAAKXZ");
    G(fRead, "?Read" F "QAAKPAXKK@Z");
    G(fWrite, "?Write" F "QAAKPBXKK@Z");
    G(fSize, "?Size" F "QAAKXZ");
    G(setCallBack, "?SetCallBack" F "SAXP6A_NKK@Z@Z");
    G(setDefBuild, "?SetDefaultBuildVersion@FileSubArchive@Jet@@SAXK@Z");
    G(setDefProduct, "?SetDefaultProductCode@FileSubArchive@Jet@@SAXK@Z");
#undef G
}

// the original's PString cache, shared by both sides
static void** pstringCache;
static Node*(__cdecl* pstringCreate)(void*, const unsigned char*);
static void(__cdecl* pstringDestroy)(void*, Node*);
static PS Name(const std::string& s) { return pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(s.c_str())); }
static void Drop(PS n)
{
    if (n) pstringDestroy(*pstringCache, const_cast<Node*>(n));
}
static std::string Text(PS n) { return n ? reinterpret_cast<const char*>(n + 0x18) : "(null)"; }

static std::string tmpDir, binDir;
// each side its own current directory: the modules count their "%08X.T%08X" temp names separately and a
// directory substitute's compressed write leaves hidden temp files behind (original bug), so one side's leftovers
// would collide with the other side's next names
static void SideCwd(int s) { SetCurrentDirectoryA((tmpDir + "c" + std::to_string(s)).c_str()); }
static std::mt19937 rng;
static unsigned Pick(unsigned n) { return n ? static_cast<unsigned>(rng() % n) : 0; }

// side-specific directories in logged text become "<W>"
static std::string sideDirs[2];
static std::string Norm(std::string s)
{
    for (const std::string& d : sideDirs) {
        if (d.empty()) continue;
        for (size_t at; (at = s.find(d)) != std::string::npos;) s.replace(at, d.size(), "<W>");
    }
    return s;
}

static std::string Try(const std::function<std::string()>& fn)
{
    try {
        return fn();
    } catch (Jet::Exception& e) {
        return std::string("threw ") + e.Reason();
    } catch (...) {
        return "threw";
    }
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    std::printf("crash %08lx at %p (side %d) ours %p orig %p\n", e->ExceptionRecord->ExceptionCode,
                e->ExceptionRecord->ExceptionAddress, side, GetModuleHandleA("KernelStandard_7c.dll"),
                GetModuleHandleA("KernelStandard_7c_orig.dll"));
    std::exit(3);
}

static void WriteDisk(const std::string& path, const std::string& data)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w = 0;
    if (h != INVALID_HANDLE_VALUE) {
        WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &w, nullptr);
        CloseHandle(h);
    }
}

// deletes a tree (read-only bits cleared first)
static void Wipe(const std::string& dir)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::string n = fd.cFileName;
            if (n == "." || n == "..") continue;
            const std::string p = dir + "\\" + n;
            SetFileAttributesA(p.c_str(), FILE_ATTRIBUTE_NORMAL);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                Wipe(p);
            else
                DeleteFileA(p.c_str());
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir.c_str());
}

// the tree as text: names (sorted by the file system), sizes, read-only bits, content hashes
static void DumpTree(const std::string& dir, const std::string& rel, std::string& out)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::string> subdirs;
    do {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            subdirs.push_back(n);
            out += " [" + rel + n + "]";
            continue;
        }
        if (n.find(".T") == 8) continue;  // a temp file left by a failed pack (named by pid)
        std::string data;
        HANDLE f = CreateFileA((dir + "\\" + n).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            char b[4096];
            DWORD r;
            while (ReadFile(f, b, sizeof(b), &r, nullptr) && r) data.append(b, r);
            CloseHandle(f);
        }
        out += " " + rel + n + "=" + Num(data.size()) + (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? "r" : "") + ":" +
               Num(Fnv(data.data(), data.size()));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    for (const std::string& s : subdirs) DumpTree(dir + "\\" + s, rel + s + "\\", out);
}

static std::string RandomName()
{
    static const char* const stems[] = {"alpha", "Beta", "gamma", "DELTA", "e", "file", "x1", "long_name_here", "a b"};
    static const char* const exts[] = {".txt", ".dat", ".TXT", "", ".cfg", ".tga"};
    return std::string(stems[Pick(9)]) + (Pick(3) ? "" : Num(Pick(5))) + exts[Pick(6)];
}

// ---- the shared files of a scenario: two directories (a, b: with a "sub" directory each) and an archive

struct Shared {
    std::vector<std::pair<std::string, std::string>> aFiles, bFiles, arcFiles;  // relative name, content
};
static Shared shared;

static std::string RandomContent()
{
    const size_t n = Pick(4) ? Pick(0x300) : Pick(0x9000);
    std::string s(n, 'a');
    // no NUL: a compressed write through a directory substitute renames its temp file by a name garbled with the
    // first bytes read (original bug), and two bytes + NUL could spell the side's own temp counter by chance
    for (char& c : s) c = static_cast<char>(Pick(3) ? 'a' + Pick(4) : 1 + Pick(255));
    return s;
}

static int origSide = 1;
static void BuildShared(int n, const Api& orig)
{
    SideCwd(origSide);
    const std::string root = tmpDir + "sh";
    Wipe(root);
    CreateDirectoryA(root.c_str(), nullptr);
    shared = Shared();
    for (const char* d : {"a", "b"}) {
        const std::string dir = root + "\\" + d;
        CreateDirectoryA(dir.c_str(), nullptr);
        CreateDirectoryA((dir + "\\sub").c_str(), nullptr);
        auto& list = *d == 'a' ? shared.aFiles : shared.bFiles;
        const unsigned k = 2 + Pick(7);
        for (unsigned i = 0; i < k; ++i) {
            const std::string rel = (Pick(3) ? "" : "sub\\") + RandomName();
            const std::string c = RandomContent();
            WriteDisk(dir + "\\" + rel, c);
            list.push_back({rel, c});
        }
    }
    // the archive: written through the original's substitute, plain and compressed files
    const std::string arc = root + "\\x.ja";
    PS an = Name("kfsmake");
    orig.subst(&an, arc.c_str(), true);
    const unsigned k = 1 + Pick(8);
    for (unsigned i = 0; i < k; ++i) {
        const std::string rel = (Pick(3) ? "" : "sub\\") + RandomName();
        const std::string c = RandomContent();
        alignas(8) unsigned char f[0x200];
        orig.fCtor(f, nullptr);
        int r = -1;
        orig.fOpen(f, &r, ("kfsmake:" + rel).c_str(), Pick(2) ? 0x102 : 0x101, 0);
        if (r == 0) {
            orig.fWrite(f, c.data(), 1, static_cast<unsigned long>(c.size()));
            orig.fClose(f);
            shared.arcFiles.push_back({rel, c});
        }
        orig.fDtor(f, nullptr);
    }
    orig.subst(&an, nullptr, false);
    Drop(an);
    (void)n;
}

// names to ask about: the files there (and case variants), directories, missing ones
static std::vector<std::string> QueryNames()
{
    std::vector<std::string> v;
    for (auto* list : {&shared.aFiles, &shared.bFiles, &shared.arcFiles})
        for (auto& f : *list) {
            v.push_back(f.first);
            std::string up = f.first;
            for (char& c : up) c = static_cast<char>(Pick(2) ? toupper(c) : c);
            v.push_back(up);
        }
    for (const char* s : {"sub", "sub\\", "\\sub", "nosuch.txt", "sub\\nosuch", "", "\\", ".", "..\\a", "SUB"}) v.push_back(s);
    for (int i = 0; i < 6; ++i) v.push_back((Pick(2) ? "/" : "") + RandomName());
    return v;
}

// ---- read

// the whole entry; in the archive scenarios only the name up to its NUL (entries read from a .ja file carry what
// the writing module left after the name: stack and heap leftovers in the original)
static bool maskNames = false;
static std::string EntryText(const unsigned char* find)
{
    if (!maskNames) return Hex(find, 0x120);
    const char* const n = reinterpret_cast<const char*>(find);
    return "'" + std::string(n, strnlen(n, 0x104)) + "' " + Hex(find + 0x104, 0x1c);
}

static void RunFind(const Api& a, const std::string& pattern, const PS* base)
{
    alignas(8) unsigned char f[0x460];
    std::memset(f, 0xcd, sizeof(f));
    a.findCtor(f, nullptr);
    const bool ok = base ? a.findOpenB(f, pattern.c_str(), base) : a.findOpen(f, pattern.c_str());
    std::string line = "find " + Norm(pattern) + (base ? " @" + Text(*base) : "") + " " + Num(ok);
    int count = 0;
    for (bool more = ok; more && count < 200; more = a.findNext(f), ++count) {
        alignas(8) unsigned char p[0x110];
        a.findPath(f, p);
        Log(line + " #" + Num(count) + " " + EntryText(f) + " " + Norm(reinterpret_cast<const char*>(p)));
    }
    // the search state: the three paths
    Log(line + " end " + Norm(reinterpret_cast<const char*>(f + 0x124)) + "|" + Norm(reinterpret_cast<const char*>(f + 0x228)) +
        "|" + Norm(reinterpret_cast<const char*>(f + 0x32c)));
    if (Pick(3) == 0) a.findClose(f);
    a.findDtor(f, nullptr);
}

static void RunRead(unsigned seed)
{
    SideCwd(side);
    const Api& a = apis[side];
    rng.seed(seed);
    const std::string root = tmpDir + "sh\\";
    PS s = Name("kfsS"), t = Name("kfsT"), none = Name("kfsNone");
    const bool addA = Pick(2) != 0;
    // kfsT first or kfsS first: the list is sorted by node, not by order
    if (Pick(2)) {
        a.subst(&t, (root + "b\\").c_str(), Pick(2) != 0);
        a.subst(&s, (root + "a\\").c_str(), addA);
    } else {
        a.subst(&s, (root + "a\\").c_str(), addA);
        a.subst(&t, (root + "b\\").c_str(), Pick(2) != 0);
    }
    a.subst(&s, (root + "x.ja").c_str(), Pick(2) != 0);  // goes first
    Log("link " + Try([&] {
            a.substLink(&s, &t);
            return std::string("ok");
        }));
    Log("link none " + Try([&] {
            a.substLink(&s, &none);
            return std::string("ok");
        }));
    int r = -1;
    Log("exists " + Num(*a.substExists(&r, &s)) + Num(*a.substExists(&r, &t)) + Num(*a.substExists(&r, &none)));
    Log("version " + Num(a.buildVersion(&s)) + " " + Num(a.productCode(&s)) + " " + Num(a.buildVersion(&t)) + " " +
        Num(a.buildVersion(&none)));

    for (int round = 0; round < 2; ++round) {
        const std::vector<std::string> names = QueryNames();
        for (const std::string& n : names) {
            std::string line = "q " + n + " |";
            for (const char* sub : {"kfsS:", "kfsS:\\", "kfsT:", "kfsNone:"}) {
                const std::string p = sub + n;
                line += " " + Num(*a.exists(&r, p.c_str()));
                PS b = Pick(4) ? s : (Pick(2) ? t : nullptr);
                if (b) b = Name(Text(b));  // a reference of its own (Exists may replace it)
                line += Num(*a.existsB(&r, &b, n.c_str())) + Text(b);
                char found[0x104];
                std::memset(found, 0x5a, sizeof(found));
                line += Num(*a.existsIC(&r, p.c_str(), found)) + found;
                line += Num(*a.existsICB(&r, &b, n.c_str(), found)) + found;
                line += Num(a.isDir(p.c_str())) + Num(a.isDirB(&s, n.c_str())) + Num(a.isDirB(&t, n.c_str()));
                long stamp = -7;
                line += Num(a.getStamp(&stamp, p.c_str())) + "/" + Num(stamp);
                char tn[0x104];
                std::memset(tn, 0x5a, sizeof(tn));
                std::strcpy(tn, p.c_str());
                line += Num(a.trueName(tn)) + Norm(tn);
                line += Num(a.getSubstS(p.c_str()) != nullptr);
                Drop(b);
            }
            // plain paths
            const std::string plain = root + (Pick(2) ? "a\\" : "b\\") + n;
            line += " p" + Num(*a.exists(&r, plain.c_str())) + Num(a.isDir(plain.c_str())) + Num(*a.checkWrite(&r, plain.c_str()));
            long stamp = -7;
            line += Num(a.getStamp(&stamp, plain.c_str())) + "/" + Num(stamp);
            char tn[0x104];
            std::strcpy(tn, plain.c_str());
            line += Num(a.trueName(tn));
            Log(line);
        }
        // searches
        std::vector<std::string> patterns = {"kfsS:*.*", "kfsS:*", "kfsS:*.txt", "kfsS:sub\\*", "kfsS:\\sub\\*.*",
                                             "kfsS:SUB\\*.dat", "kfsS:s?b\\*", "kfsS:nosuch\\*", "kfsT:*",
                                             "kfsT:sub\\*.TXT", "kfsNone:*", "kfsS:*.cfg", "kfsS:a*"};
        patterns.push_back(root + "a\\*.*");
        patterns.push_back(root + "a\\sub\\*.t*");
        patterns.push_back(root + "*");
        patterns.push_back(root + "nosuch\\*");
        for (const std::string& p : patterns) RunFind(a, p, nullptr);
        for (const char* p : {"*.*", "sub\\*", "*.txt", "s*\\*"}) {
            RunFind(a, p, &s);
            RunFind(a, p, &t);
        }
        // the linked substitute goes: the link stays, empty
        if (round == 0) a.subst(&t, nullptr, false);
    }
    // copies of a search (fresh ones only: a copy shares the names given)
    {
        alignas(8) unsigned char f[0x460], g[0x460];
        std::memset(f, 0xcd, sizeof(f));
        std::memset(g, 0xab, sizeof(g));
        a.findCtor(f, nullptr);
        a.findCopy(g, nullptr, f);
        Log("copy " + Hex(g, 0x120) + " " + Num(*reinterpret_cast<long*>(g + 0x120)));
        a.findAssign(g, nullptr, f);
        a.findDtor(g, nullptr);
        a.findDtor(f, nullptr);
    }
    a.subst(&s, nullptr, false);
    Log("after " + Num(*a.substExists(&r, &s)) + Num(*a.substExists(&r, &t)) + Num(*a.exists(&r, "kfsS:sub")));
    Drop(s);
    Drop(t);
    Drop(none);
}

// ---- write: each side its own tree

static void RunWrite(unsigned seed)
{
    SideCwd(side);
    const Api& a = apis[side];
    rng.seed(seed);
    const std::string root = sideDirs[side];  // ...\w0 or w1
    Wipe(root);
    CreateDirectoryA(root.c_str(), nullptr);
    CreateDirectoryA((root + "\\d").c_str(), nullptr);
    CreateDirectoryA((root + "\\e").c_str(), nullptr);
    for (unsigned i = 0; i < 4; ++i) WriteDisk(root + "\\d\\" + RandomName(), RandomContent());
    PS w = Name("kfsW"), l = Name("kfsL"), ro = Name("kfsR");
    a.subst(&w, (root + "\\d\\").c_str(), true);
    a.subst(&ro, (root + "\\e\\").c_str(), false);
    a.substLink(&l, &w);
    int r = -1;
    for (int i = 0; i < 24; ++i) {
        const unsigned op = Pick(12);
        const char* const subs[] = {"kfsW:", "kfsL:", "kfsR:", "kfsW:\\"};
        const std::string base = subs[Pick(4)];
        const std::string n1 = (Pick(3) ? "" : (Pick(2) ? "sub\\" : "deep\\er\\")) + RandomName();
        const std::string n2 = (Pick(3) ? "" : "sub\\") + RandomName();
        std::string line = Num(op) + " " + base + n1 + " " + n2 + " ";
        switch (op) {
        case 0:
        case 1:
        case 2: {  // write plain / compressed / append
            const std::string c = RandomContent();
            alignas(8) unsigned char f[0x200];
            a.fCtor(f, nullptr);
            const unsigned long mode = op == 0 ? 0x101 : op == 1 ? 0x102 : 0x201;
            line += Try([&] {
                std::string s = Num(*a.fOpen(f, &r, (base + n1).c_str(), mode, 0));
                if (r == 0) {
                    s += " " + Num(a.fWrite(f, c.data(), 1, static_cast<unsigned long>(c.size())));
                    s += " " + Num(a.fClose(f));
                }
                return s;
            });
            a.fDtor(f, nullptr);
            break;
        }
        case 3: {  // read back
            alignas(8) unsigned char f[0x200];
            a.fCtor(f, nullptr);
            line += Try([&] {
                std::string s = Num(*a.fOpen(f, &r, (base + n1).c_str(), 0x10, 0));
                if (r == 0) {
                    std::vector<char> b(0x10000);
                    const unsigned long size = a.fSize(f);
                    const unsigned long items = size && size <= b.size() ? a.fRead(f, b.data(), size, 1) : 0;
                    s += " " + Num(size) + " " + Num(items) + " " + Num(Fnv(b.data(), items ? size : 0));
                    a.fClose(f);
                }
                return s;
            });
            a.fDtor(f, nullptr);
            break;
        }
        case 4:
            line += Num(a.md((base + n1).c_str()));
            break;
        case 5:
            line += Num(a.rename((base + n1).c_str(), (base + n2).c_str()));
            break;
        case 6:
            line += Num(a.unlink((base + n1).c_str())) + Num(a.removeB(&w, n2.c_str())) + Num(a.unlinkB(&l, n1.c_str()));
            break;
        case 7: {
            const bool on = Pick(2) != 0;
            a.setReadonlyB(Pick(2) ? &w : &l, n1.c_str(), on);
            line += Num(*a.checkWrite(&r, (root + "\\d\\" + n1).c_str()));
            break;
        }
        case 8:
            line += Num(a.renameB(&w, n1.c_str(), n2.c_str()));
            break;
        case 9: {  // plain paths
            const std::string p = root + "\\" + (Pick(2) ? "d\\" : "") + n1;
            line += Num(a.md(p.c_str())) + Num(a.isDir(p.c_str())) + Num(a.remove(p.c_str()));
            break;
        }
        case 10: {  // prune a plain directory (sometimes with a read-only file in it)
            const std::string p = root + "\\p" + Num(Pick(3));
            CreateDirectoryA(p.c_str(), nullptr);
            CreateDirectoryA((p + "\\q").c_str(), nullptr);
            WriteDisk(p + "\\q\\" + RandomName(), "x");
            if (Pick(2)) SetFileAttributesA((p + "\\q\\" + RandomName()).c_str(), FILE_ATTRIBUTE_READONLY);
            line += Num(a.prune(p.c_str())) + Num(GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES);
            break;
        }
        default: {  // plain rename / unlink / readonly
            const std::string p = root + "\\d\\" + n1, q = root + "\\d\\" + n2;
            line += Num(a.rename(p.c_str(), q.c_str()));
            a.setReadonly(q.c_str(), Pick(2) != 0);
            line += Num(*a.checkWrite(&r, q.c_str())) + Num(a.unlink(p.c_str()));
            break;
        }
        }
        if (getenv("KFS_TREEOPS")) {
            std::string tree;
            DumpTree(root, "", tree);
            line += " /" + tree;
        }
        Log(Norm(line));
    }
    a.subst(&l, nullptr, false);
    a.subst(&w, nullptr, false);
    a.subst(&ro, nullptr, false);
    Drop(w);
    Drop(l);
    Drop(ro);
    std::string tree;
    DumpTree(root, "", tree);
    Log("tree" + tree);
}

// ---- archives: each side its own .ja, new or built here, written and read through a substitute

static bool __cdecl Progress(unsigned long done, unsigned long written)
{
    Log("cb " + Num(done) + " " + Num(written));
    return true;
}

static std::string Word(unsigned long v) { return std::string(reinterpret_cast<const char*>(&v), 4); }

static std::string ArcName()
{
    // (by name, an archive with a directory finds "/" names only: the key is normalized, the record is not)
    static const char* const dirs[] = {"", "", "sub/", "sub/", "Sub/", "sub\\", "deep/er/", "Deep/er/", "Deep\\er\\"};
    return dirs[Pick(9)] + RandomName();
}

static std::string ArcContent()
{
    static const size_t edges[] = {0, 1, 0x3fff, 0x4000, 0x4001, 0x8000, 0xc001};
    if (Pick(4) == 0) {
        const size_t n = edges[Pick(7)];
        std::string s(n, 'a');
        for (char& c : s) c = static_cast<char>(Pick(3) ? 'a' + Pick(3) : Pick(256));
        return s;
    }
    return RandomContent();
}

// a File::Entry: name, size, length, crc, three times, attrib
static std::string EntryBytes(const std::string& name, unsigned long size, unsigned long length, unsigned short attrib)
{
    std::string e(0x120, '\0');
    std::memcpy(&e[0], name.c_str(), name.size() < 0x103 ? name.size() : 0x103);
    std::memcpy(&e[0x104], &size, 4);
    std::memcpy(&e[0x108], &length, 4);
    const unsigned long crc = rng(), t = Pick(3) ? 0 : 0x50000000u + Pick(1000);
    std::memcpy(&e[0x10c], &crc, 4);
    std::memcpy(&e[0x118], &t, 4);
    std::memcpy(&e[0x11c], &attrib, 2);
    return e;
}

// an archive file made here: "ARCH", INFO (version and magic drawn), "FIL " / "FIL!" / unknown chunks, and an
// index that is right, missing, or wrong (too many entries, or pointing at a file chunk); or a short / foreign file
static std::vector<std::string> builtNames;  // the files of the last archive built
static std::string BuildArchive()
{
    const unsigned kind = Pick(10);
    if (kind == 0) return "AR";
    if (kind == 1) return "JUNK" + std::string(40, 'x');
    static const unsigned long versions[] = {0x101, 0x102, 0x102, 0x101, 0x100, 0x103};
    const unsigned long version = versions[Pick(6)];
    const unsigned long magic = Pick(8) ? 0x41524330 : 0x41524331;
    builtNames.clear();
    std::string body;  // after "ARCH" + the INFO chunk (0x28 bytes): offsets are body.size() + 0x2c
    std::vector<std::string> index;
    const unsigned k = Pick(7);
    for (unsigned i = 0; i < k; ++i) {
        if (Pick(5) == 0) body += "JUNK" + Word(12) + std::string(12, 'j');
        const std::string name = ArcName();
        const std::string plain = ArcContent();
        std::string data;
        unsigned short attrib = 0x20;
        if (Pick(2)) {  // compressed: raw chunks (0x8000 | n) in a "CFIL" stream (4 bytes of header in 0x101)
            attrib |= 0x80;
            data = "CFIL";
            if (version != 0x101) data += Word(0x101) + Word(static_cast<unsigned long>(plain.size()));
            for (size_t at = 0; at < plain.size(); at += 0x4000) {
                const size_t n = plain.size() - at < 0x4000 ? plain.size() - at : 0x4000;
                const unsigned short h = static_cast<unsigned short>(0x8000 | n);
                data.append(reinterpret_cast<const char*>(&h), 2);
                data += plain.substr(at, n);
            }
        } else {
            data = plain;
        }
        const bool deleted = Pick(6) == 0;
        const std::string e = EntryBytes(name, static_cast<unsigned long>(plain.size()), static_cast<unsigned long>(data.size()), attrib);
        const unsigned long at = static_cast<unsigned long>(body.size()) + 0x2c + 8 + 0x120;
        body += std::string(deleted ? "FIL!" : "FIL ") + Word(static_cast<unsigned long>(0x120 + data.size())) + e + data;
        if (!deleted) builtNames.push_back(name);
        if (!deleted) index.push_back(Word(at) + Word(static_cast<unsigned long>(data.size())) + e);
    }
    const unsigned how = Pick(4);  // 0 none, 1 and 2 right, 3 wrong
    unsigned long count = 0, offset = 0;
    if (how && !index.empty()) {
        offset = static_cast<unsigned long>(body.size()) + 0x2c;
        count = static_cast<unsigned long>(index.size());
        std::string idx;
        for (const std::string& x : index) idx += x;
        body += "IDX " + Word(static_cast<unsigned long>(idx.size())) + idx;
        if (how == 3) {
            if (Pick(2))
                ++count;
            else
                offset = 0x2c;
        }
    }
    const unsigned long build = Pick(3) ? 0 : 1000 + Pick(9), product = Pick(3) ? 0 : 7 + Pick(3);
    std::string info = Word(magic) + Word(version) + Word(count) + Word(offset) + Word(build) + Word(product) + Word(0) + Word(0);
    return "ARCH" + std::string("INFO") + Word(0x20) + info + body;
}

// the archive file as text: chunk by chunk, every File::Entry's name only up to its NUL (the original writes stack
// and heap leftovers after it), data as hashes; an index's leftover tail (a shorter one written over it) as entries
static std::string DumpArchiveRaw(const std::string& path);
static std::string DumpArchive(const std::string& path)
{
    try {
        return DumpArchiveRaw(path);
    } catch (std::exception& e) {
        std::printf("dump: %s\n", e.what());
        throw;
    }
}
static std::string DumpArchiveRaw(const std::string& path)
{
    std::string b;
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "none";
    // (an unlink through a record with a garbage offset writes "FIL!" hundreds of MB out: size only)
    if (GetFileSize(f, nullptr) > 0x1000000) {
        const DWORD n = GetFileSize(f, nullptr);
        CloseHandle(f);
        return "huge " + Num(n);
    }
    char buf[4096];
    DWORD r;
    while (ReadFile(f, buf, sizeof(buf), &r, nullptr) && r) b.append(buf, r);
    CloseHandle(f);
    std::string out = Num(b.size());
    auto entry = [&](size_t at) {
        out += " '" + std::string(b.c_str() + at, strnlen(b.c_str() + at, 0x104)) + "' " + Hex(b.data() + at + 0x104, 0x1c);
    };
    // a file of less than 4 bytes is kept open: what is written goes after it
    size_t pos = b.size() >= 4 && b.compare(0, 4, "ARCH") == 0 ? 4 : b.size() > 8 && b.size() >= 2 && b.compare(0, 2, "AR") == 0 && b.compare(2, 4, "FIL ") == 0 ? 2 : 0;
    if (!pos) return out + " raw " + Num(Fnv(b.data(), b.size()));
    while (pos + 8 <= b.size()) {
        const std::string id = b.substr(pos, 4);
        unsigned long size;
        std::memcpy(&size, b.data() + pos + 4, 4);
        out += " [" + id + " " + Num(size);
        const size_t data = pos + 8;
        if (data + size > b.size()) {
            out += " cut]";  // (left over from an older layout: may hold the writer's leftovers after names)
            return out;
        }
        if (id == "FIL " || id == "FIL!") {
            if (size >= 0x120) {
                entry(data);
                out += " " + Num(Fnv(b.data() + data + 0x120, size - 0x120));
            }
        } else if (id == "IDX ") {
            for (size_t e = data; e + 0x128 <= data + size; e += 0x128) {
                out += " " + Hex(b.data() + e, 8);
                entry(e + 8);
            }
        } else {
            out += " " + Hex(b.data() + data, size < 0x40 ? size : 0x40) + " " + Num(Fnv(b.data() + data, size));
        }
        out += "]";
        pos = data + size;
        // after a directory: what an older, longer layout left there (dead, with the writer's leftovers in it)
        if (id == "IDX " && pos < b.size()) return out + " tail " + Num(b.size() - pos);
    }
    if (pos < b.size()) out += " rest " + Hex(b.data() + pos, b.size() - pos < 0x40 ? b.size() - pos : 0x40);
    return out;
}

static std::string Slashed(std::string s)
{
    for (char& c : s)
        if (c == '\\') c = '/';
    return s;
}

static std::string ReadWhole(const Api& a, const std::string& p, unsigned long mode)
{
    alignas(8) unsigned char f[0x200];
    a.fCtor(f, nullptr);
    std::string s = Try([&] {
        int r = -1;
        std::string t = Num(*a.fOpen(f, &r, p.c_str(), mode, 0));
        if (r == 0) {
            std::vector<char> b(0x20000);
            const unsigned long size = a.fSize(f);
            const unsigned long items = size && size <= b.size() ? a.fRead(f, b.data(), size, 1) : 0;
            t += " " + Num(size) + " " + Num(items) + " " + Num(Fnv(b.data(), items ? size : 0));
            a.fClose(f);
        }
        return t;
    });
    a.fDtor(f, nullptr);
    return s;
}

static std::string WriteWhole(const Api& a, const std::string& p, unsigned long mode, const std::string& c)
{
    alignas(8) unsigned char f[0x200];
    a.fCtor(f, nullptr);
    std::string s = Try([&] {
        int r = -1;
        std::string t = Num(*a.fOpen(f, &r, p.c_str(), mode, 0));
        if (r == 0) {
            t += " " + Num(a.fWrite(f, c.data(), 1, static_cast<unsigned long>(c.size())));
            t += " " + Num(a.fClose(f));
        }
        return t;
    });
    a.fDtor(f, nullptr);
    return s;
}

static void RunArchive(unsigned seed)
{
    SideCwd(side);
    maskNames = true;
    const Api& a = apis[side];
    rng.seed(seed);
    const std::string root = sideDirs[side] + "j";
    Wipe(root);
    CreateDirectoryA(root.c_str(), nullptr);
    const std::string arc = root + "\\t.ja";
    // an archive file the module turns away (not "ARCH" + INFO, a foreign magic or version) is closed but still takes
    // writes, which go to descriptor -1 and end the process in msvcr80 (both modules): such ones are mounted
    // read-only. (A file of less than 4 bytes stays open: what is written goes after it, and it is turned away the
    // next time.)
    auto rejected = [&] {
        std::string b;
        HANDLE h = CreateFileA(arc.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        char head[0x14];
        DWORD got = 0;
        ReadFile(h, head, sizeof(head), &got, nullptr);
        CloseHandle(h);
        if (got < 4) return false;
        unsigned long magic = 0, version = 0;
        if (got >= 0x14) {
            std::memcpy(&magic, head + 0xc, 4);
            std::memcpy(&version, head + 0x10, 4);
        }
        return std::memcmp(head, "ARCH", 4) != 0 || std::memcmp(head + 4, "INFO", got >= 8 ? 4 : 0) != 0 ||
               magic != 0x41524330 || version < 0x101 || version > 0x102;
    };
    unsigned long mountedVersion = 0;
    auto version = [&] { return mountedVersion; };
    auto readVersion = [&] {  // the version in the file's INFO, as the archive is mounted
        mountedVersion = 0;
        HANDLE h = CreateFileA(arc.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        char head[0x14];
        DWORD got = 0;
        ReadFile(h, head, sizeof(head), &got, nullptr);
        CloseHandle(h);
        if (got == sizeof(head)) std::memcpy(&mountedVersion, head + 0x10, 4);
    };
    builtNames.clear();
    if (Pick(4)) WriteDisk(arc, BuildArchive());
    Log("built " + DumpArchive(arc));
    a.setDefBuild(Pick(2) ? 0 : 2000 + Pick(50));
    a.setDefProduct(Pick(2) ? 0 : 3 + Pick(5));
    a.setCallBack(Pick(3) ? &Progress : nullptr);
    PS n = Name("kfsJ");
    bool writable = Pick(4) != 0 && !rejected();
    Log("mount " + Num(writable) + " " + Try([&] {
            a.subst(&n, arc.c_str(), writable);
            readVersion();
            return std::string("ok");
        }));
    std::vector<std::string> names;
    for (int i = 0; i < 6; ++i) names.push_back(ArcName());
    std::vector<std::string> known = builtNames;
    int r = -1;
    for (int i = 0; i < 30; ++i) {
        const unsigned op = Pick(14);
        // reads and the rest mostly name files that are there
        const std::string n1 = op > 2 && !known.empty() && Pick(3) ? known[Pick(static_cast<unsigned>(known.size()))]
                                                                  : Pick(4) ? names[Pick(6)] : ArcName();
        std::string line = Num(op) + " " + n1 + " ";
        const std::string p = "kfsJ:" + std::string(Pick(6) ? "" : "\\") + n1;
        // writes and unlinks with '/' only: unlinking a '\' path drops the record but not its name (the hash key
        // erased is not normalized), and a later lookup of that name reads the freed record in the original, whose
        // memory may have been reused by then (the result depends on its heap; ours keeps the record)
        std::string slashed = p;
        for (size_t k = 6; k < slashed.size(); ++k)
            if (slashed[k] == '\\') slashed[k] = '/';
        if (getenv("KFS_TRACE")) std::printf("%d> %s\n", side, line.c_str());
        switch (op) {
        case 0:
        case 1:
        case 2: {
            static const unsigned long modes[] = {0x101, 0x102, 0x201, 0x301, 0x102, 0x101};
            unsigned long mode = modes[Pick(6)];
            // a compressed file written to a version 0x101 archive gets the 12-byte "CFIL" header, but is read back
            // with the 4-byte one of such archives: its chunk headers are garbage and BPE::Decomp runs off (both
            // modules)
            if (version() == 0x101 && (mode & 0xf) == 2) mode = (mode & ~0xful) | 1;
            const std::string w = WriteWhole(a, slashed, mode, ArcContent());
            if (w.compare(0, 2, "0 ") == 0) known.push_back(n1);
            line += w;
            break;
        }
        case 3:
        case 4: {
            static const unsigned long modes[] = {0x10, 0x20, 0x30};
            line += ReadWhole(a, p, modes[Pick(3)]);
            break;
        }
        case 5: {
            line += Num(*a.exists(&r, p.c_str())) + Num(a.isDir(p.c_str()));
            for (const char* d : {"kfsJ:sub", "kfsJ:Sub\\", "kfsJ:deep/er", "kfsJ:deep", "kfsJ:", "kfsJ:nosuch"})
                line += Num(*a.exists(&r, d)) + Num(a.isDir(d));
            long stamp = -7;
            line += " " + Num(a.getStamp(&stamp, p.c_str())) + "/" + Num(stamp);
            char tn[0x104];
            std::strcpy(tn, p.c_str());
            line += Num(a.trueName(tn)) + tn;
            break;
        }
        case 6:
            line += Num(a.unlink(slashed.c_str()));
            break;
        case 7:
            line += Num(a.rename(p.c_str(), ("kfsJ:" + ArcName()).c_str())) + Num(a.md(p.c_str()));
            break;
        case 8: {  // two files read at once (the archive's descriptor shared)
            alignas(8) unsigned char f[0x200], g[0x200];
            a.fCtor(f, nullptr);
            a.fCtor(g, nullptr);
            const std::string q = "kfsJ:" + names[Pick(6)];
            int r2 = -1;
            line += Try([&] {
                std::string t = Num(*a.fOpen(f, &r, p.c_str(), 0x10, 0)) + Num(*a.fOpen(g, &r2, q.c_str(), 0x10, 0));
                std::vector<char> x(0x20000), y(0x20000);
                if (r == 0 && r2 == 0) {
                    const unsigned long half = a.fSize(f) / 2;
                    const unsigned long gs = a.fSize(g);
                    const unsigned long i1 = half && half <= x.size() ? a.fRead(f, x.data(), 1, half) : 0;
                    const unsigned long i2 = gs && gs <= y.size() ? a.fRead(g, y.data(), 1, gs) : 0;
                    const unsigned long i3 = a.fRead(f, x.data() + i1, 1, 0x100);
                    t += " " + Num(i1) + " " + Num(i2) + " " + Num(i3) + " " + Num(Fnv(x.data(), i1 + i3)) + " " + Num(Fnv(y.data(), i2));
                }
                if (r == 0) a.fClose(f);
                if (r2 == 0) a.fClose(g);
                return t;
            });
            a.fDtor(f, nullptr);
            a.fDtor(g, nullptr);
            break;
        }
        case 9: {  // a search, with a file written in the middle of it
            alignas(8) unsigned char f[0x460];
            std::memset(f, 0xcd, sizeof(f));
            a.findCtor(f, nullptr);
            static const char* const patterns[] = {"*.*", "*", "sub\\*", "deep\\*", "*.txt", "SUB/*.*", "deep/er/*", "s*/*"};
            const std::string pat = patterns[Pick(8)];
            // (by the substitute: "kfsJ:pattern" finds nothing in an archive, in both modules)
            bool more = Pick(5) ? a.findOpenB(f, pat.c_str(), &n) : a.findOpen(f, ("kfsJ:" + pat).c_str());
            line += pat + " " + Num(more);
            const int at = static_cast<int>(Pick(4));
            for (int k = 0; more && k < 200; ++k, more = a.findNext(f)) {
                if (k == at) line += " w(" + WriteWhole(a, Slashed("kfsJ:" + ArcName()), 0x101, ArcContent()) + ")";
                line += " #" + EntryText(f);
            }
            a.findDtor(f, nullptr);
            break;
        }
        case 10:
            for (const char* pat : {"*.*", "sub\\*", "s*\\*", "Deep\\er\\*.*", "*"}) RunFind(a, pat, &n);
            RunFind(a, "kfsJ:*", nullptr);
            break;
        case 11:
            line += Num(a.buildVersion(&n)) + " " + Num(a.productCode(&n));
            break;
        case 12: {  // remount (a written archive with a directory rewrites it)
            line += Try([&] {  // (throws when the last mount failed: no such substitute)
                a.subst(&n, nullptr, false);
                return std::string("");
            });
            line += DumpArchive(arc);
            writable = Pick(4) != 0 && !rejected();
            line += " " + Num(writable) + " " + Try([&] {
                        a.subst(&n, arc.c_str(), writable);
            readVersion();
                        return std::string("ok");
                    });
            break;
        }
        default:
            line += ReadWhole(a, "kfsJ:" + names[Pick(6)], 0x10);
            break;
        }
        Log(Norm(line));
        // an unlink through a record with a garbage offset can write "FIL!" hundreds of MB out; scanning that (8
        // bytes at a time, both modules) takes hours: the scenario ends there
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExA(arc.c_str(), GetFileExInfoStandard, &fa) && (fa.nFileSizeHigh || fa.nFileSizeLow > 0x1000000)) {
            Log("huge");
            break;
        }
    }
    Log("unmount " + Try([&] {
            a.subst(&n, nullptr, false);
            return std::string("ok");
        }));
    Drop(n);
    a.setCallBack(nullptr);
    Log("final " + DumpArchive(arc));
    maskNames = false;
}

// ---- real archives: every entry listed and read

static unsigned long long realFiles = 0, realBytes = 0;

// the directories of an archive, read from its "FIL " records ({id, size} chunks after "ARCH"; each record starts
// with the File::Entry, its name the path inside the archive): archive searches give no directory entries
static std::vector<std::string> ArchiveDirs(const std::string& path)
{
    std::vector<std::string> dirs;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return dirs;
    char magic[4];
    DWORD r = 0;
    ReadFile(h, magic, 4, &r, nullptr);
    struct {
        char id[4];
        unsigned long size;
    } chunk;
    while (ReadFile(h, &chunk, 8, &r, nullptr) && r == 8) {
        LARGE_INTEGER next;
        LARGE_INTEGER zero = {};
        SetFilePointerEx(h, zero, &next, FILE_CURRENT);
        next.QuadPart += chunk.size;
        if (std::memcmp(chunk.id, "FIL ", 4) == 0) {
            char name[0x104];
            ReadFile(h, name, sizeof(name), &r, nullptr);
            name[0x103] = 0;
            std::string d = name;
            const size_t at = d.find_last_of("\\/");
            d = at == std::string::npos ? "" : d.substr(0, at + 1);
            bool seen = false;
            for (const std::string& x : dirs) seen = seen || x == d;
            if (!seen) dirs.push_back(d);
        }
        SetFilePointerEx(h, next, nullptr, FILE_BEGIN);
    }
    CloseHandle(h);
    return dirs;
}

// every entry of one directory of the substitute, listed by Find (by name, or with KFS_BYPATH as "name:dir\*.*")
// and read whole through File
static void ListDir(const Api& a, const std::string& name, const std::string& dir)
{
    alignas(8) unsigned char f[0x460];
    std::memset(f, 0xcd, sizeof(f));
    a.findCtor(f, nullptr);
    std::vector<char> buf;
    PS base = Name(name);
    const bool byName = getenv("KFS_BYPATH") == nullptr;
    for (bool more = byName ? a.findOpenB(f, (dir + "*.*").c_str(), &base) : a.findOpen(f, (name + ":" + dir + "*.*").c_str());
         more; more = a.findNext(f)) {
        const char* const n = reinterpret_cast<const char*>(f);
        std::string line = dir + n + " " + Hex(f + 0x104, 0x1c);
        alignas(8) unsigned char file[0x200];
        a.fCtor(file, nullptr);
        int r = -1;
        a.fOpen(file, &r, (name + ":" + dir + n).c_str(), 0x10, 0);
        line += " " + Num(r);
        if (r == 0) {
            const unsigned long size = a.fSize(file);
            buf.resize(size + 1);
            const unsigned long items = size ? a.fRead(file, buf.data(), size, 1) : 0;  // (data, item size, count)
            const unsigned long got = items ? size : 0;
            line += " " + Num(size) + " " + Num(items) + " " + Num(Fnv(buf.data(), got));
            a.fClose(file);
            if (side == 0) {
                ++realFiles;
                realBytes += got;
            }
        }
        a.fDtor(file, nullptr);
        Log(line);
    }
    a.findDtor(f, nullptr);
    Drop(base);
}

static void RunReal()
{
    SideCwd(side);
    const Api& a = apis[side];
    std::vector<std::string> archives = {binDir + "\\..\\Base.ja", binDir + "\\..\\Data.ja"};
    if (getenv("KFS_BUILTIN"))
        for (char c = 'a'; c <= 'h'; ++c) archives.push_back(binDir + "\\..\\Builtin\\ts12" + c + ".ja");
    for (const std::string& path : archives) {
        PS n = Name("kfsreal");
        a.subst(&n, path.c_str(), false);
        Log("real " + path.substr(path.rfind('\\') + 1) + " " + Num(a.buildVersion(&n)) + " " + Num(a.productCode(&n)));
        const std::vector<std::string> dirs = ArchiveDirs(path);
        Log("dirs " + Num(dirs.size()));
        for (const std::string& d : dirs) ListDir(a, "kfsreal", d);
        a.subst(&n, nullptr, false);
        Drop(n);
    }
}

static unsigned long long bad = 0;
static unsigned long long Compare(const char* what, int n)
{
    const size_t lines = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
    unsigned long long checks = 0;
    for (size_t i = 0; i < lines; ++i) {
        ++checks;
        const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
        const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
        if (x != y) {
            if (++bad <= 8)
                std::printf("%s %d line %zu:\n  ours %s\n  orig %s\n", what, n, i, x.substr(0, 1500).c_str(), y.substr(0, 1500).c_str());
            break;
        }
    }
    if (getenv("KFS_DUMP"))
        for (int s = 0; s < 2; ++s)
            for (const std::string& l : logs[s]) std::printf("%s %d %c| %s\n", what, n, s ? 'o' : 'u', l.substr(0, 300).c_str());
    logs[0].clear();
    logs[1].clear();
    return checks;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_ks_filesub <dir> [scenarios]\n");
        return 2;
    }
    binDir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 200;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(binDir.c_str());
    HMODULE orig = LoadLibraryA((binDir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((binDir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", binDir.c_str());
        return 2;
    }
    const bool swap = getenv("KFS_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours);
    Load(apis[1], swap ? ours : orig);
    const Api& origApi = apis[swap ? 0 : 1];
    origSide = swap ? 0 : 1;
    pstringCache = Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringCreate = Get<decltype(pstringCreate)>(orig, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringDestroy = Get<decltype(pstringDestroy)>(orig, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    {
        char t[MAX_PATH];
        GetTempPathA(MAX_PATH, t);
        tmpDir = std::string(t) + "kfs\\";
        CreateDirectoryA(tmpDir.c_str(), nullptr);
        CreateDirectoryA((tmpDir + "c0").c_str(), nullptr);
        CreateDirectoryA((tmpDir + "c1").c_str(), nullptr);
        SetCurrentDirectoryA(tmpDir.c_str());
        sideDirs[0] = tmpDir + "w0";
        sideDirs[1] = tmpDir + "w1";
    }
    if (getenv("KFS_PROBE")) {  // one compressed write through a directory substitute, each module
        for (side = 0; side < 2; ++side) {
            const Api& a = apis[side];
            const std::string d = tmpDir + "probe" + Num(side);
            Wipe(d);
            CreateDirectoryA(d.c_str(), nullptr);
            PS w = Name("kfsP");
            a.subst(&w, (d + "\\").c_str(), true);
            alignas(8) unsigned char f[0x200];
            a.fCtor(f, nullptr);
            int r = -1;
            a.fOpen(f, &r, "kfsP:x.dat", 0x102, 0);
            const char text[] = "hello hello hello";
            const unsigned long wr = a.fWrite(f, text, 1, sizeof(text));
            std::printf("side %d open %d write %lu path %s fd %d\n", side, r, wr, reinterpret_cast<char*>(f + 0x38), *reinterpret_cast<int*>(f));
            const unsigned long c = a.fClose(f);
            std::string tree;
            DumpTree(d, "", tree);
            std::printf("  close %lu errno? tree%s attr %lx\n", c, tree.c_str(), GetFileAttributesA((d + "\\x.dat").c_str()));
            a.fDtor(f, nullptr);
            a.subst(&w, nullptr, false);
            Drop(w);
        }
        return 0;
    }
    if (getenv("KFS_APROBE")) {  // one file written to a new archive, read back and searched, each module
        for (side = 0; side < 2; ++side) {
            SideCwd(side);
            const Api& a = apis[side];
            const std::string arc = sideDirs[side] + "p.ja";
            DeleteFileA(arc.c_str());
            PS n = Name("kfsP");
            a.subst(&n, arc.c_str(), true);
            for (const char* name : {"abc.txt", "sub\\def.txt", "sub/ghi.txt"}) {
                std::printf("side %d write %s: %s\n", side, name, WriteWhole(a, std::string("kfsP:") + name, 0x101, "hello").c_str());
                std::printf("  read: %s\n", ReadWhole(a, std::string("kfsP:") + name, 0x10).c_str());
            }
            for (const char* pat : {"kfsP:*.*", "kfsP:*", "kfsP:sub\\*", "kfsP:sub/*.txt"}) {
                alignas(8) unsigned char f[0x460];
                a.findCtor(f, nullptr);
                std::string s;
                for (bool more = a.findOpen(f, pat); more; more = a.findNext(f)) s += std::string(" ") + reinterpret_cast<char*>(f);
                a.findDtor(f, nullptr);
                std::printf("  find %s:%s\n", pat, s.c_str());
            }
            for (const char* pat : {"*.*", "*", "sub\\*", "sub/*.txt"}) {
                alignas(8) unsigned char f[0x460];
                a.findCtor(f, nullptr);
                std::string s;
                for (bool more = a.findOpenB(f, pat, &n); more; more = a.findNext(f)) s += std::string(" ") + reinterpret_cast<char*>(f);
                a.findDtor(f, nullptr);
                std::printf("  find @%s:%s\n", pat, s.c_str());
            }
            a.subst(&n, nullptr, false);
            a.subst(&n, arc.c_str(), false);
            std::printf("  remounted read-only: %s | %s\n", ReadWhole(a, "kfsP:abc.txt", 0x10).c_str(), ReadWhole(a, "kfsP:sub/ghi.txt", 0x10).c_str());
            a.subst(&n, nullptr, false);
            Drop(n);
        }
        return 0;
    }
    unsigned long long checks = 0;
    if (!getenv("KFS_NOREAL")) {
        for (side = 0; side < 2; ++side) RunReal();
        checks += Compare("real", 0);
        std::printf("real archives: %llu files, %llu bytes read on each side\n", realFiles, realBytes);
    }
    const int from = getenv("KFS_FROM") ? std::atoi(getenv("KFS_FROM")) : 0;
    for (int n = from; n < scenarios; ++n) {
        rng.seed(0x5eed1000u + n);
        BuildShared(n, origApi);
        const unsigned seed = rng();
        for (side = 0; side < 2; ++side) RunRead(seed);
        checks += Compare("read", n);
        for (side = 0; side < 2; ++side) RunWrite(seed);
        checks += Compare("write", n);
        for (side = 0; side < 2; ++side) RunArchive(seed + 1);
        checks += Compare("archive", n);
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
