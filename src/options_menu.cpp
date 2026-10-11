// In-game Options window (F1).

#include "options_menu.h"

#include <rex/cvar.h>

#include <imgui.h>

#include "dual_monitor.h"
#include "settings.h"

#include <string>

namespace oratan {
namespace {

// Rendering resolution. The original renders the 3D scene at 1024x1024 and
// stretches it onto a 1280x720 frame; native rendering draws both at exactly
// the chosen output size (native render targets, renderer rework).
struct ResolutionChoice {
  int height;  // 0 = original EDRAM emulation
  const char* label;
};
constexpr ResolutionChoice kResolutions[] = {
    {0, "1280 x 720 (original)"},
    {1080, "1920 x 1080"},
    {1440, "2560 x 1440"},
    {2160, "3840 x 2160 (4K)"},
};

int CurrentHeight() {
  if (!rex::cvar::Query<bool>("native_render_targets")) return 0;
  const int height = rex::cvar::Query<int>("native_resolution");
  return height > 0 ? height : 0;
}

// The resolution the renderer was created with; changes only apply after a
// restart.
int g_height_at_startup = -1;

// Anti-aliasing of the 3D scene (native rendering only; applies on restart).
struct MsaaChoice {
  int value;  // native_msaa
  const char* label;
};
constexpr MsaaChoice kMsaa[] = {
    {1, "Off"},
    {2, "2x MSAA"},
    {0, "4x MSAA (original)"},
};
int g_msaa_at_startup = -1;

// Anisotropic filtering (anisotropic_override; applies immediately).
struct AnisoChoice {
  int value;
  const char* label;
};
constexpr AnisoChoice kAniso[] = {
    {0, "Off"}, {2, "2x"}, {3, "4x"}, {4, "8x"}, {5, "16x"},
};

int CurrentMsaa() {
  const int msaa = rex::cvar::Query<int>("native_msaa");
  return msaa == 4 ? 0 : msaa;  // 4x is the game's own setting
}

}  // namespace

void RememberStartupSettings() {
  g_height_at_startup = CurrentHeight();
  g_msaa_at_startup = CurrentMsaa();
}

OptionsDialog::OptionsDialog(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path,
                             std::function<void()> restart)
    : ImGuiDialog(drawer),
      config_path_(std::move(config_path)),
      restart_(std::move(restart)) {}

void OptionsDialog::Save() {
  if (!config_path_.empty()) SaveSettings(config_path_);
}

void OptionsDialog::OnDraw(ImGuiIO& io) {
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Options (F1 to close)", nullptr, ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  // Resolution
  const int height = CurrentHeight();
  const char* current_label = "Custom";
  for (const auto& r : kResolutions) {
    if (r.height == height) current_label = r.label;
  }
  if (ImGui::BeginCombo("Resolution", current_label)) {
    for (const auto& r : kResolutions) {
      if (ImGui::Selectable(r.label, r.height == height)) {
        rex::cvar::SetFlagByName("native_render_targets", r.height ? "true" : "false");
        rex::cvar::SetFlagByName("native_resolution", std::to_string(r.height));
        rex::cvar::SetFlagByName("resolution_scale", "1");
        Save();
      }
    }
    ImGui::EndCombo();
  }
  // Anti-aliasing (native rendering only)
  ImGui::BeginDisabled(height == 0);
  const int msaa = CurrentMsaa();
  const char* msaa_label = "Custom";
  for (const auto& m : kMsaa) {
    if (m.value == msaa) msaa_label = m.label;
  }
  if (ImGui::BeginCombo("Anti-aliasing", msaa_label)) {
    for (const auto& m : kMsaa) {
      if (ImGui::Selectable(m.label, m.value == msaa)) {
        rex::cvar::SetFlagByName("native_msaa", std::to_string(m.value));
        Save();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();

  // Lighting (native rendering only; applies immediately)
  ImGui::BeginDisabled(height == 0);
  bool shadows = rex::cvar::Query<bool>("native_shadows");
  if (ImGui::Checkbox("Sharp shadows", &shadows)) {
    rex::cvar::SetFlagByName("native_shadows", shadows ? "true" : "false");
    Save();
  }
  bool ambient_occlusion = rex::cvar::Query<double>("native_ao_strength") > 0.0;
  ImGui::BeginDisabled(!shadows);
  if (ImGui::Checkbox("Ambient occlusion", &ambient_occlusion)) {
    rex::cvar::SetFlagByName("native_ao_strength", ambient_occlusion ? "1.0" : "0");
    Save();
  }
  ImGui::EndDisabled();
  ImGui::EndDisabled();

  // Anisotropic filtering
  const int aniso = rex::cvar::Query<int>("anisotropic_override");
  const char* aniso_label = "Game default";
  for (const auto& a : kAniso) {
    if (a.value == aniso) aniso_label = a.label;
  }
  if (ImGui::BeginCombo("Anisotropic filtering", aniso_label)) {
    for (const auto& a : kAniso) {
      if (ImGui::Selectable(a.label, a.value == aniso)) {
        rex::cvar::SetFlagByName("anisotropic_override", std::to_string(a.value));
        Save();
      }
    }
    ImGui::EndCombo();
  }

  if ((g_height_at_startup >= 0 && CurrentHeight() != g_height_at_startup) ||
      (g_msaa_at_startup >= 0 && height != 0 && CurrentMsaa() != g_msaa_at_startup)) {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Restart the game to apply.");
    ImGui::SameLine();
    if (restart_ && ImGui::Button("Restart now")) restart_();
  }

  // Display mode
  // One monitor per player manages the window itself (Alt+Enter switches it
  // between spanning and a movable window), so fullscreen waits until it's off.
  const bool dual_monitor = DualMonitorActive();
  bool fullscreen = dual_monitor ? DualMonitorSavedFullscreen() : rex::cvar::Query<bool>("fullscreen");
  ImGui::BeginDisabled(dual_monitor);
  if (ImGui::Checkbox("Fullscreen (Alt+Enter)", &fullscreen)) {
    rex::cvar::SetFlagByName("fullscreen", fullscreen ? "true" : "false");
    Save();
  }
  ImGui::EndDisabled();

  bool vsync = rex::cvar::Query<bool>("present_vsync");
  if (ImGui::Checkbox("VSync", &vsync)) {
    rex::cvar::SetFlagByName("present_vsync", vsync ? "true" : "false");
    Save();
  }

  ImGui::Separator();
  bool local_versus = rex::cvar::Query<bool>("local_versus");
  if (ImGui::Checkbox("Player 2 controls the opponent (experimental)", &local_versus)) {
    rex::cvar::SetFlagByName("local_versus", local_versus ? "true" : "false");
    Save();
  }
  ImGui::TextDisabled("Controller 2 takes over the CPU robot.");
  bool split_screen = rex::cvar::Query<bool>("split_screen");
  bool split_changed = false;
  if (ImGui::Checkbox("Split screen during battles (experimental)", &split_screen)) {
    rex::cvar::SetFlagByName("split_screen", split_screen ? "true" : "false");
    split_changed = true;
  }
  static const char* kDisplays[] = {"Staggered (one screen)", "Side by side (one screen)",
                                    "One monitor per player"};
  int display = rex::cvar::Query<int>("split_display");
  if (display < 0 || display > 2) display = 0;
  if (ImGui::Combo("Split display", &display, kDisplays, 3)) {
    rex::cvar::SetFlagByName("split_display", std::to_string(display));
    split_changed = true;
  }
  // Set when one monitor per player could not be applied (one monitor).
  static bool dual_monitor_failed = false;
  if (split_changed) {
    const bool want_dual = split_screen && display == 2;
    dual_monitor_failed = !ApplyDualMonitorWindow(want_dual) && want_dual;
    Save();
  }
  if (split_screen && display == 2) {
    if (dual_monitor_failed) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                         "Needs a second monitor; pick a one-screen display instead.");
    } else {
      ImGui::TextDisabled("Alt+Enter: switch between both monitors and a movable window.");
      ImGui::TextDisabled("Move the window and press Alt+Enter to use other monitors.");
    }
  }
  bool swap_sides = rex::cvar::Query<bool>("split_swap_sides");
  if (ImGui::Checkbox("Swap sides (robot 1 on the right)", &swap_sides)) {
    rex::cvar::SetFlagByName("split_swap_sides", swap_sides ? "true" : "false");
    Save();
  }
  ImGui::TextDisabled(local_versus ? "Robot 1's view, and robot 2's view."
                                   : "Robot 1's view, and the LIVE MONITOR camera.");

  ImGui::Separator();
  ImGui::TextDisabled("Settings are saved automatically.");
  ImGui::End();
}

}  // namespace oratan
