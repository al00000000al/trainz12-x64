// Walks Kernel_7c's EditItem block: items (caps, full caps, paging flags), item stacks (push / pop / collapse /
// page out, chunk boundaries, reference counts) and managers (made by the plugin system, Initialize with its
// named mutex and temp clean-up, item lists, GetItem / SubmitItem / BinItem, undo and redo, unique temp file
// names, paging to disk). Linked against "Kernel_7c.dll" by name: run once next to the original module and once
// next to ours; the logs must be identical. The per-process random UUID is printed as <uuid>.
//
// usage: edititem_probe.exe <out.txt>   (run from the directory holding the modules and plugins.ini)
#include <jet/EditItem.h>

#include "StaticKernel.h"

#include <windows.h>

#include <cstdio>
#include <string>

using namespace Jet;

static FILE* out;
static std::string uuid;  // GetUUID() as text, once known

static std::string Str(const PString& s)
{
    std::string t = s.Text() ? s.Text() : "(null)";
    if (!uuid.empty())
        for (size_t at; (at = t.find(uuid)) != std::string::npos;) t.replace(at, uuid.size(), "<uuid>");
    return t;
}

static int Refs(const EditItem* i) { return static_cast<const ReferenceCount*>(i)->GetReferenceCount(); }
static bool Release(const ReferenceCount* r) { return r->ReleaseReference(); }

static void DumpList(const char* what, const PStringList& list)
{
    std::fprintf(out, "  %s count=%lu chunks:", what, list.Count());
    if (auto* c = list.head) do {
            std::fprintf(out, " %u", c->count);
            c = c->next;
        } while (c != list.head);
    std::fprintf(out, " items:");
    for (auto i = list.First(); i != list.End(); ++i) std::fprintf(out, " %s", Str(*i).c_str());
    std::fprintf(out, "\n");
}

