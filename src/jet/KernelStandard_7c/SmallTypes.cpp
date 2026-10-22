// Unit U3 of KernelStandard_7c: list/tree nodes, ProtectedLong, DynamicPtr, MousePointer, BuildVersion, Date,
// Color/HSVColor, PackedColor, ImageProcessing. Behaviour follows the TS12 assembly (addresses per function);
// many of these share one body in the original (identical-code folding).
#include <jet/SmallTypes.h>

#include <windows.h>

#include <cstdio>
#include <ctime>

namespace Jet {

// ---------------------------------------------------------------------------------------------------------------
// nodes (0x10014720, 0x10014f70, 0x10014f90, 0x10014fa0, 0x1002e040..0x1002e118, 0x1003d260..0x1003d334)

DLListNode::DLListNode() : next(nullptr), prev(nullptr) {}
DLListNode& DLListNode::operator=(const DLListNode& o)
{
    next = o.next;
    prev = o.prev;
    return *this;
}
DLListNode* DLListNode::Next() const { return next; }
DLListNode* DLListNode::Prev() const { return prev; }

IListNode::IListNode() : first(nullptr), second(nullptr) {}
IListNode& IListNode::operator=(const IListNode& o)
{
    first = o.first;
    second = o.second;
    return *this;
}

ISingleListNode::ISingleListNode() : next(nullptr) {}
ISingleListNode& ISingleListNode::operator=(const ISingleListNode& o)
{
    next = o.next;
    return *this;
}

DLTreeNode::DLTreeNode() : count(0), next(nullptr), prev(nullptr), firstChild(nullptr), lastChild(nullptr), parent(nullptr) {}
DLTreeNode& DLTreeNode::operator=(const DLTreeNode& o)
{
    count = o.count;
    next = o.next;
    prev = o.prev;
    firstChild = o.firstChild;
    lastChild = o.lastChild;
    parent = o.parent;
    return *this;
}
unsigned int DLTreeNode::Count() const { return count; }
DLTreeNode* DLTreeNode::Next() const { return next; }
DLTreeNode* DLTreeNode::Prev() const { return prev; }
DLTreeNode* DLTreeNode::Child() const { return firstChild; }
DLTreeNode* DLTreeNode::FirstChild() const { return firstChild; }
DLTreeNode* DLTreeNode::LastChild() const { return lastChild; }
DLTreeNode* DLTreeNode::Parent() const { return parent; }

DLTreeNode* DLTreeNode::WalkNext() const
{
    if (next) return next;
    return parent ? parent->firstChild : nullptr;
}

DLTreeNode* DLTreeNode::WalkPrev() const
{
    if (prev) return prev;
    return parent ? parent->lastChild : nullptr;
}

DLTreeNode* DLTreeNode::NextNode() const
{
    for (const DLTreeNode* n = this; n; n = n->parent)
        if (n->next) return n->next;
    return nullptr;
}

DLTreeNode* DLTreeNode::Walk() const
{
    if (firstChild) return firstChild;
    if (next) return next;
    return parent ? parent->NextNode() : nullptr;
}

RBTreeNode::RBTreeNode() : left(nullptr), right(nullptr), red(false) {}
RBTreeNode::RBTreeNode(const RBTreeNode& o) : left(o.left), right(o.right), red(o.red) {}
RBTreeNode::~RBTreeNode() {}
RBTreeNode& RBTreeNode::operator=(const RBTreeNode& o)
{
    left = o.left;
    right = o.right;
    red = o.red;
    return *this;
}
RBTreeNode* RBTreeNode::Left() const { return left; }
RBTreeNode* RBTreeNode::Right() const { return right; }

// ---------------------------------------------------------------------------------------------------------------
// ProtectedLong (0x10001860..0x1000189b), DynamicPtr (0x1002e1e0..0x1002e22a), MousePointer (0x100329e0)

ProtectedLong::ProtectedLong() : data(0) {}
ProtectedLong::ProtectedLong(long v) : data(v) {}
ProtectedLong::ProtectedLong(const ProtectedLong& o) : data(o.data) {}
ProtectedLong::~ProtectedLong() {}
ProtectedLong& ProtectedLong::operator=(const ProtectedLong& o)
{
    data = o.data;
    return *this;
}
long __cdecl ProtectedLong::GetData() const { return data; }
long __cdecl ProtectedLong::Increment() { return InterlockedIncrement(&data); }
long __cdecl ProtectedLong::Decrement() { return InterlockedDecrement(&data); }
void __cdecl ProtectedLong::Init() {}
void __cdecl ProtectedLong::Done() {}

DynamicPtr::DynamicPtr() : base(nullptr), offset(0) {}
DynamicPtr::DynamicPtr(void* b) : base(b), offset(0) {}
DynamicPtr& DynamicPtr::operator=(const DynamicPtr& o)
{
    base = o.base;
    offset = o.offset;
    return *this;
}
void DynamicPtr::Rewind() { offset = 0; }
void* DynamicPtr::IncPtr(void* p, unsigned long n) const { return static_cast<char*>(p) + n; }

MousePointer& MousePointer::operator=(const MousePointer&) { return *this; }
void __cdecl MousePointer::Hide() { ShowCursor(FALSE); }
void __cdecl MousePointer::Show() { ShowCursor(TRUE); }

// ---------------------------------------------------------------------------------------------------------------
// BuildVersion (0x1000dad0..0x1000db05), Date (0x1000dd60, 0x1000dd70)

BuildVersion& BuildVersion::operator=(const BuildVersion&) { return *this; }
float __cdecl BuildVersion::GetJetBuildVersion()
{
    static const float version = 1234.5f;  // a static in the original (0x1007359c)
    return version;
}
const char* __cdecl BuildVersion::GetJetBuildVersionString() { return "1234.5"; }
const char* __cdecl BuildVersion::GetJetBuildString() { return "RRRRR.RR"; }
const char* __cdecl BuildVersion::GetJetBuildChangeNumString() { return "QQQQQ"; }

Date& Date::operator=(const Date&) { return *this; }
void __cdecl Date::Str(char* out) { _strdate(out); }
void __cdecl Date::YYYYMMDD(char* out)
{
    if (!GetDateFormatA(0, 0, nullptr, "yyyyMMdd", out, 9)) std::sprintf(out, "ERROR");
}

// ---------------------------------------------------------------------------------------------------------------
// Color / HSVColor (0x1000de00, 0x1000df60), single precision throughout

Color& Color::Convert(const HSVColor& hsv)
{
    const float s = hsv.s, v = hsv.v;
    if (s == 0.0f) {
        b = v;
        g = v;
        r = v;
        a = 1.0f;
        return *this;
    }
    const float h = (hsv.h == 1.0f) ? 0.0f : hsv.h * 6.0f;
    const int i = static_cast<int>(h);
    const float f = h - static_cast<float>(i);
    const float p = (1.0f - s) * v;
    const float q = (1.0f - f * s) * v;
    const float t = (1.0f - (1.0f - f) * s) * v;
    switch (static_cast<unsigned int>(i)) {
    case 0: g = t; b = p; r = v; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: g = p; b = q; r = v; break;
    default: break;  // r, g, b left as they are
    }
    a = 1.0f;
    return *this;
}

HSVColor& __cdecl HSVColor::Convert(const Color& c)
{
    float mx = (c.b > c.g) ? c.b : c.g;
    if (!(mx > c.r)) mx = c.r;
    float mn = (c.g > c.b) ? c.b : c.g;
    if (!(c.r > mn)) mn = c.r;
    const float d = mx - mn;
    v = mx;
    s = (mx == 0.0f) ? 0.0f : d / mx;
    if (s == 0.0f) {
        h = -1.0f;
        return *this;
    }
    if (c.r == mx)
        h = (c.g - c.b) / d;
    else if (c.g == mx)
        h = (c.b - c.r) / d + 2.0f;
    else
        h = (c.r - c.g) / d + 4.0f;
    h = h * 60.0f;
    if (0.0f > h) h = h + 360.0f;
    h = h * (1.0f / 360.0f);
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------
// PackedColor (0x100741dc..0x1007420c)

const PackedColor PackedColor::black = {0xFF000000u};
const PackedColor PackedColor::blue = {0xFF0000FFu};
const PackedColor PackedColor::cyan = {0xFF00FFFFu};
const PackedColor PackedColor::darkGray = {0xFF404040u};
const PackedColor PackedColor::gray = {0xFF808080u};
const PackedColor PackedColor::green = {0xFF00FF00u};
const PackedColor PackedColor::lightGray = {0xFFC0C0C0u};
const PackedColor PackedColor::magenta = {0xFFFF00FFu};
const PackedColor PackedColor::orange = {0xFFFFC800u};
const PackedColor PackedColor::pink = {0xFFFF40C8u};
const PackedColor PackedColor::red = {0xFFFF0000u};
const PackedColor PackedColor::white = {0xFFFFFFFFu};
const PackedColor PackedColor::yellow = {0xFFFFFF00u};

// ---------------------------------------------------------------------------------------------------------------
// ImageProcessing (0x1002aba0..0x1002ae6c)

namespace {
struct BitmapView {  // the fields of Jet::Bitmap that ImageProcessing reads
    unsigned int width;   // +0x00
    unsigned int height;  // +0x04
    unsigned int pad8;
    unsigned char* data;  // +0x0c
    unsigned int pitch;   // +0x10
    unsigned int pad14;
    int format;           // +0x18: 2 = 32-bit
};
JET_LAYOUT32(sizeof(BitmapView) == 0x1c);

float Clamp02(float x)
{
    float r = 0.0f;
    if (x > 0.0f) {
        r = x;
        if (x > 2.0f) r = 2.0f;
    }
    return r;
}

unsigned char ClampByte(int v) { return static_cast<unsigned char>(v > 0 ? (v > 255 ? 255 : v) : 0); }
}  // namespace

ImageProcessing::ImageProcessing() { SetupIdentity(); }
ImageProcessing::~ImageProcessing() {}
ImageProcessing& ImageProcessing::operator=(const ImageProcessing& o)
{
    for (int i = 0; i < 256; ++i) table[i] = o.table[i];
    return *this;
}

void ImageProcessing::SetupIdentity()
{
    for (int i = 0; i < 256; ++i) table[i] = static_cast<unsigned char>(i);
}

void ImageProcessing::SetupBrightness(float b)
{
    const int add = static_cast<int>((Clamp02(b) - 1.0f) * 255.0f);
    for (int i = 0; i < 256; ++i) table[i] = ClampByte(table[i] + add);
}

void ImageProcessing::SetupContrast(float c)
{
    const float k = Clamp02(c);
    for (int i = 0; i < 256; ++i) table[i] = ClampByte(static_cast<int>(static_cast<float>(table[i] - 0x80) * k + 128.0f));
}

bool ImageProcessing::Process(Bitmap& bitmap)
{
    BitmapView& bm = reinterpret_cast<BitmapView&>(bitmap);
    if (bm.format != 2) return false;
    unsigned char* p = bm.data;
    const unsigned int skip = bm.pitch - bm.width * 4;
    for (unsigned int y = 0; y < bm.height; ++y, p += skip)
        for (unsigned int x = 0; x < bm.width; ++x, p += 4) {
            p[0] = table[p[0]];
            p[1] = table[p[1]];
            p[2] = table[p[2]];
        }
    return true;
}

// Each channel moves away from the pixel's average by `saturation`. The original stores the low byte of the
// truncated value (its clamp compares the byte, so it never triggers).
bool ImageProcessing::AdjustColor(float saturation, Bitmap& bitmap)
{
    BitmapView& bm = reinterpret_cast<BitmapView&>(bitmap);
    if (bm.format != 2) return false;
    unsigned char* p = bm.data;
    const unsigned int skip = bm.pitch - bm.width * 4;
    for (unsigned int y = 0; y < bm.height; ++y, p += skip)
        for (unsigned int x = 0; x < bm.width; ++x, p += 4) {
            const int avg = (p[0] + p[2] + p[1]) / 3;
            const float favg = static_cast<float>(avg);
            for (int k = 0; k < 3; ++k)
                p[k] = static_cast<unsigned char>(static_cast<int>(static_cast<float>(p[k] - avg) * saturation + favg));
        }
    return true;
}

}  // namespace Jet
