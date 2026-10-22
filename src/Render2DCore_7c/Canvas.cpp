// Canvas and SelectionBox (Render2DCore 0x10001000..0x10002130, 0x10002a50..0x10003130), transcribed from the
// assembly; the float arithmetic is SSE scalar in the original, kept in its order.
#include <jet/Bitmap.h>
#include <jet/Render2DCore.h>

#include <cstring>

namespace Jet {

namespace {

// 0x10002070: rectangle intersection; an empty height clears x and an empty width clears y (kept)
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

// Texture's slots used here
void TextureCreate(Texture* t, Bitmap* b, DisplayFormat* f, RenderTextureConstruct* c)
{
    VCall<void>(t, 0x04, b, f, 0, 1, 0, 0, 1.0f, 1, 0, 0, 0, 1, 1, c);
}
void TextureUpload(Texture* t, DisplayFormat* f, Bitmap* b) { VCall<void>(t, 0x0c, 0, f, b, 0); }
void TextureUpdate(Texture* t, Rect* r, DisplayFormat* f, Bitmap* b) { VCall<void>(t, 0x10, 0, r, f, b, 0); }
void TextureBind(Texture* t) { VCall<void>(t, 0x20, 0); }

const int kSpriteFormat = 0x13;  // position, colour, one texture coordinate

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Canvas

Canvas::Canvas(const PString& name, int priority, bool flag) : Overlay(name, priority, flag)
{
    SetDefaultValues();
    declaration = 0;
}

Canvas::Canvas(LayerGroup* group, const PString& name, int priority, bool flag) : Overlay(group, name, priority, flag)
{
    SetDefaultValues();
    declaration = 0;
}

Canvas::Canvas(const Canvas& o) : Overlay(o)
{
    std::memcpy(static_cast<void*>(tiles), o.tiles, reinterpret_cast<const char*>(&o + 1) - reinterpret_cast<const char*>(o.tiles));
}

Canvas::~Canvas() { Destroy(); }

Canvas& Canvas::operator=(const Canvas& o)
{
    Overlay::operator=(o);
    std::memcpy(static_cast<void*>(tiles), o.tiles, reinterpret_cast<const char*>(&o + 1) - reinterpret_cast<const char*>(o.tiles));
    return *this;
}

void __cdecl Canvas::SetPosition(int px, int py)
{
    x = px;
    y = py;
}

void __cdecl Canvas::SetColor(float r, float g, float b)
{
    color.r = r;
    color.g = g;
    color.b = b;
}

void __cdecl Canvas::SetOpacity(float a) { color.a = a; }
const Color& __cdecl Canvas::GetColor() const { return color; }
float __cdecl Canvas::GetScale() const { return scale; }
void __cdecl Canvas::SetScale(float s) { scale = s; }

void __cdecl Canvas::SetDefaultValues()  // 0x10001680
{
    tileW = tileH = cols = rows = 0;
    x = y = 0;
    color.r = color.g = color.b = color.a = 1.0f;
    scale = 1.0f;
    std::memset(tiles, 0, sizeof tiles);
    std::memset(rects, 0, sizeof rects);
    std::memset(counts, 0, sizeof counts);
}

void __cdecl Canvas::Create(const Size& tileSize, const Rect& r)  // 0x10001720
{
    Display* display = Kernel()->GetDisplay();
    x = r.x;
    y = r.y;
    area = r;
    if (r.w < 1 || r.h < 1) {
        Group()->GetGroupViewRect(area);
        area.w -= x;
        area.h -= y;
    }
    tileW = tileSize.w > 0 ? tileSize.w : 0x100;
    tileH = tileSize.h > 0 ? tileSize.h : 0x100;
    cols = area.w / tileW + (area.w % tileW != 0 ? 1 : 0);
    rows = area.h / tileH + (area.h % tileH != 0 ? 1 : 0);
    const Size bitmapSize = {tileW, tileH};
    Bitmap blank(bitmapSize, Bitmap::FORMAT_ARGB32, nullptr);
    blank.Clear();
    DisplayFormat format(DFORMAT_DEFAULT);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            Texture* t = display->CreateTexture();
            tiles[col][row] = t;
            {
                RenderTextureConstruct construct;
                TextureCreate(t, &blank, &format, &construct);
            }
            TextureUpload(tiles[col][row], &format, &blank);
        }
    }
    if (declaration == 0) declaration = Kernel()->GetDisplay()->GetDeclaration(kSpriteFormat);
}

