// Differential test of KernelStandard unit U7c2 (StringTables.cpp). The base class is abstract, so each side's
// object gets a copy of that side's vftable whose pure slots are this test's functions: they log every call
// (table and entry names, ids, flags, the entry data read through that side's traits) and act as a table class
// would (SaveConfigData / SaveBin fill the tree / buffer from the scenario's model). Each scenario draws a model of
// tables and entries, then once for both sides: a config tree of it with noise (groups, wrong kinds, missing or
// zero ids, the missing-key node as a child), its text file (the original's ConfigParser::Save, sometimes cut),
// and a binary image (sometimes cut or damaged). Each side reads them through every AddConfigData / AddBin
// overload, saves through every SaveConfigData / SaveBin overload and reads its own output back, and runs the
// copy / assignment, CreateNode, the Scan functions and the traits directly. The logs must match.
//
// usage: diff_ks_stringtables.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
// KST_SWAP: the sides swapped; KST_DUMP: every log line.
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
static const bool trace = getenv("KST_TRACE") != nullptr;
static void Log(const std::string& s)
{
    if (trace) std::printf("%d> %s\n", side, s.substr(0, 300).c_str());
    logs[side].push_back(s);
}

using Node = unsigned char;
static std::string NText(const Node* n) { return n ? std::string("'") + reinterpret_cast<const char*>(n + 0x18) + "'" : "(null)"; }
static std::string Hex(const std::string& b)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;
    for (unsigned char c : b) {
        s += digits[c >> 4];
        s += digits[c & 15];
    }
    return s;
}
static std::string Num(long long v) { return std::to_string(v); }

// ---- a ConfigData tree as raw memory (see diff_ks_configdata.cpp)
struct Chunk {
    Chunk* next;
    Chunk* prev;
    uint32_t count;
    unsigned char* items[13];
};
static int Type(const unsigned char* c) { return *reinterpret_cast<const int*>(c + 4); }
static void* Data(const unsigned char* c) { return *reinterpret_cast<void* const*>(c + 8); }
static Chunk* Head(const unsigned char* c) { return *reinterpret_cast<Chunk* const*>(c + 0xc); }
static unsigned char* unknownItem;
static std::vector<unsigned char*> Children(const unsigned char* c)
{
    std::vector<unsigned char*> v;
    if (Chunk* const h = Head(c)) {
        Chunk* k = h;
        do {
            for (uint32_t i = 0; i < k->count; ++i) v.push_back(k->items[i]);
            k = k->next;
        } while (k != h);
    }
    return v;
}
static std::string Value(const unsigned char* c)
{
    const int t = Type(c);
    const void* d = Data(c);
    char buf[64];
    switch (t) {
    case 0: return d ? "none!" : "none";
    case 1: std::snprintf(buf, sizeof(buf), "int %d", static_cast<int>(reinterpret_cast<intptr_t>(d))); return buf;
    case 2: std::snprintf(buf, sizeof(buf), "float %08x", static_cast<unsigned>(reinterpret_cast<uintptr_t>(d))); return buf;
    case 3: return "str " + NText(static_cast<const Node*>(d));
    default: return "type " + std::to_string(t);
    }
}
static void DumpTree(const unsigned char* c, int depth, std::string& s)
{
    if (!c) {
        s += "(no node)";
        return;
    }
    if (c == unknownItem) {
        s += "{unknownItem}";
        return;
    }
    s += "{" + std::to_string(depth) + " " + NText(*reinterpret_cast<Node* const*>(c)) + " " + Value(c);
    if (Chunk* const h = Head(c)) {
        s += " chunks";
        Chunk* k = h;
        do {
            s += " " + std::to_string(k->count);
            k = k->next;
        } while (k != h);
    }
    for (unsigned char* k : Children(c)) DumpTree(k, depth + 1, s);
    s += "}";
}

// ---- strings as raw memory
static std::string AText(const void* s)  // an AnsiString / String: {capacity, length, text}
{
    const unsigned* const b = *static_cast<const unsigned* const*>(s);
    return "[" + std::to_string(b[0] != 0) + " " + std::to_string(b[1]) + " '" + std::string(reinterpret_cast<const char*>(b + 2), b[1]) + "']";
}
static std::string UText(const void* u)  // a UnicodeString: {text, capacity, length}
{
    const uint16_t* const t = *static_cast<const uint16_t* const*>(u);
    const unsigned cap = static_cast<const unsigned*>(u)[1], len = static_cast<const unsigned*>(u)[2];
    std::string s = "<" + std::to_string(cap) + " " + std::to_string(len) + (t ? " " : " null ");
    for (unsigned i = 0; t && i < len && i < 200; ++i) {
        char b[8];
        std::snprintf(b, sizeof(b), "%04x", t[i]);
        s += b;
    }
    return s + ">";
}

