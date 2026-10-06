// In-game Options window (F1).

#include "options_menu.h"

#include <rex/cvar.h>

#include <imgui.h>

#include <string>

namespace oratan {
namespace {

// Internal rendering resolution. The game itself renders 1280x720; the GPU
// layer can draw at a multiple of that.
struct ResolutionChoice {
  int scale;
  const char* label;
};
constexpr ResolutionChoice kResolutions[] = {
    {1, "1280 x 720 (original)"},
    {2, "2560 x 1440"},
    {3, "3840 x 2160 (4K)"},
    {4, "5120 x 2880"},
};

int CurrentScale() {
  const int scale = rex::cvar::Query<int>("resolution_scale");
  return scale > 0 ? scale : 1;
}

// The scale the renderer was created with; changes only apply after a restart.
int g_scale_at_startup = 0;

}  // namespace

void RememberStartupSettings() { g_scale_at_startup = CurrentScale(); }

OptionsDialog::OptionsDialog(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path,
                             std::function<void()> restart)
    : ImGuiDialog(drawer),
      config_path_(std::move(config_path)),
      restart_(std::move(restart)) {}

void OptionsDialog::Save() {
  if (!config_path_.empty()) rex::cvar::SaveConfig(config_path_);
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
  const int scale = CurrentScale();
  const char* current_label = "Custom";
  for (const auto& r : kResolutions) {
    if (r.scale == scale) current_label = r.label;
  }
  if (ImGui::BeginCombo("Resolution", current_label)) {
    for (const auto& r : kResolutions) {
      if (ImGui::Selectable(r.label, r.scale == scale)) {
        rex::cvar::SetFlagByName("resolution_scale", std::to_string(r.scale));
        Save();
      }
    }
    ImGui::EndCombo();
  }
  if (g_scale_at_startup && CurrentScale() != g_scale_at_startup) {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Restart the game to apply.");
    ImGui::SameLine();
    if (restart_ && ImGui::Button("Restart now")) restart_();
  }

  // Display mode
  bool fullscreen = rex::cvar::Query<bool>("fullscreen");
  if (ImGui::Checkbox("Fullscreen (Alt+Enter)", &fullscreen)) {
    rex::cvar::SetFlagByName("fullscreen", fullscreen ? "true" : "false");
    Save();
  }

  bool vsync = rex::cvar::Query<bool>("present_vsync");
  if (ImGui::Checkbox("VSync", &vsync)) {
    rex::cvar::SetFlagByName("present_vsync", vsync ? "true" : "false");
    Save();
  }

  ImGui::Separator();
  ImGui::TextDisabled("Settings are saved automatically.");
  ImGui::End();
}

}  // namespace oratan
