// Unit U8 of KernelStandard_7c: Bitmap (0x10003870..0x100079a0 and 0x1000b6a0..0x1000c4d0): construction, the
// pixel primitives (hand-written assembly in the original, 0x100079a0..0x10007ff0, rewritten here register by
// register where their arithmetic depends on it), drawing, blits, scaling and the colour operations. The file
// formats are in BitmapIO.cpp. Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U8").
#include <jet/Bitmap.h>
#include <jet/KernelStandard.h>

#include <malloc.h>

#include <cmath>
#include <cstring>
#include <vector>

namespace Jet {

namespace {

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;

// ---- pixel primitives. dest/src are pixel addresses; count is taken as unsigned (the original loops on it with
// rep or dec/jne, so 0 means 2^32 where it loops with dec/jne); pitch 0 means the next pixel.

// ImageProcessing::~ImageProcessing's `ret` serves as the do-nothing primitive of INDEX and UNSPECIFIED
void __cdecl NopGet(void*, const void*, int) {}
void __cdecl NopSet(void*, u32, int, int) {}

// "mov al, x; mul al, dl": ax = x * dl, the rest of eax kept
inline u32 MulAl(u32 eax, u32 x, u32 dl) { return (eax & 0xffff0000u) | ((x & 0xff) * (dl & 0xff)); }

// one channel of the 32/24-bit blends: the high bytes of colour * w and dest * -w, added in 8 bits
inline u8 Mix(u32 c, u32 d, u32 w) { return u8(((c & 0xff) * (w & 0xff) >> 8) + ((d & 0xff) * ((0u - w) & 0xff) >> 8)); }

// 0x100079a0: 32-bit fill
void __cdecl Set32(void* dest, u32 c, int count, int pitch)
{
    u8* p = static_cast<u8*>(dest);
    u32 n = u32(count);
    if (pitch <= 0) {
        for (; n; --n, p += 4) *reinterpret_cast<u32*>(p) = c;
    } else {
        do {
            *reinterpret_cast<u32*>(p) = c;
            p += pitch;
        } while (--n);
    }
}

// 0x100079d0: 565 to 32-bit (also the 565 -> ARGB blitter)
void __cdecl Get565To32(void* dest, const void* src, int count)
{
    u32* d = static_cast<u32*>(dest);
    const u16* s = static_cast<const u16*>(src);
    u32 n = u32(count);
    do {
        const u32 w = *s++;
        *d++ = 0xff000000u | ((w >> 8) & 0xf8) << 16 | ((w >> 3) & 0xfc) << 8 | ((w << 3) & 0xf8);
    } while (--n);
}

// 0x10007a20: 24-bit to 32-bit
void __cdecl Get24To32(void* dest, const void* src, int count)
{
    u32* d = static_cast<u32*>(dest);
    const u8* s = static_cast<const u8*>(src);
    u32 n = u32(count);
    do {
        *d++ = 0xff000000u | u32(s[2]) << 16 | u32(s[1]) << 8 | s[0];
        s += 3;
    } while (--n);
}

// rep movsd / movsw / movsb: element by element forward, so an overlapping copy repeats the pattern (the tiling
// in Image relies on it)
void ForwardCopy(void* dest, const void* src, u32 dwords, u32 tailBytes)
{
    u8* d = static_cast<u8*>(dest);
    const u8* s = static_cast<const u8*>(src);
    for (; dwords; --dwords, d += 4, s += 4) {
        u32 v;
        std::memcpy(&v, s, 4);
        std::memcpy(d, &v, 4);
    }
    if (tailBytes == 2) {
        u16 v;
        std::memcpy(&v, s, 2);
        std::memcpy(d, &v, 2);
    } else {
        for (; tailBytes; --tailBytes) *d++ = *s++;
    }
}

// 0x10007a50: 32-bit copy
void __cdecl Copy32(void* dest, const void* src, int count) { ForwardCopy(dest, src, u32(count), 0); }

// 0x10007a70: 32-bit fill blended by the colour's alpha; a destination alpha of 0 takes the colour as it is
void __cdecl Blend32(void* dest, u32 c, int count, int pitch)
{
    if (!(c & 0xff000000u)) return;
    if (c >= 0xff000000u) {
        Set32(dest, c, count, pitch);
        return;
    }
    u8* p = static_cast<u8*>(dest);
    const int step = pitch ? pitch : 4;
    const u32 wa = (c >> 24) + 1;
    u32 n = u32(count);
    do {
        const u32 da = p[3];
        if (!da) {
            *reinterpret_cast<u32*>(p) = c;
        } else {
            u32 w = wa;
            if (da != 0xff) {
                const u32 inv = 0xff - da;
                p[3] = u8((0u - (0x100 - wa) * inv) >> 8);
                w = (wa + inv) >> 1;
            }
            p[0] = Mix(c, p[0], w);
            p[1] = Mix(c >> 8, p[1], w);
            p[2] = Mix(c >> 16, p[2], w);
        }
        p += step;
    } while (--n);
}

// 0x10007b60: 32-bit to 32-bit blended by the source alpha
void __cdecl BlendARGBTo32(void* dest, const void* src, int count)
{
    u8* d = static_cast<u8*>(dest);
    const u8* s = static_cast<const u8*>(src);
    u32 n = u32(count);
    do {
        const u32 sv = *reinterpret_cast<const u32*>(s);
        if (sv & 0xff000000u) {
            const u32 dv = *reinterpret_cast<const u32*>(d);
            if (sv >= 0xff000000u || !(dv & 0xff000000u)) {
                *reinterpret_cast<u32*>(d) = sv;
            } else {
                u32 w = s[3] + 1u;
                if (dv < 0xff000000u) {
                    const u32 inv = 0xff - d[3];
                    d[3] = u8((0u - (0x100 - w) * inv) >> 8);
                    w = (w + inv) >> 1;
                }
                d[0] = Mix(s[0], d[0], w);
                d[1] = Mix(s[1], d[1], w);
                d[2] = Mix(s[2], d[2], w);
            }
        }
        s += 4;
        d += 4;
    } while (--n);
}

// a colour as 565 (no rounding)
inline u32 To565(u32 r, u32 g, u32 b) { return (r >> 3 & 0x1f) << 11 | (g >> 2 & 0x3f) << 5 | (b >> 3 & 0x1f); }

// the dest half of the 565 blends: eax after "R*nw shl 8, G*nw shl 6, (B*nw) shl ax 3, shl 5"
inline u32 Dest565(u32 eax, u32 v, u32 nw)
{
    eax = MulAl((eax & 0xffff0000u) | (v >> 11), v >> 11, nw) << 8;
    eax = MulAl(eax, (v >> 3) & 0xfc, nw) << 6;
    eax = MulAl(eax, v & 0x1f, nw);
    eax = (eax & 0xffff0000u) | ((eax << 3) & 0xffff);
    return eax << 5;
}

// the colour half: "R*w shl 5, G*w shl 6, B*w shl 5"
inline u32 Colour565(u32 eax, u32 r, u32 g, u32 b, u32 w)
{
    eax = MulAl(eax, r, w) << 5;
    eax = MulAl(eax, g, w) << 6;
    eax = MulAl(eax, b, w) << 5;
    return eax;
}

// 0x10007c80: 565 fill, blended by the colour's alpha
void __cdecl Set565(void* dest, u32 c, int count, int pitch)
{
    u8* p = static_cast<u8*>(dest);
    const int step = pitch ? pitch : 2;
    const u32 a = c >> 24;
    u32 n = u32(count);
    if (!a) return;
    if (a == 0xff) {
        const u16 v = u16(To565(c >> 16 & 0xff, c >> 8 & 0xff, c & 0xff));
        if (step == 2) {
            for (; n; --n, p += 2) *reinterpret_cast<u16*>(p) = v;  // rep stosd + stosw
        } else {
            do {
                *reinterpret_cast<u16*>(p) = v;
                p += step;
            } while (--n);
        }
        return;
    }
    const u32 w = a + 1;
    u32 eax = Colour565(0, c >> 16, c >> 8, c, w);
    const u32 edx = ((eax & 0xffff0000u) | (w & 0xffff)) ^ 0xff;
    do {
        eax = Dest565(eax, *reinterpret_cast<u16*>(p), edx) + edx;
        *reinterpret_cast<u16*>(p) = u16(eax >> 16);
        p += step;
    } while (--n);
}

// 0x10007d50: 16-bit copy
void __cdecl Copy16(void* dest, const void* src, int count) { ForwardCopy(dest, src, u32(count) >> 1, (u32(count) & 1) * 2); }

// 0x10007d70: 24-bit to 565
void __cdecl Blit24To565(void* dest, const void* src, int count)
{
    u16* d = static_cast<u16*>(dest);
    const u8* s = static_cast<const u8*>(src);
    u32 n = u32(count);
    do {
        *d++ = u16(To565(s[2], s[1], s[0]));
        s += 3;
    } while (--n);
}

// 0x10007da0: 32-bit to 565 blended by the source alpha
void __cdecl BlendARGBTo565(void* dest, const void* src, int count)
{
    u16* d = static_cast<u16*>(dest);
    const u8* s = static_cast<const u8*>(src);
    u32 n = u32(count);
    u32 eax = 0, edx = 0;
    do {
        edx = (edx & ~0xffu) | s[3];
        if (s[3] >= 1) {
            if (s[3] >= 0xff) {
                *d = u16(To565(s[2], s[1], s[0]));
            } else {
                edx = (edx & ~0xffu) | (((s[3] + 1u) & 0xff) ^ 0xff);
                eax = Dest565(eax, *d, edx);
                eax = (eax & 0xffff0000u) | (edx & 0xffff);
                const u32 t = eax;
                eax = edx;
                edx = t ^ 0xff;  // xchg; xor dl, 0ffh
                eax = Colour565(eax, s[2], s[1], s[0], edx) + edx;
                *d = u16(eax >> 16);
            }
        }
        s += 4;
        ++d;
    } while (--n);
}

// 0x10007e50: 24-bit fill, blended by the colour's alpha (the channels' sums carry into each other)
void __cdecl Set24(void* dest, u32 c, int count, int pitch)
{
    u8* p = static_cast<u8*>(dest);
    const int step = pitch ? pitch : 3;
    const u32 a = c >> 24;
    u32 n = u32(count);
    if (!a) return;
    if (a == 0xff) {
        do {
            p[0] = u8(c);
            p[1] = u8(c >> 8);
            p[2] = u8(c >> 16);
            p += step;
        } while (--n);
        return;
    }
    const u32 w = a + 1;
    u32 eax = MulAl(0, c >> 16, w) << 8;
    eax = MulAl(eax, c >> 8, w) << 8;
    eax = MulAl(eax, c, w);
    eax = (eax & ~0xffu) | (w & 0xff);
    const u32 edx = eax ^ 0xff;
    do {
        eax = MulAl(eax, p[2], edx) << 8;
        eax = MulAl(eax, p[1], edx) << 8;
        eax = MulAl(eax, p[0], edx);
        eax = (eax & 0xffffff00u) + edx;
        p[0] = u8(eax >> 8);
        eax >>= 16;
        p[1] = u8(eax);
        p[2] = u8(eax >> 8);
        p += step;
    } while (--n);
}

// 0x10007ef0: 565 to 24-bit
void __cdecl Blit565To24(void* dest, const void* src, int count)
{
    u8* d = static_cast<u8*>(dest);
    const u16* s = static_cast<const u16*>(src);
    u32 n = u32(count);
    do {
        const u32 w = *s++;
        d[0] = u8(w << 3);
        d[1] = u8((w >> 3) & 0xfc);
        d[2] = u8((w >> 8) & 0xf8);
        d += 3;
    } while (--n);
}

// 0x10007f30: 24-bit copy
void __cdecl Copy24(void* dest, const void* src, int count)
{
    const u32 n = u32(count) * 3;
    ForwardCopy(dest, src, n >> 2, n & 3);
}

// 0x10007f60: 32-bit to 24-bit blended by the source alpha
void __cdecl BlendARGBTo24(void* dest, const void* src, int count)
{
    u8* d = static_cast<u8*>(dest);
    const u8* s = static_cast<const u8*>(src);
    u32 n = u32(count);
    do {
        const u32 a = s[3];
        if (a == 0xff) {
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        } else if (a) {
            const u32 w = a + 1;
            d[0] = Mix(s[0], d[0], w);
            d[1] = Mix(s[1], d[1], w);
            d[2] = Mix(s[2], d[2], w);
        }
        s += 4;
        d += 3;
    } while (--n);
}

// 0x10007c20: fills count entries of 8 bytes with *src (a forward overlapping copy)
void Repeat8(void* dest, const void* src, int count)
{
    u8* d = static_cast<u8*>(dest);
    std::memcpy(d, src, 8);
    for (u32 i = 1; i < u32(count); ++i) std::memcpy(d + i * 8, d + (i - 1) * 8, 8);
}

// 0x100032a0: the intersection of a (ecx) and b (edx). An empty height zeroes x and h, an empty width y and w
Rect Intersect(const Rect& a, const Rect& b)
{
    Rect r;
    int left, ar, br;
    if (b.x < a.x) {
        r.x = a.x;
        left = a.x;
        br = b.x + b.w - 1;
        ar = a.x + a.w - 1;
    } else {
        r.x = b.x;
        ar = a.x + a.w - 1;
        left = b.x;
        br = b.x + b.w - 1;
    }
    if (br <= ar) ar = br;
    r.w = ar - left + 1;
    if (b.y < a.y) {
        r.y = a.y;
        const int ab = a.y + a.h - 1;
        int bb = b.y + b.h - 1;
        if (ab < bb) bb = ab;
        r.h = bb - a.y + 1;
    } else {
        r.y = b.y;
        int ab = a.y + a.h - 1;
        const int bb = b.y + b.h - 1;
        if (bb <= ab) ab = bb;
        r.h = ab - b.y + 1;
    }
    if (r.h < 0) {
        r.x = 0;
        r.h = 0;
    }
    if (r.w < 0) {
        r.y = 0;
        r.w = 0;
    }
    return r;
}

// 0x10003260
bool Contains(const Rect& r, const Point& p)
{
    return r.x <= p.x && p.x <= r.w - 1 + r.x && r.y <= p.y && p.y <= r.h - 1 + r.y;
}

// T[a][c] = (a * c + 127) / 255, the original's 256-aligned static table (built by a static initialiser)
struct alignas(256) MulTable {
    u8 t[256][256];
    MulTable()
    {
        for (int a = 0; a < 256; ++a)
            for (int c = 0; c < 256; ++c) t[a][c] = u8((a * c + 127) / 255);
    }
};
const MulTable mulTable;

// the new Bitmap the copying methods allocate: the inlined default constructor
Bitmap* NewBitmap()
{
    void* p = Alloc::New(sizeof(Bitmap));
    return p ? ::new (p) Bitmap() : nullptr;
}

}  // namespace

// ---- construction

Bitmap::Bitmap()
{
    data = nullptr;
    palette = nullptr;
    alloc = false;
    Done();
}

Bitmap::Bitmap(const Size& size, Format f, unsigned char* userData)
{
    alloc = false;
    data = nullptr;
    palette = nullptr;
    Init(size, 0, f, userData, 0);
    if (userData == reinterpret_cast<unsigned char*>(-1)) data = nullptr;
}

Bitmap::Bitmap(const Bitmap& src, const Rect& area)
{
    alloc = false;
    data = nullptr;
    palette = nullptr;
    Init(*reinterpret_cast<const Size*>(&area.w), src.pitch, src.format, nullptr, 0);
    const int bpp = src.bytesPerPixel;
    unsigned char* d = data;
    const unsigned char* s = src.data + area.y * src.pitch + area.x * bpp;
    for (int i = 0; i < area.h; ++i) {
        std::memcpy(d, s, size_t(area.w * bpp));
        s += src.pitch;
        d += pitch;
    }
}

Bitmap::Bitmap(const Bitmap&) {}  // 0x1000db10: nothing set
void Bitmap::operator=(const Bitmap&) {}

Bitmap::~Bitmap() { Done(); }

void Bitmap::Init(const Size& size, int pitch_, Format f, unsigned char* userData, int numBytes)
{
    Done();
    const int w = size.w, h = size.h;
    if (w < 0 || h < 0 || w > 0x1000 || h > 0x1000 || h * pitch_ > 0x1000000 || u32(numBytes) > 0x1000000) return;
    width = w;
    height = h;
    data = userData;
    pitch = pitch_;
    format = f;
    alloc = false;
    palette = nullptr;
    dataSize = u32(numBytes);
    switch (f) {
    case INDEX:
    case UNSPECIFIED:
        bytesPerPixel = f == INDEX ? 1 : 0;
        getPixel = NopGet;
        setPixel = NopSet;
        blendPixel = NopSet;
        blitSelf = NopGet;
        blitRGB565 = NopGet;
        blitRGB888 = NopGet;
        blitARGB888 = NopGet;
        blendARGB888 = NopGet;
        break;
    case RGB:
        setPixel = Set24;
        blendPixel = Set24;
        blitSelf = Copy24;
        blitRGB888 = Copy24;
        getPixel = Get24To32;
        blitRGB565 = Blit565To24;
        blitARGB888 = BlendARGBTo24;
        blendARGB888 = BlendARGBTo24;
        bytesPerPixel = 3;
        break;
    case ARGB:
    case XRGB:
        getPixel = Copy32;
        setPixel = Set32;
        blendPixel = Blend32;
        blitSelf = Copy32;
        blitRGB565 = Get565To32;
        blitRGB888 = Get24To32;
        blendARGB888 = BlendARGBTo32;
        bytesPerPixel = 4;
        blitARGB888 = Copy32;
        break;
    case RGB565:
        setPixel = Set565;
        blendPixel = Set565;
        blitSelf = Copy16;
        blitRGB565 = Copy16;
        getPixel = Get565To32;
        blitRGB888 = Blit24To565;
        blitARGB888 = BlendARGBTo565;
        blendARGB888 = BlendARGBTo565;
        bytesPerPixel = 2;
        break;
    default:  // an unknown format keeps Done's primitives and bytes per pixel
        break;
    }
    if (!pitch_) pitch = (bytesPerPixel * width + 3) & ~3;
    if (!numBytes && f != UNSPECIFIED) {
        dataSize = u32(height * pitch);
    } else if (u32(numBytes) <= u32(height * pitch) && u32(height * pitch) - u32(numBytes) != 0) {
        Done();
        return;
    }
    if (f != UNSPECIFIED && (width < 1 || height < 1 || pitch < width)) {
        data = nullptr;
        return;
    }
    if (userData) return;
    data = static_cast<unsigned char*>(Alloc::New(dataSize));
    if (data)
        alloc = true;
    else
        Done();
}

void Bitmap::InitInline(int w, int h, Format f)
{
    Done();
    if (w < 0 || h < 0 || w > 0x1000 || h > 0x1000) return;
    getPixel = Copy32;
    blitSelf = Copy32;
    blitARGB888 = Copy32;
    width = w;
    height = h;
    data = nullptr;
    format = f;
    alloc = false;
    palette = nullptr;
    setPixel = Set32;
    blendPixel = Blend32;
    blitRGB565 = Get565To32;
    blitRGB888 = Get24To32;
    blendARGB888 = BlendARGBTo32;
    bytesPerPixel = 4;
    pitch = (w * 4 + 3) & ~3;
    dataSize = u32(pitch * h);
    if (w < 1 || h < 1 || pitch < w) {
        data = nullptr;
        return;
    }
    data = static_cast<unsigned char*>(Alloc::New(dataSize));
    if (data)
        alloc = true;
    else
        Done();
}

void __cdecl Bitmap::Done()
{
    if (alloc) {
        if (data) Alloc::Delete(data);
        data = nullptr;
        alloc = false;
    }
    if (palette) Alloc::Delete(palette);
    palette = nullptr;
    width = 0;
    height = 0;
    pitch = 0;
    dataSize = 0;
    format = UNSPECIFIED;
    getPixel = NopGet;
    setPixel = NopSet;
    blendPixel = NopSet;
    blitSelf = NopGet;
    blitRGB565 = NopGet;
    blitRGB888 = NopGet;
    blitARGB888 = NopGet;
    blendARGB888 = NopGet;
}

void __cdecl Bitmap::DetachAlias() const
{
    if (alloc || !dataSize || !data) return;
    Bitmap* self = const_cast<Bitmap*>(this);
    void* p = Alloc::New(dataSize);
    if (!p) {
        self->Done();
        return;
    }
    std::memcpy(p, data, dataSize);
    self->data = static_cast<unsigned char*>(p);
    self->alloc = true;
}

// ---- accessors (exported out-of-line copies of the SDK's inlines)

bool Bitmap::IsValid() const { return data != nullptr; }
Bitmap::Format Bitmap::GetFormat() const { return format; }
unsigned long Bitmap::GetBytesPerPixel() const { return u32(bytesPerPixel); }
unsigned long Bitmap::GetPitch() const { return u32(pitch); }
unsigned long Bitmap::GetNumBytes() const { return dataSize; }
unsigned char* Bitmap::GetPtr() const { return data; }
unsigned char* Bitmap::GetPtr(const Point& at) const { return data + at.y * pitch + bytesPerPixel * at.x; }
PackedColor* Bitmap::GetPalette() const { return palette; }
Bitmap::operator Rect() const { return Rect{0, 0, width, height}; }
unsigned int Bitmap::CalcPitch(unsigned int w) const { return (u32(bytesPerPixel) * w + 3) & ~3u; }
unsigned char Bitmap::R16(unsigned long c) const { return u8(c >> 11) & 0x1f; }
unsigned char Bitmap::G16(unsigned long c) const { return u8(c >> 5) & 0x3f; }
unsigned char Bitmap::B16(unsigned long c) const { return u8(c) & 0x1f; }
unsigned char Bitmap::R15(unsigned long c) const { return u8(c >> 10) & 0x1f; }
unsigned char Bitmap::G15(unsigned long c) const { return u8(c >> 5) & 0x1f; }
unsigned char Bitmap::B15(unsigned long c) const { return u8(c) & 0x1f; }

Bitmap::BlitterFunc Bitmap::GetBlitter(Format f, bool blend)
{
    switch (f) {
    case RGB:
        return blitRGB888;
    case ARGB:
    case XRGB:
        return blend ? blendARGB888 : blitARGB888;
    case RGB565:
        return blitRGB565;
    default:
        return nullptr;
    }
}

// ---- pixels

// 0x10004b30, shared by both: the plain primitive, no blending
void __cdecl Bitmap::Pixel(const Point& at, PackedColor c, const Rect& clip)
{
    if (clip.x <= at.x && at.x <= clip.w - 1 + clip.x && clip.y <= at.y && at.y <= clip.h - 1 + clip.y)
        setPixel(data + pitch * at.y + bytesPerPixel * at.x, c.argb, 1, 0);
}
void __cdecl Bitmap::BlendPixel(const Point& at, PackedColor c, const Rect& clip) { Pixel(at, c, clip); }
void Bitmap::Pixel(const Point& at, PackedColor c) { Pixel(at, c, Rect{0, 0, width, height}); }
void Bitmap::BlendPixel(const Point& at, PackedColor c) { BlendPixel(at, c, Rect{0, 0, width, height}); }

PackedColor Bitmap::Pixel(const Point& at) const
{
    PackedColor r{0xffffffffu};
    if (at.x >= 0 && at.x <= width - 1 && at.y >= 0 && at.y <= height - 1)
        getPixel(&r, data + pitch * at.y + bytesPerPixel * at.x, 1);
    return r;
}

// ---- lines

void __cdecl Bitmap::ClipLine(Point& a, Point& b, Rect r)
{
    const int Y = r.y;
    int ay = a.y;
    if (ay <= Y && (b.y < Y || (ay < Y && b.y <= Y))) {
        a.x = -1;
        b.x = -1;
        return;
    }
    int by = b.y;
    float f = float(Y - by) / float(ay - by);
    if (ay < Y) {
        if (by > Y) {
            a.y = Y;
            a.x = int(float(a.x - b.x) * f + float(b.x));
        }
    } else if (by < Y && ay > Y) {
        b.y = Y;
        b.x = int(float(a.x - b.x) * f + float(b.x));
    }

    const int Yb = r.h + Y - 1;
    ay = a.y;
    if (ay >= Yb && (b.y > Yb || (ay > Yb && b.y >= Yb))) {
        a.x = -1;
        b.x = -1;
        return;
    }
    by = b.y;
    f = float(r.h - by + Y - 1) / float(ay - by);
    if (by < Yb) {
        if (ay > Yb) {
            a.y = Yb;
            a.x = int(float(a.x - b.x) * f + float(b.x));
        }
    } else if (ay < Yb && by > Yb) {
        b.y = Yb;
        b.x = int(float(a.x - b.x) * f + float(b.x));
    }

    const int X = r.x;
    int ax = a.x;
    if (ax <= X && (b.x < X || (ax < X && b.x <= X))) {
        a.x = -1;
        b.x = -1;
        return;
    }
    int bx = b.x;
    f = float(X - bx) / float(ax - bx);
    if (ax < X) {
        if (bx > X) {
            a.x = X;
            a.y = int(float(a.y - b.y) * f + float(b.y));
        }
    } else if (bx < X && ax > X) {
        b.x = X;
        b.y = int(float(a.y - b.y) * f + float(b.y));
    }

    const int Xr = X + r.w - 1;
    ax = a.x;
    if (ax >= Xr && (b.x > Xr || (ax > Xr && b.x >= Xr))) {
        a.x = -1;
        b.x = -1;
        return;
    }
    bx = b.x;
    f = float(r.w - bx + X - 1) / float(ax - bx);
    if (ax < Xr) {
        if (bx > Xr) {
            b.x = Xr;
            b.y = int(float(a.y - b.y) * f + float(b.y));
        }
    } else if (bx < Xr && ax > Xr) {
        a.x = Xr;
        a.y = int(float(a.y - b.y) * f + float(b.y));
    }
}

// run-sliced Bresenham: one primitive call per run of pixels
void __cdecl Bitmap::Line(Point a, Point b, PackedColor colour, int mode, const Rect& clip)
{
    if (format == UNSPECIFIED) return;
    const SetFn set = (mode & 4) ? blendPixel : setPixel;
    ClipLine(a, b, clip);
    if (a.y > b.y) {
        const Point t = a;
        a = b;
        b = t;
    }
    if (a.x == -1 || b.x == -1) return;
    if (mode & 2) {
        AntialiasedLine(a, b, colour, clip);
        return;
    }
    const u32 c = colour.argb;
    const int bpp = bytesPerPixel;
    auto ptr = [&](int x, int y) { return data + bpp * x + pitch * y; };
    const int dx = b.x - a.x;
    const int dir = dx < 0 ? -1 : 1;
    const int adx = dx < 0 ? -dx : dx;
    const int dy = b.y - a.y;
    if (!adx) {
        set(ptr(a.x, a.y), c, dy + 1, pitch);
        return;
    }
    if (!dy) {
        if (dir == -1)
            set(ptr(b.x, b.y), c, a.x - b.x + 1, 0);
        else
            set(ptr(a.x, a.y), c, dx + 1, 0);
        return;
    }
    if (adx == dy) {
        set(ptr(a.x, a.y), c, dy + 1, bpp * dir + pitch);
        return;
    }
    if (adx > dy) {  // horizontal runs
        const int q = adx / dy, r = adx % dy;
        const int adjUp = 2 * r, adjDown = 2 * dy;
        int err = r - 2 * dy;
        int startRun = q / 2 + 1;
        const int endRun = q / 2 + 1;
        if (!adjUp && !(q & 1)) --startRun;
        if (q & 1) err += dy;
        int x = a.x;
        if (dir == -1) x -= startRun;
        set(ptr(x, a.y), c, startRun, 0);
        if (dir != 1) startRun = -startRun;
        x = a.x + startRun;
        int row = 1;
        for (int n = dy - 1; n > 0; --n, ++row) {
            err += adjUp;
            int run = q;
            if (err > 0) {
                run = q + 1;
                err -= adjDown;
            }
            set(ptr(dir == -1 ? x - run : x, a.y + row), c, run, 0);
            x += dir == 1 ? run : -run;
        }
        set(ptr(dir == -1 ? x - endRun : x, a.y + row), c, endRun, 0);
    } else {  // vertical runs
        const int q = dy / adx, r = dy % adx;
        const int adjUp = 2 * r, adjDown = 2 * adx;
        int err = r - 2 * adx;
        int startRun = q / 2 + 1;
        const int endRun = q / 2 + 1;
        if (!adjUp && !(q & 1)) --startRun;
        if (q & 1) err += adx;
        set(ptr(a.x, a.y), c, startRun, pitch);
        int x = a.x + dir;
        int yoff = startRun;
        for (int n = adx - 1; n > 0; --n) {
            err += adjUp;
            int run = q;
            if (err > 0) {
                ++run;
                err -= adjDown;
            }
            set(ptr(x, a.y + yoff), c, run, pitch);
            x += dir;
            yoff += run;
        }
        set(ptr(x, a.y + yoff), c, endRun, pitch);
    }
}

void Bitmap::Line(Point a, Point b, PackedColor colour, int mode) { Line(a, b, colour, mode, Rect{0, 0, width, height}); }

// Wu-style: two pixels per step with complementary alphas (the colour's alpha scaled), against the bitmap's size
void __cdecl Bitmap::AntialiasedLine(Point a, Point b, PackedColor colour, Rect)
{
    if (format == UNSPECIFIED) return;
    if (a.y > b.y) {
        const Point t = a;
        a = b;
        b = t;
    }
    const u32 c = colour.argb;
    const u32 A = c >> 24;
    auto plot = [&](int x, int y, u32 col) {
        if (x >= 0 && x <= width - 1 && y >= 0 && y <= height - 1)
            setPixel(data + bytesPerPixel * x + pitch * y, col, 1, 0);
    };
    auto withAlpha = [&](u32 al) { return (c & 0xffffffu) | (al & 0xff) << 24; };
    int x = a.x, y = a.y;
    plot(x, y, c);
    int dx = b.x - x;
    int dir = 1;
    if (dx < 0) {
        dx = -dx;
        dir = -1;
    }
    u32 adx = u32(dx);
    u32 dy = u32(b.y - y);
    if (!dy) {
        while (adx) {
            --adx;
            x += dir;
            plot(x, y, c);
        }
        return;
    }
    if (!adx) {
        do {
            ++y;
            plot(x, y, c);
        } while (--dy);
        return;
    }
    if (adx == dy) {
        do {
            x += dir;
            ++y;
            plot(x, y, c);
        } while (--dy);
        return;
    }
    u32 acc = 0;
    if (adx < dy) {
        const u32 step = ((adx << 16) / dy) & 0xffff;
        for (u32 n = dy - 1; n; --n) {
            acc += step;
            if (acc & 0xffff0000u) {
                acc &= 0xffff;
                x += dir;
            }
            ++y;
            const u32 t = acc >> 8;
            plot(x, y, withAlpha(((t ^ 0xfe) * A) >> 8));
            plot(x + dir, y, withAlpha((A * t) >> 8));
        }
    } else {
        const u32 step = ((dy << 16) / adx) & 0xffff;
        for (u32 n = adx - 1; n; --n) {
            acc += step;
            if (acc & 0xffff0000u) {
                acc &= 0xffff;
                ++y;
            }
            x += dir;
            const u32 t = acc >> 8;
            plot(x, y, withAlpha(((t ^ 0xfe) * A) >> 8));
            plot(x, y + 1, withAlpha((A * t) >> 8));
        }
    }
    Pixel(b, colour);
}

// ---- boxes, circles, triangles

void __cdecl Bitmap::Box(Rect r, PackedColor colour, int mode, const Rect& clip)
{
    if (format == UNSPECIFIED) return;
    if (!(mode & 1)) {
        if (r.w <= 0) return;
        const int m = (mode & 4) | 1;
        if (r.h > 0) Line(Point{r.x, r.y}, Point{r.x + r.w - 1, r.y}, colour, m, clip);
        if (r.h > 1) Line(Point{r.x + r.w - 1, r.y + r.h - 1}, Point{r.x, r.y + r.h - 1}, colour, m, clip);
        if (r.h > 2) {
            Line(Point{r.x + r.w - 1, r.y + 1}, Point{r.x + r.w - 1, r.y + r.h - 2}, colour, m, clip);
            Line(Point{r.x, r.y + r.h - 2}, Point{r.x, r.y + 1}, colour, m, clip);
        }
        return;
    }
    const Rect f = Intersect(clip, r);
    unsigned char* p = data + bytesPerPixel * f.x + pitch * f.y;
    const SetFn set = (mode & 4) ? blendPixel : setPixel;
    if (!f.w) return;
    for (int n = f.h; n; --n) {
        set(p, colour.argb, f.w, 0);
        p += pitch;
    }
}

void Bitmap::Box(Rect r, PackedColor colour, int mode) { Box(r, colour, mode, Rect{0, 0, width, height}); }

void __cdecl Bitmap::DrawCirclePoints(Point c, int x, int y, PackedColor colour, Rect clip)
{
    if (format == UNSPECIFIED) return;
    BlendPixel(Point{c.x + x, c.y + y}, colour, clip);
    BlendPixel(Point{c.x - x, c.y + y}, colour, clip);
    BlendPixel(Point{c.x + x, c.y - y}, colour, clip);
    BlendPixel(Point{c.x - x, c.y - y}, colour, clip);
    BlendPixel(Point{c.x + y, c.y + x}, colour, clip);
    BlendPixel(Point{c.x - y, c.y + x}, colour, clip);
    BlendPixel(Point{c.x + y, c.y - x}, colour, clip);
    BlendPixel(Point{c.x - y, c.y - x}, colour, clip);
}

void __cdecl Bitmap::DrawHLine(int x1, int x2, int y, PackedColor colour, Rect clip)
{
    if (y > clip.h + clip.y - 1 || y < clip.y) return;
    if (x1 < clip.x) x1 = clip.x;
    if (x2 > clip.w + clip.x - 1) x2 = clip.w + clip.x - 1;
    if (x1 > x2) return;
    setPixel(data + bytesPerPixel * x1 + pitch * y, colour.argb, x2 - x1 + 1, 0);
}

void __cdecl Bitmap::DrawCircleLines(Point c, int x, int y, PackedColor colour, bool& newLine, Rect clip)
{
    if (newLine) {
        DrawHLine(c.x - x + 1, c.x + x - 1, c.y + y + 1, colour, clip);
        DrawHLine(c.x - x + 1, c.x + x - 1, c.y - y - 1, colour, clip);
    }
    DrawHLine(c.x - y, c.x + y, c.y + x, colour, clip);
    DrawHLine(c.x - y, c.x + y, c.y - x, colour, clip);
    newLine = false;
}

void __cdecl Bitmap::Circle(Point c, int r, PackedColor colour, int mode, const Rect& clip)
{
    if (format == UNSPECIFIED) return;
    if (c.x + r < clip.x || c.x - r > clip.x + clip.w - 1 || c.y + r < clip.y || c.y - r > clip.y + clip.h - 1) return;
    int d = 1 - r;
    int deltaSE = 5 - 2 * r;
    int deltaE = 3;
    int x = 0, y = r;
    bool newLine = false;
    const u32 alpha = colour.argb >> 24;
    PackedColor half = colour;
    if (!alpha) half.argb |= 0xff000000u;  // overwritten below where it is used
    const bool aa = (mode & 2) != 0;
    const bool filled = (mode & 1) != 0;
    if (aa) {
        half.argb = (half.argb & 0xffffffu) | (alpha >> 1) << 24;
        if (!filled) DrawCirclePoints(c, 0, r, half, clip);
        DrawCirclePoints(c, 0, r + 1, half, clip);
    } else if (!filled) {
        DrawCirclePoints(c, 0, r, colour, clip);
    }
    if (r > 0) {
        do {
            if (d < 0) {
                d += deltaE;
                deltaSE += 2;
            } else {
                d += deltaSE;
                deltaSE += 4;
                --y;
                newLine = x < y;
            }
            deltaE += 2;
            ++x;
            if (aa) {
                const double dist = std::sqrt(double(y * y + x * x)) + 0.5 - double(r);
                const double A = double(alpha);
                PackedColor p = half;
                p.argb = (p.argb & 0xffffffu) | u32(u8(int(A * dist))) << 24;
                if (!filled) DrawCirclePoints(c, x, y, p, clip);
                p.argb = (p.argb & 0xffffffu) | u32(u8(int(A * (1.0 - dist)))) << 24;
                half = p;
                DrawCirclePoints(c, x, y + 1, p, clip);
            }
            if (filled)
                DrawCircleLines(c, x, y, colour, newLine, clip);
            else if (!aa)
                DrawCirclePoints(c, x, y, colour, clip);
        } while (y > x);
    }
    if (filled) DrawHLine(c.x - r, c.x + r, c.y, colour, clip);
}

void Bitmap::Circle(Point c, int r, PackedColor colour, int mode) { Circle(c, r, colour, mode, Rect{0, 0, width, height}); }

void __cdecl Bitmap::Sort(Point& a, Point& b, Point& c)
{
    Point t;
    if (b.y < a.y) {
        t = a;
        a = b;
        b = t;
    }
    if (c.y < a.y) {
        t = a;
        a = c;
        c = t;
    }
    if (c.y < b.y) {
        t = b;
        b = c;
        c = t;
    }
}

void __cdecl Bitmap::TraceLine(Point a, Point b, ScanLine* table, bool side)
{
    const int dy = b.y - a.y;
    int x = a.x;
    if (!dy) {
        table[0].edge[side] = x;
        return;
    }
    int dx = b.x - a.x;
    int step;
    if (dx < 0) {
        step = -1;
        dx = -dx;
    } else {
        step = dx > 0 ? 1 : 0;
    }
    if (dy < 0) return;
    int acc = 0;
    ScanLine* p = table;
    for (int n = dy + 1; n; --n, ++p) {
        acc += dx;
        p->edge[side] = x;
        while (acc >= dy) {
            acc -= dy;
            x += step;
        }
    }
}

// TraceLine that also draws the edge's antialiasing pixels
void __cdecl Bitmap::TraceLineAlpha(Point a, Point b, PackedColor colour, ScanLine* table, bool side)
{
    const int s = side ? 1 : 0;
    int x = a.x;
    int y = a.y;
    u32 lines = 1;
    table[0].edge[s] = x;
    int dx = b.x - x;
    int dir = 1;
    if (dx < 0) {
        dir = -1;
        dx = -dx;
    }
    const u32 adx = u32(dx);
    u32 dy = u32(b.y - a.y);
    if (!dy) {
        if (adx) table[1].edge[s] = dir * int(adx) + x;
        return;
    }
    if (!adx) {
        for (ScanLine* p = table + 1; dy; --dy, ++p) p->edge[s] = x;
        return;
    }
    if (adx == dy) {
        for (ScanLine* p = table + 1; dy; --dy, ++p) {
            x += dir;
            p->edge[s] = x;
        }
        return;
    }
    u32 xorv;
    if (dir == -1)
        xorv = int(adx) > int(dy) ? (s ? 0 : 0xfe) : (s ? 0xfe : 0);
    else
        xorv = int(adx) > int(dy) ? (s ? 0xfe : 0) : (s ? 0 : 0xfe);
    const u32 c = colour.argb;
    const u32 A = c >> 24;
    auto plot = [&](int px, int py, u32 al) {
        if (px >= 0 && px <= width - 1 && py >= 0 && py <= height - 1)
            setPixel(data + bytesPerPixel * px + pitch * py, (c & 0xffffffu) | (al & 0xff) << 24, 1, 0);
    };
    u32 acc = 0;
    if (adx < dy) {
        const u32 step = ((adx << 16) / dy) & 0xffff;
        u32 n = dy - 1;
        lines = dy;
        if (n) {
            ScanLine* p = table + 1;
            int px = (s ? 1 : -1) + x;
            do {
                acc += step;
                if (acc & 0xffff0000u) {
                    acc &= 0xffff;
                    x += dir;
                    px += dir;
                }
                ++y;
                p->edge[s] = x;
                ++p;
                plot(px, y, (((acc >> 8) ^ xorv) * A) >> 8);
            } while (--n);
        }
    } else {
        const u32 step = ((dy << 16) / adx) & 0xffff;
        u32 n = adx - 1;
        if (n) {
            ScanLine* p = table + 1;
            const int yoff = ((s && dir == -1) || (!s && dir == 1)) ? 1 : 0;
            const int xoff = (!s && dir == 1) ? -1 : (s && dir == -1) ? 1 : 0;
            do {
                acc += step;
                if (acc & 0xffff0000u) {
                    ++lines;
                    acc &= 0xffff;
                    p->edge[s] = x;
                    ++p;
                    ++y;
                }
                x += dir;
                plot(x + xoff, y + yoff, (((acc >> 8) ^ xorv) * A) >> 8);
            } while (--n);
        }
    }
    table[lines].edge[s] = x;
}

void __cdecl Bitmap::Fill(const ScanTable* t, PackedColor colour)
{
    if (format == UNSPECIFIED) return;
    unsigned char* row = data + pitch * t->startLine;
    for (int i = 0; i < t->lines; ++i, row += pitch) {
        const ScanLine& l = t->scanLines[i];
        const int n = l.edge[1] - l.edge[0] + 1;
        if (n > 0) setPixel(row + l.edge[0] * bytesPerPixel, colour.argb, n, 0);
    }
}

void __cdecl Bitmap::Triangle(Point p1, Point p2, Point p3, PackedColor colour, int mode, const Rect& clip)
{
    if (format == UNSPECIFIED) return;
    if (mode == 0 || mode == 2) {
        Line(p1, p2, colour, mode, clip);
        Line(p2, p3, colour, mode, clip);
        Line(p3, p1, colour, mode, clip);
        return;
    }
    Sort(p1, p2, p3);
    Point q1 = p1, q2 = p2, q3 = p3;
    const bool inside = Contains(clip, p1) && Contains(clip, p2) && Contains(clip, p3);
    const int cross = p2.x * (p1.y - p3.y) + p2.y * (p3.x - p1.x) - (p1.y * (p3.x - p1.x) + p1.x * (p1.y - p3.y));
    const int height_ = p3.y - p1.y;
    ClipLine(p1, q2, clip);
    ClipLine(p2, q3, clip);
    ClipLine(p3, q1, clip);
    if (p1.x == -1 && p2.x == -1 && p3.x == -1 && q1.x == -1 && q2.x == -1 && q3.x == -1) return;
    // the original's alloca'd table: {startLine, lines, scanLines} + (height + 1) entries (plus a margin here, where
    // the original would write into its own frame)
    std::vector<ScanLine> lines(size_t(height_ < 0 ? 0 : height_) + 1 + 64);
    ScanTable table{0, 0, lines.data()};
    bool flag;
    if (cross < 0) {
        flag = false;
    } else if (cross == 0) {
        Line(p1, p3, colour, (mode & 2) | 1, clip);
        return;
    } else {
        flag = true;
    }
    int minY = clip.y;
    if (inside) {
        table.lines = height_ + 1;
        table.startLine = p1.y;
        minY = p1.y;
    } else {
        int maxY = clip.y;
        if (p3.x != -1 && maxY < p3.y) maxY = p3.y;
        if (q3.x != -1 && maxY < q3.y) maxY = q3.y;
        if (p2.x != -1 && maxY < p2.y) maxY = p2.y;
        if (q2.x != -1 && maxY < q2.y) maxY = q2.y;
        if (p1.x != -1 && minY < p1.y) minY = p1.y;
        if (q1.x != -1 && minY < q1.y) minY = q1.y;
        table.lines = maxY - minY;
        table.startLine = minY;
        lines[0].edge[0] = clip.x;
        lines[0].edge[1] = clip.w + clip.x - 1;
        if (!table.lines) return;
        if (size_t(table.lines) + 1 > lines.size()) {
            lines.resize(size_t(table.lines) + 1 + 64);
            table.scanLines = lines.data();
        }
        Repeat8(lines.data() + 1, lines.data(), table.lines);
    }
    int off = (q2.y < p2.y ? p2.y : q2.y) - minY;
    if (off <= 0) off = 0;
    if (mode == 3) {
        if (!(q1.x == -1 && p3.x == -1)) TraceLineAlpha(q1, p3, colour, table.scanLines, flag);
        if (!(p1.x == -1 && q2.x == -1)) TraceLineAlpha(p1, q2, colour, table.scanLines, !flag);
        if (!(p2.x == -1 && q3.x == -1)) TraceLineAlpha(p2, q3, colour, table.scanLines + off, !flag);
    } else if (mode == 1) {
        if (!(q1.x == -1 && p3.x == -1)) TraceLine(q1, p3, table.scanLines, flag);
        if (!(p1.x == -1 && q2.x == -1)) TraceLine(p1, q2, table.scanLines, !flag);
        if (!(p2.x == -1 && q3.x == -1)) TraceLine(p2, q3, table.scanLines + off, !flag);
    }
    Fill(&table, colour);
}

void Bitmap::Triangle(Point a, Point b, Point c, PackedColor colour, int mode)
{
    Triangle(a, b, c, colour, mode, Rect{0, 0, width, height});
}

// ---- images

void __cdecl Bitmap::Image(const Bitmap& src, const Point& at, const Rect& srcArea, int, const Rect& clip)
{
    if (format == UNSPECIFIED || !data) return;
    const Rect d = Intersect(Rect{at.x, at.y, srcArea.w, srcArea.h}, clip);
    const Rect s = Intersect(Rect{0, 0, src.width, src.height}, srcArea);
    if (!d.w || !d.h || !s.w || !s.h) return;
    const unsigned char* sp = src.data + src.bytesPerPixel * (s.x + (d.x - at.x)) + src.pitch * (s.y + (d.y - at.y));
    unsigned char* dp = data + bytesPerPixel * d.x + pitch * d.y;
    const BlitterFunc blit = GetBlitter(src.format, true);
    if (!blit) return;
    const int rows = s.h > d.h ? d.h : s.h;
    const int cols = s.w > d.w ? d.w : s.w;
    for (int n = rows; n; --n) {
        blit(dp, sp, cols);
        sp += src.pitch;
        dp += pitch;
    }
}

void __cdecl Bitmap::CopyImage(const Bitmap& src, const Point& at, const Rect& srcArea, int, const Rect& clip)
{
    if (format == UNSPECIFIED || !data) return;
    const Rect d = Intersect(Rect{at.x, at.y, srcArea.w, srcArea.h}, clip);
    const Rect s = Intersect(Rect{0, 0, src.width, src.height}, srcArea);
    if (!d.w || !d.h || !s.w || !s.h) return;
    int dx = d.x, dy = d.y;
    if (srcArea.x < 0) dx -= srcArea.x;
    if (srcArea.y < 0) dy -= srcArea.y;
    const unsigned char* sp = src.data + src.bytesPerPixel * (s.x + (dx - at.x)) + src.pitch * (s.y + (dy - at.y));
    unsigned char* dp = data + bytesPerPixel * dx + pitch * dy;
    const BlitterFunc blit = GetBlitter(src.format, false);
    if (!blit) return;
    const int rows = s.h > d.h ? d.h : s.h;
    const int cols = s.w > d.w ? d.w : s.w;
    for (int n = rows; n; --n) {
        blit(dp, sp, cols);
        sp += src.pitch;
        dp += pitch;
    }
}

// tiles srcArea of src over destArea: whole tiles while they start left of clip.x + srcArea.w, then the rest of
// each row copied from the pixels one tile width to the left
void __cdecl Bitmap::Image(const Bitmap& src, const Rect& destArea, const Rect& srcArea, int flags, const Rect& clip)
{
    if (format == UNSPECIFIED) return;
    const Rect c = Intersect(destArea, clip);
    if (!c.w || !c.h) return;
    int x = destArea.x;
    while (x + srcArea.w < c.x) x += srcArea.w;
    int y = destArea.y;
    while (y + srcArea.h < c.y) y += srcArea.h;
    int tx = x;
    if (tx < srcArea.w + c.x) {
        do {
            for (int ty = y; ty < c.y + c.h; ty += srcArea.h) Image(src, Point{tx, ty}, srcArea, flags, c);
            tx += srcArea.w;
        } while (tx < srcArea.w + c.x);
    }
    if (tx >= c.w + c.x) return;
    unsigned char* p = data + bytesPerPixel * tx + pitch * c.y;
    for (int n = c.h; n > 0; --n) {
        blitSelf(p, p - srcArea.w * bytesPerPixel, c.x - tx + c.w);
        p += pitch;
    }
}

void Bitmap::Image(const Bitmap& src, const Point& at, const Rect& srcArea, int flags)
{
    Image(src, at, srcArea, flags, Rect{0, 0, width, height});
}
void Bitmap::Image(const Bitmap& src, const Point& at, int flags)
{
    Image(src, at, Rect{0, 0, src.width, src.height}, flags, Rect{0, 0, width, height});
}
void Bitmap::CopyImage(const Bitmap& src, const Point& at, const Rect& srcArea, int flags)
{
    CopyImage(src, at, srcArea, flags, Rect{0, 0, width, height});
}
void Bitmap::CopyImage(const Bitmap& src, const Point& at, int flags)
{
    CopyImage(src, at, Rect{0, 0, src.width, src.height}, flags, Rect{0, 0, width, height});
}

// ---- textured polygons. Two edge walks (one per side) from the top vertex to the bottom one fill a table of
// {x, y, u, v} per row and side; each row is then drawn left to right, one blitter call per pixel. All single
// precision as in the original (SSE scalar); the edge ends are rounded with floor / ceil in double.
namespace {

struct Sample {
    float x, y, u, v;
};

}  // namespace

void __cdecl Bitmap::MapPolygon(int count, const Point* pts, const Bitmap& tex, const Point* tpts, int xmask, int ymask,
                                bool blend)
{
    if (format == UNSPECIFIED) return;
    BlitterFunc blit;
    switch (tex.format) {
    case RGB:
        blit = blitRGB888;
        break;
    case ARGB:
    case XRGB:
        blit = blend ? blendARGB888 : blitARGB888;
        break;
    case RGB565:
        blit = blitRGB565;
        break;
    default:
        return;
    }
    if (!blit) return;
    int top = 0, bot = 0;
    for (int i = 1; i < count; ++i) {
        if (pts[i].y < pts[top].y) top = i;
        if (pts[i].y > pts[bot].y) bot = i;
    }
    if (pts[bot].y < 0 || pts[top].y > height) return;
    const int rowBase = -pts[top].y;
    const int H = pts[bot].y - pts[top].y;
    std::vector<Sample> buf(size_t(H + 1) * 2);
    // the steps the original keeps in its frame between edges and rows (uninitialised before their first use, which
    // can only reach results through a zero-length span)
    float xs = 0, ys = 0, us = 0, vs = 0;
    for (int side = 0; side < 2; ++side) {
        int cur = top;
        int next;
        do {
            next = (side ? cur + 1 : cur + count - 1) % count;
            float x0 = float(pts[cur].x), y0 = float(pts[cur].y);
            float u0 = float(tpts[cur].x), v0 = float(tpts[cur].y);
            const float x1 = float(pts[next].x), y1 = float(pts[next].y);
            const float u1 = float(tpts[next].x), v1 = float(tpts[next].y);
            int n = int(y1 - y0);
            if (n) {
                if (n < 0) n = -n;
                const float inv = 1.0f / float(n);
                xs = inv * (x1 - x0);
                ys = (y1 - y0) * inv;
            }
            int m;
            if (n) {
                m = n;
            } else {
                const float du = u1 - u0, dv = v1 - v0;
                m = int(du > dv ? du : dv);
            }
            if (m) {
                if (m < 0) m = -m;
                const float inv = 1.0f / float(m);
                us = inv * (u1 - u0);
                vs = (v1 - v0) * inv;
            }
            Sample* e = &buf[size_t((int(y0) + rowBase) * 2 + side)];
            int k = n;
            do {
                *e = Sample{x0, y0, u0, v0};
                u0 = us + u0;
                v0 = vs + v0;
                x0 = xs + x0;
                y0 = ys + y0;
                e += 2;
            } while (--k >= 0);
            cur = next;
        } while (next != bot);
    }
    if (H < 0) return;
    float rowXs = xs, rowYs = ys;
    for (int row = 0; row <= H; ++row) {
        Sample* s0 = &buf[size_t(row) * 2];
        Sample* s1 = s0 + 1;
        const float y = s0->y;
        if (0 > y || y >= float(height)) continue;
        Sample* l = s1->x > s0->x ? s0 : s1;
        Sample* r = s1->x > s0->x ? s1 : s0;
        l->x = float(std::floor(double(l->x)));
        const float rx = float(std::ceil(double(r->x)));
        r->x = rx;
        if (l->x >= float(width) || 0 > rx) continue;
        int L = int(rx - l->x);
        if (L) {
            if (L < 0) L = -L;
            const float inv = 1.0f / float(L);
            rowXs = inv * (r->x - l->x);
            rowYs = (r->y - l->y) * inv;
        }
        // 0x10004ff0: the texture step over the span
        int nuv = L;
        if (!nuv) {
            const float a = r->v - l->v, b = r->u - l->u;
            nuv = int(b > a ? b : a);
        }
        if (nuv) {
            if (nuv < 0) nuv = -nuv;
            const float inv = 1.0f / float(nuv);
            us = inv * (r->u - l->u);
            vs = (r->v - l->v) * inv;
        }
        int sx = int(l->x + 0.5f);
        if (sx < 0) {
            int k = -sx;
            if (k > nuv) k = nuv;
            const float fk = float(k);
            l->v = vs * fk + l->v;
            l->u = l->u + us * fk;
            nuv -= k;
            int k2 = -int(l->x + 0.5f);
            if (k2 > L) k2 = L;
            const float fk2 = float(k2);
            l->y = rowYs * fk2 + l->y;
            l->x = l->x + rowXs * fk2;
            L -= k2;
        }
        sx = int(l->x + 0.5f);
        if (L + sx > width) L = width - sx;
        unsigned char* dp = data + pitch * int(l->y + 0.5f) + bytesPerPixel * sx;
        do {
            const int tu = int(l->u + 0.5f) & xmask;
            const int tv = int(l->v + 0.5f) & ymask;
            blit(dp, tex.data + tex.pitch * tv + tex.bytesPerPixel * tu, 1);
            l->u = l->u + us;
            l->v = vs + l->v;
            dp += bytesPerPixel;
            l->x = l->x + rowXs;
            l->y = rowYs + l->y;
            --nuv;
        } while (--L >= 0);
    }
}

// ---- whole-bitmap operations

void __cdecl Bitmap::Clear()
{
    if (format == UNSPECIFIED) return;
    const int row = bytesPerPixel * width;
    if (row - pitch == 0) {
        std::memset(data, 0, size_t(height * pitch));
        return;
    }
    unsigned char* p = data;
    for (int i = 0; i < height; ++i, p += pitch) std::memset(p, 0, size_t(row));
}

void __cdecl Bitmap::Clear(unsigned long colour)
{
    if (format == UNSPECIFIED) return;
    unsigned char* p = data;
    for (int i = 0; i < height; ++i, p += pitch) setPixel(p, u32(colour), width, 0);
}

void __cdecl Bitmap::Stipple(const Rect& area, PackedColor colour, const Rect& clip)
{
    const Rect r = Intersect(clip, area);
    unsigned char* p = data + pitch * r.y + bytesPerPixel * r.x;
    int parity = (r.y + r.x) & 1;
    if (r.w <= 1) return;
    for (int n = r.h; n; --n) {
        const int off = parity ? bytesPerPixel : 0;
        parity ^= 1;
        setPixel(p + off, colour.argb, (r.w + parity) / 2, bytesPerPixel * 2);
        p += pitch;
    }
}

void __cdecl Bitmap::GrayScale(const Rect& area, bool stipple, const Rect& clip)
{
    const Rect r = Intersect(clip, area);
    unsigned char* p = data + bytesPerPixel * r.x + pitch * r.y;
    std::vector<u32> tmp(size_t(r.w > 0 ? r.w : 0) + 1);
    u8 rowParity = stipple ? u8((u8(r.y) ^ u8(r.x)) & 1) : 0;
    for (int n = r.h; n; --n) {
        getPixel(tmp.data(), p, r.w);
        u8 parity = rowParity;
        u8* q = reinterpret_cast<u8*>(tmp.data());
        for (int i = 0; i < r.w; ++i, q += 4) {
            const u8 g = u8((q[0] + q[1] + q[2]) / 3);
            q[0] = g;
            q[1] = g;
            q[2] = g;
            if (stipple) {
                q[3] = u8(q[3] >> parity);
                parity ^= 1;
            }
        }
        blitARGB888(p, tmp.data(), r.w);
        p += pitch;
        rowParity ^= 1;
    }
}

void Bitmap::GrayScale(const Rect& area, const Rect& clip) { GrayScale(area, false, clip); }
void Bitmap::GrayScale(const Rect& area, bool stipple) { GrayScale(area, stipple, Rect{0, 0, width, height}); }
void Bitmap::GrayScale(const Rect& area) { GrayScale(area, false, Rect{0, 0, width, height}); }

// counts the pixels of `colour` from `from` towards `to` (a horizontal walk unless from.x == to.x)
int __cdecl Bitmap::LineScan(const Point& from, const Point& to, unsigned long colour)
{
    int x = from.x, y = from.y;
    int count = 0;
    auto read = [&]() {
        u32 v = 0xffffffffu;
        if (x >= 0 && x <= width - 1 && y >= 0 && y <= height - 1)
            getPixel(&v, data + bytesPerPixel * x + pitch * y, 1);
        return v;
    };
    if (x == to.x) {
        const int dir = y <= to.y ? 1 : -1;
        for (;;) {
            if (x == to.x && y == to.y) return 0;
            ++count;
            if (read() != u32(colour)) return count ? count : 1;
            y += dir;
        }
    }
    const int dir = x <= to.x ? 1 : -1;
    for (;;) {
        if (x == to.x && y == to.y) return 0;
        ++count;
        if (read() != u32(colour)) return count;
        x += dir;
    }
}

Bitmap* __cdecl Bitmap::Copy() const
{
    if (!width || !height || !pitch) return nullptr;
    Bitmap* b = NewBitmap();
    b->Init(Size{width, height}, pitch, format, nullptr, int(dataSize));
    std::memcpy(b->data, data, dataSize);
    return b;
}

// a box filter down by powers of two
Bitmap* __cdecl Bitmap::Scale(int w, int h) const
{
    auto pow2 = [](u32 v) { return (v & ((~(v - 1)) * 2 | (v - 1))) == 0; };
    if (!(format == ARGB || format == RGB || format == XRGB) || !pow2(u32(width)) || !pow2(u32(height)) ||
        !pow2(u32(w)) || !pow2(u32(h)) || w > width || h > height || w <= 0 || h <= 0 || (pitch & 3))
        return nullptr;
    void* mem = Alloc::New(sizeof(Bitmap));
    Bitmap* b = nullptr;
    if (mem) {
        b = static_cast<Bitmap*>(mem);
        b->alloc = false;
        b->data = nullptr;
        b->palette = nullptr;
        b->Init(Size{w, h}, 0, format, nullptr, 0);
    }
    const int kx = width / w, ky = height / h;
    int shift = 0;
    for (int n = ky * kx; (n /= 2) != 0;) ++shift;
    const u32 sp = u32(pitch);
    const u8* s = data;
    u8* d = b->data;
    if (format == ARGB || format == XRGB) {
        for (int y = h; y; --y) {
            for (int x = w; x; --x) {
                u32 c0 = 0, c1 = 0, c2 = 0, c3 = 0;
                const u8* q = s;
                for (int j = ky; j > 0; --j) {
                    for (int i = kx; i > 0; --i) {
                        c0 += q[0];
                        c1 += q[1];
                        c2 += q[2];
                        c3 += q[3];
                        q += 4;
                    }
                    q += sp - kx * 4;
                }
                d[3] = u8(c3 >> (shift & 31));
                d[0] = u8(c0 >> (shift & 31));
                d[2] = u8(c2 >> (shift & 31));
                d[1] = u8(c1 >> (shift & 31));
                d += 4;
                s += kx * 4;
            }
            s += ((sp >> 2) * ky - width) * 4;
            d += ((u32(b->pitch) >> 2) - b->width) * 4;
        }
    } else {
        for (int y = h; y; --y) {
            for (int x = w; x; --x) {
                u32 c0 = 0, c1 = 0, c2 = 0;
                const u8* q = s;
                for (int j = ky; j > 0; --j) {
                    for (int i = kx; i > 0; --i) {
                        c0 += q[0];
                        c1 += q[1];
                        c2 += q[2];
                        q += 3;
                    }
                    q += sp - kx * 3;
                }
                d[2] = u8(c2 >> (shift & 31));
                d[1] = u8(c1 >> (shift & 31));
                d[0] = u8(c0 >> (shift & 31));
                d += 3;
                s += kx * 3;
            }
            s += ((sp / 3) * ky - width) * 3;
            d += (u32(b->pitch) / 3 - b->width) * 3;
        }
    }
    return b;
}

// bilinear when both sides grow, else (32-bit only) nearest with an error term; rows are addressed by width * 4
Bitmap* __cdecl Bitmap::ScaleArbitrary(int w, int h) const
{
    const bool supported = format == ARGB || format == RGB || format == XRGB;
    const int sw = width, sh = height;
    if (w <= 0 || (pitch & 3) || !((sw <= w && sh <= h) || format == ARGB || format == XRGB) || !supported)
        return nullptr;
    void* mem = Alloc::New(sizeof(Bitmap));
    Bitmap* b = nullptr;
    if (mem) {
        b = static_cast<Bitmap*>(mem);
        b->alloc = false;
        b->data = nullptr;
        b->palette = nullptr;
        b->Init(Size{w, h}, 0, format, nullptr, 0);
    }
    if (sw < w && sh < h) {
        if (format == ARGB || format == XRGB) {
            const float xr = float(sw) / float(w);
            const float yr = float(sh) / float(h);
            const u32* src = reinterpret_cast<const u32*>(data);
            const int stride = int(u32(pitch) >> 2) - sw + sw;
            const int dstride = int(u32(b->pitch) >> 2) - b->width + w;
            for (int y = 0; y < h; ++y) {
                const float fy = float(y) * yr;
                const int iy = int(fy);
                const int iy1 = iy + 1 >= sh ? iy : iy + 1;
                u32* d = reinterpret_cast<u32*>(b->data) + dstride * y;
                for (int x = 0; x < w; ++x) {
                    const float fx = float(x) * xr;
                    const int ix = int(fx);
                    const int ix1 = ix + 1 >= sw ? ix : ix + 1;
                    const float fxf = fx - float(ix);
                    const u32 p00 = src[stride * iy + ix], p01 = src[stride * iy + ix1];
                    const u32 p10 = src[stride * iy1 + ix], p11 = src[stride * iy1 + ix1];
                    const float fyf = fy - float(iy);
                    *d = 0;
                    const float gx = 1.0f - fxf, gy = 1.0f - fyf;
                    u32 out = 0;
                    for (int c = 0; c < 32; c += 8) {
                        const u32 lo = u8(int(float(u8(p10 >> c)) * gx + float(u8(p11 >> c)) * fxf));
                        const u32 hi = u8(int(float(u8(p00 >> c)) * gx + float(u8(p01 >> c)) * fxf));
                        out |= u32(u8(int(float(lo) * fyf + float(hi) * gy))) << c;
                    }
                    *d++ = out;
                }
            }
        } else {
            const float xr = float(sw) / float(w);
            const float yr = float(sh) / float(h);
            const int stride = int(u32(pitch) / 3) - sw + sw;
            const int dstride = int(u32(b->pitch) / 3) - b->width + w;
            for (int y = 0; y < h; ++y) {
                const float fy = float(y) * yr;
                const int iy = int(fy);
                const int iy1 = iy + 1 >= sh ? iy : iy + 1;
                for (int x = 0; x < w; ++x) {
                    const float fx = float(x) * xr;
                    const int ix = int(fx);
                    const int ix1 = ix + 1 >= sw ? ix : ix + 1;
                    const u8* ra = data + stride * iy * 3 + ix * 3;
                    const u8* rb = data + stride * iy1 * 3 + ix * 3;
                    const int nx = ix1 * 3 - ix * 3;
                    const float fxf = fx - float(ix);
                    const float fyf = fy - float(iy);
                    const float gx = 1.0f - fxf, gy = 1.0f - fyf;
                    u8* d = b->data + dstride * y * 3 + x * 3;
                    for (int c = 0; c < 3; ++c) {
                        const u32 lo = u8(int(float(rb[c + nx]) * fxf + float(rb[c]) * gx));
                        const u32 hi = u8(int(float(ra[c + nx]) * fxf + float(ra[c]) * gx));
                        d[c] = u8(int(float(lo) * fyf + float(hi) * gy));
                    }
                }
            }
        }
    } else if (format == ARGB || format == XRGB) {
        const int qy = sh / h, ry = sh % h;
        const int qx = sw / w, rx = sw % w;
        const u32* row = reinterpret_cast<const u32*>(data);
        u32* d = reinterpret_cast<u32*>(b->data);
        int ey = 0;
        for (int n = h; n > 0; --n) {
            const u32* p = row;
            int ex = 0;
            int k = w;
            do {
                const u32 v = *p;
                ex += rx;
                p += qx;
                *d++ = v;
                --k;
                if (ex >= w) {
                    ex -= w;
                    ++p;
                }
            } while (k > 0);
            ey += ry;
            row += qy * sw;
            if (ey >= h) {
                ey -= h;
                row += sw;
            }
        }
    }
    return b;
}

// the area's pixels; the source rows are found by width (not pitch), the new rows are packed
Bitmap* __cdecl Bitmap::Crop(const Rect& area) const
{
    Bitmap* b = NewBitmap();
    b->Init(*reinterpret_cast<const Size*>(&area.w), 0, format, nullptr, 0);
    if (area.x >= 0 && area.y >= 0 && area.w >= 0 && area.h >= 0 && area.w + area.x <= width &&
        area.h + area.y <= height) {
        const size_t row = size_t(bytesPerPixel * area.w);
        const u8* s = data + (width * area.y + area.x) * bytesPerPixel;
        u8* d = b->data;
        for (int i = 0; i < area.h; ++i) {
            std::memcpy(d, s, row);
            s += pitch;
            d += row;
        }
    }
    return b;
}

Bitmap* __cdecl Bitmap::FlipY() const
{
    if (!width || !height || !pitch) return nullptr;
    Bitmap* b = NewBitmap();
    const size_t row = size_t(bytesPerPixel * width);
    b->Init(Size{width, height}, pitch, format, nullptr, int(dataSize));
    u8* d = b->data;
    const u8* s = data + (height - 1) * pitch;
    for (int i = 0; i < height; ++i) {
        std::memcpy(d, s, row);
        d += pitch;
        s -= pitch;
    }
    return b;
}

Bitmap* Bitmap::RotateClockwise(int degrees)
{
    if (!width || !height || !pitch || degrees % 90 != 0) return nullptr;
    if (degrees > 360) degrees = degrees - int((u32(degrees) - 361u) / 360u) * 360 - 360;
    if (degrees < 0) degrees = degrees + 360 + int((u32(-degrees) - 1u) / 360u) * 360;
    if (degrees == 0 || degrees == 360) return Copy();
    Bitmap* b = NewBitmap();
    Size s = degrees == 180 ? Size{width, height} : Size{height, width};
    b->Init(s, bytesPerPixel * s.w, format, nullptr, int(dataSize));
    const size_t bpp = size_t(bytesPerPixel);
    u8* d = b->data;
    if (degrees == 90) {
        for (int x = 0; x < width; ++x)
            for (int y = height - 1; y >= 0; --y, d += bpp) std::memcpy(d, data + bpp * x + pitch * y, bpp);
    } else if (degrees == 180) {
        for (int y = height - 1; y >= 0; --y)
            for (int x = width - 1; x >= 0; --x, d += bpp) std::memcpy(d, data + bpp * x + pitch * y, bpp);
    } else if (degrees == 270) {
        for (int x = width - 1; x >= 0; --x)
            for (int y = 0; y < height; ++y, d += bpp) std::memcpy(d, data + bpp * x + pitch * y, bpp);
    }
    return b;
}

bool __cdecl Bitmap::SetTransparency(float opacity)
{
    if (format != ARGB && format != XRGB) return false;
    float f = 0.0f;
    if (opacity > 0.0f) f = opacity > 1.0f ? 1.0f : opacity;
    const u32 k = u8(int(f * 255.0f));
    u8* p = data;
    const int skip = (int(u32(pitch) >> 2) - width) * 4;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x, p += 4)
            if (p[3]) p[3] = u8(int(p[3] * k) >> 8);
        p += skip;
    }
    return true;
}

