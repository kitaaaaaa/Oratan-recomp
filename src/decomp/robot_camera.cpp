// Hand-decompiled camera step of a robot.
//
// sub_821FF668, called each tick by the driver (sub_821F8168) unless the
// robot's control mode (+1) is 3. Each side has a camera record at
// Game + 192 * side, which starts with the 4x4 matrix Game::camera; its
// angles +86 (yaw) and +84 follow the robot's facing (+8) and +6. The camera
// mode at control +336 (set by the movement states; states 6 and 7 force 2)
// picks what happens this tick:
//    0     build the matrix (sub_821FD6B0)
//    1     yaw = facing, then mode 0
//    2     turn the yaw toward the target angle (+4490), limited per tick
//    3     turn the facing toward the target angle, the yaw after it; mode 0
//          once the yaw has caught up
//    7, 17 turn facing and yaw together toward the target angle (7: while
//          Game +24932 is set, aim with half the target angles instead)
//    8, 9  copy the facing and +6 into the camera (9 then returns to mode 0)
//    10    go to mode 11
//    11    yaw = facing (after measuring the angle to the +4476 point)
//    14    go to mode 15, or 16 while Game +24932 is set
//    15, 16, 19  other camera steps (sub_821FD860, sub_821FDB18, sub_821FF528)
//    18    turn the facing toward the yaw by at most 2560, then mode 0
//    22, 23 aim with half the target angles (pitch kept in one half)
//    others do nothing.
// The turn limits are in the robot's data (+480, +484, +488). Aiming calls
// sub_821BFF48(control, pitch, yaw, 0). All callees run as the original.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <bit>
#include <cstdint>
#include <string>

#include <fmt/format.h>

#pragma clang fp contract(off)

REX_EXTERN(__imp__sub_821FF668);
REX_EXTERN(__imp__sub_822AC098);
REX_EXTERN(__imp__sub_821FD6B0);
REX_EXTERN(__imp__sub_821FD860);
REX_EXTERN(__imp__sub_821FDB18);
REX_EXTERN(__imp__sub_821FF528);
REX_EXTERN(__imp__sub_821BFF48);
REX_EXTERN(__imp__sub_822A73D8);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;

constexpr uint32_t kGameBusy = 24932;
constexpr uint32_t kControl2 = 29516;
constexpr uint32_t kCameraYaw = 86, kCameraPitch = 84;
constexpr uint32_t kFacing = 8, kFacingPitch = 6, kTargetAngle = game::kControlRef2;

struct Control {
  uint8_t* base;
  uint32_t a;
  uint8_t B(uint32_t o) const { return *game::GuestPtr<uint8_t>(base, a + o); }
  void SetB(uint32_t o, uint32_t v) const { *game::GuestPtr<uint8_t>(base, a + o) = uint8_t(v); }
  uint16_t H(uint32_t o) const { return *game::GuestPtr<be<uint16_t>>(base, a + o); }
  void SetH(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint16_t>>(base, a + o) = uint16_t(v); }
  int32_t S16(uint32_t o) const { return int16_t(H(o)); }
  uint32_t W(uint32_t o) const { return *game::GuestPtr<be<uint32_t>>(base, a + o); }
  double F(uint32_t o) const { return double(std::bit_cast<float>(W(o))); }
};

// Limits v to -limit..limit, as the original's compares and extsh do.
int32_t Clamp(int32_t v, int32_t limit) {
  if (v > limit) return int16_t(limit);
  if (v < -limit) return int16_t(-limit);
  return v;
}

// Half of an angle (rounded toward 0), limited to -limit..limit.
int32_t HalfClamped(int32_t v, int32_t limit) {
  if (v > limit) return int16_t(limit);
  if (v < -limit) return int16_t(-limit);
  return int16_t(v / 2);
}

