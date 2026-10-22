// Unit U9b of KernelStandard_7c: the IOSlave family (0x10003420..0x10003920, 0x1002ae70..0x1002c6af): memory, growing
// memory, file and scrambled-resource slaves, the buffered read cursor and the history-keeping write cursor. Notes:
// D:/trainz12-decomp/notes/KernelStandard.md ("U9b").
#include <jet/KernelStandard.h>
#include <jet/Math.h>

#include "Crt80.h"

#include <windows.h>

#include <malloc.h>
#include <cstring>

namespace Jet {

// ---- IOSlave

IOSlave::IOSlave() {}
IOSlave::IOSlave(const IOSlave&) {}
IOSlave& IOSlave::operator=(const IOSlave&) { return *this; }
IOSlave::~IOSlave() {}
unsigned long IOSlave::GetSize() { return 0; }
unsigned long IOSlave::ReadData(void*, unsigned long) { return 0; }
unsigned long IOSlave::WriteData(const void*, unsigned long) { return 0; }

bool IOSlave::WriteByte(unsigned char v) { return WriteData(&v, 1) == 1; }
bool IOSlave::WriteUINT32(unsigned long v) { return WriteData(&v, 4) == 4; }
bool IOSlave::ReadUINT32(unsigned long& v) { return ReadData(&v, 4) == 4; }

// the cursor's bytes, 4 KB at a time, until one write comes short (the bytes written)
unsigned long IOSlave::Write(IOSlaveReadCursor& src, unsigned long size)
{
    unsigned long written = 0;
    while (size) {
        const unsigned long n = size > 0x1000 ? 0x1000 : size;
        const void* const p = src.Access(0, n);
        if (!p) break;
        src.Inc(n);
        const unsigned long w = WriteData(p, n);
        written += w;
        size -= w;
        if (w != n || n == 0 || size == 0) break;
    }
    return written;
}

// ---- IOSlaveMemory

IOSlaveMemory::IOSlaveMemory(void* data_, unsigned long size_)
    : data(static_cast<unsigned char*>(data_)), position(0), size(data_ ? size_ : 0)
{
}
IOSlaveMemory::IOSlaveMemory(const IOSlaveMemory& o) : IOSlave(o), data(o.data), position(o.position), size(o.size) {}
IOSlaveMemory& IOSlaveMemory::operator=(const IOSlaveMemory& o)
{
    data = o.data;
    position = o.position;
    size = o.size;
    return *this;
}
IOSlaveMemory::~IOSlaveMemory() {}

bool IOSlaveMemory::Seek(unsigned long p)
{
    if (p > size) return false;
    position = p;
    return true;
}
unsigned long IOSlaveMemory::Tell() { return position; }
unsigned long IOSlaveMemory::GetSize() { return size; }

unsigned long IOSlaveMemory::ReadData(void* dst, unsigned long n)
{
    const unsigned long left = size - position;
    if (n >= left) n = left;
    if (dst) std::memcpy(dst, data + position, n);
    position += n;
    return n;
}

unsigned long IOSlaveMemory::WriteData(const void* src, unsigned long n)
{
    const unsigned long left = size - position;
    if (n >= left) n = left;
    std::memcpy(data + position, src, n);
    position += n;
    return n;
}

// ---- IOSlaveMemoryDynamic: the vector as MSVC8's std::vector<char> keeps it (memory from msvcr80)

namespace {

using Vector = IOSlaveMemoryDynamic::Vector;

unsigned long VSize(const Vector& v) { return v.first ? static_cast<unsigned long>(v.last - v.first) : 0; }
unsigned long VCapacity(const Vector& v) { return v.first ? static_cast<unsigned long>(v.end - v.first) : 0; }

// resize(n) with zero bytes: growth to capacity + capacity / 2, or the size needed when that is more
void VResize(Vector& v, unsigned long n)
{
    const unsigned long size = VSize(v);
    if (n <= size) {
        if (v.first && n < size) v.last = v.first + n;
        return;
    }
    const unsigned long count = n - size;
    const unsigned long capacity = VCapacity(v);
    if (capacity < size + count) {
        unsigned long grown = 0xffffffffu - capacity / 2 < capacity ? 0 : capacity + capacity / 2;
        if (grown < size + count) grown = size + count;
        char* const p = static_cast<char*>(Crt80::operator_new(grown));
        if (size) std::memmove(p, v.first, size);
        std::memset(p + size, 0, count);
        if (v.first) Crt80::operator_delete(v.first);
        v.first = p;
        v.last = p + size + count;
        v.end = p + grown;
    } else {
        std::memset(v.last, 0, count);
        v.last += count;
    }
}

void VCopy(Vector& v, const Vector& o)
{
    const unsigned long n = VSize(o);
    v.first = v.last = v.end = nullptr;
    if (!n) return;
    char* const p = static_cast<char*>(Crt80::operator_new(n));
    v.first = v.last = p;
    v.end = p + n;
    std::memmove(p, o.first, n);
    v.last = p + n;
}

void VAssign(Vector& v, const Vector& o)
{
    if (&v == &o) return;
    const unsigned long n = VSize(o);
    if (n == 0) {
        if (v.first) v.last = v.first;
    } else if (n <= VCapacity(v)) {
        std::memmove(v.first, o.first, n);
        v.last = v.first + n;
    } else {
        if (v.first) Crt80::operator_delete(v.first);
        v.first = static_cast<char*>(Crt80::operator_new(n));
        std::memmove(v.first, o.first, n);
        v.last = v.end = v.first + n;
    }
}

}  // namespace

IOSlaveMemoryDynamic::IOSlaveMemoryDynamic() : bytes{{0, 0, 0, 0}, nullptr, nullptr, nullptr}, position(0) {}

IOSlaveMemoryDynamic::IOSlaveMemoryDynamic(const IOSlaveMemoryDynamic& o) : IOSlave(o), bytes(), position(0)
{
    VCopy(bytes, o.bytes);
    position = o.position;
}

IOSlaveMemoryDynamic& IOSlaveMemoryDynamic::operator=(const IOSlaveMemoryDynamic& o)
{
    VAssign(bytes, o.bytes);
    position = o.position;
    return *this;
}

IOSlaveMemoryDynamic::~IOSlaveMemoryDynamic()
{
    if (bytes.first) Crt80::operator_delete(bytes.first);
    bytes.first = bytes.last = bytes.end = nullptr;
}

bool IOSlaveMemoryDynamic::Seek(unsigned long p)
{
    if (p > VSize(bytes)) return false;
    position = p;
    return true;
}
unsigned long IOSlaveMemoryDynamic::Tell() { return position; }
unsigned long IOSlaveMemoryDynamic::GetSize() { return VSize(bytes); }

unsigned long IOSlaveMemoryDynamic::ReadData(void* dst, unsigned long n)
{
    const unsigned long size = VSize(bytes);
    if (size <= position) return 0;
    if (n > size - position) n = size - position;
    if (dst) std::memcpy(dst, bytes.first + position, n);
    position += n;
    return n;
}

// (the original's checked vector: writing nothing at the end, or into an empty vector, is an invalid parameter)
unsigned long IOSlaveMemoryDynamic::WriteData(const void* src, unsigned long n)
{
    if (!src) return 0;
    if (position + n > VSize(bytes)) VResize(bytes, position + n);
    if (!bytes.first || position >= VSize(bytes)) Crt80::_invalid_parameter_noinfo();
    std::memcpy(bytes.first + position, src, n);
    position += n;
    return n;
}

const char* IOSlaveMemoryDynamic::GetData() const { return bytes.first && bytes.last != bytes.first ? bytes.first : nullptr; }

// ---- IOSlaveFile

IOSlaveFile::IOSlaveFile(File* f) : file(f && f->Handle() != -1 ? f : nullptr), size(file ? file->Size() : 0) {}
IOSlaveFile::IOSlaveFile(const IOSlaveFile& o) : IOSlave(o), file(o.file), size(o.size) {}
IOSlaveFile& IOSlaveFile::operator=(const IOSlaveFile& o)
{
    file = o.file;
    size = o.size;
    return *this;
}
IOSlaveFile::~IOSlaveFile() {}

bool IOSlaveFile::Seek(unsigned long p)
{
    if (!file || p > size) return false;
    return file->Seek(static_cast<long>(p), FILE_SEEK_SET) == p;
}
unsigned long IOSlaveFile::Tell() { return file ? static_cast<unsigned long>(file->Position()) : 0; }
unsigned long IOSlaveFile::GetSize() { return size; }

unsigned long IOSlaveFile::ReadData(void* dst, unsigned long n)
{
    if (!file) return 0;
    const unsigned long left = size - static_cast<unsigned long>(file->Position());
    if (n > left) n = left;
    if (!dst) return Seek(Tell() + n) ? n : 0;
    return file->Read(dst, n, 1);
}

unsigned long IOSlaveFile::WriteData(const void* src, unsigned long n)
{
    if (!file) return 0;
    const unsigned long r = file->Write(src, n, 1);
    const unsigned long at = static_cast<unsigned long>(file->Position());
    size = at < size ? size : at;
    return r;
}

// ---- IOSlaveReadCursor

IOSlaveReadCursor::IOSlaveReadCursor() : slave(nullptr), buffer(nullptr), offset(0), filled(0), capacity(0) {}

IOSlaveReadCursor::IOSlaveReadCursor(IOSlave* s)
    : slave(s), position(0), buffer(nullptr), offset(0), filled(0), capacity(0)
{
    if (s) position = s->Tell();
}

IOSlaveReadCursor::IOSlaveReadCursor(const IOSlaveReadCursor& o)
    : slave(o.slave), position(o.offset + o.position), buffer(nullptr), offset(0), filled(0), capacity(0)
{
}

IOSlaveReadCursor::~IOSlaveReadCursor()
{
    if (buffer) Alloc::Delete(buffer);
}

void IOSlaveReadCursor::operator=(IOSlave* s)
{
    slave = s;
    position = 0;
    if (s) position = s->Tell();
    buffer = nullptr;  // (the old buffer is not freed, as in the original)
    offset = 0;
    filled = 0;
    capacity = 0;
}

void IOSlaveReadCursor::operator=(const IOSlaveReadCursor& o)
{
    if (buffer) Alloc::Delete(buffer);
    slave = o.slave;
    const unsigned long at = o.offset + o.position;
    buffer = nullptr;
    offset = 0;
    filled = 0;
    capacity = 0;
    position = at;
}

// past the buffered bytes the buffer is dropped (the stream position moves on)
void IOSlaveReadCursor::Inc(unsigned long n)
{
    unsigned long at = offset + n;
    if (at > filled) {
        at = 0;
        if (slave) position += offset + n;
        filled = 0;
    }
    offset = at;
}
void IOSlaveReadCursor::operator+=(unsigned long n) { Inc(n); }
void IOSlaveReadCursor::operator++() { Inc(1); }
void IOSlaveReadCursor::operator++(int) { Inc(1); }

unsigned char IOSlaveReadCursor::operator*()
{
    const unsigned char* const p = static_cast<const unsigned char*>(Access(0, 1));
    return p ? *p : 0;
}
unsigned char IOSlaveReadCursor::AccessByte(unsigned long at)
{
    const unsigned char* const p = static_cast<const unsigned char*>(Access(at, 1));
    return p ? *p : 0;
}
unsigned char IOSlaveReadCursor::operator[](unsigned long at) { return AccessByte(at); }

unsigned long IOSlaveReadCursor::AccessUInt32(unsigned long at)
{
    const unsigned long* const p = static_cast<const unsigned long*>(Access(at, 4));
    return p ? *p : 0;
}

int IOSlaveReadCursor::GetIndex(const void* p)
{
    const unsigned char* const q = static_cast<const unsigned char*>(p);
    if (buffer && buffer <= q && q <= buffer + filled) return static_cast<int>(q - buffer - offset);
    return 0;
}

// The unread bytes move to the buffer's start (into a bigger buffer when at + n does not fit), then the slave fills
// the rest; on failure the buffer and the slave are let go.
const void* IOSlaveReadCursor::Access(unsigned long at, unsigned long n)
{
    if (offset + at + n <= filled) return buffer + offset + at;
    if (slave) {
        const unsigned long need = at + n;
        bool ok = false;
        if (need < capacity) {
            position += offset;
            std::memmove(buffer, buffer + offset, filled - offset);
            filled -= offset;
            offset = 0;
            ok = true;
        } else {
            capacity = need;
            unsigned char* const p = static_cast<unsigned char*>(Alloc::New(need));
            if (p) {
                position += offset;
                std::memcpy(p, buffer + offset, filled - offset);
                if (buffer) Alloc::Delete(buffer);
                filled -= offset;
                buffer = p;
                offset = 0;
                ok = true;
            }
        }
        if (ok && slave->Seek(position + filled)) {
            filled += slave->ReadData(buffer + filled, capacity - filled);
            if (at + n + offset <= filled) return buffer + at;
        }
    }
    if (buffer) Alloc::Delete(buffer);
    buffer = nullptr;
    slave = nullptr;
    offset = 0;
    filled = 0;
    capacity = 0;
    return nullptr;
}

// ---- IOSlaveWriteCursor

IOSlaveWriteCursor::IOSlaveWriteCursor(IOSlave* s, unsigned long chunk_) : slave(s), chunk(chunk_), current(0)
{
    buffers[0] = static_cast<unsigned char*>(Alloc::New(chunk_));
    buffers[1] = static_cast<unsigned char*>(Alloc::New(chunk));
    fill[0] = 0;
    fill[1] = 0;
}

IOSlaveWriteCursor::~IOSlaveWriteCursor()
{
    if (buffers[0]) Alloc::Delete(buffers[0]);
    if (buffers[1]) Alloc::Delete(buffers[1]);
}

IOSlaveWriteCursor::IOSlaveWriteCursor(const IOSlaveWriteCursor& o) : IOSlave(o) {}
void IOSlaveWriteCursor::operator=(const IOSlaveWriteCursor&) {}
bool IOSlaveWriteCursor::Seek(unsigned long) { return false; }
unsigned long IOSlaveWriteCursor::Tell() { return 0; }
unsigned long IOSlaveWriteCursor::ReadData(void*, unsigned long) { return 0; }

// through to the slave (a short write forgets the history), then into the history: the current buffer fills up,
// then the other one starts over
unsigned long IOSlaveWriteCursor::WriteData(const void* src, unsigned long size)
{
    if (!slave) return 0;
    if (slave->WriteData(src, size) != size) {
        fill[0] = 0;
        fill[1] = 0;
        return 0;
    }
    if (!chunk) return size;
    const unsigned char* p = static_cast<const unsigned char*>(src);
    for (unsigned long left = size; left;) {
        unsigned long n = chunk - fill[current];
        if (!n) {
            current = 1 - current;
            fill[current] = 0;
            n = chunk;
        }
        if (n > left) n = left;
        std::memcpy(buffers[current] + fill[current], p, n);
        fill[current] += n;
        p += n;
        left -= n;
    }
    return size;
}

// [offset, offset + size) back from the end: from the history when it is all there (current buffer, then the
// older one), else read from the slave around its position
bool IOSlaveWriteCursor::ReadData(void* dst, int at, unsigned long size)
{
    unsigned char* const out = static_cast<unsigned char*>(dst);
    if (static_cast<int>(at + size) <= 0 && static_cast<unsigned long>(-at) <= fill[0] + fill[1]) {
        const unsigned long cur = fill[current];
        if (static_cast<unsigned long>(-at) <= cur) {
            std::memcpy(out, buffers[current] + cur + at, size);
            return true;
        }
        int from = at + static_cast<int>(cur);
        int skip = 0;
        int n = static_cast<int>(size);
        if (from < 0) {
            skip = -from;
            n = from + static_cast<int>(size);
            from = 0;
        }
        if (n > 0) {
            if (n + from > static_cast<int>(cur)) n = static_cast<int>(cur) - from;
            std::memcpy(out + skip, buffers[current] + from, n);
        }
        const int other = 1 - static_cast<int>(current);
        from = static_cast<int>(fill[other]) + static_cast<int>(cur) + at;
        skip = 0;
        n = static_cast<int>(size);
        if (from < 0) {
            skip = -from;
            n = from + static_cast<int>(size);
            from = 0;
        }
        if (n > 0) {
            if (n + from > static_cast<int>(fill[other])) n = static_cast<int>(fill[other]) - from;
            std::memcpy(out + skip, buffers[other] + from, n);
        }
        return true;
    }
    if (!slave) return false;
    const unsigned long here = slave->Tell();
    if (!slave->Seek(at + here)) return false;
    if (slave->ReadData(dst, size) != size) return false;
    return slave->Seek(here);
}

// a copy of what was written `offset` bytes back; a copy that overlaps itself (-offset < size, under 4 KB) reads
// the source once and writes it over and over
unsigned long IOSlaveWriteCursor::WriteCopy(int at, unsigned long size)
{
    unsigned char block[0x1000];
    unsigned long written = 0;
    unsigned long piece = 0x1000;
    bool repeat = false, haveBlock = false;
    if (at < 0 && at > -static_cast<int>(size) && static_cast<unsigned long>(-at) < 0x1000) {
        piece = static_cast<unsigned long>(-at);
        repeat = true;
    }
    while (size) {
        const unsigned long n = size > piece ? piece : size;
        if (!haveBlock) {
            if (!ReadData(block, at, n)) break;
            haveBlock = repeat;
        }
        const unsigned long w = WriteData(block, n);
        written += w;
        if (w != n || n == 0) break;
        size -= w;
    }
    return written;
}

// ---- IOSlaveResource

namespace {

const unsigned char* KeyBuffer(const AnsiString& s) { return *reinterpret_cast<const unsigned char* const*>(&s); }
unsigned KeyLength(const AnsiString& s) { return reinterpret_cast<const unsigned*>(KeyBuffer(s))[1]; }

unsigned long Ror(unsigned long x, unsigned n) { return n ? (x >> n) | (x << (32 - n)) : x; }
unsigned long Rol(unsigned long x, unsigned n) { return n ? (x << n) | (x >> (32 - n)) : x; }
unsigned char Ror8(unsigned char x, unsigned n) { return n ? static_cast<unsigned char>((x >> n) | (x << (8 - n))) : x; }
unsigned char Rol8(unsigned char x, unsigned n) { return n ? static_cast<unsigned char>((x << n) | (x >> (8 - n))) : x; }

// x rotated right by n, or left by -n when n is negative (MSVC's signed remainder: counts stay in 0..31)
unsigned long RotBy(unsigned long x, unsigned long n)
{
    const int s = static_cast<int>(n);
    if (s < 0) return Rol(x, static_cast<unsigned>(static_cast<int>(0u - n) % 32));
    return Ror(x, static_cast<unsigned>(s % 32));
}

// what both directions mix in for a position
struct Mix {
    unsigned char lead;  // the byte rotated with the data
    unsigned char rest;  // the bytes xored into it
};
Mix MixFor(unsigned long pos, unsigned long keyWord, const AnsiString& key)
{
    const unsigned long a = RotBy(pos ^ 0x5b9cd03du, pos / 5);
    const unsigned long b = RotBy((pos / 3) ^ 0x17b9d39fu, pos / 23 + 10);
    unsigned char lead = static_cast<unsigned char>(pos / 7919);
    lead = static_cast<unsigned char>(lead + static_cast<unsigned char>((static_cast<unsigned long long>(pos) * 0x80c121b3u) >> 40));
    lead = static_cast<unsigned char>(lead ^ static_cast<unsigned char>(a) ^ static_cast<unsigned char>(b));
    const unsigned char k = KeyBuffer(key)[8 + (pos / 11) % (KeyLength(key) + 1)];
    const unsigned long c = RotBy(keyWord, pos / 3);
    return {lead, static_cast<unsigned char>(static_cast<unsigned char>(pos) ^ static_cast<unsigned char>(c) ^ k)};
}

}  // namespace

unsigned char IOSlaveResource::CheckWrite(unsigned long pos, unsigned char v)
{
    const Mix m = MixFor(pos, keyWord, key);
    const unsigned long s = m.lead + keyWord + pos;
    if (static_cast<int>(s) < 0) v = Rol8(v, static_cast<unsigned>(static_cast<int>(0u - s) % 8));
    else v = Ror8(v, static_cast<unsigned>(static_cast<int>(s) % 8));
    return static_cast<unsigned char>(m.lead ^ m.rest ^ v);
}

unsigned char IOSlaveResource::CheckRead(unsigned long pos, unsigned char v)
{
    const Mix m = MixFor(pos, keyWord, key);
    const unsigned char x = static_cast<unsigned char>(m.rest ^ m.lead ^ v);
    const unsigned long t = 0u - (m.lead + keyWord + pos);
    if (static_cast<int>(t) < 0) return Rol8(x, static_cast<unsigned>(static_cast<int>(0u - t) % 8));
    return Ror8(x, static_cast<unsigned>(static_cast<int>(t) % 8));
}

// Writing: a header {1, 0, 0} with the key word from the performance counter; reading: {version <= 1, key word,
// size}, the key word then mixed with the size and (version 1) the key text's CRC. A failed header lets the slave go.
IOSlaveResource::IOSlaveResource(IOSlave* s, const String& k, bool write_)
    : slave(s), base(0), key(k), write(write_), version(0), keyWord(0), position(0), size(0)
{
    if (!slave || !KeyLength(key)) return;
    if (write) {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        keyWord = counter.LowPart;
        version = 1;
        unsigned long word = 1;
        if (s->WriteData(&word, 4) == 4) {
            word = 0;
            if (s->WriteData(&word, 4) == 4 && s->WriteUINT32(0)) {
                base = slave->Tell();
                return;
            }
        }
    } else if (s->ReadData(&version, 4) == 4 && s->ReadData(&keyWord, 4) == 4 && s->ReadUINT32(size) && version <= 1) {
        keyWord ^= size;
        const unsigned long crc = version ? Crc::Calc(static_cast<const char*>(key), KeyLength(key), 0) : 0;
        keyWord ^= crc;
        base = slave->Tell();
        return;
    }
    slave = nullptr;
}

IOSlaveResource::IOSlaveResource(const IOSlaveResource& o)
    : IOSlave(o), slave(o.slave), base(o.base), key(o.key), write(o.write), version(o.version), keyWord(o.keyWord),
      position(o.position), size(o.size)
{
}

IOSlaveResource& IOSlaveResource::operator=(const IOSlaveResource& o)
{
    slave = o.slave;
    base = o.base;
    key = o.key;
    write = o.write;
    version = o.version;
    keyWord = o.keyWord;
    position = o.position;
    size = o.size;
    return *this;
}

// A written stream is padded to 4 KB with time-based bytes, then the real header goes in front.
IOSlaveResource::~IOSlaveResource()
{
    if (!slave || !write || !KeyLength(key)) return;
    const unsigned long length = size;
    const unsigned char t = static_cast<unsigned char>(timeGetTime() ^ length);
    slave->Seek(base + length);
    const unsigned long n = 0x1000 - (size & 0xfff);
    unsigned char* const pad = static_cast<unsigned char*>(Alloc::New(n));
    for (unsigned long i = 0; i < n; ++i) pad[i] = static_cast<unsigned char>((t << (i % 24)) & 0x7f);
    IOSlaveResource::WriteData(pad, n);
    if (pad) Alloc::Delete(pad);
    unsigned long crc = 0;
    if (version) {
        const unsigned char* const b = KeyBuffer(key);
        crc = Crc::Calc(b ? b + 8 : nullptr, reinterpret_cast<const unsigned*>(b)[1], 0);
    }
    slave->Seek(0);
    unsigned long word = version;
    slave->WriteData(&word, 4);
    word = keyWord ^ crc ^ length;
    slave->WriteData(&word, 4);
    word = length;
    slave->WriteData(&word, 4);
}

// (the size limits seeks; past it the slave goes to the end and the result is false)
bool IOSlaveResource::Seek(unsigned long p)
{
    if (!slave) return false;
    if (!KeyLength(key)) return slave->Seek(p);
    const unsigned long limit = size;
    const unsigned long to = p > limit ? limit : p;
    const bool ok = slave->Seek(base + to) && p <= limit;
    position = slave->Tell() - base;
    return ok;
}

unsigned long IOSlaveResource::Tell()
{
    if (!slave) return 0;
    if (!KeyLength(key)) return slave->Tell();
    return position;
}

unsigned long IOSlaveResource::GetSize()
{
    if (!slave) return 0;
    if (!KeyLength(key)) return slave->GetSize();
    return size;
}

unsigned long IOSlaveResource::ReadData(void* dst, unsigned long n)
{
    if (!slave || write) return 0;
    if (!KeyLength(key)) return slave->ReadData(dst, n);
    if (position >= size) return 0;
    const unsigned long left = size - position;
    const unsigned long got = slave->ReadData(dst, n < left ? n : left);
    unsigned char* const p = static_cast<unsigned char*>(dst);
    for (unsigned long i = 0; i < got; ++i) p[i] = CheckRead(position++, p[i]);
    return got;
}

unsigned long IOSlaveResource::WriteData(const void* src, unsigned long n)
{
    if (!slave || !write) return 0;
    if (!KeyLength(key)) return slave->WriteData(src, n);
    unsigned char* const tmp = static_cast<unsigned char*>(n > 0x1000 ? Alloc::New(n) : _alloca(n));
    const unsigned char* const in = static_cast<const unsigned char*>(src);
    for (unsigned long i = 0; i < n; ++i) tmp[i] = CheckWrite(position + i, in[i]);
    const unsigned long w = slave->WriteData(tmp, n);
    if (n > 0x1000 && tmp) Alloc::Delete(tmp);
    position += w;
    size = position > size ? position : size;
    return w;
}

}  // namespace Jet
