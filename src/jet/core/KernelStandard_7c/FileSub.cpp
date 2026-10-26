// Unit U10b of KernelStandard_7c, part 1: the substitute list and its statics (File::Subst, SubstLink, Exists,
// IsDirectory, Unlink, Rename, Md, Prune, GetStamp, SetReadonly, CheckWriteMode, TrueName, Path::GetSubst;
// 0x1001f360..0x1001fe10, 0x10021bb0..0x10021ef0, 0x10024f30, 0x10029960..0x1002a750) and the FileSubs for
// directories and links (FileSubPath, FileSubLink, 0x10028da0..0x10029960). The archives: FileSubArchive.cpp.
// Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U10b").
#include "FileSub.h"

#include "Crt80.h"

#include <windows.h>

#include <io.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>

namespace Jet {

namespace {

const char kFileSource[] = "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/KernelStandard/File.cpp";
const char kSubSource[] = "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/KernelStandard/FileSub.cpp";
const char kListSource[] = "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/KernelStandard/FileSubstList.cpp";

constexpr unsigned long kChunk = 0x4000;
constexpr int O_BINARY_ = 0x8000, O_CREAT_ = 0x100, O_TRUNC_ = 0x200, O_EXCL_ = 0x400, O_WRONLY_ = 1;
constexpr int SH_DENYRW_ = 0x10, SH_DENYWR_ = 0x20;

bool IsSep(char c) { return c == '\\' || c == '/'; }

// "name:rest" -> "rest" without one leading separator (the statics' way of handing a path to the substitute)
const char* StripSubst(const char* p)
{
    const char* const colon = std::strchr(p, ':');
    if (colon) p = colon + 1;
    if (IsSep(*p)) ++p;
    return p;
}

// the directory part of a path: cut after its last separator or colon, empty without one
void CutAfterLastSep(File::Path& p)
{
    char* cut = nullptr;
    for (char* c = p.text; *c; ++c)
        if (IsSep(*c) || *c == ':') cut = c + 1;
    if (cut)
        *cut = 0;
    else
        p.text[0] = 0;
}

int ChmodMode(bool readonly) { return readonly ? 0x100 : 0x180; }

// ---- the list: substitutes sorted by their name's node (0x100741c8, count 0x100741cc), one lock (0x100741d8)
struct Registry {
    CRITICAL_SECTION lock;
    FileSubstitute* first = nullptr;
    unsigned long count = 0;
    Registry() { InitializeCriticalSection(&lock); }
    // 0x1002a750 at exit: every substitute and its FileSubs go, without the lock (an archive written to rewrites
    // its directory in its destructor)
    ~Registry()
    {
        while (count) {
            FileSubstitute* const s = first;
            first = s->next;
            s->next = nullptr;
            --count;
            s->name = PString();
            while (s->count) {
                FileSub* const sub = s->first;
                s->first = sub->next;
                sub->next = nullptr;
                --s->count;
                delete sub;
            }
            delete s;
        }
        DeleteCriticalSection(&lock);
    }
};
Registry& Reg()
{
    static Registry r;
    return r;
}

struct ListLock {
    ListLock() { Subst::Lock(); }
    ~ListLock() { Subst::Unlock(); }
};

// 0x1002a410: a new substitute in its place in the list
FileSubstitute* Create(const PString& name)
{
    ListLock lock;
    Registry& r = Reg();
    auto* const s = new FileSubstitute{nullptr, nullptr, 0, name};
    FileSubstitute** at = &r.first;
    while (*at && reinterpret_cast<uintptr_t>(s->name.node) > reinterpret_cast<uintptr_t>((*at)->name.node))
        at = &(*at)->next;
    s->next = *at;
    *at = s;
    ++r.count;
    return s;
}

void Add(FileSubstitute* s, FileSub* sub, bool atEnd)
{
    ListLock lock;
    FileSub** at = &s->first;
    if (atEnd)
        while (*at) at = &(*at)->next;
    sub->next = *at;
    *at = sub;
    ++s->count;
}

// 0x1002a4b0: links to the substitute are cleared in every substitute, then it goes with its FileSubs
void RemoveSubst(const PString& name)
{
    FileSubstitute* const s = Subst::Find(name);
    if (!s) {
        FileEx e(kListSource, 0x48);
        e.SetReasonf("Subst(%s) Closing non-existant subst?", name.Text());
        throw FileEx(e);
    }
    ListLock lock;
    Registry& r = Reg();
    for (FileSubstitute* e = r.first; e; e = e->next)
        for (FileSub* sub = e->first; sub; sub = sub->next) sub->Forget(&name);
    FileSubstitute** at = &r.first;
    while (*at && *at != s) at = &(*at)->next;
    *at = s->next;
    s->next = nullptr;
    --r.count;
    s->name = PString();
    while (s->count) {
        FileSub* const sub = s->first;
        s->first = sub->next;
        sub->next = nullptr;
        --s->count;
        delete sub;
    }
    delete s;
}

// ---- the list walks (0x10029ca0..0x1002a340), each under the lock

// 0x10029ca0: the first readable FileSub that has the path; base becomes the substitute's name on the way
int ExistsIn(FileSubstitute* s, PString* base, const char* path)
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next) {
        if (!sub->IsReadable()) continue;
        *base = s->name;
        if (sub->Exists(base, path)) return 0;
    }
    return 2;
}

