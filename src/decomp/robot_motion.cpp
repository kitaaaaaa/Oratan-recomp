// Hand-decompiled functions that move a robot.
//
// sub_821F6A10 (called by sub_821F8168, a robot's per-tick driver, outside
// the third rate set; sub_821F75E8 is the other case), on a RobotControl:
//   - While the countdown at +3354 runs, the robot is put back: its position
//     (+24/+28/+32, Robot::position) eases from the point at +3328 toward the
//     one at +3340 over 60 ticks, with ground checks (sub_821FCBF8,
//     sub_8222CB80), and the movement state is reset. Nothing else happens.
//   - Otherwise it builds this tick's speed (+372) from multipliers (the
//     robot's parameters, timers, a table for robot 15 (ajm), the value at
//     kControlScale, which also caps it) and adds a velocity (+380, +388)
//     that depends on the movement state: along the facing (+8), along a
//     movement angle (+16), a fixed amount from the robot's parameters, or,
//     in the default case, from the lever sums: RobotInput::sum_x along the
//     facing and sum_y across it. A counter at +338 runs some states up and
//     drops the speed to 0 after 38 ticks or when the scale value is too low.
//   - Then two original steps: sub_821E85F0 and sub_821F6568(control, 0).
//
// The angle functions are sub_822A7558 (call it A) and sub_822A75A0 (B), from
// a 16-bit angle; velocity along angle a is (-A(a), B(a)) times the speed.
// All callees run as the original code with the original's registers,
// stack slots and frame.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <string>

#include <fmt/format.h>

// Each guest float instruction is its own operation, as in robot_input.cpp.
#pragma clang fp contract(off)

REX_EXTERN(__imp__sub_821F6A10);
REX_EXTERN(__imp__sub_821FCBF8);
REX_EXTERN(__imp__sub_8222CB80);
REX_EXTERN(__imp__sub_822A7560);
REX_EXTERN(__imp__sub_8222A3D8);
REX_EXTERN(__imp__sub_822A7558);
REX_EXTERN(__imp__sub_822A75A0);
REX_EXTERN(__imp__sub_821E85F0);
REX_EXTERN(__imp__sub_821F6568);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;

constexpr uint32_t kGameRobotData = game::kGameRobotData;
constexpr uint32_t kPosX = game::kControlPosition, kPosY = kPosX + 4, kPosZ = kPosX + 8;
constexpr uint32_t kMoveAngle = game::kControlMoveAngle;
constexpr uint32_t kFreeze = game::kControlFreeze;
constexpr uint32_t kRunUp = game::kControlRunUp;
constexpr uint32_t kSpeed = game::kControlSpeed;
constexpr uint32_t kVelA = game::kControlVelocity, kVelB = game::kControlVelocity + 8;
constexpr uint32_t kWarp = game::kControlWarp;
constexpr uint32_t kWarpFrom = game::kControlWarpFrom, kWarpTo = game::kControlWarpTo;
// Not named in game_memory.h yet (meaning unknown).
constexpr uint32_t kTimer444 = 444, kTimer450 = 450, kTable454 = 454;
constexpr uint32_t kFlag577 = 577, kFlag318 = 318;

// One PowerPC float instruction each (see robot_input.cpp).
double Mul(double a, double b) { return a * b; }
double Add(double a, double b) { return a + b; }
double Sub(double a, double b) { return a - b; }
double Single(double a) { return double(float(a)); }
double MulS(double a, double b) { return double(float(a * b)); }                    // fmuls
double SubS(double a, double b) { return double(float(a - b)); }                    // fsubs
double FmaS(double a, double b, double c) { return double(float(std::fma(a, b, c))); }  // fmadds
double NegFmaSubS(double a, double b, double c) {                                   // fnmsubs
  return double(float(-std::fma(a, b, -c)));
}
double FmaSub(double a, double b, double c) { return std::fma(a, b, -c); }          // fmsub

double GuestDouble(const uint8_t* base, uint32_t address) {
  return std::bit_cast<double>(uint64_t(*game::GuestPtr<be<uint64_t>>(base, address)));
}

