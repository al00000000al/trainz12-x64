// Jet::LZSSCompress decompression, reconstructed from Kernel_7c.dll: DecompressBlock 0x10007150, model init
// 0x10005cf0/0x10005e00, model update 0x100060b0, rebuild 0x10005e60, symbol decode 0x10006230, raw bits
// 0x10005990. Copies follow IOSlaveWriteCursor::WriteCopy (KernelStandard 0x1002b8d0).
#include "JetLzss.h"

#include <cstring>

namespace Jet::JetLzss {
namespace {

// window mode -> {distance bits, symbols in the distance tree, raw low distance bits} (0x1001814c)
struct ModeInfo {
    uint8_t distBits, distSymbols, lowBits;
};
constexpr ModeInfo kModes[3] = {{8, 0x10, 4}, {12, 0x40, 6}, {14, 0x40, 8}};

// length code -> {base, extra bits} (0x10018458)
struct LenCode {
    uint8_t base, extra;
};
constexpr LenCode kLenCodes[16] = {{0, 0},  {1, 0},  {2, 0},  {3, 0},  {4, 0},  {5, 1},  {7, 1},  {9, 2},
                                   {13, 2}, {17, 3}, {25, 3}, {33, 4}, {49, 4}, {65, 5}, {97, 5}, {129, 7}};

constexpr uint32_t kFirstLength = 0x100;
constexpr uint32_t kEnd = 0x110;
constexpr uint32_t kLeaf = 0x8000;
constexpr uint32_t kUnset = 0xfffffffe;  // -2 while rebuilding

// Adaptive Huffman tree. Node arrays are kept as 32-bit values like the original so its arithmetic (and the
// odd corners of its rebuild) carries over unchanged.
struct Model {
    uint32_t leaves = 0, nodes = 0, root = 0;
    std::vector<uint32_t> freq, parent, son, leafPos;

    void Init(uint32_t symbols)
    {
        leaves = (symbols + 1) & 0xffe;
        nodes = leaves * 2 - 1;
        root = nodes - 1;
        freq.assign(nodes, 0);
        parent.assign(nodes, 0);
        son.assign(nodes, 0);
        leafPos.assign(leaves, 0);
        uint32_t i = 0;
        for (; i < leaves; ++i) {
            freq[i] = 1;
            son[i] = i | kLeaf;
        }
        for (uint32_t k = 0; i < nodes; ++i, k += 2) {
            freq[i] = freq[k] + freq[k + 1];
            son[i] = k;
            parent[k] = parent[k + 1] = i;
        }
        parent[root] = 0xffffffff;
        for (i = 0; i < leaves; ++i) leafPos[i] = i;
    }

    // 0x10005e60: halve leaf counts and rebuild the internal nodes in place.
    void Rebuild()
    {
        const uint32_t top = root;
        int32_t leavesLeft = static_cast<int32_t>(leaves);
        for (uint32_t i = 0; i < top; ++i) {
            if (son[i] & kLeaf) freq[i] = (freq[i] + 1) >> 1;
            parent[i] = kUnset;
        }
        parent[top] = kUnset;

        uint32_t scan = 0, slot = 0, a = 0, b = 0;
        for (;;) {
            for (; scan < top; ++scan) {
                if (parent[scan] == kUnset) {
                    a = scan;
                    b = scan + 1;
                    break;
                }
            }
            if (scan == top) {
                parent[top] = 0xffffffff;
                return;
            }
            scan += 2;
            for (; slot <= top; ++slot) {
                if (!(son[slot] & kLeaf)) {
                    if (parent[slot] == kUnset) break;
                } else {
                    --leavesLeft;
                }
            }
            freq[slot] = freq[a] + freq[b];
            const uint32_t f = freq[slot];
            uint32_t next = slot + 1, nextAfterMove = slot + 1, probe = slot + 1, pos = slot;
            if (f < freq[slot - 1]) {
                // shift the heavier nodes below up by one, fixing the links that point at them
                int32_t i = static_cast<int32_t>(slot) - 1;
                uint32_t t = freq[slot - 1];
                while (f < t) {
                    const uint32_t s = son[i];
                    if (!(s & kLeaf)) {
                        ++parent[s];
                        ++parent[son[i] + 1];
                    } else {
                        ++leafPos[s & 0x7fff];
                    }
                    --i;
                    t = freq[i];
                }
                pos = static_cast<uint32_t>(i + 1);
                const size_t bytes = (slot - pos) * sizeof(uint32_t);
                std::memmove(&freq[pos + 1], &freq[pos], bytes);
                std::memmove(&son[pos + 1], &son[pos], bytes);
            } else {
                // pull lighter leaves that follow down into place
                for (;;) {
                    next = nextAfterMove;
                    if (leavesLeft == 0) break;
                    uint32_t s = son[probe];
                    while (!(s & kLeaf) && probe < top) s = son[++probe];
                    if (probe == top || f <= freq[probe]) break;
                    freq[pos] = freq[probe];
                    son[pos] = son[probe];
                    parent[pos] = kUnset;
                    leafPos[son[probe] & 0x7fff] = pos;
                    son[probe] = 0;
                    nextAfterMove = next + 1;
                    pos = next;
                    --leavesLeft;
                }
            }
            freq[pos] = f;
            son[pos] = a;
            parent[a] = pos;
            parent[b] = pos;
            slot = next;
        }
    }