// 0x10029dd0: asked only of FileSubs while the base is this substitute's own name (so never for a null base)
bool IsDirectoryIn(FileSubstitute* s, PString* base, const char* path)
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next) {
        if (!sub->IsReadable() || base->node != s->name.node) continue;
        PString found = *base;
        if (sub->Exists(&found, path)) return sub->IsDirectory(base, path);
    }
    return false;
}

// 0x10029f70: every writable FileSub unlinks it
bool UnlinkIn(FileSubstitute* s, const char* path)
{
    ListLock lock;
    bool done = false;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (sub->IsReadable() && !sub->IsReadOnly() && sub->Unlink(path)) done = true;
    return done;
}

bool RenameIn(FileSubstitute* s, const char* from, const char* to)  // 0x1002a010
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (sub->IsReadable() && !sub->IsReadOnly() && sub->Rename(from, to)) return true;
    return false;
}

bool MdIn(FileSubstitute* s, const char* path)  // 0x1002a0b0
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (sub->IsReadable() && !sub->IsReadOnly() && sub->Md(path)) return true;
    return false;
}

bool TrueNameIn(FileSubstitute* s, char* path)  // 0x1002a230: readable or not
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (sub->TrueName(path)) return true;
    return false;
}

bool GetStampIn(FileSubstitute* s, File::Time* t, const char* path)  // 0x1002a2b0
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (sub->IsReadable() && sub->GetStamp(t, path)) return true;
    return false;
}

void SetReadonlyIn(FileSubstitute* s, const char* path, bool on)  // 0x1002a340
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (sub->IsReadable()) sub->SetReadonly(path, on);
}

}  // namespace

// ---- the FileSubs (the archives: FileSubArchive.cpp)

// a directory (0x114 bytes): {FileSub, Path dir +0xc, flags +0x110: bit 0 read-only, bit 1 it exists}
class FileSubPath : public FileSub {
public:
    FileSubPath(const char* path, bool add)  // 0x10028f20
    {
        dir.text[0] = 0;
        flags = (flags & ~2) | (File::Exists(path).code == 0 ? 2 : 0);
        dir = path;
        flags = (flags & ~1) | (add ? 0 : 1);
    }
    bool __cdecl IsReadOnly() override { return flags & 1; }
    bool __cdecl IsReadable() override { return (flags >> 1) & 1; }
    unsigned long __cdecl Version() override { return 0x102; }
    bool __cdecl IsType(int type) override { return type == 1; }

