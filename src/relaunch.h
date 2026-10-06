// Starting a fresh copy of the game (see relaunch.cpp).

#pragma once

#include <string>

namespace oratan {

// Starts another instance of this exe with the same command line. The caller
// then quits this one. Returns false if the new process could not start.
bool LaunchNewInstance();

}  // namespace oratan
