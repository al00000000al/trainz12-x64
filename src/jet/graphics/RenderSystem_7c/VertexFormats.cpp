// Unit U1 of RenderSystem_7c: the vertex records (include/jet/VertexFormats.h). Generated from the TS12 assembly and
// data: type ids, sizes, item offset tables and the inline accessors the original exports.
#include <jet/VertexFormats.h>

namespace Jet {

// GeoLODMorphVertex_at3Blend
GeoLODMorphVertex_at3Blend::GeoLODMorphVertex_at3Blend() {}
GeoLODMorphVertex_at3Blend& GeoLODMorphVertex_at3Blend::operator=(const GeoLODMorphVertex_at3Blend& o) = default;
int GeoLODMorphVertex_at3Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_at3Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_at3Blend::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_at3Blend::GetVertexSize() { return sizeof(GeoLODMorphVertex_at3Blend); }
VertexTypeID GeoLODMorphVertex_at3Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_at3Blend::readDataOffset = 12;
const int GeoLODMorphVertex_at3Blend::readDataSize = 40;
const int GeoLODMorphVertex_at3Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 12, 24, 36, 44, 0, 4, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODMorphVertex_at4Blend
GeoLODMorphVertex_at4Blend::GeoLODMorphVertex_at4Blend() {}
GeoLODMorphVertex_at4Blend& GeoLODMorphVertex_at4Blend::operator=(const GeoLODMorphVertex_at4Blend& o) = default;
int GeoLODMorphVertex_at4Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_at4Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_at4Blend::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_at4Blend::GetVertexSize() { return sizeof(GeoLODMorphVertex_at4Blend); }
VertexTypeID GeoLODMorphVertex_at4Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_at4Blend::readDataOffset = 12;
const int GeoLODMorphVertex_at4Blend::readDataSize = 48;
const int GeoLODMorphVertex_at4Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 12, 24, 36, 48, 0, 4, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODMorphVertex_t1
GeoLODMorphVertex_t1::GeoLODMorphVertex_t1() {}
GeoLODMorphVertex_t1& GeoLODMorphVertex_t1::operator=(const GeoLODMorphVertex_t1& o) = default;
int GeoLODMorphVertex_t1::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t1::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t1::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t1::GetVertexSize() { return sizeof(GeoLODMorphVertex_t1); }
VertexTypeID GeoLODMorphVertex_t1::GetVertexTypeID() { return static_cast<VertexTypeID>(69); }
const int GeoLODMorphVertex_t1::readDataOffset = 12;
const int GeoLODMorphVertex_t1::readDataSize = 40;
const int GeoLODMorphVertex_t1::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, -1, -1, -1, 44,
    -1, -1, -1, 12, 24, -1, -1, 0, 4, 8, -1, 36, 44, -1, -1, -1, -1, -1, -1};

// GeoLODMorphVertex_t1Blend
GeoLODMorphVertex_t1Blend::GeoLODMorphVertex_t1Blend() {}
GeoLODMorphVertex_t1Blend& GeoLODMorphVertex_t1Blend::operator=(const GeoLODMorphVertex_t1Blend& o) = default;
int GeoLODMorphVertex_t1Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float GeoLODMorphVertex_t1Blend::GetMorphIndex() { return *Floats(0x8); }
float* GeoLODMorphVertex_t1Blend::GetMorphedPos() { return reinterpret_cast<float*>(Bytes(0x18)); }
float* GeoLODMorphVertex_t1Blend::GetMorphedTex(int i) { return Floats(0x2c + i * 16); }
float* GeoLODMorphVertex_t1Blend::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0xc)); }
float* GeoLODMorphVertex_t1Blend::GetOriginalTex(int i) { return Floats(0x24 + i * 16); }
int GeoLODMorphVertex_t1Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t1Blend::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t1Blend::GetVertexSize() { return sizeof(GeoLODMorphVertex_t1Blend); }
VertexTypeID GeoLODMorphVertex_t1Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_t1Blend::readDataOffset = 16;
const int GeoLODMorphVertex_t1Blend::readDataSize = 36;
const int GeoLODMorphVertex_t1Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, -1, -1, -1, 44,
    -1, -1, -1, 16, 24, 52, 52, 0, 4, 8, -1, 36, 44, -1, -1, -1, -1, -1, -1};

// GeoLODMorphVertex_t1Blendz
GeoLODMorphVertex_t1Blendz::GeoLODMorphVertex_t1Blendz() {}
GeoLODMorphVertex_t1Blendz& GeoLODMorphVertex_t1Blendz::operator=(const GeoLODMorphVertex_t1Blendz& o) = default;
int GeoLODMorphVertex_t1Blendz::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float GeoLODMorphVertex_t1Blendz::GetMorphIndex() { return *Floats(0x8); }
float* GeoLODMorphVertex_t1Blendz::GetMorphedPos() { return reinterpret_cast<float*>(Bytes(0x18)); }
float* GeoLODMorphVertex_t1Blendz::GetMorphedTex(int i) { return Floats(0x30 + i * 24); }
float* GeoLODMorphVertex_t1Blendz::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0xc)); }
float* GeoLODMorphVertex_t1Blendz::GetOriginalTex(int i) { return Floats(0x24 + i * 24); }
int GeoLODMorphVertex_t1Blendz::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t1Blendz::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t1Blendz::GetVertexSize() { return sizeof(GeoLODMorphVertex_t1Blendz); }
VertexTypeID GeoLODMorphVertex_t1Blendz::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_t1Blendz::readDataOffset = 12;
const int GeoLODMorphVertex_t1Blendz::readDataSize = 48;
const int GeoLODMorphVertex_t1Blendz::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, -1, -1, -1, 48,
    -1, -1, -1, 12, 24, -1, -1, 0, 4, 8, -1, 36, 48, -1, -1, -1, -1, -1, -1};

// GeoLODMorphVertex_t2
GeoLODMorphVertex_t2::GeoLODMorphVertex_t2() {}
GeoLODMorphVertex_t2& GeoLODMorphVertex_t2::operator=(const GeoLODMorphVertex_t2& o) = default;
int GeoLODMorphVertex_t2::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t2::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t2::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t2::GetVertexSize() { return sizeof(GeoLODMorphVertex_t2); }
VertexTypeID GeoLODMorphVertex_t2::GetVertexTypeID() { return static_cast<VertexTypeID>(70); }
const int GeoLODMorphVertex_t2::readDataOffset = 12;
const int GeoLODMorphVertex_t2::readDataSize = 56;
const int GeoLODMorphVertex_t2::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 52, -1, -1, 44,
    60, -1, -1, 12, 24, -1, -1, 0, 4, 8, -1, 36, 44, 52, 60, -1, -1, -1, -1};

// GeoLODMorphVertex_t2Blend
GeoLODMorphVertex_t2Blend::GeoLODMorphVertex_t2Blend() {}
GeoLODMorphVertex_t2Blend& GeoLODMorphVertex_t2Blend::operator=(const GeoLODMorphVertex_t2Blend& o) = default;
int GeoLODMorphVertex_t2Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t2Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t2Blend::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t2Blend::GetVertexSize() { return sizeof(GeoLODMorphVertex_t2Blend); }
VertexTypeID GeoLODMorphVertex_t2Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_t2Blend::readDataOffset = 12;
const int GeoLODMorphVertex_t2Blend::readDataSize = 64;
const int GeoLODMorphVertex_t2Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 52, -1, -1, 44,
    60, -1, -1, 12, 24, 68, 72, 0, 4, 8, -1, 36, 44, 52, 60, -1, -1, -1, -1};

// GeoLODMorphVertex_t2Blendz
GeoLODMorphVertex_t2Blendz::GeoLODMorphVertex_t2Blendz() {}
GeoLODMorphVertex_t2Blendz& GeoLODMorphVertex_t2Blendz::operator=(const GeoLODMorphVertex_t2Blendz& o) = default;
int GeoLODMorphVertex_t2Blendz::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t2Blendz::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t2Blendz::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t2Blendz::GetVertexSize() { return sizeof(GeoLODMorphVertex_t2Blendz); }
VertexTypeID GeoLODMorphVertex_t2Blendz::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_t2Blendz::readDataOffset = 12;
const int GeoLODMorphVertex_t2Blendz::readDataSize = 72;
const int GeoLODMorphVertex_t2Blendz::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 60, -1, -1, 48,
    72, -1, -1, 12, 24, -1, -1, 0, 4, 8, -1, 36, 48, 60, 72, -1, -1, -1, -1};

// GeoLODMorphVertex_t3
GeoLODMorphVertex_t3::GeoLODMorphVertex_t3() {}
GeoLODMorphVertex_t3& GeoLODMorphVertex_t3::operator=(const GeoLODMorphVertex_t3& o) = default;
int GeoLODMorphVertex_t3::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t3::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t3::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t3::GetVertexSize() { return sizeof(GeoLODMorphVertex_t3); }
VertexTypeID GeoLODMorphVertex_t3::GetVertexTypeID() { return static_cast<VertexTypeID>(71); }
const int GeoLODMorphVertex_t3::readDataOffset = 12;
const int GeoLODMorphVertex_t3::readDataSize = 72;
const int GeoLODMorphVertex_t3::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 52, 68, -1, 44,
    60, 76, -1, 12, 24, -1, -1, 0, 4, 8, -1, 36, 44, 52, 60, 68, 76, -1, -1};

