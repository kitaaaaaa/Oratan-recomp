// Hand-decompiled weapon gauges and range flags of a robot.
//
// sub_821E0308, called each tick by the driver (sub_821F8168) unless the
// robot's control mode (+1) is 3:
//   - Gauges: each weapon has a counter (+408, +404, +412) that drops by 1 a
//     tick toward 0 and is capped at the robot's maximum (parameters +3508,
//     +3510, +3512). While Game +0xCF00 is set the counters are raised to the
//     maximum instead. Dordray (robot 0) skips the drop on 2 of every 10
//     ticks while his +450 counter is negative.
//   - Range flags (+400), two bits per weapon: both bits when the target
//     distance (+4464) is within the weapon's near limit, the low bit when it
//     is within the far limit (or the longer keep limit if the weapon was in
//     range last tick) and the angle to the target is within the weapon's
//     angle limits. Which parameters are used depends on Game +24932; without
//     it the angle is measured from a point ahead of the robot, and only when
//     +4472 is inside its limits. Game +0xCF00 sets the low bits (21).
//   - Flags 128 and 64 from the angle between the two robots against the
//     target's limits (+1676, +1672).
//   - Percentages: (maximum - counter) * 100 / maximum for the gauges (+424,
//     +420, +428) and for +416 (+432). +416 drops by +3618 a tick, stays
//     within 0..+3640, and sets flag 256 below +3642.
// The callees (two angle functions, the angle between two points, the angle
// wrap and the distance) run as the original code.
//
// sub_82257F50, whether a weapon can fire now: see WeaponReady.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <bit>
#include <climits>
#include <cmath>
#include <cstdint>
#include <string>

#include <fmt/format.h>

// Each guest float instruction is its own operation, as in robot_input.cpp.
#pragma clang fp contract(off)

REX_EXTERN(__imp__sub_821E0308);
REX_EXTERN(__imp__sub_822AC098);
REX_EXTERN(__imp__sub_822A6B70);
REX_EXTERN(__imp__sub_822A7558);
REX_EXTERN(__imp__sub_822A75A0);
REX_EXTERN(__imp__sub_822A73D8);
REX_EXTERN(__imp__sub_82227FC8);
REX_EXTERN(__imp__sub_822B6820);
REX_EXTERN(__imp__sub_82257F50);
REX_EXTERN(__imp__sub_82257ED0);
REX_EXTERN(__imp__sub_821FCCE8);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;

constexpr uint32_t kGameBusy = 24932;
constexpr uint32_t kGameHoldGauges = 0xCF00;  // u8: counters held at their maximum
constexpr uint32_t kControl1 = 25020, kControl2 = 29516;
// Move ids by weapon (+0 weapon 2, +96 weapon 1, +192 weapon 0), robot, r6 and variant (u16).
constexpr uint32_t kMoveIds = 0x82066C98;
// Per button state (+576), three u16: a kind (3: variant from the table at kMoveIds - 1128),
// then 1 when sub_82257F50 uses r6 = 1.
constexpr uint32_t kButtonKinds = kMoveIds - 1104;
constexpr uint32_t kZeroFloat = 0x8200095C;
// Limits for +4472 (floats) and the scale of the point ahead (double).
constexpr uint32_t k4472Max = 0x8200218C, k4472Min = 0x82027FF0, k4472MinRobot4 = 0x82018F90;
constexpr uint32_t k4472MinTargetState13 = 0x820172D4;
constexpr uint32_t k4472AngleMin = 0x82021628, k4472AngleMax = 0x82000AB0;
constexpr uint32_t kAheadScale = 0x82007C00;

struct Control {
  uint8_t* base;
  uint32_t a;
  uint8_t B(uint32_t o) const { return *game::GuestPtr<uint8_t>(base, a + o); }
  uint16_t H(uint32_t o) const { return *game::GuestPtr<be<uint16_t>>(base, a + o); }
  int32_t S16(uint32_t o) const { return int16_t(H(o)); }
  uint32_t W(uint32_t o) const { return *game::GuestPtr<be<uint32_t>>(base, a + o); }
  int32_t S32(uint32_t o) const { return int32_t(W(o)); }
  void SetW(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint32_t>>(base, a + o) = v; }
  double F(uint32_t o) const { return double(std::bit_cast<float>(W(o))); }
};

