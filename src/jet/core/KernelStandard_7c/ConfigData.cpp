// Unit U7b1 of KernelStandard_7c: ConfigData, the tree and its values (0x1000e0b0..0x10011830, 0x10013b10..
// 0x10013f00, 0x10014150..0x10014260). Notes: notes/KernelStandard.md "U7b1". The nodes come from the pool in
// Memory.cpp (operator new / delete); the missing-key node is unknownItem.
#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

#include <xmmintrin.h>

namespace Jet {

ConfigData ConfigData::unknownItem;  // 0x100ae168

namespace {

// a PString kept in a raw pointer slot (the data field, the items of a PString array)
PString& AsPString(void*& slot) { return reinterpret_cast<PString&>(slot); }
const PString& AsPString(void* const& slot) { return reinterpret_cast<const PString&>(slot); }
const char* Text(const PString& p) { return p.node ? p.node->text : nullptr; }

float AsFloat(void* const& slot)
{
    float f;
    std::memcpy(&f, &slot, 4);
    return f;
}

// what fld / fstp do to a float (a signalling NaN comes out quiet)
float ThroughX87(float f) { return static_cast<float>(static_cast<double>(f)); }

// 0x1000f960: the float as _gcvt prints it with 8 digits, "5." becoming "5.0" (a static buffer, as the original)
const char* FloatText(float f, unsigned& length)
{
    static char buffer[64];
    using Gcvt = char*(__cdecl*)(double, int, char*);
    static const Gcvt gcvt = reinterpret_cast<Gcvt>(Crt80::Proc("_gcvt"));
    gcvt(static_cast<double>(f), 8, buffer);
    length = static_cast<unsigned>(std::strlen(buffer));
    if (buffer[length - 1] == '.') {
        buffer[length] = '0';
        ++length;
        buffer[length] = 0;
    }
    return buffer;
}

// 0x10014150: ASCII by hand, the rest through the CRT (a signed char comes in negative)
int ToLower(int c)
{
    if (static_cast<unsigned>(c) < 0x80) return static_cast<unsigned>(c - 'A') <= 25 ? c | 0x20 : c;
    using Lower = int(__cdecl*)(int);
    static const Lower lower = reinterpret_cast<Lower>(Crt80::Proc("tolower"));
    return lower(c);
}

PString MakePString(const char* text, unsigned length)  // 0x10036860-like: Create(text, length)
{
    PString p;
    if (text) p.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(text), length);
    return p;
}

// 0x10014200 / 0x10014170: the slot's string replaced by an ANSI / UTF-16 text (true when it has a node, or when
// there was no text at all)
bool AssignANSI(void*& slot, const char* text, int length)
{
    AsPString(slot) = PString();
    if (!text) return true;
    PStringCache::Node* const n = PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(text), length);
    slot = n;
    return n != nullptr;
}

bool AssignUnicode(void*& slot, const wchar_t* text, int length)
{
    AsPString(slot) = PString();
    if (!text) return true;
    PStringCache::Node* const n = PString::Cache()->CreateUnicode(reinterpret_cast<const __wchar_t*>(text), length);
    slot = n;
    return n != nullptr;
}

template <class T>
T* NewArray(int count, size_t itemSize)
{
    return static_cast<T*>(Alloc::New(static_cast<unsigned>(count * itemSize + 4)));
}

// 0x10013ba0: a copy of an int / float array
void* CopyWords(const void* a)
{
    const int count = *static_cast<const int*>(a);
    const size_t n = count * 4 + 4;
    void* const p = Alloc::New(static_cast<unsigned>(n));
    std::memcpy(p, a, n);
    return p;
}

// 0x10013e90: the chunks of `from` appended, as copies, to `to`
void AppendChunks(ConfigData::Children& to, const ConfigData::Children& from)
{
    using Chunk = ConfigData::Children::Chunk;
    Chunk* const first = from.head;
    if (!first) return;
    Chunk* c = first;
    do {
        Chunk* const n = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
        if (n) n->count = 0;
        std::memcpy(&n->count, &c->count, c->count * sizeof(ConfigData*) + 4);
        if (to.head) {
            Chunk* const last = to.head->prev;
            n->prev = last;
            n->next = to.head;
            to.head->prev = n;
            last->next = n;
        } else {
            to.head = n;
            n->prev = n;
            n->next = n;
        }
        c = c->next;
    } while (c != first);
}

void FreeChunks(ConfigData::Children& a)  // the chunks only (the items stay)
{
    using Chunk = ConfigData::Children::Chunk;
    Chunk* const first = a.head;
    if (!first) return;
    Chunk* c = first->next;
    while (c != first) {
        Chunk* const next = c->next;
        Alloc::Delete(c);
        c = next;
    }
    Alloc::Delete(first);
    a.head = nullptr;
}

}  // namespace

const char* ConfigFloatText(float f, unsigned& length) { return FloatText(f, length); }  // for ConfigWrite.cpp

// ---- construction (0x1000e890..0x1000ed9a)
ConfigData::ConfigData() : name(), type(NONE), data(nullptr), children() {}

ConfigData::ConfigData(const PString& n) : name(n), type(NONE), data(nullptr), children() {}

ConfigData::ConfigData(const PString& n, const PString& value) : name(n), type(NONE), data(nullptr), children()
{
    ClearData();
    if (Text(value)) {
        type = PSTRING;
        AsPString(data) = value;
    }
}

