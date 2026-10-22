// Unit U4 of KernelStandard_7c: SyncObject, Mutex, Semaphore, WEvent, CriticalSection, SingleLock, MultiLock,
// SWMRGuard (0x1000e680..0x1000e840, 0x1003f810..0x100404da). Thin Win32 wrappers; behaviour follows the TS12
// assembly, including its return values (Lock succeeds only on WAIT_OBJECT_0, MultiLock's result codes).
#include <jet/KernelStandard.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

// ---- SyncObject (0x100403d0 ctor, 0x1000e680 copy, 0x1000e7d0 dtor)

SyncObject::SyncObject() : handle(nullptr) {}
SyncObject::SyncObject(const SyncObject&) {}  // the original stores only the vftable
void SyncObject::operator=(const SyncObject&) {}

SyncObject::~SyncObject()
{
    if (handle) {
        CloseHandle(handle);
        handle = nullptr;
    }
}

__cdecl SyncObject::operator void*() { return handle; }
bool __cdecl SyncObject::Lock(unsigned long timeout) const { return WaitForSingleObject(handle, timeout) == WAIT_OBJECT_0; }
bool __cdecl SyncObject::Unlock() const { return false; }
bool __cdecl SyncObject::Unlock(unsigned long) const { return false; }

bool __cdecl SyncObject::IsLocked()  // 0x1003fda0
{
    if (Lock(0)) {
        Unlock();
        return false;
    }
    return true;
}

bool __cdecl SyncObject::Signal() { return Unlock(); }
bool __cdecl SyncObject::Wait(unsigned long timeout) const { return Lock(timeout); }

// ---- Mutex (0x1003ffb0), Semaphore (0x10040000), WEvent (0x10040330..0x1004038f)

Mutex::Mutex(bool initiallyOwned, const char* name) { handle = CreateMutexA(nullptr, initiallyOwned, name); }
Mutex::~Mutex() {}
bool __cdecl Mutex::Unlock() const { return ReleaseMutex(handle) != 0; }
bool __cdecl Mutex::Unlock(unsigned long) const { return ReleaseMutex(handle) != 0; }

Semaphore::Semaphore(int initialCount, int maxCount, const char* name)
{
    handle = CreateSemaphoreA(nullptr, initialCount, maxCount, name);
}
Semaphore::~Semaphore() {}
bool __cdecl Semaphore::Unlock() const { return ReleaseSemaphore(handle, 1, nullptr) != 0; }
bool __cdecl Semaphore::Unlock(unsigned long count) const { return ReleaseSemaphore(handle, static_cast<LONG>(count), nullptr) != 0; }

WEvent::WEvent(bool manualReset, bool initialState, const char* name)
{
    handle = CreateEventA(nullptr, manualReset, initialState, name);
}
WEvent::~WEvent() {}
bool __cdecl WEvent::Unlock() const { return true; }
void __cdecl WEvent::Set() { SetEvent(handle); }
void __cdecl WEvent::Pulse() { PulseEvent(handle); }
void __cdecl WEvent::Reset() { ResetEvent(handle); }

// ---- CriticalSection (0x100403e0..0x100404da)

CriticalSection::CriticalSection(const char*)
{
    section = Alloc::New(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(static_cast<CRITICAL_SECTION*>(section));
}

CriticalSection::~CriticalSection()
{
    if (section) DeleteCriticalSection(static_cast<CRITICAL_SECTION*>(section));
    if (section) Alloc::Delete(section);
    section = nullptr;
}

bool __cdecl CriticalSection::Lock(unsigned long timeout) const
{
    auto* cs = static_cast<CRITICAL_SECTION*>(section);
    if (!cs) return true;
    if (timeout != INFINITE) return TryEnterCriticalSection(cs) != 0;
    EnterCriticalSection(cs);
    return true;
}

bool __cdecl CriticalSection::Unlock() const
{
    if (section) LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(section));
    return true;
}

// ---- SingleLock (0x1003fcf0..0x1003fd99, 0x1000e830)

SingleLock::SingleLock(const SyncObject* o, bool lockNow) : object(o), locked(false)
{
    if (lockNow) locked = object->Lock(INFINITE);
}

SingleLock::~SingleLock()
{
    if (locked) locked = !object->Unlock();
}

SingleLock& SingleLock::operator=(const SingleLock& o)  // 0x10014f70: two whole dwords, padding included
{
    std::memcpy(static_cast<void*>(this), &o, sizeof *this);
    return *this;
}

