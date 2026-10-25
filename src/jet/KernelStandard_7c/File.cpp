// Unit U10a of KernelStandard_7c: File (a buffered file over msvcr80's low-level I/O, with the BPE-compressed
// "CFIL" format) and FileEx (0x10008460..0x10008600, 0x10013f40..0x10015560, 0x1001fbb0..0x10021ef0). The
// substitutes ("name:path" through FileSub objects: archives, directories, links) are unit U10b (FileSub.cpp):
// Open looks the name up in that list and lets a FileSub open the file into this object.
// Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U10a").
#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "Crt80.h"
#include "FileSub.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

namespace Jet {

namespace {

const char kSource[] = "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/KernelStandard/File.cpp";
const char kHeader[] = "c:\\src\\trainz\\candidate\\ts12_sp1\\jet\\src\\jet\\kernelstandard\\File.h";

constexpr unsigned long kChunk = 0x4000;  // the buffer and the compressed chunk size

// msvcr80 flags
constexpr int O_RDONLY_ = 0, O_RDWR_ = 2, O_CREAT_ = 0x100, O_TRUNC_ = 0x200, O_EXCL_ = 0x400, O_BINARY_ = 0x8000;
constexpr int SH_DENYRW_ = 0x10, SH_DENYWR_ = 0x20;

// the open-files count (0x100adbc8): never read
unsigned long openFiles = 0;

}  // namespace

// ---- FileEx

FileEx::FileEx(const char* file, int line) : Exception(file, line) {}
FileEx::FileEx(const char* file, int line, const char* message) : Exception(file, line, message) {}
FileEx::FileEx(const FileEx& o) : Exception(o) {}
FileEx& FileEx::operator=(const FileEx& o)
{
    Exception::operator=(o);
    return *this;
}
FileEx::~FileEx() {}
const char* FileEx::Class() const { return "File"; }

// ---- the File internals (the original's helpers by address)

class FileImpl {
    static_assert(offsetof(File, size) == 0x14 && offsetof(File, position) == 0x1c && offsetof(File, flags) == 0x28 &&
                  offsetof(File, sub) == 0x30 && offsetof(File, encoding) == 0x34 && offsetof(File, path) == 0x38);

public:
    // 0x10021a90: reads n bytes at a raw offset (no further than the compressed size allows), -1 on failure
    static int RawRead(File& f, void* dst, unsigned long n, long offset)
    {
        if (f.sub) f.sub->Lock();
        int r;
        const long at = Crt80::_lseek(f.fd, offset, 0);
        if (at == -1 || static_cast<unsigned long>(at - f.dataStart) > f.size + (f.size >> 14) * 2)
            r = -1;
        else
            r = Crt80::_read(f.fd, dst, n);
        if (f.sub) f.sub->Unlock();
        return r;
    }
    // 0x10021b10: the same at rawPos, which moves on
    static int RawReadOn(File& f, void* dst, unsigned long n)
    {
        const int r = RawRead(f, dst, n, f.rawPos);
        if (r != -1) f.rawPos += r;
        return r;
    }
    // 0x10021690: the next buffer's worth (a chunk of a compressed file); a failure makes the size 0
    static unsigned long Fill(File& f, void* dst)
    {
        if (f.position >= f.size) return 0;
        long r;
        if ((f.mode & 0xf0) == 0x30) {
            unsigned short header = 0;
            r = RawRead(f, &header, 2, f.rawPos);
            if (r != -1) f.rawPos += r;
            if (r != 2) goto fail;
            const unsigned long n = header;
            if (n & 0x8000) {
                r = RawRead(f, dst, n & 0x7fff, f.rawPos);
                if (r != -1) f.rawPos += r;
                if (r == -1) goto fail;
                return r;
            }
            if (n > kChunk) {
                bpeTooBig() = 1;
                goto fail;
            }
            unsigned char packed[kChunk];
            if (static_cast<unsigned long>(RawReadOn(f, packed, n)) != n) goto fail;
            r = static_cast<long>(BPE::Decomp(dst, packed, n));
            if (r == -1) goto fail;
            return r;
        }
        {
            unsigned long n;
            if (f.size == static_cast<unsigned long>(-1))
                n = kChunk;
            else if (f.position + kChunk > f.size)
                n = f.size - f.position;
            else
                n = kChunk;
            r = RawRead(f, dst, n, f.rawPos);
            if (r != -1) {
                f.rawPos += r;
                return r;
            }
        }
    fail:
        f.size = 0;
        return 0;
    }
    // 0x10021840: writes the buffer out where it belongs
    static void Flush(File& f)
    {
        if (!f.dirty) return;
        const long at = Crt80::_tell(f.fd);
        const long want = static_cast<long>(f.dataStart + f.position - (f.cur - f.buffer));
        if (want != at) {
            Crt80::_lseek(f.fd, want, 0);
            f.rawPos = want;
        }
        int w = Crt80::_write(f.fd, f.buffer, f.count);
        if (w == -1) w = 0;
        f.position += w - f.count;
        f.count = 0;
        f.dirty = 0;
        f.cur = f.buffer;
    }
    // 0x100217e0: a large write past the buffer
    static unsigned long WriteDirect(File& f, const void* src, unsigned long n)
    {
        const long at = Crt80::_tell(f.fd);
        const long want = static_cast<long>(f.dataStart + f.position - (f.cur - f.buffer));
        if (want != at) {
            Crt80::_lseek(f.fd, want, 0);
            f.rawPos = want;
        }
        int w = Crt80::_write(f.fd, src, n);
        if (w == -1) {
            f.size = 0;
            w = 0;
        }
        return w;
    }
    static unsigned long Left(const File& f) { return f.count - (f.cur - f.buffer); }
    static void Refill(File& f)
    {
        if (f.position < f.Size()) {
            f.cur = f.buffer;
            f.count = Fill(f, f.buffer);
        }
    }
    // 0x10020950: an ANSI line (up to '\n', which is kept) into dst; the rest of a long line is skipped
    static unsigned long ReadLineA(File& f, char* dst, unsigned long size)
    {
        unsigned long left = size - 2, total = 0, avail = 1;
        int want = '\n';
        while (left && avail && want) {
            avail = Left(f);
            if (!avail) {
                Refill(f);
                avail = Left(f);
            }
            if (left && avail > left) avail = left;
            const void* const nl = std::memchr(f.cur, want, avail);
            if (nl) avail = static_cast<unsigned long>(static_cast<const char*>(nl) - f.cur + 1);
            if (dst) std::memcpy(dst, f.cur, avail);
            f.cur += avail;
            f.position += avail;
            if (nl) want = 0;
            if (!f.count) f.cur = f.buffer;
            dst += avail;
            total += avail;
            left -= avail;
        }
        *dst = 0;
        avail = 1;
        while (want && avail) {
            avail = Left(f);
            if (!avail) {
                Refill(f);
                avail = Left(f);
            }
            const void* const nl = std::memchr(f.cur, want, avail);
            if (nl) avail = static_cast<unsigned long>(static_cast<const char*>(nl) - f.cur + 1);
            f.cur += avail;
            f.position += avail;
            if (nl) want = 0;
            if (!f.count) f.cur = f.buffer;
        }
        return total;
    }
    // 0x10020c20: UTF-16 units up to `want` (cleared when found or at the end) from the buffer, max bytes
    static unsigned long ScanW(File& f, unsigned char* dst, unsigned long max, unsigned long& want)
    {
        unsigned long n = Left(f);
        if (max && max < n) n = max;
        unsigned long start = n;
        bool found = false;
        for (;;) {
            if (!n) break;
            const unsigned char lo = static_cast<unsigned char>(*f.cur);
            ++f.position;
            ++f.cur;
            unsigned char hi;
            if (n < 2) {
                // the unit straddles the buffer
                Refill(f);
                if (f.size <= f.position) {
                    --f.position;
                    --f.cur;
                    want = 0;
                    break;
                }
                if (dst) {
                    dst[0] = lo;
                    dst[1] = static_cast<unsigned char>(*f.cur);
                    dst += 2;
                }
                hi = static_cast<unsigned char>(*f.cur);
                ++f.position;
                ++start;
            } else {
                if (dst) {
                    dst[0] = lo;
                    dst[1] = static_cast<unsigned char>(*f.cur);
                    dst += 2;
                }
                hi = static_cast<unsigned char>(*f.cur);
                ++f.position;
                --n;
            }
            --n;
            ++f.cur;
            if ((static_cast<unsigned long>(hi) << 8 | lo) == want) {
                found = true;
                break;
            }
        }
        if (found) want = 0;
        if (!f.count) f.cur = f.buffer;
        return start - n;
    }
    // 0x10020b30: a UTF-16 line (bytes) into dst
    static unsigned long ReadLineW(File& f, unsigned char* dst, unsigned long size)
    {
        unsigned long left = (size & ~1ul) - 2, total = 0, got = 1, want = '\n';
        while (left && got && want) {
            if (!Left(f)) Refill(f);
            got = ScanW(f, dst, left, want);
            left -= got;
            dst += got;
            total += got;
        }
        *reinterpret_cast<unsigned short*>(dst) = 0;
        got = 1;
        while (want && got) {
            if (!Left(f)) Refill(f);
            got = ScanW(f, nullptr, 0, want);
        }
        return total;
    }
    static unsigned char& bpeTooBig()  // 0x100ae20e: set when a chunk claims more than 16 KB, never read
    {
        static unsigned char flag = 0;
        return flag;
    }
};

// the File's BinReader / BinWriter for the strings' Read / Write (0x10055390 / FileStringWriter)
namespace {
class FileStringReader : public BinReader {
public:
    explicit FileStringReader(File& f) : file(f) {}
    unsigned long Read(void* dst, unsigned long size) override { return file.Read(dst, size, 1); }
    File& file;
};
class FileStringWriter : public BinWriter {
public:
    explicit FileStringWriter(File& f) : file(f) {}
    unsigned long Write(const void* src, unsigned long size) override { return file.Write(src, size, 1); }
    File& file;
};
}  // namespace

// ---- File

const PString File::Sep("/");  // (0x100ae17c, made at load)
bool(__cdecl* File::callback)(unsigned long, unsigned long) = nullptr;  // 0x100adbcc

void File::SetCallBack(bool(__cdecl* progress)(unsigned long, unsigned long)) { callback = progress; }  // 0x100154c0

File::File()
{
    fd = -1;
    path.text[0] = 0;
    size = static_cast<unsigned long>(-1);
    buffer = nullptr;
    buffer = static_cast<char*>(Alloc::New(kChunk));
    cur = buffer;
    count = 0;
    dataStart = 0;
    rawPos = 0;
    position = 0;
    dirty = 0;
    mode = 0;
    subst = nullptr;
    flags = (flags & ~2) | 1;
    encoding = 1;
}

File::~File()
{
    if (fd != -1) Close();
    if ((flags & 1) && buffer) Alloc::Delete(buffer);
}

void File::SetBuffer(void* b)
{
    if (flags & 1) {
        if (buffer) Alloc::Delete(buffer);
        flags &= ~1;
    }
    buffer = static_cast<char*>(b);
}

File::Result File::Open(const char* p, unsigned long m, int enc)
{
    const char* const colon = std::strchr(p, ':');
    if (colon && colon - p >= 2) {
        PString base;
        base.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(p),
                                             static_cast<unsigned int>(colon - p));
        return Open(base, colon + 1, m, enc);
    }
    const PString none;
    return Open(none, p, m, enc);
}