// A view of the control block by offset (most of these fields aren't in
// game::RobotControl yet).
struct Control {
  uint8_t* base;
  uint32_t address;
  double F(uint32_t o) const {
    return double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, address + o))));
  }
  void SetF(uint32_t o, double v) const {
    *game::GuestPtr<be<uint32_t>>(base, address + o) = std::bit_cast<uint32_t>(float(v));
  }
  uint16_t U16(uint32_t o) const { return *game::GuestPtr<be<uint16_t>>(base, address + o); }
  int32_t S16(uint32_t o) const { return int16_t(U16(o)); }
  void SetU16(uint32_t o, uint32_t v) const {
    *game::GuestPtr<be<uint16_t>>(base, address + o) = uint16_t(v);
  }
  uint8_t U8(uint32_t o) const { return *game::GuestPtr<uint8_t>(base, address + o); }
  void SetU8(uint32_t o, uint32_t v) const { *game::GuestPtr<uint8_t>(base, address + o) = uint8_t(v); }
  void SetU32(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint32_t>>(base, address + o) = v; }
};

// Calls into the original code from this function's frame.
struct Calls {
  PPCContext frame;  // r1 = this function's frame
  uint8_t* base;
  double AngleA(uint32_t angle) {
    PPCContext c = frame;
    c.r3.u64 = angle;
    __imp__sub_822A7558(c, base);
    return c.f1.f64;
  }
  double AngleB(uint32_t angle) {
    PPCContext c = frame;
    c.r3.u64 = angle;
    __imp__sub_822A75A0(c, base);
    return c.f1.f64;
  }
};

double RobotDataFloat(const uint8_t* base, uint32_t game_address, uint32_t offset) {
  const uint32_t data = game::LoadGuestAddress(base, game_address + kGameRobotData);
  return double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, data + offset))));
}

// The ground check: 0x820022B4 is the "no ground" result's limit.
double NoGround(const uint8_t* base) {
  return double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, 0x820022B4))));
}

// The warp countdown (+3354 set).
void Warp(Calls& calls, const Control& c, uint8_t* base) {
  const uint32_t left = (uint32_t(c.U16(kWarp)) + 0xFFFF) & 0xFFFF;
  c.SetU16(kWarp, left);
  const uint32_t frame = calls.frame.r1.u32;
  if (left == 0) {
    // Arrived: snap to the end point, reset the movement state, find the ground.
    const double to_y = c.F(kWarpTo + 4);
    const double to_x = c.F(kWarpTo), to_z = c.F(kWarpTo + 8);
    c.SetU16(272, 0);
    *game::GuestPtr<be<uint32_t>>(base, frame + 84) = std::bit_cast<uint32_t>(float(to_y));
    c.SetU16(288, 0);
    c.SetF(kPosX, to_x);
    c.SetU16(290, 0);
    c.SetF(kPosZ, to_z);
    c.SetU16(274, 0);
    c.SetF(kPosY, to_y);
    c.SetU8(576, 0);
    c.SetU8(577, 0);
    c.SetU16(292, 0);
    c.SetU16(102, 0);
    c.SetU16(294, 0);
    c.SetU16(104, 0);
    c.SetU32(584, 0);
    c.SetU16(594, 0);
    c.SetU16(598, 0);
    c.SetU16(12, 0);
    c.SetU16(4492, 0);
    c.SetU32(112, 0);
    c.SetU32(300, 0);
    {
      PPCContext call = calls.frame;
      call.r3.u64 = c.address + kPosX;
      call.r4.u64 = frame + 84;
      call.r5.u64 = c.address + kPosZ;
      __imp__sub_821FCBF8(call, base);
    }
    PPCContext call = calls.frame;
    call.r7.u64 = frame + 80;
    call.r6.u64 = frame + 88;
    call.f2.f64 = c.F(kPosZ);
    call.r3.u64 = c.address;
    call.f1.f64 = c.F(kPosX);
    __imp__sub_8222CB80(call, base);
    const double ground = call.f1.f64;
    if (!(ground < NoGround(base))) {
      c.SetF(kPosX, c.F(kWarpFrom));
      c.SetF(kPosY, c.F(kWarpFrom + 4));
      c.SetF(kPosZ, c.F(kWarpFrom + 8));
    } else if (c.F(kPosY) < ground) {
      c.SetF(kPosY, ground);
    }
    return;
  }

  // Still going: keep the end point on the ground, then ease toward it.
  {
    PPCContext call = calls.frame;
    call.r3.u64 = c.address + kWarpTo;
    call.r4.u64 = c.address + kWarpTo + 4;
    call.r5.u64 = c.address + kWarpTo + 8;
    __imp__sub_821FCBF8(call, base);
    const double ground = call.f1.f64;
    if (!(ground < NoGround(base))) {
      c.SetU16(kWarp, 0);
    } else if (c.F(kWarpTo + 4) < ground) {
      c.SetF(kWarpTo + 4, ground);
    }
  }
  const uint32_t now = c.U16(kWarp);
  const double from_x = c.F(kWarpFrom), from_y = c.F(kWarpFrom + 4), from_z = c.F(kWarpFrom + 8);
  const double ticks = Single(double(int64_t(int32_t(60 - now))));
  if (now & 1) {
    const double t = Single(Mul(ticks, GuestDouble(base, 0x82016308)));
    const double dx = SubS(c.F(kWarpTo), from_x), dy = SubS(c.F(kWarpTo + 4), from_y);
    const double dz = SubS(c.F(kWarpTo + 8), from_z);
    c.SetF(kPosX, FmaS(dx, t, from_x));
    c.SetF(kPosY, FmaS(dy, t, from_y));
    c.SetF(kPosZ, FmaS(dz, t, from_z));
  } else {
    PPCContext call = calls.frame;
    call.f1.f64 = Single(FmaSub(ticks, GuestDouble(base, 0x82028AA8), GuestDouble(base, 0x82028AA0)));
    __imp__sub_822A7560(call, base);
    const double wave = call.f1.f64;
    const double dx = SubS(c.F(kWarpTo), from_x), dy = SubS(c.F(kWarpTo + 4), from_y);
    const double eased = Add(wave, GuestDouble(base, 0x82001388));
    const double dz = SubS(c.F(kWarpTo + 8), from_z);
    const double t = Single(Mul(eased, GuestDouble(base, 0x82001590)));
    c.SetF(kPosX, FmaS(dx, t, from_x));
    c.SetF(kPosY, FmaS(dy, t, from_y));
    c.SetF(kPosZ, FmaS(dz, t, from_z));
  }
  c.SetU8(576, 0);
  c.SetU16(102, 0);
  c.SetU32(584, 2);
  c.SetU16(272, 0);
  c.SetU16(274, 0);
}

