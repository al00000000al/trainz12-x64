// Unit U10b of KernelStandard_7c, part 2: the archives (FileSubArchive, 0x100251f0..0x10028d20 without the
// directory and link FileSubs), the read cursors File::Find drives, SetDefaultBuildVersion / ProductCode and the
// temp-name counter.
// Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U10b part 2").
#include "FileSub.h"

#include "Crt80.h"

#include <jet/Math.h>

#include <cstdio>
#include <cstring>
#include <deque>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Jet {

namespace {

const char kArchiveSource[] = "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/KernelStandard/FileSubArchive.cpp";

constexpr unsigned long kChunk = 0x4000;
constexpr unsigned long kArc0 = 0x41524330;  // 'ARC0'
constexpr unsigned long kEntry = sizeof(File::Entry);  // 0x120
constexpr unsigned long kIndexEntry = 8 + kEntry;      // {offset, size, Entry}
constexpr int O_BINARY_ = 0x8000, O_RDWR_ = 2, O_CREAT_ = 0x100;
constexpr int SH_DENYRW_ = 0x10, SH_DENYNO_ = 0x40;

unsigned long defaultBuild = 0;    // 0x100adbd0
unsigned long defaultProduct = 0;  // 0x100adbd4
unsigned long tempCounter = 0;     // 0x100adbc4

bool IsSep(char c) { return c == '\\' || c == '/'; }

// 0x10026f60: '\\' to '/', A-Z to lower case, one trailing '/' cut
void NormalizeDir(char* p)
{
    const int n = static_cast<int>(std::strlen(p));
    if (n < 1) return;
    for (int i = 0; i < n; ++i) {
        if (p[i] == '\\')
            p[i] = '/';
        else if (static_cast<unsigned>(p[i] - 'A') < 26)
            p[i] = static_cast<char>(p[i] + 0x20);
    }
    if (p[n - 1] == '/') p[n - 1] = 0;
}

// 0x10026fc0: a name without '.', '*' or '?' in its last component (so maybe a directory)
bool LooksLikeDir(const char* p)
{
    const int n = static_cast<int>(std::strlen(p));
    if (n < 1) return false;
    for (int i = n - 1; i >= 0; --i) {
        const char c = p[i];
        if (c == '/' || c == '\\') return true;
        if (c == '.' || c == '*' || c == '?') return false;
    }
    return true;
}

// 0x100218b0: the first chunk of that id from the descriptor's position (a different id: the next 8 bytes are read
// as the next header, the chunk is not skipped); up to `max` bytes of it read into buf, the rest skipped. Its size
// (at most max), 0 when not found.
unsigned long ReadChunk(int fd, const char* id, void* buf, unsigned long max)
{
    struct {
        char id[4];
        unsigned long size;
    } h;
    h.size = 0;
    bool found = false;
    bool ok = Crt80::_read(fd, &h, 8) == 8;
    while (ok && !found) {
        if (std::strncmp(h.id, id, 4) == 0) {
            found = true;
            if (buf) {
                unsigned long extra = 0;
                if (h.size > max) {
                    extra = h.size - max;
                    h.size = max;
                }
                Crt80::_read(fd, buf, h.size);
                if (extra) Crt80::_lseek(fd, static_cast<long>(extra), 1);
            }
        } else {
            ok = Crt80::_read(fd, &h, 8) == 8;
        }
    }
    return found ? h.size : 0;
}

void WriteChunk(int fd, const char* id, const void* data, unsigned long size)  // 0x10021a30
{
    struct {
        char id[4];
        unsigned long size;
    } h;
    std::memcpy(h.id, id, 4);
    h.size = size;
    if (Crt80::_write(fd, &h, 8) == 8) Crt80::_write(fd, data, size);
}

struct Locked {
    explicit Locked(CRITICAL_SECTION& c) : cs(c) { EnterCriticalSection(&cs); }
    ~Locked() { LeaveCriticalSection(&cs); }
    CRITICAL_SECTION& cs;
};

}  // namespace

