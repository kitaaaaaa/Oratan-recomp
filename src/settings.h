// Saving player settings (see settings.cpp).

#pragma once

#include <filesystem>

namespace oratan {

// Writes the player-facing settings (resolution, fullscreen, VSync, local
// versus, split screen) to the config file, keeping its other lines.
void SaveSettings(const std::filesystem::path& config_path);

}  // namespace oratan
