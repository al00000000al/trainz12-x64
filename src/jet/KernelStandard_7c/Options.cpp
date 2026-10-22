// Unit U7a of KernelStandard_7c: Options (0x10035220..0x100367ad, 0x1001b3d0..0x1001bca1). Notes:
// notes/KernelStandard.md G11 and "U7a". The CRT calls go to the game's msvcr80 (Crt80.h): atof's rounding and the
// ctype / case tables are its. Strings are the original's AnsiString (U14), used through its exports; Text and
// Length read its buffer {capacity, length, chars} the way the original's inlined code does.
#include <jet/DynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/SmallTypes.h>

#include "Crt80.h"

#include <malloc.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

const char kDefaults[] =
    "-DepthBits=24 -StencilBits=8 -Jet=bin -DefaultAutoMip=none -DynamicVertexBufferSize=4194304 "
    "-DynamicIndexBufferSize=196608 -VBO -width=1024 -height=768 -framerate=200";

const unsigned* Buffer(const AnsiString& s) { return *reinterpret_cast<const unsigned* const*>(&s); }
const char* Text(const AnsiString& s)  // null for an empty buffer
{
    const unsigned* b = Buffer(s);
    return b[0] ? reinterpret_cast<const char*>(b + 2) : nullptr;
}
unsigned Length(const AnsiString& s) { return Buffer(s)[1]; }

bool IsSwitch(char c) { return c == '/' || c == '-' || c == '+'; }
bool IsQuote(char c) { return c == '\'' || c == '"'; }
bool Space(char c) { return Crt80::isspace(static_cast<unsigned char>(c)) != 0; }      // (movzx)
bool SpaceS(char c) { return Crt80::isspace(static_cast<signed char>(c)) != 0; }       // (movsx)

size_t StrNLen(const char* s, size_t n)  // 0x10025170
{
    size_t i = 0;
    while (i < n && s[i]) ++i;
    return i;
}

// 0x10035fc0: a quoted value runs to the next quote (either kind); another to the whitespace before the next
// switch, or to the end with trailing whitespace dropped
const char* ValueEnd(const char* p)
{
    if (IsQuote(*p)) {
        ++p;
        while (*p && !IsQuote(*p)) ++p;
        return p;
    }
    const char* space = nullptr;
    for (; *p; ++p) {
        if (!SpaceS(*p)) {
            space = nullptr;
            continue;
        }
        if (!space) space = p;
        if (IsSwitch(p[1])) return space;
    }
    return space ? space : p;
}

// 0x10036860: a PString of the first n characters (n clipped to the text)
void AssignN(PString& out, const char* text, size_t n)
{
    PString p;
    const size_t len = std::strlen(text);
    if (n > len) n = len;
    p.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(n));
    out = p;
}

}  // namespace

Options::Options() : head(nullptr), tail(nullptr), count(0) {}

// 0x10036040: the entry of a key, compared in lower case
Options::Node* Options::Find(const char* key) const
{
    const size_t n = std::strlen(key);
    char* const lower = static_cast<char*>(_alloca((n + 4) & ~3u));
    std::memcpy(lower, key, n + 1);
    Crt80::_strlwr(lower);
    for (Node* e = head; e; e = e->next) {
        const char* k = Text(e->key);
        if (k && std::strcmp(k, lower) == 0) return e;
    }
    return nullptr;
}

void __cdecl Options::Init(const char*, int argc, char* argv[])  // 0x10035370
{
    ProcessRaw(kDefaults);
    ProcessCmdLine(argc, argv);
}

void __cdecl Options::Init(const char*, const char* commandLine)  // 0x100353a0 (the module path is fetched, unused)
{
    char path[MAX_PATH];
    path[0] = 0;
    GetModuleFileNameA(nullptr, path, 0x100);
    ProcessRaw(kDefaults);
    ProcessRaw(commandLine ? commandLine : "");
}