    // 0x100060b0
    void Update(uint32_t sym)
    {
        if (freq[root] > 0x7fff) Rebuild();
        uint32_t c = leafPos[sym];
        uint32_t l;
        do {
            const uint32_t k = ++freq[c];
            l = c;
            if (freq[c + 1] < k) {
                l = c + 2;
                while (freq[l] < k) ++l;
                --l;
                const uint32_t i = son[l];
                if (!(i & kLeaf))
                    parent[i] = parent[i + 1] = c;
                else
                    leafPos[i & 0x7fff] = c;
                const uint32_t j = son[c];
                if (!(j & kLeaf))
                    parent[j] = parent[j + 1] = l;
                else
                    leafPos[sym] = l;
                freq[c] = freq[l];
                freq[l] = k;
                son[c] = i;
                son[l] = j;
            }
            c = parent[l];
        } while (c != root);
        ++freq[root];
    }
};

struct Reader {
    const uint8_t* src;
    uint32_t size;
    uint32_t pos;
    int32_t remaining;  // mirrors the original's signed byte budget (this+0xb8)
    uint32_t bitPos = 0;

    uint8_t Byte(uint32_t at) const { return at < size ? src[at] : 0; }
    uint8_t TakeByte()
    {
        --remaining;
        return Byte(pos++);
    }
    uint32_t Peek32() const
    {
        return Byte(pos) | (Byte(pos + 1) << 8) | (Byte(pos + 2) << 16) | (uint32_t(Byte(pos + 3)) << 24);
    }
    uint32_t TakeFlags()
    {
        const uint32_t v = Peek32();
        pos += 4;
        remaining -= 4;
        return v;
    }
    void Advance(uint32_t bits)
    {
        bitPos += bits;
        pos += bitPos >> 3;
        remaining -= static_cast<int32_t>(bitPos >> 3);
        bitPos &= 7;
    }
    // 0x10005990
    uint32_t Bits(uint32_t n)
    {
        const uint32_t word = Byte(pos) | (Byte(pos + 1) << 8);
        const uint32_t shift = bitPos;
        Advance(n);
        return (word >> shift) & ((1u << n) - 1);
    }
    // 0x10006230
    uint32_t Symbol(Model& m)
    {
        uint32_t c = m.son[m.root];
        while (!(c & kLeaf)) {
            const uint32_t bit = (Byte(pos) >> bitPos) & 1;
            Advance(1);
            c = m.son[bit + c];
        }
        c &= 0x7fff;
        m.Update(c);
        return c;
    }
};

// level -> {window mode, "good enough" match length} (0x100194c0, stride 0x28)
struct LevelInfo {
    uint32_t mode, nice;
};
constexpr LevelInfo kLevels[10] = {{0, 0},  {1, 8},   {1, 16}, {1, 32},  {2, 64},
                                   {2, 128}, {2, 256}, {2, 64}, {2, 128}, {2, 256}};

// Bit output shared by the Huffman encoders (0x10005940, 0x100061b0): LSB first, flushed 16 bits at a time,
// with x86 shift semantics (counts taken mod 32) so oversized codes behave exactly like the original.
struct BitWriter {
    std::vector<uint8_t>& out;
    uint32_t buf = 0, count = 0;