// a file of the archive (PackedHeapMP 0x100ae180 in the original: {offset, size, Entry size, length, crc,
// attrib, name}); the name in lower case
struct ArcRecord {
    unsigned long offset;  // the data
    unsigned long size;    // the bytes stored
    unsigned long plain;   // Entry::size
    unsigned long length;  // Entry::length
    unsigned long crc;
    unsigned short attrib;
    std::string name;
};

// the record ring: chunks of 61 (0x100 bytes in the original) {next, prev, count, items}
struct ArcChunk {
    ArcChunk* next;
    ArcChunk* prev;
    unsigned long count;
    ArcRecord* items[61];
};

// the name hash (401 buckets in the original, each a ring of records): the first record of a name is found
struct FileSubArchive::Hash {
    std::unordered_map<std::string, std::vector<ArcRecord*>> names;
    std::vector<ArcRecord*> gone;  // unlinked records (still reachable through a name that was not erased)
};
// the directories known from the directory (a set of PStrings in the original, by their text)
struct FileSubArchive::Dirs {
    std::set<std::string> names;
};
// the read cursors (a ring of chunks of 4 in the original, never shrunk)
struct FileSubArchive::Cursors {
    std::deque<Cursor> items;
};

namespace {

void Advance(FileSubArchive::Place& p)  // 0x10027ae0
{
    if (!p.item) return;
    ++p.item;
    if (p.item >= p.chunk->items + p.chunk->count) {
        p.chunk = p.chunk->next;
        p.item = p.chunk == p.head ? nullptr : p.chunk->items;
    }
}

// 0x10028950: the item goes, the ones after it in its chunk move down; an emptied chunk is freed
void Erase(ArcChunk*& ring, FileSubArchive::Place p)
{
    if (!ring || !p.item) return;
    ArcChunk* const c = p.chunk;
    ArcRecord** const last = c->items + c->count - 1;
    if (p.item < last) std::memmove(p.item, p.item + 1, (last - p.item) * sizeof(ArcRecord*));
    if (c->count != 1) {
        --c->count;
        return;
    }
    if (c == ring && c->next == ring) {
        delete c;
        ring = nullptr;
        return;
    }
    c->prev->next = c->next;
    c->next->prev = c->prev;
    if (c == ring) ring = c->next;
    delete c;
}

void Append(ArcChunk*& ring, ArcRecord* r)  // 0x100288d0
{
    ArcChunk* last;
    if (!ring) {
        last = new ArcChunk;
        last->count = 0;
        last->next = last->prev = last;
        ring = last;
    } else {
        last = ring->prev;
        if (last->count == 61) {
            ArcChunk* const c = new ArcChunk;
            c->count = 0;
            c->next = ring;
            c->prev = last;
            ring->prev = c;
            last->next = c;
            last = c;
        }
    }
    last->items[last->count++] = r;
}

// 0x100279b0: an entry from a record: the times and the rest zero
void Fill(File::Entry& e, const ArcRecord& r)
{
    const File::Entry none;
    e = none;
    *reinterpret_cast<File::Path*>(e.name) = r.name.c_str();
    e.size = r.plain;
    e.length = r.length;
    e.crc = r.crc;
    e.attrib = r.attrib;
}

}  // namespace

// ---- the archive

FileSubArchive::FileSubArchive(const char* p, bool writable)
    : path(p), flags(0), fd(-1), readOnly(!writable), hash(new Hash), dirs(new Dirs), cursors(new Cursors)
{
    dirCache.text[0] = 0;
    InitializeCriticalSection(&fileLock);
    InitializeCriticalSection(&listLock);
    info = Info{0, 0, 0, 0, defaultBuild, defaultProduct, 0, 0};
    fd = Crt80::sopen(p, readOnly ? O_BINARY_ : O_BINARY_ | O_RDWR_, SH_DENYNO_, 0);
    if (fd == -1) {
        if (readOnly) return;
        fd = Crt80::sopen(p, O_BINARY_ | O_CREAT_ | O_RDWR_, SH_DENYNO_, 0x180);
        if (fd == -1) {
            delete hash;
            delete dirs;
            delete cursors;
            DeleteCriticalSection(&listLock);
            DeleteCriticalSection(&fileLock);
            throw FileEx(kArchiveSource, 0xfe, "Substitute() Open new archive failed");
        }
        info.magic = kArc0;
        info.version = 0x102;
        Crt80::_write(fd, "ARCH", 4);
        WriteChunk(fd, "INFO", &info, sizeof(info));
        flags |= 3;
        return;
    }
    char magic[4];
    if (Crt80::_read(fd, magic, 4) != 4) return;  // (a file of less than 4 bytes stays open, without files)
    if (std::strncmp(magic, "ARCH", 4) == 0 && ReadChunk(fd, "INFO", &info, sizeof(info))) {
        const bool ok = info.magic == kArc0 && info.version >= 0x101 && info.version <= 0x102;
        if (ReadIndex()) {
            flags = (flags & ~2) | 1;
        } else {
            flags &= ~3;
            info.index = 0;
        }
        if (ok) return;
    }
    Crt80::_close(fd);
    fd = -1;
    info.version = 0;
}

