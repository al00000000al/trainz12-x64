// Differential test of KernelStandard unit U7a (Options.cpp). Each scenario is drawn once, then run on an Options
// made by our KernelStandard_7c.dll and on one made by KernelStandard_7c_orig.dll (both in this process, called
// through raw exports): option lines from a generator of the syntax (switches / - +, keys in mixed case, = with
// quoted, unquoted, multi-word, empty values, key- / key+, stray signs, tabs and newlines, bytes >= 0x80), command
// lines, ANSI and UTF-16 files, environment variables, then every Get / GetNext overload, separators, arrays and
// ranges. After every step the entry list (key, value, separators, cursor) and every result (floats as bits) are
// logged per side; the logs must match.
//
// usage: diff_ks_options.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
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
static std::vector<std::string> logs[2];
static void Log(const char* fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logs[side].push_back(buf);
}

// a String is one pointer to {capacity, length, chars}; a PString one pointer to a node with the text at +0x18
static std::string StrText(const void* s)
{
    const auto* b = *static_cast<const unsigned* const*>(s);
    if (!b) return "(nullptr)";
    if (!b[0]) return "(null)";
    return std::string("'") + reinterpret_cast<const char*>(b + 2) + "' len" + std::to_string(b[1]);
}
static std::string PText(const void* p)
{
    const auto* n = *static_cast<const unsigned char* const*>(p);
    return n ? std::string("'") + reinterpret_cast<const char*>(n + 0x18) + "'" : "(null)";
}
static uint32_t Bits(float f)
{
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}

struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void(__cdecl* init3)(void*, const char*, int, char* const*);
    void(__cdecl* init2)(void*, const char*, const char*);
    void(__cdecl* cmdLine)(void*, int, char* const*);
    void(__cdecl* file)(void*, const char*);
    void(__cdecl* env)(void*, const char*);
    void(__cdecl* raw)(void*, const char*);
    void(__cdecl* setSeps)(const void*, const char*, const char*);
    bool(__cdecl* nextString)(const void*, const char*, void*);
    bool(__cdecl* nextPString)(const void*, const char*, void*);
    bool(__cdecl* nextBool)(const void*, const char*, bool*);
    bool(__cdecl* nextUint)(const void*, const char*, unsigned*);
    bool(__cdecl* nextInt)(const void*, const char*, int*);
    bool(__cdecl* nextFloat)(const void*, const char*, float*);
    bool(__cdecl* nextV2)(const void*, const char*, float*);
    bool(__cdecl* nextV3)(const void*, const char*, float*);
    bool(__cdecl* nextV4)(const void*, const char*, float*);
    bool(__cdecl* nextQ)(const void*, const char*, float*);
    bool(__cdecl* nextColor)(const void*, const char*, float*);
    bool(__cdecl* has)(const void*, const char*);
    bool(__cdecl* getPString)(const void*, const char*, void*);
    bool(__cdecl* getString)(const void*, const char*, void*);
    bool(__cdecl* getPStringF)(const void*, const char*, void*, const void*);
    bool(__cdecl* getStringF)(const void*, const char*, void*, const void*);
    bool(__cdecl* getBool)(const void*, const char*, bool*, bool);
    bool(__cdecl* getInt)(const void*, const char*, int*, int);
    bool(__cdecl* getUint)(const void*, const char*, unsigned*, unsigned);
    bool(__cdecl* getFloat)(const void*, const char*, float*, float);
    bool(__cdecl* getV2)(const void*, const char*, float*, const float*);
    bool(__cdecl* getV3)(const void*, const char*, float*, const float*);
    bool(__cdecl* getV4)(const void*, const char*, float*, const float*);
    bool(__cdecl* getQ)(const void*, const char*, float*, const float*);
    bool(__cdecl* getColor)(const void*, const char*, float*, const float*);
    bool(__cdecl* getArray)(const void*, const char*, void*, const char*);
    bool(__cdecl* rangeF)(const void*, const char*, float*);
    bool(__cdecl* rangeI)(const void*, const char*, int*);
};
static Api apis[2];
static void* (__fastcall* stringCtor)(void*, void*, const char*);  // the original's, for fallbacks
static void* (__cdecl* pstringCreate)(void*, const unsigned char*);
static void** pstringCache;
static int(__cdecl* putenv80)(const char*);

