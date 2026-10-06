// Player-facing names.
//
// The prebuilt SDK runtime titles its error dialogs "ReXGlue". Rather than
// fork the SDK, this redirects rexruntime.dll's MessageBoxW import so every
// dialog the player sees carries the game's name.

#include "branding.h"

#include <windows.h>

#include <cwchar>

namespace oratan {
namespace {

using MessageBoxWFn = int(WINAPI*)(HWND, LPCWSTR, LPCWSTR, UINT);
MessageBoxWFn g_real_message_box = nullptr;

int WINAPI BrandedMessageBoxW(HWND hwnd, LPCWSTR text, LPCWSTR caption, UINT type) {
  if (caption && std::wcscmp(caption, L"ReXGlue") == 0) caption = kAppTitleW;
  return g_real_message_box(hwnd, text, caption, type);
}

// Points `module`'s import of user32!MessageBoxW at BrandedMessageBoxW.
void PatchMessageBoxImport(HMODULE module) {
  auto* base = reinterpret_cast<uint8_t*>(module);
  auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!dir.VirtualAddress) return;

  for (auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
       imp->Name; ++imp) {
    if (_stricmp(reinterpret_cast<const char*>(base + imp->Name), "user32.dll") != 0) continue;
    auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);
    for (; thunk->u1.Function; ++thunk) {
      if (reinterpret_cast<MessageBoxWFn>(thunk->u1.Function) != g_real_message_box) continue;
      DWORD old_protect;
      VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old_protect);
      thunk->u1.Function = reinterpret_cast<ULONG_PTR>(&BrandedMessageBoxW);
      VirtualProtect(&thunk->u1.Function, sizeof(void*), old_protect, &old_protect);
    }
  }
}

// Runs at startup, before any SDK code can show a dialog.
struct InstallBranding {
  InstallBranding() {
    g_real_message_box = reinterpret_cast<MessageBoxWFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "MessageBoxW"));
    if (!g_real_message_box) return;
    if (HMODULE runtime = GetModuleHandleW(L"rexruntime.dll")) PatchMessageBoxImport(runtime);
  }
} g_install_branding;

}  // namespace

void ShowError(const std::string& message) {
  const int len = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
  std::wstring wide(len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wide.data(), len);
  MessageBoxW(nullptr, wide.c_str(), kAppTitleW, MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
}

}  // namespace oratan
