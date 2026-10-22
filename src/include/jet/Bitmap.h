// Jet::Bitmap (KernelStandard_7c unit U8, Bitmap.cpp / BitmapIO.cpp). 0x44 bytes in TS12, the Jet SDK layout:
// {Size, palette, data, pitch, bytes per pixel, format, owned, 8 pixel primitives, data size}. The primitives are
// plain cdecl functions picked by Init for the format; callers outside the class never call them.
// Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U8").
#pragma once

#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/SmallTypes.h>

namespace Jet {

class IOSlave;
class MemoryFile;

class Bitmap {
public:
    // INDEX: 8-bit with a palette; RGB: 24-bit B,G,R; ARGB / XRGB: 32-bit 0xAARRGGBB; RGB565: 16-bit
    enum Format { INDEX, RGB, ARGB, XRGB, RGB565, UNSPECIFIED, FORMAT_ARGB32 = ARGB };

    Bitmap();
    Bitmap(const Size& size, Format format, unsigned char* data);  // data null: allocated
    Bitmap(const Bitmap& src, const Rect& area);                    // a copy of `area` of src
    ~Bitmap();

    // pitch 0: width * bytes per pixel rounded up to 4; numBytes 0: pitch * height. Allocates when data is null
    void Init(const Size& size, int pitch, Format format, unsigned char* data, int numBytes);
    void __cdecl Done();
    void __cdecl DetachAlias() const;  // copies borrowed pixels into an owned buffer

    bool IsValid() const;
    Format GetFormat() const;
    unsigned long GetBytesPerPixel() const;
    unsigned long GetPitch() const;
    unsigned long GetNumBytes() const;
    unsigned char* GetPtr() const;
    unsigned char* GetPtr(const Point& at) const;
    PackedColor* GetPalette() const;
    operator Rect() const;
    unsigned int CalcPitch(unsigned int width) const;

    bool __cdecl Load(const PString& fileName, const PString& basePath, bool keepFormat);
    bool __cdecl AddAlpha(const char* fileName);  // an 8-bit BMP of the same size into the alpha bytes
    bool __cdecl LoadAlpha(const char* fileName, const char* alphaFileName);
    bool __cdecl Save(const char* fileName) const;
    bool __cdecl Save(IOSlave* slave, const char* fileName) const;
    static bool __cdecl GetInfo(const char* fileName, Format& format, unsigned long& width, unsigned long& height);

    void __cdecl Clear();
    void __cdecl Clear(unsigned long colour);
    void Pixel(const Point& at, PackedColor colour);
    void __cdecl Pixel(const Point& at, PackedColor colour, const Rect& clip);
    PackedColor Pixel(const Point& at) const;
    void BlendPixel(const Point& at, PackedColor colour);
    void __cdecl BlendPixel(const Point& at, PackedColor colour, const Rect& clip);
    void Line(Point a, Point b, PackedColor colour, int fillMode);
    void __cdecl Line(Point a, Point b, PackedColor colour, int fillMode, const Rect& clip);
    void Box(Rect area, PackedColor colour, int fillMode);
    void __cdecl Box(Rect area, PackedColor colour, int fillMode, const Rect& clip);
    void Circle(Point centre, int radius, PackedColor colour, int fillMode);
    void __cdecl Circle(Point centre, int radius, PackedColor colour, int fillMode, const Rect& clip);
    void Triangle(Point a, Point b, Point c, PackedColor colour, int fillMode);
    void __cdecl Triangle(Point a, Point b, Point c, PackedColor colour, int fillMode, const Rect& clip);
    void Image(const Bitmap& src, const Point& at, int flags);
    void Image(const Bitmap& src, const Point& at, const Rect& srcArea, int flags);
    void __cdecl Image(const Bitmap& src, const Point& at, const Rect& srcArea, int flags, const Rect& clip);
    void __cdecl Image(const Bitmap& src, const Rect& destArea, const Rect& srcArea, int flags, const Rect& clip);
    void CopyImage(const Bitmap& src, const Point& at, int flags);
    void CopyImage(const Bitmap& src, const Point& at, const Rect& srcArea, int flags);
    void __cdecl CopyImage(const Bitmap& src, const Point& at, const Rect& srcArea, int flags, const Rect& clip);
    void __cdecl Stipple(const Rect& area, PackedColor colour, const Rect& clip);
    void GrayScale(const Rect& area);
    void GrayScale(const Rect& area, bool stipple);
    void GrayScale(const Rect& area, const Rect& clip);
    void __cdecl GrayScale(const Rect& area, bool stipple, const Rect& clip);
    int __cdecl LineScan(const Point& from, const Point& to, unsigned long colour);
    void __cdecl MapPolygon(int count, const Point* points, const Bitmap& texture, const Point* texturePoints,
                            int textureXMask, int textureYMask, bool blend);
    Bitmap* __cdecl Copy() const;
    Bitmap* __cdecl Scale(int width, int height) const;           // powers of two, down only
    Bitmap* __cdecl ScaleArbitrary(int width, int height) const;  // bilinear up, nearest down
    Bitmap* __cdecl Crop(const Rect& area) const;
    Bitmap* __cdecl FlipY() const;
    Bitmap* RotateClockwise(int degrees);
    bool __cdecl SetTransparency(float opacity);
    void __cdecl Colorize(PackedColor colour);
    void __cdecl ColorReplace(PackedColor to, PackedColor from);
    void __cdecl ColorFill(PackedColor colour);

