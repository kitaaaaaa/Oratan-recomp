// Hand-decompiled damage step of a robot.
//
// sub_821E9900(control, other control), called each tick by the driver
// (sub_821F8168) unless the robot's control mode (+1) is 3 or +3 is set. The
// hits of the tick are waiting in the control block: +4446 damage, +4442 an
// amount added to +116, +4432 extra damage, +4440 an amount taken off the
// +466 / +468 / +470 / +464 counters, +4448 hit flags. In order:
//   - sub_821E9730: flags that start the +458 / +460 / +462 / +446 timers,
//     cut health (+80) to 2/5, fill or empty the weapon gauges, add +444 /
//     +442, and heal by a negative +4446;
//   - the +464 counter and three limits for +116 + +112 (by robot, +278, and
//     Ajim's (15) +454 scale table);
//   - +316 / +314 (counted down each tick) cancel some hits while running;
//     +320 absorbs damage; Dordray (0) and robots 4 / 12 scale it by +450;
//     +338 scales it by a per-robot percentage;
//   - the damage itself, by Game +24924 (percent) and Game +680 (rises by up
//     to 10% between 1,500 and 3,000), the substate and +278 multipliers in
//     the robot's data, capped at Game +24928, stored at +86;
//   - the reaction: a state by the flags and +116 + +112 against the limits
//     (states 3, 5 / 6, 7, 8, 16, 23, 29, 30 through the helpers below);
//   - health (+80) drops by the damage (module 0 keeps it below the maximum,
//     +88). Robot 8 is held at 29.8% while +180 is 255 and starts its +450
//     mode below half health; in a 15-mode game Ajim drains health on some
//     ticks. At 0 health +3 becomes 2 and most states go to state 30.
// Callees that are not rewritten here run as the original code.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <bit>
#include <climits>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>

#include <fmt/format.h>

// Each guest float instruction is its own operation, as in robot_input.cpp.
#pragma clang fp contract(off)

REX_EXTERN(__imp__sub_821E9900);
REX_EXTERN(__imp__sub_822AC098);
REX_EXTERN(__imp__sub_822A6B70);
REX_EXTERN(__imp__sub_822878C0);
REX_EXTERN(__imp__sub_8214F638);
REX_EXTERN(__imp__sub_8214BD48);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;

constexpr uint32_t kGameBusy = 24932;
constexpr uint32_t kGameDamagePercent = 24924;
constexpr uint32_t kGameDamageCap = 24928;
constexpr uint32_t kControl1 = 25020, kControl2 = 29516;
constexpr uint32_t kZeroFloat = 0x8200095C;
// Per robot, 100 bytes: +0 a limit, +4 25 flag words for +3296.
constexpr uint32_t kRobotFlagTable = 0x82027338;
// Ajim's +464 scale, a float per +454 value.
constexpr uint32_t kAjimScale = kRobotFlagTable + 2352;
constexpr uint32_t kFloat958 = 0x82000958, kFloat1B2C = 0x82001B2C, kFloat1598 = 0x82001598;
constexpr uint32_t kFloat137C = 0x8200137C, kFloat79C4 = 0x820079C4;

struct Control {
  uint8_t* base;
  uint32_t a;
  uint8_t B(uint32_t o) const { return *game::GuestPtr<uint8_t>(base, a + o); }
  void SetB(uint32_t o, uint32_t v) const { *game::GuestPtr<uint8_t>(base, a + o) = uint8_t(v); }
  uint16_t H(uint32_t o) const { return *game::GuestPtr<be<uint16_t>>(base, a + o); }
  void SetH(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint16_t>>(base, a + o) = uint16_t(v); }
  int32_t S16(uint32_t o) const { return int16_t(H(o)); }
  uint32_t W(uint32_t o) const { return *game::GuestPtr<be<uint32_t>>(base, a + o); }
  int32_t S32(uint32_t o) const { return int32_t(W(o)); }
  void SetW(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint32_t>>(base, a + o) = v; }
};

float GuestFloat(const uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, address)));
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

// divw as the recompiled code does it (0 for x / 0 and INT_MIN / -1).
int32_t Divide(int32_t a, int32_t b) { return (b && !(a == INT_MIN && b == -1)) ? a / b : 0; }

