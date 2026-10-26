// RenderSystem_7c unit U1 (notes/RenderSystem.md): the packed, LOD, terrain and streamed vertex records. Each one is a
// fixed layout of floats (bone weights of the Wb4I formats are packed bytes) with its type id, its size and the offset
// table of its vertex items (-1: not present). Generated from the TS12 assembly and data, one struct per record;
// accessors return pointers into the record the way the original's inline members do.
#pragma once

#include <jet/Math.h>
#include <jet/Platform.h>

namespace Jet {

enum VertexItemType { VIT_UNDEFINED = 0, VIT_POSITION = 1 };
const int kVertexItemTypes = 39;
enum VertexTypeID {};

// an item's offset and the item type the record answers for it
struct OffsetLookupCheck {
    int offset;
    int type;
};

// a streamed record's stream, offset and item type
struct CheckVertexItemTypeInt {
    int stream;
    int offset;
    int type;
};

// byte and float access into a record
template <class T>
struct VertexRecord {
protected:
    char* Bytes(int offset) { return reinterpret_cast<char*>(this) + offset; }
    const char* Bytes(int offset) const { return reinterpret_cast<const char*>(this) + offset; }
    float* Floats(int offset) { return reinterpret_cast<float*>(Bytes(offset)); }
};

struct GeoLODMorphVertex_at3Blend : VertexRecord<GeoLODMorphVertex_at3Blend> {
    GeoLODMorphVertex_at3Blend();
    GeoLODMorphVertex_at3Blend& operator=(const GeoLODMorphVertex_at3Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_at3Blend) == 52);

struct GeoLODMorphVertex_at4Blend : VertexRecord<GeoLODMorphVertex_at4Blend> {
    GeoLODMorphVertex_at4Blend();
    GeoLODMorphVertex_at4Blend& operator=(const GeoLODMorphVertex_at4Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[15];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_at4Blend) == 60);

struct GeoLODMorphVertex_t1 : VertexRecord<GeoLODMorphVertex_t1> {
    GeoLODMorphVertex_t1();
    GeoLODMorphVertex_t1& operator=(const GeoLODMorphVertex_t1& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t1) == 52);

struct GeoLODMorphVertex_t1Blend : VertexRecord<GeoLODMorphVertex_t1Blend> {
    GeoLODMorphVertex_t1Blend();
    GeoLODMorphVertex_t1Blend& operator=(const GeoLODMorphVertex_t1Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float __cdecl GetMorphIndex();
    float* __cdecl GetMorphedPos();
    float* __cdecl GetMorphedTex(int i);
    float* __cdecl GetOriginalPos();
    float* __cdecl GetOriginalTex(int i);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t1Blend) == 52);

struct GeoLODMorphVertex_t1Blendz : VertexRecord<GeoLODMorphVertex_t1Blendz> {
    GeoLODMorphVertex_t1Blendz();
    GeoLODMorphVertex_t1Blendz& operator=(const GeoLODMorphVertex_t1Blendz& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float __cdecl GetMorphIndex();
    float* __cdecl GetMorphedPos();
    float* __cdecl GetMorphedTex(int i);
    float* __cdecl GetOriginalPos();
    float* __cdecl GetOriginalTex(int i);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[15];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t1Blendz) == 60);

struct GeoLODMorphVertex_t2 : VertexRecord<GeoLODMorphVertex_t2> {
    GeoLODMorphVertex_t2();
    GeoLODMorphVertex_t2& operator=(const GeoLODMorphVertex_t2& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[17];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t2) == 68);

struct GeoLODMorphVertex_t2Blend : VertexRecord<GeoLODMorphVertex_t2Blend> {
    GeoLODMorphVertex_t2Blend();
    GeoLODMorphVertex_t2Blend& operator=(const GeoLODMorphVertex_t2Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[19];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t2Blend) == 76);

struct GeoLODMorphVertex_t2Blendz : VertexRecord<GeoLODMorphVertex_t2Blendz> {
    GeoLODMorphVertex_t2Blendz();
    GeoLODMorphVertex_t2Blendz& operator=(const GeoLODMorphVertex_t2Blendz& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[21];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t2Blendz) == 84);

struct GeoLODMorphVertex_t3 : VertexRecord<GeoLODMorphVertex_t3> {
    GeoLODMorphVertex_t3();
    GeoLODMorphVertex_t3& operator=(const GeoLODMorphVertex_t3& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[21];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t3) == 84);

struct GeoLODMorphVertex_t3Blend : VertexRecord<GeoLODMorphVertex_t3Blend> {
    GeoLODMorphVertex_t3Blend();
    GeoLODMorphVertex_t3Blend& operator=(const GeoLODMorphVertex_t3Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[25];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t3Blend) == 100);

struct GeoLODMorphVertex_t3Blendz : VertexRecord<GeoLODMorphVertex_t3Blendz> {
    GeoLODMorphVertex_t3Blendz();
    GeoLODMorphVertex_t3Blendz& operator=(const GeoLODMorphVertex_t3Blendz& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[27];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t3Blendz) == 108);

struct GeoLODMorphVertex_t4 : VertexRecord<GeoLODMorphVertex_t4> {
    GeoLODMorphVertex_t4();
    GeoLODMorphVertex_t4& operator=(const GeoLODMorphVertex_t4& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[25];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t4) == 100);

struct GeoLODMorphVertex_t4Blend : VertexRecord<GeoLODMorphVertex_t4Blend> {
    GeoLODMorphVertex_t4Blend();
    GeoLODMorphVertex_t4Blend& operator=(const GeoLODMorphVertex_t4Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[31];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t4Blend) == 124);

struct GeoLODMorphVertex_t4Blendz : VertexRecord<GeoLODMorphVertex_t4Blendz> {
    GeoLODMorphVertex_t4Blendz();
    GeoLODMorphVertex_t4Blendz& operator=(const GeoLODMorphVertex_t4Blendz& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[33];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_t4Blendz) == 132);

struct GeoLODVertex_at3Blend : VertexRecord<GeoLODVertex_at3Blend> {
    GeoLODVertex_at3Blend();
    GeoLODVertex_at3Blend& operator=(const GeoLODVertex_at3Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[6];
};
JET_LAYOUT32(sizeof(GeoLODVertex_at3Blend) == 24);

struct GeoLODVertex_at4Blend : VertexRecord<GeoLODVertex_at4Blend> {
    GeoLODVertex_at4Blend();
    GeoLODVertex_at4Blend& operator=(const GeoLODVertex_at4Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[7];
};
JET_LAYOUT32(sizeof(GeoLODVertex_at4Blend) == 28);

struct GeoLODVertex_t1 : VertexRecord<GeoLODVertex_t1> {
    GeoLODVertex_t1();
    GeoLODVertex_t1& operator=(const GeoLODVertex_t1& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[6];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t1) == 24);

struct GeoLODVertex_t1Blend : VertexRecord<GeoLODVertex_t1Blend> {
    GeoLODVertex_t1Blend();
    GeoLODVertex_t1Blend& operator=(const GeoLODVertex_t1Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float* __cdecl GetMorphedTexWeight();
    float* __cdecl GetOriginalPos();
    float* __cdecl GetOriginalTex(int i);
    float* __cdecl GetOriginalTexWeight();
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[6];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t1Blend) == 24);

struct GeoLODVertex_t1Blendz : VertexRecord<GeoLODVertex_t1Blendz> {
    GeoLODVertex_t1Blendz();
    GeoLODVertex_t1Blendz& operator=(const GeoLODVertex_t1Blendz& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float* __cdecl GetMorphedTexWeight();
    float* __cdecl GetOriginalPos();
    float* __cdecl GetOriginalTex(int i);
    float* __cdecl GetOriginalTexWeight();
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[7];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t1Blendz) == 28);

struct GeoLODVertex_t2 : VertexRecord<GeoLODVertex_t2> {
    GeoLODVertex_t2();
    GeoLODVertex_t2& operator=(const GeoLODVertex_t2& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t2) == 32);

struct GeoLODVertex_t2Blend : VertexRecord<GeoLODVertex_t2Blend> {
    GeoLODVertex_t2Blend();
    GeoLODVertex_t2Blend& operator=(const GeoLODVertex_t2Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t2Blend) == 36);

struct GeoLODVertex_t2Blendz : VertexRecord<GeoLODVertex_t2Blendz> {
    GeoLODVertex_t2Blendz();
    GeoLODVertex_t2Blendz& operator=(const GeoLODVertex_t2Blendz& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t2Blendz) == 40);

struct GeoLODVertex_t3 : VertexRecord<GeoLODVertex_t3> {
    GeoLODVertex_t3();
    GeoLODVertex_t3& operator=(const GeoLODVertex_t3& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t3) == 40);

struct GeoLODVertex_t3Blend : VertexRecord<GeoLODVertex_t3Blend> {
    GeoLODVertex_t3Blend();
    GeoLODVertex_t3Blend& operator=(const GeoLODVertex_t3Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t3Blend) == 48);

struct GeoLODVertex_t3Blendz : VertexRecord<GeoLODVertex_t3Blendz> {
    GeoLODVertex_t3Blendz();
    GeoLODVertex_t3Blendz& operator=(const GeoLODVertex_t3Blendz& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t3Blendz) == 52);

struct GeoLODVertex_t4 : VertexRecord<GeoLODVertex_t4> {
    GeoLODVertex_t4();
    GeoLODVertex_t4& operator=(const GeoLODVertex_t4& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t4) == 48);

struct GeoLODVertex_t4Blend : VertexRecord<GeoLODVertex_t4Blend> {
    GeoLODVertex_t4Blend();
    GeoLODVertex_t4Blend& operator=(const GeoLODVertex_t4Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[15];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t4Blend) == 60);

struct GeoLODVertex_t4Blendz : VertexRecord<GeoLODVertex_t4Blendz> {
    GeoLODVertex_t4Blendz();
    GeoLODVertex_t4Blendz& operator=(const GeoLODVertex_t4Blendz& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[16];
};
JET_LAYOUT32(sizeof(GeoLODVertex_t4Blendz) == 64);

struct PackedVertexA12 : VertexRecord<PackedVertexA12> {
    PackedVertexA12();
    PackedVertexA12& operator=(const PackedVertexA12& o);
    Vector2& __cdecl GetAttrib1();
    const Vector2& __cdecl GetAttrib1() const;
    void __cdecl SetAttrib1(const Vector2& p);

