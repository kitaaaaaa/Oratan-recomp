// Hand-decompiled functions of the game flow (modules and scenes).
//
// sub_822A6A30(file_name, scene, step): the module switcher. Every module
//   change goes through it (usually via sub_822A6BA0, which passes scene 0,
//   step 0). It:
//   - copies the module's file name ("GAME.BIN", a guest string) into
//     Game::module_file;
//   - sets Game::scene and Game::scene_step to start the module in;
//   - sets Game::module_switch_set to 1 and module_switch_cleared to 0;
//   - looks the name up in kModuleFileNames, ignoring case (sub_82565A90, a
//     stricmp), and stores its index as the module (kModuleIndex), or 0xFF if
//     no entry matches.
//   It only writes memory: the module's code starts later, from the frame
//   loop. No dispatch over modules happens here. Its result register is the
//   last name comparison (0 if a name matched); callers seem to ignore it, but
//   the rewrite returns the same value.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>

#include <fmt/format.h>

REX_EXTERN(__imp__sub_822A6A30);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;

// sub_82565A90(a, b): compares two guest strings ignoring the case of A-Z.
// 0 if equal; otherwise a's byte minus b's at the first difference (each
// lowercased first), or a's byte if b ended first.
int32_t CompareIgnoreCase(const uint8_t* base, uint32_t a, uint32_t b) {
  for (;; ++a, ++b) {
    int32_t ca = base[a], cb = base[b];
    if (cb == 0) return ca;
    if (ca == cb) continue;
    if (cb >= 'A' && cb <= 'Z') cb |= 0x20;
    if (ca >= 'A' && ca <= 'Z') ca |= 0x20;
    if (ca != cb) return ca - cb;
  }
}

// sub_822A6A30. Returns what the original leaves in r3 (sign-extended to 64 bits).
int64_t SwitchModule(uint8_t* base, uint32_t file_name, uint16_t scene, uint16_t step) {
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  auto* g = game::GuestPtr<game::Game>(base, game_address);
  // strcpy, byte by byte (the name is a guest string).
  for (uint32_t i = 0;; ++i) {
    const uint8_t c = base[file_name + i];
    base[game_address + uint32_t(offsetof(game::Game, module_file)) + i] = c;
    if (c == 0) break;
  }
  g->scene_step = step;
  g->scene = int16_t(scene);
  g->module_switch_set = 1;
  g->module_switch_cleared = 0;

  uint8_t& module = base[game::kModuleIndex];
  module = 0xFF;
  int64_t result = game_address;  // only if the table were empty
  const auto* names = game::GuestPtr<game::be<uint32_t>>(base, game::kModuleFileNames);
  for (uint32_t index = 0; names[index] != 0; ++index) {
    result = CompareIgnoreCase(base, names[index], file_name);
    if (result == 0) {
      module = uint8_t(index);
      break;
    }
  }
  return result;
}

// Runs the original and the rewrite over module names (each table name,
// other spellings, unknown names) and scene/step values, and logs the
// mismatches. The test names are written to unused guest stack below the
// caller's frame. Needs a live Game.
bool Verify(PPCContext& ctx, uint8_t* base) {
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) {
    return false;  // try again on a later call
  }
  const uint32_t name_buffer = ctx.r1.u32 - 0x2000;
  constexpr uint32_t kNameBufferSize = 32;
  auto field = [&](size_t offset) { return game_address + uint32_t(offset); };

  Verifier verifier("sub_822A6A30", base,
                    {{game::kModuleIndex, 1, "module"},
                     {field(offsetof(game::Game, scene)), 4, "game scene and step"},
                     {field(offsetof(game::Game, module_file)),
                      uint32_t(sizeof(game::Game::module_file)), "game module file"},
                     {field(offsetof(game::Game, module_switch_cleared)), 3,
                      "game module switch flags"},
                     {name_buffer, kNameBufferSize, "test name (guest stack)"}});
  const char* names[] = {"OPEN.BIN",  "GAME.BIN", "NETVS.BIN", "CHRSEL.BIN", "ENDING.BIN",
                         "CSTM.BIN",  "MTEST.BIN", "game.bin", "Netvs.Bin",  "GAME.BI",
                         "GAME.BINX", "",          "XYZ",      "OPEN.BIN ",  "[AME.BIN"};
  for (const char* name : names) {
    for (uint16_t scene : {0, 1, 2, 3, 5, 0x7FFF, 0x8000, 0xFFFF}) {
      for (uint16_t step : {0, 9, 0x0F, 0x15, 0x17, 0xFFFF}) {
        verifier.Check(
            [&] {
              std::memset(base + name_buffer, 0xCC, kNameBufferSize);
              std::memcpy(base + name_buffer, name, std::strlen(name) + 1);
              // A previous, longer name in the buffer shows whether the copy
              // stops at the terminator.
              std::memset(game::GuestPtr<uint8_t>(base, field(offsetof(game::Game, module_file))),
                          'Z', sizeof(game::Game::module_file));
            },
            [&] {
              PPCContext original = ctx;
              original.r3.u64 = name_buffer;
              original.r4.u64 = scene;
              original.r5.u64 = step;
              __imp__sub_822A6A30(original, base);
              return original.r3.s64;
            },
            [&] { return SwitchModule(base, name_buffer, scene, step); },
            [&] { return fmt::format("name '{}' scene {:#x} step {:#x}", name, scene, step); });
      }
    }
  }
  verifier.Finish();
  return true;
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_822A6A30) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_822A6A30(ctx, base);
    return;
  }
  static bool verified = false;
  if (!verified && REXCVAR_GET(decomp_verify)) {
    verified = oratan::decomp::Verify(ctx, base);
  }
  ctx.r3.s64 = oratan::decomp::SwitchModule(base, ctx.r3.u32, ctx.r4.u16, ctx.r5.u16);
}
