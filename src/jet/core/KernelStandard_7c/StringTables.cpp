// Unit U7c2 of KernelStandard_7c: StringTablesBase and StringTablesDefaultTraits (0x1003dcd0..0x1003f804). The
// data exports (fileTblId, results, the four token PStrings) are ours since U14b. Notes: D:/trainz12-decomp/notes/KernelStandard.md ("U7c2").
#include <jet/Config.h>
#include <jet/KernelStandard.h>
#include <jet/StringTables.h>

#include <cstring>
#include <new>

namespace Jet {

const char StringTablesBase::fileTblId[4] = {'T', 'A', 'B', 'L'};  // 0x10052218 (no NUL)
const PString StringTablesBase::TABLE_TOKEN("Table");              // 0x100ae1f8
const PString StringTablesBase::ID_TOKEN("Id");                    // 0x100ae1fc
const PString StringTablesBase::ENTRY_TOKEN("Entry");              // 0x100ae200
const PString StringTablesBase::VALUE_TOKEN("Value");              // 0x100ae204

// exported as ?results@StringTablesBase@Jet@@2QBUResultTable@2@B (implemented.txt): {name, text} pairs
extern const char* const StringTablesResultTable[] = {
    "RES_UNKNOWN", "Unknown error",
    "RES_NAMESRESOLVING_NOTDEFINED", "Not supported if namesResolving equal to false",
    "RES_FILE_OPEN_ERROR", "Unable to open file",
    "RES_FILE_WRITE_ERROR", "Can not write to the file",
    "RES_FILE_FORMAT_ERROR", "Unexpected file format",
    "RES_FILE_SUBSTNOTFOUND", "Subst not found",
    "RES_FILE_READONLY", "File is read-only",
    "RES_TABLE_EXISTS", "Table already exists",
    "RES_TABLE_NOTFOUND", "Table not found",
    "RES_CONVERT_ERROR", "Can not convert string to current character set",
    nullptr};

namespace {

const char* const kSource = "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/KernelStandard/StringTables.cpp";

// an AnsiString's buffer
struct AnsiBuf {
    unsigned capacity;
    unsigned length;
    char text[1];
};

const char* Text(const PString& p) { return p.node ? p.node->text : nullptr; }

}  // namespace

// ---- StringTablesDefaultTraits (0x1003dcd0..0x1003de1f)

StringTablesDefaultTraits& StringTablesDefaultTraits::operator=(const StringTablesDefaultTraits&) { return *this; }

char* __cdecl StringTablesDefaultTraits::NewKey(const char* const& key)
{
    char* const k = static_cast<char*>(Alloc::New(static_cast<unsigned>(std::strlen(key) + 1)));
    std::strcpy(k, key);
    return k;
}

char* __cdecl StringTablesDefaultTraits::NewKeyFromCharPtr(const char* key) { return NewKey(key); }

void __cdecl StringTablesDefaultTraits::DeleteKey(char* key)
{
    if (key) Alloc::Delete(key);
}

unsigned int __cdecl StringTablesDefaultTraits::GetHashVal(const char* key) const
{
    unsigned int h = 0;
    for (; *key; ++key) h = h * 31 + static_cast<int>(static_cast<signed char>(*key));
    return h;
}

int __cdecl StringTablesDefaultTraits::Compare(const char* a, const char* b) const
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(a);
    const unsigned char* q = reinterpret_cast<const unsigned char*>(b);
    for (; *p == *q; ++p, ++q)
        if (!*p) return 0;
    return *p < *q ? -1 : 1;
}

// (inlined in the original: ClearData, the node's string from the UTF-16 text, type PString)
bool __cdecl StringTablesDefaultTraits::DataToConfig(ConfigData& config, const UnicodeString& data)
{
    return config.SetUnicode(data);
}

bool __cdecl StringTablesDefaultTraits::DataToDynamicBuffer(DynamicBuffer& buffer, const UnicodeString& data)
{
    return buffer.Write(data);
}

