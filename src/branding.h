// Player-facing names (see branding.cpp).

#pragma once

#include <string>

namespace oratan {

// The short name players see on dialogs and the settings/save folder.
inline constexpr char kAppTitle[] = "Virtual-On OT";
inline constexpr wchar_t kAppTitleW[] = L"Virtual-On OT";

// The game window's title bar.
inline constexpr char kWindowTitle[] = "Virtual-On Oratorio Tangram MSBS ver. 5.66w";

// Shows an error dialog titled with the game's name.
void ShowError(const std::string& message);

}  // namespace oratan
