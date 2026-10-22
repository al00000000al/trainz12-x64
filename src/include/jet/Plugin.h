// Jet's plugin classes (Kernel_7c), declared so that layouts, vtables and mangled names match TS12.
// Access specifiers and calling conventions follow the exported mangled names; many members are __cdecl.
// Classes other modules derive from or read inline (PluginType, PluginConfig, PluginInstance, PluginCollection,
// the factory templates, Compress, Encrypt) must keep this exact shape.
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/UId.h>

namespace Jet {

class PluginConfig;
class PluginGroup;
class PluginLibrary;
class PluginFactory;
class PluginType;

using PluginConfigList = FastDynamicArray<PluginConfig*, 13, Less<PluginConfig*, PluginConfig*>>;
using PluginTypeList = FastDynamicArray<PluginType*, 13, Less<PluginType*, PluginType*>>;

// Jet::Result: a result code returned by value (in memory, like the original: it has a constructor).
class Result {
public:
    Result(int c = 0) : code(c) {}
    int code;
};

class PluginType {
public:
    PluginType(const PString& name, const UId& id, const UId& parentId);
    PluginType(const PluginType& o);
    PluginType& operator=(const PluginType& o);
    bool __cdecl operator==(const PString& name) const;
    bool __cdecl operator==(const PluginType& o) const;
    const UId& __cdecl GetId() const;
    const UId& __cdecl GetParentId() const;
    const PString& __cdecl GetName() const;
    const PluginGroup* __cdecl GetParent() const;
    virtual void __cdecl AddReference() const;
    virtual void __cdecl ReleaseReference() const;
    virtual ~PluginType();
    JET_HEAP_OBJECT

private:
    friend class PluginSystem;
    friend class PluginGroup;
    friend class PluginLibrary;
    friend class Input;  // InputSystem_7c compares a driver's factory id with a new factory's
    PluginType();
    PluginLibrary* __cdecl GetLibrary();
    void __cdecl SetLibrary(PluginLibrary* lib);

    UId uniqueId;          // +0x04
    PString name;          // +0x14
    PluginLibrary* lib;    // +0x18
    PluginGroup* parent;   // +0x1c
};

class PluginConfig {
public:
    PluginConfig(const PluginConfig& o);
    ~PluginConfig();
    PluginConfig& operator=(const PluginConfig& o);
    bool __cdecl operator==(const UId& id) const;
    const PString& __cdecl GetName() const;
    const UId& __cdecl GetId() const;
    const UId& __cdecl GetParentId() const;
    const PluginConfig* __cdecl GetParent() const;
    bool __cdecl HasChildren() const;
    bool __cdecl IsGroup() const;
    bool __cdecl IsMissing() const;
    void __cdecl Enumerate(PluginConfigList& list) const;
    JET_HEAP_OBJECT

private:
    friend class PluginSystem;
    PluginConfig(const PString& name, const UId& id, PluginLibrary* lib, PluginConfig* parent = nullptr,
                 bool isGroup = false);
    void __cdecl SetMissing(bool missing = true);
    void __cdecl Add(PluginConfig* child);
    PluginLibrary* __cdecl GetLibrary() const;

