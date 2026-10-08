// Frame-rate logging.
//
// Wraps the game's present function (sub_822D3180, which calls VdSwap once
// per frame) and writes the guest frame rate to the log every few seconds:
//   frames: 59.9 fps, frame time avg 16.7 ms / worst 18.2 ms
// The SDK's F3 overlay has no frame stats for this game, and this gives
// numbers from a player's log without touching their screen.

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "debug_tools.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>

#include <fmt/format.h>

#include <windows.h>

REXCVAR_DEFINE_BOOL(log_fps, true, "Oratan", "Write the game's frame rate to the log every 5 seconds");
REXCVAR_DEFINE_INT32(log_present_stack, 0, "Oratan",
                     "Developer: log the guest call stack of the next N presented frames");

REX_EXTERN(__imp__sub_822D3180);

namespace oratan {
void TagSwap();             // local_versus.cpp
int32_t CurrentDrawView();  // local_versus.cpp
}

namespace {

using Clock = std::chrono::steady_clock;
constexpr auto kReportInterval = std::chrono::seconds(5);

Clock::time_point g_window_start;
Clock::time_point g_last_frame;
uint32_t g_frames = 0;
double g_worst_ms = 0.0;

void OnFrame() {
  const auto now = Clock::now();
  if (g_frames == 0 && g_window_start == Clock::time_point{}) {
    g_window_start = g_last_frame = now;
    return;
  }
  const double frame_ms = std::chrono::duration<double, std::milli>(now - g_last_frame).count();
  g_last_frame = now;
  g_worst_ms = std::max(g_worst_ms, frame_ms);
  ++g_frames;

  const auto elapsed = now - g_window_start;
  if (elapsed < kReportInterval) return;
  const double seconds = std::chrono::duration<double>(elapsed).count();
  REXLOG_INFO("frames: {:.1f} fps, frame time avg {:.1f} ms / worst {:.1f} ms", g_frames / seconds,
              seconds * 1000.0 / g_frames, g_worst_ms);
  g_window_start = now;
  g_frames = 0;
  g_worst_ms = 0.0;
}

// True if the guest word at `addr` is backed by committed, readable memory
// (the top of a stack's back-chain can point anywhere).
bool IsReadable(const uint8_t* base, uint32_t addr) {
  MEMORY_BASIC_INFORMATION info;
  if (!VirtualQuery(base + addr, &info, sizeof(info))) return false;
  constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                              PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE;
  return info.State == MEM_COMMIT && (info.Protect & kReadable) && !(info.Protect & PAGE_GUARD);
}

uint32_t LoadGuest32(const uint8_t* base, uint32_t addr) {
  uint32_t v;
  std::memcpy(&v, base + addr, 4);
  return __builtin_bswap32(v);
}

// Walks the PPC stack back-chain: each frame's first word is the caller's r1,
// and MSVC-360 prologues save LR at -8 from the caller's r1.
void LogGuestStack(const PPCContext& ctx, const uint8_t* base) {
  std::string out = fmt::format("present stack: lr={:08X}", uint32_t(ctx.lr));
  uint32_t sp = ctx.r1.u32;
  for (int depth = 0; depth < 24 && sp; ++depth) {
    if (!IsReadable(base, sp)) break;
    const uint32_t caller_sp = LoadGuest32(base, sp);
    if (caller_sp <= sp || caller_sp - sp > 0x100000 || !IsReadable(base, caller_sp - 8)) break;
    out += fmt::format(" <- {:08X}", LoadGuest32(base, caller_sp - 8));
    sp = caller_sp;
  }
  REXLOG_INFO("{}", out);
}

}  // namespace

REX_HOOK_RAW(sub_822D3180) {
  static uint32_t frame = 0;
  oratan::CheckWatches(base, frame++);
  oratan::CaptureIfRequested(base);
  // Count game frames: in split screen each frame is presented once per
  // view, so only the first view's present counts.
  if (REXCVAR_GET(log_fps) && oratan::CurrentDrawView() <= 0) OnFrame();
  // Note game mode switches (0 OPEN, 1 GAME, 2 NETVS, 3 CHRSEL, 4 ENDING) in the log.
  static int last_module = -1;
  if (const int module = base[0x839C21BC]; module != last_module) {
    last_module = module;
    REXLOG_INFO("game module {}", module);
  }
  if (int n = REXCVAR_GET(log_present_stack); n > 0) {
    LogGuestStack(ctx, base);
    rex::cvar::SetFlagByName("log_present_stack", std::to_string(n - 1));
  }
  oratan::TagSwap();
  __imp__sub_822D3180(ctx, base);
}