// ---- exports
#define RES "?AV?$ResultClass@$1?results@StringTablesBase@Jet@@2QBUResultTable@2@BW4Results@StringTablesBase@Jet@@@2@"
struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void*(__fastcall* copy)(void*, void*, const void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    void(__fastcall* dtor)(void*, void*);
    void* const* vft;
    bool(__cdecl* resolving)(const void*);
    int*(__cdecl* addCfgBase)(void*, int*, Node* const*, const char*, bool);
    int*(__cdecl* addCfgPath)(void*, int*, const char*, bool);
    int*(__cdecl* addCfgBuf)(void*, int*, void*, unsigned, bool);
    int*(__cdecl* addCfg)(void*, int*, const void*, bool);
    int*(__cdecl* addBinBase)(void*, int*, Node* const*, const char*, bool);
    int*(__cdecl* addBinPath)(void*, int*, const char*, bool);
    int*(__cdecl* addBinFile)(void*, int*, void*, bool);
    int*(__cdecl* addBinBuf)(void*, int*, void*, bool);
    int*(__cdecl* saveCfgBase)(void*, int*, Node* const*, const char*, unsigned long);
    int*(__cdecl* saveCfgPath)(void*, int*, const char*, unsigned long);
    int*(__cdecl* saveBinBase)(void*, int*, Node* const*, const char*, unsigned long);
    int*(__cdecl* saveBinPath)(void*, int*, const char*, unsigned long);
    int*(__cdecl* saveBinFile)(void*, int*, void*, unsigned long);
    unsigned char*(__cdecl* createNode)(void*, const char*);
    int*(__fastcall* scan)(void*, void*, int*, const void*, bool);
    int*(__fastcall* scanNode)(void*, void*, int*, const void*, void*, bool);
    int*(__fastcall* scanTable)(void*, void*, int*, const void*, void*, const void*, void*, bool);
    void(__cdecl* logErr)(const char*, ...);
    void(__cdecl* logWarn)(const char*, ...);
    void*(__fastcall* traitsAssign)(void*, void*, const void*);
    char*(__cdecl* newKey)(void*, const char* const&);
    char*(__cdecl* newKeyPtr)(void*, const char*);
    void(__cdecl* deleteKey)(void*, char*);
    unsigned(__cdecl* hash)(const void*, const char*);
    int(__cdecl* compare)(const void*, const char*, const char*);
    bool(__cdecl* toConfig)(void*, void*, const void*);
    bool(__cdecl* toBuffer)(void*, void*, const void*);
    bool(__cdecl* fromConfig)(void*, const void*, void*);
    bool(__cdecl* fromBuffer)(void*, void*, void*);
};
static Api apis[2];
static void Load(Api& a, HMODULE m)
{
    a.ctor = Get<decltype(a.ctor)>(m, "??0StringTablesBase@Jet@@QAE@XZ");
    a.copy = Get<decltype(a.copy)>(m, "??0StringTablesBase@Jet@@QAE@ABV01@@Z");
    a.assign = Get<decltype(a.assign)>(m, "??4StringTablesBase@Jet@@QAEAAV01@ABV01@@Z");
    a.dtor = Get<decltype(a.dtor)>(m, "??1StringTablesBase@Jet@@UAE@XZ");
    a.vft = Get<void* const*>(m, "??_7StringTablesBase@Jet@@6B@");
    a.resolving = Get<decltype(a.resolving)>(m, "?IsNamesResolving@StringTablesBase@Jet@@QBA_NXZ");
    a.addCfgBase = Get<decltype(a.addCfgBase)>(m, "?AddConfigData@StringTablesBase@Jet@@UAA" RES "ABVPString@2@PBD_N@Z");
    a.addCfgPath = Get<decltype(a.addCfgPath)>(m, "?AddConfigData@StringTablesBase@Jet@@UAA" RES "PBD_N@Z");
    a.addCfgBuf = Get<decltype(a.addCfgBuf)>(m, "?AddConfigData@StringTablesBase@Jet@@UAA" RES "PAXI_N@Z");
    a.addCfg = Get<decltype(a.addCfg)>(m, "?AddConfigData@StringTablesBase@Jet@@UAA" RES "ABVConfigData@2@_N@Z");
    a.addBinBase = Get<decltype(a.addBinBase)>(m, "?AddBin@StringTablesBase@Jet@@UAA" RES "ABVPString@2@PBD_N@Z");
    a.addBinPath = Get<decltype(a.addBinPath)>(m, "?AddBin@StringTablesBase@Jet@@UAA" RES "PBD_N@Z");
    a.addBinFile = Get<decltype(a.addBinFile)>(m, "?AddBin@StringTablesBase@Jet@@UAA" RES "AAVFile@2@_N@Z");
    a.addBinBuf = Get<decltype(a.addBinBuf)>(m, "?AddBin@StringTablesBase@Jet@@UAA" RES "AAVDynamicBuffer@2@_N@Z");
    a.saveCfgBase = Get<decltype(a.saveCfgBase)>(m, "?SaveConfigData@StringTablesBase@Jet@@UAA" RES "ABVPString@2@PBDK@Z");
    a.saveCfgPath = Get<decltype(a.saveCfgPath)>(m, "?SaveConfigData@StringTablesBase@Jet@@UAA" RES "PBDK@Z");
    a.saveBinBase = Get<decltype(a.saveBinBase)>(m, "?SaveBin@StringTablesBase@Jet@@UAA" RES "ABVPString@2@PBDK@Z");
    a.saveBinPath = Get<decltype(a.saveBinPath)>(m, "?SaveBin@StringTablesBase@Jet@@UAA" RES "PBDK@Z");
    a.saveBinFile = Get<decltype(a.saveBinFile)>(m, "?SaveBin@StringTablesBase@Jet@@UAA" RES "AAVFile@2@K@Z");
    a.createNode = Get<decltype(a.createNode)>(m, "?CreateNode@StringTablesBase@Jet@@KAAAVConfigData@2@AAV32@PBD@Z");
    a.scan = Get<decltype(a.scan)>(m, "?ScanConfigData@StringTablesBase@Jet@@IAE" RES "ABVConfigData@2@_N@Z");
    a.scanNode = Get<decltype(a.scanNode)>(m, "?ScanConfigData@StringTablesBase@Jet@@IAE" RES "ABVConfigData@2@AAVString@2@_N@Z");
    a.scanTable = Get<decltype(a.scanTable)>(m, "?ScanTableConfigData@StringTablesBase@Jet@@IAE" RES "ABVConfigData@2@PAXABVString@2@AAV52@_N@Z");
    a.logErr = Get<decltype(a.logErr)>(m, "?LogErr@StringTablesBase@Jet@@KAXPBDZZ");
    a.logWarn = Get<decltype(a.logWarn)>(m, "?LogWarn@StringTablesBase@Jet@@KAXPBDZZ");
    a.traitsAssign = Get<decltype(a.traitsAssign)>(m, "??4StringTablesDefaultTraits@Jet@@QAEAAU01@ABU01@@Z");
    a.newKey = Get<decltype(a.newKey)>(m, "?NewKey@StringTablesDefaultTraits@Jet@@QAAPADABQBD@Z");
    a.newKeyPtr = Get<decltype(a.newKeyPtr)>(m, "?NewKeyFromCharPtr@StringTablesDefaultTraits@Jet@@QAAPADPBD@Z");
    a.deleteKey = Get<decltype(a.deleteKey)>(m, "?DeleteKey@StringTablesDefaultTraits@Jet@@QAAXPAD@Z");
    a.hash = Get<decltype(a.hash)>(m, "?GetHashVal@StringTablesDefaultTraits@Jet@@QBAIPBD@Z");
    a.compare = Get<decltype(a.compare)>(m, "?Compare@StringTablesDefaultTraits@Jet@@QBAHPBD0@Z");
    a.toConfig = Get<decltype(a.toConfig)>(m, "?DataToConfig@StringTablesDefaultTraits@Jet@@QAA_NAAVConfigData@2@ABVUnicodeString@2@@Z");
    a.toBuffer = Get<decltype(a.toBuffer)>(m, "?DataToDynamicBuffer@StringTablesDefaultTraits@Jet@@QAA_NAAVDynamicBuffer@2@ABVUnicodeString@2@@Z");
    a.fromConfig = Get<decltype(a.fromConfig)>(m, "?DataFromConfig@StringTablesDefaultTraits@Jet@@QAA_NABVConfigData@2@AAVUnicodeString@2@@Z");
    a.fromBuffer = Get<decltype(a.fromBuffer)>(m, "?DataFromDynamicBuffer@StringTablesDefaultTraits@Jet@@QAA_NAAVDynamicBuffer@2@AAVUnicodeString@2@@Z");
}

