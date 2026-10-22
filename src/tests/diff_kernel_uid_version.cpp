// Differential test: Jet::UId / Jet::Version in our Kernel_7c.dll vs Kernel_7c_orig.dll, on random inputs.
// Calls the exports through raw pointers so both sides run their own machine code.
// __thiscall is emulated with __fastcall(ECX = this, EDX unused); __cdecl members take `this` on the stack.
// usage: diff_kernel_uid_version.exe <our Kernel_7c.dll> <game bin dir holding Kernel_7c_orig.dll>
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <random>

using Ctor0 = void*(__fastcall*)(void*, void*);
using CtorI = void*(__fastcall*)(void*, void*, int);
using CtorKGG = void*(__fastcall*)(void*, void*, unsigned long, unsigned short, unsigned short);
using CtorKGGB = void*(__fastcall*)(void*, void*, unsigned long, unsigned short, unsigned short, const unsigned char*);
using CtorS = void*(__fastcall*)(void*, void*, const char*);
using Assign = void*(__fastcall*)(void*, void*, const void*);
using ThisVoid = void(__fastcall*)(void*, void*);
using CmpCdecl = bool(__cdecl*)(const void*, const void*);
using PredCdecl = bool(__cdecl*)(const void*);
using HashCdecl = unsigned(__cdecl*)(const void*);
using VCtorEEG = void*(__fastcall*)(void*, void*, unsigned char, unsigned char, unsigned short);
using VCtorK = void*(__fastcall*)(void*, void*, unsigned long);
using CmpThis = bool(__fastcall*)(const void*, void*, const void*);

static HMODULE ours, orig;
static int failures = 0;

template <class F>
static F get(HMODULE m, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing export %s\n", name);
        std::exit(2);
    }
    return p;
}

static void check(bool same, const char* what, int iter)
{
    if (!same && failures++ < 20) std::printf("MISMATCH %s at iteration %d\n", what, iter);
}

