// Jet::ConfigParser / ConfigData (text configuration files) and Jet::Exception, from KernelStandard_7c, declared as
// far as the modules built so far use them. Layouts are the TS12 ones; members not read inline are opaque.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/Result.h>
#include <jet/SmallTypes.h>

namespace Jet {

class AnsiString;
class String;
class UnicodeString;
class File;

// One node of a configuration tree (0x10 bytes from KernelStandard's ConfigData pool): a name, one typed value
// and the child nodes. Value types: 0 none, 1 int, 2 float, 3 PString (the node pointer is the data), 4 bytes,
// 5 ints, 6 floats, 7 PStrings; the arrays are one Jet::Alloc block {count, items}.
class ConfigData {
public:
    using Children = FastDynamicArray<ConfigData*, 13, Less<ConfigData*, ConfigData*>>;
    enum Type { NONE = 0, INT = 1, FLOAT = 2, PSTRING = 3, BYTES = 4, INTS = 5, FLOATS = 6, PSTRINGS = 7 };
    struct ByteArray {
        int count;
        unsigned char items[1];
    };
    struct IntArray {
        int count;
        int items[1];
    };
    struct FloatArray {
        int count;
        float items[1];
    };
    struct PStringArray {
        int count;
        PString items[1];
    };
    // Where Write / WriteBin put their bytes: {vftable, ...}. Slots: deleting destructor, Write (items written),
    // Encoding (2: the text goes out as UTF-16; the base says 0, which Write treats as "ask nobody").
    friend struct ConfigGrammar;  // ConfigParser's grammar actions fill nodes directly
    class Writer {
    public:
        virtual ~Writer() {}
        virtual unsigned long Write(const void* data, unsigned long size, unsigned long count) = 0;
        virtual int Encoding() { return 0; }
    };

    ConfigData();
    explicit ConfigData(const PString& name);
    ConfigData(const PString& name, const PString& value);
    ConfigData(const ConfigData& o);
    ~ConfigData();
    ConfigData& operator=(const ConfigData& o);

    // the pool of the original module (still its PoolBlockAlloc)
    static void* __cdecl operator new(size_t size);
    static void __cdecl operator delete(void* p);
    static void* __cdecl operator new(size_t size, void* at);
    static void __cdecl operator delete(void* p, void* at);

    // children
    ConfigData& __cdecl operator<<(ConfigData* child);
    const ConfigData& __cdecl operator()(const PString& key) const;            // the child `key`
    const ConfigData& __cdecl operator()(const PString& key, int n) const;     // the n'th child `key`
    const ConfigData& __cdecl operator()(const PString& key, const PString& value) const;
    ConfigData& __cdecl operator()(const PString& key);                        // a new child (New)
    ConfigData& __cdecl operator()(const PString& key, const PString& value);  // a new child with a value
    const ConfigData& __cdecl operator[](int index) const;                     // the index'th child
    ConfigData& __cdecl operator[](int index);
    ConfigData& __cdecl New();
    ConfigData& __cdecl New(const PString& name);
    ConfigData& __cdecl New(const PString& name, const PString& value);
    const ConfigData& __cdecl Find(const PString& key) const;
    ConfigData& __cdecl Find(const PString& key);
    const ConfigData& __cdecl Find(const PString& key, const PString& value) const;
    ConfigData& __cdecl Find(const PString& key, const PString& value);
    const ConfigData& __cdecl Find(int index) const;
    ConfigData& __cdecl Find(int index);
    const ConfigData& __cdecl Find(const PString& key, int n) const;
    ConfigData& __cdecl Find(const PString& key, int n);
    int __cdecl Count(const PString& key) const;
    ConfigData& __cdecl Replace(const PString& key, int n);
    ConfigData& __cdecl Replace(const PString& key, const PString& value, int n);
    bool Remove(const ConfigData& child, int n);
    bool Remove(const PString& key, int n);
    bool Remove(const PString& key, const PString& value, int n);
    void RemoveAll(const PString& key);
    ConfigData* __cdecl Extract(const PString& key, int n);
    Children* __cdecl GetDataList();
    const Children* __cdecl GetDataList() const;
    void __cdecl AssignNodes(Children* nodes);
    int __cdecl GetChildCount() const;
    bool HasChildren() const;
    int __cdecl GetArrayCount() const;

