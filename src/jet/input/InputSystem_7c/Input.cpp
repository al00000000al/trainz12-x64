// InputSystem_7c unit U5: Input, the input manager, plus the InputExtension base (unit U6's first part). Transcribed
// from the assembly (Input 0x10006fd0..0x10007da6, copy / assignment 0x10002390..0x10002539, the container helpers
// 0x10007db0..0x100082c5, InputExtension 0x10006e60..0x10006fc7).
#include <jet/InputSystem.h>
#include <jet/Window.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

bool SameId(const UId& a, const UId& b) { return std::memcmp(&a, &b, sizeof(UId)) == 0; }

// PluginBase's lookups are protected: the original's wrapper 0x10008240 is a member of a class derived from it
struct PluginLookup : PluginBase {
    // the factory registered as `name` in `group`, holding a reference (null when there is none)
    static PluginType* Find(const UId& group, const PString& name)
    {
        PluginType* found = nullptr;
        PluginType* type;
        if (LookupPluginType(&type, group, name).code == 0) found = type;
        return found;
    }
};

Profile::Info updateInfo = {"Input::Update", "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/InputSystem/Input.cpp",
                            0, 0, -1, 0x80000000, 0, 0};

}  // namespace

bool Input::isInstance = false;

// ---- the device list
// 0x10007eb0: at the front; a full first chunk gets a new chunk in front of it, else its items move up one
void Input::DeviceList::Push(const AutoReference<InputDevice>& item)
{
    if (!head) {
        Chunk* c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
        c->count = 0;
        c->next = c->prev = c;
        head = c;
    } else if (head->count == 13) {
        Chunk* c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
        c->count = 0;
        c->next = head;
        c->prev = head->prev;
        head->prev->next = c;
        head->prev = c;
        head = c;
    } else {
        for (uint32_t i = head->count; i > 0; --i) {
            new (&head->data[i]) AutoReference<InputDevice>(head->data[i - 1]);
            head->data[i - 1].~AutoReference();
        }
    }
    ++head->count;
    new (&head->data[0]) AutoReference<InputDevice>(item);
}

// 0x10007f90: drops the item, moves the rest of its chunk down one (reference by reference) and returns the item
// that took its place; an emptied chunk is freed
Input::DeviceList::ConstIterator Input::DeviceList::Remove(const ConstIterator& i)
{
    if (!head || !i.node) return i;
    Chunk* c = i.chunk;
    auto* node = const_cast<AutoReference<InputDevice>*>(i.node);
    AutoReference<InputDevice>* const last = &c->data[c->count - 1];
    node->~AutoReference();
    for (AutoReference<InputDevice>* p = node; p < last; ++p) {
        new (p) AutoReference<InputDevice>(p[1]);
        p[1].~AutoReference();
    }
    if (c->count == 1) {
        Chunk* const next = c->next;
        if (c == head) {
            if (next == head) {
                Alloc::Delete(c);
                head = nullptr;
                return ConstIterator(nullptr, nullptr, nullptr);
            }
            c->prev->next = next;
            next->prev = c->prev;
            Alloc::Delete(c);
            head = next;
            return ConstIterator(&next->data[0], head, next);
        }
        c->prev->next = next;
        next->prev = c->prev;
        Alloc::Delete(c);
        return ConstIterator(next != head ? &next->data[0] : nullptr, head, next);
    }
    --c->count;
    if (node != last) return ConstIterator(node, head, c);
    c = c->next;
    return ConstIterator(c != head ? &c->data[0] : nullptr, head, c);
}

// ---- construction
Input::Input()  // 0x10006fd0
    : driverCount(0), initialized(false), shutdownHandle(nullptr), startupHandle(nullptr), os(nullptr)
{
    isInstance = true;
    optionKeys[0] = "InputKeyboard";
    optionKeys[1] = "InputMouse";
    optionKeys[2] = "InputJoystick";
    for (int i = 0; i < 3; ++i) drivers[i] = typeDrivers[i] = nullptr;
}

// 0x10002390: a plain member copy (the drivers and extensions without new references, the devices with)
Input::Input(const Input& o)
    : driverCount(o.driverCount), initialized(o.initialized), devices(o.devices), caps(o.caps),
      shutdownHandle(o.shutdownHandle), startupHandle(o.startupHandle), os(o.os)
{
    for (int i = 0; i < 3; ++i) {
        optionKeys[i] = o.optionKeys[i];
        drivers[i] = o.drivers[i];
        typeDrivers[i] = o.typeDrivers[i];
    }
    extensions.AppendChunks(o.extensions);
}

Input::~Input()  // 0x10007020 (the lists are emptied by their destructors: extensions, caps, devices)
{
    isInstance = false;
    Done();
}

// 0x10002470: no self check (assigning an Input to itself empties its lists)
Input& Input::operator=(const Input& o)
{
    for (int i = 0; i < 3; ++i) {
        optionKeys[i] = o.optionKeys[i];
        drivers[i] = o.drivers[i];
        typeDrivers[i] = o.typeDrivers[i];
    }
    driverCount = o.driverCount;
    initialized = o.initialized;
    devices.RemoveAll();
    devices.AppendChunks(o.devices);
    caps.RemoveAll();
    caps.AppendChunks(o.caps);
    extensions.RemoveAll();
    extensions.AppendChunks(o.extensions);
    shutdownHandle = o.shutdownHandle;
    startupHandle = o.startupHandle;
    os = o.os;
    return *this;
}