// 0x1000eb60
ConfigData::ConfigData(const ConfigData& o) : name(), type(NONE), data(nullptr), children()
{
    name = o.name;
    switch (o.type) {
    case PSTRING:
        ClearData();
        if (Text(AsPString(o.data))) {
            type = PSTRING;
            AsPString(data) = AsPString(o.data);
        }
        break;
    case BYTES: {
        const size_t n = *static_cast<const int*>(o.data) + 4;
        void* const p = Alloc::New(static_cast<unsigned>(n));
        std::memcpy(p, o.data, n);
        data = p;
        break;
    }
    case INTS: {
        const size_t n = *static_cast<const int*>(o.data) * 4 + 4;
        void* const p = Alloc::New(static_cast<unsigned>(n));
        std::memcpy(p, o.data, n);
        data = p;
        break;
    }
    case FLOATS:
        data = CopyWords(o.data);
        break;
    case PSTRINGS:
        data = CopyPStringArray(static_cast<PStringArray*>(o.data));
        break;
    default:
        data = o.data;
        break;
    }
    type = o.type;
    CopyChildren(o);
}

ConfigData::~ConfigData()
{
    ClearData();
    if (children.head) {
        for (auto i = children.First(); i.node; ++i) delete *i.node;
        FreeChunks(children);
    }
    FreeChunks(children);
}

ConfigData& ConfigData::operator=(const ConfigData& o)
{
    if (&o == this) return *this;
    ClearData();
    for (auto i = children.First(); i.node; ++i) delete *i.node;
    FreeChunks(children);
    name = o.name;
    switch (o.type) {
    case PSTRING:
        ClearData();
        if (Text(AsPString(o.data))) {
            type = PSTRING;
            AsPString(data) = AsPString(o.data);
        }
        break;
    case BYTES: {
        const size_t n = *static_cast<const int*>(o.data) + 4;
        void* const p = Alloc::New(static_cast<unsigned>(n));
        std::memcpy(p, o.data, n);
        data = p;
        break;
    }
    case INTS:
    case FLOATS:
        data = CopyWords(o.data);
        break;
    case PSTRINGS:
        data = CopyPStringArray(static_cast<PStringArray*>(o.data));
        break;
    default:
        data = o.data;
        break;
    }
    type = o.type;
    CopyChildren(o);
    return *this;
}

// 0x1000ea50: the value goes (the strings of a PString array released, an array block freed)
void __cdecl ConfigData::ClearData()
{
    if (type == NONE) {
        data = nullptr;
        return;
    }
    if (type >= BYTES) {
        if (type == PSTRINGS) {
            PStringArray* const a = static_cast<PStringArray*>(data);
            for (int i = 0; i < a->count; ++i) reinterpret_cast<PString&>(a->items[i]).~PString();
        }
        if (data) Alloc::Delete(data);
        type = NONE;
        data = nullptr;
        return;
    }
    if (type == PSTRING) AsPString(data).~PString();
    type = NONE;
    data = nullptr;
}

void __cdecl ConfigData::CopyChildren(const ConfigData& o)  // 0x1000eda0
{
    for (auto i = o.children.First(); i.node; ++i) {
        void* const mem = operator new(sizeof(ConfigData));
        ConfigData* const c = mem ? ::new (mem) ConfigData(**i.node) : nullptr;
        children.Add(c);
    }
}

ConfigData::PStringArray* __cdecl ConfigData::CopyPStringArray(PStringArray* items)  // 0x1000ee60
{
    const size_t n = items->count * 4 + 4;
    PStringArray* const a = static_cast<PStringArray*>(Alloc::New(static_cast<unsigned>(n)));
    std::memset(a, 0, n);
    a->count = items->count;
    for (int i = 0; i < items->count; ++i) a->items[i] = items->items[i];
    return a;
}

// ---- children
ConfigData& __cdecl ConfigData::operator<<(ConfigData* child)  // 0x1000e0b0
{
    children.Add(child);
    return *this;
}

const ConfigData& __cdecl ConfigData::operator()(const PString& key) const { return Find(key); }
const ConfigData& __cdecl ConfigData::operator()(const PString& key, int n) const { return Find(key, n); }
const ConfigData& __cdecl ConfigData::operator()(const PString& key, const PString& value) const { return Find(key, value); }
ConfigData& __cdecl ConfigData::operator()(const PString& key) { return New(key); }
ConfigData& __cdecl ConfigData::operator()(const PString& key, const PString& value) { return New(key, value); }
const ConfigData& __cdecl ConfigData::operator[](int index) const { return Find(index); }
ConfigData& __cdecl ConfigData::operator[](int index) { return Find(index); }

ConfigData& __cdecl ConfigData::New()  // 0x1000e4d0
{
    void* const mem = operator new(sizeof(ConfigData));
    ConfigData* const c = mem ? ::new (mem) ConfigData(PString()) : nullptr;
    children.Add(c);
    return *c;
}

ConfigData& __cdecl ConfigData::New(const PString& n)  // 0x1000e100
{
    void* const mem = operator new(sizeof(ConfigData));
    ConfigData* const c = mem ? ::new (mem) ConfigData(n) : nullptr;
    children.Add(c);
    return *c;
}

ConfigData& __cdecl ConfigData::New(const PString& n, const PString& value)  // 0x10011180
{
    void* const mem = operator new(sizeof(ConfigData));
    ConfigData* const c = mem ? ::new (mem) ConfigData(n, value) : nullptr;
    children.Add(c);
    return *c;
}

const ConfigData& __cdecl ConfigData::Find(const PString& key) const  // 0x1000f320
{
    for (auto i = children.First(); i.node; ++i)
        if ((*i.node)->name.node == key.node) return **i.node;
    return unknownItem;
}

