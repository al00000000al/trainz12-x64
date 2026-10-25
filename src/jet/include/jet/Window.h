// Jet::Window (KernelStandard_7c unit U11, Window.cpp): the application window, 0x134 bytes. A Win32 window of
// class "JET" whose extra bytes hold the object; hooks see every message first, dispatchers every message the
// pump takes off the queue. Layout from the TS12 constructor (x64 layout: provisional).
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Math.h>

#include <cstdint>

namespace Jet {

class Options;

class Window {
public:
    // a window hook sees every message first: (context, message, result); true swallows the message
    using Hook = bool(__fastcall*)(void* context, void* message, int& result);
    // a dispatcher sees every MSG HandleMessages takes off the queue: (context, MSG*); true: not dispatched
    using Dispatcher = bool(__fastcall*)(void* context, void* msg);

    // the message a hook receives
    struct HookMessage {
        HWND__* window;
        unsigned int id;
        uintptr_t wParam;
        intptr_t lParam;
    };

    Window(HWND__* parent);
    ~Window();
    Window& operator=(const Window& o);

    bool __cdecl Open(const char* title, const Options* options);
    bool __cdecl CreateDefault();
    bool __cdecl CreateWindowedHost(int x, int y, int w, int h, void* param);
    bool __cdecl CreateFullscreenHost(int w, int h, void* param);
    bool __cdecl Create2DWindow(int x, int y, int w, int h, void* param);
    bool __cdecl SetHostState(bool fullscreen);
    bool __cdecl GetHostState() const;
    void __cdecl Close();
    void __cdecl Show(bool show);
    bool __cdecl GetVisible() const;
    bool __cdecl Move(const Rect& r, bool repaint);
    bool __cdecl Resize(const Size& s, bool repaint);
    Size __cdecl CalculateWindowArea(const Size& client, bool windowedStyle) const;
    void __cdecl Message(const char* text);
    bool __cdecl MessageConfirm(const char* text);
    void __cdecl Error(char* text);
    bool __cdecl HandleMessages();
    void __cdecl SetHandleMessages(bool on);
    void __cdecl RegisterWindowHook(Hook hook, void* context);
    void __cdecl ClearWindowHook(void* context);
    void __cdecl RegisterWindowDispatcher(Dispatcher dispatcher, void* context);
    void __cdecl ClearWindowDispatcher(void* context);
    void* __cdecl GetHandle() const;
    void __cdecl SetHandle(void* handle);
    HWND__* __cdecl GetParent() const;
    const char* __cdecl GetTitle();
    bool __cdecl SetTitle(const char* title);
    const char* __cdecl GetInternalName();
    void __cdecl SetContextOnly(bool on);
    Rect __cdecl GetClientArea() const;
    Rect __cdecl GetArea() const;
    Rect __cdecl GetParentArea() const;
    void __cdecl LocalToGlobal(Point& p) const;
    bool __cdecl IsIconic() const;
    bool __cdecl HasFocus() const;
    void __cdecl SetFocus() const;
    void __cdecl SetCapture(bool on) const;
    bool __cdecl IsActiveApp();
    bool __cdecl IsActiveWnd();
    int __cdecl GetHostBitsPerPixel();
    static Window* __cdecl GetMainWindow();

    static JET_KS_IMPORT const int WM_END_OF_MESSAGES;  // posted after each batch of messages
    static JET_KS_IMPORT const int WM_MODE_CHANGED;     // the host state changed (wParam: windowed)

    HWND__* Handle() const { return handle; }
    bool ClientIsGlobal() const { return (flags & 4) != 0; }

protected:
    static long __stdcall WindowProc(HWND__* hwnd, unsigned int msg, unsigned int wParam, long lParam);
    static int __cdecl CriticalShutdown(void* self);
#if defined(JET_KS_BUILD)
    static Window* mainWindow;  // the first one made
#endif

private:
    // the rings of hooks and dispatchers: singly linked through the head, appended at the end
    struct Node {
        Node* next;
        void* fn;
        void* context;
    };
    struct Ring {
        Node* head;  // the ring itself when empty
        int count;
    };

    HWND__* handle;        // +0x000
    char title[0x101];     // +0x004
    unsigned char active;  // +0x105 bit 0 WM_ACTIVATEAPP, bit 1 WM_ACTIVATE
    Ring hooks;            // +0x108
    Ring dispatchers;      // +0x110
    HWND__* parent;        // +0x118
    // +0x11c: 1 ShowFPS, 2 visible, 4 full-screen host, 8 handles messages, 0x10 context only, 0x20 class registered
    unsigned char flags;
    int frames;             // +0x120 since the last FPS update
    float fps;              // +0x124
    unsigned long fpsTime;  // +0x128
    int bitsPerPixel;       // +0x12c the desktop's, read once
    void* shutdown;         // +0x130 the critical-shutdown entry made by Open
};

}  // namespace Jet
