// The two game functions replays and netplay hang off.
//
// One hook per function, so both features are called from here:
//   - Update (sub_821236E0, vtable slot 2 of the run loop object) runs one game
//     tick. Ticks are counted from boot.
//   - sub_821135C0 reads the controllers into game::kControllerStates
//     (4 x 60 bytes), once per tick on the Update thread (measured).
// Order after a read: netplay first (it replaces the buffer with both players'
// inputs), then the replay (recording stores what the game actually used).

#include <rex/hook.h>

#include "tick_hooks.h"

#include <thread>

REX_EXTERN(__imp__sub_821236E0);
REX_EXTERN(__imp__sub_821135C0);

namespace {

uint32_t g_tick = 0;
std::thread::id g_game_thread;
bool g_have_game_thread = false;
bool g_started = false;

void StartFeatures(uint8_t* base) {
  if (g_started) return;
  g_started = true;
  oratan::replay::Start();
  oratan::netplay::Start(base, oratan::replay::Playing());
}

}  // namespace

namespace oratan {

uint32_t GameTick() { return g_tick; }

bool OnGameThread() {
  return !g_have_game_thread || std::this_thread::get_id() == g_game_thread;
}

}  // namespace oratan

// sub_821236E0: the run loop's Update, one game tick.
REX_HOOK_RAW(sub_821236E0) {
  if (!g_have_game_thread) {
    g_game_thread = std::this_thread::get_id();
    g_have_game_thread = true;
  }
  StartFeatures(base);
  ++g_tick;
  __imp__sub_821236E0(ctx, base);
  oratan::netplay::OnTickEnd(base);
  oratan::replay::OnTickEnd(base);
}

// sub_821135C0: reads the controllers into game::kControllerStates.
REX_HOOK_RAW(sub_821135C0) {
  StartFeatures(base);
  __imp__sub_821135C0(ctx, base);
  oratan::netplay::OnControllerRead(base);
  oratan::replay::OnControllerRead(base);
}
