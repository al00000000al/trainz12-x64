// KernelResource_7c: ResourceDatabase (0x10003820..0x10005fc4, 0x10001d80..0x10001ee6), its resource table and Find,
// and ResourceNamelessDatabase (0x100063a0..0x1000654d). Transcribed from the assembly.
#include <jet/Resource.h>

#include <cstdlib>
#include <new>

bool JetResourceFileExists(const char* path);  // FileExists.cpp: the imported File::Exists

namespace Jet {

ResourceSystem* ResourceDatabase::resourceSystem = nullptr;

namespace {

void Assign(PString& to, const PString& from)
{
    to.~PString();
    new (&to) PString(from);
}

// a PString made from part of another's text (PStringCache::Create(text, length))
PString Part(const PString& s, unsigned int start, unsigned int length)
{
    PString p;
    p.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(s.node->text + start), length);
    return p;
}

}  // namespace

// ---- the resource table
ResourceDatabase::ResourceTable::ResourceTable() : count(0), bucketCount(0x191)  // 0x100058d0
{
    auto* block = static_cast<unsigned long*>(Alloc::New(sizeof(unsigned long) + 0x191 * sizeof(Bucket)));
    if (!block) {
        buckets = nullptr;
        return;
    }
    *block = 0x191;
    buckets = reinterpret_cast<Bucket*>(block + 1);
    for (unsigned long i = 0; i < 0x191; ++i) new (&buckets[i]) Bucket;
}

ResourceDatabase::ResourceTable::~ResourceTable()  // 0x10005990
{
    if (!buckets) return;
    unsigned long* const block = reinterpret_cast<unsigned long*>(buckets) - 1;
    for (unsigned long i = *block; i; --i) buckets[i - 1].~Bucket();
    Alloc::Delete(block);
}

void ResourceDatabase::ResourceTable::Iterator::Begin(unsigned long from)  // 0x10005cb0
{
    bucket = from;
    head = chunk = table->buckets[from].head;
    node = head ? &head->data[0] : nullptr;
    while (!node && bucket < table->bucketCount - 1) {
        ++bucket;
        head = chunk = table->buckets[bucket].head;
        node = head ? &head->data[0] : nullptr;
    }
}

void ResourceDatabase::ResourceTable::Iterator::Next()  // 0x10005850
{
    if (node && ++node >= &chunk->data[chunk->count]) {
        chunk = chunk->next;
        node = chunk == head ? nullptr : &chunk->data[0];
    }
    while (!node) {
        if (++bucket >= table->bucketCount) return;
        head = chunk = table->buckets[bucket].head;
        node = head ? &head->data[0] : nullptr;
    }
}

// 0x10005b60: by the name's node (the bucket from its address)
ResourceDatabase::ResourceTable::Iterator ResourceDatabase::ResourceTable::Find(const PStringCache::Node* key)
{
    Iterator i(*this);
    const unsigned long index = reinterpret_cast<unsigned long>(key) % bucketCount;
    Bucket::Chunk* const first = buckets[index].head;
    Bucket::Chunk* chunk = first;
    const Resource** node = first ? &first->data[0] : nullptr;
    while (node) {
        if ((*node)->name.node == key) {
            i.bucket = index;
            i.node = node;
            i.head = first;
            i.chunk = chunk;
            return i;
        }
        if (++node >= &chunk->data[chunk->count]) {
            chunk = chunk->next;
            if (chunk == first) break;
            node = &chunk->data[0];
        }
    }
    return i;
}

void ResourceDatabase::ResourceTable::Insert(const Resource* r)  // 0x100059c0
{
    const unsigned long index = reinterpret_cast<unsigned long>(r->name.node) % bucketCount;
    ++count;
    buckets[index].Add(r);
}

// 0x10005a30: the count drops even when the iterator is empty (kept); the next element is returned
ResourceDatabase::ResourceTable::Iterator ResourceDatabase::ResourceTable::Remove(const Iterator& i)
{
    --count;
    const Bucket::Iterator next = buckets[i.bucket].Remove(Bucket::Iterator(i.node, i.head, i.chunk));
    Iterator out(*this);
    if (next.node) {
        out.bucket = i.bucket;
        out.node = next.node;
        out.head = next.head;
        out.chunk = next.chunk;
        return out;
    }
    if (i.bucket < bucketCount - 1) out.Begin(i.bucket + 1);
    return out;
}

