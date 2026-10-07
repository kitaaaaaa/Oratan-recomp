// Saving player settings.
//
// rex::cvar::SaveConfig writes every setting that differs from its default,
// including ones that only came from the command line (--log_file from
// run.bat, test switches) or are runtime state, and those would then stick in
// the config file. This updates just the player-facing settings and leaves the
// rest of the file as it was.

#include "settings.h"

#include <rex/cvar.h>
#include <rex/logging.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace oratan {
namespace {

// Settings the Options window and Alt+Enter manage, and how to write them.
struct Managed {
  const char* name;
  bool is_string;
};
constexpr Managed kManaged[] = {
    {"resolution_scale", false}, {"fullscreen", false},   {"present_vsync", false},
    {"local_versus", false},     {"split_screen", false},
};

std::string KeyOf(const std::string& line) {
  const size_t eq = line.find('=');
  if (eq == std::string::npos || line.empty() || line[0] == '#') return {};
  std::string key = line.substr(0, eq);
  while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
  return key;
}

}  // namespace

void SaveSettings(const std::filesystem::path& config_path) {
  if (config_path.empty()) return;

  std::map<std::string, std::string> values;
  for (const auto& m : kManaged) {
    const std::string value = rex::cvar::GetFlagByName(m.name);
    values[m.name] = m.is_string ? "\"" + value + "\"" : value;
  }

  // Keep every other line of the existing file; replace managed keys in place.
  std::vector<std::string> lines;
  {
    std::ifstream in(config_path);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      const std::string key = KeyOf(line);
      if (auto it = values.find(key); it != values.end()) {
        line = key + " = " + it->second;
        values.erase(it);
      }
      lines.push_back(line);
    }
  }
  if (lines.empty()) lines.push_back("# Virtual-On OT settings");
  for (const auto& m : kManaged) {
    if (auto it = values.find(m.name); it != values.end()) {
      lines.push_back(std::string(m.name) + " = " + it->second);
    }
  }

  std::ofstream out(config_path, std::ios::trunc);
  for (const auto& line : lines) out << line << "\n";
  out.close();
  if (out.fail()) REXLOG_ERROR("Could not save settings to {}", config_path.string());
}

}  // namespace oratan
