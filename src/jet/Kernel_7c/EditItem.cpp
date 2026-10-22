// Jet::EditItem, Jet::EditItemStack and Jet::EditItemManager (Kernel_7c 0x10002c80-0x10004a20, 0x10007a10).
// Behaviour follows the original's assembly; addresses are given per function. EditItemManager::CreateInstance
// lives with the other plugin lookups in CompressEncrypt.cpp.
#include <jet/EditItem.h>

#include <cstdlib>
#include <new>

namespace Jet {

JET_LAYOUT32(sizeof(EditItem) == 0x34);
JET_LAYOUT32(sizeof(EditItemManager) == 0x30);
JET_LAYOUT32(sizeof(EditItemStack) == 0x18);
JET_LAYOUT32(sizeof(EditItem::FullCap) == 8);

namespace {

// release through the ReferenceCount base (the object may be deleted)
void Release(const EditItem* item) { static_cast<const ReferenceCount*>(item)->ReleaseReference(); }

// s += text, as the original inlines it: PStringCache::Ins at the end (also on a null string)
void AppendText(PString& s, const char* text)
{
    s.node = PString::Cache()->Ins(s.node, reinterpret_cast<const unsigned char*>(text), s.Length());
}

// s += t (0x100033b0): Ins when s has text, otherwise s takes t's node
void Append(PString& s, const PString& t)
{
    if (s.node)
        s.node = PString::Cache()->Ins(s.node, reinterpret_cast<const unsigned char*>(t.Text()), s.Length());
    else
        s = t;
}

struct Lookup : PluginBase {
    using PluginBase::LookupPluginType;
};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// EditItem

EditItem::EditItem(PString t) : manager(nullptr), type(t), paged(false) {}  // 0x10002ed0
EditItem::~EditItem() {}                                                    // 0x10002ff0

bool __cdecl EditItem::Init(const EditItemManager* m, const PString&)  // 0x100030c0
{
    manager = m;
    return true;
}

bool __cdecl EditItem::Cancel() { return false; }
bool __cdecl EditItem::Apply() { return false; }
bool __cdecl EditItem::Redo() { return false; }
bool __cdecl EditItem::Undo() { return false; }

bool __cdecl EditItem::Save()  // 0x100030d0
{
    if (paged) return false;
    paged = true;
    return true;
}

bool __cdecl EditItem::Load()  // 0x100030f0
{
    if (!paged) return false;
    paged = false;
    return true;
}

bool __cdecl EditItem::QueryCaps(const PString& cap) const { return caps.Find(cap).node != nullptr; }  // 0x10002cc0
const PStringList& __cdecl EditItem::GetCapsList() const { return caps; }
const Array<EditItem::FullCap, 13>& __cdecl EditItem::GetFullCapsList() const { return fullCaps; }
const PString& __cdecl EditItem::GetType() const { return type; }
const EditItemManager* __cdecl EditItem::GetEditItemManager() const { return manager; }
bool __cdecl EditItem::NeedsLoad() const { return paged; }  // 0x10007a10

void __cdecl EditItem::AddCapability(const PString& cap) { caps.Add(cap); }  // 0x10002d00

void __cdecl EditItem::AddFullCap(const PString& cap, const PString& value)  // 0x10002d20
{
    const FullCap entry(cap, value);
    fullCaps.Reserve(fullCaps.count + 1);
    new (&fullCaps.data[fullCaps.count]) FullCap(entry);
    ++fullCaps.count;
}

// ---------------------------------------------------------------------------------------------------------------
// EditItemStack

EditItemStack::EditItemStack(bool undo) : mutex(false, nullptr), undoStack(undo), count(0) {}  // 0x10004510

EditItemStack::~EditItemStack()  // 0x10004580: releases the items, then the mutex, then the chunks
{
    for (auto i = items.First(); i != items.End(); ++i) Release(*i);
}

bool __cdecl EditItemStack::PushItem(EditItem* item, bool redo)  // 0x100046f0
{
    SingleLock lock(&mutex, true);
    static_cast<const ReferenceCount*>(item)->AddReference();
    items.Add(item);
    ++count;
    if (redo) {
        if (item->paged && !item->Load()) return false;
        if (!item->Redo()) return false;
    }
    return true;
}

// 0x100047d0: takes the top item. The undo stack pages it in and undoes it; when that fails the item stays
// popped (and, without `out`, unreleased), as in the original.
bool __cdecl EditItemStack::PopItem(EditItem** out)
{
    SingleLock lock(&mutex, true);
    const bool keep = out != nullptr;
    EditItem* local;
    if (!keep) out = &local;
    *out = nullptr;
    if (!items.head) return false;
    *out = items.Remove();
    --count;
    if (undoStack) {
        EditItem* item = *out;
        if (item->paged && !item->Load()) return false;
        (*out)->Undo();
    }
    if (!keep) Release(*out);
    return true;
}

void __cdecl EditItemStack::CollapseItemStack()  // 0x10004660 (no lock)
{
    for (auto i = items.First(); i != items.End(); ++i) Release(*i);
    items.RemoveAll();
    count = 0;
}

bool __cdecl EditItemStack::PageToDisk()  // 0x10004910 (no lock): saves every item that is not paged out yet
{
    bool ok = true;
    for (auto i = items.First(); i != items.End(); ++i)
        if (!(*i)->paged) ok &= (*i)->Save();
    return ok;
}

unsigned int __cdecl EditItemStack::GetNumItems() const { return count; }
bool __cdecl EditItemStack::IsEmpty() const { return count == 0; }

// ---------------------------------------------------------------------------------------------------------------
// EditItemManager

unsigned int EditItemManager::tempFileCounter = 0;
PString EditItemManager::tempFileDir;
Mutex EditItemManager::tempFileMutex(false, nullptr);
const UId EditItemManager::uid("91799A9D-353C-4e22-A3C5-BCD894D2303A");

// 0x100034e0. The mutex pointer stays unset until Initialize (the destructor uses it regardless).
EditItemManager::EditItemManager()
    : current(nullptr), undoItems(new EditItemStack(true)), redoItems(new EditItemStack(false))
{
    std::atexit(CleanupThisTempDataOnDisk);
}

EditItemManager::~EditItemManager()  // 0x10003600: the current item is not released
{
    delete undoItems;
    undoItems = nullptr;
    delete redoItems;
    redoItems = nullptr;
    if (mutex->IsLocked()) mutex->Unlock();
    delete mutex;
    mutex = nullptr;
}

// 0x10003810: makes the item, puts it on the undo stack and hands it out; one at a time
ItemHandle __cdecl EditItemManager::GetItem(const PString& type) const
{
    if (current) return ItemHandle(nullptr);
    EditItemManager* self = const_cast<EditItemManager*>(this);
    EditItem* item = CreateItem(type);
    EditItemStack* undo = undoItems;
    self->current = item;
    {
        SingleLock lock(&undo->mutex, true);
        static_cast<const ReferenceCount*>(item)->AddReference();
        undo->items.Add(item);
        ++undo->count;
    }
    return ItemHandle(current);
}

bool __cdecl EditItemManager::SubmitItem(ItemHandle& handle)  // 0x100038e0
{
    if (handle.item != current) {
        handle.item = nullptr;
        return false;
    }
    current = nullptr;
    if (!handle.item->Apply()) return false;
    Release(handle.item);
    handle.item = nullptr;
    return true;
}

// 0x10003930: takes the item off the undo stack again (the stack's reference is not dropped)
bool __cdecl EditItemManager::BinItem(ItemHandle& handle)
{
    if (handle.item != current) {
        handle.item = nullptr;
        return false;
    }
    EditItem* popped;
    if (!undoItems->PopItem(&popped)) return false;
    Release(handle.item);
    current = nullptr;
    handle.item = nullptr;
    return true;
}

bool __cdecl EditItemManager::QueryItem(const PString& type) const { return itemList.Find(type).node != nullptr; }

bool __cdecl EditItemManager::QueryItemCaps(const PString& type, const PString& cap) const  // 0x100039c0
{
    bool result = false;
    if (EditItem* item = CreateItem(type)) {
        result = item->QueryCaps(cap);
        Release(item);
    }
    return result;
}

const PStringList& __cdecl EditItemManager::GetItemList() const { return itemList; }

PStringList __cdecl EditItemManager::GetItemCapsList(const PString& type) const  // 0x10003a10
{
    PStringList list;
    if (EditItem* item = CreateItem(type)) {
        const PStringList& caps = item->GetCapsList();
        list.RemoveAll();
        list.AppendChunks(caps);
        Release(item);
    }
    return list;
}

bool __cdecl EditItemManager::Undo()  // 0x10003ab0
{
    if (current || !undoItems->count) return false;
    EditItem* item;
    if (!undoItems->PopItem(&item) || !item) return false;
    const bool r = redoItems->PushItem(item, false);
    Release(item);
    return r;
}

bool __cdecl EditItemManager::Redo()  // 0x10003b20
{
    if (current || !redoItems->count) return false;
    EditItem* item;
    if (!redoItems->PopItem(&item) || !item) return false;
    const bool r = undoItems->PushItem(item, true);
    Release(item);
    return r;
}

// 0x10004050: takes the name, opens the system-wide mutex named after GetSystemLevelIdentifier and, when this
// process gets it, cleans the application's temp directory. Always true.
bool __cdecl EditItemManager::Initialize(const PString& n)
{
    name = n;
    PString id;
    GetSystemLevelIdentifier(id);
    char text[0x100];
    if (PString::Cache()->GetANSI(id.node, text, sizeof text) != -1) {
        mutex = new Mutex(true, text);
        if (mutex->Lock(0)) CleanupAllTempDataOnDisk();
    }
    return true;
}

bool __cdecl EditItemManager::AreItemsOnItemList() const { return undoItems->count != 0; }
bool __cdecl EditItemManager::AreItemsOnRedoList() const { return redoItems->count != 0; }

void __cdecl EditItemManager::CleanupAllTempDataOnDisk() const  // 0x10003eb0
{
    PString path;
    GetBaseTempPath(path);
    File::Prune(path.Text());
}

void __cdecl EditItemManager::CleanupThisTempDataOnDisk()  // 0x10003f50
{
    if (tempFileDir.node && tempFileDir.Length()) File::Prune(tempFileDir.Text());
}

bool __cdecl EditItemManager::PageToDisk()  // 0x10003f70: both stacks, redo first
{
    const bool redo = redoItems->PageToDisk();
    const bool undo = undoItems->PageToDisk();
    return undo & redo;
}

const PString& __cdecl EditItemManager::GetName() const { return name; }

const UId* __cdecl EditItemManager::GetItemGroup(const PString&) const { return nullptr; }
bool __cdecl EditItemManager::GetBaseTempPath(PString&) const { return false; }

// 0x100036f0: the factory of `type` in GetItemGroup's group makes the item, Init gives it a unique file name
EditItem* __cdecl EditItemManager::CreateItem(const PString& type) const
{
    const UId& group = *GetItemGroup(type);
    PluginType* factory = nullptr;
    {
        PluginType* found = nullptr;
        if (Lookup::LookupPluginType(&found, group, type).code == 0) factory = found;
    }
    if (!factory) return nullptr;
    EditItem* item = static_cast<PluginFactoryBase<EditItem>*>(factory)->CreateInstance();
    PString fileName;
    GetUniqueFileName(fileName);
    if (item && !item->Init(this, fileName)) {
        Release(item);
        item = nullptr;
    }
    factory->ReleaseReference();
    return item;
}

void __cdecl EditItemManager::GetSystemLevelIdentifier(PString& id) const  // 0x10002e80: "Jet-" + GetUUID()
{
    if (id.node) PString::Cache()->Destroy(id.node);
    id.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>("Jet-"));
    Append(id, GetUUID());
}

void __cdecl EditItemManager::GetUniqueFileName(PString& fileName) const  // 0x10003bb0: <temp dir>/Temp_<n>.bin
{
    tempFileMutex.Lock(0xffffffff);
    GetApplicationUniqueTempPath(fileName);
    AppendText(fileName, "/");
    const unsigned int n = tempFileCounter++;
    fileName.Addf("Temp_%d.bin", n);
    tempFileMutex.Unlock();
}

// 0x10003c30: GetBaseTempPath + "/" + GetUUID(), made on first use and kept in tempFileDir
void __cdecl EditItemManager::GetApplicationUniqueTempPath(PString& path) const
{
    if (tempFileDir.node && tempFileDir.Length()) {
        path = tempFileDir;
        return;
    }
    GetBaseTempPath(path);
    AppendText(path, "/");
    Append(path, GetUUID());
    if (!File::Exists(path.Text())) File::Md(path.Text());
    tempFileDir = path;
}

// 0x10003d70: a random UId as text without the dashes, made once per process
const PString& __cdecl EditItemManager::GetUUID()
{
    static PString uuid;
    if (!uuid.node || !uuid.Length()) {
        UId id;
        id.SetRnd();
        uuid = id.GetString();
        uuid.node = PString::Cache()->Strip(uuid.node, reinterpret_cast<const unsigned char*>("-"));
    }
    return uuid;
}

}  // namespace Jet
