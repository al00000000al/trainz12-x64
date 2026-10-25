// The substitute system shared by File.cpp, FileSub.cpp and FileFind.cpp (unit U10b): a substitute is a name
// ("data" in "data:path") standing for a list of FileSubs, tried in order: directories (FileSubPath), archives
// (FileSubArchive, the .ja files) and links to other substitutes (FileSubLink).
// Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U10b").
#pragma once

#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include <windows.h>

namespace Jet {

struct SubChunk;

// The FileSub interface, slot for slot the original's (vftable 0x100554e4): the deleting destructor, then
// __cdecl virtuals with the object first.
class FileSub {
public:
    virtual ~FileSub() {}
    virtual void __cdecl Forget(const PString* name) {}  // 1 a substitute of that name goes away
    virtual bool __cdecl IsReadOnly() { return true; }   // 2
    virtual bool __cdecl IsReadable() { return false; }  // 3
    virtual unsigned long __cdecl Version() { return 0; }  // 4 the "CFIL" header version of its files
    virtual bool __cdecl IsType(int type) { return false; }  // 5 0 archive, 1 directory, 2 link
    virtual bool __cdecl OpenWrite(File* f, const char* path, int oflag, int pmode) { return false; }  // 6
    virtual bool __cdecl OpenRead(File* f, const char* path, int oflag, int pmode) { return false; }   // 7
    virtual void __cdecl Close(File* f);  // 8 (the base throws)
    virtual bool __cdecl Exists(PString* base, const char* path) { return false; }       // 9
    virtual bool __cdecl IsDirectory(PString* base, const char* path) { return false; }  // 10
    virtual bool __cdecl ExistsAs(PString* base, const char* path, File::Path* found);   // 11
    virtual bool __cdecl Unlink(const char* path) { return false; }                      // 12
    virtual bool __cdecl Rename(const char* from, const char* to) { return false; }      // 13
    virtual bool __cdecl Md(const char* path) { return true; }                           // 14
    virtual bool __cdecl TrueName(char* path) { return false; }                          // 15
    virtual bool __cdecl GetStamp(File::Time* t, const char* path) { return false; }     // 16
    virtual void __cdecl SetReadonly(const char* path, bool on) {}                       // 17
    virtual void __cdecl Collect(SubChunk** list) {}                                     // 18
    virtual void __cdecl Lock() {}                                                       // 19
    virtual void __cdecl Unlock() {}                                                     // 20
    virtual bool __cdecl IsPath(const char* path) { return false; }                      // 21

    FileSub* next = nullptr;  // +4 the next FileSub of its substitute (the original links the +4 fields)
    long refs = 0;            // +8 files open through it
    JET_HEAP_OBJECT
};

// a substitute: {next in the name-sorted list, FileSubs, their count, name}
class FileSubstitute {
public:
    FileSubstitute* next;
    FileSub* first;
    unsigned long count;
    PString name;
};

// the FileSubs collected through Collect (0x10028d20): chunks of 13 in a ring of Jet::Alloc blocks
struct SubChunk {
    SubChunk* next;
    SubChunk* prev;
    unsigned long count;
    FileSub* items[13];
};
JET_LAYOUT32(sizeof(SubChunk) == 0x40);
void SubChunkAdd(SubChunk** list, FileSub* sub);
void SubChunkFree(SubChunk** list);

namespace Subst {
FileSubstitute* Find(const PString& name);  // 0x1002a5f0
void Lock();
void Unlock();
// the list walks of the original (each under the lock)
bool OpenWrite(FileSubstitute* s, File* f, const char* path, int oflag, int pmode);  // 0x10029b60
bool OpenRead(FileSubstitute* s, File* f, const char* path, int oflag, int pmode);   // 0x10029c00
void Collect(FileSubstitute* s, SubChunk** list);                                    // 0x1002a150
// the archive fields read by GetSubstBuildVersion / ProductCode
unsigned long ArchiveBuildVersion(FileSub* archive);
unsigned long ArchiveProductCode(FileSub* archive);
// the counter of "%08X.T%08X" temp names (0x100adbc4)
unsigned long& TempCounter();
}  // namespace Subst

// the archive internals that File::Find drives directly (not through the vftable)
namespace ArchiveCursor {
long Open(FileSub* archive);                // 0x100268c0: a read cursor (a new one when none is free)
void Close(FileSub* archive, long cursor);  // 0x100269f0
// 0x10025b10: the next entry from the cursor that matches the pattern (a directory and a wildcard)
bool Next(FileSub* archive, const char* pattern, long cursor, File::Entry* entry);
}  // namespace ArchiveCursor

struct ArcRecord;
struct ArcChunk;

// an archive (a .ja file, FileSubArchive.cpp): "ARCH", then chunks {id, u32 size}: "INFO" (the Info below), one
// "FIL " per file {File::Entry, data} ("FIL!" once deleted) and "IDX " (the directory: {offset, size, Entry} per
// file). Files are known by records in a ring of chunks of 61 (in file order) and a hash of their lower-case names;
// without a directory the records are made while the file is scanned. Read cursors keep a file offset and a place
// in the ring.
class FileSubArchive : public FileSub {
public:
    FileSubArchive(const char* path, bool writable);  // 0x100252f0
    ~FileSubArchive() override;                        // 0x10025670
    static void __cdecl SetDefaultBuildVersion(unsigned long v);
    static void __cdecl SetDefaultProductCode(unsigned long v);