    void Put(uint32_t value, uint32_t bits)
    {
        buf |= value << (count & 31);
        count += bits;
        if (count >= 16) {
            out.push_back(static_cast<uint8_t>(buf));
            out.push_back(static_cast<uint8_t>(buf >> 8));
            count -= 16;
            buf >>= 16;
        }
    }
    void Symbol(const Model& m, uint32_t sym)
    {
        uint32_t node = m.leafPos[sym], code = 0, depth = 0;
        do {
            code = code * 2 | (node & 1);
            node = m.parent[node];
            ++depth;
        } while (node != m.root);
        Put(code, depth);
    }
};

class Compressor {
public:
    Compressor(const uint8_t* src, uint32_t size, uint32_t level, uint32_t mode, std::vector<uint8_t>& out)
        : src_(src), size_(size), level_(level), mode_(mode), out_(out), bits_{out}
    {
        // 0x10006c80
        remaining_ = static_cast<int32_t>(size);
        distBits_ = kModes[mode].distBits;
        nice_ = kLevels[level].nice;
        huffman_ = level > 6;
        window_ = 1u << distBits_;
        limit_ = window_ * 2 + 0x102;
        if (static_cast<uint32_t>(remaining_) < limit_) limit_ = static_cast<uint32_t>(remaining_);
        if (mode == 0) {
            runThreshold_ = 0x103;
            lenMask_ = 7;
        } else {
            runThreshold_ = 6;
            lenMask_ = (1u << ((16 - distBits_) & 31)) - 1;
        }
        shift_ = limit_ < 2000 ? 3 : (limit_ < 24000 ? 4 : 5);
        mask_ = 0xffffu >> ((16 - 3 * shift_) & 31);
        head_.assign(mask_ + 1, 0xffff);
        prev_.assign(limit_, 0xffff);
        if (huffman_) {
            // 0x10005e00
            lit_.Init(0x111);
            dist_.Init(kModes[mode].distSymbols);
            lowBits_ = kModes[mode].lowBits;
            lowMask_ = (1u << lowBits_) - 1;
        }
    }

    void Run()
    {
        MainLoop();
        if (!huffman_) {
            if (bit_ != 1) FlushGroup();
        } else {
            bits_.Symbol(lit_, kEnd);  // no model update after the end symbol
            if (bits_.count != 0) bits_.Put(0, 16 - bits_.count);
        }
    }

private:
    uint8_t Cur(uint32_t k) const { return cur_ + k < size_ ? src_[cur_ + k] : 0; }
    uint8_t Win(uint32_t k) const { return base_ + k < size_ ? src_[base_ + k] : 0; }
    void Rehash(uint8_t b) { hash_ = ((hash_ << (shift_ & 31)) ^ b) & mask_; }

    // ---- token format (levels 1-6, and mode 0): groups of a 32-bit flag word followed by their data
    void FlushGroup()
    {
        for (int i = 0; i < 4; ++i) out_.push_back(static_cast<uint8_t>(flags_ >> (8 * i)));
        out_.insert(out_.end(), group_.begin(), group_.end());
        group_.clear();
        flags_ = 0;
        bit_ = 1;
    }
    void NextBit()
    {
        bit_ <<= 1;
        if (bit_ == 0) FlushGroup();
    }
    // 0x10005890
    void TokenLiteral(uint8_t b)
    {
        if (mode_ != 0) NextBit();
        group_.push_back(b);
        NextBit();
    }
    // 0x10006410
    void TokenMatch(uint32_t len)
    {
        flags_ |= bit_;
        const uint32_t l = len - 3;
        const uint32_t dist = pos_ - matchPos_;
        if (mode_ == 0) {
            NextBit();
            if (static_cast<int32_t>(l) < 7) {
                if (l & 1) flags_ |= bit_;
                NextBit();
                if (l & 2) flags_ |= bit_;
                NextBit();
                if (l & 4) flags_ |= bit_;
            } else {
                flags_ |= bit_;
                NextBit();
                flags_ |= bit_;
                NextBit();
                flags_ |= bit_;
                group_.push_back(static_cast<uint8_t>(l));
            }
            group_.push_back(static_cast<uint8_t>(dist));
        } else if (static_cast<int32_t>(l) < static_cast<int32_t>(lenMask_)) {
            const uint16_t token = static_cast<uint16_t>(dist | (l << distBits_));
            group_.push_back(static_cast<uint8_t>(token));
            group_.push_back(static_cast<uint8_t>(token >> 8));
        } else {
            const uint16_t token = static_cast<uint16_t>(dist | (lenMask_ << distBits_));
            group_.push_back(static_cast<uint8_t>(token));
            group_.push_back(static_cast<uint8_t>(token >> 8));
            group_.push_back(static_cast<uint8_t>(l));
        }
        NextBit();
    }
    // 0x10006600
    void TokenLiterals(uint32_t n)
    {
        if (static_cast<int32_t>(n) < static_cast<int32_t>(runThreshold_)) {
            for (int32_t i = static_cast<int32_t>(n) - 1; i >= 0; --i) TokenLiteral(Win(litStart_++));
            return;
        }
        NextBit();
        flags_ |= bit_;
        group_.push_back(static_cast<uint8_t>(n - 3));
        for (uint32_t i = 0; i < n; ++i) group_.push_back(Win(litStart_ + i));
        litStart_ += n;
        NextBit();
    }

