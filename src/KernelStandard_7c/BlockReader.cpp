// Unit U9b of KernelStandard_7c: BlockReader (0x1000c550..0x1000d26c, Init 0x10042de0) and BPE (0x1000d270..0x1000dac9).
// Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U9b").
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <cstring>
#include <new>

namespace Jet {

namespace {

const unsigned long kLine = 0x200;

// 0x1000d180: the PString made from a C string (none for null)
void AssignText(PString& p, const char* text)
{
    PString made;
    if (text)
        made.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(text),
                                             static_cast<unsigned>(std::strlen(text)));
    p = made;
}

bool Blank(char c) { return c == ' ' || c == '\t'; }

}  // namespace

// ---- BlockReader

BlockReader::BlockReader()
    : file(nullptr), line(nullptr), token(nullptr), cursor(nullptr), bufferSize(kLine), name(), parent(nullptr),
      child(nullptr), ended(false), pending(false)
{
    line = static_cast<char*>(Alloc::New(kLine));
    cursor = line;
    token = static_cast<char*>(Alloc::New(bufferSize));
}

BlockReader::BlockReader(File& f) : BlockReader() { file = &f; }

// a child block: the parent's file and buffers
BlockReader::BlockReader(BlockReader* p)
    : file(p->file), line(p->line), token(p->token), cursor(p->line), bufferSize(p->bufferSize), name(), parent(p),
      child(nullptr), ended(false), pending(false)
{
}

BlockReader::BlockReader(const BlockReader& o)
    : file(o.file), line(o.line), token(o.token), cursor(o.cursor), bufferSize(o.bufferSize), name(o.name),
      parent(o.parent), child(o.child), ended(o.ended), pending(o.pending)
{
}

BlockReader& BlockReader::operator=(const BlockReader& o)
{
    file = o.file;
    line = o.line;
    token = o.token;
    cursor = o.cursor;
    bufferSize = o.bufferSize;
    name = o.name;
    parent = o.parent;
    child = o.child;
    ended = o.ended;
    pending = o.pending;
    return *this;
}

BlockReader::~BlockReader()
{
    if (!parent) {
        if (line) Alloc::Delete(line);
        line = nullptr;
        if (token) Alloc::Delete(token);
        token = nullptr;
    }
    if (BlockReader* const c = child) {
        c->~BlockReader();
        Alloc::Delete(c);
    }
    child = nullptr;
}

void __cdecl BlockReader::Init(File& f) { file = &f; }

BlockReader* __cdecl BlockReader::Begin()
{
    if (child || ended || !IsBlockStart()) return nullptr;
    void* const mem = Alloc::New(sizeof(BlockReader));
    child = mem ? ::new (mem) BlockReader(this) : nullptr;
    return child;
}

void __cdecl BlockReader::End()
{
    if (!child) return;
    child->End();
    child->SkipBlock();
    if (BlockReader* const c = child) {
        c->~BlockReader();
        Alloc::Delete(c);
    }
    child = nullptr;
    pending = false;
}

// The next entry's name (its first token), skipping blank and comment lines and whole "{ ... }" blocks; a line
// with "}" ends this block, one with "{" leaves a block pending (skipped by the next call unless Begin takes it).
bool __cdecl BlockReader::Next(PString& out)
{
    if (!file || child || ended) return false;
    if (pending) {
        Begin();
        End();
        pending = false;
    }
    while (file->ReadLine(line, bufferSize, 0)) {
        cursor = line;
        if (!SkipToToken(cursor) || !*cursor) continue;
        if (Crt80::sscanf_s1(cursor, "%s", token) != 1) continue;
        cursor += std::strlen(token);
        if (*token == '{') {
            pending = true;
            Begin();
            End();
            continue;
        }
        if (std::strchr(line, '}')) {
            ended = true;
            return false;
        }
        if (std::strchr(line, '{')) pending = true;
        AssignText(name, token);
        out = name;
        return true;
    }
    return false;
}

bool __cdecl BlockReader::ReadString(char* out, int size)
{
    if (cursor < line + bufferSize && out && size > 0 && SkipToToken(cursor) && ParseString()) {
        Crt80::strncpy(out, token, size - 1);
        out[size - 1] = 0;
        return true;
    }
    return false;
}

bool __cdecl BlockReader::ReadString(PString& out)
{
    if (cursor < line + bufferSize && SkipToToken(cursor) && ParseString()) {
        AssignText(out, token);
        return true;
    }
    return false;
}

// (the vector forms stop at a line end or a comment; the zero check in the blank loop is never reached, as in the
// original)
int __cdecl BlockReader::ReadStringv(PString* out, int count)
{
    int n = 0;
    if (!count) return 0;
    while (--count, cursor < line + bufferSize) {
        while (Blank(*cursor)) ++cursor;
        if (IsEol(cursor) || IsComment(cursor)) return n;
        if (!ParseString()) return n;
        AssignText(out[n], token);
        ++n;
        if (!count) return n;
    }
    return n;
}