void Camera(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  PPCContext k = ctx;
  k.r1.u64 = ctx.r1.u32 - 112;
  k.r3.u64 = 0;
  __imp__sub_822AC098(k, base);
  const uint32_t game_address = k.r3.u32;
  const Control g{base, game_address};
  const Control camera{base, c.a == game_address + 25020 ? game_address : game_address + 192};
  const Control data{base, g.W(game::kGameRobotData)};
  const bool busy = int32_t(g.W(kGameBusy)) != 0;
  const auto call = [&](void (*fn)(PPCContext&, uint8_t*), uint32_t r3, uint32_t r4) {
    k.r3.u64 = r3;
    k.r4.u64 = r4;
    fn(k, base);
  };
  const auto build = [&]() { call(__imp__sub_821FD6B0, camera.a, c.a); };
  const auto aim = [&](int32_t pitch, int32_t yaw) {
    k.r3.u64 = c.a;
    k.r4.s64 = pitch;
    k.r5.s64 = yaw;
    k.r6.u64 = 0;
    __imp__sub_821BFF48(k, base);
    build();
  };
  // Facing and yaw step together toward the target angle (modes 7 and 17).
  const auto turn_both = [&]() {
    const int32_t step = Clamp(int16_t(uint16_t(c.H(kTargetAngle) - c.H(kFacing))), int32_t(data.W(480)));
    const uint32_t facing = uint32_t(int16_t(uint16_t(c.H(kFacing) + step)));
    c.SetH(kFacing, facing);
    camera.SetH(kCameraYaw, facing);
  };

  if (c.H(272) == 6 || c.H(272) == 7) c.SetB(336, 2);
  switch (c.B(336)) {
    case 0:
      build();
      break;
    case 1:
      camera.SetH(kCameraYaw, c.H(kFacing));
      c.SetB(336, 0);
      build();
      break;
    case 2: {
      const int32_t yaw = camera.S16(kCameraYaw);
      const int32_t step = Clamp(int16_t(uint16_t(c.H(kTargetAngle) - uint32_t(yaw))), int32_t(data.W(484)));
      camera.SetH(kCameraYaw, uint32_t(step + yaw));
      break;
    }
    case 3: {
      const int32_t facing = c.S16(kFacing);
      const int32_t limit = int32_t(data.W(488));
      const int32_t step = int16_t(uint16_t(c.H(kTargetAngle) - uint32_t(facing)));
      if (step > limit) {
        c.SetH(kFacing, c.H(kFacing) + uint32_t(limit));
      } else if (step < -limit) {
        c.SetH(kFacing, c.H(kFacing) - uint32_t(limit));
      } else {
        const int32_t new_facing = int16_t(uint16_t(facing + step));
        c.SetH(kFacing, uint32_t(new_facing));
        const int32_t yaw_limit = int32_t(Control{base, g.W(game::kGameRobotData)}.W(484));
        const int32_t yaw_step = int16_t(uint16_t(uint32_t(new_facing) - camera.H(kCameraYaw)));
        if (yaw_step <= yaw_limit && yaw_step >= -yaw_limit) c.SetB(336, 0);  // caught up
        camera.SetH(kCameraYaw, camera.H(kCameraYaw) + uint32_t(Clamp(yaw_step, yaw_limit)));
      }
      break;
    }
    case 7:
      if (!busy) {
        turn_both();
      } else {
        const int32_t limit = int32_t(data.W(480));
        aim(HalfClamped(c.S16(4488), limit), HalfClamped(c.S16(kTargetAngle), limit));
      }
      break;
    case 8:
      camera.SetH(kCameraYaw, c.H(kFacing));
      camera.SetH(kCameraPitch, c.H(kFacingPitch));
      break;
    case 9:
      camera.SetH(kCameraYaw, c.H(kFacing));
      if (busy) camera.SetH(kCameraPitch, c.H(kFacingPitch));
      c.SetB(336, 0);
      break;
    case 10:
      c.SetB(336, 11);
      break;
    case 11:
      k.fpscr.disableFlushMode();
      k.f2.f64 = double(float(c.F(4484) - c.F(32)));
      k.f1.f64 = double(float(c.F(24) - c.F(4476)));
      __imp__sub_822A73D8(k, base);  // result unused, as in the original
      camera.SetH(kCameraYaw, c.H(kFacing));
      break;
    case 14:
      c.SetB(336, busy ? 16 : 15);
      break;
    case 15:
      call(__imp__sub_821FD860, camera.a, c.a);
      break;
    case 16:
      k.r3.u64 = c.a;
      __imp__sub_821FDB18(k, base);
      break;
    case 17:
      turn_both();
      break;
    case 18: {
      const int32_t step = int16_t(uint16_t(camera.H(kCameraYaw) - c.H(kFacing)));
      if (step > 2560) {
        c.SetH(kFacing, c.H(kFacing) + 2560);
      } else if (step < -2560) {
        c.SetH(kFacing, c.H(kFacing) - 2560);
      } else {
        c.SetB(336, 0);
        c.SetH(kFacing, c.H(kFacing) + uint32_t(step));
      }
      break;
    }
    case 19:
      call(__imp__sub_821FF528, camera.a, c.a);
      break;
    case 22:
    case 23: {
      const int32_t yaw = HalfClamped(c.S16(kTargetAngle), 1536);
      const int32_t pitch = c.S16(4488);
      const bool in_half = c.B(336) == 22 ? (pitch >= 0 && pitch <= 16384) : (pitch < 0 && pitch >= -16384);
      aim(in_half ? HalfClamped(pitch, 1536) : 0, yaw);
      break;
    }
    default:
      break;
  }
}

void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821FF668", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821FF668(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        Camera(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} camera mode {} state {}", control == game_address + kControl2 ? 2 : 1,
                           c.B(336), c.H(272));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821FF668) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821FF668(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::Camera(ctx, base);
}
