// Robot probe (developer): logs side 1's robot tick, for checking a port of
// the robot code outside the game against the game itself.
//
// With --robot_probe_out=FILE (and --memory_excerpt_list / _out set), the
// first fight tick of side 1's robot (sub_821FC260) writes the memory excerpt
// right then, and from then on every side 1 tick appends to FILE: the
// controller it reads (ControllerState::held), its side's held actions
// (SideInput +224), and the battle block's changes (first 0xD000 bytes):
// those made since the last record (other robots, levers, the frame) and
// those the tick itself made. A port started from the excerpt can replay the
// changes and compare its tick with the game's, tick by tick.
//
// File (little-endian): "OXPR", u32 version 1, u32 battle block address, then
// per tick: u32 tick, u32 controller held, 16 bytes of actions (as in
// memory), then two change lists (before the tick, made by it), each u32 run
// count and per run u32 offset, u32 length, bytes.

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "debug_tools.h"
#include "game_memory.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

REXCVAR_DEFINE_STRING(robot_probe_out, "", "Oratan",
                      "Developer: log side 1's robot ticks (input and battle block changes) "
                      "to this file, starting from a memory excerpt written on its first fight tick");
REXCVAR_DEFINE_INT32(robot_probe_ticks, 0, "Oratan", "Developer: stop the robot probe after this many ticks (0: no limit)");

REX_EXTERN(__imp__sub_821FC260);

namespace {

namespace game = oratan::game;

constexpr uint32_t kBlockSize = 0xD000;
constexpr uint32_t kControl1 = 25020;

struct Probe {
  FILE* file = nullptr;
  bool finished = false;
  uint32_t game = 0;
  uint32_t ticks = 0;
  std::vector<uint8_t> shadow;
};
Probe g_probe;

void Write32(FILE* f, uint32_t v) {
  const uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
  std::fwrite(b, 1, 4, f);
}

// Writes the runs where `now` differs from the shadow, and updates the shadow.
void WriteChanges(FILE* f, const uint8_t* now, std::vector<uint8_t>& shadow) {
  struct Run {
    uint32_t offset, length;
  };
  std::vector<Run> runs;
  uint32_t i = 0;
  while (i < kBlockSize) {
    if (now[i] == shadow[i]) {
      ++i;
      continue;
    }
    uint32_t j = i;
    while (j < kBlockSize && (now[j] != shadow[j] || (j + 1 < kBlockSize && now[j + 1] != shadow[j + 1]))) ++j;
    runs.push_back({i, j - i});
    i = j;
  }
  Write32(f, uint32_t(runs.size()));
  for (const Run& r : runs) {
    Write32(f, r.offset);
    Write32(f, r.length);
    std::fwrite(now + r.offset, 1, r.length, f);
    std::memcpy(shadow.data() + r.offset, now + r.offset, r.length);
  }
}

bool Logging(uint8_t* base, uint32_t control) {
  if (g_probe.finished) return false;
  const std::string out = REXCVAR_GET(robot_probe_out);
  if (out.empty()) return false;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address || control != game_address + kControl1) return false;
  const auto& g = *game::GuestPtr<game::Game>(base, game_address);
  if (!game::IsFightInProgress(g, game::CurrentModule(base))) return g_probe.file != nullptr;
  if (!g_probe.file) {
    if (uint16_t(g.scene_step) != game::battle_step::kFight) return false;
    if (!oratan::WriteMemoryExcerpt(base)) {
      g_probe.finished = true;
      return false;
    }
    g_probe.file = std::fopen(out.c_str(), "wb");
    if (!g_probe.file) {
      REXLOG_ERROR("robot_probe: can't write {}", out);
      g_probe.finished = true;
      return false;
    }
    std::fwrite("OXPR", 1, 4, g_probe.file);
    Write32(g_probe.file, 1);
    Write32(g_probe.file, game_address);
    g_probe.game = game_address;
    g_probe.shadow.assign(base + game_address, base + game_address + kBlockSize);
    REXLOG_INFO("robot_probe: logging side 1's robot from here");
  }
  return true;
}

void Finish() {
  if (g_probe.file) std::fclose(g_probe.file);
  g_probe.file = nullptr;
  g_probe.finished = true;
  REXLOG_INFO("robot_probe: {} ticks logged", g_probe.ticks);
}

}  // namespace

REX_HOOK_RAW(sub_821FC260) {
  const uint32_t control = ctx.r3.u32;
  if (!Logging(base, control)) {
    __imp__sub_821FC260(ctx, base);
    return;
  }
  if (game::LoadGuestAddress(base, game::kGamePointer) != g_probe.game) {
    Finish();  // the battle ended
    __imp__sub_821FC260(ctx, base);
    return;
  }
  FILE* f = g_probe.file;
  const uint8_t* block = base + g_probe.game;
  const auto* side = game::GuestPtr<game::SideInput>(base, game::kSideInputs);
  const int32_t pad = side[0].controller;
  uint32_t held = 0;
  if (pad >= 0 && pad < 4) held = game::GuestPtr<game::ControllerState>(base, game::kControllerStates)[pad].held;
  Write32(f, g_probe.ticks);
  Write32(f, held);
  std::fwrite(reinterpret_cast<const uint8_t*>(&side[0]) + 224, 1, 16, f);
  WriteChanges(f, block, g_probe.shadow);
  __imp__sub_821FC260(ctx, base);
  WriteChanges(f, block, g_probe.shadow);
  ++g_probe.ticks;
  const int32_t limit = REXCVAR_GET(robot_probe_ticks);
  if (limit > 0 && g_probe.ticks >= uint32_t(limit)) Finish();
}