bool __cdecl BlockReader::ReadInt(int& v)
{
    if (cursor < line + bufferSize && SkipToToken(cursor)) {
        char* end;
        const long r = Crt80::strtol(cursor, &end, 0);
        if (cursor != end) {
            v = static_cast<int>(r);
            SkipNonWhiteSpace(cursor);
            return true;
        }
    }
    return false;
}

int __cdecl BlockReader::ReadIntv(int* out, int count)
{
    int n = 0;
    if (!count) return 0;
    while (--count, cursor < line + bufferSize) {
        while (Blank(*cursor)) ++cursor;
        if (IsEol(cursor) || IsComment(cursor)) return n;
        char* end;
        const long r = Crt80::strtol(cursor, &end, 0);
        if (cursor == end) return n;
        out[n] = static_cast<int>(r);
        SkipNonWhiteSpace(cursor);
        ++n;
        if (!count) return n;
    }
    return n;
}

bool __cdecl BlockReader::ReadFloat(float& v)
{
    if (cursor < line + bufferSize && SkipToToken(cursor)) {
        char* end;
        const double r = Crt80::strtod(cursor, &end);
        if (cursor != end) {
            v = static_cast<float>(r);
            SkipNonWhiteSpace(cursor);
            return true;
        }
    }
    return false;
}

int __cdecl BlockReader::ReadFloatv(float* out, int count)
{
    int n = 0;
    if (!count) return 0;
    while (--count, cursor < line + bufferSize) {
        while (Blank(*cursor)) ++cursor;
        if (IsEol(cursor) || IsComment(cursor)) return n;
        char* end;
        const double r = Crt80::strtod(cursor, &end);
        if (cursor == end) return n;
        out[n] = static_cast<float>(r);
        SkipNonWhiteSpace(cursor);
        ++n;
        if (!count) return n;
    }
    return n;
}

bool __cdecl BlockReader::ReadBool(bool& v)
{
    if (cursor < line + bufferSize && SkipToToken(cursor)) {
        char* end;
        const long r = Crt80::strtol(cursor, &end, 0);
        if (cursor != end) {
            v = r != 0;
            SkipNonWhiteSpace(cursor);
            return true;
        }
    }
    return false;
}

// to the line that closes the block (a top-level reader: to the end of the file)
bool __cdecl BlockReader::SkipBlock()
{
    if (ended) return true;
    if (!file || child) return false;
    if (!parent) {
        file->Seek(0, FILE_SEEK_END);
        pending = false;
        return true;
    }
    int depth = pending ? 2 : 1;
    pending = false;
    while (file->ReadLine(line, bufferSize, 0)) {
        if (std::strchr(line, '{')) {
            ++depth;
        } else if (std::strchr(line, '}') && --depth <= 0) {
            ended = true;
            return true;
        }
    }
    return false;
}

bool __cdecl BlockReader::ParseEntry()
{
    if (!*cursor || Crt80::sscanf_s1(cursor, "%s", token) != 1) return false;
    cursor += std::strlen(token);
    return true;
}

// a token, or a quoted string (to the closing quote, which must be on the line)
bool __cdecl BlockReader::ParseString()
{
    if (*cursor != '"') {
        if (*cursor) {
            if (Crt80::sscanf_s1(cursor, "%s", token) != 1) return false;
            cursor += std::strlen(token);
        }
        return true;
    }
    char* out = token;
    for (++cursor;; ++cursor) {
        if (*cursor == '"') {
            ++cursor;
            *out = 0;
            return true;
        }
        if (!*cursor) return false;
        *out++ = *cursor;
    }
}

// the next line that is not blank or a comment starts with "{" (else the file goes back before it)
bool __cdecl BlockReader::IsBlockStart()
{
    if (!file) return false;
    if (pending) return true;
    long before = file->Position();
    while (file->ReadLine(line, bufferSize, 0)) {
        cursor = line;
        while (Blank(*cursor)) ++cursor;
        if (!IsEol(cursor) && !IsComment(cursor)) break;
        before = file->Position();
    }
    if (*cursor == '{') return true;
    file->Seek(before, FILE_SEEK_SET);
    return false;
}

// (spaces and tabs; false at a line end, a comment or the end of the text)
bool __cdecl BlockReader::SkipToToken(const char*& p)
{
    while (Blank(*p)) ++p;
    return !IsEol(p) && !IsComment(p);
}

bool __cdecl BlockReader::SkipWhiteSpace(const char*& p)
{
    while (Blank(*p)) ++p;
    return true;
}

