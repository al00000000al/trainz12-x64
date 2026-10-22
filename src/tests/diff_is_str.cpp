// Differential test of InputSystem_7c unit U3 (ASCIICode, InputStrings): our module against
// InputSystem_7c_orig.dll in one process. Both singletons are compared whole: the ASCIICode table byte for byte,
// GetChar for every key and flag combination, IsControl; every InputStrings name (both modules intern into the
// same PString cache, so equal texts are equal nodes), the code -> name lookups over the whole code range (they
// mask), the name -> code lookups for every name and some foreign strings, and the copy constructor, assignment
// and destructor on copies.
//
// usage: diff_is_str.exe <dir with InputSystem_7c.dll, InputSystem_7c_orig.dll and the Jet modules>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

static unsigned long long checks, bad;

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

static void Check(const std::string& what, const std::string& a, const std::string& b)
{
    ++checks;
    if (a != b && ++bad <= 20) std::printf("%s:\n  ours %s\n  orig %s\n", what.c_str(), a.c_str(), b.c_str());
}

static std::string Node(uintptr_t node)
{
    if (!node) return "(null)";
    return "'" + std::string(reinterpret_cast<const char*>(node + 0x18)) + "'@" + std::to_string(node);
}

struct Api {
    const void*(__cdecl* asciiGet)();
    unsigned short(__cdecl* getChar)(const void*, int, bool, bool, bool);
    bool(__cdecl* isControl)(const void*, int);
    const void*(__cdecl* stringsGet)();
    uintptr_t*(__cdecl* toString[4])(const void*, uintptr_t*, int);  // key, axis, pov, povdir
    uintptr_t*(__cdecl* buttonToString)(const void*, uintptr_t*, int, bool);
    int(__cdecl* fromString[5])(const void*, const uintptr_t*);  // key, button, axis, pov, povdir
    void*(__fastcall* copy)(void*, void*, const void*);
    void*(__fastcall* assign)(void*, void*, const void*);
    void(__fastcall* dtor)(void*);

    explicit Api(HMODULE m)
    {
        asciiGet = Get<decltype(asciiGet)>(m, "?Get@ASCIICode@Jet@@SAABV12@XZ");
        getChar = Get<decltype(getChar)>(m, "?GetChar@ASCIICode@Jet@@QBAGW4KeyCode@2@_N11@Z");
        isControl = Get<decltype(isControl)>(m, "?IsControl@ASCIICode@Jet@@QBA_NW4KeyCode@2@@Z");
        stringsGet = Get<decltype(stringsGet)>(m, "?Get@InputStrings@Jet@@SAABV12@XZ");
        toString[0] = Get<std::remove_reference_t<decltype(toString[0])>>(m, "?KeyToString@InputStrings@Jet@@QBA?AVPString@2@W4KeyCode@2@@Z");
        toString[1] = Get<std::remove_reference_t<decltype(toString[0])>>(m, "?AxisToString@InputStrings@Jet@@QBA?AVPString@2@W4AxisCode@2@@Z");
        toString[2] = Get<std::remove_reference_t<decltype(toString[0])>>(m, "?PovToString@InputStrings@Jet@@QBA?AVPString@2@W4PovCode@2@@Z");
        toString[3] = Get<std::remove_reference_t<decltype(toString[0])>>(m, "?PovDirToString@InputStrings@Jet@@QBA?AVPString@2@W4PovDir@2@@Z");
        buttonToString = Get<decltype(buttonToString)>(m, "?ButtonToString@InputStrings@Jet@@QBA?AVPString@2@W4ButtonCode@2@_N@Z");
        fromString[0] = Get<std::remove_reference_t<decltype(fromString[0])>>(m, "?StringToKey@InputStrings@Jet@@QBA?AW4KeyCode@2@ABVPString@2@@Z");
        fromString[1] = Get<std::remove_reference_t<decltype(fromString[0])>>(m, "?StringToButton@InputStrings@Jet@@QBA?AW4ButtonCode@2@ABVPString@2@@Z");
        fromString[2] = Get<std::remove_reference_t<decltype(fromString[0])>>(m, "?StringToAxis@InputStrings@Jet@@QBA?AW4AxisCode@2@ABVPString@2@@Z");
        fromString[3] = Get<std::remove_reference_t<decltype(fromString[0])>>(m, "?StringToPov@InputStrings@Jet@@QBA?AW4PovCode@2@ABVPString@2@@Z");
        fromString[4] = Get<std::remove_reference_t<decltype(fromString[0])>>(m, "?StringToPovDir@InputStrings@Jet@@QBA?AW4PovDir@2@ABVPString@2@@Z");
        copy = Get<decltype(copy)>(m, "??0InputStrings@Jet@@QAE@ABV01@@Z");
        assign = Get<decltype(assign)>(m, "??4InputStrings@Jet@@QAEAAV01@ABV01@@Z");
        dtor = Get<decltype(dtor)>(m, "??1InputStrings@Jet@@QAE@XZ");
    }
};