    // 0x10029000: the file's directory is made when missing
    bool __cdecl OpenWrite(File* f, const char* path, int oflag, int pmode) override
    {
        File::Path d(dir);
        if (path) d.AddPath(path, static_cast<int>(std::strlen(path)));
        CutAfterLastSep(d);
        if (File::Exists(d.text).code != 0 && !File::Md(d.text)) return false;
        flags |= 2;
        f->path.Make(dir.text, path);
        if (!f->path.text[0]) return false;
        f->fd = Crt80::sopen(f->path.text, oflag, SH_DENYRW_, pmode);
        if (f->fd == -1) return false;
        f->sub = this;
        InterlockedIncrement(&refs);
        return true;
    }
    bool __cdecl OpenRead(File* f, const char* path, int oflag, int pmode) override  // 0x10029140
    {
        f->path.Make(dir.text, path);
        if (!f->path.text[0]) return false;
        f->fd = Crt80::sopen(f->path.text, oflag, SH_DENYWR_, pmode);
        if (f->fd == -1) return false;
        f->sub = this;
        InterlockedIncrement(&refs);
        return true;
    }
    // 0x100291b0: a compressed file was written plain; it is packed now into a hidden temp file in BPE chunks,
    // which should replace it (as File::Close does for a plain path). The original keeps the temp name in 16 bytes
    // right before its read buffer: the 19-byte name runs into it, the first chunk read garbles the name, the
    // rename fails, and the file is gone with its packed copy left as a hidden temp file in the current
    // directory (only an empty file, nothing read, survives). Kept: `frame` is that stack layout.
    void __cdecl Close(File* f) override
    {
        unsigned long left = static_cast<unsigned long>(Crt80::_tell(f->fd));
        const unsigned long total = left;
        Crt80::_close(f->fd);
        InterlockedDecrement(&refs);
        if ((f->mode & 0xf) != 2) return;
        const int in = Crt80::open(f->path.text, O_BINARY_, 0);
        const unsigned long n = Subst::TempCounter()++;
        char frame[16 + kChunk + 1];  // the temp name, then the read buffer (a NUL after it)
        char* const temp = frame;
        unsigned char* const raw = reinterpret_cast<unsigned char*>(frame + 16);
        frame[16 + kChunk] = 0;
        std::sprintf(temp, "%08X.T%08X", GetCurrentProcessId(), n);
        const int out = Crt80::sopen(temp, O_BINARY_ | O_CREAT_ | O_TRUNC_ | O_WRONLY_, SH_DENYRW_, 0x180);
        SetFileAttributesA(temp, FILE_ATTRIBUTE_HIDDEN);
        Crt80::_write(out, "CFIL", 4);
        const unsigned long version = 0x101;
        Crt80::_write(out, &version, 4);
        Crt80::_write(out, &total, 4);
        unsigned long last = total;
        while (left && last) {
            const unsigned long want = left > kChunk ? kChunk : left;
            last = static_cast<unsigned long>(Crt80::_read(in, raw, want));
            left -= last;
            const unsigned long c = BPE::Comp(f->buffer, raw, last);
            const unsigned long header = c == last ? (c | 0x8000) & 0xffff : c & 0xffff;
            Crt80::_write(out, &header, 2);
            last = static_cast<unsigned long>(Crt80::_write(out, f->buffer, c));
        }
        Crt80::_close(in);
        Crt80::_close(out);
        Crt80::_unlink(f->path.text);
        Crt80::rename(temp, f->path.text);
        SetFileAttributesA(f->path.text, FILE_ATTRIBUTE_NORMAL);
    }
    bool __cdecl Exists(PString*, const char* path) override  // 0x100293a0
    {
        File::Path p;
        p.Make(dir.text, path);
        return p.text[0] && File::Exists(p.text).code == 0;
    }
    bool __cdecl IsDirectory(PString*, const char* path) override  // 0x10029440
    {
        File::Path p;
        p.Make(dir.text, path);
        return p.text[0] && File::IsDirectory(p.text);
    }
    bool __cdecl Unlink(const char* path) override  // 0x100294d0
    {
        File::Path p;
        p.Make(dir.text, path);
        return p.text[0] && File::Unlink(p.text);
    }
    bool __cdecl Rename(const char* from, const char* to) override  // 0x10029560
    {
        File::Path a, b;
        a.Make(dir.text, from);
        if (!a.text[0]) return false;
        b.Make(dir.text, to);
        if (!b.text[0]) return false;
        return File::Rename(a.text, b.text);
    }
    bool __cdecl Md(const char* path) override  // 0x10029630
    {
        File::Path p;
        p.Make(dir.text, path);
        return p.text[0] && File::Md(p.text);
    }
    bool __cdecl TrueName(char* path) override  // 0x100296d0: the path within the directory, when it is there
    {
        PString base;
        if (!Exists(&base, path)) return false;
        File::Path p;
        p.Make(dir.text, path);
        if (!p.text[0]) return false;
        std::strcpy(path, p.text);
        return true;
    }
    bool __cdecl GetStamp(File::Time* t, const char* path) override  // 0x10029830
    {
        File::Path p;
        p.Make(dir.text, path);
        return p.text[0] && File::GetStamp(*t, p.text);
    }
    void __cdecl SetReadonly(const char* path, bool on) override  // 0x100298d0
    {
        File::Path p;
        p.Make(dir.text, path);
        if (p.text[0]) Crt80::_chmod(p.text, ChmodMode(on));
    }
    void __cdecl Collect(SubChunk** list) override { SubChunkAdd(list, this); }  // 0x10026890
    bool __cdecl IsPath(const char* path) override { return std::strcmp(path, dir.text) == 0; }  // 0x10028ed0

