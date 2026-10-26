// Unit U10a of KernelStandard_7c: File::Path (a MAX_PATH buffer with path helpers), File::Time and File::Entry
// (0x10015600..0x10015ab0, 0x100237c0..0x10025170). Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U10a").
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <cstring>

namespace Jet {

namespace {

// the original's error byte (0x100adb92): set when a path would not fit, never read
bool pathTooLong = false;

bool IsSep(int c) { return c == '\\' || c == '/'; }

// the case folding of IsMatch: ASCII by hand, the rest through the CRT (the locale of the game)
int Fold(int c)
{
    if (c < 0x80) return c - 'A' < 26u ? c | 0x20 : c;
    return Crt80::tolower(c);
}

// the last '\\' or '/' (the later of strrchr of both)
char* LastSlash(char* s)
{
    char* const a = std::strrchr(s, '\\');
    char* const b = std::strrchr(s, '/');
    return a > b ? a : b;
}

// the original's strcat helper (0x10014450)
void Append(char* dst, const char* src) { std::strcat(dst, src); }

}  // namespace

// ---- File::Time

File::Time& File::Time::operator=(const Time& o)
{
    seconds = o.seconds;
    return *this;
}
bool File::Time::operator==(const Time& o) const { return seconds == o.seconds; }
bool File::Time::operator<(const Time& o) const { return seconds < o.seconds; }
bool File::Time::operator>(const Time& o) const { return o.seconds < seconds; }
bool File::Time::operator<=(const Time& o) const { return seconds <= o.seconds; }
bool File::Time::operator>=(const Time& o) const { return o.seconds <= seconds; }
void File::Time::SetSeconds(long s) { seconds = s; }

// ---- File::Entry

File::Entry::Entry()
{
    name[0] = 0;
    size = length = crc = 0;
    attrib = u11e = 0;
    u110 = u114 = time = 0;
}

File::Entry::Entry(const Entry& o)
{
    name[0] = 0;
    reinterpret_cast<Path*>(name)->operator=(*reinterpret_cast<const Path*>(o.name));
    size = o.size;
    length = o.length;
    crc = o.crc;
    u110 = o.u110;
    u114 = o.u114;
    time = o.time;
    attrib = o.attrib;
    u11e = o.u11e;
}

File::Entry& File::Entry::operator=(const Entry& o)
{
    if (this != &o) {
        reinterpret_cast<Path*>(name)->operator=(*reinterpret_cast<const Path*>(o.name));
        size = o.size;
        length = o.length;
        crc = o.crc;
        u110 = o.u110;
        u114 = o.u114;
        time = o.time;
        attrib = o.attrib;
        u11e = o.u11e;
    }
    return *this;
}

bool File::Entry::IsDir() { return (attrib >> 4) & 1; }
bool File::Entry::IsCompressed() { return (attrib & 0xff) >> 7; }
bool File::Entry::IsHidden() { return (attrib >> 1) & 1; }
void File::Entry::LittleEndian() {}

// ---- File::Path: construction and assignment (more than 0x102 characters: empty, error byte set)

File::Path::Path() { text[0] = 0; }

File::Path::Path(const char* path)
{
    text[0] = 0;
    if (!path) return;
    if (std::strlen(path) > 0x102) {
        pathTooLong = true;
        return;
    }
    std::strcpy(text, path);
}

File::Path::Path(const Path& o)
{
    if (std::strlen(o.text) > 0x102) {
        text[0] = 0;
        pathTooLong = true;
        return;
    }
    std::strcpy(text, o.text);
}

void File::Path::operator=(const char* path)
{
    text[0] = 0;
    if (!path) return;
    if (std::strlen(path) > 0x102) {
        pathTooLong = true;
        return;
    }
    std::strcpy(text, path);
}

void File::Path::operator=(const Path& o)
{
    if (std::strlen(o.text) > 0x102) {
        pathTooLong = true;
        text[0] = 0;
        return;
    }
    std::strcpy(text, o.text);
}

File::Path::operator bool() const { return true; }
bool File::Path::operator!() const { return false; }
File::Path::operator const char*() const { return text; }

bool File::Path::operator==(const Path& o) const { return std::strcmp(text, o.text) == 0; }
bool File::Path::operator==(const char* o) const { return std::strcmp(text, o) == 0; }
bool File::Path::operator!=(const Path& o) const { return std::strcmp(text, o.text) != 0; }
bool File::Path::operator!=(const char* o) const { return std::strcmp(text, o) != 0; }
bool File::Path::operator<(const Path& o) const { return std::strcmp(text, o.text) < 0; }

// ---- queries

unsigned int File::Path::Length() const { return static_cast<unsigned int>(std::strlen(text)); }

unsigned int File::Path::Last() const
{
    const size_t n = std::strlen(text);
    return n ? static_cast<unsigned char>(text[n - 1]) : 0;
}

unsigned int File::Path::GetFilePathHash() const
{
    unsigned int h = 0;
    for (const char* p = text; *p; ++p) h = h * 8 ^ static_cast<int>(*p);
    return h;
}

bool File::Path::HasValidData() const
{
    for (unsigned int i = 0; i < 0x104; ++i)
        if (!text[i]) return true;
    return false;
}

const char* File::Path::GetPath() const { return GetPath(text); }
const char* File::Path::GetPath(const char* path)
{
    const char* const colon = std::strchr(path, ':');
    return colon ? colon + 1 : path;
}

const char* File::Path::GetFileName() const { return GetFileName(text); }
const char* File::Path::GetFileName(const char* path)
{
    const char* name = nullptr;
    for (const char* p = path; *p; ++p)
        if (IsSep(*p) || *p == ':') name = p + 1;
    return name ? name : path;
}

const char* File::Path::GetExt() const { return GetExt(text); }
const char* File::Path::GetExt(const char* path)
{
    const char* ext = nullptr;
    if (path)
        for (const char* p = path; *p; ++p) {
            if (IsSep(*p) || *p == ':')
                ext = nullptr;
            else if (*p == '.')
                ext = p + 1;
        }
    return ext;
}

bool File::Path::IsExt(const char* ext) const
{
    const char* dot = text;  // the last '.', '/' or '\\' (the start when there is none)
    for (const char* p = text; *p; ++p)
        if (*p == '.' || IsSep(*p)) dot = p;
    if (!ext) return *dot == 0;
    if (*ext == '.') ++ext;
    return *dot == '.' && Crt80::_stricmp(ext, dot + 1) == 0;
}

bool File::Path::IsEqual(const char* a, const char* b)
{
    if (!a) return !b;
    if (!b) return false;
    // stops at the shorter one: a prefix counts as equal
    for (; *a && *b; ++a, ++b)
        if (*a != *b && !(IsSep(*a) && IsSep(*b))) return false;
    return true;
}

// The wildcard match (0x10023b90): '*' and '?' never match a separator, '/' and '\\' match each other, case is
// folded, ';' separates alternatives. A pattern with a directory matches the directories first; `rest` gets the
// part of the path past the matched directories (or past the pattern).
bool File::Path::IsMatch(const char* other, const char** rest) const { return IsMatch(text, other, rest); }

bool File::Path::IsMatch(const char* path, const char* pattern, const char** rest)
{
    const char* star = nullptr;  // the last '*' in the pattern
    const char* starAt = nullptr;  // where in the path it began
    const char* restart = path;  // the path an alternative starts from
    if (rest) *rest = nullptr;
    const char* q = pattern;
    if (q[0] == '.' && IsSep(q[1])) q += 2;
    const char* lastSlash = nullptr;
    for (; *q; ++q)
        if (IsSep(*q)) lastSlash = q;
    const char* s = path;
    if (s[0] == '.' && IsSep(s[1])) s += 2;
    const char* p = pattern;
    if (p[0] == '.' && IsSep(p[1])) p += 2;

    if (lastSlash) {
        // the directories, up to the pattern's last separator
        for (;;) {
            const int c = static_cast<unsigned char>(*s);
            const int pc = static_cast<unsigned char>(*p);
            bool fail = false;
            if (!c) {
                fail = true;
            } else if (pc == '*') {
                if (IsSep(c)) {
                    ++p;
                } else if (Fold(c) == Fold(static_cast<unsigned char>(p[1]))) {
                    star = p;
                    starAt = s;
                    ++p;
                } else {
                    ++s;
                }
                continue;
            } else if (pc == '?') {
                if (!IsSep(c)) ++s;
                ++p;
                continue;
            } else if (IsSep(pc)) {
                if (!IsSep(c)) {
                    fail = true;
                } else {
                    ++p;
                    ++s;
                    restart = s;
                    if (p <= lastSlash) continue;
                    if (!*p && rest) *rest = path < s ? s - 1 : nullptr;
                    break;
                }
            } else if (Fold(pc) == Fold(c)) {
                ++p;
                ++s;
                continue;
            } else {
                fail = true;
            }
            if (fail) {
                if (!star) return false;
                p = star;
                s = starAt + 1;
                star = nullptr;
            }
        }
    }

    // the name (and the alternatives after ';')
    for (;;) {
        const int c = static_cast<unsigned char>(*s);
        const int pc = static_cast<unsigned char>(*p);
        if (!c) {
            if (!pc || pc == ';') return true;
            if (pc == '*') {
                int n = static_cast<unsigned char>(p[1]);
                if (n == '.') n = static_cast<unsigned char>(p[2]);
                if (!n || n == ';') return true;
            }
        } else if (!pc) {
            if (rest && !*rest && IsSep(c)) *rest = s;
            if (!star) return false;
            s = starAt + 1;
            p = star;
            star = nullptr;
            continue;
        } else if (pc == '*') {
            if (IsSep(c)) {
                ++p;
            } else if (Fold(c) == Fold(static_cast<unsigned char>(p[1]))) {
                star = p;
                starAt = s;
                ++p;
            } else {
                ++s;
            }
            continue;
        } else if (pc == ';') {
            if (rest && !*rest && IsSep(c)) *rest = s;
        } else if (pc == '?') {
            if (!IsSep(c)) ++s;
            ++p;
            continue;
        } else if (Fold(pc) == Fold(c)) {
            ++p;
            ++s;
            continue;
        }
        // no match here: back to the last '*', or on to the next alternative
        if (star) {
            s = starAt + 1;
            p = star;
            star = nullptr;
            continue;
        }
        const char* const alt = std::strchr(p, ';');
        if (!alt) return false;
        s = restart;
        p = alt + 1;
    }
}

// ---- editing

void File::Path::Add(const char* s) { std::strcat(text, s); }
void File::Path::Add(const char* s, int count) { std::strncat(text, s, static_cast<size_t>(count)); }

void File::Path::AddPath(const char* path)
{
    if (path) AddPath(path, static_cast<int>(std::strlen(path)));
}

// appends a component with one '/' between (leading separators of `path` dropped when the text is not empty)
void File::Path::AddPath(const char* path, int count)
{
    const size_t len = std::strlen(text);
    const int last = len ? static_cast<unsigned char>(text[len - 1]) : 0;
    if (last && !IsSep(last)) {
        if (len >= 0x103) {
            text[0] = 0;
            pathTooLong = true;
            return;
        }
        Append(text, "/");
    }
    if (!path) return;
    if (last)
        while (IsSep(*path)) {
            ++path;
            --count;
        }
    if (!*path || !count) return;
    int n = static_cast<int>(std::strlen(path));
    if (n > count) n = count;
    if (std::strlen(text) + n >= 0x103) {
        text[0] = 0;
        pathTooLong = true;
        return;
    }
    std::strncat(text, path, static_cast<size_t>(count));
}

void File::Path::AddSlash()
{
    const size_t len = std::strlen(text);
    if (len) {
        const char c = text[len - 1];
        if (c && (IsSep(c) || c == ':')) return;
    }
    if (len < 0x103) {
        std::strcat(text, "/");
        return;
    }
    text[0] = 0;
    pathTooLong = true;
}

void File::Path::DelSlash()
{
    char* last = nullptr;
    for (char* p = text; *p; ++p) last = IsSep(*p) ? p : nullptr;
    if (last) *last = 0;
}

void File::Path::DelPath()
{
    char* name = nullptr;
    char* p = text;
    for (; *p; ++p)
        if (IsSep(*p) || *p == ':') name = p + 1;
    if (name) std::memmove(text, name, p + 1 - text);
}

// cuts the file name; true cuts the separator before it too (a drive colon stays); no separator: empty
void File::Path::DelName(bool delSlash)
{
    char* cut = nullptr;
    for (char* p = text; *p; ++p) {
        if (IsSep(*p)) {
            cut = delSlash ? p : p + 1;
        } else if (*p == ':') {
            cut = p + 1;
        }
    }
    if (cut) {
        *cut = 0;
        return;
    }
    text[0] = 0;
}

// drops the name and keeps the directory and the extension ("dir/name.ext" -> "dir/.ext")
void File::Path::DelFileName()
{
    if (!*text) {
        text[0] = 0;
        return;
    }
    char* dst = text;
    char* ext = nullptr;
    for (char* p = text; *p; ++p) {
        if (IsSep(*p) || *p == ':') {
            dst = p + 1;
            ext = nullptr;
        }
        if (*p == '.') ext = p;
    }
    if (ext)
        while (*ext) *dst++ = *ext++;
    *dst = 0;
}

void File::Path::Del(unsigned int pos, unsigned int count)
{
    const unsigned int len = static_cast<unsigned int>(std::strlen(text));
    if (!count || pos >= len) return;
    char* const at = text + pos;
    if (pos + count >= len) {
        *at = 0;
        return;
    }
    std::memmove(at, at + count, std::strlen(at + count) + 1);
}

void File::Path::DelDrive()
{
    char* const colon = std::strchr(text, ':');
    if (colon) std::memmove(text, colon + 1, std::strlen(colon + 1) + 1);
}

// drops the last directory before the name
void File::Path::PrevDir()
{
    char* const last = LastSlash(text);
    if (!last) return;
    *last = 0;
    char* dir = LastSlash(text);
    if (!dir) dir = std::strrchr(text, ':');
    char* const to = dir ? dir + 1 : text;
    std::memmove(to, last + 1, std::strlen(last + 1) + 1);
}

// keeps the drive and root, then the name (the directories are dropped)
void File::Path::RootDir()
{
    char* root;
    char* const colon = std::strchr(text, ':');
    if (!colon) {
        root = IsSep(*text) ? text + 1 : text;
    } else {
        root = colon + 1;
        if (!IsSep(colon[1])) {
            char* const sep = std::strpbrk(text, "\\/");
            if (!sep) return;
            root = sep + 1;
        }
    }
    char* const last = LastSlash(text);
    if (last && last + 1 != root) std::memmove(root, last + 1, std::strlen(last + 1) + 1);
}

void File::Path::SetExt(const char* ext)
{
    char* dot = text;
    for (char* p = text; *p; ++p)
        if (*p == '.' || IsSep(*p)) dot = p;
    if (*dot == '.') *dot = 0;
    if (!ext) return;
    if (*ext != '.') std::strcat(text, ".");
    std::strcat(text, ext);
}

void File::Path::ToLower()
{
    bool skip = false;  // after a non-printable character, up to the next separator
    for (char* p = text; *p; ++p) {
        const char c = *p;
        if (IsSep(c)) {
            skip = false;
        } else if (c < 0x7f && c > 0x1f) {
            if (!skip && c > '@' && c < '[') *p = c + 0x20;
        } else {
            skip = true;
        }
    }
}

void File::Path::ToUpper()
{
    bool skip = false;
    for (char* p = text; *p; ++p) {
        const char c = *p;
        if (IsSep(c)) {
            skip = false;
        } else if (c < 0x7f && c > 0x1f) {
            if (!skip && c > '`' && c < '{') *p = c - 0x20;
        } else {
            skip = true;
        }
    }
}

void File::Path::ReplaceBackSlashes()
{
    for (char* p = text; *p; ++p)
        if (*p == '\\') *p = '/';
}

void File::Path::ReplaceForwardSlashes()
{
    for (char* p = text; *p; ++p)
        if (*p == '/') *p = '\\';
}

void File::Path::GetCwd() { Crt80::_getcwd(text, 0x104); }

void File::Path::SplitPath(char* drive, char* dir, char* name, char* ext) const
{
    Crt80::_splitpath(text, drive, dir, name, ext);
}

// this = the directory of `dir` (a drive alone gets ":" and "/") and then `name`
void File::Path::Make(const char* dir, const char* name)
{
    if (!*dir) {
        text[0] = 0;
    } else {
        if (std::strlen(dir) >= 0x104) {
            text[0] = 0;
            return;
        }
        std::strcpy(text, dir);
        const char* const colon = std::strchr(text, ':');
        char* slash = LastSlash(text);
        if (slash) *++slash = 0;
        if (*text && !slash) {
            if (*text != '.' && !colon) {
                if (std::strlen(text) + 1 >= 0x104) {
                    text[0] = 0;
                    return;
                }
                Append(text, ":");
            }
            if (std::strlen(text) + 1 >= 0x104) {
                text[0] = 0;
                return;
            }
            Append(text, "/");
        }
        if (IsSep(*name)) ++name;
    }
    if (std::strlen(text) + std::strlen(name) >= 0x104) {
        text[0] = 0;
        return;
    }
    std::strcat(text, name);
}

// "." dropped, ".." folds the previous directory (kept at the root), separators made '/'
void File::Path::Normalize()
{
    AnsiString rest(text);
    text[0] = 0;
    const char* const first = rest;
    if (first && IsSep(*first)) std::strcpy(text, "/");
    while (rest.Size() > 1) {
        AnsiString token = rest.Token("\\/");
        const char* const t = token;
        if (!t) continue;
        if (std::strcmp(t, "..") == 0) {
            const char* r = std::strchr(text, ':');
            r = r ? r + 1 : text;
            if (IsSep(*r)) ++r;
            if (*r) {
                AddSlash();
                PrevDir();
                continue;
            }
        } else if (std::strcmp(t, ".") == 0) {
            continue;
        }
        AddPath(t, static_cast<int>(std::strlen(t)));
    }
}

// the first component (a drive with its colon and root separator counts as one), cut from this
File::Path File::Path::ExtractFirstPath()
{
    Path first("");
    char* p = text;
    char* from = text;
    bool lead = true;  // at the start or just after a drive colon
    for (; *p; ++p) {
        if (*p == ':') {
            from = p + 1;
            lead = true;
        } else if (IsSep(*p)) {
            if (!lead) break;
            from = p + 1;
            lead = true;
        } else {
            lead = false;
        }
    }
    const char end = *p;
    *p = 0;
    std::strcpy(first.text, from);
    if (end) {
        std::memmove(text, p + 1, std::strlen(p + 1) + 1);
        return first;
    }
    text[0] = 0;
    return first;
}

// this made relative to the directory `base`; false when they do not share a root (or are the same with no name)
bool File::Path::MakeRelativeTo(const char* base)
{
    char fileName[0x104] = "";
    Path from(text);  // this one's directory
    Path to(base);    // the base's directory
    auto cutName = [](Path& p) {
        char* cut = nullptr;
        for (char* q = p.text; *q; ++q)
            if (IsSep(*q) || *q == ':') cut = q + 1;
        if (cut)
            *cut = 0;
        else
            p.text[0] = 0;
    };
    if (GetExt(from.text)) cutName(from);
    if (GetExt(to.text)) {
        std::strcpy(fileName, GetFileName(to.text));
        cutName(to);
    }
    from.AddSlash();
    to.AddSlash();
    Path rootFrom(from), rootTo(to);
    rootFrom.RootDir();
    rootTo.RootDir();
    int cmp;
    if (*rootFrom.text || *rootTo.text) {
        cmp = Crt80::_stricmp(rootFrom.text, rootTo.text);
    } else {
        if (!*from.text) return false;
        Path a(from), b(to);
        const Path fa = a.ExtractFirstPath();
        const Path fb = b.ExtractFirstPath();
        cmp = Crt80::_stricmp(fa.text, fb.text);
    }
    if (cmp != 0) return false;

    // drop the common directories
    char a[0x104], b[0x104];
    do {
        std::strcpy(a, from.ExtractFirstPath().text);
        std::strcpy(b, to.ExtractFirstPath().text);
        if (Crt80::_stricmp(a, b) != 0) break;
    } while (*a);

    Path out;
    if (!*a) {
        if (!*b) {
            // the same directory: just the name
            if (!*fileName) return false;
            text[0] = 0;
            if (std::strlen(fileName) >= 0x103) {
                pathTooLong = true;
                return false;
            }
            std::strcpy(text, fileName);
            return true;
        }
        out = b;
        if (*to.text) out.AddPath(to.text, static_cast<int>(std::strlen(to.text)));
    } else {
        // up from this one's remaining directories, then down the base's
        out = "../";
        while (*from.text) {
            out.AddPath("../", 3);
            from.ExtractFirstPath();
        }
        if (*b) out.AddPath(b);
        if (*to.text) out.AddPath(to.text, static_cast<int>(std::strlen(to.text)));
    }
    if (*fileName) out.AddPath(fileName);
    text[0] = 0;
    if (std::strlen(out.text) >= 0x103) {
        pathTooLong = true;
        return false;
    }
    std::strcpy(text, out.text);
    return true;
}

}  // namespace Jet
