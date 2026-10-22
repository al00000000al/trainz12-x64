// SpriteInstance, SpriteManager, SpriteResource and the plugin table (Render2DCore 0x100021f0..0x10002a00,
// 0x10003130..0x10004c80), transcribed from the assembly. Float arithmetic is SSE scalar in the original except the
// rotation's cos/sin (x87, rounded to float).
#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "SpriteResource.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace Jet {

namespace {

// Texture's slots used by sprites
Size TextureSize(Texture* t)
{
    Size s;
    VCall<Size*>(t, 0x38, &s);
    return s;
}
void TextureTouch(Texture* t) { VCall<void>(t, 0x3c, -1); }
void TextureSelect(Texture* t) { VCall<void>(t, 0x44, 0); }

const int kSpriteFormat = 0x13;

void CosSin(float a, float& c, float& s)  // fcos and fsin of the float, each rounded to float
{
#if defined(_M_IX86)
    float rc, rs;
    __asm {
        fld dword ptr [a]
        fld st(0)
        fcos
        fstp dword ptr [rc]
        fsin
        fstp dword ptr [rs]
    }
    c = rc;
    s = rs;
#else
    c = static_cast<float>(std::cos(static_cast<double>(a)));
    s = static_cast<float>(std::sin(static_cast<double>(a)));
#endif
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// SpriteResource

SpriteResource::SpriteResource(const PString& name, const ResourceDatabase* owner) : Resource(name, owner)
{
    texture = nullptr;
    attr.position = Vector3(0.5f, 0.5f, 1.0f);
    attr.size = Vector2(1.0f, 1.0f);
    attr.uv[0] = 0.0f, attr.uv[1] = 0.0f, attr.uv[2] = 1.0f, attr.uv[3] = 1.0f;
    attr.color = Color{1.0f, 1.0f, 1.0f, 1.0f};
    attr.rotation = 0.0f;
    attr.scale = 1.0f;
    attr.alphaTestSet = attr.alphaBlendSet = attr.alphaTestMode = attr.alphaBlendMode = false;
    attr.alphaFunction = DFUNCTION_ALWAYS;
    attr.alphaReference = 0.0f;
    attr.srcFactor = Src::SRC_ONE;
    attr.dstFactor = Dst::DST_ONE;
    uvi = Rect{-1, -1, -1, -1};
    declaration = 0;
}

SpriteResource::~SpriteResource()  // 0x10003f70
{
    if (texture) reinterpret_cast<const ReferenceCount*>(texture)->ReleaseReference();
}

Resource::Result __cdecl SpriteResource::Load(const char*) { return Result(6); }

Resource::Result __cdecl SpriteResource::LoadTxt(const char* name)
{
    static const PString kSprite("Sprite"), kTexture("Texture"), kUV("UV"), kUVI("UVI"), kColor("Color"),
        kSize("Size"), kPosition("Position"), kRotation("Rotation"), kScale("Scale"),
        kAlphaBlendMode("AlphaBlendMode"), kAlphaTestMode("AlphaTestMode"), kAlphaTestFunction("AlphaTestFunction"),
        kAlphaRef("AlphaRef"), kSrcFactor("SrcFactor"), kDstFactor("DstFactor");
    File::Path dirPath(name);
    dirPath.DelName(false);
    const PString dir(dirPath.text);  // the folder of the sprite file, for a relative texture name
    ConfigParser parser;
    bool ok;
    {
        const PString fileName = Owner()->GetFileName(PString(name));
        ok = parser.Parse(fileName.Text()).code == 0;
    }
    const ConfigData* unknown = &ConfigData::unknownItem;
    if (!ok || parser.RootPtr() == unknown) return Result(1);
    const ConfigData& sprite = parser.Root()(kSprite);
    if (&sprite == unknown) return Result(1);

    sprite(kTexture) >> textureName;
    const char* t = textureName.Text();
    if (t && t[0] != '\\' && t[0] != '/') {
        PString full = dir;  // dir + name (PStringCache::Ins at the end of dir)
        full.node = PString::Cache()->Ins(full.node, reinterpret_cast<const unsigned char*>(t), dir.Length());
        textureName = full;
    }
    if (const float* uv = sprite(kUV).GetFloatArray()) {
        attr.uv[0] = uv[0];
        attr.uv[1] = uv[1];
        attr.uv[2] = uv[2];
        attr.uv[3] = uv[3];
    }
    sprite(kUVI) >> uvi;
    sprite(kColor) >> attr.color;
    sprite(kSize) >> attr.size;
    sprite(kPosition) >> attr.position;
    sprite(kRotation) >> attr.rotation;
    sprite(kScale) >> attr.scale;
    if (&sprite(kAlphaBlendMode) != unknown) attr.alphaBlendSet = true;
    if (&sprite(kAlphaTestMode) != unknown) attr.alphaTestSet = true;
    sprite(kAlphaBlendMode) >> attr.alphaBlendMode;
    sprite(kAlphaTestMode) >> attr.alphaTestMode;
    sprite(kAlphaRef) >> attr.alphaReference;
    PString function, src, dst;
    sprite(kAlphaTestFunction) >> function;
    sprite(kSrcFactor) >> src;
    sprite(kDstFactor) >> dst;
    attr.alphaFunction = MaterialFileSupport::ConvertDFunction(function);
    attr.srcFactor = MaterialFileSupport::ConvertSrc(src);
    attr.dstFactor = MaterialFileSupport::ConvertDst(dst);
    return Result(textureName.Length() != 0 ? 0 : 1);  // a sprite needs a texture
}

bool SpriteResource::Prepare(RenderKernel* kernel)
{
    if (!texture) {
        texture = VCall<Texture*>(kernel->TextureManager(), 0x0c, Owner(), &textureName, &ScrambleKey());
        if (!texture) return false;
    }
    if (declaration == 0) declaration = kernel->GetDisplay()->GetDeclaration(kSpriteFormat);
    const Size s = TextureSize(texture);
    if (uvi.x < 0 || uvi.y < 0) {
        uvi = Rect{0, 0, s.w, s.h};
        return true;
    }
    const int right = uvi.w + uvi.x, bottom = uvi.h + uvi.y;
    if (s.w < right || s.h < bottom) return true;
    const float w = static_cast<float>(s.w), h = static_cast<float>(s.h);
    attr.uv[0] = static_cast<float>(uvi.x) / w;
    attr.uv[1] = static_cast<float>(uvi.y) / h;
    attr.uv[2] = static_cast<float>(right) / w;
    attr.uv[3] = static_cast<float>(bottom) / h;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// SpriteInstance

SpriteInstance::SpriteInstance() : resource(nullptr), manager(nullptr), anchor(POINT_CENTRE)
{
    flags |= VISIBLE;  // the other bits stay as they were, as in the original
}

SpriteInstance::SpriteInstance(const SpriteInstance& o)
{
    std::memcpy(vertices, o.vertices, sizeof vertices);
    resource = o.resource;
    if (resource) resource->InlineAddReference();
    attr = o.attr;
    flags = (flags & ~3u) | (o.flags & 3u);
    manager = o.manager;
    anchor = o.anchor;
    fixedPixelSize = o.fixedPixelSize;
    pixelW = o.pixelW;
    pixelH = o.pixelH;
}

SpriteInstance::~SpriteInstance()
{
    if (resource) resource->ReleaseReference();
}

SpriteInstance& SpriteInstance::operator=(const SpriteInstance& o)
{
    std::memcpy(vertices, o.vertices, sizeof vertices);
    SpriteResource* old = resource;
    resource = o.resource;
    if (resource) resource->InlineAddReference();
    if (old) old->ReleaseReference();
    attr = o.attr;
    flags = (flags & ~3u) | (o.flags & 3u);
    manager = o.manager;
    anchor = o.anchor;
    fixedPixelSize = o.fixedPixelSize;
    pixelW = o.pixelW;
    pixelH = o.pixelH;
    return *this;
}

bool __cdecl SpriteInstance::Open(RenderKernel* kernel, const PString& name, const ResourceDatabase* db)  // 0x10003510
{
    const Resource* r = db->Get(name, 2);
    if (resource) resource->ReleaseReference();
    resource = const_cast<SpriteResource*>(static_cast<const SpriteResource*>(r));
    if (!resource) return false;
    bool ok = false;
    if (resource->Lock().code == 0) {
        ok = resource->Prepare(kernel);
        attr = resource->attr;
        resource->Unlock();
    }
    return ok;
}

void __cdecl SpriteInstance::SetVisible(bool v) { flags = (flags & ~1u) | (v ? 1u : 0u); }
bool __cdecl SpriteInstance::GetVisible() { return (flags & VISIBLE) != 0; }

void __cdecl SpriteInstance::SetPosition(const Vector2& p, PointPosition a)
{
    attr.position.x = p.x;
    attr.position.y = p.y;
    flags |= DIRTY;
    anchor = a;
}

Vector2 __cdecl SpriteInstance::GetPosition() { return Vector2(attr.position.x, attr.position.y); }

void __cdecl SpriteInstance::SetColor(float r, float g, float b)
{
    flags |= DIRTY;
    attr.color.r = r;
    attr.color.g = g;
    attr.color.b = b;
}

Color __cdecl SpriteInstance::GetColor() { return attr.color; }

void __cdecl SpriteInstance::SetOpacity(float a)
{
    flags |= DIRTY;
    attr.color.a = a;
}

float __cdecl SpriteInstance::GetOpacity() { return attr.color.a; }

void __cdecl SpriteInstance::SetScale(float s)
{
    flags |= DIRTY;
    attr.scale = s;
}

float __cdecl SpriteInstance::GetScale() { return attr.scale; }

void __cdecl SpriteInstance::SetRotation(float r)
{
    flags |= DIRTY;
    attr.rotation = r;
}

float __cdecl SpriteInstance::GetRotation() { return attr.rotation; }

void __cdecl SpriteInstance::SetPriority(float p)
{
    attr.position.z = p;
    if (manager) manager->managerFlags |= 1;
}

float __cdecl SpriteInstance::GetPriority() { return attr.position.z; }

void SpriteInstance::SetSize(const Vector2& s)
{
    attr.size = s;
    flags |= DIRTY;
}

const Vector2& SpriteInstance::GetSize() const { return attr.size; }

const Size& SpriteInstance::GetPixelSize() const  // the resource's UVI size
{
    return *reinterpret_cast<const Size*>(&resource->uvi.w);
}

void SpriteInstance::SetScreenSize(const Size& screen)
{
    const Size& p = GetPixelSize();
    attr.size.x = static_cast<float>(p.w) / static_cast<float>(screen.w);
    attr.size.y = static_cast<float>(p.h) / static_cast<float>(screen.h);
    flags |= DIRTY;
}

void __cdecl SpriteInstance::EnableFixedPixelSize(int w, int h)  // both divided by the screen height (kept)
{
    fixedPixelSize = true;
    const int screenH = manager->Kernel()->GetDisplay()->GetTargetSize().h;
    pixelW = w;
    pixelH = h;
    attr.size.x = static_cast<float>(w) / static_cast<float>(screenH);
    attr.size.y = static_cast<float>(h) / static_cast<float>(screenH);
    flags |= DIRTY;
}

void __cdecl SpriteInstance::DisableFixedPixelSize() { fixedPixelSize = false; }

void __cdecl SpriteInstance::SetUV(const Vector2& a, const Vector2& b)
{
    attr.uv[0] = a.x;
    attr.uv[1] = a.y;
    attr.uv[2] = b.x;
    attr.uv[3] = b.y;
    flags |= DIRTY;
}

void __cdecl SpriteInstance::GetUV(Vector2& a, Vector2& b) const
{
    a.x = attr.uv[0];
    a.y = attr.uv[1];
    b.x = attr.uv[2];
    b.y = attr.uv[3];
}

void __cdecl SpriteInstance::SetSpriteAlphaTestMode(bool on)
{
    flags |= DIRTY;
    attr.alphaTestSet = true;
    attr.alphaTestMode = on;
}

void __cdecl SpriteInstance::SetSpriteAlphaTestFunction(DFunction f, float reference)
{
    flags |= DIRTY;
    attr.alphaFunction = f;
    attr.alphaReference = reference;
}

void __cdecl SpriteInstance::SetSpriteAlphaBlendMode(bool on)
{
    flags |= DIRTY;
    attr.alphaBlendSet = true;
    attr.alphaBlendMode = on;
}

void __cdecl SpriteInstance::SetSpriteAlphaBlendFactor(Src::Factor src, Dst::Factor dst)
{
    flags |= DIRTY;
    attr.srcFactor = src;
    attr.dstFactor = dst;
}

bool __cdecl SpriteInstance::operator<(const SpriteInstance& o) { return attr.position.z < o.attr.position.z; }

int __cdecl SpriteInstance::PriorityCompare(const void* a, const void* b)  // higher priority first
{
    const float pa = (*static_cast<SpriteInstance* const*>(a))->attr.position.z;
    const float pb = (*static_cast<SpriteInstance* const*>(b))->attr.position.z;
    if (pa < pb) return 1;
    if (pb < pa) return -1;
    return 0;
}

void __cdecl SpriteInstance::Update(float) {}

void __cdecl SpriteInstance::Cull()
{
    if (flags & VISIBLE) TextureTouch(resource->texture);
}

// 0x100035e0: the four corners (counter-clockwise from the top left), rotated, aspect-corrected and placed
void __cdecl SpriteInstance::UpdateAttributes(RenderKernel* kernel)
{
    const float hw = (attr.size.x * 0.5f) * attr.scale;
    const float hh = (attr.size.y * 0.5f) * attr.scale;
    SpriteVertex* v = vertices;
    v[0].x = -0.0f - hw;
    v[3].x = -0.0f - hw;
    v[0].u = attr.uv[0];
    v[3].u = attr.uv[0];
    v[2].y = -0.0f - hh;
    v[3].y = -0.0f - hh;
    v[0].v = attr.uv[3];
    v[1].v = attr.uv[3];
    v[0].y = hh;
    v[0].z = 0.0f;
    v[1].x = hw;
    v[1].y = hh;
    v[1].z = 0.0f;
    v[2].x = hw;
    v[2].z = 0.0f;
    v[3].z = 0.0f;
    v[1].u = attr.uv[2];
    v[2].u = attr.uv[2];
    v[2].v = attr.uv[1];
    v[3].v = attr.uv[1];
    if (!(attr.rotation == 0.0f)) {
        float c, s;
        CosSin(attr.rotation, c, s);
        for (int i = 0; i < 4; ++i) {
            const float x = v[i].x, y = v[i].y;
            v[i].x = y * s + x * c;
            v[i].y = y * c - x * s;
        }
    }
    if (manager && (manager->managerFlags & 2)) {
        const Size& target = kernel->GetDisplay()->GetTargetSize();
        if (target.w > 0) {
            const float ratio = static_cast<float>(target.h) / static_cast<float>(target.w);
            for (int i = 0; i < 4; ++i) v[i].x = ratio * v[i].x;
        }
    }
    float ox = 0.0f, oy = 0.0f;
    if (anchor == POINT_CENTRE) {
        ox = attr.position.x + hw;
        oy = attr.position.y + hh;
    } else if (anchor == POINT_CORNER) {
        ox = attr.position.x;
        oy = attr.position.y;
    }
    for (int i = 0; i < 4; ++i) {
        v[i].x = v[i].x + ox;
        v[i].y = v[i].y + oy;
        v[i].color = Display::ConvertColor(attr.color);
    }
}

void __cdecl SpriteInstance::Render(RenderKernel* kernel, Texture*& current)  // 0x10003900
{
    if (!(flags & VISIBLE)) return;
    if (flags & DIRTY) {
        UpdateAttributes(kernel);
        flags &= ~DIRTY;
    }
    if (!attr.alphaTestSet && !attr.alphaBlendSet) {  // 0x10004b10
        if (resource->texture != current) {
            TextureSelect(resource->texture);
            current = resource->texture;
        }
        kernel->GetDisplay()->Draw(resource->declaration, 5, 2, vertices, 4);
        return;
    }
    Display* display = kernel->GetDisplay();
    bool oldTest = false;
    int oldFunction = DFUNCTION_ALWAYS;
    float oldReference = 0.0f;
    int oldSrc = Src::SRC_ONE, oldDst = Dst::DST_ONE;
    if (attr.alphaTestSet) {
        const unsigned char* st = display->State();
        oldTest = ((st[0xa0] >> 5) & 1) != 0;
        oldFunction = *reinterpret_cast<const int*>(st + 0xd0);
        oldReference = *reinterpret_cast<const float*>(st + 0xcc);
        display->SetAlphaTestMode(attr.alphaTestMode);
        display->SetAlphaTestFunction(attr.alphaFunction, attr.alphaReference);
    }
    if (attr.alphaBlendSet) {
        const unsigned char* st = display->State();
        oldSrc = *reinterpret_cast<const int*>(st + 0x68);
        oldDst = *reinterpret_cast<const int*>(st + 0x6c);
        display->SetAlphaBlendFunction(attr.srcFactor, attr.dstFactor);
    }
    SpriteResource* r = resource;
    if (r->texture != current) {
        TextureSelect(r->texture);
        current = r->texture;
    }
    kernel->GetDisplay()->Draw(r->declaration, 5, 2, vertices, 4);
    if (attr.alphaTestSet) {
        display->SetAlphaTestMode(oldTest);
        display->SetAlphaTestFunction(static_cast<DFunction>(oldFunction), oldReference);
    }
    if (attr.alphaBlendSet)
        display->SetAlphaBlendFunction(static_cast<Src::Factor>(oldSrc), static_cast<Dst::Factor>(oldDst));
}

// ---------------------------------------------------------------------------------------------------------------
// SpriteManager: an overlay drawing its sprites in priority order (higher first)

SpriteManager::SpriteManager(const PString& name, int priority, bool flag, bool adjustAspect)
    : Overlay(name, priority, flag), count(0), capacity(0), sprites(nullptr)
{
    overlayFlags |= 2;
    managerFlags = static_cast<unsigned char>((adjustAspect ? 2 : 0) | (managerFlags & 0xfc));
}

SpriteManager::SpriteManager(LayerGroup* group, const PString& name, int priority, bool flag, bool adjustAspect)
    : Overlay(group, name, priority, flag), count(0), capacity(0), sprites(nullptr)
{
    overlayFlags |= 2;
    managerFlags = static_cast<unsigned char>((adjustAspect ? 2 : 0) | (managerFlags & 0xfc));
}

void SpriteManager::Copy(const SpriteManager& o)  // 0x10003b90 (the sprites are shared, not copied)
{
    count = 0;
    if (!o.sprites) return;
    if (capacity < o.capacity) {
        if (sprites) Alloc::Delete(sprites);
        sprites = static_cast<SpriteInstance**>(Alloc::New(o.capacity * 4));
        capacity = o.capacity;
    }
    count = o.count;
    for (unsigned int i = 0; i < count; ++i) sprites[i] = o.sprites[i];
}

SpriteManager::SpriteManager(const SpriteManager& o) : Overlay(o), count(0), capacity(0), sprites(nullptr)
{
    Copy(o);
    managerFlags = static_cast<unsigned char>((managerFlags & ~3) | (o.managerFlags & 3));
}

SpriteManager& SpriteManager::operator=(const SpriteManager& o)
{
    Overlay::operator=(o);
    Copy(o);
    managerFlags = static_cast<unsigned char>((managerFlags & ~3) | (o.managerFlags & 3));
    return *this;
}

SpriteManager::~SpriteManager()
{
    count = 0;
    if (sprites) Alloc::Delete(sprites);
}

void __cdecl SpriteManager::Add(SpriteInstance* s)  // 0x10003ca0: grows in steps of 128
{
    if (count + 1 > capacity) {
        const unsigned int cap = ((count + 1 + 0x7f) >> 7) << 7;
        auto* p = static_cast<SpriteInstance**>(Alloc::New(cap * 4));
        if (sprites) {
            std::memcpy(p, sprites, count * 4);
            Alloc::Delete(sprites);
        }
        capacity = cap;
        sprites = p;
    }
    sprites[count++] = s;
    managerFlags |= 1;
    s->manager = this;
}

void __cdecl SpriteManager::Remove(SpriteInstance* s)
{
    for (unsigned int i = 0; i < count; ++i) {
        if (sprites[i] == s) {
            sprites[i]->manager = nullptr;
            --count;
            std::memmove(sprites + i, sprites + i + 1, (count - i) * 4);
            return;
        }
    }
}

void __cdecl SpriteManager::Clear() { count = 0; }

void __cdecl SpriteManager::Purge()  // deletes the sprites
{
    for (unsigned int i = 0; i < count; ++i)
        if (sprites[i]) VDelete(sprites[i]);
    count = 0;
}

void __cdecl SpriteManager::Cull()
{
    for (unsigned int i = 0; i < count; ++i) sprites[i]->Cull();
}

void __cdecl SpriteManager::Update(float dt)
{
    for (unsigned int i = 0; i < count; ++i) sprites[i]->Update(dt);
    for (unsigned int i = 0; i < count; ++i) sprites[i]->Cull();
}

void __cdecl SpriteManager::Render()
{
    Kernel()->GetDisplay()->SetCullMode(DCULL_NONE);
    if (managerFlags & 1) {
        if (count) std::qsort(sprites, count, 4, &SpriteInstance::PriorityCompare);
        managerFlags &= ~1;
    }
    Texture* current = nullptr;
    for (unsigned int i = 0; i < count; ++i) sprites[i]->Render(Kernel(), current);
}

void __cdecl SpriteManager::SetSortParticleArray() { managerFlags |= 1; }

void __cdecl SpriteManager::SetAdjustAspectRatio(bool on)
{
    managerFlags = static_cast<unsigned char>((managerFlags & ~2) | (on ? 2 : 0));
}

bool __cdecl SpriteManager::IsAdjustAspectRatio() const { return (managerFlags & 2) != 0; }

// ---------------------------------------------------------------------------------------------------------------
// the plugin table (0x100028a0)

extern "C" void __cdecl JetGetPluginsRender2DCore()
{
    new ResourceType<SpriteResource>(PString("sprite.txt"), UId("4C414E8A-BBDA-11d4-9AD7-0001025364B8"),
                                     UId("4C414E80-BBDA-11d4-9AD7-0001025364B8"));
}

}  // namespace Jet