static std::string Dump(const void* strings)
{
    std::string s;
    const auto* n = static_cast<const uintptr_t*>(strings);
    for (int i = 0; i < 0x13a; ++i) s += Node(n[i]) + ",";
    return s;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_is_str <dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    SetDllDirectoryA(dir.c_str());
    for (const char* dep : {"KernelStandard_7c", "Kernel_7c", "KernelResource_7c"})
        if (!LoadLibraryA((dir + "\\" + dep + ".dll").c_str())) std::printf("cannot load %s\n", dep);
    HMODULE mods[2] = {LoadLibraryA((dir + "\\InputSystem_7c.dll").c_str()),
                       LoadLibraryA((dir + "\\InputSystem_7c_orig.dll").c_str())};
    if (!mods[0] || !mods[1]) {
        std::printf("cannot load the modules\n");
        return 2;
    }
    const Api api[2] = {Api(mods[0]), Api(mods[1])};

    // ASCIICode
    const void* ascii[2] = {api[0].asciiGet(), api[1].asciiGet()};
    Check("ASCIICode table", std::to_string(std::memcmp(ascii[0], ascii[1], 0x600)), "0");
    Check("ASCIICode singleton", std::to_string(api[0].asciiGet() == ascii[0]), std::to_string(api[1].asciiGet() == ascii[1]));
    for (int key = 0; key < 256; ++key)
        for (int f = 0; f < 8; ++f) {
            std::string r[2];
            for (int s = 0; s < 2; ++s)
                r[s] = std::to_string(api[s].getChar(ascii[s], key, f & 1, (f & 2) != 0, (f & 4) != 0)) + " " +
                       std::to_string(api[s].isControl(ascii[s], key));
            Check("GetChar " + std::to_string(key) + " flags " + std::to_string(f), r[0], r[1]);
        }

    // InputStrings
    const void* strs[2] = {api[0].stringsGet(), api[1].stringsGet()};
    Check("InputStrings names", Dump(strs[0]), Dump(strs[1]));
    for (int s = 0; s < 2; ++s) {
        const auto* n = static_cast<const uintptr_t*>(strs[s]);
        Check("name of key 1", n[1] ? reinterpret_cast<const char*>(n[1] + 0x18) : "(null)", "KC_Esc");
    }
    for (int code = -40; code < 300; ++code) {
        std::string r[2];
        for (int s = 0; s < 2; ++s) {
            uintptr_t out[6];
            for (int k = 0; k < 4; ++k) r[s] += Node(*api[s].toString[k](strs[s], &out[k], code)) + " ";
            r[s] += Node(*api[s].buttonToString(strs[s], &out[4], code, false)) + " ";
            r[s] += Node(*api[s].buttonToString(strs[s], &out[5], code, true));
        }
        Check("to string " + std::to_string(code), r[0], r[1]);
    }
    // every name of ours, the null string and a foreign interned string, through all five reverse lookups
    const auto* names = static_cast<const uintptr_t*>(strs[1]);
    uintptr_t foreign[2] = {0, names[1] + 0};
    for (int i = -2; i < 0x13a; ++i) {
        const uintptr_t key = i == -2 ? 0 : i == -1 ? reinterpret_cast<uintptr_t>(&foreign) : names[i];
        std::string r[2];
        for (int s = 0; s < 2; ++s)
            for (int k = 0; k < 5; ++k) r[s] += std::to_string(api[s].fromString[k](strs[s], &key)) + " ";
        Check("from string " + std::to_string(i), r[0], r[1]);
    }
    // copy, assignment (onto a copy of the other module's), destruction
    alignas(8) unsigned char a[2][0x4e8], b[2][0x4e8];
    for (int s = 0; s < 2; ++s) {
        api[s].copy(a[s], nullptr, strs[s]);
        api[s].copy(b[s], nullptr, strs[1 - s]);
        api[s].assign(b[s], nullptr, a[s]);
    }
    Check("copies", Dump(a[0]) + Dump(b[0]), Dump(a[1]) + Dump(b[1]));
    for (int s = 0; s < 2; ++s) {
        api[s].dtor(a[s]);
        api[s].dtor(b[s]);
    }
    Check("names after the copies went", Dump(strs[0]), Dump(strs[1]));
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