    float v[2];
};
JET_LAYOUT32(sizeof(PackedVertexA12) == 8);

struct PackedVertexO2 : VertexRecord<PackedVertexO2> {
    PackedVertexO2();
    PackedVertexO2& operator=(const PackedVertexO2& o);
    Vector2& __cdecl GetOffset();
    const Vector2& __cdecl GetOffset() const;
    void __cdecl SetOffset(const Vector2& p);

    float v[2];
};
JET_LAYOUT32(sizeof(PackedVertexO2) == 8);

struct PackedVertexPFX_OLVT : VertexRecord<PackedVertexPFX_OLVT> {
    PackedVertexPFX_OLVT();
    Vector2& __cdecl operator=(const Vector2& p);
    PackedVertexPFX_OLVT& operator=(const PackedVertexPFX_OLVT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(PackedVertexPFX_OLVT) == 40);

struct PackedVertexPTA12 : VertexRecord<PackedVertexPTA12> {
    PackedVertexPTA12();
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPTA12& operator=(const PackedVertexPTA12& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[7];
};
JET_LAYOUT32(sizeof(PackedVertexPTA12) == 28);

struct PackedVertexV3 : VertexRecord<PackedVertexV3> {
    PackedVertexV3();
    PackedVertexV3& operator=(const PackedVertexV3& o);
    Vector3& __cdecl GetVelocity();
    const Vector3& __cdecl GetVelocity() const;
    void __cdecl SetVelocity(const Vector3& p);

