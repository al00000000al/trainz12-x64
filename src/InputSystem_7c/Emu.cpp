// InputSystem_7c unit U4: EmuDevice, a mouse or joystick played from the keyboard. Transcribed from the assembly
// (0x10002090..0x10003c48 and the helpers 0x10001870 queue push, 0x10005390 bit count, 0x100049d0 / 0x10005160
// binding lookup) and Load (0x10003c50, a config file read through the module's scanner, Lexer.cpp).
#include <jet/Config.h>
#include <jet/InputSystem.h>

#include "Lexer.h"

#include <cmath>
#include <cstring>
#include <new>

namespace Jet {

namespace {

// what the original does for `pstring = "text"`: drop the old node, intern the new text
void SetText(PString& s, const char* text)
{
    s.~PString();
    new (&s) PString(text);
}

// 0x10005390: a bit count with `(x & m) > s` where `(x & m) >> s` was meant (kept: GetCaps' counts come from it)
unsigned long BitCount(unsigned long x)
{
    x = static_cast<unsigned long>((x & 0xAAAAAAAAu) > 1) + (x & 0x55555555u);
    x = static_cast<unsigned long>((x & 0xCCCCCCCCu) > 2) + (x & 0x33333333u);
    x = static_cast<unsigned long>((x & 0xF0F0F0F0u) > 4) + (x & 0x0F0F0F0Fu);
    x = static_cast<unsigned long>((x & 0xFF00FF00u) > 8) + (x & 0x00FF00FFu);
    x = static_cast<unsigned long>((x & 0xFFFF0000u) > 16) + (x & 0x0000FFFFu);
    return x;
}

// float to int the way the original's fld / fistp does it: round to nearest even
int RoundToInt(float f) { return static_cast<int>(std::nearbyint(f)); }

// the modifiers of a key event (bits 27..29: shift, ctrl, alt) as a binding's bits 16..18
unsigned long Modifiers(int type)
{
    return ((((static_cast<unsigned long>(type) >> 29) & 1) << 2 | ((static_cast<unsigned long>(type) >> 28) & 1) << 1 |
             ((static_cast<unsigned long>(type) >> 27) & 1))
            << 16);
}

}  // namespace

// ---- the event queue
// 0x10001870: writes at `write`; when that catches up with `read` the oldest event goes and the result is false
bool EmuDevice::EventQueue::Push(const InputEvent& e)
{
    const unsigned long at = write;
    bool kept = true;
    write = at == last ? 0 : at + 1;
    if (write == read) {
        kept = false;
        read = read == last ? 0 : read + 1;
    }
    if (count <= last) ++count;
    data[at] = e;
    return kept;
}

// ---- construction
EmuDevice::EmuDevice() : InputDevice(nullptr), buttons(0), autoCenter(1)  // 0x10002800
{
    queue.read = 0;
    queue.write = 0;
    queue.last = 0xff;
    queue.count = 0;
    queue.data = static_cast<InputEvent*>(Alloc::New(0x100 * sizeof(InputEvent)));
    source = nullptr;
    std::memset(axes, 0, sizeof(axes));
}

EmuDevice::~EmuDevice()  // 0x10002090
{
    bindings.RemoveAll();
    if (queue.data) Alloc::Delete(queue.data);
}

// a keyboard-driven mouse (UIdMouseEmu) or joystick (UIdJoystickEmu); the bindings come from the files listed in the
// "InputEmulation" option, else the defaults
bool EmuDevice::Init(Input* input, const UId& id, OS* os)  // 0x10002900
{
    Done();
    if (!input->IsInitialized()) return false;
    instanceId = id;
    int type;
    if (std::memcmp(&id, &UIdMouseEmu, sizeof(UId)) == 0)
        type = DEVICE_MOUSE;
    else if (std::memcmp(&id, &UIdJoystickEmu, sizeof(UId)) == 0)
        type = DEVICE_JOYSTICK;
    else
        return false;
    state.type = type;
    prevState.type = type;
    source = input->GetDevice(UIdKeyboard);
    if (!source.Get()) {
        Done();
        return false;
    }
    bool loaded = false;
    if (os && os->GetOptions()) {
        os->GetOptions()->SetSeparators("InputEmulation", ",");
        do {
            PString file;
            if (!os->GetOptions()->GetNext("InputEmulation", file)) break;
            if (Load(file.Text(), nullptr)) loaded = true;
        } while (!loaded);
    }
    if (!loaded) LoadDefaults();
    for (int i = 0; i < 3; ++i) {
        Min(i) = 0;
        Max(i) = 0xffff;
        axes[i].value = state.type == DEVICE_MOUSE ? 0 : 0x7fff;
    }
    source.Get()->AddObserver(this, 0x7fffffff);
    valid = true;
    return true;
}

void EmuDevice::Done()  // 0x10002bf0
{
    if (source.Get()) {
        source.Get()->RemoveObserver(this);
        source = nullptr;
    }
    std::memset(axes, 0, sizeof(axes));
    state.type = 0;
    bindings.RemoveAll();
    handle = nullptr;
    valid = false;
}

// ---- bindings
// 0x100021b0. The original packs the low word of the key argument's stack slot, so the target byte is that slot's
// second byte (its `target` store is dead code): 0 from every caller here. Kept: the default joystick's second
// button and axis land on button / axis 0.
void EmuDevice::Bind(unsigned char key, bool shift, bool ctrl, bool alt, unsigned char kind, unsigned char,
                     unsigned long direction)
{
    unsigned long flags = (kind & 3) | (direction << 2);
    flags = (flags << 1) | (alt ? 1 : 0);
    flags = (flags << 1) | (ctrl ? 1 : 0);
    flags = (flags << 1) | (shift ? 1 : 0);
    bindings.Insert((flags << 16) | key);
}

void EmuDevice::InitAxis(unsigned char axis, int speed)  // 0x10002220
{
    axes[axis].flags &= ~3;
    axes[axis].speed = speed;
    axes[axis].value = 0;
    axes[axis].time = 0.0f;
}

// 0x10003a40: the mouse defaults are written as packed bindings (with their targets), the joystick's go through Bind
void EmuDevice::LoadDefaults()
{
    if (state.type == DEVICE_MOUSE) {
        bindings.Insert(0x2001D);
        bindings.Insert(0x4011D);
        bindings.Insert(0x80061);
        bindings.Insert(0x280062);
        InitAxis(0, 600);
        bindings.Insert(0x8015F);
        bindings.Insert(0x280164);
        InitAxis(1, 400);
        bindings.Insert(0x80260);
        bindings.Insert(0x280265);
        InitAxis(2, 120);
    } else if (state.type == DEVICE_JOYSTICK) {
        Bind(0x3a, false, true, false, 0, 0, 0);
        Bind(0x1d, false, false, true, 0, 1, 0);
        Bind(0x61, false, false, false, 1, 0, 0);
        Bind(0x62, false, false, false, 1, 0, 1);
        InitAxis(0, 500);
        Bind(0x5f, false, false, false, 1, 1, 0);
        Bind(0x64, false, false, false, 1, 1, 1);
        InitAxis(1, 500);
    }
}

// ---- loading bindings
namespace {

// the index of a name among `count` names of the table (names are interned: one node per text), or -1
int FindName(const PString* names, int count, const char* text)
{
    PString name(text);
    for (int i = 0; i < count; ++i)
        if (names[i] == name) return i;
    return -1;
}

// a key name: its code, 0 when unknown
int FindKey(const PString* names, const char* text)
{
    const int k = FindName(names, 0x100, text);
    return k < 0 ? 0 : k;
}

}  // namespace

// The section "MouseEmulator" or "JoystickEmulator" of a config file: each entry's key names a button, an axis or a
// pov, its values the key that drives it (with +shift / +ctrl / +alt), an axis' speed and direction (inc / dec), a
// pov's direction. Complete only when button 0 and both directions of the axes X and Y are bound. (An axis not found
// but given a speed writes past the axis table, kept.)
bool EmuDevice::Load(const char* file, const ResourceDatabase* db)  // 0x10003c50
{
    unsigned long bound = 0;
    ConfigParser parser;
    bool failed;
    if (db) {
        PString name(file);
        PString path = db->GetFileName(name);
        failed = parser.Parse(path.Text()).code != 0;
    } else {
        failed = parser.Parse(file).code != 0;
    }
    if (failed || !parser.RootPtr()) return false;
    const ConfigData& section =
        parser.Root()(PString(state.type == DEVICE_MOUSE ? "MouseEmulator" : "JoystickEmulator"));
    const PString* const names = InputStrings::Get().names;
    const int entries = section.GetChildCount();
    for (int i = 0; i < entries; ++i) {
        const ConfigData& entry = section[i];
        const PString& key = entry.Name();
        InputLex head(key.Text(), key.node ? key.Length() + 1 : 0);
        for (int t = head.Lex(); t > 0; t = head.Lex()) {
            if (t != InputLex::BUTTON_NAME && t != InputLex::AXIS_NAME && t != InputLex::POV_NAME) continue;
            const char* const target = head.Text();
            String text;
            bool shift = false, ctrl = false, alt = false;
            int keyCode = 0;
            if (t == InputLex::BUTTON_NAME) {
                int button = FindName(names + InputStrings::BUTTONS, 0x25, target);
                if (button >= 0x20) button -= 0x20;  // the mouse buttons
                if (state.type == DEVICE_MOUSE && button > 4) continue;
                JoinValues(entry, text);
                InputLex lex(static_cast<const char*>(text), text.Size());
                for (int v = lex.Lex(); v > 0; v = lex.Lex()) {
                    if (v == InputLex::SHIFT) shift = true;
                    else if (v == InputLex::CTRL) ctrl = true;
                    else if (v == InputLex::ALT) alt = true;
                    else if (v == InputLex::KEY_NAME) keyCode = FindKey(names, lex.Text());
                }
                if (!keyCode) continue;
                if (button == 0) bound |= 1;
                Bind(static_cast<unsigned char>(keyCode), shift, ctrl, alt, 0, static_cast<unsigned char>(button), 0);
            } else if (t == InputLex::AXIS_NAME) {
                const int axis = FindName(names + InputStrings::AXES, 8, target);
                if (state.type == DEVICE_MOUSE && axis > 2) continue;
                JoinValues(entry, text);
                InputLex lex(static_cast<const char*>(text), text.Size());
                int speed = -1;
                bool up = false;
                for (int v = lex.Lex(); v > 0; v = lex.Lex()) {
                    switch (v) {
                    case InputLex::INT:
                    case InputLex::HEX: speed = lex.IntValue(); break;
                    case InputLex::INC: up = true; break;
                    case InputLex::DEC: up = false; break;
                    case InputLex::SHIFT: shift = true; break;
                    case InputLex::CTRL: ctrl = true; break;
                    case InputLex::ALT: alt = true; break;
                    case InputLex::KEY_NAME: keyCode = FindKey(names, lex.Text()); break;
                    }
                }
                if (!keyCode) continue;
                if (axis == 0 || axis == 1) bound |= 1u << ((up ? 0 : 2) + 0x10 + axis);
                Bind(static_cast<unsigned char>(keyCode), shift, ctrl, alt, 1, static_cast<unsigned char>(axis), up);
                if (speed != -1) {  // InitAxis at the axis' low byte (255 when unknown: past the table)
                    unsigned char* const a = reinterpret_cast<unsigned char*>(this) + 0x260 + 0x14 * (axis & 0xff);
                    a[0x10] &= ~3;
                    *reinterpret_cast<int*>(a) = 0;
                    *reinterpret_cast<float*>(a + 8) = 0.0f;
                    *reinterpret_cast<int*>(a + 0xc) = speed;
                }
            } else {
                const int pov = FindName(names + InputStrings::POVS, 4, target);
                JoinValues(entry, text);
                InputLex lex(static_cast<const char*>(text), text.Size());
                unsigned long direction = 0xffffffff;
                for (int v = lex.Lex(); v > 0; v = lex.Lex()) {
                    if (v == InputLex::INT || v == InputLex::HEX) direction = static_cast<unsigned long>(lex.IntValue());
                    else if (v >= InputLex::POV_N && v <= InputLex::POV_N + 7) direction = 4500ul * (v - InputLex::POV_N);
                    else if (v == InputLex::SHIFT) shift = true;
                    else if (v == InputLex::CTRL) ctrl = true;
                    else if (v == InputLex::ALT) alt = true;
                    else if (v == InputLex::KEY_NAME) keyCode = FindKey(names, lex.Text());
                }
                if (keyCode && direction < 36000)
                    Bind(static_cast<unsigned char>(keyCode), shift, ctrl, alt, 2, static_cast<unsigned char>(pov),
                         direction);
            }
        }
    }
    return bound == 0xF0001;
}

// ---- caps and properties
void EmuDevice::GetCaps(InputDeviceCaps& c) const  // 0x10002ca0
{
    c.type = state.type;
    c.caps = 0xCC002801;
    c.buttonMask = 0;
    c.axisMask = 0;
    c.unknown34 = 0;
    if (state.type == DEVICE_MOUSE || state.type == DEVICE_JOYSTICK) {
        const bool mouse = state.type == DEVICE_MOUSE;
        c.id = mouse ? UIdMouseEmu : UIdJoystickEmu;
        SetText(c.name, mouse ? "Mouse emulator" : "Joystick emulator");
        SetText(c.description, mouse ? "Mouse emulator" : "Joystick emulator");
        for (BindingList::ConstIterator i = bindings.First(); i.node; ++i) {
            const unsigned long b = *i;
            const unsigned long bit = 1u << (((b >> 8) & 0xff) & 31);
            switch ((b >> 19) & 3) {
            case 0: c.buttonMask |= bit; break;
            case 1: c.axisMask |= bit; break;
            case 2:
                if (!mouse) c.unknown34 |= bit;
                break;
            }
        }
        c.axisMask &= 0xff;
        if (!mouse) c.unknown34 &= 0xf;
        c.numButtons = BitCount(c.buttonMask);
        c.numAxes = BitCount(c.axisMask);
        c.numPovs = mouse ? 0 : BitCount(c.unknown34);
    } else {
        c.id = UIdUnknown;
        SetText(c.name, "");
        SetText(c.description, "");
        c.caps = 0;
        c.numButtons = 0;
        c.numAxes = 0;
        c.numPovs = 0;
    }
}

bool EmuDevice::GetProperty(DeviceProperty& p)  // 0x10002fb0
{
    if (!valid) return false;
    switch (p.id) {
    case PROPERTY_RANGE:
        if (p.index >= 8) return false;
        p.a = Min(p.index);
        p.b = Max(p.index);
        return true;
    case PROPERTY_POSITION:
        if (state.type == DEVICE_MOUSE) {
            if (p.index >= 3) return false;
            p.a = state.mouse.position[p.index];
            return true;
        }
        if (state.type == DEVICE_JOYSTICK) {
            if (p.index >= 8) return false;
            p.a = state.joystick.position[p.index];
            return true;
        }
        return false;
    case PROPERTY_AUTOCENTER:
        p.index = autoCenter;
        return true;
    default:
        return false;
    }
}

bool EmuDevice::SetProperty(const DeviceProperty& p)  // 0x10003050
{
    if (!valid) return false;
    const int i = p.index;
    switch (p.id) {
    case PROPERTY_AUTOCENTER:
        autoCenter = p.index;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    case PROPERTY_POSITION: {
        int* position;
        if (state.type == DEVICE_MOUSE) {
            if (i >= 3) return false;
            position = &state.mouse.position[i];
        } else if (state.type == DEVICE_JOYSTICK) {
            if (i >= 8) return false;
            position = &state.joystick.position[i];
        } else {
            return false;
        }
        *position = p.a > Max(i) ? Max(i) : p.a < Min(i) ? Min(i) : p.a;
        axes[i].value = *position;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    }
    case PROPERTY_RANGE:
        if (i >= 8) return false;
        Min(i) = p.a;
        Max(i) = p.b;
        if (state.type == DEVICE_MOUSE) {
            int v = Min(i) >= axes[i].value ? Min(i) : axes[i].value;
            if (v > p.b) v = p.b;
            axes[i].value = v;
            if (i < 3) state.mouse.position[i] = v;
        } else if (state.type == DEVICE_JOYSTICK) {
            const int centre = (p.b - Min(i)) / 2;
            axes[i].value = centre;
            state.joystick.position[i] = centre;
        }
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    default:
        return false;
    }
}

// ---- playing the keyboard
// keyboard events are queued and played at the next Update; repeats are ignored. A full queue starts over with a
// lost / got pair before the event.
InputEvent::Propagation EmuDevice::OnInputEvent(const UId&, const InputEvent& e)  // 0x100039b0
{
    if (e.type & INPUT_REPEAT) return InputEvent::PROPAGATE;
    if (!queue.Push(e)) {
        queue.count = 0;
        queue.write = 0;
        queue.read = 0;
        InputEvent marker;
        marker.type = state.type | InputEvent::EVENT_LOST;
        marker.time = e.time;
        queue.Push(marker);
        marker.type = state.type | InputEvent::EVENT_GET;
        marker.time = e.time;
        queue.Push(marker);
        queue.Push(e);
    }
    return InputEvent::PROPAGATE;
}

// an axis moving while its key is held: start +- speed * t^2 / 2 within the range; a released joystick axis with
// auto-centre returns to the middle at speed * (t / 2 + 1.5) * t
void EmuDevice::UpdateAxis(unsigned char axis, float dt, unsigned long time)  // 0x100036f0
{
    Axis& a = axes[axis];
    if (!(a.flags & 4)) return;
    if (a.flags & 1) {
        if (a.value >= Max(axis)) return;
        a.time = dt + a.time;
        a.value = a.start + RoundToInt(static_cast<float>(a.speed) * a.time * a.time * 0.5f);
        if (a.value > Max(axis)) a.value = Max(axis);
    } else if (a.flags & 2) {
        if (a.value <= Min(axis)) return;
        a.time = dt + a.time;
        a.value = a.start - RoundToInt(static_cast<float>(a.speed) * a.time * a.time * 0.5f);
        if (a.value < Min(axis)) a.value = Min(axis);
    } else {
        if (state.type != DEVICE_JOYSTICK || !autoCenter) return;
        const int centre = (Max(axis) - Min(axis)) / 2;
        if (a.value == centre) return;
        a.time = dt + a.time;
        if (centre < a.start) {
            a.value = a.start - RoundToInt(static_cast<float>(a.speed) * (a.time * 0.5f + 1.5f) * a.time);
            if (a.value < centre) a.value = centre;
        } else if (centre > a.start) {
            a.value = a.start + RoundToInt(static_cast<float>(a.speed) * (a.time * 0.5f + 1.5f) * a.time);
            if (a.value > centre) a.value = centre;
        } else {
            return;
        }
    }
    if (state.type == DEVICE_MOUSE && axis < 3) {
        const int delta = a.value - state.mouse.position[axis];
        state.mouse.delta[axis] = delta;
        state.mouse.position[axis] = a.value;
        AxisEvent(axis, a.value, delta, time);
    } else if (state.type == DEVICE_JOYSTICK) {
        state.joystick.delta[axis] = a.value - state.joystick.position[axis];
        state.joystick.position[axis] = a.value;
        // the original reports the mouse's fields here (bytes of the joystick buttons / pressure); kept
        int value, delta;
        std::memcpy(&value, state.raw + 0x18 + 4 * axis, 4);
        std::memcpy(&delta, state.raw + 0x0c + 4 * axis, 4);
        AxisEvent(axis, value, delta, time);
    }
}

void EmuDevice::Update(float dt)  // 0x10003230
{
    if (!valid || !state.type) return;
    const unsigned long time = Time::Ms();
    int old[8];
    if (state.type == DEVICE_MOUSE)
        std::memcpy(old, state.mouse.position, 3 * sizeof(int));
    else
        std::memcpy(old, state.joystick.position, 8 * sizeof(int));
    prevState = state;
    for (unsigned char i = 0; i < 8; ++i) {
        axes[i].flags |= 4;
        UpdateAxis(i, dt, time);
        axes[i].flags &= ~4;
    }
    for (;;) {
        const InputEvent* e = queue.count ? &queue.data[queue.read] : nullptr;
        if (queue.count) {
            queue.read = queue.read == queue.last ? 0 : queue.read + 1;
            --queue.count;
        }
        if (!e) break;
        const int kind = e->type & 0xf0;
        if (kind == InputEvent::EVENT_LOST) {
            std::memset(&state, 0, 0x100);  // (not the last 4 bytes) and the type too: the event goes nowhere
            buttons = 0;
            for (Axis& a : axes) a.flags &= ~3;
            InputEvent lost;
            lost.type = InputEvent::EVENT_LOST;
            lost.time = time;
            NotifyObservers(lost, true);
            continue;
        }
        if (kind == InputEvent::EVENT_GET) {
            InputEvent got;
            got.type = InputEvent::EVENT_GET;
            got.time = time;
            NotifyObservers(got, true);
            continue;
        }
        if (kind != InputEvent::EVENT_KEY) continue;
        const unsigned long code = e->code;
        const unsigned long lookup = Modifiers(e->type) | code;
        // 0x100049d0: the first chunk whose last key is not below, then a binary search in it for the first equal key
        BindingList::Chunk* const head = bindings.head;
        BindingList::Chunk* c = head;
        unsigned long* found = nullptr;
        if (c) {
            while ((c->data[c->count - 1] & 0xff) < code) {
                c = c->next;
                if (c == head) {
                    c = nullptr;
                    break;
                }
            }
        }
        if (c) {
            int lo = 0, hi = static_cast<int>(c->count) - 1;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                const unsigned long k = c->data[mid] & 0xff;
                if (k < code) {
                    lo = mid + 1;
                } else if (k == code) {
                    while (mid > lo && (c->data[mid - 1] & 0xff) == code) --mid;
                    found = &c->data[mid];
                    break;
                } else {
                    hi = mid - 1;
                }
            }
        }
        if (!found) continue;
        const bool pressed = !(e->type & INPUT_UP);
        for (BindingList::Iterator it(found, head, c); it.node && (*it.node & 0xff) == code; ++it) {
            const unsigned long b = *it.node;
            if (pressed && ((b ^ lookup) & 0x70000)) continue;  // the modifiers must match a press
            const unsigned int target = (b >> 8) & 0xff;
            switch ((b >> 19) & 3) {
            case 2:  // pov
                if (state.type != DEVICE_JOYSTICK || target > 3) break;
                state.joystick.pov[target] = pressed ? static_cast<int>(((b >> 21) * 100) % 36000) : -1;
                PovEvent(static_cast<int>(target), state.joystick.pov[target], time);
                break;
            case 1: {  // axis
                if (target > 7) break;
                Axis& a = axes[target];
                const bool up = (b & 0xFFE00000) != 0;
                if (pressed)
                    a.flags = static_cast<unsigned char>((a.flags & ~3) | (up ? 1 : 2));
                else
                    a.flags &= up ? ~1 : ~2;
                a.start = a.value;
                a.time = 0.0f;
                a.flags |= 4;
                break;
            }
            case 0: {  // button
                const unsigned long bit = 1u << (target & 31);
                const bool held = (buttons & bit) != 0;
                if (held == pressed) break;
                if (pressed)
                    buttons |= bit;
                else
                    buttons &= ~bit;
                if (state.type == DEVICE_MOUSE) {
                    state.raw[4 + target] = pressed ? 0x80 : 0;
                } else if (state.type == DEVICE_JOYSTICK) {
                    state.raw[4 + target] = pressed ? 0x80 : 0;
                    state.raw[0x24 + target] = state.raw[4 + target] ? 0xff : 0;
                } else {
                    break;
                }
                ButtonEvent(static_cast<int>(target), pressed ? INPUT_DOWN : INPUT_UP, time);
                break;
            }
            }
        }
    }
    for (unsigned char i = 0; i < 8; ++i) {
        UpdateAxis(i, dt, time);
        if (state.type == DEVICE_MOUSE) {
            if (i < 3) state.mouse.delta[i] = state.mouse.position[i] - old[i];
        } else if (state.type == DEVICE_JOYSTICK) {
            state.joystick.delta[i] = state.joystick.position[i] - old[i];
        }
    }
}

}  // namespace Jet