// GeoLODMorphVertex_t3Blend
GeoLODMorphVertex_t3Blend::GeoLODMorphVertex_t3Blend() {}
GeoLODMorphVertex_t3Blend& GeoLODMorphVertex_t3Blend::operator=(const GeoLODMorphVertex_t3Blend& o) = default;
int GeoLODMorphVertex_t3Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t3Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t3Blend::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t3Blend::GetVertexSize() { return sizeof(GeoLODMorphVertex_t3Blend); }
VertexTypeID GeoLODMorphVertex_t3Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_t3Blend::readDataOffset = 12;
const int GeoLODMorphVertex_t3Blend::readDataSize = 88;
const int GeoLODMorphVertex_t3Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 52, 68, -1, 44,
    60, 76, -1, 12, 24, 84, 92, 0, 4, 8, -1, 36, 44, 52, 60, 68, 76, -1, -1};

// GeoLODMorphVertex_t3Blendz
GeoLODMorphVertex_t3Blendz::GeoLODMorphVertex_t3Blendz() {}
GeoLODMorphVertex_t3Blendz& GeoLODMorphVertex_t3Blendz::operator=(const GeoLODMorphVertex_t3Blendz& o) = default;
int GeoLODMorphVertex_t3Blendz::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t3Blendz::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t3Blendz::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t3Blendz::GetVertexSize() { return sizeof(GeoLODMorphVertex_t3Blendz); }
VertexTypeID GeoLODMorphVertex_t3Blendz::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_t3Blendz::readDataOffset = 12;
const int GeoLODMorphVertex_t3Blendz::readDataSize = 96;
const int GeoLODMorphVertex_t3Blendz::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 60, 84, -1, 48,
    72, 96, -1, 12, 24, -1, -1, 0, 4, 8, -1, 36, 48, 60, 72, 84, 96, -1, -1};

// GeoLODMorphVertex_t4
GeoLODMorphVertex_t4::GeoLODMorphVertex_t4() {}
GeoLODMorphVertex_t4& GeoLODMorphVertex_t4::operator=(const GeoLODMorphVertex_t4& o) = default;
int GeoLODMorphVertex_t4::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t4::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t4::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t4::GetVertexSize() { return sizeof(GeoLODMorphVertex_t4); }
VertexTypeID GeoLODMorphVertex_t4::GetVertexTypeID() { return static_cast<VertexTypeID>(72); }
const int GeoLODMorphVertex_t4::readDataOffset = 12;
const int GeoLODMorphVertex_t4::readDataSize = 88;
const int GeoLODMorphVertex_t4::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 52, 68, 84, 44,
    60, 76, 92, 12, 24, -1, -1, 0, 4, 8, -1, 36, 44, 52, 60, 68, 76, 84, 92};

// GeoLODMorphVertex_t4Blend
GeoLODMorphVertex_t4Blend::GeoLODMorphVertex_t4Blend() {}
GeoLODMorphVertex_t4Blend& GeoLODMorphVertex_t4Blend::operator=(const GeoLODMorphVertex_t4Blend& o) = default;
int GeoLODMorphVertex_t4Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t4Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t4Blend::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t4Blend::GetVertexSize() { return sizeof(GeoLODMorphVertex_t4Blend); }
VertexTypeID GeoLODMorphVertex_t4Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_t4Blend::readDataOffset = 12;
const int GeoLODMorphVertex_t4Blend::readDataSize = 112;
const int GeoLODMorphVertex_t4Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 52, 68, 84, 44,
    60, 76, 92, 12, 24, 100, 112, 0, 4, 8, -1, 36, 44, 52, 60, 68, 76, 84, 92};

// GeoLODMorphVertex_t4Blendz
GeoLODMorphVertex_t4Blendz::GeoLODMorphVertex_t4Blendz() {}
GeoLODMorphVertex_t4Blendz& GeoLODMorphVertex_t4Blendz::operator=(const GeoLODMorphVertex_t4Blendz& o) = default;
int GeoLODMorphVertex_t4Blendz::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_t4Blendz::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_t4Blendz::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_t4Blendz::GetVertexSize() { return sizeof(GeoLODMorphVertex_t4Blendz); }
VertexTypeID GeoLODMorphVertex_t4Blendz::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_t4Blendz::readDataOffset = 12;
const int GeoLODMorphVertex_t4Blendz::readDataSize = 120;
const int GeoLODMorphVertex_t4Blendz::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, 60, 84, 108, 48,
    72, 96, 120, 12, 24, -1, -1, 0, 4, 8, -1, 36, 48, 60, 72, 84, 96, 108, 120};

// GeoLODVertex_at3Blend
GeoLODVertex_at3Blend::GeoLODVertex_at3Blend() {}
GeoLODVertex_at3Blend& GeoLODVertex_at3Blend::operator=(const GeoLODVertex_at3Blend& o) = default;
int GeoLODVertex_at3Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_at3Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_at3Blend::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_at3Blend::GetVertexSize() { return sizeof(GeoLODVertex_at3Blend); }
VertexTypeID GeoLODVertex_at3Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_at3Blend::readDataOffset = 4;
const int GeoLODVertex_at3Blend::readDataSize = 20;
const int GeoLODVertex_at3Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 4, -1, 16, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODVertex_at4Blend
GeoLODVertex_at4Blend::GeoLODVertex_at4Blend() {}
GeoLODVertex_at4Blend& GeoLODVertex_at4Blend::operator=(const GeoLODVertex_at4Blend& o) = default;
int GeoLODVertex_at4Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_at4Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_at4Blend::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_at4Blend::GetVertexSize() { return sizeof(GeoLODVertex_at4Blend); }
VertexTypeID GeoLODVertex_at4Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_at4Blend::readDataOffset = 4;
const int GeoLODVertex_at4Blend::readDataSize = 24;
const int GeoLODVertex_at4Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 4, -1, 16, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODVertex_t1
GeoLODVertex_t1::GeoLODVertex_t1() {}
GeoLODVertex_t1& GeoLODVertex_t1::operator=(const GeoLODVertex_t1& o) = default;
int GeoLODVertex_t1::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t1::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t1::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t1::GetVertexSize() { return sizeof(GeoLODVertex_t1); }
VertexTypeID GeoLODVertex_t1::GetVertexTypeID() { return static_cast<VertexTypeID>(64); }
const int GeoLODVertex_t1::readDataOffset = 4;
const int GeoLODVertex_t1::readDataSize = 20;
const int GeoLODVertex_t1::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, 16, -1, -1, -1, -1, -1, -1, -1};

// GeoLODVertex_t1Blend
GeoLODVertex_t1Blend::GeoLODVertex_t1Blend() {}
GeoLODVertex_t1Blend& GeoLODVertex_t1Blend::operator=(const GeoLODVertex_t1Blend& o) = default;
int GeoLODVertex_t1Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float* GeoLODVertex_t1Blend::GetMorphedTexWeight() { return 0; }
float* GeoLODVertex_t1Blend::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0x4)); }
float* GeoLODVertex_t1Blend::GetOriginalTex(int i) { return Floats(0x10 + i * 8); }
float* GeoLODVertex_t1Blend::GetOriginalTexWeight() { return 0; }
int GeoLODVertex_t1Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t1Blend::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t1Blend::GetVertexSize() { return sizeof(GeoLODVertex_t1Blend); }
VertexTypeID GeoLODVertex_t1Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_t1Blend::readDataOffset = 4;
const int GeoLODVertex_t1Blend::readDataSize = 20;
const int GeoLODVertex_t1Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, -1,
    -1, -1, -1, 4, -1, 24, -1, 0, -1, -1, -1, 16, -1, -1, -1, -1, -1, -1, -1};

// GeoLODVertex_t1Blendz
GeoLODVertex_t1Blendz::GeoLODVertex_t1Blendz() {}
GeoLODVertex_t1Blendz& GeoLODVertex_t1Blendz::operator=(const GeoLODVertex_t1Blendz& o) = default;
int GeoLODVertex_t1Blendz::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float* GeoLODVertex_t1Blendz::GetMorphedTexWeight() { return 0; }
float* GeoLODVertex_t1Blendz::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0x4)); }
float* GeoLODVertex_t1Blendz::GetOriginalTex(int i) { return Floats(0x10 + i * 12); }
float* GeoLODVertex_t1Blendz::GetOriginalTexWeight() { return 0; }
int GeoLODVertex_t1Blendz::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t1Blendz::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t1Blendz::GetVertexSize() { return sizeof(GeoLODVertex_t1Blendz); }
VertexTypeID GeoLODVertex_t1Blendz::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_t1Blendz::readDataOffset = 4;
const int GeoLODVertex_t1Blendz::readDataSize = 24;
const int GeoLODVertex_t1Blendz::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, -1, -1, -1, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, 16, -1, -1, -1, -1, -1, -1, -1};

// GeoLODVertex_t2
GeoLODVertex_t2::GeoLODVertex_t2() {}
GeoLODVertex_t2& GeoLODVertex_t2::operator=(const GeoLODVertex_t2& o) = default;
int GeoLODVertex_t2::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t2::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t2::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t2::GetVertexSize() { return sizeof(GeoLODVertex_t2); }
VertexTypeID GeoLODVertex_t2::GetVertexTypeID() { return static_cast<VertexTypeID>(65); }
const int GeoLODVertex_t2::readDataOffset = 4;
const int GeoLODVertex_t2::readDataSize = 28;
const int GeoLODVertex_t2::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 24, -1, -1, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, 16, -1, 24, -1, -1, -1, -1, -1};