    File::Path dir;
    unsigned char flags = 0;
};
JET_LAYOUT32(sizeof(FileSubPath) == 0x114);

namespace {

// a link to another substitute (0x10 bytes): {FileSub, FileSubstitute* +0xc, cleared when that one goes}
class FileSubLink : public FileSub {
public:
    explicit FileSubLink(FileSubstitute* t) : target(t) {}
    void __cdecl Forget(const PString* name) override  // 0x10028dc0
    {
        if (target && target->name.node == name->node) target = nullptr;
    }
    bool __cdecl IsReadOnly() override { return false; }
    bool __cdecl IsReadable() override { return target != nullptr; }
    bool __cdecl IsType(int type) override { return type == 2; }
    // (a link whose substitute went away is still asked to write: the original reads through null too)
    bool __cdecl OpenWrite(File* f, const char* path, int oflag, int pmode) override
    {
        return Subst::OpenWrite(target, f, path, oflag, pmode);
    }
    bool __cdecl OpenRead(File* f, const char* path, int oflag, int pmode) override
    {
        return Subst::OpenRead(target, f, path, oflag, pmode);
    }
    bool __cdecl Exists(PString* base, const char* path) override { return ExistsIn(target, base, path) == 0; }
    bool __cdecl IsDirectory(PString* base, const char* path) override { return IsDirectoryIn(target, base, path); }
    void __cdecl Collect(SubChunk** list) override { Subst::Collect(target, list); }

    FileSubstitute* target;
};
JET_LAYOUT32(sizeof(FileSubLink) == 0x10);

}  // namespace

void FileSub::Close(File*) { throw FileEx(kSubSource, 0x57, "CloseFile() Logic Error"); }  // 0x100251a0

bool FileSub::ExistsAs(PString* base, const char* path, File::Path* found)  // 0x100252c0
{
    *found = path;
    return Exists(base, path);
}

// ---- the collected lists (0x10028d20)

void SubChunkAdd(SubChunk** list, FileSub* sub)
{
    SubChunk* last;
    if (!*list) {
        last = static_cast<SubChunk*>(Alloc::New(sizeof(SubChunk)));
        last->count = 0;
        *list = last;
        last->prev = last;
        last->next = last;
    } else {
        last = (*list)->prev;
        if (last->count == 13) {
            SubChunk* const c = static_cast<SubChunk*>(Alloc::New(sizeof(SubChunk)));
            c->count = 0;
            c->next = *list;
            c->prev = last;
            (*list)->prev = c;
            last->next = c;
            last = c;
        }
    }
    last->items[last->count++] = sub;
}

