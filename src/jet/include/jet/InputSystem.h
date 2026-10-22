// InputSystem_7c: input devices and the driver plugins that create them. Declared as far as the drivers
// (InputWin32_7c) need: layouts, vtable order and mangled names match TS12.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/Math.h>
#include <jet/Plugin.h>
#include <jet/Resource.h>
#include <jet/UId.h>

#if defined(JET_IS_BUILD)  // compiling InputSystem_7c itself
#define JET_IS_IMPORT
#else
#define JET_IS_IMPORT __declspec(dllimport)
#endif

namespace Jet {

class ForceEffect;
class ForceInfo;
class Input;
class InputDriver;
class InputObserver;
class OS;
class ResourceDatabase;
struct ForceTypeDesc;

extern JET_IS_IMPORT const UId UIdKeyboard;
extern JET_IS_IMPORT const UId UIdMouse;
extern JET_IS_IMPORT const UId UIdJoystick1;
extern JET_IS_IMPORT const UId UIdJoystick2;
extern JET_IS_IMPORT const UId UIdUnknown;
extern JET_IS_IMPORT const UId UIdMouseEmu;
extern JET_IS_IMPORT const UId UIdJoystickEmu;
extern JET_IS_IMPORT const UId UIdNullDev;

enum InputDeviceType { DEVICE_KEYBOARD = 1, DEVICE_MOUSE = 2, DEVICE_JOYSTICK = 4 };

// key, button, axis and pov codes; their names ("KC_Esc", "BC_Left", "AC_X", "PC_1", "NE") are in InputStrings
enum KeyCode { KC_NONE = 0, KC_ESC = 1 };  // DirectInput scan code order
enum ButtonCode { BC_1 = 0 };              // joystick buttons 0..31; mouse buttons 0..4 (ButtonToString's `mouse`)
enum AxisCode { AC_X = 0 };
enum PovCode { PC_1 = 0 };
enum PovDir { POV_N = 0 };

// the character each key gives (0x600 bytes: one 6-byte entry per KeyCode)
class ASCIICode {
public:
    static const ASCIICode& __cdecl Get();
    ASCIICode& operator=(const ASCIICode& o);
    unsigned short __cdecl GetChar(KeyCode key, bool capsLock, bool shift, bool numLock) const;
    bool __cdecl IsControl(KeyCode key) const;

private:
    ASCIICode();
    enum Kind { CONTROL = 0, LETTER = 1, SYMBOL = 2, KEYPAD_OPERATOR = 3, KEYPAD_DIGIT = 4 };
    struct Entry {
        unsigned short normal, shifted;
        unsigned char kind, pad;
    };
    Entry entries[256];
};
JET_LAYOUT32(sizeof(ASCIICode) == 0x600);

// the names of the codes (0x4e8 bytes: 256 keys, 37 buttons, 8 axes, 4 povs, 9 pov directions, one PString each)
class InputStrings {
public:
    static const InputStrings& __cdecl Get();
    InputStrings(const InputStrings& o);
    ~InputStrings();
    InputStrings& operator=(const InputStrings& o);

    PString __cdecl KeyToString(KeyCode key) const;
    PString __cdecl ButtonToString(ButtonCode button, bool mouse) const;
    PString __cdecl AxisToString(AxisCode axis) const;
    PString __cdecl PovToString(PovCode pov) const;
    PString __cdecl PovDirToString(PovDir dir) const;
    KeyCode __cdecl StringToKey(const PString& s) const;
    ButtonCode __cdecl StringToButton(const PString& s) const;
    AxisCode __cdecl StringToAxis(const PString& s) const;
    PovCode __cdecl StringToPov(const PString& s) const;
    PovDir __cdecl StringToPovDir(const PString& s) const;

private:
    friend class EmuDevice;          // their loaders compare names with the table directly
    friend class InputActionMapper;
    InputStrings();
    enum { KEYS = 0, BUTTONS = 0x100, AXES = 0x125, POVS = 0x12d, POV_DIRS = 0x131, COUNT = 0x13a };
    PString names[COUNT];
};
JET_LAYOUT32(sizeof(InputStrings) == 0x4e8);

// what a driver reports for each device it can create (0x38 bytes)
class InputDeviceCaps {
public:
    InputDeviceCaps() {}

