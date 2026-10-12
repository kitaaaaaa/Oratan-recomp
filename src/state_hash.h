// A fingerprint of the game state, taken after each tick (see state_hash.cpp).
//
// Used by replays (stored per tick, compared on replay) and netplay (exchanged
// per tick, compared between the two machines) to find the first tick where
// two runs of the game stop agreeing, and which part of the state differs.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace oratan {

enum StateRegion : uint32_t {
  kStateGame,    // the battle state (first `game_bytes` of game::Game)
  kStateRobot1,  // robot record 1
  kStateRobot2,  // robot record 2
  kStateFlow,    // side input records, module byte, next stage
  kStateRegions
};

using StateHashes = std::array<uint64_t, kStateRegions>;

// Hashes the regions; robot heap pointers are left out. Outside battles the
// game and robot regions are 0.
StateHashes HashState(uint8_t* base, int32_t game_bytes);

// "game, robot 2": the regions where `a` and `b` differ.
std::string DifferingRegions(const StateHashes& a, const StateHashes& b);

// Copies the hashed regions as they are now: the first `game_bytes` of the
// battle state (left out outside battles), then the side input records.
void CaptureStateRegions(uint8_t* base, int32_t game_bytes, std::vector<uint8_t>* out);

// Writes the hashed regions (as CaptureStateRegions lays them out) to `path`,
// for diffing two runs at the same tick.
void DumpStateRegions(uint8_t* base, const std::string& path, int32_t game_bytes);
void WriteStateRegions(const std::vector<uint8_t>& regions, const std::string& path);

}  // namespace oratan