File::Result File::Open(const PString& base, const char* p, unsigned long m, int enc)
{
    encoding = enc ? enc : 1;
    if (fd != -1) return Result(3);
    flags &= ~2;
    sub = nullptr;
    subst = nullptr;
    mode = m;
    fd = -1;
    cur = buffer;
    count = 0;
    dataStart = 0;
    position = 0;
    rawPos = 0;
    size = static_cast<unsigned long>(-1);
    dirty = 0;
    if ((m & 0xf000) == 0x1000) return Result(4);
    const unsigned long create = m & 0xf00;
    int oflag = O_BINARY_;
    if (create == 0x200)
        oflag = O_BINARY_ | O_CREAT_;
    else if (create == 0x100)
        oflag = O_BINARY_ | O_CREAT_ | O_TRUNC_;
    else if (create == 0x300)
        oflag = O_BINARY_ | O_CREAT_ | O_EXCL_;
    oflag |= (m & 0xf) ? O_RDWR_ : O_RDONLY_;
    const int pmode = (oflag & O_CREAT_) ? 0x180 : 0;
    if (create == 0x100 || create == 0x300) size = 0;

    subst = base.node && base.node->text ? Subst::Find(base) : nullptr;
    if (subst) {
        if ((mode & 0xf0) == 0) {
            Subst::OpenWrite(subst, this, p, oflag, pmode);
        } else {
            Subst::OpenRead(subst, this, p, oflag, pmode);
            if (fd == -1) return Result(2);
        }
    } else {
        path.Make("", p);
        fd = Crt80::sopen(path.text, oflag, (mode & 0xf) ? SH_DENYRW_ : SH_DENYWR_, pmode);
    }
    if (fd == -1) return Result((mode & 0xf00) == 0x300 ? 8 : 2);

    const unsigned long c = mode & 0xf00;
    if (c == 0x100 || c == 0x300) {
        if ((mode & 0xf0) == 0x10) mode = (mode & ~0xd0ul) | 0x20;
    } else if ((mode & 0xf0) == 0x10 || (mode & 0xf0) == 0x30) {
        // a "CFIL" header {"CFIL", version, size} (4 bytes only in version 0x101 archives) makes it compressed
        unsigned long version = 0x102;
        unsigned long headerLen = 0xc;
        if (sub) {
            version = sub->Version();
            headerLen = version == 0x101 ? 4 : 0xc;
        }
        const int n = FileImpl::RawReadOn(*this, buffer, headerLen);
        if (n == static_cast<int>(headerLen)) {
            if (std::strncmp(buffer, "CFIL", 4) != 0) {
                if ((mode & 0xf0) == 0x30) {
                    if (!(flags & 2)) Crt80::_close(fd);
                    return Result(6);
                }
                count += n;
                mode = (mode & ~0xd0ul) | 0x20;
            } else {
                unsigned long v = version;
                if (static_cast<int>(headerLen) > 4) v = *reinterpret_cast<unsigned long*>(buffer + 4);
                dataStart += headerLen;
                mode = (mode & ~0xc0ul) | 0x30;
                if (v >= 0x101 && static_cast<int>(headerLen) > 8) size = *reinterpret_cast<unsigned long*>(buffer + 8);
                if (mode & 0xf) {
                    if (!(flags & 2)) Crt80::_close(fd);
                    return Result(5);
                }
            }
        } else if (n == -1) {
            if (!(flags & 2)) {
                Crt80::_close(fd);
                fd = -1;
            }
            return Result(1);
        } else {
            mode = (mode & ~0xd0ul) | 0x20;
            count += n;
        }
    }
    if ((mode & 0xf00) == 0x200) Seek(0, FILE_SEEK_END);
    ++openFiles;
    return Result(0);
}