// the original's own helpers (the same for both sides)
static void** pstringCache;
static Node* (__cdecl* pstringCreate)(void*, const unsigned char*);
static void(__cdecl* pstringDestroy)(void*, Node*);
static void* (__cdecl* allocNew)(unsigned);
static void(__cdecl* allocDelete)(void*);
static void* (__cdecl* cdNew)(size_t);
static void(__cdecl* cdDelete)(void*);
static void* (__fastcall* cdCtor)(void*, void*, Node* const*);
static void(__fastcall* cdDtor)(void*, void*);
static unsigned char* (__cdecl* cdNewChild)(void*, Node* const*);
static void* (__cdecl* cdAddChild)(void*, void*);
static void(__fastcall* cdInt)(void*, void*, int);
static void(__fastcall* cdFloat)(void*, void*, float);
static void(__fastcall* cdPString)(void*, void*, Node* const*);
static const unsigned char* (__cdecl* cdFind)(const void*, Node* const*);
static bool(__cdecl* cdSetUnicode)(void*, const void*);
static void* (__fastcall* cpCtorConfig)(void*, void*, void*);
static void(__cdecl* cpSetVersion)(void*, float);
static void(__cdecl* cpSetConfig)(void*, void*);
static bool(__fastcall* cpSave)(const void*, void*, const char*, bool, int);
static void(__fastcall* cpDtor)(void*, void*);
static void* (__fastcall* fileCtor)(void*, void*);
static void(__fastcall* fileDtor)(void*, void*);
static int* (__cdecl* fileOpen)(void*, int*, const char*, unsigned long, int);
static unsigned long(__cdecl* fileClose)(void*);
static void* (__fastcall* dbCtor)(void*, void*, unsigned long, unsigned long);
static void(__fastcall* dbDtor)(void*, void*);
static bool(__fastcall* dbWrite32)(void*, void*, unsigned long);
static unsigned long(__fastcall* dbWriteRaw)(void*, void*, const void*, unsigned long, unsigned long);
static bool(__fastcall* dbWriteUs)(void*, void*, const void*);
static void* (__fastcall* usCtor)(void*, void*);
static void(__fastcall* usDtor)(void*, void*);
static void(__cdecl* usSet)(void*, const wchar_t*, int, int);
static void* (__fastcall* asCtor)(void*, void*);
static void* (__fastcall* asCtorText)(void*, void*, const char*);
static void(__fastcall* asDtor)(void*, void*);
static const void* (__cdecl* asAssign)(void*, const char*);
static Node* tokTable;
static Node* tokId;
static Node* tokEntry;
static Node* tokValue;
static void Release(Node* n)
{
    if (n) pstringDestroy(*pstringCache, n);
}
static Node* Make(const char* t) { return t ? pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(t)) : nullptr; }
static unsigned char* NewTree(const char* name)
{
    Node* n = Make(name);
    void* const t = cdNew(0x10);
    cdCtor(t, nullptr, &n);
    Release(n);
    return static_cast<unsigned char*>(t);
}
static unsigned char* Child(void* parent, const char* name)
{
    Node* n = Make(name);
    unsigned char* const c = cdNewChild(parent, &n);
    Release(n);
    return c;
}
static void SetText(void* node, const char* text)
{
    Node* n = Make(text);
    cdPString(node, nullptr, &n);
    Release(n);
}

// ---- the generator (drawn once per scenario)
static std::mt19937 rng(7272);
static uint32_t R(uint32_t n) { return n ? rng() % n : 0; }
static const char* const kParts[] = {"ui", "menu", "a", "b", "c", "x y", "long_name", "", "\xc9t\xc9", "1", "dots", "Z"};
static std::string Dotted()
{
    std::string s;
    const uint32_t n = 1 + R(3);
    for (uint32_t i = 0; i < n; ++i) s += (i ? "." : "") + std::string(kParts[R(sizeof(kParts) / sizeof(kParts[0]))]);
    return s;
}
static std::wstring WideText()
{
    static const wchar_t chars[] = {L'a', L'b', L'z', L' ', L'0', 0x410, 0x44f, 0xe9, 0x3000, L'"', L'\n', L'.', L';'};
    std::wstring s;
    const uint32_t n = R(4) ? R(20) : 0;
    for (uint32_t i = 0; i < n; ++i) s += chars[R(sizeof(chars) / sizeof(chars[0]))];
    return s;
}
static uint32_t Id()
{
    static const uint32_t special[] = {1, 2, 7, 100, 0x7fffffff, 0x80000000, 0xffffffff, 65536};
    return R(3) ? special[R(8)] : 1 + R(1000);
}

struct Entry {
    uint32_t id;
    std::string name;
    std::wstring text;
    uint32_t idKind;  // 0 an int, 1 zero, 2 missing, 3 a string, 4 an int with a stray ENTRY value elsewhere
};
struct Table {
    uint32_t id;
    std::string name;
    std::vector<Entry> entries;
    uint32_t idKind;     // as for entries
    uint32_t valueKind;  // 0 TABLE_TOKEN, 1 ENTRY_TOKEN, 2 another string, 3 an int
};
struct Scenario {
    std::vector<Table> tables;
    uint32_t charSize;
    bool resolving;
    bool flag;
    unsigned long saveFlags;
    uint32_t failTableMod, failEntryMod;  // AddTable null / AddEntry 11 for ids that are multiples (0: never)
    int saveCfgResult, saveBinResult;     // what the pure SaveConfigData / SaveBin return
    bool readData;                        // AddEntry (binary) reads the entry's data
    int partialAt;                        // see PartialAt
    std::string text;                     // the config text file
    std::string bin;                      // the binary file
    unsigned char* tree;                  // the config tree (left to leak when it holds the missing-key node)
    bool treeHasUnknown;
    std::vector<std::string> nodePaths;   // for CreateNode
    std::vector<std::string> keys;        // for the traits
    std::vector<std::wstring> texts;
    bool entryPrefix;    // the entry path given to ScanTableConfigData starts with text
    bool createUnknown;  // CreateNode's tree holds the missing-key node
};
static Scenario sc;

static void* NewUs(const std::wstring& w)
{
    void* const u = allocNew(12);
    usCtor(u, nullptr);
    usSet(u, w.c_str(), static_cast<int>(w.size()), static_cast<int>(w.size()));
    return u;
}
static void DeleteUs(void* u)
{
    usDtor(u, nullptr);
    allocDelete(u);
}

static void SetId(void* node, uint32_t id, uint32_t kind)
{
    if (kind == 2) return;
    unsigned char* const c = cdNewChild(node, &tokId);
    if (kind == 1) cdInt(c, nullptr, 0);
    else if (kind == 3) SetText(c, std::to_string(id).c_str());
    else cdInt(c, nullptr, static_cast<int>(id));
}
// the groups of a dotted name under `parent` (each part a new child), the last one returned
static unsigned char* Groups(void* parent, const std::string& name)
{
    unsigned char* node = static_cast<unsigned char*>(parent);
    size_t from = 0;
    for (;;) {
        const size_t dot = name.find('.', from);
        node = Child(node, name.substr(from, dot == std::string::npos ? std::string::npos : dot - from).c_str());
        if (dot == std::string::npos) return node;
        from = dot + 1;
    }
}
static void Noise(void* parent)
{
    switch (R(8)) {
    case 0: cdInt(Child(parent, "n"), nullptr, static_cast<int>(rng())); break;
    case 1: cdFloat(Child(parent, "f"), nullptr, 1.5f); break;
    case 2: SetText(Child(parent, "s"), "table"); break;
    case 3: {
        unsigned char* const c = Child(parent, "e");
        cdPString(c, nullptr, &tokEntry);
        cdInt(cdNewChild(c, &tokId), nullptr, 3);
        break;
    }
    case 4: Child(parent, "g"); break;
    case 5:
        if (R(4) == 0) {
            cdAddChild(parent, unknownItem);
            sc.treeHasUnknown = true;
        }
        break;
    default: break;
    }
}
static void BuildTree()
{
    sc.tree = NewTree(R(4) ? nullptr : "root");
    sc.treeHasUnknown = false;
    for (const Table& t : sc.tables) {
        if (R(6) == 0) Noise(sc.tree);
        unsigned char* const tn = Groups(sc.tree, t.name);
        switch (t.valueKind) {
        case 0: cdPString(tn, nullptr, &tokTable); break;
        case 1: cdPString(tn, nullptr, &tokEntry); break;
        case 2: SetText(tn, "TABLE"); break;
        default: cdInt(tn, nullptr, 5); break;
        }
        SetId(tn, t.id, t.idKind);
        for (const Entry& e : t.entries) {
            if (R(8) == 0) Noise(tn);
            unsigned char* const en = Groups(tn, e.name);
            cdPString(en, nullptr, e.idKind == 4 ? &tokTable : &tokEntry);
            SetId(en, e.id, e.idKind == 4 ? 0 : e.idKind);
            if (R(10)) {
                unsigned char* const v = cdNewChild(en, &tokValue);
                void* const u = NewUs(e.text);
                cdSetUnicode(v, u);
                DeleteUs(u);
            }
        }
    }
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
    } else {
        b = "(no file)";
    }
    return b;
}
static void PutFile(const std::string& path, const std::string& bytes)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}