void ResourceDatabase::ResourceTable::Clear()  // 0x10005af0
{
    for (unsigned long i = 0; i < bucketCount; ++i) buckets[i].RemoveAll();
    count = 0;
}

// ---- construction
void ResourceDatabase::Construct(const PString& n)
{
    unknown04 = unknown08 = nullptr;
    parents.head = nullptr;
    variants.next = variants.prev = variants.Head();
    variants.count = 0;
    Assign(name, n);
    lookups = requests = invalidRequests = 0;
}

ResourceDatabase::ResourceDatabase(const PString& n)  // 0x10003880
{
    Construct(n);
    resourceSystem->AddDatabase(this);
    flags |= 1;
}

// 0x10003970: "<path>/<name>.ja" (an archive) and "<path>/<name>/" are mounted as the drive "<name>:"
ResourceDatabase::ResourceDatabase(const PString& n, const PString& path)
{
    Construct(n);
    flags |= 1;
    File::Path p;
    p = path.Text();
    p.AddPath(n.Text());
    p.Add(".ja");
    File::Subst(n, p.text, false);
    p = path.Text();
    p.AddPath(n.Text());
    p.Add("/");
    File::Subst(n, p.text, true);
    resourceSystem->AddDatabase(this);
}

// 0x10003b60: the loader is held off while queued resources are taken back; then every resource nobody holds goes
ResourceDatabase::~ResourceDatabase()
{
    ResourceSystem* const rs = resourceSystem;
    if ((rs->flags & 1) && rs->paused == 0) rs->LoaderMutex().Lock(~0ul);
    ++rs->paused;
    {
        ResourceTable::Iterator i(resources);
        for (i.Begin(0); i.node; i.Next()) {
            const Resource* const r = *i.node;
            if (r->status == Resource::STATUS_CACHED) {
                if (resourceSystem->flags & 1) resourceSystem->LoaderClaim(r);
            } else if (r->status == Resource::STATUS_LOADED) {
                resourceSystem->Unqueue(r);
            }
        }
    }
    --resourceSystem->paused;
    if ((resourceSystem->flags & 1) && resourceSystem->paused == 0) resourceSystem->LoaderMutex().Unlock();
    {
        SingleLock guard(&lock, true);
        for (;;) {
            ResourceTable::Iterator i(resources);
            i.Begin(0);
            while (i.node && (*i.node)->refs != 0) i.Next();
            if (!i.node) break;
            const Resource* const r = *i.node;
            if (r->status == Resource::STATUS_INACTIVE) resourceSystem->bin->Remove(r);
            resourceSystem->Unbin(r);
            if (--r->internalRefs == 0) r->DeleteThis();
        }
        resources.Clear();
    }
    File::Subst(name, nullptr, false);
    resourceSystem->RemoveDatabase(this);
    // (then the lock, the variants, the parents, the table and the name)
    while (variants.count) {
        ResourceVariant* const v = variants.next;
        v->prev->next = v->next;
        v->next->prev = v->prev;
        v->prev = v->next = nullptr;
        --variants.count;
        delete v;
    }
}

// ---- lookups
// 0x10003e50: a name not here (flag 4 unset) becomes an invalid resource, its load name the text from the first
// ';'. A redirection resolves to its target; one found in another database gets a redirection here.
const Resource* ResourceDatabase::Get(const PString& n, int getFlags) const
{
    if (!n.node || !n.node->length) return nullptr;
    const Resource* r = GetFromThis(n);
    if (!r) {
        ++requests;
        if (!(getFlags & 4)) {
            PString load(n);
            const int at = n.node ? PString::Cache()->Find(n.node, ';', 0) : -1;
            if (at != -1) {
                PString part;
                const unsigned int length = n.node ? n.node->length : 0;
                if (static_cast<unsigned int>(at) <= length) part = Part(n, at, length - at);
                Assign(load, part);
            }
            if (const Resource* invalid = CreateInvalidResource(load, n)) invalid->status = Resource::STATUS_INVALID;
        }
        ++invalidRequests;
        return nullptr;
    }
    if (r->status == Resource::STATUS_INVALID) return nullptr;
    if (r->flags & 8) {
        do r = r->GetRedirection();
        while (r->flags & 8);
        if (r->status == Resource::STATUS_INVALID) return nullptr;
    } else if (r->owner->name.node != name.node) {
        CreateResource(r);
    }
    r->orig = this;
    if (!(getFlags & 2) && r->status == Resource::STATUS_READY && (Resource::resourceSystem->flags & 1))
        Resource::resourceSystem->LoaderQueue(r);
    r->lastUse = Resource::resourceSystem->time;
    return r;
}

