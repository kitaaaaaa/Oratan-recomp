// oratan - Virtual-On Oratorio Tangram (XBLA) static recompilation
//
// ReXApp subclass: boot defaults and window setup.

#pragma once

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/rex_app.h>
#include <rex/system.h>
#include <rex/ui/window.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "debug_tools.h"

class OratanApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<OratanApp>(new OratanApp(ctx, "oratan",
        PPCImageConfig));
  }

  // Boot defaults. These run before the user's oratan.toml is loaded, so
  // anything set there still wins.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    // No --game_data_root: look for the game files so double-clicking the
    // exe just works. Checks "assets" next to the exe, then the repo's
    // assets folder (the exe builds to out/build/<preset>/).
    if (paths.game_data_root.empty()) {
      const auto exe_dir = rex::filesystem::GetExecutableFolder();
      for (const auto& dir : {exe_dir / "assets", exe_dir / ".." / ".." / ".." / "assets"}) {
        if (std::filesystem::exists(dir / "default.xex")) {
          paths.game_data_root = std::filesystem::weakly_canonical(dir);
          break;
        }
      }
      if (paths.game_data_root.empty()) {
        rex::ShowSimpleMessageBox(
            rex::SimpleMessageBoxType::Error,
            std::string("Virtual-On OT game files not found.\n\n"
                        "Copy your extracted copy of the game (default.xex and "
                        "the media folder) into:\n\n") +
                (exe_dir / "assets").string() +
                "\n\nSee \"HOW TO ADD THE GAME.txt\" in that folder.");
        std::exit(1);
      }
    }
    // XBLA titles run as the trial unless the full-game license bit is set.
    rex::cvar::SetFlagByName("license_mask", "1");
  }

  // Use the Xenos GPU emulation plugin (rexgpu-xenos.dll) unless the user
  // picked something else with --gpu_plugin.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) config.gpu_plugin = "xenos";
  }

  void OnPostLoadXexImage() override {
    oratan::DumpGuestImageIfRequested(runtime());
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    (void)drawer;
    if (window()) window()->SetTitle("Virtual-On Oratorio Tangram");
  }
};
