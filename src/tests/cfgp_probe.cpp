// Probe: does the original ConfigParser parse anything in a test process? Prints the result code and the root's
// child count for a few texts. usage: cfgp_probe <dir with KernelStandard_7c_orig.dll>
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

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

// stands in for the original's log printf (0x1000dde0)
static void __cdecl LogStub(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::printf("  log: ");
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}
static void Hook(void* at, void* to)
{
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    unsigned char* p = static_cast<unsigned char*>(at);
    p[0] = 0xe9;
    const int rel = static_cast<int>(static_cast<unsigned char*>(to) - (p + 5));
    std::memcpy(p + 1, &rel, 4);
    VirtualProtect(at, 5, old, &old);
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string dir = argv[1];
    SetDllDirectoryA(dir.c_str());
    HMODULE m = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    if (!m) return 2;
    Hook(reinterpret_cast<unsigned char*>(m) + 0xdde0, reinterpret_cast<void*>(&LogStub));
    auto ctor = Get<void*(__fastcall*)(void*, void*)>(m, "??0ConfigParser@Jet@@QAE@XZ");
    auto parse = Get<int*(__cdecl*)(void*, int*, const char*, const void*, unsigned)>(
        m, "?Parse@ConfigParser@Jet@@QAA?AV?$ResultClass@$1?results@ConfigParser@Jet@@2QBUResultTable@2@BW4Results@ConfigParser@Jet@@@2@PBDPAXI@Z");
    auto getConfig = Get<unsigned char*(__cdecl*)(void*)>(m, "?GetConfig@ConfigParser@Jet@@QAAPAVConfigData@2@XZ");
    auto childCount = Get<int(__cdecl*)(const void*)>(m, "?GetChildCount@ConfigData@Jet@@QBAHXZ");
    static std::string real;
    if (argc > 2) {
        FILE* f = std::fopen(argv[2], "rb");
        char buf[65536];
        const size_t n = std::fread(buf, 1, sizeof(buf), f);
        std::fclose(f);
        real.assign(buf, n);
    }
    const char* const texts[] = {"version 1.00\r\na = 1;\r\nb = \"x\";\r\nv = 1,2,3;\r\nf = 2.5;\r\n\r\nblk\r\n{\r\n  c = \"y\";\r\n}\r\n",
                                 "ver 1\r\na 1\r\n", "ver 1.0\r\nname \"x\"\r\nb 2\r\n", "ver 2\r\nc { d 1 }\r\n",
                                 "ver 1\r\nv = (1, 2, 3);\r\n", "ver 1;\r\na = 1;\r\nb { c = \"x\"; }\r\n","\xef\xbb\xbf\r\nusername \"x\"\r\nkind 2\r\n",
                                 "username \"x\"\r\n", "\r\nkind 2\r\n", real.c_str()};
    for (const char* t : texts) {
        alignas(8) unsigned char cp[0x40] = {};
        ctor(cp, nullptr);
        int r = -7;
        parse(cp, &r, "probe.txt", t, static_cast<unsigned>(std::strlen(t)));
        unsigned char* root = getConfig(cp);
        std::printf("[%s] -> %d root %p children %d\n", t, r, static_cast<void*>(root), root ? childCount(root) : -1);
    }
    // the tokens: the core the last Parse left ([parser+4]) restarted on our own {cur, base, size} input
    std::printf("module base %p\n", static_cast<void*>(m));
    for (const char* t : texts) {
        alignas(8) unsigned char cp[0x40] = {};
        ctor(cp, nullptr);
        int r = -7;
        parse(cp, &r, "probe.txt", "x", 1);
        void** core = *reinterpret_cast<void***>(cp + 4);
        struct In {
            const char* cur;
            const char* base;
            unsigned size;
        } in = {t, t, static_cast<unsigned>(std::strlen(t))};
        void** vt = *reinterpret_cast<void***>(core);
        reinterpret_cast<void(__fastcall*)(void*, void*, void*)>(vt[4])(core, nullptr, &in);  // yyrestart
        static int startState = 0;
        startState = 1;
        *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(core) + 0x3c) = startState;
        std::printf("start %d [%s]:", startState, t);
        for (int i = 0; i < 20; ++i) {
            const int tok = reinterpret_cast<int(__fastcall*)(void*, void*)>(vt[5])(core, nullptr);  // yylex
            const char* text = *reinterpret_cast<const char**>(reinterpret_cast<unsigned char*>(core) + 4);  // yytext
            std::printf(" %d", tok);
            if (text && tok > 0) std::printf("'%s'", text);
            if (tok <= 0) break;
        }
        std::printf("\n");
    }
    return 0;
}
