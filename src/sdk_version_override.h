// Force-included into the SDK's rex_app.cpp (see CMakeLists.txt) so the window
// title is just the game's name, without the "[rexglue-vX-Release]" build tag.
// rex/version.h is #pragma once, so including it here first and redefining
// the macros makes rex_app.cpp's own #include a no-op.

#pragma once

#include <rex/version.h>

#undef REXGLUE_BUILD_TITLE
#define REXGLUE_BUILD_TITLE ""