// the tree's text through the original's ConfigParser::Save (the tree is given back before the parser goes)
static std::string TreeText()
{
    const std::string path = tmpDir + "st_gen.txt";
    DeleteFileA(path.c_str());
    alignas(8) unsigned char cp[0x40];
    cpCtorConfig(cp, nullptr, sc.tree);
    cpSetVersion(cp, 1.0f);
    cpSave(cp, nullptr, path.c_str(), false, R(3) == 0 ? 2 : 1);
    cpSetConfig(cp, nullptr);
    cpDtor(cp, nullptr);
    std::string t = FileBytes(path);
    if (R(6) == 0 && t.size() > 2) t.resize(R(static_cast<uint32_t>(t.size())));
    if (R(12) == 0) t = "junk " + t;
    return t;
}

// a DynamicBuffer's bytes {data, position, capacity, size, ...}
static std::string BufferBytes(const void* db)
{
    const char* const d = *static_cast<const char* const*>(db);
    return d ? std::string(d, static_cast<const unsigned long*>(db)[3]) : std::string();
}
// the scenario's tables in the binary form, written with the original's DynamicBuffer and `toBuffer` (the side's
// traits, or the original's for the input file)
static void WriteBin(void* db, bool names, uint32_t charSize, bool (*data)(void*, const void*))
{
    const unsigned char header[8] = {'S', 'T', 1, 1, static_cast<unsigned char>(names), static_cast<unsigned char>(charSize), 0, 0};
    dbWriteRaw(db, nullptr, header, 8, 1);
    for (const Table& t : sc.tables) {
        dbWriteRaw(db, nullptr, "TABL", 4, 1);
        dbWrite32(db, nullptr, t.id);
        if (names) {
            dbWrite32(db, nullptr, static_cast<unsigned long>(t.name.size()));
            dbWriteRaw(db, nullptr, t.name.data(), static_cast<unsigned long>(t.name.size()), 1);
        }
        dbWrite32(db, nullptr, static_cast<unsigned long>(t.entries.size()));
        for (const Entry& e : t.entries) {
            dbWrite32(db, nullptr, e.id);
            if (names) {
                dbWrite32(db, nullptr, static_cast<unsigned long>(e.name.size()));
                dbWriteRaw(db, nullptr, e.name.data(), static_cast<unsigned long>(e.name.size()), 1);
            }
            void* const u = NewUs(e.text);
            data(db, u);
            DeleteUs(u);
        }
    }
}
static bool OrigToBuffer(void* db, const void* u) { return dbWriteUs(db, nullptr, u); }
static bool SideToBuffer(void* db, const void* u)
{
    unsigned char traits = 0;
    return apis[side].toBuffer(&traits, db, u);
}
static std::string BinImage()
{
    alignas(8) unsigned char db[0x40];
    dbCtor(db, nullptr, 0x100, 0x100);
    const bool names = R(4) != 0;
    WriteBin(db, names, R(5) ? sc.charSize : R(4), &OrigToBuffer);
    std::string b = BufferBytes(db);
    dbDtor(db, nullptr);
    switch (R(6)) {
    case 0:  // cut (never into the header: a short header is read from whatever is on the stack)
        b.resize(8 + R(static_cast<uint32_t>(b.size() - 7)));
        break;
    case 1:  // one byte
        b[R(static_cast<uint32_t>(b.size()))] ^= static_cast<char>(1 + R(255));
        break;
    case 2:
        if (b.size() >= 12) {
            static const uint32_t words[] = {0, 0xffffffff, 0x40000010, 0x7fffffff, 3, 0x40000003, 0x10};
            const uint32_t w = words[R(7)];
            std::memcpy(&b[8 + R(static_cast<uint32_t>(b.size() - 11))], &w, 4);
        }
        break;
    case 3: b += std::string(R(16), static_cast<char>(R(256))); break;
    default: break;
    }
    return b;
}

static bool Fails(uint32_t id, uint32_t mod) { return mod && id % mod == 0; }

// How the binary reader goes over sc.bin with this test's slots: the number of AddTable / AddEntry calls before a
// u32 read that gets only 1-3 bytes, or -1. DynamicBuffer::Read(long&) then succeeds with the rest of the value
// left as it was on the stack (count, entry id, name length), so what follows is not compared.
static int PartialAt()
{
    const std::string& b = sc.bin;
    const size_t size = b.size();
    if (size < 8 || b[0] != 'S' || b[1] != 'T' || b[2] != 1 || b[3] != 1) return -1;
    const bool names = b[4] != 0;
    if (names ? sc.resolving && static_cast<unsigned char>(b[5]) != sc.charSize : sc.resolving) return -1;
    size_t pos = 8;
    const auto read = [&](uint32_t& v) -> int {  // 0 nothing, 1 all four bytes, 2 a part
        if (pos >= size) return 0;
        const size_t n = size - pos < 4 ? size - pos : 4;
        v = 0;
        std::memcpy(&v, b.data() + pos, n);
        pos += n;
        return n == 4 ? 1 : 2;
    };
    const auto name = [&]() -> int {
        uint32_t length;
        const int r = read(length);
        if (r != 1) return r;
        if (length > size - pos) return 0;
        pos += length;
        return 1;
    };
    int calls = 0;
    while (size - pos >= 12) {
        if (b.compare(pos, 4, "TABL")) return -1;
        pos += 4;
        uint32_t id, count;
        read(id);
        int r = names ? name() : 1;
        if (r != 1) return r == 2 ? calls : -1;
        ++calls;
        if (Fails(id, sc.failTableMod)) return -1;
        if ((r = read(count)) != 1) return r == 2 ? calls : -1;
        for (uint32_t i = 0; i < count; ++i) {
            if ((r = read(id)) != 1) return r == 2 ? calls : -1;
            if (names && (r = name()) != 1) return r == 2 ? calls : -1;
            ++calls;
            if (sc.readData && size - pos >= 4) {  // as AddEntryBin
                uint32_t head;
                std::memcpy(&head, b.data() + pos, 4);
                if ((head & 0xbfffffffu) <= size - pos - 4) pos += 4 + (head & 0xbfffffffu);
            }
            if (Fails(id, sc.failEntryMod)) return -1;
        }
    }
    return -1;
}