unsigned long File::Close()
{
    if (fd == -1) return 0;
    FileImpl::Flush(*this);
    if (sub) {
        sub->Close(this);
    } else if ((mode & 0xf) == 2) {
        // written plain to the file: packed now into a hidden temp file in BPE chunks, which replaces it
        unsigned long left = static_cast<unsigned long>(Crt80::_tell(fd));
        unsigned long total = left;
        Crt80::_close(fd);
        const int in = Crt80::open(path.text, O_BINARY_, 0);
        const unsigned long n = Subst::TempCounter()++;
        char temp[20];
        std::sprintf(temp, "%08X.T%08X", GetCurrentProcessId(), n);
        const int out = Crt80::sopen(temp, O_BINARY_ | O_CREAT_ | O_TRUNC_ | 1, SH_DENYRW_, 0x180);
        SetFileAttributesA(temp, FILE_ATTRIBUTE_HIDDEN);
        Crt80::_write(out, "CFIL", 4);
        const unsigned long version = 0x101;
        Crt80::_write(out, &version, 4);
        Crt80::_write(out, &total, 4);
        unsigned char raw[kChunk];
        unsigned long last = total;
        while (left && last) {
            const unsigned long want = left > kChunk ? kChunk : left;
            last = static_cast<unsigned long>(Crt80::_read(in, raw, want));
            left -= last;
            const unsigned long c = BPE::Comp(buffer, raw, last);
            const unsigned long header = c == last ? (c | 0x8000) & 0xffff : c & 0xffff;
            Crt80::_write(out, &header, 2);
            last = static_cast<unsigned long>(Crt80::_write(out, buffer, c));
        }
        Crt80::_close(in);
        Crt80::_close(out);
        Crt80::_unlink(path.text);
        Crt80::rename(temp, path.text);
        SetFileAttributesA(path.text, FILE_ATTRIBUTE_NORMAL);
    } else {
        Crt80::_close(fd);
    }
    --openFiles;
    fd = -1;
    return 0;
}

