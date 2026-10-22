// Jet::Task and Jet::TaskManager (Kernel_7c 0x1000c780-0x1000d790): the engine's main loop. Reconstructed from
// TS12's code; the SDK 1.0 headers give the names. TS12 adds lockFPS (fixed time step via Time::IncFrame).
#include <jet/Task.h>

namespace Jet {

TaskManager* Task::taskManager = nullptr;
bool TaskManager::shutdown = false;
bool TaskManager::running = false;

JET_LAYOUT32(sizeof(Task) == 0x28);
JET_LAYOUT32(sizeof(TaskManager) == 0x38);

// ---------------------------------------------------------------------------------------------------------------
// Task

Task::Task(const PString& name, float pri, bool updated)  // 0x1000c780
    : taskName(name), priority(pri)
{
    updatedTime = Time::GetSeconds() - 1.0;
    isUpdating = updated;
    isOnUpdateList = false;
    isInitialized = false;
    TaskManager* tm = taskManager;
    tm->taskArray.Add(this);
    tm->taskChangeArray.Add(this);
    tm->taskChanges = true;
}

// 0x100010e0: copies the list links too, and does not register the copy
Task::Task(const Task& o)
    : IListNode(o), taskName(o.taskName), updatedTime(o.updatedTime), priority(o.priority),
      isUpdating(o.isUpdating), isOnUpdateList(o.isOnUpdateList), isInitialized(o.isInitialized) {}

Task& Task::operator=(const Task& o)  // 0x10001180
{
    IListNode::operator=(o);
    taskName = o.taskName;
    updatedTime = o.updatedTime;
    priority = o.priority;
    isUpdating = o.isUpdating;
    isOnUpdateList = o.isOnUpdateList;
    isInitialized = o.isInitialized;
    return *this;
}

Task::~Task()  // 0x1000c880
{
    if (taskManager) taskManager->Remove(this);
}

const PString& __cdecl Task::GetName() const { return taskName; }
bool __cdecl Task::IsUpdating() const { return isUpdating; }
bool __cdecl Task::IsOnUpdateList() const { return isOnUpdateList; }
bool __cdecl Task::IsInitialized() const { return isInitialized; }
int __cdecl Task::operator<(const Task& o) const { return priority < o.priority; }
int __cdecl Task::operator==(const Task& o) const { return priority == o.priority; }

bool __cdecl Task::Init(OS*)  // 0x1000c900
{
    isInitialized = true;
    return true;
}

void __cdecl Task::Done() {}                                            // 0x10001b40
Task::UpdateCode __cdecl Task::Update(float) { return CONTINUE; }      // 0x100014c0

void __cdecl Task::Suspend()  // 0x1000c910
{
    isUpdating = false;
    taskManager->SetTaskChanged(this);
}

void __cdecl Task::Resume()  // 0x1000c940
{
    isUpdating = true;
    taskManager->SetTaskChanged(this);
}

void __cdecl Task::SetOnUpdateList(bool onList) { isOnUpdateList = onList; }
void __cdecl Task::SetInitialized(bool initialized) { isInitialized = initialized; }

// ---------------------------------------------------------------------------------------------------------------
// TaskManager

// 0x10004f10: built by the module's attach code
TaskManager::TaskManager()
    : os(nullptr), lastTime(0.0), frameEndTask(nullptr), frameRate(0), lockFPS(0.0f), waveServer(nullptr)
{
    initialised = false;
    taskChanges = false;
    Task::taskManager = this;
}

// 0x1000c9b0: deletes the tasks still on the update list
TaskManager::~TaskManager()
{
    Task::taskManager = nullptr;
    while (taskUpdateList.count) {
        Task* t = taskUpdateList.PopFirst();
        delete t;
    }
    taskChangeArray.RemoveAll();
    taskArray.RemoveAll();
}

OS* __cdecl TaskManager::GetOS() const { return os; }
int __cdecl TaskManager::GetFrameRate() const { return frameRate; }
const TaskArray& __cdecl TaskManager::GetTaskArr() const { return taskArray; }
void __cdecl TaskManager::Shutdown() { shutdown = true; }
bool __cdecl TaskManager::IsRunning() { return running; }
void __cdecl TaskManager::ResetTime() { lastTime = Time::GetSeconds(); }

Task* __cdecl TaskManager::GetTask(const PString& name)  // 0x1000ca70
{
    for (TaskArray::Iterator i = taskArray.First(); i != taskArray.End(); ++i)
        if ((*i)->GetName() == name) return *i;
    return nullptr;
}

// 0x1000cad0: no check that the task exists (as the original)
Task::UpdateCode __cdecl TaskManager::UpdateTask(const PString& name, float dt) { return GetTask(name)->Update(dt); }

void __cdecl TaskManager::Add(Task* task)  // 0x1000ce00
{
    taskArray.Add(task);
    taskChangeArray.Add(task);
    taskChanges = true;
}

void __cdecl TaskManager::Remove(Task* task)  // 0x1000ce30
{
    TaskArray::Iterator i = taskArray.Find(task);
    if (i.node) taskArray.Remove(i);
    i = taskChangeArray.Find(task);
    if (i.node) taskChangeArray.Remove(i);
    if (task->isOnUpdateList) {
        taskUpdateList.Unlink(task);
        task->isOnUpdateList = false;
    }
}

void __cdecl TaskManager::SetTaskChanged(Task* task)  // 0x1000cee0
{
    if (initialised) {
        taskChangeArray.Add(task);
        taskChanges = true;
    }
}

// 0x1000ccf0: initialise new tasks and move tasks on or off the update list
bool TaskManager::CheckTaskChanges()
{
    while (taskChangeArray.head) {
        Task* t = *taskChangeArray.First();
        taskChangeArray.Remove(taskChangeArray.First());
        if (!t->isInitialized && !t->Init(os)) return false;
        if (t->isUpdating && !t->isOnUpdateList) {
            taskUpdateList.InsertSorted(t);
            t->isOnUpdateList = true;
        } else if (!t->isUpdating && t->isOnUpdateList) {
            taskUpdateList.Unlink(t);
            t->isOnUpdateList = false;
        }
    }
    taskChanges = false;
    return true;
}

// 0x1000cb80
bool TaskManager::Init()
{
    initialised = true;
    Options* opts = os->GetOptions();
    frameEndTask = new Task(PString("Frame End"), 3.402823466e+38f, true);
    taskUpdateList.InsertSorted(frameEndTask);
    frameEndTask->isOnUpdateList = true;
    InitWave();
    if (!opts->Get("vSync")) opts->Get("FrameRate", frameRate, 0);
    if (opts->Get("lockFPS", lockFPS, lockFPS)) {
        Time::SetFrameLockTime(1.0 / lockFPS);
        lockFPS = 1.0f / lockFPS;
    }
    return true;
}

// 0x1000d160: optional "WaveServer_7c.dll" when the options say WaveServer=On
void TaskManager::InitWave()
{
    waveServer = nullptr;
    Options* opts = os->GetOptions();
    if (!opts) return;
    String value;
    if (!GetTask(PString("WaveServerTask")) && opts->Get("WaveServer", value) && value.EqualsIgnoreCase("On")) {
        waveServer = os->CreateDynamicLibrary();
        if (waveServer) {
            File::Path path("WaveServer");
            path.Add("_7c.dll");
            waveServer->Open(path.text);
        }
    }
}

void TaskManager::DoneWave()  // 0x1000d2e0
{
    if (Task* t = GetTask(PString("WaveServerTask"))) t->Done();
    delete waveServer;
    waveServer = nullptr;
}

// 0x1000d080
void TaskManager::Done()
{
    DoneWave();
    while (taskUpdateList.count) taskUpdateList.PopFirst()->isOnUpdateList = false;
    taskChangeArray.RemoveAll();
    while (taskArray.head) taskArray.Remove()->Done();
    delete frameEndTask;
    frameEndTask = nullptr;
}

bool __cdecl TaskManager::Start(OS* o)  // 0x1000d390
{
    if (!running) {
        os = o;
        shutdown = false;
        running = Init();
    }
    return running;
}

void __cdecl TaskManager::Stop()  // 0x1000d540
{
    if (running) {
        Done();
        running = false;
    }
}

int __cdecl TaskManager::Main(OS* o)  // 0x1000cb00
{
    if (!running) {
        os = o;
        shutdown = false;
        running = Init();
        if (!running) return -6;
    }
    Loop();
    if (running) {
        Done();
        running = false;
    }
    return 0;
}

// 0x1000d3c0: one frame. The update list is a queue: tasks before the "Frame End" task are updated and go to
// the back, then "Frame End" itself goes to the back.
bool __cdecl TaskManager::Update(float dt)
{
    if (!running || shutdown) {
        shutdown = true;
        return false;
    }
    bool quit = shutdown;
    if (taskChanges) {
        if (!CheckTaskChanges()) {
            quit = true;
            shutdown = true;
        } else {
            quit = shutdown;
        }
    }
    Task* t = taskUpdateList.PopFirst();
    while (!quit && t != frameEndTask) {
        const Task::UpdateCode code = t->Update(dt);
        if (code == Task::EXIT) {
            quit = true;
            shutdown = true;
            break;
        }
        if (code == Task::STOP)
            t->isOnUpdateList = false;
        else
            taskUpdateList.Append(t);
        t = taskUpdateList.PopFirst();
        quit = shutdown;
    }
    taskUpdateList.Append(t);
    return !quit;
}

// 0x1000cf00: the main loop. FrameRate > 0 paces frames with Time::Delay and carries the error (clamped to half a
// frame) into the next frame; lockFPS > 0 feeds a fixed time step.
void TaskManager::Loop()
{
    double frameStart = Time::GetSeconds();
    lastTime = frameStart;
    double frameTime = 0.0, carry = 0.0;
    if (frameRate > 0) frameTime = 1.0 / frameRate;
    static int startTrace = -1;
    while (!shutdown) {
        if (startTrace < 0) startTrace = Profile::RegisterTracePoint("START");
        Profile::TracePoint(startTrace);
        float dt;
        if (lockFPS > 0.0f) {
            dt = lockFPS;
            Time::IncFrame();
        } else {
            dt = static_cast<float>(frameStart - lastTime);
        }
        Update(dt);
        if (frameRate > 0) {
            const double target = carry + frameTime;
            const double waitMs = (target - (Time::GetSeconds() - frameStart)) * 1000.0;
            if (waitMs > 0.0) Time::Delay(static_cast<unsigned long>(static_cast<long long>(waitMs)));
            const double elapsed = Time::GetSeconds() - frameStart;
            const double next = (frameTime - elapsed) + carry;
            const double low = frameTime * -0.5, high = frameTime * 0.5;
            carry = low > next ? low : (next > high ? high : next);
        }
        lastTime = frameStart;
        frameStart = Time::GetSeconds();
    }
}

}  // namespace Jet