void __cdecl Options::ProcessCmdLine(int argc, char* argv[])  // 0x100357e0: the arguments joined by spaces
{
    size_t size = 0;
    for (int i = 0; i < argc; ++i) size += std::strlen(argv[i]) + 1;
    char* const line = static_cast<char*>(Alloc::New(static_cast<unsigned>(size + 1)));
    char* p = line;
    for (int i = 0; i < argc; ++i) {
        const size_t n = std::strlen(argv[i]);
        std::memcpy(p, argv[i], n);
        p += n;
        *p++ = ' ';
    }
    *p = 0;
    ProcessRaw(line);
    if (line) Alloc::Delete(line);
}

// 0x100358a0: read whole; ANSI copied (encoding 1), anything else taken as UTF-16 and converted
void __cdecl Options::ProcessFile(const char* path)
{
    File f;
    if (f.Open(path, 0x10, 0).code == 0) {
        const unsigned long size = f.Size();
        f.DetectEncoding();
        if (size) {
            char* const raw = static_cast<char*>(Alloc::New(size + 1));
            char* const text = static_cast<char*>(Alloc::New(size + 1));
            std::memset(text, 0, size + 1);
            f.Read(raw, size, 1);
            raw[size] = 0;
            if (f.GetEncoding() == 1)
                Crt80::strncpy(text, raw, size + 1);
            else
                Crt80::wcstombs(text, reinterpret_cast<const wchar_t*>(raw), size + 1);
            if (raw) Alloc::Delete(raw);
            ProcessRaw(text);
            if (text) Alloc::Delete(text);
        }
        f.Close();
    }
}

void __cdecl Options::ProcessEnv(const char* variable)  // 0x10035a20
{
    if (const char* v = Crt80::getenv(variable)) ProcessRaw(v);
}

// 0x10035a40: [switch] key [= value | - | +] ...; a key repeats its entry ("+key" appends ","value to a value that
// is not empty, anything else replaces it, a key alone with no value)
void __cdecl Options::ProcessRaw(const char* line)
{
    String key, value;
    const char* p = line;
    while (*p) {
        bool haveKey = false;
        key = String();
        value = String();
        while (*p && Space(*p)) ++p;
        const char sw = IsSwitch(*p) ? *p : 0;
        const char* const start = IsSwitch(*p) ? p + 1 : p;
        const char* q = start;
        while (*q && !SpaceS(*q) && *q != '=' && *q != '-' && *q != '+') ++q;
        p = start;
        if (q - start > 0) {
            key.Setn(start, static_cast<unsigned>(StrNLen(start, q - start)));
            p = q;
            haveKey = true;
        }
        const char* const beforeSpace = p;
        while (*p && Space(*p)) ++p;
        if (*p == '=') {
            ++p;
            while (*p && Space(*p)) ++p;
            const char* const end = ValueEnd(p);
            const char* const v = IsQuote(*p) ? p + 1 : p;
            if (end - v > 0) value.Setn(v, static_cast<unsigned>(StrNLen(v, end - v)));
            p = IsQuote(*end) ? end + 1 : end;
        } else if ((*p == '-' || *p == '+') && beforeSpace == p) {
            value.Setn(p, static_cast<unsigned>(StrNLen(p, 1)));
            ++p;
        }
        if (haveKey) {
            Node* e = Find(Text(key));
            if (!e) {
                void* const mem = Alloc::New(sizeof(Node));
                if (mem) {
                    e = ::new (mem) Node{nullptr, nullptr, String(), String(), String(), nullptr};
                    e->key = Text(key);
                    if (Length(e->key)) {
                        e->key.MakeBufferUnique(0, true);
                        Crt80::_strlwr(const_cast<char*>(Text(e->key)));
                    }
                    if (tail) {
                        e->prev = tail;
                        tail->next = e;
                    } else {
                        head = e;
                    }
                    ++count;
                    tail = e;
                }
            }
            if (sw == '+' && Length(e->value) > 0) {
                e->value.Ins(",", Length(e->value));
                e->value.Ins(Text(value), Length(e->value));
            } else {
                e->value = value;
            }
        }
    }
}