    bool __cdecl IsReadOnly() override { return readOnly; }
    bool __cdecl IsReadable() override { return fd != -1; }
    unsigned long __cdecl Version() override { return info.version; }
    bool __cdecl IsType(int type) override { return type == 0; }
    bool __cdecl OpenWrite(File* f, const char* path, int oflag, int pmode) override;
    bool __cdecl OpenRead(File* f, const char* path, int oflag, int pmode) override;
    void __cdecl Close(File* f) override;
    bool __cdecl Exists(PString* base, const char* path) override;
    bool __cdecl IsDirectory(PString* base, const char* path) override { return IsDir(path, true); }
    bool __cdecl Unlink(const char* path) override;
    bool __cdecl GetStamp(File::Time* t, const char* path) override;
    void __cdecl Collect(SubChunk** list) override { SubChunkAdd(list, this); }
    void __cdecl Lock() override;
    void __cdecl Unlock() override;
    bool __cdecl IsPath(const char* path) override;

    long OpenCursor();
    void CloseCursor(long cursor);
    bool Next(const char* pattern, long cursor, File::Entry* entry, bool advance, bool byName);
    unsigned long BuildVersion() const { return info.build; }
    unsigned long ProductCode() const { return info.product; }

    // a place in the record ring: {item, the ring's first chunk when it was taken, chunk}; no item: at the end
    struct Place {
        ArcRecord** item;
        ArcChunk* head;
        ArcChunk* chunk;
    };

private:
    struct Info {  // "INFO", 0x20 bytes
        unsigned long magic;    // 'ARC0'
        unsigned long version;  // 0x101 / 0x102
        unsigned long count;    // files in the directory
        unsigned long index;    // the "IDX " chunk's offset, 0 none
        unsigned long build;
        unsigned long product;
        unsigned long u18, u1c;
    };
    struct Cursor {
        long pos;  // -1 free
        Place at;
    };
    bool ReadIndex();                                                              // 0x10027010
    void WriteIndex();                                                             // 0x10027330
    void AddRecord(const File::Entry& e, unsigned long offset, unsigned long size);  // 0x10026b20
    void AddDirs(const char* name);                                                // 0x10026be0
    bool IsDir(const char* path, bool normalize);                                  // 0x10026d70
    ArcRecord* NewRecord(const File::Entry& e, unsigned long offset, unsigned long size);  // 0x10027a40
    void Insert(ArcRecord* r);  // into the hash and at the ring's end
    unsigned long RecordCount() const;
    Place First() const;
    Place Last() const;  // 0x10028a50

    File::Path dirCache;  // +0x28 the last directory found
    File::Path path;      // +0x130
    Info info;            // +0x234
    unsigned char flags;  // +0x254 bit 0 the directory is known, bit 1 it changed
    int fd;               // +0x258
    bool readOnly;        // +0x25c
    CRITICAL_SECTION fileLock;  // +0x260 the descriptor
    CRITICAL_SECTION listLock;  // +0x26c the records and cursors
    ArcChunk* ring = nullptr;   // +0x0c
    struct Hash;
    Hash* hash;                 // +0x10
    struct Dirs;
    Dirs* dirs;                 // +0x1c
    struct Cursors;
    Cursors* cursors;           // +0x12c
};

}  // namespace Jet
