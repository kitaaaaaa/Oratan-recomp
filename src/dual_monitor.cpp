// One monitor per player (split screen display 2).
//
// Mimics the arcade's twin cabinets without a second window: the game window
// becomes borderless and spans two neighbouring monitors. The GPU side
// composes the two views side by side (and mirrors single-view screens into
// both halves), so each monitor shows one player's full screen. Works best
// with two equal monitors placed side by side.
//
// Alt+Enter switches between spanning and a normal movable window showing both
// views; moving that window and pressing Alt+Enter again spans the monitors
// it was moved to (e.g. monitors 2 and 3 of three). The last span is saved
// and reused on the next launch.

#include "dual_monitor.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/window.h>

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>

REXCVAR_DEFINE_STRING(dual_monitor_span, "", "Oratan",
                      "One monitor per player: the desktop rectangle (left,top,right,bottom) "
                      "of the two monitors last spanned, reused when they are still connected");

namespace oratan {
namespace {

rex::ui::Window* g_window = nullptr;

enum class Mode { kOff, kSpanning, kWindowed };
// Changed on the UI thread; DualMonitorActive is also read by the game thread.
std::atomic<Mode> g_mode{Mode::kOff};

// Window state to go back to when the mode is turned off.
LONG_PTR g_saved_style = 0;
RECT g_saved_rect{};
bool g_saved_fullscreen = false;
// The monitors last spanned, for sizing the movable window.
RECT g_span{};

HWND GameWindow() {
  return g_window ? static_cast<HWND>(g_window->GetNativeWindowHandle()) : nullptr;
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

LONGLONG IntersectionArea(const RECT& a, const RECT& b) {
  RECT i;
  if (!IntersectRect(&i, &a, &b)) return 0;
  return LONGLONG(i.right - i.left) * (i.bottom - i.top);
}

bool Adjacent(const RECT& a, const RECT& b) {
  return (a.left == b.right || a.right == b.left) &&
         std::min(a.bottom, b.bottom) > std::max(a.top, b.top);
}

// The two monitors to span for a window at `window_rect`: the monitor it is
// mostly on, plus the neighbour it overlaps most, else the neighbour on the
// side its centre leans towards, else any other monitor.
bool PickSpan(const std::vector<RECT>& monitors, const RECT& window_rect, RECT* span) {
  MONITORINFO info{sizeof(info)};
  if (!GetMonitorInfoW(MonitorFromRect(&window_rect, MONITOR_DEFAULTTONEAREST), &info)) {
    return false;
  }
  const RECT current = info.rcMonitor;
  const LONG window_center = (window_rect.left + window_rect.right) / 2;
  const LONG current_center = (current.left + current.right) / 2;
  const RECT* best = nullptr;
  std::tuple<bool, LONGLONG, bool, LONG> best_score{};
  for (const RECT& r : monitors) {
    if (EqualRect(&r, &current)) continue;
    const bool toward = window_center >= current_center ? r.left >= current.right
                                                        : r.right <= current.left;
    const LONG vertical = std::max(0L, std::min(r.bottom, current.bottom) - std::max(r.top, current.top));
    const auto score = std::make_tuple(Adjacent(r, current), IntersectionArea(r, window_rect), toward,
                                       vertical);
    if (!best || score > best_score) {
      best = &r;
      best_score = score;
    }
  }
  if (!best) return false;
  UnionRect(span, &current, best);
  return true;
}

// The span saved by the last session, if those two monitors are still there.
bool SavedSpan(const std::vector<RECT>& monitors, RECT* span) {
  RECT saved;
  const std::string text = REXCVAR_GET(dual_monitor_span);
  if (std::sscanf(text.c_str(), "%ld,%ld,%ld,%ld", &saved.left, &saved.top, &saved.right,
                  &saved.bottom) != 4) {
    return false;
  }
  for (const RECT& a : monitors) {
    for (const RECT& b : monitors) {
      if (EqualRect(&a, &b) || !Adjacent(a, b)) continue;
      RECT u;
      UnionRect(&u, &a, &b);
      if (EqualRect(&u, &saved)) {
        *span = u;
        return true;
      }
    }
  }
  return false;
}

void SpanMonitors(HWND hwnd, const RECT& span) {
  SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
  SetWindowPos(hwnd, HWND_TOP, span.left, span.top, span.right - span.left, span.bottom - span.top,
               SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  g_span = span;
  g_mode = Mode::kSpanning;
  char text[64];
  std::snprintf(text, sizeof(text), "%ld,%ld,%ld,%ld", span.left, span.top, span.right, span.bottom);
  rex::cvar::SetFlagByName("dual_monitor_span", text);
  REXLOG_INFO("dual monitor: window spans {},{} {}x{}", span.left, span.top, span.right - span.left,
              span.bottom - span.top);
}

// A normal window on the left monitor of the span, 90% of its width, with the
// span's shape so both views keep their full size relative to each other.
void MakeMovableWindow(HWND hwnd) {
  MONITORINFO info{sizeof(info)};
  const RECT left_half{g_span.left, g_span.top, (g_span.left + g_span.right) / 2, g_span.bottom};
  GetMonitorInfoW(MonitorFromRect(&left_half, MONITOR_DEFAULTTONEAREST), &info);
  const RECT work = info.rcWork;
  const LONG span_w = std::max(1L, g_span.right - g_span.left);
  const LONG span_h = std::max(1L, g_span.bottom - g_span.top);
  LONG client_w = (work.right - work.left) * 9 / 10;
  LONG client_h = client_w * span_h / span_w;
  if (client_h > (work.bottom - work.top) * 9 / 10) {
    client_h = (work.bottom - work.top) * 9 / 10;
    client_w = client_h * span_w / span_h;
  }
  constexpr DWORD kStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  RECT frame{0, 0, client_w, client_h};
  AdjustWindowRectEx(&frame, kStyle, FALSE, 0);
  const LONG w = frame.right - frame.left;
  const LONG h = frame.bottom - frame.top;
  SetWindowLongPtrW(hwnd, GWL_STYLE, kStyle);
  SetWindowPos(hwnd, HWND_TOP, work.left + (work.right - work.left - w) / 2,
               work.top + (work.bottom - work.top - h) / 2, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  g_mode = Mode::kWindowed;
  REXLOG_INFO("dual monitor: movable window {}x{}", client_w, client_h);
}

}  // namespace

void AttachDualMonitorWindow(rex::ui::Window* window) { g_window = window; }

bool DualMonitorActive() { return g_mode != Mode::kOff; }

bool DualMonitorSavedFullscreen() { return g_saved_fullscreen; }

bool ApplyDualMonitorWindow(bool enable) {
  HWND hwnd = GameWindow();
  if (!hwnd) return false;

  if (!enable) {
    if (g_mode == Mode::kOff) return true;
    SetWindowLongPtrW(hwnd, GWL_STYLE, g_saved_style);
    SetWindowPos(hwnd, nullptr, g_saved_rect.left, g_saved_rect.top,
                 g_saved_rect.right - g_saved_rect.left, g_saved_rect.bottom - g_saved_rect.top,
                 SWP_NOZORDER | SWP_FRAMECHANGED);
    if (g_saved_fullscreen) rex::cvar::SetFlagByName("fullscreen", "true");
    g_mode = Mode::kOff;
    REXLOG_INFO("dual monitor: off");
    return true;
  }
  if (g_mode != Mode::kOff) return true;

  const std::vector<RECT> monitors = MonitorRects();
  if (monitors.size() < 2) {
    REXLOG_WARN("dual monitor: only one monitor connected");
    return false;
  }
  RECT window_rect;
  GetWindowRect(hwnd, &window_rect);
  RECT span;
  if (!SavedSpan(monitors, &span) && !PickSpan(monitors, window_rect, &span)) return false;

  g_saved_fullscreen = rex::cvar::Query<bool>("fullscreen");
  // Leave SDL's fullscreen (tied to one display) before spanning two. The
  // style and size are read afterwards, so turning the mode off puts back a
  // normal window and then re-enters fullscreen if it was on.
  if (g_saved_fullscreen) rex::cvar::SetFlagByName("fullscreen", "false");
  g_saved_style = GetWindowLongPtrW(hwnd, GWL_STYLE);
  GetWindowRect(hwnd, &g_saved_rect);
  SpanMonitors(hwnd, span);
  return true;
}

void ToggleDualMonitorWindowed() {
  HWND hwnd = GameWindow();
  if (!hwnd || g_mode == Mode::kOff) return;
  if (g_mode == Mode::kSpanning) {
    MakeMovableWindow(hwnd);
    return;
  }
  RECT window_rect;
  GetWindowRect(hwnd, &window_rect);
  RECT span;
  if (PickSpan(MonitorRects(), window_rect, &span)) {
    SpanMonitors(hwnd, span);
  } else {
    REXLOG_WARN("dual monitor: no second monitor to span");
  }
}

}  // namespace oratan
