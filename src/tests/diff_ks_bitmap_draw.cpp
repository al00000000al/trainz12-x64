// Differential test of KernelStandard unit U8, drawing (Bitmap.cpp). Each scenario draws, once, a destination and a
// source bitmap (format, size, random pixels) and a list of operations, then runs them on our module's bitmaps and
// on the original's: Pixel / BlendPixel / Pixel(), Line (plain, blended, antialiased, clipped), Box, Circle (all
// modes), Triangle (outlined, filled, filled + antialiased), Image / CopyImage / tiled Image between all format
// pairs, Stipple, GrayScale, LineScan, Clear, MapPolygon, Scale, ScaleArbitrary, Crop, FlipY, RotateClockwise,
// Copy, SetTransparency, Colorize, ColorReplace, ColorFill, Init over external pixels, DetachAlias. After each
// operation every bitmap's header (size, pitch, bytes per pixel, format, data size) and its pixel rows are logged
// (row bytes up to width * bytes per pixel: the padding is never written).
//
// usage: diff_ks_bitmap_draw.exe <dir with our KernelStandard_7c.dll and KernelStandard_7c_orig.dll> [scenarios]
// KBD_SWAP: the sides swapped; KBD_DUMP: every log line.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

template <class F>
static F Get(HMODULE m, const char* name)
{
    auto p = reinterpret_cast<F>(GetProcAddress(m, name));
    if (!p) {
        std::printf("missing %s\n", name);
        std::exit(2);
    }
    return p;
}

static int side;
static std::vector<std::string> logs[2];
static void Log(const std::string& s) { logs[side].push_back(s); }
static std::string Num(long long v) { return std::to_string(v); }
static std::string Hex(const void* p, size_t n)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<const unsigned char*>(p)[i];
        s += digits[c >> 4];
        s += digits[c & 15];
    }
    return s;
}

struct Rect {
    int x, y, w, h;
};
struct Point {
    int x, y;
};
struct Size {
    int w, h;
};
struct Colour {
    unsigned int argb;
};
using Bm = unsigned char;  // a 0x44-byte Bitmap

static int W(const Bm* b) { return *reinterpret_cast<const int*>(b); }
static int H(const Bm* b) { return *reinterpret_cast<const int*>(b + 4); }
static unsigned char* Data(const Bm* b) { return *reinterpret_cast<unsigned char* const*>(b + 0xc); }
static int Pitch(const Bm* b) { return *reinterpret_cast<const int*>(b + 0x10); }
static int Bpp(const Bm* b) { return *reinterpret_cast<const int*>(b + 0x14); }
static int Fmt(const Bm* b) { return *reinterpret_cast<const int*>(b + 0x18); }
static unsigned NumBytes(const Bm* b) { return *reinterpret_cast<const unsigned*>(b + 0x40); }

