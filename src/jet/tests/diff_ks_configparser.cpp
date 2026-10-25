// Differential test of KernelStandard unit U7c1 (ConfigParser.cpp, ConfigLexer.cpp). Each scenario makes one text,
// then parses it with our module's ConfigParser and with the original's (both in this process, through raw
// exports): texts written by the original's ConfigData::Write from random trees (every value kind, names that
// need quoting, special floats, string arrays, bytes) behind a "version" line, the same texts cut, spliced and
// sprinkled with tokens and stray characters, and token soup. Each side logs the result code, the version bits, the
// whole tree, the file name, SaveString's text and Save's file (both encodings), then parses its own output again
// through Parse(path), Parse(base, path) on a subst drive and Parse(MemoryFile&). The logs must match.
//
// usage: diff_ks_configparser.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
#include <windows.h>

#include <cstdarg>
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
static const bool trace = getenv("KSP_TRACE") != nullptr;
static std::vector<std::string> logs[2];
static void Log(const std::string& s) { logs[side].push_back(s); }

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
    default: break;
    }
    if (!d) return "type " + std::to_string(t) + " (no block)";
    const int n = *static_cast<const int*>(d);
    std::string s = "type " + std::to_string(t) + " n" + std::to_string(n) + " [";
    for (int i = 0; i < n && i < 200; ++i) {
        if (t == 4) std::snprintf(buf, sizeof(buf), "%02x", static_cast<const unsigned char*>(d)[4 + i]);
        else if (t == 7) std::snprintf(buf, sizeof(buf), "%s ", NText(static_cast<Node* const*>(d)[1 + i]).c_str());
        else std::snprintf(buf, sizeof(buf), "%08x ", static_cast<const unsigned*>(d)[1 + i]);
        s += buf;
    }
    return s + "]";
}
static void DumpTree(const unsigned char* c, int depth, std::string& s)
{
    if (!c) {
        s += "(no node)";
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

// ---- exports
struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void*(__fastcall* ctorConfig)(void*, void*, void*);
    void*(__fastcall* copy)(void*, void*, const void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    void(__fastcall* dtor)(void*, void*);
    int*(__cdecl* parseBuffer)(void*, int*, const char*, const void*, unsigned);
    int*(__cdecl* parsePath)(void*, int*, const char*);
    int*(__cdecl* parseBase)(void*, int*, Node* const*, const char*);
    int*(__cdecl* parseFile)(void*, int*, void*);
    float(__cdecl* getVersion)(const void*);
    void(__cdecl* setVersion)(void*, float);
    unsigned char*(__cdecl* getConfig)(void*);
    const unsigned char*(__cdecl* getConfigC)(const void*);
    void(__cdecl* setConfig)(void*, void*);
    const char*(__cdecl* getFileName)(const void*);
    void(__cdecl* setFileName)(void*, const char*);
    void(__fastcall* setKey)(void*, void*, const void*);
    bool(__fastcall* save)(const void*, void*, const char*, bool, int);
    bool(__fastcall* saveBase)(const void*, void*, const char*, const char*, bool, int);
    bool(__fastcall* saveString)(const void*, void*, Node**, int);
    const unsigned char*(__fastcall* root)(const void*, void*);
};
static Api apis[2];
#define RES "?AV?$ResultClass@$1?results@ConfigParser@Jet@@2QBUResultTable@2@BW4Results@ConfigParser@Jet@@@2@"
static void Load(Api& a, HMODULE m)
{
    a.ctor = Get<decltype(a.ctor)>(m, "??0ConfigParser@Jet@@QAE@XZ");
    a.ctorConfig = Get<decltype(a.ctorConfig)>(m, "??0ConfigParser@Jet@@QAE@PAVConfigData@1@@Z");
    a.copy = Get<decltype(a.copy)>(m, "??0ConfigParser@Jet@@QAE@ABV01@@Z");
    a.assign = Get<decltype(a.assign)>(m, "??4ConfigParser@Jet@@QAEAAV01@ABV01@@Z");
    a.dtor = Get<decltype(a.dtor)>(m, "??1ConfigParser@Jet@@UAE@XZ");
    a.parseBuffer = Get<decltype(a.parseBuffer)>(m, "?Parse@ConfigParser@Jet@@QAA" RES "PBDPAXI@Z");
    a.parsePath = Get<decltype(a.parsePath)>(m, "?Parse@ConfigParser@Jet@@QAA" RES "PBD@Z");
    a.parseBase = Get<decltype(a.parseBase)>(m, "?Parse@ConfigParser@Jet@@QAA" RES "ABVPString@2@PBD@Z");
    a.parseFile = Get<decltype(a.parseFile)>(m, "?Parse@ConfigParser@Jet@@QAA" RES "AAVMemoryFile@2@@Z");
    a.getVersion = Get<decltype(a.getVersion)>(m, "?GetVersion@ConfigParser@Jet@@QBAMXZ");
    a.setVersion = Get<decltype(a.setVersion)>(m, "?SetVersion@ConfigParser@Jet@@QAAXM@Z");
    a.getConfig = Get<decltype(a.getConfig)>(m, "?GetConfig@ConfigParser@Jet@@QAAPAVConfigData@2@XZ");
    a.getConfigC = Get<decltype(a.getConfigC)>(m, "?GetConfig@ConfigParser@Jet@@QBAPBVConfigData@2@XZ");
    a.setConfig = Get<decltype(a.setConfig)>(m, "?SetConfig@ConfigParser@Jet@@QAAXPAVConfigData@2@@Z");
    a.getFileName = Get<decltype(a.getFileName)>(m, "?GetFileName@ConfigParser@Jet@@QBAPBDXZ");
    a.setFileName = Get<decltype(a.setFileName)>(m, "?SetFileName@ConfigParser@Jet@@QAAXPBD@Z");
    a.setKey = Get<decltype(a.setKey)>(m, "?SetResourceKey@ConfigParser@Jet@@QAEXABVString@2@@Z");
    a.save = Get<decltype(a.save)>(m, "?Save@ConfigParser@Jet@@QBE_NPBD_NH@Z");
    a.saveBase = Get<decltype(a.saveBase)>(m, "?Save@ConfigParser@Jet@@QBE_NPBD0_NH@Z");
    a.saveString = Get<decltype(a.saveString)>(m, "?SaveString@ConfigParser@Jet@@QBE_NAAVPString@2@H@Z");
    a.root = Get<decltype(a.root)>(m, "?GetConfigRoot@ConfigParser@Jet@@IBEPBVConfigData@2@XZ");
}

// the original's own helpers (the same for both sides)
static void** pstringCache;
static Node* (__cdecl* pstringCreate)(void*, const unsigned char*);
static void(__cdecl* pstringDestroy)(void*, Node*);
static void* (__cdecl* cdNew)(size_t);
static void(__cdecl* cdDelete)(void*);
static void* (__fastcall* cdCtor)(void*, void*, Node* const*);
static void(__fastcall* cdDtor)(void*, void*);
static unsigned char* (__cdecl* cdNewChild)(void*, Node* const*);
static void(__fastcall* cdInt)(void*, void*, int);
static void(__fastcall* cdFloat)(void*, void*, float);
static void(__fastcall* cdPString)(void*, void*, Node* const*);
static void(__cdecl* cdBytes)(void*, const unsigned char*, int);
static void(__cdecl* cdInts)(void*, const int*, int);
static void(__cdecl* cdFloats)(void*, const float*, int);
static void(__cdecl* cdPStrings)(void*, Node* const*, int);
static bool(__cdecl* cdWrite)(const void*, Node**, int);
static void* (__fastcall* stringCtor)(void*, void*, const char*);
static void(__fastcall* stringDtor)(void*, void*);
static void* (__fastcall* mfCtor)(void*, void*, const void*, unsigned, bool, int);
static void(__fastcall* mfDtor)(void*, void*);
static void Release(Node* n)
{
    if (n) pstringDestroy(*pstringCache, n);
}
static Node* Make(const char* t) { return t ? pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>(t)) : nullptr; }
static uint32_t Bits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
static float Float(uint32_t u)
{
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

// ---- the generator (drawn once per scenario)
static std::mt19937 rng(5151);
static uint32_t R(uint32_t n) { return n ? rng() % n : 0; }
static const char* const kNames[] = {"a", "name", "kuid", "x y", "q\"uote", "back\\slash", "tab\tbed", "new\nline",
                                     "1digit", "#hash", "/slash", "semi;", "eq=", "(p)", "{b}", "c,d", "", "UPPER",
                                     "\xc9t\xc9", "`tick", "long-name_with.dots"};
static const char* const kTexts[] = {"text", "", "with space", "q\"x", "a\\b", "\r\n", "12", "-3.5", "1e9", "0x1F",
                                     "{", "}", "(x)", ";", "=", "\xff\xfe", "true", "\t"};
static uint32_t FloatBits()
{
    static const uint32_t special[] = {0, 0x3f800000, 0xc0200000, 0x3dcccccd, 0x501502f9, 0x7fc00000, 0xffc00000,
                                       0x7f800000, 0xff800000, 0x7f800001, 0x80000000, 0x00000001, 0x7fe00000,
                                       0xffe00000, 0x47f12065, 0xbeaaaaab};
    return R(3) ? special[R(16)] : static_cast<uint32_t>(rng());
}
static int IntValue()
{
    static const int special[] = {0, 1, -1, 2147483647, -2147483647 - 1, 100, 255, -42, 65536};
    return R(3) ? special[R(9)] : static_cast<int>(rng());
}

static void Build(void* node, int depth)
{
    const uint32_t kids = depth < 3 ? R(depth == 0 ? 7 : 4) : 0;
    for (uint32_t k = 0; k < kids; ++k) {
        Node* name = R(8) ? Make(kNames[R(sizeof(kNames) / sizeof(kNames[0]))]) : nullptr;
        unsigned char* const child = cdNewChild(node, &name);
        Release(name);
        switch (R(10)) {
        case 0: break;
        case 1: cdInt(child, nullptr, IntValue()); break;
        case 2: cdFloat(child, nullptr, Float(FloatBits())); break;
        case 3: {
            Node* v = Make(kTexts[R(sizeof(kTexts) / sizeof(kTexts[0]))]);
            cdPString(child, nullptr, &v);
            Release(v);
            break;
        }
        case 4: {
            unsigned char b[70];
            const int n = static_cast<int>(R(70));
            for (int i = 0; i < n; ++i) b[i] = static_cast<unsigned char>(rng());
            cdBytes(child, b, n);
            break;
        }
        case 5: {
            int v[14];
            const int n = 1 + static_cast<int>(R(13));
            for (int i = 0; i < n; ++i) v[i] = IntValue();
            cdInts(child, v, n);
            break;
        }
        case 6: {
            float v[14];
            const int n = 1 + static_cast<int>(R(13));
            for (int i = 0; i < n; ++i) v[i] = Float(FloatBits());
            cdFloats(child, v, n);
            break;
        }
        case 7: {
            Node* v[14];
            const int n = 1 + static_cast<int>(R(13));
            for (int i = 0; i < n; ++i) v[i] = Make(kTexts[R(sizeof(kTexts) / sizeof(kTexts[0]))]);
            cdPStrings(child, v, n);
            for (int i = 0; i < n; ++i) Release(v[i]);
            break;
        }
        default: break;
        }
        if (R(3) == 0) Build(child, depth + 1);
    }
}

static std::string TreeText()  // the original's ConfigData::Write of a random tree
{
    Node* none = nullptr;
    void* const root = cdNew(0x10);
    cdCtor(root, nullptr, &none);
    Build(root, 0);
    Node* text = nullptr;
    cdWrite(root, &text, R(4) == 0 ? 2 : 1);
    std::string s = text ? reinterpret_cast<const char*>(text + 0x18) : "";
    Release(text);
    cdDtor(root, nullptr);
    cdDelete(root);
    return s;
}

static std::string Token()
{
    static const char* const tokens[] = {
        "version 1.00\r\n", "ver ", "=", ";", "{", "}", "(", ")", ",", "\r\n", "\n", " ", "\t", "\"q\"", "\"un",
        "\"e\\\"s\\\\c\\n\\r\\t\\b\\x\"", "name", "a b", "12", "-7", "0x1F", "1.5", "-2.5e3", ".5", "1.#INF",
        "-1.#INF", "1.#QNAN", "-1.#IND", "1.#IND", ": 0A1B\r\n", ": \r\n    ff00", "# comment\r\n", "/* c */",
        "/*", "*/", "//", "\\", "@", "~", "\x80", "\x01", "kuid", "<", ">", ":", "x=1;", "y(\"v\")\r\n{\r\n}\r\n",
        "`raw text`", "``", "`q\"in`", "x = `bt`;\r\n", "y = 010;\r\n", "z = 0;\r\n", "w = 0.0;\r\n",
        "i = 0.#INF;\r\n", "n = -0.#QNAN;\r\n", "d = 0.#IND;\r\n", "a = 010,0.#INF,`s`;\r\n", "010", "0.#INF"};
    return tokens[R(sizeof(tokens) / sizeof(tokens[0]))];
}

static std::string Header()
{
    static const char* const versions[] = {"version 1.00\r\n", "version 2.5\r\n", "ver 1\r\n", "version -3\r\n",
                                           "version 1e2\r\n", "version 1.#INF\r\n", "version \r\n", "",
                                           "# leading comment\r\nversion 1.00\r\n", "/* c */ version 0.5\r\n"};
    return versions[R(R(3) ? 3 : 10)];
}

// A backslash outside quotes (and comments) never ends in either module: rule 29 puts it back and enters the
// bare-string state, whose rule 43 puts it back again and returns an empty STRING, which the grammar joins
// forever. Such texts are not drawn.
static bool BareBackslash(const std::string& t)
{
    bool quoted = false, block = false, line = false;
    for (size_t i = 0; i < t.size(); ++i) {
        const char c = t[i];
        if (line) {
            if (c == '\n') line = false;
        } else if (block) {
            if (c == '*' && i + 1 < t.size() && t[i + 1] == '/') block = false, ++i;
        } else if (quoted) {
            if (c == '\\') ++i;
            else if (c == '"') quoted = false;
        } else if (c == '`') {  // a raw string: a quote in it is just a character
            const size_t end = t.find('`', i + 1);
            if (end == std::string::npos) return t.find('\\', i) != std::string::npos;
            if (t.substr(i, end - i).find('\\') != std::string::npos) return true;
            i = end;
        } else if (c == '"') {
            quoted = true;
        } else if (c == '\\') {  // (also in a comment: '#' is a comment only in some states, "1.#IND" is not one)
            return true;
        }
    }
    return false;
}

static std::string MakeText();
static std::string MakeTextOnce();
static std::string MakeText()
{
    for (;;) {
        const std::string t = MakeTextOnce();
        if (!BareBackslash(t)) return t;
    }
}
static std::string MakeTextOnce()
{
    const uint32_t kind = R(10);
    if (kind == 9) {  // token soup
        std::string t = R(2) ? Header() : "";
        for (uint32_t i = 0, n = R(30); i < n; ++i) t += Token();
        return t;
    }
    std::string t = Header() + TreeText();
    if (kind >= 5) {  // damaged
        for (uint32_t i = 0, n = 1 + R(3); i < n && !t.empty(); ++i) {
            const size_t at = R(static_cast<uint32_t>(t.size()));
            switch (R(5)) {
            case 0: t.insert(at, Token()); break;
            case 1: t.erase(at, R(8)); break;
            case 2: t.insert(at, 1, static_cast<char>(R(256))); break;
            case 3: t = t.substr(0, at); break;
            default: t.insert(at, t.substr(R(static_cast<uint32_t>(t.size())), R(20))); break;
            }
        }
    }
    return t;
}

// ---- one side
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

static std::string State(const void* cp)
{
    const Api& a = apis[side];
    std::string s = "version " + std::to_string(Bits(a.getVersion(cp)));
    const char* const fn = a.getFileName(cp);
    s += " file " + (fn ? std::string("'") + fn + "'" : std::string("(null)"));
    s += " tree ";
    DumpTree(a.getConfigC(cp), 0, s);
    s += a.root(cp, nullptr) == a.getConfigC(cp) ? " root=" : " root!=";
    return s;
}

static void Run(const std::string& text, uint32_t enc, uint32_t flags)
{
    const Api& a = apis[side];
    alignas(8) unsigned char cp[0x40];
    a.ctor(cp, nullptr);
    a.setVersion(cp, Float(0x12345678));  // (the default constructor leaves it unset)
    int r = -7;
    a.parseBuffer(cp, &r, flags & 1 ? "scenario.txt" : nullptr, text.data(), static_cast<unsigned>(text.size()));
    Log("parse " + std::to_string(r) + " " + State(cp));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    Node* out = nullptr;
    const bool ok = a.saveString(cp, nullptr, &out, static_cast<int>(enc));
    // (UTF-16 into a PString: the string writer copies up to a zero byte, and a non-ASCII character's wide form has
    // none, so it reads on past the converted text into the stack; only the result is compared then)
    bool ascii = true;
    for (unsigned char ch : text) ascii = ascii && ch < 0x80;
    if (enc == 2 && !ascii)
        Log("saveString " + std::to_string(ok));
    else
        Log("saveString " + std::to_string(ok) + " " + (out ? Hex(reinterpret_cast<const char*>(out + 0x18)) : "(null)"));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    Release(out);
    const std::string path = tmpDir + "diff_ksp.txt";
    DeleteFileA(path.c_str());
    const bool saved = flags & 2 ? a.saveBase(cp, nullptr, "ksp", "diff_ksp.txt", (flags & 4) != 0, static_cast<int>(enc))
                                 : a.save(cp, nullptr, path.c_str(), (flags & 4) != 0, static_cast<int>(enc));
    Log("save " + std::to_string(saved) + " " + Hex(FileBytes(path)));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    // its own file, back through the other overloads
    alignas(8) unsigned char cq[0x40];
    a.ctor(cq, nullptr);
    a.setVersion(cq, 0.0f);
    r = -7;
    a.parsePath(cq, &r, path.c_str());
    Log("path " + std::to_string(r) + " " + State(cq));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    Node* drive = Make("ksp");
    r = -7;
    a.parseBase(cq, &r, &drive, "diff_ksp.txt");
    Release(drive);
    Log("base " + std::to_string(r) + " " + State(cq));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    {
        const std::string bytes = FileBytes(path);
        alignas(8) unsigned char mf[0x20];
        mfCtor(mf, nullptr, bytes.data(), static_cast<unsigned>(bytes.size()), false, 0);
        r = -7;
        a.parseFile(cq, &r, mf);
        Log("memory " + std::to_string(r) + " " + State(cq));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
        mfDtor(mf, nullptr);
    }
    // copies share the scanner and the tree: the copy's state, then it is let go without its destructor
    alignas(8) unsigned char cc[0x40];
    a.copy(cc, nullptr, cq);
    Log("copy " + State(cc));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    a.assign(cc, nullptr, cp);
    Log("assigned " + State(cc));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    a.setFileName(cq, flags & 8 ? "renamed" : nullptr);
    alignas(4) unsigned char key[4];
    stringCtor(key, nullptr, "key");
    a.setKey(cq, nullptr, key);
    stringDtor(key, nullptr);
    Log("renamed " + State(cq));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    a.dtor(cq, nullptr);
    // a parser over a given tree: Save of it, then the tree taken back
    alignas(8) unsigned char cg[0x40];
    a.ctorConfig(cg, nullptr, a.getConfig(cp));
    Log("given " + State(cg));
    if (trace) std::printf("  step %d side %d\n", __LINE__, side);  // KSP_STEP
    a.setConfig(cg, nullptr);
    a.dtor(cg, nullptr);
    a.dtor(cp, nullptr);
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

// KSP_WATCH=<seconds>: then the main thread's EIP and stack words (module + offset) are printed and the process ends
static HANDLE mainThread;
static std::string Where(DWORD a)
{
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<const char*>(a), &m))
        return "";
    GetModuleFileNameA(m, name, MAX_PATH);
    const char* base = std::strrchr(name, '\\');
    char buf[400];
    std::snprintf(buf, sizeof(buf), "%s+%lx", base ? base + 1 : name, a - reinterpret_cast<DWORD>(m));
    return buf;
}
static volatile long beat;     // scenarios done
static std::string currentText;
static DWORD WINAPI Watch(void* seconds)
{
    for (long last = -1;;) {
        Sleep(static_cast<DWORD>(reinterpret_cast<uintptr_t>(seconds)) * 1000);
        if (beat != last) {
            last = beat;
            continue;
        }
        break;
    }
    SuspendThread(mainThread);
    std::printf("watch: no progress in scenario %ld [%s]\n", beat, Hex(currentText.substr(0, 300)).c_str());
    CONTEXT c = {};
    c.ContextFlags = CONTEXT_FULL;
    GetThreadContext(mainThread, &c);
    std::printf("watch: eip %s side %d\n", Where(c.Eip).c_str(), side);
    const DWORD* sp = reinterpret_cast<const DWORD*>(c.Esp);
    for (int i = 0, shown = 0; i < 2048 && shown < 24; ++i) {
        const std::string w = Where(sp[i]);
        if (!w.empty() && w.find(".exe") == std::string::npos) {
            std::printf("  [esp+%x] %s\n", i * 4, w.c_str());
            ++shown;
        }
    }
    TerminateProcess(GetCurrentProcess(), 9);
    return 0;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (const char* w = getenv("KSP_WATCH")) {
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
        CreateThread(nullptr, 0, &Watch, reinterpret_cast<void*>(static_cast<uintptr_t>(std::atoi(w))), 0, nullptr);
    }
    if (argc < 2) {
        std::printf("usage: diff_ks_configparser <dir> [scenarios]\n");
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
    const bool swap = getenv("KSP_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours);
    Load(apis[1], swap ? ours : orig);
    pstringCache = Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    pstringCreate = Get<decltype(pstringCreate)>(orig, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringDestroy = Get<decltype(pstringDestroy)>(orig, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    cdNew = Get<decltype(cdNew)>(orig, "??2ConfigData@Jet@@SAPAXI@Z");
    cdDelete = Get<decltype(cdDelete)>(orig, "??3ConfigData@Jet@@SAXPAX@Z");
    cdCtor = Get<decltype(cdCtor)>(orig, "??0ConfigData@Jet@@QAE@ABVPString@1@@Z");
    cdDtor = Get<decltype(cdDtor)>(orig, "??1ConfigData@Jet@@QAE@XZ");
    cdNewChild = Get<decltype(cdNewChild)>(orig, "?New@ConfigData@Jet@@QAAAAV12@ABVPString@2@@Z");
    cdInt = Get<decltype(cdInt)>(orig, "??6ConfigData@Jet@@QAEXH@Z");
    cdFloat = Get<decltype(cdFloat)>(orig, "??6ConfigData@Jet@@QAEXM@Z");
    cdPString = Get<decltype(cdPString)>(orig, "??6ConfigData@Jet@@QAEXABVPString@1@@Z");
    cdBytes = Get<decltype(cdBytes)>(orig, "?SetByteArray@ConfigData@Jet@@QAAXPAEH@Z");
    cdInts = Get<decltype(cdInts)>(orig, "?SetIntArray@ConfigData@Jet@@QAAXPAHH@Z");
    cdFloats = Get<decltype(cdFloats)>(orig, "?SetFloatArray@ConfigData@Jet@@QAAXPAMH@Z");
    cdPStrings = Get<decltype(cdPStrings)>(orig, "?SetPStringArray@ConfigData@Jet@@QAAXPAVPString@2@H@Z");
    cdWrite = Get<decltype(cdWrite)>(orig, "?Write@ConfigData@Jet@@QBA_NAAVPString@2@H@Z");
    stringCtor = Get<decltype(stringCtor)>(orig, "??0String@Jet@@QAE@PBD@Z");
    stringDtor = Get<decltype(stringDtor)>(orig, "??1String@Jet@@QAE@XZ");
    mfCtor = Get<decltype(mfCtor)>(orig, "??0MemoryFile@Jet@@QAE@PBXI_NH@Z");
    mfDtor = Get<decltype(mfDtor)>(orig, "??1MemoryFile@Jet@@QAE@XZ");
    {
        char t[MAX_PATH];
        GetTempPathA(MAX_PATH, t);
        tmpDir = t;
        Node* drive = Make("ksp");
        // each module keeps its own substitute list (U10b)
        for (HMODULE m : {orig, ours})
            Get<void(__cdecl*)(Node* const*, const char*, bool)>(m, "?Subst@File@Jet@@SAXABVPString@2@PBD_N@Z")(&drive, tmpDir.c_str(), true);
        Release(drive);
    }
    if (const char* one = getenv("KSP_FILE")) {  // one file, each side
        FILE* f = std::fopen(one, "rb");
        std::string text;
        char buf[4096];
        size_t n;
        while (f && (n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
        if (f) std::fclose(f);
        for (side = 0; side < 2; ++side) Run(text, 1, 1);
        for (side = 0; side < 2; ++side)
            for (const std::string& l : logs[side]) std::printf("%d| %s\n", side, l.substr(0, 400).c_str());
        return 0;
    }

    unsigned long long checks = 0, bad = 0;
    for (int sc = 0; sc < scenarios; ++sc) {
        const std::string text = MakeText();
        currentText = text;
        InterlockedIncrement(&beat);
        const uint32_t enc = R(3), flags = R(16);
        if (trace) std::printf("scenario %d [%s]\n", sc, Hex(text.substr(0, 200)).c_str());
        for (side = 0; side < 2; ++side) Run(text, enc, flags);
        const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < n; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 8)
                    std::printf("scenario %d line %zu:\n  text %s\n  ours %s\n  orig %s\n", sc, i, Hex(text.substr(0, 300)).c_str(),
                                x.substr(0, 1500).c_str(), y.substr(0, 1500).c_str());
                break;
            }
        }
        if (getenv("KSP_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", sc, l.substr(0, 600).c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
