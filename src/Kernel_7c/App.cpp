// Jet::App and Jet::EntryPoint (Kernel_7c 0x10001000-0x10001500, 0x10004d30-0x10004dc0).
#include <jet/Task.h>

#include <new>

namespace Jet {

TaskManager* __cdecl GetTaskManager();

JET_LAYOUT32(sizeof(App) == 0xc);

App::App() : os(nullptr) {}  // 0x100010c0 (+8 is left as it is)
App::App(const App& o) : os(o.os), reserved(o.reserved) {}

App& App::operator=(const App& o)
{
    os = o.os;
    reserved = o.reserved;
    return *this;
}

static void DeleteOS(OS* os)
{
    if (os) {
        os->~OS();
        Alloc::Delete(os);
    }
}

App::~App()  // 0x10001330
{
    DeleteOS(os);
    os = nullptr;
}

const char* __cdecl App::GetAppName() const { return os ? os->GetAppName() : nullptr; }
OS* __cdecl App::GetOS() const { return os; }

// 0x10001380
bool __cdecl App::Init(const char* appName, const char* commandLine, void* instance, HWND__* window)
{
    void* mem = Alloc::New(sizeof(OS));
    os = mem ? new (mem) OS(appName, commandLine, instance, window) : nullptr;
    if (os && os->Init()) return true;
    DeleteOS(os);
    os = nullptr;
    return false;
}

int __cdecl App::Main()  // 0x10001450
{
    void* handle = ExceptionHandler::AddCriticalShutdown(CriticalShutdownCallBack, this);
    const int result = ExceptionHandler::Main(MainCallBack, this, nullptr);
    ExceptionHandler::RemoveCriticalShutdown(handle);
    return result;
}

int __cdecl App::ProtectedMain()  // 0x10001490
{
    if (!os) return -1;
    return GetTaskManager()->Main(os);
}

int __cdecl App::CriticalShutdown() { return 0; }
int __cdecl App::MainCallBack(void* app) { return static_cast<App*>(app)->ProtectedMain(); }
int __cdecl App::CriticalShutdownCallBack(void* app) { return static_cast<App*>(app)->CriticalShutdown(); }

// ---------------------------------------------------------------------------------------------------------------
// EntryPoint: an interface with TS12 defaults (both slots share the original's "return 0")

EntryPoint::EntryPoint() {}
EntryPoint::EntryPoint(const EntryPoint&) {}
EntryPoint& EntryPoint::operator=(const EntryPoint&) { return *this; }
EntryPoint::~EntryPoint() {}
bool __cdecl EntryPoint::Init(OS*) { return false; }
void __cdecl EntryPoint::Done() {}

}  // namespace Jet