// ---- start and stop
// the drivers are loaded keyboard, mouse, joystick; the first that fails ends the loading (Init still succeeds)
bool Input::Init(OS* o)  // 0x100070d0
{
    if (initialized) return true;
    if (!o || !o->GetOptions()) return false;
    shutdownHandle = ExceptionHandler::AddCriticalShutdown(&Input::CriticalShutdown, this);
    startupHandle = ExceptionHandler::AddCriticalStartup(&Input::CriticalStartup, this);
    for (int type = DRIVER_KEYBOARD; type <= DRIVER_JOYSTICK; ++type)
        if (!LoadDriver(static_cast<DRIVER>(type), o, o->GetOptions())) break;
    initialized = true;
    os = o;
    return true;
}

void Input::Done()  // 0x10007160
{
    if (!initialized) return;
    for (ExtensionList::Iterator it = extensions.First(); it.node;) {
        InputExtension* const ext = *it;
        if (ext->GetReferenceCount() == 1) ext->Done();
        ext->ReferenceCount::ReleaseReference();
        it = extensions.Remove(it);
    }
    for (DeviceList::ConstIterator it = devices.First(); it.node;) {
        auto& ref = const_cast<AutoReference<InputDevice>&>(*it);
        ref.Get()->Done();
        ref = static_cast<InputDevice*>(nullptr);
        it = devices.Remove(it);
    }
    for (InputDriver*& d : typeDrivers) {
        if (d) {
            d->ReleaseReference();
            d = nullptr;
        }
    }
    while (driverCount) {
        --driverCount;
        if (drivers[driverCount]) {
            drivers[driverCount]->ReleaseReference();
            drivers[driverCount] = nullptr;
        }
    }
    if (shutdownHandle) {
        ExceptionHandler::RemoveCriticalShutdown(shutdownHandle);
        shutdownHandle = nullptr;
    }
    if (startupHandle) {
        ExceptionHandler::RemoveCriticalStartup(startupHandle);
        startupHandle = nullptr;
    }
    initialized = false;
    os = nullptr;
}

// drivers, devices, then the extensions; an extension only Input still holds is dropped (through Done)
bool Input::Update(float dt)  // 0x100072e0
{
    Profile profile(&updateInfo);
    for (int i = 0; i < driverCount; ++i)
        if (drivers[i]) drivers[i]->Update(dt);
    for (DeviceList::ConstIterator it = devices.First(); it.node; ++it) it->Get()->Update(dt);
    for (ExtensionList::Iterator it = extensions.First(); it.node;) {
        InputExtension* const ext = *it;
        if (ext->GetReferenceCount() == 1) {
            it = extensions.Remove(it);
            if (ext->GetReferenceCount() == 1) ext->Done();
            ext->ReferenceCount::ReleaseReference();
        } else {
            ext->Update(dt);
            ++it;
        }
    }
    return true;
}

bool Input::IsInitialized() const { return initialized; }  // 0x10002540

bool Input::IsActiveWnd() const  // 0x10007d70
{
    if (!os || !os->GetWindow()) return false;
    return os->GetWindow()->IsActiveWnd();
}

// ---- devices
// 0x10007480: the devices of the types in `typeMask` having all of `capsMask`; a new list the caller owns, or null
InputDeviceCapsList* Input::EnumerateDevices(int typeMask, unsigned long capsMask)
{
    EnumerateAll();
    if (!caps.head) return nullptr;
    auto* list = static_cast<InputDeviceCapsList*>(Alloc::New(sizeof(InputDeviceCapsList)));
    if (list) new (list) InputDeviceCapsList;
    for (InputDeviceCapsList::ConstIterator c = caps.First(); c.node; ++c)
        if ((c->type & typeMask) && (!capsMask || (c->caps & capsMask) == capsMask)) list->Add(*c);
    if (!list->head) {
        list->RemoveAll();
        Alloc::Delete(list);
        return nullptr;
    }
    return list;
}

// the device with `id`: the emulated mouse / joystick are built here; others are looked up among the devices handed
// out, else created by the driver of the device's type (as reported by the drivers) and updated once
AutoReference<InputDevice> Input::GetDevice(const UId& id)  // 0x10007540
{
    AutoReference<InputDevice> local;
    if (SameId(id, UIdMouseEmu) || SameId(id, UIdJoystickEmu)) {
        EmuDevice* const emu = new EmuDevice;
        if (!emu) return AutoReference<InputDevice>();
        if (!emu->Init(this, id, os)) {
            delete emu;
            return AutoReference<InputDevice>();
        }
        local = emu;
        devices.Add(local);
    } else {
        for (DeviceList::ConstIterator it = devices.First(); it.node; ++it)
            if (SameId(it->Get()->instanceId, id)) return *it;
        int type = FindDeviceType(id);
        if (!type) {
            EnumerateAll();
            type = FindDeviceType(id);
        }
        InputDriver* driver;
        switch (type) {
        case DEVICE_KEYBOARD: driver = typeDrivers[DRIVER_KEYBOARD]; break;
        case DEVICE_MOUSE: driver = typeDrivers[DRIVER_MOUSE]; break;
        case DEVICE_JOYSTICK: driver = typeDrivers[DRIVER_JOYSTICK]; break;
        default: return AutoReference<InputDevice>();
        }
        local = driver->CreateDevice(id);
        devices.Push(local);
    }
    local.Get()->Update(0.0f);
    return local;
}

