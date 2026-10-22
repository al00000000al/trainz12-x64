// Unit U8 of KernelStandard_7c: Bitmap's file formats (0x10008650..0x1000b6a0): TGA, BMP, PCX and JPEG info and
// loading, the 8-bit alpha files, saving TGA / BMP / JPEG. Files are read through the original's File and MemoryFile
// (U10, not ours yet). JPEG decoding goes through Intel's ijl10.dll as in the original (32-bit only; libjpeg-turbo
// takes its place in the x64 build), encoding through libjpeg-turbo in place of the original's static IJG
// encoder, with the same settings (quality 100: every quantiser 1). Notes: notes/KernelStandard.md ("U8").
#include <jet/Bitmap.h>
#include <jet/KernelStandard.h>

#include <windows.h>

#include <csetjmp>
#include <cstdio>
#include <cstring>
#include <vector>

#include <jpeglib.h>

namespace Jet {

namespace {

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;

// the reads the loaders inline: a block only if all of it is there
bool Take(MemoryFile& f, void* dst, size_t n)
{
    if (f.cur + n > f.start + f.size) return false;
    std::memcpy(dst, f.cur, n);
    f.cur += n;
    return true;
}
bool TakeByte(MemoryFile& f, u8& b)
{
    if (f.cur + 1 > f.start + f.size) return false;
    b = u8(*f.cur++);
    return true;
}

int Abs(int v) { return v < 0 ? -v : v; }

#pragma pack(push, 1)
struct TgaHeader {
    u8 idLength, colourMapType, imageType;
    u16 mapFirst, mapLength;
    u8 mapEntrySize;
    u16 xOrigin, yOrigin, width, height;
    u8 bits, descriptor;
};
struct BmpFileHeader {
    u16 type;
    u32 size;
    u16 reserved1, reserved2;
    u32 offBits;
};
struct BmpInfoHeader {
    u32 size;
    int width, height;
    u16 planes, bitCount;
    u32 compression, sizeImage;
    int xPels, yPels;
    u32 clrUsed, clrImportant;
};
struct PcxHeader {
    u8 manufacturer, version, encoding, bitsPerPixel;
    short xMin, yMin, xMax, yMax;
    u8 rest[0x74];
};
#pragma pack(pop)
static_assert(sizeof(TgaHeader) == 0x12 && sizeof(BmpFileHeader) == 0xe && sizeof(BmpInfoHeader) == 0x28 &&
              sizeof(PcxHeader) == 0x80);

// ---- Intel JPEG Library 1.0 (ijl10.dll: ijlInit, ijlFree, ijlRead by ordinal), JPEG_PROPERTIES by offset
struct Ijl {
    using Fn = int(__stdcall*)(void*);
    using ReadFn = int(__stdcall*)(void*, int);
    Fn init = nullptr, free = nullptr;
    ReadFn read = nullptr;
    Ijl()
    {
        if (const HMODULE m = LoadLibraryA("ijl10.dll")) {
            init = reinterpret_cast<Fn>(GetProcAddress(m, MAKEINTRESOURCEA(2)));
            free = reinterpret_cast<Fn>(GetProcAddress(m, MAKEINTRESOURCEA(3)));
            read = reinterpret_cast<ReadFn>(GetProcAddress(m, MAKEINTRESOURCEA(4)));
        }
    }
    bool Ok() const { return init && free && read; }
};
const Ijl& IjlLib()
{
    static const Ijl lib;
    return lib;
}

// JPEG_PROPERTIES (0x4e3c bytes in IJL 1.0): DIBBytes +4, DIBWidth +8, DIBHeight +0xc, DIBChannels +0x14,
// DIBColor +0x18, JPGBytes +0x24, JPGSizeBytes +0x28, JPGWidth +0x2c, JPGHeight +0x30
struct alignas(8) IjlProps {
    u8 raw[0x5000];
    template <class T>
    T& At(size_t off) { return *reinterpret_cast<T*>(raw + off); }
};
enum { IJL_JBUFF_READPARAMS = 1, IJL_JBUFF_READWHOLEIMAGE = 3, IJL_RGBA_FPX = 5 };

// ---- libjpeg-turbo destination over an IOSlave, a 4 KB buffer as in the original (0x1000b0a0..0x1000b0f0)
struct SlaveDest {
    jpeg_destination_mgr mgr;
    IOSlave* slave;
    JOCTET buffer[0x1000];
};
void InitDest(j_compress_ptr c)
{
    SlaveDest* d = reinterpret_cast<SlaveDest*>(c->dest);
    d->mgr.next_output_byte = d->buffer;
    d->mgr.free_in_buffer = sizeof(d->buffer);
}
boolean EmptyDest(j_compress_ptr c)
{
    SlaveDest* d = reinterpret_cast<SlaveDest*>(c->dest);
    d->slave->WriteData(d->buffer, sizeof(d->buffer));
    d->mgr.next_output_byte = d->buffer;
    d->mgr.free_in_buffer = sizeof(d->buffer);
    return TRUE;
}
void TermDest(j_compress_ptr c)
{
    SlaveDest* d = reinterpret_cast<SlaveDest*>(c->dest);
    d->slave->WriteData(d->buffer, sizeof(d->buffer) - d->mgr.free_in_buffer);
}
struct JumpError {
    jpeg_error_mgr mgr;
    jmp_buf jump;
};
void ErrorExit(j_common_ptr c)  // 0x1000b120: output_message, then back to SaveJPG
{
    JumpError* e = reinterpret_cast<JumpError*>(c->err);
    e->mgr.output_message(c);
    longjmp(e->jump, 1);
}

}  // namespace

// ---- file information

bool __cdecl Bitmap::GetInfoTGA(const char* name, Format& f, unsigned long& w, unsigned long& h)
{
    File file;
    if (file.Open(name, File::READ, 0).code) return false;
    TgaHeader hd{};
    file.Read(&hd, sizeof(hd), 1);
    if (hd.imageType == 1)
        f = INDEX;
    else if (hd.imageType == 2)
        f = hd.bits != 32 ? XRGB : ARGB;
    else
        f = UNSPECIFIED;
    w = hd.width;
    h = hd.height;
    return true;
}

bool __cdecl Bitmap::GetInfoBMP(const char* name, Format& f, unsigned long& w, unsigned long& h)
{
    File file;
    if (file.Open(name, File::READ, 0).code) return false;
    BmpFileHeader fh{};
    file.Read(&fh, sizeof(fh), 1);
    if (fh.type != 0x4d42) return false;
    BmpInfoHeader ih{};
    file.Read(&ih, sizeof(ih), 1);
    if (ih.size != 0x28) return false;
    f = XRGB;
    w = u32(ih.width);
    h = u32(ih.height);
    return true;
}

bool __cdecl Bitmap::GetInfoPCX(const char* name, Format& f, unsigned long& w, unsigned long& h)
{
    File file;
    if (file.Open(name, File::READ, 0).code) return false;
    PcxHeader hd{};
    file.Read(&hd, sizeof(hd), 1);
    if (hd.version != 5) return false;
    f = XRGB;
    w = u32(hd.xMax - hd.xMin);
    h = u32(hd.yMax - hd.yMin);
    return true;
}

bool __cdecl Bitmap::GetInfoJPG(const char* name, Format& f, unsigned long& w, unsigned long& h)
{
    File file;
    if (file.Open(name, File::READ, 0).code) return false;
    const Ijl& ijl = IjlLib();
    if (!ijl.Ok()) return false;
    IjlProps props{};
    if (ijl.init(&props)) return false;
    MemoryFile mf(file, false, 0);
    props.At<const char*>(0x24) = mf.start;
    props.At<u32>(0x28) = mf.size;
    if (ijl.read(&props, IJL_JBUFF_READPARAMS)) {
        ijl.free(&props);
        return false;
    }
    if (ijl.free(&props)) return false;
    f = XRGB;
    w = props.At<u32>(0x2c);
    h = props.At<u32>(0x30);
    return true;
}

bool __cdecl Bitmap::GetInfo(const char* name, Format& f, unsigned long& w, unsigned long& h)
{
    const char* ext = std::strrchr(name, '.');
    if (!ext) return false;
    ++ext;
    if (!_strnicmp(ext, "tga", 3)) return GetInfoTGA(name, f, w, h);
    if (!_strnicmp(ext, "bmp", 3)) return GetInfoBMP(name, f, w, h);
    if (!_strnicmp(ext, "pcx", 3)) return GetInfoPCX(name, f, w, h);
    if (!_strnicmp(ext, "jpg", 3) || !_strnicmp(ext, "jpeg", 4)) return GetInfoJPG(name, f, w, h);
    return false;
}

// ---- loading

bool __cdecl Bitmap::Load(const PString& fileName, const PString& basePath, bool keepFormat)
{
    Done();
    const char* name = fileName.Text();
    if (!name) return false;
    const char* ext = std::strrchr(name, '.');
    if (!ext) {
        Done();
        return false;
    }
    File file;
    const char* base = basePath.Text();
    if (base) {
        if (file.Open(basePath, name, File::READ, 0).code) return false;
    } else {
        if (file.Open(name, File::READ, 0).code) return false;
    }
    bool ok = false;
    {
        MemoryFile mf(file, false, 0);
        ++ext;
        if (!_strnicmp(ext, "tga", 3))
            ok = LoadTGA(mf, keepFormat);
        else if (!_strnicmp(ext, "bmp", 3))
            ok = LoadBMP(mf, keepFormat);
        else if (!_strnicmp(ext, "pcx", 3))
            ok = LoadPCX(mf, keepFormat);
        else if (!_strnicmp(ext, "jpg", 3) || !_strnicmp(ext, "jpeg", 4))
            ok = LoadJPG(mf);
    }
    if (ok) return true;
    Done();
    return false;
}

bool __cdecl Bitmap::LoadAlpha(const char* fileName, const char* alphaFileName)
{
    const PString name(fileName);
    const PString none;
    if (!Load(name, none, false)) return false;
    return AddAlpha(alphaFileName);
}

// the alpha bytes from an 8-bit BMP of the same size (palette indices taken as alpha). Rows are found by width * 4
bool __cdecl Bitmap::AddAlpha(const char* fileName)
{
    MemoryFile mf;
    if (format != ARGB && format != XRGB) return false;
    if (mf.Open(fileName, File::READ, false, 0).code) return false;
    BmpFileHeader fh{};
    Take(mf, &fh, sizeof(fh));
    if (fh.type != 0x4d42) return false;
    BmpInfoHeader ih{};
    Take(mf, &ih, sizeof(ih));
    if (ih.size != 0x28 || ih.planes != 1 || ih.compression || (ih.width & 3) || ih.width != width ||
        Abs(ih.height) != height)
        return false;
    if (ih.bitCount != 8) return false;
    mf.SeekSet(int(fh.offBits));
    u8* p = data;
    if (height < 1) {
        height = -height;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x, p += 4) {
                u8 b = 0;
                mf.Read(&b, 1, 1);
                p[3] = b;
            }
    } else {
        p += (height - 1) * width * 4;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x, p += 4) p[3] = u8(mf.Readsbyte());
            p -= width * 8;
        }
    }
    mf.Close();
    return true;
}

