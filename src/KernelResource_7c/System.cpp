// KernelResource_7c: ResourceSystem (0x10006550..0x10007c8a), its task, and the loader thread (0x10007c90..0x10008564).
// Transcribed from the assembly.
#include <windows.h>

#include <jet/Resource.h>
#include <jet/Task.h>

#include <cstdio>
#include <new>

namespace Jet {

namespace {

Profile::Info updateInfo = {"ResourceSystem::Update",
                            "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/KernelResource/ResourceSystem.cpp", 0, 0, -1,
                            0x80000000, 0, 0};

// statics of the original: the system Init installed (0x1001066c), when CheckSize runs next (0x10010678) and a
// dump request nobody sets (0x10010674)
ResourceSystem* mainSystem = nullptr;
float nextSizeCheck = 0.0f;
int dumpRequested = 0;

const char* const kResourceGroup = "4C414E80-BBDA-11d4-9AD7-0001025364B8";

// 0x10007bd0: the resource type registered for an extension (a reference), or null
struct TypeLookup : PluginBase {
    static ResourceTypeBase* Find(const PString& extension)
    {
        PluginType* type = nullptr;
        ResourceTypeBase* found = nullptr;
        if (LookupPluginType(&type, UId(kResourceGroup), extension).code == 0) found = static_cast<ResourceTypeBase*>(type);
        return found;
    }
};

}  // namespace

// ---- the task: updates the system every frame (priority 5)
class ResourceSystemTask : public Task {
public:
    explicit ResourceSystemTask(ResourceSystem* rs) : Task(PString("ResourceSystem"), 5.0f, true), system(rs) {}  // 0x10006550

protected:
    UpdateCode __cdecl Update(float dt) override  // 0x10007c70
    {
        system->Update(dt);
        return CONTINUE;
    }

private:
    ResourceSystem* system;  // +0x28
};

// ---- the loader thread: loads the queued resources (a heap by heap priority) one at a time (0x6c bytes)
class ResourceLoader {
public:
    ResourceLoader();   // 0x10007e80
    ~ResourceLoader();  // 0x10007fe0
    void Queue(const Resource* r);         // 0x10008090
    bool Unqueue(const Resource* r);       // 0x10008140
    void TakeNext();                       // 0x10008250
    void Claim(const Resource* r);         // 0x10008310
    unsigned int Run();                    // 0x10007c90
    JET_HEAP_OBJECT