    int type;                  // +0x00 InputDeviceType
    UId id;                    // +0x04
    PString name;              // +0x14
    PString description;       // +0x18
    unsigned long caps;        // +0x1c
    unsigned long numButtons;  // +0x20
    unsigned long buttonMask;  // +0x24
    unsigned long numAxes;     // +0x28
    unsigned long axisMask;    // +0x2c
    unsigned long numPovs;     // +0x30
    unsigned long unknown34;   // +0x34
};
JET_LAYOUT32(sizeof(InputDeviceCaps) == 0x38);

using InputDeviceCapsList = DynamicArray<InputDeviceCaps, 6, Less<InputDeviceCaps, InputDeviceCaps>>;

// a device's state block (0x104 bytes): the device type, then the layout of that type
union InputData {
    int type;
    struct {
        int type;
        unsigned char keys[0x100];  // Jet key codes; bit 7 down, bit 0 toggled
    } keyboard;
    struct {
        int type;
        unsigned char buttons[8];  // bit 7 down
        int delta[3];              // x, y, wheel since the last update
        int position[3];
    } mouse;
    struct {
        int type;
        unsigned char buttons[32];   // bit 7 down
        unsigned char pressure[32];  // read while the button is down (InputWin32 writes 0xff)
        int delta[8];                // x, y, z, rx, ry, rz, u, v since the last update
        int position[8];
        int pov[4];  // hundredths of a degree, -1 centred
    } joystick;
    unsigned char raw[0x104];
};
static_assert(sizeof(InputData) == 0x104);

enum DevicePropertyID {
    PROPERTY_LOCK_KEY = 0,      // keyboard: caps / num / scroll lock state
    PROPERTY_AUTOREPEAT = 1,    // joystick buttons repeat while held
    PROPERTY_REPEAT_DELAY = 2,  // in ms
    PROPERTY_RANGE = 3,         // an axis' output range
    PROPERTY_THRESHOLD = 4,     // an axis' dead zone
    PROPERTY_POSITION = 5,      // an axis' position
    PROPERTY_AUTOCENTER = 8,    // emulated joystick: released axes drift back to the centre
    PROPERTY_EXCLUSIVE = 9,     // mouse: hidden cursor captured in the window
};

class DeviceProperty {
public:
    DevicePropertyID id;  // +0x00
    union {
        struct {
            unsigned char index;  // +0x04 axis / key, or the flag of a yes/no property
            unsigned char state;  // +0x05 keyboard lock state
        };
        int value;  // +0x04
    };
    int a, b;  // +0x08, +0x0c: minimum and maximum, or a single value in `a`
};
JET_LAYOUT32(sizeof(DeviceProperty) == 0x10);

// a reference that releases its object when it goes (returned by value, so through a hidden pointer)
template <class T>
class AutoReference {
public:
    AutoReference() : object(nullptr) {}
    AutoReference(const AutoReference& o) : object(o.object)
    {
        if (object) object->AddReference();
    }
    ~AutoReference()
    {
        if (object) object->ReleaseReference();
    }
    AutoReference& operator=(const AutoReference& o)
    {
        T* const n = o.object;
        if (object) object->ReleaseReference();
        object = n;
        if (object) object->AddReference();
        return *this;
    }
    // takes over the reference the new object was created with
    AutoReference& operator=(T* o)
    {
        if (object) object->ReleaseReference();
        object = o;
        return *this;
    }
    T* Get() const { return object; }

private:
    T* object;
};

// a list of observers, highest priority first (the SDK's ObserverSubject plus a priority). While locked (during a
// notification) removals only clear the entry's flag; the last unlock drops the cleared entries. The members are
// defined in InputSystem_7c (InputSystem_7c/Observers.cpp) for the three observer types it exports.
template <class O>
class ObserverSubject {
public:
    struct UncertainObserver {
        UncertainObserver() : isValid(false), priority(0) {}
        UncertainObserver(O* o, int p) : isValid(true), observer(o), priority(p) {}
        bool operator==(const UncertainObserver& o) const { return observer == o.observer; }

        bool isValid;
        O* observer;
        int priority;
    };
    struct HigherPriority {
        bool operator()(const UncertainObserver& a, const UncertainObserver& b) const { return a.priority > b.priority; }
    };
    using List = FastDynamicArray<UncertainObserver, 4, HigherPriority>;

    class ObserverIterator {
    public:
        ObserverIterator(const typename List::Iterator& i, List& l) : observer(i), list(&l) {}
        ObserverIterator(const ObserverIterator& o) : observer(o.observer), list(o.list) {}
        bool operator==(const ObserverIterator& o) const { return observer == o.observer; }
        bool operator!=(const ObserverIterator& o) const { return observer != o.observer; }
        O* operator*() const { return (*observer).observer; }
        // the next entry still observing (0x10012380)
        ObserverIterator& operator++()
        {
            do {
                ++observer;
            } while (observer.node && !observer.node->isValid);
            return *this;
        }

        typename List::Iterator observer;
        List* list;
    };
    class ConstObserverIterator {
    public:
        ConstObserverIterator(const typename List::ConstIterator& i, const List& l) : observer(i), list(&l) {}
        ConstObserverIterator(const ConstObserverIterator& o) : observer(o.observer), list(o.list) {}
        bool operator==(const ConstObserverIterator& o) const { return observer == o.observer; }
        bool operator!=(const ConstObserverIterator& o) const { return observer != o.observer; }
        const O* operator*() const { return (*observer).observer; }

        typename List::ConstIterator observer;
        const List* list;
    };