void __cdecl Options::Dump()  // 0x100360f0
{
    for (Node* e = head; e; e = e->next) Crt80::printf("[%s] = [%s]\n", Text(e->key), Text(e->value));
}

// 0x10036140: GetNext starts again at the value's start
void __cdecl Options::SetSeparators(const char* key, const char* separators) const
{
    if (Node* e = Find(key)) {
        e->separators = separators;
        e->cursor = Text(e->value);
    }
}

// 0x10036180 / 0x10036270: the run up to the next separator, trailing whitespace dropped; then past the following
// whitespace and separators. An empty run ends the walk.
namespace {
template <class Out>
bool NextRun(Options const*, const char*& cursor, const String& separators, Out& out, void (*assign)(Out&, const char*, size_t))
{
    if (!cursor) return false;
    const char* const seps = Text(separators);
    const char* end = cursor + std::strcspn(cursor, seps);
    if (cursor >= end) return false;
    const char* last = end - 1;
    while (last > cursor && SpaceS(*last)) --last;
    assign(out, cursor, StrNLen(cursor, last - cursor) + 1);
    const char* p = end;
    while (*p && (Space(*p) || std::strchr(Text(separators), static_cast<unsigned char>(*p)))) ++p;
    cursor = p;
    return true;
}
void AssignString(String& out, const char* text, size_t n) { out.Setn(text, static_cast<unsigned>(n)); }
void AssignPString(PString& out, const char* text, size_t n) { AssignN(out, text, n); }
}  // namespace

bool __cdecl Options::GetNext(const char* key, String& value) const
{
    Node* const e = Find(key);
    return e && NextRun(this, e->cursor, e->separators, value, &AssignString);
}

bool __cdecl Options::GetNext(const char* key, PString& value) const
{
    Node* const e = Find(key);
    return e && NextRun(this, e->cursor, e->separators, value, &AssignPString);
}

// 0x1001b590: false/off false, true/on true, otherwise a number (the run in lower case)
bool __cdecl Options::GetNext(const char* key, bool& value) const
{
    PString s;
    if (!GetNext(key, s)) return false;
    if (s.node && s.node->length) {
        PStringCache* const cache = PString::Cache();
        cache->Lock();
        PStringCache::Node* const u = cache->GetUnique(s.node, s.node->length);
        Crt80::_strlwr(reinterpret_cast<char*>(u->text));
        s.node = cache->Insert(u);
        cache->Unlock();
    }
    if (s.Compare("false") == 0 || s.Compare("off") == 0)
        value = false;
    else if (s.Compare("true") == 0 || s.Compare("on") == 0)
        value = true;
    else
        value = (s.node ? Crt80::strtol(reinterpret_cast<const char*>(s.node->text), nullptr, 10) : 0) != 0;
    return true;
}

bool __cdecl Options::GetNext(const char* key, unsigned int& value) const  // 0x1001b700 (also unsigned long)
{
    PString s;
    if (!GetNext(key, s)) return false;
    value = s.node ? Crt80::strtoul(reinterpret_cast<const char*>(s.node->text), nullptr, 10) : 0;
    return true;
}

bool __cdecl Options::GetNext(const char* key, unsigned long& value) const
{
    return GetNext(key, reinterpret_cast<unsigned int&>(value));
}

bool __cdecl Options::GetNext(const char* key, int& value) const  // 0x1001b810 (also long)
{
    PString s;
    if (!GetNext(key, s)) return false;
    value = s.node ? Crt80::strtol(reinterpret_cast<const char*>(s.node->text), nullptr, 10) : 0;
    return true;
}

bool __cdecl Options::GetNext(const char* key, long& value) const { return GetNext(key, reinterpret_cast<int&>(value)); }

