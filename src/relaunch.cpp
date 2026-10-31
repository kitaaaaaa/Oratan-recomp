// Starting a fresh copy of the game (for settings that need a restart, and to
// start or leave netplay).

#include "relaunch.h"

#include <windows.h>

#include <shellapi.h>

namespace oratan {
namespace {

std::wstring Widen(const std::string& text) {
  if (text.empty()) return {};
  const int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
  std::wstring wide(size_t(len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), wide.data(), len);
  return wide;
}

// Quotes one argument the way CommandLineToArgvW reads it back.
void AppendQuoted(std::wstring* out, const std::wstring& arg) {
  if (!out->empty()) out->push_back(L' ');
  if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    out->append(arg);
    return;
  }
  out->push_back(L'"');
  for (size_t i = 0;; ++i) {
    size_t backslashes = 0;
    while (i < arg.size() && arg[i] == L'\\') {
      ++i;
      ++backslashes;
    }
    if (i == arg.size()) {
      out->append(backslashes * 2, L'\\');  // they precede the closing quote
      break;
    }
    if (arg[i] == L'"') {
      out->append(backslashes * 2 + 1, L'\\');
    } else {
      out->append(backslashes, L'\\');
    }
    out->push_back(arg[i]);
  }
  out->push_back(L'"');
}

// Whether `arg` is option `name`, written "--name=value" or just "--name".
bool IsOption(const std::wstring& arg, const std::wstring& name, bool* value_follows) {
  if (arg == name) {
    *value_follows = true;
    return true;
  }
  *value_follows = false;
  return arg.size() > name.size() && arg.compare(0, name.size(), name) == 0 &&
         arg[name.size()] == L'=';
}

}  // namespace

bool LaunchNewInstance(const RelaunchArgs& args) {
  wchar_t exe_path[MAX_PATH];
  if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH)) return false;

  // This instance's arguments (so the same launch options), minus `remove`,
  // plus `add`.
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv) return false;
  std::wstring command_line;
  for (int i = 0; i < argc; ++i) {
    const std::wstring arg = argv[i];
    bool removed = false, value_follows = false;
    for (const std::string& name : args.remove) {
      if (i > 0 && IsOption(arg, Widen(name), &value_follows)) {
        removed = true;
        break;
      }
    }
    if (removed) {
      if (value_follows && i + 1 < argc && argv[i + 1][0] != L'-') ++i;  // and its value
      continue;
    }
    AppendQuoted(&command_line, arg);
  }
  LocalFree(argv);
  for (const std::string& arg : args.add) AppendQuoted(&command_line, Widen(arg));

  // CreateProcessW may modify the buffer, so it gets its own copy.
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