static void Load(Api& a, HMODULE m)
{
    a.ctor = Get<decltype(a.ctor)>(m, "??0Options@Jet@@QAE@XZ");
    a.init3 = Get<decltype(a.init3)>(m, "?Init@Options@Jet@@QAAXPBDHQAPAD@Z");
    a.init2 = Get<decltype(a.init2)>(m, "?Init@Options@Jet@@QAAXPBD0@Z");
    a.cmdLine = Get<decltype(a.cmdLine)>(m, "?ProcessCmdLine@Options@Jet@@QAAXHQAPAD@Z");
    a.file = Get<decltype(a.file)>(m, "?ProcessFile@Options@Jet@@QAAXPBD@Z");
    a.env = Get<decltype(a.env)>(m, "?ProcessEnv@Options@Jet@@QAAXPBD@Z");
    a.raw = Get<decltype(a.raw)>(m, "?ProcessRaw@Options@Jet@@QAAXPBD@Z");
    a.setSeps = Get<decltype(a.setSeps)>(m, "?SetSeparators@Options@Jet@@QBAXPBD0@Z");
    a.nextString = Get<decltype(a.nextString)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAVString@2@@Z");
    a.nextPString = Get<decltype(a.nextPString)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAVPString@2@@Z");
    a.nextBool = Get<decltype(a.nextBool)>(m, "?GetNext@Options@Jet@@QBA_NPBDAA_N@Z");
    a.nextUint = Get<decltype(a.nextUint)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAI@Z");
    a.nextInt = Get<decltype(a.nextInt)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAH@Z");
    a.nextFloat = Get<decltype(a.nextFloat)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAM@Z");
    a.nextV2 = Get<decltype(a.nextV2)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAVVector2@2@@Z");
    a.nextV3 = Get<decltype(a.nextV3)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAVVector3@2@@Z");
    a.nextV4 = Get<decltype(a.nextV4)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAVVector4@2@@Z");
    a.nextQ = Get<decltype(a.nextQ)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAVQuaternion@2@@Z");
    a.nextColor = Get<decltype(a.nextColor)>(m, "?GetNext@Options@Jet@@QBA_NPBDAAVColor@2@@Z");
    a.has = Get<decltype(a.has)>(m, "?Get@Options@Jet@@QBA_NPBD@Z");
    a.getPString = Get<decltype(a.getPString)>(m, "?Get@Options@Jet@@QBA_NPBDAAVPString@2@@Z");
    a.getString = Get<decltype(a.getString)>(m, "?Get@Options@Jet@@QBA_NPBDAAVString@2@@Z");
    a.getPStringF = Get<decltype(a.getPStringF)>(m, "?Get@Options@Jet@@QBA_NPBDAAVPString@2@ABV32@@Z");
    a.getStringF = Get<decltype(a.getStringF)>(m, "?Get@Options@Jet@@QBA_NPBDAAVString@2@ABV32@@Z");
    a.getBool = Get<decltype(a.getBool)>(m, "?Get@Options@Jet@@QBA_NPBDAA_N_N@Z");
    a.getInt = Get<decltype(a.getInt)>(m, "?Get@Options@Jet@@QBA_NPBDAAHH@Z");
    a.getUint = Get<decltype(a.getUint)>(m, "?Get@Options@Jet@@QBA_NPBDAAII@Z");
    a.getFloat = Get<decltype(a.getFloat)>(m, "?Get@Options@Jet@@QBA_NPBDAAMM@Z");
    a.getV2 = Get<decltype(a.getV2)>(m, "?Get@Options@Jet@@QBA_NPBDAAVVector2@2@ABV32@@Z");
    a.getV3 = Get<decltype(a.getV3)>(m, "?Get@Options@Jet@@QBA_NPBDAAVVector3@2@ABV32@@Z");
    a.getV4 = Get<decltype(a.getV4)>(m, "?Get@Options@Jet@@QBA_NPBDAAVVector4@2@ABV32@@Z");
    a.getQ = Get<decltype(a.getQ)>(m, "?Get@Options@Jet@@QBA_NPBDAAVQuaternion@2@ABV32@@Z");
    a.getColor = Get<decltype(a.getColor)>(m, "?Get@Options@Jet@@QBA_NPBDAAVColor@2@ABV32@@Z");
    a.getArray = Get<decltype(a.getArray)>(m, "?Get@Options@Jet@@QBA_NPBDAAV?$DynamicArray@VString@Jet@@$0CJ@U?$Less@VString@Jet@@V12@@2@@2@0@Z");
    a.rangeF = Get<decltype(a.rangeF)>(m, "?Get@Options@Jet@@QBA_NPBDAAU?$Range@M@2@@Z");
    a.rangeI = Get<decltype(a.rangeI)>(m, "?Get@Options@Jet@@QBA_NPBDAAU?$Range@H@2@@Z");
}