void Input::RegisterExtension(InputExtension* ext)  // 0x10007840
{
    if (extensions.Find(ext).node) return;
    ext->AddReference();
    extensions.Add(ext);
}

// 0x10007880: the names listed in the type's option, in order; "Default" is Win32 for keyboard and mouse and
// DirectX for joysticks. A name with no factory ends the search (false); a factory already loaded for another type
// is shared; else the first driver whose Init succeeds is kept.
bool Input::LoadDriver(DRIVER type, OS* o, Options* options)
{
    static const PString defaultName("Default");
    PString name;
    options->SetSeparators(optionKeys[type], ",");
    while (options->GetNext(optionKeys[type], name)) {
        if (name == defaultName && type >= 0) {
            if (type <= DRIVER_MOUSE) {
                name.~PString();
                new (&name) PString("Win32");
            } else if (type == DRIVER_JOYSTICK) {
                name.~PString();
                new (&name) PString("DirectX");
            }
        }
        PluginType* const factory = PluginLookup::Find(UId("F3117B18-F364-41a1-8993-966236A35B5E"), name);
        if (!factory) return false;
        for (int i = 0; i < driverCount; ++i) {
            if (SameId(drivers[i]->pluginFactory->uniqueId, factory->uniqueId)) {
                typeDrivers[type] = drivers[i];
                drivers[i]->AddReference();
                factory->ReleaseReference();
                return true;
            }
        }
        InputDriver* const driver = static_cast<PluginFactoryBase<InputDriver>*>(factory)->CreateInstance();
        drivers[driverCount] = driver;
        if (driver) {
            if (driver->Init(o)) {
                typeDrivers[type] = drivers[driverCount];
                ++driverCount;
                typeDrivers[type]->AddReference();
                factory->ReleaseReference();
                return true;
            }
            drivers[driverCount]->ReleaseReference();  // (the slot keeps the pointer)
        }
        factory->ReleaseReference();
    }
    return false;
}

// 0x10007bb0: each type's driver adds what it has to `caps` (never emptied: repeated calls add the devices again)
void Input::EnumerateAll()
{
    for (int i = 0; i < 3; ++i)
        if (typeDrivers[i]) typeDrivers[i]->Enumerate(caps, 1 << i);
}

int Input::FindDeviceType(const UId& id)  // 0x10007bf0
{
    for (InputDeviceCapsList::ConstIterator c = caps.First(); c.node; ++c)
        if (SameId(c->id, id)) return c->type;
    return 0;
}

// ---- crash handling: devices before drivers going down, drivers before devices coming back
void Input::CriticalShutdown()  // 0x10007c60
{
    for (DeviceList::ConstIterator it = devices.First(); it.node; ++it) it->Get()->CriticalShutdown();
    for (int i = 0; i < driverCount; ++i)
        if (drivers[i]) drivers[i]->CriticalShutdown();
}

int Input::CriticalShutdown(void* self)  // 0x10007ce0
{
    static_cast<Input*>(self)->CriticalShutdown();
    return 0;
}

void Input::CriticalStartup()  // 0x10007cf0
{
    for (int i = 0; i < driverCount; ++i)
        if (drivers[i]) drivers[i]->CriticalStartup();
    for (DeviceList::ConstIterator it = devices.First(); it.node; ++it) it->Get()->CriticalStartup();
}

int Input::CriticalStartup(void* self)  // 0x10007d60
{
    static_cast<Input*>(self)->CriticalStartup();
    return 0;
}

// ---- InputExtension
InputExtension::InputExtension() : active(true), exclusive(false) {}  // 0x10006ef0
InputExtension::~InputExtension() {}                                  // 0x10006e60

bool InputExtension::Init(Input& input)  // 0x10006f20
{
    input.RegisterExtension(this);
    return true;
}

bool InputExtension::ReleaseReference()  // 0x10006f70
{
    if (GetReferenceCount() == 1) Done();
    return ReferenceCount::ReleaseReference();
}

void InputExtension::Activate(bool on) { active = on; }          // 0x10006f90
bool InputExtension::IsActive() const { return active; }         // 0x10006fa0
void InputExtension::SetExclusive(bool on) { exclusive = on; }   // 0x10006fb0
bool InputExtension::GetExclusive() const { return exclusive; }  // 0x10006fc0
void InputExtension::Reset() {}                                   // 0x10002300 (shared empty body)
void InputExtension::Done() {}
void InputExtension::Update(float) {}

}  // namespace Jet
