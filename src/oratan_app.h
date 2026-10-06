// oratan - Virtual-On Oratorio Tangram (XBLA) static recompilation
//
// ReXApp subclass: boot defaults and window setup.

#pragma once

#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/ui/window.h>

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
    (void)paths;
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