    ObserverSubject();
    ObserverSubject(const ObserverSubject& o);
    virtual ~ObserverSubject();
    ObserverSubject& operator=(const ObserverSubject& o);
    virtual void __cdecl AddObserver(O* observer, int priority);
    virtual void __cdecl RemoveObserver(O* observer);
    virtual bool __cdecl HasObserver(const O* observer) const;
    void __cdecl LockObservers();
    void __cdecl UnlockObservers();
    unsigned long __cdecl Count();
    ObserverIterator __cdecl FirstObserver();
    ConstObserverIterator __cdecl FirstObserver() const;
    ObserverIterator __cdecl LastObserver();
    ConstObserverIterator __cdecl LastObserver() const;
    ObserverIterator __cdecl EndObserver();
    ConstObserverIterator __cdecl EndObserver() const;
    int __cdecl GetNumObserverLocks() const;
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

private:
    List observers;    // +0x04
    int observerLock;  // +0x08
};

// one input event (0x14 bytes). `type` holds the kind in bits 4..7 (EVENT_*), the device type in bits 0..3
// (added by InputDevice::NotifyObservers) and the INPUT_* flags. What an observer returns: STOP ends the notification
// unless the event is one that every observer must see (lost / get / property changed).
struct InputEvent {
    enum Propagation { PROPAGATE = 0, STOP = 0xff };
    enum Kind {
        EVENT_LOST = 0x10,
        EVENT_GET = 0x20,
        EVENT_KEY = 0x30,  // keys and buttons
        EVENT_CHAR = 0x40,
        EVENT_AXIS = 0x50,
        EVENT_POV = 0x60,
        EVENT_PROPERTY = 0x70,
    };

    int type;  // +0x00
    union {
        struct {
            unsigned char code;  // +0x04 key, button, axis or pov
            unsigned char down;  // +0x05 key / button: 0xff when pressed
        };
        unsigned short ch;  // +0x04 character
        int id;             // +0x04 DevicePropertyID
    };
    int value;           // +0x08 axis / pov
    int delta;           // +0x0c axis
    unsigned long time;  // +0x10
};
JET_LAYOUT32(sizeof(InputEvent) == 0x14);

// what InputUIExtension tells its observers. The header word of a mouse event: bits 0..5 the modifier state
// (InputUIExtension::UpdateState), bit 7 the button is down, bits 16..23 the kind (MOUSE_*), bits 24..28 the button.
class KeyboardEvent {
public:
    unsigned long data;  // bits 16..23 the key, bits 0..7 the event's flags (see InputUIExtension::ProcessEvent)
};
class CharEvent {
public:
    unsigned long flags;
    unsigned short ch;
};
class MouseEvent {
public:
    enum Kind { MOUSE_DOWN = 1, MOUSE_UP, MOUSE_CLICK, MOUSE_DOUBLE_CLICK, MOUSE_MOVE, MOUSE_DRAG_START, MOUSE_DRAG_END,
                MOUSE_POSITION };
    unsigned long header;
    unsigned long buttons;  // bit per button held
    int x, y, dx, dy, z, dz;
};
JET_LAYOUT32(sizeof(MouseEvent) == 0x20);

// the observer bases: their virtuals do nothing (the original's identical-code folding shares the bodies)
class InputObserver {
public:
    InputObserver();
    InputObserver(const InputObserver& o);
    InputObserver& operator=(const InputObserver& o);

protected:
    friend class InputDevice;
    virtual InputEvent::Propagation __cdecl OnInputEvent(const UId& device, const InputEvent& e);
};

class InputActionObserver {
public:
    InputActionObserver();
    InputActionObserver(const InputActionObserver& o);
    InputActionObserver& operator=(const InputActionObserver& o);

protected:
    friend class InputActionMapper;
    // an action completed on `device` (InputActionMapper::OnInputEvent); STOP ends the notification
    virtual InputEvent::Propagation __cdecl OnInputAction(const UId& device, const PString& action, const InputEvent& e);
};

class InputUIExtensionObserver {
public:
    InputUIExtensionObserver();
    InputUIExtensionObserver(const InputUIExtensionObserver& o);
    InputUIExtensionObserver& operator=(const InputUIExtensionObserver& o);

    virtual void __cdecl OnInputGetEvent();                                           // 0 (InputUIExtension::Activate)
    virtual void __cdecl OnInputLostEvent();                                          // 1
    virtual InputEvent::Propagation __cdecl OnKeyboardEvent(const KeyboardEvent& e);  // 2
    virtual InputEvent::Propagation __cdecl OnCharEvent(const CharEvent& e);          // 3
    virtual InputEvent::Propagation __cdecl OnMouseEvent(const MouseEvent& e);        // 4 (NotifyMouseEvent)
};

// event flags passed with keys and buttons
enum {
    INPUT_DOWN = 0x10000,
    INPUT_UP = 0x20000,
    INPUT_REPEAT = 0x100000,
};

class InputDevice : public ObserverSubject<InputObserver>, public ReferenceCount {
public:
    InputDevice(InputDriver* driver);
    ~InputDevice() override;

    bool __cdecl IsValid() const;
    const UId& __cdecl GetInstanceId() const;
    unsigned long __cdecl GetTypeId() const;
    const InputData& __cdecl GetState() const;
    const InputData& __cdecl GetPrevState() const;
    void* __cdecl GetHandle();