bool __cdecl Options::GetNext(const char* key, float& value) const  // 0x1001b920
{
    PString s;
    if (!GetNext(key, s)) return false;
    value = s.node ? static_cast<float>(Crt80::atof(reinterpret_cast<const char*>(s.node->text))) : 0.0f;
    return true;
}

bool __cdecl Options::GetNext(const char* key, Vector2& v) const  // 0x1001ba30
{
    float* const f = reinterpret_cast<float*>(&v);
    return GetNext(key, f[0]) && GetNext(key, f[1]);
}

bool __cdecl Options::GetNext(const char* key, Vector3& v) const  // 0x1001ba70
{
    float* const f = reinterpret_cast<float*>(&v);
    return GetNext(key, f[0]) && GetNext(key, f[1]) && GetNext(key, f[2]);
}

bool __cdecl Options::GetNext(const char* key, Vector4& v) const  // 0x1001bad0
{
    float* const f = reinterpret_cast<float*>(&v);
    return GetNext(key, f[0]) && GetNext(key, f[1]) && GetNext(key, f[2]) && GetNext(key, f[3]);
}

// 0x1001bb40: three angles, through Quaternion::Set
bool __cdecl Options::GetNext(const char* key, Quaternion& q) const
{
    float a, b, c;
    if (!GetNext(key, a) || !GetNext(key, b) || !GetNext(key, c)) return false;
    Quaternion t;
    t.Set(a, b, c);
    q = t;
    return true;
}

bool __cdecl Options::GetNext(const char* key, Color& c) const  // 0x1001bbf0
{
    float f[4];
    if (!GetNext(key, *reinterpret_cast<Vector4*>(f))) return false;
    std::memcpy(&c, f, sizeof(f));
    return true;
}

// ---- Get: the fallback when the key is missing (the result false), or the value parsed (true)
namespace {
// the fallbacks are copied through the x87 stack (fld / fstp), which quiets a signalling NaN
void CopyFloats(void* dst, const void* src, int n)
{
    for (int i = 0; i < n; ++i)
        static_cast<float*>(dst)[i] = static_cast<float>(static_cast<double>(static_cast<const float*>(src)[i]));
}
}  // namespace

bool __cdecl Options::Get(const char* key) const { return Find(key) != nullptr; }

bool __cdecl Options::Get(const char* key, PString& value) const  // 0x10035440
{
    Node* const e = Find(key);
    if (!e) return false;
    value = PString(Text(e->value));
    return true;
}

bool __cdecl Options::Get(const char* key, String& value) const  // 0x10035490
{
    Node* const e = Find(key);
    if (!e) return false;
    value = e->value;
    return true;
}

bool __cdecl Options::Get(const char* key, PString& value, const PString& fallback) const  // 0x100354c0
{
    Node* const e = Find(key);
    if (!e) {
        value = fallback;
        return false;
    }
    value = PString(Text(e->value));
    return true;
}

bool __cdecl Options::Get(const char* key, String& value, const String& fallback) const  // 0x10035520
{
    Node* const e = Find(key);
    if (!e) {
        value = fallback;
        return false;
    }
    const String copy(e->value);
    value = copy;
    return true;
}

// 0x100355d0: "-..." / false / off / 0 false; "+..." / true / on / 1 true; no value or anything else: !fallback
bool __cdecl Options::Get(const char* key, bool& value, bool fallback) const
{
    Node* const e = Find(key);
    if (!e) {
        value = fallback;
        return false;
    }
    const char* const t = Text(e->value);
    if (!t) {
        value = !fallback;
        return true;
    }
    if (*t == '-' || Crt80::_stricmp(t, "false") == 0 || Crt80::_stricmp(t, "off") == 0 || Crt80::_stricmp(t, "0") == 0) {
        value = false;
        return true;
    }
    if (*t == '+' || Crt80::_stricmp(t, "true") == 0 || Crt80::_stricmp(t, "on") == 0 || Crt80::_stricmp(t, "1") == 0) {
        value = true;
        return true;
    }
    value = !fallback;
    return true;
}