bool ResourceDatabase::Exists(const PString& n) const  // 0x10004050
{
    if (!n.node || !n.node->length) return false;
    const Resource* const r = GetFromThis(n);
    if (!r || r->status == Resource::STATUS_INVALID) return false;
    r->ReleaseReference();
    return true;
}

// 0x10004090: "a;b;c" tries a, b and c in turn
const Resource* ResourceDatabase::GetFromThis(const PString& n) const
{
    if (!n.node) return GetOneFromThis(n);
    int at = PString::Cache()->Find(n.node, ';', 0);
    if (at == -1) return GetOneFromThis(n);
    unsigned int start = 0;
    do {
        PString part;
        const unsigned int length = n.node ? n.node->length : 0;
        if (start <= length) {
            int count = at - static_cast<int>(start);
            if (count < 0 || count + start > length) count = static_cast<int>(length - start);
            part = Part(n, start, static_cast<unsigned int>(count));
        }
        if (const Resource* r = GetOneFromThis(part)) return r;
        start = static_cast<unsigned int>(at) + 1;
        at = n.node ? PString::Cache()->Find(n.node, ';', start) : -1;
    } while (at != -1);
    if (!start) return GetOneFromThis(n);
    PString last;
    const unsigned int length = n.node ? n.node->length : 0;
    if (start <= length) last = Part(n, start, length - start);
    return GetOneFromThis(last);
}

// 0x100042c0: in the table (a binned one is taken back; one being deleted is made again), else from a file, made
// on the fly, or from the parents. Found here it gains a reference (a redirection's target does).
const Resource* ResourceDatabase::GetOneFromThis(const PString& n) const
{
    ++lookups;
    {
        SingleLock guard(&lock, true);
        const ResourceTable::Iterator i = resources.Find(n.node);
        if (i.node) {
            const Resource* r = *i.node;
            while (r->flags & 8) r = r->GetRedirection();
            if (r->status == Resource::STATUS_INVALID) return *i.node;
            if (r->status != Resource::STATUS_INACTIVE || !resourceSystem->bin->Reuse(r)) {
                if (r->status == Resource::STATUS_DELETE) goto make;
            }
            ++r->refs;
            return *i.node;
        }
    make:
        const Resource* r = GetFromFile(n);
        if (!r) r = GetFromOnTheFly(n);
        if (r) {
            if (!(r->flags & 8)) ++r->refs;
            return r;
        }
    }
    return GetFromParents(n);
}

// 0x10004400: "<database>:<name>" (a database with no name: the name)
PString ResourceDatabase::GetFileName(const PString& n) const
{
    if (name.node && name.node->length) {
        PString drive(name);
        drive.node = PString::Cache()->Ins(drive.node, reinterpret_cast<const unsigned char*>(":"), drive.node ? drive.node->length : 0);
        PString full(drive);
        full.node = PString::Cache()->Ins(full.node, reinterpret_cast<const unsigned char*>(n.node ? n.node->text : nullptr),
                                          full.node ? full.node->length : 0);
        return full;
    }
    return n;
}

void ResourceDatabase::AddParent(const PString& n)  // 0x10004530
{
    ResourceDatabase** const slot = resourceSystem->FindDatabaseSlot(n);
    if (slot && *slot) parents.Add(*slot);
}

void ResourceDatabase::AddParent(ResourceDatabase* db) { parents.Add(db); }  // 0x10004570

// 0x10004590: the parents' names, depth first, each once
void ResourceDatabase::GetParentResourceDatabases(DynamicArray<PString, 13, Less<PString, PString>>& out)
{
    for (ParentList::Iterator i = parents.First(); i.node; ++i) {
        ResourceDatabase* const p = *i.node;
        if (!out.Find(p->name).node) out.Add(p->name);
        p->GetParentResourceDatabases(out);
    }
}

// 0x10004600: in each variant's folder, then the database's own
const Resource* ResourceDatabase::GetFromFile(const PString& n) const
{
    for (ResourceVariant* v = variants.next; v != const_cast<VariantList&>(variants).Head(); v = v->next)
        if (const Resource* r = GetFromPath(n, v->GetName())) return r;
    return GetFromPath(n, PString(""));
}

