// Hand-decompiled functions of the robot loader (sub_821C5C30).
//
// sub_821C5060: Bal-Bados' form for the stage.
//   The loader passes every robot number through it. Bal-Bados and its two
//   forms (robot_id kBalBados / kBalKeros / kBalBaros) come back as the form
//   for the stage; every other robot comes back unchanged:
//   - plain Bal-Bados in two situations identified by Game::scene_mode /
//     scene_param: (3, 21), or (0, 31) while the Ending module runs;
//   - Bal-Baros on the water stage, Bal-Keros on the final stage. The stage is
//     the next fight's (kNextStage), and when scene_mode is 0 also the stage in
//     the side record that Game::record_side selects; the water stage is
//     checked first;
//   - plain Bal-Bados otherwise.
//   It reads the game state through sub_822AC098 (the Game pointer) and
//   sub_822A6B70 (the module byte) and writes no memory.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

REX_EXTERN(__imp__sub_821C5060);

namespace oratan::decomp {
namespace {

using game::Module;
namespace robot_id = game::robot_id;
namespace stage_id = game::stage_id;

// What sub_821C5060's decision depends on, read from guest memory.
struct StageFormInputs {
  int32_t robot;
  Module module;
  int32_t next_stage;
  uint16_t scene_mode;
  uint16_t scene_param;
  // The selected side record's stage; only meaningful when scene_mode is 0
  // (the original only reads it then).
  int32_t side_stage;
};

bool IsBalBadosForm(int32_t robot) {
  return robot == robot_id::kBalBados || robot == robot_id::kBalKeros ||
         robot == robot_id::kBalBaros;
}

int32_t StageFormOf(const StageFormInputs& in) {
  if (!IsBalBadosForm(in.robot)) {
    return in.robot;
  }
  if (in.scene_mode == 3 && in.scene_param == 21) {
    return robot_id::kBalBados;
  }
  if (in.module == Module::kEnding && in.scene_mode == 0 && in.scene_param == 31) {
    return robot_id::kBalBados;
  }
  const bool check_side = in.scene_mode == 0;
  if (in.next_stage == stage_id::kWater || (check_side && in.side_stage == stage_id::kWater)) {
    return robot_id::kBalBaros;
  }
  if (in.next_stage == stage_id::kTangram ||
      (check_side && in.side_stage == stage_id::kTangram)) {
    return robot_id::kBalKeros;
  }
  return robot_id::kBalBados;
}

// Reads the inputs the way the original does: the game state only for the
// Bal-Bados forms, the side record only when scene_mode is 0. The Game pointer
// isn't null-checked (neither is it in the original).
StageFormInputs ReadStageFormInputs(const uint8_t* base, int32_t robot) {
  StageFormInputs in{};
  in.robot = robot;
  if (!IsBalBadosForm(robot)) {
    return in;
  }
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  const auto* g = game::GuestPtr<game::Game>(base, game_address);
  in.module = game::CurrentModule(base);
  in.next_stage = *game::GuestPtr<game::be<int32_t>>(base, game::kNextStage);
  in.scene_mode = g->scene_mode;
  in.scene_param = g->scene_param;
  if (in.scene_mode == 0) {
    // record_side is signed and not range-checked, as in the original.
    const uint32_t record = game_address + uint32_t(offsetof(game::Game, side_records)) +
                            uint32_t(int32_t(g->record_side) * int32_t(sizeof(game::GameSideRecord)));
    in.side_stage = game::GuestPtr<game::GameSideRecord>(base, record)->stage;
  }
  return in;
}

// Runs the original and the rewrite over every combination of inputs and logs
// the mismatches. Needs a live Game (its fields are overwritten and restored).
bool Verify(PPCContext& ctx, uint8_t* base) {
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) {
    return false;  // try again on a later call
  }
  auto* g = game::GuestPtr<game::Game>(base, game_address);
  uint8_t* module = base + game::kModuleIndex;
  auto* next_stage = game::GuestPtr<game::be<int32_t>>(base, game::kNextStage);

  // Save everything the sweep writes.
  const uint8_t saved_module = *module;
  const int32_t saved_next_stage = *next_stage;
  const uint16_t saved_mode = g->scene_mode, saved_param = g->scene_param;
  const int8_t saved_side = g->record_side;
  const int32_t saved_records[2] = {g->side_records[0].stage, g->side_records[1].stage};

  uint32_t inputs = 0, mismatches = 0;
  for (int32_t robot = -2; robot <= 40; ++robot) {
    for (int module_value : {0, 1, 2, 3, 4, 5, 6, 7, 0x84, 0xFF}) {
      for (int32_t stage = -2; stage <= 40; ++stage) {
        for (uint16_t mode : {0, 1, 2, 3, 4, 0x8000, 0xFFFF}) {
          for (uint16_t param : {0, 5, 21, 31, 32, 0xFFFF}) {
            for (int8_t side : {0, 1}) {
              for (int32_t side_stage : {-1, 0, 5, 21, 22}) {
                *module = uint8_t(module_value);
                *next_stage = stage;
                g->scene_mode = mode;
                g->scene_param = param;
                g->record_side = side;
                g->side_records[side].stage = side_stage;
                g->side_records[side ^ 1].stage = side_stage == 5 ? 21 : 5;  // decoy

                PPCContext original = ctx;
                original.r3.s64 = robot;
                __imp__sub_821C5060(original, base);
                const int32_t expected = original.r3.s32;
                const int32_t actual = StageFormOf(ReadStageFormInputs(base, robot));
                ++inputs;
                if (expected != actual) {
                  if (mismatches < 20) {
                    REXLOG_ERROR(
                        "decomp verify sub_821C5060: robot {} module {} stage {} mode {} param "
                        "{} side {} side stage {}: original {}, rewrite {}",
                        robot, module_value, stage, mode, param, side, side_stage, expected,
                        actual);
                  }
                  ++mismatches;
                }
              }
            }
          }
        }
      }
    }
  }

  *module = saved_module;
  *next_stage = saved_next_stage;
  g->scene_mode = saved_mode;
  g->scene_param = saved_param;
  g->record_side = saved_side;
  g->side_records[0].stage = saved_records[0];
  g->side_records[1].stage = saved_records[1];
  REXLOG_INFO("decomp verify sub_821C5060: {} inputs, {} mismatches", inputs, mismatches);
  return true;
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821C5060) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821C5060(ctx, base);
    return;
  }
  static bool verified = false;
  if (!verified && REXCVAR_GET(decomp_verify)) {
    verified = oratan::decomp::Verify(ctx, base);
  }
  const int32_t robot = ctx.r3.s32;
  if (!oratan::decomp::IsBalBadosForm(robot)) {
    return;  // r3 unchanged, as in the original
  }
  ctx.r3.s64 = oratan::decomp::StageFormOf(oratan::decomp::ReadStageFormInputs(base, robot));
}
