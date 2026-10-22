// Jet's plugin management system (Kernel_7c), reconstructed from TS12. Behaviour spec: MODLOG.md
// "Plugin system: behaviour spec". Internal containers are our own; ordering rules of the original's hash
// tables are kept: lookups by id return the first registered entry, by-name indexes put a new entry before
// existing entries with the same name.
#include <jet/Plugin.h>
#include <jet/Task.h>

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace Jet {

// ---------------------------------------------------------------------------------------------------------------
// internal classes

class PluginGroup : public PluginType {
public:
    PluginGroup(const PString& name, const UId& id, const UId& parentId) : PluginType(name, id, parentId), refs(1) {}
    ~PluginGroup() override {}
    void __cdecl AddReference() const override { ++refs; }                 // 0x10009530
    void __cdecl ReleaseReference() const override                        // 0x10009540
    {
        if (--refs == 0) delete this;
    }

    mutable int refs;        // +0x20
    PluginTypeList members;  // +0x24
};
JET_LAYOUT32(sizeof(PluginGroup) == 0x28);

class PluginLibrary {
public:
    explicit PluginLibrary(const PString& n) : name(n) {}
    virtual ~PluginLibrary() {}
    JET_HEAP_OBJECT

    // 0x100097d0: delete this library's plugin types and forget the module handle. The original only calls
    // FreeLibrary when the name *equals* "*" (PString::Compare is strcmp-like and the test is "== 0"), so real
    // plugin DLLs are never unloaded; a later lookup loads (LoadLibrary refcount++) and registers them again.
    virtual void Unload()
    {
        HMODULE h = module;
        module = nullptr;
        PString nameCopy(name);
        while (types.head) {
            PluginType* t = types.head->data[0];
            types.Remove(types.First());
            delete t;
        }
        if (h && nameCopy.Compare("*") == 0) FreeLibrary(h);
    }

    // 0x10009920: 0 = loaded and registered, 1 = could not load, 2 = no JetGetPlugins entry
    int Load();

    PString name;          // +0x04
    int refs = 0;          // +0x08
    PluginTypeList types;  // +0x0c
    HMODULE module = nullptr;  // +0x10
    bool unused14 = false;     // +0x14
    bool failed = false;       // +0x15
};

class PluginSystem {
public:
    int refs = 1;
    std::vector<std::pair<UId, UId>> remaps;     // +0x08
    std::vector<PluginType*> typesById;          // +0x14
    std::vector<PluginType*> typesByName;        // +0x20
    std::vector<PluginConfig*> configsById;      // +0x2c
    std::vector<PluginConfig*> configsByName;    // +0x38
    std::vector<PluginLibrary*> libraries;       // +0x44

    ~PluginSystem()
    {
        for (PluginConfig* c : configsById) delete c;
        for (PluginLibrary* l : libraries) delete l;
    }

    // ---- indexes
    const UId& Remap(const UId& id) const
    {
        for (const auto& r : remaps)
            if (r.first == id) return r.second;
        return id;
    }
    PluginType* TypeById(const UId& id) const
    {
        for (PluginType* t : typesById)
            if (t->uniqueId == id) return t;
        return nullptr;
    }
    PluginConfig* ConfigById(const UId& id) const
    {
        for (PluginConfig* c : configsById)
            if (c->id == id) return c;
        return nullptr;
    }
    template <class T>
    static void InsertByName(std::vector<T*>& index, T* item, const PString& name)
    {
        auto at = std::find_if(index.begin(), index.end(), [&](T* e) { return e->name == name; });
        index.insert(at, item);
    }
    void AddConfig(PluginConfig* c)
    {
        configsById.push_back(c);
        InsertByName(configsByName, c, c->name);
    }

    // ---- 0x10009fb0 / 0x10009f20
    void Register(PluginType* t)
    {
        PluginConfig* c = ConfigById(t->uniqueId);
        t->lib = c ? c->lib : nullptr;
        if (t->lib) t->lib->types.Add(t);
        InsertByName(typesByName, t, t->name);
        typesById.push_back(t);
    }
    void Unregister(PluginType* t)
    {
        // the original removes the first entry with the same id (even if it is another type)
        for (auto i = typesById.begin(); i != typesById.end(); ++i)
            if ((*i)->uniqueId == t->uniqueId) {
                typesById.erase(i);
                break;
            }
        auto j = std::find(typesByName.begin(), typesByName.end(), t);
        if (j != typesByName.end()) typesByName.erase(j);
    }

    // ---- 0x1000b520: the group type for `id`, made from its config on first use
    int GroupFor(const UId& id, PluginGroup** out)
    {
        if (PluginType* t = TypeById(id)) {
            *out = static_cast<PluginGroup*>(t);
            t->AddReference();
            return 0;
        }
        PluginConfig* c = ConfigById(id);
        if (!c) return 1;
        *out = new PluginGroup(c->name, c->id, c->parent ? c->parent->id : UIdNull);
        return 0;
    }

    // ---- 0x1000a020: by id
    int LookupById(PluginType** out, const UId& requested)
    {
        const UId id = Remap(requested);
        if (PluginType* t = TypeById(id)) {
            if (!t->lib) return 4;
            *out = t;
            t->AddReference();
            return 0;
        }
        PluginConfig* c = ConfigById(id);
        if (c && !c->isMissing && c->lib && !c->lib->failed && !c->lib->module && c->lib->Load() == 0) {
            if (LookupById(out, id) == 0) return 0;
            c->isMissing = true;
            c->lib->Unload();
        }
        return 1;
    }

    // ---- 0x1000b290: a registered type named `name` below group `parentId` (any depth)
    int FindTypeByName(PluginType** out, const UId& parentId, const PString& name)
    {
        for (PluginType* t : typesByName) {
            if (t->name != name) continue;
            for (PluginGroup* g = t->parent; g; g = g->parent) {
                if (g->uniqueId != parentId) continue;
                const UId& id = Remap(t->uniqueId);
                if (id == t->uniqueId) {
                    if (!t->lib) return 4;
                    *out = t;
                } else {
                    if (LookupById(out, id) == 0) return 0;
                    if (!t->lib) return 4;
                    *out = t;
                }
                t->AddReference();
                return 0;
            }
        }
        return 3;
    }

    // ---- 0x1000b200: the library of a config named `name` below group `parentId`
    int FindConfigLibByName(PluginLibrary** out, const UId& parentId, const PString& name)
    {
        for (PluginConfig* c : configsByName) {
            if (c->name != name) continue;
            for (PluginConfig* p = c->parent; p; p = p->parent)
                if (p->id == parentId) {
                    *out = c->lib;
                    return 0;
                }
        }
        return 3;
    }

    // ---- 0x1000a140: by group + name, loading the library on demand
    int LookupByName(PluginType** out, const UId& parentId, const PString& name)
    {
        if (FindTypeByName(out, parentId, name) == 0) return 0;
        PluginLibrary* lib = nullptr;
        if (int r = FindConfigLibByName(&lib, parentId, name)) return r;
        if (!lib || lib->failed || lib->module) return 3;
        if (lib->Load() != 0) return 4;
        const int r = FindTypeByName(out, parentId, name);
        if (r == 0) return 0;
        lib->Unload();
        return r;
    }

    // ---- 0x1000b3d0: non-group configs below `parentId` named `name` or whose parent is named `name`
    int CollectByName(PluginConfigList& out, const UId& parentId, const PString& name)
    {
        PluginConfig* p = ConfigById(parentId);
        if (!p) return 2;
        PluginConfigList all;
        p->Enumerate(all);
        for (PluginConfigList::Iterator i = all.First(); i != all.End(); ++i) {
            PluginConfig* c = *i;
            if ((c->parent->name == name || c->name == name) && !c->isGroup) out.Add(c);
        }
        return 0;
    }

    PluginLibrary* LibraryFor(const PString& name)  // 0x1000a260
    {
        for (PluginLibrary* l : libraries)
            if (l->name == name) return l;
        PluginLibrary* l = new PluginLibrary(name);
        libraries.push_back(l);
        return l;
    }

    void LoadIni();
};

// ---------------------------------------------------------------------------------------------------------------
// globals: the system (ref-counted, built on first use) and the one lock around everything

static PluginSystem* g_system = nullptr;
static CRITICAL_SECTION g_lock;
static struct LockInit {
    LockInit() { InitializeCriticalSection(&g_lock); }
} g_lockInit;

struct Locked {
    Locked() { EnterCriticalSection(&g_lock); }
    ~Locked() { LeaveCriticalSection(&g_lock); }
};

static void SystemAcquire()  // 0x1000c460
{
    if (!g_system) {
        g_system = new PluginSystem;
        g_system->LoadIni();
    } else {
        ++g_system->refs;
    }
}

static void SystemRelease()
{
    if (g_system && --g_system->refs == 0) {
        delete g_system;
        g_system = nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// plugins.ini

// 0x1000ae1a: the OS object of the task manager
static OS* TaskManagerOS() { return GetTaskManager() ? GetTaskManager()->GetOS() : nullptr; }

// 0x1000afd0: every "<dir>/*.<ext>" module that exports PluginList contributes option lines
static void ScanPluginLists(const char* ext, const char* dir, Options& opts)
{
    std::string pattern = dir;
    pattern += *dir ? "/*." : "*.";
    pattern += ext;
    File::Find find;
    if (find.Open(pattern.c_str())) {
        do {
            DynamicLibrary lib;
            File::Path path = find.GetNameAndPath();
            if (lib.DynamicLibrary::Open(path.text)) {
                using ListFn = const char** (*)();
                ListFn list = nullptr;
                if (lib.DynamicLibrary::Import(reinterpret_cast<void**>(&list), "PluginList"))
                    for (const char** line = list(); *line; ++line) opts.ProcessRaw(*line);
                lib.DynamicLibrary::Close();
            }
        } while (find.Next());
    }
    find.Close();
}

void PluginSystem::LoadIni()  // 0x1000acd0
{
    Options opts;
    File::Path kernelIni("");
    GetModuleFileNameA(GetModuleHandleA("Kernel_7c.dll"), kernelIni.text, 0x104);
    kernelIni.DelName(false);
    kernelIni.AddPath("plugins.ini");
    if (File::Exists(kernelIni.text)) opts.ProcessFile(kernelIni.text);

    File::Path processIni("");
    Thread::GetProcessName(processIni.text);
    processIni.DelName(false);
    processIni.AddPath("plugins.ini");
    if (!processIni.IsMatch(kernelIni.text, nullptr) && File::Exists(processIni.text)) opts.ProcessFile(processIni.text);

    if (OS* os = TaskManagerOS()) {
        Options* osOpts = os->GetOptions();
        if (osOpts->Get("AutoPlugin")) {
            ScanPluginLists("dll", "", opts);
            osOpts->SetSeparators("PluginDir", ",");
            String dir;
            while (osOpts->GetNext("PluginDir", dir)) ScanPluginLists("dll", dir, opts);
        }
    }

    // groups: Name:GUID[:ParentGUID]
    {
        opts.SetSeparators("PluginGroup", ",");
        String line;
        while (opts.GetNext("PluginGroup", line)) {
            PString name(static_cast<const char*>(line.Token(":")));
            const UId id(static_cast<const char*>(line.Token(":")));
            PluginConfig* parent = nullptr;
            if (std::strlen(line) != 0) {
                const UId parentId(static_cast<const char*>(line.Token(":")));
                parent = ConfigById(parentId);
                if (!parent) continue;
            }
            PluginConfig* c = new PluginConfig(name, id, nullptr, parent, true);
            if (parent) parent->children.Add(c);
            AddConfig(c);
        }
    }
    // plugins: Dll:Name:GUID[:ParentGUID]
    {
        opts.SetSeparators("Plugin", ",");
        String line;
        PluginLibrary* lib = nullptr;
        while (opts.GetNext("Plugin", line)) {
            PString dll(static_cast<const char*>(line.Token(":")));
            PString name(static_cast<const char*>(line.Token(":")));
            const UId id(static_cast<const char*>(line.Token(":")));
            if (!lib || lib->name != dll) lib = LibraryFor(dll);
            PluginConfig* parent = nullptr;
            if (std::strlen(line) != 0) {
                const UId parentId(static_cast<const char*>(line.Token(":")));
                parent = ConfigById(parentId);
                if (!parent) continue;
            }
            PluginConfig* c = new PluginConfig(name, id, lib, parent, false);
            if (parent) parent->children.Add(c);
            AddConfig(c);
        }
    }
    // remaps: FromGUID:ToGUID
    {
        opts.SetSeparators("PluginRemap", ",");
        String line;
        while (opts.GetNext("PluginRemap", line)) {
            const UId from(static_cast<const char*>(line.Token(":")));
            const UId to(static_cast<const char*>(line.Token(":")));
            remaps.emplace_back(from, to);
        }
    }
}

int PluginLibrary::Load()
{
    if (failed) return 1;
    File::Path path(name.Text());
    const PString star("*");
    if (name == star) {
        module = GetModuleHandleA(nullptr);
    } else {
        if (path.IsExt("exe")) {
            path.SetExt(nullptr);
            path.Add("_7c.exe");
        } else {
            if (path.IsExt("dll")) path.SetExt(nullptr);
            path.Add("_7c.dll");
        }
        module = LoadLibraryA(path.text);
    }
    if (!module) {
        failed = true;
        return 1;
    }
    String entry("JetGetPlugins");
    if (name.node && name.Length() != 0 && name != star) {
        File::Path original(name.Text());
        entry.Addf("%s", original.GetFileName());
    }
    FARPROC proc = GetProcAddress(module, entry);
    if (!proc) {
        FreeLibrary(module);
        failed = true;
        return 2;
    }
    reinterpret_cast<void(__cdecl*)()>(proc)();
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// PluginType

PluginType::PluginType() : lib(nullptr), parent(nullptr) {}

PluginType::PluginType(const PString& n, const UId& id, const UId& parentId)  // 0x1000c4f0
    : uniqueId(id), name(n)
{
    SystemAcquire();
    lib = nullptr;
    parent = nullptr;
    if (parentId) {
        PluginGroup* g = nullptr;
        if (g_system->GroupFor(parentId, &g) != 0 || !g) return;  // not registered (as in the original)
        parent = g;
        g->members.Add(this);
    }
    g_system->Register(this);
}

PluginType::PluginType(const PluginType& o)
    : uniqueId(o.uniqueId), name(o.name), lib(o.lib), parent(o.parent) {}

PluginType& PluginType::operator=(const PluginType& o)
{
    uniqueId = o.uniqueId;
    name = o.name;
    lib = o.lib;
    parent = o.parent;
    return *this;
}

PluginType::~PluginType()  // 0x1000c600
{
    Locked lock;
    if (parent) {
        PluginTypeList::Iterator i = parent->members.Find(this);
        parent->members.Remove(i);
        parent->ReleaseReference();
    }
    g_system->Unregister(this);
    SystemRelease();
}

bool __cdecl PluginType::operator==(const PString& n) const { return name == n; }
bool __cdecl PluginType::operator==(const PluginType& o) const { return uniqueId == o.uniqueId; }
const UId& __cdecl PluginType::GetId() const { return uniqueId; }
const UId& __cdecl PluginType::GetParentId() const { return parent->uniqueId; }
const PString& __cdecl PluginType::GetName() const { return name; }
const PluginGroup* __cdecl PluginType::GetParent() const { return parent; }
PluginLibrary* __cdecl PluginType::GetLibrary() { return lib; }
void __cdecl PluginType::SetLibrary(PluginLibrary* l) { lib = l; }

void __cdecl PluginType::AddReference() const  // 0x1000c700
{
    Locked lock;
    ++lib->refs;
}

void __cdecl PluginType::ReleaseReference() const  // 0x1000c730
{
    Locked lock;
    if (--lib->refs == 0) lib->Unload();
}

// ---------------------------------------------------------------------------------------------------------------
// PluginConfig

PluginConfig::PluginConfig(const PString& n, const UId& i, PluginLibrary* l, PluginConfig* p, bool group)
    : name(n), id(i), parent(p), lib(l), type(nullptr), isGroup(group), isMissing(false) {}

PluginConfig::PluginConfig(const PluginConfig& o)
    : name(o.name), id(o.id), parent(o.parent), lib(o.lib), type(o.type), children(o.children),
      isGroup(o.isGroup), isMissing(o.isMissing) {}

PluginConfig::~PluginConfig() {}

PluginConfig& PluginConfig::operator=(const PluginConfig& o)
{
    name = o.name;
    id = o.id;
    parent = o.parent;
    lib = o.lib;
    type = o.type;
    children = o.children;
    isGroup = o.isGroup;
    isMissing = o.isMissing;
    return *this;
}

bool __cdecl PluginConfig::operator==(const UId& other) const { return id == other; }
const PString& __cdecl PluginConfig::GetName() const { return name; }
const UId& __cdecl PluginConfig::GetId() const { return id; }
const UId& __cdecl PluginConfig::GetParentId() const { return parent->id; }
const PluginConfig* __cdecl PluginConfig::GetParent() const { return parent; }
bool __cdecl PluginConfig::HasChildren() const { return children.head != nullptr; }
bool __cdecl PluginConfig::IsGroup() const { return isGroup; }
bool __cdecl PluginConfig::IsMissing() const { return isMissing; }
void __cdecl PluginConfig::SetMissing(bool m) { isMissing = m; }
void __cdecl PluginConfig::Add(PluginConfig* child) { children.Add(child); }
PluginLibrary* __cdecl PluginConfig::GetLibrary() const { return lib; }

void __cdecl PluginConfig::Enumerate(PluginConfigList& list) const  // 0x100093c0
{
    for (PluginConfigList::ConstIterator i = children.First(); i != children.End(); ++i) {
        if (!(*i)->isGroup)
            list.Add(*i);
        else
            (*i)->Enumerate(list);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// PluginBase, PluginCollection

PluginBase& PluginBase::operator=(const PluginBase&) { return *this; }

Result __cdecl PluginBase::LookupPluginType(PluginType** plugin, const UId& id)  // 0x10007a40
{
    Locked lock;
    SystemAcquire();
    const int r = g_system->LookupById(plugin, id);
    SystemRelease();
    return Result(r);
}

Result __cdecl PluginBase::LookupPluginType(PluginType** plugin, const UId& parentId, const PString& name)
{
    Locked lock;
    SystemAcquire();
    const int r = g_system->LookupByName(plugin, parentId, name);
    SystemRelease();
    return Result(r);
}

// 0x10007b20: the original takes no lock and no reference here
Result __cdecl PluginBase::LookupPluginType(PluginConfigList& list, const UId& parentId)
{
    if (!g_system) return Result(2);
    PluginConfig* c = g_system->ConfigById(parentId);
    if (!c) return Result(2);
    c->Enumerate(list);
    return Result(0);
}

Result __cdecl PluginBase::LookupPluginType(PluginConfigList& list, const UId& parentId, const PString& name)
{
    if (!g_system) return Result(2);
    return Result(g_system->CollectByName(list, parentId, name));
}

UId __cdecl PluginBase::GetRemap(const UId& id)  // 0x10007c70
{
    Locked lock;
    SystemAcquire();
    const UId r = g_system->Remap(id);
    SystemRelease();
    return r;
}

bool __cdecl PluginBase::IsRemapped(const UId& id)  // 0x10007ba0
{
    Locked lock;
    SystemAcquire();
    const UId r = g_system->Remap(id);
    SystemRelease();
    return r != id;
}

PluginCollection::PluginCollection(const UId& id)  // 0x10007e40
{
    Locked lock;
    SystemAcquire();
    if (PluginConfig* c = g_system->ConfigById(id)) c->Enumerate(*this);
}

PluginCollection::PluginCollection(const UId& id, const PString& name)  // 0x10007ee0
{
    Locked lock;
    SystemAcquire();
    g_system->CollectByName(*this, id, name);
}

// 0x10007e00: copies without taking a system reference, while the destructor releases one (as the original)
PluginCollection::PluginCollection(const PluginCollection& o) : PluginConfigList(o) {}

PluginCollection::~PluginCollection()  // 0x10007f60
{
    Locked lock;
    SystemRelease();
}

PluginCollection& PluginCollection::operator=(const PluginCollection& o)
{
    PluginConfigList::operator=(o);
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------
// PluginInstance

PluginInstance::PluginInstance() : pluginFactory(nullptr) {}
PluginInstance::~PluginInstance() {}
const UId& __cdecl PluginInstance::GetID() const { return pluginFactory->GetId(); }
const PString& __cdecl PluginInstance::GetName() const { return pluginFactory->GetName(); }

void __cdecl PluginInstance::Assign(const PluginFactory* factory)  // 0x10001c40
{
    const PluginFactory* old = pluginFactory;
    pluginFactory = factory;
    if (pluginFactory) pluginFactory->AddReference();
    if (old) old->ReleaseReference();
}

void __cdecl PluginInstance::DeleteThis() const  // 0x10009440
{
    const PluginFactory* factory = pluginFactory;
    delete this;
    if (factory) factory->ReleaseReference();
}

}  // namespace Jet