    PString name;               // +0x00
    UId id;                     // +0x04
    PluginConfig* parent;       // +0x14
    PluginLibrary* lib;         // +0x18
    PluginType* type;           // +0x1c
    PluginConfigList children;  // +0x20
    bool isGroup;               // +0x24
    bool isMissing;             // +0x25
};

// TS12: the reference count is a *virtual* base (vbptr +0, factory +4, vtordisp +8, ReferenceCount +0xc).
class PluginInstance : public virtual ReferenceCount {
public:
    PluginInstance();
    virtual ~PluginInstance();
    const UId& __cdecl GetID() const;
    const PString& __cdecl GetName() const;
    JET_HEAP_OBJECT

protected:
    void __cdecl DeleteThis() const override;

private:
    friend class PluginFactory;
    friend class Input;
    void __cdecl Assign(const PluginFactory* factory);
    const PluginFactory* pluginFactory;
};

class PluginFactory : public PluginType {
public:
    PluginFactory(const PString& name, const UId& id, const UId& parentId) : PluginType(name, id, parentId) {}

protected:
    void AssignFactory(PluginInstance* plugin) const { plugin->Assign(this); }
};

template <class Base>
class PluginFactoryBase : public PluginFactory {
public:
    PluginFactoryBase(const PString& name, const UId& id, const UId& parentId) : PluginFactory(name, id, parentId) {}
    virtual Base* __cdecl CreateInstance() const = 0;
};

template <class T, class Base>
class PluginFactoryOf : public PluginFactoryBase<Base> {
public:
    PluginFactoryOf(const PString& name, const UId& id, const UId& parentId)
        : PluginFactoryBase<Base>(name, id, parentId) {}
    Base* __cdecl CreateInstance() const override
    {
        Base* b = new T;
        this->AssignFactory(b);
        return b;
    }
};

class PluginBase {
public:
    PluginBase& operator=(const PluginBase&);
    static bool __cdecl IsRemapped(const UId& id);
    static UId __cdecl GetRemap(const UId& id);

protected:
    static Result __cdecl LookupPluginType(PluginType** plugin, const UId& id);
    static Result __cdecl LookupPluginType(PluginType** plugin, const UId& parentId, const PString& name);
    static Result __cdecl LookupPluginType(PluginConfigList& list, const UId& parentId);
    static Result __cdecl LookupPluginType(PluginConfigList& list, const UId& parentId, const PString& name);
};

class PluginCollection : public PluginConfigList, public PluginBase {
public:
    PluginCollection(const UId& id);
    PluginCollection(const UId& id, const PString& name);
    PluginCollection(const PluginCollection& o);
    ~PluginCollection();
    PluginCollection& operator=(const PluginCollection& o);
};

// ---- compression and encryption plugin bases
class Compress : public PluginInstance {
public:
    static Compress* __cdecl CreateInstance(const UId& id);
    static Compress* __cdecl CreateInstance(const PString& name);
    static Compress* __cdecl CreateInstance();
    unsigned long CompressBlock(void* dst, unsigned long dstSize, const void* src, unsigned long srcSize,
                                unsigned int ratio);
    unsigned long DecompressBlock(void* dst, unsigned long dstSize, const void* src, unsigned long srcSize);
    static const UId uid;

    // vtable slots 0 and 1 (not exported; TS12 replaced the SDK's stream methods with these)
    virtual bool CompressStream(IOSlave* dst, IOSlave* src, unsigned long srcSize, unsigned int level) = 0;
    virtual bool DecompressStream(IOSlave* dst, IOSlave* src, unsigned long srcSize) = 0;

protected:
    Compress();
    virtual ~Compress();
    virtual bool Init();  // slot 2

private:
    friend void __fastcall Jet_CompressVbaseDestructor(Compress* self);
};

struct ResultTable;

class Encrypt : public PluginInstance {
public:
    enum Results { UNKNOWN = 1, INVALID_PARAMETERS, KEY_TOO_SHORT, KEY_TOO_LONG, BUFFER_TOO_SMALL, BUFFER_TOO_LARGE, MAX };
    struct Caps {
        unsigned long keyMinSize, keyMaxSize, bufferMinSize, bufferMaxSize, bufferAlignment;
    };
    // the ResultClass<results, Results> the original returns: one int, returned in memory
    class EResult {
    public:
        EResult(int c = 0) : code(c) {}
        int code;
    };

    static Encrypt* __cdecl CreateInstance(const UId& id);
    static Encrypt* __cdecl CreateInstance(const PString& name);
    static Encrypt* __cdecl CreateInstance();
    static const ResultTable* const results;
    static const UId uid;

    // vtable slots 0-4
    virtual EResult GetCaps(Caps& caps) = 0;
    virtual EResult SetKey(const void* key, unsigned long size) = 0;
    virtual unsigned long GetOptimalSize(unsigned long size) = 0;
    virtual EResult EncryptData(void* data, unsigned long size) = 0;
    virtual EResult DecryptData(void* data, unsigned long size) = 0;

protected:
    Encrypt();
    virtual ~Encrypt();

private:
    friend void __fastcall Jet_EncryptVbaseDestructor(Encrypt* self);
};

class LZSSCompress : public Compress {
public:
    LZSSCompress();
    virtual ~LZSSCompress();
    bool CompressStream(IOSlave* dst, IOSlave* src, unsigned long srcSize, unsigned int level) override;
    bool DecompressStream(IOSlave* dst, IOSlave* src, unsigned long srcSize) override;
    virtual void Reset();  // slot 3: the original frees its work buffers here

private:
    // the original keeps its codec state inline; our codec keeps it on the stack, but the object keeps the
    // original size (0x115c) because other modules may allocate it and call the exported constructor
    unsigned char unused[0x1144];
};

class EditItemManager;

}  // namespace Jet
