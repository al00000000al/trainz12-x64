// KernelStandard_7c unit U3 (notes/KernelStandard.md): small value types and leaf helpers. Declarations match
// the exported names (struct vs class and calling conventions are part of them); layouts are the TS12 ones.
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Platform.h>

namespace Jet {

class Bitmap;  // KernelStandard; ImageProcessing reads its {width, height, ..., data +0xc, pitch +0x10, format +0x18}

// intrusive list and tree nodes
struct DLListNode {
    DLListNode();
    DLListNode& operator=(const DLListNode& o);
    DLListNode* Next() const;
    DLListNode* Prev() const;
    DLListNode* next;  // +0
    DLListNode* prev;  // +4
};

struct IListNode {
    IListNode();
    IListNode& operator=(const IListNode& o);
    void* first;
    void* second;
};

struct ISingleListNode {
    ISingleListNode();
    ISingleListNode& operator=(const ISingleListNode& o);
    void* next;
};

struct DLTreeNode {
    DLTreeNode();
    DLTreeNode& operator=(const DLTreeNode& o);
    unsigned int Count() const;     // children
    DLTreeNode* Next() const;       // next sibling
    DLTreeNode* Prev() const;
    DLTreeNode* Child() const;      // = FirstChild
    DLTreeNode* FirstChild() const;
    DLTreeNode* LastChild() const;
    DLTreeNode* Parent() const;
    DLTreeNode* WalkNext() const;   // next sibling, wrapping to the first
    DLTreeNode* WalkPrev() const;   // previous sibling, wrapping to the last
    DLTreeNode* NextNode() const;   // next sibling of this node or of the nearest ancestor that has one
    DLTreeNode* Walk() const;       // pre-order successor
    unsigned int count;     // +0x00
    DLTreeNode* next;       // +0x04
    DLTreeNode* prev;       // +0x08
    DLTreeNode* firstChild; // +0x0c
    DLTreeNode* lastChild;  // +0x10
    DLTreeNode* parent;     // +0x14
};
JET_LAYOUT32(sizeof(DLTreeNode) == 0x18);

struct RBTreeNode {
    RBTreeNode();
    RBTreeNode(const RBTreeNode& o);
    virtual ~RBTreeNode();
    RBTreeNode& operator=(const RBTreeNode& o);
    RBTreeNode* Left() const;
    RBTreeNode* Right() const;
    JET_HEAP_OBJECT
    static void* operator new[](size_t n) { return Alloc::New(static_cast<unsigned int>(n)); }
    static void operator delete[](void* p) { Alloc::Delete(p); }
    RBTreeNode* left;   // +0x04
    RBTreeNode* right;  // +0x08
    bool red;           // +0x0c
};
JET_LAYOUT32(sizeof(RBTreeNode) == 0x10);

class ProtectedLong {
public:
    ProtectedLong();
    ProtectedLong(long v);
    ProtectedLong(const ProtectedLong& o);
    ~ProtectedLong();
    ProtectedLong& operator=(const ProtectedLong& o);
    long __cdecl GetData() const;
    long __cdecl Increment();  // interlocked, returns the new value
    long __cdecl Decrement();
    static void __cdecl Init();
    static void __cdecl Done();

private:
    long data;
};

// a base pointer and an offset; IncPtr is the pointer arithmetic helper
class DynamicPtr {
public:
    DynamicPtr();
    DynamicPtr(void* base);
    DynamicPtr& operator=(const DynamicPtr& o);
    void Rewind();

protected:
    void* IncPtr(void* p, unsigned long n) const;

private:
    void* base;            // +0
    unsigned long offset;  // +4
};

class MousePointer {
public:
    MousePointer& operator=(const MousePointer&);
    static void __cdecl Hide();
    static void __cdecl Show();
};

// build stamps: TS12 ships the unfilled placeholders
class BuildVersion {
public:
    BuildVersion& operator=(const BuildVersion&);
    static float __cdecl GetJetBuildVersion();
    static const char* __cdecl GetJetBuildVersionString();
    static const char* __cdecl GetJetBuildString();
    static const char* __cdecl GetJetBuildChangeNumString();
};

class Date {
public:
    Date& operator=(const Date&);
    static void __cdecl Str(char* out);       // _strdate: "MM/DD/YY"
    static void __cdecl YYYYMMDD(char* out);  // 9 bytes; "ERROR" when the system call fails
};

class HSVColor;

class Color {
public:
    Color& Convert(const HSVColor& hsv);  // alpha becomes 1
    float r, g, b, a;
};

class HSVColor {
public:
    HSVColor& __cdecl Convert(const Color& c);  // h in [0,1) (-1 for grey), s, v; the 4th float is untouched
    float h, s, v, a;
};

// 0xAARRGGBB constants
struct PackedColor {
    unsigned int argb;
    static const PackedColor black, blue, cyan, darkGray, gray, green, lightGray, magenta, orange, pink, red, white,
        yellow;
};

// a 256-entry byte lookup table applied to the first three channels of 32-bit bitmaps
class ImageProcessing {
public:
    ImageProcessing();
    ~ImageProcessing();
    ImageProcessing& operator=(const ImageProcessing& o);
    void SetupIdentity();
    void SetupBrightness(float b);  // clamps b to [0, 2]; adds (b - 1) * 255 to every entry
    void SetupContrast(float c);    // clamps c to [0, 2]; (entry - 128) * c + 128
    bool Process(Bitmap& bm);
    bool AdjustColor(float saturation, Bitmap& bm);

private:
    unsigned char table[256];
};

}  // namespace Jet