bool Bitmap::LoadTGA(MemoryFile& f, bool keepPalette)
{
    TgaHeader hd{};
    Take(f, &hd, sizeof(hd));
    if (hd.idLength) {
        f.cur += hd.idLength;
        if (f.cur < f.start) f.cur = f.start;
    }
    if (hd.descriptor & 0x10) return false;
    const bool topDown = (hd.descriptor & 0x20) != 0;
    if (hd.imageType == 1) {
        if (hd.colourMapType != 1 || hd.mapEntrySize != 24 || hd.bits != 8) return false;
        if (keepPalette) {
            Init(Size{hd.width, hd.height}, 0, INDEX, nullptr, 0);
            if (topDown) height = -height;
            TGA8(f, true);
            f.Close();
            return true;
        }
        InitInline(hd.width, hd.height, XRGB);
        if (topDown) height = -height;
        TGA8(f, false);
    } else if (hd.imageType == 2 && hd.colourMapType == 0) {
        if (hd.bits == 16) {
            InitInline(hd.width, hd.height, XRGB);
            if (topDown) height = -height;
            Load15(f);
        } else if (hd.bits == 24) {
            InitInline(hd.width, hd.height, XRGB);
            if (topDown) height = -height;
            Load24(f);
        } else if (hd.bits == 32) {
            InitInline(hd.width, hd.height, ARGB);
            if (topDown) height = -height;
            Load32(f);
        } else {
            return false;
        }
    } else {
        return false;
    }
    f.Close();
    return true;
}