void File::Commit()
{
    if (fd == -1 || !dirty) return;
    const unsigned long at = position;
    FileImpl::Flush(*this);
    Crt80::_commit(fd);
    Seek(static_cast<long>(at), FILE_SEEK_SET);
}

unsigned long File::Size()
{
    if (size == static_cast<unsigned long>(-1)) {
        size = static_cast<unsigned long>(Crt80::_filelength(fd));
        if (size == static_cast<unsigned long>(-1)) throw FileEx(kHeader, 0xb46, "Size() failed to get file length");
    }
    return size;
}

unsigned long File::Tell() const { return position; }
bool File::Eof() { return Size() <= position; }
bool File::IsOpen() { return fd != -1; }
bool File::IsCompressed() { return (mode & 0xf0) == 0x30 || (mode & 0xf) == 2; }
const char* File::Name() const { return path.text; }
int File::GetEncoding() const { return encoding; }
void File::SetEncoding(int e) { encoding = e; }
int File::GetDefaultEncoding() { return 1; }
void File::SetLog(bool) {}

unsigned long File::Read(void* data, unsigned long itemSize, unsigned long items)
{
    if (fd == -1) throw FileEx(kSource, 0x8f4, "Read() File is not open");
    if ((mode & 0xf0) == 0) throw FileEx(kSource, 0x8f8, "Read() File not open for reading");
    unsigned long n = itemSize * items;
    if (!n) return 0;
    char* dst = static_cast<char*>(data);
    unsigned long total = 0, step = 1;
    do {
        if (!step) break;
        step = FileImpl::Left(*this);
        if (!step) {
            if (n < kChunk) {
                if (position < Size()) {
                    cur = buffer;
                    count = FileImpl::Fill(*this, buffer);
                }
                step = FileImpl::Left(*this);
                if (step) {
                    if (n < step) step = n;
                    std::memcpy(dst, cur, step);
                    cur += step;
                    position += step;
                }
            } else {
                step = FileImpl::Fill(*this, dst);
                position += step;
            }
        } else {
            if (n < step) step = n;
            std::memcpy(dst, cur, step);
            cur += step;
            position += step;
        }
        dst += step;
        total += step;
        n -= step;
    } while (n);
    return total / items;
}