// 0x100046c0: "<variant>/<name>" as a file, or "<variant>/<name>.txt" (a .txt resource), made by its type
const Resource* ResourceDatabase::GetFromPath(const PString& n, const PString& variant) const
{
    PString base(n);
    File::Path p;
    p = variant.Text();
    p.AddPath(n.Text());
    File::Path plain;
    {
        const PString file = GetFileName(PString(p.text));
        plain = file.Text();
    }
    p.Add(".txt");
    {
        const PString file = GetFileName(PString(p.text));
        p = file.Text();
    }
    bool txt;
    if (JetResourceFileExists(plain.text))
        txt = false;
    else if (JetResourceFileExists(p.text))
        txt = true;
    else
        return nullptr;
    File::Path load(variant.Text());
    load.AddPath(n.Text());
    const PString loadName(load.text);
    return CreateResource(n, loadName, txt);
}

// 0x10004a40: a resource its type makes without a file
const Resource* ResourceDatabase::GetFromOnTheFly(const PString& n) const
{
    const Resource* const r = ResourceSystem::CreateOnTheFly(n, this);
    if (!r) return nullptr;
    SingleLock guard(&lock, true);
    resources.Insert(r);
    ++resourceSystem->resources;
    if (r->CachedSize() != ~0u) resourceSystem->size += r->CachedSize();
    r->status = Resource::STATUS_READY;
    return r;
}

const Resource* ResourceDatabase::GetFromParents(const PString& n) const  // 0x10004b00
{
    for (ParentList::ConstIterator i = parents.First(); i.node; ++i)
        if (const Resource* r = (*i.node)->GetFromThis(n)) return r;
    return nullptr;
}

void ResourceDatabase::Dump() const {}  // 0x10004b60

// 0x10004b70: db is no parent any more; the redirections here to its resources leave the table (not deleted)
void ResourceDatabase::Invalidate(ResourceDatabase* db)
{
    ParentList::Iterator p = parents.Find(db);
    if (!p.node) return;
    parents.Remove(p);
    ResourceTable::Iterator i(resources);
    i.Begin(0);
    while (i.node) {
        const Resource* const r = *i.node;
        if ((r->flags & 8) && r->GetRedirection()->owner == db)
            i = resources.Remove(i);
        else
            i.Next();
    }
}

void ResourceDatabase::AddVariant(const PString& n)  // 0x10004c60
{
    for (ResourceVariant* v = variants.next; v != variants.Head(); v = v->next)
        if (v->GetName().node == n.node) return;
    ResourceVariant* const v = new ResourceVariant(n);
    v->next = variants.Head();
    v->prev = variants.prev;
    variants.prev->next = v;
    ++variants.count;
    variants.prev = v;
}

// 0x10004cf0: made by the type of the name's extension, ready to load from loadName
const Resource* ResourceDatabase::CreateResource(const PString& n, const PString& loadName, bool txt) const
{
    const Resource* const r = ResourceSystem::CreateFromType(n, this, txt);
    if (!r) return nullptr;
    r->SetLoadName(loadName);
    SingleLock guard(&lock, true);
    resources.Insert(r);
    ++resourceSystem->resources;
    if (r->CachedSize() != ~0u) resourceSystem->size += r->CachedSize();
    r->status = Resource::STATUS_READY;
    return r;
}

// 0x10004de0: a binned resource is taken back and loses the table's internal reference
void ResourceDatabase::Remove(const PString& n) const
{
    const Resource* r;
    {
        SingleLock guard(&lock, true);
        const ResourceTable::Iterator i = resources.Find(n.node);
        if (!i.node) return;
        r = *i.node;
        if (r->status == Resource::STATUS_INACTIVE) resourceSystem->bin->Reuse(r);
        resourceSystem->Unbin(r);
    }
    if (--r->internalRefs == 0) r->DeleteThis();
}

// 0x10004eb0: a redirection here to a resource of another database
void ResourceDatabase::CreateResource(const Resource* target) const
{
    RedirResource* const redir = new RedirResource(target->name, this, target);
    target->redirs.Add(redir);
    SingleLock guard(&lock, true);
    resources.Insert(redir);
    ++resourceSystem->resources;
    if (redir->CachedSize() != ~0u) resourceSystem->size += redir->CachedSize();
}

void ResourceDatabase::SetWarnings(bool on) const  // 0x10004fb0
{
    flags = static_cast<unsigned char>((flags & ~1) | (on ? 1 : 0));
}

