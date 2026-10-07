// One monitor per player (see dual_monitor.cpp).

#pragma once

namespace oratan {

// Makes the game window borderless across its monitor and the neighbouring
// one (enable), or restores it. Call on the UI thread. Returns false if there
// is no second monitor or no window.
bool ApplyDualMonitorWindow(bool enable);

}  // namespace oratan