// What the helpers share: the game block and its robot data (Game +38508).
struct Hit {
  const Control& c;
  uint8_t* base;
  uint32_t game_address;
  bool busy() const { return Control{base, game_address}.W(kGameBusy) != 0; }
  uint16_t data_h(uint32_t o) const {
    const uint32_t data = Control{base, game_address}.W(game::kGameRobotData);
    return Control{base, data}.H(o);
  }
};

// +4 bit 2 from a bit of the robot's parameter +116, then bit 4 cleared.
void SetFlags4(const Control& c, bool set) {
  const uint32_t v = set ? (c.H(4) | 2) : (c.H(4) & ~2u);
  c.SetH(4, v & ~4u);
}

// The class at +4430 as 2 (class 2) or 3 (others).
uint32_t Class584(const Control& c) { return c.B(4430) == 2 ? 2 : 3; }

// While Game +24932 is set the helpers go to state 8 instead.
void ToState8(const Control& c) {
  for (uint32_t o : {274u, 292u, 102u, 276u}) c.SetH(o, 0);
  c.SetB(286, 0);
  c.SetB(577, 0);
  c.SetB(0, 0);
  c.SetW(116, 0);
  c.SetH(272, 8);
  c.SetB(576, 28);
  c.SetH(4, (c.H(4) & ~6u) | 2);
  c.SetH(16, c.H(4436));
  c.SetH(14, c.H(4434));
  c.SetW(584, Class584(c));
}

// State 5 (or 6 when `state6`), facing by +4429; shared by sub_821E8E60 and
// sub_821E9388. Returns the value for +64.
uint32_t ToState5(const Control& c, uint32_t state) {
  c.SetH(272, state);
  c.SetH(274, 0);
  const uint32_t n = Class584(c);
  c.SetW(584, n);
  const Control p{c.base, c.W(76)};
  uint32_t bit;
  if (c.B(4429) == 0) {
    c.SetB(576, 28);
    bit = 1u << n;
  } else {
    c.SetB(576, 29);
    bit = 16u << n;
  }
  SetFlags4(c, (p.H(116) & bit) != 0);
  c.SetB(0, 0);
  for (uint32_t o : {292u, 102u}) c.SetH(o, 0);
  c.SetH(16, c.H(4436));
  c.SetH(14, c.H(4434));
  c.SetB(286, 0);
  c.SetB(577, 0);
  c.SetW(116, 0);
  c.SetH(276, 0);
  c.SetH(338, 0);
  return p.W(3664);
}

// State 7, facing by class; shared by sub_821E9040 and sub_821E95C0.
uint32_t ToState7(const Control& c, bool clear_338) {
  c.SetH(274, 0);
  const uint32_t cls = c.B(4430);
  c.SetW(584, cls);
  c.SetH(272, 7);
  c.SetB(576, 31);
  const Control p{c.base, c.W(76)};
  SetFlags4(c, (p.H(116) & (256u << cls)) != 0);
  c.SetB(0, 0);
  c.SetH(16, c.H(4436));
  c.SetH(14, c.H(4434));
  for (uint32_t o : {292u, 102u}) c.SetH(o, 0);
  c.SetB(286, 0);
  c.SetB(577, 0);
  c.SetW(116, 0);
  c.SetH(276, 0);
  if (clear_338) c.SetH(338, 0);
  return p.W(3664);
}

// From substate 17 to 18 (and from 18 on): the shared end of a light hit.
void Substate17To18(const Hit& h) {
  const Control& c = h.c;
  if (c.H(274) == 17) {
    c.SetH(274, 18);
    c.SetH(16, c.H(4436));
    c.SetH(14, c.H(4434));
  }
  c.SetH(292, 0);
  c.SetB(286, 0);
  c.SetH(316, h.data_h(3874));
}

