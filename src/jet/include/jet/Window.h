// Jet::Window (KernelStandard_7c): the application window. Only what the input drivers call is declared; the
// handle is its first field and +0x11c holds flags (bit 2: the client area is already in screen coordinates).
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Math.h>

#include <cstdint>

namespace Jet {

class Window {
public:
    // a window hook sees every message first: (context, message, result); true swallows the message
    using Hook = bool(__fastcall*)(void* context, void* message, int& result);

    // the message a hook receives
    struct Message {
        HWND__* window;
        unsigned int id;
        uintptr_t wParam;
        intptr_t lParam;
    };

    void __cdecl RegisterWindowHook(Hook hook, void* context);
    void __cdecl ClearWindowHook(void* context);
    Rect __cdecl GetClientArea() const;
    void __cdecl LocalToGlobal(Point& p) const;
    bool __cdecl IsIconic() const;
    bool __cdecl IsActiveApp();
    bool __cdecl IsActiveWnd();
    void __cdecl SetCapture(bool on) const;

    static JET_KS_IMPORT const int WM_END_OF_MESSAGES;  // posted after each batch of messages
    static JET_KS_IMPORT const int WM_MODE_CHANGED;     // the display mode changed

    HWND__* Handle() const { return *reinterpret_cast<HWND__* const*>(this); }
    bool ClientIsGlobal() const
    {
        return (reinterpret_cast<const unsigned char*>(this)[0x11c] & 4) != 0;  // x64 offset: provisional
    }

private:
    Window() = delete;
};

}  // namespace Jet