    // ---- Huffman format (levels 7-9)
    // 0x100062c0
    void HuffMatch(uint32_t len)
    {
        const uint32_t l = len - 3;  // length table 0x10018158: {code, extra value, extra bits}
        uint32_t code = 0;
        while (code < 15 && kLenCodes[code + 1].base <= l) ++code;
        bits_.Symbol(lit_, kFirstLength + code);
        lit_.Update(kFirstLength + code);
        if (kLenCodes[code].extra) bits_.Put(l - kLenCodes[code].base, kLenCodes[code].extra);
        const uint32_t dist = pos_ - matchPos_;
        bits_.Symbol(dist_, dist >> (lowBits_ & 31));
        dist_.Update(dist >> (lowBits_ & 31));
        bits_.Put(dist & lowMask_, lowBits_);
    }
    // 0x10006390
    void HuffLiterals(uint32_t n)
    {
        for (int32_t i = static_cast<int32_t>(n) - 1; i >= 0; --i) {
            const uint8_t b = Win(litStart_);
            bits_.Symbol(lit_, b);
            lit_.Update(b);
            ++litStart_;
        }
    }

    void EmitMatch(uint32_t len) { huffman_ ? HuffMatch(len) : TokenMatch(len); }
    void EmitLiterals(uint32_t n) { huffman_ ? HuffLiterals(n) : TokenLiterals(n); }

    // 0x10006700: move the window forward by pos - window
    void Slide()
    {
        const uint32_t d = pos_ - window_;
        base_ += d;
        pos_ -= d;
        litStart_ -= d;
        auto rebase = [d](uint16_t& v) {
            const int32_t s = static_cast<int16_t>(v);
            v = (s < static_cast<int32_t>(d)) ? 0xffff : static_cast<uint16_t>(v - static_cast<uint16_t>(d));
        };
        for (auto& h : head_) rebase(h);
        std::memmove(prev_.data(), prev_.data() + d, (limit_ - d) * sizeof(uint16_t));
        for (uint32_t i = 0; i < limit_ - d; ++i) rebase(prev_[i]);
        for (uint32_t i = limit_ - d; i < limit_; ++i) prev_[i] = 0xffff;
    }

    // 0x100067d0
    bool FindMatch()
    {
        int32_t lowest = static_cast<int32_t>(pos_ - window_);
        if (lowest < 0) lowest = -1;
        int32_t maxLen = static_cast<int32_t>(limit_ - pos_);
        if (remaining_ < maxLen) maxLen = remaining_;
        int32_t goal = static_cast<int32_t>(nice_);
        if (maxLen <= goal) goal = maxLen;
        bestLen_ = 2;
        for (;;) {
            const uint32_t at = base_ + chainPos_;
            if (at + bestLen_ + 1 <= size_) {
                const uint8_t* cand = src_ + at;
                if (cand[0] == Cur(0) && cand[bestLen_] == Cur(bestLen_) && cand[1] == Cur(1)) {
                    uint32_t len = 3;
                    for (int32_t k = maxLen - 4; k >= 0 && Win(chainPos_ + len) == Cur(len); --k) ++len;
                    if (static_cast<int32_t>(bestLen_) < static_cast<int32_t>(len)) {
                        matchPos_ = chainPos_;
                        bestLen_ = len;
                        if (goal <= static_cast<int32_t>(len)) return true;
                    }
                }
            }
            const uint16_t next = prev_[chainPos_];
            chainPos_ = next;
            if (static_cast<int16_t>(next) <= lowest) return 2 < static_cast<int32_t>(bestLen_);
        }
    }

