// Unit U2b of RenderSystem_7c: ExtendedGfxLanguage, the base of the vertex programs and texture combiners
// (0x10020520..0x10020f60 with its accessors folded into shared bodies elsewhere), and the texture combiner's parse
// records (TextureCombiner::Instruction / RegisterItem / ModifierItem, 0x1000fdc0..0x10010290, 0x100407f0,
// 0x10040810).
#include <jet/GfxLanguage.h>
#include <jet/Resource.h>

#include <cstring>

namespace Jet {

namespace {

// the characters of an AnsiString's block (ProcessDefines writes them in place, shared or not)
char* Chars(AnsiString& s) { return *reinterpret_cast<char**>(&s) + 8; }

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// ExtendedGfxLanguage

ExtendedGfxLanguage::ExtendedGfxLanguage(Display* d)
    : display(d), generation(1.0f), userData(0), fallback(nullptr)
{
}

ExtendedGfxLanguage::~ExtendedGfxLanguage()
{
    if (fallback) fallback->ReleaseReference();
}

bool __cdecl ExtendedGfxLanguage::SupportedByPlatform(float) { return false; }
bool __cdecl ExtendedGfxLanguage::LoadNativeFormat(const PString&, const char*) { return true; }
bool __cdecl ExtendedGfxLanguage::CompileProgram() { return true; }
bool __cdecl ExtendedGfxLanguage::Bind() const { return false; }
bool __cdecl ExtendedGfxLanguage::End() const { return false; }
bool __cdecl ExtendedGfxLanguage::Defines(const PString&) const { return false; }
bool __cdecl ExtendedGfxLanguage::Load(MemoryFile&, const PString&, const PString&) { return false; }

// 0x10020820: the text (with the defines applied) as a memory file named "built-in"
bool __cdecl ExtendedGfxLanguage::LoadProgram(const PString& name, const char* text, const PreProcessorArray* defines)
{
    AnsiString processed;
    if (defines) {
        processed = text;
        ProcessDefines(processed, defines);
        text = processed;
    }
    MemoryFile file(text, static_cast<unsigned int>(std::strlen(text)), false, 0);
    PString builtIn("built-in");
    return Load(file, name, builtIn);
}

// 0x10020950: the file the database maps `fileName` to, read whole
bool __cdecl ExtendedGfxLanguage::LoadFromFile(const ResourceDatabase* db, const PString& name,
                                               const PString& fileName, const PreProcessorArray* defines)
{
    MemoryFile file;
    bool failed;
    {
        PString path = db->GetFileName(fileName);
        failed = file.Open(path.node ? path.node->text : nullptr, 0x10, false, 0).code != 0;
    }
    if (failed) return false;
    bool ok;
    if (!defines) {
        ok = Load(file, name, fileName);
    } else {
        AnsiString text(file.start, file.size);
        ProcessDefines(text, defines);
        const char* s = text;
        MemoryFile processed(s, static_cast<unsigned int>(std::strlen(s)), false, 0);
        ok = Load(processed, name, fileName);
    }
    file.Close();
    return ok;
}

void __cdecl ExtendedGfxLanguage::SetFilename(const PString& f)
{
    filename = f;
    const unsigned int n = f.node ? f.node->length : 0;
    int at = -1;
    if (f.node) at = PString::Cache()->Find(f.node, reinterpret_cast<const unsigned char*>(".txt"), n - 4);
    if (at == static_cast<int>(n - 4) && filename.node && n - 4 < filename.node->length)
        filename.node = PString::Cache()->Del(filename.node, n - 4, filename.node->length - (n - 4));
}

void __cdecl ExtendedGfxLanguage::SetProgramName(const PString& n) { programName = n; }
void __cdecl ExtendedGfxLanguage::SetRegisteredName(const PString& n) { registeredName = n; }

// 0x100206e0
void __cdecl ExtendedGfxLanguage::InitInstance(ExtendedGfxLanguage& c) const
{
    c.display = display;
    c.source = source;
    c.filename = filename;
    c.programName = programName;
    c.registeredName = registeredName;
    c.generation = generation;
    c.symbolNames = symbolNames;
    c.userData = userData;
    c.fallback = fallback;
    if (fallback) fallback->AddReference();
}

// 0x10020c90: every occurrence of each define's name replaced by its value, written into the text's block. The
// next search starts one value length past the end of the replacement (as in the original).
void __cdecl ExtendedGfxLanguage::ProcessDefines(AnsiString& text, const PreProcessorArray* defines) const
{
    if (!defines) return;
    for (auto it = defines->First(); it.array;) {
        const PreProcessorDefine& d = (*it.array)[it.index];
        const char* name = d.name;
        const char* value = d.value;
        const int nameLength = static_cast<int>(d.name.Length());
        const int valueLength = static_cast<int>(d.value.Length());
        int at = text.Find(name, 0);
        if (at != -1) {
            const int grow = valueLength - nameLength;
            do {
                if (grow > 0)
                    text.Ins(' ', at, grow);
                else if (grow < 0)
                    text.Del(at, -grow);
                for (int i = 0; i < valueLength; ++i) Chars(text)[at++] = value[i];
                at = text.Find(name, at + valueLength);
            } while (at != -1);
        }
        if (++it.index >= it.array->Count()) it = Array<PreProcessorDefine, 3>::ConstIterator();
    }
}

void __cdecl ExtendedGfxLanguage::AddSymbolName(const PString& name, int number, bool local, int extra)
{
    SymbolName s{name, {local, number, extra}};
    symbolNames.Add(s);
}

bool __cdecl ExtendedGfxLanguage::GetSymbolId(const PString& name, GfxSymbolId& id) const
{
    for (unsigned int i = 0; i < symbolNames.Count(); ++i)
        if (symbolNames[i].name.node == name.node) {
            std::memcpy(&id, &symbolNames[i].id, sizeof id);  // the padding too
            return true;
        }
    return false;
}

int __cdecl ExtendedGfxLanguage::GetLocalConstantNumber(const PString& name) const
{
    for (unsigned int i = 0; i < symbolNames.Count(); ++i)
        if (symbolNames[i].name.node == name.node && !symbolNames[i].id.local) return symbolNames[i].id.number;
    return -1;
}

Display* __cdecl ExtendedGfxLanguage::GetDisplay() const { return display; }
const AnsiString& __cdecl ExtendedGfxLanguage::GetProgramString() const { return source; }
const PString& __cdecl ExtendedGfxLanguage::GetFilename() const { return filename; }
const PString& __cdecl ExtendedGfxLanguage::GetProgramName() const { return programName; }
const PString& __cdecl ExtendedGfxLanguage::GetRegisteredName() const { return registeredName; }
float __cdecl ExtendedGfxLanguage::GetGeneration() const { return generation; }
void __cdecl ExtendedGfxLanguage::SetGeneration(float g) { generation = g; }
unsigned long __cdecl ExtendedGfxLanguage::GetUserData() const { return userData; }
void __cdecl ExtendedGfxLanguage::SetUserData(unsigned long d) const { userData = d; }
const ExtendedGfxLanguage* __cdecl ExtendedGfxLanguage::GetFallbackProgram() const { return fallback; }
const Array<ExtendedGfxLanguage::SymbolName, 3>& ExtendedGfxLanguage::GetSymbolNameList() const { return symbolNames; }

void __cdecl ExtendedGfxLanguage::SetFallbackProgram(const ExtendedGfxLanguage* p) const
{
    if (fallback) fallback->ReleaseReference();
    fallback = p;
    if (p) p->AddReference();
}

// ---------------------------------------------------------------------------------------------------------------
// TextureCombiner parse records

namespace {

template <class T>
void Append(ParseList<T>* list, T* item)
{
    if (!item) return;
    if (!list->tail)
        list->head = item;
    else
        list->tail->next = item;
    ++list->count;
    list->tail = item;
}

ParseList<TextureCombiner::ModifierItem>* CopyModifiers(const ParseList<TextureCombiner::ModifierItem>* from)
{
    auto* list = new ParseList<TextureCombiner::ModifierItem>{nullptr, nullptr, 0};
    for (auto* m = from->head; m; m = m->next) Append(list, new TextureCombiner::ModifierItem(m->type));
    return list;
}

template <class T, class Free>
void FreeList(ParseList<T>* list, Free free)
{
    if (!list) return;
    while (T* item = list->head) {
        list->head = item->next;
        if (!item->next) list->tail = nullptr;
        --list->count;
        free(item);
    }
    Alloc::Delete(list);
}

}  // namespace

TextureCombiner::ModifierItem::ModifierItem(ModifierType t) : type(t), next(nullptr) {}

TextureCombiner::ModifierItem& TextureCombiner::ModifierItem::operator=(const ModifierItem& o)
{
    type = o.type;
    next = o.next;
    return *this;
}

TextureCombiner::RegisterItem::RegisterItem(RegType t, int i)
    : type(t), index(i), mods1(nullptr), mods2(nullptr), next(nullptr)
{
}

TextureCombiner::RegisterItem::RegisterItem(const RegisterItem* o)
    : type(o->type), index(o->index), mods1(nullptr), mods2(nullptr), next(nullptr)
{
    if (o->mods1) mods1 = CopyModifiers(o->mods1);
    if (o->mods2) mods2 = CopyModifiers(o->mods2);
}

TextureCombiner::RegisterItem::~RegisterItem()
{
    FreeList(mods1, [](ModifierItem* m) { Alloc::Delete(m); });
    mods1 = nullptr;
    FreeList(mods2, [](ModifierItem* m) { Alloc::Delete(m); });
    mods2 = nullptr;
}

TextureCombiner::RegisterItem& TextureCombiner::RegisterItem::operator=(const RegisterItem& o)
{
    type = o.type;
    index = o.index;
    mods1 = o.mods1;
    mods2 = o.mods2;
    next = o.next;
    return *this;
}

TextureCombiner::Instruction::Instruction(TextureOpcode op, ParseList<RegisterItem>* r, ParseList<ModifierItem>* m,
                                          TextureAddressType a, float v, bool f)
    : opcode(op), operands(r), mods(m), address(a), flag(f), value(v)
{
}

TextureCombiner::Instruction::Instruction(const Instruction& o)
    : opcode(o.opcode), operands(o.operands), mods(o.mods), address(o.address), flag(o.flag), field18(o.field18),
      field1c(o.field1c), field20(o.field20), field24(o.field24), value(o.value)
{
}

TextureCombiner::Instruction::Instruction(const Instruction* o)
    : opcode(o->opcode), operands(nullptr), mods(nullptr), address(o->address), flag(o->flag), value(o->value)
{
    if (o->operands) {
        operands = new ParseList<RegisterItem>{nullptr, nullptr, 0};
        for (auto* r = o->operands->head; r; r = r->next) Append(operands, new RegisterItem(r));
    }
    if (o->mods) mods = CopyModifiers(o->mods);
}

TextureCombiner::Instruction& TextureCombiner::Instruction::operator=(const Instruction& o)
{
    opcode = o.opcode;
    operands = o.operands;
    mods = o.mods;
    address = o.address;
    flag = o.flag;
    field18 = o.field18;
    field1c = o.field1c;
    field20 = o.field20;
    field24 = o.field24;
    value = o.value;
    return *this;
}

TextureCombiner::Instruction::~Instruction()
{
    FreeList(operands, [](RegisterItem* r) { delete r; });
    operands = nullptr;
    FreeList(mods, [](ModifierItem* m) { Alloc::Delete(m); });
    mods = nullptr;
}

}  // namespace Jet