bool __cdecl StringTablesDefaultTraits::DataFromConfig(const ConfigData& config, UnicodeString& data)
{
    return config.GetUnicode(data);
}

bool __cdecl StringTablesDefaultTraits::DataFromDynamicBuffer(DynamicBuffer& buffer, UnicodeString& data)
{
    return buffer.Read(data);
}

// ---- StringTablesBase: construction (0x1003de20..0x1003e087)

// (+4 and +8 are the derived class's to set)
StringTablesBase::StringTablesBase() {}

// the text copied with capacity = length (UnicodeString::Set)
void StringTablesBase::CopyUnicode(UnicodeString& to, const UnicodeString& from)
{
    const int length = static_cast<int>(from.length);
    to.Set(reinterpret_cast<const __wchar_t*>(from.text), length, length);
}

StringTablesBase::StringTablesBase(const StringTablesBase& o)
    : keyCharSize(o.keyCharSize), namesResolving(o.namesResolving), unknown0c(o.unknown0c), unknown1c(o.unknown1c)
{
    CopyUnicode(unknown10, o.unknown10);
}

StringTablesBase& StringTablesBase::operator=(const StringTablesBase& o)
{
    keyCharSize = o.keyCharSize;
    namesResolving = o.namesResolving;
    unknown0c = o.unknown0c;
    CopyUnicode(unknown10, o.unknown10);
    unknown1c = o.unknown1c;
    return *this;
}

StringTablesBase::~StringTablesBase() {}

bool __cdecl StringTablesBase::IsNamesResolving() const { return namesResolving; }

void __cdecl StringTablesBase::LogErr(const char*, ...) {}
void __cdecl StringTablesBase::LogWarn(const char*, ...) {}

// ---- config input (0x1003e090..0x1003e39c)

// "base:path" (each part cut to MAX_PATH, as the original's strncpy / strncat do) when the base has text
StringTablesBase::Result __cdecl StringTablesBase::AddConfigData(const PString& base, const char* path, bool flag)
{
    if (base.node && base.node->length) {
        char full[260];
        std::strncpy(full, base.node->text, 260);
        std::strncat(full, ":", 260);
        std::strncat(full, path, 260);
        return AddConfigData(static_cast<const char*>(full), flag);
    }
    return AddConfigData(path, flag);
}

StringTablesBase::Result __cdecl StringTablesBase::AddConfigData(const char* path, bool flag)
{
    ConfigParser parser;
    const ConfigParser::Result r = parser.Parse(path);
    if (r.code) return r.code == 2 ? 3 : 8;
    if (!parser.RootPtr()) return 0;
    return AddConfigData(parser.Root(), flag);
}

StringTablesBase::Result __cdecl StringTablesBase::AddConfigData(void* data, unsigned int size, bool flag)
{
    ConfigParser parser;
    const ConfigParser::Result r = parser.Parse("StringTableBuffer", data, size);
    if (r.code) return 8;
    if (!parser.RootPtr()) return 0;
    return AddConfigData(parser.Root(), flag);
}

StringTablesBase::Result __cdecl StringTablesBase::AddConfigData(const ConfigData& config, bool flag)
{
    return ScanConfigData(config, flag);
}

// ---- binary input (0x1003e3a0..0x1003ea7d)

StringTablesBase::Result __cdecl StringTablesBase::AddBin(const PString& base, const char* path, bool flag)
{
    if (base.node && base.node->length) {
        char full[260];
        std::strncpy(full, base.node->text, 260);
        std::strncat(full, ":", 260);
        std::strncat(full, path, 260);
        return AddBin(static_cast<const char*>(full), flag);
    }
    return AddBin(path, flag);
}

// File::Open results: 2 → 4, 7 → 9, others → 3; a failed close is thrown and caught (the result stays)
StringTablesBase::Result __cdecl StringTablesBase::AddBin(const char* path, bool flag)
{
    File file;
    const File::Result r = file.Open(path, File::READ, 0);
    if (r.code) return r.code == 2 ? 4 : r.code == 7 ? 9 : 3;
    const Result result = AddBin(file, flag);
    try {
        if (file.Close()) throw FileEx(kSource, 0xa8, "File close error");
    } catch (FileEx&) {
    }
    return result;
}