    // the base versions do nothing (NullInputDevice keeps them)
    virtual void __cdecl GetCaps(InputDeviceCaps& caps) const;   // 4
    virtual bool __cdecl GetProperty(DeviceProperty& p);         // 5
    virtual bool __cdecl SetProperty(const DeviceProperty& p);   // 6
    virtual ForceTypeDesc* __cdecl EnumerateEffects();           // 7
    virtual void __cdecl StopAllEffects();                       // 8
    virtual void __cdecl ResetEffects();                         // 9
    virtual void __cdecl PauseEffects(bool on);                  // 10
    virtual void __cdecl MuteEffects(bool on);                   // 11
    virtual ForceEffect* __cdecl CreateEffect(const ForceInfo& info, unsigned long flags);  // 13 (overloads reverse)
    virtual ForceEffect* __cdecl CreateEffect(const PString& name, const ResourceDatabase* db,
                                              unsigned long flags);                         // 12
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
    friend class Input;
    friend class InputActionMapper;  // reads instanceId inline
    virtual void __cdecl Update(float dt);    // 14
    virtual void __cdecl Done();              // 15
    virtual void __cdecl CriticalShutdown();  // 16
    virtual void __cdecl CriticalStartup();   // 17

    void __cdecl KeyEvent(unsigned long key, int flags, unsigned long time);
    void __cdecl CharEvent(unsigned short ch, int flags, unsigned long time);
    void __cdecl ButtonEvent(int button, int flags, unsigned long time);
    void __cdecl AxisEvent(int axis, int value, int delta, unsigned long time);
    void __cdecl PovEvent(int pov, int value, unsigned long time);
    void __cdecl PropertyChangedEvent(DevicePropertyID id, unsigned long time);
    void __cdecl InputLostEvent(unsigned long time);
    void __cdecl InputGetEvent(unsigned long time);
    void __cdecl NotifyObservers(InputEvent& e, bool all);

    UId instanceId;          // +0x014 (the constructor leaves it as is; the driver sets it)
    InputData state;         // +0x024
    InputData prevState;     // +0x128
    InputDriver* driver;     // +0x22c holds a reference
    void* handle;            // +0x230
    bool valid;              // +0x234
};
JET_LAYOUT32(sizeof(InputDevice) == 0x238);

class KeyboardDevice : public InputDevice {
public:
    KeyboardDevice(InputDriver* driver);
    ~KeyboardDevice() override;
    bool __cdecl IsKeyPressed(unsigned char key) const;
    bool __cdecl IsKeyToggled(unsigned char key) const;
};
JET_LAYOUT32(sizeof(KeyboardDevice) == 0x238);

class MouseDevice : public InputDevice {
public:
    MouseDevice(InputDriver* driver);
    ~MouseDevice() override;
    int __cdecl GetRelX() const;
    int __cdecl GetRelY() const;
    int __cdecl GetRelZ() const;
    int __cdecl GetAbsX() const;
    int __cdecl GetAbsY() const;
    int __cdecl GetAbsZ() const;
    bool __cdecl IsButtonPressed(unsigned char button) const;
    void __cdecl SetPosition(const Point& p);
    void __cdecl SetBoundings(const Rect& r);
};
JET_LAYOUT32(sizeof(MouseDevice) == 0x238);

class JoystickDevice : public InputDevice {
public:
    JoystickDevice(InputDriver* driver);
    ~JoystickDevice() override;
    int __cdecl GetRelX() const;
    int __cdecl GetRelY() const;
    int __cdecl GetRelZ() const;
    int __cdecl GetRelRX() const;
    int __cdecl GetRelRY() const;
    int __cdecl GetRelRZ() const;
    int __cdecl GetRelU() const;
    int __cdecl GetRelV() const;
    int __cdecl GetAbsX() const;
    int __cdecl GetAbsY() const;
    int __cdecl GetAbsZ() const;
    int __cdecl GetAbsRX() const;
    int __cdecl GetAbsRY() const;
    int __cdecl GetAbsRZ() const;
    int __cdecl GetAbsU() const;
    int __cdecl GetAbsV() const;
    bool __cdecl IsButtonPressed(unsigned char button) const;
    unsigned char __cdecl GetButtonPressure(unsigned char button) const;
    int __cdecl GetPov(unsigned char pov) const;
};
JET_LAYOUT32(sizeof(JoystickDevice) == 0x238);

// a driver plugin (InputWin32, InputDirectX): reports devices and creates them. The base versions do nothing.
class InputDriver : public PluginInstance {
public:
    InputDriver();
    ~InputDriver() override;

    virtual bool __cdecl Init(OS* os);  // 0

protected:
    friend class Input;
    virtual void __cdecl Update(float dt);  // 1

public:
    virtual void __cdecl Enumerate(InputDeviceCapsList& list, int type);     // 2
    virtual AutoReference<InputDevice> __cdecl CreateDevice(const UId& id);  // 3

protected:
    virtual void __cdecl CriticalShutdown();  // 4
    virtual void __cdecl CriticalStartup();   // 5
};
JET_LAYOUT32(sizeof(InputDriver) == 0x18);  // vfptr, vbptr, factory, vtordisp, ReferenceCount

// a mouse or joystick played from the keyboard: key bindings press buttons, drive axes (accelerating while held,
// joystick axes may centre themselves) and set povs. Built by Input for UIdMouseEmu / UIdJoystickEmu.
class EmuDevice : public InputDevice, public InputObserver {
public:
    ~EmuDevice() override;
    void __cdecl GetCaps(InputDeviceCaps& caps) const override;
    bool __cdecl GetProperty(DeviceProperty& p) override;
    bool __cdecl SetProperty(const DeviceProperty& p) override;
    bool __cdecl Load(const char* file, const ResourceDatabase* db);
    void __cdecl LoadDefaults();

private:
    friend class Input;
    EmuDevice();
    bool __cdecl Init(Input* input, const UId& id, OS* os);
    void __cdecl Done() override;
    void __cdecl Update(float dt) override;
    InputEvent::Propagation __cdecl OnInputEvent(const UId& device, const InputEvent& e) override;
    void __cdecl Bind(unsigned char key, bool shift, bool ctrl, bool alt, unsigned char kind, unsigned char target,
                      unsigned long direction);
    void __cdecl InitAxis(unsigned char axis, int speed);
    void __cdecl UpdateAxis(unsigned char axis, float dt, unsigned long time);