bool __cdecl Options::Get(const char* key, int& value, int fallback) const  // 0x100356f0 (all four int types)
{
    Node* const e = Find(key);
    if (!e) {
        value = fallback;
        return false;
    }
    const char* const t = Text(e->value);
    value = t ? Crt80::atoi(t) : 0;
    return true;
}

bool __cdecl Options::Get(const char* key, unsigned int& value, unsigned int fallback) const
{
    return Get(key, reinterpret_cast<int&>(value), static_cast<int>(fallback));
}
bool __cdecl Options::Get(const char* key, long& value, long fallback) const
{
    return Get(key, reinterpret_cast<int&>(value), static_cast<int>(fallback));
}
bool __cdecl Options::Get(const char* key, unsigned long& value, unsigned long fallback) const
{
    return Get(key, reinterpret_cast<int&>(value), static_cast<int>(fallback));
}

bool __cdecl Options::Get(const char* key, float& value, float fallback) const  // 0x10035760
{
    Node* const e = Find(key);
    if (!e) {
        value = fallback;
        return false;
    }
    const char* const t = Text(e->value);
    value = t ? static_cast<float>(Crt80::atof(t)) : 0.0f;
    return true;
}

bool __cdecl Options::Get(const char* key, Vector2& value, const Vector2& fallback) const  // 0x1001b3d0
{
    if (GetNext(key, value)) return true;
    CopyFloats(&value, &fallback, 2);
    return false;
}

bool __cdecl Options::Get(const char* key, Vector3& value, const Vector3& fallback) const  // 0x1001b420
{
    if (GetNext(key, value)) return true;
    CopyFloats(&value, &fallback, 3);
    return false;
}

bool __cdecl Options::Get(const char* key, Vector4& value, const Vector4& fallback) const  // 0x1001b490
{
    if (GetNext(key, value)) return true;
    CopyFloats(&value, &fallback, 4);
    return false;
}

bool __cdecl Options::Get(const char* key, Quaternion& value, const Quaternion& fallback) const  // 0x1001b4d0
{
    if (GetNext(key, value)) return true;
    CopyFloats(&value, &fallback, 4);
    return false;
}

bool __cdecl Options::Get(const char* key, Color& value, const Color& fallback) const  // 0x1001b510
{
    float f[4];
    if (GetNext(key, *reinterpret_cast<Vector4*>(f))) {
        std::memcpy(&value, f, sizeof(f));
        return true;
    }
    CopyFloats(&value, &fallback, 4);
    return false;
}

// 0x10036370: every run, appended; true when the array has any (it may have had some before)
bool __cdecl Options::Get(const char* key, DynamicArray<String, 41, Less<String, String>>& values, const char* separators) const
{
    if (Find(key)) {
        SetSeparators(key, separators);
        String s;
        while (GetNext(key, s)) values.Add(s);
    }
    return values.head != nullptr;
}

// 0x10036540 splits "a:b" (b not empty)
bool __cdecl Options::Get(const char* key, Range<float>& value) const  // 0x100365b0
{
    Node* const e = Find(key);
    if (!e) return false;
    const String copy(e->value);
    const char* const t = Text(copy);
    const char* const colon = std::strchr(t, ':');
    if (!colon || !colon[1]) return false;
    value.min = static_cast<float>(Crt80::atof(t));
    value.max = static_cast<float>(Crt80::atof(colon + 1));
    return true;
}

bool __cdecl Options::Get(const char* key, Range<int>& value) const  // 0x100366b0
{
    Node* const e = Find(key);
    if (!e) return false;
    const String copy(e->value);
    const char* const t = Text(copy);
    const char* const colon = std::strchr(t, ':');
    if (!colon || !colon[1]) return false;
    value.min = Crt80::atoi(t);
    value.max = Crt80::atoi(colon + 1);
    return true;
}

}  // namespace Jet
