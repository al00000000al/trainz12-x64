// InputWin32_7c: the "Win32" input driver plugin. Keyboard and mouse come from window messages (through a Window
// hook) and GetKeyboardState / GetKeyState polling; joysticks from the WinMM joy* API. Transcribed from the
// assembly; InputDevice and InputDriver live in InputSystem_7c, Window and OS in KernelStandard_7c.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <jet/InputSystem.h>
#include <jet/Window.h>

#include <emmintrin.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "winmm.lib")
#pragma warning(disable : 4996)  // GetVersionExA

namespace Jet {

namespace {

// Windows virtual key -> Jet key code, 0xff for none (built at load time by the original, 0x10002e00)
const unsigned char kVkToKey[256] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x10, 0xff, 0xff, 0xff, 0x1d, 0xff, 0xff,  // 0x00
    0x2b, 0x1e, 0x39, 0x02, 0x3b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0xff,  // 0x10
    0x3a, 0x60, 0x65, 0x63, 0x5e, 0x61, 0x5f, 0x62, 0x64, 0xff, 0xff, 0xff, 0x6b, 0x66, 0x67, 0xff,  // 0x20
    0x0c, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 0x30
    0xff, 0x1f, 0x31, 0x2f, 0x21, 0x13, 0x22, 0x23, 0x24, 0x18, 0x25, 0x26, 0x27, 0x33, 0x32, 0x19,  // 0x40
    0x1a, 0x11, 0x14, 0x20, 0x15, 0x17, 0x30, 0x12, 0x2e, 0x16, 0x2d, 0x68, 0x69, 0x6a, 0xff, 0xff,  // 0x50
    0x53, 0x50, 0x51, 0x52, 0x4c, 0x4d, 0x4e, 0x48, 0x49, 0x4a, 0x38, 0x4f, 0xff, 0x4b, 0x54, 0x5b,  // 0x60
    0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x55, 0x56, 0x57, 0x58, 0x59, 0xff,  // 0x70
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 0x80
    0x46, 0x47, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 0x90
    0x2b, 0x37, 0x1e, 0x5c, 0x39, 0x5d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 0xa0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0x0e, 0x34, 0x0d, 0x35, 0x36,  // 0xb0
    0x2a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 0xc0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1b, 0x2c, 0x1c, 0x29, 0xff,  // 0xd0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 0xe0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // 0xf0
};

// the Jet keys of the left and right Shift, Ctrl and Alt
enum { KEY_LSHIFT = 0x2b, KEY_RSHIFT = 0x37, KEY_LCONTROL = 0x1e, KEY_RCONTROL = 0x5c, KEY_LALT = 0x39, KEY_RALT = 0x5d };

// the message registered for old wheel mice (MSWHEEL_ROLLMSG), 0 until a mouse first asked (0x100075d0)
UINT g_wheelMessage;

// what the original does for `pstring = "text"`: drop the old node, intern the new text
void SetText(PString& s, const char* text)
{
    s.~PString();
    new (&s) PString(text);
}

unsigned long Bits(unsigned long count)
{
    unsigned long mask = 0;
    for (unsigned long i = 0; i < count; ++i) mask |= 1u << i;
    return mask;
}

void FillKeyboardCaps(InputDeviceCaps& c)  // 0x10002580
{
    c.type = DEVICE_KEYBOARD;
    c.id = UIdKeyboard;
    SetText(c.name, "Keyboard");
    SetText(c.description, "System keyboard");
    c.numAxes = 0;
    c.axisMask = 0;
    c.numPovs = 0;
    c.unknown34 = 0;
    c.caps = 0xDC000101;
    c.numButtons = 0x100;
    c.buttonMask = 0xffffffff;
}

void FillMouseCaps(InputDeviceCaps& c)  // 0x100033d0
{
    c.type = DEVICE_MOUSE;
    c.id = UIdMouse;
    SetText(c.name, "Mouse");
    SetText(c.description, "System mouse");
    c.caps = static_cast<unsigned long>(GetSystemMetrics(SM_MOUSEPRESENT) != 0) | 0xDC023800;
    OSVERSIONINFOA v;
    v.dwOSVersionInfoSize = sizeof(v);
    const BOOL ok = GetVersionExA(&v);
    c.numAxes = 2;
    if (!ok) {
        c.numButtons = 3;
    } else {
        c.numButtons = v.dwMajorVersion < 5 ? 3 : 5;
        if (v.dwMajorVersion >= 4 && (v.dwPlatformId == VER_PLATFORM_WIN32_NT ||
                                      (v.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS && v.dwMinorVersion > 0))) {
            c.numAxes += GetSystemMetrics(SM_MOUSEWHEELPRESENT) != 0;
        } else if (g_wheelMessage == 0) {
            const UINT support = RegisterWindowMessageA("MSH_WHEELSUPPORT_MSG");
            g_wheelMessage = RegisterWindowMessageA("MSWHEEL_ROLLMSG");
            const HWND w = FindWindowA("MouseZ", "Magellan MSWHEEL");
            if (w && support) c.numAxes += SendMessageA(w, support, 0, 0) != 0;
        } else {
            c.numAxes = 3;
        }
    }
    c.buttonMask = Bits(c.numButtons);
    c.axisMask = Bits(c.numAxes);
    c.unknown34 = 0;
    c.numPovs = 0;
}

bool FillJoystickCaps(unsigned int joystick, InputDeviceCaps& c)  // 0x10001df0
{
    JOYCAPSA caps;
    if (joyGetDevCapsA(joystick, &caps, sizeof(caps)) != JOYERR_NOERROR) return false;
    c.type = DEVICE_JOYSTICK;
    SetText(c.name, joystick ? "Joystick2" : "Joystick1");
    c.caps = 0x7C003E00;
    c.id = joystick ? UIdJoystick2 : UIdJoystick1;
    SetText(c.description, caps.szPname);
    JOYINFO info;
    if (joyGetPos(joystick, &info) == JOYERR_NOERROR) c.caps |= 1;
    c.numButtons = caps.wNumButtons;
    c.buttonMask = Bits(caps.wNumButtons);
    c.numAxes = caps.wNumAxes;
    // x, y always; z, r, u, v from the caps bits (axes 3 and 4 are never used)
    c.axisMask = (((caps.wCaps & 0xe) << 4) | ((caps.wCaps & JOYCAPS_HASZ) << 2)) | 3;
    c.numPovs = (caps.wCaps >> 4) & 1;
    c.unknown34 = c.numPovs;
    return true;
}

// Scroll, Num and Caps Lock state, Alt, Ctrl and Shift down, in bits 20..27 of an event's flags
int Modifiers()
{
    int m = (GetKeyState(VK_SCROLL) & 1) << 1;
    m = (m | (GetKeyState(VK_NUMLOCK) & 1)) << 1;
    m = (m | (GetKeyState(VK_CAPITAL) & 1)) << 2;
    m = (m | (GetKeyState(VK_MENU) & 0x80)) << 1;
    m = (m | (GetKeyState(VK_CONTROL) & 0x80)) << 1;
    m |= GetKeyState(VK_SHIFT) & 0x80;
    return m << 20;
}

#ifdef IW32_TRACE  // off by default: logs what the game creates and the events that go through (in-game checks)
void Trace(const char* fmt, ...)
{
    FILE* f = std::fopen("D:/trainz12-decomp/probe/iw32_trace.txt", "a");
    if (!f) return;
    va_list a;
    va_start(a, fmt);
    std::vfprintf(f, fmt, a);
    va_end(a);
    std::fputc('\n', f);
    std::fclose(f);
}
#define TRACE(...) Trace(__VA_ARGS__)
#else
#define TRACE(...) ((void)0)
#endif

}  // namespace

