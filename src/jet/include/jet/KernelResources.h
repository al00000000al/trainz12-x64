// KernelResources_7c: bitmap resources (BitmapBaseResource, BitmapFrameBaseResource, BitmapResource) and their
// instance handles. Mangled names, vtables and layouts match TS12; the vtable slots past Resource's are noted.
#pragma once

#include <jet/Bitmap.h>
#include <jet/Resource.h>

namespace Jet {

class BitmapBaseResource : public Resource {
public:
    BitmapBaseResource(const PString& name, const ResourceDatabase* owner);
    BitmapBaseResource(const BitmapBaseResource& o);
    ~BitmapBaseResource() override;
    BitmapBaseResource& operator=(const BitmapBaseResource& o);

    // MSVC puts overloaded virtuals in reverse order: slot 18 is the (area, just, clip) one, 19 the tiled, 20 this
    virtual void __cdecl Render(const ResourceDatabase* orig, Bitmap& dest, const Rect& area, const Rect& clip);
    virtual void __cdecl Render(const ResourceDatabase* orig, Bitmap& dest, const Rect& area, const Rect& clip,
                                unsigned int just, const Size& maxRepeat);
    virtual void __cdecl Render(const ResourceDatabase* orig, Bitmap& dest, const Rect& area, unsigned int just,
                                const Rect& clip);
    virtual const Bitmap* __cdecl GetBitmap() const;  // 21: throws
    virtual const Rect __cdecl GetArea() const;       // 22
    virtual const Point __cdecl GetOffset() const;    // 23
};
JET_LAYOUT32(sizeof(BitmapBaseResource) == 0x5c);

class BitmapFrameBaseResource : public BitmapBaseResource {
public:
    BitmapFrameBaseResource(const PString& name, const ResourceDatabase* owner);
    BitmapFrameBaseResource(const BitmapFrameBaseResource& o);
    ~BitmapFrameBaseResource() override;
    BitmapFrameBaseResource& operator=(const BitmapFrameBaseResource& o);
};

class BitmapResource : public BitmapFrameBaseResource {
public:
    BitmapResource(const PString& name, const ResourceDatabase* owner);
    BitmapResource(const BitmapResource& o);
    ~BitmapResource() override;
    BitmapResource& operator=(const BitmapResource& o);
    static void __cdecl SetMaxSizeTextureStreaming(int size);

protected:
    Result __cdecl Load(const char* name) override;
    bool __cdecl Unload() override;
    unsigned int __cdecl CalculateSize() const override;
    unsigned int __cdecl CalculateDataSize() const override;
    const Bitmap* __cdecl GetBitmap() const override;
    const Rect __cdecl GetArea() const override;
    virtual void __cdecl SetBitmap(const Bitmap* bitmap);  // slot 24

    static int MAX_STREAMING_TEXTURE_SIZE;  // larger bitmaps are halved until width * height fits its square

private:
    Bitmap* bitmap;  // +0x5c
};
JET_LAYOUT32(sizeof(BitmapResource) == 0x60);

class BitmapBaseInstance : public ResourceInstance {
public:
    BitmapBaseInstance();
    BitmapBaseInstance(const BitmapBaseInstance& o);
    ~BitmapBaseInstance();
    BitmapBaseInstance& operator=(const BitmapBaseInstance& o);

    void __cdecl Render(Bitmap& dest, const Rect& area, const Rect& clip) const;
    void __cdecl Render(Bitmap& dest, const Rect& area, const Rect& clip, unsigned int just,
                        const Size& maxRepeat) const;
    void __cdecl Render(Bitmap& dest, const Rect& area, unsigned int just, const Rect& clip) const;
    const Bitmap* __cdecl GetBitmap() const;
    const Rect __cdecl GetArea() const;
    const Point __cdecl GetOffset() const;
    bool __cdecl IsAvailable() const;

private:
    BitmapBaseResource* __cdecl GetBitmapBaseResource() const;
};

class BitmapFrameBaseInstance : public BitmapBaseInstance {
public:
    BitmapFrameBaseInstance();
    BitmapFrameBaseInstance(const BitmapFrameBaseInstance& o);
    ~BitmapFrameBaseInstance();
    BitmapFrameBaseInstance& operator=(const BitmapFrameBaseInstance& o);
};

class BitmapInstance : public BitmapFrameBaseInstance {
public:
    BitmapInstance();
    BitmapInstance(const BitmapInstance& o);
    ~BitmapInstance();
    BitmapInstance& operator=(const BitmapInstance& o);
};

}  // namespace Jet
