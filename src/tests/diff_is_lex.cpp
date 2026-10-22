// Differential test of InputSystem_7c's key binding scanner: our InputLex (Lexer.cpp, built into this test) against
// the original's, called inside InputSystem_7c_orig.dll by address (its constructors from a String at RVA 0x25e0
// and from a PString at RVA 0x2730, yylex through the vtable, yylval at +0x78, the name buffer at +0x84).
// Inputs: random texts made of the scanner's words, the input names, numbers, punctuation, white space and random
// bytes, through both constructors. Compared: every token, its value and, for names, the text.
//
// usage: diff_is_lex.exe <dir with InputSystem_7c_orig.dll and the Jet modules> [texts]
#include <windows.h>

#include "../InputSystem_7c/Lexer.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static std::mt19937 rng(99);
static unsigned Pick(unsigned n) { return rng() % n; }

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_lex <dir> [texts]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int texts = argc > 2 ? std::atoi(argv[2]) : 100000;
    SetDllDirectoryA(dir.c_str());
    HMODULE ks = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    LoadLibraryA((dir + "\\Kernel_7c.dll").c_str());
    LoadLibraryA((dir + "\\KernelResource_7c.dll").c_str());
    HMODULE orig = LoadLibraryA((dir + "\\InputSystem_7c_orig.dll").c_str());
    if (!ks || !orig) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    auto* base = reinterpret_cast<unsigned char*>(orig);
    using CtorP = void*(__stdcall*)(void*, void* const*);
    using CtorS = void*(__stdcall*)(void*, void*);
    using Dtor = void(__stdcall*)(void*);
    const CtorS ctorFromString = reinterpret_cast<CtorS>(base + 0x25e0);
    const CtorP ctorFromPString = reinterpret_cast<CtorP>(base + 0x2730);
    const Dtor dtor = reinterpret_cast<Dtor>(base + 0x26d0);
    auto stringCtor = reinterpret_cast<void*(__fastcall*)(void*, void*, const char*)>(GetProcAddress(ks, "??0String@Jet@@QAE@PBD@Z"));
    auto stringDtor = reinterpret_cast<void(__fastcall*)(void*, void*)>(GetProcAddress(ks, "??1String@Jet@@QAE@XZ"));
    auto cache = reinterpret_cast<void**>(GetProcAddress(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A"));
    auto create = reinterpret_cast<void*(__cdecl*)(void*, const unsigned char*)>(GetProcAddress(ks, "?Create@PStringCache@Jet@@QAAPAUNode@12@PBE@Z"));
    auto destroy = reinterpret_cast<void(__cdecl*)(void*, void*)>(GetProcAddress(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z"));
    if (!stringCtor || !stringDtor || !cache || !create || !destroy) {
        std::printf("missing KernelStandard exports\n");
        return 2;
    }
    // the words: the scanner's own and the input names
    std::vector<std::string> words = {"pressed", "released", "+shift", "+ctrl", "+alt", "inc", "dec", "increase",
                                      "decrease", "changed", "positive", "negative", "pos", "neg", "true", "false",
                                      "N", "NE", "E", "SE", "S", "SW", "W", "NW", "C", "center", "centre", "key",
                                      "button", "axis", "pov", "action", "variant", "flush", "actionmap", "version",
                                      "shift", "ctrl", "alt", "Shift", "PRESSED", "MouseEmulator"};
    for (const char* p : {"KC_", "BC_", "AC_", "PC_"})
        for (const char* s : {"Esc", "A", "Z", "1", "F1", "Left", "Right", "X", "Y", "Z", "RX", "1", "2", "32",
                              "Space", "Return", "Up", "Numpad0", "Middle", "4", "LShift"})
            words.push_back(std::string(p) + s);
    const char* const punct[] = {" ", " ", "\t", "\n", "\r\n", ",", ";", "=", "(", ")", "{", "}", "\"", "//", "/*",
                                 "*/", "#", ".", "-", "+", "_"};
    unsigned long long checks = 0, bad = 0, tokens = 0, names = 0;
    unsigned long long kinds[64] = {};
    alignas(16) static unsigned char obj[0x800];
    for (int n = 0; n < texts; ++n) {
        std::string text;
        const unsigned parts = Pick(20);
        for (unsigned i = 0; i < parts; ++i) {
            switch (Pick(10)) {
            case 0: text += std::to_string(static_cast<int>(rng() % 100000) - (Pick(4) ? 0 : 50000)); break;
            case 1: {
                char b[32];
                std::snprintf(b, sizeof(b), Pick(2) ? "0x%x" : "0X%X", rng() % 0x100000);
                text += b;
                break;
            }
            case 2: {
                char b[32];
                std::snprintf(b, sizeof(b), "%d.%d", static_cast<int>(rng() % 100), static_cast<int>(rng() % 1000));
                text += b;
                break;
            }
            case 3: text += punct[Pick(sizeof(punct) / sizeof(punct[0]))]; break;
            case 4: text += static_cast<char>(1 + Pick(255)); break;
            case 5: {  // a word, cut or with a random tail of name characters (backing up, the clamp at 0x7b)
                std::string w = words[Pick(static_cast<unsigned>(words.size()))];
                w.resize(Pick(static_cast<unsigned>(w.size()) + 1));
                static const char chars[] = "abcxyzABCXYZ0189_.+{|}~";
                for (unsigned k = Pick(6); k; --k) w += chars[Pick(sizeof(chars) - 1)];
                text += w;
                break;
            }
            default: text += words[Pick(static_cast<unsigned>(words.size()))]; break;
            }
            if (Pick(3)) text += ' ';
        }
        const bool fromPString = Pick(2) != 0;
        std::vector<std::string> seen[2];
        // ours
        {
            Jet::InputLex lex(text.c_str(), static_cast<unsigned long>(text.size() + (fromPString ? 1 : 0)));
            for (int i = 0; i < 4000; ++i) {
                const int t = lex.Lex();
                char b[64];
                // yylval is set only by the number rules (the names leave a pointer to their buffer there)
                std::snprintf(b, sizeof(b), "%d %08x", t, t >= 1 && t <= 3 ? static_cast<unsigned>(lex.IntValue()) : 0u);
                std::string s = b;
                if (t >= 0x19 && t <= 0x1c) s += std::string(" ") + lex.Text();
                seen[0].push_back(s);
                if (t <= 0) break;
            }
        }
        // the original's
        {
            void* node = nullptr;
            alignas(8) unsigned char str[16] = {};
            std::memset(obj, 0xcc, sizeof(obj));
            if (fromPString) {
                node = create(*cache, reinterpret_cast<const unsigned char*>(text.c_str()));
                ctorFromPString(obj, &node);
            } else {
                stringCtor(str, nullptr, text.c_str());
                ctorFromString(obj, str);
            }
            void** vt = *reinterpret_cast<void***>(obj);
            auto yylex = reinterpret_cast<int(__fastcall*)(void*, void*)>(vt[5]);
            for (int i = 0; i < 4000; ++i) {
                const int t = yylex(obj, nullptr);
                const uint32_t v = *reinterpret_cast<uint32_t*>(obj + 0x78);
                char b[64];
                std::snprintf(b, sizeof(b), "%d %08x", t, t >= 1 && t <= 3 ? v : 0u);
                std::string s = b;
                if (t >= 0x19 && t <= 0x1c) s += std::string(" ") + reinterpret_cast<const char*>(obj + 0x84);
                seen[1].push_back(s);
                if (t <= 0) break;
            }
            dtor(obj);
            if (node) destroy(*cache, node);
            if (!fromPString) stringDtor(str, nullptr);
        }
        const size_t m = seen[0].size() > seen[1].size() ? seen[0].size() : seen[1].size();
        for (size_t i = 0; i < m; ++i) {
            ++checks;
            const std::string x = i < seen[0].size() ? seen[0][i] : "(end)";
            const std::string y = i < seen[1].size() ? seen[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 8) {
                    std::string shown;
                    for (char c : text) shown += (c >= 32 && c < 127) ? c : '?';
                    std::printf("text %d (%s) token %zu:\n  ours %s\n  orig %s\n  \"%s\"\n", n,
                                fromPString ? "PString" : "String", i, x.c_str(), y.c_str(), shown.c_str());
                }
                break;
            }
            const int t = std::atoi(x.c_str());
            if (t > 0) {
                ++tokens;
                ++kinds[t & 63];
                if (t >= 0x19) ++names;
            }
        }
    }
    std::printf("tokens %llu (names %llu); by kind:", tokens, names);
    for (int k = 1; k <= 0x1c; ++k) std::printf(" %d:%llu", k, kinds[k]);
    std::printf("\n%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