// ---- keyboard (0x240 bytes)
class Win32Keyboard : public InputDevice {
public:
    Win32Keyboard(InputDriver* driver, OS* os);
    ~Win32Keyboard() override;
    void __cdecl GetCaps(InputDeviceCaps& caps) const override { FillKeyboardCaps(caps); }
    bool __cdecl GetProperty(DeviceProperty& p) override;
    bool __cdecl SetProperty(const DeviceProperty& p) override;

protected:
    void __cdecl Update(float dt) override;
    void __cdecl CriticalShutdown() override;
    void __cdecl CriticalStartup() override;

private:
    static bool __fastcall Hook(void* self, void* message, int& result);
    void Key(uintptr_t vk, int flags);

    Window* window;               // +0x238
    unsigned char keyboardFlags;  // +0x23c bit 0: Windows 9x (no separate left / right modifier keys)
};
JET_LAYOUT32(sizeof(Win32Keyboard) == 0x240);

Win32Keyboard::Win32Keyboard(InputDriver* driver, OS* os) : InputDevice(driver)  // 0x100023d0
{
    window = os->GetWindow();
    window->RegisterWindowHook(&Hook, this);
    prevState.type = DEVICE_KEYBOARD;
    state.type = DEVICE_KEYBOARD;
    instanceId = UIdKeyboard;
    OSVERSIONINFOA v;
    v.dwOSVersionInfoSize = sizeof(v);
    GetVersionExA(&v);
    keyboardFlags = (keyboardFlags & ~1) | (v.dwPlatformId != VER_PLATFORM_WIN32_NT);
}

Win32Keyboard::~Win32Keyboard()  // 0x10002510
{
    window->ClearWindowHook(this);
}

void Win32Keyboard::CriticalShutdown()  // 0x10002dc0
{
    if (window) window->ClearWindowHook(this);
}

void Win32Keyboard::CriticalStartup()  // 0x10002de0
{
    if (window) window->RegisterWindowHook(&Hook, this);
}

