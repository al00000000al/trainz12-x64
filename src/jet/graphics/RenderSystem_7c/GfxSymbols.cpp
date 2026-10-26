// Unit U2 of RenderSystem_7c, the data side of the shader-language front-end: symbol records, the vertex symbol
// table, register operands, instructions and preprocessor defines (0x1000f190..0x1000fc80, 0x1001cb10..0x1001d180,
// 0x100211a0..0x10021840). Several share one body in the original (identical-code folding).
#include <jet/GfxLanguage.h>

#include <cstring>
#include <new>

namespace Jet {

template class Array<PreProcessorDefine, 3>;

namespace {

void DeleteApiData(ApiDataBase* d)
{
    using Dtor = void*(__thiscall*)(void*, unsigned);
    reinterpret_cast<Dtor>((*reinterpret_cast<void***>(d))[0])(d, 1);
}

void DeleteRegister(RegisterItem* r)
{
    if (r) delete r;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// SymbolEntry (0x1000f190, 0x1000f260, 0x1000f350, 0x1000f9b0..0x1000fc50)

SymbolEntry::SymbolEntry(SymbolType t, bool referenced)
    : alias(nullptr), type(t), field08(-1), field0c(-1), field10(-1), field14(-1), hint(0), field1c(5), field20(0),
      field34(0), refs(referenced), next(nullptr), api(nullptr)
{
    flags = static_cast<unsigned char>((flags & 0xfb) | 3);
}

SymbolEntry::SymbolEntry(const SymbolEntry& o)
    : alias(o.alias), type(o.type), field08(o.field08), field0c(o.field0c), field10(o.field10), field14(o.field14),
      hint(o.hint), field1c(o.field1c), field20(o.field20), field24(o.field24), field28(o.field28),
      field2c(o.field2c), field30(o.field30), field34(o.field34), name(o.name), refs(o.refs), next(o.next), api(o.api)
{
    flags = static_cast<unsigned char>((flags & 0xf8) | (o.flags & 7));
}

SymbolEntry& SymbolEntry::operator=(const SymbolEntry& o)
{
    alias = o.alias;
    type = o.type;
    field08 = o.field08;
    field0c = o.field0c;
    field10 = o.field10;
    field14 = o.field14;
    hint = o.hint;
    field1c = o.field1c;
    field20 = o.field20;
    flags = static_cast<unsigned char>((flags & 0xf8) | (o.flags & 7));
    field24 = o.field24;
    field28 = o.field28;
    field2c = o.field2c;
    field30 = o.field30;
    field34 = o.field34;
    name = o.name;
    refs = o.refs;
    next = o.next;
    api = o.api;
    return *this;
}

// `SymbolEntry::'default constructor closure'` (0x1000f350), exported by alias: SymbolEntry(0, false)
void __fastcall SymbolEntryDefaultClosure(SymbolEntry* self) { ::new (static_cast<void*>(self)) SymbolEntry(static_cast<SymbolType>(0), false); }

SymbolEntry::~SymbolEntry()
{
    if (next) delete next;
    next = nullptr;
    if (flags & 4) {
        if (alias) delete alias;
        alias = nullptr;
    }
    if (api) DeleteApiData(api);
    api = nullptr;
}

void __cdecl SymbolEntry::SetName(const char* text)
{
    if (name.node) PString::Cache()->Destroy(name.node);
    name.node = text ? PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(text), -1) : nullptr;
    flags &= 0xfe;
}

const PString& __cdecl SymbolEntry::GetName() const { return name; }

void __cdecl SymbolEntry::AddRef()
{
    for (SymbolEntry* s = this;; s = s->alias) {
        ++s->refs;
        if (!s->alias) break;
    }
}

void __cdecl SymbolEntry::DecRef()
{
    for (SymbolEntry* s = this;; s = s->alias) {
        --s->refs;
        if (!s->alias) break;
    }
}

bool __cdecl SymbolEntry::IsAnonymous() const { return (flags & 1) != 0; }
bool __cdecl SymbolEntry::IsReferenced() const { return refs != 0; }

bool __cdecl SymbolEntry::LikeType(const SymbolEntry* o) const
{
    return type == o->type && field0c == o->field0c && ((flags ^ o->flags) & 2) == 0 && field20 == o->field20 &&
           field10 == o->field10 && field14 == o->field14 && field34 == o->field34;
}

void __cdecl SymbolEntry::AppendSymbol(SymbolEntry* s)
{
    SymbolEntry* e = this;
    while (e->next) e = e->next;
    e->next = s;
}

SymbolEntry* __cdecl SymbolEntry::GetNextSymbol() const { return next; }

const SymbolEntry* __cdecl SymbolEntry::UnAlias() const
{
    const SymbolEntry* s = this;
    while (s->alias) s = s->alias;
    return s;
}

void __cdecl SymbolEntry::SetApiData(ApiDataBase* d) const { api = d; }
ApiDataBase* __cdecl SymbolEntry::GetApiData() const { return api; }
ApiDataBase* __cdecl SymbolEntry::GetAndZeroApiData() const
{
    ApiDataBase* d = api;
    api = nullptr;
    return d;
}
unsigned long __cdecl SymbolEntry::GetHint() const { return hint; }
void __cdecl SymbolEntry::SetHint(unsigned long h) { hint = h; }

// ---------------------------------------------------------------------------------------------------------------
// VertexSymbolEntry / TextureSymbolEntry (0x1000f390..0x1000f420, 0x1000fc60)

VertexSymbolEntry::VertexSymbolEntry(VertexSymbolClass cls, SymbolType t, bool referenced)
    : SymbolEntry(t, referenced), symbolClass(cls) {}
VertexSymbolEntry::VertexSymbolEntry(const VertexSymbolEntry& o) : SymbolEntry(o), symbolClass(o.symbolClass) {}
VertexSymbolEntry& VertexSymbolEntry::operator=(const VertexSymbolEntry& o)
{
    SymbolEntry::operator=(o);
    symbolClass = o.symbolClass;
    return *this;
}
VertexSymbolEntry::~VertexSymbolEntry() {}

bool __cdecl VertexSymbolEntry::LikeType(const VertexSymbolEntry* o) const
{
    return o->symbolClass == symbolClass && SymbolEntry::LikeType(o);
}

TextureSymbolEntry::TextureSymbolEntry(TextureSymbolClass cls, SymbolType t, bool referenced)
    : SymbolEntry(t, referenced), symbolClass(cls) {}
TextureSymbolEntry::TextureSymbolEntry(const TextureSymbolEntry& o) : SymbolEntry(o), symbolClass(o.symbolClass) {}
TextureSymbolEntry& TextureSymbolEntry::operator=(const TextureSymbolEntry& o)
{
    SymbolEntry::operator=(o);
    symbolClass = o.symbolClass;
    return *this;
}
TextureSymbolEntry::~TextureSymbolEntry() {}

// ---------------------------------------------------------------------------------------------------------------
// VertexSymbolTable (0x1000f660, 0x1000f680, 0x1000f850, 0x1000f8a0..0x1000f9a0, 0x100211a0; copy 0x10007a70)

namespace {

void CopyPointers(VertexSymbolTable& to, const VertexSymbolTable& from)
{
    to.count = 0;
    if (!from.data) return;
    if (to.capacity < from.capacity) {
        if (to.data) Alloc::Delete(to.data);
        to.data = static_cast<VertexSymbolEntry**>(Alloc::New(from.capacity * 4));
        to.capacity = from.capacity;
    }
    to.count = from.count;
    for (unsigned long i = 0; i < to.count; ++i) to.data[i] = from.data[i];
}

}  // namespace

VertexSymbolTable::VertexSymbolTable() : count(0), capacity(0), data(nullptr) {}
VertexSymbolTable::VertexSymbolTable(const VertexSymbolTable& o) : count(0), capacity(0), data(nullptr)
{
    CopyPointers(*this, o);
}
VertexSymbolTable& VertexSymbolTable::operator=(const VertexSymbolTable& o)
{
    CopyPointers(*this, o);
    return *this;
}

VertexSymbolTable::~VertexSymbolTable()
{
    for (unsigned long i = 0; i < count; ++i) {
        SymbolEntry* e = data[i];
        if (e) {
            e->~SymbolEntry();
            Alloc::Delete(e);
        }
        data[i] = nullptr;
    }
    count = 0;
    if (data) Alloc::Delete(data);
}

VertexSymbolEntry* __cdecl VertexSymbolTable::Find(const PString& n) const
{
    for (unsigned long i = 0; i < count; ++i)
        if (data[i]->name == n) return data[i];
    return nullptr;
}

VertexSymbolEntry* __cdecl VertexSymbolTable::FindLike(const VertexSymbolEntry* e) const
{
    for (unsigned long i = 0; i < count; ++i) {
        const VertexSymbolEntry* s = data[i];
        if (s->symbolClass == e->symbolClass && s->type == e->type && s->field0c == e->field0c &&
            ((s->flags ^ e->flags) & 2) == 0 && s->field20 == e->field20 && s->field10 == e->field10 &&
            s->field14 == e->field14 && s->field34 == e->field34)
            return data[i];
    }
    return nullptr;
}

// grows in steps of 128 entries (0x10051b70)
void __cdecl VertexSymbolTable::AddSymbol(VertexSymbolEntry* e)
{
    const unsigned long needed = count + 1;
    if (capacity < needed) {
        const unsigned long blocks = (needed + 0x7f) >> 7;
        auto* block = static_cast<VertexSymbolEntry**>(Alloc::New(blocks * 0x200));
        if (data) {
            std::memcpy(block, data, count * 4);
            if (data) Alloc::Delete(data);
        }
        capacity = blocks << 7;
        data = block;
    }
    data[count] = e;
    ++count;
}

// ---------------------------------------------------------------------------------------------------------------
// RegisterItem, OffsetAddress (0x100213a0..0x10021520, 0x10024fa0, 0x1000f440, 0x1000f460)

RegisterModifier& RegisterModifier::operator=(const RegisterModifier&) { return *this; }

OffsetAddress::OffsetAddress(RegisterItem* i, int o) : item(i), offset(o) {}
OffsetAddress::~OffsetAddress()
{
    DeleteRegister(item);
    item = nullptr;
}
OffsetAddress& OffsetAddress::operator=(const OffsetAddress& o)
{
    item = o.item;
    offset = o.offset;
    return *this;
}

RegisterItem::RegisterItem(SymbolEntry* s, char* sw, bool f, RegisterModifier* a, RegisterModifier* b)
    : symbol(s), offset(nullptr), swizzle(sw), flag(f), m1(a), m2(b) {}

RegisterItem::RegisterItem(const RegisterItem* o)
    : symbol(o->symbol), offset(nullptr), swizzle(nullptr), flag(o->flag), m1(nullptr), m2(nullptr)
{
    if (o->swizzle) {
        swizzle = static_cast<char*>(Alloc::New(6));
        char* d = swizzle;
        const char* s = o->swizzle;
        while ((*d++ = *s++) != 0) {}
    }
    if (o->offset) {
        auto* a = static_cast<OffsetAddress*>(Alloc::New(sizeof(OffsetAddress)));
        if (a) {
            a->item = o->offset->item;
            a->offset = o->offset->offset;
            offset = a;
        }
    }
}

RegisterItem::~RegisterItem()
{
    if (swizzle) Alloc::Delete(swizzle);
    swizzle = nullptr;
    if (OffsetAddress* a = offset) {
        DeleteRegister(a->item);
        a->item = nullptr;
        Alloc::Delete(a);
    }
    offset = nullptr;
    if (m1) Alloc::Delete(m1);
    m1 = nullptr;
    if (m2) Alloc::Delete(m2);
    m2 = nullptr;
}

RegisterItem& RegisterItem::operator=(const RegisterItem& o)
{
    symbol = o.symbol;
    offset = o.offset;
    swizzle = o.swizzle;
    flag = o.flag;
    m1 = o.m1;
    m2 = o.m2;
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------
// Instruction, VertexInstruction, TextureInstruction (0x1000f480..0x1000f600, 0x10021270)

Instruction::Instruction(RegisterList* r) : operands(r) {}
Instruction::Instruction(const Instruction& o) : operands(o.operands) {}
Instruction& Instruction::operator=(const Instruction& o)
{
    operands = o.operands;
    return *this;
}

// deletes the operands and their array (the array is read without a null test, as in the original)
Instruction::~Instruction()
{
    RegisterList* r = operands;
    for (unsigned long i = 0; i < r->count; ++i) {
        DeleteRegister(r->data[i]);
        r->data[i] = nullptr;
    }
    if (RegisterList* l = operands) {
        l->count = 0;
        if (l->data) Alloc::Delete(l->data);
        Alloc::Delete(l);
    }
    operands = nullptr;
}

VertexInstruction::VertexInstruction(VertexOpcode op, RegisterList* r) : Instruction(r), opcode(op) {}
VertexInstruction::VertexInstruction(const VertexInstruction& o) : Instruction(o), opcode(o.opcode) {}
VertexInstruction& VertexInstruction::operator=(const VertexInstruction& o)
{
    operands = o.operands;
    opcode = o.opcode;
    return *this;
}
VertexInstruction::~VertexInstruction() {}

TextureInstruction::TextureInstruction(TextureOpcode op, RegisterList* r) : Instruction(r), opcode(op) {}
TextureInstruction::TextureInstruction(const TextureInstruction& o) : Instruction(o), opcode(o.opcode) {}
TextureInstruction& TextureInstruction::operator=(const TextureInstruction& o)
{
    operands = o.operands;
    opcode = o.opcode;
    return *this;
}
TextureInstruction::~TextureInstruction() {}

// ---------------------------------------------------------------------------------------------------------------
// PreProcessorDefine, PreProcessorArray (0x1000f6a0..0x1000f880, 0x1001cb10, 0x10021530)

PreProcessorDefine::PreProcessorDefine() {}
PreProcessorDefine::PreProcessorDefine(const AnsiString& n, const AnsiString& v) : name(n), value(v) {}
PreProcessorDefine::PreProcessorDefine(const PreProcessorDefine& o) : name(o.name), value(o.value) {}
PreProcessorDefine::~PreProcessorDefine() {}
PreProcessorDefine& PreProcessorDefine::operator=(const PreProcessorDefine& o)
{
    name = o.name;
    value = o.value;
    return *this;
}

PreProcessorArray::PreProcessorArray() {}
PreProcessorArray::PreProcessorArray(const PreProcessorArray& o) : Array<PreProcessorDefine, 3>(o) {}
PreProcessorArray::~PreProcessorArray() {}
PreProcessorArray& PreProcessorArray::operator=(const PreProcessorArray& o)
{
    Array<PreProcessorDefine, 3>::operator=(o);
    return *this;
}

AnsiString __cdecl PreProcessorArray::ConcatenateStrings() const
{
    AnsiString out("");
    for (unsigned long i = 0; i < count; ++i) {
        AnsiString a(data[i].name);
        a.Ins(":", 0);
        AnsiString b(a);
        b.Ins(":", b.Length());
        AnsiString c(b);
        c += data[i].value;
        out += c;
    }
    return out;
}

}  // namespace Jet
