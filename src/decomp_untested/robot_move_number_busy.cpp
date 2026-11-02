// Hand-decompiled move number of a robot.
//
// sub_82242528(control), called each tick by the driver (sub_821F8168) and
// by other code. It clears +580 bits 0x04100090 and +608, flips +584
// between 0 and 1 while +580 bit 0x10000000 is set, sets +106 from the
// button state (+576), +577 and +584 (a formula per button state, below),
// and calls the robot's handler for that button state: the table at
// 0x828A1E98 has a list per robot (17 for every robot above 17) with 164 + 164
// entries, the handlers at +164. Robot 17 (Bradtos) then ORs +2192 into +608.
// Button states 40 and 42 pass sub_82241DC8's result to the handler (the
// others -1); 81 and 82 also set +580 bit 32.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <cstdint>
#include <string>

#include <fmt/format.h>

REX_EXTERN(__imp__sub_82242528);
REX_EXTERN(__imp__sub_82241DC8);

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
  k.r1.u64 = ctx.r1.u32 - 96;
  k.r3.u64 = c.a;
  __imp__sub_82241DC8(k, base);
  const uint32_t found = k.r3.u32;
  c.SetW(608, 0);
  c.SetW(580, c.W(580) & 0xFBEFFF6F);
  if (c.W(580) & 0x10000000) {
    if (c.W(584) == 0) {
      c.SetW(584, 1);
    } else if (c.W(584) == 1) {
      c.SetW(584, 0);
    }
  }
  const uint32_t b576 = c.B(576);
  const uint32_t b577 = c.B(577);
  const uint32_t w584 = c.W(584);
  uint32_t r4 = uint32_t(-1);
  switch (b576) {
    case 0:
      c.SetH(106, 0);
      break;
    case 1:
      c.SetH(106, 3 * w584 + b577 + 1);
      break;
    case 2:
      c.SetH(106, 3 * w584 + b577 + 13);
      break;
    case 5:
      c.SetH(106, 3 * w584 + b577 + 25);
      break;
    case 6: case 7:
      c.SetH(106, 3 * w584 + b577 + 31);
      break;
    case 20:
      c.SetH(106, b577 + 43);
      break;
    case 24: case 25:
      c.SetH(106, (w584 + 23) * 2 + b577);
      break;
    case 26:
      c.SetH(106, (w584 + 27) * 2 + b577);
      break;
    case 28: case 29: case 30:
      c.SetH(106, (w584 + 14) * 4 + b577);
      break;
    case 31:
      c.SetH(106, (w584 + 18) * 4 + b577);
      break;
    case 32:
      c.SetH(106, b577 + 112);
      break;
    case 33:
      c.SetH(106, b577 + 109);
      break;
    case 34:
      c.SetH(106, b577 + 115);
      break;
    case 38:
      c.SetH(106, 3 * w584 + b577 + 142);
      break;
    case 40:
      r4 = found;
      c.SetH(106, 3 * w584 + b577 + 166);
      break;
    case 41:
      c.SetH(106, 3 * w584 + b577 + 154);
      break;
    case 42:
      r4 = found;
      c.SetH(106, 3 * w584 + b577 + 178);
      break;
    case 60: case 77:
      c.SetH(106, b577 + 316);
      break;
    case 61:
      c.SetH(106, b577 + 319);
      break;
    case 62:
      c.SetH(106, b577 + 322);
      break;
    case 63:
      c.SetH(106, b577 + 325);
      break;
    case 80:
      c.SetH(106, b577 + 211);
      break;
    case 81:
      c.SetW(580, c.W(580) | 32);
      c.SetH(106, b577 + 208);
      break;
    case 82:
      c.SetW(580, c.W(580) | 32);
      c.SetH(106, b577 + 214);
      break;
    case 92:
      c.SetH(106, 3 * w584 + b577 + 223);
      break;
    case 93:
      c.SetH(106, 3 * w584 + b577 + 217);
      break;
    case 94:
      c.SetH(106, 3 * w584 + b577 + 229);
      break;
    case 96:
      c.SetH(106, b577 + 238);
      break;
    case 97:
      c.SetH(106, b577 + 235);
      break;
    case 98:
      c.SetH(106, b577 + 241);
      break;
    case 108:
      c.SetH(106, 3 * w584 + b577 + 250);
      break;
    case 109:
      c.SetH(106, 3 * w584 + b577 + 244);
      break;
    case 110:
      c.SetH(106, 3 * w584 + b577 + 256);
      break;
    case 114:
      c.SetH(106, b577 + 286);
      break;
    case 116:
      c.SetH(106, 3 * w584 + b577 + 196);
      break;
    case 117:
      c.SetH(106, 3 * w584 + b577 + 190);
      break;
    case 118:
      c.SetH(106, 3 * w584 + b577 + 202);
      break;
    case 120:
      c.SetH(106, b577 + 292);
      break;
    case 121:
      c.SetH(106, b577 + 289);
      break;
    case 122:
      c.SetH(106, b577 + 295);
      break;
    case 127:
      c.SetH(106, b577 + 88);
      break;
    case 132:
      c.SetH(106, b577 + 301);
      break;
    case 133:
      c.SetH(106, b577 + 298);
      break;
    case 134:
      c.SetH(106, b577 + 304);
      break;
    case 136:
      c.SetH(106, b577 + 310);
      break;
    case 137:
      c.SetH(106, b577 + 307);
      break;
    case 138:
      c.SetH(106, b577 + 313);
      break;
    case 144: case 146:
      c.SetH(106, b577 + 90);
      break;
    case 145:
      c.SetH(106, b577 + 92);
      break;
    case 148:
      c.SetH(106, b577 + 96);
      break;
    case 152:
      c.SetH(106, 99);
      break;
    case 156:
      c.SetH(106, 100);
      break;
    case 159:
      c.SetH(106, 2 * w584 + b577 + 101);
      break;
    case 160:
      c.SetH(106, b577 + 328);
      break;
    case 161:
      c.SetH(106, b577 + 331);
      break;
    case 162:
      c.SetH(106, b577 + 334);
      break;
    case 163:
      c.SetH(106, b577 + 337);
      break;
    default:
      break;
  }

  const uint32_t robot = c.B(2) > 17 ? 17 : c.B(2);
  const uint32_t list = *game::GuestPtr<be<uint32_t>>(base, kHandlerLists + robot * 4);
  if (b576 < 164) {
    const uint32_t handler = *game::GuestPtr<be<uint32_t>>(base, list + (b576 + 164) * 4);
    if (handler != 0) {
      k.ctr.u64 = handler;
      k.r3.u64 = c.a;
      k.r4.s64 = int32_t(r4);
      rex::runtime::ResolveIndirectFunction(handler)(k, base);
    }
  }
  if (c.B(2) == 17) c.SetW(608, c.W(608) | c.W(2192));
}

void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_82242528", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_82242528(original, base);
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

REX_HOOK_RAW(sub_82242528) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_82242528(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::MoveNumber(ctx, base);
}
