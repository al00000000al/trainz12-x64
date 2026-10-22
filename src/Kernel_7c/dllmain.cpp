// Kernel_7c.dll replacement: module attach/detach (0x10004ed0 / 0x10004dd0 / 0x10004f10). Like the original, the
// work runs under Jet's crash handler: attach builds the TaskManager, the "Log" task and the TimeEventSystem;
// detach tears them down in reverse.
#include <jet/Task.h>

#include <windows.h>

#include <new>

namespace Jet {

static TaskManager* g_taskManager = nullptr;  // 0x10019c20
static Task* g_logTask = nullptr;             // 0x10019ce8
static Events* g_timeEvents = nullptr;        // 0x10019c30

TaskManager* __cdecl GetTaskManager() { return g_taskManager; }  // 0x10004f00

// ---- the "Log" task: starts Jet's log from the options once the TaskManager runs (0x10004c60, 0x10005780)
class LogTask : public Task {
public:
    LogTask() : Task(PString("Log"), 0.0f, false) {}

protected:
    bool __cdecl Init(OS* os) override
    {
        Log::Init(os->GetOptions());
        return Task::Init(os);  // marks it initialised
    }
};

// ---- TimeEventSystem (0x1000d610, 0x1000d720, 0x1000d690)
static void CreateTimeEvents()
{
    if (!g_timeEvents) {
        void* mem = Alloc::New(sizeof(Events));
        g_timeEvents = mem ? new (mem) Events : nullptr;
    }
}

TimeEventSystem& __cdecl TimeEventSystem::Get()
{
    CreateTimeEvents();
    return *reinterpret_cast<TimeEventSystem*>(g_timeEvents);
}

// The original empties the three event lists by hand (lists {first, last, count} at +8/+0x14/+0x20, items
// linked through +4) before ~Events. These are 32-bit offsets into a KernelStandard object; an x64 KernelStandard
// must empty its lists in ~Events itself, so the x64 build skips the walk.
static void DestroyTimeEvents()
{
    Events* e = g_timeEvents;
    if (!e) return;
#if !defined(_WIN64)
    unsigned char* raw = reinterpret_cast<unsigned char*>(e);
    static const unsigned offsets[] = {0x8, 0x14, 0x20};
    for (unsigned off : offsets) {
        auto** first = reinterpret_cast<unsigned char**>(raw + off);
        while (*first) {
            unsigned char* item = *first;
            unsigned char* link = *reinterpret_cast<unsigned char**>(item + 4);
            *first = link ? link - 4 : nullptr;
            *reinterpret_cast<void**>(item + 4) = nullptr;
            *reinterpret_cast<void**>(item + 8) = nullptr;
        }
        *reinterpret_cast<void**>(raw + off + 4) = nullptr;
        *reinterpret_cast<void**>(raw + off + 8) = nullptr;
    }
#endif
    e->~Events();
    Alloc::Delete(e);
    g_timeEvents = nullptr;
}

// ---- interned names the original module keeps (0x10005380; header statics of the camera/scene code)
static PString g_names[21];
static void InitNames()
{
    static const char* const texts[21] = {
        "OnMinZoom", "OnMaxZoom", "OnMinPitch", "OnMaxPitch", "OnMinYaw", "OnMaxYaw", "OnMinRoll",
        "OnMaxRoll", "OnMinTargetPitch", "OnMaxTargetPitch", "OnMinTargetYaw", "OnMaxTargetYaw", "undefined",
        "default", "MCP", "NULL", "BaseLandscape", "Sky", "Main_void", "UserInterface", ""};
    for (int i = 0; i < 21; ++i) g_names[i] = PString(texts[i]);
}

static bool Attach()  // 0x10004f10 (3)
{
    InitNames();
    g_taskManager = new TaskManager;  // also becomes Task::taskManager
    g_logTask = new LogTask;
    CreateTimeEvents();
    return true;
}

static void Detach()  // 0x10004dd0 (0)
{
    DestroyTimeEvents();
    delete g_logTask;
    g_logTask = nullptr;
    delete g_taskManager;
    g_taskManager = nullptr;
}

struct ModuleEvent {
    HINSTANCE instance;
    DWORD reason;
};

static int __cdecl OnModuleEvent(void* arg)
{
    switch (static_cast<ModuleEvent*>(arg)->reason) {
    case DLL_PROCESS_DETACH:
        Detach();
        return 1;
    case DLL_PROCESS_ATTACH:
        return Attach() ? 1 : 0;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        return 1;
    default:
        return 0;
    }
}

#if defined(JET_KS_STATIC)
// Linked statically (the x64 harness) there is no DllMain: the harness calls these first and last in main, where
// the DLL would have been attached (after all statics) and detached.
void StaticModuleAttach()
{
    ModuleEvent e{GetModuleHandleA(nullptr), DLL_PROCESS_ATTACH};
    ExceptionHandler::Main(OnModuleEvent, &e, nullptr);
}

void StaticModuleDetach()
{
    ModuleEvent e{GetModuleHandleA(nullptr), DLL_PROCESS_DETACH};
    ExceptionHandler::Main(OnModuleEvent, &e, nullptr);
}
#endif

}  // namespace Jet

#if !defined(JET_KS_STATIC)
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    Jet::ModuleEvent e{instance, reason};
    return Jet::ExceptionHandler::Main(Jet::OnModuleEvent, &e, nullptr);
}
#endif