// the whole file into a buffer; a short read is 6
StringTablesBase::Result __cdecl StringTablesBase::AddBin(File& file, bool flag)
{
    const unsigned long size = file.Size();
    DynamicBuffer buffer(size);  // (the original builds the same inline)
    try {
        if (file.Read(buffer.data, size, 1) != size) throw FileEx(kSource, 0xc2, "File read error");
    } catch (FileEx&) {
        return 6;
    }
    buffer.size = size > buffer.capacity ? buffer.capacity : size;
    return AddBin(buffer, flag);
}

// The header {'S', 'T', u16 0x101, names flag, key character size} must match how this table reads names; then
// while 12 bytes are left, a table: "TABL", long id, [name], long count, then per entry long id, [name] and the
// data, which AddEntry reads. A name is a long length and the bytes; resolved names go to one growing buffer
// (a bigger one replaces it without freeing it) followed by keyCharSize NULs, others are skipped.
StringTablesBase::Result __cdecl StringTablesBase::AddBin(DynamicBuffer& buffer, bool flag)
{
    DynamicBuffer& b = buffer;  // (read inline by the original)
    unsigned char header[8];  // (left as it was on the stack where the buffer is short, as in the original)
    if (b.position < b.size) {
        unsigned long n = b.size - b.position;
        if (n > 8) n = 8;
        if (b.data && n) {
            std::memcpy(header, b.data + b.position, n);
            b.position += n;
        }
    }
    if (header[0] != 'S' || header[1] != 'T' || *reinterpret_cast<unsigned short*>(header + 2) != 0x101) return 8;
    const bool names = header[4] != 0;
    if (names ? namesResolving && header[5] != keyCharSize : namesResolving) return 8;

    char* name = nullptr;
    unsigned long capacity = 0;
    const auto fail = [&](int code) -> Result {
        if (name) Alloc::Delete(name);
        return code;
    };
    const auto readName = [&]() -> bool {
        unsigned long length;
        if (!buffer.Read(reinterpret_cast<long&>(length))) return false;
        if (length > b.size - b.position) return false;
        if (namesResolving) {
            if (length >= capacity) {
                capacity = length + 0x200;
                name = static_cast<char*>(Alloc::New(capacity));
            }
            if (length) buffer.Read(name, length, 1);
            for (unsigned i = 0; i < keyCharSize; ++i) name[length + i] = 0;
        } else {
            buffer.Seek(b.position + length);
        }
        return true;
    };
    while (b.size - b.position >= 12) {
        char tag[4];
        buffer.Read(tag, 4, 1);
        if (std::strncmp(tag, fileTblId, 4)) return fail(8);
        long tableId = 0;
        if (!buffer.Read(tableId)) return fail(8);
        if (names && !readName()) return fail(8);
        void* const table = AddTable(static_cast<const char*>(name), tableId);
        if (!table) return fail(8);
        unsigned long count;
        if (!buffer.Read(reinterpret_cast<long&>(count))) return fail(8);
        for (unsigned long i = 0; i < count; ++i) {
            long id;
            if (!buffer.Read(id)) return fail(8);
            if (names && !readName()) return fail(8);
            const Result r = AddEntry(table, static_cast<const char*>(name), id, buffer, flag);
            if (r.code) return fail(r.code);
        }
    }
    return fail(0);
}

// ---- output (0x1003ea80..0x1003f0d5)

// (names must be resolved to write config text)
StringTablesBase::Result __cdecl StringTablesBase::SaveConfigData(const PString& base, const char* path,
                                                                  unsigned long flags)
{
    if (!namesResolving) return 2;
    if (base.node && base.node->length) {
        char full[260];
        std::strncpy(full, base.node->text, 260);
        std::strncat(full, ":", 260);
        std::strncat(full, path, 260);
        return SaveConfigData(static_cast<const char*>(full), flags);
    }
    return SaveConfigData(path, flags);
}