    float v[3];
};
JET_LAYOUT32(sizeof(PackedVertexV3) == 12);

struct StreamedVertexN : VertexRecord<StreamedVertexN> {
    StreamedVertexN();
    StreamedVertexN& operator=(const StreamedVertexN& o);

    float v[3];
};
JET_LAYOUT32(sizeof(StreamedVertexN) == 12);

struct StreamedVertexP : VertexRecord<StreamedVertexP> {
    StreamedVertexP();
    StreamedVertexP& operator=(const StreamedVertexP& o);

    float v[3];
};
JET_LAYOUT32(sizeof(StreamedVertexP) == 12);

struct StreamedVertexT : VertexRecord<StreamedVertexT> {
    StreamedVertexT();
    StreamedVertexT& operator=(const StreamedVertexT& o);

    float v[2];
};
JET_LAYOUT32(sizeof(StreamedVertexT) == 8);

struct TerrainBlend2t2Vertex : VertexRecord<TerrainBlend2t2Vertex> {
    TerrainBlend2t2Vertex();
    TerrainBlend2t2Vertex& operator=(const TerrainBlend2t2Vertex& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float* __cdecl GetOriginalPos();
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(TerrainBlend2t2Vertex) == 32);

struct TerrainBlend3t2Vertex : VertexRecord<TerrainBlend3t2Vertex> {
    TerrainBlend3t2Vertex();
    TerrainBlend3t2Vertex& operator=(const TerrainBlend3t2Vertex& o);
    static VertexTypeID __cdecl GetVertexTypeID();

