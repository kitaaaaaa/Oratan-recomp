// Hand-decompiled effect step of a robot.
//
// sub_821B3AF8(control), called each tick by the driver (sub_821F8168).
// It starts effects with sub_821B2CB0(kind, x, y, z, record, flags) and
// sub_82286120(id, r4, x, y, z, ...) at the robot's position (+24 / +28 /
// +32), its feet (+872 / +880, +896 / +904) or other points (+656..+664,
// +36..+44, +1328.., +1352..), by stage kind (0x83876DF4), movement state
// and button state (+576 / +577). Each side has its own records (the
// effects' angles at +2) and two foot counters; flags carry the side (0 / 1,
// 256 for the third control at Game +34012). In order:
//   - the ground height under the robot (sub_82229F40; above it, a value
//     from the stage's other height plus 0x838770A0), and whether the robot
//     is under the stage's level (sub_8222A3D8, stage kinds 0 / 9 / 5);
//   - a downed robot (state 30 or no health) smokes (id 3352) once
//     0x83876F98 is past 130;
//   - +576 == 127: effects around the other robot's kind every 3rd and 5th
//     tick with random offsets, and a sound every 60 ticks;
//   - otherwise, at most one of: the state 12 effects (+577 1 / 3), the dash
//     effects every 6 ticks of +102 with a sound and sub_8214F638, the
//     landing effects, robots 8 / 9's effects in some states, stage kind 5's
//     effects, or footsteps by stage kind.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <string>

#include <fmt/format.h>

// Each guest float instruction is its own operation, as in robot_input.cpp.
#pragma clang fp contract(off)

REX_EXTERN(__imp__sub_821B3AF8);
REX_EXTERN(__imp__sub_822AC098);
REX_EXTERN(__imp__sub_82229F40);
REX_EXTERN(__imp__sub_8222A3D8);
REX_EXTERN(__imp__sub_82286120);
REX_EXTERN(__imp__sub_822A74B0);
REX_EXTERN(__imp__sub_821B2CB0);
REX_EXTERN(__imp__sub_821A5C20);
REX_EXTERN(__imp__sub_822AAAC0);
REX_EXTERN(__imp__sub_8214F638);
REX_EXTERN(__imp__sub_821B3140);
REX_EXTERN(__imp__sub_822A6B70);
REX_EXTERN(__imp__sub_822B5A18);
REX_EXTERN(__imp__sub_822B5908);
REX_EXTERN(__imp__sub_822A74F8);
REX_EXTERN(__imp__sub_822A74E8);
REX_EXTERN(__imp__sub_822A7508);
REX_EXTERN(__imp__sub_822B61B0);
REX_EXTERN(__imp__sub_822B5A78);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;

constexpr uint32_t kGameBusy = 24932;
constexpr uint32_t kControl1 = 25020, kControl2 = 29516, kControl3 = 34012;
constexpr uint32_t kStageKind = 0x83876DF4;
constexpr uint32_t kDownSmokeTimer = 0x83876F98;
constexpr uint32_t kZeroFloat = 0x8200095C;
// Effect records (+2 angle, +4 random), one per use.
constexpr uint32_t kRecordState12a = 0x8385477C, kRecordState12b = 0x83854774;
constexpr uint32_t kRecordDash = 0x8385476C, kRecordLanding = 0x83854764;
constexpr uint32_t kRecordKind5Side2 = 0x8385475C, kRecordKind5Side1 = 0x83854754;
constexpr uint32_t kRecordUnderDash = 0x8385474C, kRecordUnderStep = 0x83854744;
constexpr uint32_t kRecordStep = 0x8385473C, kRecordStepGround = 0x83854734;
constexpr uint32_t kRecordUnder89 = 0x8385472C;
constexpr uint32_t kRecord127a = 0x8385478C, kRecord127b = 0x83854784;

struct Control {
  uint8_t* base;
  uint32_t a;
  uint8_t B(uint32_t o) const { return *game::GuestPtr<uint8_t>(base, a + o); }
  uint16_t H(uint32_t o) const { return *game::GuestPtr<be<uint16_t>>(base, a + o); }
  void SetH(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint16_t>>(base, a + o) = uint16_t(v); }
  int32_t S16(uint32_t o) const { return int16_t(H(o)); }
  uint32_t W(uint32_t o) const { return *game::GuestPtr<be<uint32_t>>(base, a + o); }
  float F(uint32_t o) const { return std::bit_cast<float>(W(o)); }
};