ConfigData& __cdecl ConfigData::Find(const PString& key)
{
    for (auto i = children.First(); i.node; ++i)
        if ((*i.node)->name.node == key.node) return **i.node;
    return unknownItem;
}

const ConfigData& __cdecl ConfigData::Find(const PString& key, const PString& value) const  // 0x1000f3c0
{
    for (auto i = children.First(); i.node; ++i) {
        const ConfigData* const c = *i.node;
        if (c->type == PSTRING && c->name.node == key.node && c->data == value.node) return *c;
    }
    return unknownItem;
}

ConfigData& __cdecl ConfigData::Find(const PString& key, const PString& value)
{
    for (auto i = children.First(); i.node; ++i) {
        ConfigData* const c = *i.node;
        if (c->type == PSTRING && c->name.node == key.node && c->data == value.node) return *c;
    }
    return unknownItem;
}

// 0x1000f4a0: by position (a negative one finds nothing)
const ConfigData& __cdecl ConfigData::Find(int index) const
{
    if (index < 0) return unknownItem;
    for (auto i = children.First(); i.node; ++i)
        if (index-- == 0) return **i.node;
    return unknownItem;
}

ConfigData& __cdecl ConfigData::Find(int index)
{
    if (index < 0) return unknownItem;
    for (auto i = children.First(); i.node; ++i)
        if (index-- == 0) return **i.node;
    return unknownItem;
}

const ConfigData& __cdecl ConfigData::Find(const PString& key, int n) const  // 0x1000f550
{
    int k = 0;
    for (auto i = children.First(); i.node; ++i)
        if ((*i.node)->name.node == key.node && k++ == n) return **i.node;
    return unknownItem;
}

ConfigData& __cdecl ConfigData::Find(const PString& key, int n)
{
    int k = 0;
    for (auto i = children.First(); i.node; ++i)
        if ((*i.node)->name.node == key.node && k++ == n) return **i.node;
    return unknownItem;
}

int __cdecl ConfigData::Count(const PString& key) const  // 0x10011130
{
    int n = 0;
    for (auto i = children.First(); i.node; ++i)
        if ((*i.node)->name.node == key.node) ++n;
    return n;
}

// 0x100111d0: the n'th child `key`, made (with any missing ones before it) when there are fewer
ConfigData& __cdecl ConfigData::Replace(const PString& key, int n)
{
    int k = 0;
    for (auto i = children.First(); i.node; ++i)
        if ((*i.node)->name.node == key.node && k++ == n) return **i.node;
    ConfigData* last = nullptr;
    for (int m = k; m <= n; ++m) {
        void* const mem = operator new(sizeof(ConfigData));
        last = mem ? ::new (mem) ConfigData(key) : nullptr;
        children.Add(last);
    }
    return *last;
}

ConfigData& __cdecl ConfigData::Replace(const PString& key, const PString& value, int n)  // 0x10011390
{
    int k = 0;
    for (auto i = children.First(); i.node; ++i) {
        ConfigData* const c = *i.node;
        if (c->type == PSTRING && c->name.node == key.node && c->data == value.node && k++ == n) return *c;
    }
    ConfigData* last = nullptr;
    for (int m = k; m <= n; ++m) {
        void* const mem = operator new(sizeof(ConfigData));
        last = mem ? ::new (mem) ConfigData(key, value) : nullptr;
        children.Add(last);
    }
    return *last;
}

// 0x100114f0: the n'th child equal to `child` in type, name and data word
bool ConfigData::Remove(const ConfigData& child, int n)
{
    if (&child == &unknownItem) return false;
    int k = 0;
    for (auto i = children.First(); i.node; ++i) {
        ConfigData* const c = *i.node;
        if (c->type == child.type && c->name.node == child.name.node && c->data == child.data && k++ == n) {
            delete c;
            children.Remove(i);
            return true;
        }
    }
    return false;
}

bool ConfigData::Remove(const PString& key, int n)  // 0x100115f0
{
    int k = 0;
    for (auto i = children.First(); i.node; ++i) {
        ConfigData* const c = *i.node;
        if (c->name.node == key.node && k++ == n) {
            delete c;
            children.Remove(i);
            return true;
        }
    }
    return false;
}

bool ConfigData::Remove(const PString& key, const PString& value, int n)  // 0x100116b0
{
    int k = 0;
    for (auto i = children.First(); i.node; ++i) {
        ConfigData* const c = *i.node;
        if (c->type == PSTRING && c->name.node == key.node && c->data == value.node && k++ == n) {
            delete c;
            children.Remove(i);
            return true;
        }
    }
    return false;
}

void ConfigData::RemoveAll(const PString& key)  // 0x10011780
{
    for (auto i = children.First(); i.node;) {
        ConfigData* const c = *i.node;
        if (c->name.node == key.node) {
            delete c;
            i = children.Remove(i);
        } else {
            ++i;
        }
    }
}

ConfigData* __cdecl ConfigData::Extract(const PString& key, int n)  // 0x10011830: out of the list, not deleted
{
    int k = 0;
    for (auto i = children.First(); i.node; ++i) {
        ConfigData* const c = *i.node;
        if (c->name.node == key.node && k++ == n) {
            children.Remove(i);
            return c;
        }
    }
    return nullptr;
}

ConfigData::Children* __cdecl ConfigData::GetDataList() { return &children; }
const ConfigData::Children* __cdecl ConfigData::GetDataList() const { return &children; }

// 0x10013bd0: the children and `nodes` swap contents (as chunk copies)
void __cdecl ConfigData::AssignNodes(Children* nodes)
{
    Children saved;
    AppendChunks(saved, children);
    FreeChunks(children);
    AppendChunks(children, *nodes);
    FreeChunks(*nodes);
    AppendChunks(*nodes, saved);
    FreeChunks(saved);
}

