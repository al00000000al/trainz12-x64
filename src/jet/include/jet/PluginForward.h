// Forward declarations for the plugin classes (full definitions come with the plugin system).
#pragma once

#include <jet/FastDynamicArray.h>

namespace Jet {

class PluginConfig;
class PluginType;
class PluginLibrary;
class PluginGroup;

using PluginConfigList = FastDynamicArray<PluginConfig*, 13, Less<PluginConfig*, PluginConfig*>>;

}  // namespace Jet
