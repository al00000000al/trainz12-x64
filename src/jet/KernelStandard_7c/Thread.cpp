// Unit U13 of KernelStandard_7c: Thread (0x1003fc90, 0x10040070..0x10040530, 0x1000e7f0..0x1000e820) and
// ThreadLocalStorage (0x1002dfe0..0x1002e030, 0x10040540..0x10040af0, helpers 0x1002ecf0 / 0x1002edc0).
// mainThreadId, the TLS singleton and its index statics are still the original's (forwarded data): its
// KernelStandardInit sets them up and its Profile reads the indices.
#include <jet/KernelStandard.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

// ---- Thread

Thread::Thread(Function fn_, void* arg_, bool start) : done(false, false, nullptr), alive(true), fn(fn_), arg(arg_)
{
    handle = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(&Start), this,
                          start ? 0 : CREATE_SUSPENDED, &id);
}

// wakes the thread with `done` and waits for its end; the thread handle is dropped, not closed (as the original)
Thread::~Thread()
{
    if (handle) {
        done.Set();
        ResumeThread(handle);
        Lock(INFINITE);
        DWORD code = 0;
        GetExitCodeThread(handle, &code);
    }
    handle = nullptr;
}

unsigned long __cdecl Thread::Terminate()
{
    if (!handle) return 0xffffffff;
    done.Set();
    ResumeThread(handle);
    Lock(INFINITE);
    DWORD code = 0;
    GetExitCodeThread(handle, &code);
    return code;
}

__cdecl Thread::operator void*() { return done.operator void*(); }
bool __cdecl Thread::Resume() { return ResumeThread(handle) != 0xffffffff; }
bool __cdecl Thread::Suspend() { return SuspendThread(handle) != 0xffffffff; }
void __cdecl Thread::SetPriority(Priority p) { SetThreadPriority(handle, p); }
Thread::Priority __cdecl Thread::GetPriority() const { return static_cast<Priority>(GetThreadPriority(handle)); }
bool __cdecl Thread::StartedOk() const { return handle != nullptr; }
bool __cdecl Thread::IsAlive() const { return alive; }
unsigned long __cdecl Thread::GetThreadID() { return id; }

bool __cdecl Thread::IsDone()
{
    if (done.Lock(0)) {
        done.Unlock();
        return true;
    }
    return false;
}

void __cdecl Thread::SetDone() { done.Set(); }

unsigned long __stdcall Thread::Start(void* thread)
{
    const unsigned long r = static_cast<unsigned long>(ExceptionHandler::Main(&SafeStart, thread, nullptr));
    static_cast<Thread*>(thread)->alive = false;
    return r;
}

int __cdecl Thread::SafeStart(void* thread)
{
    Thread* const t = static_cast<Thread*>(thread);
    return static_cast<int>(t->fn(t->arg));
}

unsigned long __cdecl Thread::GetCurrentId() { return GetCurrentThreadId(); }
unsigned long __cdecl Thread::GetProcessId() { return GetCurrentProcessId(); }
unsigned long __cdecl Thread::GetMainThreadId() { return mainThreadId; }
void __cdecl Thread::GetProcessName(char* out) { GetModuleFileNameA(nullptr, out, 0x100); }
void __cdecl Thread::GlobalInit() { mainThreadId = GetCurrentThreadId(); }

bool __cdecl Thread::IsProcessAlive(unsigned long processId)
{
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (!p) return false;
    DWORD code;
    GetExitCodeProcess(p, &code);
    CloseHandle(p);
    return code == STILL_ACTIVE;
}

void __cdecl Thread::TerminateProcess(unsigned long processId)
{
    if (!processId) return;
    HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (!p) return;
    ::TerminateProcess(p, 1);
    CloseHandle(p);
}

// ---- ThreadLocalStorage

struct TlsWork {
    using Entry = ThreadLocalStorage::Entry;

    // the original's inlined PString copy and release (the release never destroys the node)
    static PStringCache::Node* AddRef(PStringCache::Node* n)
    {
        if (n) {
            PStringCache* const cache = PString::Cache();
            cache->Lock();
            ++n->refs;
            ++cache->liveRefs;
            cache->Unlock();
        }
        return n;
    }
    static void Release(PStringCache::Node* n)
    {
        PStringCache* const cache = PString::Cache();
        if (!n || !cache) return;
        cache->Lock();
        if (n->refs == 1) --*reinterpret_cast<int*>(&cache->unknown0);
        --cache->liveRefs;
        --n->refs;
        cache->Unlock();
    }