// ---- exports
struct Api {
    void*(__fastcall* ctor)(void*, void*);
    void*(__fastcall* ctorSize)(void*, void*, const Size*, int, unsigned char*);
    void*(__fastcall* ctorArea)(void*, void*, const Bm*, const Rect*);
    void(__fastcall* dtor)(void*, void*);
    void(__fastcall* init)(void*, void*, const Size*, int, int, unsigned char*, int);
    void(__cdecl* done)(void*);
    void(__cdecl* detach)(const void*);
    void(__cdecl* pixel)(void*, const Point*, Colour, const Rect*);
    void(__cdecl* blendPixel)(void*, const Point*, Colour, const Rect*);
    void(__fastcall* pixelT)(void*, void*, const Point*, Colour);
    void(__fastcall* blendPixelT)(void*, void*, const Point*, Colour);
    Colour*(__fastcall* getPixel)(const void*, void*, Colour*, const Point*);
    void(__cdecl* line)(void*, Point, Point, Colour, int, const Rect*);
    void(__fastcall* lineT)(void*, void*, Point, Point, Colour, int);
    void(__cdecl* box)(void*, Rect, Colour, int, const Rect*);
    void(__fastcall* boxT)(void*, void*, Rect, Colour, int);
    void(__cdecl* circle)(void*, Point, int, Colour, int, const Rect*);
    void(__fastcall* circleT)(void*, void*, Point, int, Colour, int);
    void(__cdecl* triangle)(void*, Point, Point, Point, Colour, int, const Rect*);
    void(__fastcall* triangleT)(void*, void*, Point, Point, Point, Colour, int);
    void(__cdecl* image)(void*, const Bm*, const Point*, const Rect*, int, const Rect*);
    void(__cdecl* imageTile)(void*, const Bm*, const Rect*, const Rect*, int, const Rect*);
    void(__fastcall* imageT)(void*, void*, const Bm*, const Point*, const Rect*, int);
    void(__fastcall* imageT2)(void*, void*, const Bm*, const Point*, int);
    void(__cdecl* copyImage)(void*, const Bm*, const Point*, const Rect*, int, const Rect*);
    void(__fastcall* copyImageT)(void*, void*, const Bm*, const Point*, const Rect*, int);
    void(__fastcall* copyImageT2)(void*, void*, const Bm*, const Point*, int);
    void(__cdecl* stipple)(void*, const Rect*, Colour, const Rect*);
    void(__cdecl* gray)(void*, const Rect*, bool, const Rect*);
    void(__fastcall* grayT)(void*, void*, const Rect*);
    void(__fastcall* grayTb)(void*, void*, const Rect*, bool);
    void(__fastcall* grayTc)(void*, void*, const Rect*, const Rect*);
    int(__cdecl* lineScan)(void*, const Point*, const Point*, unsigned long);
    void(__cdecl* mapPolygon)(void*, int, const Point*, const Bm*, const Point*, int, int, bool);
    Bm*(__cdecl* copy)(const void*);
    Bm*(__cdecl* scale)(const void*, int, int);
    Bm*(__cdecl* scaleArb)(const void*, int, int);
    Bm*(__cdecl* crop)(const void*, const Rect*);
    Bm*(__cdecl* flipY)(const void*);
    Bm*(__fastcall* rotate)(void*, void*, int);
    bool(__cdecl* transparency)(void*, float);
    void(__cdecl* colorize)(void*, Colour);
    void(__cdecl* colorReplace)(void*, Colour, Colour);
    void(__cdecl* colorFill)(void*, Colour);
    void(__cdecl* clear)(void*);
    void(__cdecl* clearC)(void*, unsigned long);
    unsigned(__fastcall* calcPitch)(const void*, void*, unsigned);
    Rect*(__fastcall* toRect)(const void*, void*, Rect*);
    void*(__cdecl* allocNew)(unsigned);
    void(__cdecl* allocDelete)(void*);
};
static Api apis[2];
#define B "@Bitmap@Jet@@"
static void Load(Api& a, HMODULE m, HMODULE orig)
{
    a.ctor = Get<decltype(a.ctor)>(m, "??0Bitmap@Jet@@QAE@XZ");
    a.ctorSize = Get<decltype(a.ctorSize)>(m, "??0Bitmap@Jet@@QAE@ABUSize@1@W4Format@01@PAE@Z");
    a.ctorArea = Get<decltype(a.ctorArea)>(m, "??0Bitmap@Jet@@QAE@ABV01@ABURect@1@@Z");
    a.dtor = Get<decltype(a.dtor)>(m, "??1Bitmap@Jet@@QAE@XZ");
    a.init = Get<decltype(a.init)>(m, "?Init" B "QAEXABUSize@2@HW4Format@12@PAEH@Z");
    a.done = Get<decltype(a.done)>(m, "?Done" B "QAAXXZ");
    a.detach = Get<decltype(a.detach)>(m, "?DetachAlias" B "QBAXXZ");
    a.pixel = Get<decltype(a.pixel)>(m, "?Pixel" B "QAAXABUPoint@2@UPackedColor@2@ABURect@2@@Z");
    a.blendPixel = Get<decltype(a.blendPixel)>(m, "?BlendPixel" B "QAAXABUPoint@2@UPackedColor@2@ABURect@2@@Z");
    a.pixelT = Get<decltype(a.pixelT)>(m, "?Pixel" B "QAEXABUPoint@2@UPackedColor@2@@Z");
    a.blendPixelT = Get<decltype(a.blendPixelT)>(m, "?BlendPixel" B "QAEXABUPoint@2@UPackedColor@2@@Z");
    a.getPixel = Get<decltype(a.getPixel)>(m, "?Pixel" B "QBE?AUPackedColor@2@ABUPoint@2@@Z");
    a.line = Get<decltype(a.line)>(m, "?Line" B "QAAXUPoint@2@0UPackedColor@2@HABURect@2@@Z");
    a.lineT = Get<decltype(a.lineT)>(m, "?Line" B "QAEXUPoint@2@0UPackedColor@2@H@Z");
    a.box = Get<decltype(a.box)>(m, "?Box" B "QAAXURect@2@UPackedColor@2@HABU32@@Z");
    a.boxT = Get<decltype(a.boxT)>(m, "?Box" B "QAEXURect@2@UPackedColor@2@H@Z");
    a.circle = Get<decltype(a.circle)>(m, "?Circle" B "QAAXUPoint@2@HUPackedColor@2@HABURect@2@@Z");
    a.circleT = Get<decltype(a.circleT)>(m, "?Circle" B "QAEXUPoint@2@HUPackedColor@2@H@Z");
    a.triangle = Get<decltype(a.triangle)>(m, "?Triangle" B "QAAXUPoint@2@00UPackedColor@2@HABURect@2@@Z");
    a.triangleT = Get<decltype(a.triangleT)>(m, "?Triangle" B "QAEXUPoint@2@00UPackedColor@2@H@Z");
    a.image = Get<decltype(a.image)>(m, "?Image" B "QAAXABV12@ABUPoint@2@ABURect@2@H2@Z");
    a.imageTile = Get<decltype(a.imageTile)>(m, "?Image" B "QAAXABV12@ABURect@2@1H1@Z");
    a.imageT = Get<decltype(a.imageT)>(m, "?Image" B "QAEXABV12@ABUPoint@2@ABURect@2@H@Z");
    a.imageT2 = Get<decltype(a.imageT2)>(m, "?Image" B "QAEXABV12@ABUPoint@2@H@Z");
    a.copyImage = Get<decltype(a.copyImage)>(m, "?CopyImage" B "QAAXABV12@ABUPoint@2@ABURect@2@H2@Z");
    a.copyImageT = Get<decltype(a.copyImageT)>(m, "?CopyImage" B "QAEXABV12@ABUPoint@2@ABURect@2@H@Z");
    a.copyImageT2 = Get<decltype(a.copyImageT2)>(m, "?CopyImage" B "QAEXABV12@ABUPoint@2@H@Z");
    a.stipple = Get<decltype(a.stipple)>(m, "?Stipple" B "QAAXABURect@2@UPackedColor@2@0@Z");
    a.gray = Get<decltype(a.gray)>(m, "?GrayScale" B "QAAXABURect@2@_N0@Z");
    a.grayT = Get<decltype(a.grayT)>(m, "?GrayScale" B "QAEXABURect@2@@Z");
    a.grayTb = Get<decltype(a.grayTb)>(m, "?GrayScale" B "QAEXABURect@2@_N@Z");
    a.grayTc = Get<decltype(a.grayTc)>(m, "?GrayScale" B "QAEXABURect@2@0@Z");
    a.lineScan = Get<decltype(a.lineScan)>(m, "?LineScan" B "QAAHABUPoint@2@0K@Z");
    a.mapPolygon = Get<decltype(a.mapPolygon)>(m, "?MapPolygon" B "QAAXHPBUPoint@2@ABV12@0HH_N@Z");
    a.copy = Get<decltype(a.copy)>(m, "?Copy" B "QBAPAV12@XZ");
    a.scale = Get<decltype(a.scale)>(m, "?Scale" B "QBAPAV12@HH@Z");
    a.scaleArb = Get<decltype(a.scaleArb)>(m, "?ScaleArbitrary" B "QBAPAV12@HH@Z");
    a.crop = Get<decltype(a.crop)>(m, "?Crop" B "QBAPAV12@ABURect@2@@Z");
    a.flipY = Get<decltype(a.flipY)>(m, "?FlipY" B "QBAPAV12@XZ");
    a.rotate = Get<decltype(a.rotate)>(m, "?RotateClockwise" B "QAEPAV12@H@Z");
    a.transparency = Get<decltype(a.transparency)>(m, "?SetTransparency" B "QAA_NM@Z");
    a.colorize = Get<decltype(a.colorize)>(m, "?Colorize" B "QAAXUPackedColor@2@@Z");
    a.colorReplace = Get<decltype(a.colorReplace)>(m, "?ColorReplace" B "QAAXUPackedColor@2@0@Z");
    a.colorFill = Get<decltype(a.colorFill)>(m, "?ColorFill" B "QAAXUPackedColor@2@@Z");
    a.clear = Get<decltype(a.clear)>(m, "?Clear" B "QAAXXZ");
    a.clearC = Get<decltype(a.clearC)>(m, "?Clear" B "QAAXK@Z");
    a.calcPitch = Get<decltype(a.calcPitch)>(m, "?CalcPitch" B "QBEII@Z");
    a.toRect = Get<decltype(a.toRect)>(m, "??BBitmap@Jet@@QBE?AURect@1@XZ");
    a.allocNew = Get<decltype(a.allocNew)>(orig, "?New@Alloc@Jet@@SAPAXI@Z");
    a.allocDelete = Get<decltype(a.allocDelete)>(orig, "?Delete@Alloc@Jet@@SAXPAX@Z");
}