int __cdecl ConfigData::GetChildCount() const  // 0x1000e480
{
    int n = 0;
    if (Children::Chunk* const first = children.head) {
        Children::Chunk* c = first;
        do {
            n += c->count;
            c = c->next;
        } while (c != first);
    }
    return n;
}

bool ConfigData::HasChildren() const { return children.head != nullptr; }
bool __cdecl ConfigData::HasDataItemOnly() const { return type != NONE && !children.head; }

int __cdecl ConfigData::GetArrayCount() const  // 0x1000e460
{
    if (type == NONE) return 0;
    if (type < BYTES) return 1;
    return *static_cast<const int*>(data);
}

// ---- name and value
const PString& __cdecl ConfigData::GetName() const { return name; }
void __cdecl ConfigData::SetName(const PString& n) { name = n; }
bool __cdecl ConfigData::operator==(const PString& n) const { return name.node == n.node; }
int __cdecl ConfigData::GetDataType() const { return type; }
long __cdecl ConfigData::GetIntData() const { return reinterpret_cast<long>(data); }
float __cdecl ConfigData::GetFloatData() const { return AsFloat(data); }
const PString& __cdecl ConfigData::GetPStringData() const { return AsPString(data); }
ConfigData::ByteArray* __cdecl ConfigData::GetInnerByteArr() const { return static_cast<ByteArray*>(data); }
ConfigData::IntArray* __cdecl ConfigData::GetInnerIntArr() const { return static_cast<IntArray*>(data); }
ConfigData::FloatArray* __cdecl ConfigData::GetInnerFloatArr() const { return static_cast<FloatArray*>(data); }
ConfigData::PStringArray* __cdecl ConfigData::GetInnerPStringArr() const { return static_cast<PStringArray*>(data); }

void __cdecl ConfigData::SetIntData(long v)
{
    ClearData();
    type = INT;
    data = reinterpret_cast<void*>(v);
}

void __cdecl ConfigData::SetFloatData(float v)
{
    ClearData();
    type = FLOAT;
    std::memcpy(&data, &v, 4);
}

void __cdecl ConfigData::SetPStringData(const PString& v)
{
    ClearData();
    if (Text(v)) {
        type = PSTRING;
        AsPString(data) = v;
    }
}

// 0x10010710..0x10010795: the items (a single int / float / string is its own one-item array)
const unsigned char* __cdecl ConfigData::GetByteArray() const
{
    if (this == &unknownItem || type == NONE) return nullptr;
    if (type == INT) return reinterpret_cast<const unsigned char*>(&data);
    return static_cast<const unsigned char*>(data) + 4;
}

const int* __cdecl ConfigData::GetIntArray() const { return reinterpret_cast<const int*>(GetByteArray()); }

const float* __cdecl ConfigData::GetFloatArray() const
{
    if (this == &unknownItem || type == NONE) return nullptr;
    if (type == FLOAT) return reinterpret_cast<const float*>(&data);
    return reinterpret_cast<const float*>(static_cast<const unsigned char*>(data) + 4);
}

const PString* __cdecl ConfigData::GetPStringArray() const
{
    if (this == &unknownItem || type == NONE) return nullptr;
    if (type == PSTRING) return reinterpret_cast<const PString*>(&data);
    return reinterpret_cast<const PString*>(static_cast<const unsigned char*>(data) + 4);
}

void __cdecl ConfigData::SetByteArray(unsigned char* items, int count)  // 0x1000fc70
{
    ClearData();
    ByteArray* const a = static_cast<ByteArray*>(Alloc::New(static_cast<unsigned>(count + 4)));
    type = BYTES;
    data = a;
    a->count = count;
    if (count) std::memcpy(a->items, items, count);
}

void __cdecl ConfigData::SetIntArray(int* items, int count)
{
    ClearData();
    IntArray* const a = NewArray<IntArray>(count, 4);
    type = INTS;
    data = a;
    a->count = count;
    if (count) std::memcpy(a->items, items, count * 4);
}

void __cdecl ConfigData::SetFloatArray(float* items, int count)
{
    ClearData();
    FloatArray* const a = NewArray<FloatArray>(count, 4);
    type = FLOATS;
    data = a;
    a->count = count;
    if (count) std::memcpy(a->items, items, count * 4);
}

void __cdecl ConfigData::SetPStringArray(PString* items, int count)  // 0x1000fd60
{
    ClearData();
    PStringArray* const a = NewArray<PStringArray>(count, 4);
    type = PSTRINGS;
    data = a;
    std::memset(a->items, 0, count * 4);
    a->count = count;
    for (int i = 0; i < count; ++i) a->items[i] = items[i];
}

void __cdecl ConfigData::SetPStringList(const DynamicArray<PString, 13, Less<PString, PString>>& items)  // 0x1000fe30
{
    int count = 0;
    if (auto* const first = items.head) {
        auto* c = first;
        do {
            count += c->count;
            c = c->next;
        } while (c != first);
    }
    ClearData();
    PStringArray* const a = NewArray<PStringArray>(count, 4);
    type = PSTRINGS;
    data = a;
    std::memset(a->items, 0, count * 4);
    a->count = count;
    int k = 0;
    if (auto* const first = items.head) {
        auto* c = first;
        do {
            for (unsigned i = 0; i < c->count && k < count; ++i) a->items[k++] = c->data[i];
            c = c->next;
        } while (c != first);
    }
}