// a root named by the path, filled by the derived class (on failure the root is left, as in the original), saved
// through a ConfigParser that owns it
StringTablesBase::Result __cdecl StringTablesBase::SaveConfigData(const char* path, unsigned long flags)
{
    if (!namesResolving) return 2;
    ConfigData* root = nullptr;
    if (void* const mem = ConfigData::operator new(sizeof(ConfigData))) {
        PString name;
        if (path)
            name.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(path),
                                                 static_cast<unsigned>(std::strlen(path)));
        root = ::new (mem) ConfigData(name);
    }
    const Result r = SaveConfigData(*root, flags);
    if (r.code) return r;
    ConfigParser parser(root);
    return parser.Save(nullptr, path, false, 0) ? 0 : 7;
}

StringTablesBase::Result __cdecl StringTablesBase::SaveBin(const PString& base, const char* path,
                                                           unsigned long flags)
{
    if (base.node && base.node->length) {
        char full[260];
        std::strncpy(full, base.node->text, 260);
        std::strncat(full, ":", 260);
        std::strncat(full, path, 260);
        return SaveBin(static_cast<const char*>(full), flags);
    }
    return SaveBin(path, flags);
}

// File::Open results: 9 → 10, others → 5; a failed close is 7
StringTablesBase::Result __cdecl StringTablesBase::SaveBin(const char* path, unsigned long flags)
{
    File file;
    const File::Result r = file.Open(path, File::CREATE_WRITE, 0);
    if (r.code) return r.code == 9 ? 10 : 5;
    const Result result = SaveBin(file, flags);
    try {
        if (file.Close()) throw FileEx(kSource, 0x1c3, "File close error");
    } catch (FileEx&) {
        return 7;
    }
    return result;
}

// the derived class writes into a 32 KB buffer, which goes to the file in one write (a short write is 7)
StringTablesBase::Result __cdecl StringTablesBase::SaveBin(File& file, unsigned long flags)
{
    DynamicBuffer buffer(0x8000, 0x8000);
    const Result r = SaveBin(buffer, flags);
    if (r.code) return r;
    try {
        const unsigned long size = buffer.size;
        if (file.Write(buffer.data, size, 1) != size) throw FileEx(kSource, 0x1e0, "File write error");
    } catch (FileEx&) {
        return 7;
    }
    return 0;
}

// ---- config scanning (0x1003f0e0..0x1003f804)

// each dot-separated part the first child of that name (the missing-key node counts as none), or a new one
ConfigData& __cdecl StringTablesBase::CreateNode(ConfigData& parent, const char* path)
{
    PString name;
    ConfigData* node = &parent;
    for (const char* part = path;;) {
        const char* const dot = std::strchr(part, '.');
        unsigned length = dot ? static_cast<unsigned>(dot - part) : 0x7fffffffu;
        const unsigned whole = static_cast<unsigned>(std::strlen(part));
        if (length > whole) length = whole;
        name = PString();
        name.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(part), length);
        ConfigData* found = nullptr;
        for (auto i = node->GetDataList()->First(); i.node; ++i) {
            if ((*i)->Name().node == name.node) {
                found = *i;
                break;
            }
        }
        if (!found || found == &ConfigData::unknownItem) {
            void* const mem = ConfigData::operator new(sizeof(ConfigData));
            ConfigData* const child = mem ? ::new (mem) ConfigData(name) : nullptr;
            *node << child;
            node = child;
        } else {
            node = found;
        }
        if (!dot) return *node;
        part = dot + 1;
    }
}

// each top-level node scanned with an empty path
StringTablesBase::Result StringTablesBase::ScanConfigData(const ConfigData& config, bool flag)
{
    AnsiString path;
    for (int i = 0;; ++i) {
        const ConfigData& child = config[i];
        if (&child == &ConfigData::unknownItem) return 0;
        const Result r = ScanConfigData(child, reinterpret_cast<String&>(path), flag);
        if (r.code) return r;
    }
}

