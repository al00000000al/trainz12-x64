// Unit U14a of KernelStandard_7c: UnicodeString (0x10001440..0x10001876, 0x10040b00..0x10041f42, 0x1000e670,
// 0x1000e7a0). {text, capacity in bytes, length in characters}; the buffer comes from Alloc (still the original's
// until U14b). The CRT calls are msvcr80's: case mapping, MBCS conversion and number parsing follow its locale.
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <cstring>
#include <cwchar>

namespace Jet {

namespace {

typedef __wchar_t wch;

inline unsigned NewCap(unsigned n) { return (n * 2 + 0x12) & ~0xfu; }

inline unsigned WLen(const wch* s)
{
    const wch* p = s;
    while (*p) ++p;
    return static_cast<unsigned>(p - s);
}

inline wchar_t* W(const wch* s) { return reinterpret_cast<wchar_t*>(const_cast<wch*>(s)); }

// an AnsiString block: {capacity, length, chars} after the reference count
inline const unsigned* Block(const void* data) { return static_cast<const unsigned*>(data); }

// the vswprintf buffer of Add / Set (0x10041f50): 0x400 characters on the stack, then growing heap buffers
struct WFormat {
    wchar_t* text;
    wchar_t local[0x400];
    int bytes;
    int count;

    WFormat(const wch* fmt, va_list args)
    {
        int n = Crt80::_vsnwprintf(local, 0x3ff, W(fmt), args);
        if (n >= 0) {
            local[n] = 0;
            text = local;
            count = n;
            bytes = n * 2;
            return;
        }
        unsigned size = 0x8000;
        text = static_cast<wchar_t*>(Alloc::New(0x10000));
        n = Crt80::_vsnwprintf(text, 0x7fff, W(fmt), args);
        while (n <= 0) {
            if (text) Alloc::Delete(text);
            size *= 4;
            if (static_cast<int>(size) > 0x7ffffff) {
                text = local;
                local[0] = 0;
                count = 0;
                bytes = 0;
                return;
            }
            text = static_cast<wchar_t*>(Alloc::New(size * 2));
            n = Crt80::_vsnwprintf(text, size - 1, W(fmt), args);
        }
        text[n] = 0;
        count = n;
        bytes = n * 2;
    }
    ~WFormat()
    {
        if (text != local && text) Alloc::Delete(text);
    }
};

}  // namespace

// ---- construction

UnicodeString::UnicodeString() {}  // 0x10001440

UnicodeString::UnicodeString(const UnicodeString& o)  // 0x10040bc0
{
    Set(reinterpret_cast<const wch*>(o.text), o.length, o.length);
}

UnicodeString::UnicodeString(const PString& s)  // 0x10040bf0
{
    const PStringCache::Node* node = s.node;
    capacity = NewCap(node ? node->length : 0);
    text = static_cast<wchar_t*>(Jet::Alloc::New(capacity));
    int n;
    if (capacity < 2 || !node) {
        text[0] = 0;
        n = 0;
    } else {
        n = static_cast<int>(node->length);
        if (static_cast<int>(capacity - 1) < n) n = static_cast<int>(capacity - 1);
        n = static_cast<int>(Crt80::mbstowcs(text, node->text, n));
        if (n < 0)
            n = -1;
        else
            text[n] = 0;
    }
    length = n;
    if (n == -1) {
        if (text) Jet::Alloc::Delete(text);
        text = nullptr;
        capacity = 0;
        length = 0;
    }
}

UnicodeString::UnicodeString(const AnsiString& s)  // 0x10040ca0 (also the String constructor)
{
    const unsigned* b = Block(s.data);
    capacity = NewCap(b[1]);
    text = static_cast<wchar_t*>(Jet::Alloc::New(capacity));
    unsigned max = b[1];
    if (capacity >> 1 <= max) max = (capacity >> 1) - 1;
    int n = static_cast<int>(Crt80::mbstowcs(text, reinterpret_cast<const char*>(b + 2), max));
    if (n < 0) {
        text[0] = 0;
        n = -1;
    } else {
        text[n] = 0;
    }
    length = n;
    if (n == -1) {
        if (text) Jet::Alloc::Delete(text);
        text = nullptr;
        capacity = 0;
        length = 0;
    }
}

UnicodeString::UnicodeString(const String& s) : UnicodeString(static_cast<const AnsiString&>(s)) {}

UnicodeString::UnicodeString(const char* s) { SetANSI(s, -1); }                  // 0x10040d30
UnicodeString::UnicodeString(const char* s, unsigned int n) { SetANSI(s, n); }    // 0x10040d90
UnicodeString::UnicodeString(const wch* s) { Set(s, -1, -1); }                    // 0x10040d60
UnicodeString::UnicodeString(const wch* s, unsigned int n) { Set(s, -1, n); }     // 0x10040dc0

UnicodeString::UnicodeString(unsigned int ch, unsigned int count)  // 0x10040df0
{
    if (!count) return;
    length = count;
    capacity = NewCap(count);
    text = static_cast<wchar_t*>(Jet::Alloc::New(capacity));
    for (unsigned i = 0; i < count; ++i) text[i] = static_cast<wchar_t>(ch);
    text[length] = 0;
}

UnicodeString::~UnicodeString()  // 0x10040e70
{
    if (text) Jet::Alloc::Delete(text);
    text = nullptr;
}

// ---- the buffer

wch* UnicodeString::Alloc(unsigned int size) { return static_cast<wch*>(Jet::Alloc::New(size)); }  // 0x10040b00
unsigned int UnicodeString::NewSize(unsigned int n) { return NewCap(n); }                          // 0x10001850
unsigned int UnicodeString::AllocSize() const { return capacity; }                                 // 0x1000e7a0
unsigned int UnicodeString::Length() const { return length; }                                      // 0x1000e670
UnicodeString::operator const wch*() const { return reinterpret_cast<const wch*>(text); }         // 0x10001870
unsigned int UnicodeString::Size() const { return capacity ? length * 2 + 2 : 0; }                 // 0x10001780

void UnicodeString::Set(const wch* s, int n, int fallback)  // 0x10040b10
{
    if (!s) {
        capacity = 0;
        length = 0;
        if (text) Jet::Alloc::Delete(text);
        text = nullptr;
        return;
    }
    length = n;
    if (n < 0 && (n = fallback) < 0) n = static_cast<int>(WLen(s));
    const unsigned need = NewCap(n);
    length = n;
    if (capacity < need) {
        if (text) Jet::Alloc::Delete(text);
        capacity = need;
        text = static_cast<wchar_t*>(Jet::Alloc::New(need));
    }
    std::memcpy(text, s, length * 2);
    text[length] = 0;
}

bool UnicodeString::SetANSI(const char* s, int n)  // 0x100410a0
{
    if (n < 0) n = static_cast<int>(Crt80::_mbslen(reinterpret_cast<const unsigned char*>(s)));
    const unsigned need = NewCap(n);
    if (capacity < need) {
        capacity = need;
        if (text) Jet::Alloc::Delete(text);
        text = static_cast<wchar_t*>(Jet::Alloc::New(capacity));
    }
    const int r = static_cast<int>(Crt80::mbstowcs(text, s, n));
    length = r;
    if (r < 0) {
        if (text) Jet::Alloc::Delete(text);
        text = nullptr;
        length = 0;
        capacity = 0;
        return false;
    }
    text[r] = 0;
    return true;
}

bool UnicodeString::SetUnicode(const wch* s, int n)  // 0x10041170
{
    if (n < 0) n = static_cast<int>(WLen(s));
    Set(s, n, n);  // the second length is a leftover register; only the first is used for n >= 0
    return true;
}

int UnicodeString::GetANSI(char* out, unsigned int size) const  // 0x10041140
{
    const int n = static_cast<int>(Crt80::wcstombs(out, text, size - 1));
    if (n < 0) {
        out[0] = 0;
        return -1;
    }
    out[n] = 0;
    return n;
}

int UnicodeString::GetUnicode(wch* out, unsigned int size) const  // 0x100411b0
{
    if (size < 2) return -1;
    unsigned n = (size >> 1) - 1;
    if (length < n) n = length;
    if (n) std::memcpy(out, text, n * 2);
    out[n] = 0;
    return static_cast<int>(n);
}

void UnicodeString::SetLength(unsigned int n)  // 0x10041900: grows the buffer, never the length
{
    const unsigned need = NewCap(n);
    if (length <= n) n = length;
    length = n;
    if (capacity < need) {
        wchar_t* p = static_cast<wchar_t*>(Jet::Alloc::New(need));
        if (text) {
            std::memcpy(p, text, length * 2 + 2);
            if (text) Jet::Alloc::Delete(text);
        }
        text = p;
        capacity = need;
    }
    text[length] = 0;
}

// ---- assignment

const UnicodeString& UnicodeString::operator=(const UnicodeString& o)  // 0x10040ed0
{
    Set(reinterpret_cast<const wch*>(o.text), o.length, o.length);
    return *this;
}

const UnicodeString& UnicodeString::operator=(const AnsiString& s)  // 0x10040e90
{
    const unsigned* b = Block(s.data);
    SetANSI(b[0] ? reinterpret_cast<const char*>(b + 2) : nullptr, -1);
    return *this;
}

const UnicodeString& UnicodeString::operator=(const wch* s)  // 0x10041200
{
    Set(s, -1, -1);
    return *this;
}

const UnicodeString& UnicodeString::operator=(const PString& s)  // 0x10040ef0
{
    const PStringCache::Node* node = s.node;
    if (!node) {  // (and &node->text against null)
        capacity = 0;
        length = 0;
        if (text) Jet::Alloc::Delete(text);
        text = nullptr;
        return *this;
    }
    const unsigned need = NewCap(node->length);
    if (capacity < need) {
        if (text) Jet::Alloc::Delete(text);
        capacity = need;
        text = static_cast<wchar_t*>(Jet::Alloc::New(need));
    }
    int n;
    if (capacity < 2) {
        text[0] = 0;
        n = 0;
    } else {
        n = static_cast<int>(node->length);
        if (static_cast<int>(capacity - 1) < n) n = static_cast<int>(capacity - 1);
        n = static_cast<int>(Crt80::mbstowcs(text, node->text, n));
        if (n < 0)
            n = -1;
        else
            text[n] = 0;
    }
    length = n;
    if (n == -1) {
        if (text) Jet::Alloc::Delete(text);
        capacity = 0;
        length = 0;
        text = nullptr;
    }
    return *this;
}

const UnicodeString& UnicodeString::operator=(const String& s)  // 0x10040fd0
{
    const unsigned* b = Block(s.data);
    if (b[0] == 0) {  // (the original also tests the characters' address against null)
        capacity = 0;
        length = 0;
        if (text) Jet::Alloc::Delete(text);
        text = nullptr;
        return *this;
    }
    const unsigned need = NewCap(b[1]);
    if (capacity < need) {
        if (text) Jet::Alloc::Delete(text);
        capacity = need;
        text = static_cast<wchar_t*>(Jet::Alloc::New(need));
    }
    unsigned max = Block(s.data)[1];
    if (capacity >> 1 <= max) max = (capacity >> 1) - 1;
    int n = static_cast<int>(Crt80::mbstowcs(text, reinterpret_cast<const char*>(Block(s.data) + 2), max));
    if (n < 0) {
        text[0] = 0;
        n = -1;
    } else {
        text[n] = 0;
    }
    length = n;
    if (n == -1) {
        if (text) Jet::Alloc::Delete(text);
        capacity = 0;
        length = 0;
        text = nullptr;
    }
    return *this;
}

// ---- comparison: Less 0, Equal 1, Greater 2; a null text equals only another null

UnicodeString::CompareResult UnicodeString::CompareWith(const wch* s) const  // 0x100415d0
{
    if (!text) return s == nullptr ? Equal : Less;
    if (!s) return Greater;
    const int r = Crt80::_wcsicmp(text, W(s));
    if (r < 0) return Less;
    return r > 0 ? Greater : Equal;
}

UnicodeString::CompareResult UnicodeString::CompareWith(const UnicodeString& o) const  // 0x10041610
{
    return CompareWith(reinterpret_cast<const wch*>(o.text));
}

UnicodeString::CompareResult UnicodeString::CompareWithMatchCase(const wch* s) const  // 0x10041660
{
    if (!text) return s == nullptr ? Equal : Less;
    if (!s) return Greater;
    const unsigned short* a = text;
    const unsigned short* b = reinterpret_cast<const unsigned short*>(s);
    for (;; ++a, ++b) {
        if (*a != *b) return *a < *b ? Less : Greater;
        if (!*a) return Equal;
    }
}

UnicodeString::CompareResult UnicodeString::CompareWithMatchCase(const UnicodeString& o) const  // 0x100416c0
{
    return CompareWithMatchCase(reinterpret_cast<const wch*>(o.text));
}

bool UnicodeString::operator<(const wch* s) const { return CompareWithMatchCase(s) == Less; }
bool UnicodeString::operator>(const wch* s) const { return CompareWithMatchCase(s) == Greater; }
bool UnicodeString::operator==(const wch* s) const { return CompareWithMatchCase(s) == Equal; }
bool UnicodeString::operator!=(const wch* s) const { return CompareWithMatchCase(s) != Equal; }
bool UnicodeString::operator<=(const wch* s) const { return CompareWithMatchCase(s) != Greater; }
bool UnicodeString::operator>=(const wch* s) const { return CompareWithMatchCase(s) != Less; }
bool UnicodeString::operator<(const UnicodeString& o) const { return CompareWithMatchCase(o) == Less; }
bool UnicodeString::operator>(const UnicodeString& o) const { return CompareWithMatchCase(o) == Greater; }
bool UnicodeString::operator==(const UnicodeString& o) const { return CompareWithMatchCase(o) == Equal; }
bool UnicodeString::operator!=(const UnicodeString& o) const { return CompareWithMatchCase(o) != Equal; }
bool UnicodeString::operator<=(const UnicodeString& o) const { return CompareWithMatchCase(o) != Greater; }
bool UnicodeString::operator>=(const UnicodeString& o) const { return CompareWithMatchCase(o) != Less; }

bool UnicodeString::EqualsIgnoreCase(const wch* s) const  // 0x100416d0
{
    if (!text) return s == nullptr;
    if (!s) return false;
    return Crt80::_wcsicmp(text, W(s)) == 0;
}

bool UnicodeString::EqualsIgnoreCase(const UnicodeString& o) const  // 0x10001660
{
    return EqualsIgnoreCase(reinterpret_cast<const wch*>(o.text));
}

// -1 / 0 / 1 (an empty text equals null), the case-insensitive ones return _wcsicmp itself
int UnicodeString::Compare(const wch* s) const  // 0x10041790
{
    if (!text) return (s && *s) ? -1 : 0;
    if (!s) return *text != 0;
    const unsigned short* a = text;
    const unsigned short* b = reinterpret_cast<const unsigned short*>(s);
    for (;; ++a, ++b) {
        if (*a != *b) return *a < *b ? -1 : 1;
        if (!*a) return 0;
    }
}

int UnicodeString::Compare(const UnicodeString& o) const  // 0x10041730
{
    return Compare(reinterpret_cast<const wch*>(o.text));
}

int UnicodeString::CompareIgnoreCase(const wch* s) const  // 0x10041840
{
    if (!text) return (s && *s) ? -1 : 0;
    if (!s) return *text != 0;
    return Crt80::_wcsicmp(text, W(s));
}

int UnicodeString::CompareIgnoreCase(const UnicodeString& o) const  // 0x100417f0
{
    return CompareIgnoreCase(reinterpret_cast<const wch*>(o.text));
}

// ---- characters and search

wch& UnicodeString::operator[](int i) { return reinterpret_cast<wch*>(text)[i]; }                 // 0x10001750
wch& UnicodeString::operator[](unsigned int i) { return reinterpret_cast<wch*>(text)[i]; }
unsigned int UnicodeString::operator[](int i) const { return text[i]; }                          // 0x10001740
unsigned int UnicodeString::operator[](unsigned int i) const { return text[i]; }

int UnicodeString::Find(unsigned int ch, unsigned int start) const  // 0x10041880
{
    if (!text) return -1;
    const wchar_t* p = std::wcschr(text + start, static_cast<wchar_t>(ch));
    return p ? static_cast<int>(p - text) : -1;
}

int UnicodeString::Find(const wch* s, unsigned int start) const  // 0x100418b0
{
    if (!text) return -1;
    const wchar_t* p = std::wcsstr(text + start, W(s));
    return p ? static_cast<int>(p - text) : -1;
}

int UnicodeString::FindIgnoreCase(unsigned int ch, unsigned int start) const  // 0x100017a0
{
    return FindIgnoreCase(reinterpret_cast<const wch*>(text), ch, start);
}

int UnicodeString::FindIgnoreCase(const wch* s, unsigned int start) const  // 0x100017b0
{
    return FindIgnoreCase(reinterpret_cast<const wch*>(text), length, s, start);
}

namespace {
inline unsigned Lower(unsigned c)
{
    if (c < 0x80) return c - 0x41 < 0x1a ? (c | 0x20) : c;
    return Crt80::towlower(static_cast<unsigned short>(c));
}
inline unsigned Upper(unsigned c)
{
    if (c < 0x80) return c - 0x61 < 0x1a ? (c & ~0x20u) : c;
    return Crt80::towupper(static_cast<unsigned short>(c));
}
}  // namespace

int UnicodeString::FindIgnoreCase(const wch* t, unsigned int ch, unsigned int start)  // 0x10041bb0
{
    if (!t) return -1;
    const unsigned short* p = reinterpret_cast<const unsigned short*>(t) + start;
    const unsigned lo = Lower(ch), hi = Upper(ch);
    for (; *p; ++p)
        if (*p == lo || *p == hi) return static_cast<int>(p - reinterpret_cast<const unsigned short*>(t));
    return -1;
}

int UnicodeString::FindIgnoreCase(const wch* t, unsigned int len, const wch* s, unsigned int start)  // 0x10041c50
{
    if (!t) return -1;
    const unsigned n = WLen(s);
    if (!n) return static_cast<int>(start);
    const unsigned short* base = reinterpret_cast<const unsigned short*>(t);
    const unsigned first = static_cast<unsigned short>(s[0]);
    const unsigned lo = Lower(first), hi = Upper(first);
    const unsigned short* p = base + start;
    if (len - static_cast<unsigned>(p - base) < n) return -1;
    while (*p) {
        while (*p != lo && *p != hi) {
            ++p;
            if (!*p) return -1;
        }
        if (Crt80::_wcsnicmp(reinterpret_cast<const wchar_t*>(p), W(s), n) == 0)
            return static_cast<int>(p - base);
        ++p;
        if (len - static_cast<unsigned>(p - base) < n) return -1;
    }
    return -1;
}

// ---- numbers

double __cdecl UnicodeString_GetDouble(const UnicodeString* s)  // 0x10001490 / 0x100014c0: wcstod, unrounded
{
    const wchar_t* t = reinterpret_cast<const wchar_t*>(static_cast<const wch*>(*s));
    return t ? Crt80::wcstod(t, nullptr) : 0.0;
}
double UnicodeString::GetDouble() const { return UnicodeString_GetDouble(this); }
float UnicodeString::GetFloat() const { return static_cast<float>(UnicodeString_GetDouble(this)); }

int UnicodeString::GetInt(int base) const { return text ? Crt80::wcstol(text, nullptr, base) : 0; }  // 0x10001450
unsigned int UnicodeString::GetUInt(int base) const  // 0x10001470
{
    return text ? Crt80::wcstoul(text, nullptr, base) : 0;
}

void UnicodeString::Set(int value, unsigned int radix)  // 0x10041570
{
    wchar_t buf[66];
    Crt80::_itow(value, buf, static_cast<int>(radix));
    Set(reinterpret_cast<const wch*>(buf), -1, -1);
}

// ---- editing

void UnicodeString::Left(unsigned int n)  // 0x100418e0
{
    if (n < length) {
        text[n] = 0;
        length = n;
    }
}

void UnicodeString::Right(unsigned int n)  // 0x10001760
{
    if (n < length) Del(0, length - n);
}

void UnicodeString::Del(unsigned int pos, unsigned int count)  // 0x10041a70
{
    if (!count || pos >= length) return;
    if (length < pos + count) {
        text[pos] = 0;
        length = pos;
        return;
    }
    std::memmove(text + pos, text + pos + count, ((length - pos) - count) * 2 + 2);
    length -= count;
}

void UnicodeString::Ins(const wch* s, unsigned int pos)  // 0x10041970
{
    if (!s) return;
    const unsigned n = WLen(s);
    if (!n) return;
    const unsigned len = length;
    if (pos > len) return;
    SetLength(len + n);
    std::memmove(text + n + pos, text + pos, (len - pos) * 2 + 2);
    std::memcpy(text + pos, s, n * 2);
    length += n;
}

// fills only `count` bytes with the low byte of ch (a memset on a wide buffer), as the original
void UnicodeString::Ins(unsigned int ch, unsigned int pos, unsigned int count)  // 0x10041a00
{
    if (!count) return;
    const unsigned len = length;
    if (pos > len) return;
    SetLength(len + count);
    std::memmove(text + pos + count, text + pos, (len - pos) * 2 + 2);
    std::memset(text + pos, static_cast<int>(ch), count);
    length += count;
}

UnicodeString& UnicodeString::operator+=(const wch* s)  // 0x100017d0
{
    Ins(s, length);
    return *this;
}

UnicodeString& UnicodeString::operator+=(const UnicodeString& o)  // 0x100017f0
{
    Ins(reinterpret_cast<const wch*>(o.text), length);
    return *this;
}

void UnicodeString::Setn(const wch* s, unsigned int n)  // 0x100414f0
{
    unsigned len = WLen(s);
    if (n <= len) len = n;
    length = len;
    const unsigned need = NewCap(len);
    if (capacity < need) {
        capacity = need;
        if (text) Jet::Alloc::Delete(text);
        text = static_cast<wchar_t*>(Jet::Alloc::New(capacity));
    }
    std::memcpy(text, s, length * 2);
    text[length] = 0;
}

void UnicodeString::Strip(const wch* chars)  // 0x10041220
{
    unsigned out = 0;
    for (unsigned i = 0; i < length; ++i)
        if (!std::wcschr(W(chars), text[i])) text[out++] = text[i];
    text[out] = 0;
    length = out;
}

void UnicodeString::ToLower()  // 0x10001810
{
    if (text) Crt80::_wcslwr(text);
}

void UnicodeString::ToUpper()  // 0x10001830
{
    if (text) Crt80::_wcsupr(text);
}

// the first run of characters not in `delimiters` (after leading delimiters); everything up to its end is cut
UnicodeString UnicodeString::Token(const wch* delimiters)  // 0x10041ad0
{
    UnicodeString r;
    unsigned start = 0;
    while (start < length && std::wcschr(W(delimiters), text[start])) ++start;
    unsigned end = start;
    while (end < length && !std::wcschr(W(delimiters), text[end])) ++end;
    if (end != start) r.Setn(reinterpret_cast<const wch*>(text + start), end - start);
    if (end) Del(0, end);
    return r;
}

// ---- formatting

int UnicodeString::Add(const wch* fmt, char* args)  // 0x10041270
{
    WFormat f(fmt, args);
    if (f.count > 0) {
        const unsigned need = NewCap(length + f.count);
        if (capacity < need) {
            capacity = need;
            wchar_t* p = static_cast<wchar_t*>(Jet::Alloc::New(need));
            std::memcpy(p, text, length * 2);
            if (text) Jet::Alloc::Delete(text);
            text = p;
        }
        std::memcpy(text + length, f.text, f.count * 2);
        length += f.count;
        text[length] = 0;
    }
    return f.count;
}

int UnicodeString::Addf(const wch* fmt, ...)  // 0x100413a0
{
    va_list ap;
    va_start(ap, fmt);
    const int r = Add(fmt, ap);
    va_end(ap);
    return r;
}

void UnicodeString::Set(const wch* fmt, char* args)  // 0x100413c0
{
    WFormat f(fmt, args);
    length = f.count;
    if (f.count == 0) {
        if (text) text[0] = 0;
    } else {
        const unsigned need = NewCap(f.count);
        if (capacity < need) {
            capacity = need;
            if (text) Jet::Alloc::Delete(text);
            text = static_cast<wchar_t*>(Jet::Alloc::New(capacity));
        }
        std::memcpy(text, f.text, length * 2);
        text[length] = 0;
    }
}

void UnicodeString::Setf(const wch* fmt, ...)  // 0x100414d0
{
    va_list ap;
    va_start(ap, fmt);
    Set(fmt, ap);
    va_end(ap);
}

// ---- binary form: a 4-byte header, bit 30 set for UTF-16 (then the byte count), else an ANSI character count

bool UnicodeString::Read(BinReader& r)  // 0x10041d70
{
    unsigned header;
    if (r.Read(&header, 4) != 4) {
        length = 0;
        capacity = 0;
        if (text) Jet::Alloc::Delete(text);
        text = nullptr;
        return false;
    }
    const bool ansi = !(header & 0x40000000);
    unsigned bytes = header, chars = header;
    if (!ansi) {
        bytes = header & 0xbfffffff;
        chars = bytes >> 1;
    }
    if (!chars) {
        SetLength(0);
        return true;
    }
    bool ok;
    if (!ansi) {
        SetLength(chars);
        ok = r.Read(text, bytes) == bytes;
        if (ok) {
            length = chars;
            text[chars] = 0;
        }
    } else {
        char local[0x400];
        char* buf = local;
        if (bytes + 2 > 0x400) buf = static_cast<char*>(Jet::Alloc::New(bytes + 2));
        ok = r.Read(buf, bytes) == bytes;
        if (ok) {
            buf[bytes] = 0;
            ok = SetANSI(buf, bytes);
        }
        if (buf != local && buf) Jet::Alloc::Delete(buf);
    }
    if (!ok) {
        length = 0;
        capacity = 0;
        if (text) Jet::Alloc::Delete(text);
        text = nullptr;
    }
    return ok;
}

bool UnicodeString::Write(BinWriter& w) const  // 0x10041ef0
{
    unsigned header = (length * 2) | 0x40000000;
    if (w.Write(&header, 4) != 4) return false;
    header &= 0xbfffffff;
    if (!header) return true;
    return w.Write(text, header) == header;
}

}  // namespace Jet
