// Unit U6 of KernelStandard_7c: DynamicLibrary, Events / Events::Event, SharedMemory, SharedScratchPad, Job, Jobs
// (0x1001bd00..0x1001ca90, 0x1002d4d0..0x1002dfd0, 0x1003d410..0x1003da64, plus a few folded stubs). Notes:
// notes/KernelStandard.md G13. Behaviour follows the TS12 assembly, quirks included (see the comments).
#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>

#include <windows.h>

#include <cstddef>
#include <initializer_list>
#include <new>

namespace Jet {

// ---- the intrusive lists of Events and Jobs (0x1001ca10 unlink, 0x1001c950 sorted insert; appends inlined)
template <class T>
struct ListOps {
    static T* Of(ListLink* l) { return l ? reinterpret_cast<T*>(reinterpret_cast<char*>(l) - offsetof(T, link)) : nullptr; }

    static void Unlink(LinkedList<T>& list, T* e)
    {
        ListLink* const prev = e->link.prev;
        ListLink* const next = e->link.next;
        if (prev)
            prev->next = next;
        else
            list.head = Of(next);
        if (next)
            next->prev = prev;
        else
            list.tail = Of(prev);
        e->link.next = e->link.prev = nullptr;
        --list.count;
    }

    static void Append(LinkedList<T>& list, T* e)
    {
        if (list.tail) {
            e->link.prev = &list.tail->link;
            list.tail->link.next = &e->link;
        } else {
            list.head = e;
        }
        ++list.count;
        list.tail = e;
    }

    // before the first one due no earlier (ties: the new one goes first)
    static void InsertSorted(LinkedList<T>& list, T* e)
    {
        T* at = list.head;
        while (at && at->nextTime < e->nextTime) at = Of(at->link.next);
        if (!e) return;
        if (!at) {
            Append(list, e);
            return;
        }
        if (at == list.head) {
            e->link.next = &at->link;
            at->link.prev = &e->link;
            ++list.count;
            list.head = e;
            return;
        }
        e->link.next = &at->link;
        e->link.prev = at->link.prev;
        at->link.prev->next = &e->link;
        at->link.prev = &e->link;
        ++list.count;
    }