bool Bitmap::LoadBMP(MemoryFile& f, bool keepPalette)
{
    BmpFileHeader fh{};
    Take(f, &fh, sizeof(fh));
    if (fh.type != 0x4d42) return false;
    BmpInfoHeader ih{};
    Take(f, &ih, sizeof(ih));
    if (ih.size != 0x28 || ih.planes != 1 || ih.compression || (ih.width & 3)) return false;
    height = ih.height;
    const int h = Abs(ih.height);
    if (ih.bitCount == 8) {
        if (keepPalette) {
            Init(Size{ih.width, h}, 0, INDEX, nullptr, 0);
            BMP8(f, true, ih.clrUsed);
            f.Close();
            return true;
        }
        InitInline(ih.width, h, XRGB);
        BMP8(f, false, ih.clrUsed);
    } else if (ih.bitCount == 24) {
        InitInline(ih.width, h, XRGB);
        Load24(f);
    } else if (ih.bitCount == 32) {
        InitInline(ih.width, h, ARGB);
        Load32(f);
    } else {
        return false;
    }
    f.Close();
    return true;
}

// 8-bit RLE PCX with the 256-colour palette at the end. Without keepPalette the run length is the whole byte
// (192..255), as in the original
bool Bitmap::LoadPCX(MemoryFile& f, bool keepPalette)
{
    PcxHeader hd{};
    Take(f, &hd, sizeof(hd));
    if (hd.version != 5 || hd.encoding != 1 || hd.bitsPerPixel != 8) return false;
    const int h = hd.yMax - hd.yMin + 1;
    const int w = hd.xMax - hd.xMin + 1;
    const char* end = f.start + f.size;
    f.cur = end - 0x301;
    if (reinterpret_cast<uintptr_t>(f.cur) < reinterpret_cast<uintptr_t>(f.start)) f.cur = f.start;
    u8 value = 0;  // the byte after a run code (and first the palette marker)
    f.Read(&value, 1, 1);
    if (value != 0x0c) return false;
    u8 pal[0x300] = {};
    f.Read(pal, sizeof(pal), 1);
    f.cur = f.start + 0x80;
    if (reinterpret_cast<uintptr_t>(f.cur) < reinterpret_cast<uintptr_t>(f.start)) f.cur = f.start;
    u8 code = 0;
    int i = 0;
    if (keepPalette) {
        Init(Size{w, h}, 0, INDEX, nullptr, 0);
        if (!palette) palette = static_cast<PackedColor*>(Alloc::New(0x400));
        u8* pp = reinterpret_cast<u8*>(palette);
        for (int k = 0; k < 256; ++k) {
            pp[k * 4] = pal[k * 3 + 2];
            pp[k * 4 + 1] = pal[k * 3 + 1];
            pp[k * 4 + 2] = pal[k * 3];
            pp[k * 4 + 3] = 0;
        }
        const int n = w * h;
        while (i < n) {
            TakeByte(f, code);
            if (code < 0xc0) {
                data[i++] = code;
            } else {
                int run = code - 0xc0;
                TakeByte(f, value);
                for (; run > 0; --run) data[i++] = value;
            }
        }
    } else {
        InitInline(w, h, XRGB);
        const int n = h * w;
        u32* px = reinterpret_cast<u32*>(data);
        while (i < n) {
            TakeByte(f, code);
            if (code < 0xc0) {
                const u8* c = pal + code * 3;
                px[i++] = 0xff000000u | u32(c[2]) << 16 | u32(c[1]) << 8 | c[0];
            } else {
                int run = code;
                f.Read(&value, 1, 1);
                const u8* c = pal + value * 3;
                const u32 v = 0xff000000u | u32(c[2]) << 16 | u32(c[1]) << 8 | c[0];
                for (; run > 0; --run) px[i++] = v;
            }
        }
    }
    f.Close();
    return true;
}