namespace {
// the lock key a property names: Caps, Num or Scroll Lock (by Jet key code)
int LockKeyVk(unsigned char key)
{
    switch (key) {
    case 0x3b: return VK_CAPITAL;
    case 0x46: return VK_NUMLOCK;
    case 0x47: return VK_SCROLL;
    default: return 0;
    }
}
}  // namespace

bool Win32Keyboard::GetProperty(DeviceProperty& p)  // 0x10002630
{
    if (!valid || p.id != PROPERTY_LOCK_KEY) return false;
    const int vk = LockKeyVk(p.index);
    if (!vk) return false;
    p.state = GetKeyState(vk) & 1;
    return true;
}

// toggles the lock key with a synthesized press and release when it differs
bool Win32Keyboard::SetProperty(const DeviceProperty& p)  // 0x100026a0
{
    if (!valid || p.id != PROPERTY_LOCK_KEY) return false;
    const int vk = LockKeyVk(p.index);
    if (!vk) return false;
    if ((GetKeyState(vk) & 1) != p.state) {
        INPUT in[2];
        memset(in, 0, sizeof(in));
        in[0].type = in[1].type = INPUT_KEYBOARD;
        in[0].ki.wVk = in[1].ki.wVk = static_cast<WORD>(vk);
        in[0].ki.time = in[1].ki.time = timeGetTime();
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
    }
    PropertyChangedEvent(p.id, Time::Ms());
    return true;
}

void Win32Keyboard::Update(float)  // 0x10002790
{
    if (!valid) return;
    prevState = state;
    unsigned char kb[256];
    if (!GetKeyboardState(kb)) return;
    kb[VK_LBUTTON] = kb[VK_RBUTTON] = kb[VK_MBUTTON] = kb[VK_XBUTTON1] = kb[VK_XBUTTON2] = 0;
    if (keyboardFlags & 1) {
        kb[VK_LSHIFT] = kb[VK_RSHIFT] = kb[VK_SHIFT];
        kb[VK_LCONTROL] = kb[VK_RCONTROL] = kb[VK_CONTROL];
        kb[VK_LMENU] = kb[VK_RMENU] = kb[VK_MENU];
    }
    for (int vk = 0; vk < 256; ++vk) {
        const unsigned char key = kVkToKey[vk];
        if (key != 0xff) state.keyboard.keys[key] = kb[vk] & 0x81;
    }
}

void Win32Keyboard::Key(uintptr_t vk, int flags)  // 0x10002d40
{
    const unsigned char key = kVkToKey[vk & 0xff];
    if (key == 0xff) return;
    const unsigned char s = static_cast<unsigned char>(GetKeyState(static_cast<int>(vk)));
    state.keyboard.keys[key] = s;
    if (keyboardFlags & 1) {
        switch (vk) {
        case VK_SHIFT: state.keyboard.keys[KEY_RSHIFT] = state.keyboard.keys[KEY_LSHIFT] = s; break;
        case VK_CONTROL: state.keyboard.keys[KEY_RCONTROL] = state.keyboard.keys[KEY_LCONTROL] = s; break;
        case VK_MENU: state.keyboard.keys[KEY_RALT] = state.keyboard.keys[KEY_LALT] = s; break;
        }
    }
    TRACE("key vk %x key %x flags %x", static_cast<unsigned>(vk), key, flags);
    KeyEvent(key, flags, Time::Ms());
}

bool __fastcall Win32Keyboard::Hook(void* self, void* message, int&)  // 0x10002910
{
    Win32Keyboard* const k = static_cast<Win32Keyboard*>(self);
    const Window::Message* const m = static_cast<const Window::Message*>(message);
    const unsigned int lParam = static_cast<unsigned int>(m->lParam);
    switch (m->id) {
    case WM_ACTIVATEAPP:
        if (!m->wParam) {
            k->InputLostEvent(Time::Ms());
        } else {
            k->Update(0.0f);
            k->InputGetEvent(Time::Ms());
        }
        return false;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:  // bit 30 of lParam (already down) becomes INPUT_REPEAT
        k->Key(m->wParam, Modifiers() | (((lParam & 0x40000000) | 0x4000000) >> 10));
        return false;
    case WM_KEYUP:  // Print Screen only sends the release
        if (m->wParam == VK_SNAPSHOT) k->Key(m->wParam, Modifiers() | INPUT_DOWN);
        k->Key(m->wParam, Modifiers() | INPUT_UP);
        return false;
    case WM_SYSKEYUP:
        k->Key(m->wParam, Modifiers() | INPUT_UP);
        return false;
    case WM_CHAR:
    case WM_IME_CHAR: {
        const int mods = Modifiers();
        TRACE("char %x", static_cast<unsigned>(m->wParam));
        k->CharEvent(static_cast<unsigned short>(m->wParam), mods | ((lParam >> 10) & INPUT_REPEAT), Time::Ms());
        return true;
    }
    default:
        return false;
    }
}

