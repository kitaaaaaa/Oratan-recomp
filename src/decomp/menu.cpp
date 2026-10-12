// Hand-decompiled functions of the main menu.
//
// sub_8213CCD0(side): the main menu's side choice (1P = side 0, 2P = side 1).
//   Gives the active controller (kActiveController, handed out on the title
//   screen) to the chosen side and -1 (none) to the other. With no active
//   controller, side 0 gets controller 0 and side 1 controller 1 whatever the
//   choice. Then every controller except the active one is disabled
//   (ControllerMasks). The side isn't range-checked: any value other than 0
//   or 1 leaves both sides without a controller.
//   It calls sub_82114C88 (read the active controller) and, per controller,
//   sub_82114CD0 -> sub_82114A00 (enable / disable it); all three only touch
//   the globals above, nothing outside guest memory.
//
// The same hook runs local versus' step afterwards (oratan::OnSideChosen), with
// the rewrite or the original alike.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "../local_versus.h"
#include "decomp.h"

#include <climits>
#include <cstdint>
#include <initializer_list>

#include <fmt/format.h>

REX_EXTERN(__imp__sub_8213CCD0);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;

// sub_82114C88: the active controller, or -1 for none.
int32_t ActiveController(const uint8_t* base) {
  const uint32_t pad = *game::GuestPtr<game::be<uint32_t>>(base, game::kActiveController);
  return pad < 4 ? int32_t(pad) : -1;
}

// sub_82114A00(pad, on): enables or disables a controller; either way clears
// its bit in the two other masks.
void SetControllerEnabled(uint8_t* base, int32_t pad, bool on) {
  if (uint32_t(pad) >= 4) return;
  const uint32_t bit = 1u << pad;
  game::ControllerMasks* masks = game::GetControllerMasks(base);
  masks->enabled = on ? (masks->enabled | bit) : (masks->enabled & ~bit);
  masks->unk_mask_4 = masks->unk_mask_4 & ~bit;
  masks->unk_mask_8 = masks->unk_mask_8 & ~bit;
}

// sub_8213CCD0.
void ChooseSide(uint8_t* base, int32_t side) {
  const int32_t active = ActiveController(base);
  game::SideInput* inputs = game::GetSideInputs(base);
  if (active < 0) {
    inputs[0].controller = 0;
    inputs[1].controller = 1;
  } else {
    inputs[0].controller = side == 0 ? active : -1;
    inputs[1].controller = side == 1 ? active : -1;
  }
  // sub_82114CD0(pad, false) for each pad: it skips the active controller.
  for (int32_t pad = 0; pad < 4; ++pad) {
    if (pad != active) SetControllerEnabled(base, pad, false);
  }
}

// Runs the original and the rewrite over every combination of inputs and logs
// the mismatches.
void Verify(PPCContext& ctx, uint8_t* base) {
  auto* active = game::GuestPtr<game::be<uint32_t>>(base, game::kActiveController);
  game::SideInput* inputs = game::GetSideInputs(base);
  game::ControllerMasks* masks = game::GetControllerMasks(base);

  Verifier verifier("sub_8213CCD0", base,
                    {{game::kActiveController, 4, "active controller"},
                     {game::kSideInputs, uint32_t(2 * sizeof(game::SideInput)), "side inputs"},
                     {game::kControllerMasks, uint32_t(sizeof(game::ControllerMasks)),
                      "controller masks"}});
  for (int32_t side : {INT_MIN, -2, -1, 0, 1, 2, 3, 4, INT_MAX}) {
    for (uint32_t pad : {0xFFFFFFFFu, 0u, 1u, 2u, 3u, 4u, 0x7FFFFFFFu, 0x80000000u}) {
      for (int32_t pad0 = -1; pad0 <= 3; ++pad0) {
        for (int32_t pad1 = -1; pad1 <= 3; ++pad1) {
          for (uint32_t enabled : {0u, 0xFu, 0x5u, 0xAu, 0xFFFFFFFFu}) {
            for (uint32_t mask_4 : {0u, 0x6u, 0xFFFFFFFFu}) {
              for (uint32_t mask_8 : {0u, 0x9u, 0xFFFFFFFFu}) {
                verifier.Check(
                    [&] {
                      *active = pad;
                      inputs[0].controller = pad0;
                      inputs[1].controller = pad1;
                      masks->enabled = enabled;
                      masks->unk_mask_4 = mask_4;
                      masks->unk_mask_8 = mask_8;
                    },
                    [&] {
                      PPCContext original = ctx;
                      original.r3.s64 = side;
                      __imp__sub_8213CCD0(original, base);
                      return int64_t(0);
                    },
                    [&] {
                      ChooseSide(base, side);
                      return int64_t(0);
                    },
                    [&] {
                      return fmt::format("side {} active {:#x} pads {} {} masks {:#x} {:#x} {:#x}",
                                         side, pad, pad0, pad1, enabled, mask_4, mask_8);
                    });
              }
            }
          }
        }
      }
    }
  }
  verifier.Finish();
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_8213CCD0) {
  const int32_t side = ctx.r3.s32;
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_8213CCD0(ctx, base);
  } else {
    static bool verified = false;
    if (!verified && REXCVAR_GET(decomp_verify)) {
      verified = true;
      oratan::decomp::Verify(ctx, base);
    }
    oratan::decomp::ChooseSide(base, side);
  }
  oratan::OnSideChosen(base, uint32_t(side));
}