bool Bitmap::LoadJPG(MemoryFile& f)
{
    const Ijl& ijl = IjlLib();
    if (!ijl.Ok()) return false;
    IjlProps props{};
    if (ijl.init(&props)) return false;
    props.At<const char*>(0x24) = f.start;
    props.At<u32>(0x28) = f.size;
    if (ijl.read(&props, IJL_JBUFF_READPARAMS)) {
        ijl.free(&props);
        return false;
    }
    const int w = props.At<int>(0x2c), h = props.At<int>(0x30);
    InitInline(w, h, XRGB);
    props.At<int>(0x08) = w;
    props.At<int>(0x0c) = h;
    props.At<u32>(0x18) = IJL_RGBA_FPX;
    props.At<u32>(0x14) = 4;
    props.At<u8*>(0x04) = data;
    bool ok = true;
    if (ijl.read(&props, IJL_JBUFF_READWHOLEIMAGE)) {
        ok = false;
    } else {
        // R, G, B, A in memory to 0xffRRGGBB
        u32* p = reinterpret_cast<u32*>(data);
        for (u32 n = u32(props.At<int>(0x0c) * props.At<int>(0x08)); n; --n, ++p) {
            const u32 v = *p;
            *p = (v | 0xffffff00u) << 16 | ((v >> 16) & 0xff) | (v & 0xff00);
        }
    }
    if (ijl.free(&props)) ok = false;
    return ok;
}

