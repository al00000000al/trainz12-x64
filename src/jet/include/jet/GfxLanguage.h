// RenderSystem_7c unit U2 (notes/RenderSystem.md), the shader-language front-end: the symbol records, register
// operands and instructions the vertex/texture program parsers build, and the preprocessor defines. Layouts follow
// the TS12 assembly (addresses per member in RenderSystem_7c/GfxSymbols.cpp).
#pragma once

#include <jet/DynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>

namespace Jet {

struct ApiDataBase;  // the renderer's per-symbol data (U9); deleted through its virtual destructor

enum SymbolType {};
enum VertexSymbolClass {};
enum TextureSymbolClass {};
enum VertexOpcode {};
enum TextureOpcode {};

// a named or anonymous symbol of a program (0x48 bytes). Symbols chain through `next` (AppendSymbol) and an alias
// points at the symbol it stands for (UnAlias); a symbol owns its chain, its API data and, with bit 2 of `flags`,
// its alias.
struct SymbolEntry {
    SymbolEntry(SymbolType type, bool referenced);
    SymbolEntry(const SymbolEntry& o);
    SymbolEntry& operator=(const SymbolEntry& o);
    ~SymbolEntry();

    void __cdecl SetName(const char* name);  // null: no name; either way the symbol is no longer anonymous
    const PString& __cdecl GetName() const;
    void __cdecl AddRef();  // on the symbol and every alias it stands for
    void __cdecl DecRef();
    bool __cdecl IsAnonymous() const;
    bool __cdecl IsReferenced() const;
    bool __cdecl LikeType(const SymbolEntry* o) const;
    void __cdecl AppendSymbol(SymbolEntry* s);  // at the end of the `next` chain
    SymbolEntry* __cdecl GetNextSymbol() const;
    const SymbolEntry* __cdecl UnAlias() const;
    void __cdecl SetApiData(ApiDataBase* d) const;
    ApiDataBase* __cdecl GetApiData() const;
    ApiDataBase* __cdecl GetAndZeroApiData() const;
    unsigned long __cdecl GetHint() const;
    void __cdecl SetHint(unsigned long h);

    JET_HEAP_OBJECT

    SymbolEntry* alias;  // +0
    SymbolType type;     // +4
    int field08, field0c, field10, field14;  // +8.. (-1 when new; LikeType compares +0xc..+0x14)
    unsigned long hint;  // +0x18
    int field1c;         // +0x1c (5 when new)
    unsigned char field20;  // +0x20
    unsigned char flags;    // +0x21 bit 0 anonymous, bit 1 (compared by LikeType), bit 2 owns the alias
    int field24, field28, field2c, field30;  // +0x24 (left by the constructors)
    int field34;                 // +0x34
    PString name;                // +0x38
    int refs;                    // +0x3c
    SymbolEntry* next;           // +0x40
    mutable ApiDataBase* api;    // +0x44
};
JET_LAYOUT32(sizeof(SymbolEntry) == 0x48);

// the two languages' symbols add their class (+0x48); they share their bodies in the original
struct VertexSymbolEntry : SymbolEntry {
    VertexSymbolEntry(VertexSymbolClass cls, SymbolType type, bool referenced);
    VertexSymbolEntry(const VertexSymbolEntry& o);
    VertexSymbolEntry& operator=(const VertexSymbolEntry& o);
    ~VertexSymbolEntry();
    bool __cdecl LikeType(const VertexSymbolEntry* o) const;  // the same class, then SymbolEntry::LikeType

    VertexSymbolClass symbolClass;  // +0x48
};
JET_LAYOUT32(sizeof(VertexSymbolEntry) == 0x4c);

struct TextureSymbolEntry : SymbolEntry {
    TextureSymbolEntry(TextureSymbolClass cls, SymbolType type, bool referenced);
    TextureSymbolEntry(const TextureSymbolEntry& o);
    TextureSymbolEntry& operator=(const TextureSymbolEntry& o);
    ~TextureSymbolEntry();