    // name and value
    const PString& __cdecl GetName() const;
    void __cdecl SetName(const PString& name);
    bool __cdecl operator==(const PString& name) const;
    int __cdecl GetDataType() const;
    long __cdecl GetIntData() const;
    float __cdecl GetFloatData() const;
    const PString& __cdecl GetPStringData() const;
    const unsigned char* __cdecl GetByteArray() const;
    const int* __cdecl GetIntArray() const;
    const float* __cdecl GetFloatArray() const;
    const PString* __cdecl GetPStringArray() const;
    void __cdecl SetByteArray(unsigned char* items, int count);
    void __cdecl SetIntArray(int* items, int count);
    void __cdecl SetFloatArray(float* items, int count);
    void __cdecl SetPStringArray(PString* items, int count);
    void __cdecl SetPStringList(const DynamicArray<PString, 13, Less<PString, PString>>& items);
    void __cdecl AddByteArray(unsigned char* items, int count);
    void __cdecl AddIntArray(int* items, int count);
    void __cdecl AddFloatArray(float* items, int count);
    void __cdecl AddPStringArray(PString* items, int count);
    bool __cdecl SetAnsi(const AnsiString& s);
    bool __cdecl SetUnicode(const UnicodeString& s);
    bool __cdecl GetAnsi(AnsiString& s) const;
    bool __cdecl GetUnicode(UnicodeString& s) const;
    bool __cdecl CompareData(const ConfigData& o) const;
    void __cdecl CopyData(const ConfigData& o);
    unsigned long __cdecl Crc() const;
    unsigned long __cdecl Crc(const PString& skip) const;  // children named `skip` left out

    // the children as text (one "name = value;" line per value, blocks in braces); encoding 0: the writer's
    bool __cdecl Write(File& file, int encoding) const;
    bool __cdecl Write(PString& text, int encoding) const;
    bool __cdecl Write(Writer& writer, int encoding) const;
    // the node and its children in the binary form ("CD", version 0x101, then the nodes depth first)
    bool __cdecl WriteBin(Writer& writer, int encoding) const;
    bool __cdecl WriteBin(File& file, int encoding) const;
    bool __cdecl WriteBin(PString& text, int encoding) const;
    bool __cdecl WriteBin(const char* path, bool flag, int encoding) const;
    bool __cdecl WriteBin(const PString& base, const char* path, bool flag, int encoding) const;
    bool __cdecl ReadBin(File& file);
    bool __cdecl ReadBin(const char* path);
    bool __cdecl ReadBin(const PString& base, const char* path);

    operator bool() const;  // not the missing-key node
    operator const PString&() const;
    operator const char*() const;
    operator int() const;
    operator unsigned int() const;
    operator float() const;
    operator Vector2() const;
    operator Vector3() const;
    operator Vector4() const;
    operator Quaternion() const;
    operator Color() const;
    operator Point() const;
    operator Size() const;
    operator Rect() const;

    void operator<<(const PString& v);
    void operator<<(const String& v);
    void operator<<(char v);
    void operator<<(unsigned char v);
    void operator<<(bool v);
    void operator<<(short v);
    void operator<<(unsigned short v);
    void operator<<(int v);
    void operator<<(unsigned int v);
    void operator<<(long v);
    void operator<<(unsigned long v);
    void operator<<(float v);
    void operator<<(const Range<float>& v);
    void operator<<(const Vector2& v);
    void operator<<(const Vector3& v);
    void operator<<(const Vector4& v);
    void operator<<(const Color& v);
    void operator<<(const Quaternion& v);
    void operator<<(const Range<int>& v);
    void operator<<(const Point& v);
    void operator<<(const Size& v);
    void operator<<(const Rect& v);

