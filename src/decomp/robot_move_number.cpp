// Hand-decompiled move number of a robot.
//
// sub_82241FB0(control), called each tick by the driver (sub_821F8168)
// unless Game +24932 is set (sub_82242528 then). By the button state
// (+576) it may run sub_82241D58, set +580 bits 32 / 64, or set +106; then it
// calls the robot's handler for the button state from the first 164 entries
// of its list at 0x828A1E98 (17 for every robot above 17). While it runs,
// +584 is changed: button states 14 / 15 set bit 2, and +580 bit
// 0x10000000 flips it between 0 and 1; it is put back at the end. Robot 9
// clears +608 bits 4 / 0x80000 when any of 0x7007FA is set and sets 512
// while +450 runs; robot 17 copies +2192 into +608.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <cstdint>
#include <string>

#include <fmt/format.h>

REX_EXTERN(__imp__sub_82241FB0);
REX_EXTERN(__imp__sub_82241DC8);
REX_EXTERN(__imp__sub_82241E58);
REX_EXTERN(__imp__sub_82241D58);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;

constexpr uint32_t kControl2 = 29516;
constexpr uint32_t kHandlerLists = 0x828A1E98;

struct Control {
  uint8_t* base;
  uint32_t a;
  uint8_t B(uint32_t o) const { return *game::GuestPtr<uint8_t>(base, a + o); }
  uint16_t H(uint32_t o) const { return *game::GuestPtr<be<uint16_t>>(base, a + o); }
  void SetH(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint16_t>>(base, a + o) = uint16_t(v); }
  uint32_t W(uint32_t o) const { return *game::GuestPtr<be<uint32_t>>(base, a + o); }
  void SetW(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint32_t>>(base, a + o) = v; }
};

void MoveNumber(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  PPCContext k = ctx;
  k.r1.u64 = ctx.r1.u32 - 112;
  k.r3.u64 = c.a;
  __imp__sub_82241DC8(k, base);
  const uint32_t found = k.r3.u32;
  k.r3.u64 = c.a;
  __imp__sub_82241E58(k, base);
  const auto d58 = [&] {
    k.r3.u64 = c.a;
    __imp__sub_82241D58(k, base);
  };
  const auto set_580 = [&](uint32_t bits) { c.SetW(580, c.W(580) | bits); };

  const uint32_t saved_584 = c.W(584);
  // The direction class: 584 0 -> 1, 1 -> 0, 2 -> 3, else 2.
  uint32_t cls = saved_584 == 0 ? 1 : saved_584 == 1 ? 0 : saved_584 == 2 ? 3 : 2;
  const uint32_t b576 = c.B(576);
  if (b576 == 14 || b576 == 15) {
    c.SetW(584, saved_584 | 2);
    cls |= 2;
  }
  const uint32_t w580 = c.W(580);
  if (w580 & 0x10000000) {
    if (c.W(584) == 0) {
      c.SetW(584, 1);
    } else if (c.W(584) == 1) {
      c.SetW(584, 0);
    }
  }

  uint32_t r4 = uint32_t(-1);
  switch (b576) {
    case 0: case 1: case 2: case 4: case 5: case 15: case 20: case 25: case 26: case 32: case 34: case 38:
    case 41: case 44: case 45: case 46: case 48: case 49: case 50: case 52: case 53: case 54: case 76:
    case 97: case 114: case 119: case 120: case 121: case 122: case 124: case 125: case 127: case 128:
    case 129: case 130: case 132: case 133: case 134: case 136: case 137: case 138: case 140: case 141:
    case 142: case 144: case 145: case 146: case 148: case 152: case 156: case 160: case 161: case 162:
    case 163:
      d58();
      break;
    case 3: case 60: case 61: case 62: case 63: case 77: case 80: case 81: case 84: case 85: case 86:
    case 96: case 98: case 100: case 101: case 102:
      d58();
      set_580(32);
      break;
    case 14:
      d58();
      if (c.B(577) != 0) set_580(32);
      break;
    case 16:
      d58();
      set_580(32);
      if (c.B(577) == 1) set_580(64);
      break;
    case 17: case 18: case 64: case 65: case 66: case 68: case 69: case 70: case 73: case 74: case 88:
    case 89: case 90: case 104: case 105: case 106:
      d58();
      set_580(96);
      break;
    case 19:
      c.SetH(106, (c.W(584) & 1) + 172);
      break;
    case 24: case 31:
      r4 = cls;
      break;
    case 28:
      c.SetW(580, w580 | 32);
      c.SetH(106, c.B(577) + (cls + 30) * 4);
      break;
    case 29:
      c.SetW(580, w580 | 32);
      c.SetH(106, c.B(577) + (cls + 34) * 4);
      break;
    case 30:
      d58();
      set_580(32);
      if (c.B(577) == 0) set_580(64);
      break;
    case 40: case 42: case 56: case 57: case 58:
      d58();
      r4 = found;
      break;
    case 72:
      d58();
      set_580(96);
      r4 = found;
      break;
    case 82:
      c.SetW(580, w580 | 32);
      break;
    default:
      break;
  }

  const uint32_t robot = c.B(2) > 17 ? 17 : c.B(2);
  const uint32_t list = *game::GuestPtr<be<uint32_t>>(base, kHandlerLists + robot * 4);
  if (c.B(576) < 164) {
    const uint32_t handler = *game::GuestPtr<be<uint32_t>>(base, list + c.B(576) * 4);
    if (handler != 0) {
      k.ctr.u64 = handler;
      k.r3.u64 = c.a;
      k.r4.s64 = int32_t(r4);
      rex::runtime::ResolveIndirectFunction(handler)(k, base);
    }
  }
  const uint32_t robot2 = c.B(2);
  if (robot2 == 9) {
    if (c.W(608) & 0x7007FA) c.SetW(608, c.W(608) & ~0x80004u);
    if (c.H(450) != 0) c.SetW(608, c.W(608) | 512);
  }
  c.SetW(584, saved_584);
  if (robot2 == 17) c.SetW(608, c.W(2192));
}

void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_82241FB0", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_82241FB0(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        MoveNumber(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} button {} b577 {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.B(576), c.B(577), c.B(2));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_82241FB0) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_82241FB0(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::MoveNumber(ctx, base);
}
