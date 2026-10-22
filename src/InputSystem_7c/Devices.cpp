// InputSystem_7c unit U2: InputDevice (state blocks, events to the observers), its Keyboard / Mouse / Joystick
// accessors and the InputDriver plugin base. Transcribed from the assembly (InputDevice 0x1000f1a0..0x1000f2fd,
// events 0x10001aa0..0x10001cd0, devices 0x10001d50..0x10002087, InputDriver 0x10002260..0x10002383).
#include <jet/InputSystem.h>

#include <cstring>

namespace Jet {

// ---- InputDevice
InputDevice::InputDevice(InputDriver* d) : handle(nullptr), valid(true)
{
    std::memset(&state, 0, sizeof(state));
    std::memset(&prevState, 0, sizeof(prevState));
    driver = d;
    if (driver) driver->AddReference();
}

InputDevice::~InputDevice()
{
    handle = nullptr;
    if (driver) {
        driver->ReleaseReference();
        driver = nullptr;
    }
}

bool InputDevice::IsValid() const { return valid; }
const UId& InputDevice::GetInstanceId() const { return instanceId; }
unsigned long InputDevice::GetTypeId() const { return static_cast<unsigned long>(state.type); }
const InputData& InputDevice::GetState() const { return state; }
const InputData& InputDevice::GetPrevState() const { return prevState; }
void* InputDevice::GetHandle() { return handle; }

void InputDevice::GetCaps(InputDeviceCaps&) const {}
bool InputDevice::GetProperty(DeviceProperty&) { return false; }
bool InputDevice::SetProperty(const DeviceProperty&) { return false; }
ForceTypeDesc* InputDevice::EnumerateEffects() { return nullptr; }
void InputDevice::StopAllEffects() {}
void InputDevice::ResetEffects() {}
void InputDevice::PauseEffects(bool) {}
void InputDevice::MuteEffects(bool) {}
ForceEffect* InputDevice::CreateEffect(const ForceInfo&, unsigned long) { return nullptr; }
ForceEffect* InputDevice::CreateEffect(const PString&, const ResourceDatabase*, unsigned long) { return nullptr; }
void InputDevice::Update(float) {}

void InputDevice::Done()
{
    valid = false;
    handle = nullptr;
}

void InputDevice::CriticalShutdown()
{
    handle = nullptr;
    valid = false;
}

void InputDevice::CriticalStartup() {}

// the event's fields that a kind does not use are left as they are (stack contents), as in the original
void InputDevice::KeyEvent(unsigned long key, int flags, unsigned long time)  // 0x10001aa0 (ButtonEvent too)
{
    InputEvent e;
    e.type = flags | InputEvent::EVENT_KEY;
    e.code = static_cast<unsigned char>(key);
    e.down = (flags & INPUT_DOWN) ? 0xff : 0;
    e.time = time;
    NotifyObservers(e, false);
}

void InputDevice::ButtonEvent(int button, int flags, unsigned long time)
{
    InputEvent e;
    e.type = flags | InputEvent::EVENT_KEY;
    e.code = static_cast<unsigned char>(button);
    e.down = (flags & INPUT_DOWN) ? 0xff : 0;
    e.time = time;
    NotifyObservers(e, false);
}

void InputDevice::CharEvent(unsigned short ch, int flags, unsigned long time)
{
    InputEvent e;
    e.type = flags | InputEvent::EVENT_CHAR;
    e.ch = ch;
    e.time = time;
    NotifyObservers(e, false);
}

void InputDevice::AxisEvent(int axis, int value, int delta, unsigned long time)
{
    InputEvent e;
    e.type = InputEvent::EVENT_AXIS;
    e.code = static_cast<unsigned char>(axis);
    e.value = value;
    e.delta = delta;
    e.time = time;
    NotifyObservers(e, false);
}

void InputDevice::PovEvent(int pov, int value, unsigned long time)
{
    InputEvent e;
    e.type = InputEvent::EVENT_POV;
    e.code = static_cast<unsigned char>(pov);
    e.value = value;
    e.time = time;
    NotifyObservers(e, false);
}

void InputDevice::InputGetEvent(unsigned long time)
{
    InputEvent e;
    e.type = InputEvent::EVENT_GET;
    e.time = time;
    NotifyObservers(e, true);
}

void InputDevice::InputLostEvent(unsigned long time)
{
    InputEvent e;
    e.type = InputEvent::EVENT_LOST;
    e.time = time;
    NotifyObservers(e, true);
}

void InputDevice::PropertyChangedEvent(DevicePropertyID id, unsigned long time)
{
    InputEvent e;
    e.type = InputEvent::EVENT_PROPERTY;
    e.id = id;
    e.time = time;
    NotifyObservers(e, true);
}

// tags the event with the device type and offers it to the observers, highest priority first; STOP ends the round
// unless `all` (the first entry is taken even when it was cleared, as in the original)
void InputDevice::NotifyObservers(InputEvent& e, bool all)  // 0x10001c40
{
    if (!valid) return;
    e.type |= state.type & 0xf;
    if (!(e.type & 0xf)) return;
    LockObservers();
    for (ObserverIterator i = FirstObserver(); i.observer.node; ++i)
        if ((*i)->OnInputEvent(instanceId, e) == InputEvent::STOP && !all) break;
    UnlockObservers();
}

// ---- KeyboardDevice
KeyboardDevice::KeyboardDevice(InputDriver* d) : InputDevice(d) {}
KeyboardDevice::~KeyboardDevice() {}
bool KeyboardDevice::IsKeyPressed(unsigned char key) const { return (state.keyboard.keys[key] >> 7) != 0; }
bool KeyboardDevice::IsKeyToggled(unsigned char key) const { return (state.keyboard.keys[key] & 1) != 0; }

// ---- MouseDevice
MouseDevice::MouseDevice(InputDriver* d) : InputDevice(d) {}
MouseDevice::~MouseDevice() {}
int MouseDevice::GetRelX() const { return state.mouse.delta[0]; }
int MouseDevice::GetRelY() const { return state.mouse.delta[1]; }
int MouseDevice::GetRelZ() const { return state.mouse.delta[2]; }
int MouseDevice::GetAbsX() const { return state.mouse.position[0]; }
int MouseDevice::GetAbsY() const { return state.mouse.position[1]; }
int MouseDevice::GetAbsZ() const { return state.mouse.position[2]; }

bool MouseDevice::IsButtonPressed(unsigned char button) const
{
    return button < 8 && state.mouse.buttons[button] != 0;
}

// through the driver's SetProperty: x then y, in one property (its `b` is never set)
void MouseDevice::SetPosition(const Point& p)  // 0x10001e40
{
    DeviceProperty prop;
    prop.id = PROPERTY_POSITION;
    prop.index = 0;
    prop.a = p.x;
    SetProperty(prop);
    prop.a = p.y;
    prop.index = 1;
    SetProperty(prop);
}

void MouseDevice::SetBoundings(const Rect& r)  // 0x10001e90
{
    DeviceProperty prop;
    prop.id = PROPERTY_RANGE;
    prop.index = 0;
    prop.a = r.x;
    prop.b = r.x + r.w - 1;
    SetProperty(prop);
    prop.a = r.y;
    prop.b = r.y + r.h - 1;
    prop.index = 1;
    SetProperty(prop);
}

// ---- JoystickDevice
JoystickDevice::JoystickDevice(InputDriver* d) : InputDevice(d) {}
JoystickDevice::~JoystickDevice() {}
int JoystickDevice::GetRelX() const { return state.joystick.delta[0]; }
int JoystickDevice::GetRelY() const { return state.joystick.delta[1]; }
int JoystickDevice::GetRelZ() const { return state.joystick.delta[2]; }
int JoystickDevice::GetRelRX() const { return state.joystick.delta[3]; }
int JoystickDevice::GetRelRY() const { return state.joystick.delta[4]; }
int JoystickDevice::GetRelRZ() const { return state.joystick.delta[5]; }
int JoystickDevice::GetRelU() const { return state.joystick.delta[6]; }
int JoystickDevice::GetRelV() const { return state.joystick.delta[7]; }
int JoystickDevice::GetAbsX() const { return state.joystick.position[0]; }
int JoystickDevice::GetAbsY() const { return state.joystick.position[1]; }
int JoystickDevice::GetAbsZ() const { return state.joystick.position[2]; }
int JoystickDevice::GetAbsRX() const { return state.joystick.position[3]; }
int JoystickDevice::GetAbsRY() const { return state.joystick.position[4]; }
int JoystickDevice::GetAbsRZ() const { return state.joystick.position[5]; }
int JoystickDevice::GetAbsU() const { return state.joystick.position[6]; }
int JoystickDevice::GetAbsV() const { return state.joystick.position[7]; }

bool JoystickDevice::IsButtonPressed(unsigned char button) const
{
    return button < 32 && state.joystick.buttons[button] != 0;
}

unsigned char JoystickDevice::GetButtonPressure(unsigned char button) const
{
    return button < 32 && state.joystick.buttons[button] ? state.joystick.pressure[button] : 0;
}

int JoystickDevice::GetPov(unsigned char pov) const { return pov < 4 ? state.joystick.pov[pov] : -1; }

// ---- InputDriver
InputDriver::InputDriver() {}
InputDriver::~InputDriver() {}
bool InputDriver::Init(OS*) { return false; }
void InputDriver::Update(float) {}
void InputDriver::Enumerate(InputDeviceCapsList&, int) {}
// the original's body only clears eax and leaves the result unwritten; nothing calls the base version
AutoReference<InputDevice> InputDriver::CreateDevice(const UId&) { return AutoReference<InputDevice>(); }
void InputDriver::CriticalShutdown() {}
void InputDriver::CriticalStartup() {}

}  // namespace Jet