    void operator>>(PString& out) const;
    void operator>>(String& out) const;
    void operator>>(bool& out) const;
    void operator>>(int& out) const;
    void operator>>(long& out) const;
    void operator>>(char& out) const;
    void operator>>(unsigned char& out) const;
    void operator>>(short& out) const;
    void operator>>(unsigned short& out) const;
    void operator>>(unsigned int& out) const;
    void operator>>(unsigned long& out) const;
    void operator>>(float& out) const;
    void operator>>(Range<int>& out) const;
    void operator>>(Range<float>& out) const;
    void operator>>(Vector2& out) const;
    void operator>>(Vector3& out) const;
    void operator>>(Vector4& out) const;
    void operator>>(Quaternion& out) const;
    void operator>>(Color& out) const;
    void operator>>(Point& out) const;
    void operator>>(Size& out) const;
    void operator>>(Rect& out) const;
    void operator>>(ConfigData& out) const;

    // read inline by other modules: {name PString +0, type +4, data +8}
    const PString& Name() const { return name; }
    int Kind() const { return type; }
    const int* Data() const { return reinterpret_cast<const int*>(data); }
    const PString& Value() const { return reinterpret_cast<const PString&>(data); }

    static JET_KS_FORWARDED ConfigData unknownItem;  // what a missing key returns (still the original's)

protected:
    bool __cdecl HasDataItemOnly() const;
    void __cdecl ClearData();
    void __cdecl CopyChildren(const ConfigData& o);
    PStringArray* __cdecl CopyPStringArray(PStringArray* items);
    void __cdecl SetIntData(long v);
    void __cdecl SetFloatData(float v);
    void __cdecl SetPStringData(const PString& v);
    ByteArray* __cdecl GetInnerByteArr() const;
    IntArray* __cdecl GetInnerIntArr() const;
    FloatArray* __cdecl GetInnerFloatArr() const;
    PStringArray* __cdecl GetInnerPStringArr() const;
    PString __cdecl DataToPString() const;
    bool __cdecl WriteChildList(Writer& w, int depth, int encoding) const;
    bool __cdecl WriteBlock(Writer& w, int depth, int encoding) const;
    bool WriteWrap(Writer& w, const char* text, int length, int& column, int indent, int encoding) const;
    bool __cdecl WriteDataItem(Writer& w, int depth, int encoding) const;
    bool __cdecl WriteDataBlock(Writer& w, int depth, int encoding) const;
    bool __cdecl WriteTabs(Writer& w, int depth, int encoding) const;
    bool WriteChars(Writer& w, const char* text, int length, int encoding) const;
    bool WritePString(Writer& w, const PString& s, int encoding) const;
    void __cdecl FormatDataString(String& s) const;
    bool __cdecl WriteBinFile(Writer& w, int encoding) const;
    bool __cdecl ReadBinFile(File& file);

private:
    unsigned long __cdecl CalcCRC(unsigned long crc, const PString& skip) const;

    PString name;      // +0x00
    int type;          // +0x04
    void* data;        // +0x08 int / float bits / PString node / array block
    Children children; // +0x0c
};
JET_LAYOUT32(sizeof(ConfigData) == 0x10);

class MemoryFile;

// Reads its own format: "version x.xx" then ConfigData::Write text (Save writes it; it is not the parser of
// Trainz config.txt). Flex scanner + Lemon parser inside (ConfigParser.cpp / ConfigLexer.cpp). 0x18 bytes.
class ConfigParser {
public:
    enum Results {};
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    ConfigParser();  // (the version is left uninitialised, as in the original)
    explicit ConfigParser(ConfigData* config);
    ConfigParser(const ConfigParser& o);  // shallow: both share the scanner and the tree
    ConfigParser& operator=(const ConfigParser& o);
    virtual ~ConfigParser();