bool __cdecl SingleLock::Lock(unsigned long timeout) const
{
    if (!locked) locked = object->Lock(timeout);
    return locked;
}

bool __cdecl SingleLock::Unlock() const
{
    if (locked) locked = !object->Unlock();
    return !locked;
}

bool __cdecl SingleLock::IsLocked() const { return locked; }

// ---- MultiLock (0x1003fdf0..0x1003ffa1, 0x1000e840)

static void* AllocArray(unsigned long count, unsigned long size)  // the original's overflow-checked new[]
{
    const unsigned long long bytes = static_cast<unsigned long long>(count) * size;
    return Alloc::New(bytes > 0xffffffffull ? 0xffffffffu : static_cast<unsigned int>(bytes));
}

MultiLock::MultiLock(SyncObject** objs, unsigned long n, bool lockNow) : count(n), objects(objs)
{
    handles = static_cast<void**>(AllocArray(n, sizeof(void*)));
    locked = static_cast<bool*>(Alloc::New(n));
    for (unsigned long i = 0; i < n; ++i) {
        handles[i] = static_cast<void*>(*objects[i]);
        locked[i] = false;
    }
    if (lockNow) {
        const DWORD r = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
        if (r != WAIT_TIMEOUT && r < count) locked[r] = true;
    }
}

MultiLock::~MultiLock()
{
    Unlock();
    if (handles) Alloc::Delete(handles);
    handles = nullptr;
    if (locked) Alloc::Delete(locked);
    locked = nullptr;
}

MultiLock& MultiLock::operator=(const MultiLock& o)
{
    handles = o.handles;
    locked = o.locked;
    count = o.count;
    objects = o.objects;
    return *this;
}

MultiLock::Result __cdecl MultiLock::Lock(unsigned long timeout, bool waitAll) const
{
    const DWORD r = WaitForMultipleObjects(count, handles, waitAll, timeout);
    if (r == WAIT_TIMEOUT) return TIMEOUT;
    if (r < count) {
        if (waitAll)
            for (unsigned long i = 0; i < count; ++i) locked[i] = true;
        else
            locked[r] = true;
        return static_cast<Result>(r + ACQUIRED);
    }
    if (r >= WAIT_ABANDONED_0 && r < count + WAIT_ABANDONED_0) return static_cast<Result>(r - WAIT_ABANDONED_0 + ABANDONED);
    return FAILED;
}

bool __cdecl MultiLock::Unlock() const
{
    for (unsigned long i = 0; i < count; ++i)
        if (locked[i]) locked[i] = !objects[i]->Unlock();
    return true;
}

bool __cdecl MultiLock::Unlock(unsigned long) const { return false; }
bool __cdecl MultiLock::IsLocked(unsigned long index) const { return locked[index]; }

// ---- SWMRGuard (0x1003f810..0x1003facf)

SWMRGuard::SWMRGuard() : noReaders(true, true, nullptr), writer(false, nullptr), readers(0)
{
    both[0] = &writer;
    both[1] = &noReaders;
    lock = new MultiLock(both, 2, false);
}

SWMRGuard::~SWMRGuard()
{
    if (lock) delete lock;
}

bool __cdecl SWMRGuard::WaitToWrite(unsigned long timeout)
{
    return lock && lock->Lock(timeout, true) == MultiLock::ACQUIRED;
}

void SWMRGuard::DoneWriting() { writer.Unlock(); }

bool __cdecl SWMRGuard::WaitToRead(unsigned long timeout)
{
    if (!writer.Lock(timeout)) return false;
    if (readers == 0) ResetEvent(static_cast<void*>(noReaders));
    ++readers;
    writer.Unlock();
    return true;
}

void SWMRGuard::DoneReading()
{
    writer.Lock(INFINITE);
    if (readers) --readers;
    if (readers == 0) SetEvent(static_cast<void*>(noReaders));
    writer.Unlock();
}

// ---- default constructor closures (exported under ??_F<class> through implemented.txt)

void __fastcall Jet_CriticalSectionDefaultCtor(CriticalSection* p) { ::new (p) CriticalSection(); }
void __fastcall Jet_MutexDefaultCtor(Mutex* p) { ::new (p) Mutex(); }
void __fastcall Jet_SemaphoreDefaultCtor(Semaphore* p) { ::new (p) Semaphore(); }
void __fastcall Jet_WEventDefaultCtor(WEvent* p) { ::new (p) WEvent(); }

}  // namespace Jet