    // a binding: byte 0 the key, byte 1 the target, bits 16..18 shift / ctrl / alt, bits 19..20 the kind (0 button,
    // 1 axis, 2 pov), bits 21.. the direction (axis: up when not 0; pov: hundredths of a degree / 100)
    struct KeyLess {
        bool operator()(unsigned long a, unsigned long b) const { return (a & 0xff) < (b & 0xff); }
    };
    using BindingList = FastDynamicArray<unsigned long, 13, KeyLess>;
    // the keyboard events not yet played (256; full: the oldest goes)
    struct EventQueue {
        unsigned long read, write, last, count;
        InputEvent* data;
        bool Push(const InputEvent& e);
    };
    struct Axis {
        int value;
        int start;          // the value when the key went down or up
        float time;         // since then
        int speed;
        unsigned char flags;  // bit 0 rising, 1 falling, 2 update this frame
    };
    struct Range {
        int unused;  // never written
        int min, max;
    };
    int& Min(int axis) { return ranges[axis].min; }
    int& Max(int axis) { return ranges[axis].max; }
    int Min(int axis) const { return ranges[axis].min; }
    int Max(int axis) const { return ranges[axis].max; }

    AutoReference<InputDevice> source;  // +0x23c the keyboard
    EventQueue queue;                   // +0x240
    BindingList bindings;               // +0x254 sorted by key
    unsigned long buttons;              // +0x258 the bound buttons held
    unsigned char autoCenter;           // +0x25c (the byte as set)
    Axis axes[8];                       // +0x260
    Range ranges[8];                    // +0x300
};
JET_LAYOUT32(sizeof(EmuDevice) == 0x360);

// a reference-counted observer that Input keeps and updates every frame (unit U6): InputActionMapper and
// InputUIExtension. The base virtuals do nothing; Init registers the extension with the Input.
class InputExtension : public InputObserver, public ReferenceCount {
public:
    InputExtension();
    ~InputExtension() override;
    virtual bool __cdecl Init(Input& input);       // 1
    virtual void __cdecl Activate(bool on);        // 2
    virtual void __cdecl SetExclusive(bool on);    // 3
    virtual void __cdecl Reset();                  // 4
    bool __cdecl IsActive() const;
    bool __cdecl GetExclusive() const;
    // the last reference goes through Done first
    bool ReleaseReference();
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
    friend class Input;
    virtual void __cdecl Done();              // 5
    virtual void __cdecl Update(float dt);    // 6

    bool active;     // +0x0c
    bool exclusive;  // +0x0d
};
JET_LAYOUT32(sizeof(InputExtension) == 0x10);

// the input manager: loads one driver per device type (the "InputKeyboard" / "InputMouse" / "InputJoystick"
// options, "Default" meaning Win32, Win32 and DirectX), hands out devices by id (creating them on first use; the
// emulated mouse and joystick are built here) and updates drivers, devices and extensions every frame
class Input {
public:
    enum DRIVER { DRIVER_KEYBOARD = 0, DRIVER_MOUSE = 1, DRIVER_JOYSTICK = 2 };

    Input();
    Input(const Input& o);
    ~Input();
    Input& operator=(const Input& o);

    bool __cdecl Init(OS* os);
    void __cdecl Done();
    bool __cdecl Update(float dt);
    bool __cdecl IsInitialized() const;
    bool __cdecl IsActiveWnd() const;
    InputDeviceCapsList* __cdecl EnumerateDevices(int typeMask, unsigned long capsMask);
    AutoReference<InputDevice> __cdecl GetDevice(const UId& id);

    // the devices handed out: a DynamicArray<AutoReference<InputDevice>, 13> (KernelStandard's layout) that also
    // inserts at the front and removes by iterator, moving the references along
    class DeviceList : public DynamicArray<AutoReference<InputDevice>, 13> {
    public:
        DeviceList() {}
        DeviceList(const DeviceList& o) : DynamicArray(o) {}
        void Push(const AutoReference<InputDevice>& item);                    // 0x10007eb0
        ConstIterator Remove(const ConstIterator& i);                         // 0x10007f90
    };
    using ExtensionList = FastDynamicArray<InputExtension*, 13>;

private:
    friend class InputExtension;
    void __cdecl RegisterExtension(InputExtension* ext);
    bool __cdecl LoadDriver(DRIVER type, OS* os, Options* options);
    void __cdecl EnumerateAll();
    int __cdecl FindDeviceType(const UId& id);
    void __cdecl CriticalShutdown();
    static int __cdecl CriticalShutdown(void* self);
    void __cdecl CriticalStartup();
    static int __cdecl CriticalStartup(void* self);

