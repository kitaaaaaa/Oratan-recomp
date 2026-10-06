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

#include <algorithm>
#include <chrono>
#include <cstdint>

REXCVAR_DEFINE_BOOL(log_fps, true, "Oratan", "Write the game's frame rate to the log every 5 seconds");

REX_EXTERN(__imp__sub_822D3180);

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

}  // namespace

REX_HOOK_RAW(sub_822D3180) {
  if (REXCVAR_GET(log_fps)) OnFrame();
  __imp__sub_822D3180(ctx, base);
}
