// Jet::OptimalRects (KernelStandard 0x10034380..0x10035219): a set of non-overlapping dirty rectangles.
//
// Invalidate puts a rectangle on a pending list (or straight into the set when the set is empty), then takes
// pending rectangles one by one and compares each with the set: one that overlaps nothing joins the set; else
// the first set member it touches decides, by which sides of the new rectangle stick out past it (Classify),
// how the new one (or that member) is cut, and the pieces go back on the pending list. Process runs the same
// loop. Rect = {x, y, w, h}.
//
// Kept as in the original: the right/lower strips cut off in cases 3, 6, 10 (x = member's last column) and
// 12 (y = member's last row, one row short at the bottom) overlap the member by one; members cut down to
// nothing are freed. Nodes are 0x18 bytes from Jet::Alloc: {Rect, link {next, prev}} where the links point at
// the neighbours' link fields.
#include <jet/KernelStandard.h>
#include <jet/Math.h>

#include <cstddef>
#include <cstdint>

namespace Jet {

namespace {
struct Link {
    Link* next;
    Link* prev;
};
}  // namespace

struct OptimalRects::Node {
    Rect r;     // +0x00
    Link link;  // +0x10
};
JET_LAYOUT32(sizeof(OptimalRects) == 0x18);

namespace {

using Node = OptimalRects::Node;
static_assert(offsetof(Node, link) == sizeof(Rect));
JET_LAYOUT32(sizeof(Node) == 0x18);

struct List {
    Node* head;
    Node* tail;
    int count;
};

Node* FromLink(Link* l) { return l ? reinterpret_cast<Node*>(reinterpret_cast<char*>(l) - offsetof(Node, link)) : nullptr; }
Node* Next(Node* n) { return FromLink(n->link.next); }

Node* NewNode(const Rect& r)  // 0x100344d0 after Alloc::New(0x18)
{
    auto* n = static_cast<Node*>(Alloc::New(sizeof(Node)));
    if (n) {
        n->r = r;
        n->link.next = nullptr;
        n->link.prev = nullptr;
    }
    return n;
}

void Append(List& l, Node* n)  // 0x10035160 and its inlined copies; n's own next link is expected to be null
{
    if (!n) return;
    if (l.tail) {
        n->link.prev = &l.tail->link;
        l.tail->link.next = &n->link;
    } else {
        l.head = n;
    }
    ++l.count;
    l.tail = n;
}

void Unlink(List& l, Node* n)  // 0x10035190
{
    Link* prev = n->link.prev;
    Link* next = n->link.next;
    if (prev)
        prev->next = next;
    else
        l.head = FromLink(next);
    if (next)
        next->prev = prev;
    else
        l.tail = FromLink(prev);
    n->link.next = nullptr;
    n->link.prev = nullptr;
    --l.count;
}

// 0x10034380: low nibble = sides of a sticking out past b (1 left, 2 right, 4 top, 8 bottom), high nibble set
// when a lies wholly outside b on that side
uint8_t Classify(const Rect& a, const Rect& b)
{
    uint8_t c = 0;
    const int aRight = a.w - 1 + a.x, bRight = b.w - 1 + b.x;
    if (a.x < b.x) c = (b.x <= aRight) ? 0x01 : 0x10;
    if (bRight < aRight) c |= (a.x <= bRight) ? 0x02 : 0x20;
    const int aBottom = a.h - 1 + a.y, bBottom = b.h - 1 + b.y;
    if (a.y < b.y) c |= (b.y <= aBottom) ? 0x04 : 0x40;
    if (bBottom < aBottom) c |= (a.y <= bBottom) ? 0x08 : 0x80;
    return c;
}

void DropMember(List& set, Node* b)
{
    Unlink(set, b);
    Alloc::Delete(b);
}

// one pending rectangle `a` against set member `b` they overlap (0x100344f0..0x10034bf0, by case)
void Split(List& set, List& pending, Node* an, Node* bn, int which)
{
    Rect& a = an->r;
    Rect& b = bn->r;
    switch (which) {
    case 0:  // a inside b
        Alloc::Delete(an);
        return;
    case 1:  // left
        a.w = b.x - a.x;
        break;
    case 2: {  // right
        const int d = a.x - b.w - b.x;
        a.w += d;
        a.x = b.w + b.x;
        break;
    }
    case 3:  // left and right
        Append(pending, NewNode({b.x + b.w - 1, a.y, a.x + (a.w - b.x - b.w) + 1, a.h}));
        a.w = b.x - a.x;
        break;
    case 4:  // top
        a.h = b.y - a.y;
        break;
    case 5:  // left and top
        Append(pending, NewNode({a.x, b.y, b.x - a.x, (a.h + a.y) - b.y}));
        a.h = b.y - a.y;
        break;
    case 6:  // right and top
        Append(pending, NewNode({b.x + b.w - 1, b.y, a.x + (a.w - b.x - b.w) + 1, (a.h + a.y) - b.y}));
        a.h = b.y - a.y;
        break;
    case 7: {  // left, right and top: b loses its covered top part
        const int d = (a.h + a.y) - b.y;
        b.h -= d;
        b.y += d;
        if (b.h <= 0) DropMember(set, bn);
        break;
    }
    case 8: {  // bottom
        const int d = a.y - b.y - b.h;
        a.h += d;
        a.y = b.y + b.h;
        break;
    }
    case 9: {  // left and bottom
        Append(pending, NewNode({a.x, a.y, b.x - a.x, (b.h + b.y) - a.y}));
        const int d = a.y - b.h - b.y;
        a.h += d;
        a.y = b.h + b.y;
        break;
    }
    case 10: {  // right and bottom
        Append(pending, NewNode({b.x + b.w - 1, a.y, a.x + (a.w - b.x - b.w) + 1, (b.h + b.y) - a.y}));
        const int d = a.y - b.h - b.y;
        a.h += d;
        a.y = b.h + b.y;
        break;
    }
    case 11:  // left, right and bottom: b keeps its top part
        b.h = a.y - b.y;
        if (b.h <= 0) DropMember(set, bn);
        break;
    case 12:  // top and bottom
        Append(pending, NewNode({a.x, b.y + b.h - 1, a.w, (a.h + a.y) - b.y - b.h}));
        a.h = b.y - a.y;
        break;
    case 13: {  // left, top and bottom: b keeps its right part
        const int d = (a.w - b.x) + a.x;
        b.w -= d;
        b.x += d;
        if (b.w <= 0) DropMember(set, bn);
        break;
    }
    case 14:  // right, top and bottom: b keeps its left part
        b.w = a.x - b.x;
        if (b.w <= 0) DropMember(set, bn);
        break;
    default:  // 15: a covers b
        DropMember(set, bn);
        break;
    }
    Append(pending, an);
}

// the loop shared by Invalidate (0x10034e30) and Process (0x10034c40) for one pending rectangle
void Settle(List& set, List& pending, Node* a)
{
    for (Node* b = set.head; b; b = Next(b)) {
        const uint8_t c = Classify(a->r, b->r);
        if (c & 0xf0) continue;
        Split(set, pending, a, b, c & 0x0f);
        return;
    }
    Append(set, a);
}

}  // namespace

void __cdecl OptimalRects::Invalidate(const Rect& r)
{
    if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0) return;
    List& set = *reinterpret_cast<List*>(&head);
    List& pending = *reinterpret_cast<List*>(&pendingHead);
    if (set.count == 0) {
        Append(set, NewNode(r));
        return;
    }
    Append(pending, NewNode(r));
    while (pending.count != 0) {
        Node* a = pending.head;
        if (a) Unlink(pending, a);
        if (!a) {
            if (pending.count == 0) break;
            continue;
        }
        Settle(set, pending, a);
    }
}