unsigned long File::Write(const void* data, unsigned long itemSize, unsigned long items)
{
    if (fd == -1) throw FileEx(kSource, 0x937, "Write() File not open");
    if ((mode & 0xf) == 0) throw FileEx(kSource, 0x93b, "Write() File not open for writing");
    unsigned long n = itemSize * items;
    if (!n) return 0;
    const char* src = static_cast<const char*>(data);
    unsigned long total = 0, step = 1;
    do {
        if (!step) break;
        if (n < kChunk && count < kChunk) {
            step = kChunk - count;
            if (n < step) step = n;
            std::memcpy(cur, src, step);
            cur += step;
            const unsigned long used = static_cast<unsigned long>(cur - buffer);
            if (count < used) count = used;
            position += step;
            dirty += step;
            if (size < position) size = position;
            if (count == kChunk) FileImpl::Flush(*this);
        } else {
            if (count) FileImpl::Flush(*this);
            step = FileImpl::WriteDirect(*this, src, n);
            position += step;
            if (size < position) size = position;
        }
        src += step;
        total += step;
        n -= step;
    } while (n);
    return total / items;
}

// (the typed reads and writes move sizeof bytes as one item each of size 1: a short one counts as failed)
bool File::Read(signed char& v) { return Read(&v, 1, 1) != 0; }
bool File::Read(unsigned char& v) { return Read(&v, 1, 1) != 0; }
bool File::Read(short& v) { return Read(&v, 1, 2) != 0; }
bool File::Read(unsigned short& v) { return Read(&v, 1, 2) != 0; }
bool File::Read(long& v) { return Read(&v, 1, 4) != 0; }
bool File::Read(unsigned long& v) { return Read(&v, 1, 4) != 0; }
bool File::Read(float& v) { return Read(&v, 1, 4) != 0; }
bool File::Read(__int64& v) { return Read(&v, 1, 8) != 0; }
bool File::Read(unsigned __int64& v) { return Read(&v, 1, 8) != 0; }
bool File::Read32(int& v)
{
    int t = 0;
    const bool r = Read(&t, 1, 4) != 0;
    v = t;
    return r;
}
bool File::Read32(unsigned int& v)
{
    unsigned int t = 0;
    const bool r = Read(&t, 1, 4) != 0;
    v = t;
    return r;
}
bool File::Write(signed char v) { return Write(&v, 1, 1) != 0; }
bool File::Write(unsigned char v) { return Write(&v, 1, 1) != 0; }
bool File::Write(short v) { return Write(&v, 1, 2) != 0; }
bool File::Write(unsigned short v) { return Write(&v, 1, 2) != 0; }
bool File::Write(long v) { return Write(&v, 1, 4) != 0; }
bool File::Write(unsigned long v) { return Write(&v, 1, 4) != 0; }
bool File::Write(float v) { return Write(&v, 1, 4) != 0; }
bool File::Write(__int64 v) { return Write(&v, 1, 8) != 0; }
bool File::Write(unsigned __int64 v) { return Write(&v, 1, 8) != 0; }
bool File::Write32(int v) { return Write(&v, 1, 4) != 0; }
bool File::Write32(unsigned int v) { return Write(&v, 1, 4) != 0; }

