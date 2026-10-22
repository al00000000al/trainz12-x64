// Deterministic results of Kernel_7c's pure-logic code, built for Win32 and for x64 from the same source: LZSS
// (compressed bytes at every level, round trips), the Blowfish variant (key schedule, encrypt, decrypt, error
// codes), FastDynamicArray (random operation runs, whole chunk ring after every step), UId and Version. The
// 32-bit codecs and containers are proven equal to the originals (diff_lzss, diff_blowfish, diff_fda,
// diff_kernel_uid_version), so the Win32 output is the reference: the x64 output must be identical.
//
// usage: golden_kernel.exe <out.txt> [<sizes.txt>]   (sizes.txt: this build's object sizes, informational)
#include <jet/FastDynamicArray.h>
#include <jet/Task.h>
#include <jet/UId.h>
#include <jet/Version.h>

#include "../Kernel_7c/AuranBlowfish.h"
#include "../Kernel_7c/JetLzss.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace Jet;

static FILE* out;

static uint32_t Fnv(const void* data, size_t n, uint32_t h = 2166136261u)
{
    const auto* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

// ---------------------------------------------------------------------------------------------------------------

static std::vector<uint8_t> MakeInput(std::mt19937& rng, int kind, size_t n)
{
    std::vector<uint8_t> v(n);
    static const char* const words[] = {"kuid", "config", "train", "mesh", "texture", "bogey", "<kuid:", "\r\n",
                                        "  ", "asset-filename", "0.000000", "true", "{", "}", "category-class"};
    switch (kind) {
    case 0:
        for (auto& b : v) b = static_cast<uint8_t>(rng());
        break;
    case 1: {
        std::string s;
        while (s.size() < n) s += std::string(words[rng() % 15]) + (rng() % 3 ? " " : "\n");
        if (n) std::memcpy(v.data(), s.data(), n);
        break;
    }
    case 2:
        for (size_t i = 0; i < n;) {
            const uint8_t c = static_cast<uint8_t>(rng() % 4);
            for (size_t r = 1 + rng() % 600; r && i < n; --r) v[i++] = c;
        }
        break;
    case 3:
        break;
    default:
        for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>((i % 12 < 4) ? rng() % 8 : (i * 7) & 0xff);
    }
    return v;
}

static void Lzss()
{
    std::fprintf(out, "== lzss\n");
    std::mt19937 rng(12345);
    static const size_t sizes[] = {0, 1, 6, 7, 100, 255, 256, 257, 1000, 4095, 4096, 16384, 40000, 65536, 70000, 200000};
    unsigned long checks = 0, bad = 0;
    for (int kind = 0; kind < 5; ++kind)
        for (size_t n : sizes) {
            const std::vector<uint8_t> in = MakeInput(rng, kind, n);
            std::fprintf(out, "kind %d size %zu:", kind, n);
            for (uint32_t level = 0; level <= 9; ++level) {
                const std::vector<uint8_t> c = JetLzss::Compress(in.data(), static_cast<uint32_t>(in.size()), level);
                std::vector<uint8_t> back;
                const bool ok = JetLzss::Decompress(c.data(), static_cast<uint32_t>(c.size()), back) && back == in;
                ++checks;
                if (!ok) ++bad;
                std::fprintf(out, " %zu/%08x%s", c.size(), Fnv(c.data(), c.size()), ok ? "" : "!");
            }
            std::fprintf(out, "\n");
        }
    // corrupted streams: only whether decoding fails and how much it produced
    std::mt19937 noise(777);
    uint32_t h = 2166136261u;
    for (int i = 0; i < 300; ++i) {
        const std::vector<uint8_t> in = MakeInput(rng, i % 5, 500 + i * 37);
        std::vector<uint8_t> c = JetLzss::Compress(in.data(), static_cast<uint32_t>(in.size()), i % 10);
        if (c.size() > 2) c[1 + noise() % (c.size() - 1)] ^= static_cast<uint8_t>(1 + noise() % 255);
        std::vector<uint8_t> back;
        const bool ok = JetLzss::Decompress(c.data(), static_cast<uint32_t>(c.size()), back);
        const uint32_t sz = static_cast<uint32_t>(back.size());
        h = Fnv(&ok, 1, h);
        h = Fnv(&sz, 4, h);
    }
    std::fprintf(out, "corrupted: %08x\nlzss round trips %lu, failed %lu\n", h, checks, bad);
}

// ---------------------------------------------------------------------------------------------------------------

static void Blowfish()
{
    std::fprintf(out, "== blowfish\n");
    std::mt19937 rng(54321);
    static const uint32_t keySizes[] = {1, 2, 7, 8, 16, 55, 56, 57, 100, 255, 256};
    static const uint32_t bufSizes[] = {8, 9, 15, 16, 17, 64, 1000, 4097};
    for (uint32_t ks : keySizes) {
        std::vector<uint8_t> key(ks);
        for (auto& b : key) b = static_cast<uint8_t>(rng());
        auto* st = new AuranBlowfish::State;
        const int code = AuranBlowfish::SetKey(*st, key.data(), ks);
        std::fprintf(out, "key %u -> %d P %08x S %08x:", ks, code, Fnv(st->P, sizeof st->P), Fnv(st->S, sizeof st->S));
        for (uint32_t bs : bufSizes) {
            std::vector<uint8_t> plain(bs);
            for (auto& b : plain) b = static_cast<uint8_t>(rng());
            std::vector<uint8_t> data = plain;
            const int e = AuranBlowfish::Encrypt(*st, data.data(), bs);
            const uint32_t ch = Fnv(data.data(), data.size());
            const int d = AuranBlowfish::Decrypt(*st, data.data(), bs);
            std::fprintf(out, " %u:%d%d/%08x%s", bs, e, d, ch, data == plain ? "" : "!");
        }
        std::fprintf(out, "\n");
        delete st;
    }
    auto* st = new AuranBlowfish::State;
    uint8_t k[300] = {}, buf[16] = {};
    const int key0 = AuranBlowfish::SetKey(*st, k, 0);
    const int key257 = AuranBlowfish::SetKey(*st, k, 257);
    AuranBlowfish::SetKey(*st, k, 8);
    const int buf7 = AuranBlowfish::Encrypt(*st, buf, 7);
    const int buf0 = AuranBlowfish::Decrypt(*st, buf, 0);
    std::fprintf(out, "codes: key0 %d key257 %d buf7 %d buf0 %d optimal 0:%u 1:%u 8:%u 9:%u\n", key0, key257, buf7, buf0,
                 AuranBlowfish::OptimalSize(0), AuranBlowfish::OptimalSize(1), AuranBlowfish::OptimalSize(8),
                 AuranBlowfish::OptimalSize(9));
    delete st;
}

// ---------------------------------------------------------------------------------------------------------------
// FastDynamicArray<const int*, 13>: items point into a pool and are printed as pool indices, so the output does
// not depend on addresses or on the chunk size in bytes.

static int pool[64];
using IntPtr = const int*;
using Fda = FastDynamicArray<IntPtr, 13>;

static long Idx(IntPtr p) { return p ? static_cast<long>(p - pool) : -1; }

static uint32_t Shape(const Fda& a)
{
    uint32_t h = 2166136261u;
    if (auto* c = a.head) {
        do {
            h = Fnv(&c->count, 4, h);
            for (uint32_t i = 0; i < c->count; ++i) {
                const long v = Idx(c->data[i]);
                h = Fnv(&v, sizeof v, h);
            }
            c = c->next;
        } while (c != a.head);
    }
    return h;
}

static long Pos(const Fda& a, IntPtr* node)
{
    if (!node) return -1;
    long n = 0;
    if (auto* c = a.head) {
        do {
            for (uint32_t i = 0; i < c->count; ++i, ++n)
                if (&c->data[i] == node) return n;
            c = c->next;
        } while (c != a.head);
    }
    return -2;
}

static void FastArrays()
{
    std::fprintf(out, "== fastdynamicarray\n");
    std::mt19937 rng(2024);
    unsigned long steps = 0;
    for (int run = 0; run < 200; ++run) {
        Fda a;
        uint32_t h = 2166136261u;
        const int ops = 50 + static_cast<int>(rng() % 400);
        for (int k = 0; k < ops; ++k) {
            IntPtr v = &pool[rng() % (run % 2 ? 8 : 64)];  // many duplicates on odd runs
            const unsigned long count = a.Count();
            long r = 0;
            switch (rng() % 16) {
            case 0: a.Add(v); break;
            case 1: a.AddPack(v); break;
            case 2: a.AddUnique(v); break;
            case 3: a.Push(v); break;
            case 4: a.PushUnique(v); break;
            case 5: r = Pos(a, a.Insert(v).node); break;
            case 6: r = Pos(a, a.InsertUnique(v).node); break;
            case 7: if (count) r = Pos(a, a.InsertAfter(a.Index(rng() % count), v).node); break;
            case 8: if (count) r = Pos(a, a.InsertBefore(a.Index(rng() % count), v).node); break;
            case 9: if (count) r = Idx(a.Pop()); break;
            case 10: if (count) r = Idx(a.Remove()); break;
            case 11: if (count) r = Pos(a, a.Remove(a.Index(rng() % count)).node); break;
            case 12: if (count) r = Pos(a, a.RemovePack(a.Index(rng() % count)).node); break;
            case 13: if (count) a.RemoveLast(1 + rng() % (count < 20 ? count : 20)); break;
            case 14: {
                Fda::Iterator f = a.Find(v);
                r = static_cast<long>(a.Index(f)) * 1000 + a.Exists(f);
                break;
            }
            default:
                if (rng() % 8 == 0)
                    a.RemoveAll();
                else
                    a.Sort();
            }
            h = Fnv(&r, sizeof r, h);
            const uint32_t s = Shape(a);
            h = Fnv(&s, 4, h);
            ++steps;
        }
        std::fprintf(out, "run %d ops %d count %lu hash %08x\n", run, ops, a.Count(), h);
    }
    Fda empty;
    std::fprintf(out, "empty: index(end) %lu steps %lu\n", empty.Index(empty.End()), steps);
}

// ---------------------------------------------------------------------------------------------------------------

static void Ids()
{
    std::fprintf(out, "== uid/version\n");
    std::mt19937 rng(99);
    uint32_t h = 2166136261u;
    for (int i = 0; i < 2000; ++i) {
        unsigned char d4[8];
        for (auto& b : d4) b = static_cast<unsigned char>(rng() % 3 ? rng() : 0);
        const UId a(rng() % 4 ? rng() : 0, static_cast<unsigned short>(rng() % 3), static_cast<unsigned short>(rng()), d4);
        UId b = a;
        if (rng() % 2) b.data4[rng() % 8] ^= 1;
        const PString s = a.GetString();
        const UId parsed(s.Text());
        const unsigned flags = static_cast<unsigned>(a == b) | (a != b) << 1 | (a < b) << 2 | (a > b) << 3 | (a <= b) << 4 | (a >= b) << 5 |
                               static_cast<bool>(a) << 6 | (!a) << 7 | (parsed == a) << 8;
        const unsigned hash = a.GetHashID();
        h = Fnv(s.Text(), s.Length(), h);
        h = Fnv(&flags, 4, h);
        h = Fnv(&hash, 4, h);
        if (i < 3) std::fprintf(out, "uid %s flags %03x hash %08x\n", s.Text(), flags, hash);
    }
    std::fprintf(out, "uid hash %08x null %d\n", h, static_cast<bool>(UIdNull));
    h = 2166136261u;
    for (int i = 0; i < 2000; ++i) {
        const Version a(static_cast<unsigned char>(rng() % 3), static_cast<unsigned char>(rng() % 3),
                        static_cast<unsigned short>(rng() % 3));
        const Version b(static_cast<unsigned long>(rng() % 2 ? a.Packed() : rng()));
        const unsigned flags = static_cast<unsigned>(a == b) | (a != b) << 1 | (a < b) << 2 | (a > b) << 3 | (a <= b) << 4 | (a >= b) << 5;
        const unsigned long pa = a.Packed();
        h = Fnv(&flags, 4, h);
        h = Fnv(&pa, 4, h);
    }
    std::fprintf(out, "version hash %08x\n", h);
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    out = std::fopen(argv[1], "w");
    Lzss();
    Blowfish();
    FastArrays();
    Ids();
    std::fprintf(out, "== end\n");
    std::fclose(out);
    if (argc > 2) {
        FILE* s = std::fopen(argv[2], "w");
        std::fprintf(s, "pointer %zu\nPString %zu\nPStringCache::Node text at %zu\nUId %zu\nVersion %zu\n",
                     sizeof(void*), sizeof(PString), offsetof(PStringCache::Node, text), sizeof(UId), sizeof(Version));
        std::fprintf(s, "FastDynamicArray<T*,13>::Chunk %zu (data at %zu)\nTask %zu\nTaskManager %zu\nMutex %zu\n",
                     sizeof(Fda::Chunk), offsetof(Fda::Chunk, data), sizeof(Task), sizeof(TaskManager), sizeof(Mutex));
        std::fclose(s);
    }
    return 0;
}
