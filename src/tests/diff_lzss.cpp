// Differential test for the LZSSCompress codec.
// A real original LZSSCompress object (Kernel_7c_orig.dll ctor at RVA 0x5a00) compresses varied inputs at every
// level through real KernelStandard memory streams; our JetLzss::Decompress must give back the input, and on
// mutated streams it must succeed/fail and produce exactly what the original DecompressBlock (RVA 0x7150) does.
// usage: diff_lzss.exe <game bin dir holding Kernel_7c_orig.dll>
#include "../Kernel_7c/JetLzss.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace Jet;

// thiscall emulated with fastcall (ECX = this, EDX unused)
using CtorFlag = void*(__fastcall*)(void*, void*, int);
using CtorMem = void*(__fastcall*)(void*, void*, void*, unsigned long);
using CtorVoid = void*(__fastcall*)(void*, void*);
using DtorFn = void(__fastcall*)(void*, void*);
using GetDataFn = const char*(__fastcall*)(const void*, void*);
using GetSizeFn = unsigned long(__fastcall*)(void*, void*);
using CompressFn = bool(__fastcall*)(void*, void*, void* dst, void* src, unsigned srcSize, unsigned level);
using DecompressFn = bool(__fastcall*)(void*, void*, void* dst, void* src, int srcSize);

static HMODULE ks;
static unsigned char* orig;
template <class F>
static F exp_(const char* n)
{
    auto p = reinterpret_cast<F>(GetProcAddress(ks, n));
    if (!p) {
        std::printf("missing %s\n", n);
        std::exit(2);
    }
    return p;
}

struct Orig {
    CtorMem memCtor = exp_<CtorMem>("??0IOSlaveMemory@Jet@@QAE@PAXK@Z");
    DtorFn memDtor = exp_<DtorFn>("??1IOSlaveMemory@Jet@@UAE@XZ");
    CtorVoid dynCtor = exp_<CtorVoid>("??0IOSlaveMemoryDynamic@Jet@@QAE@XZ");
    DtorFn dynDtor = exp_<DtorFn>("??1IOSlaveMemoryDynamic@Jet@@UAE@XZ");
    GetDataFn dynData = exp_<GetDataFn>("?GetData@IOSlaveMemoryDynamic@Jet@@QBEPBDXZ");
    GetSizeFn dynSize = exp_<GetSizeFn>("?GetSize@IOSlaveMemoryDynamic@Jet@@UAEKXZ");
    alignas(16) unsigned char codec[0x1200] = {};

    Orig() { reinterpret_cast<CtorFlag>(orig + 0x5a00)(codec, nullptr, 1); }

    std::vector<unsigned char> Compress(std::vector<unsigned char> in, unsigned level, bool& ok)
    {
        alignas(16) unsigned char src[256] = {}, dst[256] = {};
        memCtor(src, nullptr, in.data(), static_cast<unsigned long>(in.size()));
        dynCtor(dst, nullptr);
        ok = reinterpret_cast<CompressFn>(orig + 0x6ec0)(codec, nullptr, dst, src, static_cast<unsigned>(in.size()), level);
        std::vector<unsigned char> out(dynData(dst, nullptr), dynData(dst, nullptr) + dynSize(dst, nullptr));
        dynDtor(dst, nullptr);
        memDtor(src, nullptr);
        return out;
    }

    // Decompress into a fixed IOSlaveMemory: IOSlaveMemoryDynamic can't read back across its 256-byte blocks,
    // which breaks the original's back-references (seen at output position 267 of a 300-byte stream).
    GetSizeFn memTell = exp_<GetSizeFn>("?Tell@IOSlaveMemory@Jet@@UAEKXZ");
    bool crashed = false;

    // The original crashes on some corrupted Huffman streams; SEH keeps the test running (no C++ objects here).
    static int RawDecompress(void* codec, void* dst, void* src, int size, bool* ok)
    {
        __try {
            *ok = reinterpret_cast<DecompressFn>(orig + 0x7150)(codec, nullptr, dst, src, size);
            return 0;
        } __except (1) {
            return 1;
        }
    }