    static JET_IS_IMPORT bool isInstance;  // set while an Input exists

    const char* optionKeys[3];     // +0x00
    InputDriver* drivers[3];       // +0x0c the drivers loaded, each holding a reference
    InputDriver* typeDrivers[3];   // +0x18 keyboard, mouse, joystick (references)
    int driverCount;               // +0x24
    bool initialized;              // +0x28
    DeviceList devices;            // +0x2c
    InputDeviceCapsList caps;      // +0x30 what the drivers reported
    ExtensionList extensions;      // +0x34 (references)
    void* shutdownHandle;          // +0x38
    void* startupHandle;           // +0x3c
    OS* os;                        // +0x40
};
JET_LAYOUT32(sizeof(Input) == 0x44);

// the keyboard and mouse as a user interface sees them: tracks focus (lost / got for the keyboard and the mouse),
// the keys held, the modifier state, the mouse position and its buttons, and turns them into keyboard, character
// and mouse events (down, up, click, double click, move, drag start / end) for its observers
class InputUIExtension : public ObserverSubject<InputUIExtensionObserver>, public InputExtension {
public:
    InputUIExtension();
    ~InputUIExtension() override;
    bool __cdecl Init(Input& input) override;
    void __cdecl Activate(bool on) override;
    void __cdecl Reset() override;
    KeyboardDevice* __cdecl GetKeyboard();
    MouseDevice* __cdecl GetMouse();
    static JET_IS_IMPORT unsigned long doubleClickTime;  // ms (250)
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
    void __cdecl Done() override;
    void __cdecl Update(float dt) override;
    InputEvent::Propagation __cdecl OnInputEvent(const UId& device, const InputEvent& e) override;
    void ProcessEvent(const InputEvent& e);
    void __cdecl UpdateState();
    void __cdecl NotifyMouseEvent(const MouseEvent& e);
    void __cdecl UpdateMouseButton(const InputEvent& e);
    void __cdecl UpdateMouseMove(const InputEvent& e);

    // one mouse button: where its last click was, its last press, and bit 0 down, 1 a click waiting for a second
    // one, 2 dragging
    struct Button {
        int clickX, clickY;
        unsigned long clickTime;
        int downX, downY;
        unsigned long downTime;
        unsigned char flags;
    };
    enum { FOCUS_KEYBOARD = 0x01, INACTIVE = 0x02, FOCUS_MOUSE = 0x80, FOCUS_ALL = 0x81 };

    Button buttons[8];                    // +0x1c
    AutoReference<InputDevice> keyboard;  // +0xfc
    AutoReference<InputDevice> mouse;     // +0x100
    unsigned long modifiers;              // +0x104 bits 0..5 (UpdateState)
    unsigned char focus;                  // +0x108 FOCUS_* | INACTIVE
    bool attached;                        // +0x109 between Init and Done (the constructor leaves it as is)
    unsigned char unused10a[2];
    unsigned char keys[32];               // +0x10c one bit per key held
    unsigned long mouseButtons;           // +0x12c bit per button held
    int mouseX, mouseY;                   // +0x130
    int mouseDX, mouseDY;                 // +0x138 movement not reported yet
    int mouseZ, mouseDZ;                  // +0x140
};
JET_LAYOUT32(sizeof(InputUIExtension) == 0x148);

// ---- force feedback (unit U8)
// one effect's parameters, as DirectInput's DIEFFECT plus the type (0x74 bytes; no exported members)
class ForceInfo {
public:
    enum Type { CONSTANT = 1, RAMP = 2, SQUARE = 4, SINE, TRIANGLE, SAWTOOTH_UP, SAWTOOTH_DOWN, SPRING, DAMPER,
                INERTIA, FRICTION };  // 3 (custom force) is never read or written
    // what the original's constructors write; the other fields are left as they are
    ForceInfo()
        : unknown00(0), duration(-1), samplePeriod(0), gain(10000), triggerButton(-1), triggerRepeat(0),
          startDelay(0), hasEnvelope(false), unknown38(0), type(0), cartesian(0)
    {
        direction[0] = direction[1] = 0;
    }

