// Calls into classes whose definitions are not reconstructed yet (RenderSystem's Display, Texture, ...): a __cdecl
// virtual function called by its byte offset in the vtable, as the original code does. Each use names the slot in a
// wrapper so it can become a real virtual call once the class exists.
#pragma once

namespace Jet {

template <class R, class... A>
R VCall(const void* obj, unsigned offset, A... args)
{
    using F = R(__cdecl*)(const void*, A...);
    return (*reinterpret_cast<F* const*>(obj))[offset / sizeof(void*)](obj, args...);
}

// a scalar deleting destructor (slot 0, __thiscall): delete obj through its own class
inline void VDelete(void* obj)
{
    using F = void*(__thiscall*)(void*, unsigned);
    (*reinterpret_cast<F* const*>(obj))[0](obj, 1);
}

}  // namespace Jet
