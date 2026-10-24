// In-game Options window (F1).

#include "options_menu.h"

#include <rex/cvar.h>

#include <imgui.h>

#include "dual_monitor.h"
#include "netplay.h"
#include "settings.h"

#include <algorithm>
#include <cstdio>
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
                             std::function<void(const RelaunchArgs&)> restart)
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
    if (restart_ && ImGui::Button("Restart now")) restart_({});
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

  DrawNetplay();

  ImGui::Separator();
  ImGui::TextDisabled("Settings are saved automatically.");
  ImGui::End();
}

// Netplay. It runs from boot so both games stay in step, so hosting, joining
// and leaving restart the game with --netplay_host / --netplay_join added or
// removed (never saved: a normal launch is offline). The port, address, delay
// and controller are remembered.
void OptionsDialog::DrawNetplay() {
  using State = NetplayStatus::State;
  const NetplayStatus status = GetNetplayStatus();
  const ImVec4 kWarning(1.0f, 0.8f, 0.2f, 1.0f);
  const ImVec4 kError(1.0f, 0.4f, 0.3f, 1.0f);
  RelaunchArgs offline;
  offline.remove = {"--netplay_host", "--netplay_join"};

  ImGui::Separator();
  if (!ImGui::CollapsingHeader("Netplay (experimental)",
                               status.state == State::kOff ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  // Shown to the host: what player 2 types in.
  auto show_addresses = [](uint32_t port) {
    const std::vector<std::string> addresses = LocalIpv4Addresses();
    if (addresses.empty()) {
      ImGui::TextDisabled("Player 2 joins with this PC's address and port %u.", port);
    } else {
      ImGui::TextDisabled("Player 2 joins with:");
      for (const std::string& a : addresses) ImGui::BulletText("%s:%u", a.c_str(), port);
    }
    ImGui::TextDisabled("Over the internet: forward UDP port %u to this PC,", port);
    ImGui::TextDisabled("or both use a VPN such as ZeroTier or Tailscale.");
  };

  switch (status.state) {
    case State::kWaiting:
      ImGui::Text("Waiting for player 2 on UDP port %u...", status.port);
      show_addresses(status.port);
      if (!status.message.empty()) ImGui::TextColored(kWarning, "%s", status.message.c_str());
      ImGui::TextDisabled("The game starts once player 2 has joined.");
      if (ImGui::Button("Cancel (play offline)")) CancelNetplayWait();
      return;
    case State::kJoining:
      ImGui::Text("Joining %s...", status.peer.c_str());
      if (ImGui::Button("Cancel (play offline)")) CancelNetplayWait();
      return;
    case State::kConnected:
      ImGui::Text("Connected to %s as player %u.", status.peer.c_str(), status.player);
      ImGui::Text("Input delay: %u ticks.", status.delay);
      if (status.recent_stalls > 0) {
        ImGui::TextDisabled("Last 30 s: waited for the other player %u times (%.0f ms).",
                            status.recent_stalls, status.recent_stall_ms);
      }
      if (status.recent_stall_ms > 1000.0) {
        ImGui::TextColored(kWarning, "Frequent waits: try a higher input delay next time.");
      }
      if (status.desynced) {
        ImGui::TextColored(kError, "Out of sync since tick %u: the two games no longer match.",
                           status.desync_tick);
        ImGui::TextColored(kError, "Leave and start again (same game files, save and settings).");
      }
      if (restart_ && ImGui::Button("Leave netplay (restarts the game)")) restart_(offline);
      return;
    case State::kEnded:
      if (!status.message.empty()) {
        ImGui::TextColored(kWarning, "Netplay stopped: %s", status.message.c_str());
      }
      break;
    case State::kOff:
      break;
  }

  // Setting up a session.
  static int role = rex::cvar::Query<std::string>("netplay_address").empty() ? 0 : 1;
  ImGui::RadioButton("Host (player 1)", &role, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Join (player 2)", &role, 1);

  int pad = std::clamp(rex::cvar::Query<int>("netplay_pad"), 0, 3);
  static const char* kPads[] = {"Controller 1", "Controller 2", "Controller 3", "Controller 4"};
  if (ImGui::Combo("My controller", &pad, kPads, 4)) {
    rex::cvar::SetFlagByName("netplay_pad", std::to_string(pad));
    Save();
  }

  if (role == 0) {
    int delay = std::clamp(rex::cvar::Query<int>("netplay_delay"), 1, 15);
    if (ImGui::SliderInt("Input delay (ticks)", &delay, 1, 8)) {
      rex::cvar::SetFlagByName("netplay_delay", std::to_string(delay));
      Save();
    }
    ImGui::TextDisabled("Higher hides more lag: 3 on a LAN, 4-5 for about 40 ms.");
    int port = rex::cvar::Query<int>("netplay_port");
    if (ImGui::InputInt("UDP port", &port, 0)) {
      port = std::clamp(port, 1024, 65535);
      rex::cvar::SetFlagByName("netplay_port", std::to_string(port));
      Save();
    }
    show_addresses(uint32_t(port));
    if (restart_ && ImGui::Button("Host (restarts the game)")) {
      RelaunchArgs args = offline;
      args.add = {"--netplay_host=" + std::to_string(port)};
      restart_(args);
    }
  } else {
    static char address[128] = {};
    static bool loaded = false;
    if (!loaded) {
      std::snprintf(address, sizeof(address), "%s",
                    rex::cvar::Query<std::string>("netplay_address").c_str());
      loaded = true;
    }
    ImGui::InputText("Host address", address, sizeof(address));
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      rex::cvar::SetFlagByName("netplay_address", address);
      Save();
    }
    ImGui::TextDisabled("The host's address and port, e.g. 192.168.1.20:7000.");
    ImGui::TextDisabled("The host's input delay is used.");
    std::string target = address;
    target.erase(0, target.find_first_not_of(" \t"));
    target.erase(target.find_last_not_of(" \t") + 1);
    ImGui::BeginDisabled(target.empty());
    if (restart_ && ImGui::Button("Join (restarts the game)")) {
      if (target.find(':') == std::string::npos) {
        target += ":" + std::to_string(rex::cvar::Query<int>("netplay_port"));
      }
      rex::cvar::SetFlagByName("netplay_address", target);
      Save();
      RelaunchArgs args = offline;
      args.add = {"--netplay_join=" + target};
      restart_(args);
    }
    ImGui::EndDisabled();
  }
  ImGui::TextDisabled("Both players need the same game files, save data and settings.");
  ImGui::TextDisabled("The game restarts; both play together from the title screen.");
}

}  // namespace oratan