bool __cdecl BlockReader::SkipNonWhiteSpace(const char*& p)
{
    for (char c = *p;;) {
        if (c == ' ') return true;
        c = *p;
        if (c == '\t') return true;
        if (!c) return false;
        c = *++p;
    }
}

bool __cdecl BlockReader::IsEol(const char* p) { return (p[0] == '\r' && p[1] == '\n') || p[0] == '\n'; }
bool __cdecl BlockReader::IsComment(const char* p) { return p[0] == '/' && p[1] == '/'; }

// ---- BPE (Philip Gage's byte pair encoding; one 0x3a04-byte workspace)

namespace {

struct Work {
    unsigned char buffer[0x800];  // +0x0000 the block
    unsigned char leftcode[0x100];   // +0x0800
    unsigned char rightcode[0x100];  // +0x0900 (while reading: "byte used")
    unsigned char left[0x1000];   // +0x0a00 the pair hash
    unsigned char right[0x1000];  // +0x1a00
    unsigned char count[0x1000];  // +0x2a00
    int size;                     // +0x3a00
};
static_assert(sizeof(Work) == 0x3a04, "BPE workspace");

unsigned Hash(unsigned char a, unsigned char b) { return ((static_cast<unsigned>(b) << 5) ^ a) & 0xfff; }

// the slot of pair (a, b): the first empty one or the pair's own
unsigned Lookup(const Work& w, unsigned char a, unsigned char b)
{
    unsigned h = Hash(a, b);
    while (w.count[h] && (w.right[h] != b || w.left[h] != a)) h = (h + 1) & 0xfff;
    return h;
}

// 0x1000d270: a block of up to 0x800 bytes and 210 distinct values, its pairs counted
const unsigned char* Fill(Work& w, const unsigned char* in, const unsigned char* end)
{
    int used = 0;
    std::memset(w.count, 0, sizeof(w.count));
    std::memset(w.rightcode, 0, sizeof(w.rightcode));
    for (int i = 0; i < 0x100; ++i) w.leftcode[i] = static_cast<unsigned char>(i);
    w.size = 0;
    do {
        if (used >= 0xd2 || in >= end) break;
        const unsigned char c = *in++;
        if (w.size > 0) {
            const unsigned char prev = w.buffer[w.size - 1];
            const unsigned h = Lookup(w, prev, c);
            w.left[h] = prev;
            w.right[h] = c;
            if (w.count[h] < 0xff) ++w.count[h];
        }
        w.buffer[w.size++] = c;
        if (!w.rightcode[c]) {
            ++used;
            w.rightcode[c] = 1;
        }
    } while (w.size < 0x800);
    return in;
}

// 0x1000d390: the pair table (runs of unchanged codes, groups of pairs), the size, the block; past `end` the
// pointer that would be (nothing more written)
unsigned char* Emit(const Work& w, unsigned char* out, unsigned char* end)
{
    int c = 0;
    while (c < 0x100) {
        if (c == w.leftcode[c]) {
            if (out + 1 >= end) return out + 1;
            int len = 1;
            ++c;
            while (c < 0x100 && c == w.leftcode[c]) {
                ++len;
                ++c;
                if (len >= 0x7f) break;
            }
            *out++ = static_cast<unsigned char>(len + 0x7f);
            continue;
        }
        int len = 0;
        int at = c + 1;
        int next = at + 1;
        for (;;) {
            if (!(len >= 0x7f || at >= 0x100 || at == w.leftcode[at])) {
                ++len, ++at, ++next;
                continue;
            }
            if (len < 0x7d && next < 0xff && next != w.leftcode[at + 1]) {
                ++len, ++at, ++next;
                continue;
            }
            break;
        }
        at = at - len - 1;
        *out++ = static_cast<unsigned char>(len);
        if (out + w.size + len + 2 >= end) return out + w.size + len + 2;
        if (len >= 0) {
            for (int k = len + 1; k; --k) {
                *out++ = w.leftcode[at];
                if (at != w.leftcode[at]) *out++ = w.rightcode[at];
                ++at;
            }
        }
        c = at;
    }
    if (out + w.size + 2 >= end) return out + w.size + 2;
    *out++ = static_cast<unsigned char>(w.size >> 8);
    *out++ = static_cast<unsigned char>(w.size);
    std::memcpy(out, w.buffer, w.size);
    return out + w.size;
}

}  // namespace