// a grey image's blue channel becomes the alpha of `colour` (premultiplied by its alpha unless opaque)
void __cdecl Bitmap::Colorize(PackedColor colour)
{
    if (format != ARGB && format != XRGB) return;
    u32 c = colour.argb;
    if (c < 0xff000000u) {
        const u8* t = mulTable.t[c >> 24];
        c = (c & 0xff000000u) | u32(t[c >> 16 & 0xff]) << 16 | u32(t[c >> 8 & 0xff]) << 8 | t[c & 0xff];
    }
    const u32 rgb = (c | 0xff000000u) & 0xffffffu;
    for (int y = 0; y < height; ++y) {
        u32* p = reinterpret_cast<u32*>(data + pitch * y);
        for (int x = 0; x < width; ++x, ++p)
            if (*p) *p = *p << 24 | rgb;
    }
}

void __cdecl Bitmap::ColorReplace(PackedColor to, PackedColor from)
{
    if (format != ARGB) return;
    for (int y = 0; y < height; ++y) {
        u32* p = reinterpret_cast<u32*>(data + pitch * y);
        for (int x = 0; x < width; ++x, ++p)
            if (*p && *p == from.argb) *p = to.argb;
    }
}

void __cdecl Bitmap::ColorFill(PackedColor colour)
{
    if (format != ARGB) return;
    for (int y = 0; y < height; ++y) {
        u32* p = reinterpret_cast<u32*>(data + pitch * y);
        for (int x = 0; x < width; ++x, ++p)
            if (*p) *p = colour.argb;
    }
}

}  // namespace Jet

