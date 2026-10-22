// Drives the TaskManager through its external-control path (Start / Update / Stop) with scripted tasks and logs
// everything observable: update order, list flags, lookups, Init/Done calls. Linked against "Kernel_7c.dll" by
// name: run once next to the original module and once next to ours; the logs must be identical.
//
// usage: task_probe.exe <out.txt>   (run from the directory holding the modules)
#include <jet/Task.h>

#include "StaticKernel.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

using namespace Jet;

static FILE* out;

class Probe : public Task {
public:
    // script: per frame, what Update returns ('c' continue, 's' stop, 'x' exit), plus actions
    Probe(const char* name, float priority, bool updated, std::string script = "")
        : Task(PString(name), priority, updated), label(name), script(std::move(script)) {}
    ~Probe() override { std::fprintf(out, "  ~%s\n", label.c_str()); }

    std::string label, script;
    int frame = 0;
    bool failInit = false;
    Probe* suspendOther = nullptr;  // suspended on frame 1, resumed on frame 3
    void Suspend_() { Suspend(); }
    void Resume_() { Resume(); }

protected:
    bool __cdecl Init(OS* os) override
    {
        std::fprintf(out, "  init %s os=%d\n", label.c_str(), os != nullptr);
        if (failInit) return false;
        return Task::Init(os);
    }
    void __cdecl Done() override { std::fprintf(out, "  done %s\n", label.c_str()); }
    UpdateCode __cdecl Update(float dt) override
    {
        const char c = frame < static_cast<int>(script.size()) ? script[frame] : 'c';
        std::fprintf(out, "  update %s dt=%.3f f%d -> %c onlist=%d\n", label.c_str(), dt, frame, c, IsOnUpdateList());
        if (suspendOther && frame == 1) suspendOther->Suspend_();
        if (suspendOther && frame == 3) suspendOther->Resume_();
        ++frame;
        return c == 's' ? STOP : c == 'x' ? EXIT : CONTINUE;
    }
};

static void Frame(TaskManager* tm, int n, float dt)
{
    const bool r = tm->Update(dt);
    std::fprintf(out, "frame %d -> %d running=%d\n", n, r, TaskManager::IsRunning());
}

static void State(const char* what, std::vector<Probe*>& all)
{
    std::fprintf(out, "[%s]", what);
    for (Probe* p : all)
        if (p) std::fprintf(out, " %s:%d%d", p->label.c_str(), p->IsUpdating(), p->IsOnUpdateList());
    std::fprintf(out, "\n");
}

int main(int argc, char** argv)
{
    JET_STATIC_KERNEL;
    if (argc < 2) return 2;
    out = std::fopen(argv[1], "w");
    TaskManager* tm = GetTaskManager();
    std::fprintf(out, "tm=%d running=%d\n", tm != nullptr, TaskManager::IsRunning());

    void* mem = Alloc::New(sizeof(OS));
    OS* os = new (mem) OS("probe", "", GetModuleHandleA(nullptr), nullptr);
    std::fprintf(out, "os init %d\n", os->Init());
    std::fprintf(out, "framerate before %d\n", tm->GetFrameRate());

    // tasks made before Start
    std::vector<Probe*> all;
    all.push_back(new Probe("b5", 5.0f, true));
    all.push_back(new Probe("a1", 1.0f, true, "ccs"));          // stops on frame 2
    all.push_back(new Probe("c5", 5.0f, true));                 // equal priority to b5
    all.push_back(new Probe("idle", 2.0f, false));              // not updated
    all.push_back(new Probe("neg", -3.0f, true, "cccccx"));     // asks to exit on frame 5
    all[0]->suspendOther = all[2];                              // b5 suspends c5 at frame 1, resumes at 3
    State("before start", all);

    std::fprintf(out, "start %d running=%d framerate=%d\n", tm->Start(os), TaskManager::IsRunning(), tm->GetFrameRate());
    State("after start", all);

    std::fprintf(out, "gettask a1=%d idle=%d none=%d\n", tm->GetTask(PString("a1")) == all[1],
                 tm->GetTask(PString("idle")) == all[3], tm->GetTask(PString("nope")) != nullptr);
    std::fprintf(out, "gettask Log=%d FrameEnd=%d\n", tm->GetTask(PString("Log")) != nullptr,
                 tm->GetTask(PString("Frame End")) != nullptr);

    Frame(tm, 0, 0.25f);
    State("f0", all);
    all.push_back(new Probe("late", 0.5f, true));               // added while running
    Frame(tm, 1, 0.5f);
    State("f1", all);
    std::fprintf(out, "updatetask idle -> %d\n", tm->UpdateTask(PString("idle"), 1.5f));
    delete all[2];                                              // c5 deleted while suspended
    all[2] = nullptr;
    all[0]->suspendOther = nullptr;
    Frame(tm, 2, 0.125f);
    State("f2", all);
    Probe* bad = new Probe("badinit", 9.0f, true);
    bad->failInit = true;
    Frame(tm, 3, 0.0625f);                                      // Init fails -> shutdown
    State("f3", all);
    Frame(tm, 4, 0.0625f);
    std::fprintf(out, "running=%d\n", TaskManager::IsRunning());
    tm->Stop();
    std::fprintf(out, "stopped running=%d\n", TaskManager::IsRunning());
    State("after stop", all);

    // second run: restart and let "neg" exit
    for (Probe* p : all)
        if (p) p->frame = 0;
    std::fprintf(out, "restart %d\n", tm->Start(os));
    for (Probe* p : all)
        if (p && p != all[3]) p->Resume_();
    all.push_back(new Probe("exit3", 4.0f, true, "ccx"));
    State("restarted", all);
    for (int i = 0; i < 8; ++i) {
        Frame(tm, 10 + i, 1.0f);
        State("run2", all);
    }
    TaskManager::Shutdown();
    Frame(tm, 20, 1.0f);
    tm->Stop();
    for (Probe* p : all) delete p;
    delete bad;
    std::fprintf(out, "done\n");
    std::fclose(out);
    return 0;
}