// ---- mouse (0x294 bytes)
class Win32Mouse : public InputDevice {
public:
    Win32Mouse(Window* window, InputDriver* driver);
    ~Win32Mouse() override;
    void __cdecl GetCaps(InputDeviceCaps& caps) const override { FillMouseCaps(caps); }
    bool __cdecl GetProperty(DeviceProperty& p) override;
    bool __cdecl SetProperty(const DeviceProperty& p) override;

protected:
    void __cdecl Update(float dt) override;
    void __cdecl CriticalShutdown() override;
    void __cdecl CriticalStartup() override;

private:
    enum { PENDING = 1, WHEEL = 2, FIVE_BUTTONS = 4, EXCLUSIVE = 8 };

    static bool __fastcall Hook(void* self, void* message, int& result);
    void Activate(bool on, bool notify);
    bool AboveThreshold(int axis, int delta) const;
    void Button(int button, int flags)  // 0x10003140
    {
        TRACE("button %d %x", button, flags);
        ButtonEvent(button, flags, Time::Ms());
    }
    void Wheel(int delta);
    int& Min(int axis) { return range[axis * 3]; }
    int& Max(int axis) { return range[axis * 3 + 1]; }

    Window* window;            // +0x238
    int last[2];               // +0x23c where the cursor was last seen (client coordinates)
    int unknown244;            // +0x244
    int moved[3];              // +0x248 x, y, wheel since the last Update
    int unknown254;            // +0x254
    int range[8];              // +0x258 per axis {min, max} at a 12-byte stride
    int threshold[3];          // +0x278 the smallest wheel step that counts
    int saved[2];              // +0x284 the position before exclusive mode
    unsigned char mouseFlags;  // +0x28c PENDING (activation while iconic), WHEEL, FIVE_BUTTONS, EXCLUSIVE
    HCURSOR savedCursor;       // +0x290 the cursor hidden by exclusive mode
};
JET_LAYOUT32(sizeof(Win32Mouse) == 0x294);

Win32Mouse::Win32Mouse(Window* w, InputDriver* driver) : InputDevice(driver)  // 0x100031b0
{
    window = w;
    InputUIExtension::doubleClickTime = GetDoubleClickTime();
    window->RegisterWindowHook(&Hook, this);
    prevState.type = DEVICE_MOUSE;
    state.type = DEVICE_MOUSE;
    InputDeviceCaps c;
    FillMouseCaps(c);
    instanceId = c.id;
    mouseFlags = (mouseFlags & 0xf0) | ((c.numButtons > 3) << 2) | ((c.numAxes > 2) << 1);
    savedCursor = nullptr;
    moved[0] = moved[1] = moved[2] = 0;
    for (int i = 0; i < 3; ++i) {
        Min(i) = 0;
        Max(i) = 0xffff;
        threshold[i] = 0;
    }
    Activate(true, true);
}

Win32Mouse::~Win32Mouse()  // 0x10003350
{
    Activate(false, true);
    window->ClearWindowHook(this);
}

void Win32Mouse::CriticalShutdown()  // 0x100041d0
{
    window->ClearWindowHook(this);
}

void Win32Mouse::CriticalStartup()  // 0x100041f0
{
    if (window) window->RegisterWindowHook(&Hook, this);
    Activate(true, true);
}

// gains or loses the input: exclusive mode clips and hides the cursor at the window centre
void Win32Mouse::Activate(bool on, bool notify)  // 0x100039d0
{
    savedCursor = nullptr;
    if (!on) {
        ClipCursor(nullptr);
        SetCursor(LoadCursorA(nullptr, IDC_ARROW));
        window->SetCapture(false);
        if (notify) InputLostEvent(Time::Ms());
        mouseFlags &= ~PENDING;
        return;
    }
    if (window->IsIconic()) {  // later, on WM_MOVE / WM_SIZE / mode change
        mouseFlags = (mouseFlags & ~PENDING) | (notify ? PENDING : 0);
        return;
    }
    const Rect r = window->GetClientArea();
    last[0] = state.mouse.position[0] = r.x + r.w / 2;
    last[1] = state.mouse.position[1] = r.y + r.h / 2;
    Point g = {r.x, r.y};
    if (!window->ClientIsGlobal()) window->LocalToGlobal(g);
    const RECT clip = {g.x, g.y, g.x + r.w - 1, g.y + r.h - 1};
    if (mouseFlags & EXCLUSIVE) {
        ClipCursor(&clip);
        SetCursorPos(g.x + r.w / 2, g.y + r.h / 2);
    } else {
        ClipCursor(nullptr);
    }
    SetCursor((mouseFlags & EXCLUSIVE) ? nullptr : LoadCursorA(nullptr, IDC_ARROW));
    if (mouseFlags & EXCLUSIVE) window->SetCapture(true);
    Update(0.0f);
    if (notify) InputGetEvent(Time::Ms());
    mouseFlags &= ~PENDING;
}

bool Win32Mouse::AboveThreshold(int axis, int delta) const  // 0x10003190
{
    if (!delta) return false;
    return (delta < 0 ? -delta : delta) >= threshold[axis];
}