    static bool Contains(const LinkedList<T>& list, const T* e)
    {
        for (T* i = list.head; i; i = Of(i->link.next))
            if (i == e) return true;
        return false;
    }
};

// ---- DynamicLibrary (0x1001bd00..0x1001be3e)
DynamicLibrary::DynamicLibrary() : handle(nullptr) {}
DynamicLibrary::DynamicLibrary(const DynamicLibrary& o) : handle(o.handle) {}

DynamicLibrary& DynamicLibrary::operator=(const DynamicLibrary& o)
{
    handle = o.handle;
    return *this;
}

DynamicLibrary::~DynamicLibrary()
{
    if (handle) {
        FreeLibrary(static_cast<HMODULE>(handle));
        handle = nullptr;
    }
}

bool __cdecl DynamicLibrary::Open(const char* path)
{
    if (handle) Close();
    handle = LoadLibraryExA(path, nullptr, 0);
    return handle != nullptr;
}

void __cdecl DynamicLibrary::Close()
{
    if (handle) {
        FreeLibrary(static_cast<HMODULE>(handle));
        handle = nullptr;
    }
}

bool __cdecl DynamicLibrary::Import(void** proc, const char* name)
{
    *proc = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
    return *proc != nullptr;
}

bool __cdecl DynamicLibrary::GetModuleNameAndAddress(void*, String&, void*&) { return false; }  // 0x10014fe0

// ---- Events::Event (0x1001c510..0x1001c600, 0x10003940/50)
Events::Event::Event(unsigned long interval_, unsigned long nextTime_)
    : interval(interval_), id(0), nextTime(nextTime_)
{
    link.next = link.prev = nullptr;
}

// (the copies take the links too)
Events::Event::Event(const Event& o) : link(o.link), interval(o.interval), id(o.id), nextTime(o.nextTime) {}

Events::Event& Events::Event::operator=(const Event& o)
{
    link = o.link;
    interval = o.interval;
    id = o.id;
    nextTime = o.nextTime;
    return *this;
}

int Events::Event::operator<(const Event& o) const { return nextTime < o.nextTime; }

bool Events::Event::Update()
{
    nextTime += interval;
    return true;
}

unsigned long Events::Event::GetId() const { return id; }
unsigned long Events::Event::GetNextTime() const { return nextTime; }
void Events::Event::SetId(unsigned long i) { id = i; }
void Events::Event::UpdateNext() { nextTime += interval; }

// ---- Events (0x1001c490..0x1001c93f)
Events::Events() : Events(true) {}

Events::Events(bool owns) : ownsEvents(owns), lastId(0), removed{}, queue{}, requeue{} {}

Events::~Events()
{
    for (LinkedList<Event>* list : {&requeue, &queue, &removed})
        while (Event* e = list->head) {
            ListOps<Event>::Unlink(*list, e);
            Alloc::Delete(e);
        }
}

Events& Events::operator=(const Events& o) = default;

void Events::GenerateId(Event* e)
{
    lastId = (lastId + 1) & 0x7fffffff;
    e->id = lastId;
}

void Events::Add(Event* e) { ListOps<Event>::InsertSorted(queue, e); }

// the ones removed by id go; those due are updated in time order (the queue head each time, so an update may
// add or remove others); a finished one leaves the queue (deleted when owned), another goes back in after the pass
void Events::Update(unsigned long time)
{
    while (Event* e = removed.head) {
        ListOps<Event>::Unlink(removed, e);
        Alloc::Delete(e);
    }
    while (Event* e = queue.head) {
        if (e->nextTime > time) break;
        const bool finished = e->Update();
        if (!ListOps<Event>::Contains(queue, e)) continue;
        ListOps<Event>::Unlink(queue, e);
        if (!finished)
            ListOps<Event>::Append(requeue, e);
        else if (ownsEvents)
            Alloc::Delete(e);
    }
    while (requeue.count) {
        Event* const e = requeue.head;
        if (e) ListOps<Event>::Unlink(requeue, e);
        ListOps<Event>::InsertSorted(queue, e);
    }
}

bool Events::RemoveEventSafely(Event* e)
{
    if (!ListOps<Event>::Contains(queue, e)) return false;
    ListOps<Event>::Unlink(queue, e);
    return true;
}

// an owned event goes to the removed list (deleted by the next Update), so it may remove itself in its Update
bool Events::Remove(unsigned long id)
{
    for (LinkedList<Event>* list : {&queue, &requeue})
        for (Event* e = list->head; e; e = ListOps<Event>::Of(e->link.next)) {
            if (e->GetId() != id) continue;
            ListOps<Event>::Unlink(*list, e);
            if (ownsEvents) ListOps<Event>::InsertSorted(removed, e);
            return true;
        }
    return false;
}

void Events::Remove(Event* e)
{
    for (LinkedList<Event>* list : {&queue, &requeue})
        if (ListOps<Event>::Contains(*list, e)) {
            ListOps<Event>::Unlink(*list, e);
            return;
        }
}

// ---- SharedMemory (0x1003d410..0x1003d4ea, 0x10014d90, 0x1003cc90; TryExclusiveLock / Unlock are folded stubs)
SharedMemory::SharedMemory(const char* name_, unsigned int size_) : data(nullptr), name(name_), size(size_), mapping(nullptr) {}

SharedMemory::~SharedMemory()
{
    if (data) {
        if (!UnmapViewOfFile(data)) return;
        data = nullptr;
    }
    if (mapping && CloseHandle(mapping)) mapping = nullptr;
}

SharedMemory& SharedMemory::operator=(const SharedMemory& o) = default;

bool SharedMemory::ProcessAttach()
{
    mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, name);
    if (!mapping) return false;
    data = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, 0);
    return data != nullptr;
}

bool SharedMemory::ProcessDetach()
{
    if (data) {
        if (!UnmapViewOfFile(data)) return false;
        data = nullptr;
    }
    if (mapping) {
        if (!CloseHandle(mapping)) return false;
        mapping = nullptr;
    }
    return true;
}

void* SharedMemory::Data() { return data; }
bool SharedMemory::TryExclusiveLock() { return false; }
void SharedMemory::Unlock() {}

// ---- SharedScratchPad (0x1003d550..0x1003d825; the array helpers 0x1003d830..0x1003da64)
CriticalSection SharedScratchPad::lock;
FastDynamicArray<SharedScratchPad*, 13, Less<SharedScratchPad*, SharedScratchPad*>> SharedScratchPad::scratchPadArray;

SharedScratchPad::SharedScratchPad(unsigned long id) : threadId(id), buffer(nullptr), capacity(0)
{
    const bool locked = lock.Lock(~0ul);
    scratchPadArray.Add(this);
    if (locked) lock.Unlock();
}

SharedScratchPad::~SharedScratchPad()
{
    const bool locked = lock.Lock(~0ul);
    const auto i = scratchPadArray.Find(this);
    if (i.node) scratchPadArray.Remove(i);
    if (buffer) Alloc::Delete(buffer);
    buffer = nullptr;
    if (locked) lock.Unlock();
}