// The multipliers several states share.
void SpeedModifiers(const Control& c, uint8_t* base, uint32_t game_address, bool low) {
  if (c.U8(2) == 15) {
    const double factor = double(std::bit_cast<float>(uint32_t(
        *game::GuestPtr<be<uint32_t>>(base, 0x82027914 + uint32_t(c.U16(kTable454)) * 4))));
    c.SetF(kSpeed, MulS(factor, c.F(kSpeed)));
  }
  const int32_t timer450 = c.S16(kTimer450);
  if (timer450 > 0) {
    c.SetF(kSpeed, MulS(RobotDataFloat(base, game_address, 1448), c.F(kSpeed)));
  } else if (timer450 < 0) {
    c.SetF(kSpeed, Single(Mul(c.F(kSpeed), GuestDouble(base, 0x82026080))));
  }
  if (c.S16(kTimer444) > 0) {
    c.SetF(kSpeed, MulS(RobotDataFloat(base, game_address, 1452), c.F(kSpeed)));
  }
  if (low) c.SetF(kSpeed, MulS(RobotDataFloat(base, game_address, 1460), c.F(kSpeed)));
}

// velocity += amount * (-A(angle), B(angle))
void PushAlong(Calls& calls, const Control& c, uint32_t angle, double amount_a, double amount_b) {
  const double a = calls.AngleA(angle);
  c.SetF(kVelA, NegFmaSubS(a, amount_a, c.F(kVelA)));
  const double b = calls.AngleB(angle);
  c.SetF(kVelB, FmaS(b, amount_b, c.F(kVelB)));
}
void PushAlongAtSpeed(Calls& calls, const Control& c, uint32_t angle) {
  const double a = calls.AngleA(angle);
  c.SetF(kVelA, NegFmaSubS(a, c.F(kSpeed), c.F(kVelA)));
  const double b = calls.AngleB(angle);
  c.SetF(kVelB, FmaS(b, c.F(kSpeed), c.F(kVelB)));
}