void __cdecl OptimalRects::Validate(const Rect&) {}  // 0x1002abe0: an empty function in TS12

void __cdecl OptimalRects::Process()
{
    List& set = *reinterpret_cast<List*>(&head);
    List& pending = *reinterpret_cast<List*>(&pendingHead);
    while (Node* a = pending.head) {
        Unlink(pending, a);
        Settle(set, pending, a);
    }
}

// 0x10034420: bounding box; an empty side (w or h < 1) takes the other's span
static Rect Union(const Rect& a, const Rect& b)
{
    Rect o;
    int w;
    if (a.w < 1) {
        o.x = b.x;
        w = b.w;
    } else if (b.w < 1) {
        o.x = a.x;
        w = a.w;
    } else {
        o.x = a.x <= b.x ? a.x : b.x;
        const int br = b.x - 1 + b.w, ar = a.x - 1 + a.w;
        w = (ar < br ? br : ar) - o.x + 1;
    }
    o.w = w;
    if (a.h < 1) {
        o.y = b.y;
        o.h = b.h;
    } else if (b.h < 1) {
        o.y = a.y;
        o.h = a.h;
    } else {
        o.y = a.y <= b.y ? a.y : b.y;
        const int bb = b.h - 1 + b.y, ab = a.y - 1 + a.h;
        o.h = (ab < bb ? bb : ab) - o.y + 1;
    }
    return o;
}

Rect __cdecl OptimalRects::Combine()  // 0x100350d0
{
    Rect r = {0, 0, 0, 0};
    if (Node* n = head) {
        r = n->r;
        for (n = Next(n); n; n = Next(n)) r = Union(r, n->r);
    }
    return r;
}

}  // namespace Jet