    float v[8];
};
JET_LAYOUT32(sizeof(TerrainBlend3t2Vertex) == 32);

struct TerrainBlend3t3Vertex : VertexRecord<TerrainBlend3t3Vertex> {
    TerrainBlend3t3Vertex();
    TerrainBlend3t3Vertex& operator=(const TerrainBlend3t3Vertex& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(TerrainBlend3t3Vertex) == 40);

struct TerrainBlend4t2Vertex : VertexRecord<TerrainBlend4t2Vertex> {
    TerrainBlend4t2Vertex();
    TerrainBlend4t2Vertex& operator=(const TerrainBlend4t2Vertex& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(TerrainBlend4t2Vertex) == 36);

struct TerrainBlend4t4Vertex : VertexRecord<TerrainBlend4t4Vertex> {
    TerrainBlend4t4Vertex();
    TerrainBlend4t4Vertex& operator=(const TerrainBlend4t4Vertex& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(TerrainBlend4t4Vertex) == 52);

struct TerrainBlendt4Vertex : VertexRecord<TerrainBlendt4Vertex> {
    TerrainBlendt4Vertex();
    TerrainBlendt4Vertex& operator=(const TerrainBlendt4Vertex& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(TerrainBlendt4Vertex) == 48);

struct GeoLODMorphVertex : VertexRecord<GeoLODMorphVertex> {
    GeoLODMorphVertex& operator=(const GeoLODMorphVertex& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float __cdecl GetMorphIndex();
    float* __cdecl GetMorphedPos();
    float* __cdecl GetOriginalPos();
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex) == 36);

struct GeoLODMorphVertex_at1Blend : VertexRecord<GeoLODMorphVertex_at1Blend> {
    GeoLODMorphVertex_at1Blend& operator=(const GeoLODMorphVertex_at1Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float __cdecl GetMorphIndex();
    float* __cdecl GetMorphedPos();
    float* __cdecl GetOriginalPos();
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_at1Blend) == 36);

struct GeoLODMorphVertex_at2Blend : VertexRecord<GeoLODMorphVertex_at2Blend> {
    GeoLODMorphVertex_at2Blend& operator=(const GeoLODMorphVertex_at2Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[11];
};
JET_LAYOUT32(sizeof(GeoLODMorphVertex_at2Blend) == 44);

struct GeoLODVertex : VertexRecord<GeoLODVertex> {
    GeoLODVertex& operator=(const GeoLODVertex& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float* __cdecl GetOriginalPos();
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[4];
};
JET_LAYOUT32(sizeof(GeoLODVertex) == 16);

struct GeoLODVertex_at1Blend : VertexRecord<GeoLODVertex_at1Blend> {
    GeoLODVertex_at1Blend& operator=(const GeoLODVertex_at1Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    float* __cdecl GetMorphedTexWeight();
    float* __cdecl GetOriginalPos();
    float* __cdecl GetOriginalTexWeight();
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[4];
};
JET_LAYOUT32(sizeof(GeoLODVertex_at1Blend) == 16);

struct GeoLODVertex_at2Blend : VertexRecord<GeoLODVertex_at2Blend> {
    GeoLODVertex_at2Blend& operator=(const GeoLODVertex_at2Blend& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetReadDataOffset();
    static int __cdecl GetReadDataSize();
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int readDataOffset;
    static const int readDataSize;
    static const int vertexItemOffset[kVertexItemTypes];

    float v[5];
};
JET_LAYOUT32(sizeof(GeoLODVertex_at2Blend) == 20);

struct PackedVertexC : VertexRecord<PackedVertexC> {
    PackedVertexC& operator=(const PackedVertexC& o);