// 0x10025670: a writable archive whose directory changed rewrites it
FileSubArchive::~FileSubArchive()
{
    if (hash) {
        if (!readOnly && (flags & 1) && (flags & 2)) WriteIndex();
        for (ArcChunk* c = ring; c;) {
            for (unsigned long i = 0; i < c->count; ++i) delete c->items[i];
            ArcChunk* const n = c->next;
            delete c;
            c = n == ring ? nullptr : n;
        }
        ring = nullptr;
        for (ArcRecord* r : hash->gone) delete r;
        delete hash;
        delete dirs;
        delete cursors;
        hash = nullptr;
    }
    if (fd != -1) {
        Crt80::_close(fd);
        fd = -1;
    }
    DeleteCriticalSection(&listLock);
    DeleteCriticalSection(&fileLock);
}

void FileSubArchive::SetDefaultBuildVersion(unsigned long v) { defaultBuild = v; }  // 0x10027990
void FileSubArchive::SetDefaultProductCode(unsigned long v) { defaultProduct = v; }  // 0x100279a0

unsigned long FileSubArchive::RecordCount() const
{
    unsigned long n = 0;
    if (ArcChunk* c = ring) {
        do {
            n += c->count;
            c = c->next;
        } while (c != ring);
    }
    return n;
}

FileSubArchive::Place FileSubArchive::First() const
{
    return ring ? Place{ring->items, ring, ring} : Place{nullptr, nullptr, nullptr};
}

FileSubArchive::Place FileSubArchive::Last() const
{
    if (!ring) return Place{nullptr, nullptr, nullptr};
    ArcChunk* const c = ring->prev;
    return Place{c->items + c->count - 1, ring, c};
}

ArcRecord* FileSubArchive::NewRecord(const File::Entry& e, unsigned long offset, unsigned long size)
{
    File::Path name;
    name = *reinterpret_cast<const File::Path*>(e.name);
    name.ToLower();
    return new ArcRecord{offset, size, e.size, e.length, e.crc, e.attrib, name.text};
}

void FileSubArchive::Insert(ArcRecord* r)  // 0x10028680 and 0x100288d0
{
    hash->names[r->name].push_back(r);
    Append(ring, r);
}

void FileSubArchive::AddRecord(const File::Entry& e, unsigned long offset, unsigned long size)
{
    ArcRecord* const r = NewRecord(e, offset, size);
    if (r->name.empty()) {
        delete r;
        return;
    }
    Insert(r);
    info.count = RecordCount();
}

// each directory above the file that is not known yet; the file's own directory is the cached one then
void FileSubArchive::AddDirs(const char* name)
{
    File::Path p(*reinterpret_cast<const File::Path*>(name));
    NormalizeDir(p.text);
    char* cut = std::strrchr(p.text, '/');
    if (!cut) return;
    *cut = 0;
    const File::Path keep(p);
    do {
        if (IsDir(p.text, false)) break;
        dirs->names.insert(p.text);
        cut = std::strrchr(p.text, '/');
        if (!cut) {
            p.text[0] = 0;
            break;
        }
        *cut = 0;
    } while (p.text[0]);
    dirCache = keep;
}

