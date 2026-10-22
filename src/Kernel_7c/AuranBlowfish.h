// The Blowfish variant used by Jet::BFEncrypt (Kernel_7c), reconstructed from the TS12 binary.
// It differs from standard Blowfish in ways that matter for reading existing game data:
//  - key bytes are repeated/truncated to 56 bytes and read as little-endian 32-bit words;
//  - blocks are two little-endian words, L = first, R = second;
//  - F(x) = ((((S1[b] & 1) + S0[a]) ^ 0x20) + (S3[d] & 1) + S2[c]) with a..d the bytes of x from low to high,
//    and the round does R ^= F(L) ^ 0x20;
//  - the S-box fill loop rewinds every outer iteration, so only S0[0], S1[0], S2[1], S3[1] are ever rewritten
//    (a bug in the original that the data on disk depends on);
//  - a buffer whose length is not a multiple of 8 gets its tail encrypted as one more block ending at the
//    buffer end, overlapping the previous block (decryption undoes that block first).
#pragma once

#include <cstdint>

namespace Jet::AuranBlowfish {

struct State {
    uint32_t P[18];
    uint32_t S[4][256];
};

// Result codes as returned by BFEncrypt (Encrypt::Results; 0 = success).
enum Code : int { OK = 0, INVALID_PARAMETERS = 2, KEY_TOO_SHORT = 3, KEY_TOO_LONG = 4, BUFFER_TOO_SMALL = 5 };

Code SetKey(State& st, const void* key, uint32_t size);
void EncryptBlock(const State& st, uint32_t& l, uint32_t& r);
void DecryptBlock(const State& st, uint32_t& l, uint32_t& r);
Code Encrypt(const State& st, void* data, uint32_t size);
Code Decrypt(const State& st, void* data, uint32_t size);
inline uint32_t OptimalSize(uint32_t size) { return (size & 7) ? (size & ~7u) + 8 : size; }

}  // namespace Jet::AuranBlowfish