void Win32Mouse::Wheel(int delta)
{
    int v = state.mouse.position[2] + delta;
    if (Min(2) >= v) v = Min(2);
    if (v > Max(2)) v = Max(2);
    state.mouse.position[2] = v;
    AxisEvent(2, v, delta, Time::Ms());
    moved[2] += delta;
}

bool Win32Mouse::GetProperty(DeviceProperty& p)  // 0x100035b0
{
    if (!valid) return false;
    switch (p.id) {
    case PROPERTY_RANGE:
        if (p.index >= 3) return false;
        p.a = Min(p.index);
        p.b = Max(p.index);
        return true;
    case PROPERTY_THRESHOLD:
        if (p.index >= 3) return false;
        p.a = threshold[p.index];
        return true;
    case PROPERTY_POSITION:
        if (p.index >= 3) return false;
        p.a = state.mouse.position[p.index];
        return true;
    case PROPERTY_EXCLUSIVE:
        p.index = (mouseFlags >> 3) & 1;
        return true;
    default:
        return false;
    }
}

bool Win32Mouse::SetProperty(const DeviceProperty& p)  // 0x10003660
{
    if (!valid) return false;
    switch (p.id) {
    case PROPERTY_RANGE:
        if (p.index >= 3) return false;
        Min(p.index) = p.a;
        Max(p.index) = p.b;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    case PROPERTY_THRESHOLD:
        if (p.index >= 3) return false;
        threshold[p.index] = p.a;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    case PROPERTY_POSITION:
        if (p.index >= 3) return false;
        state.mouse.position[p.index] = p.a;
        if (p.index < 2) last[p.index] = state.mouse.position[p.index];
        if (!window->IsIconic()) {
            const Rect r = window->GetClientArea();
            Point g = {r.x, r.y};
            window->LocalToGlobal(g);
            if (!(mouseFlags & EXCLUSIVE))
                SetCursorPos(state.mouse.position[0] + g.x, state.mouse.position[1] + g.y);
        }
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    case PROPERTY_EXCLUSIVE: {
        if (p.index == ((mouseFlags >> 3) & 1)) return false;
        const Rect r = window->GetClientArea();
        Point g = {r.x, r.y};
        if (!window->ClientIsGlobal()) window->LocalToGlobal(g);
        RECT clip = {g.x, g.y, g.x + r.w - 1, g.y + r.h - 1};
        mouseFlags = (mouseFlags & ~EXCLUSIVE) | ((p.index << 3) & EXCLUSIVE);
        if (mouseFlags & EXCLUSIVE) {
            saved[0] = state.mouse.position[0];
            saved[1] = state.mouse.position[1];
            if (window->IsIconic()) return false;
            ClipCursor(&clip);
            SetCursorPos(g.x + r.w / 2, g.y + r.h / 2);
            savedCursor = SetCursor(nullptr);
            window->SetCapture(true);
            last[0] = r.x + r.w / 2;
            last[1] = r.y + r.h / 2;
        } else {
            state.mouse.position[0] = saved[0];
            state.mouse.position[1] = saved[1];
            ClipCursor(nullptr);
            window->SetCapture(false);
            if (window->IsIconic()) return false;
            if (savedCursor) {
                SetCursor(savedCursor);
                savedCursor = nullptr;
            }
            last[1] = state.mouse.position[1];
            last[0] = state.mouse.position[0];
            SetCursorPos(state.mouse.position[0] + g.x, state.mouse.position[1] + g.y);
        }
        state.mouse.position[0] = last[0];
        state.mouse.position[1] = last[1];
        Update(0.0f);
        return false;
    }
    default:
        return false;
    }
}

void Win32Mouse::Update(float)  // 0x10003bb0
{
    if (!valid) return;
    prevState = state;
    for (int i = 0; i < 3; ++i) {
        state.mouse.delta[i] = moved[i];
        moved[i] = 0;
    }
    state.mouse.buttons[0] = GetKeyState(VK_LBUTTON) & 0x80;
    state.mouse.buttons[1] = GetKeyState(VK_RBUTTON) & 0x80;
    state.mouse.buttons[2] = GetKeyState(VK_MBUTTON) & 0x80;
    if (mouseFlags & FIVE_BUTTONS) {
        state.mouse.buttons[3] = GetKeyState(VK_XBUTTON1) & 0x80;
        state.mouse.buttons[4] = GetKeyState(VK_XBUTTON2) & 0x80;
    }
}