    static void Clear(ThreadLocalStorage& t)  // 0x1002edc0
    {
        for (unsigned int i = 0; i < t.count; ++i) Release(t.entries[i].name);
        t.count = 0;
    }

    // 0x1002ecf0: the storage grows to the source's capacity only when it is smaller
    static void CopyFrom(ThreadLocalStorage& t, const ThreadLocalStorage& o)
    {
        Clear(t);
        if (!o.entries) return;
        if (t.capacity < o.capacity) {
            if (t.entries) Alloc::Delete(t.entries);
            t.entries = static_cast<Entry*>(Alloc::New(o.capacity * sizeof(Entry)));
            t.capacity = o.capacity;
        }
        t.count = o.count;
        for (unsigned int i = 0; i < t.count; ++i) {
            t.entries[i].name = AddRef(o.entries[i].name);
            t.entries[i].index = o.entries[i].index;
        }
    }

    static int Named(ThreadLocalStorage& t, const char* name, unsigned int length)
    {
        PString s;
        s.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(name), length);
        const int index = t.GetIndex(s);
        Release(s.node);
        s.node = nullptr;
        return index;
    }
};

ThreadLocalStorage::ThreadLocalStorage() : lock(new Mutex()), count(0), capacity(0), entries(nullptr)
{
    ThreadTlsIndex = TlsWork::Named(*this, "ThreadIndex", 0xb);
    ProfilePtTlsIndex = TlsWork::Named(*this, "ProfilePtIndex", 0xe);
    ProfileTraceTlsIndex = TlsWork::Named(*this, "ProfileTraceIndex", 0x11);
    ProfileRenderTlsIndex = TlsWork::Named(*this, "ProfileRenderIndex", 0x12);
}

ThreadLocalStorage::ThreadLocalStorage(const ThreadLocalStorage& o) : lock(o.lock), count(0), capacity(0), entries(nullptr)
{
    TlsWork::CopyFrom(*this, o);
}

ThreadLocalStorage& ThreadLocalStorage::operator=(const ThreadLocalStorage& o)
{
    lock = o.lock;
    TlsWork::CopyFrom(*this, o);
    return *this;
}

ThreadLocalStorage::~ThreadLocalStorage()
{
    lock->Lock(INFINITE);
    for (unsigned int i = 0; i < count; ++i) TlsFree(entries[i].index);
    lock->Unlock();
    delete lock;
    lock = nullptr;
    TlsWork::Clear(*this);
    if (entries) Alloc::Delete(entries);
}

int __cdecl ThreadLocalStorage::GetIndex(const PString& name)
{
    const bool locked = lock->Lock(INFINITE);
    for (unsigned int i = 0; i < count; ++i) {
        if (entries[i].name == name.node) {
            const int index = entries[i].index;
            if (locked) lock->Unlock();
            return index;
        }
    }
    const int index = static_cast<int>(TlsAlloc());
    PStringCache::Node* const n = TlsWork::AddRef(name.node);  // the original's temporary copy
    if (capacity < count + 1) {
        const unsigned int blocks = (count + 0x80) >> 7;
        Entry* const grown = static_cast<Entry*>(Alloc::New(blocks * 0x400));
        if (entries) {
            std::memcpy(grown, entries, count * sizeof(Entry));
            Alloc::Delete(entries);
        }
        entries = grown;
        capacity = blocks << 7;
    }
    entries[count].name = TlsWork::AddRef(n);
    entries[count].index = index;
    ++count;
    TlsWork::Release(n);
    if (locked) lock->Unlock();
    return index;
}

bool __cdecl ThreadLocalStorage::CreateTLS()
{
    void* const p = Alloc::New(sizeof(ThreadLocalStorage));
    singleton = p ? ::new (p) ThreadLocalStorage() : nullptr;
    return true;
}

void __cdecl ThreadLocalStorage::DestroyTLS()
{
    ThreadLocalStorage* const t = singleton;
    if (t) {
        t->~ThreadLocalStorage();
        Alloc::Delete(t);
    }
    singleton = nullptr;
}

void* __cdecl ThreadLocalStorage::GetThreadData(int index) { return TlsGetValue(index); }
void __cdecl ThreadLocalStorage::SetThreadData(int index, void* data) { TlsSetValue(index, data); }

ThreadLocalStorage& GetTLS() { return *ThreadLocalStorage::singleton; }

}  // namespace Jet