    float v[1];
};
JET_LAYOUT32(sizeof(PackedVertexC) == 4);

struct PackedVertexL3 : VertexRecord<PackedVertexL3> {
    PackedVertexL3& operator=(const PackedVertexL3& o);
    Vector3& __cdecl GetLifeData();
    const Vector3& __cdecl GetLifeData() const;
    void __cdecl SetLifeData(const Vector3& p);

    float v[3];
};
JET_LAYOUT32(sizeof(PackedVertexL3) == 12);

struct PackedVertexN : VertexRecord<PackedVertexN> {
    PackedVertexN& operator=(const PackedVertexN& o);
    Vector3& __cdecl GetNormal();
    const Vector3& __cdecl GetNormal() const;
    void __cdecl SetNormal(const Vector3& p);

    float v[3];
};
JET_LAYOUT32(sizeof(PackedVertexN) == 12);

struct PackedVertexP4 : VertexRecord<PackedVertexP4> {
    Vector4& __cdecl operator=(const Vector4& p);
    PackedVertexP4& operator=(const PackedVertexP4& o);
    __cdecl operator Vector4&();
    __cdecl operator Vector4*();
    __cdecl operator const Vector4&() const;
    __cdecl operator const Vector4*() const;
    static int __cdecl GetItemOffset(VertexItemType type);
    Vector4& __cdecl GetPosition();
    const Vector4& __cdecl GetPosition() const;
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    void __cdecl SetPosition(const Vector3& p, float w);
    void __cdecl SetPosition(const Vector4& p);
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[4];
};
JET_LAYOUT32(sizeof(PackedVertexP4) == 16);

struct PackedVertexP4Wb4IN : VertexRecord<PackedVertexP4Wb4IN> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexP4Wb4IN& operator=(const PackedVertexP4Wb4IN& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(PackedVertexP4Wb4IN) == 36);

struct PackedVertexP : VertexRecord<PackedVertexP> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexP& operator=(const PackedVertexP& o);
    __cdecl operator Vector3&();
    __cdecl operator Vector3*();
    __cdecl operator const Vector3&() const;
    __cdecl operator const Vector3*() const;
    static int __cdecl GetItemOffset(VertexItemType type);
    Vector3& __cdecl GetPosition();
    const Vector3& __cdecl GetPosition() const;
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    void __cdecl SetPosition(const Vector3& p);
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[3];
};
JET_LAYOUT32(sizeof(PackedVertexP) == 12);

struct PackedVertexPC : VertexRecord<PackedVertexPC> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPC& operator=(const PackedVertexPC& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[4];
};
JET_LAYOUT32(sizeof(PackedVertexPC) == 16);

struct PackedVertexPCNT : VertexRecord<PackedVertexPCNT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPCNT& operator=(const PackedVertexPCNT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(PackedVertexPCNT) == 36);

struct PackedVertexPCNTA : VertexRecord<PackedVertexPCNTA> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPCNTA& operator=(const PackedVertexPCNTA& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(PackedVertexPCNTA) == 48);

struct PackedVertexPCT : VertexRecord<PackedVertexPCT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPCT& operator=(const PackedVertexPCT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[6];
};
JET_LAYOUT32(sizeof(PackedVertexPCT) == 24);

struct PackedVertexPCTT : VertexRecord<PackedVertexPCTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPCTT& operator=(const PackedVertexPCTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(PackedVertexPCTT) == 32);

struct PackedVertexPN : VertexRecord<PackedVertexPN> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPN& operator=(const PackedVertexPN& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[6];
};
JET_LAYOUT32(sizeof(PackedVertexPN) == 24);

struct PackedVertexPNT : VertexRecord<PackedVertexPNT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPNT& operator=(const PackedVertexPNT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(PackedVertexPNT) == 32);

struct PackedVertexPNTA : VertexRecord<PackedVertexPNTA> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPNTA& operator=(const PackedVertexPNTA& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[11];
};
JET_LAYOUT32(sizeof(PackedVertexPNTA) == 44);

struct PackedVertexPNTAI : VertexRecord<PackedVertexPNTAI> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPNTAI& operator=(const PackedVertexPNTAI& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(PackedVertexPNTAI) == 48);

struct PackedVertexPNTT : VertexRecord<PackedVertexPNTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPNTT& operator=(const PackedVertexPNTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(PackedVertexPNTT) == 40);

struct PackedVertexPNTTA : VertexRecord<PackedVertexPNTTA> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPNTTA& operator=(const PackedVertexPNTTA& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(PackedVertexPNTTA) == 52);

struct PackedVertexPT : VertexRecord<PackedVertexPT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPT& operator=(const PackedVertexPT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[5];
};
JET_LAYOUT32(sizeof(PackedVertexPT) == 20);

struct PackedVertexPW1IC : VertexRecord<PackedVertexPW1IC> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW1IC& operator=(const PackedVertexPW1IC& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[6];
};
JET_LAYOUT32(sizeof(PackedVertexPW1IC) == 24);

struct PackedVertexPW1ICT : VertexRecord<PackedVertexPW1ICT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW1ICT& operator=(const PackedVertexPW1ICT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(PackedVertexPW1ICT) == 32);

struct PackedVertexPW1ICTT : VertexRecord<PackedVertexPW1ICTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW1ICTT& operator=(const PackedVertexPW1ICTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(PackedVertexPW1ICTT) == 40);

struct PackedVertexPW1IN : VertexRecord<PackedVertexPW1IN> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW1IN& operator=(const PackedVertexPW1IN& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(PackedVertexPW1IN) == 32);

struct PackedVertexPW1INT : VertexRecord<PackedVertexPW1INT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW1INT& operator=(const PackedVertexPW1INT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(PackedVertexPW1INT) == 40);

struct PackedVertexPW1INTT : VertexRecord<PackedVertexPW1INTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW1INTT& operator=(const PackedVertexPW1INTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(PackedVertexPW1INTT) == 48);

struct PackedVertexPW2IC : VertexRecord<PackedVertexPW2IC> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW2IC& operator=(const PackedVertexPW2IC& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[7];
};
JET_LAYOUT32(sizeof(PackedVertexPW2IC) == 28);

struct PackedVertexPW2ICT : VertexRecord<PackedVertexPW2ICT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW2ICT& operator=(const PackedVertexPW2ICT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(PackedVertexPW2ICT) == 36);

struct PackedVertexPW2ICTT : VertexRecord<PackedVertexPW2ICTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW2ICTT& operator=(const PackedVertexPW2ICTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[11];
};
JET_LAYOUT32(sizeof(PackedVertexPW2ICTT) == 44);

struct PackedVertexPW2IN : VertexRecord<PackedVertexPW2IN> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW2IN& operator=(const PackedVertexPW2IN& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(PackedVertexPW2IN) == 36);

struct PackedVertexPW2INT : VertexRecord<PackedVertexPW2INT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW2INT& operator=(const PackedVertexPW2INT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[11];
};
JET_LAYOUT32(sizeof(PackedVertexPW2INT) == 44);

struct PackedVertexPW2INTT : VertexRecord<PackedVertexPW2INTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW2INTT& operator=(const PackedVertexPW2INTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(PackedVertexPW2INTT) == 52);

struct PackedVertexPW3IC : VertexRecord<PackedVertexPW3IC> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW3IC& operator=(const PackedVertexPW3IC& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(PackedVertexPW3IC) == 32);

struct PackedVertexPW3ICT : VertexRecord<PackedVertexPW3ICT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW3ICT& operator=(const PackedVertexPW3ICT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(PackedVertexPW3ICT) == 40);

struct PackedVertexPW3ICTT : VertexRecord<PackedVertexPW3ICTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW3ICTT& operator=(const PackedVertexPW3ICTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(PackedVertexPW3ICTT) == 48);

struct PackedVertexPW3IN : VertexRecord<PackedVertexPW3IN> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW3IN& operator=(const PackedVertexPW3IN& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(PackedVertexPW3IN) == 40);

struct PackedVertexPW3INT : VertexRecord<PackedVertexPW3INT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW3INT& operator=(const PackedVertexPW3INT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(PackedVertexPW3INT) == 48);

struct PackedVertexPW3INTT : VertexRecord<PackedVertexPW3INTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW3INTT& operator=(const PackedVertexPW3INTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[14];
};
JET_LAYOUT32(sizeof(PackedVertexPW3INTT) == 56);

struct PackedVertexPW4IC : VertexRecord<PackedVertexPW4IC> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW4IC& operator=(const PackedVertexPW4IC& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(PackedVertexPW4IC) == 36);

struct PackedVertexPW4ICT : VertexRecord<PackedVertexPW4ICT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW4ICT& operator=(const PackedVertexPW4ICT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[11];
};
JET_LAYOUT32(sizeof(PackedVertexPW4ICT) == 44);

struct PackedVertexPW4ICTT : VertexRecord<PackedVertexPW4ICTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW4ICTT& operator=(const PackedVertexPW4ICTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(PackedVertexPW4ICTT) == 52);

struct PackedVertexPW4IN : VertexRecord<PackedVertexPW4IN> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW4IN& operator=(const PackedVertexPW4IN& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[11];
};
JET_LAYOUT32(sizeof(PackedVertexPW4IN) == 44);

struct PackedVertexPW4INT : VertexRecord<PackedVertexPW4INT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW4INT& operator=(const PackedVertexPW4INT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(PackedVertexPW4INT) == 52);

struct PackedVertexPW4INTT : VertexRecord<PackedVertexPW4INTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPW4INTT& operator=(const PackedVertexPW4INTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[15];
};
JET_LAYOUT32(sizeof(PackedVertexPW4INTT) == 60);

struct PackedVertexPWC : VertexRecord<PackedVertexPWC> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWC& operator=(const PackedVertexPWC& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[5];
};
JET_LAYOUT32(sizeof(PackedVertexPWC) == 20);

struct PackedVertexPWCT : VertexRecord<PackedVertexPWCT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWCT& operator=(const PackedVertexPWCT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[7];
};
JET_LAYOUT32(sizeof(PackedVertexPWCT) == 28);

struct PackedVertexPWCTT : VertexRecord<PackedVertexPWCTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWCTT& operator=(const PackedVertexPWCTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(PackedVertexPWCTT) == 36);

struct PackedVertexPWN : VertexRecord<PackedVertexPWN> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWN& operator=(const PackedVertexPWN& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[7];
};
JET_LAYOUT32(sizeof(PackedVertexPWN) == 28);

struct PackedVertexPWNT : VertexRecord<PackedVertexPWNT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWNT& operator=(const PackedVertexPWNT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[9];
};
JET_LAYOUT32(sizeof(PackedVertexPWNT) == 36);

struct PackedVertexPWNTA : VertexRecord<PackedVertexPWNTA> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWNTA& operator=(const PackedVertexPWNTA& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(PackedVertexPWNTA) == 48);

struct PackedVertexPWNTAI : VertexRecord<PackedVertexPWNTAI> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWNTAI& operator=(const PackedVertexPWNTAI& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[13];
};
JET_LAYOUT32(sizeof(PackedVertexPWNTAI) == 52);

struct PackedVertexPWNTT : VertexRecord<PackedVertexPWNTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWNTT& operator=(const PackedVertexPWNTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[11];
};
JET_LAYOUT32(sizeof(PackedVertexPWNTT) == 44);

struct PackedVertexPWNTTA : VertexRecord<PackedVertexPWNTTA> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWNTTA& operator=(const PackedVertexPWNTTA& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[14];
};
JET_LAYOUT32(sizeof(PackedVertexPWNTTA) == 56);

struct PackedVertexPWb4IC : VertexRecord<PackedVertexPWb4IC> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWb4IC& operator=(const PackedVertexPWb4IC& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[6];
};
JET_LAYOUT32(sizeof(PackedVertexPWb4IC) == 24);

struct PackedVertexPWb4ICNT : VertexRecord<PackedVertexPWb4ICNT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWb4ICNT& operator=(const PackedVertexPWb4ICNT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[11];
};
JET_LAYOUT32(sizeof(PackedVertexPWb4ICNT) == 44);

struct PackedVertexPWb4ICT : VertexRecord<PackedVertexPWb4ICT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWb4ICT& operator=(const PackedVertexPWb4ICT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(PackedVertexPWb4ICT) == 32);

struct PackedVertexPWb4ICTT : VertexRecord<PackedVertexPWb4ICTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWb4ICTT& operator=(const PackedVertexPWb4ICTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(PackedVertexPWb4ICTT) == 40);

struct PackedVertexPWb4IN : VertexRecord<PackedVertexPWb4IN> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWb4IN& operator=(const PackedVertexPWb4IN& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[8];
};
JET_LAYOUT32(sizeof(PackedVertexPWb4IN) == 32);

struct PackedVertexPWb4INT : VertexRecord<PackedVertexPWb4INT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWb4INT& operator=(const PackedVertexPWb4INT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[10];
};
JET_LAYOUT32(sizeof(PackedVertexPWb4INT) == 40);

struct PackedVertexPWb4INTT : VertexRecord<PackedVertexPWb4INTT> {
    Vector3& __cdecl operator=(const Vector3& p);
    PackedVertexPWb4INTT& operator=(const PackedVertexPWb4INTT& o);
    static int __cdecl GetItemOffset(VertexItemType type);
    static int __cdecl GetVertexSize();
    static VertexTypeID __cdecl GetVertexTypeID();
    static const OffsetLookupCheck vertexItemOffset[kVertexItemTypes];