bool __fastcall Win32Mouse::Hook(void* self, void* message, int&)  // 0x10003c40
{
    Win32Mouse* const ms = static_cast<Win32Mouse*>(self);
    const Window::Message* const m = static_cast<const Window::Message*>(message);
    const unsigned int id = m->id;
    if (id == static_cast<unsigned int>(Window::WM_END_OF_MESSAGES)) {
        // exclusive: put the cursor back at the window centre
        Window* const w = ms->window;
        if ((ms->mouseFlags & EXCLUSIVE) && w->IsActiveApp() && !w->IsIconic()) {
            const Rect r = w->GetClientArea();
            ms->last[0] = r.x + r.w / 2;
            ms->last[1] = r.y + r.h / 2;
            Point g = {r.x, r.y};
            w->LocalToGlobal(g);
            SetCursorPos(g.x + r.w / 2, g.y + r.h / 2);
        }
        return false;
    }
    if (id == static_cast<unsigned int>(Window::WM_MODE_CHANGED)) {
        ms->Activate(true, ms->mouseFlags & PENDING);
        return false;
    }
    switch (id) {
    case WM_DESTROY:
        ms->Activate(false, true);
        return false;
    case WM_MOVE:
    case WM_SIZE:
        ms->Activate(true, ms->mouseFlags & PENDING);
        return false;
    case WM_ACTIVATEAPP: {
        if (!m->wParam) {
            ms->Activate(false, true);
            return false;
        }
        ms->Activate(true, true);
        // a move to where the cursor is now, when it is over the window
        POINT pt;
        GetCursorPos(&pt);
        RECT wr;
        GetWindowRect(reinterpret_cast<HWND>(m->window), &wr);
        if (wr.left < pt.x && pt.x < wr.right && wr.top < pt.y && pt.y < wr.bottom)
            SendMessageA(reinterpret_cast<HWND>(m->window), WM_MOUSEMOVE, 0,
                         ((pt.y - wr.top) << 16) | (pt.x - wr.left));
        return false;
    }
    case WM_SYSCOMMAND:
        if (m->wParam != SC_CLOSE) return false;
        SendMessageA(reinterpret_cast<HWND>(m->window), WM_CLOSE, 0, 0);
        return true;
    case WM_MOUSEMOVE: {
        const unsigned int lParam = static_cast<unsigned int>(m->lParam);
        const int pos[2] = {static_cast<short>(lParam & 0xffff), static_cast<short>(lParam >> 16)};
        const int delta[2] = {pos[0] - ms->last[0], pos[1] - ms->last[1]};
        for (int i = 0; i < 2; ++i) {
            if (!delta[i]) continue;  // (exclusive mode also asks AboveThreshold, which is false for 0)
            int v = ms->state.mouse.position[i] + delta[i];
            if (ms->Min(i) >= v) v = ms->Min(i);
            if (v > ms->Max(i)) v = ms->Max(i);
            ms->state.mouse.position[i] = v;
            TRACE("move axis %d %d delta %d", i, v, delta[i]);
            ms->AxisEvent(i, v, delta[i], Time::Ms());
            ms->moved[i] += delta[i];
            ms->last[i] = pos[i];
        }
        return false;
    }
    case WM_LBUTTONDOWN: ms->Button(0, INPUT_DOWN); return false;
    case WM_LBUTTONUP: ms->Button(0, INPUT_UP); return false;
    case WM_RBUTTONDOWN: ms->Button(1, INPUT_DOWN); return false;
    case WM_RBUTTONUP: ms->Button(1, INPUT_UP); return false;
    case WM_MBUTTONDOWN: ms->Button(2, INPUT_DOWN); return false;
    case WM_MBUTTONUP: ms->Button(2, INPUT_UP); return false;
    case WM_MOUSEWHEEL: {
        const int delta = static_cast<short>(m->wParam >> 16);
        if (ms->AboveThreshold(2, delta)) ms->Wheel(delta);
        return false;
    }
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:  // both report a release (kept)
        if (ms->mouseFlags & FIVE_BUTTONS) ms->Button(static_cast<unsigned short>(m->wParam >> 16) + 2, INPUT_UP);
        return true;
    default:
        break;
    }
    if (g_wheelMessage && id == g_wheelMessage) {  // old wheel mice: the step in wParam
        const int delta = static_cast<int>(m->wParam);
        if (ms->AboveThreshold(2, delta)) ms->Wheel(delta);
        return true;
    }
    return false;
}

// ---- joystick (0x540 bytes)
class Win32Joystick : public InputDevice {
public:
    Win32Joystick(const UId& id, InputDriver* driver);
    ~Win32Joystick() override {}
    void __cdecl GetCaps(InputDeviceCaps& caps) const override { FillJoystickCaps(joystick, caps); }
    bool __cdecl GetProperty(DeviceProperty& p) override;
    bool __cdecl SetProperty(const DeviceProperty& p) override;

protected:
    void __cdecl Update(float dt) override;

private:
    enum { AXES = 8, BUTTONS = 32 };
    int& RawMin(int axis) { return rawRange[axis * 3]; }
    int& RawMax(int axis) { return rawRange[axis * 3 + 1]; }
    int& OutMin(int axis) { return outRange[axis * 3]; }
    int& OutMax(int axis) { return outRange[axis * 3 + 1]; }
    void UpdateScale(int axis);