bool File::Read(PString& s)
{
    FileStringReader r(*this);
    return s.Read(r);
}
bool File::Read(AnsiString& s)
{
    FileStringReader r(*this);
    return s.Read(r);
}
bool File::Read(String& s) { return Read(static_cast<AnsiString&>(s)); }
bool File::Read(UnicodeString& s)
{
    FileStringReader r(*this);
    return s.Read(r);
}
bool File::Write(const PString& s)
{
    FileStringWriter w(*this);
    return s.Write(w);
}
bool File::Write(const AnsiString& s)
{
    FileStringWriter w(*this);
    return s.Write(w);
}
bool File::Write(const String& s) { return Write(static_cast<const AnsiString&>(s)); }
bool File::Write(const UnicodeString& s)
{
    FileStringWriter w(*this);
    return s.Write(w);
}

// (the original compares the items read with the item size, not the count)
void File::SafeRead(void* data, unsigned long itemSize, unsigned long items)
{
    if (Read(data, itemSize, items) != itemSize) {
        FileEx e(kHeader, 0xaf0);
        e.SetReasonf("SafeRead(buffer,%lu,%lu) failed to read all data", itemSize, items);
        throw FileEx(e);
    }
}

void File::SafeWrite(const void* data, unsigned long itemSize, unsigned long items)
{
    if (Write(data, itemSize, items) != itemSize) {
        FileEx e(kHeader, 0xb16);
        e.SetReasonf("SafeWrite(buffer,%lu,%lu) failed to write all data", itemSize, items);
        throw FileEx(e);
    }
}

