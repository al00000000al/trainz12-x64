// Render2DCore_7c: 2D overlays (Canvas, SelectionBox, SpriteManager) and sprites (SpriteInstance, SpriteResource).
// Layouts, vtables and mangled names match TS12.
#pragma once

#include <jet/RenderSystem.h>
#include <jet/Resource.h>

namespace Jet {

class SpriteManager;
class SpriteResource;

// the dirty-rectangle list Canvas::Update reads: nodes {Rect, link}, each link pointing at the next node's link
class RectList {
public:
    struct Node {
        Rect r;
        Node* link;  // the next node's `link` field, or null
        const Node* Next() const { return link ? reinterpret_cast<const Node*>(reinterpret_cast<const char*>(link) - 0x10) : nullptr; }
    };
    const Node* First() const { return head; }

private:
    const Node* head;
};

// an off-screen surface drawn as up to 7 x 5 textures; Update uploads the dirty parts of a pixel buffer
class Canvas : public Overlay {
public:
    Canvas(const PString& name, int priority, bool flag);
    Canvas(LayerGroup* group, const PString& name, int priority, bool flag);
    Canvas(const Canvas& o);
    ~Canvas() override;
    Canvas& operator=(const Canvas& o);

    void __cdecl Render() override;
    void __cdecl Create(const Size& tileSize, const Rect& area);
    void __cdecl Destroy();
    void __cdecl Update(const RectList& dirty, const RectList& active, unsigned long* pixels, int pitch);
    void __cdecl SetPosition(int x, int y);
    void __cdecl SetColor(float r, float g, float b);
    void __cdecl SetOpacity(float a);
    const Color& __cdecl GetColor() const;
    float __cdecl GetScale() const;
    void __cdecl SetScale(float s);

protected:
    void __cdecl FindActiveList(const RectList& list);
    void __cdecl SetDefaultValues();

private:
    enum { COLS = 7, ROWS = 5, PER_TILE = 16 };
    Texture* tiles[COLS][ROWS];               // +0x3c
    Rect rects[COLS][ROWS][PER_TILE];         // +0xc8 the parts of each tile to draw / upload
    int counts[COLS][ROWS];                   // +0x23c8
    int tileW, tileH;                         // +0x2454
    int cols, rows;                           // +0x245c
    int x, y;                                 // +0x2464
    Color color;                              // +0x246c
    Rect area;                                // +0x247c
    float scale;                              // +0x248c
    int declaration;                          // +0x2490
};
JET_LAYOUT32(sizeof(Canvas) == 0x2494);

class SelectionBox : public Overlay {
public:
    SelectionBox(const PString& name, int priority, bool flag);
    SelectionBox(LayerGroup* group, const PString& name, int priority, bool flag);
    SelectionBox(const SelectionBox& o);
    ~SelectionBox() override;
    SelectionBox& operator=(const SelectionBox& o);

    void SetRect(const Rect& r);
    const Rect& GetRect() const;
    void SetAreaColor(const Color& c);
    void SetDarkColor(const Color& c);
    void SetLightColor(const Color& c);

protected:
    void __cdecl Render() override;

private:
    Rect rect;         // +0x3c
    Color dark;        // +0x4c
    Color light;       // +0x5c
    Color areaColor;   // +0x6c
};
JET_LAYOUT32(sizeof(SelectionBox) == 0x7c);

// what a sprite resource file sets and every instance copies (0x50 bytes)
struct SpriteAttributes {
    Vector3 position;  // z is the drawing priority
    Vector2 size;
    float uv[4];       // u0, v0, u1, v1
    Color color;
    float rotation;
    float scale;
    bool alphaTestSet, alphaBlendSet, alphaTestMode, alphaBlendMode;
    DFunction alphaFunction;
    float alphaReference;
    Src::Factor srcFactor;
    Dst::Factor dstFactor;
};
static_assert(sizeof(SpriteAttributes) == 0x50);

struct SpriteVertex {
    float x, y, z;
    unsigned long color;
    float u, v;
};

class SpriteInstance {
public:
    SpriteInstance();
    SpriteInstance(const SpriteInstance& o);
    virtual ~SpriteInstance();
    SpriteInstance& operator=(const SpriteInstance& o);