    unsigned long unknown00;       // +0x00
    long duration;                 // +0x04 microseconds, -1 infinite
    unsigned long samplePeriod;    // +0x08
    unsigned long gain;            // +0x0c 0..10000
    long direction[2];             // +0x10
    long triggerButton;            // +0x18 -1 or 0..31
    unsigned long triggerRepeat;   // +0x1c
    unsigned long startDelay;      // +0x20
    bool hasEnvelope;              // +0x24
    unsigned char unused25[3];
    unsigned long attackLevel;     // +0x28
    unsigned long attackTime;      // +0x2c
    unsigned long fadeLevel;       // +0x30
    unsigned long fadeTime;        // +0x34
    unsigned long unknown38;       // +0x38
    long params[12];               // +0x3c the type's parameters (two conditions of six for the condition types)
    unsigned long type;            // +0x6c Type
    unsigned long cartesian;       // +0x70 1 cartesian, 0 polar directions
};
JET_LAYOUT32(sizeof(ForceInfo) == 0x74);

// an effect on a device; drivers derive the playing ones. The base forwards every operation to the effects attached
// to it (a compound effect made from a .ffe file holds one child per extra effect in the file).
class ForceEffect : public ReferenceCount {
public:
    ForceEffect(InputDevice* device);
    virtual bool __cdecl IsValid() const;                                         // 2
    virtual void __cdecl Invalidate();                                            // 3
    virtual bool __cdecl IsPlaying();                                             // 4
    virtual bool __cdecl IsLoaded();                                              // 5 (false)
    virtual bool __cdecl Start(unsigned long iterations, bool solo);              // 6
    virtual bool __cdecl Stop();                                                  // 7
    virtual bool __cdecl Update();                                                // 8
    virtual bool __cdecl Load();                                                  // 9
    virtual bool __cdecl Unload();                                                // 10
    // (MSVC puts overloads in reverse order: the file one is slot 11, the info one slot 12)
    virtual bool __cdecl Create(const ForceInfo& info, unsigned long flags);      // 12 (the base fails)
    virtual bool __cdecl Create(const PString& name, const ResourceDatabase* db, unsigned long flags);  // 11
    bool __cdecl Attach(ForceEffect* effect);
    bool __cdecl Detach(ForceEffect* effect);
    using EffectList = DynamicArray<ForceEffect*, 13, Less<ForceEffect*, ForceEffect*>>;
    const EffectList& __cdecl EnumerateAttachedEffects();
    ForceInfo& __cdecl GetInfo();
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
    ~ForceEffect() override;
    virtual void __cdecl CriticalShutdown();  // 13 releases the device

    using Children = FastDynamicArray<ForceEffect*, 13>;  // (the exported type above has the same layout)
    InputDevice* device;  // +0x08 holds a reference
    ForceInfo info;       // +0x0c
    Children effects;     // +0x80 holds references
    bool valid;           // +0x84
};
JET_LAYOUT32(sizeof(ForceEffect) == 0x88);

// a DirectInput force-feedback file (.ffe: RIFF "FORC", one LIST per effect) as a resource
class ForceResource : public Resource {
public:
    ForceResource(const PString& name, const ResourceDatabase* owner);
    ForceResource(const ForceResource& o);
    ~ForceResource() override;
    ForceResource& operator=(const ForceResource& o);
    const Array<ForceInfo, 1>& __cdecl GetEffectList() const;
    static bool __cdecl Save(const ForceEffect& effect, const char* file, const ResourceDatabase* db);
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
    Result __cdecl Load(const char* name) override;

private:
    int __cdecl LoadEffect(File& file, ForceInfo& info) const;  // 1 read, -1 skipped, 0 failed
    static bool __cdecl SaveEffect(File& file, const ForceInfo& info, unsigned long index);
    void Append(const ForceInfo& info);  // 0x10006df0 grows by one item
    void Assign(const Array<ForceInfo, 1>& o);  // 0x10006d70

    Array<ForceInfo, 1> effects;  // +0x5c
};
JET_LAYOUT32(sizeof(ForceResource) == 0x68);

// ---- actions (unit U7): named sequences of key / button / axis / pov strokes, matched against device events
// one stroke (8 bytes). `code`: bits 24..31 the kind (0 key, 1 button, 2 axis, 3 pov), 16..23 the key, button,
// axis or pov; a key: bit 7 pressed (else released), bits 2 / 1 / 0 shift / ctrl / alt; a button: bit 7 pressed;
// an axis: bits 8..9 the modification (AxisModification), 10..11 the direction (AxisDir); a pov: bits 12..15 the
// direction (PovDir, 8 centred). `time`: ms allowed since the previous stroke.
struct StrokeDef {
    unsigned long code;
    unsigned long time;
};
union InputExtensionState {
    unsigned long code;  // a StrokeDef's code
};
enum AxisModification { AXIS_CHANGED = 0, AXIS_INC = 1, AXIS_DEC = 2 };
enum AxisDir { AXIS_ANY = 0, AXIS_POSITIVE = 1, AXIS_NEGATIVE = 2 };

// an intrusive ring of T linked through T's IListLink (InputAction::link): the head and the count
struct IListLink {
    IListLink* next;
    IListLink* prev;
};
template <class T>
class IList {
public:
    IList() : count(0) { head.next = head.prev = &head; }
    IListLink head;       // +0
    unsigned long count;  // +8
};

class ConfigData;

class InputAction {
public:
    InputAction();
    InputAction(const InputAction& o);
    virtual ~InputAction();
    InputAction& __cdecl operator=(const InputAction& o);
    bool __cdecl operator==(const InputAction& o) const;
    PString __cdecl GetName() const;
    void __cdecl SetName(const char* name);
    void __cdecl Reset();
    unsigned int __cdecl Count();
    void __cdecl BeginAdd();
    void __cdecl EndAdd();
    void __cdecl SetFlush(bool on);
    void __cdecl AddKey(KeyCode key, bool shift, bool ctrl, bool alt, bool pressed, float time);
    void __cdecl AddButton(ButtonCode button, bool pressed, float time);
    void __cdecl AddAxis(AxisCode axis, AxisModification mod, AxisDir dir, float time);
    void __cdecl AddPov(PovCode pov, PovDir dir, float time);
    Array<StrokeDef, 2>* __cdecl GetStrokeList() const;  // a copy the caller owns
    void __cdecl SetStrokeList(const Array<StrokeDef, 2>& list);
    bool __cdecl TestDevice(const InputDevice* device) const;
    bool __cdecl TestDevice(const InputDeviceCaps& caps) const;
    JET_HEAP_OBJECT

protected:
    friend class InputActionMapper;
    void __cdecl Add(const InputExtensionState& state, float time);

