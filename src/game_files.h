// Locating the player's game files (see game_files.cpp).

#pragma once

#include <filesystem>
#include <string>

namespace oratan {

// Finds the folder holding default.xex. Players may supply either:
//  - the extracted game (default.xex + media/), anywhere under assets/, or
//  - the raw Xbox Live Arcade package file, which is unpacked into assets/
//    on first launch.
// Returns an empty path and fills `error` if nothing usable is found.
std::filesystem::path LocateGameFiles(const std::filesystem::path& exe_dir, std::string* error);

}  // namespace oratan