// ---- scenario data, drawn once
static std::mt19937 rng;
static int R(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
static unsigned U32() { return rng(); }

enum Fmt { INDEX, RGB, ARGB, XRGB, RGB565, UNSPECIFIED };
static const int drawFormats[] = {RGB, ARGB, XRGB, RGB565};

static unsigned Colour_()
{
    switch (R(0, 5)) {
    case 0: return U32() & 0x00ffffff;          // alpha 0
    case 1: return U32() | 0xff000000;          // opaque
    case 2: return (U32() & 0x00ffffff) | 0x80000000;
    default: return U32();
    }
}

struct Op {
    int kind;
    int a[24];
    unsigned c[2];
    float f;
};

struct Scenario {
    int dfmt, dw, dh, sfmt, sw, sh;
    std::vector<unsigned char> dpix, spix;
    std::vector<Op> ops;
} sc;

static Point RP(int w, int h) { return Point{R(-w / 2 - 2, w + w / 2 + 2), R(-h / 2 - 2, h + h / 2 + 2)}; }

static void Draw()
{
    sc.dfmt = drawFormats[R(0, 3)];
    sc.sfmt = drawFormats[R(0, 3)];
    sc.dw = R(1, 48);
    sc.dh = R(1, 48);
    sc.sw = R(1, 40);
    sc.sh = R(1, 40);
    sc.dpix.resize(size_t(sc.dh) * 0x200);
    sc.spix.resize(size_t(sc.sh) * 0x200);
    for (auto& b : sc.dpix) b = static_cast<unsigned char>(R(0, 2) ? U32() : (R(0, 1) ? 0 : 255));
    for (auto& b : sc.spix) b = static_cast<unsigned char>(R(0, 2) ? U32() : (R(0, 1) ? 0 : 255));
    sc.ops.clear();
    const int n = R(1, 12);
    for (int i = 0; i < n; ++i) {
        Op o{};
        o.kind = R(0, 33);
        for (int& v : o.a) v = R(-8, 60);
        o.c[0] = Colour_();
        o.c[1] = Colour_();
        o.f = std::uniform_real_distribution<float>(-0.3f, 1.3f)(rng);
        // points inside and around the destination
        const Point p1 = RP(sc.dw, sc.dh), p2 = RP(sc.dw, sc.dh), p3 = RP(sc.dw, sc.dh);
        o.a[0] = p1.x;
        o.a[1] = p1.y;
        o.a[2] = p2.x;
        o.a[3] = p2.y;
        o.a[4] = p3.x;
        o.a[5] = p3.y;
        sc.ops.push_back(o);
    }
}

// ---- running
static void DumpBm(const Api&, const char* tag, const Bm* b)
{
    std::string s = std::string(tag) + " " + Num(W(b)) + "x" + Num(H(b)) + " p" + Num(Pitch(b)) + " b" + Num(Bpp(b)) +
                    " f" + Num(Fmt(b)) + " n" + Num(NumBytes(b)) + " d" + Num(Data(b) != nullptr);
    Log(s);
    if (!Data(b) || W(b) <= 0 || H(b) <= 0) return;
    const size_t row = size_t(W(b)) * Bpp(b);
    for (int y = 0; y < H(b); ++y) Log(Hex(Data(b) + size_t(y) * Pitch(b), row));
}

static void FreeBm(const Api& a, Bm* b)
{
    if (!b) return;
    a.dtor(b, nullptr);
    a.allocDelete(b);
}

static void Run()
{
    const Api& a = apis[side];
    alignas(8) Bm dst[0x44], src[0x44];
    std::memset(dst, 0xcd, sizeof(dst));
    std::memset(src, 0xcd, sizeof(src));
    a.ctor(dst, nullptr);
    a.ctor(src, nullptr);
    const Size ds{sc.dw, sc.dh}, ss{sc.sw, sc.sh};
    a.init(dst, nullptr, &ds, 0, sc.dfmt, nullptr, 0);
    a.init(src, nullptr, &ss, 0, sc.sfmt, nullptr, 0);
    for (int y = 0; y < sc.dh && Data(dst); ++y) std::memcpy(Data(dst) + y * Pitch(dst), &sc.dpix[size_t(y) * 0x200], size_t(Pitch(dst)));
    for (int y = 0; y < sc.sh && Data(src); ++y) std::memcpy(Data(src) + y * Pitch(src), &sc.spix[size_t(y) * 0x200], size_t(Pitch(src)));
    DumpBm(a, "dst0", dst);
    Log("pitch " + Num(a.calcPitch(dst, nullptr, 37)));
    for (size_t i = 0; i < sc.ops.size(); ++i) {
        const Op& o = sc.ops[i];
        const Point p1{o.a[0], o.a[1]}, p2{o.a[2], o.a[3]}, p3{o.a[4], o.a[5]};
        // the drawing code trusts its clip rectangle to lie inside the bitmap (it writes outside it otherwise), so the
        // test's clips do. Image sources are read past their area when the destination is clipped on the left or top
        // (rows wrap; the last row reads past the buffer), so image positions start inside the clip
        const int cx = (o.a[6] & 0x7fff) % sc.dw, cy = (o.a[7] & 0x7fff) % sc.dh;
        const Rect clip{cx, cy, (o.a[8] & 0x7fff) % (sc.dw - cx + 1), (o.a[9] & 0x7fff) % (sc.dh - cy + 1)};
        const Rect full{0, 0, sc.dw, sc.dh};
        const Rect& cl = (o.a[10] & 1) ? clip : full;
        const Colour c{o.c[0]}, c2{o.c[1]};
        const Point ip{cl.x + std::abs(p1.x) % 30, cl.y + std::abs(p1.y) % 30}, ip2{cl.x + std::abs(p2.x) % 30, cl.y + std::abs(p2.y) % 30};
        const Point fp{std::abs(p1.x) % 30, std::abs(p1.y) % 30}, fp2{std::abs(p2.x) % 30, std::abs(p2.y) % 30};
        std::string what = "op " + Num(o.kind);
        if (const char* skip = getenv("KBD_SKIP")) {  // comma-separated op kinds left out (for bisecting)
            const std::string k = "," + std::string(skip) + ",";
            if (k.find("," + Num(o.kind) + ",") != std::string::npos) continue;
        }
        if (getenv("KBD_TRACE")) std::printf("d %dx%d f%d s %dx%d f%d side %d op %d (", sc.dw, sc.dh, sc.dfmt, sc.sw, sc.sh, sc.sfmt,
                                             side, o.kind);
        if (getenv("KBD_TRACE")) std::printf("%d %d %d %d %d %d | %d %d %d %d | %d %d %d %d) %08x\n", o.a[0], o.a[1],
                                             o.a[2], o.a[3], o.a[4], o.a[5], cl.x, cl.y, cl.w, cl.h, o.a[11], o.a[12], o.a[13], o.a[14], o.c[0]);
        switch (o.kind) {
        case 0:
            a.pixel(dst, &p1, c, &cl);
            break;
        case 1:
            a.blendPixel(dst, &p1, c, &cl);
            a.pixelT(dst, nullptr, &p2, c2);
            a.blendPixelT(dst, nullptr, &p3, c);
            break;
        case 2: {
            Colour r{0};
            a.getPixel(dst, nullptr, &r, &p1);
            what += " " + Num(r.argb);
            break;
        }
        case 3:
        case 4:
            a.line(dst, p1, p2, c, o.a[11] & 7, &cl);
            break;
        case 5:
            a.lineT(dst, nullptr, p1, p2, c, o.a[11] & 7);
            break;
        case 6:
            a.box(dst, Rect{p1.x, p1.y, o.a[12] % 30, o.a[13] % 30}, c, o.a[11] & 7, &cl);
            break;
        case 7:
            a.boxT(dst, nullptr, Rect{p1.x, p1.y, o.a[12] % 30, o.a[13] % 30}, c, o.a[11] & 7);
            break;
        case 8:
        case 9:
            a.circle(dst, p1, o.a[12] % 25, c, o.a[11] & 3, &cl);
            break;
        case 10:
            a.circleT(dst, nullptr, p1, o.a[12] % 25, c, o.a[11] & 3);
            break;
        case 11:
        case 12:
        case 13: {
            // filled: the original's scan table (alloca, height + 1 rows) overflows when the clip cuts the top vertex
            // away and the rows start at the clip's top instead; keeping y inside the clip with one vertex on its top
            // edge stays within the table
            const Rect& tc = o.kind == 13 ? full : cl;
            Point q[3] = {p1, p2, p3};
            const int mode = o.a[11] & 3;
            if (mode == 1 && tc.h > 0) {
                for (Point& v : q) v.y = std::min(std::max(v.y, tc.y), tc.y + tc.h - 1);
                q[o.a[12] % 3 < 0 ? 0 : o.a[12] % 3].y = tc.y;
            }
            // antialiased: TraceLineAlpha writes one row past the table for a flat bottom edge (and the clipped
            // case has the overflow above), so only triangles inside the clip without a flat bottom
            if (mode == 3) {
                if (tc.w <= 0 || tc.h <= 0) break;
                for (Point& v : q) v = Point{tc.x + std::abs(v.x) % tc.w, tc.y + std::abs(v.y) % tc.h};
                int ys[3] = {q[0].y, q[1].y, q[2].y};
                std::sort(ys, ys + 3);
                if (ys[1] == ys[2]) break;
            }
            if (o.kind == 13)
                a.triangleT(dst, nullptr, q[0], q[1], q[2], c, mode);
            else
                a.triangle(dst, q[0], q[1], q[2], c, mode, &cl);
            break;
        }
        case 14: {
            const Rect sa{(o.a[12] & 0x7fff) % 12, (o.a[13] & 0x7fff) % 12, o.a[14], o.a[15]};
            a.image(dst, src, &ip, &sa, 0, &cl);
            break;
        }
        case 15: {
            const Rect sa{(o.a[12] & 0x7fff) % 12, (o.a[13] & 0x7fff) % 12, o.a[14], o.a[15]};
            a.copyImage(dst, src, &ip2, &sa, 0, &cl);
            break;
        }
        case 16: {
            const Rect sa{(o.a[12] & 0x7fff) % 12, (o.a[13] & 0x7fff) % 12, 1 + (o.a[14] & 15), 1 + (o.a[15] & 15)};
            const Rect da{ip.x, ip.y, o.a[16], o.a[17]};
            if (sa.x + sa.w <= sc.sw && sa.y + sa.h <= sc.sh) a.imageTile(dst, src, &da, &sa, 0, &cl);
            break;
        }
        case 17:
            a.imageT2(dst, nullptr, src, &fp, 0);
            a.copyImageT2(dst, nullptr, src, &fp2, 0);
            break;
        case 18: {
            const Rect sa{(o.a[12] & 0x7fff) % 12, (o.a[13] & 0x7fff) % 12, o.a[14], o.a[15]};
            a.imageT(dst, nullptr, src, &fp, &sa, 0);
            a.copyImageT(dst, nullptr, src, &fp2, &sa, 0);
            break;
        }
        case 19: {
            const Rect ar{p1.x, p1.y, o.a[12], o.a[13]};
            a.stipple(dst, &ar, c, &cl);
            break;
        }
        case 20: {
            const Rect ar{p1.x, p1.y, o.a[12], o.a[13]};
            // an empty width with rows left reads 0 pixels per row, which the 16/24-bit primitives (dec / jne loops)
            // take as 2^32 in the original too
            const Rect& c2r = (o.a[14] & 3) == 0 || (o.a[14] & 3) == 3 ? cl : full;
            const int l = std::max(ar.x, c2r.x), r = std::min(ar.x + ar.w - 1, c2r.x + c2r.w - 1);
            const int t = std::max(ar.y, c2r.y), b = std::min(ar.y + ar.h - 1, c2r.y + c2r.h - 1);
            if (r - l + 1 <= 0 && b - t + 1 > 0) break;
            switch (o.a[14] & 3) {
            case 0: a.gray(dst, &ar, (o.a[15] & 1) != 0, &cl); break;
            case 1: a.grayT(dst, nullptr, &ar); break;
            case 2: a.grayTb(dst, nullptr, &ar, (o.a[15] & 1) != 0); break;
            default: a.grayTc(dst, nullptr, &ar, &cl); break;
            }
            break;
        }
        case 21: {
            // a walk that ends: along a row or a column of the destination
            Point from = p1, to = p2;
            if (o.a[12] & 1) to.y = from.y; else to.x = from.x;
            Colour r{0};
            const Point q{from.x, from.y};
            a.getPixel(dst, nullptr, &r, &q);
            what += " " + Num(a.lineScan(dst, &from, &to, (o.a[13] & 1) ? r.argb : c.argb));
            break;
        }
        case 22:
            if (o.a[12] & 1) a.clear(dst); else a.clearC(dst, c.argb);
            break;
        case 23: {
            // a convex polygon (a triangle either way round, or an axis-aligned quad) textured from src: convex, so
            // both edge walks cover every row
            Point pts[4] = {p1, p2, p3, p3};
            int n = 3;
            if (o.a[14] & 1) {
                const int x0 = std::min(p1.x, p2.x), x1 = std::max(p1.x, p2.x);
                const int y0 = std::min(p1.y, p2.y), y1 = std::max(p1.y, p2.y);
                pts[0] = Point{x0, y0};
                pts[1] = Point{x1, y0};
                pts[2] = Point{x1, y1};
                pts[3] = Point{x0, y1};
                n = 4;
            }
            // the right end of a span is drawn inclusively: x up to width - 2 keeps it inside the row
            // and a row is drawn at the left end's y rounded, which drifts by float error from the row tested against
            // the height: y up to height - 2 keeps it inside the bitmap
            for (Point& q : pts) q.x = std::min(q.x, sc.dw - 2), q.y = std::min(q.y, sc.dh - 2);
            Point tex[4];
            for (int k = 0; k < 4; ++k) tex[k] = Point{(o.a[15 + k] & 63), (o.a[19 + k] & 63)};
            int xm = 1, ym = 1;
            while (xm * 2 <= sc.sw) xm *= 2;
            while (ym * 2 <= sc.sh) ym *= 2;
            a.mapPolygon(dst, n, pts, src, tex, xm - 1, ym - 1, (o.a[23] & 1) != 0);
            break;
        }
        case 24:
        case 25: {
            Bm* r = nullptr;
            const int k = o.a[12] & 7;
            // pixels the original leaves unwritten: a 24-bit ScaleArbitrary that does not grow both ways, a Crop of an
            // area outside the bitmap (and a Crop's rows are packed, so its bytes are compared as one block)
            bool pixels = true, packed = false, rgbScaled = false;
            const Bm* from = dst;
            if (k == 0) r = a.copy(dst);
            else if (k == 1) r = a.scale(dst, 1 << (o.a[13] & 3), 1 << (o.a[14] & 3));
            else if (k == 2 || k == 3) {
                from = k == 2 ? dst : src;
                const int w = 1 + (o.a[13] & (k == 2 ? 63 : 15)), h = 1 + (o.a[14] & (k == 2 ? 63 : 15));
                r = a.scaleArb(from, w, h);
                pixels = Fmt(from) != RGB || (w > W(from) && h > H(from));
                // its 24-bit rows are written pitch / 3 * 3 bytes apart: compare the bytes written, as one block
                rgbScaled = pixels && Fmt(from) == RGB;
            } else if (k == 4) {
                const Rect ar{o.a[13] % 10, o.a[14] % 10, o.a[15] % 20, o.a[16] % 20};
                r = a.crop(dst, &ar);
                pixels = ar.x >= 0 && ar.y >= 0 && ar.w >= 0 && ar.h >= 0 && ar.x + ar.w <= sc.dw && ar.y + ar.h <= sc.dh;
                packed = true;
            } else if (k == 5) r = a.flipY(dst);
            else r = a.rotate(dst, nullptr, (o.a[13] % 9 - 4) * 90 + ((o.a[14] & 15) == 0 ? 45 : 0));
            if (r) {
                if (!pixels) {
                    Log("result " + Num(W(r)) + "x" + Num(H(r)) + " p" + Num(Pitch(r)) + " f" + Num(Fmt(r)) + " d" + Num(Data(r) != nullptr));
                } else if (rgbScaled) {
                    Log("result " + Num(W(r)) + "x" + Num(H(r)) + " p" + Num(Pitch(r)) + " f" + Num(Fmt(r)));
                    for (int y = 0; Data(r) && y < H(r); ++y) Log(Hex(Data(r) + size_t(y) * (Pitch(r) / 3 * 3), size_t(W(r)) * 3));
                } else if (packed) {
                    Log("result " + Num(W(r)) + "x" + Num(H(r)) + " p" + Num(Pitch(r)) + " f" + Num(Fmt(r)));
                    if (Data(r) && W(r) > 0 && H(r) > 0) Log(Hex(Data(r), size_t(W(r)) * Bpp(r) * H(r)));
                } else {
                    DumpBm(a, "result", r);
                }
                FreeBm(a, r);
            } else {
                what += " null";
            }
            break;
        }
        case 26:
            what += " " + Num(a.transparency(dst, o.f));
            break;
        case 27:
            a.colorize(dst, c);
            break;
        case 28: {
            Colour r{0};
            a.getPixel(dst, nullptr, &r, &p1);
            a.colorReplace(dst, c, r);
            break;
        }
        case 29:
            a.colorFill(dst, c);
            break;
        case 30: {
            // Init over external pixels, an area copy, DetachAlias
            alignas(8) Bm ext[0x44];
            a.ctor(ext, nullptr);
            std::vector<unsigned char> mem(size_t(sc.dh) * 0x200 + 64);
            std::memcpy(mem.data(), sc.dpix.data(), std::min(mem.size(), sc.dpix.size()));
            const int pitch = (o.a[12] & 1) ? 0 : 0x100;
            a.init(ext, nullptr, &ds, pitch, sc.dfmt, mem.data(), (o.a[13] & 1) ? 0 : sc.dh * 0x100);
            DumpBm(a, "ext", ext);
            a.detach(ext);
            Log("detached " + Num(Data(ext) == mem.data()));
            if (Data(ext)) a.pixel(ext, &p1, c, &full);
            DumpBm(a, "ext2", ext);
            alignas(8) Bm part[0x44];
            const Rect ar{0, 0, sc.dw > 2 ? sc.dw - 2 : 1, sc.dh > 1 ? sc.dh - 1 : 1};
            a.ctorArea(part, nullptr, ext, &ar);
            DumpBm(a, "part", part);
            a.dtor(part, nullptr);
            a.dtor(ext, nullptr);
            break;
        }
        case 31: {
            Rect r{};
            a.toRect(dst, nullptr, &r);
            what += " " + Num(r.x) + " " + Num(r.y) + " " + Num(r.w) + " " + Num(r.h);
            alignas(8) Bm s2[0x44];
            a.ctorSize(s2, nullptr, &ss, sc.sfmt, nullptr);
            Log("s2 " + Num(W(s2)) + "x" + Num(H(s2)) + " p" + Num(Pitch(s2)) + " f" + Num(Fmt(s2)) + " n" + Num(NumBytes(s2)) +
                " d" + Num(Data(s2) != nullptr));  // its pixels are not set
            a.dtor(s2, nullptr);
            break;
        }
        case 32: {
            // a bigger triangle fully inside (the table path that needs no clipping)
            const Point q1{std::abs(o.a[12]) % sc.dw, std::abs(o.a[13]) % sc.dh}, q2{std::abs(o.a[14]) % sc.dw, std::abs(o.a[15]) % sc.dh},
                q3{std::abs(o.a[16]) % sc.dw, std::abs(o.a[17]) % sc.dh};
            int ys[3] = {q1.y, q2.y, q3.y};
            std::sort(ys, ys + 3);
            if ((o.a[11] & 1) && ys[1] == ys[2]) break;  // see op 11
            a.triangle(dst, q1, q2, q3, c, 1 + 2 * (o.a[11] & 1), &full);
            break;
        }
        default:
            // circles and lines fully inside
            a.circle(dst, Point{sc.dw / 2, sc.dh / 2}, (sc.dw < sc.dh ? sc.dw : sc.dh) / 2, c, o.a[11] & 3, &full);
            a.line(dst, Point{0, 0}, Point{sc.dw - 1, sc.dh - 1}, c2, o.a[12] & 6, &full);
            break;
        }
        Log(what);
        if (getenv("KBD_HEAPCHECK")) {  // which operation corrupts a heap
            HANDLE heaps[64];
            const DWORD nh = GetProcessHeaps(64, heaps);
            for (DWORD k = 0; k < nh && k < 64; ++k)
                if (!HeapValidate(heaps[k], 0, nullptr)) {
                    std::printf("heap %lu invalid after side %d op %d\n", k, side, o.kind);
                    std::exit(3);
                }
        }
        DumpBm(a, "dst", dst);
    }
    a.dtor(dst, nullptr);
    a.dtor(src, nullptr);
}

static LONG WINAPI Crash(EXCEPTION_POINTERS* e)
{
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(e->ExceptionRecord->ExceptionAddress), &m);
    if (m) GetModuleFileNameA(m, name, MAX_PATH);
    std::printf("crash %08lx at %p (%s +%lx) side %d\n", e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress, name,
                static_cast<unsigned long>(static_cast<const char*>(e->ExceptionRecord->ExceptionAddress) - reinterpret_cast<const char*>(m)),
                side);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(&Crash);
    if (argc < 2) {
        std::printf("usage: diff_ks_bitmap_draw <dir> [scenarios] [seed]\n");
        return 2;
    }
    const std::string dir = argv[1];
    const int scenarios = argc > 2 ? std::atoi(argv[2]) : 3000;
    rng.seed(argc > 3 ? unsigned(std::atoi(argv[3])) : 12345u);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetDllDirectoryA(dir.c_str());
    HMODULE orig = LoadLibraryA((dir + "\\KernelStandard_7c_orig.dll").c_str());
    HMODULE ours = LoadLibraryA((dir + "\\KernelStandard_7c.dll").c_str());
    if (!orig || !ours) {
        std::printf("cannot load the modules from %s\n", dir.c_str());
        return 2;
    }
    const bool swap = getenv("KBD_SWAP") != nullptr;
    Load(apis[0], swap ? orig : ours, orig);
    Load(apis[1], swap ? ours : orig, orig);
    unsigned long long checks = 0, bad = 0;
    for (int n = 0; n < scenarios; ++n) {
        Draw();
        for (side = 0; side < 2; ++side) Run();
        const size_t lines = logs[0].size() > logs[1].size() ? logs[0].size() : logs[1].size();
        for (size_t i = 0; i < lines; ++i) {
            ++checks;
            const std::string x = i < logs[0].size() ? logs[0][i] : "(end)";
            const std::string y = i < logs[1].size() ? logs[1][i] : "(end)";
            if (x != y) {
                if (++bad <= 8) {
                    std::string ctx;
                    for (size_t k = i; k-- > 0;)
                        if (logs[0][k].rfind("op ", 0) == 0) {
                            ctx = logs[0][k];
                            break;
                        }
                    size_t at = 0;
                    while (at < x.size() && at < y.size() && x[at] == y[at]) ++at;
                    const size_t from = at > 40 ? at - 40 : 0;
                    std::printf("scenario %d (d f%d %dx%d s f%d %dx%d) line %zu after '%s', column %zu:\n  ours %s\n  orig %s\n", n,
                                sc.dfmt, sc.dw, sc.dh, sc.sfmt, sc.sw, sc.sh, i, ctx.c_str(), at, x.substr(from, 160).c_str(),
                                y.substr(from, 160).c_str());
                }
                break;
            }
        }
        if (getenv("KBD_DUMP") && (!getenv("KBD_ONLY") || std::atoi(getenv("KBD_ONLY")) == n))
            for (const std::string& l : logs[0]) std::printf("%d| %s\n", n, l.substr(0, 300).c_str());
        logs[0].clear();
        logs[1].clear();
    }
    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}