// 0x1000ffc0..0x10010358: appended to an array of the same kind (an empty node becomes one; others are left)
void __cdecl ConfigData::AddByteArray(unsigned char* items, int count)
{
    if (type == NONE) {
        SetByteArray(items, count);
        return;
    }
    if (type != BYTES) return;
    ByteArray* const old = static_cast<ByteArray*>(data);
    ByteArray* const a = static_cast<ByteArray*>(Alloc::New(static_cast<unsigned>(old->count + count + 4)));
    data = a;
    a->count = old->count + count;
    unsigned char* p = a->items;
    if (old->count) {
        std::memcpy(p, old->items, old->count);
        p += old->count;
    }
    if (count) std::memcpy(p, items, count);
    Alloc::Delete(old);
}

void __cdecl ConfigData::AddIntArray(int* items, int count)
{
    if (type == NONE) {
        SetIntArray(items, count);
        return;
    }
    if (type != INTS) return;
    IntArray* const old = static_cast<IntArray*>(data);
    IntArray* const a = NewArray<IntArray>(old->count + count, 4);
    data = a;
    a->count = old->count + count;
    int* p = a->items;
    if (old->count) {
        std::memcpy(p, old->items, old->count * 4);
        p += old->count;
    }
    if (count) std::memcpy(p, items, count * 4);
    Alloc::Delete(old);
}

void __cdecl ConfigData::AddFloatArray(float* items, int count)
{
    if (type == NONE) {
        SetFloatArray(items, count);
        return;
    }
    if (type != FLOATS) return;
    FloatArray* const old = static_cast<FloatArray*>(data);
    FloatArray* const a = NewArray<FloatArray>(old->count + count, 4);
    data = a;
    a->count = old->count + count;
    float* p = a->items;
    if (old->count) {
        std::memcpy(p, old->items, old->count * 4);
        p += old->count;
    }
    if (count) std::memcpy(p, items, count * 4);
    Alloc::Delete(old);
}

// (the old strings are copied with a reference each and the old block freed without releasing them: one
// reference per string leaks, as in the original)
void __cdecl ConfigData::AddPStringArray(PString* items, int count)
{
    if (type == NONE) {
        SetPStringArray(items, count);
        return;
    }
    if (type != PSTRINGS) return;
    PStringArray* const old = static_cast<PStringArray*>(data);
    const int total = old->count + count;
    PStringArray* const a = NewArray<PStringArray>(total, 4);
    data = a;
    a->count = total;
    std::memset(a->items, 0, total * 4);
    for (int i = 0; i < old->count; ++i) a->items[i] = old->items[i];
    for (int i = 0; i < count; ++i) a->items[old->count + i] = items[i];
    Alloc::Delete(old);
}

// 0x10010680: true when the text made a node, or when there was no text (then the value is an empty string)
bool __cdecl ConfigData::SetAnsi(const AnsiString& s)
{
    ClearData();
    const unsigned* const b = *reinterpret_cast<const unsigned* const*>(&s);
    const char* const text = b[0] ? reinterpret_cast<const char*>(b + 2) : nullptr;
    if (!AssignANSI(data, text, static_cast<int>(b[1]))) return false;
    type = PSTRING;
    return true;
}

bool __cdecl ConfigData::SetUnicode(const UnicodeString& s)  // 0x100106d0: {text, ?, length}
{
    ClearData();
    const wchar_t* const text = *reinterpret_cast<const wchar_t* const*>(&s);
    const int length = reinterpret_cast<const int*>(&s)[2];
    if (!AssignUnicode(data, text, length)) return false;
    type = PSTRING;
    return true;
}

// 0x10010ee0: false only for the missing-key node, or a string that did not copy
bool __cdecl ConfigData::GetAnsi(AnsiString& s) const
{
    if (this == &unknownItem) return false;
    if (type == PSTRING) {
        s = AsPString(data);
        const unsigned* const b = *reinterpret_cast<const unsigned* const*>(&s);
        return !(b[0] == 0 && Text(AsPString(data)));
    }
    if (type == INT) {
        char buf[16];
        _itoa(static_cast<int>(reinterpret_cast<intptr_t>(data)), buf, 10);
        s = buf;
        return true;
    }
    if (type == FLOAT) {
        unsigned length;
        const char* const t = FloatText(AsFloat(data), length);
        s.Setn(t, length);
        return true;
    }
    s = AnsiString();
    return true;
}

bool __cdecl ConfigData::GetUnicode(UnicodeString& s) const  // 0x10011000
{
    if (this == &unknownItem) return false;
    int* const u = reinterpret_cast<int*>(&s);
    if (type == PSTRING) {
        s = AsPString(data);
        return !(u[0] == 0 && Text(AsPString(data)));
    }
    if (type == INT) {
        wchar_t buf[16];
        _itow(static_cast<int>(reinterpret_cast<intptr_t>(data)), buf, 10);
        s.Set(reinterpret_cast<const __wchar_t*>(buf), -1, -1);
        return true;
    }
    if (type == FLOAT) {
        unsigned length;
        const char* const t = FloatText(AsFloat(data), length);
        s.SetANSI(t, static_cast<int>(length));
        return true;
    }
    u[1] = 0;
    u[2] = 0;
    if (u[0]) Alloc::Delete(reinterpret_cast<void*>(u[0]));
    u[0] = 0;
    return true;
}