    float v[12];
};
JET_LAYOUT32(sizeof(PackedVertexPWb4INTT) == 48);

struct PackedVertexT : VertexRecord<PackedVertexT> {
    PackedVertexT& operator=(const PackedVertexT& o);
    Vector2& __cdecl GetTexCoord();
    const Vector2& __cdecl GetTexCoord() const;
    void __cdecl SetTexCoord(const Vector2& p);

    float v[2];
};
JET_LAYOUT32(sizeof(PackedVertexT) == 8);

struct PackedVertexTT : VertexRecord<PackedVertexTT> {
    PackedVertexTT& operator=(const PackedVertexTT& o);
    Vector2& __cdecl GetTexCoord2();
    const Vector2& __cdecl GetTexCoord2() const;
    void __cdecl SetTexCoord2(const Vector2& p);

    float v[2];
};
JET_LAYOUT32(sizeof(PackedVertexTT) == 8);

struct PackedVertexW1I : VertexRecord<PackedVertexW1I> {
    PackedVertexW1I& operator=(const PackedVertexW1I& o);

    float v[2];
};
JET_LAYOUT32(sizeof(PackedVertexW1I) == 8);

struct PackedVertexW2I : VertexRecord<PackedVertexW2I> {
    PackedVertexW2I& operator=(const PackedVertexW2I& o);

