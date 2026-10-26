// Unit U12b of KernelStandard_7c: Console (0x1001a7f0..0x1001b3a0), Jet's debug text console. The character ring
// is printed into at a cursor that wraps at the end of a line (NewLine scrolls `top` when it reaches it); Print
// takes "<color:RRGGBB>" / "</color>" tags out of the text into a span list. The 8x8 bitmap font (f50_font) is read
// from the installed game's module at build time (ConsoleFont.h), like StdMath's table.
#include <jet/KernelStandard.h>
#include <jet/Math.h>

#include "ConsoleFont.h"  // generated at build time from the installed game's KernelStandard_7c.dll
#include "Crt80.h"

#include <cstdarg>
#include <cstring>

namespace Jet {

unsigned char Console::f50_font[0x800] = {CONSOLE_F50_FONT_DATA};  // 0x10072d98

struct ConsoleWork {
    using Chunk = Console::Chunk;
    static_assert(sizeof(Console::Span) == 12 && sizeof(Chunk) == 0xa8);

    // the whole ring, the head last (0x1001aa7b, 0x1001b1f0)
    static void Free(Chunk*& list)
    {
        if (!list) return;
        Chunk* p = list->next;
        if (p != list) {
            Chunk* n;
            do {
                n = p->next;
                Alloc::Delete(p);
                p = n;
            } while (n != list);
        }
        if (p) Alloc::Delete(p);
        list = nullptr;
    }

    static Chunk* NewChunk()
    {
        Chunk* c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
        if (c) c->count = 0;
        return c;
    }

    // 0x1001b240: into the last chunk, a new one when it holds 13
    static void Append(Chunk*& list, const Console::Span& s)
    {
        Chunk* c;
        if (!list) {
            c = NewChunk();
            list = c;
            c->prev = c;
            c->next = c;
        } else {
            c = list->prev;
            if (c->count == 13) {
                Chunk* n = NewChunk();
                n->prev = c;
                n->next = list;
                list->prev = n;
                c->next = n;
                c = n;
            }
        }
        c->spans[c->count] = s;
        ++c->count;
    }

    // 0x1001b2d0: every chunk of src appended as it is (count and spans copied)
    static void CopyFrom(Chunk*& dst, Chunk* src)
    {
        if (!src) return;
        Chunk* c = src;
        Chunk* tail;
        if (!dst) {
            Chunk* n = NewChunk();
            dst = n;
            n->prev = n;
            n->next = n;
            std::memcpy(&n->count, &c->count, c->count * sizeof(Console::Span) + 4);
            tail = n;
            c = c->next;
            if (c == src) return;
        } else {
            tail = dst->prev;
        }
        do {
            Chunk* n = NewChunk();
            std::memcpy(&n->count, &c->count, c->count * sizeof(Console::Span) + 4);
            n->prev = tail;
            n->next = dst;
            dst->prev = n;
            tail->next = n;
            tail = n;
            c = c->next;
        } while (c != src);
    }

    static const int* Data(const AnsiString& s) { return static_cast<const int*>(s.data); }
    static const char* Text(const AnsiString& s) { return Data(s)[0] ? reinterpret_cast<const char*>(Data(s) + 2) : nullptr; }
    static unsigned int Length(const AnsiString& s) { return static_cast<unsigned int>(Data(s)[1]); }
    static void Truncate(AnsiString& s, unsigned int n)
    {
        s.MakeBufferUnique(0, true);
        int* d = static_cast<int*>(s.data);
        d[1] = static_cast<int>(n);
        reinterpret_cast<char*>(d + 2)[n] = 0;
    }

    static void Put(Console& c, unsigned char ch)  // a character without wrapping (the cursor stays on the line)
    {
        *c.cursor = ch;
        ++c.cursor;
        if (c.cursor >= c.lineEnd) --c.cursor;
    }