    // a pixel primitive set; dest and src are pixel addresses, count pixels, pitch the byte step (0: contiguous)
    using GetFn = void(__cdecl*)(void* dest, const void* src, int count);
    using SetFn = void(__cdecl*)(void* dest, unsigned int colour, int count, int pitch);
    using BlitterFunc = void(__cdecl*)(void* dest, const void* src, int count);
    struct ScanLine {
        int edge[2];
    };
    struct ScanTable {
        int startLine, lines;
        ScanLine* scanLines;
    };

protected:
    void __cdecl DrawCirclePoints(Point centre, int x, int y, PackedColor colour, Rect clip);
    void __cdecl AntialiasedLine(Point a, Point b, PackedColor colour, Rect clip);
    void __cdecl ClipLine(Point& a, Point& b, Rect clip);
    void __cdecl Sort(Point& a, Point& b, Point& c);
    void __cdecl TraceLine(Point a, Point b, ScanLine* table, bool side);
    void __cdecl TraceLineAlpha(Point a, Point b, PackedColor colour, ScanLine* table, bool side);
    void __cdecl DrawHLine(int x1, int x2, int y, PackedColor colour, Rect clip);
    void __cdecl DrawCircleLines(Point centre, int x, int y, PackedColor colour, bool& newLine, Rect clip);
    void __cdecl Fill(const ScanTable* table, PackedColor colour);
    BlitterFunc GetBlitter(Format sourceFormat, bool blend);
    unsigned char R16(unsigned long c) const;
    unsigned char G16(unsigned long c) const;
    unsigned char B16(unsigned long c) const;
    unsigned char R15(unsigned long c) const;
    unsigned char G15(unsigned long c) const;
    unsigned char B15(unsigned long c) const;
    bool LoadJPG(MemoryFile& file);
    bool LoadTGA(MemoryFile& file, bool keepPalette);
    bool LoadBMP(MemoryFile& file, bool keepPalette);
    bool LoadPCX(MemoryFile& file, bool keepPalette);
    bool SaveJPG(IOSlave* slave) const;
    bool SaveTGA(IOSlave* slave) const;
    bool SaveBMP(IOSlave* slave, bool keepPalette) const;
    void Load32(MemoryFile& file);
    void Load24(MemoryFile& file);
    void Load16(MemoryFile& file);
    void Load15(MemoryFile& file);
    void BMP8(MemoryFile& file, bool keepPalette, unsigned int colours);
    void TGA16(MemoryFile& file);
    void TGA8(MemoryFile& file, bool keepPalette);
    static bool __cdecl GetInfoTGA(const char* fileName, Format& format, unsigned long& width, unsigned long& height);
    static bool __cdecl GetInfoBMP(const char* fileName, Format& format, unsigned long& width, unsigned long& height);
    static bool __cdecl GetInfoPCX(const char* fileName, Format& format, unsigned long& width, unsigned long& height);
    static bool __cdecl GetInfoJPG(const char* fileName, Format& format, unsigned long& width, unsigned long& height);

public:
    int width;   // +0x00
    int height;  // +0x04 (the loaders keep a negative height for top-down rows while they read)

protected:
    PackedColor* palette;          // +0x08 256 entries (INDEX)
    unsigned char* data;           // +0x0c
    int pitch;                     // +0x10
    int bytesPerPixel;             // +0x14
    Format format;                 // +0x18
    bool alloc;                    // +0x1c data is owned
    GetFn getPixel;                // +0x20 to 32-bit pixels
    SetFn setPixel;                // +0x24
    SetFn blendPixel;              // +0x28
    BlitterFunc blitSelf;          // +0x2c same format
    BlitterFunc blitRGB565;        // +0x30 from 16-bit
    BlitterFunc blitRGB888;        // +0x34 from 24-bit
    BlitterFunc blitARGB888;       // +0x38 from 32-bit
    BlitterFunc blendARGB888;      // +0x3c from 32-bit, blending by its alpha

public:
    unsigned int dataSize;  // +0x40 bytes of pixel data

private:
    Bitmap(const Bitmap&);           // the original's does nothing
    void operator=(const Bitmap&);   // does nothing

    friend struct BitmapLoader;
    void InitInline(int w, int h, Format f);  // the inlined Init of the loaders (no 16 MB limit)
};
JET_LAYOUT32(sizeof(Bitmap) == 0x44);

}  // namespace Jet
