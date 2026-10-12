// A fingerprint of the game state (see state_hash.h).

#include "state_hash.h"

#include <rex/logging.h>

#include "game_memory.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

namespace oratan {
namespace {

namespace game = oratan::game;

constexpr const char* kRegionNames[kStateRegions] = {"game", "robot 1", "robot 2",
                                                     "inputs/module/stage"};

// FNV-1a; the regions are small (tens of KB per tick).
uint64_t Hash(const uint8_t* data, size_t size, uint64_t h = 0xCBF29CE484222325ull) {
  for (size_t i = 0; i < size; ++i) {
    h = (h ^ data[i]) * 0x100000001B3ull;
  }
  return h;
}

// Robot fields holding guest heap pointers: where the allocator put things can
// differ between sessions with identical gameplay (the native renderer on and
// off placed them 0xC1A0 apart), so they are zeroed before hashing.
constexpr size_t kRobotPointerFields[] = {offsetof(game::Robot, heap_pointer_0B4),
                                          offsetof(game::Robot, heap_pointer_0C8),
                                          offsetof(game::Robot, heap_pointer_2D8)};

// Zeroes the robot pointer fields that fall inside `copy`, a copy of guest
// bytes that starts at Game offset `start`.
void MaskPointers(uint8_t* copy, size_t start, size_t size) {
  for (size_t robot = 0; robot < 2; ++robot) {
    for (size_t field : kRobotPointerFields) {
      const size_t at = offsetof(game::Game, robots) + robot * sizeof(game::Robot) + field;
      if (at >= start && at + 4 <= start + size) std::memset(copy + (at - start), 0, 4);
    }
  }
}

}  // namespace

StateHashes HashState(uint8_t* base, int32_t game_bytes) {
  StateHashes h{};
  if (game::Game* g = game::GetGame(base)) {
    static thread_local std::vector<uint8_t> copy;
    const auto* bytes = reinterpret_cast<const uint8_t*>(g);
    copy.assign(bytes, bytes + game_bytes);
    MaskPointers(copy.data(), 0, copy.size());
    h[kStateGame] = Hash(copy.data(), copy.size());
    for (int robot = 0; robot < 2; ++robot) {
      const size_t start = offsetof(game::Game, robots) + robot * sizeof(game::Robot);
      copy.assign(bytes + start, bytes + start + sizeof(game::Robot));
      MaskPointers(copy.data(), start, copy.size());
      h[robot == 0 ? kStateRobot1 : kStateRobot2] = Hash(copy.data(), copy.size());
    }
  }
  uint64_t flow = Hash(reinterpret_cast<const uint8_t*>(game::GetSideInputs(base)),
                       2 * sizeof(game::SideInput));
  flow = Hash(base + game::kModuleIndex, 1, flow);
  flow = Hash(base + game::kNextStage, 4, flow);
  h[kStateFlow] = flow;
  return h;
}

std::string DifferingRegions(const StateHashes& a, const StateHashes& b) {
  std::string regions;
  for (uint32_t r = 0; r < kStateRegions; ++r) {
    if (a[r] != b[r]) regions += std::string(regions.empty() ? "" : ", ") + kRegionNames[r];
  }
  return regions;
}

void CaptureStateRegions(uint8_t* base, int32_t game_bytes, std::vector<uint8_t>* out) {
  out->clear();
  if (game::Game* g = game::GetGame(base)) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(g);
    out->insert(out->end(), bytes, bytes + game_bytes);
  }
  const auto* inputs = reinterpret_cast<const uint8_t*>(game::GetSideInputs(base));
  out->insert(out->end(), inputs, inputs + 2 * sizeof(game::SideInput));
}

void WriteStateRegions(const std::vector<uint8_t>& regions, const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fwrite(regions.data(), 1, regions.size(), f);
  std::fclose(f);
  REXLOG_INFO("state regions written to {}", path);
}

void DumpStateRegions(uint8_t* base, const std::string& path, int32_t game_bytes) {
  std::vector<uint8_t> regions;
  CaptureStateRegions(base, game_bytes, &regions);
  WriteStateRegions(regions, path);
}

}  // namespace oratan