double GuestFloat(const uint8_t* base, uint32_t address) {
  return double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, address))));
}

void SetGuestFloat(uint8_t* base, uint32_t address, double v) {
  *game::GuestPtr<be<uint32_t>>(base, address) = std::bit_cast<uint32_t>(float(v));
}

struct Caller {
  PPCContext k;
  uint8_t* base;
  uint32_t Call(void (*fn)(PPCContext&, uint8_t*)) {
    fn(k, base);
    return k.r3.u32;
  }
  uint32_t Call(void (*fn)(PPCContext&, uint8_t*), uint32_t r3) {
    k.r3.u64 = r3;
    return Call(fn);
  }
};

// divw as the recompiled code does it (0 for x / 0 and INT_MIN / -1).
int32_t Divide(int32_t a, int32_t b) { return (b && !(a == INT_MIN && b == -1)) ? a / b : 0; }

// (max - value) * 100 / max, with the original's trap checks (tw: logged only).
int32_t Percent(uint32_t max, int32_t value) {
  if (max == 0) REXLOG_WARN("tw/td trap hit (type 0)");
  const uint32_t product = uint32_t(int32_t(max) - value) * 100u;
  if ((max & ~(std::rotl(product, 1) - 1)) == 0xFFFFFFFFu) REXLOG_WARN("tw/td trap hit (type 0)");
  return Divide(int32_t(product), int32_t(max));
}

uint32_t Abs16(int32_t v) { return uint32_t(v < 0 ? -v : v); }

