// Jet::BFEncrypt's cipher, reconstructed from Kernel_7c.dll: block encrypt 0x10001dd0 (EAX=&L, ECX=&R,
// ESI=state), decrypt 0x10001f60, SetKey 0x10002230, EncryptData 0x100024d0, DecryptData 0x10002550.
#include "AuranBlowfish.h"

#include "BlowfishPi.h"

#include <cstring>

namespace Jet::AuranBlowfish {

static inline uint32_t F(const State& st, uint32_t x)
{
    uint32_t t = ((st.S[1][(x >> 8) & 0xff] & 1) + st.S[0][x & 0xff]) ^ 0x20;
    t += st.S[3][x >> 24] & 1;
    t += st.S[2][(x >> 16) & 0xff];
    return t;
}

void EncryptBlock(const State& st, uint32_t& l, uint32_t& r)
{
    for (int i = 0; i < 16; ++i) {
        l ^= st.P[i];
        r ^= F(st, l) ^ 0x20;
        const uint32_t t = l;
        l = r;
        r = t;
    }
    const uint32_t t = l;
    l = r;
    r = t;
    r ^= st.P[16];
    l ^= st.P[17];
}

void DecryptBlock(const State& st, uint32_t& l, uint32_t& r)
{
    l ^= st.P[17];
    r ^= st.P[16];
    uint32_t t = l;
    l = r;
    r = t;
    for (int i = 15; i >= 0; --i) {
        t = l;
        l = r;
        r = t;
        r ^= F(st, l) ^ 0x20;
        l ^= st.P[i];
    }
}

Code SetKey(State& st, const void* key, uint32_t size)
{
    if (!key) return INVALID_PARAMETERS;
    if (size == 0) return KEY_TOO_SHORT;
    if (size > 0x100) return KEY_TOO_LONG;

    // 56 bytes of key material: the key repeated (and cut) when shorter, its first 56 bytes when longer
    uint32_t words[14];
    unsigned char* w = reinterpret_cast<unsigned char*>(words);
    if (size < sizeof words) {
        uint32_t pos = 0;
        for (uint32_t n = sizeof words / size; n; --n, pos += size) std::memcpy(w + pos, key, size);
        if (sizeof words % size) std::memcpy(w + pos, key, sizeof words % size);
    } else {
        std::memcpy(words, key, sizeof words);
    }

    std::memcpy(st.P, BlowfishPi::P, sizeof st.P);
    std::memcpy(st.S, BlowfishPi::S, sizeof st.S);
    for (int i = 0; i < 18; ++i) st.P[i] ^= words[i % 14];

    uint32_t l = 0, r = 0;
    for (int i = 0; i < 18; i += 2) {
        EncryptBlock(st, l, r);
        st.P[i] = l;
        st.P[i + 1] = r;
    }
    // The original's S fill: its write pointer starts at S1[0] again on every one of the 256 outer passes,
    // so each pass only rewrites (S0[0], S1[0]) and then (S2[1], S3[1]).
    for (int pass = 0; pass < 256; ++pass) {
        EncryptBlock(st, l, r);
        st.S[0][0] = l;
        st.S[1][0] = r;
        EncryptBlock(st, l, r);
        st.S[2][1] = l;
        st.S[3][1] = r;
    }
    return OK;
}

static inline void Block(const State& st, unsigned char* p, bool encrypt)
{
    uint32_t l, r;
    std::memcpy(&l, p, 4);
    std::memcpy(&r, p + 4, 4);
    if (encrypt)
        EncryptBlock(st, l, r);
    else
        DecryptBlock(st, l, r);
    std::memcpy(p, &l, 4);
    std::memcpy(p + 4, &r, 4);
}

Code Encrypt(const State& st, void* data, uint32_t size)
{
    if (!data) return INVALID_PARAMETERS;
    if (size < 8) return BUFFER_TOO_SMALL;
    unsigned char* p = static_cast<unsigned char*>(data);
    for (uint32_t n = size >> 3; n; --n, p += 8) Block(st, p, true);
    if (size & 7) Block(st, p + (size & 7) - 8, true);
    return OK;
}

Code Decrypt(const State& st, void* data, uint32_t size)
{
    if (!data) return INVALID_PARAMETERS;
    if (size < 8) return BUFFER_TOO_SMALL;
    unsigned char* p = static_cast<unsigned char*>(data);
    if (size & 7) Block(st, p + size - 8, false);
    for (uint32_t n = size >> 3; n; --n, p += 8) Block(st, p, false);
    return OK;
}

}  // namespace Jet::AuranBlowfish