// GeoLODVertex_t2Blend
GeoLODVertex_t2Blend::GeoLODVertex_t2Blend() {}
GeoLODVertex_t2Blend& GeoLODVertex_t2Blend::operator=(const GeoLODVertex_t2Blend& o) = default;
int GeoLODVertex_t2Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t2Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t2Blend::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t2Blend::GetVertexSize() { return sizeof(GeoLODVertex_t2Blend); }
VertexTypeID GeoLODVertex_t2Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_t2Blend::readDataOffset = 4;
const int GeoLODVertex_t2Blend::readDataSize = 32;
const int GeoLODVertex_t2Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 24, -1, -1, -1,
    -1, -1, -1, 4, -1, 32, -1, 0, -1, -1, -1, 16, -1, 24, -1, -1, -1, -1, -1};

// GeoLODVertex_t2Blendz
GeoLODVertex_t2Blendz::GeoLODVertex_t2Blendz() {}
GeoLODVertex_t2Blendz& GeoLODVertex_t2Blendz::operator=(const GeoLODVertex_t2Blendz& o) = default;
int GeoLODVertex_t2Blendz::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t2Blendz::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t2Blendz::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t2Blendz::GetVertexSize() { return sizeof(GeoLODVertex_t2Blendz); }
VertexTypeID GeoLODVertex_t2Blendz::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_t2Blendz::readDataOffset = 4;
const int GeoLODVertex_t2Blendz::readDataSize = 36;
const int GeoLODVertex_t2Blendz::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 28, -1, -1, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, 16, -1, 28, -1, -1, -1, -1, -1};

// GeoLODVertex_t3
GeoLODVertex_t3::GeoLODVertex_t3() {}
GeoLODVertex_t3& GeoLODVertex_t3::operator=(const GeoLODVertex_t3& o) = default;
int GeoLODVertex_t3::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t3::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t3::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t3::GetVertexSize() { return sizeof(GeoLODVertex_t3); }
VertexTypeID GeoLODVertex_t3::GetVertexTypeID() { return static_cast<VertexTypeID>(66); }
const int GeoLODVertex_t3::readDataOffset = 4;
const int GeoLODVertex_t3::readDataSize = 36;
const int GeoLODVertex_t3::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 24, 32, -1, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, 16, -1, 24, -1, 32, -1, -1, -1};

// GeoLODVertex_t3Blend
GeoLODVertex_t3Blend::GeoLODVertex_t3Blend() {}
GeoLODVertex_t3Blend& GeoLODVertex_t3Blend::operator=(const GeoLODVertex_t3Blend& o) = default;
int GeoLODVertex_t3Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t3Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t3Blend::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t3Blend::GetVertexSize() { return sizeof(GeoLODVertex_t3Blend); }
VertexTypeID GeoLODVertex_t3Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_t3Blend::readDataOffset = 4;
const int GeoLODVertex_t3Blend::readDataSize = 44;
const int GeoLODVertex_t3Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 24, 32, -1, -1,
    -1, -1, -1, 4, -1, 40, -1, 0, -1, -1, -1, 16, -1, 24, -1, 32, -1, -1, -1};

// GeoLODVertex_t3Blendz
GeoLODVertex_t3Blendz::GeoLODVertex_t3Blendz() {}
GeoLODVertex_t3Blendz& GeoLODVertex_t3Blendz::operator=(const GeoLODVertex_t3Blendz& o) = default;
int GeoLODVertex_t3Blendz::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t3Blendz::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t3Blendz::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t3Blendz::GetVertexSize() { return sizeof(GeoLODVertex_t3Blendz); }
VertexTypeID GeoLODVertex_t3Blendz::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_t3Blendz::readDataOffset = 4;
const int GeoLODVertex_t3Blendz::readDataSize = 48;
const int GeoLODVertex_t3Blendz::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 28, 40, -1, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, 16, -1, 28, -1, 40, -1, -1, -1};

// GeoLODVertex_t4
GeoLODVertex_t4::GeoLODVertex_t4() {}
GeoLODVertex_t4& GeoLODVertex_t4::operator=(const GeoLODVertex_t4& o) = default;
int GeoLODVertex_t4::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t4::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t4::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t4::GetVertexSize() { return sizeof(GeoLODVertex_t4); }
VertexTypeID GeoLODVertex_t4::GetVertexTypeID() { return static_cast<VertexTypeID>(67); }
const int GeoLODVertex_t4::readDataOffset = 4;
const int GeoLODVertex_t4::readDataSize = 44;
const int GeoLODVertex_t4::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 24, 32, 40, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, 16, -1, 24, -1, 32, -1, 40, -1};

// GeoLODVertex_t4Blend
GeoLODVertex_t4Blend::GeoLODVertex_t4Blend() {}
GeoLODVertex_t4Blend& GeoLODVertex_t4Blend::operator=(const GeoLODVertex_t4Blend& o) = default;
int GeoLODVertex_t4Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t4Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t4Blend::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t4Blend::GetVertexSize() { return sizeof(GeoLODVertex_t4Blend); }
VertexTypeID GeoLODVertex_t4Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_t4Blend::readDataOffset = 4;
const int GeoLODVertex_t4Blend::readDataSize = 56;
const int GeoLODVertex_t4Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 24, 32, 40, -1,
    -1, -1, -1, 4, -1, 48, -1, 0, -1, -1, -1, 16, -1, 24, -1, 32, -1, 40, -1};

// GeoLODVertex_t4Blendz
GeoLODVertex_t4Blendz::GeoLODVertex_t4Blendz() {}
GeoLODVertex_t4Blendz& GeoLODVertex_t4Blendz::operator=(const GeoLODVertex_t4Blendz& o) = default;
int GeoLODVertex_t4Blendz::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_t4Blendz::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_t4Blendz::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_t4Blendz::GetVertexSize() { return sizeof(GeoLODVertex_t4Blendz); }
VertexTypeID GeoLODVertex_t4Blendz::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_t4Blendz::readDataOffset = 4;
const int GeoLODVertex_t4Blendz::readDataSize = 60;
const int GeoLODVertex_t4Blendz::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 28, 40, 52, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, 16, -1, 28, -1, 40, -1, 52, -1};

// PackedVertexA12
PackedVertexA12::PackedVertexA12() {}
PackedVertexA12& PackedVertexA12::operator=(const PackedVertexA12& o) = default;
Vector2& PackedVertexA12::GetAttrib1() { return *reinterpret_cast<Vector2*>(Bytes(0)); }
const Vector2& PackedVertexA12::GetAttrib1() const { return *reinterpret_cast<const Vector2*>(Bytes(0)); }
void PackedVertexA12::SetAttrib1(const Vector2& p) { v[0] = p.x; v[1] = p.y; }

// PackedVertexO2
PackedVertexO2::PackedVertexO2() {}
PackedVertexO2& PackedVertexO2::operator=(const PackedVertexO2& o) = default;
Vector2& PackedVertexO2::GetOffset() { return *reinterpret_cast<Vector2*>(Bytes(0)); }
const Vector2& PackedVertexO2::GetOffset() const { return *reinterpret_cast<const Vector2*>(Bytes(0)); }
void PackedVertexO2::SetOffset(const Vector2& p) { v[0] = p.x; v[1] = p.y; }