// sub_821E9730: what the flags do before the damage.
void ApplyFlags(const Hit& h) {
  const Control& c = h.c;
  if (c.H(314) == 0) {
    const uint32_t flags = c.W(4448);
    if (flags & 0x10) {
      for (uint32_t o : {462u, 460u, 458u}) c.SetH(o, 720);
    }
    if (flags & 2) c.SetH(458, 720);
    if (flags & 4) c.SetH(460, 720);
    if (flags & 8) c.SetH(462, 720);
    if ((flags & 0x200000) && Control{h.base, h.game_address}.W(588) % 10 <= 6) c.SetH(446, 600);
    if ((flags & 0x40) && c.S16(80) > 0) {
      // Health to 2/5, at least 1.
      const int16_t v = int16_t(Divide(c.S16(80) * 2, 5));
      c.SetH(80, uint16_t(v));
      if (v == 0) c.SetH(80, 1);
    }
    if (flags & 0x80) {
      // Gauges full.
      const Control p{c.base, c.W(76)};
      c.SetW(408, p.H(3508));
      c.SetW(404, p.H(3510));
      c.SetW(412, p.H(3512));
    }
    if (flags & 0x100) {
      // Gauges empty, health full.
      c.SetW(412, 0);
      c.SetW(404, 0);
      c.SetW(408, 0);
      c.SetH(80, c.H(88));
    }
  }
  const uint32_t flags = c.W(4448);
  if (flags & 0x100000) c.SetH(444, 8);
  if (flags & 0x40000) c.SetH(442, 4);
  const int32_t heal = c.S16(4446);
  if (heal < 0) {
    if (flags & 0x2000000) {
      const int16_t v = int16_t(uint32_t(c.H(80)) - uint32_t(heal));
      c.SetH(80, uint16_t(v));
      if (v > c.S16(88)) c.SetH(80, c.H(88));
      c.SetW(4448, 0);
    }
    c.SetH(4446, 0);
  }
}

// sub_821E8D28: state 16 once +116 + +112 reaches the parameter +3814.
void Knockdown(const Hit& h) {
  const Control& c = h.c;
  if (c.H(274) == 17) return;
  const Control p{c.base, c.W(76)};
  const int32_t total = c.S32(116) + c.S32(112);
  if (total >= int32_t(p.H(3814))) {
    c.SetH(290, 0);
    c.SetH(272, 16);
    c.SetH(274, 0);
    c.SetW(584, c.B(4430));
    SetFlags4(c, (p.H(116) & 0x4000) != 0);
    c.SetH(292, 0);
    c.SetH(102, 0);
    c.SetB(286, 0);
    c.SetB(577, 0);
    c.SetW(64, Control{h.base, Control{h.base, h.game_address}.W(game::kGameRobotData)}.W(3660));
    c.SetH(16, c.H(4436));
    c.SetH(14, c.H(4434));
    c.SetH(10, 0);
    c.SetH(6, 0);
    c.SetB(0, 0);
  } else if (total >= int32_t(p.H(3808))) {
    c.SetH(316, p.H(3874));
    c.SetH(314, p.H(3874));
  }
}

// sub_821E8E60: state 5 / 6 (8 while busy), +314 / +316 set.
void HitWhileRecovering(const Hit& h) {
  const Control& c = h.c;
  uint32_t v64;
  if (!h.busy()) {
    v64 = ToState5(c, (c.W(4448) & 0x3400) != 0 ? 6 : 5);
  } else {
    ToState8(c);
    c.SetH(338, 0);
    v64 = *game::GuestPtr<be<uint32_t>>(h.base, kZeroFloat);
  }
  c.SetW(64, v64);
  const uint16_t v = h.data_h(3874);
  c.SetH(316, v);
  c.SetH(314, v);
}

// sub_821E9040 (and sub_821E95C0, which also clears +338): state 7 (8
// while busy), +316 set.
void HeavyHit(const Hit& h, bool clear_338) {
  const Control& c = h.c;
  uint32_t v64;
  if (!h.busy()) {
    v64 = ToState7(c, clear_338);
  } else {
    ToState8(c);
    if (clear_338) c.SetH(338, 0);
    v64 = *game::GuestPtr<be<uint32_t>>(h.base, kZeroFloat);
  }
  c.SetW(64, v64);
  c.SetH(316, h.data_h(3874));
}