void WeaponStatus(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 224;
  const uint32_t frame = k.k.r1.u32;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  const Control p{base, c.W(76)};
  const Control other{base, c.W(72)};
  const uint32_t old_flags = c.W(400);
  const auto param = [&](uint32_t o) { return int32_t(p.H(o)); };

  // Gauge counters.
  constexpr uint32_t kGauge[3] = {408, 404, 412};
  if (int8_t(k.Call(__imp__sub_822A6B70)) != 0 && g.B(kGameHoldGauges) != 0) {
    for (int i = 0; i < 3; ++i) {
      if (c.S32(kGauge[i]) < param(3508 + 2 * i)) c.SetW(kGauge[i], uint32_t(param(3508 + 2 * i)));
    }
  } else {
    const int32_t fall = c.S16(450);
    const bool hold = c.B(2) == 0 && fall < 0 && (-fall) % 10 >= 8;
    if (!hold) {
      for (uint32_t o : kGauge) {
        if (c.S32(o) > 0) c.SetW(o, c.W(o) - 1);
      }
    }
    for (int i = 0; i < 3; ++i) {
      if (c.S32(kGauge[i]) > param(3508 + 2 * i)) c.SetW(kGauge[i], uint32_t(param(3508 + 2 * i)));
    }
  }

  // Range flags.
  c.SetW(400, 0);
  const auto add_flags = [&](uint32_t bits) { c.SetW(400, c.W(400) | bits); };
  k.k.fpscr.disableFlushMode();
  const uint8_t other_robot = other.B(2);
  const bool skip_ranges = (other_robot == 17 || other_robot == 18) && c.B(2) != 4;
  if (!skip_ranges && g.W(kGameBusy) != 0) {
    const double zero = GuestFloat(base, kZeroFloat);
    for (uint32_t i = 0; i < 3; ++i) {
      const double far_limit = p.F(1572 + 4 * i), near_limit = p.F(1584 + 4 * i);
      const int32_t yaw_limit = p.S32(1596 + 8 * i), pitch_limit = p.S32(1600 + 8 * i);
      if (!(far_limit > zero) || !(near_limit > zero) || yaw_limit == 0 || pitch_limit == 0) continue;
      const double distance = c.F(4464);
      if (distance < near_limit) {
        add_flags(3u << (2 * i));
        continue;
      }
      const bool was_in = (old_flags & (3u << (2 * i))) != 0;
      if (!((was_in && distance < p.F(1620 + 4 * i)) || distance < far_limit)) continue;
      if (int32_t(Abs16(c.S16(4488))) > yaw_limit) continue;
      if (int32_t(Abs16(c.S16(4490))) > pitch_limit) continue;
      add_flags(1u << (2 * i));
    }
  } else if (!skip_ranges) {
    const uint8_t robot = c.B(2);
    const double v4472 = c.F(4472), high = GuestFloat(base, k4472Max);
    const double low = GuestFloat(base, robot == 4 ? k4472MinRobot4 : k4472Min);
    bool check = !(v4472 < low) && !(v4472 > high);
    if (!check) {
      const uint32_t state = c.H(272);
      if (state == 12 && (c.B(577) == 0 || (robot == 2 && c.H(274) == 1))) {
        check = true;
      } else if (state == 13) {
        check = true;
      } else if (other.H(272) == 13 && !(c.F(4472) < GuestFloat(base, k4472MinTargetState13))) {
        check = true;
      }
    }
    if (check) {
      const double zero = GuestFloat(base, kZeroFloat);
      const double ahead = std::bit_cast<double>(uint64_t(*game::GuestPtr<be<uint64_t>>(base, kAheadScale)));
      SetGuestFloat(base, frame + 84, zero);
      SetGuestFloat(base, frame + 100, zero);
      for (uint32_t i = 0; i < 3; ++i) {
        const double far_limit = p.F(1464 + 4 * i), near_limit = p.F(1476 + 4 * i);
        const int32_t angle_limit = p.S32(1488 + 4 * i);
        if (!(far_limit > zero) || !(near_limit > zero) || angle_limit == 0) continue;
        const double distance = c.F(4464);
        if (!(distance > near_limit)) {
          add_flags(3u << (2 * i));
          continue;
        }
        const bool was_in = (old_flags & (3u << (2 * i))) != 0;
        if (!((was_in && distance < p.F(1500 + 4 * i)) || distance < far_limit)) continue;
        // A point ahead of the robot along its facing (+8), then the angle
        // and distance from that point to the target.
        k.Call(__imp__sub_822A7558, uint32_t(angle_limit) & 0xFFFF);
        const double reach = double(float(k.k.f1.f64 * p.F(1476 + 4 * i)));
        k.Call(__imp__sub_822A7558, c.H(8));
        const double ahead_x = double(float(std::fma(k.k.f1.f64 * reach, ahead, c.F(24))));
        const double z = c.F(32);
        SetGuestFloat(base, frame + 80, ahead_x);
        k.Call(__imp__sub_822A75A0, c.H(8));
        const double across = k.k.f1.f64 * reach;
        const double point_x = GuestFloat(base, frame + 80);
        const double other_z = other.F(32), other_x = other.F(24);
        k.k.f1.f64 = double(float(point_x - other_x));
        const double ahead_z = double(float(-std::fma(across, ahead, -z)));
        SetGuestFloat(base, frame + 88, ahead_z);
        k.k.f2.f64 = double(float(other_z - ahead_z));
        const uint32_t angle = k.Call(__imp__sub_822A73D8);
        k.Call(__imp__sub_82227FC8, angle - uint32_t(c.S16(8)));
        const int32_t turn = int32_t(k.k.r3.u32 & 0xFFFF);
        SetGuestFloat(base, frame + 96, other.F(24));
        SetGuestFloat(base, frame + 104, other.F(32));
        k.k.r4.u64 = frame + 96;
        k.Call(__imp__sub_822B6820, frame + 80);
        if (turn < p.S32(1488 + 4 * i) && !(k.k.f1.f64 < p.F(1476 + 4 * i))) add_flags(1u << (2 * i));
      }
    }
  }
  if (g.B(kGameHoldGauges) != 0) add_flags(21);

  // Flags 128 / 64 from the angle between the robots.
  if (Control{base, c.W(72)}.B(2) != 17 && g.W(kGameBusy) == 0) {
    const Control other_params{base, other.W(76)};
    k.k.f2.f64 = double(float(c.F(32) - other.F(32)));
    k.k.f1.f64 = double(float(other.F(24) - c.F(24)));
    const uint32_t from_other = (k.Call(__imp__sub_822A73D8) - uint32_t(other.S16(8))) & 0xFFFF;
    const double v4472 = c.F(4472);
    if (v4472 > GuestFloat(base, k4472AngleMin) && v4472 < GuestFloat(base, k4472AngleMax) &&
        c.F(4464) < Control{base, c.W(76)}.F(1684)) {
      k.Call(__imp__sub_82227FC8, uint32_t(c.S16(game::kControlRef2) - c.S16(8)));
      if (int32_t(k.k.r3.u32 & 0xFFFF) < Control{base, c.W(76)}.S32(1680) && (c.W(2192) & 2) == 0) {
        const int32_t turn = int32_t(from_other);
        const int32_t limit128 = other_params.S32(1676), limit64 = other_params.S32(1672);
        if (turn > 0x8000 - limit128 && turn < 0x8000 + limit128) {
          add_flags(128);
        } else if (turn > 0x10000 - limit64 || turn < limit64) {
          add_flags(64);
        }
      }
    }
  }

  // Percentages, and +416.
  const Control own{base, c.W(76)};
  c.SetW(424, uint32_t(Percent(own.H(3508), c.S32(408))));
  c.SetW(420, uint32_t(Percent(own.H(3510), c.S32(404))));
  c.SetW(428, uint32_t(Percent(own.H(3512), c.S32(412))));
  c.SetW(416, c.W(416) - p.H(3618));
  if (c.S32(416) > param(3640)) c.SetW(416, uint32_t(param(3640)));
  if (c.S32(416) < 0) c.SetW(416, 0);
  if (c.S32(416) < param(3642)) add_flags(256);
  c.SetW(432, uint32_t(Percent(p.H(3640), c.S32(416))));
}