// PackedVertexPFX_OLVT
PackedVertexPFX_OLVT::PackedVertexPFX_OLVT() {}
Vector2& PackedVertexPFX_OLVT::operator=(const Vector2& p)
{
    v[0] = p.x; v[1] = p.y;
    return *reinterpret_cast<Vector2*>(v);
}
PackedVertexPFX_OLVT& PackedVertexPFX_OLVT::operator=(const PackedVertexPFX_OLVT& o) = default;
int PackedVertexPFX_OLVT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPFX_OLVT::GetVertexSize() { return sizeof(PackedVertexPFX_OLVT); }
VertexTypeID PackedVertexPFX_OLVT::GetVertexTypeID() { return static_cast<VertexTypeID>(56); }
const OffsetLookupCheck PackedVertexPFX_OLVT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {-1, 1}, {-1, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {32, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {0, 23},
    {8, 24}, {20, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPTA12
PackedVertexPTA12::PackedVertexPTA12() {}
Vector3& PackedVertexPTA12::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPTA12& PackedVertexPTA12::operator=(const PackedVertexPTA12& o) = default;
int PackedVertexPTA12::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPTA12::GetVertexSize() { return sizeof(PackedVertexPTA12); }
VertexTypeID PackedVertexPTA12::GetVertexTypeID() { return static_cast<VertexTypeID>(38); }
const OffsetLookupCheck PackedVertexPTA12::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {20, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {12, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexV3
PackedVertexV3::PackedVertexV3() {}
PackedVertexV3& PackedVertexV3::operator=(const PackedVertexV3& o) = default;
Vector3& PackedVertexV3::GetVelocity() { return *reinterpret_cast<Vector3*>(Bytes(0)); }
const Vector3& PackedVertexV3::GetVelocity() const { return *reinterpret_cast<const Vector3*>(Bytes(0)); }
void PackedVertexV3::SetVelocity(const Vector3& p) { v[0] = p.x; v[1] = p.y; v[2] = p.z; }

// StreamedVertexN
StreamedVertexN::StreamedVertexN() {}
StreamedVertexN& StreamedVertexN::operator=(const StreamedVertexN& o) = default;

// StreamedVertexP
StreamedVertexP::StreamedVertexP() {}
StreamedVertexP& StreamedVertexP::operator=(const StreamedVertexP& o) = default;

// StreamedVertexT
StreamedVertexT::StreamedVertexT() {}
StreamedVertexT& StreamedVertexT::operator=(const StreamedVertexT& o) = default;

// TerrainBlend2t2Vertex
TerrainBlend2t2Vertex::TerrainBlend2t2Vertex() {}
TerrainBlend2t2Vertex& TerrainBlend2t2Vertex::operator=(const TerrainBlend2t2Vertex& o) = default;
int TerrainBlend2t2Vertex::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float* TerrainBlend2t2Vertex::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0)); }
int TerrainBlend2t2Vertex::GetReadDataOffset() { return readDataOffset; }
int TerrainBlend2t2Vertex::GetReadDataSize() { return readDataSize; }
int TerrainBlend2t2Vertex::GetVertexSize() { return sizeof(TerrainBlend2t2Vertex); }
VertexTypeID TerrainBlend2t2Vertex::GetVertexTypeID() { return static_cast<VertexTypeID>(57); }
const int TerrainBlend2t2Vertex::readDataOffset = 0;
const int TerrainBlend2t2Vertex::readDataSize = 32;
const int TerrainBlend2t2Vertex::vertexItemOffset[kVertexItemTypes] = {
    -1, 0, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 24, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// TerrainBlend3t2Vertex
TerrainBlend3t2Vertex::TerrainBlend3t2Vertex() {}
TerrainBlend3t2Vertex& TerrainBlend3t2Vertex::operator=(const TerrainBlend3t2Vertex& o) = default;
VertexTypeID TerrainBlend3t2Vertex::GetVertexTypeID() { return static_cast<VertexTypeID>(58); }

// TerrainBlend3t3Vertex
TerrainBlend3t3Vertex::TerrainBlend3t3Vertex() {}
TerrainBlend3t3Vertex& TerrainBlend3t3Vertex::operator=(const TerrainBlend3t3Vertex& o) = default;
int TerrainBlend3t3Vertex::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int TerrainBlend3t3Vertex::GetReadDataOffset() { return readDataOffset; }
int TerrainBlend3t3Vertex::GetReadDataSize() { return readDataSize; }
int TerrainBlend3t3Vertex::GetVertexSize() { return sizeof(TerrainBlend3t3Vertex); }
VertexTypeID TerrainBlend3t3Vertex::GetVertexTypeID() { return static_cast<VertexTypeID>(60); }
const int TerrainBlend3t3Vertex::readDataOffset = 0;
const int TerrainBlend3t3Vertex::readDataSize = 40;
const int TerrainBlend3t3Vertex::vertexItemOffset[kVertexItemTypes] = {
    -1, 0, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 24, 32, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// TerrainBlend4t2Vertex
TerrainBlend4t2Vertex::TerrainBlend4t2Vertex() {}
TerrainBlend4t2Vertex& TerrainBlend4t2Vertex::operator=(const TerrainBlend4t2Vertex& o) = default;
int TerrainBlend4t2Vertex::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int TerrainBlend4t2Vertex::GetReadDataOffset() { return readDataOffset; }
int TerrainBlend4t2Vertex::GetReadDataSize() { return readDataSize; }
int TerrainBlend4t2Vertex::GetVertexSize() { return sizeof(TerrainBlend4t2Vertex); }
VertexTypeID TerrainBlend4t2Vertex::GetVertexTypeID() { return static_cast<VertexTypeID>(59); }
const int TerrainBlend4t2Vertex::readDataOffset = 0;
const int TerrainBlend4t2Vertex::readDataSize = 36;
const int TerrainBlend4t2Vertex::vertexItemOffset[kVertexItemTypes] = {
    -1, 0, 12, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 28, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// TerrainBlend4t4Vertex
TerrainBlend4t4Vertex::TerrainBlend4t4Vertex() {}
TerrainBlend4t4Vertex& TerrainBlend4t4Vertex::operator=(const TerrainBlend4t4Vertex& o) = default;
int TerrainBlend4t4Vertex::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int TerrainBlend4t4Vertex::GetReadDataOffset() { return readDataOffset; }
int TerrainBlend4t4Vertex::GetReadDataSize() { return readDataSize; }
int TerrainBlend4t4Vertex::GetVertexSize() { return sizeof(TerrainBlend4t4Vertex); }
VertexTypeID TerrainBlend4t4Vertex::GetVertexTypeID() { return static_cast<VertexTypeID>(61); }
const int TerrainBlend4t4Vertex::readDataOffset = 0;
const int TerrainBlend4t4Vertex::readDataSize = 52;
const int TerrainBlend4t4Vertex::vertexItemOffset[kVertexItemTypes] = {
    -1, 0, 12, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 28, 36, 44, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// TerrainBlendt4Vertex
TerrainBlendt4Vertex::TerrainBlendt4Vertex() {}
TerrainBlendt4Vertex& TerrainBlendt4Vertex::operator=(const TerrainBlendt4Vertex& o) = default;
int TerrainBlendt4Vertex::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int TerrainBlendt4Vertex::GetReadDataOffset() { return readDataOffset; }
int TerrainBlendt4Vertex::GetReadDataSize() { return readDataSize; }
int TerrainBlendt4Vertex::GetVertexSize() { return sizeof(TerrainBlendt4Vertex); }
VertexTypeID TerrainBlendt4Vertex::GetVertexTypeID() { return static_cast<VertexTypeID>(62); }
const int TerrainBlendt4Vertex::readDataOffset = 0;
const int TerrainBlendt4Vertex::readDataSize = 48;
const int TerrainBlendt4Vertex::vertexItemOffset[kVertexItemTypes] = {
    -1, 0, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 28, 36, 44, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODMorphVertex
GeoLODMorphVertex& GeoLODMorphVertex::operator=(const GeoLODMorphVertex& o) = default;
int GeoLODMorphVertex::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float GeoLODMorphVertex::GetMorphIndex() { return *Floats(0x8); }
float* GeoLODMorphVertex::GetMorphedPos() { return reinterpret_cast<float*>(Bytes(0x18)); }
float* GeoLODMorphVertex::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0xc)); }
int GeoLODMorphVertex::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex::GetVertexSize() { return sizeof(GeoLODMorphVertex); }
VertexTypeID GeoLODMorphVertex::GetVertexTypeID() { return static_cast<VertexTypeID>(68); }
const int GeoLODMorphVertex::readDataOffset = 12;
const int GeoLODMorphVertex::readDataSize = 24;
const int GeoLODMorphVertex::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 12, 24, -1, -1, 0, 4, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODMorphVertex_at1Blend
GeoLODMorphVertex_at1Blend& GeoLODMorphVertex_at1Blend::operator=(const GeoLODMorphVertex_at1Blend& o) = default;
int GeoLODMorphVertex_at1Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float GeoLODMorphVertex_at1Blend::GetMorphIndex() { return *Floats(0x8); }
float* GeoLODMorphVertex_at1Blend::GetMorphedPos() { return reinterpret_cast<float*>(Bytes(0x18)); }
float* GeoLODMorphVertex_at1Blend::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0xc)); }
int GeoLODMorphVertex_at1Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_at1Blend::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_at1Blend::GetVertexSize() { return sizeof(GeoLODMorphVertex_at1Blend); }
VertexTypeID GeoLODMorphVertex_at1Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_at1Blend::readDataOffset = 12;
const int GeoLODMorphVertex_at1Blend::readDataSize = 24;
const int GeoLODMorphVertex_at1Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 12, 24, 36, 36, 0, 4, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODMorphVertex_at2Blend
GeoLODMorphVertex_at2Blend& GeoLODMorphVertex_at2Blend::operator=(const GeoLODMorphVertex_at2Blend& o) = default;
int GeoLODMorphVertex_at2Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODMorphVertex_at2Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODMorphVertex_at2Blend::GetReadDataSize() { return readDataSize; }
int GeoLODMorphVertex_at2Blend::GetVertexSize() { return sizeof(GeoLODMorphVertex_at2Blend); }
VertexTypeID GeoLODMorphVertex_at2Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODMorphVertex_at2Blend::readDataOffset = 12;
const int GeoLODMorphVertex_at2Blend::readDataSize = 32;
const int GeoLODMorphVertex_at2Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 12, 0, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 12, 24, 36, 40, 0, 4, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODVertex
GeoLODVertex& GeoLODVertex::operator=(const GeoLODVertex& o) = default;
int GeoLODVertex::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float* GeoLODVertex::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0x4)); }
int GeoLODVertex::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex::GetReadDataSize() { return readDataSize; }
int GeoLODVertex::GetVertexSize() { return sizeof(GeoLODVertex); }
VertexTypeID GeoLODVertex::GetVertexTypeID() { return static_cast<VertexTypeID>(63); }
const int GeoLODVertex::readDataOffset = 4;
const int GeoLODVertex::readDataSize = 12;
const int GeoLODVertex::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 4, -1, -1, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODVertex_at1Blend
GeoLODVertex_at1Blend& GeoLODVertex_at1Blend::operator=(const GeoLODVertex_at1Blend& o) = default;
int GeoLODVertex_at1Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
float* GeoLODVertex_at1Blend::GetMorphedTexWeight() { return 0; }
float* GeoLODVertex_at1Blend::GetOriginalPos() { return reinterpret_cast<float*>(Bytes(0x4)); }
float* GeoLODVertex_at1Blend::GetOriginalTexWeight() { return 0; }
int GeoLODVertex_at1Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_at1Blend::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_at1Blend::GetVertexSize() { return sizeof(GeoLODVertex_at1Blend); }
VertexTypeID GeoLODVertex_at1Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_at1Blend::readDataOffset = 4;
const int GeoLODVertex_at1Blend::readDataSize = 12;
const int GeoLODVertex_at1Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 4, -1, 16, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// GeoLODVertex_at2Blend
GeoLODVertex_at2Blend& GeoLODVertex_at2Blend::operator=(const GeoLODVertex_at2Blend& o) = default;
int GeoLODVertex_at2Blend::GetItemOffset(VertexItemType type) { return vertexItemOffset[type]; }
int GeoLODVertex_at2Blend::GetReadDataOffset() { return readDataOffset; }
int GeoLODVertex_at2Blend::GetReadDataSize() { return readDataSize; }
int GeoLODVertex_at2Blend::GetVertexSize() { return sizeof(GeoLODVertex_at2Blend); }
VertexTypeID GeoLODVertex_at2Blend::GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
const int GeoLODVertex_at2Blend::readDataOffset = 4;
const int GeoLODVertex_at2Blend::readDataSize = 16;
const int GeoLODVertex_at2Blend::vertexItemOffset[kVertexItemTypes] = {
    -1, 4, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 4, -1, 16, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// PackedVertexC
PackedVertexC& PackedVertexC::operator=(const PackedVertexC& o) = default;

// PackedVertexL3
PackedVertexL3& PackedVertexL3::operator=(const PackedVertexL3& o) = default;
Vector3& PackedVertexL3::GetLifeData() { return *reinterpret_cast<Vector3*>(Bytes(0)); }
const Vector3& PackedVertexL3::GetLifeData() const { return *reinterpret_cast<const Vector3*>(Bytes(0)); }
void PackedVertexL3::SetLifeData(const Vector3& p) { v[0] = p.x; v[1] = p.y; v[2] = p.z; }

// PackedVertexN
PackedVertexN& PackedVertexN::operator=(const PackedVertexN& o) = default;
Vector3& PackedVertexN::GetNormal() { return *reinterpret_cast<Vector3*>(Bytes(0)); }
const Vector3& PackedVertexN::GetNormal() const { return *reinterpret_cast<const Vector3*>(Bytes(0)); }
void PackedVertexN::SetNormal(const Vector3& p) { v[0] = p.x; v[1] = p.y; v[2] = p.z; }

// PackedVertexP4
Vector4& PackedVertexP4::operator=(const Vector4& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z; v[3] = p.w;
    return *reinterpret_cast<Vector4*>(v);
}
PackedVertexP4& PackedVertexP4::operator=(const PackedVertexP4& o) = default;
PackedVertexP4::operator Vector4&() { return *reinterpret_cast<Vector4*>(Bytes(0)); }
PackedVertexP4::operator Vector4*() { return reinterpret_cast<Vector4*>(Bytes(0)); }
PackedVertexP4::operator const Vector4&() const { return *reinterpret_cast<const Vector4*>(Bytes(0)); }
PackedVertexP4::operator const Vector4*() const { return reinterpret_cast<const Vector4*>(Bytes(0)); }
int PackedVertexP4::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
Vector4& PackedVertexP4::GetPosition() { return *reinterpret_cast<Vector4*>(Bytes(0)); }
const Vector4& PackedVertexP4::GetPosition() const { return *reinterpret_cast<const Vector4*>(Bytes(0)); }
int PackedVertexP4::GetVertexSize() { return sizeof(PackedVertexP4); }
VertexTypeID PackedVertexP4::GetVertexTypeID() { return static_cast<VertexTypeID>(1); }
void PackedVertexP4::SetPosition(const Vector3& p, float w) { v[0] = p.x; v[1] = p.y; v[2] = p.z; v[3] = w; }
void PackedVertexP4::SetPosition(const Vector4& p) { v[0] = p.x; v[1] = p.y; v[2] = p.z; v[3] = p.w; }
const OffsetLookupCheck PackedVertexP4::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexP4Wb4IN
Vector3& PackedVertexP4Wb4IN::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexP4Wb4IN& PackedVertexP4Wb4IN::operator=(const PackedVertexP4Wb4IN& o) = default;
int PackedVertexP4Wb4IN::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexP4Wb4IN::GetVertexSize() { return sizeof(PackedVertexP4Wb4IN); }
VertexTypeID PackedVertexP4Wb4IN::GetVertexTypeID() { return static_cast<VertexTypeID>(7); }
const OffsetLookupCheck PackedVertexP4Wb4IN::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {24, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {16, 9}, {20, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexP
Vector3& PackedVertexP::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexP& PackedVertexP::operator=(const PackedVertexP& o) = default;
PackedVertexP::operator Vector3&() { return *reinterpret_cast<Vector3*>(Bytes(0)); }
PackedVertexP::operator Vector3*() { return reinterpret_cast<Vector3*>(Bytes(0)); }
PackedVertexP::operator const Vector3&() const { return *reinterpret_cast<const Vector3*>(Bytes(0)); }
PackedVertexP::operator const Vector3*() const { return reinterpret_cast<const Vector3*>(Bytes(0)); }
int PackedVertexP::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
Vector3& PackedVertexP::GetPosition() { return *reinterpret_cast<Vector3*>(Bytes(0)); }
const Vector3& PackedVertexP::GetPosition() const { return *reinterpret_cast<const Vector3*>(Bytes(0)); }
int PackedVertexP::GetVertexSize() { return sizeof(PackedVertexP); }
VertexTypeID PackedVertexP::GetVertexTypeID() { return static_cast<VertexTypeID>(0); }
void PackedVertexP::SetPosition(const Vector3& p) { v[0] = p.x; v[1] = p.y; v[2] = p.z; }
const OffsetLookupCheck PackedVertexP::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPC
Vector3& PackedVertexPC::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPC& PackedVertexPC::operator=(const PackedVertexPC& o) = default;
int PackedVertexPC::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPC::GetVertexSize() { return sizeof(PackedVertexPC); }
VertexTypeID PackedVertexPC::GetVertexTypeID() { return static_cast<VertexTypeID>(3); }
const OffsetLookupCheck PackedVertexPC::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {12, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPCNT
Vector3& PackedVertexPCNT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPCNT& PackedVertexPCNT::operator=(const PackedVertexPCNT& o) = default;
int PackedVertexPCNT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPCNT::GetVertexSize() { return sizeof(PackedVertexPCNT); }
VertexTypeID PackedVertexPCNT::GetVertexTypeID() { return static_cast<VertexTypeID>(20); }
const OffsetLookupCheck PackedVertexPCNT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {12, 2}, {-1, 3}, {16, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPCNTA
Vector3& PackedVertexPCNTA::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPCNTA& PackedVertexPCNTA::operator=(const PackedVertexPCNTA& o) = default;
int PackedVertexPCNTA::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPCNTA::GetVertexSize() { return sizeof(PackedVertexPCNTA); }
VertexTypeID PackedVertexPCNTA::GetVertexTypeID() { return static_cast<VertexTypeID>(40); }
const OffsetLookupCheck PackedVertexPCNTA::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {12, 2}, {-1, 3}, {16, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {36, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPCT
Vector3& PackedVertexPCT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPCT& PackedVertexPCT::operator=(const PackedVertexPCT& o) = default;
int PackedVertexPCT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPCT::GetVertexSize() { return sizeof(PackedVertexPCT); }
VertexTypeID PackedVertexPCT::GetVertexTypeID() { return static_cast<VertexTypeID>(19); }
const OffsetLookupCheck PackedVertexPCT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {12, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {16, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPCTT
Vector3& PackedVertexPCTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPCTT& PackedVertexPCTT::operator=(const PackedVertexPCTT& o) = default;
int PackedVertexPCTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPCTT::GetVertexSize() { return sizeof(PackedVertexPCTT); }
VertexTypeID PackedVertexPCTT::GetVertexTypeID() { return static_cast<VertexTypeID>(35); }
const OffsetLookupCheck PackedVertexPCTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {12, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {16, 15},
    {24, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPN
Vector3& PackedVertexPN::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPN& PackedVertexPN::operator=(const PackedVertexPN& o) = default;
int PackedVertexPN::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPN::GetVertexSize() { return sizeof(PackedVertexPN); }
VertexTypeID PackedVertexPN::GetVertexTypeID() { return static_cast<VertexTypeID>(2); }
const OffsetLookupCheck PackedVertexPN::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {12, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPNT
Vector3& PackedVertexPNT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPNT& PackedVertexPNT::operator=(const PackedVertexPNT& o) = default;
int PackedVertexPNT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPNT::GetVertexSize() { return sizeof(PackedVertexPNT); }
VertexTypeID PackedVertexPNT::GetVertexTypeID() { return static_cast<VertexTypeID>(18); }
const OffsetLookupCheck PackedVertexPNT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {12, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPNTA
Vector3& PackedVertexPNTA::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPNTA& PackedVertexPNTA::operator=(const PackedVertexPNTA& o) = default;
int PackedVertexPNTA::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPNTA::GetVertexSize() { return sizeof(PackedVertexPNTA); }
VertexTypeID PackedVertexPNTA::GetVertexTypeID() { return static_cast<VertexTypeID>(39); }
const OffsetLookupCheck PackedVertexPNTA::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {12, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {32, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPNTAI
Vector3& PackedVertexPNTAI::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPNTAI& PackedVertexPNTAI::operator=(const PackedVertexPNTAI& o) = default;
int PackedVertexPNTAI::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPNTAI::GetVertexSize() { return sizeof(PackedVertexPNTAI); }
VertexTypeID PackedVertexPNTAI::GetVertexTypeID() { return static_cast<VertexTypeID>(44); }
const OffsetLookupCheck PackedVertexPNTAI::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {12, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {32, 26}, {-1, 27}, {-1, 28}, {44, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPNTT
Vector3& PackedVertexPNTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPNTT& PackedVertexPNTT::operator=(const PackedVertexPNTT& o) = default;
int PackedVertexPNTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPNTT::GetVertexSize() { return sizeof(PackedVertexPNTT); }
VertexTypeID PackedVertexPNTT::GetVertexTypeID() { return static_cast<VertexTypeID>(34); }
const OffsetLookupCheck PackedVertexPNTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {12, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {32, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPNTTA
Vector3& PackedVertexPNTTA::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPNTTA& PackedVertexPNTTA::operator=(const PackedVertexPNTTA& o) = default;
int PackedVertexPNTTA::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPNTTA::GetVertexSize() { return sizeof(PackedVertexPNTTA); }
VertexTypeID PackedVertexPNTTA::GetVertexTypeID() { return static_cast<VertexTypeID>(41); }
const OffsetLookupCheck PackedVertexPNTTA::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {12, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {32, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {40, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPT
Vector3& PackedVertexPT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPT& PackedVertexPT::operator=(const PackedVertexPT& o) = default;
int PackedVertexPT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPT::GetVertexSize() { return sizeof(PackedVertexPT); }
VertexTypeID PackedVertexPT::GetVertexTypeID() { return static_cast<VertexTypeID>(4); }
const OffsetLookupCheck PackedVertexPT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {12, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW1IC
Vector3& PackedVertexPW1IC::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW1IC& PackedVertexPW1IC::operator=(const PackedVertexPW1IC& o) = default;
int PackedVertexPW1IC::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW1IC::GetVertexSize() { return sizeof(PackedVertexPW1IC); }
VertexTypeID PackedVertexPW1IC::GetVertexTypeID() { return static_cast<VertexTypeID>(14); }
const OffsetLookupCheck PackedVertexPW1IC::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {20, 2}, {-1, 3}, {-1, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW1ICT
Vector3& PackedVertexPW1ICT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW1ICT& PackedVertexPW1ICT::operator=(const PackedVertexPW1ICT& o) = default;
int PackedVertexPW1ICT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW1ICT::GetVertexSize() { return sizeof(PackedVertexPW1ICT); }
VertexTypeID PackedVertexPW1ICT::GetVertexTypeID() { return static_cast<VertexTypeID>(29); }
const OffsetLookupCheck PackedVertexPW1ICT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {20, 2}, {-1, 3}, {-1, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW1ICTT
Vector3& PackedVertexPW1ICTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW1ICTT& PackedVertexPW1ICTT::operator=(const PackedVertexPW1ICTT& o) = default;
int PackedVertexPW1ICTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW1ICTT::GetVertexSize() { return sizeof(PackedVertexPW1ICTT); }
VertexTypeID PackedVertexPW1ICTT::GetVertexTypeID() { return static_cast<VertexTypeID>(52); }
const OffsetLookupCheck PackedVertexPW1ICTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {20, 2}, {-1, 3}, {-1, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {32, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW1IN
Vector3& PackedVertexPW1IN::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW1IN& PackedVertexPW1IN::operator=(const PackedVertexPW1IN& o) = default;
int PackedVertexPW1IN::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW1IN::GetVertexSize() { return sizeof(PackedVertexPW1IN); }
VertexTypeID PackedVertexPW1IN::GetVertexTypeID() { return static_cast<VertexTypeID>(10); }
const OffsetLookupCheck PackedVertexPW1IN::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {20, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW1INT
Vector3& PackedVertexPW1INT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW1INT& PackedVertexPW1INT::operator=(const PackedVertexPW1INT& o) = default;
int PackedVertexPW1INT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW1INT::GetVertexSize() { return sizeof(PackedVertexPW1INT); }
VertexTypeID PackedVertexPW1INT::GetVertexTypeID() { return static_cast<VertexTypeID>(25); }
const OffsetLookupCheck PackedVertexPW1INT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {20, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {32, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW1INTT
Vector3& PackedVertexPW1INTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW1INTT& PackedVertexPW1INTT::operator=(const PackedVertexPW1INTT& o) = default;
int PackedVertexPW1INTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW1INTT::GetVertexSize() { return sizeof(PackedVertexPW1INTT); }
VertexTypeID PackedVertexPW1INTT::GetVertexTypeID() { return static_cast<VertexTypeID>(48); }
const OffsetLookupCheck PackedVertexPW1INTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {20, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {32, 15},
    {40, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW2IC
Vector3& PackedVertexPW2IC::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW2IC& PackedVertexPW2IC::operator=(const PackedVertexPW2IC& o) = default;
int PackedVertexPW2IC::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW2IC::GetVertexSize() { return sizeof(PackedVertexPW2IC); }
VertexTypeID PackedVertexPW2IC::GetVertexTypeID() { return static_cast<VertexTypeID>(15); }
const OffsetLookupCheck PackedVertexPW2IC::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {24, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {12, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {20, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW2ICT
Vector3& PackedVertexPW2ICT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW2ICT& PackedVertexPW2ICT::operator=(const PackedVertexPW2ICT& o) = default;
int PackedVertexPW2ICT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW2ICT::GetVertexSize() { return sizeof(PackedVertexPW2ICT); }
VertexTypeID PackedVertexPW2ICT::GetVertexTypeID() { return static_cast<VertexTypeID>(30); }
const OffsetLookupCheck PackedVertexPW2ICT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {24, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {12, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {20, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW2ICTT
Vector3& PackedVertexPW2ICTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW2ICTT& PackedVertexPW2ICTT::operator=(const PackedVertexPW2ICTT& o) = default;
int PackedVertexPW2ICTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW2ICTT::GetVertexSize() { return sizeof(PackedVertexPW2ICTT); }
VertexTypeID PackedVertexPW2ICTT::GetVertexTypeID() { return static_cast<VertexTypeID>(53); }
const OffsetLookupCheck PackedVertexPW2ICTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {24, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {12, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {20, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {36, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW2IN
Vector3& PackedVertexPW2IN::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW2IN& PackedVertexPW2IN::operator=(const PackedVertexPW2IN& o) = default;
int PackedVertexPW2IN::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW2IN::GetVertexSize() { return sizeof(PackedVertexPW2IN); }
VertexTypeID PackedVertexPW2IN::GetVertexTypeID() { return static_cast<VertexTypeID>(11); }
const OffsetLookupCheck PackedVertexPW2IN::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {24, 4}, {-1, 5}, {12, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {20, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW2INT
Vector3& PackedVertexPW2INT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW2INT& PackedVertexPW2INT::operator=(const PackedVertexPW2INT& o) = default;
int PackedVertexPW2INT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW2INT::GetVertexSize() { return sizeof(PackedVertexPW2INT); }
VertexTypeID PackedVertexPW2INT::GetVertexTypeID() { return static_cast<VertexTypeID>(26); }
const OffsetLookupCheck PackedVertexPW2INT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {24, 4}, {-1, 5}, {12, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {20, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {36, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW2INTT
Vector3& PackedVertexPW2INTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW2INTT& PackedVertexPW2INTT::operator=(const PackedVertexPW2INTT& o) = default;
int PackedVertexPW2INTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW2INTT::GetVertexSize() { return sizeof(PackedVertexPW2INTT); }
VertexTypeID PackedVertexPW2INTT::GetVertexTypeID() { return static_cast<VertexTypeID>(49); }
const OffsetLookupCheck PackedVertexPW2INTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {24, 4}, {-1, 5}, {12, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {20, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {36, 15},
    {44, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW3IC
Vector3& PackedVertexPW3IC::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW3IC& PackedVertexPW3IC::operator=(const PackedVertexPW3IC& o) = default;
int PackedVertexPW3IC::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW3IC::GetVertexSize() { return sizeof(PackedVertexPW3IC); }
VertexTypeID PackedVertexPW3IC::GetVertexTypeID() { return static_cast<VertexTypeID>(16); }
const OffsetLookupCheck PackedVertexPW3IC::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {28, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {12, 7},
    {-1, 8}, {-1, 9}, {24, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW3ICT
Vector3& PackedVertexPW3ICT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW3ICT& PackedVertexPW3ICT::operator=(const PackedVertexPW3ICT& o) = default;
int PackedVertexPW3ICT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW3ICT::GetVertexSize() { return sizeof(PackedVertexPW3ICT); }
VertexTypeID PackedVertexPW3ICT::GetVertexTypeID() { return static_cast<VertexTypeID>(31); }
const OffsetLookupCheck PackedVertexPW3ICT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {28, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {12, 7},
    {-1, 8}, {-1, 9}, {24, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {32, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW3ICTT
Vector3& PackedVertexPW3ICTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW3ICTT& PackedVertexPW3ICTT::operator=(const PackedVertexPW3ICTT& o) = default;
int PackedVertexPW3ICTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW3ICTT::GetVertexSize() { return sizeof(PackedVertexPW3ICTT); }
VertexTypeID PackedVertexPW3ICTT::GetVertexTypeID() { return static_cast<VertexTypeID>(54); }
const OffsetLookupCheck PackedVertexPW3ICTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {28, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {12, 7},
    {-1, 8}, {-1, 9}, {24, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {32, 15},
    {40, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW3IN
Vector3& PackedVertexPW3IN::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW3IN& PackedVertexPW3IN::operator=(const PackedVertexPW3IN& o) = default;
int PackedVertexPW3IN::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW3IN::GetVertexSize() { return sizeof(PackedVertexPW3IN); }
VertexTypeID PackedVertexPW3IN::GetVertexTypeID() { return static_cast<VertexTypeID>(12); }
const OffsetLookupCheck PackedVertexPW3IN::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {28, 4}, {-1, 5}, {-1, 6}, {12, 7},
    {-1, 8}, {-1, 9}, {24, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW3INT
Vector3& PackedVertexPW3INT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW3INT& PackedVertexPW3INT::operator=(const PackedVertexPW3INT& o) = default;
int PackedVertexPW3INT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW3INT::GetVertexSize() { return sizeof(PackedVertexPW3INT); }
VertexTypeID PackedVertexPW3INT::GetVertexTypeID() { return static_cast<VertexTypeID>(27); }
const OffsetLookupCheck PackedVertexPW3INT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {28, 4}, {-1, 5}, {-1, 6}, {12, 7},
    {-1, 8}, {-1, 9}, {24, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {40, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW3INTT
Vector3& PackedVertexPW3INTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW3INTT& PackedVertexPW3INTT::operator=(const PackedVertexPW3INTT& o) = default;
int PackedVertexPW3INTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW3INTT::GetVertexSize() { return sizeof(PackedVertexPW3INTT); }
VertexTypeID PackedVertexPW3INTT::GetVertexTypeID() { return static_cast<VertexTypeID>(50); }
const OffsetLookupCheck PackedVertexPW3INTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {28, 4}, {-1, 5}, {-1, 6}, {12, 7},
    {-1, 8}, {-1, 9}, {24, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {40, 15},
    {48, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW4IC
Vector3& PackedVertexPW4IC::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW4IC& PackedVertexPW4IC::operator=(const PackedVertexPW4IC& o) = default;
int PackedVertexPW4IC::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW4IC::GetVertexSize() { return sizeof(PackedVertexPW4IC); }
VertexTypeID PackedVertexPW4IC::GetVertexTypeID() { return static_cast<VertexTypeID>(17); }
const OffsetLookupCheck PackedVertexPW4IC::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {32, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {12, 8}, {-1, 9}, {28, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW4ICT
Vector3& PackedVertexPW4ICT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW4ICT& PackedVertexPW4ICT::operator=(const PackedVertexPW4ICT& o) = default;
int PackedVertexPW4ICT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW4ICT::GetVertexSize() { return sizeof(PackedVertexPW4ICT); }
VertexTypeID PackedVertexPW4ICT::GetVertexTypeID() { return static_cast<VertexTypeID>(32); }
const OffsetLookupCheck PackedVertexPW4ICT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {32, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {12, 8}, {-1, 9}, {28, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {36, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW4ICTT
Vector3& PackedVertexPW4ICTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW4ICTT& PackedVertexPW4ICTT::operator=(const PackedVertexPW4ICTT& o) = default;
int PackedVertexPW4ICTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW4ICTT::GetVertexSize() { return sizeof(PackedVertexPW4ICTT); }
VertexTypeID PackedVertexPW4ICTT::GetVertexTypeID() { return static_cast<VertexTypeID>(55); }
const OffsetLookupCheck PackedVertexPW4ICTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {32, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {12, 8}, {-1, 9}, {28, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {36, 15},
    {44, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW4IN
Vector3& PackedVertexPW4IN::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW4IN& PackedVertexPW4IN::operator=(const PackedVertexPW4IN& o) = default;
int PackedVertexPW4IN::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW4IN::GetVertexSize() { return sizeof(PackedVertexPW4IN); }
VertexTypeID PackedVertexPW4IN::GetVertexTypeID() { return static_cast<VertexTypeID>(13); }
const OffsetLookupCheck PackedVertexPW4IN::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {32, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {12, 8}, {-1, 9}, {28, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW4INT
Vector3& PackedVertexPW4INT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW4INT& PackedVertexPW4INT::operator=(const PackedVertexPW4INT& o) = default;
int PackedVertexPW4INT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW4INT::GetVertexSize() { return sizeof(PackedVertexPW4INT); }
VertexTypeID PackedVertexPW4INT::GetVertexTypeID() { return static_cast<VertexTypeID>(28); }
const OffsetLookupCheck PackedVertexPW4INT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {32, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {12, 8}, {-1, 9}, {28, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {44, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPW4INTT
Vector3& PackedVertexPW4INTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPW4INTT& PackedVertexPW4INTT::operator=(const PackedVertexPW4INTT& o) = default;
int PackedVertexPW4INTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPW4INTT::GetVertexSize() { return sizeof(PackedVertexPW4INTT); }
VertexTypeID PackedVertexPW4INTT::GetVertexTypeID() { return static_cast<VertexTypeID>(51); }
const OffsetLookupCheck PackedVertexPW4INTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {32, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {12, 8}, {-1, 9}, {28, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {44, 15},
    {52, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWC
Vector3& PackedVertexPWC::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWC& PackedVertexPWC::operator=(const PackedVertexPWC& o) = default;
int PackedVertexPWC::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWC::GetVertexSize() { return sizeof(PackedVertexPWC); }
VertexTypeID PackedVertexPWC::GetVertexTypeID() { return static_cast<VertexTypeID>(6); }
const OffsetLookupCheck PackedVertexPWC::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {16, 2}, {-1, 3}, {-1, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWCT
Vector3& PackedVertexPWCT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWCT& PackedVertexPWCT::operator=(const PackedVertexPWCT& o) = default;
int PackedVertexPWCT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWCT::GetVertexSize() { return sizeof(PackedVertexPWCT); }
VertexTypeID PackedVertexPWCT::GetVertexTypeID() { return static_cast<VertexTypeID>(33); }
const OffsetLookupCheck PackedVertexPWCT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {16, 2}, {-1, 3}, {-1, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {20, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWCTT
Vector3& PackedVertexPWCTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWCTT& PackedVertexPWCTT::operator=(const PackedVertexPWCTT& o) = default;
int PackedVertexPWCTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWCTT::GetVertexSize() { return sizeof(PackedVertexPWCTT); }
VertexTypeID PackedVertexPWCTT::GetVertexTypeID() { return static_cast<VertexTypeID>(37); }
const OffsetLookupCheck PackedVertexPWCTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {16, 2}, {-1, 3}, {-1, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {20, 15},
    {28, 15}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWN
Vector3& PackedVertexPWN::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWN& PackedVertexPWN::operator=(const PackedVertexPWN& o) = default;
int PackedVertexPWN::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWN::GetVertexSize() { return sizeof(PackedVertexPWN); }
VertexTypeID PackedVertexPWN::GetVertexTypeID() { return static_cast<VertexTypeID>(5); }
const OffsetLookupCheck PackedVertexPWN::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {16, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWNT
Vector3& PackedVertexPWNT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWNT& PackedVertexPWNT::operator=(const PackedVertexPWNT& o) = default;
int PackedVertexPWNT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWNT::GetVertexSize() { return sizeof(PackedVertexPWNT); }
VertexTypeID PackedVertexPWNT::GetVertexTypeID() { return static_cast<VertexTypeID>(21); }
const OffsetLookupCheck PackedVertexPWNT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {16, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWNTA
Vector3& PackedVertexPWNTA::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWNTA& PackedVertexPWNTA::operator=(const PackedVertexPWNTA& o) = default;
int PackedVertexPWNTA::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWNTA::GetVertexSize() { return sizeof(PackedVertexPWNTA); }
VertexTypeID PackedVertexPWNTA::GetVertexTypeID() { return static_cast<VertexTypeID>(42); }
const OffsetLookupCheck PackedVertexPWNTA::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {16, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {36, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWNTAI
Vector3& PackedVertexPWNTAI::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWNTAI& PackedVertexPWNTAI::operator=(const PackedVertexPWNTAI& o) = default;
int PackedVertexPWNTAI::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWNTAI::GetVertexSize() { return sizeof(PackedVertexPWNTAI); }
VertexTypeID PackedVertexPWNTAI::GetVertexTypeID() { return static_cast<VertexTypeID>(45); }
const OffsetLookupCheck PackedVertexPWNTAI::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {16, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {36, 26}, {-1, 27}, {-1, 28}, {48, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWNTT
Vector3& PackedVertexPWNTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWNTT& PackedVertexPWNTT::operator=(const PackedVertexPWNTT& o) = default;
int PackedVertexPWNTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWNTT::GetVertexSize() { return sizeof(PackedVertexPWNTT); }
VertexTypeID PackedVertexPWNTT::GetVertexTypeID() { return static_cast<VertexTypeID>(36); }
const OffsetLookupCheck PackedVertexPWNTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {16, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {36, 15}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWNTTA
Vector3& PackedVertexPWNTTA::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWNTTA& PackedVertexPWNTTA::operator=(const PackedVertexPWNTTA& o) = default;
int PackedVertexPWNTTA::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWNTTA::GetVertexSize() { return sizeof(PackedVertexPWNTTA); }
VertexTypeID PackedVertexPWNTTA::GetVertexTypeID() { return static_cast<VertexTypeID>(43); }
const OffsetLookupCheck PackedVertexPWNTTA::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {16, 4}, {12, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {-1, 9}, {-1, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {28, 15},
    {36, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {44, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWb4IC
Vector3& PackedVertexPWb4IC::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWb4IC& PackedVertexPWb4IC::operator=(const PackedVertexPWb4IC& o) = default;
int PackedVertexPWb4IC::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWb4IC::GetVertexSize() { return sizeof(PackedVertexPWb4IC); }
VertexTypeID PackedVertexPWb4IC::GetVertexTypeID() { return static_cast<VertexTypeID>(9); }
const OffsetLookupCheck PackedVertexPWb4IC::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {20, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {12, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWb4ICNT
Vector3& PackedVertexPWb4ICNT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWb4ICNT& PackedVertexPWb4ICNT::operator=(const PackedVertexPWb4ICNT& o) = default;
int PackedVertexPWb4ICNT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWb4ICNT::GetVertexSize() { return sizeof(PackedVertexPWb4ICNT); }
VertexTypeID PackedVertexPWb4ICNT::GetVertexTypeID() { return static_cast<VertexTypeID>(24); }
const OffsetLookupCheck PackedVertexPWb4ICNT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {20, 2}, {-1, 3}, {24, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {12, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {36, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWb4ICT
Vector3& PackedVertexPWb4ICT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWb4ICT& PackedVertexPWb4ICT::operator=(const PackedVertexPWb4ICT& o) = default;
int PackedVertexPWb4ICT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWb4ICT::GetVertexSize() { return sizeof(PackedVertexPWb4ICT); }
VertexTypeID PackedVertexPWb4ICT::GetVertexTypeID() { return static_cast<VertexTypeID>(23); }
const OffsetLookupCheck PackedVertexPWb4ICT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {20, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {12, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWb4ICTT
Vector3& PackedVertexPWb4ICTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWb4ICTT& PackedVertexPWb4ICTT::operator=(const PackedVertexPWb4ICTT& o) = default;
int PackedVertexPWb4ICTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWb4ICTT::GetVertexSize() { return sizeof(PackedVertexPWb4ICTT); }
VertexTypeID PackedVertexPWb4ICTT::GetVertexTypeID() { return static_cast<VertexTypeID>(47); }
const OffsetLookupCheck PackedVertexPWb4ICTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {20, 2}, {-1, 3}, {-1, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {12, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {24, 15},
    {32, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWb4IN
Vector3& PackedVertexPWb4IN::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWb4IN& PackedVertexPWb4IN::operator=(const PackedVertexPWb4IN& o) = default;
int PackedVertexPWb4IN::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWb4IN::GetVertexSize() { return sizeof(PackedVertexPWb4IN); }
VertexTypeID PackedVertexPWb4IN::GetVertexTypeID() { return static_cast<VertexTypeID>(8); }
const OffsetLookupCheck PackedVertexPWb4IN::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {20, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {12, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {-1, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWb4INT
Vector3& PackedVertexPWb4INT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWb4INT& PackedVertexPWb4INT::operator=(const PackedVertexPWb4INT& o) = default;
int PackedVertexPWb4INT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWb4INT::GetVertexSize() { return sizeof(PackedVertexPWb4INT); }
VertexTypeID PackedVertexPWb4INT::GetVertexTypeID() { return static_cast<VertexTypeID>(22); }
const OffsetLookupCheck PackedVertexPWb4INT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {20, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {12, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {32, 15},
    {-1, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexPWb4INTT
Vector3& PackedVertexPWb4INTT::operator=(const Vector3& p)
{
    v[0] = p.x; v[1] = p.y; v[2] = p.z;
    return *reinterpret_cast<Vector3*>(v);
}
PackedVertexPWb4INTT& PackedVertexPWb4INTT::operator=(const PackedVertexPWb4INTT& o) = default;
int PackedVertexPWb4INTT::GetItemOffset(VertexItemType type) { return vertexItemOffset[type].offset; }
int PackedVertexPWb4INTT::GetVertexSize() { return sizeof(PackedVertexPWb4INTT); }
VertexTypeID PackedVertexPWb4INTT::GetVertexTypeID() { return static_cast<VertexTypeID>(46); }
const OffsetLookupCheck PackedVertexPWb4INTT::vertexItemOffset[kVertexItemTypes] = {
    {-1, 0}, {0, 1}, {-1, 2}, {-1, 3}, {20, 4}, {-1, 5}, {-1, 6}, {-1, 7},
    {-1, 8}, {12, 9}, {16, 10}, {-1, 11}, {-1, 12}, {-1, 13}, {-1, 14}, {32, 15},
    {40, 16}, {-1, 17}, {-1, 18}, {-1, 19}, {-1, 20}, {-1, 21}, {-1, 22}, {-1, 23},
    {-1, 24}, {-1, 25}, {-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}, {-1, 30}, {-1, 31},
    {-1, 32}, {-1, 33}, {-1, 34}, {-1, 35}, {-1, 36}, {-1, 37}, {-1, 38}};

// PackedVertexT
PackedVertexT& PackedVertexT::operator=(const PackedVertexT& o) = default;
Vector2& PackedVertexT::GetTexCoord() { return *reinterpret_cast<Vector2*>(Bytes(0)); }
const Vector2& PackedVertexT::GetTexCoord() const { return *reinterpret_cast<const Vector2*>(Bytes(0)); }
void PackedVertexT::SetTexCoord(const Vector2& p) { v[0] = p.x; v[1] = p.y; }

// PackedVertexTT
PackedVertexTT& PackedVertexTT::operator=(const PackedVertexTT& o) = default;
Vector2& PackedVertexTT::GetTexCoord2() { return *reinterpret_cast<Vector2*>(Bytes(0)); }
const Vector2& PackedVertexTT::GetTexCoord2() const { return *reinterpret_cast<const Vector2*>(Bytes(0)); }
void PackedVertexTT::SetTexCoord2(const Vector2& p) { v[0] = p.x; v[1] = p.y; }

// PackedVertexW1I
PackedVertexW1I& PackedVertexW1I::operator=(const PackedVertexW1I& o) = default;

// PackedVertexW2I
PackedVertexW2I& PackedVertexW2I::operator=(const PackedVertexW2I& o) = default;

// PackedVertexW3I
PackedVertexW3I& PackedVertexW3I::operator=(const PackedVertexW3I& o) = default;

// PackedVertexW4
PackedVertexW4& PackedVertexW4::operator=(const PackedVertexW4& o) = default;

// PackedVertexW4I
PackedVertexW4I& PackedVertexW4I::operator=(const PackedVertexW4I& o) = default;

// PackedVertexW
PackedVertexW& PackedVertexW::operator=(const PackedVertexW& o) = default;

// PackedVertexWb4I
PackedVertexWb4I& PackedVertexWb4I::operator=(const PackedVertexWb4I& o) = default;
void PackedVertexWb4I::SetWeights(unsigned long a, unsigned long b) { u[0] = a; u[1] = b; }

// StreamedVertexC
StreamedVertexC& StreamedVertexC::operator=(const StreamedVertexC& o) = default;

// StreamedVertexPCTBase
StreamedVertexPCTBase& StreamedVertexPCTBase::operator=(const StreamedVertexPCTBase& o) = default;
VertexTypeID StreamedVertexPCTBase::GetVertexTypeID() { return static_cast<VertexTypeID>(98); }
const int StreamedVertexPCTBase::sizeLookup[4] = {24, 12, 4, 8};
const CheckVertexItemTypeInt StreamedVertexPCTBase::vertexItemOffset[kVertexItemTypes] = {
    {-1, -1, 0}, {0, 0, 1}, {0, 1, 2}, {-1, -1, 3}, {-1, -1, 4}, {-1, -1, 5}, {-1, -1, 6}, {-1, -1, 7},
    {-1, -1, 8}, {-1, -1, 9}, {-1, -1, 10}, {-1, -1, 11}, {-1, -1, 12}, {-1, -1, 13}, {-1, -1, 14}, {0, 2, 15},
    {-1, -1, 16}, {-1, -1, 17}, {-1, -1, 18}, {-1, -1, 19}, {-1, -1, 20}, {-1, -1, 21}, {-1, -1, 22}, {-1, -1, 23},
    {-1, -1, 24}, {-1, -1, 25}, {-1, -1, 26}, {-1, -1, 27}, {-1, -1, 28}, {-1, -1, 29}, {-1, -1, 30}, {-1, -1, 31},
    {-1, -1, 32}, {-1, -1, 33}, {-1, -1, 34}, {-1, -1, 35}, {-1, -1, 36}, {-1, -1, 37}, {-1, -1, 38}};

// StreamedVertexPNTBase
StreamedVertexPNTBase& StreamedVertexPNTBase::operator=(const StreamedVertexPNTBase& o) = default;
VertexTypeID StreamedVertexPNTBase::GetVertexTypeID() { return static_cast<VertexTypeID>(97); }
const int StreamedVertexPNTBase::sizeLookup[4] = {32, 12, 12, 8};
const CheckVertexItemTypeInt StreamedVertexPNTBase::vertexItemOffset[kVertexItemTypes] = {
    {-1, -1, 0}, {0, 0, 1}, {-1, -1, 2}, {-1, -1, 3}, {0, 1, 4}, {-1, -1, 5}, {-1, -1, 6}, {-1, -1, 7},
    {-1, -1, 8}, {-1, -1, 9}, {-1, -1, 10}, {-1, -1, 11}, {-1, -1, 12}, {-1, -1, 13}, {-1, -1, 14}, {0, 2, 15},
    {-1, -1, 16}, {-1, -1, 17}, {-1, -1, 18}, {-1, -1, 19}, {-1, -1, 20}, {-1, -1, 21}, {-1, -1, 22}, {-1, -1, 23},
    {-1, -1, 24}, {-1, -1, 25}, {-1, -1, 26}, {-1, -1, 27}, {-1, -1, 28}, {-1, -1, 29}, {-1, -1, 30}, {-1, -1, 31},
    {-1, -1, 32}, {-1, -1, 33}, {-1, -1, 34}, {-1, -1, 35}, {-1, -1, 36}, {-1, -1, 37}, {-1, -1, 38}};

}  // namespace Jet