void SubChunkFree(SubChunk** list)
{
    SubChunk* const head = *list;
    if (!head) return;
    SubChunk* c = head->next;
    while (c != head) {
        SubChunk* const n = c->next;
        Alloc::Delete(c);
        c = n;
    }
    Alloc::Delete(head);
    *list = nullptr;
}

// ---- Subst

void Subst::Lock() { EnterCriticalSection(&Reg().lock); }
void Subst::Unlock() { LeaveCriticalSection(&Reg().lock); }

FileSubstitute* Subst::Find(const PString& name)  // 0x1002a5f0
{
    if (!name.node || !name.node->length) return nullptr;
    ListLock lock;
    for (FileSubstitute* s = Reg().first; s; s = s->next) {
        const auto key = reinterpret_cast<uintptr_t>(name.node);
        const auto at = reinterpret_cast<uintptr_t>(s->name.node);
        if (key <= at) return key == at ? s : nullptr;
    }
    return nullptr;
}

// 0x10029b60: the first writable FileSub that opens it
bool Subst::OpenWrite(FileSubstitute* s, File* f, const char* path, int oflag, int pmode)
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (!sub->IsReadOnly() && sub->OpenWrite(f, path, oflag, pmode)) return true;
    return false;
}

// 0x10029c00: the first readable FileSub that opens it
bool Subst::OpenRead(FileSubstitute* s, File* f, const char* path, int oflag, int pmode)
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (sub->IsReadable() && sub->OpenRead(f, path, oflag, pmode)) return true;
    return false;
}

void Subst::Collect(FileSubstitute* s, SubChunk** list)  // 0x1002a150: links give their substitute's FileSubs
{
    ListLock lock;
    for (FileSub* sub = s->first; sub; sub = sub->next)
        if (sub->IsReadable()) sub->Collect(list);
}

// ---- the File statics

FileSubstitute* File::Path::GetSubst(const char* path)  // 0x10024f30: "x:\" and "x:/" are drives
{
    const char* const colon = std::strchr(path, ':');
    if (!colon) return nullptr;
    if (colon == path + 1 && IsSep(colon[1])) return nullptr;
    unsigned int n = static_cast<unsigned int>(colon - path);
    const unsigned int len = static_cast<unsigned int>(std::strlen(path));
    if (n > len) n = len;
    PString name;
    if (path || n) name.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(path), n);
    return Subst::Find(name);
}

FileSubstitute* File::Path::GetSubst() const { return GetSubst(text); }  // 0x10015890

void File::Subst(const PString& name, const char* path, bool add)  // 0x1001f6e0
{
    FileSubstitute* s = Subst::Find(name);
    if (path) {
        if (!s) s = Create(name);
        // 0x10029960: a path ending in a separator is a directory, anything else an archive; put first
        const size_t len = std::strlen(path);
        const char last = path[len - 1];
        FileSub* const sub = IsSep(last) ? static_cast<FileSub*>(new FileSubPath(path, add)) : new FileSubArchive(path, add);
        Add(s, sub, false);
    } else if (s) {
        RemoveSubst(name);
    }
}

File::Result File::SubstExists(const PString& name)  // 0x1001f720
{
    return Result(Subst::Find(name) ? 0 : 7);
}

void File::SubstLink(const PString& name, const PString& other)  // 0x1001f360: a link goes last
{
    FileSubstitute* const o = Subst::Find(other);
    if (!o) {
        FileEx e(kFileSource, 0xfe);
        e.SetReasonf("SubstLink(%s,%s) Other subst (%s) not found", name.Text(), other.Text(), other.Text());
        throw FileEx(e);
    }
    FileSubstitute* s = Subst::Find(name);
    if (!s) s = Create(name);
    Add(s, new FileSubLink(o), true);
}

