// Unit U10b of KernelStandard_7c, part 2: File::Find (0x10014ff0..0x10015130, 0x10015ab0, 0x10022ce0..0x100237c0)
// over directories and the archives' read cursors.
// Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U10b").
#include "FileSub.h"

#include "Crt80.h"

#include <windows.h>

#include <io.h>

#include <cstring>

namespace Jet {

// the names given so far: {next, lower-case name}, 0x108 bytes from Jet::Alloc, in a ring through Find::names
struct NameNode {
    NameNode* next;
    char name[0x104];
};

namespace {

bool IsSepOrColon(char c) { return c == '\\' || c == '/' || c == ':'; }

void Fill(File::Entry& e, const _finddata64i32_t& fd)
{
    e.time = static_cast<unsigned long>(fd.time_write);
    e.u114 = static_cast<unsigned long>(fd.time_access);
    e.u110 = static_cast<unsigned long>(fd.time_create);
    e.size = fd.size;
    e.length = fd.size;
    e.crc = 0;
    e.attrib = static_cast<unsigned short>(fd.attrib);
    std::strcpy(e.name, fd.name);
}

}  // namespace

// ---- File::Find (the archive cursors: FileSubArchive.cpp)

const long File::Find::ClosedHandle = -1;

class FindImpl {
public:
    static NameNode* Sentinel(File::Find& f) { return reinterpret_cast<NameNode*>(&f.names); }
    static void FreeNames(File::Find& f)
    {
        while (f.nameCount) {
            NameNode* const n = f.names;
            f.names = n->next;
            n->next = nullptr;
            --f.nameCount;
            Alloc::Delete(n);
        }
    }
    // 0x10013e90: a copy of every chunk, in order
    static void CopySubs(SubChunk** dst, SubChunk* src)
    {
        if (!src) return;
        SubChunk* c = src;
        do {
            SubChunk* const n = static_cast<SubChunk*>(Alloc::New(sizeof(SubChunk)));
            n->count = 0;
            std::memcpy(&n->count, &c->count, c->count * 4 + 4);
            if (!*dst) {
                *dst = n;
                n->prev = n;
                n->next = n;
            } else {
                SubChunk* const last = (*dst)->prev;
                n->prev = last;
                n->next = *dst;
                (*dst)->prev = n;
                last->next = n;
            }
            c = c->next;
        } while (c != src);
    }
    // the last FileSub collected, or null (NextSearch takes them from the end)
    static FileSub* Pop(File::Find& f)
    {
        SubChunk* const head = f.subs;
        if (!head) return nullptr;
        SubChunk* const last = head->prev;
        FileSub* const sub = last->items[--last->count];
        if (!last->count) {
            if (last == head) {
                Alloc::Delete(last);
                f.subs = nullptr;
            } else {
                last->prev->next = head;
                head->prev = last->prev;
                Alloc::Delete(last);
            }
        }
        return sub;
    }
    static void CloseHandle(File::Find& f)
    {
        if (f.archive)
            ArchiveCursor::Close(f.archive, f.handle);
        else if (f.handle != -1)
            Crt80::_findclose(f.handle);
    }
};

File::Find::Find()  // 0x10015ab0
{
    handle = -1;
    subs = nullptr;
    names = FindImpl::Sentinel(*this);
    nameCount = 0;
    archive = nullptr;
}

// 0x10014ff0: the collected FileSubs are copied, the names are shared (the copy points into the other's ring)
File::Find::Find(const Find& o)
    : Entry(o), handle(o.handle), dir(o.dir), current(o.current), wildcard(o.wildcard), subs(nullptr)
{
    FindImpl::CopySubs(&subs, o.subs);
    names = o.names;
    nameCount = o.nameCount;
    archive = o.archive;
}

// 0x10015090: the old names are left as they are
File::Find& File::Find::operator=(const Find& o)
{
    Entry::operator=(o);
    handle = o.handle;
    dir = o.dir;
    current = o.current;
    wildcard = o.wildcard;
    SubChunkFree(&subs);
    FindImpl::CopySubs(&subs, o.subs);
    names = o.names;
    nameCount = o.nameCount;
    archive = o.archive;
    return *this;
}

File::Find::~Find()  // 0x10022ce0
{
    Close();
    FindImpl::FreeNames(*this);
    SubChunkFree(&subs);
}

// 0x10022f60 (the archive stays set)
void File::Find::Close()
{
    FindImpl::CloseHandle(*this);
    handle = -1;
    FindImpl::FreeNames(*this);
    SubChunkFree(&subs);
}

// 0x100234f0: the name part, and the directory up to the last separator or colon
void File::Find::ReduceWildcard(const char* pattern)
{
    wildcard = pattern;
    wildcard.DelPath();
    dir = pattern;
    char* cut = nullptr;
    for (char* c = dir.text; *c; ++c)
        if (IsSepOrColon(*c)) cut = c + 1;
    if (cut)
        *cut = 0;
    else
        dir.text[0] = 0;
    current = dir;
}

// 0x10022db0: the FileSubs of a substitute given by name; no wildcard in the directory
bool File::Find::Open(const char* pattern, const PString& base)
{
    ReduceWildcard(pattern);
    if (std::strchr(dir.text, '*') || std::strchr(dir.text, '?')) return false;
    FileSubstitute* const s = Subst::Find(base);
    if (!s) return false;
    Subst::Collect(s, &subs);
    return Next();
}

// 0x10022e20: "name:dir\*.x" searches the substitute, anything else the directory (the first entry is not
// checked against the names given)
bool File::Find::Open(const char* pattern)
{
    ReduceWildcard(pattern);
    if (std::strchr(dir.text, '*') || std::strchr(dir.text, '?')) return false;
    if (PString::Cache()) {
        if (FileSubstitute* const s = Path::GetSubst(dir.text)) {
            Subst::Collect(s, &subs);
            if (const char* p = std::strchr(dir.text, ':')) {
                do {
                    ++p;
                } while (*p == '\\' || *p == '/');
                std::memmove(dir.text, p, std::strlen(p) + 1);
            }
            return Next();
        }
    }
    Path p(current.text);
    p.AddPath("*.*", 3);
    return FirstDirect(p.text);
}

// 0x10023020: names lose their directory; one already given is skipped
bool File::Find::Next()
{
    while (NextFile()) {
        const char* from = nullptr;
        const char* c = name;
        for (; *c; ++c)
            if (IsSepOrColon(*c)) from = c + 1;
        if (from) std::memmove(name, from, static_cast<size_t>(c + 1 - name));
        if (!CheckFiles()) return true;
    }
    return false;
}

bool File::Find::NextFile()  // 0x100230b0
{
    if (archive) {
        Path p(dir.text);
        p.AddPath(wildcard.text, static_cast<int>(std::strlen(wildcard.text)));
        if (ArchiveCursor::Next(archive, p.text, handle, this)) return true;
    } else if (handle != -1 && NextDirect()) {
        return true;
    }
    return NextSearch();
}

// 0x100231a0: the next FileSub collected (from the end): an archive is read from a new cursor, a directory
// searched with the pattern's directory (its substitute's name taken off) under it. The directory is first set
// back from `current`, so the name Open took off is there again.
bool File::Find::NextSearch()
{
    for (;;) {
        dir = current;
        FileSub* const sub = FindImpl::Pop(*this);
        FindImpl::CloseHandle(*this);
        archive = nullptr;
        handle = -1;
        if (!sub) return false;
        if (sub->IsType(0)) {
            archive = sub;
            handle = ArchiveCursor::Open(sub);
            if (handle == -1) return false;
            return NextFile();
        }
        if (sub->IsType(1)) {
            Path p(reinterpret_cast<const char*>(sub) + 0xc);  // FileSubPath::dir
            const char* rest = dir.text;
            if (const char* colon = std::strchr(rest, ':')) rest = colon + 1;
            p.AddPath(rest, static_cast<int>(std::strlen(rest)));
            p.AddPath("*.*", 3);
            if (FirstDirect(p.text)) return true;
        }
    }
}

// 0x10023380: true when the name (lower case) was given already; else it is added in order
bool File::Find::CheckFiles()
{
    char lower[0x104];
    std::strcpy(lower, name);
    Crt80::_strlwr(lower);
    NameNode* const end = FindImpl::Sentinel(*this);
    NameNode* prev = end;
    for (NameNode* n = names; n != end; prev = n, n = n->next) {
        const int c = std::strcmp(n->name, lower);
        if (c == 0) return true;
        if (c > 0) break;
    }
    NameNode* const node = static_cast<NameNode*>(Alloc::New(sizeof(NameNode)));
    node->next = nullptr;
    std::strcpy(node->name, name);
    Crt80::_strlwr(node->name);
    node->next = prev->next;
    prev->next = node;
    ++nameCount;
    return false;
}

void File::Find::Dump()  // 0x100234a0
{
    int i = 0;
    Crt80::printf("\n");
    for (NameNode* n = names; n != FindImpl::Sentinel(*this); n = n->next) Crt80::printf("%d. %s\n", i++, n->name);
}

bool File::Find::FirstDirect(const char* pattern)  // 0x10023580
{
    _finddata64i32_t fd;
    handle = static_cast<long>(Crt80::_findfirst64i32(pattern, &fd));
    if (handle == -1) return false;
    if (Path::IsMatch(fd.name, wildcard.text, nullptr)) {
        Fill(*this, fd);
        return true;
    }
    return NextDirect();
}

bool File::Find::NextDirect()  // 0x10023680
{
    _finddata64i32_t fd;
    while (Crt80::_findnext64i32(handle, &fd) != -1) {
        if (Path::IsMatch(fd.name, wildcard.text, nullptr)) {
            Fill(*this, fd);
            return true;
        }
    }
    return false;
}

File::Path File::Find::GetNameAndPath() const  // 0x10023780
{
    Path p(current);
    p.AddPath(name, static_cast<int>(std::strlen(name)));
    return p;
}

JET_LAYOUT32(sizeof(File::Find) == 0x440);

}  // namespace Jet
