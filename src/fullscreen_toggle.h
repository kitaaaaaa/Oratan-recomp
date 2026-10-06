// Alt+Enter switches between fullscreen and a movable window.
//
// SDL's borderless fullscreen follows the monitor the window is on, so to
// move the game to another screen: Alt+Enter, drag the window, Alt+Enter.
// The choice is saved to the config file and used on the next launch.

#pragma once

#include <rex/cvar.h>
#include <rex/ui/ui_event.h>
#include <rex/ui/virtual_key.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

#include <filesystem>

namespace oratan {

class FullscreenToggle : public rex::ui::WindowInputListener {
 public:
  // Above the app's own listener (z-order 0), so Alt+Enter never reaches
  // the game as an Enter press.
  static constexpr size_t kZOrder = 100;

  void Attach(rex::ui::Window* window, std::filesystem::path config_path) {
    window_ = window;
    config_path_ = std::move(config_path);
    window_->AddInputListener(this, kZOrder);
  }

  void Detach() {
    if (window_) window_->RemoveInputListener(this);
    window_ = nullptr;
  }

  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (e.virtual_key() != rex::ui::VirtualKey::kReturn || !e.is_alt_pressed()) return;
    e.set_handled(true);
    if (e.prev_state() || !window_) return;  // ignore key repeat
    // The fullscreen cvar's change callback resizes the window.
    rex::cvar::SetFlagByName("fullscreen", window_->IsFullscreen() ? "false" : "true");
    if (!config_path_.empty()) rex::cvar::SaveConfig(config_path_);
  }

 private:
  rex::ui::Window* window_ = nullptr;
  std::filesystem::path config_path_;
};

}  // namespace oratan