int main(int argc, char** argv)
{
    if (argc < 3) return 2;
    SetDllDirectoryA(argv[2]);
    ours = LoadLibraryA(argv[1]);
    char origPath[MAX_PATH];
    std::snprintf(origPath, sizeof origPath, "%s\\Kernel_7c_orig.dll", argv[2]);
    orig = LoadLibraryA(origPath);
    if (!ours || !orig || ours == orig) {
        std::printf("load failed: ours=%p orig=%p err=%lu\n", (void*)ours, (void*)orig, GetLastError());
        return 2;
    }

    std::mt19937 rng(12345);
    auto rnd32 = [&] { return static_cast<unsigned long>(rng()); };
    auto fill = [&](unsigned char* p, size_t n) { for (size_t i = 0; i < n; ++i) p[i] = static_cast<unsigned char>(rng()); };

    const char* cmpNames[] = {"??8UId@Jet@@QBA_NABV01@@Z", "??9UId@Jet@@QBA_NABV01@@Z", "??MUId@Jet@@QBA_NABV01@@Z",
                              "??OUId@Jet@@QBA_NABV01@@Z", "??NUId@Jet@@QBA_NABV01@@Z", "??PUId@Jet@@QBA_NABV01@@Z"};
    const int N = 200000;
    int iter = 0;

    // ---- UId comparisons, bool, !, hash, assign, Zero
    for (; iter < N; ++iter) {
        unsigned char a[16], b[16];
        fill(a, 16);
        std::memcpy(b, a, 16);
        int mode = iter % 5;  // equal, differ in one random byte, fully random, zero a, zero both
        if (mode == 1) b[rng() % 16] ^= static_cast<unsigned char>(1 + rng() % 255);
        if (mode == 2) fill(b, 16);
        if (mode >= 3) std::memset(a, 0, 16);
        if (mode == 4) std::memset(b, 0, 16);
        for (const char* n : cmpNames)
            check(get<CmpCdecl>(ours, n)(a, b) == get<CmpCdecl>(orig, n)(a, b), n, iter);
        check(get<PredCdecl>(ours, "??BUId@Jet@@QBA_NXZ")(a) == get<PredCdecl>(orig, "??BUId@Jet@@QBA_NXZ")(a), "bool", iter);
        check(get<PredCdecl>(ours, "??7UId@Jet@@QBA_NXZ")(a) == get<PredCdecl>(orig, "??7UId@Jet@@QBA_NXZ")(a), "!", iter);
        check(get<HashCdecl>(ours, "?GetHashID@UId@Jet@@QBAIXZ")(a) == get<HashCdecl>(orig, "?GetHashID@UId@Jet@@QBAIXZ")(a), "hash", iter);
        unsigned char x[16], y[16];
        fill(x, 16);
        std::memcpy(y, x, 16);
        void* rx = get<Assign>(ours, "??4UId@Jet@@QAEAAV01@ABV01@@Z")(x, nullptr, a);
        void* ry = get<Assign>(orig, "??4UId@Jet@@QAEAAV01@ABV01@@Z")(y, nullptr, a);
        check(std::memcmp(x, y, 16) == 0 && rx == x && ry == y, "operator=", iter);
        get<ThisVoid>(ours, "?Zero@UId@Jet@@QAEXXZ")(x, nullptr);
        get<ThisVoid>(orig, "?Zero@UId@Jet@@QAEXXZ")(y, nullptr);
        check(std::memcmp(x, y, 16) == 0, "Zero", iter);
    }

    // ---- UId constructors
    for (int i = 0; i < N; ++i, ++iter) {
        unsigned char x[16], y[16], d4[8];
        fill(x, 16);
        std::memcpy(y, x, 16);
        fill(d4, 8);
        unsigned long d1 = rnd32();
        unsigned short d2 = static_cast<unsigned short>(rng()), d3 = static_cast<unsigned short>(rng());
        get<CtorKGG>(ours, "??0UId@Jet@@QAE@KGG@Z")(x, nullptr, d1, d2, d3);
        get<CtorKGG>(orig, "??0UId@Jet@@QAE@KGG@Z")(y, nullptr, d1, d2, d3);
        check(std::memcmp(x, y, 16) == 0, "UId(KGG)", iter);
        const unsigned char* bytes = (i % 7 == 0) ? nullptr : d4;
        get<CtorKGGB>(ours, "??0UId@Jet@@QAE@KGGQBE@Z")(x, nullptr, d1, d2, d3, bytes);
        get<CtorKGGB>(orig, "??0UId@Jet@@QAE@KGGQBE@Z")(y, nullptr, d1, d2, d3, bytes);
        check(std::memcmp(x, y, 16) == 0, "UId(KGG,bytes)", iter);
        get<CtorI>(ours, "??0UId@Jet@@QAE@H@Z")(x, nullptr, static_cast<int>(rng()));
        get<CtorI>(orig, "??0UId@Jet@@QAE@H@Z")(y, nullptr, static_cast<int>(rng()));
        check(std::memcmp(x, y, 16) == 0, "UId(int)", iter);
        fill(x, 16);
        std::memcpy(y, x, 16);
        get<Ctor0>(ours, "??0UId@Jet@@QAE@XZ")(x, nullptr);
        get<Ctor0>(orig, "??0UId@Jet@@QAE@XZ")(y, nullptr);
        check(std::memcmp(x, y, 16) == 0, "UId()", iter);

        // string form, mixing case and occasionally junk characters
        char s[48];
        std::snprintf(s, sizeof s, (i & 1) ? "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"
                                           : "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                      d1, d2, d3, d4[0], d4[1], d4[2], d4[3], d4[4], d4[5], d4[6], d4[7]);
        if (i % 11 == 0) s[rng() % 36] = "G-zx 9"[rng() % 6];
        if (i % 13 == 0) s[rng() % 36] = 0;  // short string
        fill(x, 16);
        std::memcpy(y, x, 16);
        get<CtorS>(ours, "??0UId@Jet@@QAE@PBD@Z")(x, nullptr, s);
        get<CtorS>(orig, "??0UId@Jet@@QAE@PBD@Z")(y, nullptr, s);
        check(std::memcmp(x, y, 16) == 0, "UId(const char*)", iter);
    }

    // ---- Version
    const char* vcmp[] = {"??8Version@Jet@@QBE_NABU01@@Z", "??9Version@Jet@@QBE_NABU01@@Z", "??MVersion@Jet@@QBE_NABU01@@Z",
                          "??OVersion@Jet@@QBE_NABU01@@Z", "??NVersion@Jet@@QBE_NABU01@@Z", "??PVersion@Jet@@QBE_NABU01@@Z"};
    for (int i = 0; i < N; ++i, ++iter) {
        unsigned long x = rnd32(), y = x;
        unsigned char ma = static_cast<unsigned char>(rng()), mi = static_cast<unsigned char>(rng());
        unsigned short bu = static_cast<unsigned short>(rng());
        get<VCtorEEG>(ours, "??0Version@Jet@@QAE@EEG@Z")(&x, nullptr, ma, mi, bu);
        get<VCtorEEG>(orig, "??0Version@Jet@@QAE@EEG@Z")(&y, nullptr, ma, mi, bu);
        check(x == y, "Version(EEG)", iter);
        unsigned long k = rnd32();
        get<VCtorK>(ours, "??0Version@Jet@@QAE@K@Z")(&x, nullptr, k);
        get<VCtorK>(orig, "??0Version@Jet@@QAE@K@Z")(&y, nullptr, k);
        check(x == y, "Version(K)", iter);
        unsigned long a = rnd32(), b = (i % 3 == 0) ? a : rnd32();
        for (const char* n : vcmp)
            check(get<CmpThis>(ours, n)(&a, nullptr, &b) == get<CmpThis>(orig, n)(&a, nullptr, &b), n, iter);
        unsigned long p = rnd32(), q = p;
        void* rp = get<Assign>(ours, "??4Version@Jet@@QAEAAU01@ABU01@@Z")(&p, nullptr, &a);
        void* rq = get<Assign>(orig, "??4Version@Jet@@QAEAAU01@ABU01@@Z")(&q, nullptr, &a);
        check(p == q && rp == &p && rq == &q, "Version=", iter);
    }

    // ---- UId Read/Write/GetString through real KernelStandard objects
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!ks) {
        std::printf("KernelStandard_7c.dll not loaded\n");
        return 2;
    }
    using DBCtor = void*(__fastcall*)(void*, void*, unsigned long, unsigned long);
    using DBVoid = void(__fastcall*)(void*, void*);
    using DBLen = unsigned long(__fastcall*)(const void*, void*);
    using DBData = void*(__fastcall*)(const void*, void*);
    using DBWriteRaw = unsigned long(__fastcall*)(void*, void*, const void*, unsigned long, unsigned long);
    using UIdBuf = bool(__cdecl*)(void*, void*);
    using UIdStr = void*(__cdecl*)(const void*, void*);  // member returning a class: this, then hidden result
    using GetANSI = int(__cdecl*)(void*, void*, char*, unsigned);
    using Destroy = void(__cdecl*)(void*, void*);
    auto dbCtor = get<DBCtor>(ks, "??0DynamicBuffer@Jet@@QAE@KK@Z");
    auto dbDtor = get<DBVoid>(ks, "??1DynamicBuffer@Jet@@QAE@XZ");
    auto dbRewind = get<DBVoid>(ks, "?Rewind@DynamicBuffer@Jet@@QAEXXZ");
    auto dbLen = get<DBLen>(ks, "?GetLength@DynamicBuffer@Jet@@QBEKXZ");
    auto dbData = get<DBData>(ks, "?GetData@DynamicBuffer@Jet@@QBEPAXXZ");
    auto dbWrite = get<DBWriteRaw>(ks, "?Write@DynamicBuffer@Jet@@QAEKPBXKK@Z");
    auto cache = *get<void**>(ks, "?cache@PString@Jet@@0PAVPStringCache@2@A");
    auto getAnsi = get<GetANSI>(ks, "?GetANSI@PStringCache@Jet@@QBAHPAUNode@12@PADI@Z");
    auto destroy = get<Destroy>(ks, "?Destroy@PStringCache@Jet@@QAAXPAUNode@12@@Z");
    const char* W = "?Write@UId@Jet@@QAA_NAAVDynamicBuffer@2@@Z";
    const char* R = "?Read@UId@Jet@@QAA_NAAVDynamicBuffer@2@@Z";
    const char* S = "?GetString@UId@Jet@@QBA?AVPString@2@XZ";
    alignas(16) unsigned char bufA[256], bufB[256];  // DynamicBuffer is small; 256 bytes is ample room

    for (int i = 0; i < 20000; ++i, ++iter) {
        unsigned char u[16];
        fill(u, 16);
        dbCtor(bufA, nullptr, 64, 64);
        dbCtor(bufB, nullptr, 64, 64);
        bool ra = get<UIdBuf>(ours, W)(u, bufA), rb = get<UIdBuf>(orig, W)(u, bufB);
        check(ra == rb && dbLen(bufA, nullptr) == dbLen(bufB, nullptr) &&
                  std::memcmp(dbData(bufA, nullptr), dbData(bufB, nullptr), dbLen(bufA, nullptr)) == 0,
              "UId::Write", iter);
        dbDtor(bufA, nullptr);
        dbDtor(bufB, nullptr);

        // read back from a buffer of 0..16 bytes: short buffers must fail at the same field
        unsigned char src[16];
        fill(src, 16);
        unsigned long n = (i % 4 == 0) ? rng() % 17 : 16;
        dbCtor(bufA, nullptr, 64, 64);
        dbCtor(bufB, nullptr, 64, 64);
        if (n) {
            dbWrite(bufA, nullptr, src, n, 1);
            dbWrite(bufB, nullptr, src, n, 1);
        }
        dbRewind(bufA, nullptr);
        dbRewind(bufB, nullptr);
        unsigned char x[16], y[16];
        fill(x, 16);
        std::memcpy(y, x, 16);
        ra = get<UIdBuf>(ours, R)(x, bufA);
        rb = get<UIdBuf>(orig, R)(y, bufB);
        check(ra == rb && std::memcmp(x, y, 16) == 0, "UId::Read", iter);
        dbDtor(bufA, nullptr);
        dbDtor(bufB, nullptr);

        void* na = nullptr;
        void* nb = nullptr;
        get<UIdStr>(ours, S)(u, &na);
        get<UIdStr>(orig, S)(u, &nb);
        char sa[64] = {}, sb[64] = {};
        getAnsi(cache, na, sa, sizeof sa);
        getAnsi(cache, nb, sb, sizeof sb);
        check(std::strcmp(sa, sb) == 0 && sa[0], "UId::GetString", iter);
        if (i == 0) std::printf("GetString sample: %s\n", sa);
        if (na) destroy(cache, na);
        if (nb) destroy(cache, nb);
    }

    // ---- SetRnd: random by design, so check shape instead of equality
    using UIdVoidC = void(__cdecl*)(void*);
    unsigned char prev[16] = {};
    for (int i = 0; i < 1000; ++i, ++iter) {
        unsigned char g[16] = {};
        get<UIdVoidC>(ours, "?SetRnd@UId@Jet@@QAAXXZ")(g);
        bool nonzero = false;
        for (unsigned char b : g) nonzero |= b != 0;
        check(nonzero && std::memcmp(g, prev, 16) != 0 && (g[7] >> 4) == 4, "UId::SetRnd v4 guid", iter);
        std::memcpy(prev, g, 16);
    }

    std::printf("%d checks rounds, %d mismatches\n", iter, failures);
    return failures ? 1 : 0;
}