// sub_821F6A10.
void MoveRobot(PPCContext& ctx, uint8_t* base) {
  ctx.fpscr.disableFlushMode();
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  const Control c{base, ctx.r3.u32};
  Calls calls{ctx, base};
  calls.frame.r1.u64 = ctx.r1.u32 - 160;

  if (c.U16(kWarp) != 0) {
    Warp(calls, c, base);
    return;
  }

  // Close to the ground on some stages (sub_8222A3D8, on stages 0 and 9).
  const int32_t stage = *game::GuestPtr<be<int32_t>>(base, game::kNextStage);
  bool low = false;
  if (stage == 0 || stage == 9) {
    PPCContext call = calls.frame;
    call.f1.f64 = c.F(kPosY);
    __imp__sub_8222A3D8(call, base);
    low = call.r3.u32 != 0;
  }
  const double scale = c.F(game::kControlScale);
  if (scale < double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, 0x8201E7F4))))) {
    c.SetF(kSpeed, Single(Mul(c.F(kSpeed), GuestDouble(base, 0x82028298))));
  }
  const double zero = double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, 0x8200095C))));
  const auto data = [&](uint32_t offset) { return RobotDataFloat(base, game_address, offset); };

  const uint32_t state = c.U16(272);
  if (c.U16(kFreeze) != 0 || state == 30 || state == 31) {
    c.SetF(kVelA, zero);
    c.SetF(kVelB, zero);
  } else if (c.U16(kRunUp) != 0) {
    const uint32_t run_up = (uint32_t(c.U16(kRunUp)) + 1) & 0xFFFF;
    const uint8_t robot = c.U8(2);
    c.SetU16(kRunUp, run_up);
    SpeedModifiers(c, base, game_address, low);
    const uint32_t facing = c.U16(8);
    const int32_t relative = int16_t(uint16_t(c.U16(game::kControlRef2) - facing));
    if (scale > double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, 0x82001518))))) {
      if (relative > 4096 || relative < -4096) {
        c.SetF(kSpeed, Single(Mul(c.F(kSpeed), GuestDouble(base, 0x82001590))));
      }
    }
    // Whether the run-up keeps its speed.
    bool keep = true;
    if (run_up > 38) {
      keep = false;
    } else if (state == 19 || state == 11) {
      keep = scale > data(1476) && scale > data(1480) && scale > data(1484);
    } else if (state == 9) {
      keep = scale > data(1476) || scale > data(1480) || scale > data(1484);
    } else if (state == 0) {
      const uint32_t sub = c.U16(274);
      if (!(robot == 0 && sub == 15 && c.U16(278) == 1)) {
        if ((sub == 11 || sub == 12) && !(scale > data(1476))) {
          keep = false;
        } else if ((sub == 13 || sub == 14) && !(scale > data(1480))) {
          keep = false;
        } else if (sub == 15 || sub == 16) {
          keep = scale > data(1484);
        }
      }
    }
    if (!keep) {
      c.SetU16(kRunUp, 0);
      c.SetF(kSpeed, zero);
    }
    if (c.F(kSpeed) > scale) c.SetF(kSpeed, scale);
    PushAlongAtSpeed(calls, c, facing);
  } else {
    const uint32_t sub = c.U16(274);
    const uint8_t robot = c.U8(2);
    const uint32_t move_angle = c.U16(kMoveAngle);
    if (state == 0 && sub >= 11 && sub <= 16) {
      // no velocity
    } else if (state == 13 && sub != 0) {
      SpeedModifiers(c, base, game_address, low);
      if (c.U8(kFlag577) == 0) {
        c.SetF(kSpeed, Single(Mul(c.F(kSpeed), GuestDouble(base, 0x82001590))));
      }
      PushAlongAtSpeed(calls, c, c.U16(8));
    } else if (state == 3) {
      PushAlongAtSpeed(calls, c, move_angle);
    } else if (state == 2) {
      SpeedModifiers(c, base, game_address, low);
      PushAlongAtSpeed(calls, c, (uint32_t(c.U16(8)) + c.U16(kMoveAngle)) & 0xFFFF);
    } else if (state == 15 || (state == 19 && (sub != 0 || c.U8(kFlag577) < 2))) {
      SpeedModifiers(c, base, game_address, low);
      if (c.U16(kFlag318) != 0) {
        c.SetF(kSpeed, Single(Mul(c.F(kSpeed), GuestDouble(base, 0x82017310))));
      }
      PushAlongAtSpeed(calls, c, c.U16(kMoveAngle));
    } else if (state == 10) {
      if (c.U8(kFlag577) == 1) PushAlongAtSpeed(calls, c, move_angle);
    } else if (state == 5 || state == 6 || state == 7) {
      PushAlongAtSpeed(calls, c, move_angle);
    } else if (sub == 18) {
      const double amount = data(3884);
      const double a = calls.AngleA(move_angle);
      c.SetF(kVelA, NegFmaSubS(a, amount, c.F(kVelA)));
      const double b = calls.AngleB(move_angle);
      c.SetF(kVelB, FmaS(b, data(3884), c.F(kVelB)));
    } else if (state >= 24 && state <= 28) {
      // no velocity
    } else {
      // The levers: sum_x along the facing, sum_y across it.
      if (robot == 17 && c.S16(80) <= c.S16(88) / 2) {
        c.SetF(kSpeed, Single(Mul(c.F(kSpeed), GuestDouble(base, 0x82017330))));
      }
      const double low_scale_end =
          double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, 0x82017830))));
      if (scale < low_scale_end) {
        const double ramp = Add(scale, GuestDouble(base, 0x82028290));
        c.SetF(kSpeed, Single(Mul(Mul(ramp, c.F(kSpeed)), GuestDouble(base, 0x82028288))));
      }
      SpeedModifiers(c, base, game_address, low);
      const uint32_t facing = c.U16(8);
      const uint32_t sum_y_offset = 0xA0 + uint32_t(offsetof(game::RobotInput, sum_y));
      const double sum_y = c.F(sum_y_offset);
      const uint32_t sum_x_offset = 0xA0 + uint32_t(offsetof(game::RobotInput, sum_x));
      const double a1 = calls.AngleA(facing);
      const double along_a = MulS(a1, c.F(sum_x_offset));
      const double b1 = calls.AngleB(facing);
      const double push_a = FmaS(b1, sum_y, along_a);
      c.SetF(kVelA, NegFmaSubS(push_a, c.F(kSpeed), c.F(kVelA)));
      const double b2 = calls.AngleB(facing);
      const double along_b = MulS(b2, c.F(sum_x_offset));
      const double a2 = calls.AngleA(facing);
      const double push_b = NegFmaSubS(a2, c.F(sum_y_offset), along_b);
      c.SetF(kVelB, FmaS(push_b, c.F(kSpeed), c.F(kVelB)));
    }
  }

  // State 3 also gets a fixed push along the movement angle.
  if (c.U16(kFreeze) == 0 && c.U16(272) == 3) {
    const uint32_t angle = c.U16(kMoveAngle);
    const double a = calls.AngleA(angle);
    c.SetF(kVelA, NegFmaSubS(a, data(3880), c.F(kVelA)));
    const double b = calls.AngleB(angle);
    c.SetF(kVelB, FmaS(b, data(3880), c.F(kVelB)));
  }
  {
    PPCContext call = calls.frame;
    call.r3.u64 = c.address;
    __imp__sub_821E85F0(call, base);
  }
  PPCContext call = calls.frame;
  call.r4.u64 = 0;
  call.r3.u64 = c.address;
  __imp__sub_821F6568(call, base);
}

// Compares the rewrite with the original on this call (decomp_verify_calls).
void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    const uint32_t generators = game::LoadGuestAddress(base, game::kRandomGenerators);
    const uint32_t random_state = game::LoadGuestAddress(base, generators + 16 * 1 + 4);
    verifier = new Verifier("sub_821F6A10", base,
                            {{game_address, 0xD000, "game"},
                             {game::kRandomCalls, 4, "random calls"},
                             {random_state, game::kRandomStateSize, "random state"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821F6A10(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        MoveRobot(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} state {} sub {}", control == game_address + 29516 ? 2 : 1,
                           c.U16(272), c.U16(274));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821F6A10) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821F6A10(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::MoveRobot(ctx, base);
}
