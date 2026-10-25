// Unit U14a of KernelStandard_7c: MultiByteString (0x10032a00..0x10034373, plus the members shared with
// UnicodeString: 0x10001870, 0x1000e7a0, 0x10040b00, 0x10040e70). {data, capacity, bytes, characters}; the buffer
// comes from Alloc. The MBCS calls are msvcr80's, so they follow its multibyte code page.
#include <jet/KernelStandard.h>

#include "Crt80.h"
#include "StringFormat.h"

#include <cstring>

namespace Jet {

namespace {

typedef unsigned char uch;

inline unsigned NewCap(unsigned n) { return (n + 0x11) & ~0xfu; }

// one MBCS character as the original reads it: lead << 8 + trail, or the byte
inline unsigned CharAt(const uch* p)
{
    if (Crt80::_ismbblead(*p)) return (unsigned(p[0]) << 8) + p[1];
    return p[0];
}

}  // namespace

// ---- construction

MultiByteString::MultiByteString() {}  // 0x10032a00

MultiByteString::MultiByteString(const MultiByteString& o) { Set(o.data, o.bytes, o.chars); }  // 0x100332c0

MultiByteString::MultiByteString(const MultiByteString& o, unsigned int pos, int count)  // 0x100332f0
{
    if (pos > o.chars) return;
    if (count >= 0 && pos + count > o.chars) count = static_cast<int>(o.chars - pos);
    Set(o.data + pos, count, count);
}

MultiByteString::MultiByteString(const char* s) { Set(reinterpret_cast<const uch*>(s), -1, -1); }  // 0x10033340
MultiByteString::MultiByteString(const char* s, unsigned int n)                                   // 0x10033370
{
    Set(reinterpret_cast<const uch*>(s), -1, n);
}

// a single-byte ch fills count bytes; a double-byte one (ch >> 8 != 0) writes count {ch >> 8, ch} pairs
MultiByteString::MultiByteString(unsigned int ch, unsigned int count)  // 0x100333a0
{
    if (!count) return;
    const uch lead = static_cast<uch>(ch >> 8);
    chars = count;
    if (!lead) {
        bytes = count;
        capacity = NewCap(count);
        data = static_cast<uch*>(Jet::Alloc::New(capacity));
        std::memset(data, static_cast<int>(ch), bytes);
    } else {
        bytes = count * 2;
        capacity = NewCap(count * 2);
        data = static_cast<uch*>(Jet::Alloc::New(capacity));
        for (unsigned i = 0; i < count; ++i) {
            data[2 * i] = lead;
            data[2 * i + 1] = static_cast<uch>(ch);
        }
    }
    data[bytes] = 0;
}

MultiByteString::~MultiByteString()  // 0x10040e70
{
    if (data) Jet::Alloc::Delete(data);
    data = nullptr;
}

// ---- the buffer

uch* MultiByteString::Alloc(unsigned int size) { return static_cast<uch*>(Jet::Alloc::New(size)); }  // 0x10040b00
unsigned int MultiByteString::NewSize(unsigned int n) { return NewCap(n); }                          // 0x100331e0
unsigned int MultiByteString::AllocSize() const { return capacity; }                                 // 0x1000e7a0
unsigned int MultiByteString::Length() const { return chars; }                                       // 0x100330e0
MultiByteString::operator const char*() const { return reinterpret_cast<const char*>(data); }       // 0x10001870
const char* MultiByteString::CStr() const { return reinterpret_cast<const char*>(data); }
unsigned int MultiByteString::Size() const { return capacity ? bytes + 1 : 0; }                      // 0x10033110

unsigned int MultiByteString::GetLengthAndSize(const uch* s, unsigned int& size)  // 0x100331f0
{
    size = static_cast<unsigned>(std::strlen(reinterpret_cast<const char*>(s)));
    return size;
}

// both counts become the byte count
void MultiByteString::Set(const uch* s, int n, int fallback)  // 0x10033210
{
    if (!s) {
        capacity = 0;
        bytes = 0;
        if (data) Jet::Alloc::Delete(data);
        data = nullptr;
        chars = 0;
        return;
    }
    bytes = n;
    if (n < 0 && (n = fallback) < 0) n = static_cast<int>(std::strlen(reinterpret_cast<const char*>(s)));
    const unsigned need = NewCap(n);
    bytes = n;
    chars = n;
    if (capacity < need) {
        if (data) Jet::Alloc::Delete(data);
        capacity = need;
        data = static_cast<uch*>(Jet::Alloc::New(need));
    }
    std::memcpy(data, s, bytes);
    data[bytes] = 0;
}

bool MultiByteString::SetANSI(const char* s, int n)  // 0x10033870
{
    Set(reinterpret_cast<const uch*>(s), -1, n);
    return true;
}

bool MultiByteString::SetUnicode(const __wchar_t* s, int n)  // 0x100338f0
{
    if (n < 0) {
        const __wchar_t* p = s;
        while (*p) ++p;
        n = static_cast<int>(p - s);
    }
    const unsigned size = static_cast<unsigned>(n) * 2;
    char local[0x1000];
    char* buf = local;
    if (size > 0xffe) buf = static_cast<char*>(Jet::Alloc::New(size));
    const int r = static_cast<int>(Crt80::wcstombs(buf, reinterpret_cast<const wchar_t*>(s), size));
    if (r < 0) {
        if (data) Jet::Alloc::Delete(data);
        if (buf != local && buf) Jet::Alloc::Delete(buf);
        data = nullptr;
        bytes = 0;
        capacity = 0;
        return false;
    }
    bytes = r;
    Set(reinterpret_cast<const uch*>(buf), r, n);
    if (buf != local && buf) Jet::Alloc::Delete(buf);
    return true;
}

int MultiByteString::GetANSI(char* out, unsigned int size) const  // 0x10033890
{
    if (bytes <= size - 1) {
        std::strcpy(out, reinterpret_cast<const char*>(data));
        return static_cast<int>(bytes);
    }
    const int n = static_cast<int>(size - 1);
    if (n > 0) {
        std::memcpy(out, data, n);
        out[n] = 0;
        return n;
    }
    out[0] = 0;
    return 0;
}

int MultiByteString::GetUnicode(__wchar_t* out, unsigned int size) const  // 0x100339e0
{
    int n = static_cast<int>(chars);
    if (static_cast<int>(size - 1) < n) n = static_cast<int>(size - 1);
    const int r = static_cast<int>(
        Crt80::mbstowcs(reinterpret_cast<wchar_t*>(out), reinterpret_cast<const char*>(data), n));
    if (r < 0) return -1;
    out[r] = 0;
    return r;
}

// a longer n grows the byte count by the difference; a shorter one sets both counts to n
void MultiByteString::SetLength(unsigned int n)  // 0x10033d10
{
    if (n == chars) return;
    if (chars < n) {
        n = (bytes - chars) + n;
    } else {
        bytes = n;
        chars = n;
    }
    const unsigned need = NewCap(n);
    if (capacity < need) {
        uch* p = static_cast<uch*>(Jet::Alloc::New(need));
        if (data) {
            std::memcpy(p, data, bytes + 1);
            if (data) Jet::Alloc::Delete(data);
        }
        data = p;
        capacity = need;
    }
    data[bytes] = 0;
}

// ---- assignment

const MultiByteString& MultiByteString::operator=(const MultiByteString& o)  // 0x10033470
{
    Set(o.data, o.bytes, o.chars);
    return *this;
}

const MultiByteString& MultiByteString::operator=(const char* s)  // 0x100334a0
{
    Set(reinterpret_cast<const uch*>(s), -1, -1);
    return *this;
}

// ---- comparison: Less 0, Equal 1, Greater 2; a null text equals only another null

MultiByteString::CompareResult MultiByteString::CompareWith(const char* s) const  // 0x10033a70
{
    if (!data) return s == nullptr ? Equal : Less;
    if (!s) return Greater;
    const int r = Crt80::_mbsicmp(data, reinterpret_cast<const uch*>(s));
    if (r < 0) return Less;
    return r > 0 ? Greater : Equal;
}

MultiByteString::CompareResult MultiByteString::CompareWith(const MultiByteString& o) const  // 0x10033ab0
{
    return CompareWith(reinterpret_cast<const char*>(o.data));
}

MultiByteString::CompareResult MultiByteString::CompareWithMatchCase(const char* s) const  // 0x10033b00
{
    if (!data) return s == nullptr ? Equal : Less;
    if (!s) return Greater;
    const int r = Crt80::_mbscmp(data, reinterpret_cast<const uch*>(s));
    if (r < 0) return Less;
    return r > 0 ? Greater : Equal;
}

MultiByteString::CompareResult MultiByteString::CompareWithMatchCase(const MultiByteString& o) const  // 0x10033b40
{
    return CompareWithMatchCase(reinterpret_cast<const char*>(o.data));
}

// the operators inline CompareWithMatchCase (0x10032aa0..0x10032fd9)
bool MultiByteString::operator<(const char* s) const { return CompareWithMatchCase(s) == Less; }
bool MultiByteString::operator>(const char* s) const { return CompareWithMatchCase(s) == Greater; }
bool MultiByteString::operator==(const char* s) const { return CompareWithMatchCase(s) == Equal; }
bool MultiByteString::operator!=(const char* s) const { return CompareWithMatchCase(s) != Equal; }
bool MultiByteString::operator<=(const char* s) const { return CompareWithMatchCase(s) != Greater; }
bool MultiByteString::operator>=(const char* s) const { return CompareWithMatchCase(s) != Less; }
bool MultiByteString::operator<(const MultiByteString& o) const { return CompareWithMatchCase(o) == Less; }
bool MultiByteString::operator>(const MultiByteString& o) const { return CompareWithMatchCase(o) == Greater; }
bool MultiByteString::operator==(const MultiByteString& o) const { return CompareWithMatchCase(o) == Equal; }
bool MultiByteString::operator!=(const MultiByteString& o) const { return CompareWithMatchCase(o) != Equal; }
bool MultiByteString::operator<=(const MultiByteString& o) const { return CompareWithMatchCase(o) != Greater; }
bool MultiByteString::operator>=(const MultiByteString& o) const { return CompareWithMatchCase(o) != Less; }

bool MultiByteString::EqualsIgnoreCase(const char* s) const  // 0x10033050
{
    if (!data) return s == nullptr;
    if (!s) return false;
    return Crt80::_mbsicmp(data, reinterpret_cast<const uch*>(s)) == 0;
}

bool MultiByteString::EqualsIgnoreCase(const MultiByteString& o) const  // 0x10032fe0
{
    return EqualsIgnoreCase(reinterpret_cast<const char*>(o.data));
}

int MultiByteString::Compare(const char* s) const  // 0x10033bd0
{
    if (!data) return (s && *s) ? -1 : 0;
    if (!s) return *data != 0;
    return Crt80::_mbscmp(data, reinterpret_cast<const uch*>(s));
}

int MultiByteString::Compare(const MultiByteString& o) const  // 0x10033b90
{
    return Compare(reinterpret_cast<const char*>(o.data));
}

int MultiByteString::CompareIgnoreCase(const char* s) const  // 0x10033c50
{
    if (!data) return (s && *s) ? -1 : 0;
    if (!s) return *data != 0;
    return Crt80::_mbsicmp(data, reinterpret_cast<const uch*>(s));
}

int MultiByteString::CompareIgnoreCase(const MultiByteString& o) const  // 0x10033c10
{
    return CompareIgnoreCase(reinterpret_cast<const char*>(o.data));
}

// ---- characters and search

char& MultiByteString::operator[](int i) { return reinterpret_cast<char*>(data)[i]; }  // 0x100330d0
char& MultiByteString::operator[](unsigned int i) { return reinterpret_cast<char*>(data)[i]; }
unsigned int MultiByteString::operator[](int i) const { return data[i]; }  // 0x100330c0
unsigned int MultiByteString::operator[](unsigned int i) const { return data[i]; }

int MultiByteString::Find(unsigned int ch, unsigned int start) const  // 0x10033c90
{
    if (!data) return -1;
    const char* t = reinterpret_cast<const char*>(data);
    const char* p = std::strchr(t + start, static_cast<int>(ch));
    return p ? static_cast<int>(p - t) : -1;
}

int MultiByteString::Find(const char* s, unsigned int start) const  // 0x10033cc0
{
    if (!data) return -1;
    const char* t = reinterpret_cast<const char*>(data);
    const char* p = std::strstr(t + start, s);
    return p ? static_cast<int>(p - t) : -1;
}

int MultiByteString::FindIgnoreCase(unsigned int ch, unsigned int start) const  // 0x10033130
{
    return FindIgnoreCase(reinterpret_cast<const char*>(data), ch, start);
}

int MultiByteString::FindIgnoreCase(const char* s, unsigned int start) const  // 0x10033140
{
    return FindIgnoreCase(reinterpret_cast<const char*>(data), bytes, s, start);
}

namespace {
inline unsigned Lower(unsigned c)
{
    if (c < 0x80) return c - 0x41 < 0x1a ? (c | 0x20) : c;
    return static_cast<unsigned>(Crt80::tolower(static_cast<int>(c)));
}
inline unsigned Upper(unsigned c)
{
    if (c < 0x80) return c - 0x61 < 0x1a ? (c & ~0x20u) : c;
    return static_cast<unsigned>(Crt80::toupper(static_cast<int>(c)));
}
}  // namespace

// start counts characters; the search compares single bytes with both cases of ch
int MultiByteString::FindIgnoreCase(const char* t, unsigned int ch, unsigned int start)  // 0x10034050
{
    if (!t) return -1;
    const uch* p = Crt80::_mbsninc(reinterpret_cast<const uch*>(t), start);
    const unsigned lo = Lower(ch), hi = Upper(ch);
    for (; *p; ++p)
        if (*p == lo || *p == hi) return static_cast<int>(reinterpret_cast<const char*>(p) - t);
    return -1;
}

int MultiByteString::FindIgnoreCase(const char* t, unsigned int len, const char* s, unsigned int start)  // 0x100340f0
{
    if (!t) return -1;
    const unsigned n = static_cast<unsigned>(std::strlen(s));
    if (!n) return static_cast<int>(start);
    const uch* p = Crt80::_mbsninc(reinterpret_cast<const uch*>(t), start);
    const unsigned first = CharAt(reinterpret_cast<const uch*>(s));
    const unsigned lo = Lower(first), hi = Upper(first);
    const auto left = [&] { return len - static_cast<unsigned>(reinterpret_cast<const char*>(p) - t); };
    if (left() < n) return -1;
    while (*p) {
        while (*p != lo && *p != hi) {
            ++p;
            if (!*p) return -1;
        }
        if (Crt80::_mbsnbicmp(p, reinterpret_cast<const uch*>(s), n) == 0)
            return static_cast<int>(reinterpret_cast<const char*>(p) - t);
        ++p;
        if (left() < n) return -1;
    }
    return -1;
}

// ---- numbers: atof's double is returned unrounded (the float member is an alias of the double worker)

double __cdecl MultiByteString_GetDouble(const MultiByteString* s)  // 0x10032a50 / 0x10032a80
{
    const char* t = s->CStr();
    return t ? Crt80::atof(t) : 0.0;
}
double MultiByteString::GetDouble() const { return MultiByteString_GetDouble(this); }
float MultiByteString::GetFloat() const { return static_cast<float>(MultiByteString_GetDouble(this)); }

int MultiByteString::GetInt(int base) const  // 0x10032a10
{
    return data ? Crt80::strtol(reinterpret_cast<const char*>(data), nullptr, base) : 0;
}

unsigned int MultiByteString::GetUInt(int base) const  // 0x10032a30
{
    return data ? Crt80::strtoul(reinterpret_cast<const char*>(data), nullptr, base) : 0;
}

void MultiByteString::Set(int value, unsigned int radix)  // 0x10033a20
{
    char buf[68];
    Crt80::_itoa(value, buf, static_cast<int>(radix));
    Set(reinterpret_cast<const uch*>(buf), -1, -1);
}

// ---- editing

void MultiByteString::Left(unsigned int n)  // 0x10033cf0
{
    if (n < chars) {
        data[n] = 0;
        bytes = n;
        chars = n;
    }
}

void MultiByteString::Right(unsigned int n)  // 0x100330f0
{
    if (n < chars) Del(0, chars - n);
}

// pos counts characters, count bytes
void MultiByteString::Del(unsigned int pos, unsigned int count)  // 0x10033e60
{
    if (!count || pos >= chars) return;
    uch* p = Crt80::_mbsninc(data, pos);
    if (chars <= pos + count) {
        chars = pos;
        bytes -= static_cast<unsigned>(std::strlen(reinterpret_cast<const char*>(p)));
        *p = 0;
        return;
    }
    std::memmove(p, p + count, ((bytes - static_cast<unsigned>(p - data)) - count) + 1);
    bytes -= count;
    chars -= count;
}

void MultiByteString::Ins(const char* s, unsigned int pos)  // 0x10033d90
{
    if (!s) return;
    const unsigned n = static_cast<unsigned>(std::strlen(s));
    if (!n) return;
    const unsigned len = chars;
    if (pos > len) return;
    SetLength(len + n);
    std::memmove(data + n + pos, data + pos, (len - pos) + 1);
    std::memcpy(data + pos, s, n);
    bytes += n;
    chars += n;
}

void MultiByteString::Ins(unsigned int ch, unsigned int pos, unsigned int count)  // 0x10033e00
{
    if (!count) return;
    const unsigned len = chars;
    if (pos > len) return;
    SetLength(len + count);
    std::memmove(data + pos + count, data + pos, (len - pos) + 1);
    std::memset(data + pos, static_cast<int>(ch), count);
    chars += count;
    bytes += count;
}

MultiByteString& MultiByteString::operator+=(const char* s)  // 0x10033160
{
    Ins(s, chars);
    return *this;
}

MultiByteString& MultiByteString::operator+=(const MultiByteString& o)  // 0x10033180
{
    Ins(reinterpret_cast<const char*>(o.data), chars);
    return *this;
}

void MultiByteString::Setn(const char* s, unsigned int n)  // 0x100337f0
{
    unsigned len = static_cast<unsigned>(std::strlen(s));
    if (n <= len) len = n;
    bytes = len;
    chars = len;
    const unsigned need = NewCap(len);
    if (capacity < need) {
        capacity = need;
        if (data) Jet::Alloc::Delete(data);
        data = static_cast<uch*>(Jet::Alloc::New(capacity));
    }
    std::memcpy(data, s, bytes);
    data[bytes] = 0;
}

void MultiByteString::Strip(const char* set)  // 0x100334c0
{
    if (!bytes) return;
    uch* in = data;
    uch* out = data;
    unsigned count = 0;
    while (*in) {
        const unsigned c = CharAt(in);
        in = Crt80::_mbsinc(in);
        if (!Crt80::_mbschr(reinterpret_cast<const uch*>(set), c)) {
            if (c >> 8) *out++ = static_cast<uch>(c >> 8);
            *out++ = static_cast<uch>(c);
            ++count;
        }
    }
    chars = count;
    bytes = static_cast<unsigned>(out - data);
    data[bytes] = 0;
}

void MultiByteString::ToLower()  // 0x100331a0
{
    if (data) Crt80::_mbslwr(data);
}

void MultiByteString::ToUpper()  // 0x100331c0
{
    if (data) Crt80::_mbsupr(data);
}

// the token is copied with its character count as the byte count (Setn), and the rest moved down
MultiByteString MultiByteString::Token(const char* delimiters)  // 0x10033ed0
{
    MultiByteString r;
    const uch* set = reinterpret_cast<const uch*>(delimiters);
    uch* start = data;
    unsigned first = 0;
    while (first < chars) {
        if (!Crt80::_mbschr(set, CharAt(start))) break;
        ++first;
        start = Crt80::_mbsinc(start);
    }
    uch* end = start;
    unsigned last = first;
    while (last < chars) {
        if (Crt80::_mbschr(set, CharAt(end))) break;
        ++last;
        end = Crt80::_mbsinc(end);
    }
    if (last != first) r.Setn(reinterpret_cast<const char*>(start), last - first);
    if (last) {
        bytes += static_cast<unsigned>(data - end);
        std::memmove(data, end, bytes + 1);
        chars -= last;
    }
    return r;
}

// ---- formatting

int MultiByteString::Add(const char* fmt, char* args)  // 0x10033560
{
    StringFormat f(fmt, args);
    if (f.bytes > 0) {
        const unsigned need = NewCap(bytes + f.bytes);
        if (capacity < need) {
            capacity = need;
            uch* p = static_cast<uch*>(Jet::Alloc::New(need));
            std::memcpy(p, data, bytes);
            if (data) Jet::Alloc::Delete(data);
            data = p;
        }
        std::memcpy(data + bytes, f.text, f.bytes);
        chars += f.count;
        bytes += f.bytes;
        data[bytes] = 0;
    }
    return f.count;
}

int MultiByteString::Addf(const char* fmt, ...)  // 0x10033690
{
    va_list ap;
    va_start(ap, fmt);
    const int r = Add(fmt, ap);
    va_end(ap);
    return r;
}

void MultiByteString::Set(const char* fmt, char* args)  // 0x100336b0
{
    StringFormat f(fmt, args);
    bytes = f.bytes;
    if (f.bytes == 0) {
        chars = 0;
        if (data) data[0] = 0;
    } else {
        chars = f.count;
        const unsigned need = NewCap(f.count);
        if (capacity < need) {
            capacity = need;
            if (data) Jet::Alloc::Delete(data);
            data = static_cast<uch*>(Jet::Alloc::New(capacity));
        }
        std::memcpy(data, f.text, bytes);
        data[bytes] = 0;
    }
}

void MultiByteString::Setf(const char* fmt, ...)  // 0x100337d0
{
    va_list ap;
    va_start(ap, fmt);
    Set(fmt, ap);
    va_end(ap);
}

// ---- binary form: a 4-byte header (the byte count; bit 30 marks a UTF-16 count, halved)

bool MultiByteString::Read(BinReader& r)  // 0x10034220
{
    unsigned header = 0;
    if (r.Read(&header, 4) != 4) {
        capacity = 0;
        bytes = 0;
        if (data) Jet::Alloc::Delete(data);
        data = nullptr;
        chars = 0;
        return false;
    }
    unsigned size = header, n = header;
    if (header & 0x40000000) {
        size = header & 0xbfffffff;
        n = size >> 1;
    }
    if (!n) {
        SetLength(0);
        return true;
    }
    SetLength(n);
    if (r.Read(data, size) == size) {
        bytes = n;
        data[n] = 0;
        chars = n;
        return true;
    }
    capacity = 0;
    bytes = 0;
    if (data) Jet::Alloc::Delete(data);
    data = nullptr;
    chars = 0;
    return false;
}

bool MultiByteString::Write(BinWriter& w) const  // 0x10034320
{
    unsigned header = bytes;
    if (w.Write(&header, 4) != 4) return false;
    header &= 0xbfffffff;
    if (!header) return true;
    return w.Write(data, header) == header;
}

}  // namespace Jet