// 0x1000f620: type 0 counts as a string; strings by node, arrays by contents, int / float by bits
bool __cdecl ConfigData::CompareData(const ConfigData& o) const
{
    const int a = type ? type : PSTRING;
    const int b = o.type ? o.type : PSTRING;
    if (a != b) return false;
    switch (a) {
    case PSTRING:
        return data == o.data;
    case BYTES:
    case INTS:
    case FLOATS: {
        const int n = *static_cast<const int*>(data);
        if (n != *static_cast<const int*>(o.data)) return false;
        if (!n) return true;
        return std::memcmp(static_cast<const int*>(data) + 1, static_cast<const int*>(o.data) + 1, a == BYTES ? n : n * 4) == 0;
    }
    case PSTRINGS: {
        const PStringArray* const x = static_cast<const PStringArray*>(data);
        const PStringArray* const y = static_cast<const PStringArray*>(o.data);
        if (x->count != y->count) return false;
        for (int i = 0; i < x->count; ++i)
            if (x->items[i].node != y->items[i].node) return false;
        return true;
    }
    default:
        return data == o.data;
    }
}

// 0x1000f860: the value only (not the name, not the children)
void __cdecl ConfigData::CopyData(const ConfigData& o)
{
    if (&o == this) return;
    ClearData();
    switch (o.type) {
    case PSTRING:
        ClearData();
        if (Text(AsPString(o.data))) {
            type = PSTRING;
            AsPString(data) = AsPString(o.data);
        }
        break;
    case BYTES: {
        const size_t n = *static_cast<const int*>(o.data) + 4;
        void* const p = Alloc::New(static_cast<unsigned>(n));
        std::memcpy(p, o.data, n);
        data = p;
        break;
    }
    case INTS:
    case FLOATS:
        data = CopyWords(o.data);
        break;
    case PSTRINGS:
        data = CopyPStringArray(static_cast<PStringArray*>(o.data));
        break;
    default:
        data = o.data;
        break;
    }
    type = o.type;
}

// 0x1000ef40: the name with its NUL, the value (floats as their text), then the children (but those named `skip`)
unsigned long __cdecl ConfigData::CalcCRC(unsigned long crc, const PString& skip) const
{
    crc = Crc::Calc(Text(name), name.node ? name.node->length + 1 : 0, crc);
    switch (type) {
    case PSTRING: {
        const PString& v = AsPString(data);
        crc = Crc::Calc(Text(v), v.node ? v.node->length + 1 : 0, crc);
        break;
    }
    case BYTES:
        crc = Crc::Calc(data, *static_cast<const int*>(data) + 4, crc);
        break;
    case INTS:
        crc = Crc::Calc(data, *static_cast<const int*>(data) * 4 + 4, crc);
        break;
    case PSTRINGS: {
        const PStringArray* const a = static_cast<const PStringArray*>(data);
        for (int i = 0; i < a->count; ++i) {
            const PString& v = a->items[i];
            crc = Crc::Calc(Text(v), v.node ? v.node->length + 1 : 0, crc);
        }
        break;
    }
    case FLOAT: {
        unsigned length;
        const char* const t = FloatText(AsFloat(data), length);
        crc = Crc::Calc(t, length, crc);
        break;
    }
    case FLOATS: {
        const FloatArray* const a = static_cast<const FloatArray*>(data);
        for (int i = 0; i < a->count; ++i) {
            unsigned length;
            const char* const t = FloatText(a->items[i], length);
            crc = Crc::Calc(t, length, crc);
        }
        break;
    }
    default:
        crc = Crc::Calc(&data, 4, crc);
        break;
    }
    const bool skipping = Text(skip) != nullptr;
    for (auto i = children.First(); i.node; ++i) {
        if (skipping && (*i.node)->name.node == skip.node) continue;
        crc = (*i.node)->CalcCRC(crc, PString());
    }
    return crc;
}

unsigned long __cdecl ConfigData::Crc() const { return CalcCRC(0, PString()); }
unsigned long __cdecl ConfigData::Crc(const PString& skip) const { return CalcCRC(0, skip); }

// 0x1000fb60: the value as a string (ints by _itoa, floats as FloatText); arrays give none
PString __cdecl ConfigData::DataToPString() const
{
    if (type == PSTRING) return AsPString(data);
    if (type == INT) {
        char buf[16];
        _itoa(static_cast<int>(reinterpret_cast<intptr_t>(data)), buf, 10);
        return MakePString(buf, static_cast<unsigned>(std::strlen(buf)));
    }
    if (type == FLOAT) {
        unsigned length;
        const char* const t = FloatText(AsFloat(data), length);
        void* slot = nullptr;
        AssignANSI(slot, t, static_cast<int>(length));
        PString p;
        p.node = static_cast<PStringCache::Node*>(slot);
        return p;
    }
    return PString();
}

// ---- conversions (0x1000e150..0x1000e445, 0x1000fa60..0x1000fb53). The original returns its uninitialised
// locals for a node with no array; these return zeros there.
ConfigData::operator bool() const { return this != &unknownItem; }
ConfigData::operator const PString&() const { return AsPString(data); }
ConfigData::operator const char*() const { return Text(AsPString(data)); }

ConfigData::operator int() const
{
    if (type == INT) return static_cast<int>(reinterpret_cast<intptr_t>(data));
    if (type == PSTRING) {
        const char* const t = Text(AsPString(data));
        return t ? Crt80::strtol(t, nullptr, 10) : 0;
    }
    if (type == FLOAT) return static_cast<int>(AsFloat(data));
    return 0;
}

ConfigData::operator unsigned int() const  // (strtoul base 0; a float through a 64-bit truncation)
{
    if (type == INT) return static_cast<unsigned>(reinterpret_cast<uintptr_t>(data));
    if (type == PSTRING) {
        const char* const t = Text(AsPString(data));
        return t ? Crt80::strtoul(t, nullptr, 0) : 0;
    }
    if (type == FLOAT) return static_cast<unsigned>(static_cast<long long>(AsFloat(data)));
    return 0;
}