void ResourceDatabase::Inactive(const Resource* r) const  // 0x10004fd0
{
    lock.Lock(~0ul);
    if (r->refs == 0) resourceSystem->bin->Add(r);
    lock.Unlock();
}

// 0x10005010: out of the table (the count drops even when it is not there); the resource loses its owner
void ResourceDatabase::Remove(const Resource* r) const
{
    SingleLock guard(&lock, true);
    const ResourceTable::Iterator i = resources.Find(r->name.node);
    resources.Remove(i);
    r->owner = nullptr;
}

const Resource* ResourceDatabase::CreateInvalidResource(const PString& n, const PString& loadName) const  // 0x100050b0
{
    Resource* const r = new Resource(n, this);
    Assign(r->loadName, n);  // (the second name is not used)
    SingleLock guard(&lock, true);
    resources.Insert(r);
    ++resourceSystem->resources;
    if (r->CachedSize() != ~0u) resourceSystem->size += r->CachedSize();
    return r;
}

void ResourceDatabase::RemoveParent(ResourceDatabase* db) { Invalidate(db); }  // 0x100051d0
void ResourceDatabase::Lock() const { lock.Lock(~0ul); }                      // 0x100051e0
void ResourceDatabase::Unlock() const { lock.Unlock(); }                      // 0x10005200

int ResourceDatabase::OrderResourceLRU(const void* a, const void* b)  // 0x10005220: oldest use first
{
    const float ta = (*static_cast<const Resource* const*>(a))->lastUse;
    const float tb = (*static_cast<const Resource* const*>(b))->lastUse;
    if (ta > tb) return 1;
    if (tb > ta) return -1;
    return 0;
}

// 0x10005250: up to five of the available resources nobody holds (the most recently used first) go to the bin,
// their sizes taken off `excess`
void ResourceDatabase::ProcessLRU(int& excess) const
{
    lock.Lock(~0ul);
    const Resource** const list = static_cast<const Resource**>(_alloca(resources.count * sizeof(Resource*)));
    unsigned long n = 0;
    ResourceTable::Iterator i(resources);
    for (i.Begin(0); i.node; i.Next()) {
        const Resource* const r = *i.node;
        if (r->refs == 0 && r->status == Resource::STATUS_AVAILABLE && r->locks == 0) list[n++] = r;
    }
    if (n) {
        std::qsort(list, n, sizeof(*list), OrderResourceLRU);
        int left = 5;
        while (excess > 0 && n && left--) {
            const Resource* const r = list[--n];
            const unsigned int s = r->CachedSize(), d = r->CachedDataSize();
            if (s != ~0u) excess -= static_cast<int>(s);
            if (d != ~0u) excess -= static_cast<int>(d);
            if (r->refs != 0) {
                const_cast<Resource*>(r)->Unload();
            } else if (r->status == Resource::STATUS_AVAILABLE) {
                --Resource::totalActive;
                const ResourceDatabase* const db = r->owner;
                db->lock.Lock(~0ul);
                if (r->refs == 0) resourceSystem->bin->Add(r);
                db->lock.Unlock();
            }
        }
    }
    lock.Unlock();
}

// 0x10005670: every unused, unlocked resource that is not in use by the loader goes
void ResourceDatabase::DumpUnusedResourcesFromCache()
{
    lock.Lock(~0ul);
    {
        SingleLock guard(&resourceSystem->loadedLock, true);
        const Resource** const list = static_cast<const Resource**>(_alloca(resources.count * sizeof(Resource*)));
        unsigned long n = 0;
        ResourceTable::Iterator i(resources);
        for (i.Begin(0); i.node; i.Next()) {
            const Resource* const r = *i.node;
            if (r->refs != 0 || r->locks != 0) continue;
            if (r->status == Resource::STATUS_CACHED) {
                if (resourceSystem->flags & 1) resourceSystem->LoaderClaim(r);
                resourceSystem->Unqueue(r);
                r->status = Resource::STATUS_READY;
            } else if (r->status == Resource::STATUS_LOADED) {
                resourceSystem->Unqueue(r);
                if (const_cast<Resource*>(r)->LoadComplete().code == 0) {
                    r->SetAvailableOnLoad();
                    r->OnLoadNotify();
                } else {
                    r->status = Resource::STATUS_INVALID;
                }
            }
            if (r->status == Resource::STATUS_AVAILABLE || r->status == Resource::STATUS_READY ||
                r->status == Resource::STATUS_LOCKING || r->status == Resource::STATUS_INVALID)
                list[n++] = r;
        }
        while (n) {
            const Resource* const r = list[--n];
            if (r->refs != 0) continue;
            if (r->status == Resource::STATUS_INACTIVE) resourceSystem->bin->Remove(r);
            resourceSystem->Unbin(r);
            if (--r->internalRefs == 0) r->DeleteThis();
        }
        lock.Unlock();
    }
}