namespace {
// 0x1001f480 / 0x1001f5b0: the first archive among the substitute's FileSubs (links followed) with the field set
unsigned long FirstArchiveField(const PString& name, unsigned long (*field)(FileSub*))
{
    FileSubstitute* const s = Subst::Find(name);
    if (!s) return 0;
    SubChunk* list = nullptr;
    Subst::Collect(s, &list);
    unsigned long v = 0;
    if (SubChunk* c = list) {
        do {
            for (unsigned long i = 0; i < c->count && !v; ++i)
                if (c->items[i]->IsType(0)) v = field(c->items[i]);
            c = c->next;
        } while (!v && c != list);
    }
    SubChunkFree(&list);
    return v;
}
}  // namespace

unsigned long File::GetSubstBuildVersion(const PString& name) { return FirstArchiveField(name, Subst::ArchiveBuildVersion); }
unsigned long File::GetSubstProductCode(const PString& name) { return FirstArchiveField(name, Subst::ArchiveProductCode); }

File::Result File::Exists(const char* path)  // 0x1001f750
{
    if (FileSubstitute* const s = Path::GetSubst(path)) {
        PString base;
        return Result(ExistsIn(s, &base, StripSubst(path)));
    }
    if (Crt80::_access(path, 0) != 0) {
        const int e = *Crt80::_errno();
        if (e == 2 || e == 0xe || e == 0x26 || e == 0x14) return Result(2);
    }
    return Result(0);
}

File::Result File::Exists(PString& base, const char* path)  // 0x1001f9b0
{
    if (!base.node) return Exists(path);
    FileSubstitute* const s = Subst::Find(base);
    if (!s) return Result(7);
    return Result(ExistsIn(s, &base, path));
}

File::Result File::ExistsIgnoreCase(const char* path, Path& found)  // 0x10015560
{
    found = path;
    return Exists(path);
}

File::Result File::ExistsIgnoreCase(PString& base, const char* path, Path& found)  // 0x10015590
{
    found = path;
    if (!base.node) return Exists(path);
    FileSubstitute* const s = Subst::Find(base);
    if (!s) return Result(7);
    return Result(ExistsIn(s, &base, path));
}

bool File::IsDirectory(const char* path)  // 0x1001f890
{
    if (FileSubstitute* const s = Path::GetSubst(path)) {
        PString base;  // (null: no FileSub is asked)
        return IsDirectoryIn(s, &base, StripSubst(path));
    }
    struct _stat64i32 st;
    if (Crt80::_stat64i32(path, &st) == -1) return false;
    return (st.st_mode >> 14) & 1;
}

bool File::IsDirectory(const PString& base, const char* path)  // 0x1001fa00
{
    if (!base.node) return IsDirectory(path);
    FileSubstitute* const s = Subst::Find(base);
    if (!s) return false;
    return IsDirectoryIn(s, const_cast<PString*>(&base), path);
}

bool File::Unlink(const char* path)  // 0x1001fa40
{
    if (FileSubstitute* const s = Path::GetSubst(path)) return UnlinkIn(s, StripSubst(path));
    return Crt80::_unlink(path) != -1;
}

bool File::Unlink(const PString& base, const char* path)  // 0x1001faa0 (also Remove)
{
    if (base.node)
        if (FileSubstitute* const s = Subst::Find(base)) return UnlinkIn(s, path);
    return Unlink(path);
}

bool File::Remove(const char* path) { return Unlink(path); }  // 0x10015150
bool File::Remove(const PString& base, const char* path) { return Unlink(base, path); }

bool File::Rename(const char* from, const char* to)  // 0x1001fae0
{
    FileSubstitute* const s = Path::GetSubst(from);
    if (!s) return Crt80::rename(from, to) == 0;
    from = StripSubst(from);
    to = StripSubst(to);
    return RenameIn(s, from, to);
}

bool File::Rename(const PString& base, const char* from, const char* to)  // 0x1001fb70
{
    if (base.node)
        if (FileSubstitute* const s = Subst::Find(base)) return RenameIn(s, from, to);
    return Rename(from, to);
}