void __cdecl Canvas::Destroy()  // 0x10001990
{
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col)
            if (tiles[col][row]) VDelete(tiles[col][row]);
    SetDefaultValues();
}

// 0x10001480: for each tile, the parts of the rectangles in `list` inside it; more than 16 make the whole tile
void __cdecl Canvas::FindActiveList(const RectList& list)
{
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int& n = counts[col][row];
            n = 0;
            const Rect tileRect = {tileW * col, tileH * row, tileW, tileH};
            const Rect t = Intersect(area, tileRect);
            for (const RectList::Node* node = list.First(); node; node = node->Next()) {
                const Rect& r = node->r;
                const int dx = r.x > t.x ? (t.w - r.x) + t.x - 1 : (r.w - t.x) + r.x - 1;
                const int dy = r.y > t.y ? (t.h - r.y) + t.y - 1 : (r.h - t.y) + r.y - 1;
                if (dx < 0 || dy < 0) continue;
                if (n == PER_TILE) {
                    rects[col][row][0] = t;
                    n = 1;
                    break;
                }
                rects[col][row][n++] = Intersect(r, t);
            }
        }
    }
}

// 0x10001a80: uploads the dirty parts of `pixels` (32-bit, `pitch` pixels per row), then keeps `active` for Render
void __cdecl Canvas::Update(const RectList& dirty, const RectList& active, unsigned long* pixels, int pitch)
{
    FindActiveList(dirty);
    DisplayFormat format(DFORMAT_DEFAULT);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            for (int i = 0; i < counts[col][row]; ++i) {
                const Rect& r = rects[col][row][i];
                unsigned long* p = pixels + (r.y * pitch + r.x);
                Bitmap b;
                const Size s = {r.w, r.h};
                b.Init(s, pitch * 4, Bitmap::FORMAT_ARGB32, reinterpret_cast<unsigned char*>(p), 0);
                Rect local = {r.x - tileW * col, r.y - tileH * row, r.w, r.h};
                TextureUpdate(tiles[col][row], &local, &format, &b);
            }
        }
    }
    FindActiveList(active);
}

void __cdecl Canvas::Render()  // 0x10001c90
{
    Display* display = Kernel()->GetDisplay();
    const bool wire = (display->State()[0xa0] & 1) != 0;
    if (wire) display->SetWireFrameMode(false);
    const float offset = display->TexelOffset();
    display->SetCullMode(DCULL_NONE);
    const unsigned long c = Display::ConvertColor(color);
    display->SetScissorMode(false);
    SpriteVertex v[4];
    v[0].color = v[1].color = v[2].color = v[3].color = c;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const int n = counts[col][row];
            if (n == 0) continue;
            const int x0 = tileW * col, y0 = tileH * row;
            for (int i = 0; i < n; ++i) {
                const Rect& r = rects[col][row][i];
                const float tw = static_cast<float>(tileW), th = static_cast<float>(tileH);
                float u0 = static_cast<float>(r.x - x0) / tw;
                float v0 = static_cast<float>(r.y - y0) / th;
                float u1 = static_cast<float>(r.w) / tw + u0;
                float v1 = static_cast<float>(r.h) / th + v0;
                if (!(offset == 0.0f)) {
                    const float du = (1.0f / static_cast<float>(tileW)) * offset;
                    u0 = u0 - du;
                    u1 = u1 - du;
                    const float dv = (1.0f / static_cast<float>(tileH)) * offset;
                    v0 = v0 - dv;
                    v1 = v1 - dv;
                }
                const float X0 = static_cast<float>(r.x) * scale + static_cast<float>(x);
                const float Y0 = static_cast<float>(r.y) * scale + static_cast<float>(y);
                const float X1 = static_cast<float>(r.w) * scale + X0;
                const float Y1 = static_cast<float>(r.h) * scale + Y0;
                TextureBind(tiles[col][row]);
                v[0].x = X0, v[0].y = Y0, v[0].z = 0.0f, v[0].u = u0, v[0].v = v0;
                v[1].x = X1, v[1].y = Y0, v[1].z = 0.0f, v[1].u = u1, v[1].v = v0;
                v[2].x = X1, v[2].y = Y1, v[2].z = 0.0f, v[2].u = u1, v[2].v = v1;
                v[3].x = X0, v[3].y = Y1, v[3].z = 0.0f, v[3].u = u0, v[3].v = v1;
                display->Draw(declaration, 5, 2, v, 4);
            }
        }
    }
    if (wire) display->SetWireFrameMode(true);
}