    static unsigned char* LineEnd(const Console& c)
    {
        return c.buffer + (static_cast<int>(c.cursor - c.buffer) / c.width + 1) * c.width;
    }
};

Console::Console() : buffer(nullptr), spans(nullptr) {}

Console::Console(const Console& o) : width(o.width), height(o.height), buffer(o.buffer), top(o.top), spans(nullptr)
{
    ConsoleWork::CopyFrom(spans, o.spans);
    cursor = o.cursor;
    lineEnd = o.lineEnd;
}

Console& Console::operator=(const Console& o)
{
    width = o.width;
    height = o.height;
    buffer = o.buffer;
    top = o.top;
    ConsoleWork::Free(spans);
    ConsoleWork::CopyFrom(spans, o.spans);
    cursor = o.cursor;
    lineEnd = o.lineEnd;
    return *this;
}

Console::~Console()
{
    if (buffer) {
        Alloc::Delete(buffer);
        buffer = nullptr;
    }
    ConsoleWork::Free(spans);
}

void __cdecl Console::Done()
{
    if (buffer) {
        Alloc::Delete(buffer);
        buffer = nullptr;
    }
}

void __cdecl Console::Init(const Size& size)
{
    width = size.w;
    height = size.h;
    buffer = static_cast<unsigned char*>(Alloc::New(width * height));
    Clear();
}

void __cdecl Console::Clear()
{
    top = 0;
    for (int i = 0; i < height * width; ++i) buffer[i] = ' ';
    cursor = buffer;
    lineEnd = buffer + (0 / width + 1) * width;
}

// to the start of the next line (the first when past the end); entering the top row scrolls it away and blanks it
void __cdecl Console::NewLine()
{
    unsigned char* c = buffer + (static_cast<int>(cursor - buffer) / width + 1) * width;
    cursor = c;
    if (!(c < buffer + height * width)) cursor = buffer;
    if (static_cast<int>(cursor - buffer) / width <= top) {
        top = top + 1 < height ? top + 1 : 0;
        for (int i = 0; i < width; ++i) cursor[i] = ' ';
    }
    lineEnd = ConsoleWork::LineEnd(*this);
}

void __cdecl Console::Goto(int x, int y)
{
    unsigned char* c = buffer + (top + y) * width + x;
    cursor = c;
    if (!(c < buffer + height * width)) cursor = c - height * width;
    lineEnd = ConsoleWork::LineEnd(*this);
}

void __cdecl Console::Printf(const char* format, ...)
{
    char text[0x1000];
    va_list ap;
    va_start(ap, format);
    Crt80::_vsnprintf(text, sizeof text, format, ap);
    va_end(ap);
    Print(text);
}

void __cdecl Console::Print(char ch, int count)
{
    while (count) {
        *cursor = static_cast<unsigned char>(ch);
        ++cursor;
        --count;
        if (cursor >= lineEnd) {
            --cursor;
            NewLine();
        }
    }
}

// the decimal digits right-aligned in cursor[1..width] (no digit for 0), then blanks; the cursor does not move
void __cdecl Console::Print(int value, int w)
{
    if (value != 0) {
        do {
            if (w == 0) return;
            const int q = value / 10;
            cursor[w] = static_cast<unsigned char>(static_cast<unsigned char>(value) -
                                                   static_cast<unsigned char>(static_cast<unsigned char>(q) * 10) + '0');
            --w;
            value = q;
        } while (value != 0);
    }
    while (w != 0) {
        --w;
        cursor[w + 1] = ' ';
    }
}

// a bar of halfSteps / 2 '#' (a '!' for an odd half), padded with blanks to width
void __cdecl Console::Bar(int halfSteps, int w)
{
    const int full = halfSteps >> 1;
    Print('#', full);
    int rest = w - full;
    if (halfSteps & 1) {
        *cursor = '!';
        ++cursor;
        if (cursor >= lineEnd) {
            --cursor;
            NewLine();
        }
        --rest;
    }
    Print(' ', rest);
}

// The text, wrapping at the line ends ('\n' starts a new line). A "<...>" tag is taken out: "</color>" ends a colour
// span, "<color:RRGGBB>" (the last 8 characters of the tag are the hex value) starts one; the span's position is the
// index of the character before the tag. Any other tag, or one with no '>', is printed as it is.
void __cdecl Console::Print(const char* text)
{
    ConsoleWork::Free(spans);
    const unsigned char* cur = reinterpret_cast<const unsigned char*>(text);
    const unsigned char* p = cur + 1;
    int c = *cur;
    int idx = 0;
    bool skip = false;
    while (c != 0) {
        if (c == '\n') {
            NewLine();
        } else if (c == '<') {
            if (skip) {
                skip = false;
                goto next;
            }
            {
                AnsiString tag;
                const int at = idx;
                bool ok = false;
                int c2 = *p++;
                ++idx;
                while (c2 != 0 && c2 != '>') {
                    const char one[2] = {static_cast<char>(c2), 0};
                    tag.Ins(one, ConsoleWork::Length(tag));
                    c2 = *p++;
                    ++idx;
                }
                if (c2 != 0) {
                    const char* t = ConsoleWork::Text(tag);
                    if (ConsoleWork::Data(tag)[0] && std::strchr(t, '/')) {
                        const unsigned int n = ConsoleWork::Length(tag);
                        if (n - 1 < n) tag.Del(0, 1);
                        if (tag.Compare("color") == 0) {
                            ConsoleWork::Append(spans, {0, at - 1, 0});
                            ok = true;
                        }
                    } else {
                        AnsiString value(tag);
                        if (ConsoleWork::Length(value) > 8) value.Del(0, ConsoleWork::Length(value) - 8);
                        const char* colon = ConsoleWork::Data(tag)[0] ? std::strchr(t, ':') : nullptr;
                        if (colon) {
                            const unsigned int k = static_cast<unsigned int>(colon - t);
                            if (k < ConsoleWork::Length(tag)) ConsoleWork::Truncate(tag, k);
                            if (tag.Compare("color") == 0) {
                                const unsigned int v = static_cast<unsigned int>(
                                    StringToHex(ConsoleWork::Text(value), static_cast<int>(ConsoleWork::Length(value))));
                                const unsigned long rgb = (v & 0xff00ff00u) | ((v >> 16) & 0xff) | ((v & 0xff) << 16);
                                ConsoleWork::Append(spans, {1, at - 1, rgb});
                                ok = true;
                            }
                        }
                    }
                }
                if (ok) {
                    c = c2;
                    idx = at - 1;
                } else {  // printed as it is: the '<' now, the rest from the next character on
                    idx = at;
                    skip = true;
                    p = cur;
                }
            }
            if (skip) ConsoleWork::Put(*this, static_cast<unsigned char>(c));
            goto next;
        } else {
            *cursor = static_cast<unsigned char>(c);
            ++cursor;
            if (cursor < lineEnd) goto after;
            --cursor;
            NewLine();
        }
    after:
        if (skip) ConsoleWork::Put(*this, static_cast<unsigned char>(c));
    next:
        cur = p;
        c = *p++;
        ++idx;
    }
}

void __cdecl Console::BlitToMono(unsigned char* screen) const
{
    int h = height;
    int r = top;
    for (int i = 0; i < h; ++i) {
        if (r >= h) r = 0;
        const int w = width;
        const unsigned char* src = buffer + w * r;
        unsigned char* dst = screen;
        for (int k = w; k != 0; --k) {
            *dst = *src++;
            dst += 2;
        }
        h = height;
        screen += 0xa0;
        ++r;
    }
}

// 8x8 characters into a 4-bank interleaved bitmap (0x2000 bytes a bank, 0x5a bytes a line, 0xb4 a text row)
void __cdecl Console::BlitToMono44(int x, int y, unsigned char* screen) const
{
    unsigned char* base = screen + y * 0xb4 + x;
    int h = height;
    int r = top;
    int w = width;
    for (int i = 0; i < h; ++i) {
        if (r >= h) r = 0;
        const unsigned char* src = buffer + w * r;
        if (0 < w) {
            int col = 0;
            do {
                const unsigned char* g = f50_font + *src * 8;
                unsigned char* d = base + col;
                d[0] = g[0];
                d[0x2000] = g[1];
                d[0x4000] = g[2];
                d[0x6000] = g[3];
                d[0x5a] = g[4];
                d[0x205a] = g[5];
                d[0x405a] = g[6];
                d[0x605a] = g[7];
                w = width;
                ++col;
                ++src;
            } while (col < w);
        }
        h = height;
        base += 0xb4;
        ++r;
    }
}

// "[+-][0x|#]hex digits", length characters at most, stopping at the first non-digit
int __cdecl Console::StringToHex(const char* s, int length)
{
    const char first = *s;
    int v = 0;
    if (first == '+' || first == '-') {
        ++s;
        --length;
    }
    if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        length -= 2;
    } else if (*s == '#') {
        ++s;
        --length;
    }
    while (length != 0) {
        const char ch = *s;
        --length;
        ++s;
        int d;
        if (static_cast<unsigned char>(ch - '0') < 10)
            d = ch - '0';
        else if (static_cast<unsigned char>(ch - 'A') < 6)
            d = ch - 0x37;
        else if (static_cast<unsigned char>(ch - 'a') < 6)
            d = ch - 0x57;
        else
            break;
        v = v * 16 + d;
    }
    return first == '-' ? -v : v;
}

}  // namespace Jet