static bool DirExists(const char* path)
{
    const DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static void MakeTree(const char* dir)
{
    CreateDirectoryA(dir, nullptr);
    const std::string sub = std::string(dir) + "\\sub";
    CreateDirectoryA(sub.c_str(), nullptr);
    if (FILE* f = std::fopen((sub + "\\file.txt").c_str(), "w")) {
        std::fputs("x", f);
        std::fclose(f);
    }
}

// ---------------------------------------------------------------------------------------------------------------

static int itemCounter = 0;

class ProbeItem : public EditItem {
public:
    ProbeItem() : EditItem(PString("ProbeItem")), label("i" + std::to_string(++itemCounter))
    {
        AddCapability(PString("move"));
        AddCapability(PString("rotate"));
        AddFullCap(PString("move"), PString("xy"));
        std::fprintf(out, "  new %s\n", label.c_str());
    }
    ~ProbeItem() override { std::fprintf(out, "  ~%s\n", label.c_str()); }

    using EditItem::AddCapability;
    using EditItem::AddFullCap;

    std::string label;
    bool applyResult = true, redoResult = true, undoResult = true, saveFail = false, loadFail = false, initResult = true;

    bool __cdecl Init(const EditItemManager* m, const PString& fileName) override
    {
        const bool r = EditItem::Init(m, fileName);
        std::fprintf(out, "  %s Init(%s) base=%d -> %d manager=%d\n", label.c_str(), Str(fileName).c_str(), r, initResult,
                     GetEditItemManager() == m);
        return initResult;
    }
    bool __cdecl Cancel() override { return Log("Cancel", false); }
    bool __cdecl Apply() override { return Log("Apply", applyResult); }
    bool __cdecl Redo() override { return Log("Redo", redoResult); }
    bool __cdecl Undo() override { return Log("Undo", undoResult); }
    bool __cdecl Save() override
    {
        const bool r = saveFail ? false : EditItem::Save();
        return Log("Save", r);
    }
    bool __cdecl Load() override
    {
        const bool r = loadFail ? false : EditItem::Load();
        return Log("Load", r);
    }

private:
    bool Log(const char* what, bool r)
    {
        std::fprintf(out, "  %s %s -> %d (paged=%d refs=%d)\n", label.c_str(), what, r, NeedsLoad(), Refs(this));
        return r;
    }
};

static const char* kGroup = "91799A9D-353C-4e22-A3C5-BCD894D2303A";  // EditItemManager::uid

class ProbeManager : public EditItemManager {
public:
    ProbeManager() { std::fprintf(out, "  new manager\n"); }
    ~ProbeManager() override { std::fprintf(out, "  ~manager\n"); }

    std::string base = "edtmp";
    std::string fixedId;  // when set, GetSystemLevelIdentifier gives this instead of "Jet-<uuid>"
    UId group{kGroup};

    using EditItemManager::CreateItem;
    using EditItemManager::GetApplicationUniqueTempPath;
    using EditItemManager::GetUniqueFileName;
    using EditItemManager::GetUUID;

    void BaseSystemId(PString& id) const { EditItemManager::GetSystemLevelIdentifier(id); }

protected:
    const UId* __cdecl GetItemGroup(const PString& type) const override
    {
        std::fprintf(out, "  GetItemGroup(%s)\n", Str(type).c_str());
        return &group;
    }
    bool __cdecl GetBaseTempPath(PString& path) const override
    {
        std::fprintf(out, "  GetBaseTempPath(in=%s)\n", Str(path).c_str());
        path = PString(base.c_str());
        return true;
    }
    void __cdecl GetSystemLevelIdentifier(PString& id) const override
    {
        if (fixedId.empty())
            EditItemManager::GetSystemLevelIdentifier(id);
        else
            id = PString(fixedId.c_str());
        if (uuid.empty() && GetUUID().Text()) uuid = GetUUID().Text();  // called inside Initialize, before main knows it
        std::fprintf(out, "  GetSystemLevelIdentifier -> %s\n", Str(id).c_str());
    }
};

// ---------------------------------------------------------------------------------------------------------------

static void Items()
{
    std::fprintf(out, "== items\n");
    ProbeItem* a = new ProbeItem;
    std::fprintf(out, "  type=%s manager=%d needsload=%d refs=%d\n", Str(a->GetType()).c_str(),
                 a->GetEditItemManager() != nullptr, a->NeedsLoad(), Refs(a));
    std::fprintf(out, "  caps move=%d rotate=%d scale=%d null=%d\n", a->QueryCaps(PString("move")),
                 a->QueryCaps(PString("rotate")), a->QueryCaps(PString("scale")), a->QueryCaps(PString()));
    for (int i = 0; i < 25; ++i) a->AddCapability(PString(("cap" + std::to_string(i)).c_str()));
    a->AddCapability(PString("move"));  // duplicates are kept
    a->AddCapability(PString());
    DumpList("caps", a->GetCapsList());
    std::fprintf(out, "  caps cap24=%d null=%d\n", a->QueryCaps(PString("cap24")), a->QueryCaps(PString()));

    const Array<EditItem::FullCap, 13>& full = a->GetFullCapsList();
    std::fprintf(out, "  full count=%lu capacity=%lu\n", full.Count(), full.Capacity());
    for (int i = 0; i < 40; ++i) {
        a->AddFullCap(PString(("k" + std::to_string(i)).c_str()), PString(("v" + std::to_string(i * 3)).c_str()));
        if (i % 6 == 0 || i > 36) std::fprintf(out, "  full +%d count=%lu capacity=%lu\n", i, full.Count(), full.Capacity());
    }
    a->AddFullCap(PString(), PString("nokey"));
    std::fprintf(out, "  full:");
    for (unsigned long i = 0; i < full.Count(); ++i)
        std::fprintf(out, " %s=%s", Str(full[i].cap).c_str(), Str(full[i].value).c_str());
    std::fprintf(out, "\n");

    std::fprintf(out, "  base save=%d save=%d needsload=%d load=%d load=%d needsload=%d\n", a->EditItem::Save(),
                 a->EditItem::Save(), a->NeedsLoad(), a->EditItem::Load(), a->EditItem::Load(), a->NeedsLoad());
    EditItem* plain = new EditItem(PString("Plain"));
    std::fprintf(out, "  plain type=%s cancel=%d apply=%d redo=%d undo=%d\n", Str(plain->GetType()).c_str(),
                 plain->Cancel(), plain->Apply(), plain->Redo(), plain->Undo());
    std::fprintf(out, "  plain init=%d manager=%d caps=%lu full=%lu\n",
                 plain->Init(reinterpret_cast<EditItemManager*>(0x1234), PString("f")),
                 plain->GetEditItemManager() == reinterpret_cast<EditItemManager*>(0x1234), plain->GetCapsList().Count(),
                 plain->GetFullCapsList().Count());
    std::fprintf(out, "  plain release=%d\n", Release(plain));
    a->AddReference();
    {
        const int r1 = Refs(a);
        const bool rel = Release(a);
        std::fprintf(out, "  addref refs=%d release=%d refs=%d\n", r1, rel, Refs(a));
    }
    std::fprintf(out, "  release=%d\n", Release(a));
}

static void Stacks()
{
    std::fprintf(out, "== stacks\n");
    EditItemStack* undo = new EditItemStack(true);
    EditItemStack* redo = new EditItemStack(false);
    auto state = [&](const char* what) {
        std::fprintf(out, "  [%s] undo n=%u empty=%d redo n=%u empty=%d\n", what, undo->GetNumItems(), undo->IsEmpty(),
                     redo->GetNumItems(), redo->IsEmpty());
    };
    state("new");
    EditItem* got = reinterpret_cast<EditItem*>(1);
    bool r = undo->PopItem(&got);
    std::fprintf(out, "  pop empty -> %d got=%d\n", r, got != nullptr);
    std::fprintf(out, "  pop empty null -> %d\n", redo->PopItem(nullptr));
    std::fprintf(out, "  page empty -> %d collapse\n", undo->PageToDisk());
    undo->CollapseItemStack();

    ProbeItem* items[20];
    for (int i = 0; i < 20; ++i) items[i] = new ProbeItem;
    for (int i = 0; i < 18; ++i) std::fprintf(out, "  push %s -> %d\n", items[i]->label.c_str(), undo->PushItem(items[i], false));
    state("18 pushed");
    std::fprintf(out, "  refs i1=%d i18=%d\n", Refs(items[0]), Refs(items[17]));

    // the undo stack pages in and undoes what it pops
    items[17]->Save();
    std::fprintf(out, "  pop -> %d\n", undo->PopItem(&got));
    std::fprintf(out, "  got=%s refs=%d\n", got ? static_cast<ProbeItem*>(got)->label.c_str() : "null", got ? Refs(got) : 0);
    items[16]->Save();
    items[16]->loadFail = true;
    r = undo->PopItem(&got);
    std::fprintf(out, "  pop (load fails) -> %d got=%s\n", r, got ? static_cast<ProbeItem*>(got)->label.c_str() : "null");
    items[16]->loadFail = false;
    items[15]->undoResult = false;
    r = undo->PopItem(nullptr);
    std::fprintf(out, "  pop (undo fails) null -> %d refs i16=%d\n", r, Refs(items[15]));
    state("3 popped");

    // the redo stack only stores
    std::fprintf(out, "  push i18 redo -> %d\n", redo->PushItem(items[17], false));
    std::fprintf(out, "  pop redo -> %d got=%s refs=%d\n", redo->PopItem(&got), got ? static_cast<ProbeItem*>(got)->label.c_str() : "null",
                 Refs(items[17]));
    Release(got);

    // push with redo: pages in, then Redo
    items[18]->Save();
    std::fprintf(out, "  push i19 redo=1 -> %d\n", undo->PushItem(items[18], true));
    items[19]->Save();
    items[19]->loadFail = true;
    std::fprintf(out, "  push i20 redo=1 (load fails) -> %d\n", undo->PushItem(items[19], true));
    items[19]->loadFail = false;
    items[14]->redoResult = false;
    std::fprintf(out, "  push i15 again redo=1 (redo fails) -> %d\n", undo->PushItem(items[14], true));
    state("pushed back");

    // page out: only the items not paged yet
    items[3]->Save();
    items[5]->saveFail = true;
    std::fprintf(out, "  page undo -> %d\n", undo->PageToDisk());
    std::fprintf(out, "  page undo again -> %d\n", undo->PageToDisk());
    std::fprintf(out, "  pop paged -> %d\n", undo->PopItem(nullptr));

    std::fprintf(out, "  refs before collapse:");
    for (ProbeItem* p : items) std::fprintf(out, " %d", Refs(p));
    std::fprintf(out, "\n");
    redo->PushItem(items[0], false);
    redo->PushItem(items[1], true);
    undo->CollapseItemStack();
    state("collapsed");
    std::fprintf(out, "  push after collapse -> %d\n", undo->PushItem(items[2], false));
    state("one");

    std::fprintf(out, "  release my references\n");
    for (ProbeItem* p : items) Release(p);
    std::fprintf(out, "  delete undo\n");
    delete undo;
    std::fprintf(out, "  delete redo\n");
    delete redo;
}

// opens the protected slots of a plain EditItemManager (never constructed: calls go through the module's vftable)
struct OpenManager : EditItemManager {
    using EditItemManager::GetBaseTempPath;
    using EditItemManager::GetItemGroup;
    using EditItemManager::GetSystemLevelIdentifier;
};

static void Managers()
{
    std::fprintf(out, "== managers\n");
    // factories for a manager and its items, in the EditItemManager group of plugins.ini. A type is only found
    // by name when its id has a config (that gives it a library), so these borrow the ids of the two
    // TerrainEditSupport managers listed there (the library is not installed and is never loaded).
    const UId group(kGroup);
    new PluginFactoryOf<ProbeManager, EditItemManager>(PString("ProbeManager"), UId("97006A0D-BEED-4318-AA3D-3989917AFCB7"), group);
    new PluginFactoryOf<ProbeItem, EditItem>(PString("ProbeItem"), UId("B46748D9-D66D-48de-B28B-EC2D48AC3FC6"), group);

    std::fprintf(out, "  create NoSuchManager -> %d\n", EditItemManager::CreateInstance(PString("NoSuchManager")) != nullptr);

    MakeTree("edtmp");
    std::fprintf(out, "  edtmp before=%d\n", DirExists("edtmp"));
    EditItemManager* em = EditItemManager::CreateInstance(PString("ProbeManager"));
    std::fprintf(out, "  create ProbeManager -> %d\n", em != nullptr);
    if (!em) return;
    ProbeManager* m = static_cast<ProbeManager*>(em);
    const std::string id = m->GetUUID().Text() ? m->GetUUID().Text() : "";
    std::fprintf(out, "  uuid length=%zu dashes=%d same=%d\n", id.size(), id.find('-') != std::string::npos,
                 &m->GetUUID() == &ProbeManager::GetUUID());
    uuid = id;
    std::fprintf(out, "  edtmp after initialize=%d name=%s\n", DirExists("edtmp"), Str(m->GetName()).c_str());

    PString sys;
    m->BaseSystemId(sys);
    std::fprintf(out, "  base system id=%s\n", Str(sys).c_str());
    PString keep("keep");
    // a plain manager: the unnamed slots of the module's own vftable (left alive: without Initialize its
    // destructor would use an unset mutex pointer)
    OpenManager* plain = static_cast<OpenManager*>(new EditItemManager);
    std::fprintf(out, "  plain: base temp path -> %d %s, base group null=%d name=%s\n", plain->GetBaseTempPath(keep),
                 Str(keep).c_str(), plain->GetItemGroup(PString("x")) == nullptr, Str(plain->GetName()).c_str());
    PString plainId;
    plain->GetSystemLevelIdentifier(plainId);
    std::fprintf(out, "  plain: system id=%s list=%lu onitemlist=%d\n", Str(plainId).c_str(), plain->GetItemList().Count(),
                 plain->AreItemsOnItemList());

    // temp paths and file names
    PString p("junk");
    m->GetApplicationUniqueTempPath(p);
    std::fprintf(out, "  temp path %s exists=%d\n", Str(p).c_str(), DirExists(p.Text()));
    for (int i = 0; i < 3; ++i) {
        PString f("old");
        m->GetUniqueFileName(f);
        std::fprintf(out, "  file %s\n", Str(f).c_str());
    }
    PString empty;
    m->GetUniqueFileName(empty);
    std::fprintf(out, "  file (empty in) %s\n", Str(empty).c_str());

    // item list and caps
    std::fprintf(out, "  query ProbeItem=%d\n", m->QueryItem(PString("ProbeItem")));
    PStringList& list = const_cast<PStringList&>(m->GetItemList());
    list.Add(PString("ProbeItem"));
    list.Add(PString("Other"));
    DumpList("item list", m->GetItemList());
    std::fprintf(out, "  query ProbeItem=%d Missing=%d\n", m->QueryItem(PString("ProbeItem")), m->QueryItem(PString("Missing")));
    std::fprintf(out, "  querycaps move -> %d\n", m->QueryItemCaps(PString("ProbeItem"), PString("move")));
    std::fprintf(out, "  querycaps scale -> %d\n", m->QueryItemCaps(PString("ProbeItem"), PString("scale")));
    std::fprintf(out, "  querycaps Missing -> %d\n", m->QueryItemCaps(PString("Missing"), PString("move")));
    {
        PStringList caps = m->GetItemCapsList(PString("ProbeItem"));
        DumpList("caps of ProbeItem", caps);
        PStringList none = m->GetItemCapsList(PString("Missing"));
        DumpList("caps of Missing", none);
    }
    EditItem* made = m->CreateItem(PString("ProbeItem"));
    std::fprintf(out, "  createitem -> %s refs=%d\n", made ? static_cast<ProbeItem*>(made)->label.c_str() : "null", made ? Refs(made) : 0);
    if (made) Release(made);

    auto state = [&](const char* what) {
        std::fprintf(out, "  [%s] onitemlist=%d onredolist=%d\n", what, m->AreItemsOnItemList(), m->AreItemsOnRedoList());
    };
    state("start");

    // GetItem / SubmitItem / BinItem
    ItemHandle h = m->GetItem(PString("ProbeItem"));
    ProbeItem* first = static_cast<ProbeItem*>(h.item);
    std::fprintf(out, "  getitem -> %s refs=%d\n", first ? first->label.c_str() : "null", first ? Refs(first) : 0);
    ItemHandle h2 = m->GetItem(PString("ProbeItem"));
    std::fprintf(out, "  getitem while one is out -> %d\n", h2.item != nullptr);
    state("one out");
    std::fprintf(out, "  undo while one is out -> %d\n", m->Undo());
    ItemHandle wrong(first);
    wrong.item = reinterpret_cast<EditItem*>(&wrong);
    std::fprintf(out, "  submit wrong handle -> %d handle=%d\n", m->SubmitItem(wrong), wrong.item != nullptr);
    std::fprintf(out, "  submit -> %d handle=%d refs=%d\n", m->SubmitItem(h), h.item != nullptr, Refs(first));
    state("submitted");

    ItemHandle h3 = m->GetItem(PString("ProbeItem"));
    ProbeItem* second = static_cast<ProbeItem*>(h3.item);
    second->applyResult = false;
    std::fprintf(out, "  submit (apply fails) -> %d handle=%d refs=%d\n", m->SubmitItem(h3), h3.item != nullptr, Refs(second));
    std::fprintf(out, "  submit again -> %d handle=%d\n", m->SubmitItem(h3), h3.item != nullptr);
    state("failed submit");

    ItemHandle h4 = m->GetItem(PString("ProbeItem"));
    ProbeItem* third = static_cast<ProbeItem*>(h4.item);
    std::fprintf(out, "  getitem -> %s refs=%d\n", third->label.c_str(), Refs(third));
    ItemHandle other(second);
    std::fprintf(out, "  bin other -> %d handle=%d\n", m->BinItem(other), other.item != nullptr);
    std::fprintf(out, "  bin -> %d handle=%d refs=%d\n", m->BinItem(h4), h4.item != nullptr, Refs(third));
    state("binned");

    // undo / redo
    for (int i = 0; i < 3; ++i) {
        ItemHandle x = m->GetItem(PString("ProbeItem"));
        const std::string label = static_cast<ProbeItem*>(x.item)->label;
        const bool r = m->SubmitItem(x);
        std::fprintf(out, "  get+submit %s -> %d\n", label.c_str(), r);
    }
    state("three more");
    std::fprintf(out, "  undo -> %d\n", m->Undo());
    std::fprintf(out, "  undo -> %d\n", m->Undo());
    state("undone twice");
    std::fprintf(out, "  redo -> %d\n", m->Redo());
    state("redone");
    std::fprintf(out, "  page -> %d\n", m->PageToDisk());
    std::fprintf(out, "  page again -> %d\n", m->PageToDisk());
    std::fprintf(out, "  redo (paged) -> %d\n", m->Redo());
    std::fprintf(out, "  undo (paged) -> %d\n", m->Undo());
    for (int i = 0; i < 8; ++i) std::fprintf(out, "  undo -> %d\n", m->Undo());
    state("all undone");
    for (int i = 0; i < 9; ++i) std::fprintf(out, "  redo -> %d\n", m->Redo());
    state("all redone");
    std::fprintf(out, "  temp dir exists=%d\n", DirExists(p.Text()));

    std::fprintf(out, "  release manager\n");
    std::fprintf(out, "  -> %d\n", Release(m));
    std::fprintf(out, "  temp dir exists=%d\n", DirExists(p.Text()));
    EditItemManager::CleanupThisTempDataOnDisk();
    std::fprintf(out, "  after cleanup: temp dir exists=%d edtmp=%d\n", DirExists(p.Text()), DirExists("edtmp"));
    std::fprintf(out, "  items still alive: i1..: refs");
    std::fprintf(out, " first=%d second=%d third=%d\n", Refs(first), Refs(second), Refs(third));
}

// another thread holds the named mutex, so Initialize must not clean up
static DWORD WINAPI Holder(void* arg)
{
    HANDLE m = CreateMutexA(nullptr, TRUE, "EditItemProbeMutex");
    SetEvent(static_cast<HANDLE>(arg));
    Sleep(3000);
    ReleaseMutex(m);
    CloseHandle(m);
    return 0;
}

static void SecondManager()
{
    std::fprintf(out, "== manager made directly\n");
    HANDLE ready = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    HANDLE t = CreateThread(nullptr, 0, Holder, ready, 0, nullptr);
    WaitForSingleObject(ready, INFINITE);

    MakeTree("edtmp2");
    ProbeManager* m = new ProbeManager;
    m->base = "edtmp2";
    m->fixedId = "EditItemProbeMutex";
    std::fprintf(out, "  [new] onitemlist=%d onredolist=%d name=%s\n", m->AreItemsOnItemList(), m->AreItemsOnRedoList(),
                 Str(m->GetName()).c_str());
    std::fprintf(out, "  initialize -> %d edtmp2=%d name=%s\n", m->Initialize(PString("second")), DirExists("edtmp2"),
                 Str(m->GetName()).c_str());
    std::fprintf(out, "  initialize again -> %d name=%s\n", m->Initialize(PString("again")), Str(m->GetName()).c_str());
    std::fprintf(out, "  page empty -> %d undo=%d redo=%d\n", m->PageToDisk(), m->Undo(), m->Redo());
    PString p;
    m->GetApplicationUniqueTempPath(p);
    std::fprintf(out, "  temp path (kept from the first manager) %s\n", Str(p).c_str());
    m->CleanupAllTempDataOnDisk();
    std::fprintf(out, "  cleanup all: edtmp2=%d\n", DirExists("edtmp2"));
    std::fprintf(out, "  release -> %d\n", Release(m));
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    CloseHandle(ready);
}

int main(int argc, char** argv)
{
    JET_STATIC_KERNEL;
    if (argc < 2) return 2;
    out = std::fopen(argv[1], "w");
    std::setvbuf(out, nullptr, _IONBF, 0);  // keep everything up to a crash
    // like the game, hold a plugin system reference for the whole run
    PluginCollection* keep = new PluginCollection(UId("00000000-0000-0000-0000-000000000000"));
    Items();
    Stacks();
    Managers();
    SecondManager();
    std::fprintf(out, "== end\n");
    std::fclose(out);
    (void)keep;
    return 0;
}
