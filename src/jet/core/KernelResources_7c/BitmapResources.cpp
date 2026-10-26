// KernelResources_7c (TS12, 0x10001000..0x10003060): bitmap resources, the internal bitmap-frame and colour resources,
// their factories and the plugin table JetGetPluginsKernelResources. Transcribed from the assembly; Resource,
// ResourceInstance and the factory base live in KernelResource_7c, Bitmap and ConfigParser in KernelStandard_7c.
#include <jet/Config.h>
#include <jet/KernelResources.h>

#include <new>

namespace Jet {

namespace {

// 0x10001840: the intersection of two rectangles. An empty height clears x (not y) and an empty width clears y
// (not x): kept as in the original.
Rect Intersect(const Rect& a, const Rect& b)
{
    Rect r;
    r.x = a.x > b.x ? a.x : b.x;
    const int ar = a.x + a.w - 1, br = b.x + b.w - 1;
    r.w = (br <= ar ? br : ar) - r.x + 1;
    r.y = a.y > b.y ? a.y : b.y;
    const int ab = a.y + a.h - 1, bb = b.y + b.h - 1;
    r.h = (bb <= ab ? bb : ab) - r.y + 1;
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

const Point& Corner(const Rect& r) { return *reinterpret_cast<const Point*>(&r); }

void DeleteBitmap(Bitmap*& b)
{
    if (b) {
        b->~Bitmap();
        Alloc::Delete(b);
    }
    b = nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// BitmapBaseResource

BitmapBaseResource::BitmapBaseResource(const PString& name, const ResourceDatabase* owner) : Resource(name, owner) {}
BitmapBaseResource::BitmapBaseResource(const BitmapBaseResource& o) : Resource(o) {}
BitmapBaseResource::~BitmapBaseResource() {}

BitmapBaseResource& BitmapBaseResource::operator=(const BitmapBaseResource& o)
{
    Resource::operator=(o);
    return *this;
}

void __cdecl BitmapBaseResource::Render(const ResourceDatabase* orig, Bitmap& dest, const Rect& area,
                                        const Rect& clip)  // 0x10001310
{
    const Size none = {0, 0};
    Render(orig, dest, area, clip, 0, none);
}

// 0x10001350: tiles the bitmap over `area` (at most maxRepeat tiles each way when non-zero), justified by `just`
// (bits 0-1: 2 right, 3 centre; bits 2-3: 8 bottom, 0xc centre), each tile clipped to the area and to `clip`
void __cdecl BitmapBaseResource::Render(const ResourceDatabase*, Bitmap& dest, const Rect& area, const Rect& clip,
                                        unsigned int just, const Size& maxRepeat)
{
    if (area.w <= 0 || area.h <= 0) return;
    if (Lock().code == 0) {
        const Bitmap* src = GetBitmap();
        const Rect s = GetArea();
        if (s.w > 0 && s.h > 0) {
            int nx = (area.w + s.w - 1) / s.w;
            int ny = (area.h + s.h - 1) / s.h;
            if (maxRepeat.w != 0 && maxRepeat.w <= nx) nx = maxRepeat.w;
            if (maxRepeat.h != 0 && maxRepeat.h <= ny) ny = maxRepeat.h;

            int x0, x1, dx;
            if ((just & 3) == 2) {
                x0 = area.x - s.w + area.w;
                x1 = x0 - nx * s.w;
                dx = -s.w;
            } else {
                x0 = (just & 3) == 3 ? (area.w / 2 - s.w / 2) + area.x : area.x;
                x1 = nx * s.w + x0;
                dx = s.w;
            }
            int y0, y1, dy;
            if ((just & 0xc) == 8) {
                y0 = area.h - s.h + area.y;
                y1 = y0 - ny * s.h;
                dy = -s.h;
            } else {
                y0 = (just & 0xc) == 0xc ? (area.h / 2 - s.h / 2) + area.y : area.y;
                y1 = ny * s.h + y0;
                dy = s.h;
            }

            const int bottom = area.y + area.h, right = area.x + area.w - 1;
            for (int y = y0; y != y1; y += dy) {
                int tileH = s.h;
                for (int x = x0; x != x1; x += dx) {
                    int tileW = s.w;
                    const int cy = y < 0 ? -y : 0;
                    int h = s.h - cy;
                    const int cx = x < 0 ? -x : 0;
                    int w = s.w - cx;
                    if (y + tileH - 1 > bottom - 1) tileH = bottom - y;
                    if (x + s.w - 1 > right) tileW = area.x - x + area.w;
                    Rect r = {x + cx, y + cy, w, h};
                    if (r.y + h - 1 > bottom - 1) r.h = bottom - r.y;
                    if (r.x + w - 1 > right) r.w = area.x - r.x + area.w;
                    const Rect c = Intersect(clip, r);
                    const Rect from = {c.x - x + s.x, c.y + (s.y - y), c.w, c.h};
                    const Rect tile = {x, y, tileW, tileH};
                    const Rect to = Intersect(clip, tile);
                    if (to.w > 0 && to.h > 0) dest.Image(*src, Corner(to), from, 0);
                }
            }
        }
    }
    Unlock();
}

// 0x10001680: one copy of the bitmap justified in `area`, clipped to `clip`
void __cdecl BitmapBaseResource::Render(const ResourceDatabase*, Bitmap& dest, const Rect& area, unsigned int just,
                                        const Rect& clip)
{
    if (Lock().code == 0) {
        const Bitmap* src = GetBitmap();
        const Rect s = GetArea();
        int ox = 0, oy = 0;
        if ((just & 3) == 2)
            ox = area.w - s.w;
        else if ((just & 3) == 3)
            ox = (area.w - s.w) / 2;
        if ((just & 0xc) == 8)
            oy = area.h - s.h;
        else if ((just & 0xc) == 0xc)
            oy = (area.h - s.h) / 2;
        const Rect placed = {area.x + ox, area.y + oy, s.w, s.h};
        const Rect to = Intersect(clip, placed);
        const Rect from = {to.x - area.x - ox + s.x, to.y - area.y - oy + s.y, to.w, to.h};
        if (to.w > 0 && to.h > 0) dest.Image(*src, Corner(to), from, 0);
    }
    Unlock();
}

const Bitmap* __cdecl BitmapBaseResource::GetBitmap() const  // 0x100017c0
{
    throw Exception("C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/KernelResources/BitmapBaseResource.cpp", 0x118,
                    "GetBitmap() not implimented");
}

const Rect __cdecl BitmapBaseResource::GetArea() const { return Rect{0, 0, 0, 0}; }
const Point __cdecl BitmapBaseResource::GetOffset() const { return Point{0, 0}; }

// ---------------------------------------------------------------------------------------------------------------
// BitmapFrameBaseResource

BitmapFrameBaseResource::BitmapFrameBaseResource(const PString& name, const ResourceDatabase* owner)
    : BitmapBaseResource(name, owner)
{
}
BitmapFrameBaseResource::BitmapFrameBaseResource(const BitmapFrameBaseResource& o) : BitmapBaseResource(o) {}
BitmapFrameBaseResource::~BitmapFrameBaseResource() {}

BitmapFrameBaseResource& BitmapFrameBaseResource::operator=(const BitmapFrameBaseResource& o)
{
    Resource::operator=(o);
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------
// BitmapResource: a whole image file

int BitmapResource::MAX_STREAMING_TEXTURE_SIZE = 2048;

BitmapResource::BitmapResource(const PString& name, const ResourceDatabase* owner)
    : BitmapFrameBaseResource(name, owner), bitmap(nullptr)
{
}

BitmapResource::BitmapResource(const BitmapResource& o) : BitmapFrameBaseResource(o), bitmap(o.bitmap) {}

BitmapResource::~BitmapResource() { DeleteBitmap(bitmap); }

BitmapResource& BitmapResource::operator=(const BitmapResource& o)  // shallow, as in the original
{
    Resource::operator=(o);
    bitmap = o.bitmap;
    return *this;
}

void __cdecl BitmapResource::SetMaxSizeTextureStreaming(int size) { MAX_STREAMING_TEXTURE_SIZE = size; }

unsigned int __cdecl BitmapResource::CalculateSize() const { return sizeof(BitmapResource); }
unsigned int __cdecl BitmapResource::CalculateDataSize() const { return bitmap ? bitmap->dataSize + 0x44 : 0; }

Resource::Result __cdecl BitmapResource::Load(const char* name)  // 0x10002010
{
    void* p = Alloc::New(sizeof(Bitmap));
    bitmap = p ? ::new (p) Bitmap() : nullptr;
    bool ok;
    {
        const PString fileName = Owner()->GetFileName(PString(name));
        const PString none;
        ok = bitmap->Load(fileName, none, false);
    }
    if (ok) {
        const int max = MAX_STREAMING_TEXTURE_SIZE * MAX_STREAMING_TEXTURE_SIZE;
        if (bitmap->width * bitmap->height <= max) return Result(0);
        int w = bitmap->width, h = bitmap->height;
        while (h * w > max) {
            h >>= 1;
            w >>= 1;
        }
        if (w > 0 && h > 0) {
            Bitmap* scaled = bitmap->Scale(w, h);
            DeleteBitmap(bitmap);
            bitmap = scaled;
            return Result(0);
        }
    }
    DeleteBitmap(bitmap);
    return Result(1);
}

void __cdecl BitmapResource::SetBitmap(const Bitmap* b)  // 0x100021f0
{
    DeleteBitmap(bitmap);
    void* p = Alloc::New(sizeof(Bitmap));
    bitmap = p ? ::new (p) Bitmap(*b, Rect{0, 0, b->width, b->height}) : nullptr;
}

const Bitmap* __cdecl BitmapResource::GetBitmap() const { return bitmap; }
const Rect __cdecl BitmapResource::GetArea() const { return Rect{0, 0, bitmap->width, bitmap->height}; }

bool __cdecl BitmapResource::Unload()
{
    DeleteBitmap(bitmap);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// BitmapFrameResource ("bmframe.txt", not exported): an area of another bitmap resource, from a text file
//   Frame { Image <resource name>  Area <x, y, w, h> }

namespace {

class BitmapFrameResource : public BitmapFrameBaseResource {
public:
    BitmapFrameResource(const PString& name, const ResourceDatabase* owner) : BitmapFrameBaseResource(name, owner) {}
    ~BitmapFrameResource() override {}

    Result __cdecl Lock() const override  // 0x100019d0: both locked, or neither
    {
        if (Resource::Lock().code == 0) {
            if (image.Lock().code == 0) return Result(0);
            Resource::Unlock();
        }
        return Result(4);
    }

    void __cdecl Unlock() const override
    {
        image.Unlock();
        Resource::Unlock();
    }

    Result __cdecl IsAllLoaded() const override
    {
        const Result r = Resource::IsAllLoaded();
        if (r.code != 0) return r;
        return image.IsAllLoaded();
    }

protected:
    Result __cdecl LoadTxt(const char* name) override;  // 0x10001a90

    bool __cdecl Unload() override  // 0x10001dd0
    {
        area = Rect{0, 0, 0, 0};
        image.Reset();
        return true;
    }

    unsigned int __cdecl CalculateSize() const override { return sizeof(BitmapFrameResource); }

    const Bitmap* __cdecl GetBitmap() const override  // 0x10001970
    {
        if (image.Lock().code != 0) return nullptr;
        const Bitmap* b = static_cast<const BitmapBaseResource*>(ImageResource())->GetBitmap();
        image.Unlock();
        return b;
    }

    const Rect __cdecl GetArea() const override { return area; }

private:
    // the frame's source resource (a ResourceInstance's first field)
    class Handle : public ResourceInstance {
    public:
        const Resource* Get() const { return resource; }
    };
    const Resource* ImageResource() const { return image.Get(); }

    Rect area;     // +0x5c, not initialised by the constructor
    Handle image;  // +0x6c
};

Resource::Result __cdecl BitmapFrameResource::LoadTxt(const char* name)
{
    static const PString kFrame("Frame"), kImage("Image"), kArea("Area");
    const PString fileName = Owner()->GetFileName(PString(name));
    ConfigParser parser;
    if (parser.Parse(fileName.Text()).code != 0) return Result(1);
    const ConfigData& frame = parser.Root()(kFrame);
    PString image;
    frame(kImage) >> image;
    if (image.Length() != 0) {
        String s(image.Text());
        ConvertResourceName(s);
        this->image.Init(Owner(), PString(static_cast<const char*>(s)));
    }
    frame(kArea) >> area;
    return Result(0);
}

// ColourResource ("colour.txt", "color.txt", not exported): a plain colour that renders as a box
//   Color { Red <0-255>  Green  Blue  Alpha  Blend <bool> }
class ColourResource : public BitmapBaseResource {
public:
    ColourResource(const PString& name, const ResourceDatabase* owner) : BitmapBaseResource(name, owner)
    {
        colour.argb = 0;
        if ((colour.argb >> 24) == 0) colour.argb |= 0xff000000u;  // an inlined PackedColor default
        blend = false;
    }
    ~ColourResource() override {}

    // slots 19 and 20 share one function in the original (0x10002690)
    void __cdecl Render(const ResourceDatabase*, Bitmap& dest, const Rect& area, const Rect& clip) override
    {
        Box(dest, area, clip);
    }
    void __cdecl Render(const ResourceDatabase*, Bitmap& dest, const Rect& area, const Rect& clip, unsigned int,
                        const Size&) override
    {
        Box(dest, area, clip);
    }
    using BitmapBaseResource::Render;

protected:
    Result __cdecl LoadTxt(const char* name) override;  // 0x10002320
    unsigned int __cdecl CalculateSize() const override { return sizeof(ColourResource); }

private:
    void Box(Bitmap& dest, const Rect& area, const Rect& clip)
    {
        if (Lock().code == 0) dest.Box(Intersect(clip, area), colour, blend ? 5 : 1);
        Unlock();
    }

    PackedColor colour;  // +0x5c bytes b, g, r, a
    bool blend;          // +0x60
};

Resource::Result __cdecl ColourResource::LoadTxt(const char* name)
{
    static const PString kColor("Color"), kRed("Red"), kGreen("Green"), kBlue("Blue"), kAlpha("Alpha"),
        kBlend("Blend");
    colour.argb = 0xff000000u;
    blend = true;
    const PString fileName = Owner()->GetFileName(PString(name));
    ConfigParser parser;
    if (parser.Parse(fileName.Text()).code != 0) return Result(1);
    unsigned char* c = reinterpret_cast<unsigned char*>(&colour);
    const ConfigData& section = parser.Root()(kColor);
    section(kRed) >> c[2];
    section(kGreen) >> c[1];
    section(kBlue) >> c[0];
    section(kAlpha) >> c[3];
    section(kBlend) >> blend;
    return Result(0);
}

}  // namespace

JET_LAYOUT32(sizeof(BitmapFrameResource) == 0x78);
JET_LAYOUT32(sizeof(ColourResource) == 0x64);

// ---------------------------------------------------------------------------------------------------------------
// instances: a ResourceInstance whose resource is a BitmapBaseResource

BitmapBaseInstance::BitmapBaseInstance() {}
BitmapBaseInstance::BitmapBaseInstance(const BitmapBaseInstance& o) : ResourceInstance(o) {}
BitmapBaseInstance::~BitmapBaseInstance() {}

BitmapBaseInstance& BitmapBaseInstance::operator=(const BitmapBaseInstance& o)
{
    ResourceInstance::operator=(o);
    return *this;
}

BitmapBaseResource* __cdecl BitmapBaseInstance::GetBitmapBaseResource() const
{
    return const_cast<BitmapBaseResource*>(static_cast<const BitmapBaseResource*>(resource));
}

bool __cdecl BitmapBaseInstance::IsAvailable() const
{
    return resource && resource->Status() == Resource::STATUS_AVAILABLE;
}

const Bitmap* __cdecl BitmapBaseInstance::GetBitmap() const
{
    if (Lock().code != 0) return nullptr;
    const Bitmap* b = GetBitmapBaseResource()->GetBitmap();
    Unlock();
    return b;
}

const Rect __cdecl BitmapBaseInstance::GetArea() const
{
    Rect r = {0, 0, 0, 0};
    if (Lock().code == 0) {
        r = GetBitmapBaseResource()->GetArea();
        Unlock();
    }
    return r;
}

const Point __cdecl BitmapBaseInstance::GetOffset() const { return GetBitmapBaseResource()->GetOffset(); }

void __cdecl BitmapBaseInstance::Render(Bitmap& dest, const Rect& area, const Rect& clip) const
{
    if (resource && Lock().code == 0) {
        GetBitmapBaseResource()->Render(orig, dest, area, clip);
        Unlock();
    }
}

void __cdecl BitmapBaseInstance::Render(Bitmap& dest, const Rect& area, const Rect& clip, unsigned int just,
                                        const Size& maxRepeat) const
{
    if (resource && Lock().code == 0) {
        GetBitmapBaseResource()->Render(orig, dest, area, clip, just, maxRepeat);
        Unlock();
    }
}

void __cdecl BitmapBaseInstance::Render(Bitmap& dest, const Rect& area, unsigned int just, const Rect& clip) const
{
    if (resource && Lock().code == 0) {
        GetBitmapBaseResource()->Render(orig, dest, area, just, clip);
        Unlock();
    }
}

BitmapFrameBaseInstance::BitmapFrameBaseInstance() {}
BitmapFrameBaseInstance::BitmapFrameBaseInstance(const BitmapFrameBaseInstance& o) : BitmapBaseInstance(o) {}
BitmapFrameBaseInstance::~BitmapFrameBaseInstance() {}
BitmapFrameBaseInstance& BitmapFrameBaseInstance::operator=(const BitmapFrameBaseInstance& o)
{
    BitmapBaseInstance::operator=(o);
    return *this;
}

BitmapInstance::BitmapInstance() {}
BitmapInstance::BitmapInstance(const BitmapInstance& o) : BitmapFrameBaseInstance(o) {}
BitmapInstance::~BitmapInstance() {}
BitmapInstance& BitmapInstance::operator=(const BitmapInstance& o)
{
    BitmapFrameBaseInstance::operator=(o);
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------
// the plugin table (0x10002e40): the factories register themselves with the plugin system as they are built

extern "C" void __cdecl JetGetPluginsKernelResources()
{
    const UId group("4C414E80-BBDA-11d4-9AD7-0001025364B8");
    new ResourceType<BitmapResource>(PString("tga"), UId("4C414E81-BBDA-11d4-9AD7-0001025364B8"), group);
    new ResourceType<BitmapResource>(PString("bmp"), UId("4C414E82-BBDA-11d4-9AD7-0001025364B8"), group);
    new ResourceType<BitmapResource>(PString("jpg"), UId("4C414E82-BBDA-11d4-9AD7-0001025364B8"), group);
    new ResourceType<BitmapResource>(PString("pcx"), UId("4C414E82-BBDA-11d4-9AD7-0001025364B8"), group);
    new ResourceType<BitmapFrameResource>(PString("bmframe.txt"), UId("4C414E83-BBDA-11d4-9AD7-0001025364B8"),
                                          group);
    new ResourceType<ColourResource>(PString("colour.txt"), UId("4C414E84-BBDA-11d4-9AD7-0001025364B8"), group);
    new ResourceType<ColourResource>(PString("color.txt"), UId("4C414E85-BBDA-11d4-9AD7-0001025364B8"), group);
}

}  // namespace Jet