    IListLink link;               // +0x04 in an IList<InputAction>
    PString name;                 // +0x0c
    bool flush;                   // +0x10 a completed action resets the device's other actions
    Array<StrokeDef, 2> strokes;  // +0x14
};
JET_LAYOUT32(sizeof(InputAction) == 0x20);

// an extension that turns the events of the devices added to it into actions: the action maps of Load
// ("actionmap" sections) or AddAction, kept by name; each device holds copies of the actions it can issue
class InputActionMapper : public ObserverSubject<InputActionObserver>, public InputExtension {
public:
    enum Results {};  // 2 invalid parameters, 3 partial support, 4 not supported, 5 file error
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    InputActionMapper();
    ~InputActionMapper() override;
    void __cdecl Activate(bool on) override;
    void __cdecl Reset() override;
    Result __cdecl AddDevice(InputDevice* device);
    void __cdecl RemoveDevice(const UId& id);
    void __cdecl RemoveDevice(InputDevice* device);
    void __cdecl RemoveAllDevices();
    bool __cdecl TestDevice(InputDevice* device) const;
    bool __cdecl TestDevice(InputDevice* device, const PString& action) const;
    Result __cdecl Load(const char* file, const ResourceDatabase* db, const char* mapName);
    Result __cdecl Save(const char* file, const ResourceDatabase* db, const char* mapName);
    Result __cdecl AddAction(const InputAction& action);
    void __cdecl RemoveAction(const InputAction& action);
    void __cdecl RemoveAllActions();
    InputDeviceCapsList* __cdecl EnumerateDevices();  // a new list the caller owns, or null
    IList<InputAction>* __cdecl EnumerateActions();    // copies in a new list the caller owns, or null
    JET_HEAP_OBJECT
    JET_HEAP_ARRAYS

protected:
    void __cdecl Done() override;
    InputEvent::Propagation __cdecl OnInputEvent(const UId& device, const InputEvent& e) override;
    bool __cdecl TestDevice(const InputDeviceCaps& caps, IList<InputAction>& list) const;
    bool __cdecl AddToActionList(const InputAction& action);
    Result __cdecl LoadActionMap(const ConfigData& map);

    // the actions of one name
    struct ActionList {
        ~ActionList();             // 0x1000bd20 deletes the actions
        JET_HEAP_OBJECT
        PString name;              // +0
        IList<InputAction> list;   // +4
    };
    // the action lists by name: 97 buckets (hash of the name's last 16 characters), each an array of lists
    struct ActionTable {
        using Bucket = FastDynamicArray<ActionList*, 4>;
        // {table, bucket, the element, the bucket's first chunk, the element's chunk}: 0x1000c4e0 / 0x1000c000
        struct Iterator {
            explicit Iterator(ActionTable& t) : table(&t), bucket(0), node(nullptr), head(nullptr), chunk(nullptr) {}
            void Begin();
            void Next();
            ActionTable* table;
            unsigned long bucket;
            ActionList** node;
            Bucket::Chunk* head;
            Bucket::Chunk* chunk;
        };
        ActionTable();
        ~ActionTable();
        unsigned long Index(PString name) const;     // 0x1000c7b0
        Iterator Find(PString name);                 // 0x1000c370 (node null when absent)
        void Insert(ActionList* list);               // 0x1000c280
        void Clear();                                // 0x1000c300

        Bucket* buckets;             // +0x00 (new[] of bucketCount)
        unsigned long count;         // +0x04
        unsigned long bucketCount;   // +0x08 97
    };
    // a copy of an action held by a device: how far it got and when (vftable 0x10016524)
    class DeviceAction : public InputAction {
    public:
        explicit DeviceAction(const InputAction& a) : InputAction(a), lastTime(0), progress(0) {}
        unsigned long lastTime;  // +0x20 time of the last stroke matched
        unsigned long progress;  // +0x24 strokes matched
    };
    // a device added: its actions and the axes' dead zones (0x58 bytes)
    struct Device {
        Device(InputDevice* device);  // 0x1000bdd0
        ~Device();                    // 0x1000be50
        void UpdateZones();           // 0x1000bf10
        JET_HEAP_OBJECT
        Device* next;                 // +0x00
        Device* prev;                 // +0x04
        InputDevice* device;          // +0x08 holds a reference
        IList<InputAction> actions;   // +0x0c DeviceActions
        struct {
            int lo, hi;
        } zones[8];                    // +0x18
    };
    struct DeviceList {
        Device* next;          // +0x28 a ring through the head
        Device* prev;
        unsigned long count;   // +0x30
        Device* Head() { return reinterpret_cast<Device*>(this); }
    };

    ActionTable table;    // +0x1c
    DeviceList devices;   // +0x28
};
JET_LAYOUT32(sizeof(InputActionMapper) == 0x34);

}  // namespace Jet