bool FileSubArchive::IsDir(const char* p, bool normalize)
{
    File::Path n;
    const char* key = p;
    if (normalize) {
        n = p;
        NormalizeDir(n.text);
        key = n.text;
    }
    Locked lock(listLock);
    if (std::strcmp(dirCache.text, key) == 0) return true;
    const bool found = dirs->names.count(key) != 0;
    if (found) dirCache = key;
    return found;
}

// the directory read whole, in blocks of up to 0x1000 entries; a name without a NUL ends it (still true)
bool FileSubArchive::ReadIndex()
{
    if (!info.index || !info.count) return false;
    Locked a(fileLock), b(listLock);
    Crt80::_lseek(fd, static_cast<long>(info.index), 0);
    char h[8];
    if (Crt80::_read(fd, h, 8) != 8 || std::strncmp(h, "IDX ", 4) != 0) return false;
    const unsigned long total = info.count;
    std::vector<unsigned char> block;
    for (unsigned long done = 0; done < total;) {
        const unsigned long n = total - done > 0x1000 ? 0x1000 : total - done;
        if (block.empty()) block.resize(n * kIndexEntry);
        const unsigned long bytes = n * kIndexEntry;
        if (static_cast<unsigned long>(Crt80::_read(fd, block.data(), bytes)) != bytes) return false;
        for (unsigned long i = 0; i < n; ++i) {
            unsigned char* const at = block.data() + i * kIndexEntry;
            const File::Entry& e = *reinterpret_cast<const File::Entry*>(at + 8);
            if (!std::memchr(e.name, 0, sizeof(e.name))) return true;
            AddRecord(e, *reinterpret_cast<unsigned long*>(at), *reinterpret_cast<unsigned long*>(at + 4));
            AddDirs(e.name);
        }
        done += n;
    }
    return true;
}

// at the old directory's place (a shorter one leaves the old one's tail after it), or at the end; then the header
// and INFO again
void FileSubArchive::WriteIndex()
{
    Locked a(fileLock), b(listLock);
    if (!info.index) {
        Crt80::_lseek(fd, 0, 2);
        info.index = static_cast<unsigned long>(Crt80::_tell(fd));
        info.count = 0;
    } else {
        Crt80::_lseek(fd, static_cast<long>(info.index), 0);
    }
    Crt80::_write(fd, "IDX ", 4);
    const long sizeAt = Crt80::_tell(fd);
    unsigned long zero = 0;
    Crt80::_write(fd, &zero, 4);
    info.count = RecordCount();
    // (the original's buffer is not cleared: the bytes after each name are whatever the heap held)
    std::vector<unsigned char> block(info.count * kIndexEntry + 1);  // (never a null buffer for _write)
    unsigned char* at = block.data();
    for (Place p = First(); p.item; Advance(p), at += kIndexEntry) {
        const ArcRecord& r = **p.item;
        *reinterpret_cast<unsigned long*>(at) = r.offset;
        *reinterpret_cast<unsigned long*>(at + 4) = r.size;
        Fill(*reinterpret_cast<File::Entry*>(at + 8), r);
    }
    const int bytes = static_cast<int>(info.count * kIndexEntry);
    const int written = Crt80::_write(fd, block.data(), static_cast<unsigned>(bytes));
    if (written != bytes) return;
    Crt80::_lseek(fd, sizeAt, 0);
    Crt80::_write(fd, &written, 4);
    Crt80::_lseek(fd, 0, 0);
    Crt80::_write(fd, "ARCH", 4);
    WriteChunk(fd, "INFO", &info, sizeof(info));
}

// ---- the read cursors

long FileSubArchive::OpenCursor()  // 0x100268c0: a free one, else a new one; at offset 4 and the first record
{
    Locked lock(listLock);
    long i = 0;
    for (Cursor& c : cursors->items) {
        if (c.pos == -1) {
            c.pos = 4;
            c.at = First();
            return i;
        }
        ++i;
    }
    cursors->items.push_back(Cursor{4, First()});
    return i;
}

void FileSubArchive::CloseCursor(long cursor)  // 0x100269f0 (no lock)
{
    if (cursor >= 0 && static_cast<unsigned long>(cursor) < cursors->items.size()) cursors->items[cursor].pos = -1;
}