unsigned long File::ReadLine(void* data, unsigned long bufSize, int enc)
{
    if (bufSize <= 1) return 0;
    if (!enc) enc = encoding;
    if (enc != 2) return FileImpl::ReadLineA(*this, static_cast<char*>(data), bufSize);
    wchar_t local[0x800];
    wchar_t* wide = local;
    if (bufSize > 0xffe) wide = static_cast<wchar_t*>(Alloc::New(bufSize + 2));
    unsigned long r = FileImpl::ReadLineW(*this, reinterpret_cast<unsigned char*>(wide), bufSize);
    if (r) {
        wide[r >> 1] = 0;
        const int n = static_cast<int>(Crt80::wcstombs(static_cast<char*>(data), wide, bufSize));
        if (n > 0) {
            static_cast<char*>(data)[n] = 0;
            r = static_cast<unsigned long>(n);
        } else {
            r = 0;
        }
    }
    if (wide != local && wide) Alloc::Delete(wide);
    return r;
}

unsigned long File::WriteLine(const char* text, int enc)
{
    if (!enc) enc = encoding;
    const size_t len = std::strlen(text);
    if (!len) return 0;
    if (enc != 2) return Write(text, static_cast<unsigned long>(len), 1);
    wchar_t local[0x800];
    wchar_t* wide = local;
    if (len * 2 + 2 > 0x1000) {
        const unsigned long long bytes = (static_cast<unsigned long long>(len) + 1) * 2;
        wide = static_cast<wchar_t*>(Alloc::New(bytes >> 32 ? 0xffffffffu : static_cast<unsigned int>(bytes)));
    }
    const size_t n = Crt80::mbstowcs(wide, text, len);
    wide[n] = 0;
    const unsigned long r = Write(wide, static_cast<unsigned long>(n * 2), 1);
    if (wide != local && wide) Alloc::Delete(wide);
    return r;
}

void File::DetectEncoding()
{
    const unsigned long at = position;
    unsigned short bom;
    if (Read(&bom, 2, 1) == 2 && bom == 0xfeff) {
        encoding = 2;
        return;
    }
    Seek(static_cast<long>(at), FILE_SEEK_SET);
    encoding = 1;
}

// the position in the data; a compressed file reads the chunk it lands in (from the start when it is behind)
unsigned long File::Seek(long offset, FileSeek whence)
{
    if ((mode & 0xf) == 2) throw FileEx(kSource, 0xa07, "Seek() Can't seek writing compressed file");
    FileImpl::Flush(*this);
    const bool compressed = (mode & 0xf0) == 0x30;
    // the start of the buffered chunk; with nothing buffered the original takes the old position itself
    unsigned long chunkStart = 0;
    if (compressed) chunkStart = count ? (position - static_cast<unsigned long>(cur - buffer)) & ~(kChunk - 1) : position;
    if (whence == FILE_SEEK_CUR)
        position += offset;
    else if (whence == FILE_SEEK_END) {
        Size();
        position = size - offset;
    } else if (whence == FILE_SEEK_SET)
        position = offset;
    else
        return static_cast<unsigned long>(-1);
    if (!compressed || position >= size) {
        cur = buffer;
        rawPos = dataStart + position;
        count = 0;
        return position;
    }
    unsigned long raw, at;
    if (position < chunkStart) {
        raw = dataStart;
        at = 0;
    } else {
        if (position - chunkStart <= count) {
            cur = buffer + (position - chunkStart);
            return position;
        }
        raw = rawPos;
        at = count + chunkStart;
    }
    // walk the chunk headers to the one holding the position
    unsigned long header, n;
    for (;;) {
        header = 0;
        FileImpl::RawRead(*this, &header, 2, raw);
        header &= 0xffff;
        unsigned long unpacked;
        if (header & 0x8000) {
            n = header & 0x7fff;
            unpacked = n;
        } else {
            n = header;
            unpacked = kChunk;
        }
        if (unpacked + at > position) break;
        raw += n + 2;
        at += unpacked;
    }
    rawPos = raw + 2;
    unsigned long got;
    if (header & 0x8000) {
        got = static_cast<unsigned long>(FileImpl::RawReadOn(*this, buffer, n));
    } else {
        unsigned char packed[kChunk];
        if (static_cast<unsigned long>(FileImpl::RawReadOn(*this, packed, n)) != n) {
            size = 0;
            return static_cast<unsigned long>(-1);
        }
        got = BPE::Decomp(buffer, packed, n);
    }
    count = got;
    cur = buffer - at + position;
    return position;
}