// Codes from 254 down that no byte of the block uses take the most frequent pair (3 or more) until none is left.
unsigned long __cdecl BPE::Comp(void* dst, const void* src, unsigned long size)
{
    const unsigned char* in = static_cast<const unsigned char*>(src);
    const unsigned char* const inEnd = in + size;
    unsigned char* const outStart = static_cast<unsigned char*>(dst);
    unsigned char* const outEnd = outStart + size;
    unsigned char* out = outStart;
    unsigned char leftch = 0, rightch = 0;
    Work* const w = static_cast<Work*>(Alloc::New(sizeof(Work)));
    if (in < inEnd) {
        do {
            in = Fill(*w, in, inEnd);
            for (int code = 0xff - 1; code >= 0; --code) {
                while (code >= 0 && !(code == w->leftcode[code] && !w->rightcode[code])) --code;
                if (code < 0) break;
                unsigned best = 2;
                for (int i = 0; i < 0x1000; ++i) {
                    if (w->count[i] > best) {
                        best = w->count[i];
                        leftch = w->left[i];
                        rightch = w->right[i];
                    }
                }
                if (best < 3) break;
                const int last = w->size - 1;
                int i = 0, j = 0;
                const unsigned char cd = static_cast<unsigned char>(code);
                for (; i < last; ++i) {
                    if (w->buffer[i] == leftch && w->buffer[i + 1] == rightch) {
                        if (i > 0) {
                            const unsigned char prev = w->buffer[j - 1];
                            unsigned h = Lookup(*w, prev, leftch);
                            w->left[h] = prev;
                            w->right[h] = leftch;
                            if (w->count[h] > 1) --w->count[h];
                            h = Lookup(*w, prev, cd);
                            w->left[h] = prev;
                            w->right[h] = cd;
                            if (w->count[h] < 0xff) ++w->count[h];
                        }
                        if (i < last - 1) {
                            const unsigned char next = w->buffer[i + 2];
                            unsigned h = Lookup(*w, rightch, next);
                            w->left[h] = rightch;
                            w->right[h] = next;
                            if (w->count[h] > 1) --w->count[h];
                            h = Lookup(*w, cd, next);
                            w->left[h] = cd;
                            w->right[h] = next;
                            if (w->count[h] < 0xff) ++w->count[h];
                        }
                        w->buffer[j++] = cd;
                        ++i;
                        --w->size;
                    } else {
                        w->buffer[j++] = w->buffer[i];
                    }
                }
                w->buffer[j] = w->buffer[i];
                w->leftcode[code] = leftch;
                w->rightcode[code] = rightch;
                const unsigned h = Lookup(*w, leftch, rightch);
                w->left[h] = leftch;
                w->right[h] = rightch;
                w->count[h] = 1;
            }
            out = Emit(*w, out, outEnd);
            if (out >= outEnd) {
                if (w) Alloc::Delete(w);
                std::memcpy(dst, src, size);
                return size;
            }
        } while (in < inEnd);
    }
    if (w) Alloc::Delete(w);
    return static_cast<unsigned long>(out - outStart);
}

// (the pair stack, the tables and the identity table lie in one frame as in the original, so a stack run past its
// 257 bytes lands in the identity table the same way)
unsigned long __cdecl BPE::Decomp(void* dst, const void* src, unsigned long size)
{
    unsigned char frame[0x404];
    unsigned char* const left = frame;            // esp+0x20
    unsigned char* const stack = frame + 0xff;    // stack[k] at esp+0x11f+k
    unsigned char* const identity = frame + 0x200;  // esp+0x220
    unsigned char* const right = frame + 0x300;   // esp+0x320
    const unsigned char* in = static_cast<const unsigned char*>(src);
    const unsigned char* const end = in + size;
    unsigned char* out = static_cast<unsigned char*>(dst);
    for (int i = 0; i < 0x100; ++i) identity[i] = static_cast<unsigned char>(i);
    while (in < end) {
        unsigned count = *in++;
        std::memcpy(left, identity, 0x100);
        unsigned c = 0;
        for (;;) {
            if (count > 0x7f) {
                c += count - 0x7f;
            } else {
                for (unsigned k = count + 1; k; --k) {
                    const unsigned char v = *in++;
                    left[c] = v;
                    if (c != v) right[c] = *in++;
                    ++c;
                }
            }
            if (static_cast<int>(c) > 0xff) break;
            count = *in++;
        }
        unsigned n = (static_cast<unsigned>(in[0]) << 8) | in[1];
        in += 2;
        unsigned sp = 0;
        for (;;) {
            unsigned v;
            if (sp) {
                v = stack[sp];
                --sp;
            } else {
                if (!n) break;
                --n;
                v = *in++;
            }
            const unsigned char l = left[v];
            if (v == l) {
                *out++ = static_cast<unsigned char>(v);
            } else {
                stack[sp + 1] = right[v];
                stack[sp + 2] = l;
                sp += 2;
            }
        }
    }
    return static_cast<unsigned long>(out - static_cast<unsigned char*>(dst));
}

}  // namespace Jet
