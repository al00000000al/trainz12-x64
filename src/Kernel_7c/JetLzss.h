// The codec behind Jet::LZSSCompress (Kernel_7c), reconstructed from the TS12 binary.
//
// Stream = 1 header byte + payload. Header: low nibble = level 0..9 (0 = stored), bits 4-5 = window mode.
//  mode 0 (any level):  32-bit LE flag words; flag 0 = literal byte, 1 = match with a 3-bit length
//                       (7 -> next byte) and a 1-byte distance; length + 3.
//  modes 1-2, level 1-6: flag 0 then next flag: 0 = literal, 1 = raw run (byte + 3 bytes copied as is);
//                       flag 1 = 16-bit LE token, distance in the low 12/14 bits, length above (all ones ->
//                       next byte); length + 3.
//  modes 1-2, level 7-9: two adaptive Huffman trees, bits LSB first: symbols 0-255 literals, 256-271 length
//                       codes (base + extra bits, + 3), 272 end; then distance = (tree2 symbol << lowBits) +
//                       raw low bits, masked to the window.
// A match whose distance is 0 or reaches before the start of the output makes decompression fail.
#pragma once

#include <cstdint>
#include <vector>

namespace Jet::JetLzss {

// Same success rules as LZSSCompress::DecompressBlock. On failure `out` holds what was produced so far.
bool Decompress(const uint8_t* src, uint32_t srcSize, std::vector<uint8_t>& out);

// Same bytes as LZSSCompress::CompressBlock (levels above 9 act as 9; level 0 or inputs up to 6 bytes are
// stored).
std::vector<uint8_t> Compress(const uint8_t* src, uint32_t srcSize, uint32_t level);

}  // namespace Jet::JetLzss