ConfigData::operator float() const
{
    if (type == FLOAT) return AsFloat(data);
    if (type == INT) return static_cast<float>(static_cast<int>(reinterpret_cast<intptr_t>(data)));
    if (type == PSTRING) {
        const char* const t = Text(AsPString(data));
        return t ? static_cast<float>(Crt80::atof(t)) : 0.0f;
    }
    return 0.0f;
}

namespace {
template <class T, int N>
T WordsOf(const void* data)  // the first N words of an array's items, zeros without one
{
    T v{};
    if (data) std::memcpy(&v, static_cast<const int*>(data) + 1, N * 4);
    return v;
}
}  // namespace

ConfigData::operator Vector2() const { return WordsOf<Vector2, 2>(data); }
ConfigData::operator Vector3() const { return WordsOf<Vector3, 3>(data); }
ConfigData::operator Vector4() const { return WordsOf<Vector4, 4>(data); }
ConfigData::operator Point() const { return WordsOf<Point, 2>(data); }
ConfigData::operator Size() const { return WordsOf<Size, 2>(data); }
ConfigData::operator Rect() const { return WordsOf<Rect, 4>(data); }

// 0x1000e270: three items are Euler angles; four are normalised (the length on the x87 stack)
ConfigData::operator Quaternion() const
{
    Quaternion q{};
    if (!data) return q;
    const float* const f = static_cast<const float*>(data) + 1;
    if (*static_cast<const int*>(data) == 3) {
        q.Set(ThroughX87(f[0]), ThroughX87(f[1]), ThroughX87(f[2]));
        return q;
    }
    const float v[4] = {ThroughX87(f[0]), f[1], f[2], f[3]};
    float norm;
#if defined(_M_IX86)
    // (the NaN a sum of NaNs keeps is the x87's: the larger payload)
    const float* const pv = v;
    __asm {
        mov eax, pv
        fld dword ptr [eax]
        fmul st(0), st(0)
        fld dword ptr [eax+4]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld dword ptr [eax+8]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld dword ptr [eax+12]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fsqrt
        fstp norm
    }
#else
    norm = static_cast<float>(std::sqrt(static_cast<double>(v[0]) * v[0] + static_cast<double>(v[1]) * v[1] +
                                          static_cast<double>(v[2]) * v[2] + static_cast<double>(v[3]) * v[3]));
#endif
    // divss / mulss with the reciprocal first (an SSE NaN result is the first operand's)
    const __m128 inv = _mm_div_ss(_mm_set_ss(1.0f), _mm_set_ss(norm));
    float* const out = reinterpret_cast<float*>(&q);
    for (int i = 0; i < 4; ++i) out[i] = _mm_cvtss_f32(_mm_mul_ss(inv, _mm_set_ss(v[i])));
    return q;
}

ConfigData::operator Color() const  // 0x1000e360: alpha 1 with three items
{
    float c[4] = {};
    if (data) {
        const float* const f = static_cast<const float*>(data) + 1;
        c[0] = f[0];
        c[1] = f[1];
        c[2] = f[2];
        c[3] = *static_cast<const int*>(data) >= 4 ? f[3] : 1.0f;
    }
    Color out;
    float* const o = reinterpret_cast<float*>(&out);
    for (int i = 0; i < 4; ++i) o[i] = ThroughX87(c[i]);
    return out;
}

// ---- operator<< (0x10010360..0x1001066f)
void ConfigData::operator<<(const PString& v) { SetPStringData(v); }

void ConfigData::operator<<(const String& v)
{
    const unsigned* const b = *reinterpret_cast<const unsigned* const*>(&v);
    const char* const text = b[0] ? reinterpret_cast<const char*>(b + 2) : nullptr;
    const PString p = text ? MakePString(text, static_cast<unsigned>(std::strlen(text))) : PString();
    ClearData();
    if (p.node && p.node->text) {
        type = PSTRING;
        AsPString(data) = p;
    }
}

void ConfigData::operator<<(char v) { SetIntData(static_cast<signed char>(v)); }
void ConfigData::operator<<(unsigned char v) { SetIntData(v); }
void ConfigData::operator<<(bool v) { SetIntData(static_cast<unsigned char>(v)); }
void ConfigData::operator<<(short v) { SetIntData(v); }
void ConfigData::operator<<(unsigned short v) { SetIntData(v); }
void ConfigData::operator<<(int v) { SetIntData(v); }
void ConfigData::operator<<(unsigned int v) { SetIntData(static_cast<long>(v)); }
void ConfigData::operator<<(long v) { SetIntData(v); }
void ConfigData::operator<<(unsigned long v) { SetIntData(static_cast<long>(v)); }

void ConfigData::operator<<(float v)
{
    ClearData();
    type = FLOAT;
    std::memcpy(&data, &v, 4);
}

namespace {
// a float array of n items, copied through the x87 stack
void* FloatItems(const void* v, int n)
{
    ConfigData::FloatArray* const a = static_cast<ConfigData::FloatArray*>(Alloc::New(static_cast<unsigned>(n * 4 + 4)));
    a->count = n;
    for (int i = 0; i < n; ++i) a->items[i] = ThroughX87(static_cast<const float*>(v)[i]);
    return a;
}
void* IntItems(const void* v, int n)
{
    ConfigData::IntArray* const a = static_cast<ConfigData::IntArray*>(Alloc::New(static_cast<unsigned>(n * 4 + 4)));
    a->count = n;
    std::memcpy(a->items, v, n * 4);
    return a;
}
}  // namespace