    int unknown238;              // +0x238
    bool present;                // +0x23c joyGetDevCaps succeeded
    bool hasPov;                 // +0x23d
    unsigned char autoRepeat;    // +0x23e
    float repeatDelay;           // +0x240 seconds
    unsigned int joystick;       // +0x244 0 or 1
    JOYCAPSA caps;               // +0x248
    unsigned long axisMask;      // +0x3dc
    int unknown3e0;              // +0x3e0
    int rawRange[AXES * 3];      // +0x3e4 what the driver reports, {min, max} at a 12-byte stride
    int outRange[AXES * 3 - 1];  // +0x444 what we report, same stride (the last entry has no third word)
    float scale[AXES];           // +0x4a0
    float repeatTime[BUTTONS];   // +0x4c0 time each button has been held since its last repeat
};
JET_LAYOUT32(sizeof(Win32Joystick) == 0x540);

Win32Joystick::Win32Joystick(const UId& id, InputDriver* driver) : InputDevice(driver)  // 0x10001a10
{
    unknown238 = 0;
    autoRepeat = 0;
    repeatDelay = 0.33f;
    joystick = memcmp(&id, &UIdJoystick2, sizeof(UId)) == 0;
    prevState.type = DEVICE_JOYSTICK;
    state.type = DEVICE_JOYSTICK;
    InputDeviceCaps c;
    FillJoystickCaps(joystick, c);
    instanceId = c.id;
    axisMask = c.axisMask;
    present = joyGetDevCapsA(joystick, &caps, sizeof(caps)) == JOYERR_NOERROR;
    hasPov = (caps.wCaps >> 4) & 1;
    const UINT reported[AXES][2] = {{caps.wXmin, caps.wXmax}, {caps.wYmin, caps.wYmax}, {caps.wZmin, caps.wZmax},
                                    {0, 0}, {0, 0}, {caps.wRmin, caps.wRmax}, {caps.wUmin, caps.wUmax},
                                    {caps.wVmin, caps.wVmax}};
    for (int i = 0; i < AXES; ++i) {
        RawMin(i) = reported[i][0];
        RawMax(i) = reported[i][1];
    }
    for (int i = 0; i < AXES; ++i) {
        OutMin(i) = -32767;
        OutMax(i) = 32767;
        const int span = RawMax(i) - RawMin(i);
        scale[i] = span ? 65534.0f / static_cast<float>(span) : 1.0f;
    }
    memset(repeatTime, 0, sizeof(repeatTime));
}

bool Win32Joystick::GetProperty(DeviceProperty& p)  // 0x10001f60
{
    if (!valid) return false;
    switch (p.id) {
    case PROPERTY_AUTOREPEAT:
        p.index = autoRepeat;
        return true;
    case PROPERTY_REPEAT_DELAY:  // float arithmetic, rounded to nearest (fistp)
        p.value = static_cast<int>(std::nearbyint(repeatDelay * 1000.0f - 0.5f));
        return true;
    case PROPERTY_RANGE:
        if (p.index >= AXES) return false;
        p.a = OutMin(p.index);
        p.b = OutMax(p.index);
        return true;
    case PROPERTY_THRESHOLD: {
        UINT t;
        if (joyGetThreshold(joystick, &t) != JOYERR_NOERROR) return false;
        p.a = t;
        return true;
    }
    case PROPERTY_POSITION:
        if (p.index >= AXES) return false;
        p.a = state.joystick.position[p.index];
        return true;
    default:
        return false;
    }
}

bool Win32Joystick::SetProperty(const DeviceProperty& p)  // 0x10002050
{
    if (!valid) return false;
    switch (p.id) {
    case PROPERTY_AUTOREPEAT:
        autoRepeat = p.index;  // the byte as given
        return true;
    case PROPERTY_REPEAT_DELAY:  // ms (unsigned) to seconds
        repeatDelay = static_cast<float>(static_cast<double>(static_cast<unsigned int>(p.value)) *
                                         static_cast<double>(0.001f));
        return true;
    case PROPERTY_RANGE: {
        const int i = p.index;
        if (i >= AXES) return false;
        OutMin(i) = p.a;
        OutMax(i) = p.b;
        const int span = RawMax(i) - RawMin(i);
        scale[i] = span ? (static_cast<float>(p.b) - static_cast<float>(OutMin(i))) / static_cast<float>(span) : 1.0f;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    }
    case PROPERTY_THRESHOLD:
        if (joySetThreshold(joystick, p.a) != JOYERR_NOERROR) return false;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    default:
        return false;
    }
}