double GuestDouble(const uint8_t* base, uint32_t address) {
  return std::bit_cast<double>(uint64_t(*game::GuestPtr<be<uint64_t>>(base, address)));
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

bool In(uint32_t v, std::initializer_list<uint32_t> set) {
  for (uint32_t x : set) {
    if (v == x) return true;
  }
  return false;
}

void EffectStep(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  const uint32_t frame = ctx.r1.u32 - 272;
  k.k.r1.u64 = frame;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  const Control mem{base, 0};
  const bool side1 = c.a == game_address + kControl1;
  // Per side: two foot counters, a landing flag and the angle record.
  const uint32_t foot1 = side1 ? 0x83854408 : 0x83854388;
  const uint32_t foot2 = side1 ? 0x83854710 : 0x838543F0;
  const uint32_t landing = side1 ? 0x838543F4 : 0x8385438C;
  const uint32_t angles = side1 ? 0x83854400 : 0x838543F8;
  mem.SetH(angles, c.H(6));
  const uint32_t flags = c.a == game_address + kControl3 ? 256 : (side1 ? 0 : 1);
  const float zero = mem.F(kZeroFloat);

  const auto effect = [&](uint32_t kind, float x, float y, float z, uint32_t record, uint32_t effect_flags) {
    k.k.f1.f64 = double(x);
    k.k.f2.f64 = double(y);
    k.k.f3.f64 = double(z);
    k.k.r7.u64 = record;
    k.k.r8.u64 = effect_flags;
    k.Call(__imp__sub_821B2CB0, kind);
  };
  // Both feet when their counter hits `at` (modulo `every`).
  const auto feet = [&](uint32_t kind, float y, uint32_t record, int32_t every, int32_t at) {
    if (mem.H(foot1) % every == at) effect(kind, c.F(872), y, c.F(880), record, flags);
    if (mem.H(foot2) % every == at) effect(kind, c.F(896), y, c.F(904), record, flags);
  };
  const auto set_angle = [&](uint32_t record, uint32_t v) { mem.SetH(record + 2, v); };

  // Ground height under the robot.
  k.k.r5.u64 = frame + 80;
  k.k.f2.f64 = double(c.F(32));
  k.k.f1.f64 = double(c.F(24));
  k.Call(__imp__sub_82229F40);
  float ground = float(k.k.f1.f64);
  const float other_height = mem.F(frame + 80);
  k.k.f1.f64 = double(c.F(28));
  if (!(c.F(28) < other_height)) ground = mem.F(0x838770A0) + other_height;
  const uint32_t under = k.Call(__imp__sub_8222A3D8);

  if ((c.H(272) == 30 || c.H(80) == 0) && mem.W(kDownSmokeTimer) > 130 && c.B(2) != 18) {
    k.k.f3.f64 = double(c.F(44));
    k.k.r8.u64 = c.a + 6;
    k.k.f2.f64 = double(c.F(40));
    k.k.f1.f64 = double(c.F(36));
    k.k.r4.u64 = 19;
    k.k.r9.u64 = side1 ? 0 : 1;
    k.Call(__imp__sub_82286120, 3352);
  }

  const uint32_t b576 = c.B(576);
  const uint32_t robot = c.B(2);
  if (b576 == 127) {
    // Effects by the other robot's kind.
    const Control other{base, c.W(72)};
    uint32_t kind = 16;
    switch (other.B(2)) {
      case 0: kind = 30; break;
      case 1:
      case 5:
      case 16: kind = 19; break;
      case 13: {
        const bool bit = (mem.W(0x83862000) & 1) != 0;
        const bool first = other.a == game_address + kControl1;
        const bool second = other.a == game_address + kControl2;
        kind = (bit && first) || (!bit && second) ? 16 : 19;
        break;
      }
      default: break;
    }
    if (g.W(588) % 3 == 0) {
      const double d7720 = GuestDouble(base, 0x82017720), d7340 = GuestDouble(base, 0x82017340);
      const auto random200 = [&] { return float(double(int32_t(k.Call(__imp__sub_822A74B0)) % 200)); };
      const float dx = float((double(random200()) - d7720) * d7340);
      const float dy = float(double(random200()) * d7340);
      const float dz = float((double(random200()) - d7720) * GuestDouble(base, 0x82017B38));
      const uint32_t r = k.Call(__imp__sub_822A74B0);
      mem.SetH(kRecord127a + 4, r << 1);
      k.k.r6.u64 = kRecord127a;
      effect(kind, c.F(24) + dx, c.F(28) + dy, dz + c.F(32), kRecord127a, 128);
      if ((g.W(588) + 30) % 60 == 0) {
        k.k.r5.u64 = 0x82017B24;
        k.k.r4.u64 = 0x2604A9;
        k.Call(__imp__sub_821A5C20, frame + 128);
        k.Call(__imp__sub_822AAAC0);
      }
    }
    if (g.W(588) % 5 == 0) {
      mem.SetH(kRecord127b, k.Call(__imp__sub_822A74B0) << 2);
      mem.SetH(kRecord127b + 2, k.Call(__imp__sub_822A74B0) << 2);
      effect(kind, c.F(36), c.F(40), c.F(44), kRecord127b, 128);
    }
    return;
  }

  const bool busy = g.W(kGameBusy) != 0;
  const uint32_t stage = mem.W(kStageKind);
  const bool water_stage = stage == 0 || stage == 9;
  // State 12's effects.
  uint32_t state12_record = 0;
  if (!busy && c.H(272) == 12 && c.B(577) == 1 && c.H(102) == 0 &&
      c.F(28) - ground < mem.F(0x82010844)) {
    state12_record = kRecordState12a;
  } else if (!busy && c.H(272) == 12 && c.B(577) == 3 && c.H(102) == 0) {
    state12_record = kRecordState12b;
  }
  if (state12_record) {
    set_angle(state12_record, c.H(8));
    if (water_stage && under == 1) {
      effect(7, c.F(24), zero, c.F(32), state12_record, flags);
    } else {
      effect(1, c.F(24), c.F(28), c.F(32), state12_record, flags);
    }
    return;
  }

  // Dashing: every 6 ticks of +102.
  const uint32_t b577 = c.B(577);
  const bool dashing = (In(b576, {28, 29, 30}) && b577 == 1) ||
                       (b576 == 15 && ((b577 == 1 && c.S16(102) >= 30) || b577 == 2)) || b576 == 19 ||
                       (b576 == 31 && b577 == 1);
  if (dashing && c.S16(102) % 6 == 0 && !busy) {
    set_angle(kRecordDash, c.H(8));
    if (water_stage && under == 1) {
      effect(7, c.F(24), zero, c.F(32), kRecordDash, flags);
    } else {
      effect(2, c.F(24), c.F(28), c.F(32), kRecordDash, flags);
    }
    k.k.r4.u64 = (c.B(2) == 8 || c.B(2) == 9 ? 23u : 22u) << 16 | 1449;
    k.k.r5.u64 = 0x82017B18;
    k.Call(__imp__sub_821A5C20, frame + 128);
    k.Call(__imp__sub_822AAAC0);
    if (c.B(1) == 0 && g.H(578) != 19) {
      k.k.f1.f64 = double(mem.F(0x82001B2C));
      k.k.r6.u64 = 2;
      k.k.r5.u64 = 2;
      k.Call(__imp__sub_8214F638, side1 ? 0 : 1);
    }
    return;
  }

  // Landing.
  if (mem.H(landing) == 1 && robot != 17 && !busy) {
    set_angle(kRecordLanding, mem.H(angles + 2));
    if (water_stage && under == 1) {
      effect(6, c.F(24), zero, c.F(32), kRecordLanding, flags | 2);
    } else if (g.B(24916) == 0) {
      if (stage == 10 || robot == 9 || robot == 16) {
        effect(4, c.F(24), ground, c.F(32), kRecordLanding, flags | 2);
      } else {
        effect(4, c.F(24), ground, c.F(32), kRecordLanding, flags | 8);
        effect(4, c.F(24), ground, c.F(32), kRecordLanding, flags | 4);
      }
    }
    return;
  }

  // Walking or running (buttons 1-7 and some robots' own).
  const auto running = [&](uint32_t b) {
    return (b >= 1 && b <= 7) || (robot == 10 && b == 160) || (robot == 7 && In(b, {60, 61}) && c.B(577) == 1) ||
           (robot == 0 && b == 163 && c.B(577) == 1);
  };

  const uint32_t state = c.H(272);
  if ((robot == 8 || robot == 9) && (In(state, {19, 15, 12}) || (state == 13 && c.H(274) != 0))) {
    const uint32_t t = g.W(588);
    if (t % 6 == 0) {
      const auto launch = [&](uint32_t id, float x, float y, float z) {
        k.k.f1.f64 = double(x);
        k.k.f2.f64 = double(y);
        k.k.f3.f64 = double(z);
        k.k.r9.u64 = c.a == game_address + kControl2 ? 1 : 0;
        k.k.r8.u64 = angles;
        k.k.r4.u64 = 5;
        k.Call(__imp__sub_82286120, id);
      };
      const uint32_t sub = c.H(274);
      if (robot == 9) {
        if (!(sub >= 2 && sub <= 10) && !(sub >= 11 && sub <= 16)) {
          // A random point around +656, turned by the robot's angles.
          k.Call(__imp__sub_822B5A18);
          k.Call(__imp__sub_822B5908);
          k.k.r3.s64 = c.S16(8);
          k.Call(__imp__sub_822A74F8);
          k.k.r3.s64 = c.S16(6);
          k.Call(__imp__sub_822A74E8);
          k.k.r3.s64 = c.S16(10);
          k.Call(__imp__sub_822A7508);
          const double d6318 = GuestDouble(base, 0x82016318), d7b10 = GuestDouble(base, 0x82017B10);
          const auto random = [&] {
            const int32_t r = int32_t(k.Call(__imp__sub_822A74B0)) % 100;
            return float(double(float(double(r) * d6318)) - d7b10);
          };
          const float rx = random();
          *game::GuestPtr<be<uint32_t>>(base, frame + 96) = std::bit_cast<uint32_t>(rx);
          const float ry = random();
          *game::GuestPtr<be<uint32_t>>(base, frame + 104) = std::bit_cast<uint32_t>(zero);
          *game::GuestPtr<be<uint32_t>>(base, frame + 100) = std::bit_cast<uint32_t>(ry);
          k.k.r4.u64 = frame + 96;
          k.Call(__imp__sub_822B61B0, frame + 112);
          k.Call(__imp__sub_822B5A78);
          launch(3307, c.F(656) + mem.F(frame + 112), c.F(660) + mem.F(frame + 116), c.F(664) + mem.F(frame + 120));
        }
      } else if (robot == 8) {
        const bool skip = int8_t(k.Call(__imp__sub_822A6B70)) != 0 && c.H(274) >= 2 && c.H(274) <= 10;
        if (!skip) {
          const uint32_t id = 3290 + (c.H(450) != 0 ? 1 : 0);
          if (t % 12 == 0) {
            launch(id, c.F(1352), c.F(1356), c.F(1360));
          } else {
            launch(id, c.F(1328), c.F(1332), c.F(1336));
          }
        }
      }
    }
    if (water_stage && under == 1) {
      set_angle(kRecordUnder89, mem.H(angles + 2));
      if (k.Call(__imp__sub_821B3140, c.a) != 0 && mem.H(foot1) % 8 == 1) {
        effect(6, c.F(656), zero, c.F(664), kRecordUnder89, flags);
      }
    }
    return;
  }

  if (stage == 5) {
    // Every 60 ticks an effect at one of five points (+656 + 24 * n).
    const bool second = c.a == game_address + kControl2;
    const uint32_t t = second ? g.W(588) : g.W(588) + 30;
    if (t % 60 == 0) {
      const uint32_t point = 656 + (t % 300) / 60 * 24;
      uint32_t record = c.a + 6;
      if (second) {
        *game::GuestPtr<be<uint16_t>>(base, frame + 88) = c.H(6);
        *game::GuestPtr<be<uint16_t>>(base, frame + 90) = c.H(8);
        *game::GuestPtr<be<uint16_t>>(base, frame + 92) = c.H(10);
        record = frame + 88;
      }
      effect(10, c.F(point), c.F(point + 4), c.F(point + 8), record, flags | 64);
    }
    if (running(c.B(576))) {
      const uint32_t record = second ? kRecordKind5Side2 : kRecordKind5Side1;
      set_angle(record, mem.H(angles + 2));
      feet(second ? 15 : 14, c.F(28), record, 16, 8);
    }
    return;
  }

  if (((ground >= zero) || under == 0) && In(stage, {0, 1, 9}) && running(b576)) {
    set_angle(kRecordStepGround, mem.H(angles + 2));
    feet(18, c.F(28), kRecordStepGround, 16, 8);
    return;
  }
  if (stage == 3 && robot == 17) return;
  if (In(stage, {2, 3, 4, 6, 8, 7, 10, 17, 20}) && running(b576)) {
    set_angle(kRecordStep, mem.H(angles + 2));
    feet(13, c.F(28), kRecordStep, 8, 1);
    return;
  }
  if (water_stage && under == 1) {
    if (k.Call(__imp__sub_821B3140, c.a) != 0) {
      if (mem.H(foot1) % 16 == 8) {
        set_angle(kRecordUnderDash, mem.H(angles + 2));
        effect(6, c.F(656), zero, c.F(664), kRecordUnderDash, flags | 2);
      }
    } else if (b576 == 0) {
      set_angle(kRecordUnderStep, c.H(8));
      feet(9, zero, kRecordUnderStep, 40, 1);
    }
  }
}

void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    // sub_822A74B0 draws from generator 2 and counts its calls at 0x839C28E4.
    const uint32_t generators = game::LoadGuestAddress(base, game::kRandomGenerators);
    const uint32_t random_state = game::LoadGuestAddress(base, generators + 16 * 2 + 4);
    verifier = new Verifier("sub_821B3AF8", base,
                            {{game_address, 0xD000, "game"},
                             {0x83854380, 0x420, "effect records"},
                             {0x8389B488, 8 + 2 * 64 * 40, "sub_82286120 pools"},
                             {0x839C28E4, 4, "random calls"},
                             {random_state, game::kRandomStateSize, "random state"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821B3AF8(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        EffectStep(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} state {} robot {} button {} stage {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(272), c.B(2), c.B(576), Control{base, 0}.W(kStageKind));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821B3AF8) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821B3AF8(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::EffectStep(ctx, base);
}
