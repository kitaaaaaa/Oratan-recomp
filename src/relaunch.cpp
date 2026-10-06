// Starting a fresh copy of the game (for settings that need a restart).

#include "relaunch.h"

#include <windows.h>

namespace oratan {

bool LaunchNewInstance() {
  wchar_t exe_path[MAX_PATH];
  if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH)) return false;

  // Same command line (and so the same launch options) as this instance.
  // CreateProcessW may modify the buffer, so pass a copy.
  std::wstring command_line = GetCommandLineW();
  STARTUPINFOW startup_info{};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info{};
  if (!CreateProcessW(exe_path, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      nullptr, &startup_info, &process_info)) {
    return false;
  }
  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);
  return true;
}

}  // namespace oratan