static void Draw()
{
    sc.tables.clear();
    const uint32_t n = R(5);
    for (uint32_t i = 0; i < n; ++i) {
        Table t;
        t.id = Id();
        t.name = Dotted();
        t.idKind = R(8) ? 0 : R(4);
        t.valueKind = R(10) ? 0 : 1 + R(3);
        const uint32_t m = R(6);
        for (uint32_t j = 0; j < m; ++j) t.entries.push_back({Id(), Dotted(), WideText(), R(8) ? 0 : R(5)});
        sc.tables.push_back(t);
    }
    sc.resolving = R(3) != 0;
    sc.charSize = sc.resolving ? 1 + R(3) : R(4);  // (resolved names are read as text: at least one NUL after them)
    sc.flag = R(2) != 0;
    sc.saveFlags = R(2) ? rng() : 0;
    sc.failTableMod = R(4) ? 0 : 1 + R(4);
    sc.failEntryMod = R(4) ? 0 : 1 + R(4);
    sc.saveCfgResult = R(6) ? 0 : 4;
    sc.saveBinResult = R(6) ? 0 : 3;
    sc.readData = R(8) != 0;
    BuildTree();
    sc.text = TreeText();
    sc.bin = BinImage();
    sc.partialAt = PartialAt();
    sc.nodePaths.clear();
    for (uint32_t i = 0, k = 1 + R(5); i < k; ++i) {
        static const char* const odd[] = {"", ".", "a..b", "a.b.", ".a", "ui.menu.x"};
        sc.nodePaths.push_back(R(3) ? Dotted() : odd[R(6)]);
    }
    sc.keys.clear();
    for (uint32_t i = 0, k = 2 + R(3); i < k; ++i) {
        std::string s;
        if (R(2)) s = Dotted();
        else
            for (uint32_t j = 0, l = R(10); j < l; ++j) s += static_cast<char>(1 + R(255));
        sc.keys.push_back(s);
    }
    sc.entryPrefix = R(2) != 0;
    sc.createUnknown = R(3) == 0;
    sc.texts.clear();
    for (uint32_t i = 0, k = 1 + R(3); i < k; ++i) sc.texts.push_back(WideText());
}

// ---- the test table class: the side's vftable with the pure slots replaced
static void* vtables[2][20];
static std::vector<std::string> tableNames[2];

static std::string ObjState(const unsigned char* o)
{
    char b[64];
    std::snprintf(b, sizeof(b), "+4 %08x +8 %02x", *reinterpret_cast<const uint32_t*>(o + 4), o[8]);
    return std::string(b) + " c " + AText(o + 0xc) + " u " + UText(o + 0x10) + " 1c " + AText(o + 0x1c) +
           (*reinterpret_cast<void* const*>(o) == apis[side].vft ? " vft" : *reinterpret_cast<void* const*>(o) == vtables[side] ? " test-vft" : " other-vft");
}
static std::string TableOf(void* t)
{
    const uintptr_t i = reinterpret_cast<uintptr_t>(t);
    return i >= 0x1000 && i < 0x1000 + tableNames[side].size() ? "t" + std::to_string(i - 0x1000) : "t?";
}
// while sc.bin is read: the AddTable / AddEntry (binary) calls still logged (-1: all)
static int logBudget = -1;
static bool Quiet()
{
    if (logBudget == 0) return true;
    if (logBudget > 0) --logBudget;
    return false;
}

static int* __cdecl SaveConfigSlot(void* self, int* res, unsigned char* root, unsigned long flags)
{
    std::string s = "slot SaveConfigData root ";
    DumpTree(root, 0, s);
    Log(s + " flags " + Num(flags) + " self " + Num(self != nullptr));
    unsigned char traits = 0;
    for (const Table& t : sc.tables) {
        unsigned char* const tn = apis[side].createNode(root, t.name.c_str());
        cdPString(tn, nullptr, &tokTable);
        cdInt(cdNewChild(tn, &tokId), nullptr, static_cast<int>(t.id));
        for (const Entry& e : t.entries) {
            unsigned char* const en = apis[side].createNode(tn, e.name.c_str());
            cdPString(en, nullptr, &tokEntry);
            cdInt(cdNewChild(en, &tokId), nullptr, static_cast<int>(e.id));
            void* const u = NewUs(e.text);
            const bool ok = apis[side].toConfig(&traits, cdNewChild(en, &tokValue), u);
            DeleteUs(u);
            if (!ok) Log("toConfig failed");
        }
    }
    *res = sc.saveCfgResult;
    return res;
}
static int* __cdecl SaveBinSlot(void* self, int* res, void* buffer, unsigned long flags)
{
    const unsigned long* const b = static_cast<const unsigned long*>(buffer);
    Log("slot SaveBin flags " + Num(flags) + " buffer " + Num(b[1]) + " " + Num(b[2]) + " " + Num(b[3]) + " " + Num(b[4]) +
        " " + Num(static_cast<const unsigned char*>(buffer)[0x14]) + " self " + Num(self != nullptr));
    WriteBin(buffer, sc.resolving, sc.charSize, &SideToBuffer);
    *res = sc.saveBinResult;
    return res;
}
static void* __cdecl AddTableBin(void* self, const char* name, long id)
{
    if (!Quiet())
        Log("slot AddTable(bin) " + (name ? "'" + std::string(name) + "'" : std::string("(null)")) + " " + Num(id) + " self " + Num(self != nullptr));
    if (Fails(static_cast<uint32_t>(id), sc.failTableMod)) return nullptr;
    tableNames[side].push_back(name ? name : "");
    return reinterpret_cast<void*>(0x1000 + tableNames[side].size() - 1);
}
static void* __cdecl AddTableCfg(void* self, const void* name, unsigned id)
{
    Log("slot AddTable(cfg) " + AText(name) + " " + Num(id) + " self " + Num(self != nullptr));
    if (Fails(id, sc.failTableMod)) return nullptr;
    tableNames[side].push_back(AText(name));
    return reinterpret_cast<void*>(0x1000 + tableNames[side].size() - 1);
}
static int* __cdecl AddEntryBin(void* self, int* res, void* table, const char* name, long id, void* buffer, bool flag)
{
    const bool quiet = Quiet();
    const unsigned long* const b = static_cast<const unsigned long*>(buffer);
    std::string s = "slot AddEntry(bin) " + TableOf(table) + " " + (name ? "'" + std::string(name) + "'" : std::string("(null)")) +
                    " " + Num(id) + " " + Num(flag) + " at " + Num(b[1]) + " self " + Num(self != nullptr);
    // the data: a u32 byte count (0x40000000: UTF-16) and the bytes; a count past the end is not read (it would
    // be allocated first)
    const unsigned char* const data = *static_cast<unsigned char* const*>(buffer);
    const unsigned long left = b[3] > b[1] ? b[3] - b[1] : 0;
    uint32_t head = 0;
    if (left >= 4) std::memcpy(&head, data + b[1], 4);
    if (sc.readData && left >= 4 && (head & 0xbfffffffu) <= left - 4) {
        unsigned char traits = 0;
        alignas(4) unsigned char u[12];
        usCtor(u, nullptr);
        const bool ok = apis[side].fromBuffer(&traits, buffer, u);
        s += " data " + Num(ok) + " " + UText(u) + " to " + Num(b[1]);
        usDtor(u, nullptr);
    }
    if (!quiet) Log(s);
    *res = Fails(static_cast<uint32_t>(id), sc.failEntryMod) ? 11 : 0;
    return res;
}
static int* __cdecl AddEntryCfg(void* self, int* res, void* table, const void* name, unsigned id, const unsigned char* node, bool flag)
{
    std::string s = "slot AddEntry(cfg) " + TableOf(table) + " " + AText(name) + " " + Num(id) + " " + Num(flag) + " node ";
    DumpTree(node, 0, s);
    const unsigned char* const v = cdFind(node, &tokValue);
    unsigned char traits = 0;
    alignas(4) unsigned char u[12];
    usCtor(u, nullptr);
    const bool ok = apis[side].fromConfig(&traits, v, u);
    Log(s + " value " + Num(ok) + " " + UText(u) + " self " + Num(self != nullptr));
    usDtor(u, nullptr);
    *res = Fails(id, sc.failEntryMod) ? 11 : 0;
    return res;
}