ResourceDatabase* ResourceDatabase::FindDatabase(const PString& n)  // 0x10005630
{
    if (!resourceSystem) return nullptr;
    ResourceDatabase** const slot = resourceSystem->FindDatabaseSlot(n);
    return slot ? *slot : nullptr;
}

const Options* ResourceDatabase::GetOptions() const { return resourceSystem ? resourceSystem->options : nullptr; }
ResourceSystem* ResourceDatabase::GetMainResourceSystem() { return resourceSystem; }    // 0x10001d80
void ResourceDatabase::InitRS(ResourceSystem* rs) { resourceSystem = rs; }               // 0x10001e20
const PString& ResourceDatabase::GetName() const { return name; }                        // 0x10001df0
bool ResourceDatabase::operator==(const PString& n) const { return name.node == n.node; }  // 0x10001e00

// ---- search handles
// 0x1000a52c: a File::Find over the database's drive
bool ResourceDatabase::RDBSearchHandle::Open(const char* pattern, ResourceDatabase* db) { return find.Open(pattern, db->GetName()); }
void ResourceDatabase::RDBSearchHandle::Close() { find.Close(); }
bool ResourceDatabase::RDBSearchHandle::Next() { return find.Next(); }
const File::Entry* ResourceDatabase::RDBSearchHandle::GetEntry() { return reinterpret_cast<const File::Entry*>(&find); }

bool ResourceDatabase::SearchHandle::Open(const char*, ResourceDatabase*) { return false; }  // 0x100085a0
void ResourceDatabase::SearchHandle::Close() {}
bool ResourceDatabase::SearchHandle::Next() { return false; }
const File::Entry* ResourceDatabase::SearchHandle::GetEntry() { return nullptr; }

ResourceDatabase::SearchHandle* ResourceDatabase::GetSearchHandle() { return new RDBSearchHandle; }  // 0x10001e50

ResourceDatabase::Find::Find() : handle(nullptr) {}                             // 0x10005410
ResourceDatabase::Find::Find(const Find& o) : entry(o.entry), handle(o.handle) {}  // 0x10001d90 (shared)

ResourceDatabase::Find& ResourceDatabase::Find::operator=(const Find& o)  // 0x10001dc0
{
    entry = o.entry;
    handle = o.handle;
    return *this;
}

ResourceDatabase::Find::~Find() { Close(); }  // 0x10005430

bool ResourceDatabase::Find::Open(const char* pattern, ResourceDatabase* db)  // 0x10005470
{
    Close();
    if (!db) return false;
    handle = db->GetSearchHandle();
    if (!handle) return false;
    if (handle->Open(pattern, db)) {
        entry = *handle->GetEntry();
        return true;
    }
    Close();
    return false;
}

void ResourceDatabase::Find::Close()  // 0x10005510
{
    if (!handle) return;
    handle->Close();
    delete handle;
    handle = nullptr;
}

bool ResourceDatabase::Find::Next()  // 0x10005550
{
    if (!handle || !handle->Next()) return false;
    entry = *handle->GetEntry();
    return true;
}

const File::Entry& ResourceDatabase::Find::GetEntry()  // 0x100055a0
{
    return handle ? *handle->GetEntry() : entry;
}

// ---- ResourceNamelessDatabase
// 0x10006410: "<path>.ja" and "<path>/" (a trailing slash dropped) as the drive "<name>:" (the path comes first)
ResourceNamelessDatabase::ResourceNamelessDatabase(const PString& path, const PString& n) : ResourceDatabase(n)
{
    File::Path p(path.Text());
    const unsigned int length = p.Length();
    char& last = p.text[length - 1];
    if (last == '\\' || last == '/') last = 0;
    File::Path q;
    q = p;
    q.Add(".ja");
    File::Subst(name, q.text, false);
    q = p;
    q.Add("/");
    File::Subst(name, q.text, true);
}

ResourceNamelessDatabase::~ResourceNamelessDatabase() {}  // 0x100063a0

}  // namespace Jet
