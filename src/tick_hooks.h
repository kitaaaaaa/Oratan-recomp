// The game tick and the controller read, shared by replays and netplay (see
// tick_hooks.cpp).

#pragma once

#include <cstdint>

namespace oratan {

// Update calls started since boot: the game tick (one per 1/60 s, menus
// included). Controller reads before the first Update happen at tick 0.
uint32_t GameTick();

// Whether the caller is on the thread that runs Update (true until the first
// Update, when it isn't known yet).
bool OnGameThread();

namespace replay {
// Called once, from whichever hook runs first.
void Start();
// Whether a recording is being played (netplay then stays off).
bool Playing();
// After the game read the controllers (and after netplay replaced them).
void OnControllerRead(uint8_t* base);
// After each Update.
void OnTickEnd(uint8_t* base);
}  // namespace replay

namespace netplay {
// Called once, from whichever hook runs first; connects to the other player
// (blocking) when netplay is set up.
void Start(uint8_t* base, bool replay_playing);
// Before the game reads the controllers: gives the read this machine's own
// controller history back (the read computes presses from it).
void BeforeControllerRead(uint8_t* base);
// After the game read the controllers: replaces them with both players'
// inputs for this tick, waiting for the other player's if needed.
void OnControllerRead(uint8_t* base);
// After each Update.
void OnTickEnd(uint8_t* base);
}  // namespace netplay

}  // namespace oratan
