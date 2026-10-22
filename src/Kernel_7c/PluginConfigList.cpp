// Kernel_7c exports the FastDynamicArray<PluginConfig*,13,Less<...>> instance (PluginCollection derives from it).
// Explicit instantiation emits every member so the .def can export them by their mangled names.
#include <jet/PluginForward.h>

template class Jet::FastDynamicArray<Jet::PluginConfig*, 13, Jet::Less<Jet::PluginConfig*, Jet::PluginConfig*>>;