// The node's name goes on the path: a node without a value is a group (a dot, then its children); a node whose
// value is TABLE_TOKEN is a table, with a non-zero ID_TOKEN, given to AddTable with the path (or an empty name
// when names are not resolved) and its children scanned for entries. The path is cut back on success only.
StringTablesBase::Result StringTablesBase::ScanConfigData(const ConfigData& node, String& path, bool flag)
{
    AnsiString& p = path;
    const unsigned length = static_cast<AnsiBuf*>(p.data)->length;
    p.Ins(Text(node.Name()), length);
    if (node.Kind() == ConfigData::NONE) {
        p.Ins('.', static_cast<AnsiBuf*>(p.data)->length, 1);
        for (int i = 0;; ++i) {
            const ConfigData& child = node[i];
            if (&child == &ConfigData::unknownItem) break;
            const Result r = ScanConfigData(child, path, flag);
            if (r.code) return r;
        }
    } else if (node.Kind() == ConfigData::PSTRING && node.Value().node == TABLE_TOKEN.node) {
        const ConfigData& idNode = node.Find(ID_TOKEN);
        if (&idNode == &ConfigData::unknownItem) return 8;
        const unsigned id = idNode;
        if (!id) return 8;
        void* table;
        if (namesResolving) {
            table = AddTable(static_cast<const String&>(path), id);
        } else {
            AnsiString none;
            table = AddTable(reinterpret_cast<const String&>(none), id);
        }
        if (!table) return 8;
        AnsiString entryPath;
        for (int i = 0;; ++i) {
            const ConfigData& entry = node[i];
            if (&entry == &ConfigData::unknownItem) break;
            const Result r = ScanTableConfigData(entry, table, path, reinterpret_cast<String&>(entryPath), flag);
            if (r.code) return r;
        }
    }
    p.MakeBufferUnique((length + 0x11) & ~0xfu, true);
    AnsiBuf* const buf = static_cast<AnsiBuf*>(p.data);
    if (length < buf->length) buf->length = length;
    buf->text[length] = 0;
    return 0;
}

// The same walk inside a table: groups add to the entry path, a node whose value is ENTRY_TOKEN with a non-zero
// ID_TOKEN goes to AddEntry (its result is not looked at) with the entry path or an empty name.
StringTablesBase::Result StringTablesBase::ScanTableConfigData(const ConfigData& node, void* table,
                                                               const String& tablePath, String& path, bool flag)
{
    AnsiString none;
    AnsiString& p = path;
    const unsigned length = static_cast<AnsiBuf*>(p.data)->length;
    p.Ins(Text(node.Name()), length);
    if (node.Kind() == ConfigData::NONE) {
        p.Ins('.', static_cast<AnsiBuf*>(p.data)->length, 1);
        for (int i = 0;; ++i) {
            const ConfigData& child = node[i];
            if (&child == &ConfigData::unknownItem) break;
            const Result r = ScanTableConfigData(child, table, tablePath, path, flag);
            if (r.code) return r;
        }
    } else if (node.Kind() == ConfigData::PSTRING && node.Value().node == ENTRY_TOKEN.node) {
        const ConfigData& idNode = node.Find(ID_TOKEN);
        if (&idNode == &ConfigData::unknownItem) return 8;
        const unsigned id = idNode;
        if (!id) return 8;
        AddEntry(table, namesResolving ? static_cast<const String&>(path) : reinterpret_cast<const String&>(none),
                 id, node, flag);
    }
    p.MakeBufferUnique((length + 0x11) & ~0xfu, true);
    AnsiBuf* const buf = static_cast<AnsiBuf*>(p.data);
    if (length < buf->length) buf->length = length;
    buf->text[length] = 0;
    return 0;
}

}  // namespace Jet