    TextureSymbolClass symbolClass;  // +0x48
};
JET_LAYOUT32(sizeof(TextureSymbolEntry) == 0x4c);

// the symbols of a vertex program: an Array<VertexSymbolEntry*, 128> it owns
struct VertexSymbolTable {
    VertexSymbolTable();
    VertexSymbolTable(const VertexSymbolTable& o);  // copies the pointers
    VertexSymbolTable& operator=(const VertexSymbolTable& o);
    ~VertexSymbolTable();  // deletes the symbols

    VertexSymbolEntry* __cdecl Find(const PString& name) const;
    VertexSymbolEntry* __cdecl FindLike(const VertexSymbolEntry* e) const;  // the first of the same class and type
    void __cdecl AddSymbol(VertexSymbolEntry* e);

    unsigned long count, capacity;  // +0, +4 (capacity in steps of 128)
    VertexSymbolEntry** data;       // +8
};
JET_LAYOUT32(sizeof(VertexSymbolTable) == 0xc);

// a modifier of a register operand (negation, write mask...): copies nothing
struct RegisterModifier {
    RegisterModifier& operator=(const RegisterModifier& o);
};

struct RegisterItem;

// an indexed operand: the index register and a constant offset
struct OffsetAddress {
    OffsetAddress(RegisterItem* item, int offset);
    ~OffsetAddress();  // deletes the index register
    OffsetAddress& operator=(const OffsetAddress& o);

    RegisterItem* item;  // +0
    int offset;          // +4
};

// one operand of an instruction (0x18 bytes); it owns its swizzle text, offset and modifiers
struct RegisterItem {
    RegisterItem(SymbolEntry* symbol, char* swizzle, bool flag, RegisterModifier* m1, RegisterModifier* m2);
    explicit RegisterItem(const RegisterItem* o);  // a copy without the modifiers; the offset's register is shared
    ~RegisterItem();
    RegisterItem& operator=(const RegisterItem& o);  // a plain copy of the six fields

    SymbolEntry* symbol;     // +0
    OffsetAddress* offset;   // +4
    char* swizzle;           // +8 (copies get 6 bytes)
    bool flag;               // +0xc
    RegisterModifier* m1;    // +0x10
    RegisterModifier* m2;    // +0x14
    JET_HEAP_OBJECT
};
JET_LAYOUT32(sizeof(RegisterItem) == 0x18);

using RegisterList = Array<RegisterItem*, 128>;

// an instruction: its operands (an array it owns, deleted with them). The constructors take the array as is.
struct Instruction {
    explicit Instruction(RegisterList* operands);
    Instruction(const Instruction& o);
    Instruction& operator=(const Instruction& o);
    virtual ~Instruction();

    RegisterList* operands;  // +4
    JET_HEAP_OBJECT
};

struct VertexInstruction : Instruction {
    VertexInstruction(VertexOpcode op, RegisterList* operands);
    VertexInstruction(const VertexInstruction& o);
    VertexInstruction& operator=(const VertexInstruction& o);
    ~VertexInstruction() override;

    VertexOpcode opcode;  // +8
};

struct TextureInstruction : Instruction {
    TextureInstruction(TextureOpcode op, RegisterList* operands);
    TextureInstruction(const TextureInstruction& o);
    TextureInstruction& operator=(const TextureInstruction& o);
    ~TextureInstruction() override;

    TextureOpcode opcode;  // +8
};

// a #define handed to a program: name and value
struct PreProcessorDefine {
    PreProcessorDefine();
    PreProcessorDefine(const AnsiString& name, const AnsiString& value);
    PreProcessorDefine(const PreProcessorDefine& o);
    ~PreProcessorDefine();
    PreProcessorDefine& operator=(const PreProcessorDefine& o);

    AnsiString name, value;
};

struct PreProcessorArray : Array<PreProcessorDefine, 3> {
    PreProcessorArray();
    PreProcessorArray(const PreProcessorArray& o);
    ~PreProcessorArray();
    PreProcessorArray& operator=(const PreProcessorArray& o);

    AnsiString __cdecl ConcatenateStrings() const;  // ":name:value" for each define
};

}  // namespace Jet