// sub_821E91A8: below the second limit, state 3 (a stagger), or substate 18.
void LightHit(const Hit& h) {
  const Control& c = h.c;
  const uint32_t flags = c.W(4448);
  const uint32_t m = flags & 0x3400;
  if (m != 0 && In(c.H(274), {17, 18})) {
    Substate17To18(h);
    return;
  }
  const uint32_t state = c.H(272);
  if (state == 19 || state == 21) {
    c.SetH(m != 0 ? 316 : 314, h.data_h(3874));
    c.SetH(318, 8);
    return;
  }
  if (state == 3 && (flags & 0x1E0000) != 0) return;
  if (flags & 0x140000) return;
  c.SetH(272, 3);
  if (m != 0) {
    c.SetH(274, 1);
    c.SetH(316, h.data_h(3874));
  } else {
    c.SetH(274, 0);
    if ((flags & 0xA0000) == 0) c.SetH(314, h.data_h(3874));
  }
  c.SetH(290, 0);
  const uint32_t cls = c.B(4430);
  c.SetW(584, cls == 2 ? 3 : cls == 0 ? 1 : cls == 1 ? 0 : 2);
  c.SetH(292, 0);
  c.SetH(102, 0);
  c.SetB(286, 0);
  c.SetB(577, 0);
  c.SetB(100, 4);
  c.SetH(16, c.H(4436));
}

// sub_821E9388: below the third limit, state 5 / 6 (8 while busy), or
// substate 18.
void MediumHit(const Hit& h) {
  const Control& c = h.c;
  uint32_t v64;
  if (!h.busy()) {
    const uint32_t m = c.W(4448) & 0x3400;
    if (m != 0 && In(c.H(274), {17, 18})) {
      Substate17To18(h);
      return;
    }
    v64 = ToState5(c, m != 0 ? 6 : 5);
  } else {
    ToState8(c);
    c.SetH(338, 0);
    v64 = *game::GuestPtr<be<uint32_t>>(h.base, kZeroFloat);
  }
  c.SetW(64, v64);
}

// Every field a reset to the free state clears.
void ClearToFree(const Control& c) {
  for (uint32_t o : {272u, 288u, 290u, 274u}) c.SetH(o, 0);
  c.SetB(576, 0);
  c.SetB(577, 0);
  for (uint32_t o : {292u, 102u, 294u, 104u}) c.SetH(o, 0);
  c.SetW(584, 0);
  for (uint32_t o : {594u, 598u, 12u, 4492u}) c.SetH(o, 0);
  c.SetW(112, 0);
}

// Percentages of +4446 and +4442 while +338 is set, by robot.
void Percent338(uint32_t robot, int32_t& damage, int32_t& amount) {
  switch (robot) {
    case 0:
    case 6: damage = 80, amount = 85; break;
    case 2: damage = 60, amount = 60; break;
    case 4: damage = 70, amount = 70; break;
    case 7: damage = 70, amount = 60; break;
    case 8:
    case 14: damage = 40, amount = 40; break;
    default: damage = 80, amount = 75; break;
  }
}