// 0x10025b10: the next file (or directory: attrib 0x11, the name cut after it) matching the pattern. By name: the
// hash; else the records from the cursor's place. Not found and the archive is not a read-only one with a directory:
// the file is scanned on from the cursor's offset, each "FIL " met becoming a record (cursors at the end of the ring
// then point at it), until one matches. `advance` moves the cursor past the data found.
bool FileSubArchive::Next(const char* pattern, long cursor, File::Entry* entry, bool advance, bool byName)
{
    const bool live = !(readOnly && (flags & 1));
    unsigned long size = 0;
    EnterCriticalSection(&listLock);
    if (cursor < 0 || static_cast<unsigned long>(cursor) >= cursors->items.size() || cursors->items[cursor].pos == -1) {
        LeaveCriticalSection(&listLock);
        return false;
    }
    Cursor& c = cursors->items[cursor];
    bool held = true;
    if (!live) {
        held = false;
        LeaveCriticalSection(&listLock);
    }
    bool found = false;
    if (ring && byName) {
        File::Path p(pattern);
        p.ToLower();
        p.Normalize();
        auto at = hash->names.find(p.text);
        if (at != hash->names.end() && !at->second.empty()) {
            const ArcRecord& r = *at->second.front();
            c.pos = static_cast<long>(r.offset);
            size = r.size;
            found = true;
            Fill(*entry, r);
            c.at = Place{nullptr, nullptr, nullptr};
        } else {
            c.at = Last();
            c.pos = static_cast<long>((*c.at.item)->size + (*c.at.item)->offset);
        }
    } else {
        while (c.at.item) {
            const ArcRecord& r = **c.at.item;
            const char* rest;
            if (File::Path::IsMatch(r.name.c_str(), pattern, &rest)) {
                c.pos = static_cast<long>(r.offset);
                size = r.size;
                found = true;
                Fill(*entry, r);
                Advance(c.at);
                break;
            }
            if (rest && rest > r.name.c_str()) {
                c.pos = static_cast<long>(r.offset);
                size = r.size;
                found = true;
                Fill(*entry, r);
                entry->attrib |= 0x11;
                entry->name[rest - r.name.c_str()] = 0;
                Advance(c.at);
                break;
            }
            c.pos = static_cast<long>(r.offset + r.size);
            Advance(c.at);
        }
    }
    if (held) LeaveCriticalSection(&listLock);
    if (!found) {
        if (!live) return false;
        Locked a(fileLock), b(listLock);
        Crt80::_lseek(fd, c.pos, 0);
        bool first = true;
        struct {
            char id[4];
            unsigned long size;
        } h;
        for (;;) {
            if (Crt80::_read(fd, &h, 8) != 8) return false;
            c.pos += 8;
            if (std::strncmp(h.id, "FIL ", 4) == 0) {
                h.size -= Crt80::_read(fd, entry, kEntry);
                c.pos += kEntry;
                ArcRecord* const r = NewRecord(*entry, static_cast<unsigned long>(c.pos), h.size);
                Insert(r);
                info.count = RecordCount();
                if (first) {
                    long i = 0;
                    for (Cursor& o : cursors->items) {
                        if (i++ != cursor && o.pos != -1 && !o.at.item) {
                            o.pos = static_cast<long>(r->offset);
                            o.at = Last();
                        }
                    }
                    first = false;
                }
                const char* rest;
                bool match = File::Path::IsMatch(entry->name, pattern, &rest);
                if (!match && rest && rest > entry->name) {
                    entry->attrib |= 0x11;
                    *const_cast<char*>(rest) = 0;
                    match = true;
                }
                if (match) {
                    size = h.size;
                    break;
                }
            }
            Crt80::_lseek(fd, static_cast<long>(h.size), 1);
            c.pos += h.size;
        }
    }
    if (advance) c.pos += size;
    return true;
}

// ---- the FileSub slots