// rows of width * 4 bytes; a row is copied only when all of it is there
void Bitmap::Load32(MemoryFile& f)
{
    const int h = height;
    if (h < 1) {
        height = -h;
        for (int i = 0; i < height; ++i) Take(f, data + pitch * i, size_t(width) * 4);
        return;
    }
    int i = 0;
    do {
        Take(f, data + pitch * (height - i - 1), size_t(width) * 4);
    } while (++i < height);
}

void Bitmap::Load24(MemoryFile& f)
{
    auto pixel = [&]() {
        u8 b = 0, g = 0, r = 0;
        TakeByte(f, b);
        TakeByte(f, g);
        TakeByte(f, r);
        return 0xff000000u | u32(r) << 16 | u32(g) << 8 | b;
    };
    if (height < 1) {
        height = -height;
        for (int y = 0; y < height; ++y) {
            u32* p = reinterpret_cast<u32*>(data + pitch * y);
            for (int x = 0; x < width; ++x) *p++ = pixel();
        }
        return;
    }
    int y = 0;
    do {
        u32* p = reinterpret_cast<u32*>(data + pitch * (height - y - 1));
        for (int x = 0; x < width; ++x) *p++ = pixel();
    } while (++y < height);
}

// 16-bit rows through a row buffer (a short row keeps the previous one, as the original's alloca buffer did)
namespace {
template <class Convert>
void Load16Bit(MemoryFile& f, int width, int& height, int pitch, u8* data, Convert convert)
{
    std::vector<u16> row(size_t(width > 0 ? width : 0) + 1);
    auto one = [&](int y) {
        Take(f, row.data(), size_t(width) * 2);
        u32* p = reinterpret_cast<u32*>(data + pitch * y);
        for (int x = 0; x < width; ++x) *p++ = convert(row[size_t(x)]);
    };
    const int h = height;
    if (h < 1) {
        height = -h;
        for (int y = 0; y < height; ++y) one(y);
        return;
    }
    int y = 0;
    do {
        one(height - y - 1);
    } while (++y < height);
}
}  // namespace

void Bitmap::Load16(MemoryFile& f)
{
    Load16Bit(f, width, height, pitch, data, [](u32 v) {
        return (((u32(u8(v >> 11)) | 0xffffffe0u) << 9 | (v >> 5 & 0x3f)) << 7 | (v & 0x1f)) * 8;
    });
}

void Bitmap::Load15(MemoryFile& f)
{
    Load16Bit(f, width, height, pitch, data, [](u32 v) {
        return (((u32(u8(v >> 10)) | 0xffffffe0u) << 8 | (v >> 5 & 0x1f)) << 8 | (v & 0x1f)) * 8;
    });
}