// a table object of the current side: the side's constructor over a 0xAB pattern (+4 and +8 stay unset), then the
// test vftable and the scenario's key size / resolving flag
static void NewTable(unsigned char* o)
{
    std::memset(o, 0xab, 0x40);
    apis[side].ctor(o, nullptr);
    Log("ctor " + ObjState(o));
    *reinterpret_cast<void**>(o) = vtables[side];
    *reinterpret_cast<uint32_t*>(o + 4) = sc.charSize;
    o[8] = sc.resolving;
}

static void Run()
{
    const Api& a = apis[side];
    tableNames[side].clear();
    alignas(8) unsigned char obj[0x40];
    NewTable(obj);
    Log("resolving " + Num(a.resolving(obj)));
    int r;
    Node* base = Make("kst");

    // config input
    r = -7;
    a.addCfg(obj, &r, sc.tree, sc.flag);
    Log("addCfg " + Num(r));
    const std::string textPath = tmpDir + "st_text.txt";
    r = -7;
    a.addCfgPath(obj, &r, textPath.c_str(), sc.flag);
    Log("addCfgPath " + Num(r));
    r = -7;
    a.addCfgBase(obj, &r, &base, "st_text.txt", !sc.flag);
    Log("addCfgBase " + Num(r));
    Node* none = nullptr;
    r = -7;
    a.addCfgBase(obj, &r, &none, textPath.c_str(), sc.flag);
    Log("addCfgBase(no base) " + Num(r));
    r = -7;
    a.addCfgPath(obj, &r, (tmpDir + "st_missing.txt").c_str(), sc.flag);
    Log("addCfgPath(missing) " + Num(r));
    {
        std::string text = sc.text;
        r = -7;
        a.addCfgBuf(obj, &r, &text[0], static_cast<unsigned>(text.size()), sc.flag);
        Log("addCfgBuf " + Num(r));
    }

    // binary input (the result is not compared after a partial u32 read, see PartialAt)
    const auto binResult = [&](int code) { return sc.partialAt >= 0 ? std::string("(partial read)") : Num(code); };
    const std::string binPath = tmpDir + "st_bin.bin";
    r = -7;
    logBudget = sc.partialAt;
    a.addBinPath(obj, &r, binPath.c_str(), sc.flag);
    Log("addBinPath " + binResult(r));
    r = -7;
    logBudget = sc.partialAt;
    a.addBinBase(obj, &r, &base, "st_bin.bin", !sc.flag);
    Log("addBinBase " + binResult(r));
    logBudget = -1;
    r = -7;
    a.addBinPath(obj, &r, (tmpDir + "st_missing.bin").c_str(), sc.flag);
    Log("addBinPath(missing) " + Num(r));
    {
        alignas(8) unsigned char f[0x200];
        fileCtor(f, nullptr);
        int fr = -7;
        fileOpen(f, &fr, binPath.c_str(), 0x10, 0);
        r = -7;
        logBudget = sc.partialAt;
        a.addBinFile(obj, &r, f, sc.flag);
        logBudget = -1;
        Log("addBinFile " + Num(fr) + " " + binResult(r));
        fileClose(f);
        fileDtor(f, nullptr);
    }
    {
        std::string bytes = sc.bin;
        struct {
            void* data;
            unsigned long position, capacity, size, increment;
            bool external;
        } db = {&bytes[0], 0, static_cast<unsigned long>(bytes.size()), static_cast<unsigned long>(bytes.size()), 0x200, true};
        r = -7;
        logBudget = sc.partialAt;
        a.addBinBuf(obj, &r, &db, sc.flag);
        logBudget = -1;
        Log("addBinBuf " + binResult(r) + " at " + Num(db.position));
    }

    // output, then read back
    const std::string outText = tmpDir + "st_out.txt", outBin = tmpDir + "st_out.bin";
    DeleteFileA(outText.c_str());
    r = -7;
    a.saveCfgPath(obj, &r, outText.c_str(), sc.saveFlags);
    Log("saveCfgPath " + Num(r) + " " + Hex(FileBytes(outText)));
    r = -7;
    a.addCfgPath(obj, &r, outText.c_str(), sc.flag);
    Log("addCfgPath(saved) " + Num(r));
    DeleteFileA(outText.c_str());
    r = -7;
    a.saveCfgBase(obj, &r, &base, "st_out.txt", sc.saveFlags);
    Log("saveCfgBase " + Num(r) + " " + Hex(FileBytes(outText)));
    DeleteFileA(outBin.c_str());
    r = -7;
    a.saveBinPath(obj, &r, outBin.c_str(), sc.saveFlags);
    Log("saveBinPath " + Num(r) + " " + Hex(FileBytes(outBin)));
    r = -7;
    a.addBinPath(obj, &r, outBin.c_str(), sc.flag);
    Log("addBinPath(saved) " + Num(r));
    DeleteFileA(outBin.c_str());
    r = -7;
    a.saveBinBase(obj, &r, &base, "st_out.bin", sc.saveFlags);
    Log("saveBinBase " + Num(r) + " " + Hex(FileBytes(outBin)));
    r = -7;
    a.saveBinPath(obj, &r, tmpDir.c_str(), sc.saveFlags);  // a directory: Open fails
    Log("saveBinPath(dir) " + Num(r));
    r = -7;
    a.saveCfgPath(obj, &r, (tmpDir + "no_dir\\x\\st.txt").c_str(), sc.saveFlags);
    Log("saveCfgPath(bad) " + Num(r));
    {
        alignas(8) unsigned char f[0x200];
        fileCtor(f, nullptr);
        int fr = -7;
        DeleteFileA(outBin.c_str());
        fileOpen(f, &fr, outBin.c_str(), 0x101, 0);
        r = -7;
        a.saveBinFile(obj, &r, f, sc.saveFlags);
        fileClose(f);
        fileDtor(f, nullptr);
        Log("saveBinFile " + Num(fr) + " " + Num(r) + " " + Hex(FileBytes(outBin)));
    }

    // the Scan functions with a path already holding text
    {
        alignas(4) unsigned char path[4];
        asCtorText(path, nullptr, "pre.");
        for (const unsigned char* c : Children(sc.tree)) {
            if (c == unknownItem) break;
            r = -7;
            a.scanNode(obj, nullptr, &r, c, path, sc.flag);
            Log("scanNode " + Num(r) + " " + AText(path));
            alignas(4) unsigned char entryPath[4];
            asCtorText(entryPath, nullptr, sc.entryPrefix ? "e." : "");
            r = -7;
            a.scanTable(obj, nullptr, &r, c, reinterpret_cast<void*>(0x1000), path, entryPath, sc.flag);
            Log("scanTable " + Num(r) + " " + AText(path) + " " + AText(entryPath));
            asDtor(entryPath, nullptr);
        }
        r = -7;
        a.scan(obj, nullptr, &r, sc.tree, !sc.flag);
        Log("scan " + Num(r));
        asDtor(path, nullptr);
    }

    // copy and assignment, with text in the members
    asAssign(obj + 0xc, sc.keys[0].c_str());
    asAssign(obj + 0x1c, sc.keys[1].c_str());
    usSet(obj + 0x10, sc.texts[0].c_str(), static_cast<int>(sc.texts[0].size()), static_cast<int>(sc.texts[0].size()));
    alignas(8) unsigned char copy[0x40];
    std::memset(copy, 0xcd, sizeof(copy));
    a.copy(copy, nullptr, obj);
    Log("copy " + ObjState(copy));
    alignas(8) unsigned char other[0x40];
    NewTable(other);
    asAssign(other + 0xc, "old");
    a.assign(other, nullptr, copy);
    Log("assigned " + ObjState(other));
    a.dtor(copy, nullptr);
    a.dtor(other, nullptr);
    Log("final " + ObjState(obj));
    // the deleting destructor (slot 0) on a heap copy
    {
        unsigned char* const h = static_cast<unsigned char*>(allocNew(0x20));
        a.copy(h, nullptr, obj);
        reinterpret_cast<void*(__fastcall*)(void*, void*, unsigned)>(a.vft[0])(h, nullptr, 1);
        Log("deleted");
    }
    a.dtor(obj, nullptr);
    Release(base);
    Log("destroyed");

    // CreateNode on a fresh tree
    {
        unsigned char* const t = NewTree("cn");
        bool unknown = false;
        if (sc.createUnknown) {
            cdAddChild(t, unknownItem);
            unknown = true;
        }
        Child(t, "ui");
        for (const std::string& p : sc.nodePaths) {
            const unsigned char* const n = a.createNode(t, p.c_str());
            std::string s = "createNode '" + p + "' ";
            DumpTree(n, 0, s);
            Log(s);
        }
        std::string s = "createNode tree ";
        DumpTree(t, 0, s);
        Log(s);
        if (!unknown) {
            cdDtor(t, nullptr);
            cdDelete(t);
        }
    }

    // the traits
    {
        unsigned char traits = 0, other2 = 1;
        Log("traits= " + Num(a.traitsAssign(&traits, nullptr, &other2) == &traits));
        for (size_t i = 0; i < sc.keys.size(); ++i) {
            const char* const k = sc.keys[i].c_str();
            char* const n = a.newKey(&traits, k);
            char* const m = a.newKeyPtr(&traits, k);
            Log("key '" + Hex(n) + "' '" + Hex(m) + "' hash " + Num(a.hash(&traits, k)) + " cmp " +
                Num(a.compare(&traits, k, sc.keys[(i + 1) % sc.keys.size()].c_str())) + " " +
                Num(a.compare(&traits, sc.keys[(i + 1) % sc.keys.size()].c_str(), k)) + " " + Num(a.compare(&traits, k, n)));
            a.deleteKey(&traits, n);
            a.deleteKey(&traits, m);
        }
        a.deleteKey(&traits, nullptr);
        for (const std::wstring& w : sc.texts) {
            void* const u = NewUs(w);
            unsigned char* const c = NewTree("v");
            cdInt(c, nullptr, 9);
            const bool ok = a.toConfig(&traits, c, u);
            std::string s = "toConfig " + Num(ok) + " ";
            DumpTree(c, 0, s);
            alignas(4) unsigned char back[12];
            usCtor(back, nullptr);
            bool got = a.fromConfig(&traits, c, back);
            s += " fromConfig " + Num(got) + " " + UText(back);
            got = a.fromConfig(&traits, unknownItem, back);
            s += " fromConfig(missing) " + Num(got) + " " + UText(back);
            usDtor(back, nullptr);
            Log(s);
            cdDtor(c, nullptr);
            cdDelete(c);
            alignas(8) unsigned char db[0x40];
            dbCtor(db, nullptr, 0x10, 0x10);
            const bool wrote = a.toBuffer(&traits, db, u);
            s = "toBuffer " + Num(wrote) + " " + Hex(BufferBytes(db));
            reinterpret_cast<unsigned long*>(db)[1] = 0;  // read from the start
            alignas(4) unsigned char rd[12];
            usCtor(rd, nullptr);
            got = a.fromBuffer(&traits, db, rd);
            s += " fromBuffer " + Num(got) + " " + UText(rd);
            got = a.fromBuffer(&traits, db, rd);
            s += " again " + Num(got) + " " + UText(rd);
            usDtor(rd, nullptr);
            dbDtor(db, nullptr);
            Log(s);
            DeleteUs(u);
        }
        a.logErr("x %d", 1);
        a.logWarn("y %s", "z");
    }
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    const void* at = e->ExceptionRecord->ExceptionAddress;
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<const char*>(at), &m);
    if (m) GetModuleFileNameA(m, name, MAX_PATH);
    std::printf("crash %08lx at %p (%s +%tx) data %p side %d\n", e->ExceptionRecord->ExceptionCode, at, name,
                static_cast<const char*>(at) - reinterpret_cast<const char*>(m),
                reinterpret_cast<void*>(e->ExceptionRecord->ExceptionInformation[1]), side);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_ks_stringtables <dir> [scenarios]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 3000;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    HMODULE orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    const bool swap = getenv("KST_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours);
    Load(apis[1], swap ? ours : orig);
    pstringCache = Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringCreate = Get<decltype(pstringCreate)>(orig, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringDestroy = Get<decltype(pstringDestroy)>(orig, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    allocNew = Get<decltype(allocNew)>(orig, "?New@Alloc@Jet@@SAPAXI@Z");
    allocDelete = Get<decltype(allocDelete)>(orig, "?Delete@Alloc@Jet@@SAXPAX@Z");
    cdNew = Get<decltype(cdNew)>(orig, "??2ConfigData@Jet@@SAPAXI@Z");
    cdDelete = Get<decltype(cdDelete)>(orig, "??3ConfigData@Jet@@SAXPAX@Z");
    cdCtor = Get<decltype(cdCtor)>(orig, "??0ConfigData@Jet@@QAE@ABVPString@1@@Z");
    cdDtor = Get<decltype(cdDtor)>(orig, "??1ConfigData@Jet@@QAE@XZ");
    cdNewChild = Get<decltype(cdNewChild)>(orig, "?New@ConfigData@Jet@@QAAAAV12@ABVPString@2@@Z");
    cdAddChild = Get<decltype(cdAddChild)>(orig, "??6ConfigData@Jet@@QAAAAV01@PAV01@@Z");
    cdInt = Get<decltype(cdInt)>(orig, "??6ConfigData@Jet@@QAEXH@Z");
    cdFloat = Get<decltype(cdFloat)>(orig, "??6ConfigData@Jet@@QAEXM@Z");
    cdPString = Get<decltype(cdPString)>(orig, "??6ConfigData@Jet@@QAEXABVPString@1@@Z");
    cdFind = Get<decltype(cdFind)>(orig, "?Find@ConfigData@Jet@@QBAABV12@ABVPString@2@@Z");
    cdSetUnicode = Get<decltype(cdSetUnicode)>(orig, "?SetUnicode@ConfigData@Jet@@QAA_NABVUnicodeString@2@@Z");
    unknownItem = Get<unsigned char*>(orig, "?unknownItem@ConfigData@Jet@@2V12@A");
    cpCtorConfig = Get<decltype(cpCtorConfig)>(orig, "??0ConfigParser@Jet@@QAE@PAVConfigData@1@@Z");
    cpSetVersion = Get<decltype(cpSetVersion)>(orig, "?SetVersion@ConfigParser@Jet@@QAAXM@Z");
    cpSetConfig = Get<decltype(cpSetConfig)>(orig, "?SetConfig@ConfigParser@Jet@@QAAXPAVConfigData@2@@Z");
    cpSave = Get<decltype(cpSave)>(orig, "?Save@ConfigParser@Jet@@QBE_NPBD_NH@Z");
    cpDtor = Get<decltype(cpDtor)>(orig, "??1ConfigParser@Jet@@UAE@XZ");
    fileCtor = Get<decltype(fileCtor)>(orig, "??0File@Jet@@QAE@XZ");
    fileDtor = Get<decltype(fileDtor)>(orig, "??1File@Jet@@QAE@XZ");
    fileOpen = Get<decltype(fileOpen)>(orig, "?Open@File@Jet@@QAA?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@PBDKH@Z");
    fileClose = Get<decltype(fileClose)>(orig, "?Close@File@Jet@@QAAKXZ");
    dbCtor = Get<decltype(dbCtor)>(orig, "??0DynamicBuffer@Jet@@QAE@KK@Z");
    dbDtor = Get<decltype(dbDtor)>(orig, "??1DynamicBuffer@Jet@@QAE@XZ");
    dbWrite32 = Get<decltype(dbWrite32)>(orig, "?Write@DynamicBuffer@Jet@@QAE_NK@Z");
    dbWriteRaw = Get<decltype(dbWriteRaw)>(orig, "?Write@DynamicBuffer@Jet@@QAEKPBXKK@Z");
    dbWriteUs = Get<decltype(dbWriteUs)>(orig, "?Write@DynamicBuffer@Jet@@QAE_NABVUnicodeString@2@@Z");
    usCtor = Get<decltype(usCtor)>(orig, "??0UnicodeString@Jet@@QAE@XZ");
    usDtor = Get<decltype(usDtor)>(orig, "??1UnicodeString@Jet@@QAE@XZ");
    usSet = Get<decltype(usSet)>(orig, "?Set@UnicodeString@Jet@@AAAXPB_WHH@Z");
    asCtor = Get<decltype(asCtor)>(orig, "??0AnsiString@Jet@@QAE@XZ");
    asCtorText = Get<decltype(asCtorText)>(orig, "??0AnsiString@Jet@@QAE@PBD@Z");
    asDtor = Get<decltype(asDtor)>(orig, "??1AnsiString@Jet@@QAE@XZ");
    asAssign = Get<decltype(asAssign)>(orig, "??4AnsiString@Jet@@QAAABV01@PBD@Z");
    tokTable = *Get<Node**>(orig, "?TABLE_TOKEN@StringTablesBase@Jet@@1VPString@2@B");
    tokId = *Get<Node**>(orig, "?ID_TOKEN@StringTablesBase@Jet@@1VPString@2@B");
    tokEntry = *Get<Node**>(orig, "?ENTRY_TOKEN@StringTablesBase@Jet@@1VPString@2@B");
    tokValue = *Get<Node**>(orig, "?VALUE_TOKEN@StringTablesBase@Jet@@1VPString@2@B");
    std::printf("tokens %s %s %s %s, fileTblId %.4s\n", NText(tokTable).c_str(), NText(tokId).c_str(), NText(tokEntry).c_str(),
                NText(tokValue).c_str(), Get<const char*>(orig, "?fileTblId@StringTablesBase@Jet@@1QBDB"));
    for (int s = 0; s < 2; ++s) {
        for (int i = 0; i < 20; ++i) vtables[s][i] = apis[s].vft[i];
        vtables[s][9] = reinterpret_cast<void*>(&SaveConfigSlot);
        vtables[s][12] = reinterpret_cast<void*>(&SaveBinSlot);
        vtables[s][16] = reinterpret_cast<void*>(&AddTableBin);
        vtables[s][17] = reinterpret_cast<void*>(&AddTableCfg);
        vtables[s][18] = reinterpret_cast<void*>(&AddEntryBin);
        vtables[s][19] = reinterpret_cast<void*>(&AddEntryCfg);
    }
    {
        char t[MAX_PATH];
        GetTempPathA(MAX_PATH, t);
        tmpDir = t;
        SetCurrentDirectoryA(tmpDir.c_str());  // (the original's File leaves its temporary files in the current directory)
        Node* drive = Make("kst");
        Get<void(__cdecl*)(Node* const*, const char*, bool)>(orig, "?Subst@File@Jet@@SAXABVPString@2@PBD_N@Z")(&drive, tmpDir.c_str(), true);
        Release(drive);
    }

    unsigned long long checks = 0, bad = 0;
    for (int n = 0; n < scenarios; ++n) {
        Draw();
        PutFile(tmpDir + "st_text.txt", sc.text);
        PutFile(tmpDir + "st_bin.bin", sc.bin);
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
        if (getenv("KST_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", n, l.substr(0, 600).c_str());
        if (const char* only = getenv("KST_ONLY")) {  // both sides of one scenario, and its binary file kept
            if (std::atoi(only) == n) {
                for (int s = 0; s < 2; ++s)
                    for (const std::string& l : logs[s]) std::printf("%d %d| %s\n", n, s, l.substr(0, 600).c_str());
                PutFile(tmpDir + "st_case.bin", sc.bin);
                std::printf("bin %s\n", Hex(sc.bin).c_str());
            }
        }
        logs[0].clear();
        logs[1].clear();
        if (!sc.treeHasUnknown) {
            cdDtor(sc.tree, nullptr);
            cdDelete(sc.tree);
        }
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