    float __cdecl GetVersion() const;
    void __cdecl SetVersion(float version);
    void __cdecl SetConfig(ConfigData* config);
    ConfigData* __cdecl GetConfig();
    const ConfigData* __cdecl GetConfig() const;
    const char* __cdecl GetFileName() const;
    void __cdecl SetFileName(const char* name);
    void SetResourceKey(const String& key);

    Result __cdecl Parse(const PString& base, const char* path);
    Result __cdecl Parse(MemoryFile& file);
    Result __cdecl Parse(const char* path);
    Result __cdecl Parse(const char* name, void* data, unsigned int size);
    bool Save(const char* path, bool flag, int encoding) const;
    bool Save(const char* base, const char* path, bool flag, int encoding) const;
    bool SaveString(PString& text, int encoding) const;

    const ConfigData& Root() const { return *root; }  // GetConfigRoot, inlined by the original's callers
    const ConfigData* RootPtr() const { return root; }

    static void* operator new(size_t size);  // Jet::Alloc
    static void operator delete(void* p);

protected:
    Result __cdecl Parse(const char* unused, const char* name, MemoryFile& file);
    bool WriteData(File& file, int encoding) const;
    const ConfigData* GetConfigRoot() const;

private:
    friend struct ConfigGrammar;

    class ConfigLexer* core;  // +0x04 the scanner of the last Parse
    PString fileName;         // +0x08
    ConfigData* root;         // +0x0c
    float version;            // +0x10
    void* key;                // +0x14 an AnsiString (the resource key for MemoryFile::Open)
};
JET_LAYOUT32(sizeof(ConfigParser) == 0x18);

// {vptr, description[0x1000] +4 (the call stack text), reason[0x100] +0x1004, hardware +0x1104, line +0x1108,
// file +0x110c}, 0x1110 bytes (unit U12a, Exception.cpp). vftable: deleting destructor, Class,
// IsHardwareException, Description, Reason.
class Exception {
public:
    Exception();
    Exception(const char* file, int line);  // description "No call stack available\r\n"
    Exception(const char* file, int line, const char* message);  // message: the reason
    Exception(const Exception& o);
    virtual ~Exception();
    virtual const char* __cdecl Class() const;  // "Unknown"
    virtual bool __cdecl IsHardwareException() const;
    virtual const char* __cdecl Description() const;
    virtual const char* __cdecl Reason() const;
    Exception& operator=(const Exception& o);
    void __cdecl SetReasonf(const char* fmt, ...);
    void __cdecl VSetReasonf(const char* fmt, char* args);
    void __cdecl AddReasonf(const char* fmt, ...);  // "  -  " and the text appended
    void __cdecl SetDescriptionf(const char* fmt, ...);
    void __cdecl VSetDescriptionf(const char* fmt, char* args);
    void __cdecl InitMembers();
    int __cdecl Line() const;
    const char* __cdecl File() const;
    bool __cdecl HardwareException() const;
    void __cdecl SetHardwareException(bool on);
    void __cdecl Log(int level);  // does nothing
    JET_HEAP_OBJECT  // the deleting destructor frees with Alloc::Delete

private:
#if defined(JET_KS_BUILD)
    char description[0x1000];  // +0x0004
    char reason[0x100];        // +0x1004
    bool hardware;             // +0x1104
    int line;                  // +0x1108
    const char* file;          // +0x110c
#else
    unsigned char data[0x110c];
#endif
};
JET_LAYOUT32(sizeof(Exception) == 0x1110);

// what Jet's file code throws on an I/O error; ours since U10a (File.cpp), Class() is "File"
class FileEx : public Exception {
public:
    FileEx(const char* file, int line);
    FileEx(const char* file, int line, const char* message);
    FileEx(const FileEx& o);
    FileEx& operator=(const FileEx& o);
    virtual ~FileEx();
    const char* __cdecl Class() const override;
};

}  // namespace Jet