void Bitmap::TGA16(MemoryFile& f) { Load15(f); }

void Bitmap::BMP8(MemoryFile& f, bool keepPalette, unsigned int colours)
{
    if (!colours || colours > 0x100) colours = 0x100;
    if (keepPalette) {
        if (!palette) palette = static_cast<PackedColor*>(Alloc::New(0x400));
        if (colours < 0x100) std::memset(reinterpret_cast<u8*>(palette) + colours * 4, 0, (0x100 - colours) * 4);
        Take(f, palette, colours * 4);
        if (height > 0) {
            for (int y = 0; y < height; ++y) Take(f, data + pitch * y, size_t(width));
            return;
        }
        height = -height;
        Take(f, data, size_t(width * height));
        return;
    }
    u8 pal[0x404] = {};
    if (colours < 0x100) std::memset(pal + colours * 4, 0, (0x100 - colours) * 4);
    Take(f, pal, colours * 4);
    u8 b = 0;
    auto one = [&](int y) {
        u32* p = reinterpret_cast<u32*>(data + pitch * y);
        for (int x = 0; x < width; ++x) {
            TakeByte(f, b);
            const u8* c = pal + b * 4;
            *p++ = 0xff000000u | u32(c[2]) << 16 | u32(c[1]) << 8 | c[0];
        }
    };
    if (height < 1) {
        height = -height;
        for (int y = 0; y < height; ++y) one(y);
    } else {
        for (int y = 0; y < height; ++y) one(height - y - 1);
    }
}

// a 256 x 24-bit palette, then indices. Kept as indices, bottom-up files are all copied over row 0 (as in the
// original); as colours the palette's bytes are taken as R, G, B in that order
void Bitmap::TGA8(MemoryFile& f, bool keepPalette)
{
    u8 pal[0x300] = {};
    Take(f, pal, sizeof(pal));
    if (keepPalette) {
        if (!palette) palette = static_cast<PackedColor*>(Alloc::New(0x400));
        u8* pp = reinterpret_cast<u8*>(palette);
        for (int k = 0; k < 256; ++k) {
            pp[k * 4] = pal[k * 3 + 2];
            pp[k * 4 + 1] = pal[k * 3 + 1];
            pp[k * 4 + 2] = pal[k * 3];
            pp[k * 4 + 3] = 0;
        }
        if (height > 0) {
            for (int y = 0; y < height; ++y) Take(f, data, size_t(width));
            return;
        }
        height = -height;
        for (int y = 0; y < height; ++y) Take(f, data + pitch * y, size_t(width));
        return;
    }
    u8 b = 0;
    auto one = [&](int y) {
        u32* p = reinterpret_cast<u32*>(data + pitch * y);
        for (int x = 0; x < width; ++x) {
            TakeByte(f, b);
            const u8* c = pal + b * 3;
            *p++ = 0xff000000u | u32(c[0]) << 16 | u32(c[1]) << 8 | c[2];
        }
    };
    if (height < 1) {
        height = -height;
        for (int y = 0; y < height; ++y) one(y);
    } else {
        for (int y = 0; y < height; ++y) one(height - y - 1);
    }
}

// ---- saving

bool __cdecl Bitmap::Save(const char* fileName) const
{
    const char* ext = std::strrchr(fileName, '.');
    if (!ext) return false;
    ++ext;
    File file;
    if (file.Open(fileName, File::CREATE_WRITE, 0).code) return false;
    IOSlaveFile slave(&file);
    if (!_strnicmp(ext, "tga", 3)) return SaveTGA(&slave);
    if (!_strnicmp(ext, "bmp", 3)) return SaveBMP(&slave, false);
    if (!_strnicmp(ext, "jpg", 3) || !_strnicmp(ext, "jpeg", 4)) return SaveJPG(&slave);
    return false;
}

bool __cdecl Bitmap::Save(IOSlave* slave, const char* fileName) const
{
    const char* ext = std::strrchr(fileName, '.');
    ext = ext ? ext + 1 : fileName;
    if (!_strnicmp(ext, "tga", 3)) return SaveTGA(slave);
    if (!_strnicmp(ext, "bmp", 3)) return SaveBMP(slave, false);
    if (!_strnicmp(ext, "jpg", 3) || !_strnicmp(ext, "jpeg", 4)) return SaveJPG(slave);
    return false;
}