// 0x100258d0: written to a hidden temp file in the current directory, named in the File's path buffer right
// after the path (Close puts it in the archive)
bool FileSubArchive::OpenWrite(File* f, const char* p, int, int)
{
    const DWORD pid = GetCurrentProcessId();
    f->path.Make("", p);
    char* const temp = f->path.text + std::strlen(f->path.text) + 1;
    const unsigned long n = tempCounter++;
    std::sprintf(temp, "%08X.T%08X", pid, n);
    f->fd = Crt80::sopen(temp, O_BINARY_ | 0x301, SH_DENYRW_, 0x180);  // (create, truncate, write only)
    if (f->fd == -1) return false;
    SetFileAttributesA(temp, FILE_ATTRIBUTE_HIDDEN);
    f->sub = this;
    InterlockedIncrement(&refs);
    return true;
}

// 0x10025970: the archive's own descriptor, from the file's data; the cursor is left free
bool FileSubArchive::OpenRead(File* f, const char* p, int, int)
{
    if (IsSep(*p)) ++p;
    File::Entry e;
    const long cursor = OpenCursor();
    if (cursor < 0) return false;
    if (!Next(p, cursor, &e, false, true)) {
        CloseCursor(cursor);
        return false;
    }
    Cursor& c = cursors->items[cursor];
    f->fd = fd;
    f->path.Make("", e.name);
    f->flags |= 2;
    f->sub = this;
    InterlockedIncrement(&refs);
    f->size = (f->mode & 0xf0) == 0x20 ? e.length : e.size;
    f->dataStart = static_cast<unsigned long>(c.pos);
    f->rawPos = static_cast<unsigned long>(c.pos);
    c.pos = -1;
    return true;
}

// 0x100261b0: a written file goes in as a new "FIL " record (an old one of that name is unlinked first) at the
// directory's place or at the end: plain, or for mode 2 a "CFIL" stream of BPE chunks; the CRC is of the plain data.
// File::callback hears (done, stored) per chunk (stored 0 for a plain file).
void FileSubArchive::Close(File* f)
{
    InterlockedDecrement(&refs);
    const unsigned long kind = f->mode & 0xf;
    if (kind != 1 && kind != 2) return;
    File::Entry e;
    e.length = static_cast<unsigned long>(Crt80::_tell(f->fd));
    Crt80::_close(f->fd);
    const char* const temp = f->path.text + std::strlen(f->path.text) + 1;
    const int in = Crt80::open(temp, O_BINARY_, 0);
    std::strcpy(e.name, f->path.text);
    e.size = f->size;
    PString base;
    if (Exists(&base, f->path.text)) Unlink(f->path.text);
    Locked a(fileLock), b(listLock);
    if ((flags & 1) && info.index)
        Crt80::_lseek(fd, static_cast<long>(info.index), 0);
    else
        Crt80::_lseek(fd, 0, 2);
    Crt80::_write(fd, "FIL ", 4);
    const long sizeAt = Crt80::_tell(fd);
    unsigned long word = 0;
    Crt80::_write(fd, &word, 4);
    Crt80::_write(fd, &e, kEntry);
    unsigned long left = e.length;
    e.length = 0;
    e.crc = 0;
    e.attrib = 0;
    unsigned long n = 1, done = 0;
    const long dataAt = Crt80::_tell(fd);
    if (kind == 2) {
        Crt80::_write(fd, "CFIL", 4);
        word = 0x101;
        Crt80::_write(fd, &word, 4);
        Crt80::_write(fd, &e.size, 4);
        e.length += 0xc;
        e.attrib |= 0x80;
    }
    unsigned char raw[kChunk];
    while (left && n) {
        n = static_cast<unsigned long>(Crt80::_read(in, raw, left > kChunk ? kChunk : left));
        if (n == static_cast<unsigned long>(-1)) n = 0;
        e.crc = Crc::Calc(raw, n, e.crc);
        if (kind == 2) {
            done += n;
            left -= n;
            const unsigned long c = BPE::Comp(f->buffer, raw, n);
            const unsigned long header = c == n ? (c | 0x8000) & 0xffff : c & 0xffff;
            Crt80::_write(fd, &header, 2);
            e.length += 2 + c;
            n = static_cast<unsigned long>(Crt80::_write(fd, f->buffer, c));
            if (n == static_cast<unsigned long>(-1)) n = 0;
            if (File::callback) File::callback(done, e.length);
        } else {
            n = static_cast<unsigned long>(Crt80::_write(fd, raw, n));
            if (n == static_cast<unsigned long>(-1)) n = 0;
            done += n;
            e.length += n;
            left -= n;
            if (File::callback) File::callback(done, 0);
        }
    }
    if (flags & 1) {
        AddRecord(e, static_cast<unsigned long>(dataAt), e.length);
        flags |= 2;
        info.index = static_cast<unsigned long>(Crt80::_tell(fd));
    }
    Crt80::_lseek(fd, sizeAt, 0);
    word = e.length + kEntry;
    Crt80::_write(fd, &word, 4);
    Crt80::_write(fd, &e, kEntry);
    Crt80::_close(in);
    Crt80::_unlink(temp);
}