void DamageStep(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  const Control other{base, ctx.r4.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 224;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  const Hit h{c, base, game_address};
  const auto d = [&] { return Control{base, g.W(game::kGameRobotData)}; };
  const float f958 = GuestFloat(base, kFloat958);

  if (c.B(1) == 3 || c.B(3) != 0) return;
  if (c.B(1) == 0 && Control{base, c.W(72)}.H(86) != 0) {
    c.SetH(392, 255);
  } else if (c.H(392) != 0) {
    c.SetH(392, c.H(392) - 1);
  }
  if (c.B(2) == 10 && c.H(452) != 0 && c.H(452) < 65534) {
    c.SetH(4446, 0);
    c.SetH(4432, 0);
  }
  c.SetH(86, 0);
  int32_t damage = 0;
  ApplyFlags(h);

  // The +464 counter and the three limits for +116 + +112.
  const uint32_t robot = c.B(2);
  const Control p{base, c.W(76)};
  int32_t c464 = 0, limit1 = 0, limit2 = 0, limit3 = 0;
  const auto limits = [&](uint32_t a, uint32_t b, uint32_t e) {
    limit1 = p.H(a);
    limit2 = p.H(b);
    limit3 = p.H(e);
  };
  if (robot == 18) {
    // The limits stay unset: robots 17 / 18 never use them.
  } else if (robot == 17 && c.H(272) == 19) {
    c.SetH(466, p.H(3800));
    limits(3806, 3812, 3818);
  } else if (robot == 0 && (c.S16(450) > 0 || c.H(272) == 24)) {
    c464 = 1100;
    limit1 = p.H(3806);
    limit2 = 1000;
    limit3 = 1500;
  } else if (c.H(278) == 1) {
    c464 = c.H(470);
    limits(3810, 3816, 3820);
  } else {
    const uint32_t s278 = c.H(278);
    const bool mid = s278 >= 2 && s278 <= 4;
    c464 = mid ? c.H(468) : c.H(466);
    if (robot == 15 && c.H(454) != 2) {
      const float scale = GuestFloat(base, kAjimScale + c.H(454) * 4);
      c464 = int32_t(std::trunc(double(scale * float(c464))));
    }
    if (mid) {
      limits(3808, 3814, 3818);
    } else {
      limits(3806, 3812, 3818);
    }
  }
  c.SetH(464, uint32_t(c464));

  // Hits cancelled while +316 / +314 run.
  if (c.S16(316) != 0 && c.B(4495) == 0 && (c.W(4448) & 0x3C00) != 0) {
    c.SetH(4446, 0);
    c.SetH(4442, 0);
    c.SetH(316, c.S16(316) + 1);
    c.SetW(4448, c.W(4448) & ~0x3C00u);
  } else if (c.S16(314) != 0 && c.B(4495) == 0 && (c.W(4448) & 0x01EA023F) != 0) {
    c.SetH(4446, 0);
    c.SetH(4442, 0);
    c.SetH(314, c.S16(314) + 1);
    c.SetW(4448, c.W(4448) & ~0x01EA023Fu);
  } else if (const uint32_t flags = c.W(4448);
             (flags & 0x800000) && c.S16(4446) > 0 && c.B(4495) == 0) {
    c.SetH(292, 0);
    c.SetB(286, 0);
    c.SetH(274, 1);
    c.SetH(272, 23);
    c.SetH(314, d().H(3874));
  } else if ((flags & 0x20) && c.B(4495) == 0) {
    c.SetH(274, 0);
    c.SetH(272, 29);
    c.SetH(292, 0);
    c.SetB(286, 0);
    c.SetH(312, c.H(4444));
  } else if (flags & 0x10000) {
    ClearToFree(c);
    c.SetH(156, 32);
    c.SetH(16, c.H(4436));
    c.SetH(14, c.H(4434));
    c.SetH(18, 0);
    c.SetH(316, 32);
  } else if ((c.S16(4446) != 0 || c.H(4442) != 0 || c.H(4432) != 0) && flags != 0) {
    // +320 absorbs +4442, then +4446.
    if (c.S16(320) >= int32_t(c.H(4442))) {
      c.SetH(320, c.S16(320) - c.H(4442));
      c.SetH(4442, 0);
    } else {
      c.SetH(320, 0);
    }
    const int32_t first = c.S16(4446);
    if (c.S16(320) >= first) {
      c.SetH(320, c.S16(320) - first);
      c.SetH(4446, 0);
    } else {
      c.SetH(320, 0);
    }
    if ((robot == 4 || robot == 12) && c.S16(450) != 0) {
      // Doubled while +450 is negative, halved while positive.
      c.SetH(4446, c.S16(450) > 0 ? Divide(c.S16(4446), 2) : c.S16(4446) << 1);
    } else if (robot == 0 && (c.S16(450) > 0 || c.H(272) == 24)) {
      c.SetH(4446, Divide(c.S16(4446), 2));
    }
    if (c.H(338) != 0) {
      int32_t pd, pa;
      Percent338(robot, pd, pa);
      c.SetH(4446, Divide(c.S16(4446) * pd, 100));
      c.SetH(4442, Divide(int32_t(c.H(4442)) * pa, 100));
    }

    // +4440 counts the counters down; above 30 it sets a random +3296 bit.
    const int32_t drain = c.S16(4440);
    if (drain > 0) {
      if (drain > 30) c.SetW(3296, c.W(3296) | (1u << (g.W(588) % 19)));
      const uint32_t h466 = c.H(466);
      if (int32_t(h466) < drain) {
        c.SetH(466, 0);
        c.SetW(3296, 0xFFFFFFFF);
      } else {
        const uint32_t n = uint16_t(h466 - c.H(4440));
        c.SetH(466, n);
        if (robot < 15) {
          const uint32_t row = kRobotFlagTable + robot * 100;
          const uint32_t limit = *game::GuestPtr<be<uint32_t>>(base, row);
          const uint32_t d3800 = p.H(3800);
          const uint32_t v = d3800 ? (n * limit) / d3800 : 0;
          if (v < limit) {
            const uint32_t bits = *game::GuestPtr<be<uint32_t>>(base, kRobotFlagTable + 4 + (robot * 25 + v) * 4);
            c.SetW(3296, bits | c.W(3296));
          }
        }
      }
      for (uint32_t o : {470u, 468u}) {
        c.SetH(o, int32_t(c.H(o)) < drain ? 0 : c.H(o) - c.H(4440));
      }
      c464 = c464 < drain ? 0 : c464 - drain;
      c.SetH(4440, 0);
    }
    c.SetH(464, uint32_t(c464));

    // The damage.
    const uint32_t h4438 = c.H(4438);
    if (c464 > 0 && (h4438 == 0 || int32_t(h4438) < c464) && (flags & 0xFFFF01FF) != 0) {
      damage = 0;
    } else if (robot == 18 && c.B(4428) == 0) {
      damage = int16_t(Divide(c.S16(4446), 10));
    } else {
      const int32_t t = g.S32(680), percent = g.S32(kGameDamagePercent);
      if (t < 1500) {
        damage = int16_t(Divide(percent * c.S16(4446), 100));
      } else if (t < 3000) {
        const int32_t rise = Divide(t - 1500, 144) + 100;
        damage = int16_t(Divide(rise * percent * c.S16(4446), 10000));
      } else {
        damage = int16_t(Divide(percent * c.S16(4446) * 110, 10000));
      }
    }
    // Multipliers from the robot's data, by substate and +278.
    const uint32_t sub = c.H(274);
    const auto scale = [&](uint32_t o) { damage = int16_t(Divide(d().S32(o) * damage, 100)); };
    if (robot == 17 && c.H(272) != 19) {
      damage = 0;
    } else if (In(sub, {17, 18}) && (((flags & 0x400) && c.B(4429) == 0) || (flags & 0x3800))) {
      scale(3776);
    } else if (In(sub, {16, 14, 12}) && (flags & 0x400) && c.B(4429) == 1) {
      scale(3784);
    } else if ((flags & 0x3C00) == 0 && c.H(278) == 1) {
      scale(3780);
    }
    if (damage > g.S32(kGameDamageCap)) damage = int16_t(g.S32(kGameDamageCap));
    // +116 grows by +4442 (scaled in substates 17 / 18).
    const uint32_t sub2 = c.H(274);
    int32_t add = c.H(4442);
    if (In(sub2, {17, 18}) && (flags & 0x800)) {
      add = Divide(d().S32(3788) * int32_t(c.H(4442)), 100);
    } else if (In(sub2, {17, 18}) && (flags & 0x3400)) {
      add = Divide(d().S32(3792) * int32_t(c.H(4442)), 100);
    }
    c.SetW(116, uint32_t(add + c.S32(116)));
    if (flags & 0x140000) damage = int16_t(c.H(4432) + damage);
    if ((flags & 0x1000000) && c.H(448) < 32768) damage = int16_t(c.H(4432) + damage);
    c.SetH(86, uint32_t(damage));

    const auto side = [&](const Control& x) { return x.a != game_address + kControl1 ? 1u : 0u; };
    const auto rumble = [&](const Control& x, float f1, uint32_t r5, uint32_t r6) {
      k.k.f1.f64 = double(f1);
      k.k.r6.u64 = r6;
      k.k.r5.u64 = r5;
      k.Call(__imp__sub_8214F638, side(x));
    };
    if (flags & 0xBC00) {
      if (other.H(278) != 1 && In(sub2, {17, 18})) {
        k.k.r4.u64 = 1;
        k.Call(__imp__sub_822878C0, c.a);
        if (g.H(578) == 15) {
          const float f = GuestFloat(base, kFloat1B2C);
          if (c.B(1) == 0) rumble(c, f, 3, 2);
          if (other.B(1) == 0) rumble(other, f, 3, 2);
        }
      } else {
        k.k.r4.u64 = 0;
        k.Call(__imp__sub_822878C0, c.a);
        if (other.B(1) == 0 && g.H(578) == 15) {
          rumble(other, f958, 5, 2);
          k.k.r4.u64 = other.a;
          k.Call(__imp__sub_8214BD48, 7);
          k.k.r4.u64 = other.a;
          k.Call(__imp__sub_8214BD48, 8);
        }
      }
      c.SetH(316, 4);
      c.SetH(156, 15);
    } else if (robot != 17 && robot != 18) {
      c.SetH(314, 4);
    }

    // The reaction.
    const uint32_t robot2 = c.B(2);
    const uint32_t f = c.W(4448);
    if (robot2 == 17 || robot2 == 18) {
      c.SetH((f & 0xBC00) != 0 ? 316 : 314, 32);
    } else if (f & 0x8000) {
      if (c.B(4495) != 0) {
        c.SetH(276, c.H(276) + 64);
        c.SetB(4495, 0);
        c.SetH(102, 0);
        c.SetH(274, 1);
        c.SetH(316, d().H(3874));
        c.SetW(116, 0);
        const int32_t limit = Divide(c.S16(88), 7);
        if (damage > limit) damage = int16_t(limit);
      } else {
        c.SetH(86, 0);
        damage = 0;
        c.SetH(4442, 0);
        c.SetH(4446, 0);
      }
    } else if (c.B(4495) != 0) {
      c.SetH(274, 1);
      c.SetH(102, 0);
      damage = int16_t(Divide(damage, 5));
      c.SetB(4495, 0);
      if (damage > 50) damage = 50;
      c.SetW(116, 0);
      c.SetH(314, d().H(3874));
    } else if (f & 0x4000) {
      // No reaction.
    } else {
      const uint32_t state = c.H(272);
      if (state == 23 && c.H(274) == 0 && (f & 0xBC00)) {
        c.SetH(448, uint32_t(-40));
        c.SetH(316, d().H(3874));
      } else if (state == 23 && c.H(274) == 0 && (f & 0x3E03FF)) {
        c.SetH(448, uint32_t(-40));
        c.SetH(314, d().H(3874));
      } else if ((f & 0x1000000) && c.H(448) < 32768) {
        if (state != 10) {
          if (state != 23) {
            c.SetH(288, 0);
            c.SetH(274, 0);
            c.SetH(272, 23);
            c.SetH(292, 0);
            c.SetB(286, 0);
            c.SetW(300, 0);
            c.SetH(448, 60);
          }
          c.SetH(314, 0);
          c.SetH(448, c.H(448) + 1);
        }
      } else if (f & 0x400000) {
        c.SetH(274, 2);
        c.SetH(272, 23);
        c.SetH(292, 0);
        c.SetB(286, 0);
        c.SetH(448, c.H(4444));
      } else if (state == 23 && c.H(274) == 2) {
        c.SetH((f & 0xBC00) != 0 ? 316 : 314, d().H(3874));
      } else if (g.W(kGameBusy) == 0 && c.H(278) >= 2 && c.H(278) <= 4) {
        Knockdown(h);
      } else if (c.S16(448) > 0 && (f & 0x3E3F)) {
        HitWhileRecovering(h);
      } else if (other.B(2) == 9 && (f & 0x800) && other.S16(450) > 0) {
        // Robot 9 in its +450 mode takes damage back from a 0x800 hit.
        int32_t back = damage;
        if (In(robot2, {4, 11, 12}) && c.S16(450) > 0) {
          back = Divide(damage * 3, 2);
        } else if (robot2 == 11 && c.H(450) >= 32768) {
          back = Divide(damage, 2);
        } else if (robot2 == 0 && c.S16(450) > 0) {
          back = Divide(damage * 6, 5);
        }
        other.SetH(4446, other.H(4446) - back);
        other.SetW(4448, other.W(4448) | 0x2000000);
        c.SetH(272, 23);
        c.SetH(288, 0);
        c.SetH(274, 2);
        c.SetH(292, 0);
        c.SetB(286, 0);
        c.SetW(300, 0);
        c.SetH(448, 90);
      } else if (f & 0x800) {
        HeavyHit(h, false);
      } else {
        const int32_t total = c.S32(116) + c.S32(112);
        if (total < limit1) {
          if (state == 29) {
            ClearToFree(c);
            c.SetH(312, 0);
          } else if ((f & 0x3400) && In(c.H(274), {17, 18})) {
            Substate17To18(h);
          }
        } else if (total < limit2) {
          LightHit(h);
        } else if (total < limit3) {
          MediumHit(h);
        } else {
          HeavyHit(h, true);
        }
      }
    }
  }

  // Health.
  if (int8_t(k.Call(__imp__sub_822A6B70)) == 0 && damage >= c.S16(88)) damage = int16_t(c.S16(88) - 1);
  if (damage > 0 && c.B(1) == 0 && g.H(578) == 15) {
    float top = float(double(c.S16(88))) * GuestFloat(base, kFloat1598);
    if (top < f958) top = f958;
    float amount = float(double(damage));
    if (!(amount < top)) amount = top;
    const float ratio = f958 / top;
    const float base_value = GuestFloat(base, kFloat137C);
    const float strength = float(std::fma(double(ratio * amount), double(base_value), double(base_value)));
    k.k.f1.f64 = double(strength);
    k.k.r6.u64 = 1;
    k.k.r5.u64 = strength > GuestFloat(base, kFloat79C4) ? 5 : 4;
    k.Call(__imp__sub_8214F638, c.a != game_address + kControl1 ? 1 : 0);
  }
  c.SetH(80, int16_t(c.H(80) - damage));
  const uint32_t robot3 = c.B(2);
  if (robot3 == 8) {
    if (c.B(180) == 255 && c.H(450) == 0) {
      const int32_t limit = Divide(c.S16(88) * 298, 1000);
      if (c.S16(80) > limit) c.SetH(80, uint32_t(limit));
    }
    if (c.S16(80) < Divide(c.S16(88), 2) && c.H(450) == 0) {
      c.SetH(450, 32767);
      c.SetH(466, 500);
      c.SetH(468, 450);
      c.SetH(470, 550);
    }
  }
  c.SetB(4431, 0);
  for (uint32_t o : {4446u, 4432u, 4440u, 4442u}) c.SetH(o, 0);
  c.SetW(4448, 0);
  if (c.B(1) == 0 && g.H(578) == 15 && robot3 == 15 && Control{base, c.W(72)}.B(2) != 15) {
    // Ajim drains health on the 6th tick of every 50 (Game +668).
    const int32_t t = g.S32(668);
    if (t % 50 == 6) {
      const int32_t q = t / 50;
      int32_t n = g.S32(672);
      if (n > 99) n = 100;
      const int32_t div = n - 1;
      const int32_t full = c.S16(88);
      int32_t step = Divide(full, div);
      if ((full - Divide(full, div) * div) * q >= Divide(full, div) * div) step += 1;
      const uint32_t old = c.H(80);
      if (c.S16(80) > step) {
        const uint32_t v = c.H(80) - uint32_t(step);
        if (c.H(82) == old) {
          c.SetH(80, v);
          c.SetH(82, v);
        } else {
          c.SetH(80, v);
        }
      } else {
        c.SetH(80, 0);
        if (c.H(82) == old) c.SetH(82, 0);
      }
    }
  }
  if (c.S16(314) != 0) c.SetH(314, c.S16(314) - 1);
  if (c.S16(316) != 0) c.SetH(316, c.S16(316) - 1);
  if (c.S16(80) > 0 || c.B(3) == 2 || c.B(3) == 3) return;
  // Down: most states go to state 30.
  const uint32_t state = c.H(272);
  c.SetB(3, 2);
  c.SetH(80, 0);
  const bool keep = In(state, {5, 10, 6, 7, 16, 12, 13, 15, 24, 25, 26, 27, 28}) ||
                    (state == 18 && (c.H(274) == 1 || c.B(577) == 2));
  if (keep) return;
  if (state == 23) {
    c.SetH(448, uint32_t(-30));
    return;
  }
  c.SetH(274, 0);
  c.SetH(102, 0);
  c.SetB(577, 0);
  c.SetH(272, 30);
  c.SetB(286, 0xFF);
  c.SetH(292, 0xFFFF);
}

void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821E9900", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821E9900(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        DamageStep(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} state {} sub {} robot {} flags {:X} damage {}",
                           control == game_address + kControl2 ? 2 : 1, c.H(272), c.H(274), c.B(2), c.W(4448),
                           c.S16(4446));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821E9900) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E9900(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::DamageStep(ctx, base);
}