    bool __cdecl Open(RenderKernel* kernel, const PString& name, const ResourceDatabase* db);
    void __cdecl SetVisible(bool v);
    bool __cdecl GetVisible();
    void __cdecl SetPosition(const Vector2& p, PointPosition anchor);
    Vector2 __cdecl GetPosition();
    void __cdecl SetColor(float r, float g, float b);
    Color __cdecl GetColor();
    void __cdecl SetOpacity(float a);
    float __cdecl GetOpacity();
    void __cdecl SetScale(float s);
    float __cdecl GetScale();
    void __cdecl SetRotation(float r);
    float __cdecl GetRotation();
    void __cdecl SetPriority(float p);
    float __cdecl GetPriority();
    void SetSize(const Vector2& s);
    const Vector2& GetSize() const;
    void SetScreenSize(const Size& screen);
    const Size& GetPixelSize() const;
    void __cdecl EnableFixedPixelSize(int w, int h);
    void __cdecl DisableFixedPixelSize();
    void __cdecl SetUV(const Vector2& a, const Vector2& b);
    void __cdecl GetUV(Vector2& a, Vector2& b) const;
    void __cdecl SetSpriteAlphaTestMode(bool on);
    void __cdecl SetSpriteAlphaTestFunction(DFunction f, float reference);
    void __cdecl SetSpriteAlphaBlendMode(bool on);
    void __cdecl SetSpriteAlphaBlendFactor(Src::Factor src, Dst::Factor dst);
    JET_HEAP_OBJECT

protected:
    friend class SpriteManager;
    virtual void __cdecl Update(float dt);                         // 1
    virtual void __cdecl Cull();                                   // 2
    virtual void __cdecl UpdateAttributes(RenderKernel* kernel);   // 3
    void __cdecl Render(RenderKernel* kernel, Texture*& current);
    bool __cdecl operator<(const SpriteInstance& o);
    static int __cdecl PriorityCompare(const void* a, const void* b);

    enum { VISIBLE = 1, DIRTY = 2 };
    SpriteVertex vertices[4];       // +0x04
    SpriteResource* resource;       // +0x64
    SpriteAttributes attr;          // +0x68
    unsigned int flags;             // +0xb8
    SpriteManager* manager;         // +0xbc
    PointPosition anchor;           // +0xc0
    bool fixedPixelSize;            // +0xc4
    int pixelW, pixelH;             // +0xc8
};
JET_LAYOUT32(sizeof(SpriteInstance) == 0xd0);

class SpriteManager : public Overlay {
public:
    SpriteManager(const PString& name, int priority, bool flag, bool adjustAspect);
    SpriteManager(LayerGroup* group, const PString& name, int priority, bool flag, bool adjustAspect);
    SpriteManager(const SpriteManager& o);
    ~SpriteManager() override;
    SpriteManager& operator=(const SpriteManager& o);

    void __cdecl Render() override;
    void __cdecl Update(float dt) override;
    void __cdecl Add(SpriteInstance* s);
    void __cdecl Remove(SpriteInstance* s);
    void __cdecl Clear();
    void __cdecl Purge();
    void __cdecl SetSortParticleArray();
    void __cdecl SetAdjustAspectRatio(bool on);
    bool __cdecl IsAdjustAspectRatio() const;

protected:
    void __cdecl Cull();

private:
    friend class SpriteInstance;
    void Copy(const SpriteManager& o);

    unsigned int count;          // +0x3c
    unsigned int capacity;       // +0x40
    SpriteInstance** sprites;    // +0x44
    unsigned char managerFlags;  // +0x48 bit 0: sort pending, bit 1: adjust the aspect ratio
};
JET_LAYOUT32(sizeof(SpriteManager) == 0x4c);

}  // namespace Jet