#define JET_CONFIG_SET_ITEMS(Type, kind, n, make) \
    void ConfigData::operator<<(const Type& v)   \
    {                                             \
        ClearData();                              \
        void* const a = make(&v, n);              \
        type = kind;                              \
        data = a;                                 \
    }
JET_CONFIG_SET_ITEMS(Range<float>, FLOATS, 2, FloatItems)
JET_CONFIG_SET_ITEMS(Vector2, FLOATS, 2, FloatItems)
JET_CONFIG_SET_ITEMS(Vector3, FLOATS, 3, FloatItems)
JET_CONFIG_SET_ITEMS(Vector4, FLOATS, 4, FloatItems)
JET_CONFIG_SET_ITEMS(Color, FLOATS, 4, FloatItems)
JET_CONFIG_SET_ITEMS(Quaternion, FLOATS, 4, FloatItems)
JET_CONFIG_SET_ITEMS(Range<int>, INTS, 2, IntItems)
JET_CONFIG_SET_ITEMS(Point, INTS, 2, IntItems)
JET_CONFIG_SET_ITEMS(Size, INTS, 2, IntItems)
JET_CONFIG_SET_ITEMS(Rect, INTS, 4, IntItems)
#undef JET_CONFIG_SET_ITEMS

// ---- operator>> (0x100107a0..0x10010ed2): nothing happens for the missing-key node
void ConfigData::operator>>(PString& out) const
{
    if (this == &unknownItem) return;
    out = DataToPString();
}

void ConfigData::operator>>(String& out) const
{
    if (this == &unknownItem) return;
    const PString p = DataToPString();
    out = Text(p);
}

// int 0 / not 0; a string "1" or "true" in any case; no value false; floats and arrays leave it
void ConfigData::operator>>(bool& out) const
{
    if (this == &unknownItem) return;
    if (type == INT) {
        out = data != nullptr;
        return;
    }
    if (type == NONE) {
        out = false;
        return;
    }
    if (type != PSTRING) return;
    const char* const t = Text(AsPString(data));
    if (!t) {
        out = false;
        return;
    }
    if (t[0] == '1' && !t[1]) {
        out = true;
        return;
    }
    out = ToLower(static_cast<signed char>(t[0])) == 't' && ToLower(static_cast<signed char>(t[1])) == 'r' &&
          ToLower(static_cast<signed char>(t[2])) == 'u' && ToLower(static_cast<signed char>(t[3])) == 'e' && !t[4];
}

void ConfigData::operator>>(int& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<int>(*this);
}

void ConfigData::operator>>(long& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<int>(*this);
}

void ConfigData::operator>>(char& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<char>(static_cast<int>(*this));
}

void ConfigData::operator>>(unsigned char& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<unsigned char>(static_cast<unsigned>(*this));
}

void ConfigData::operator>>(short& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<short>(static_cast<int>(*this));
}

void ConfigData::operator>>(unsigned short& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<unsigned short>(static_cast<unsigned>(*this));
}

void ConfigData::operator>>(unsigned int& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<unsigned>(*this);
}

void ConfigData::operator>>(unsigned long& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<unsigned>(*this);
}

void ConfigData::operator>>(float& out) const
{
    if (this == &unknownItem) return;
    out = static_cast<float>(*this);
}

void ConfigData::operator>>(Range<int>& out) const  // (no check for an array)
{
    if (this == &unknownItem) return;
    const int* const a = static_cast<const int*>(data);
    out.min = a[1];
    out.max = a[2];
}

void ConfigData::operator>>(Range<float>& out) const
{
    if (this == &unknownItem) return;
    const float* const a = static_cast<const float*>(data);
    out.min = ThroughX87(a[1]);
    out.max = ThroughX87(a[2]);
}

void ConfigData::operator>>(Vector2& out) const
{
    if (this != &unknownItem) out = static_cast<Vector2>(*this);
}
void ConfigData::operator>>(Vector3& out) const
{
    if (this != &unknownItem) out = static_cast<Vector3>(*this);
}
void ConfigData::operator>>(Vector4& out) const
{
    if (this != &unknownItem) out = static_cast<Vector4>(*this);
}
void ConfigData::operator>>(Point& out) const
{
    if (this != &unknownItem) out = static_cast<Point>(*this);
}
void ConfigData::operator>>(Size& out) const
{
    if (this != &unknownItem) out = static_cast<Size>(*this);
}
void ConfigData::operator>>(Rect& out) const
{
    if (this != &unknownItem) out = static_cast<Rect>(*this);
}

void ConfigData::operator>>(Quaternion& out) const
{
    if (this == &unknownItem) return;
    const Quaternion q = static_cast<Quaternion>(*this);
    float* const o = reinterpret_cast<float*>(&out);
    const float* const s = reinterpret_cast<const float*>(&q);
    for (int i = 0; i < 4; ++i) o[i] = ThroughX87(s[i]);
}

void ConfigData::operator>>(Color& out) const  // 0x10010d90 (no x87 copy here)
{
    if (this == &unknownItem) return;
    float c[4] = {};
    if (data) {
        const float* const f = static_cast<const float*>(data) + 1;
        c[0] = f[0];
        c[1] = f[1];
        c[2] = f[2];
        c[3] = *static_cast<const int*>(data) >= 4 ? f[3] : 1.0f;
    }
    std::memcpy(&out, c, sizeof(c));
}

void ConfigData::operator>>(ConfigData& out) const
{
    if (this != &unknownItem) out = *this;
}

}  // namespace Jet
