// Differential test: our AuranBlowfish vs BFEncrypt's methods inside Kernel_7c_orig.dll, called by RVA.
// The original methods are __thiscall, callee-cleaned, with a hidden Result* first: emulated with __fastcall
// (ECX = this, EDX unused). They only touch P (this+0x0c) and S (this+0x54), so a zeroed 0x1060-byte block
// stands in for a real BFEncrypt object.
// usage: diff_blowfish.exe <game bin dir holding Kernel_7c_orig.dll>
#include "../Kernel_7c/AuranBlowfish.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace Jet;
using Method = void(__fastcall*)(void* self, void*, int* result, const void* ptr, unsigned size);

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    SetDllDirectoryA(argv[1]);
    char path[MAX_PATH];
    std::snprintf(path, sizeof path, "%s\\Kernel_7c_orig.dll", argv[1]);
    auto base = reinterpret_cast<unsigned char*>(LoadLibraryA(path));
    if (!base) {
        std::printf("cannot load %s (%lu)\n", path, GetLastError());
        return 2;
    }
    auto setKey = reinterpret_cast<Method>(base + 0x2230);
    auto encrypt = reinterpret_cast<Method>(base + 0x24d0);
    auto decrypt = reinterpret_cast<Method>(base + 0x2550);

    std::mt19937 rng(777);
    std::vector<unsigned char> obj(0x1060);
    AuranBlowfish::State st;
    int failures = 0, checks = 0;
    auto fail = [&](const char* what, int i) {
        if (failures++ < 20) std::printf("MISMATCH %s at %d\n", what, i);
    };

    // error paths
    {
        int r = -1;
        setKey(obj.data(), nullptr, &r, nullptr, 5);
        if (r != AuranBlowfish::SetKey(st, nullptr, 5)) fail("SetKey(null)", 0);
        unsigned char k[300] = {};
        for (unsigned size : {0u, 1u, 55u, 56u, 57u, 256u, 257u, 299u}) {
            r = -1;
            setKey(obj.data(), nullptr, &r, k, size);
            if (r != AuranBlowfish::SetKey(st, k, size)) fail("SetKey code", static_cast<int>(size));
            ++checks;
        }
        unsigned char d[8] = {};
        for (unsigned size : {0u, 7u}) {
            r = -1;
            encrypt(obj.data(), nullptr, &r, d, size);
            if (r != AuranBlowfish::Encrypt(st, d, size)) fail("Encrypt code", static_cast<int>(size));
            r = -1;
            decrypt(obj.data(), nullptr, &r, d, size);
            if (r != AuranBlowfish::Decrypt(st, d, size)) fail("Decrypt code", static_cast<int>(size));
        }
    }

    for (int i = 0; i < 3000; ++i) {
        // keys of every length class: tiny, < 56, exactly 56, long
        unsigned keyLen = (i % 4 == 0) ? 1 + rng() % 8 : (i % 4 == 1) ? 9 + rng() % 47 : (i % 4 == 2) ? 56 : 57 + rng() % 200;
        std::vector<unsigned char> key(keyLen);
        for (auto& b : key) b = static_cast<unsigned char>(rng());
        std::fill(obj.begin(), obj.end(), 0);
        int r = -1;
        setKey(obj.data(), nullptr, &r, key.data(), keyLen);
        int ours = AuranBlowfish::SetKey(st, key.data(), keyLen);
        ++checks;
        if (r != ours || std::memcmp(obj.data() + 0x0c, st.P, sizeof st.P) != 0 ||
            std::memcmp(obj.data() + 0x54, st.S, sizeof st.S) != 0) {
            fail("SetKey state", i);
            continue;
        }
        for (int j = 0; j < 8; ++j) {
            unsigned size = 8 + rng() % 300;
            std::vector<unsigned char> a(size), b;
            for (auto& x : a) x = static_cast<unsigned char>(rng());
            b = a;
            int ra = -1;
            encrypt(obj.data(), nullptr, &ra, a.data(), size);
            int rb = AuranBlowfish::Encrypt(st, b.data(), size);
            ++checks;
            if (ra != rb || a != b) fail("Encrypt", i);
            // decrypt random bytes too, not only our own ciphertext
            for (auto& x : a) x = static_cast<unsigned char>(rng());
            b = a;
            decrypt(obj.data(), nullptr, &ra, a.data(), size);
            rb = AuranBlowfish::Decrypt(st, b.data(), size);
            ++checks;
            if (ra != rb || a != b) fail("Decrypt", i);
            // and the round trip
            std::vector<unsigned char> c = a;
            AuranBlowfish::Encrypt(st, c.data(), size);
            AuranBlowfish::Decrypt(st, c.data(), size);
            ++checks;
            if (c != a) fail("round trip", i);
        }
    }
    std::printf("%d checks, %d mismatches\n", checks, failures);
    return failures ? 1 : 0;
}