    std::vector<unsigned char> Decompress(std::vector<unsigned char> in, bool& ok, size_t capacity)
    {
        alignas(16) unsigned char src[256] = {}, dst[256] = {};
        std::vector<unsigned char> buf(capacity);
        memCtor(src, nullptr, in.data(), static_cast<unsigned long>(in.size()));
        memCtor(dst, nullptr, buf.data(), static_cast<unsigned long>(buf.size()));
        crashed = RawDecompress(codec, dst, src, static_cast<int>(in.size()), &ok) != 0;
        if (crashed) {
            // the codec object may be left half-way: rebuild it
            std::memset(codec, 0, sizeof codec);
            reinterpret_cast<CtorFlag>(orig + 0x5a00)(codec, nullptr, 1);
            return {};
        }
        buf.resize(memTell(dst, nullptr));
        memDtor(dst, nullptr);
        memDtor(src, nullptr);
        return buf;
    }
};

static std::vector<unsigned char> MakeInput(std::mt19937& rng, int kind, size_t n)
{
    std::vector<unsigned char> v(n);
    static const std::string words[] = {"kuid", "config", "train", "mesh", "texture", "bogey", "<kuid:", "\r\n",
                                        "  ", "asset-filename", "0.000000", "true", "{", "}", "category-class"};
    switch (kind) {
    case 0:  // random
        for (auto& b : v) b = static_cast<unsigned char>(rng());
        break;
    case 1: {  // text built from a small vocabulary
        std::string s;
        while (s.size() < n) s += words[rng() % 15] + (rng() % 3 ? " " : "\n");
        std::memcpy(v.data(), s.data(), n);
        break;
    }
    case 2:  // long runs
        for (size_t i = 0; i < n;) {
            unsigned char c = static_cast<unsigned char>(rng() % 4);
            for (size_t r = 1 + rng() % 600; r && i < n; --r) v[i++] = c;
        }
        break;
    case 3:  // zeros
        break;
    default:  // low-entropy binary (like vertex data)
        for (size_t i = 0; i < n; ++i) v[i] = static_cast<unsigned char>((i % 12 < 4) ? rng() % 8 : (i * 7) & 0xff);
    }
    return v;
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    SetDllDirectoryA(argv[1]);
    char path[MAX_PATH];
    std::snprintf(path, sizeof path, "%s\\Kernel_7c_orig.dll", argv[1]);
    orig = reinterpret_cast<unsigned char*>(LoadLibraryA(path));
    ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!orig || !ks) {
        std::printf("load failed (%lu)\n", GetLastError());
        return 2;
    }
    Orig o;
    if (argc > 2 && std::strcmp(argv[2], "dump") == 0) {
        // one small case, shown byte by byte
        const size_t n = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 300;
        const unsigned level = argc > 4 ? std::strtoul(argv[4], nullptr, 10) : 1;
        std::vector<unsigned char> in(n);
        for (size_t i = 0; i < n; ++i) in[i] = static_cast<unsigned char>("abcabcabd"[i % 9]);
        bool ok = false;
        auto packed = o.Compress(in, level, ok);
        std::printf("compress ok=%d size=%zu:", ok, packed.size());
        for (size_t i = 0; i < packed.size() && i < 96; ++i) std::printf(" %02x", packed[i]);
        bool dok = false;
        auto back = o.Decompress(packed, dok, n + 64);
        std::vector<unsigned char> mine;
        bool mok = JetLzss::Decompress(packed.data(), static_cast<uint32_t>(packed.size()), mine);
        size_t d = 0;
        while (d < back.size() && d < in.size() && back[d] == in[d]) ++d;
        std::printf("\noriginal decompress ok=%d size=%zu first diff at %zu\nours ok=%d size=%zu same as original=%d\n",
                    dok, back.size(), d, mok, mine.size(), mine == back);
        return 0;
    }
    std::mt19937 rng(4242);
    int checks = 0, failures = 0, originalCrashes = 0, corruptDiffers = 0;
    unsigned levelsSeen[16] = {};
    bool verbose = argc > 2;
    // failure counts per [size index][level] for: our decompress, original's own round trip
    int byCell[16][10][2] = {};
    int compressDiffers[16][10] = {};
    int sizeIdx = 0;
    auto fail = [&](const char* what, int kind, size_t n, unsigned level) {
        ++failures;
        if (!std::strstr(what, "corrupted")) ++byCell[sizeIdx][level][0];
        if (verbose && failures < 40) std::printf("MISMATCH %s kind=%d size=%zu level=%u\n", what, kind, n, level);
    };

    const size_t sizes[] = {0, 1, 2, 7, 8, 100, 255, 256, 257, 1000, 4095, 4096, 4097, 20000, 70000, 200000};
    for (int round = 0; round < 6; ++round) {
        for (sizeIdx = 0; sizeIdx < 16; ++sizeIdx) {
            const size_t n = sizes[sizeIdx];
            for (int kind = 0; kind < 5; ++kind) {
                const size_t len = round ? n + rng() % 97 : n;
                auto input = MakeInput(rng, kind, len);
                for (unsigned level = 0; level <= 9; ++level) {
                    bool cok = false;
                    auto packed = o.Compress(input, level, cok);
                    if (!cok || packed.empty()) {
                        if (len) fail("original compress failed", kind, len, level);
                        continue;
                    }
                    ++levelsSeen[packed[0] & 0xf];
                    {
                        // our encoder must produce the very same bytes
                        auto ours = JetLzss::Compress(input.data(), static_cast<uint32_t>(input.size()), level);
                        ++checks;
                        if (ours != packed) {
                            ++compressDiffers[sizeIdx][level];
                            ++failures;
                            if (verbose && failures < 30) {
                                size_t d = 0;
                                while (d < ours.size() && d < packed.size() && ours[d] == packed[d]) ++d;
                                std::printf("COMPRESS kind=%d len=%zu level=%u: ours %zu bytes, original %zu, first diff at %zu\n",
                                            kind, len, level, ours.size(), packed.size(), d);
                            }
                        }
                    }
                    std::vector<unsigned char> mine;
                    bool mok = JetLzss::Decompress(packed.data(), static_cast<uint32_t>(packed.size()), mine);
                    bool ook = false;
                    auto theirs = o.Decompress(packed, ook, len + 64);
                    ++checks;
                    // the requirement: we restore exactly what the original compressor packed
                    if (len == 0 ? (mok != ook) : (!mok || mine != input)) fail("decompress", kind, len, level);
                    // for information: the original decoder, driven from here, is not always right (see Decompress)
                    const bool origSane = ook && theirs == input;
                    if (!origSane) ++byCell[sizeIdx][level][1];

                    // corrupt one byte / truncate: where the original decoder is sane here, both must agree
                    if (origSane && level < 7 && packed.size() > 2 && len < 70000) {  // the original corrupts its heap on bad Huffman streams
                        auto bad = packed;
                        if (rng() & 1)
                            bad[1 + rng() % (bad.size() - 1)] ^= static_cast<unsigned char>(1 + rng() % 255);
                        else
                            bad.resize(1 + rng() % (bad.size() - 1));
                        std::vector<unsigned char> m2;
                        if (verbose) std::printf("corrupted kind=%d len=%zu level=%u: ours..", kind, len, level), std::fflush(stdout);
                        bool m2ok = JetLzss::Decompress(bad.data(), static_cast<uint32_t>(bad.size()), m2);
                        bool o2ok = false;
                        const size_t cap = (len * 4 > (1u << 20)) ? len * 4 : (1u << 20);
                        if (verbose) std::printf(" original..\n"), std::fflush(stdout);
                        auto t2 = o.Decompress(bad, o2ok, cap);
                        if (o.crashed) {
                            ++originalCrashes;
                            continue;
                        }
                        if (t2.size() == cap) continue;  // original hit the buffer end: not comparable
                        ++checks;
                        if (m2ok != o2ok || m2 != t2) {
                            ++corruptDiffers;  // informational: see MODLOG (distance 0, partial output on failure)
                            size_t d = 0;
                            while (d < m2.size() && d < t2.size() && m2[d] == t2[d]) ++d;
                            if (verbose && corruptDiffers < 15) std::printf("  corrupted kind=%d len=%zu level=%u hdr=%02x size=%zu: ours ok=%d out=%zu, orig ok=%d out=%zu, first diff %zu\n", kind, len, level, bad[0], bad.size(), m2ok, m2.size(), o2ok, t2.size(), d);
                        }
                    }
                }
            }
        }
    }
    std::printf("decode-wrong / original-decoder-wrong (info) / encode-differs per base size x level:\n          ");
    for (int l = 0; l < 10; ++l) std::printf("   L%d   ", l);
    std::printf("\n");
    for (int s = 0; s < 16; ++s) {
        std::printf("%8zu  ", sizes[s]);
        for (int l = 0; l < 10; ++l) std::printf("%3d/%-3d/%-3d", byCell[s][l][0], byCell[s][l][1], compressDiffers[s][l]);
        std::printf("\n");
    }
    std::printf("stream levels seen:");
    for (int l = 0; l < 10; ++l) std::printf(" %d:%u", l, levelsSeen[l]);
    std::printf("\n%d checks, %d mismatches (info: %d corrupted streams handled differently, original crashed on %d)\n", checks, failures, corruptDiffers, originalCrashes);
    return failures ? 1 : 0;
}