unsigned long File::SeekSet(long offset) { return Seek(offset, FILE_SEEK_SET); }
unsigned long File::SeekCur(long offset) { return Seek(offset, FILE_SEEK_CUR); }
unsigned long File::SeekEnd(long offset) { return Seek(offset, FILE_SEEK_END); }

// 0x100218b0: skips 8-byte {id, size} blocks to the one with the id and reads up to `max` bytes of it (straight
// from the descriptor); the size read (clipped to max when data is given), 0 when there is none
unsigned long File::ReadBlock(const char* id, void* data, unsigned long max)
{
    if ((flags & 2) && sub) {
        sub->Lock();
        Crt80::_lseek(fd, static_cast<long>(rawPos), 0);
    }
    struct {
        char id[4];
        unsigned long size;
    } block;
    block.size = 0;
    bool found = false;
    bool more = Crt80::_read(fd, &block, 8) == 8;
    while (more && !found) {
        if (std::strncmp(block.id, id, 4) == 0) {
            found = true;
            if (data) {
                long skip = 0;
                if (block.size > max) {
                    skip = static_cast<long>(block.size - max);
                    block.size = max;
                }
                Crt80::_read(fd, data, block.size);
                if (skip) Crt80::_lseek(fd, skip, 1);
            }
        } else {
            more = Crt80::_read(fd, &block, 8) == 8;
        }
    }
    if ((flags & 2) && sub) {
        rawPos = static_cast<unsigned long>(Crt80::_tell(fd));
        sub->Unlock();
    }
    return found ? block.size : 0;
}

// 0x10021a30: an 8-byte {id, size} header and the data, straight to the descriptor (the bytes of data written)
unsigned long File::WriteBlock(const char* id, const void* data, unsigned long n)
{
    struct {
        unsigned long id;
        unsigned long size;
    } block = {*reinterpret_cast<const unsigned long*>(id), n};
    if (Crt80::_write(fd, &block, 8) != 8) return 0;
    return static_cast<unsigned long>(Crt80::_write(fd, data, n));
}

bool File::GetStamp(Time& t)
{
    struct {
        unsigned char head[0x20];
        long mtime;
        unsigned char tail[0x0c];
    } st;
    if (Crt80::_fstat64i32(fd, &st) != 0) return false;
    t.seconds = st.mtime;
    return true;
}

bool File::SetStamp(Time& t)
{
    const __int64 times[2] = {t.seconds, t.seconds};
    return Crt80::_futime64(fd, const_cast<__int64*>(times)) == 0;
}

void File::SetStamp(Time& t, const char* p)
{
    struct {
        unsigned char head[0x18];
        __int64 atime;
        long mtime;
        unsigned char tail[0x0c];
    } st;
    if (Crt80::_stat64i32(p, &st) != 0) {
        FileEx e(kSource, 0xddd);
        e.SetReasonf("SetStamp() failure");
        throw FileEx(e);
    }
    __int64 times[2] = {st.atime, t.seconds};
    if (Crt80::_utime64(p, times) != 0) {
        FileEx e(kSource, 0xdee);
        e.SetReasonf("SetStamp() failure");
        throw FileEx(e);
    }
}

bool File::Cd(const char* p) { return Crt80::_chdir(p) != -1; }
bool File::GetCwd(Path& p) { return Crt80::_getcwd(p.text, 0x104) != nullptr; }
bool File::Rd(const char* p) { return Crt80::_rmdir(p) != -1; }

}  // namespace Jet
