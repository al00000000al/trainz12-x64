// Unit U10a of KernelStandard_7c: MemoryFile, a file's contents in memory read like a file (0x10003f10..0x10004690,
// 0x10031d90..0x10032710). Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U10a").
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <cstring>

namespace Jet {

namespace {

// releases owned data and empties (the start of every Open)
void Reset(MemoryFile& m)
{
    if (m.owned && m.start) Alloc::Delete(const_cast<char*>(m.start));
    m.owned = false;
    m.cur = nullptr;
    m.start = nullptr;
    m.size = 0;
    m.name = "";
}

template <class T>
T Take(const MemoryFile& m)
{
    if (m.start + m.size < m.cur + sizeof(T)) return T();
    T v;
    std::memcpy(&v, m.cur, sizeof(T));
    m.cur += sizeof(T);
    return v;
}

}  // namespace

MemoryFile::MemoryFile() : cur(nullptr), start(nullptr), size(0), owned(false) {}

MemoryFile::MemoryFile(const char* n, unsigned int s) : cur(nullptr), start(nullptr), owned(false)
{
    Open(s);
    name = n;
}

MemoryFile::MemoryFile(const void* data, unsigned int s, bool unicode, int encoding)
    : cur(nullptr), start(nullptr), owned(false)
{
    Open(data, s, unicode, encoding);
}

MemoryFile::MemoryFile(File& file, unsigned int s, bool unicode, int encoding)
    : cur(nullptr), start(nullptr), owned(false)
{
    Open(file, s, unicode, encoding);
}

MemoryFile::MemoryFile(File& file, bool unicode, int encoding) : cur(nullptr), start(nullptr), owned(false)
{
    const unsigned long at = file.Tell();
    Open(file, file.Size() - at, unicode, encoding);
}

MemoryFile::MemoryFile(File& file, const String& key) : cur(nullptr), start(nullptr), owned(false)
{
    Open(file, key);
}

MemoryFile::MemoryFile(const MemoryFile& o) : cur(o.cur), start(o.start), size(o.size), owned(o.owned), name(o.name)
{
}

MemoryFile& MemoryFile::operator=(const MemoryFile& o)
{
    cur = o.cur;
    start = o.start;
    size = o.size;
    owned = o.owned;
    name = o.name;
    return *this;
}

MemoryFile::~MemoryFile()
{
    if (owned && start) Alloc::Delete(const_cast<char*>(start));
    owned = false;
    cur = nullptr;
    start = nullptr;
    size = 0;
    name = "";
}

void MemoryFile::Close() { Reset(*this); }

// a file opened, read in from its position, closed
File::Result MemoryFile::Open(const char* path, unsigned long mode, bool unicode, int encoding)
{
    File file;
    Reset(*this);
    File::Result r = file.Open(path, mode, 0);
    if (r.code == 0) {
        const unsigned long at = file.Tell();
        if (!Open(file, file.Size() - at, unicode, encoding)) r = File::Result(1);
    }
    return r;
}

File::Result MemoryFile::Open(const PString& base, const char* path, unsigned long mode, bool unicode, int encoding)
{
    File file;
    Reset(*this);
    File::Result r = file.Open(base, path, mode, 0);
    if (r.code == 0) {
        const unsigned long at = file.Tell();
        if (!Open(file, file.Size() - at, unicode, encoding)) r = File::Result(1);
    }
    return r;
}

bool MemoryFile::Open(File& file, unsigned int s, bool unicode, int encoding)
{
    Reset(*this);
    name = file.Name();
    return ReadData(file, s, unicode, encoding);
}

bool MemoryFile::Open(File& file, bool unicode, int encoding)
{
    const unsigned long at = file.Tell();
    return Open(file, file.Size() - at, unicode, encoding);
}

bool MemoryFile::Open(File& file, const String& key)
{
    Reset(*this);
    name = file.Name();
    return ReadData(file, key);
}

// over the caller's data; UTF-16 data was meant to be converted, but the original clears the first character
// before wcstombs (it indexes with the size it has just zeroed), so it always ends up plain over the caller's data
// (the size without a byte-order mark), leaking the conversion buffer
bool MemoryFile::Open(const void* data, unsigned int s, bool unicode, int encoding)
{
    Reset(*this);
    if (!s) return false;
    unsigned int n = s;
    if (unicode) {
        const void* src = data;
        bool convert = false;
        if (encoding == 0) {
            if (n >= 2 && *static_cast<const unsigned short*>(data) == 0xfeff) {
                src = static_cast<const char*>(data) + 2;
                n -= 2;
                if (!n) return false;
                convert = true;
            }
        } else if (encoding == 2) {
            convert = true;
        }
        if (convert) {
            wchar_t* const wide = static_cast<wchar_t*>(Alloc::New(n + 2));
            std::memcpy(wide, src, n);
            *reinterpret_cast<unsigned short*>(reinterpret_cast<char*>(wide) + size) = 0;  // size is 0 here
            size = n + 1;
            start = static_cast<char*>(Alloc::New(n + 1));
            size = static_cast<unsigned int>(Crt80::wcstombs(const_cast<char*>(start), wide, size));
            Alloc::Delete(wide);
            if (size) {
                cur = start;
                owned = true;
                return true;
            }
            Reset(*this);
        }
    }
    size = n;
    cur = static_cast<const char*>(data);
    start = static_cast<const char*>(data);
    owned = false;
    return true;
}

void MemoryFile::Open(unsigned int s)
{
    Reset(*this);
    if (s) {
        start = static_cast<char*>(Alloc::New(s));
        cur = start;
    }
    size = s;
    owned = true;
}

// size bytes from the file; UTF-16 text (a byte-order mark when encoding is 0, or encoding 2) is read as lines
// converted to ANSI. Without a mark the original seeks the file to 0, not back where it was.
bool MemoryFile::ReadData(File& file, unsigned int s, bool unicode, int encoding)
{
    if (!s) return false;
    if (unicode) {
        bool lines = false;
        if (encoding == 0) {
            const long back = static_cast<long>(cur - start);
            unsigned short bom;
            if (file.Read(&bom, 2, 1) == 2 && bom == 0xfeff) {
                s -= 2;
                if (!s) return false;
                lines = true;
            } else {
                file.Seek(back, FILE_SEEK_SET);
            }
        } else if (encoding == 2) {
            lines = true;
        }
        if (lines) {
            size = s + 1;
            start = static_cast<char*>(Alloc::New(size));
            cur = start;
            char* p = const_cast<char*>(cur);
            unsigned long got = file.ReadLine(p, size, 2);
            while (got) {
                p += std::strlen(p);
                cur = p;
                got = file.ReadLine(p, size - static_cast<unsigned int>(p - start), 2);
            }
            size = static_cast<unsigned int>(cur - start);
            if (size) {
                cur = start;
                owned = true;
                return true;
            }
            Close();
        }
    }
    char* data;
    try {
        data = static_cast<char*>(Alloc::New(s));
    } catch (...) {
        size = 0;
        Close();
        return false;
    }
    start = cur = data;
    size = file.Read(data, s, 1);
    if (size != s) {
        Close();
        return false;
    }
    owned = true;
    return true;
}

// through an IOSlaveResource over the file (scrambled with the key)
bool MemoryFile::ReadData(File& file, const String& key)
{
    IOSlaveFile slave(&file);
    IOSlaveResource resource(&slave, key, false);
    const unsigned long n = resource.GetSize();
    if (!n) return false;
    try {
        start = cur = static_cast<char*>(Alloc::New(n));
    } catch (...) {
        size = 0;
        Close();
        return false;
    }
    owned = true;
    size = resource.ReadData(const_cast<char*>(start), n);
    if (size == n) return true;
    Close();
    return false;
}

unsigned int MemoryFile::Read(void* dst, unsigned int itemSize, unsigned int items) const
{
    const unsigned int n = itemSize * items;
    if (start + size < cur + n) return 0;
    std::memcpy(dst, cur, n);
    cur += n;
    return n;
}

void MemoryFile::Read(signed char& v) const { v = Take<signed char>(*this); }
void MemoryFile::Read(unsigned char& v) const { v = Take<unsigned char>(*this); }
void MemoryFile::Read(short& v) const { v = Take<short>(*this); }
void MemoryFile::Read(unsigned short& v) const { v = Take<unsigned short>(*this); }
void MemoryFile::Read(long& v) const { v = Take<long>(*this); }
void MemoryFile::Read(unsigned long& v) const { v = Take<unsigned long>(*this); }
void MemoryFile::Read(float& v) const { v = Take<float>(*this); }
signed char MemoryFile::Readsbyte() const { return Take<signed char>(*this); }
unsigned char MemoryFile::Readubyte() const { return Take<unsigned char>(*this); }
short MemoryFile::Readsword() const { return Take<short>(*this); }
unsigned short MemoryFile::Readuword() const { return Take<unsigned short>(*this); }
long MemoryFile::Readsdword() const { return Take<long>(*this); }
unsigned long MemoryFile::Readudword() const { return Take<unsigned long>(*this); }
float MemoryFile::Readfloat() const { return Take<float>(*this); }

unsigned char MemoryFile::Peek() const
{
    if (start + size < cur + 1) return 0;
    return static_cast<unsigned char>(*cur);
}

// {u32 length (bit 30: UTF-16 bytes), text}; a length past the whole size reads nothing and leaves s
void MemoryFile::Read(PString& s, bool) const
{
    unsigned int n = Take<unsigned int>(*this);
    const bool wide = (n & 0x40000000) != 0;
    if (wide) n &= 0xbfffffff;
    if (n > size) return;
    PStringCache* const cache = PString::Cache();
    if (s.node) {
        // the original's inlined release: the node is not destroyed here
        cache->Lock();
        if (s.node->refs == 1) --cache->usedNodes;
        --cache->liveRefs;
        --s.node->refs;
        cache->Unlock();
    }
    if (wide) {
        char* const text = static_cast<char*>(Alloc::New(n + 2));
        if (cur + n <= start + size) {
            std::memcpy(text, cur, n);
            cur += n;
        }
        *reinterpret_cast<unsigned short*>(text + (n & ~1u)) = 0;
        s.node = cache->CreateUnicode(reinterpret_cast<const __wchar_t*>(text), -1);
        Alloc::Delete(text);
    } else {
        char* const text = static_cast<char*>(Alloc::New(n + 1));
        if (cur + n <= start + size) {
            std::memcpy(text, cur, n);
            cur += n;
        }
        text[n] = 0;
        s.node = cache->CreateANSI(reinterpret_cast<const unsigned char*>(text), -1);
        Alloc::Delete(text);
    }
}

void MemoryFile::Write(const void* src, unsigned int n)
{
    if (cur + n > start + size) return;
    std::memcpy(const_cast<char*>(cur), src, n);
    cur += n;
}

unsigned int MemoryFile::Seek(int offset, FileSeek whence) const
{
    if (whence == FILE_SEEK_SET)
        cur = start + offset;
    else if (whence == FILE_SEEK_CUR)
        cur += offset;
    else if (whence == FILE_SEEK_END)
        cur = start + (size - offset);
    else
        return static_cast<unsigned int>(cur - start);
    if (cur < start) cur = start;
    return static_cast<unsigned int>(cur - start);
}

unsigned int MemoryFile::SeekSet(int offset) const
{
    cur = start + offset;
    if (cur < start) cur = start;
    return static_cast<unsigned int>(cur - start);
}

unsigned int MemoryFile::SeekCur(int offset) const
{
    cur += offset;
    if (cur < start) cur = start;
    return static_cast<unsigned int>(cur - start);
}

unsigned int MemoryFile::SeekEnd(int offset) const
{
    cur = start + (size - offset);
    if (cur < start) cur = start;
    return static_cast<unsigned int>(cur - start);
}

unsigned int MemoryFile::Tell() const { return static_cast<unsigned int>(cur - start); }
unsigned int MemoryFile::Size() const { return size; }
bool MemoryFile::Eof() const { return static_cast<unsigned int>(cur - start) >= size; }
const void* MemoryFile::Data() const { return start; }
const void* MemoryFile::CurrentPosition() const { return cur; }
const char* MemoryFile::Name() const { return name; }
void MemoryFile::SetName(const char* n) { name = n; }
void MemoryFile::SetName(const String& n) { name = n; }

}  // namespace Jet