// ---- the generator
static std::mt19937 rng(4242);
static uint32_t R(uint32_t n) { return n ? rng() % n : 0; }
static const char* const kKeys[] = {"Width", "height", "jet", "VBO", "x", "path", "Mods", "list", "a", "Level", "ZZ", "f"};
static std::string Key()
{
    std::string k = kKeys[R(12)];
    for (char& c : k)
        if (R(3) == 0) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c >= 'A' && c <= 'Z' ? c + 32 : c);
    return k;
}
static std::string Word()
{
    static const char* const words[] = {"1", "0", "-3", "2.5", "1e3", "true", "OFF", "on", "False", "+7", "abc", "x,y",
                                        "1,2,3", "4;5", "0.1 0.2", "c:\\path\\to file", "8:16", "-1.5:2", ":", "a:",
                                        "\xe9t\xe9", "  ", "", "nan", "inf", "0x10", "  7  ,  8 ", "1 , 2 , 3 , 4"};
    return words[R(28)];
}
static std::string Space()
{
    static const char* const s[] = {" ", " ", "  ", "\t", "\n", "\r\n", " \t "};
    return s[R(7)];
}
static std::string Line()
{
    std::string l;
    const int n = 1 + static_cast<int>(R(6));
    for (int i = 0; i < n; ++i) {
        if (R(4)) l += Space();
        const uint32_t sw = R(10);
        if (sw < 5) l += '-';
        else if (sw < 6) l += '/';
        else if (sw < 8) l += '+';
        if (R(25) == 0) {  // a stray sign or equals
            l += "=+-"[R(3)];
            continue;
        }
        l += Key();
        const uint32_t v = R(10);
        if (v < 5) {
            if (R(4) == 0) l += Space();
            l += '=';
            if (R(3) == 0) l += Space();
            const uint32_t q = R(6);
            if (q == 0) l += "\"" + Word() + " " + Word() + "\"";
            else if (q == 1) l += "'" + Word() + "\"";
            else if (q == 2) l += Word() + " " + Word();
            else if (q == 3) l += "\"" + Word();  // unterminated
            else l += Word();
        } else if (v < 7) {
            l += "-+"[R(2)];
        }
    }
    if (R(10) == 0) l += Space();
    return l;
}

static std::string Lower(std::string k);
static bool Walkable(const std::vector<std::string>& ok, const std::string& key);

// cursors are shown for the walkable keys only: a stale one may point anywhere, even at a new buffer that took the
// old one's place
static std::string DumpOptions(const unsigned char* o, const std::vector<std::string>& walkable = {})
{
    std::string s = "count " + std::to_string(*reinterpret_cast<const int*>(o + 8));
    for (auto* e = *reinterpret_cast<const unsigned char* const*>(o); e; e = *reinterpret_cast<const unsigned char* const*>(e)) {
        s += " {" + StrText(e + 8) + "=" + StrText(e + 0xc) + " sep " + StrText(e + 0x10);
        const char* cursor = *reinterpret_cast<const char* const*>(e + 0x14);
        const auto* vb = *reinterpret_cast<const unsigned* const*>(e + 0xc);
        const char* text = vb && vb[0] ? reinterpret_cast<const char*>(vb + 2) : nullptr;
        const auto* kb = *reinterpret_cast<const unsigned* const*>(e + 8);
        const bool shown = kb[0] && Walkable(walkable, reinterpret_cast<const char*>(kb + 2));
        if (!shown)
            s += " @?";
        else if (!cursor)
            s += " @-";
        else if (text && cursor >= text && cursor <= text + vb[1])
            s += " @" + std::to_string(cursor - text);
        else
            s += " @stale";
        s += "}";
    }
    return s;
}

struct Op {
    int kind;
    std::string a, b;
    uint32_t x, y;
    std::vector<std::string> args;
};