// ---------------------------------------------------------------------------------------------------------------
// SelectionBox: a translucent box with a dark and a light outline

SelectionBox::SelectionBox(const PString& name, int priority, bool flag) : Overlay(name, priority, flag)
{
    dark = Color{0.0f, 0.0f, 0.0f, 0.8f};
    light = Color{1.0f, 1.0f, 1.0f, 0.8f};
    areaColor = Color{1.0f, 1.0f, 1.0f, 0.125f};
}

SelectionBox::SelectionBox(LayerGroup* group, const PString& name, int priority, bool flag)
    : Overlay(group, name, priority, flag)
{
    dark = Color{0.0f, 0.0f, 0.0f, 0.8f};
    light = Color{1.0f, 1.0f, 1.0f, 0.8f};
    areaColor = Color{1.0f, 1.0f, 1.0f, 0.125f};
}

SelectionBox::SelectionBox(const SelectionBox& o)
    : Overlay(o), rect(o.rect), dark(o.dark), light(o.light), areaColor(o.areaColor)
{
}

SelectionBox::~SelectionBox() {}

SelectionBox& SelectionBox::operator=(const SelectionBox& o)
{
    Overlay::operator=(o);
    rect = o.rect;
    dark = o.dark;
    light = o.light;
    areaColor = o.areaColor;
    return *this;
}

void SelectionBox::SetRect(const Rect& r) { rect = r; }
const Rect& SelectionBox::GetRect() const { return rect; }
void SelectionBox::SetAreaColor(const Color& c) { areaColor = c; }
void SelectionBox::SetDarkColor(const Color& c) { dark = c; }
void SelectionBox::SetLightColor(const Color& c) { light = c; }

void __cdecl SelectionBox::Render()  // 0x10002d60
{
    Display* display = Kernel()->GetDisplay();
    const float L = static_cast<float>(rect.x) + 0.375f;
    const float T = static_cast<float>(rect.y) + 0.375f;
    const float R = static_cast<float>(rect.w) + L;
    const float B = static_cast<float>(rect.h) + T;
    display->SetTexture(0);
    SpriteVertex v[8];
    std::memset(v, 0, sizeof v);
    auto set = [&v](int i, float px, float py, unsigned long c) {
        v[i].x = px;
        v[i].y = py;
        v[i].z = 0.0f;
        v[i].color = c;
    };
    // the inside: a quad 2 pixels in
    unsigned long c = Display::ConvertColor(areaColor);
    const float l2 = L + 2.0f, t2 = T + 2.0f, r2 = R - 2.0f, b2 = B - 2.0f;
    set(0, l2, t2, c);
    set(1, r2, t2, c);
    set(2, r2, b2, c);
    set(3, l2, b2, c);
    display->DrawUser(kSpriteFormat, 5, 2, v, 4);
    // the dark outline, 1 pixel in, as four lines
    c = Display::ConvertColor(dark);
    const float l1 = L + 1.0f, t1 = T + 1.0f, r1 = R - 1.0f, b1 = B - 1.0f;
    set(0, l1, t1, c);
    set(1, r1, t1, c);
    set(2, r1, t1, c);
    set(3, r1, b1, c);
    set(4, r1, b1, c);
    set(5, l1, b1, c);
    set(6, l1, b1, c);
    set(7, l1, t1, c);
    display->DrawUser(kSpriteFormat, 1, 4, v, 8);
    // the light outline on the edge (its far sides 2 pixels in)
    c = Display::ConvertColor(light);
    set(0, L, T, c);
    set(1, r2, T, c);
    set(2, r2, T, c);
    set(3, r2, b2, c);
    set(4, r2, b2, c);
    set(5, L, b2, c);
    set(6, L, b2, c);
    set(7, L, T, c);
    display->DrawUser(kSpriteFormat, 1, 4, v, 8);
}

}  // namespace Jet
