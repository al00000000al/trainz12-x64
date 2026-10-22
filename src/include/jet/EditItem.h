// Jet::EditItem, Jet::EditItemManager, Jet::EditItemStack (Kernel_7c): an undo/redo framework for editors.
// A manager (a plugin of group EditItemManager::uid) makes items (plugins too) by type name, hands them out
// through ItemHandles, keeps submitted items on an undo stack and undone ones on a redo stack, and can page
// items to temporary files. TS12 ships no module that uses it (TerrainEditSupport is listed in plugins.ini but
// not installed), but the exports and layouts are kept exactly.
//
// Vtable slots that are not exported have names of our own (marked "slot n"); the original's bodies of these
// are all "return 0".
#pragma once

#include <jet/DynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/Plugin.h>
#include <jet/PString.h>
#include <jet/UId.h>

namespace Jet {

class EditItem;
class EditItemManager;
class EditItemStack;

using PStringList = DynamicArray<PString, 13, Less<PString, PString>>;

// the item a manager handed out (GetItem); a plain pointer, returned in memory
class ItemHandle {
public:
    ItemHandle(EditItem* p = nullptr) : item(p) {}
    EditItem* item;
};

class EditItem : public PluginInstance {
public:
    struct FullCap {
        FullCap(const PString& c, const PString& v) : cap(c), value(v) {}
        PString cap;
        PString value;
    };

    EditItem(PString type);
    virtual ~EditItem();

    virtual bool __cdecl Init(const EditItemManager* manager, const PString& fileName);  // slot 0: keeps the manager
    virtual bool __cdecl Cancel();  // slot 1: not called by the kernel
    virtual bool __cdecl Apply();   // slot 2: SubmitItem; true = done, the handle is released
    virtual bool __cdecl Redo();    // slot 3: pushed onto the undo stack again (Redo)
    virtual bool __cdecl Undo();    // slot 4: popped from the undo stack (Undo, BinItem)
    virtual bool __cdecl Save();    // slot 5: page out (base: only flips the paged flag)
    virtual bool __cdecl Load();    // slot 6: page in
    virtual bool __cdecl QueryCaps(const PString& cap) const;
    virtual const PStringList& __cdecl GetCapsList() const;
    virtual const Array<FullCap, 13>& __cdecl GetFullCapsList() const;

    const PString& __cdecl GetType() const;
    const EditItemManager* __cdecl GetEditItemManager() const;
    bool __cdecl NeedsLoad() const;  // paged out

protected:
    void __cdecl AddCapability(const PString& cap);
    void __cdecl AddFullCap(const PString& cap, const PString& value);

private:
    friend class EditItemStack;
    const EditItemManager* manager;  // +0x0c
    PString type;                    // +0x10
    PStringList caps;                // +0x14
    Array<FullCap, 13> fullCaps;     // +0x18
    bool paged;                      // +0x24
};

class EditItemStack {
public:
    EditItemStack(bool undoStack);
    virtual ~EditItemStack();
    bool __cdecl PushItem(EditItem* item, bool redo);
    bool __cdecl PopItem(EditItem** item);  // null: the popped item is released
    void __cdecl CollapseItemStack();
    bool __cdecl PageToDisk();
    unsigned int __cdecl GetNumItems() const;
    bool __cdecl IsEmpty() const;
    JET_HEAP_OBJECT

private:
    friend class EditItemManager;
    FastDynamicArray<EditItem*, 15> items;  // +0x04, top of the stack last
    Mutex mutex;                            // +0x08
    bool undoStack;                         // +0x10: page items in and call Undo when popped
    unsigned int count;                     // +0x14
};

class EditItemManager : public PluginInstance {
public:
    static EditItemManager* __cdecl CreateInstance(const PString& name);

    EditItemManager();
    virtual ~EditItemManager();

    virtual ItemHandle __cdecl GetItem(const PString& type) const;
    virtual bool __cdecl SubmitItem(ItemHandle& handle);
    virtual bool __cdecl BinItem(ItemHandle& handle);
    virtual bool __cdecl QueryItem(const PString& type) const;
    virtual bool __cdecl QueryItemCaps(const PString& type, const PString& cap) const;
    virtual const PStringList& __cdecl GetItemList() const;
    virtual PStringList __cdecl GetItemCapsList(const PString& type) const;
    virtual bool __cdecl Undo();
    virtual bool __cdecl Redo();
    virtual bool __cdecl Initialize(const PString& name);

    bool __cdecl AreItemsOnItemList() const;  // undo stack not empty
    bool __cdecl AreItemsOnRedoList() const;
    void __cdecl CleanupAllTempDataOnDisk() const;
    static void __cdecl CleanupThisTempDataOnDisk();
    bool __cdecl PageToDisk();
    const PString& __cdecl GetName() const;

protected:
    virtual const UId* __cdecl GetItemGroup(const PString& type) const;  // slot 10: plugin group of the item types
    virtual EditItem* __cdecl CreateItem(const PString& type) const;
    virtual void __cdecl GetSystemLevelIdentifier(PString& id) const;
    virtual void __cdecl GetUniqueFileName(PString& fileName) const;
    virtual bool __cdecl GetBaseTempPath(PString& path) const;  // slot 14: the application's temp directory

    void __cdecl GetApplicationUniqueTempPath(PString& path) const;
    static const PString& __cdecl GetUUID();

    static unsigned int tempFileCounter;
    static PString tempFileDir;
    static Mutex tempFileMutex;
    static const UId uid;

private:
    PStringList itemList;      // +0x0c
    EditItem* current;         // +0x10: handed out, not yet submitted or binned
    EditItemStack* undoItems;  // +0x14
    EditItemStack* redoItems;  // +0x18
    Mutex* mutex;              // +0x1c: named after GetSystemLevelIdentifier (left unset until Initialize)
    PString name;              // +0x20
};

}  // namespace Jet
