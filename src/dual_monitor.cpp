// One monitor per player (split screen display 2).
//
// Mimics the arcade's twin cabinets without a second window: the game window
// becomes borderless and spans its monitor plus the neighbouring one. The GPU
// side composes the two views side by side (and mirrors single-view screens
// into both halves), so each monitor shows one player's full screen. Works
// best with two equal monitors placed side by side.

#include "dual_monitor.h"

#include <rex/cvar.h>
#include <rex/logging.h>

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <vector>

namespace oratan {
namespace {

HWND FindGameWindow() {
  struct Search {
    DWORD pid;
    HWND found;
  } search{GetCurrentProcessId(), nullptr};
  EnumWindows(
      [](HWND hwnd, LPARAM param) -> BOOL {
        auto* s = reinterpret_cast<Search*>(param);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == s->pid && IsWindowVisible(hwnd) && !GetWindow(hwnd, GW_OWNER)) {
          s->found = hwnd;
          return FALSE;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&search));
  return search.found;
}

std::vector<RECT> MonitorRects() {
  std::vector<RECT> rects;
  EnumDisplayMonitors(
      nullptr, nullptr,
      [](HMONITOR monitor, HDC, LPRECT, LPARAM param) -> BOOL {
        MONITORINFO info{sizeof(info)};
        if (GetMonitorInfoW(monitor, &info)) {
          reinterpret_cast<std::vector<RECT>*>(param)->push_back(info.rcMonitor);
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&rects));
  return rects;
}

// Window state to go back to when the mode is turned off.
bool g_spanning = false;
LONG_PTR g_saved_style = 0;
RECT g_saved_rect{};
bool g_saved_fullscreen = false;

}  // namespace

bool ApplyDualMonitorWindow(bool enable) {
  HWND hwnd = FindGameWindow();
  if (!hwnd) return false;

  if (!enable) {
    if (!g_spanning) return true;
    SetWindowLongPtrW(hwnd, GWL_STYLE, g_saved_style);
    SetWindowPos(hwnd, nullptr, g_saved_rect.left, g_saved_rect.top,
                 g_saved_rect.right - g_saved_rect.left, g_saved_rect.bottom - g_saved_rect.top,
                 SWP_NOZORDER | SWP_FRAMECHANGED);
    if (g_saved_fullscreen) rex::cvar::SetFlagByName("fullscreen", "true");
    g_spanning = false;
    REXLOG_INFO("dual monitor: off");
    return true;
  }

  const std::vector<RECT> monitors = MonitorRects();
  if (monitors.size() < 2) {
    REXLOG_WARN("dual monitor: only one monitor connected");
    return false;
  }

  // The monitor the game is on, and the neighbour sharing the most of its
  // height (side by side), else any other monitor.
  MONITORINFO current_info{sizeof(current_info)};
  GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &current_info);
  const RECT current = current_info.rcMonitor;
  const RECT* best = nullptr;
  LONG best_overlap = -1;
  for (const RECT& r : monitors) {
    if (EqualRect(&r, &current)) continue;
    const bool adjacent = r.left == current.right || r.right == current.left;
    const LONG overlap = std::max(0L, std::min(r.bottom, current.bottom) - std::max(r.top, current.top));
    const LONG score = (adjacent ? 100000 : 0) + overlap;
    if (score > best_overlap) {
      best_overlap = score;
      best = &r;
    }
  }
  if (!best) return false;
  RECT span;
  UnionRect(&span, &current, best);

  if (!g_spanning) {
    g_saved_fullscreen = rex::cvar::Query<bool>("fullscreen");
    g_saved_style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    GetWindowRect(hwnd, &g_saved_rect);
  }
  // Leave SDL's fullscreen (tied to one display) before spanning two.
  if (rex::cvar::Query<bool>("fullscreen")) rex::cvar::SetFlagByName("fullscreen", "false");
  SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
  SetWindowPos(hwnd, HWND_TOP, span.left, span.top, span.right - span.left, span.bottom - span.top,
               SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  g_spanning = true;
  REXLOG_INFO("dual monitor: window spans {},{} {}x{}", span.left, span.top, span.right - span.left,
              span.bottom - span.top);
  return true;
}

}  // namespace oratan