// 0x10026780: a file of that name, else a name that could be a directory and is one (the base is not used)
bool FileSubArchive::Exists(PString*, const char* p)
{
    const long cursor = OpenCursor();
    if (cursor < 0) return false;
    File::Entry e;
    bool found = Next(p, cursor, &e, false, true);
    if (!found && LooksLikeDir(p)) found = IsDir(p, true);
    CloseCursor(cursor);
    return found;
}

// 0x100276c0: the record's chunk becomes "FIL!"; the name leaves the hash (as given, in lower case) and the first
// record matching it leaves the ring
bool FileSubArchive::Unlink(const char* p)
{
    if (IsReadOnly()) return false;
    if (IsSep(*p)) ++p;
    const long cursor = OpenCursor();
    if (cursor < 0) return false;
    File::Entry e;
    if (!Next(p, cursor, &e, false, true)) {
        CloseCursor(cursor);
        return false;
    }
    {
        Locked a(fileLock), b(listLock);
        Crt80::_lseek(fd, cursors->items[cursor].pos - static_cast<long>(kIndexEntry), 0);
        Crt80::_write(fd, "FIL!", 4);
        File::Path lower(p);
        lower.ToLower();
        auto at = hash->names.find(lower.text);
        if (at != hash->names.end() && !at->second.empty()) at->second.erase(at->second.begin());
        for (Place it = First(); it.item; Advance(it)) {
            const char* rest;
            if (File::Path::IsMatch((*it.item)->name.c_str(), p, &rest)) {
                hash->gone.push_back(*it.item);
                Erase(ring, it);
                break;
            }
        }
        if (flags & 1) flags |= 2;
    }
    CloseCursor(cursor);
    return true;
}

// 0x10026a50: the entry's time (0 for a record from the directory; kept from the last record scanned when none
// matched)
bool FileSubArchive::GetStamp(File::Time* t, const char* p)
{
    const long cursor = OpenCursor();
    if (cursor < 0) return false;
    File::Entry e;
    const bool found = Next(p, cursor, &e, false, true);
    CloseCursor(cursor);
    t->seconds = static_cast<long>(e.time);
    return found;
}

void FileSubArchive::Lock() { EnterCriticalSection(&fileLock); }    // 0x10025240
void FileSubArchive::Unlock() { LeaveCriticalSection(&fileLock); }  // 0x10025260
bool FileSubArchive::IsPath(const char* p) { return std::strcmp(p, path.text) == 0; }  // 0x100251f0

// ---- what File::Find and the substitute statics use

long ArchiveCursor::Open(FileSub* a) { return static_cast<FileSubArchive*>(a)->OpenCursor(); }
void ArchiveCursor::Close(FileSub* a, long cursor) { static_cast<FileSubArchive*>(a)->CloseCursor(cursor); }
bool ArchiveCursor::Next(FileSub* a, const char* pattern, long cursor, File::Entry* entry)
{
    return static_cast<FileSubArchive*>(a)->Next(pattern, cursor, entry, true, false);
}

unsigned long Subst::ArchiveBuildVersion(FileSub* a) { return static_cast<FileSubArchive*>(a)->BuildVersion(); }
unsigned long Subst::ArchiveProductCode(FileSub* a) { return static_cast<FileSubArchive*>(a)->ProductCode(); }
unsigned long& Subst::TempCounter() { return tempCounter; }

}  // namespace Jet
