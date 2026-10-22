// InputSystem_7c unit U6: InputUIExtension, the keyboard and mouse as a user interface sees them. Transcribed from
// the assembly (0x10010d10..0x10012320; the doubles 2.0 / 50.0 / 8.0 at 0x10017008..0x10017018).
#include <jet/InputSystem.h>

#include <cmath>
#include <cstring>

namespace Jet {

unsigned long InputUIExtension::doubleClickTime = 250;

namespace {

// 0x10010d10: a key event's flags (bits 16, 20, 24..28 of InputEvent::type) packed into the low byte
unsigned long KeyFlags(unsigned long t)
{
    unsigned long d = (t >> 2 & 0x8000000) | (t & 0x10000000);
    d = (d >> 2) | (t & 0x8000000);
    d = (d >> 3) | (t & 0x6000000);
    d = (d >> 3) | (t & 0x1000000);
    d = (d >> 5) | (t & 0x100000);
    d = (d >> 5) | (t & 0x10000);
    return d >> 9;
}

// the same for a character event (0x10011b6e), with a constant bit in the place of the key-down flag
unsigned long CharFlags(unsigned long t)
{
    unsigned long d = (t >> 2 & 0x8000000) | (t & 0x10000000);
    d = (d >> 2) | (t & 0x8000000);
    d = (d >> 3) | (t & 0x6000000) | 0x20000000;
    d = (d >> 3) | (t & 0x1000000);
    d = (d >> 5) | (t & 0x100000);
    return d >> 0xe;
}

// sqrt of an int sum of squares (the original's fild / fsqrt; the sum wraps like its imul / add)
double Distance(int dx, int dy)
{
    const int sum = static_cast<int>(static_cast<unsigned>(dx) * static_cast<unsigned>(dx) +
                                     static_cast<unsigned>(dy) * static_cast<unsigned>(dy));
    return std::sqrt(static_cast<double>(sum));
}

unsigned long WithKind(unsigned long header, unsigned long kind) { return (header & 0xFF00FFFF) | kind << 16; }

}  // namespace

// ---- construction
InputUIExtension::InputUIExtension()  // 0x100110a0
{
    for (Button& b : buttons) {
        b.downX = b.downY = 0;
        b.clickX = 0;
        b.clickY = 0;
        b.downTime = 0;
        b.clickTime = 0;
        b.flags &= ~7;
    }
    modifiers = 0;
    focus = FOCUS_ALL;
    mouseButtons = 0;
    mouseX = mouseY = mouseDX = mouseDY = mouseZ = mouseDZ = 0;
    keyboard = static_cast<InputDevice*>(nullptr);
    mouse = static_cast<InputDevice*>(nullptr);
    modifiers = 0;
    InputUIExtension::Reset();
}

InputUIExtension::~InputUIExtension() {}  // 0x10010dd0 (mouse, keyboard, the bases)

KeyboardDevice* InputUIExtension::GetKeyboard() { return static_cast<KeyboardDevice*>(keyboard.Get()); }  // 0x10010f30
MouseDevice* InputUIExtension::GetMouse() { return static_cast<MouseDevice*>(mouse.Get()); }              // 0x10010f40

// ---- start and stop
// takes the keyboard and the mouse from the Input and observes both
bool InputUIExtension::Init(Input& input)  // 0x10011310
{
    if (!input.IsInitialized()) return false;
    keyboard = input.GetDevice(UIdKeyboard);
    mouse = input.GetDevice(UIdMouse);
    if (keyboard.Get()) keyboard.Get()->AddObserver(this, 0);
    if (mouse.Get()) mouse.Get()->AddObserver(this, 0);
    UpdateState();
    Reset();
    attached = true;
    return InputExtension::Init(input);
}

void InputUIExtension::Done()  // 0x100114b0
{
    if (keyboard.Get()) {
        keyboard.Get()->RemoveObserver(this);
        keyboard = static_cast<InputDevice*>(nullptr);
    }
    if (mouse.Get()) {
        mouse.Get()->RemoveObserver(this);
        mouse = static_cast<InputDevice*>(nullptr);
    }
    attached = false;
}

// switching off tells the observers the input is lost (when both devices had the focus), switching on that it is back
void InputUIExtension::Activate(bool on)  // 0x10011540
{
    if (active) {
        if (on) return;
        active = false;
        if (focus == FOCUS_ALL) {
            Reset();
            LockObservers();
            for (ObserverIterator i = FirstObserver(); i.observer.node; ++i) (*i)->OnInputLostEvent();
            UnlockObservers();
        }
        focus |= INACTIVE;
    } else {
        if (!on) return;
        focus &= ~INACTIVE;
        active = true;
        if (focus == FOCUS_ALL) {
            Reset();
            LockObservers();
            for (ObserverIterator i = FirstObserver(); i.observer.node; ++i) (*i)->OnInputGetEvent();
            UnlockObservers();
        }
    }
}

// forgets the keys and buttons held and the movement not reported (the position stays)
void InputUIExtension::Reset()  // 0x10011640
{
    mouseButtons = 0;
    std::memset(keys, 0, sizeof(keys));
    for (Button& b : buttons) {
        b.downX = b.downY = 0;
        b.clickX = 0;
        b.clickY = b.downY;
        b.downTime = 0;
        b.clickTime = 0;
        b.flags &= ~7;
    }
    mouseDX = mouseDY = mouseDZ = 0;
}

// ---- state
// the modifiers from the keyboard's state: bit 0..2 either of two keys down, bits 3..5 three lock keys toggled
void InputUIExtension::UpdateState()  // 0x10010f50
{
    InputDevice* const k = keyboard.Get();
    if (!k) return;
    const unsigned char* const s = k->GetState().keyboard.keys;
    unsigned long m = modifiers;
    m = (m & ~1u) | ((s[0x39] & 0x80) || (s[0x5d] & 0x80) ? 1 : 0);
    m = (m & ~2u) | ((s[0x1e] & 0x80) || (s[0x5c] & 0x80) ? 2 : 0);
    m = (m & ~4u) | ((s[0x2b] & 0x80) || (s[0x37] & 0x80) ? 4 : 0);
    m = (m & ~8u) | (s[0x46] & 1u) << 3;
    m = (m & ~0x10u) | (s[0x47] & 1u) << 4;
    m = (m & ~0x20u) | (s[0x3b] & 1u) << 5;
    modifiers = m;
}

// movement since the last update goes out as one move event
void InputUIExtension::Update(float)  // 0x100117e0
{
    if (!attached) return;
    if (mouseDX || mouseDY || mouseDZ) {
        MouseEvent e;
        e.header = (modifiers & 0xFF05FF3F) | MouseEvent::MOUSE_MOVE << 16;
        e.buttons = mouseButtons;
        e.x = mouseX;
        e.y = mouseY;
        e.dx = mouseDX;
        e.dy = mouseDY;
        e.z = mouseZ;
        e.dz = mouseDZ;
        NotifyMouseEvent(e);
    }
    mouseDX = mouseDY = mouseDZ = 0;
    UpdateState();
}

// ---- events
// while inactive only focus changes and properties get through
InputEvent::Propagation InputUIExtension::OnInputEvent(const UId&, const InputEvent& e)  // 0x100118c0
{
    if (!active) {
        const int kind = e.type & 0xf0;
        if (kind != InputEvent::EVENT_LOST && kind != InputEvent::EVENT_GET && kind != InputEvent::EVENT_PROPERTY)
            return InputEvent::PROPAGATE;
    }
    ProcessEvent(e);
    return InputEvent::PROPAGATE;
}

// the observers until one stops the round (the first entry is taken as is, valid or not)
void InputUIExtension::NotifyMouseEvent(const MouseEvent& e)  // 0x10011020
{
    LockObservers();
    for (ObserverIterator i = FirstObserver(); i.observer.node; ++i)
        if ((*i)->OnMouseEvent(e) == InputEvent::STOP) break;
    UnlockObservers();
}

void InputUIExtension::ProcessEvent(const InputEvent& e)  // 0x100118f0
{
    const unsigned long type = static_cast<unsigned long>(e.type);
    switch (type & 0xf0) {
    case InputEvent::EVENT_LOST:
        Reset();
        if (focus == FOCUS_ALL) {
            LockObservers();
            for (ObserverIterator i = FirstObserver(); i.observer.node; ++i) (*i)->OnInputLostEvent();
            UnlockObservers();
        }
        if ((type & 0xf) == DEVICE_KEYBOARD)
            focus &= ~FOCUS_KEYBOARD;
        else
            focus &= ~FOCUS_MOUSE;
        break;
    case InputEvent::EVENT_GET:
        Reset();
        if ((type & 0xf) == DEVICE_KEYBOARD)
            focus |= FOCUS_KEYBOARD;
        else
            focus |= FOCUS_MOUSE;
        if (focus != FOCUS_ALL) break;
        LockObservers();
        for (ObserverIterator i = FirstObserver(); i.observer.node; ++i) (*i)->OnInputGetEvent();
        UnlockObservers();
        break;
    case InputEvent::EVENT_KEY: {
        if (focus != FOCUS_ALL) break;
        if ((type & 0xf) != DEVICE_KEYBOARD) {
            UpdateMouseButton(e);
            break;
        }
        const unsigned code = e.code;
        const bool repeat = (type >> 20 & 1) != 0;
        const bool pressed = (type >> 16 & 1) != 0;
        unsigned char& byte = keys[code >> 3];
        const unsigned char bit = static_cast<unsigned char>(1u << (code & 7));
        const bool down = (byte & bit) != 0;
        if (down && pressed && !repeat) break;
        if (!down && !pressed) break;
        if (!down && pressed && repeat) break;
        if (pressed)
            byte |= bit;
        else
            byte &= static_cast<unsigned char>(~bit);
        KeyboardEvent k;
        k.data = (static_cast<unsigned long>(code) << 16 & 0xFFFFFF00) | KeyFlags(type);
        LockObservers();
        for (ObserverIterator i = FirstObserver(); i.observer.node; ++i)
            if ((*i)->OnKeyboardEvent(k) == InputEvent::STOP) break;
        UnlockObservers();
        break;
    }
    case InputEvent::EVENT_CHAR: {
        if (focus != FOCUS_ALL) break;
        CharEvent c;
        c.flags = CharFlags(type);
        c.ch = e.ch;
        LockObservers();
        for (ObserverIterator i = FirstObserver(); i.observer.node; ++i)
            if ((*i)->OnCharEvent(c) == InputEvent::STOP) break;
        UnlockObservers();
        break;
    }
    case InputEvent::EVENT_AXIS:
        if (focus == FOCUS_ALL && (type & 0xf) == DEVICE_MOUSE) UpdateMouseMove(e);
        break;
    case InputEvent::EVENT_PROPERTY: {
        // the mouse was moved by a program: read its position and report it
        InputDevice* const m = mouse.Get();
        if ((type & 0xf) != DEVICE_MOUSE || !m || e.id != PROPERTY_POSITION) break;
        DeviceProperty p;
        p.id = PROPERTY_POSITION;
        p.index = 0;
        m->GetProperty(p);
        mouseDX = p.a - mouseX;
        mouseX = p.a;
        p.index = 1;
        m->GetProperty(p);
        mouseDY = p.a - mouseY;
        mouseY = p.a;
        p.index = 2;
        m->GetProperty(p);
        mouseDZ = p.a - mouseZ;
        mouseZ = p.a;
        MouseEvent ev;
        ev.header = (modifiers & 0xFF08FF3F) | 0x80000;
        ev.buttons = mouseButtons;
        ev.x = mouseX;
        ev.y = mouseY;
        ev.dx = mouseDX;
        ev.dy = mouseDY;
        ev.z = mouseZ;
        ev.dz = mouseDZ;
        NotifyMouseEvent(ev);
        mouseDX = mouseDY = mouseDZ = 0;
        break;
    }
    default:
        break;
    }
}

// a mouse button: down / up, then a click on release (a second one soon and near enough: a double click), or the
// end of a drag. The button index is not checked (the original's records run on past the eighth).
void InputUIExtension::UpdateMouseButton(const InputEvent& e)  // 0x10011e20
{
    Update(0.0f);  // (virtual) the movement so far goes out first
    const unsigned code = e.code;
    const bool pressed = !(static_cast<unsigned long>(e.type) >> 17 & 1);
    const unsigned long bit = 1u << (code & 31);
    const bool held = (mouseButtons & bit) != 0;
    if (held == pressed) return;
    if (pressed)
        mouseButtons |= bit;
    else
        mouseButtons &= ~bit;
    Button& b = buttons[code];
    if ((b.flags & 2) && b.clickTime + doubleClickTime < e.time) b.flags &= ~2;
    b.flags = static_cast<unsigned char>((b.flags & ~1) | (pressed ? 1 : 0));
    MouseEvent ev;
    ev.header = (code & 0x1f) << 24 | (pressed ? 1ul : 2ul) << 16 | (pressed ? 1ul : 0ul) << 7 | (modifiers & 0xff7f);
    ev.buttons = mouseButtons;
    ev.x = mouseX;
    ev.y = mouseY;
    ev.dx = mouseDX;
    ev.dy = mouseDY;
    ev.z = mouseZ;
    ev.dz = mouseDZ;
    NotifyMouseEvent(ev);
    mouseDX = mouseDY = mouseDZ = 0;
    if (pressed) {
        b.downTime = e.time;
        b.downX = mouseX;
        b.downY = mouseY;
    }
    const unsigned char fl = b.flags;
    if ((fl & 4) && !(fl & 1)) {  // a drag ends
        b.flags = fl & ~6;
        ev.header = WithKind(ev.header, MouseEvent::MOUSE_DRAG_END);
        ev.dy = ev.y - b.downY;
        ev.dx = ev.x - b.downX;
        ev.buttons = mouseButtons;
        NotifyMouseEvent(ev);
        return;
    }
    if (fl & 2) {  // a click is waiting for a second one
        if (pressed) return;
        if (Distance(mouseX - b.clickX, mouseY - b.clickY) < 8.0 && b.clickTime + doubleClickTime >= e.time) {
            b.flags = fl & ~2;
            ev.x = b.clickX;
            ev.y = b.clickY;
            ev.dy = mouseY - b.clickY;
            ev.dx = mouseX - b.clickX;
            ev.header = WithKind(ev.header, MouseEvent::MOUSE_DOUBLE_CLICK);
            NotifyMouseEvent(ev);
            return;
        }
        b.clickX = b.downX;
        b.clickY = b.downY;
        b.clickTime = e.time;
        ev.x = b.downX;
        ev.y = b.downY;
        ev.dx = mouseX - b.downX;
        ev.dy = mouseY - b.downY;
        ev.header = WithKind(ev.header, MouseEvent::MOUSE_CLICK);
        NotifyMouseEvent(ev);
        return;
    }
    if (pressed) {
        b.clickX = mouseX;
        b.clickY = mouseY;
        return;
    }
    b.clickTime = e.time;
    b.flags = fl | 2;
    ev.header = WithKind(ev.header, MouseEvent::MOUSE_CLICK);
    ev.x = b.downX;
    ev.y = b.downY;
    ev.dy = mouseY - b.downY;
    ev.dx = mouseX - b.downX;
    NotifyMouseEvent(ev);
}

// a mouse axis: the position, the movement to report at the next Update, and drags starting (a held button moved
// more than 50 pixels, or more than 2 after 200 ms) unless one is already going on
void InputUIExtension::UpdateMouseMove(const InputEvent& e)  // 0x10012180
{
    MouseEvent ev;
    ev.header = modifiers & 0xFFFFFF3F;
    ev.buttons = mouseButtons;
    ev.dx = ev.dy = ev.z = ev.dz = 0;
    switch (e.code) {
    case 0:
        mouseX = e.value;
        mouseDX += e.delta;
        break;
    case 1:
        mouseY = e.value;
        mouseDY += e.delta;
        break;
    case 2:
        mouseZ = e.value;
        mouseDZ += e.delta;
        break;
    }
    for (const Button& b : buttons)
        if (b.flags & 4) return;
    for (unsigned i = 0; i < 8; ++i) {
        Button& b = buttons[i];
        if ((b.flags & 2) && b.clickTime + doubleClickTime < e.time) b.flags &= ~2;
        const unsigned char fl = b.flags;
        if (!(fl & 1) || (fl & 4)) continue;
        const double d = Distance(mouseX - b.downX, mouseY - b.downY);
        if (!(d > 50.0) && !(d > 2.0 && b.downTime + 200 < e.time)) continue;
        b.flags = static_cast<unsigned char>((fl & ~2) | 4);
        ev.header = (i & 0x1f) << 24 | (ev.header & 0xffff) | 0x60000;
        ev.x = b.downX;
        ev.dx = mouseX - b.downX;
        ev.y = b.downY;
        ev.dy = mouseY - b.downY;
        NotifyMouseEvent(ev);
    }
}

}  // namespace Jet