void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    verifier = new Verifier("sub_821E0308", base, {{game_address, 0xD000, "game"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821E0308(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        WeaponStatus(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} state {} robot {} flags {:X}", control == game_address + kControl2 ? 2 : 1,
                           c.H(272), c.B(2), c.W(400));
      },
      600);
}


// sub_82257F50(control, weapon, variant, r6): whether the weapon (0, 1, 2)
// can fire now. The move comes from the table at kMoveIds by robot, r6
// (0 / 1; with r6 == 0, 1 when the button state's entry says so),
// variant (replaced by the table at kMoveIds - 1128 for button states of
// kind 3 while +577 < 2), and weapon; r6 == 6 picks robots 2 / 10's own
// moves. sub_82257ED0 gives the move's data, whose +148 * +150 is its cost:
// the weapon's gauge counter (+404 / +408 / +412) must be at most the
// maximum (+3510 / +3508 / +3512) minus the cost. Some robots' moves are
// refused by +2192 bits. When refused, Game + (9640 / 9644 / 9642 + side) * 4
// is set to 32. Returns 1 when allowed (or sub_821FCCE8's result for Ajim).
uint32_t WeaponReady(const Control& c, Caller& k, uint8_t* base, uint32_t weapon, int32_t variant, int32_t r6) {
  k.k.r1.u64 = k.k.r1.u32 - 144;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  const auto table_h = [&](uint32_t address) { return *game::GuestPtr<be<uint16_t>>(base, address); };
  const uint32_t button = c.B(576);
  int32_t move = 0;
  if (r6 == 6) {
    if (c.B(2) == 2) {
      move = weapon == 0 ? 2129 : weapon == 1 ? 2128 : weapon == 2 ? 2130 : 0;
    } else if (c.B(2) == 10) {
      move = weapon == 0 ? 3207 : weapon == 2 ? 3208 : 0;
    }
    r6 = 0;
  } else if (r6 == 0 && table_h(kButtonKinds + 2 + button * 6) == 1) {
    r6 = 1;
  }
  if (table_h(kButtonKinds + c.B(576) * 6) == 3 && c.B(577) < 2) {
    variant = int32_t(*game::GuestPtr<be<uint32_t>>(base, kMoveIds - 1128 + c.W(588) * 4));
  }
  if (weapon >= 3) return 0;
  // The move id from the table for this weapon.
  const auto table_move = [&](uint32_t table) {
    const uint32_t index = (uint32_t(18 * c.B(2) + r6) * 8 + uint32_t(variant)) * 2;
    return int32_t(int16_t(table_h(table + index)));
  };
  const auto refuse = [&](uint32_t slot) {
    const uint32_t side = c.a != game_address + kControl1 ? 1 : 0;
    g.SetW((slot + side) * 4, 32);
    return 0u;
  };
  // Gauge counter `counter` within the maximum at `max` minus the move's cost.
  const auto affordable = [&](uint32_t move_data, uint32_t counter, uint32_t max) {
    const Control m{base, move_data};
    const int32_t cost = m.S16(150) * m.S16(148);
    return c.S32(counter) <= int32_t(Control{base, c.W(76)}.H(max)) - cost;
  };

  if (weapon == 0) {
    if (move == 0) move = table_move(kMoveIds + 192);
    k.k.r4.s64 = move;
    const uint32_t data = k.Call(__imp__sub_82257ED0, c.a);
    const uint32_t robot = c.B(2);
    if (robot == 0 && (c.W(2192) & 1) != 0) return refuse(9640);
    if (robot == 10 && variant == 2 && (c.W(2192) & 1) != 0) return refuse(9640);
    return affordable(data, 404, 3510) ? 1 : refuse(9640);
  }
  if (weapon == 1) {
    if (move == 0) move = table_move(kMoveIds + 96);
    k.k.r4.s64 = move;
    const uint32_t data = k.Call(__imp__sub_82257ED0, c.a);
    const uint32_t robot = c.B(2);
    if (robot == 0 && (c.W(2192) & 2) != 0 && variant == 2) return refuse(9644);
    if (robot == 10 && variant == 2 && (c.W(2192) & 1) != 0) return refuse(9644);
    return affordable(data, 408, 3508) ? 1 : refuse(9644);
  }
  if (move == 0) move = table_move(kMoveIds);
  if (c.S16(438) > 1 && move >= 633 && move <= 680) move += 64;
  k.k.r4.s64 = move;
  const uint32_t data = k.Call(__imp__sub_82257ED0, c.a);
  const uint32_t robot = c.B(2);
  if (robot == 10 && (variant == 2 || move == 3208) && (c.W(2192) & 1) != 0) return refuse(9642);
  if (robot == 12 && (c.W(2192) & 2) != 0 && !(r6 == 1 && c.W(588) == 0 && c.W(412) == 0)) {
    if ((c.H(304) & 2) == 0 || c.B(3322) != 0 || c.H(450) != 0) return refuse(9642);
  }
  if (robot == 15 && variant == 2) {
    k.k.r4.u64 = 0;
    const uint32_t r = k.Call(__imp__sub_821FCCE8, c.a);
    return r != 0 ? r : refuse(9642);
  }
  return affordable(data, 412, 3512) ? 1 : refuse(9642);
}

void WeaponReadyHook(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  ctx.r3.u64 = WeaponReady(c, k, base, ctx.r4.u32, ctx.r5.s32, ctx.r6.s32);
}

void VerifyWeaponReady(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_82257F50", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_82257F50(original, base);
        return int64_t(original.r3.u32);
      },
      [&] {
        PPCContext rewrite = ctx;
        WeaponReadyHook(rewrite, base);
        return int64_t(rewrite.r3.u32);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} weapon {} variant {} r6 {} robot {} button {}",
                           control == game_address + kControl2 ? 2 : 1, ctx.r4.u32, ctx.r5.s32, ctx.r6.s32,
                           c.B(2), c.B(576));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821E0308) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E0308(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::WeaponStatus(ctx, base);
}

REX_HOOK_RAW(sub_82257F50) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_82257F50(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyWeaponReady(ctx, base);
  oratan::decomp::WeaponReadyHook(ctx, base);
}