    // 0x10006940
    void MainLoop()
    {
        if (remaining_ > 2) {
            hash_ = Cur(0);
            Rehash(Cur(1));
        }
        while (remaining_ > 0) {
            if (remaining_ > 2) {
                uint32_t pending = pos_ - litStart_;
                if (0x101 < pending) {
                    EmitLiterals(pending);
                    litStart_ = pos_;
                    pending = 0;
                }
                if (window_ * 2 <= pos_) Slide();
                Rehash(Cur(2));
                const uint16_t cand = head_[hash_];
                chainPos_ = cand;
                prev_[pos_] = cand;
                head_[hash_] = static_cast<uint16_t>(pos_);
                if (chainPos_ != 0xffff && static_cast<int32_t>(pos_ - window_) < static_cast<int32_t>(chainPos_) &&
                    FindMatch()) {
                    if (pending) EmitLiterals(pending);
                    uint32_t len = bestLen_;
                    if (static_cast<int32_t>(len) < 0x103) {
                        EmitMatch(len);
                    } else {
                        do {
                            EmitMatch(0x102);
                            len -= 0x102;
                        } while (0x102 < len);
                        if (2 < len)
                            EmitMatch(len);
                        else
                            bestLen_ -= len;  // a 1-2 byte remainder goes back to the input
                    }
                    for (int32_t left = static_cast<int32_t>(--bestLen_); left > 0; --left) {
                        ++cur_;
                        ++pos_;
                        --remaining_;
                        if (remaining_ > 2) {
                            Rehash(Cur(2));
                            prev_[pos_] = head_[hash_];
                            head_[hash_] = static_cast<uint16_t>(pos_);
                        }
                    }
                    litStart_ = pos_ + 1;
                }
            }
            ++cur_;
            ++pos_;
            --remaining_;
        }
        const uint32_t tail = pos_ - litStart_;
        if (tail) EmitLiterals(tail);
    }

