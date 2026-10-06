// In-game Options window (F1): player-facing settings, saved to the config
// file. The SDK's F4 overlay edits every cvar, including ones that break the
// game (e.g. its "vsync", which is really the console's 60 Hz timer); this
// one only offers safe choices. VSync here is the display sync added by
// sdk-patches/0001-present-vsync.patch.

#pragma once

#include <rex/ui/imgui_dialog.h>

#include <filesystem>
#include <functional>

namespace oratan {

class OptionsDialog : public rex::ui::ImGuiDialog {
 public:
  // `restart` relaunches the game; offered when a setting needs a restart.
  OptionsDialog(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path,
                std::function<void()> restart);

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void Save();

  std::filesystem::path config_path_;
  std::function<void()> restart_;
};

// Records the settings the game started with, so the Options window can tell
// which changes still need a restart. Call once at startup.
void RememberStartupSettings();

}  // namespace oratan
