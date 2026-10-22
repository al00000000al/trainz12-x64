// Jet::StringTablesBase / StringTablesDefaultTraits (KernelStandard_7c, StringTables.cpp): the reader and writer
// of string tables (localisation), as config text or a binary file. The base is abstract: the derived table class
// (a template over its traits, in the modules that use it) keeps the entries and supplies the pure slots.
#pragma once

#include <jet/Config.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/Result.h>

namespace Jet {

// The key and data helpers of the default table: keys are Jet::Alloc'ed C strings, data a UnicodeString.
struct StringTablesDefaultTraits {
    StringTablesDefaultTraits& operator=(const StringTablesDefaultTraits& o);  // (0x1000db10, shared)
    char* __cdecl NewKey(const char* const& key);
    char* __cdecl NewKeyFromCharPtr(const char* key);
    void __cdecl DeleteKey(char* key);
    unsigned int __cdecl GetHashVal(const char* key) const;  // h = h * 31 + (signed char)c
    int __cdecl Compare(const char* a, const char* b) const;  // -1 / 0 / 1, bytes unsigned
    bool __cdecl DataToConfig(ConfigData& config, const UnicodeString& data);
    bool __cdecl DataToDynamicBuffer(DynamicBuffer& buffer, const UnicodeString& data);
    bool __cdecl DataFromConfig(const ConfigData& config, UnicodeString& data);
    bool __cdecl DataFromDynamicBuffer(DynamicBuffer& buffer, UnicodeString& data);
};

// 0x20 bytes. The binary form ("ST", version 0x101, a names flag and the key character size, then per table
// "TABL", id, [name], entry count, per entry id, [name], data) and the config form (a node whose value is
// TABLE_TOKEN with an ID_TOKEN child is a table, its ENTRY_TOKEN nodes with an ID_TOKEN child the entries; names
// are the dotted paths) are read here and handed to the derived class through AddTable / AddEntry.
class StringTablesBase {
public:
    enum Results {};
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    StringTablesBase();
    StringTablesBase(const StringTablesBase& o);
    StringTablesBase& operator=(const StringTablesBase& o);

    // vftable 0x1005607c; MSVC puts the overloads of one name in reverse declaration order
    virtual ~StringTablesBase();
    virtual Result __cdecl AddConfigData(const PString& base, const char* path, bool flag);  // slot 4
    virtual Result __cdecl AddConfigData(const char* path, bool flag);                       // slot 3
    virtual Result __cdecl AddConfigData(void* data, unsigned int size, bool flag);          // slot 2
    virtual Result __cdecl AddConfigData(const ConfigData& config, bool flag);               // slot 1
    virtual Result __cdecl AddBin(const PString& base, const char* path, bool flag);         // slot 8
    virtual Result __cdecl AddBin(const char* path, bool flag);                              // slot 7
    virtual Result __cdecl AddBin(File& file, bool flag);                                    // slot 6
    virtual Result __cdecl AddBin(DynamicBuffer& buffer, bool flag);                         // slot 5
    virtual Result __cdecl SaveConfigData(const PString& base, const char* path, unsigned long flags);  // slot 11
    virtual Result __cdecl SaveConfigData(const char* path, unsigned long flags);                       // slot 10
    virtual Result __cdecl SaveConfigData(ConfigData& config, unsigned long flags) = 0;                 // slot 9
    virtual Result __cdecl SaveBin(const PString& base, const char* path, unsigned long flags);  // slot 15
    virtual Result __cdecl SaveBin(const char* path, unsigned long flags);                       // slot 14
    virtual Result __cdecl SaveBin(File& file, unsigned long flags);                             // slot 13
    virtual Result __cdecl SaveBin(DynamicBuffer& buffer, unsigned long flags) = 0;              // slot 12
    // a new table; null fails the read (the config form passes the table's path, the binary form its stored
    // name, each an empty name unless names are resolved)
    virtual void* __cdecl AddTable(const String& name, unsigned int id) = 0;  // slot 17
    virtual void* __cdecl AddTable(const char* name, long id) = 0;            // slot 16
    // an entry of `table`, its data still in the node / at the buffer's position; the config form ignores the
    // result
    virtual Result __cdecl AddEntry(void* table, const String& name, unsigned int id, const ConfigData& node,
                                    bool flag) = 0;  // slot 19
    virtual Result __cdecl AddEntry(void* table, const char* name, long id, DynamicBuffer& buffer,
                                    bool flag) = 0;  // slot 18

    bool __cdecl IsNamesResolving() const;
    // the deleting destructors free with Jet::Alloc
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
    static ConfigData& __cdecl CreateNode(ConfigData& parent, const char* path);  // the dotted path, made as needed
    static void __cdecl LogErr(const char* fmt, ...);   // (empty)
    static void __cdecl LogWarn(const char* fmt, ...);  // (empty)
    Result ScanConfigData(const ConfigData& config, bool flag);
    Result ScanConfigData(const ConfigData& node, String& path, bool flag);
    static void CopyUnicode(UnicodeString& to, const UnicodeString& from);  // (inlined in the original)
    Result ScanTableConfigData(const ConfigData& node, void* table, const String& tablePath, String& path,
                               bool flag);

    static JET_KS_FORWARDED const char fileTblId[];  // "TABL"
    static JET_KS_FORWARDED const PString TABLE_TOKEN;
    static JET_KS_FORWARDED const PString ID_TOKEN;
    static JET_KS_FORWARDED const PString ENTRY_TOKEN;
    static JET_KS_FORWARDED const PString VALUE_TOKEN;

    unsigned int keyCharSize;  // +0x04 the NULs after a stored name (and the size the binary header must give)
    bool namesResolving;       // +0x08 names are read (and SaveConfigData allowed)
    AnsiString unknown0c;      // +0x0c
    UnicodeString unknown10;   // +0x10
    AnsiString unknown1c;      // +0x1c
};
JET_LAYOUT32(sizeof(StringTablesBase) == 0x20);

}  // namespace Jet