unsigned char* __cdecl SharedScratchPad::GetBuffer(unsigned int size)
{
    if (size > capacity) {
        if (buffer) Alloc::Delete(buffer);
        buffer = nullptr;
        buffer = static_cast<unsigned char*>(Alloc::New(size));
        capacity = size;
    }
    return buffer;
}

SharedScratchPad* __cdecl SharedScratchPad::GetAccess()
{
    const unsigned long id = GetCurrentThreadId();
    const bool locked = lock.Lock(~0ul);
    SharedScratchPad* pad = nullptr;
    for (auto i = scratchPadArray.First(); i.node; ++i)
        if ((*i.node)->threadId == id) {
            pad = *i.node;
            pad->AddReference();
            break;
        }
    if (!pad) {
        void* const mem = Alloc::New(sizeof(SharedScratchPad));
        pad = mem ? ::new (mem) SharedScratchPad(id) : nullptr;
    }
    if (locked) lock.Unlock();
    return pad;
}

// ---- Job (0x1002d4d0..0x1002d5d8, 0x1002de80..0x1002def6, 0x10008570, 0x1002abe0)
Job::Job(Job* p) : progress(0.0f), parent(p), locks(0)
{
    link.next = link.prev = nullptr;
    const unsigned long now = timeGetTime();
    created = ended = started = unknown24 = now;
    if (p) ++p->locks;
}

// (the copies take the links too)
Job::Job(const Job& o)
    : link(o.link), progress(o.progress), parent(o.parent), locks(o.locks), created(o.created), started(o.started),
      ended(o.ended), unknown24(o.unknown24)
{
}

Job& Job::operator=(const Job& o)
{
    link = o.link;
    progress = o.progress;
    parent = o.parent;
    locks = o.locks;
    created = o.created;
    started = o.started;
    ended = o.ended;
    unknown24 = o.unknown24;
    return *this;
}

Job::~Job() {}
const char* Job::GetName() const { return "Unknown"; }
float Job::GetProgress() const { return progress; }
void Job::SetProgress(float p) { progress = p; }
void Job::Lock() { ++locks; }
void Job::Unlock() { --locks; }
bool Job::IsDone() const { return locks == 0; }
void __cdecl Job::Process() {}

void __cdecl Job::Dispatch()
{
    if (parent) --parent->locks;
}

void __cdecl Job::StartProcessing() { started = timeGetTime(); }
void __cdecl Job::EndProcessing() { ended = timeGetTime(); }

// the default constructor closure (??_FJob@Jet@@QAEXXZ, thiscall)
void __fastcall JobDefaultConstructor(Job* self) { ::new (self) Job(); }

// ---- Jobs (0x1002d680..0x1002dfd0)
static unsigned char* const kMonoScreen = reinterpret_cast<unsigned char*>(0xb0000);

Jobs::Jobs()
    : unknown00(0), unknown04(0), queue{}, queueLock(false, nullptr), queued(0, 0x7fffffff, nullptr), finished{},
      finishedLock(false, nullptr), finishedCount(0, 0x7fffffff, nullptr), pending{}, consoleOn(false), active(false)
{
    console.buffer = nullptr;
    console.spans = nullptr;
    queueConsole.buffer = nullptr;
    queueConsole.spans = nullptr;
    finishedConsole.buffer = nullptr;
    finishedConsole.spans = nullptr;
}

void __cdecl Jobs::Init(Options* options)
{
    options->Get("JobsConsole", consoleOn, false);
    if (consoleOn) {
        console.width = 0x28;
        console.height = 10;
        console.buffer = static_cast<unsigned char*>(Alloc::New(0x190));
        console.Clear();
        queueConsole.width = 0x19;
        queueConsole.height = 10;
        queueConsole.buffer = static_cast<unsigned char*>(Alloc::New(0xfa));
        queueConsole.Clear();
        finishedConsole.width = 0x19;
        finishedConsole.height = 10;
        finishedConsole.buffer = static_cast<unsigned char*>(Alloc::New(0xfa));
        finishedConsole.Clear();
        console.BlitToMono44(0, 0x1c, kMonoScreen);
        finishedConsole.BlitToMono44(0x41, 0x1c, kMonoScreen);
        queueConsole.BlitToMono44(0x28, 0x1c, kMonoScreen);
    }
    void* const mem = Alloc::New(sizeof(Thread));
    thread = mem ? ::new (mem) Thread(&Main, this, true) : nullptr;
    thread->SetPriority(Thread::PRIORITY_1);
}