static std::string Lower(std::string k)
{
    for (char& c : k) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
    return k;
}

// A cursor points into the value it was set on; once a Process call replaces values it dangles (in both modules),
// so GetNext (also through the typed Gets) only runs on keys whose separators were set since the last Process.
// the value text of a key's entry (null when the key is missing or has no value)
static const char* ValueOf(const unsigned char* o, const std::string& key)
{
    for (auto* e = *reinterpret_cast<const unsigned char* const*>(o); e; e = *reinterpret_cast<const unsigned char* const*>(e)) {
        const auto* kb = *reinterpret_cast<const unsigned* const*>(e + 8);
        if (kb[0] && Lower(key) == reinterpret_cast<const char*>(kb + 2)) {
            const auto* vb = *reinterpret_cast<const unsigned* const*>(e + 0xc);
            return vb[0] ? reinterpret_cast<const char*>(vb + 2) : nullptr;
        }
    }
    return nullptr;
}

static bool Walkable(const std::vector<std::string>& ok, const std::string& key)
{
    for (const std::string& k : ok)
        if (k == Lower(key)) return true;
    return false;
}

static void Run(const std::vector<Op>& ops, const std::string& fileA, const std::string& fileW)
{
    std::vector<std::string> walkable;
    const Api& A = apis[side];
    alignas(8) unsigned char o[0xc];
    std::memset(o, 0xcd, sizeof(o));
    A.ctor(o, nullptr);
    for (const Op& op : ops) {
        const char* k = op.a.c_str();
        if (getenv("KSO_TRACE")) std::fprintf(stderr, "side %d op %d x %u a [%s]\n", side, op.kind, op.x, k);
        if (op.kind <= 3) walkable.clear();
        if (op.kind == 4 || op.kind == 7) walkable.push_back(Lower(op.a));
        const bool walk = Walkable(walkable, op.a);
        // (a range of a key with no value: strchr(null) in both modules)
        if (op.kind == 8 && !ValueOf(o, op.a)) {
            Log("skip range %s", k);
            continue;
        }
        if ((op.kind == 5 || (op.kind == 6 && op.x % 13 >= 9)) && !walk) {  // separators first, then the walk
            A.setSeps(o, k, op.b.c_str());
            walkable.push_back(Lower(op.a));
            Log("seps first %s '%s'", k, op.b.c_str());
        }
        switch (op.kind) {
        case 0:
            A.raw(o, op.a.c_str());
            Log("raw");
            break;
        case 1: {
            std::vector<char*> argv;
            for (const std::string& s : op.args) argv.push_back(const_cast<char*>(s.c_str()));
            A.cmdLine(o, static_cast<int>(argv.size()), argv.data());
            Log("cmdline");
            break;
        }
        case 2:
            A.file(o, op.x & 1 ? fileW.c_str() : fileA.c_str());
            Log("file");
            break;
        case 3:
            A.env(o, op.x & 1 ? "DIFF_KS_OPT" : "DIFF_KS_NOPE");
            Log("env");
            break;
        case 4:
            A.setSeps(o, k, op.b.c_str());
            Log("seps %s '%s'", k, op.b.c_str());
            break;
        case 5: {  // GetNext of one type
            bool r = false;
            std::string v;
            switch (op.x % 10) {
            case 0: {
                void* s = nullptr;
                stringCtor(&s, nullptr, "old");
                r = A.nextString(o, k, &s);
                v = StrText(&s);
                break;
            }
            case 1: {
                void* p = nullptr;
                r = A.nextPString(o, k, &p);
                v = PText(&p);
                break;
            }
            case 2: {
                bool b = true;
                r = A.nextBool(o, k, &b);
                v = std::to_string(b);
                break;
            }
            case 3: {
                unsigned u = 77;
                r = A.nextUint(o, k, &u);
                v = std::to_string(u);
                break;
            }
            case 4: {
                int i = 77;
                r = A.nextInt(o, k, &i);
                v = std::to_string(i);
                break;
            }
            case 5: {
                float f = 7.0f;
                r = A.nextFloat(o, k, &f);
                v = std::to_string(Bits(f));
                break;
            }
            default: {
                float f[4] = {1, 2, 3, 4};
                const int t = op.x % 10;
                r = t == 6 ? A.nextV2(o, k, f) : t == 7 ? A.nextV3(o, k, f) : t == 8 ? A.nextV4(o, k, f) : (op.y & 1) ? A.nextQ(o, k, f) : A.nextColor(o, k, f);
                for (float g : f) v += std::to_string(Bits(g)) + ",";
                break;
            }
            }
            Log("next %u %s -> %d %s", op.x % 10, k, r, v.c_str());
            break;
        }
        case 6: {  // Get of one type, with fallbacks
            bool r = false;
            std::string v;
            const float fb[4] = {0.5f, -1.0f, 1e30f, 0.0f};
            switch (op.x % 13) {
            case 0:
                r = A.has(o, k);
                break;
            case 1: {
                void* p = nullptr;
                r = A.getPString(o, k, &p);
                v = PText(&p);
                break;
            }
            case 2: {
                void* s = nullptr;
                stringCtor(&s, nullptr, "old");
                r = A.getString(o, k, &s);
                v = StrText(&s);
                break;
            }
            case 3: {
                void* p = nullptr;
                void* f = pstringCreate(*pstringCache, reinterpret_cast<const unsigned char*>("fallback"));
                r = A.getPStringF(o, k, &p, &f);
                v = PText(&p);
                break;
            }
            case 4: {
                void* s = nullptr;
                void* f = nullptr;
                stringCtor(&s, nullptr, "old");
                stringCtor(&f, nullptr, "fb");
                r = A.getStringF(o, k, &s, &f);
                v = StrText(&s);
                break;
            }
            case 5: {
                bool b = (op.y & 2) != 0;
                r = A.getBool(o, k, &b, (op.y & 1) != 0);
                v = std::to_string(b);
                break;
            }
            case 6: {
                int i = 5;
                r = A.getInt(o, k, &i, -9);
                v = std::to_string(i);
                break;
            }
            case 7: {
                unsigned u = 5;
                r = A.getUint(o, k, &u, 9);
                v = std::to_string(u);
                break;
            }
            case 8: {
                float f = 5;
                r = A.getFloat(o, k, &f, 0.25f);
                v = std::to_string(Bits(f));
                break;
            }
            default: {
                float f[4] = {9, 9, 9, 9};
                const int t = op.x % 13;
                r = t == 9 ? A.getV2(o, k, f, fb) : t == 10 ? A.getV3(o, k, f, fb) : t == 11 ? A.getV4(o, k, f, fb) : (op.y & 1) ? A.getQ(o, k, f, fb) : A.getColor(o, k, f, fb);
                for (float g : f) v += std::to_string(Bits(g)) + ",";
                break;
            }
            }
            Log("get %u %s -> %d %s", op.x % 13, k, r, v.c_str());
            break;
        }
        case 7: {  // the array of runs
            void* arr = nullptr;  // DynamicArray<String,41>: {Chunk* head}
            const bool r = A.getArray(o, k, &arr, op.b.c_str());
            std::string v;
            if (auto* head = static_cast<unsigned char*>(arr)) {
                unsigned char* c = head;
                do {
                    const unsigned n = *reinterpret_cast<unsigned*>(c + 8);
                    for (unsigned i = 0; i < n; ++i) v += StrText(c + 0xc + 4 * i) + ";";
                    v += "|";
                    c = *reinterpret_cast<unsigned char**>(c);
                } while (c != head);
            }
            Log("array %s '%s' -> %d %s", k, op.b.c_str(), r, v.c_str());
            break;
        }
        case 8: {
            float f[2] = {3, 3};
            int i[2] = {3, 3};
            const bool r1 = A.rangeF(o, k, f), r2 = A.rangeI(o, k, i);
            Log("range %s -> %d %u %u %d %d %d", k, r1, Bits(f[0]), Bits(f[1]), r2, i[0], i[1]);
            break;
        }
        case 9: {  // Init on a fresh object
            alignas(8) unsigned char o2[0xc];
            A.ctor(o2, nullptr);
            if (op.x & 1) {
                std::vector<char*> argv;
                for (const std::string& s : op.args) argv.push_back(const_cast<char*>(s.c_str()));
                A.init3(o2, "app", static_cast<int>(argv.size()), argv.data());
            } else {
                A.init2(o2, "app", op.y & 1 ? op.a.c_str() : nullptr);
            }
            Log("init %s", DumpOptions(o2).c_str());
            break;
        }
        }
        Log("%s", DumpOptions(o, walkable).c_str());
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
    std::printf("crash %08lx at %p (%s +%tx) data %p\n", e->ExceptionRecord->ExceptionCode, at, name,
                static_cast<const char*>(at) - reinterpret_cast<const char*>(m),
                reinterpret_cast<void*>(e->ExceptionRecord->ExceptionInformation[1]));
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_ks_options <dir> [scenarios]\n");
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
    if (getenv("KSO_CRT")) {
        HMODULE crt = GetModuleHandleA("msvcr80.dll");
        std::printf("msvcr80 %p\n", static_cast<void*>(crt));
        for (const char* f : {"isspace", "atoi", "atof", "strtol", "strtoul", "_strlwr", "_stricmp", "wcstombs", "strncpy", "getenv", "vprintf"})
            std::printf("  %s %p\n", f, crt ? reinterpret_cast<void*>(GetProcAddress(crt, f)) : nullptr);
    }
    Load(apis[0], getenv("KSO_SWAP") ? orig : ours);
    Load(apis[1], getenv("KSO_SWAP") ? ours : orig);
    stringCtor = Get<decltype(stringCtor)>(orig, "??0String@Jet@@QAE@PBD@Z");
    pstringCreate = Get<decltype(pstringCreate)>(orig, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z");
    pstringCache = Get<void**>(orig, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    putenv80 = reinterpret_cast<int(__cdecl*)(const char*)>(GetProcAddress(GetModuleHandleA("msvcr80.dll"), "_putenv"));
    if (const char* one = getenv("KSO_FILE")) {  // ProcessFile of one file, each side
        for (side = 0; side < 2; ++side) {
            alignas(8) unsigned char o[0xc];
            apis[side].ctor(o, nullptr);
            std::printf("side %d...\n", side);
            apis[side].file(o, one);
            std::printf("side %d: %s\n", side, DumpOptions(o).c_str());
        }
        return 0;
    }
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    const std::string fileA = std::string(tmp) + "diff_ks_options_a.txt", fileW = std::string(tmp) + "diff_ks_options_w.txt";
    unsigned long long checks = 0, bad = 0;
    for (int sc = 0; sc < scenarios; ++sc) {
        // the files and the environment of this scenario
        {
            const std::string a = Line(), w = Line();
            FILE* f = std::fopen(fileA.c_str(), "wb");
            std::fwrite(a.data(), 1, a.size(), f);
            std::fclose(f);
            f = std::fopen(fileW.c_str(), "wb");
            if (R(2)) std::fwrite("\xff\xfe", 1, 2, f);
            for (char c : w) {
                const wchar_t wc = static_cast<unsigned char>(c);
                std::fwrite(&wc, 2, 1, f);
            }
            // a wide NUL at the end: ProcessFile terminates the buffer with one zero byte only, so wcstombs reads
            // on into whatever the heap holds past it (in both modules)
            const wchar_t zero = 0;
            std::fwrite(&zero, 2, 1, f);
            std::fclose(f);
            const std::string e = "DIFF_KS_OPT=" + Line();
            if (putenv80) putenv80(e.c_str());
        }
        std::vector<Op> ops(1 + R(40));
        for (Op& op : ops) {
            const uint32_t k = R(100);
            op.kind = k < 25 ? 0 : k < 29 ? 1 : k < 32 ? 2 : k < 34 ? 3 : k < 44 ? 4 : k < 70 ? 5 : k < 90 ? 6 : k < 94 ? 7 : k < 97 ? 8 : 9;
            op.x = static_cast<uint32_t>(rng());
            op.y = static_cast<uint32_t>(rng());
            if (op.kind == 0 || op.kind == 9) op.a = Line();
            else op.a = Key();
            static const char* const seps[] = {",", ", ", ";,", " ", "", ":", ",;:"};
            op.b = seps[R(7)];
            if (op.kind == 1 || op.kind == 9)
                for (uint32_t i = 0, n = R(5); i < n; ++i) op.args.push_back(R(2) ? "-" + Key() + "=" + Word() : Word());
        }
        for (side = 0; side < 2; ++side) Run(ops, fileA, fileW);
        const size_t n = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < n; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 10) std::printf("scenario %d line %zu:\n  ours %s\n  orig %s\n", sc, i, x.c_str(), y.c_str());
                break;
            }
        }
        if (getenv("KSO_DUMP"))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", sc, l.c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