void Win32Joystick::Update(float dt)  // 0x10002170
{
    if (!valid || !present) return;
    JOYINFOEX info;
    info.dwSize = sizeof(info);
    info.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNR | JOY_RETURNU | JOY_RETURNV | JOY_RETURNBUTTONS |
                   JOY_RETURNPOVCTS;  // no JOY_RETURNPOV (0x2bf)
    if (joyGetPosEx(joystick, &info) != JOYERR_NOERROR) return;
    prevState = state;
    const DWORD raw[AXES] = {info.dwXpos, info.dwYpos, info.dwZpos, 0, 0, info.dwRpos, info.dwUpos, info.dwVpos};
    for (int i = 0; i < AXES; ++i) {
        if (axisMask & (1u << i)) {
            const int offset = static_cast<int>(raw[i] - RawMin(i));
            // floor, then _ftol2's SSE2 path (cvttsd2si: 0x80000000 when out of range)
            const int v = _mm_cvttsd_si32(_mm_set_sd(std::floor(static_cast<double>(offset) * scale[i] + OutMin(i) + 0.5f)));
            state.joystick.position[i] = v;
            state.joystick.delta[i] = v - prevState.joystick.position[i];
        } else {
            state.joystick.position[i] = 0;
            state.joystick.delta[i] = 0;
        }
    }
    for (int i = 0; i < BUTTONS; ++i) {
        const unsigned char b = (info.dwButtons & (1u << i)) ? 0x80 : 0;
        state.joystick.buttons[i] = b;
        if (b) state.joystick.pressure[i] = 0xff;
    }
    state.joystick.pov[0] = info.dwPOV >= 0xffff ? -1 : static_cast<int>(info.dwPOV);
    const unsigned long time = Time::Ms();
    for (int i = 0; i < AXES; ++i)
        if (state.joystick.delta[i]) AxisEvent(i, state.joystick.position[i], state.joystick.delta[i], time);
    for (int i = 0; i < BUTTONS; ++i) {
        const unsigned char b = state.joystick.buttons[i];
        if (b != prevState.joystick.buttons[i]) {
            ButtonEvent(i, b ? INPUT_DOWN : INPUT_UP, time);
        } else if (autoRepeat && b) {
            repeatTime[i] += dt;
            if (repeatTime[i] >= repeatDelay) {
                repeatTime[i] = 0.0f;
                ButtonEvent(i, INPUT_REPEAT | INPUT_DOWN, time);
            }
        }
    }
    if (hasPov && state.joystick.pov[0] != prevState.joystick.pov[0]) PovEvent(0, state.joystick.pov[0], time);
}

// ---- the driver (0x1c bytes)
class InputDriverWin32 : public InputDriver {
public:
    InputDriverWin32() : os(nullptr) {}
    ~InputDriverWin32() override { ReleaseCapture(); }  // 0x100011e0
    bool __cdecl Init(OS* os) override;
    void __cdecl Enumerate(InputDeviceCapsList& list, int type) override;
    AutoReference<InputDevice> __cdecl CreateDevice(const UId& id) override;

protected:
    void __cdecl CriticalShutdown() override { ReleaseCapture(); }  // 0x100016d0
    void __cdecl CriticalStartup() override;

private:
    OS* os;  // +0x0c
};
JET_LAYOUT32(sizeof(InputDriverWin32) == 0x1c);

bool InputDriverWin32::Init(OS* o)  // 0x10001240
{
    TRACE("Init");
    if (!o->GetWindow() || !o->GetWindow()->Handle()) return false;
    os = o;
    return true;
}

void InputDriverWin32::CriticalStartup()  // 0x100016e0
{
    if (os && os->GetWindow()) os->GetWindow()->SetCapture(true);
}

void InputDriverWin32::Enumerate(InputDeviceCapsList& list, int type)  // 0x10001280
{
    InputDeviceCaps c;
    switch (type) {
    case DEVICE_KEYBOARD:
        FillKeyboardCaps(c);
        list.Add(c);
        break;
    case DEVICE_MOUSE:
        FillMouseCaps(c);
        list.Add(c);
        break;
    case DEVICE_JOYSTICK:
        if (FillJoystickCaps(0, c)) list.Add(c);
        if (FillJoystickCaps(1, c)) list.Add(c);
        break;
    }
}

AutoReference<InputDevice> InputDriverWin32::CreateDevice(const UId& id)  // 0x10001430
{
    const auto is = [&id](const UId& u) { return memcmp(&id, &u, sizeof(UId)) == 0; };
    AutoReference<InputDevice> device;
    TRACE("CreateDevice %08lx", id.data1);
    if (is(UIdKeyboard)) {
        device = new Win32Keyboard(this, os);
    } else if (is(UIdMouse)) {
        device = new Win32Mouse(os->GetWindow(), this);
    } else if (is(UIdJoystick1) || is(UIdJoystick2)) {
        // a missing first joystick falls back to the second
        InputDeviceCaps c;
        if (is(UIdJoystick1) && FillJoystickCaps(0, c))
            device = new Win32Joystick(UIdJoystick1, this);
        else if (FillJoystickCaps(1, c))
            device = new Win32Joystick(UIdJoystick2, this);
    }
    return device;
}

extern "C" void __cdecl JetGetPluginsInputWin32()  // 0x10001840
{
    new PluginFactoryOf<InputDriverWin32, InputDriver>(PString("Win32"), UId("445ED0AC-4D40-4e53-B4F2-D75899C21778"),
                                                       UId("F3117B18-F364-41a1-8993-966236A35B5E"));
}

}  // namespace Jet