// uncompressed, top-down (descriptor 0x20), 24 or 32 bits
bool Bitmap::SaveTGA(IOSlave* slave) const
{
    if (format != ARGB && format != XRGB && format != RGB) return false;
    TgaHeader hd{};
    hd.imageType = 2;
    hd.width = u16(width);
    hd.height = u16(height);
    hd.bits = format != RGB ? 32 : 24;
    hd.descriptor = 0x20;
    slave->WriteData(&hd, sizeof(hd));
    for (int y = 0; y < height; ++y) slave->WriteData(data + pitch * y, u32(bytesPerPixel * width));
    return true;
}

// bottom-up; 32 bits for ARGB, 24 for RGB and XRGB
bool Bitmap::SaveBMP(IOSlave* slave, bool) const
{
    if (!data || !width || !height || (format != ARGB && format != XRGB && format != RGB)) return false;
    const int bpp = format == ARGB ? 4 : 3;
    BmpFileHeader fh{};
    fh.type = 0x4d42;
    fh.size = u32(height * width * bpp + 0x36);
    fh.offBits = 0x36;
    slave->WriteData(&fh, sizeof(fh));
    BmpInfoHeader ih{};
    ih.size = 0x28;
    ih.width = width;
    ih.height = height;
    ih.planes = 1;
    ih.bitCount = u16(bpp * 8);
    ih.sizeImage = u32(height * width * bpp);
    slave->WriteData(&ih, sizeof(ih));
    if (format == RGB || format == ARGB) {
        const u8* p = data + pitch * (height - 1);
        for (int y = height - 1; y >= 0; --y, p -= pitch) slave->WriteData(p, u32(width * bpp));
    } else {
        for (int y = height - 1; y >= 0; --y) {
            const u8* p = data + pitch * y;
            for (int x = 0; x < width; ++x, p += bytesPerPixel) slave->WriteData(p, 3);
        }
    }
    return true;
}

// baseline JPEG, RGB in, every quantiser 1 (the original's tables scaled by 0)
bool Bitmap::SaveJPG(IOSlave* slave) const
{
    if (!((format == RGB && bytesPerPixel == 3) || ((format == ARGB || format == XRGB) && bytesPerPixel == 4)))
        return false;
    jpeg_compress_struct c{};
    JumpError err{};
    c.err = jpeg_std_error(&err.mgr);
    err.mgr.error_exit = ErrorExit;
    SlaveDest* dest = new SlaveDest{};
    u8* row = nullptr;
    if (setjmp(err.jump)) {
        jpeg_destroy_compress(&c);
        delete dest;
        if (row) Alloc::Delete(row);
        return false;
    }
    jpeg_create_compress(&c);
    dest->mgr.init_destination = InitDest;
    dest->mgr.empty_output_buffer = EmptyDest;
    dest->mgr.term_destination = TermDest;
    dest->slave = slave;
    c.dest = &dest->mgr;
    c.image_width = JDIMENSION(width);
    c.image_height = JDIMENSION(height);
    c.input_components = 3;
    c.in_color_space = JCS_RGB;
    jpeg_set_defaults(&c);
    jpeg_set_linear_quality(&c, 0, TRUE);
    jpeg_start_compress(&c, TRUE);
    row = static_cast<u8*>(Alloc::New(u32(width * 3)));
    while (c.next_scanline < c.image_height) {
        const u8* s = data + pitch * c.next_scanline;
        for (int x = 0; x < width; ++x, s += bytesPerPixel) {
            row[x * 3] = s[2];
            row[x * 3 + 1] = s[1];
            row[x * 3 + 2] = s[0];
        }
        JSAMPROW rows[1] = {row};
        jpeg_write_scanlines(&c, rows, 1);
    }
    Alloc::Delete(row);
    row = nullptr;
    jpeg_finish_compress(&c);
    jpeg_destroy_compress(&c);
    delete dest;
    return true;
}

}  // namespace Jet
