// SpriteResource ("sprite.txt", not exported): a texture and the default attributes of the sprites drawn with it.
//   Sprite { Texture <file>  UV <u0 v0 u1 v1>  UVI <x y w h in pixels>  Color  Size  Position  Rotation  Scale
//            AlphaBlendMode  AlphaTestMode  AlphaTestFunction  AlphaRef  SrcFactor  DstFactor }
#pragma once

#include <jet/Render2DCore.h>

namespace Jet {

class SpriteResource : public Resource {
public:
    SpriteResource(const PString& name, const ResourceDatabase* owner);  // 0x10003e50
    ~SpriteResource() override;

    // 0x100049f0, when an instance opens: loads the texture, makes the vertex declaration, turns UVI into UV
    bool Prepare(RenderKernel* kernel);

    Texture* texture;            // +0x5c held by reference
    SpriteAttributes attr;       // +0x60
    PString textureName;         // +0xb0
    Rect uvi;                    // +0xb4 (-1: the whole texture)
    int declaration;             // +0xc4

protected:
    Result __cdecl Load(const char* name) override;     // binary sprites: not supported
    Result __cdecl LoadTxt(const char* name) override;  // 0x10004010
    unsigned int __cdecl CalculateSize() const override { return sizeof(SpriteResource); }
};
JET_LAYOUT32(sizeof(SpriteResource) == 0xc8);

}  // namespace Jet