// 0x1001fbf0: each missing directory from the root down; a component that is there must be a directory
bool File::Md(const char* path)
{
    if (FileSubstitute* const s = Path::GetSubst(path)) return MdIn(s, StripSubst(path));
    Path rest(path);
    Path built(path);
    built.RootDir();
    CutAfterLastSep(built);
    while (rest.text[0]) {
        const Path part = rest.ExtractFirstPath();
        built.AddPath(part.text, static_cast<int>(std::strlen(part.text)));
        if (Crt80::_access(built.text, 0) == -1) {
            if (Crt80::_mkdir(built.text) == -1) return false;
            continue;
        }
        const size_t n = std::strlen(built.text);
        if (n && IsSep(built.text[n - 1])) continue;
        struct _stat64i32 st;
        if (Crt80::_stat64i32(built.text, &st) != 0) return false;
        if (!(st.st_mode & 0x4000)) return false;
    }
    return true;
}

// 0x1001fe10: plain paths only. Files are made writable and deleted, directories pruned; after a failure the
// rest is still made writable but kept, and the directory stays.
bool File::Prune(const char* path)
{
    if (std::strlen(path) + 4 >= 0x104) return false;
    bool ok = true;
    char buf[0x104];
    std::strcpy(buf, path);
    std::strcat(buf, "\\*.*");
    struct _finddata64i32_t fd;
    const intptr_t h = Crt80::_findfirst64i32(buf, &fd);
    if (h != -1) {
        do {
            if (std::strlen(path) + std::strlen(fd.name) + 1 >= 0x104) {
                ok = false;
                break;
            }
            std::strcpy(buf, path);
            std::strcat(buf, "\\");
            std::strcat(buf, fd.name);
            if (fd.attrib & 0x10) {
                if (std::strcmp(fd.name, ".") != 0 && std::strcmp(fd.name, "..") != 0) ok = ok && Prune(buf);
            } else {
                ok = Crt80::_chmod(buf, 0x180) == 0 && ok && Crt80::_unlink(buf) != -1;
            }
        } while (Crt80::_findnext64i32(h, &fd) == 0);
        Crt80::_findclose(h);
        if (!ok) return false;
    }
    return Crt80::_rmdir(path) != -1;
}

bool File::GetStamp(Time& t, const char* path)  // 0x10021bb0
{
    if (FileSubstitute* const s = Path::GetSubst(path)) {
        t.seconds = 0;
        return GetStampIn(s, &t, StripSubst(path));
    }
    struct _stat64i32 st;
    if (Crt80::_stat64i32(path, &st) != 0) return false;
    t.seconds = static_cast<long>(st.st_mtime);
    return true;
}

void File::SetReadonly(const char* path, bool on) { Crt80::_chmod(path, ChmodMode(on)); }  // 0x10021dc0

// 0x10021de0: through the substitute, and then the same path as a plain one as well
void File::SetReadonly(const PString& base, const char* path, bool on)
{
    if (base.node)
        if (FileSubstitute* const s = Subst::Find(base)) SetReadonlyIn(s, path, on);
    Crt80::_chmod(path, ChmodMode(on));
}

File::Result File::CheckWriteMode(const char* path)  // 0x10021e30
{
    struct _stat64i32 st;
    if (Crt80::_stat64i32(path, &st) != 0) return Result(2);
    if (!(st.st_mode & 0x80)) return Result(9);
    return Result(0);
}

bool File::TrueName(char* path)  // 0x10021e80: the substitute's name is cut off in place first
{
    FileSubstitute* const s = Path::GetSubst(path);
    if (!s) return Crt80::_access(path, 0) != -1;
    const char* const colon = std::strchr(path, ':');
    const char* from = colon ? colon + 1 : path;
    if (IsSep(*from)) ++from;
    if (from != path) std::memmove(path, from, std::strlen(from) + 1);
    return TrueNameIn(s, path);
}

}  // namespace Jet
