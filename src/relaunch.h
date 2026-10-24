// Starting a fresh copy of the game (see relaunch.cpp).

#pragma once

#include <string>
#include <vector>

namespace oratan {

// Changes to the command line for the new copy.
struct RelaunchArgs {
  // Options to leave out ("--netplay_host" drops "--netplay_host=7000", and
  // "--netplay_host 7000" with its value).
  std::vector<std::string> remove;
  // Arguments to add (UTF-8), e.g. "--netplay_join=192.168.1.20:7000".
  std::vector<std::string> add;
};

// Starts another instance of this exe with this one's command line, changed by
// `args`. The caller then quits this one. Returns false if the new process
// could not start.
bool LaunchNewInstance(const RelaunchArgs& args = {});

}  // namespace oratan