    Thread* thread;                // +0x00
    void* unknown04;               // +0x04
    unsigned long count;           // +0x08 the heap: count, capacity, data
    unsigned long capacity;        // +0x0c (5000)
    const Resource** data;         // +0x10
    Mutex work;                    // +0x14 held while a resource loads (PauseProcessing takes it)
    Mutex queue;                   // +0x1c guards the heap
    Semaphore queued;              // +0x24 one count per resource queued
    WEvent idle;                   // +0x2c set between loads
    const Resource* current;       // +0x34
    bool paused;                   // +0x38
    char accessName[0x18];         // +0x39 (made, never used)
    char itemsName[0x18];          // +0x51

private:
    static unsigned int __stdcall Main(void* self) { return static_cast<ResourceLoader*>(self)->Run(); }  // 0x10007e70
    void SiftUp(unsigned long i);    // 0x10008460
    void SiftDown(unsigned long i);  // 0x100084c0
    void Heapify();                  // 0x100083f0
};

ResourceLoader::ResourceLoader()
    : unknown04(nullptr), count(0), capacity(5000),
      data(static_cast<const Resource**>(Alloc::New(5000 * sizeof(Resource*)))), work(false, nullptr),
      queue(false, nullptr), queued(0, 0x7fffffff, nullptr), idle(true, false, nullptr), current(nullptr),
      paused(false)
{
    const unsigned long pid = Thread::GetProcessId();
    std::sprintf(itemsName, "ResourceItems.%08X", pid);
    std::sprintf(accessName, "ResourceAccess.%08X", pid);
    thread = new Thread(&Main, this, false);
    thread->Resume();
    thread->SetPriority(Thread::PRIORITY_NORMAL);
}

ResourceLoader::~ResourceLoader()
{
    if (thread) delete thread;
    count = 0;
    if (data) Alloc::Delete(data);
}

void ResourceLoader::SiftUp(unsigned long i)
{
    if (!i) return;
    const Resource* const x = data[i];
    if (!(x->heapPriority > data[(i - 1) >> 1]->heapPriority)) return;
    do {
        data[i] = data[(i - 1) >> 1];
        i = (i - 1) >> 1;
    } while (i && x->heapPriority > data[(i - 1) >> 1]->heapPriority);
    data[i] = x;
}

void ResourceLoader::Heapify()
{
    for (unsigned long k = 1; k < count; ++k) SiftUp(k);
}

void ResourceLoader::SiftDown(unsigned long i)
{
    const Resource* const x = data[i];
    while (i < count >> 1) {
        unsigned long c = 2 * i + 1;
        if (c < count - 1 && !(data[c]->heapPriority > data[c + 1]->heapPriority)) ++c;
        if (x->heapPriority > data[c]->heapPriority) break;
        data[i] = data[c];
        i = c;
    }
    data[i] = x;
}

// 0x10008090: a ready resource joins the heap (with an internal reference) and is counted
void ResourceLoader::Queue(const Resource* r)
{
    SingleLock guard(&queue, true);
    if (count >= 5000 || r->status == Resource::STATUS_CACHED) return;
    ++r->internalRefs;
    r->status = Resource::STATUS_CACHED;
    if (count != capacity) {
        data[count] = r;
        SiftUp(count);
        ++count;
    }
    queued.Signal();
}

// 0x10008140: out of the heap (its count and internal reference taken back)
bool ResourceLoader::Unqueue(const Resource* r)
{
    SingleLock guard(&queue, true);
    const Resource** p = data;
    const Resource** const end = data + count;
    while (p < end && *p != r) ++p;
    if (p >= end) return false;
    const Resource* const last = data[count - 1];
    *p = last;
    --count;
    const unsigned long at = static_cast<unsigned long>(p - data);
    if (r->heapPriority > last->heapPriority)
        SiftDown(at);
    else
        SiftUp(at);
    if (--r->internalRefs == 0) r->DeleteThis();
    queued.Lock(~0ul);
    return true;
}

// 0x10008250: the heap priority grows with age; the top one becomes the current load
void ResourceLoader::TakeNext()
{
    if (count) {
        const float now = static_cast<float>(Time::GetSeconds());
        for (unsigned long i = 0; i < count; ++i)
            data[i]->heapPriority = now - data[i]->creationTime + data[i]->priority;
        Heapify();
        const Resource* const top = data[0];
        --count;
        data[0] = data[count];
        SiftDown(0);
        current = top;
        top->status = Resource::STATUS_LOADING;
    }
    queue.Unlock();
}

// 0x10008310: a queued resource is taken out to load on the caller's thread; one loading now is waited for
void ResourceLoader::Claim(const Resource* r)
{
    SingleLock guard(&queue, true);
    if (r->status == Resource::STATUS_CACHED && Unqueue(r)) {
        r->status = Resource::STATUS_LOCKING;
        return;
    }
    if (r != current) return;
    guard.Unlock();
    ++Resource::totalClashes;
    thread->SetPriority(Thread::PRIORITY_2);
    idle.Wait(~0ul);
    thread->SetPriority(Thread::PRIORITY_NORMAL);
}

// 0x10007c90: waits for a queued resource with both mutexes, loads it, repeats; at the end the heap is emptied
unsigned int ResourceLoader::Run()
{
    SyncObject* objects[3] = {&queued, &work, &queue};
    MultiLock lock(objects, 3, false);
    idle.Set();
    while (!thread->IsDone()) {
        if (paused) {
            idle.Set();
            Time::Delay(1);
            idle.Reset();
            continue;
        }
        idle.Set();
        Time::Delay(1);
        idle.Reset();
        if (lock.Lock(1000, true) == MultiLock::TIMEOUT) continue;
        TakeNext();
        if (const Resource* const r = current) {
            if (!r->TemplateLoad(r->loadName.Text())) {
                r->status = Resource::STATUS_INVALID;
            } else {
                ResourceSystem::QueueLoaded(r);
                ++Resource::totalLoaded;
            }
            if (--current->internalRefs == 0) current->DeleteThis();
            current = nullptr;
        }
        work.Unlock();
    }
    for (unsigned long i = 0; i < count; ++i)
        if (--data[i]->internalRefs == 0) data[i]->DeleteThis();
    count = 0;
    return 0;
}

// ---- ResourceSystem
ResourceSystem::ResourceSystem()  // 0x10006630
    : options(nullptr), bin(nullptr), task(nullptr), loader(nullptr), loadedLock("LoadComplete"), slotsLock(nullptr),
      slotsFree(8, 8, nullptr), time(0.0f), resources(0), loadedResources(0), binned(0), size(0), dataSize(0),
      binSize(0), maxSize(0), paused(0), dumpUnused(false)
{
    flags &= ~1;
    for (Slot& s : slots) {
        s.resource = nullptr;
        s.used = false;
    }
}

// 0x10006730: the task, the loader and the bin go; the statics are cleared
ResourceSystem::~ResourceSystem()
{
    if (task) delete task;
    if ((flags & 1) && loader) delete loader;
    if (bin) delete bin;
    Resource::resourceSystem = nullptr;
    ResourceDatabase::resourceSystem = nullptr;
    mainSystem = nullptr;
}

// 0x10006890: options "ResourceCaching" (a loader thread, default on; also on with no options) and
// "ResourceInstantBin"
void ResourceSystem::Init(const Options* o)
{
    Resource::resourceSystem = this;
    ResourceDatabase::resourceSystem = this;
    flags |= 1;
    mainSystem = this;
    resources = size = loadedResources = dataSize = binned = binSize = 0;
    options = o;
    ResourceBin* b;
    if (o) {
        bool caching = true;
        o->Get("ResourceCaching", caching, true);
        flags = static_cast<unsigned char>((flags & ~1) | (caching ? 1 : 0));
        maxSize = 30000000;
        if (o->Get("ResourceInstantBin"))
            b = new ResourceBinInstant;
        else
            b = new ResourceBinCache;
    } else {
        b = new ResourceBinCache;
    }
    InitBin(b);
    loader = (flags & 1) ? new ResourceLoader : nullptr;
    task = new ResourceSystemTask(this);
}

void ResourceSystem::InitBin(ResourceBin* b)  // 0x100076b0 (none: an instant one)
{
    if (bin) delete bin;
    bin = nullptr;
    if (!b) b = new ResourceBinInstant;
    bin = b;
    b->Init(options);
}

ResourceDatabase* ResourceSystem::FindDatabase(const PString& name)  // 0x10006cc0
{
    ResourceDatabase** const slot = FindDatabaseSlot(name);
    return slot ? *slot : nullptr;
}

ResourceDatabase** ResourceSystem::FindDatabaseSlot(const PString& name)  // 0x10007b60
{
    for (FastDynamicArray<ResourceDatabase*, 0x1d>::Iterator i = databases.First(); i.node; ++i)
        if ((*i.node)->name.node == name.node) return i.node;
    return nullptr;
}

void ResourceSystem::AddDatabase(ResourceDatabase* db) { databases.Add(db); }  // 0x10007a60

// 0x10006c00: the other databases drop it as a parent (from the last), then it leaves the list
void ResourceSystem::RemoveDatabase(ResourceDatabase* db)
{
    ResourceSystem* const rs = ResourceDatabase::resourceSystem;
    if (FastDynamicArray<ResourceDatabase*, 0x1d>::Chunk* const head = rs->databases.head) {
        FastDynamicArray<ResourceDatabase*, 0x1d>::Chunk* chunk = head->prev;
        ResourceDatabase** item = &chunk->data[chunk->count - 1];
        while (item) {
            ResourceDatabase* const other = *item;
            if (--item < &chunk->data[0]) {
                if (chunk == head) {
                    item = nullptr;
                } else {
                    chunk = chunk->prev;
                    item = &chunk->data[chunk->count - 1];
                }
            }
            if (other != db) other->Invalidate(db);
        }
    }
    rs->databases.Remove(rs->databases.Find(db));
}

void ResourceSystem::Dump()  // 0x10006ec0
{
    if (bin) bin->Dump();
}

const Options* ResourceSystem::GetOptions() const { return options; }  // 0x10007920

int ResourceSystem::GetQueuedResourceCount() { return loader ? static_cast<int>(loader->count) : 0; }  // 0x10007a40

void ResourceSystem::PauseProcessing()  // 0x10007160
{
    if ((flags & 1) && paused == 0) loader->work.Lock(~0ul);
    ++paused;
}

void ResourceSystem::ResumeProcessing()  // 0x100071a0
{
    --paused;
    if ((flags & 1) && paused == 0) loader->work.Unlock();
}

// 0x100071d0: completes what the loader loaded, runs the bin, and every 0.1 s keeps the size under the limit
void ResourceSystem::Update(float dt)
{
    Profile profile(&updateInfo);
    if (dumpUnused) DumpUnused();
    if (flags & 1) ProcessLoaded();
    bin->Update(dt);
    time += dt;
    if (time > nextSizeCheck) CheckSize();
    if (dumpRequested) Dump();
}

// 0x100072a0: a quarter of the loaded queue (at least five) is completed on this thread
void ResourceSystem::ProcessLoaded()
{
    FastDynamicArray<const Resource*, 13> batch;
    {
        SingleLock guard(&loadedLock, true);
        unsigned long total = 0;
        if (FastDynamicArray<const Resource*, 13>::Chunk* const head = loaded.head) {
            FastDynamicArray<const Resource*, 13>::Chunk* c = head;
            do {
                total += c->count;
                c = c->next;
            } while (c != head);
        }
        unsigned long n = total >> 2;
        if (n <= 5) n = 5;
        if (n > total) n = total;
        while (n--) {
            const FastDynamicArray<const Resource*, 13>::Iterator first = loaded.First();
            const Resource* const r = *first.node;
            loaded.Remove(first);
            batch.Add(r);
        }
    }
    for (FastDynamicArray<const Resource*, 13>::Iterator i = batch.First(); i.node; ++i) {
        const Resource* const r = *i.node;
        if (r->status == Resource::STATUS_READY || r->status == Resource::STATUS_INACTIVE) continue;
        if (const_cast<Resource*>(r)->LoadComplete().code == 0) {
            r->SetAvailableOnLoad();
            r->OnLoadNotify();
        } else {
            r->status = Resource::STATUS_INVALID;
        }
    }
}

void ResourceSystem::QueueLoaded(const Resource* r)  // 0x100074a0: loaded by the thread, completed by Update
{
    ResourceSystem* const rs = Resource::resourceSystem;
    if (r->status != Resource::STATUS_LOADING) return;
    SingleLock guard(&rs->loadedLock, true);
    rs->loaded.Add(r);
    r->status = Resource::STATUS_LOADED;
}

// 0x10007530: one of eight slots for loading r on this thread; a slot already taken for r is waited on
void ResourceSystem::AcquireSlot(const Resource* r)
{
    ResourceSystem* const rs = Resource::resourceSystem;
    rs->slotsLock.Lock(~0ul);
    for (Slot& s : rs->slots) {
        if (!s.used || s.resource != r) continue;
        rs->slotsLock.Unlock();
        s.lock.Lock(~0ul);
        rs->slotsFree.Wait(~0ul);
        s.used = true;
        return;
    }
    rs->slotsFree.Wait(~0ul);
    for (Slot& s : rs->slots) {
        if (s.used) continue;
        s.resource = r;
        s.lock.Lock(~0ul);
        s.used = true;
        rs->slotsLock.Unlock();
        return;
    }
}

void ResourceSystem::ReleaseSlot(const Resource* r)  // 0x10007610
{
    ResourceSystem* const rs = Resource::resourceSystem;
    rs->slotsLock.Lock(~0ul);
    Slot* s = rs->slots;
    while (!(s->used && s->resource == r)) ++s;
    s->used = false;
    s->lock.Unlock();
    rs->slotsFree.Signal();
    rs->slotsLock.Unlock();
}

void ResourceSystem::Unqueue(const Resource* r)  // 0x10007720: out of the loaded queue
{
    SingleLock guard(&loadedLock, true);
    const FastDynamicArray<const Resource*, 13>::Iterator i = loaded.Find(r);
    if (i.node) loaded.Remove(i);
}

void ResourceSystem::Unbin(const Resource* r)  // 0x100077b0 (the databases' system)
{
    ResourceSystem* const rs = ResourceDatabase::resourceSystem;
    SingleLock guard(&rs->loadedLock, true);
    const FastDynamicArray<const Resource*, 13>::Iterator i = rs->loaded.Find(r);
    if (i.node) rs->loaded.Remove(i);
}

// 0x10007880: over the limit, the databases (in order) send their least wanted resources to the bin
void ResourceSystem::CheckSize()
{
    if (size - binSize + dataSize > maxSize) {
        int excess = static_cast<int>(size - maxSize + dataSize);
        FastDynamicArray<ResourceDatabase*, 0x1d>::Iterator i = databases.First();
        if (excess > 0) {
            while (i.node) {
                (*i.node)->ProcessLRU(excess);
                ++i;
                if (excess <= 0) break;
            }
        }
    }
    nextSizeCheck = time + 0.1f;
}

void ResourceSystem::LoaderClaim(const Resource* r) { loader->Claim(r); }
void ResourceSystem::LoaderQueue(const Resource* r) { loader->Queue(r); }
Mutex& ResourceSystem::LoaderMutex() { return loader->work; }

// 0x10007930: asked from another thread, the next Update does it
void ResourceSystem::DumpUnusedResourcesFromCache()
{
    if (loader) loader->paused = true;
    dumpUnused = true;
    if (Thread::mainThreadId == Thread::GetCurrentId()) DumpUnused();
}

// 0x10007970: with the loader held (or not busy), every database and the bin let go of what nobody uses
void ResourceSystem::DumpUnused()
{
    if (loader && paused == 0 && !loader->work.Lock(0)) return;
    ++paused;
    for (FastDynamicArray<ResourceDatabase*, 0x1d>::Iterator i = databases.First(); i.node; ++i)
        (*i.node)->DumpUnusedResourcesFromCache();
    if (bin) static_cast<ResourceBinCache*>(bin)->DumpAll();  // (whatever the bin's kind)
    --paused;
    if (loader && paused == 0) loader->work.Unlock();
    dumpUnused = false;
    if (loader) loader->paused = false;
}

// 0x10006cf0: the type of the name's extension ("<ext>.txt" for a .txt resource) makes it
const Resource* ResourceSystem::CreateFromType(const PString& name, const ResourceDatabase* owner, bool txt)
{
    File::Path p(name.Text());
    p.DelPath();
    p.DelFileName();
    p.ToLower();
    if (!p.Length()) return nullptr;
    if (p.text[0] == '.') p.Del(0, 1);
    if (txt) p.Add(".txt");
    ResourceTypeBase* type = TypeLookup::Find(PString(p.text));
    if (!type) return nullptr;
    Resource* const r = type->Create(name, owner);
    if (r) {
        ResourceTypeBase* const t = type;
        type = nullptr;
        if (r->type) r->type->ReleaseReference();
        r->flags = static_cast<unsigned char>((r->flags & ~4) | (txt ? 4 : 0));
        r->type = t;
    }
    if (type) type->ReleaseReference();
    return r;
}

// 0x10006f10: the type of the extension (or "<ext>.txt") makes it on the fly
const Resource* ResourceSystem::CreateOnTheFly(const PString& name, const ResourceDatabase* owner)
{
    File::Path p(name.Text());
    p.DelPath();
    p.DelFileName();
    p.ToLower();
    if (!p.Length()) return nullptr;
    if (p.text[0] == '.') p.Del(0, 1);
    ResourceTypeBase* type = TypeLookup::Find(PString(p.text));
    if (!type) {
        p.Add(".txt");
        type = TypeLookup::Find(PString(p.text));
        if (!type) return nullptr;
    }
    const Resource* const r = type->CreateOnTheFly(name, owner);
    if (r) {
        ResourceTypeBase* const t = type;
        type = nullptr;
        if (r->type) r->type->ReleaseReference();
        r->type = t;
    }
    if (type) type->ReleaseReference();
    return r;
}

}  // namespace Jet