    const uint8_t* src_;
    uint32_t size_, level_, mode_;
    std::vector<uint8_t>& out_;
    BitWriter bits_;
    bool huffman_ = false;
    int32_t remaining_ = 0;
    uint32_t distBits_ = 0, nice_ = 0, window_ = 0, limit_ = 0, runThreshold_ = 0, lenMask_ = 0;
    uint32_t shift_ = 0, mask_ = 0, hash_ = 0;
    uint32_t cur_ = 0, base_ = 0, pos_ = 0, litStart_ = 0, chainPos_ = 0, matchPos_ = 0, bestLen_ = 0;
    std::vector<uint16_t> head_, prev_;
    uint32_t flags_ = 0, bit_ = 1;
    std::vector<uint8_t> group_;
    Model lit_, dist_;
    uint32_t lowBits_ = 0, lowMask_ = 0;
};

// IOSlaveWriteCursor::WriteCopy(-distance, length)
bool Copy(std::vector<uint8_t>& out, uint32_t distance, uint32_t length)
{
    if (distance == 0 || distance > out.size()) return length == 0;
    size_t from = out.size() - distance;
    for (uint32_t i = 0; i < length; ++i) out.push_back(out[from + i]);
    return true;
}

}  // namespace

// 0x10006ec0 (CompressBlock)
std::vector<uint8_t> Compress(const uint8_t* src, uint32_t srcSize, uint32_t level)
{
    std::vector<uint8_t> out;
    if (level > 9) level = 9;
    if (level > 0 && srcSize > 6) {
        uint32_t mode = kLevels[level].mode;
        if (srcSize < 0x100) {
            mode = 0;
            if (level > 6) level = 6;
        }
        // one step down when the input fits the previous mode's window (table 0x10018149 + 3 * mode)
        if (mode != 0 && srcSize <= (1u << kModes[mode - 1].distBits)) --mode;
        out.push_back(static_cast<uint8_t>(mode << 4 | level));
        Compressor c(src, srcSize, level, mode, out);
        c.Run();
        return out;
    }
    out.push_back(0);
    out.insert(out.end(), src, src + srcSize);
    return out;
}

bool Decompress(const uint8_t* src, uint32_t srcSize, std::vector<uint8_t>& out)
{
    if (srcSize == 0) return false;
    Reader in{src, srcSize, 1, static_cast<int32_t>(srcSize - 1)};
    const uint8_t header = src[0];
    const uint32_t mode = (header >> 4) & 3;
    const uint32_t level = header & 0xf;

    if (level == 0) {
        out.insert(out.end(), src + 1, src + srcSize);
        return srcSize - 1 != 0;
    }
    if (mode > 2) return false;  // the original would index past its 3-entry table

    if (mode == 0) {
        while (in.remaining > 0) {
            uint32_t flags = in.TakeFlags();
            uint32_t bit = 1;
            if (in.remaining <= 0) continue;
            while (bit != 0) {
                if (!(flags & bit)) {
                    out.push_back(in.TakeByte());
                } else {
                    auto next = [&] {
                        bit <<= 1;
                        if (bit == 0) {
                            flags = in.TakeFlags();
                            bit = 1;
                        }
                    };
                    next();
                    uint32_t len = (flags & bit) ? 1 : 0;
                    next();
                    if (flags & bit) len |= 2;
                    next();
                    if (flags & bit) len |= 4;
                    if (len == 7) len = in.TakeByte();
                    const uint32_t dist = in.TakeByte();
                    if (!Copy(out, dist, len + 3)) return false;
                }
                bit <<= 1;
                if (in.remaining < 1) return true;
            }
        }
        return true;
    }

    const uint32_t distBits = kModes[mode].distBits;
    const uint32_t distMask = (1u << distBits) - 1;

    if (level < 7) {
        const uint32_t lenMask = (1u << (16 - distBits)) - 1;
        while (in.remaining > 0) {
            uint32_t flags = in.TakeFlags();
            uint32_t bit = 1;
            if (in.remaining <= 0) continue;
            while (bit != 0) {
                if (!(flags & bit)) {
                    bit <<= 1;
                    if (bit == 0) {
                        flags = in.TakeFlags();
                        bit = 1;
                    }
                    if (!(bit & flags)) {
                        out.push_back(in.TakeByte());
                    } else {
                        const uint32_t n = in.Byte(in.pos) + 3u;
                        ++in.pos;
                        in.remaining += -1 - static_cast<int32_t>(n);
                        const uint32_t avail = in.pos < srcSize ? srcSize - in.pos : 0;
                        const uint32_t take = n < avail ? n : avail;
                        out.insert(out.end(), src + in.pos, src + in.pos + take);
                        in.pos += take;
                        if (take != n) return false;
                    }
                } else {
                    const uint32_t token = in.Peek32();
                    in.pos += 2;
                    in.remaining -= 2;
                    uint32_t len = (token >> distBits) & lenMask;
                    if (len == lenMask) len = in.TakeByte();
                    if (!Copy(out, token & distMask, len + 3)) return false;
                }
                bit <<= 1;
                if (in.remaining < 1) return true;
            }
        }
        return true;
    }

    Model lit, dist;
    lit.Init(0x111);
    dist.Init(kModes[mode].distSymbols);
    const uint32_t lowBits = kModes[mode].lowBits;
    while (in.remaining >= 0) {
        const uint32_t sym = in.Symbol(lit);
        if (sym == kEnd) break;
        if (sym < kFirstLength) {
            out.push_back(static_cast<uint8_t>(sym));
            continue;
        }
        const LenCode& lc = kLenCodes[(sym - kFirstLength) & 15];
        const uint32_t len = lc.base + 3 + in.Bits(lc.extra);
        const uint32_t hi = in.Symbol(dist);
        const uint32_t lo = in.Bits(lowBits);
        if (!Copy(out, ((hi << lowBits) + lo) & distMask, len)) return false;
    }
    return true;
}

}  // namespace Jet::JetLzss