    float v[3];
};
JET_LAYOUT32(sizeof(PackedVertexW2I) == 12);

struct PackedVertexW3I : VertexRecord<PackedVertexW3I> {
    PackedVertexW3I& operator=(const PackedVertexW3I& o);

    float v[4];
};
JET_LAYOUT32(sizeof(PackedVertexW3I) == 16);

struct PackedVertexW4 : VertexRecord<PackedVertexW4> {
    PackedVertexW4& operator=(const PackedVertexW4& o);

    float v[4];
};
JET_LAYOUT32(sizeof(PackedVertexW4) == 16);

struct PackedVertexW4I : VertexRecord<PackedVertexW4I> {
    PackedVertexW4I& operator=(const PackedVertexW4I& o);

    float v[5];
};
JET_LAYOUT32(sizeof(PackedVertexW4I) == 20);

struct PackedVertexW : VertexRecord<PackedVertexW> {
    PackedVertexW& operator=(const PackedVertexW& o);

    float v[1];
};
JET_LAYOUT32(sizeof(PackedVertexW) == 4);

struct PackedVertexWb4I : VertexRecord<PackedVertexWb4I> {
    PackedVertexWb4I& operator=(const PackedVertexWb4I& o);
    void __cdecl SetWeights(unsigned long a, unsigned long b);

    unsigned long u[2];
};
JET_LAYOUT32(sizeof(PackedVertexWb4I) == 8);

struct StreamedVertexC : VertexRecord<StreamedVertexC> {
    StreamedVertexC& operator=(const StreamedVertexC& o);

    float v[1];
};
JET_LAYOUT32(sizeof(StreamedVertexC) == 4);

struct StreamedVertexPCTBase : VertexRecord<StreamedVertexPCTBase> {
    StreamedVertexPCTBase& operator=(const StreamedVertexPCTBase& o);
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int sizeLookup[4];
    static const CheckVertexItemTypeInt vertexItemOffset[kVertexItemTypes];
};

struct StreamedVertexPNTBase : VertexRecord<StreamedVertexPNTBase> {
    StreamedVertexPNTBase& operator=(const StreamedVertexPNTBase& o);
    static VertexTypeID __cdecl GetVertexTypeID();
    static const int sizeLookup[4];
    static const CheckVertexItemTypeInt vertexItemOffset[kVertexItemTypes];
};

}  // namespace Jet