void __cdecl Jobs::Done()
{
    if (consoleOn) {
        console.Printf("Terminating Thread...\n");
        console.BlitToMono44(0, 0x1c, kMonoScreen);
    }
    if (thread) delete thread;  // (the pointer stays)
    if (consoleOn) {
        console.Printf("Jobs Done.\n");
        console.BlitToMono44(0, 0x1c, kMonoScreen);
        for (Console* c : {&console, &queueConsole, &finishedConsole})
            if (c->buffer) {
                Alloc::Delete(c->buffer);
                c->buffer = nullptr;
            }
    }
}

bool Jobs::IsActive() const { return active; }

// the finished jobs are dispatched and deleted, on a finished signal only (one wait per call)
void __cdecl Jobs::Update()
{
    if (!finishedCount.Lock(1)) return;
    if (!finishedLock.Lock(5)) return;
    while (Job* job = finished.head) {
        ListOps<Job>::Unlink(finished, job);
        job->Dispatch();
        ShowFinished();
        delete job;
    }
    finishedLock.Unlock();
}

void __cdecl Jobs::Add(Job* job)
{
    queueLock.Lock(~0ul);
    if (job) ListOps<Job>::Append(queue, job);
    queueLock.Unlock();
    ShowQueue();
    queued.Unlock();
}

unsigned int __stdcall Jobs::Main(void* jobs)
{
    static_cast<Jobs*>(jobs)->Run();
    return 0;
}

// waits for a queued job or the thread's end; a processed job with children pending waits for them, then every
// waiting one whose children are all dispatched is finished too
void Jobs::Run()
{
    if (consoleOn) {
        console.Printf("Jobs Started!\n");
        console.BlitToMono44(0, 0x1c, kMonoScreen);
        ShowFinished();
        ShowQueue();
    }
    SyncObject* objects[2] = {&queued, thread};
    MultiLock wait(objects, 2, false);
    for (;;) {
        active = false;
        if (consoleOn && queued.Lock(0)) {
            queued.Unlock();
            ShowFinished();
            ShowQueue();
            console.Printf("Waiting...\n");
            console.BlitToMono44(0, 0x1c, kMonoScreen);
        }
        if (wait.Lock(INFINITE, false) != MultiLock::ACQUIRED) break;  // the thread ending, or a wait failing
        queueLock.Lock(~0ul);
        Job* const job = queue.head;
        if (job) ListOps<Job>::Unlink(queue, job);
        queueLock.Unlock();
        active = true;
        if (consoleOn) {
            ShowQueue();
            console.Printf("Processing Job: %s\n", job->GetName());
            console.BlitToMono44(0, 0x1c, kMonoScreen);
        }
        job->started = timeGetTime();
        job->Process();
        job->ended = timeGetTime();
        if (job->locks == 0) {
            Finish(job);
        } else {
            if (consoleOn) {
                console.Printf("Job Pending: %s\n", job->GetName());
                console.BlitToMono44(0, 0x1c, kMonoScreen);
            }
            ListOps<Job>::Append(pending, job);
        }
        for (Job* j = pending.head; j;) {
            Job* const next = ListOps<Job>::Of(j->link.next);
            if (j->locks == 0) {
                ListOps<Job>::Unlink(pending, j);
                Finish(j);
            }
            j = next;
        }
    }
    if (consoleOn) {
        console.Printf("Job Thread Ended.\n");
        console.BlitToMono44(0, 0x1c, kMonoScreen);
    }
}

void Jobs::Finish(Job* job)  // 0x1002dde0
{
    if (consoleOn) {
        console.Printf("Job Complete: %s (%lu ms)\n", job->GetName(), job->ended - job->started);
        console.BlitToMono44(0, 0x1c, kMonoScreen);
        ShowFinished();
    }
    finishedLock.Lock(~0ul);
    if (job) ListOps<Job>::Append(finished, job);
    finishedLock.Unlock();
    finishedCount.Unlock();
}

void Jobs::ShowFinished()  // 0x1002df00
{
    if (!consoleOn) return;
    finishedConsole.Clear();
    int n = 0;
    for (Job* j = finished.head; j; j = ListOps<Job>::Of(j->link.next)) finishedConsole.Printf("%d. %s\n", n++, j->GetName());
    finishedConsole.BlitToMono44(0x41, 0x1c, kMonoScreen);
}

void Jobs::ShowQueue()  // 0x1002df70
{
    if (!consoleOn) return;
    queueConsole.Clear();
    int n = 0;
    for (Job* j = queue.head; j; j = ListOps<Job>::Of(j->link.next)) queueConsole.Printf("%d. %s\n", n++, j->GetName());
    queueConsole.BlitToMono44(0x28, 0x1c, kMonoScreen);
}

}  // namespace Jet
