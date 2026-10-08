// One monitor per player (see dual_monitor.cpp).

#pragma once

namespace rex::ui {
class Window;
}

namespace oratan {

// The game window the mode moves around. Call once the window is open.
void AttachDualMonitorWindow(rex::ui::Window* window);

// Makes the game window borderless across two neighbouring monitors (enable),
// or puts it back as it was. Call on the UI thread. Returns false if there is
// no second monitor or no window.
bool ApplyDualMonitorWindow(bool enable);

// True while the mode is on (spanning or windowed). Any thread.
bool DualMonitorActive();

// Alt+Enter while the mode is on: switches between spanning two monitors and a
// normal, movable window showing both views. Going back to spanning uses the
// two monitors the window is on (or nearest to), so the views can be moved to
// any pair of monitors.
void ToggleDualMonitorWindowed();

// The player's fullscreen choice from before the mode turned SDL's fullscreen
// off, so it is the one saved to the config while the mode is on.
bool DualMonitorSavedFullscreen();

}  // namespace oratan
