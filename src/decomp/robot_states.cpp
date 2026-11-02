// Hand-decompiled movement state handlers (the table at 0x82027C80, one per
// RobotControl movement_state, called by the per-tick driver sub_821F8168).
//
// sub_821EE610, state 0: the free state. It reads nearly every input field
// and starts moves: it resets a few things each tick, handles the substate
// (+274) it is in (dashes and turns through sub_821DD180, the substates
// 2-10 / 11-16 / 17-18 / 19-20 helpers), and then, by the buttons' hold
// timers, the latched direction class and RobotInput::angle, may move the
// robot to another state (1, 2, 12, 19, 24, 25) with values taken from the
// tables at 0x82027C28 / 0x82027C38 / 0x82027E00 / 0x82027E10 indexed by the
// latched class. Most of the control block fields it touches are not named
// yet, so they are written by offset here.
//
// sub_821F2538, state 19: substate 0 hands over to sub_821F1BE0. Other
// substates set the speed (+372) from the robot's data (by substate group
// 2-4, 5-7 or the rest; +278 == 1 and substates 11-16 use other values), set
// the +337 / +4494 flags from timer windows in the robot's parameters, and,
// when sub_821DD180 reports the move finished, return to state 0, go to
// state 12 substate 5, or switch substate. Robots 5 (Bal-Keros), 15 (Ajim,
// with the SDE_aji_up / SDE_aji_down sounds) and 17 (Bradtos) have extra cases.
//
// sub_821F1228, state 15. Substate 0 sets the speed and the move from the
// tables, and may start a special when the command from sub_821DF440 is 8
// (5 for Dordray's state 28) and the gauge counters are 0: Temjin's surfing
// ram and the Apharmds' move (state 24), Cypher's transformation (25),
// Dordray's states 26 / 28; any other command becomes the substate. Other
// substates set the speed from the robot's data and end like state 19.
// While +0 is set, the state ends once sub_821DD180 reports it.
//
// sub_821E4F88, state 18. It resets a few timers each tick; substate 1
// waits for sub_821DD180 and +102. Otherwise, after sub_821DD968 picks the
// next substate (+290) and the move is over, it switches to substates 11-16
// or 2-10, or to state 21 (+174 set), state 19 (a class at +164 and flag 256),
// state 12 (+288 == 12) or the free state.
//
// sub_821F0288, state 12: in the air (the rise, attacks and dashes in the
// air, the landing and the fall); see State12.
//
// sub_821E2328: substate 0 of the state sub_821E4D58 handles; see
// MoveSubstate0.
//
// sub_821E1FF0: after sub_821E4D58's substate handler, a jump (state 12) or
// a class move (state 19); see JumpOrClassMove.
//
// sub_821E4BC0: sub_821E4D58's other substates while +278 isn't 1; see
// MoveOtherSubstates.
//
// sub_821DD2B0: fills sub_821DD968's record; see FillChoiceRecord.
//
// sub_821DD180, sub_821DD230, sub_821DD890, sub_821E0160, sub_821E01E0,
// sub_821DF990: small helpers (MotionFlags, TurnTarget, SpeedByAngle,
// AddTo112, AddTo112BySubstate, AttackFlag4494); sub_821DF6E0 (LandingDash) and
// sub_821DF440 (Command), sub_821DFFA8 (AirAttack), sub_821DEE00 (NextAirAttack).
//
// sub_821F8168, a robot's per-tick driver, in order:
//   - sets Game +38508 to the robot's data and moves the +4476 point halfway
//     toward the other robot (after 18 ticks of +338; before that it copies
//     the other robot's position);
//   - runs the steps before the state (weapon gauges, sub_821E9900, ...),
//     ends the +102 / +104 timers and counts +116 / +448 down;
//   - calls the movement state's handler from the table at 0x82027C80;
//   - turns +600 toward +598 by at most 2560 a tick;
//   - runs the steps after the state and the motion function (sub_821F6A10,
//     or sub_821F75E8 while Game +24932 is set);
//   - counts the other timers down and copies the next values (+284..+300)
//     into place.
// Robot extras: Dordray's (0) size scale at +48 (1.0 = normal) with its
// +450 timer; Specineff's (10) Death Mode countdown at +452 with its sounds,
// which sets health (+80) to 0 at the end; Bradtos (17) runs +102 twice as
// fast at half health (+88 is the maximum).
//
// Every callee runs as the original code. The original passes some of them
// the registers the previous call left (for example sub_821E0160 right after
// sub_821DD230), so the rewrite keeps one register context running through
// all its calls, as the original's registers do.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>

#include <fmt/format.h>

#pragma clang fp contract(off)

REX_EXTERN(__imp__sub_821EE610);
REX_EXTERN(__imp__sub_821E1EA8);
REX_EXTERN(__imp__sub_821F9260);
REX_EXTERN(__imp__sub_821A5C20);
REX_EXTERN(__imp__sub_821CFB90);
REX_EXTERN(__imp__sub_822AAAC0);
REX_EXTERN(__imp__sub_821ED918);
REX_EXTERN(__imp__sub_82257F50);
REX_EXTERN(__imp__sub_821DD180);
REX_EXTERN(__imp__sub_821E1930);
REX_EXTERN(__imp__sub_821BFF48);
REX_EXTERN(__imp__sub_821DD968);
REX_EXTERN(__imp__sub_821DD810);
REX_EXTERN(__imp__sub_821E0EA0);
REX_EXTERN(__imp__sub_821EDA98);
REX_EXTERN(__imp__sub_821DD1E0);
REX_EXTERN(__imp__sub_821DD230);
REX_EXTERN(__imp__sub_821E0160);
REX_EXTERN(__imp__sub_821C01D8);
REX_EXTERN(__imp__sub_821F2538);
REX_EXTERN(__imp__sub_821F1228);
REX_EXTERN(__imp__sub_821E4F88);
REX_EXTERN(__imp__sub_821DD2B0);
REX_EXTERN(__imp__sub_822AC098);
REX_EXTERN(__imp__sub_821F1BE0);
REX_EXTERN(__imp__sub_821DFC50);
REX_EXTERN(__imp__sub_821DD890);
REX_EXTERN(__imp__sub_821DF990);
REX_EXTERN(__imp__sub_821DF200);
REX_EXTERN(__imp__sub_821DF440);
REX_EXTERN(__imp__sub_821E01E0);
REX_EXTERN(__imp__sub_821F8168);
REX_EXTERN(__imp__sub_821DFBA0);
REX_EXTERN(__imp__sub_821E4BC0);
REX_EXTERN(__imp__sub_821DFDD8);
REX_EXTERN(__imp__sub_821DF7B8);
REX_EXTERN(__imp__sub_821E3510);
REX_EXTERN(__imp__sub_821E39C0);
REX_EXTERN(__imp__sub_821E4088);
REX_EXTERN(__imp__sub_821E1FF0);
REX_EXTERN(__imp__sub_821E2328);
REX_EXTERN(__imp__sub_822583A8);
REX_EXTERN(__imp__sub_821F0288);
REX_EXTERN(__imp__sub_821DEE00);
REX_EXTERN(__imp__sub_821DFFA8);
REX_EXTERN(__imp__sub_821DEC58);
REX_EXTERN(__imp__sub_821DF6E0);
// The driver's callees, by the names the original calls (some are rewrites).
REX_EXTERN(sub_822AC098);
REX_EXTERN(sub_821E8B98);
REX_EXTERN(sub_821E0308);
REX_EXTERN(sub_821FF668);
REX_EXTERN(sub_821E9900);
REX_EXTERN(sub_82561440);
REX_EXTERN(sub_8216D160);
REX_EXTERN(sub_821DD118);
REX_EXTERN(sub_821F9260);
REX_EXTERN(sub_821A5C20);
REX_EXTERN(sub_821CFB90);
REX_EXTERN(sub_822AAAC0);
REX_EXTERN(sub_82242528);
REX_EXTERN(sub_82241FB0);
REX_EXTERN(sub_822A6B70);
REX_EXTERN(sub_8224FE78);
REX_EXTERN(sub_822518C8);
REX_EXTERN(sub_821B3AF8);
REX_EXTERN(sub_821FC8C0);
REX_EXTERN(sub_821F75E8);
REX_EXTERN(sub_821F6A10);
REX_EXTERN(sub_8214EED8);
REX_EXTERN(sub_8214F638);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;

constexpr uint32_t kGameBusy = 24932;
constexpr uint32_t kControl2 = 29516;
// Indexed by the latched direction class (u16 each).
constexpr uint32_t kMoveTable0 = 0x82027C28, kMoveTable1 = 0x82027C38;
constexpr uint32_t kMoveTable2 = 0x82027E10, kMoveTable3 = 0x82027E00;

struct Control {
  uint8_t* base;
  uint32_t a;
  uint8_t B(uint32_t o) const { return *game::GuestPtr<uint8_t>(base, a + o); }
  void SetB(uint32_t o, uint32_t v) const { *game::GuestPtr<uint8_t>(base, a + o) = uint8_t(v); }
  uint16_t H(uint32_t o) const { return *game::GuestPtr<be<uint16_t>>(base, a + o); }
  void SetH(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint16_t>>(base, a + o) = uint16_t(v); }
  uint32_t W(uint32_t o) const { return *game::GuestPtr<be<uint32_t>>(base, a + o); }
  void SetW(uint32_t o, uint32_t v) const { *game::GuestPtr<be<uint32_t>>(base, a + o) = v; }
  int32_t S16(uint32_t o) const { return int16_t(H(o)); }
};

uint16_t Table(const uint8_t* base, uint32_t table, uint32_t index) {
  return *game::GuestPtr<be<uint16_t>>(base, table + index * 2);
}

// One register context running through the calls, like the original's.
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
  uint32_t Call(void (*fn)(PPCContext&, uint8_t*), uint32_t r3, uint32_t r4) {
    k.r4.u64 = r4;
    return Call(fn, r3);
  }
};

bool In(uint32_t v, std::initializer_list<uint32_t> set) {
  for (uint32_t x : set) {
    if (v == x) return true;
  }
  return false;
}

// The end of every path: maybe start a move from the buttons and the angle.
void StartMoves(const Control& c, Caller& k, uint8_t* base, bool busy) {
  const auto angle_move = [&]() {
    const uint32_t a = c.W(248) & 0xFFFF;
    c.SetH(272, 1);
    c.SetH(292, 0);
    c.SetW(584, k.Call(__imp__sub_821DD1E0, a));
    uint32_t v = a;
    if (a > 16384 && a < 49152) v = (a - 32768) & 0xFFFF;
    c.SetH(12, v);
    k.Call(__imp__sub_821DD230, c.a, 1);
    c.SetH(598, 0);
    k.Call(__imp__sub_821E0160);  // with the registers sub_821DD230 left
    if (c.H(278) == 1) {
      const uint32_t sub = c.H(274);
      if (((sub >= 11 && sub <= 16) || (sub >= 2 && sub <= 10)) && c.B(577) != 2) {
        c.SetB(286, 2);
        return;
      }
      c.SetB(284, 8);
      c.SetW(300, 0);
    }
    c.SetB(286, 0);
    c.SetH(274, 0);
    c.SetH(290, 0);
  };
  const auto angle_ready = [&](uint8_t unk_172) {
    const uint32_t sub = c.H(274);
    return c.W(264) == 0 && c.W(248) <= 65535 && unk_172 == 0 && (c.B(4494) & 1) == 0 &&
           c.H(312) == 0 && sub < 17 && !(sub >= 11 && sub <= 16);
  };
  const auto one_button = [&]() {
    const uint8_t a = c.B(178), b = c.B(179);
    return (a != 0) != (b != 0);
  };

  if (busy) {
    if (angle_ready(c.B(172))) {
      angle_move();
      return;
    }
    const uint32_t cls = c.B(164);
    if (cls == 255 || !one_button() || c.B(170) != 0 || (c.W(400) & 0x100) == 0 ||
        (c.B(4494) & 4) != 0) {
      return;
    }
    c.SetH(290, 0);
    c.SetH(288, 0);
    c.SetH(272, 19);
    c.SetH(274, 0);
    c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, cls)))));
    c.SetW(588, uint32_t(int32_t(int16_t(Table(base, kMoveTable1, cls)))));
    c.SetH(12, Table(base, kMoveTable2, cls));
    c.SetH(20, Table(base, kMoveTable3, cls));
    c.SetH(598, 0);
    c.SetH(292, 0);
    c.SetB(286, 0);
    c.SetW(300, 0);
    k.Call(__imp__sub_821DD230, c.a, 1024);
    k.k.r4.u64 = c.H(20);
    k.Call(__imp__sub_821C01D8);  // r3 as sub_821DD230 left it
    const uint8_t latched = c.B(164);
    c.SetH(4492, 0);
    c.SetH(592, 0);
    c.SetH(306, latched);
    k.Call(__imp__sub_821E0160, c.a, 2);
    return;
  }

  const uint8_t unk_172 = c.B(172);
  if (unk_172 == 255 && (c.B(4494) & 2) == 0) {
    const uint32_t sub = c.H(274);
    if (c.B(2) == 6 && In(sub, {8, 9, 10})) return;
    if (sub >= 11 && sub <= 16 && c.H(278) == 1) return;
    c.SetH(288, 0);
    c.SetH(290, 0);
    c.SetH(272, 12);
    c.SetH(274, 0);
    c.SetB(286, 0);
    c.SetH(292, 0);
    c.SetW(300, 0);
    c.SetB(0, 0);
    c.SetW(584, 2);
    c.SetH(12, 0);
    c.SetH(598, 0);
    c.SetH(594, 0);
    k.Call(__imp__sub_821E0160, c.a, 5);
    c.SetH(312, 0);
    return;
  }
  const uint32_t cls = c.B(164);
  if (cls != 255 && one_button() && c.B(170) == 0 && (c.W(400) & 0x100) != 0 &&
      (c.B(4494) & 4) == 0) {
    const uint8_t a = c.B(178), b = c.B(179);
    if ((c.W(400) & 0x3F) != 0 && a != 255 && b != 255) {
      c.SetH(274, 0);
      c.SetB(286, 0);
      c.SetH(272, 2);
      c.SetH(290, 0);
      const uint8_t unk_148 = c.B(148);
      c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, cls)))));
      c.SetW(588, uint32_t(int32_t(int16_t(Table(base, kMoveTable1, cls)))));
      c.SetH(20, Table(base, kMoveTable3, cls));
      c.SetH(16, Table(base, kMoveTable3, cls));
      c.SetH(14, 0);
      c.SetH(598, 0);
      c.SetH(594, 0);
      c.SetH(12, 0);
      c.SetH(292, 0);
      c.SetH(338, 0);
      c.SetH(4492, 0);
      c.SetH(592, 0);
      c.SetH(306, cls);
      c.SetW(300, 0);
      if (unk_148 != 0) c.SetB(336, 17);
      k.Call(__imp__sub_821E0160, c.a, 4);
      return;
    }
    c.SetH(290, 0);
    c.SetH(274, 0);
    c.SetH(272, 19);
    c.SetH(288, 0);
    c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, cls)))));
    c.SetW(588, uint32_t(int32_t(int16_t(Table(base, kMoveTable1, cls)))));
    c.SetH(12, Table(base, kMoveTable2, cls));
    c.SetH(598, 0);
    c.SetH(292, 0);
    c.SetB(286, 0);
    c.SetW(300, 0);
    c.SetH(20, Table(base, kMoveTable3, cls));
    k.Call(__imp__sub_821DD230, c.a, 8);
    const uint32_t direction = (uint32_t(c.H(8)) + c.H(20)) & 0xFFFF;
    const uint8_t latched = c.B(164);
    c.SetH(338, 0);
    c.SetH(14, 0);
    c.SetH(18, 0);
    c.SetH(4492, 0);
    c.SetH(592, 0);
    c.SetH(306, latched);
    c.SetH(16, direction);
    k.k.r4.u64 = 2;
    k.Call(__imp__sub_821E0160);  // r3 as sub_821DD230 left it
    return;
  }
  if (angle_ready(unk_172)) angle_move();
}

// The "b336" fix-up several substate paths end with.
void SettleB336(const Control& c, bool busy) {
  const uint8_t v = c.B(336);
  if (!busy) {
    if (v != 18) c.SetB(336, 0);
  } else if (v == 7 || v == 8) {
    c.SetB(336, 9);
  }
}

// sub_821EE610.
void FreeState(PPCContext& ctx, uint8_t* base) {
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 240;
  const uint32_t frame = k.k.r1.u32;
  const bool busy = *game::GuestPtr<be<int32_t>>(base, game_address + kGameBusy) != 0;
  const auto robot_data_float = [&](uint32_t offset) {
    const uint32_t data = game::LoadGuestAddress(base, game_address + game::kGameRobotData);
    return *game::GuestPtr<be<uint32_t>>(base, data + offset);
  };
  const auto relative_now = [&]() { return int32_t(int16_t(uint16_t(c.H(game::kControlRef2) - c.H(8)))); };

  c.SetB(337, 0);
  c.SetB(4494, 0);
  const uint32_t zero = *game::GuestPtr<be<uint32_t>>(base, 0x8200095C);
  c.SetW(376, zero);
  c.SetW(372, zero);

  if (c.H(312) != 0) {
    c.SetB(576, 0);
    c.SetB(577, 0);
    c.SetB(440, 0);
    c.SetH(438, 0);
    c.SetB(441, 0);
    c.SetB(4494, 253);
    if (k.Call(__imp__sub_821E1EA8, c.a) != 0) {
      c.SetW(584, 0);
      c.SetH(594, 0);
      c.SetH(598, 0);
      c.SetH(596, 0);
      c.SetH(312, 0);
    }
  } else if (c.B(2) == 10 && c.B(180) != 0 && c.H(452) == 0xFFFF) {
    // Compares two of this robot's ratios with the other robot's.
    k.k.fpscr.disableFlushMode();
    const Control other{base, c.W(72)};
    const auto single = [](int32_t v) { return double(float(double(int64_t(v)))); };
    const double mine = double(float(single(c.S16(80)) / single(c.S16(88))));
    const double theirs = double(float(single(other.S16(80)) / single(other.S16(88))));
    if (mine < theirs) {
      c.SetH(452, 0xFFFE);
      k.k.r3.s64 = c.S16(80);  // as the original's r3 is at the call
      if (c.B(1) == 0 && int8_t(*game::GuestPtr<uint8_t>(base, game_address + 24914)) > 0 &&
          k.Call(__imp__sub_821F9260) == c.a) {
        k.k.r4.u64 = 0x3204A9;
        k.k.r5.u64 = 0x8201EB94;
        if (c.a == game_address + kControl2) {
          const uint32_t r = k.Call(__imp__sub_821A5C20, frame + 96);
          k.k.r5.u64 = 256;
          k.Call(__imp__sub_821CFB90, frame + 128, r);
        } else {
          k.Call(__imp__sub_821A5C20, frame + 128);
        }
        k.Call(__imp__sub_822AAAC0);
      }
    }
  }

  const uint8_t robot = c.B(2);
  if (robot == 0 && c.B(180) > 240 && c.H(450) == 0 && c.B(3322) == 0) {
    c.SetH(290, 0);
    c.SetH(274, 0);
    c.SetH(272, 24);
    c.SetB(286, 0);
    c.SetH(292, 0);
    c.SetH(276, 0);
    c.SetB(3322, 1);
    c.SetH(312, 0);
    k.k.r4.u64 = 0xC17A9;
    k.k.r5.u64 = 0x82028088;
    const uint32_t r = k.Call(__imp__sub_821A5C20, frame + 128);
    k.Call(__imp__sub_821ED918, c.a, r);
    StartMoves(c, k, base, busy);
    return;
  }
  if (robot == 10 && c.B(178) > 240 && c.B(179) > 240 && c.B(169) == 255) {
    k.k.r6.u64 = 6;
    k.k.r5.u64 = 0;
    if (k.Call(__imp__sub_82257F50, c.a, 2) != 0 && (c.W(2192) & 1) == 0) {
      c.SetH(274, 0);
      c.SetB(286, 0);
      c.SetH(272, 24);
      c.SetH(292, 0);
      c.SetW(300, 0);
      c.SetH(312, 0);
      StartMoves(c, k, base, busy);
      return;
    }
  }
  if (c.H(312) != 0) {
    StartMoves(c, k, base, busy);
    return;
  }

  const uint32_t sub = c.H(274);
  const auto dash = [&]() { return k.Call(__imp__sub_821DD180, c.a, 1) != 0; };
  if (sub == 0 && (c.W(264) != 0 || c.B(576) == 5)) {
    c.SetB(4494, 0);
    c.SetH(312, 0);
    const int32_t turn = int32_t(c.W(264));
    bool reset = false;  // EA14
    const auto try_dash = [&]() {
      if (c.B(577) == 0 && dash()) {
        c.SetB(286, 1);
        c.SetH(292, 0);
      } else if (c.B(577) == 1 && dash()) {
        c.SetH(292, 0);
      }
    };
    const auto begin = [&](uint32_t side) {
      c.SetW(584, side);
      c.SetB(100, 8);
      c.SetB(576, 5);
      c.SetH(598, 0);
      c.SetH(594, 0);
      c.SetH(12, 0);
      reset = true;
    };
    if (turn > 0) {
      if (c.B(576) == 5 && c.W(584) == 0) try_dash(); else begin(0);
    } else if (turn < 0) {
      if (c.B(576) == 5 && c.W(584) == 1) try_dash(); else begin(1);
    } else if (c.B(577) != 2) {
      c.SetB(286, 2);
      c.SetH(292, 0);
    } else if (dash()) {
      c.SetB(576, 0);
      c.SetW(584, 2);
      reset = true;
    }
    if (reset) {
      c.SetB(577, 0);
      c.SetH(102, 0);
    }
    k.Call(__imp__sub_821E1930, c.a);
    if (!busy) {
      const uint32_t facing = c.H(8);
      const uint32_t amount = c.W(256);
      c.SetB(336, 0);
      c.SetH(8, amount + facing);
    } else {
      k.k.r6.u64 = 0;
      k.k.r5.u64 = uint32_t(int32_t(int16_t(uint16_t(c.W(256)))));
      k.Call(__imp__sub_821BFF48, c.a, uint32_t(int32_t(int16_t(uint16_t(c.W(268))))));
    }
    StartMoves(c, k, base, busy);
    return;
  }

  c.SetB(4494, 0);
  if (c.H(278) != 0 && c.W(264) != 0) {
    c.SetW(264, 0);
    c.SetW(256, 0);
  }
  c.SetW(584, 2);

  if (sub == 20 || sub == 19) {
    const uint32_t params = c.W(76);
    c.SetH(592, 0);
    uint32_t limits;
    if (sub == 20) {
      c.SetB(568, 28);
      c.SetB(576, 125);
      limits = params + 2104;
      c.SetW(372, robot_data_float(1560));
    } else {
      c.SetB(568, 27);
      c.SetB(576, 124);
      limits = params + 2114;
      c.SetW(372, robot_data_float(1556));
    }
    if (dash()) {
      const uint8_t v = c.B(577);
      if (v == 2) {
        for (uint32_t o : {272u, 288u, 290u, 274u}) c.SetH(o, 0);
        c.SetB(576, 0);
        c.SetB(577, 0);
        c.SetH(102, 0);
        c.SetH(294, 0);
        c.SetH(104, 0);
        c.SetW(584, 0);
        c.SetH(594, 0);
        c.SetH(598, 0);
        c.SetH(12, 0);
        c.SetH(4492, 0);
        c.SetW(112, 0);
        c.SetW(300, 0);
      } else if (v == 0) {
        c.SetB(286, 1);
      } else {
        c.SetB(286, 2);
      }
      c.SetH(292, 0);
      c.SetB(337, 0);
      c.SetB(4494, 0);
    }
    const uint8_t v = c.B(577);
    const int32_t t = c.S16(102);
    const auto limit = [&](uint32_t o) { return int32_t(Table(base, limits, o / 2)); };
    if (v == 0) {
      c.SetB(4494, 255);
    } else if (v == 1) {
      uint32_t flags = (limit(0) <= t && limit(2) > t) ? 1 : 0;
      c.SetB(337, flags);
      if (limit(4) <= t && limit(6) > t) c.SetB(337, flags | 2);
      c.SetB(4494, 255);
    } else {
      c.SetB(337, 0);
      c.SetB(4494, t < limit(8) ? 255 : 0);
    }
    StartMoves(c, k, base, busy);
    return;
  }

  if (sub == 17 || sub == 18) {
    c.SetH(104, 0);
    c.SetB(4494, 0);
    c.SetB(337, 0);
    c.SetB(336, 0);
    if (sub == 18) {
      c.SetB(576, 26);
      if (dash()) {
        const uint8_t v = c.B(577);
        if (v == 0) {
          c.SetB(286, 1);
        } else if (v == 1) {
          c.SetH(274, 17);
          c.SetB(286, 1);
        }
        c.SetH(292, 0);
      }
      StartMoves(c, k, base, busy);
      return;
    }
    const uint8_t state577 = c.B(577);
    c.SetB(576, 20);
    bool go_on = true;  // to F044
    if (state577 != 0) {
      uint32_t move = 0;
      if (c.B(167) >= 253) {
        move = 11;
      } else if (c.B(168) >= 253) {
        move = 13;
      }
      const int32_t kind = int32_t(*game::GuestPtr<be<uint32_t>>(base, 0x82027F10 + move * 4));
      bool ok = false;
      if (kind == 2) {
        ok = c.H(460) != 0;
      } else if (kind == 5) {
        ok = c.H(458) != 0;
      } else if (kind == 8) {
        ok = c.H(462) != 0;
      }
      if (!ok && move != 0) {
        c.SetH(274, move == 13 ? 20 : 19);
        c.SetB(286, 0);
        c.SetH(292, 0);
        const uint32_t f = c.B(148) != 0 ? 1 : 0;
        c.SetH(338, f);
        if (f != 0) c.SetB(336, 14);
        go_on = false;
      }
    }
    if (go_on) {
      const int32_t relative = relative_now();
      const bool clear = c.B(170) != 0 && (c.W(400) & 21) != 0 && relative < 20480 &&
                         relative > -20480 && c.B(168) == 0 && c.B(167) == 0 && c.B(169) == 0;
      if (clear || state577 == 2) {
        if (dash()) {
          const uint8_t v = c.B(577);
          if (v == 0) {
            c.SetB(286, 1);
          } else if (v == 2) {
            c.SetH(274, 0);
            c.SetB(286, 1);
          }
          c.SetH(292, 0);
        }
      } else {
        c.SetB(286, 2);
        c.SetH(292, 0);
      }
    }
    StartMoves(c, k, base, busy);
    return;
  }

  if (sub != 0) {
    if (sub >= 2 && sub <= 10) {
      k.Call(__imp__sub_821E0EA0, c.a);
      SettleB336(c, busy);
    } else if (sub >= 11 && sub <= 16) {
      c.SetW(264, 0);
      c.SetW(256, 0);
      k.Call(__imp__sub_821EDA98, c.a);
    }
    StartMoves(c, k, base, busy);
    return;
  }

  // Substate 0.
  c.SetB(4494, 0);
  c.SetH(104, 0);
  c.SetB(576, 0);
  c.SetB(577, 0);
  c.SetB(440, 0);
  c.SetH(438, 0);
  c.SetB(441, 0);
  bool clear_292 = true;  // EEC8 (else straight to EECC)
  const auto finish_ee10 = [&]() {
    c.SetH(4492, 0);
    c.SetH(592, 0);
    c.SetB(286, 0);
    c.SetH(104, 0);
    c.SetH(296, 0);
    c.SetH(312, 0);
  };
  if ((robot == 3 || robot == 13) && (busy || c.B(170) != 0) && c.B(180) == 255 &&
      c.B(3320) == 0) {
    c.SetH(274, 0);
    c.SetH(278, 1);
    c.SetH(272, 24);
    c.SetW(584, 0);
    c.SetH(276, 0);
    c.SetB(286, 0);
    c.SetH(4492, 0);
    c.SetH(312, 0);
    c.SetB(3320, 1);
  } else if (robot == 9 && c.B(170) != 0 && c.B(180) == 255) {
    c.SetH(274, 0);
    c.SetW(584, 0);
    c.SetH(272, 24);
    c.SetH(276, 0);
    c.SetB(286, 0);
    c.SetH(4492, 0);
    c.SetH(312, 0);
    c.SetH(320, 320);
  } else {
    k.k.r6.u64 = 0;
    k.k.r5.u64 = uint64_t(-1);
    const uint32_t s = k.Call(__imp__sub_821DD968, c.a, 1) & 0xFFFF;
    c.SetH(274, s);
    if (s == 0) {
      const uint8_t b170 = c.B(170);
      const int32_t relative = relative_now();
      if (b170 > 230 && b170 < 252 && (c.W(400) & 21) != 0 && relative < 20480 &&
          relative > -20480 && c.B(168) == 0 && c.B(167) == 0 && c.B(169) == 0) {
        c.SetH(274, 17);
        c.SetB(286, 0);
        c.SetH(598, 0);
        c.SetH(594, 0);
        c.SetW(584, 0);
        c.SetH(312, 0);
      } else if (!dash()) {
        clear_292 = false;
      }
    } else if (robot == 0 && s == 16) {
      c.SetH(272, 9);
      c.SetH(274, 0);
      c.SetW(584, 0);
      c.SetH(276, 0);
      c.SetB(286, 0);
      c.SetH(292, 0);
      c.SetH(4492, 0);
      finish_ee10();
    } else if (!busy && robot == 9 && c.S16(450) > 0 && s == 12 &&
               In(Control{base, c.W(72)}.H(274), {17, 18})) {
      const uint8_t unk_148 = c.B(148);
      c.SetH(274, 0);
      c.SetH(272, 25);
      c.SetB(286, 0);
      c.SetH(292, 0);
      c.SetH(276, 0);
      c.SetH(338, unk_148 != 0 ? 1 : 0);
      finish_ee10();
    } else if (In(robot, {1, 5, 16}) && (c.W(2192) & 0xF) == 15 && s == 9) {
      if (c.B(170) >= 252) {
        c.SetH(274, 0);
        c.SetW(584, 0);
        c.SetH(272, 25);
        c.SetH(276, 0);
        c.SetB(286, 0);
        c.SetH(292, 0);
        c.SetH(4492, 0);
      } else if (c.B(3321) == 0) {
        c.SetH(274, 0);
        c.SetW(584, 0);
        c.SetH(272, 24);
        c.SetH(276, 0);
        c.SetB(286, 0);
        c.SetH(292, 0);
        c.SetH(4492, 0);
        c.SetB(3321, 1);
      }
      finish_ee10();
    } else if (robot == 0 && c.S16(450) > 0) {
      finish_ee10();
    } else {
      static constexpr std::initializer_list<uint32_t> kMoving = {8, 0, 1, 16, 11};
      const bool other_flag = Control{base, c.W(72)}.B(4495) != 0;
      if (other_flag && ((In(s, {13, 14}) && In(robot, kMoving)) ||
                         (In(s, {11, 12}) && !In(robot, kMoving)))) {
        k.Call(__imp__sub_821DD810, c.a);
      } else if (In(s, {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15}) && c.B(170) >= 240) {
        c.SetH(278, 1);
        c.SetB(284, 4);
      }
      finish_ee10();
    }
  }
  if (clear_292) c.SetH(292, 0);
  {
    const uint8_t v = c.B(336);
    if (!busy) {
      if (v != 18) c.SetB(336, 0);
    } else if (v == 7 || v == 8) {
      c.SetB(336, 9);
    }
  }
  StartMoves(c, k, base, busy);
}

void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    const uint32_t generators = game::LoadGuestAddress(base, game::kRandomGenerators);
    const uint32_t random_state = game::LoadGuestAddress(base, generators + 16 * 1 + 4);
    verifier = new Verifier("sub_821EE610", base,
                            {{game_address, 0xD000, "game"},
                             {game::kRandomCalls, 4, "random calls"},
                             {random_state, game::kRandomStateSize, "random state"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821EE610(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        FreeState(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} sub {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(274), c.B(2));
      },
      600);
}

constexpr uint32_t kSideFlags = 0x83869E18;  // u32 per side, +16 set by state 19
constexpr uint32_t kSoundAjiDown = 0x82028104, kSoundAjiUp = 0x820280F8;  // sound names
constexpr uint32_t kState19Speed = 0x82010844;          // robot 5's speed in substates 11-16
constexpr uint32_t kState19NextSubstate = 0x82027FA0;   // u32, by sub_821DF440's result

void SetFloat(const Control& c, uint32_t o, double v) {
  *game::GuestPtr<be<uint32_t>>(c.base, c.a + o) = std::bit_cast<uint32_t>(float(v));
}

// Back to state 12 substate 5.
void ToState12(const Control& c, Caller& k) {
  c.SetH(288, 0);
  c.SetH(272, 12);
  c.SetH(290, 0);
  c.SetH(274, 0);
  c.SetB(286, 0);
  c.SetH(292, 0);
  c.SetW(300, 0);
  c.SetB(0, 0);
  c.SetW(584, 2);
  c.SetH(12, 0);
  c.SetH(598, 0);
  c.SetH(594, 0);
  k.Call(__imp__sub_821E0160, c.a, 5);
}

// Back to the free state.
void EndMove(const Control& c) {
  for (uint32_t o : {272u, 288u, 290u, 274u, 294u, 594u, 598u, 12u, 4492u, 292u}) c.SetH(o, 0);
  c.SetW(584, 0);
  c.SetW(112, 0);
}

void State19(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 176;
  const uint32_t frame = k.k.r1.u32;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  k.Call(__imp__sub_822AC098, 0);
  const auto game_word = [&](uint32_t o) { return *game::GuestPtr<be<uint32_t>>(base, game_address + o); };
  const uint32_t side = c.a != game_address + 25020 ? 1 : 0;
  auto flags = game::GuestPtr<be<uint32_t>>(base, kSideFlags + side * 4);
  *flags = uint32_t(*flags) | 16;
  const uint32_t sub = c.H(274);
  c.SetW(580, c.W(580) | 0x8000);
  if (sub == 0) {
    k.Call(__imp__sub_821F1BE0, c.a);
    return;
  }
  const uint32_t data = game_word(game::kGameRobotData);
  const uint32_t entry = c.W(588);
  const uint32_t params = c.W(76);
  const auto param = [&](uint32_t o) { return uint32_t(*game::GuestPtr<be<uint16_t>>(base, params + o)); };
  const auto speed = [&](uint32_t table) {
    k.k.r4.s64 = c.S16(20);
    k.Call(__imp__sub_821DD890, table);
    SetFloat(c, 372, k.k.f1.f64);
  };
  const auto group_offset = [&](uint32_t first) {
    const uint32_t s = c.H(274);
    return In(s, {2, 3, 4}) ? first : In(s, {5, 6, 7}) ? first + 20 : first + 40;
  };
  const auto sound = [&](uint32_t id, uint32_t table) {
    k.k.r4.u64 = id;
    k.k.r5.u64 = table;
    const uint32_t r = k.Call(__imp__sub_821A5C20, frame + 96);
    k.Call(__imp__sub_821ED918, c.a, r);
  };
  const auto finish = [&]() {
    c.SetB(286, 1);
    k.Call(__imp__sub_821E01E0, c.a);
    c.SetH(292, 0);
  };
  const auto hold = [&]() {
    c.SetH(292, 0);
    c.SetB(286, 2);
    c.SetB(284, 8);
  };
  // Another substate from sub_821DF200's +290 (the original's loc_821F2F94).
  const auto change_substate = [&](uint32_t next) {
    if (next != c.H(274)) {
      c.SetH(274, next);
      c.SetH(438, 0);
      c.SetH(296, 0);
      c.SetB(440, c.B(440) + 1);
      k.Call(__imp__sub_821E01E0, c.a);
    }
    c.SetH(290, 0);
    c.SetH(292, 0);
  };
  const auto from_df200 = [&]() {
    if (k.Call(__imp__sub_821DF200, c.a) != 0) {
      c.SetH(292, 0);
      return;
    }
    const uint32_t next = c.H(290);
    if (next != 0) {
      change_substate(next);
    } else if (c.H(288) == 12) {
      c.SetH(290, 0);
      c.SetH(274, 0);
      c.SetB(286, 0);
      c.SetH(272, 12);
      c.SetW(300, 0);
      c.SetB(0, 0);
      c.SetW(584, 2);
      c.SetH(12, 0);
      c.SetH(598, 0);
      c.SetH(594, 0);
      c.SetH(312, 8);
      c.SetB(284, 8);
      k.Call(__imp__sub_821E0160, c.a, 5);
      c.SetH(292, 0);
    } else {
      hold();
    }
  };
  const auto df440 = [&](uint32_t r6) {
    k.k.r6.u64 = r6;
    k.k.r5.u64 = 1;
    return int32_t(k.Call(__imp__sub_821DF440, c.a, 1));
  };
  const auto may_end_to_12 = [&]() { return c.B(172) == 255 && (c.B(4494) & 2) == 0; };
  const auto ajim_sound = [&]() { return c.B(2) == 15 && c.H(274) == 8 && (entry == 0 || entry == 3); };

  if (c.H(278) == 1) {
    k.Call(__imp__sub_821DD230, c.a, 32);
    k.k.r4.u64 = 0x38393A;
    k.k.r5.u64 = data + entry * 48 + 3028;
    k.Call(__imp__sub_821DFC50);  // r3 as sub_821DD230 left it
    speed(data + group_offset(1104));
    k.Call(__imp__sub_821DF990, c.a, data + entry * 48 + 3028);
    const uint8_t mode = c.B(577);
    if (mode == 1) {
      const uint32_t limit = param(3640);
      if (int32_t(c.W(416)) >= int32_t(limit)) {
        c.SetW(416, limit);
        c.SetB(286, 2);
        c.SetH(292, 0);
        return;
      }
      if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
        from_df200();
      } else if (c.B(2) == 17) {
        const int32_t r = df440(1);
        if (r != 0) c.SetH(290, uint32_t(r));
      }
    } else if (mode == 0) {
      if (k.Call(__imp__sub_821DD180, c.a, 1) == 0) return;
      if (ajim_sound() && c.H(454) != 0) {
        c.SetH(454, c.H(454) - 1);
        sound(0x911A9, kSoundAjiDown);
        return;
      }
      const uint32_t s = c.H(274);
      const int32_t kind = s == 8 ? 2 : s == 2 ? 1 : s == 5 ? 0 : -1;
      k.k.r6.u64 = 2;
      k.k.r5.u64 = 0;
      if (kind >= 0 && k.Call(__imp__sub_82257F50, c.a, uint32_t(kind)) != 0) {
        c.SetB(286, 1);
        c.SetH(292, 0);
        k.Call(__imp__sub_821E01E0, c.a);
        return;
      }
      c.SetB(286, 2);
      c.SetH(292, 0);
    } else {
      if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
        const uint32_t s = c.H(274);
        c.SetH(312, s == 5 ? param(entry * 8 + 3556) : s == 2 ? param(entry * 8 + 3558)
                                                       : param(entry * 8 + 3560));
        EndMove(c);
        c.SetB(576, 0);
        c.SetB(577, 0);
        c.SetH(102, 0);
        c.SetH(104, 0);
        c.SetW(300, 0);
      } else if (may_end_to_12()) {
        ToState12(c, k);
      }
    }
    return;
  }

  if (sub >= 11 && sub <= 16) {
    if (c.B(336) != 8) c.SetB(336, 7);
    uint32_t window;
    if (sub == 11) {
      c.SetB(568, 4);
      k.k.r4.s64 = c.S16(20);
      k.Call(__imp__sub_821DD890, data + 984);
      window = params + 2024;
      c.SetB(576, 120);
      SetFloat(c, 372, k.k.f1.f64);
    } else if (sub == 13) {
      c.SetB(568, 11);
      k.k.r4.s64 = c.S16(20);
      k.Call(__imp__sub_821DD890, data + 1004);
      window = params + 2014;
      c.SetB(576, 121);
      SetFloat(c, 372, k.k.f1.f64);
    } else if (c.B(2) == 5) {
      c.SetB(568, 30);
      window = params + 2154;
      c.SetB(576, 63);
      c.SetW(372, *game::GuestPtr<be<uint32_t>>(base, kState19Speed));
    } else {
      c.SetB(568, 16);
      k.k.r4.s64 = c.S16(20);
      k.Call(__imp__sub_821DD890, data + 1024);
      window = params + 2034;
      c.SetB(576, 122);
      SetFloat(c, 372, k.k.f1.f64);
    }
    const auto window_h = [&](uint32_t o) { return uint32_t(*game::GuestPtr<be<uint16_t>>(base, window + o)); };
    const uint8_t mode = c.B(577);
    if (mode == 2) {
      k.k.fpscr.disableFlushMode();
      const int32_t total = int32_t(c.W(612));
      const double whole = double(float(double(int64_t(total))));
      const double left = double(float(double(int64_t(int32_t(uint32_t(total) - uint32_t(c.S16(102)))))));
      const double now = double(std::bit_cast<float>(c.W(372)));
      SetFloat(c, 372, double(float(double(float(left * now)) / whole)));
    }
    const int32_t timer = c.S16(102);
    if (mode == 1) {
      uint32_t window_flags = int32_t(window_h(0)) <= timer && int32_t(window_h(2)) > timer ? 1 : 0;
      c.SetB(337, window_flags);
      if (int32_t(window_h(4)) <= timer && int32_t(window_h(6)) > timer) c.SetB(337, window_flags | 2);
      const uint32_t end = window_h(6);
      const uint64_t sum = uint64_t(uint32_t(timer) - end) + uint64_t(end ^ 0x80000000u);
      c.SetB(4494, (sum >> 32) != 0 ? 0 : 255);
    } else if (mode == 2) {
      const uint32_t t = uint32_t(timer), end = window_h(8);
      const uint32_t carry = t >= end ? 1 : 0;
      c.SetB(4494, (~(t >> 31) + carry) & 0xFF);
      c.SetB(337, 0);
    } else {
      c.SetB(4494, 0);
      c.SetB(337, 0);
    }
    if (k.Call(__imp__sub_821DD180, c.a, 1) == 0) return;
    const uint8_t mode2 = c.B(577);
    if (mode2 == 0) {
      finish();
    } else if (mode2 == 2) {
      const uint32_t s = c.H(274);
      c.SetH(312, param(s == 13 ? 3596 : s == 11 ? 3598 : 3600));
      EndMove(c);
      c.SetB(286, 0);
      c.SetB(284, 4);
    } else {
      c.SetH(292, 0);
      c.SetB(286, 2);
    }
    return;
  }

  const bool busy = game_word(kGameBusy) != 0;
  k.Call(__imp__sub_821DD230, c.a, busy ? 2048 : 16);
  k.k.r4.u64 = 0x28292A;
  k.k.r5.u64 = data + entry * 48 + 2548;
  k.Call(__imp__sub_821DFC50, c.a);
  speed(data + group_offset(984));
  k.Call(__imp__sub_821DF990, c.a, data + entry * 48 + 2548);
  if (game_word(kGameBusy) == 0 && (c.W(584) == 2 || c.W(584) == 3)) {
    if (c.B(577) == 0 || c.B(577) == 1) {
      int32_t d = int16_t(uint16_t(uint32_t(c.S16(game::kControlRef2) - c.S16(594)) - c.H(8)));
      if (d > 8192) d = 8192;
      if (d < -8192) d = -8192;
      c.SetH(598, uint32_t(d));
    } else {
      c.SetH(598, 0);
    }
  }
  const uint32_t over = k.Call(__imp__sub_821DD180, c.a, 1);
  const uint8_t mode = c.B(577);
  if (over != 0) {
    if (mode == 0) {
      if (ajim_sound() && c.H(456) <= 480) {
        c.SetH(456, c.H(456) == 0 ? 480 : 960);
        sound(0x811A9, kSoundAjiUp);
      }
      finish();
    } else if (mode == 2) {
      const uint32_t s = c.H(274);
      c.SetH(312, s == 5 ? param(entry * 8 + 3516) : s == 2 ? param(entry * 8 + 3518)
                                                         : param(entry * 8 + 3520));
      EndMove(c);
      c.SetB(576, 0);
      c.SetB(577, 0);
      c.SetH(102, 0);
      c.SetH(104, 0);
      if (game_word(kGameBusy) != 0) c.SetB(336, 9);
    } else {
      from_df200();
    }
    return;
  }
  if (mode == 1) {
    if (c.B(2) == 17) {
      const int32_t r = df440(1);
      if (r != 0) c.SetH(290, *game::GuestPtr<be<uint32_t>>(base, kState19NextSubstate + uint32_t(r) * 4));
    } else if (may_end_to_12()) {
      ToState12(c, k);
      c.SetH(312, param(entry * 8 + 3514));
    }
  } else if (mode == 2) {
    if (may_end_to_12()) {
      ToState12(c, k);
    } else if (c.B(2) == 17) {
      const int32_t r = df440(0);
      if (r == 0 || (r >= 11 && r <= 16)) return;
      if (uint32_t(r) != c.H(274)) {
        c.SetH(438, 0);
        c.SetH(274, uint32_t(r));
        c.SetH(290, 0);
        c.SetH(296, 0);
        c.SetB(440, c.B(440) + 1);
        k.Call(__imp__sub_821E01E0, c.a);
      }
      c.SetB(286, 1);
      c.SetH(292, 0);
    }
  }
}

constexpr uint32_t kSoundTemjinSurf = 0x8201ECE8, kSoundCypherMorph = 0x820280E8;  // sound names
constexpr uint32_t kState15Speed = 0x8200133C;  // float, substates 11-16

void State15(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 160;
  const uint32_t frame = k.k.r1.u32;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const uint32_t game_again = k.Call(__imp__sub_822AC098, 0);
  auto flags = game::GuestPtr<be<uint32_t>>(base, kSideFlags + (c.a != game_again + 25020 ? 4 : 0));
  *flags = uint32_t(*flags) | 16;
  const auto data = [&]() { return *game::GuestPtr<be<uint32_t>>(base, game_address + game::kGameRobotData); };
  const auto zero_float = [&]() { return *game::GuestPtr<be<uint32_t>>(base, 0x8200095C); };
  const auto negated_3700 = [&]() {
    SetFloat(c, 64, -double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, data() + 3700)))));
  };
  const auto sound = [&](uint32_t id, uint32_t table) {
    k.k.r4.u64 = id;
    k.k.r5.u64 = table;
    const uint32_t r = k.Call(__imp__sub_821A5C20, frame + 80);
    k.Call(__imp__sub_821ED918, c.a, r);
  };
  const auto df440 = [&](uint32_t r5, uint32_t r6) {
    k.k.r6.u64 = r6;
    k.k.r5.u64 = r5;
    return int32_t(k.Call(__imp__sub_821DF440, c.a, 1));
  };
  // Leaves this state for a special one (24-28).
  const auto to_special = [&](uint32_t state) {
    c.SetH(274, 0);
    c.SetB(286, 0);
    c.SetH(292, 0);
    c.SetH(272, state);
    c.SetH(438, 0);
    c.SetH(276, 0);
  };
  const auto change_substate = [&](uint32_t next) {
    if (next != c.H(274)) {
      c.SetH(438, 0);
      c.SetH(274, next);
      c.SetH(296, 0);
      c.SetB(440, c.B(440) + 1);
      k.Call(__imp__sub_821E01E0, c.a);
    }
  };
  const auto to_state12 = [&]() {
    c.SetH(272, 12);
    c.SetH(274, 0);
    c.SetH(290, 0);
    c.SetB(286, 1);
    c.SetB(284, 8);
    c.SetH(292, 0);
  };

  const uint8_t flag0 = c.B(0);
  c.SetW(580, c.W(580) | 0x8000);
  if (flag0 != 0 && c.H(274) == 0) {
    c.SetB(576, 18);
    c.SetB(577, 2);
    if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      const uint32_t w580 = c.W(580);
      for (uint32_t o : {272u, 290u, 274u, 292u, 598u, 594u, 12u}) c.SetH(o, 0);
      c.SetB(286, 0);
      c.SetW(300, 0);
      c.SetW(584, 0);
      c.SetW(580, w580 & ~0x04000000u);
    }
    c.SetW(372, zero_float());
  } else if (c.H(274) == 0) {
    // Substate 0: the first part of the state, which may start a special.
    k.k.r4.s64 = c.S16(20);
    k.Call(__imp__sub_821DD890, data() + 904);
    const uint32_t r4_after = k.k.r4.u32;  // the original adds the r4 the call left
    c.SetB(440, 0);
    c.SetH(438, 0);
    c.SetB(576, 18);
    SetFloat(c, 372, k.k.f1.f64);
    c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, uint32_t(c.S16(306)))))));
    c.SetH(16, c.H(8) + r4_after);
    if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      if (c.B(577) == 0) c.SetB(286, 1);
      c.SetH(292, 0);
    }
    const uint32_t limit = *game::GuestPtr<be<uint16_t>>(base, c.W(76) + 3640);
    if (int32_t(c.W(416)) >= int32_t(limit)) {
      c.SetW(416, limit);
      to_state12();
      c.SetW(300, 4);
      negated_3700();
      k.Call(__imp__sub_821E0160, c.a, 5);
    } else {
      // A class at +162 (while +188 is 0xFFFF) may pick another move from the tables.
      const uint8_t cls = c.B(162);
      bool new_move = false;
      int32_t move = 0;
      if (c.H(188) == 0xFFFF && cls != 255 && c.B(577) == 1) {
        move = int16_t(Table(base, kMoveTable0, cls));
        const int32_t current = int32_t(c.W(584));
        new_move = (current == 2 && move != 3) || (current == 3 && move != 2) ||
                   (current == 0 && move != 1) || (current == 1 && move != 0);
      }
      if (new_move) {
        c.SetW(584, uint32_t(move));
        c.SetW(588, uint32_t(int32_t(int16_t(Table(base, kMoveTable1, cls)))));
        c.SetH(12, Table(base, kMoveTable2, cls));
        c.SetH(306, cls);
        c.SetH(20, Table(base, kMoveTable3, cls));
        k.Call(__imp__sub_821DD230, c.a, 64);
        c.SetH(598, 0);
        c.SetH(292, 0);
        c.SetB(284, 4);
        c.SetH(16, c.H(8) + c.H(20));
      } else {
        c.SetB(441, 0);
        const int32_t command = df440(c.B(2) == 2 ? 1 : 0, 0);
        if (command != 0) {
          const uint8_t robot = c.B(2);
          const bool weapon0 = c.W(588) == 0;
          const bool gauges_full = c.W(404) == 0 && c.W(408) == 0 && c.W(412) == 0;
          if (robot == 7 && command == 8 && weapon0 && gauges_full) {
            to_special(24);  // Temjin's surfing ram
            c.SetW(64, zero_float());
            sound(0x3004A9, kSoundTemjinSurf);
          } else if (In(robot, {4, 11, 12}) && command == 8 && weapon0 && c.W(412) == 0) {
            to_special(24);  // the Apharmds
            c.SetW(64, zero_float());
            c.SetB(3321, c.B(3321) + 1);
          } else if (robot == 0 && weapon0 && command == 8 && c.W(412) == 0) {
            to_special(26);  // Dordray
            c.SetW(300, 4);
            c.SetW(64, zero_float());
          } else if (robot == 0 && weapon0 && command == 5 && c.W(404) == 0) {
            to_special(28);  // Dordray
            c.SetW(300, 4);
          } else if (robot == 2 && weapon0 && command == 8 && gauges_full) {
            to_special(25);  // Cypher's transformation
            c.SetW(64, zero_float());
            sound(0x719A9, kSoundCypherMorph);
          } else {
            if (robot == 15 && (c.W(588) == 0 || c.W(588) == 3) && (command == 15 || command == 8) &&
                c.H(454) < 4) {
              c.SetH(454, c.H(454) + 1);
              sound(0x811A9, kSoundAjiUp);
            }
            const uint32_t h304 = c.H(304);
            c.SetH(290, 0);
            c.SetH(274, uint32_t(command));
            c.SetB(286, 0);
            c.SetH(292, 0);
            c.SetH(296, 0);
            c.SetH(304, h304 | 4);
            if (command < 11 || command > 16) c.SetB(336, 7);
          }
        }
      }
    }
  } else if (c.H(274) >= 11 && c.H(274) <= 16) {
    c.SetW(372, *game::GuestPtr<be<uint32_t>>(base, kState15Speed));
    c.SetB(576, 61);
    c.SetB(568, 31);
    c.SetB(4494, 255);
    c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, uint32_t(c.S16(306)))))));
    c.SetB(337, c.B(577) == 0 ? 3 : 0);
    if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      const uint8_t mode = c.B(577);
      if (mode == 0) {
        c.SetB(286, 1);
        k.Call(__imp__sub_821E01E0, c.a);
        c.SetH(292, 0);
      } else {
        c.SetW(300, 4);
        if (mode == 2) {
          to_state12();
        } else {
          c.SetB(286, 2);
          negated_3700();
          c.SetH(292, 0);
        }
      }
    }
  } else {
    c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, uint32_t(c.S16(306)))))));
    k.Call(__imp__sub_821DD230, c.a, 128);
    const uint32_t entry = data() + c.W(588) * 48 + 2788;
    k.k.r4.u64 = 0x48494A;
    k.k.r5.u64 = entry;
    k.Call(__imp__sub_821DFC50);  // r3 as sub_821DD230 left it
    const uint32_t sub = c.H(274);
    k.k.r4.s64 = c.S16(20);
    k.Call(__imp__sub_821DD890, data() + (sub == 5 ? 1244 : sub == 2 ? 1224 : 1264));
    SetFloat(c, 372, k.k.f1.f64);
    const uint32_t over = k.Call(__imp__sub_821DD180, c.a, 1);
    const uint8_t mode = c.B(577);
    // 0: clear +290 and +292; 1: clear +292; 2: neither.
    int clear = 2;
    if (over != 0) {
      if (mode == 0) {
        c.SetB(286, 1);
        k.Call(__imp__sub_821E01E0, c.a);
        clear = 1;
      } else if (mode == 2) {
        c.SetH(274, 0);
        c.SetH(292, 0);
        c.SetH(272, 12);
        c.SetW(300, 4);
        c.SetB(286, 1);
        c.SetB(284, 8);
        clear = 0;
      } else if (k.Call(__imp__sub_821DF200, c.a) != 0) {
        clear = 1;
      } else if (const uint32_t next = c.H(290); next != 0) {
        if (next != c.H(274)) {
          c.SetB(440, c.B(440) + 1);
          c.SetH(438, 0);
          c.SetH(296, 0);
        }
        c.SetH(274, next);
        clear = 0;
      } else {
        c.SetB(286, 2);
        c.SetW(300, 4);
        negated_3700();
        clear = 1;
      }
    } else if (mode == 1) {
      if (c.B(2) == 2 && c.B(440) == 0) {
        const int32_t r = df440(1, 1);
        if (r != 0 && int32_t(c.H(290)) != r) c.SetH(290, uint32_t(r));
      }
    } else if (mode == 2) {
      c.SetB(441, 0);
      if (c.B(2) == 2 && c.B(440) == 0) {
        const int32_t r = df440(0, 0);
        if (r >= 2 && r <= 10) {
          change_substate(uint32_t(r));
          c.SetB(286, 1);
          clear = 0;
        }
      }
    }
    if (clear == 0) c.SetH(290, 0);
    if (clear <= 1) c.SetH(292, 0);
    k.Call(__imp__sub_821DF990, c.a, data() + c.W(588) * 48 + 2788);
  }

  // In substate 0 the levers' turn (+256) turns the facing and +16.
  if (c.H(274) == 0) {
    const int32_t turn = int32_t(c.W(256));
    const uint32_t rate = *game::GuestPtr<be<uint32_t>>(base, c.W(76) + 476);
    const int32_t step = turn > 0 ? int16_t(rate) : turn < 0 ? int16_t(-int32_t(rate)) : 0;
    c.SetH(8, c.H(8) + uint32_t(step));
    c.SetH(16, c.H(16) + uint32_t(step));
  }
}

void VerifyState15(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    verifier = new Verifier("sub_821F1228", base,
                            {{game_address, 0xD000, "game"}, {kSideFlags, 8, "side flags"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821F1228(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        State15(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} sub {} mode {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(274), c.B(577), c.B(2));
      },
      100);
}

void State18(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 128;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const auto data_h = [&](uint32_t o) {
    const uint32_t data = *game::GuestPtr<be<uint32_t>>(base, game_address + game::kGameRobotData);
    return uint32_t(*game::GuestPtr<be<uint16_t>>(base, data + o));
  };
  const auto data_float = [&](uint32_t o) {
    const uint32_t data = *game::GuestPtr<be<uint32_t>>(base, game_address + game::kGameRobotData);
    return double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, data + o))));
  };
  const uint32_t zero = *game::GuestPtr<be<uint32_t>>(base, 0x8200095C);
  const auto set_314_316 = [&](uint32_t v) {
    c.SetH(316, v);
    c.SetH(314, v);
  };
  const auto ask_dd968 = [&]() {
    k.k.r6.u64 = 0;
    k.k.r5.s64 = -1;
    const uint32_t r = k.Call(__imp__sub_821DD968, c.a, 1);
    if (int32_t(r) != 0) c.SetH(290, r);
  };

  const uint32_t sub = c.H(274);
  c.SetH(278, 0);
  c.SetW(116, 0);
  c.SetB(337, 0);
  set_314_316(3);
  c.SetH(444, uint32_t(-3));
  c.SetH(442, uint32_t(-3));
  c.SetH(10, 0);
  c.SetH(6, 0);

  if (sub == 1) {
    c.SetB(576, 19);
    c.SetW(584, (c.H(4) & 2) != 0 ? 1 : 0);
    if (c.S16(102) > 0 && k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      c.SetH(274, 0);
      c.SetH(292, 0);
      c.SetB(286, 2);
      c.SetH(288, 0);
    }
    return;
  }

  const bool mode2 = c.B(577) == 2;
  const uint32_t next_284 = mode2 ? 12 : 10;
  if (mode2) {
    if (c.B(4495) != 0) set_314_316(uint32_t(int16_t((Control{base, game_address}.W(588) & 3) + 3)));
    c.SetB(576, (c.H(4) & 4) != 0 ? 29 : 28);
    c.SetW(584, (c.H(4) & 2) != 0 ? 3 : 2);
    c.SetH(438, 0);
    c.SetB(441, 0);
    if (c.H(276) < 12) ask_dd968();
    if (c.H(276) != 0) c.SetH(276, c.H(276) - 1);
    if (c.B(181) != 0 && c.H(276) != 0) c.SetH(276, c.H(276) - 1);
    if (c.B(336) != 2) c.SetB(336, 2);
    if (c.B(172) == 255) {
      c.SetH(288, 12);
      c.SetH(290, 0);
    }
    if (c.H(276) != 0 || c.B(3) == 2) {
      c.SetW(372, zero);
      return;
    }
    c.SetB(4495, 0);
  } else {
    c.SetB(440, 0);
    c.SetH(438, 0);
    c.SetB(441, 0);
    ask_dd968();
    if (k.Call(__imp__sub_821DD180, c.a, 1) == 0) {
      c.SetW(372, zero);
      return;
    }
    set_314_316(data_h(3878));
  }

  const uint32_t next = c.H(290);
  if (next >= 11 && next <= 16) {
    c.SetH(274, next);
    for (uint32_t o : {272u, 288u, 290u, 294u, 292u, 598u, 594u, 12u, 4492u, 592u}) c.SetH(o, 0);
    c.SetB(286, 0);
    const uint32_t flag = c.B(148) != 0 ? 1 : 0;
    c.SetH(338, flag);
    c.SetW(584, 2);
    c.SetB(284, next_284);
    set_314_316(data_h(3874));
    c.SetB(336, flag != 0 ? 14 : 18);
    c.SetW(372, zero);
  } else if (next >= 2 && next <= 10) {
    c.SetH(274, next);
    for (uint32_t o : {272u, 288u, 290u, 294u, 292u, 598u, 594u, 12u, 4492u, 296u, 438u}) c.SetH(o, 0);
    c.SetB(286, 0);
    c.SetW(584, 2);
    c.SetW(372, zero);
    c.SetB(284, next_284);
    c.SetB(440, 0);
    set_314_316(data_h(3874));
    c.SetB(336, 18);
  } else if (c.B(174) != 255) {
    // To state 21; whether +174 is 0 picks the sign of +64 and the values 0 / 1.
    const uint8_t b174 = c.B(174);
    for (uint32_t o : {288u, 290u, 274u, 12u, 598u, 292u}) c.SetH(o, 0);
    c.SetH(272, 21);
    c.SetB(286, 0);
    if (b174 == 0) {
      c.SetW(588, 0);
      c.SetW(584, 0);
      c.SetW(300, 2);
      SetFloat(c, 64, data_float(3644));
    } else {
      c.SetW(588, 1);
      c.SetW(584, 1);
      c.SetW(300, 4);
      SetFloat(c, 64, -data_float(3644));
    }
    k.Call(__imp__sub_821DD230, c.a, 0);
    k.Call(__imp__sub_821C01D8);  // r3 as sub_821DD230 left it
    k.Call(__imp__sub_821E0160, c.a, 2);
    c.SetW(372, zero);
  } else if (c.B(164) != 255 && (c.W(400) & 256) != 0) {
    // To state 19 with the move for the class at +164.
    const uint8_t cls = c.B(164);
    c.SetH(290, 0);
    c.SetH(288, 0);
    c.SetH(272, 19);
    c.SetH(274, 0);
    c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, cls)))));
    c.SetW(588, uint32_t(int32_t(int16_t(Table(base, kMoveTable1, cls)))));
    c.SetH(12, Table(base, kMoveTable2, cls));
    c.SetH(598, 0);
    c.SetH(292, 0);
    c.SetB(286, 0);
    c.SetH(20, Table(base, kMoveTable3, cls));
    k.Call(__imp__sub_821DD230, c.a, 8);
    c.SetH(14, 0);
    c.SetH(18, 0);
    c.SetB(336, 18);
    c.SetH(16, c.H(game::kControlRef2) + c.H(20));
    k.Call(__imp__sub_821E0160, c.a, 2);
    c.SetW(372, zero);
  } else if (mode2) {
    const uint32_t s288 = c.H(288);
    c.SetH(292, 0);
    if (s288 == 12) {
      c.SetH(272, 12);
      for (uint32_t o : {288u, 290u, 274u, 12u, 598u, 594u}) c.SetH(o, 0);
      c.SetB(286, 0);
      c.SetW(300, 0);
      c.SetB(0, 0);
      c.SetW(584, 2);
      k.Call(__imp__sub_821E0160, c.a, 5);
      c.SetW(372, zero);
      c.SetB(336, 18);
      set_314_316(data_h(3874));
    } else {
      c.SetB(286, 3);
      c.SetB(336, 18);
      c.SetW(372, zero);
    }
  } else {
    const uint32_t s288 = c.H(288);
    for (uint32_t o : {288u, 290u, 274u, 292u, 12u, 598u, 594u}) c.SetH(o, 0);
    if (s288 == 12) {
      c.SetB(286, 0);
      c.SetW(300, 0);
      c.SetH(272, 12);
      c.SetB(0, 0);
      c.SetW(584, 2);
      k.Call(__imp__sub_821E0160, c.a, 5);
      c.SetB(336, 18);
      c.SetW(372, zero);
      set_314_316(data_h(3874));
    } else {
      // Back to the free state.
      for (uint32_t o : {272u, 102u, 294u, 104u, 4492u}) c.SetH(o, 0);
      c.SetB(576, 0);
      c.SetB(577, 0);
      c.SetW(584, 0);
      c.SetW(112, 0);
      c.SetB(336, 18);
      c.SetW(372, zero);
    }
  }
}

void VerifyState18(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821E4F88", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821E4F88(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        State18(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} sub {} mode {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(274), c.B(577), c.B(2));
      },
      600);
}

constexpr uint32_t kSubstateKinds = 0x82027F10;  // u32 per substate: 2, 5, 8 or other

// sub_821DD968(control, r4, mode, r6): the next substate, or 0. sub_821DD2B0
// fills a record on the stack (frame +80): +0 a substate chosen already,
// +4 passed on to sub_82257F50, +8 / +12 two signed values, +16..+36 six
// flags that pick a branch, +40 / +44 / +48 three parameter records with
// hold limits. Each branch tries up to three substates, each one only if
// sub_82257F50(control, weapon, variant, +4) agrees. A substate whose kind
// (kSubstateKinds) is 2, 5 or 8 is refused while +460, +458 or +462 runs.
uint32_t ChooseSubstate(const Control& c, Caller& k, uint8_t* base, uint32_t r4, int32_t mode, uint32_t r6) {
  const uint32_t frame = k.k.r1.u32 - 192;
  k.k.r1.u64 = frame;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  k.k.r7.u64 = frame + 80;
  k.k.r6.u64 = r6;
  k.k.r5.s64 = mode;
  k.Call(__imp__sub_821DD2B0, c.a, r4);
  const Control record{base, frame + 80};
  const auto rw = [&](uint32_t o) { return int32_t(record.W(o)); };
  const int32_t flag36 = rw(36), flag32 = rw(32), flag28 = rw(28);
  int32_t a, b;  // the +8 and +12 values, both 0 while the side's Game flag is set
  if ((c.a == game_address + kControl2 && g.B(53014) != 0) ||
      (c.a == game_address + 25020 && g.B(53013) != 0)) {
    a = 0;
    b = 0;
  } else {
    b = c.B(2) == 14 && (flag28 != 0 || flag32 != 0 || flag36 != 0) ? 0 : rw(12);
    a = rw(8);
  }
  const bool busy = int32_t(g.W(kGameBusy)) != 0;
  const int32_t timer = c.S16(438), t102 = c.S16(102);
  // Whether the hold timers allow a substate, by the record at +40 / +44 / +48.
  const auto allowed = [&](uint32_t record_offset, uint32_t hold, uint32_t limit102) {
    const Control p{base, record.W(record_offset)};
    if (mode < 0) return true;
    if (mode == 0) return timer < int32_t(p.H(hold));
    if (mode == 1) return timer < int32_t(p.H(hold)) && t102 < int32_t(p.H(limit102));
    return false;
  };
  const auto try_weapon = [&](uint32_t weapon, uint32_t variant, uint32_t substate) {
    k.k.r6.u64 = record.W(4);
    k.k.r5.u64 = variant;
    return int32_t(k.Call(__imp__sub_82257F50, c.a, weapon)) != 0 ? substate : 0u;
  };
  // A branch with three tries: a > 0 (variant 2), b > 0 (variant 1), both 0 (variant 0).
  const auto three_tries = [&](uint32_t weapon, uint32_t hold, uint32_t limit102, uint32_t s2, uint32_t s1,
                               uint32_t s0) -> uint32_t {
    if (a > 0 && allowed(48, hold, limit102)) return try_weapon(weapon, 2, s2);
    if (b > 0) return allowed(44, hold, limit102) ? try_weapon(weapon, 1, s1) : 0;
    if (b != 0 || a != 0) return 0;
    return allowed(40, hold, limit102) ? try_weapon(weapon, 0, s0) : 0;
  };

  uint32_t result = record.W(0);
  if (result == 0) {
    if (flag36 != 0) {
      if (a > 0 && allowed(48, 46, 40)) {
        result = try_weapon(2, 2, 10);
      } else if (b > 0) {
        if ((c.H(272) == 1 || c.H(272) == 0) && !busy) result = 16;
      } else if (b == 0 && a >= 0) {
        result = 15;
      }
    } else if (rw(24) != 0) {
      result = three_tries(2, 46, 40, 10, 9, 8);
    } else if (flag32 != 0) {
      if (a > 0 && allowed(48, 30, 24)) {
        result = try_weapon(1, 2, 4);
      } else if (b > 0) {
        if (!busy) result = 12;
      } else if (b == 0 && a == 0) {
        result = 11;
      }
    } else if (rw(20) != 0) {
      result = three_tries(1, 30, 24, 4, 3, 2);
    } else if (flag28 != 0) {
      if (a > 0 && allowed(48, 14, 8)) {
        result = try_weapon(0, 2, 7);
      } else if (b > 0) {
        if (!busy) result = 14;
      } else if (b == 0 && a == 0) {
        result = 13;
      }
    } else if (rw(16) != 0) {
      result = three_tries(0, 14, 8, 7, 6, 5);
    }
  }

  const int32_t kind = int32_t(*game::GuestPtr<be<uint32_t>>(base, kSubstateKinds + result * 4));
  if (kind == 2) return c.H(460) == 0 ? result : 0;
  if (kind == 5 && c.H(458) != 0) return 0;
  if (kind == 8 && c.H(462) != 0) return 0;
  return result;
}

void ChooseSubstateHook(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  const uint32_t r = ChooseSubstate(c, k, base, ctx.r4.u32, ctx.r5.s32, ctx.r6.u32);
  ctx.r3.u64 = r;
}

void VerifyChooseSubstate(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821DD968", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821DD968(original, base);
        return int64_t(original.r3.u32);
      },
      [&] {
        PPCContext rewrite = ctx;
        ChooseSubstateHook(rewrite, base);
        return int64_t(rewrite.r3.u32);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} state {} sub {} mode {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(272), c.H(274), ctx.r5.s32);
      },
      600);
}

void VerifyState19(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    const uint32_t generators = game::LoadGuestAddress(base, game::kRandomGenerators);
    const uint32_t random_state = game::LoadGuestAddress(base, generators + 16 * 1 + 4);
    verifier = new Verifier("sub_821F2538", base,
                            {{game_address, 0xD000, "game"},
                             {kSideFlags, 8, "side flags"},
                             {game::kRandomCalls, 4, "random calls"},
                             {random_state, game::kRandomStateSize, "random state"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821F2538(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        State19(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} sub {} b278 {} mode {} robot {}",
                           control == game_address + kControl2 ? 2 : 1, c.H(274), c.H(278), c.B(577),
                           c.B(2));
      },
      600);
}

constexpr uint32_t kStateTable = 0x82027C80;  // 32 x {handler, two words}
constexpr uint32_t kDriverFlag = 0x83858820;  // u8, set while side 1 runs an extra pass
constexpr uint32_t kZeroFloat = 0x8200095C;
constexpr uint32_t kHalf = 0x82001590;  // double 0.5
// Dordray's size scale: normal 1.0 (float); with +450 at 0 it shrinks by 0.1
// while above 1.1; in the last 30 ticks of +450 it is 1 + +450 / 15 (doubles).
constexpr uint32_t kNormalScale = 0x82000958, kShrinkAbove = 0x820224B8, kShrinkStep = 0x82017340;
constexpr uint32_t kScalePerTick = 0x820282F8, kScaleBase = 0x82001388;
constexpr uint32_t kCountAmount = 0x820176C8;  // float 0.3, passed to sub_8214F638
// Specineff's Death Mode sounds (names): start, end, the count every 50 ticks.
constexpr uint32_t kSoundDeathModeStart = 0x82028300, kSoundDeathModeEnd = 0x820282E4;
constexpr uint32_t kSoundDeathModeCount = 0x820282D0;

double GuestFloat(const uint8_t* base, uint32_t address) {
  return double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, address))));
}

double GuestDouble(const uint8_t* base, uint32_t address) {
  return std::bit_cast<double>(uint64_t(*game::GuestPtr<be<uint64_t>>(base, address)));
}

// u16 counter: one step toward 0 (+-1), as the original's adds of 0xFFFF / 1.
void CountToZero(const Control& c, uint32_t o) {
  const int32_t v = c.S16(o);
  if (v > 0) c.SetH(o, uint32_t(v - 1));
  else if (v < 0) c.SetH(o, uint32_t(v + 1));
}

void Driver(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 240;
  const uint32_t frame = k.k.r1.u32;
  const auto call = [&](void (*fn)(PPCContext&, uint8_t*)) { return k.Call(fn, c.a); };
  const uint32_t game_address = k.Call(sub_822AC098, 0);
  const uint32_t side_object = k.Call(sub_822AC098, 0) + (c.a == game_address + 25020 ? 0 : 192);
  const Control side{base, side_object};
  const Control g{base, game_address};
  const bool side1 = c.a == game_address + 25020, side2 = c.a == game_address + kControl2;
  // Side 2 builds the sound in `first` and copies it into the other buffer.
  const auto sound = [&](uint32_t id, uint32_t table, uint32_t first) {
    k.k.r4.u64 = id;
    k.k.r5.u64 = table;
    if (side2) {
      const uint32_t r = k.Call(sub_821A5C20, frame + first);
      k.k.r5.u64 = 256;
      k.Call(sub_821CFB90, frame + (first == 128 ? 160 : 128), r);
      k.Call(sub_822AAAC0);
      return true;
    }
    k.Call(sub_821A5C20, frame + 160);
    k.Call(sub_822AAAC0);
    return false;
  };

  g.SetW(game::kGameRobotData, c.W(76));
  const uint32_t other = c.W(72);
  k.k.fpscr.disableFlushMode();
  const double other_x = GuestFloat(base, other + 24);
  if (c.H(338) > 18) {
    const double half = GuestDouble(base, kHalf);
    const double y = GuestFloat(base, c.a + 4480), z = GuestFloat(base, c.a + 4484);
    SetFloat(c, 4476, double(float(double(float(other_x + GuestFloat(base, c.a + 4476))) * half)));
    SetFloat(c, 4480, double(float(double(float(GuestFloat(base, other + 28) + y)) * half)));
    SetFloat(c, 4484, double(float(double(float(GuestFloat(base, other + 32) + z)) * half)));
  } else {
    SetFloat(c, 4476, other_x);
    SetFloat(c, 4480, GuestFloat(base, other + 32));
    SetFloat(c, 4484, GuestFloat(base, other + 32));
  }
  k.k.r4.u64 = other;
  call(sub_821E8B98);
  if (c.B(1) != 3) {
    call(sub_821E0308);
    call(sub_821FF668);
  }
  k.k.r4.u64 = other;
  call(sub_821E9900);

  // Sets +340 = {2, 0} and copies an empty 24-byte name to +348, through the stack.
  *game::GuestPtr<uint8_t>(base, frame + 104) = 0;
  *game::GuestPtr<be<uint32_t>>(base, frame + 96) = 2;
  *game::GuestPtr<be<uint32_t>>(base, frame + 100) = 0;
  c.SetW(340, 2);
  c.SetW(344, 0);
  k.k.r5.u64 = 24;
  k.k.r4.u64 = 0;
  const uint32_t end = k.Call(sub_82561440, frame + 104);
  k.k.r6.u64 = end != 0 ? end - (frame + 104) : 24;
  k.k.r5.u64 = 24;
  k.k.r4.u64 = frame + 104;
  k.Call(sub_8216D160, c.a + 348);

  c.SetW(580, c.W(580) & ~0x280000u);
  const uint32_t game_again = k.Call(sub_822AC098, 0);
  *game::GuestPtr<be<uint32_t>>(base, kSideFlags + (c.a != game_again + 25020 ? 4 : 0)) = 0;

  const auto timer_end = [&](uint32_t timer, uint32_t hold, uint32_t length, uint32_t at_end,
                             uint32_t near_end) {
    const int32_t t = c.S16(timer);
    if (t == 0 || int8_t(c.B(hold)) > 0) return;
    const int32_t n = int32_t(c.W(length));
    if (t >= n - 1) {
      c.SetH(timer, uint32_t(n - 1));
      k.k.r4.u64 = at_end;
    } else if (t >= n - 2) {
      k.k.r4.u64 = near_end;
    } else {
      return;
    }
    call(sub_821DD118);
  };
  timer_end(102, 100, 612, 1, 4);
  timer_end(104, 101, 628, 2, 8);

  const uint32_t state = c.H(272);
  c.SetB(569, 0);
  const double zero = GuestFloat(base, kZeroFloat);
  SetFloat(c, 376, zero);
  SetFloat(c, 372, zero);
  if (state != 0 && state != 1) c.SetB(4494, 0);
  const uint32_t robot = c.B(2);
  const int32_t cooldown = int32_t(c.W(116));
  if (In(robot, {0, 6, 3, 13, 4, 12})) {
    c.SetW(116, cooldown > 1 ? uint32_t(cooldown - 2) : 0);
  } else if (cooldown > 0) {
    c.SetW(116, uint32_t(cooldown - 1));
  }
  if (c.S16(448) < 0) c.SetH(448, uint32_t(c.S16(448) + 1));

  if (robot == 10 && c.H(452) == 0xFFFE) {
    if (c.B(180) == 255) {
      if (c.B(1) == 0 && k.Call(sub_821F9260) == c.a) sound(0xC1DA9, kSoundDeathModeStart, 128);
      c.SetH(452, 680);
    } else if (state != 0 || c.H(274) != 0) {
      c.SetH(452, 0xFFFF);
    }
  }

  if (c.H(272) < 32) {
    const uint32_t handler = *game::GuestPtr<be<uint32_t>>(base, kStateTable + c.H(272) * 12);
    k.k.ctr.u64 = handler;
    k.k.r3.u64 = c.a;
    rex::runtime::ResolveIndirectFunction(handler)(k.k, base);
  }

  const uint8_t turn_mode = c.B(569);
  if (turn_mode == 0) {
    const int32_t now = c.S16(600);
    int32_t d = int16_t(uint16_t(uint32_t(c.H(598)) - uint32_t(now)));
    if (d > 2560) d = 2560;
    if (d < -2560) d = -2560;
    c.SetH(600, uint32_t(d + now));
  } else if (turn_mode == 1) {
    const uint32_t target = c.H(game::kControlRef2);
    c.SetH(600, 0);
    c.SetH(598, target);
    if (int16_t(target) > 2560) c.SetH(598, 2560);
    else if (int16_t(target) < -2560) c.SetH(598, uint32_t(-2560));
    c.SetB(569, 0);
  } else if (turn_mode == 2) {
    c.SetH(598, 0);
    c.SetH(600, 0);
    c.SetB(569, 0);
  }

  if (c.B(1) == 0) {
    const int32_t v = side.S16(100);
    const auto settle = [&]() {
      if (v > 0) side.SetH(100, v <= 256 ? 0 : uint32_t(v - 256));
      else if (v < 0) side.SetH(100, v >= -256 ? 0 : uint32_t(v + 256));
    };
    if (c.H(272) == 5) {
      settle();
    } else {
      const int32_t w = int32_t(c.W(256));
      if (w > 0) {
        if (v < 1536) side.SetH(100, uint32_t(v + 256));
      } else if (w < 0) {
        if (v > -1536) side.SetH(100, uint32_t(v - 256));
      } else {
        settle();
      }
    }
    const uint32_t a = c.H(276), st = c.H(272);
    uint32_t value = 0;
    if (a == 4 && st == 3) value = 36;
    else if (a == 8 && st == 5) value = 48;
    else if (a == 2 && st == 16 && c.B(577) == 1) value = 48;
    else if (a == 8 && st == 8) value = 48;
    else if (a == 116 && st == 10) value = 64;
    if (value != 0) side.SetH(140, value);
  }

  call(int32_t(g.W(kGameBusy)) != 0 ? sub_82242528 : sub_82241FB0);
  if (int8_t(k.Call(sub_822A6B70)) != 4) call(sub_8224FE78);
  const bool second_pass = In(c.B(2), {17, 18}) || g.W(24920) != 0 ||
                           (c.S16(314) < 4 && c.S16(316) < 4) || (g.W(588) & 1) != 0;
  if (second_pass) {
    if (int8_t(k.Call(sub_822A6B70)) != 4) {
      call(sub_822518C8);
    } else if (side1) {
      *game::GuestPtr<uint8_t>(base, kDriverFlag) = 1;
      call(sub_8224FE78);
      call(sub_822518C8);
      *game::GuestPtr<uint8_t>(base, kDriverFlag) = 0;
      call(sub_8224FE78);
      call(sub_822518C8);
    }
  }
  if (int8_t(k.Call(sub_822A6B70)) != 4 || side1) {
    call(sub_821B3AF8);
    call(sub_821FC8C0);
  }

  if (c.B(1) != 3) {
    call(int32_t(g.W(kGameBusy)) != 0 ? sub_821F75E8 : sub_821F6A10);
    call(sub_8214EED8);
    if (c.H(156) != 0) c.SetH(156, c.H(156) - 1);
    if ((c.H(156) & 7) == 0) {
      const int32_t hold = int8_t(c.B(100));
      const int32_t v = c.S16(436);
      if (hold > 0) {
        c.SetB(100, uint32_t(hold - 1));
        c.SetH(436, uint32_t(v < 0 ? v : -v));
      } else {
        const int32_t t = int16_t(uint16_t(c.H(102) + 1));
        c.SetB(100, 0);
        c.SetH(102, uint32_t(t));
        if (c.B(2) == 17 && c.S16(80) <= c.S16(88) / 2 && c.H(272) != 19 && c.H(272) != 30 &&
            t < int32_t(c.W(612)) - 1) {
          c.SetH(102, c.H(102) + 1);
        }
        c.SetH(436, uint32_t((v < 0 ? -v : v) + 1));
      }
    }
    CountToZero(c, 442);
    CountToZero(c, 444);

    const uint32_t robot2 = c.B(2);
    if (robot2 == 0 && c.H(450) == 0) {
      const double y = GuestFloat(base, c.a + 48), threshold = GuestFloat(base, kNormalScale);
      if (y > threshold && c.H(272) != 24) {
        if (y > GuestDouble(base, kShrinkAbove)) {
          SetFloat(c, 48, double(float(y - GuestDouble(base, kShrinkStep))));
        } else {
          SetFloat(c, 48, threshold);
          const uint32_t params = c.W(76);
          c.SetH(450, uint32_t(-480));
          const auto param = [&](uint32_t o) { return uint32_t(*game::GuestPtr<be<uint16_t>>(base, params + o)); };
          c.SetW(408, param(3508));
          c.SetW(404, param(3510));
          c.SetW(412, param(3512));
        }
      }
    }
    const int32_t h450 = c.S16(450);
    if (h450 > 0 && h450 < 32767) {
      const int32_t n = int16_t(uint16_t(h450 - 1));
      c.SetH(450, uint32_t(n));
      if (robot2 == 0) {
        if (n < 30) {
          const double x = double(int64_t(c.S16(450)));
          SetFloat(c, 48, double(float(std::fma(x, GuestDouble(base, kScalePerTick), GuestDouble(base, kScaleBase)))));
        }
      } else if (n == 0 && In(robot2, {4, 11, 12})) {
        c.SetH(450, uint32_t(-600));
      }
    }

    if (robot2 == 10 && c.H(452) != 0 && c.H(452) < 0xFFFE) {
      const uint32_t n = (c.H(452) + 0xFFFF) & 0xFFFF;
      c.SetH(452, n);
      if (n == 0) {
        c.SetH(82, 0);
        c.SetH(80, 0);
        if (c.B(1) == 0 && int8_t(g.B(24914)) > 0 && k.Call(sub_821F9260) == c.a) sound(0xE1DA9, kSoundDeathModeEnd, 160);
      }
      if (c.B(1) == 0 && int8_t(g.B(24914)) > 0 && c.H(452) % 50 == 0 && k.Call(sub_821F9260) == c.a) {
        k.k.r3.u64 = sound(0xD1DA9, kSoundDeathModeCount, 160) ? 1 : 0;
        k.k.r6.u64 = 4;
        k.k.r5.u64 = 10;
        k.k.f1.f64 = GuestFloat(base, kCountAmount);
        k.Call(sub_8214F638);
      }
    }

    const auto blink = [&](uint32_t o) {
      if (c.H(o) == 0) return;
      const uint32_t n = (c.H(o) + 0xFFFF) & 0xFFFF;
      c.SetH(o, n);
      if (n > 3 && c.B(181) != 0) c.SetH(o, (n + 0xFFFC) & 0xFFFF);
    };
    blink(446);
    if (c.H(456) != 0) c.SetH(456, c.H(456) - 1);
    if (c.S16(318) > 0) c.SetH(318, uint32_t(c.S16(318) - 1));
    blink(458);
    blink(460);
    blink(462);
    if (c.H(450) >= 0x8000 && c.S16(450) > -32767) c.SetH(450, c.H(450) + 1);

    if (int8_t(c.B(286)) >= 0) c.SetB(577, c.B(286));
    if (c.S16(292) >= 0) c.SetH(102, c.H(292));
    if (c.S16(294) >= 0) c.SetH(104, c.H(294));
    if (int32_t(c.W(300)) >= 0) c.SetH(278, c.W(300));
    if (c.S16(296) >= 0) c.SetH(436, c.H(296));
    if (int8_t(c.B(284)) >= 0) c.SetB(100, c.B(284));
    if (int8_t(c.B(285)) >= 0) c.SetB(101, c.B(285));
  }
  if (c.S16(80) <= 0) c.SetB(337, 0);
}

void VerifyDriver(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    const uint32_t generators = game::LoadGuestAddress(base, game::kRandomGenerators);
    const uint32_t random_state = game::LoadGuestAddress(base, generators + 16 * 1 + 4);
    verifier = new Verifier("sub_821F8168", base,
                            {{game_address, 0xD000, "game"},
                             {kSideFlags, 8, "side flags"},
                             {kDriverFlag, 1, "driver flag"},
                             {game::kRandomCalls, 4, "random calls"},
                             {random_state, game::kRandomStateSize, "random state"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821F8168(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        Driver(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} state {} sub {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(272), c.H(274), c.B(2));
      },
      600);
}

// sub_821E1930: in the free state (and states 12 / 19's substate 0), picks
// the next substate with sub_821DD968 and stores it at +274; some robots'
// substates start a special instead: Ajim (15) state 24 (from a button held
// 253 ticks), robot 7 / Specineff (10) state 25, robot 9 state 25 while the
// other robot is in substate 17 / 18, Dordray (0) states 27 and 9, robot 10
// state 26. When the other robot's +4495 is set, some substate / robot pairs
// call sub_821DD810 instead. Returns 1 when a plain substate was taken.
uint32_t PickSubstate(const Control& c, Caller& k, uint8_t* base) {
  k.k.r1.u64 = k.k.r1.u32 - 128;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  c.SetB(440, 0);
  c.SetH(438, 0);
  c.SetB(441, 0);
  if (c.H(312) != 0) return 0;

  const int32_t w264 = int32_t(c.W(264));
  const bool w264_small = w264 != 0 && w264 > -9 && w264 < 9;
  const bool w264_negative = w264 < 0 && w264 > -9;
  const uint32_t robot = c.B(2);
  // Start state `state` with +274 = `sub`; the shared resets of every special.
  const auto special = [&](uint32_t state, uint32_t sub) {
    c.SetH(274, sub);
    c.SetH(276, 0);
    c.SetH(272, state);
    c.SetB(286, 0);
    c.SetH(292, 0);
    return 0u;
  };

  if (robot == 15 && w264_negative && c.H(278) == 0) {
    // Ajim: the first of +169 / +167 / +168 held 253 ticks picks a substate.
    uint32_t held = 0;
    if (c.B(169) >= 253) {
      held = 15;
    } else if (c.B(167) >= 253) {
      held = 11;
    } else if (c.B(168) >= 253) {
      held = 13;
    }
    const int32_t kind = int32_t(*game::GuestPtr<be<uint32_t>>(base, kSubstateKinds + held * 4));
    const bool refused = (kind == 2 && c.H(460) != 0) || (kind == 5 && c.H(458) != 0) ||
                         (kind == 8 && c.H(462) != 0);
    if (!refused && held == 15 && c.W(404) == 0 && c.W(408) == 0) {
      c.SetH(320, 500);
      return special(24, 0);
    }
  }

  k.k.r6.u64 = 0;
  k.k.r5.s64 = -1;
  const uint32_t next = k.Call(__imp__sub_821DD968, c.a, 1) & 0xFFFF;
  c.SetH(274, next);
  if (next == 0) return 0;
  const bool busy = int32_t(Control{base, game_address}.W(kGameBusy)) != 0;

  if (!busy && robot == 7 && w264_small && c.H(278) == 0 && (next == 8 || next == 15) && c.W(412) == 0) {
    c.SetH(320, 800);
    return special(25, w264 < 0 ? 1 : 0);
  }
  if (!busy && robot == 9 && c.S16(450) > 0 && next == 12 && In(Control{base, c.W(72)}.H(274), {17, 18})) {
    c.SetH(338, c.B(148) != 0 ? 1 : 0);
    return special(25, 0);
  }
  if (robot == 0 && c.S16(450) > 0) {
    c.SetH(272, 0);
    c.SetH(288, 0);
    for (uint32_t o : {438u, 296u, 104u, 292u}) c.SetH(o, 0);
    c.SetB(286, 0);
    return 0;
  }
  if (robot == 0) {
    if (!busy && w264_negative && c.H(278) == 0 && (next == 8 || next == 15) && c.W(412) == 0) {
      return special(27, 0);
    }
    if (next == 16) {
      c.SetW(584, 0);
      c.SetH(4492, 0);
      return special(9, 0);
    }
  }
  if (robot == 10 && w264_small && c.H(278) == 0) {
    k.k.r6.u64 = 6;
    k.k.r5.u64 = 0;
    if (k.Call(__imp__sub_82257F50, c.a, 0) != 0 && In(c.H(274), {5, 13})) {
      return special(25, int32_t(c.W(264)) < 0 ? 1 : 0);
    }
  }
  if (c.B(2) == 10 && w264_negative && c.H(278) == 0 && c.H(274) == 15) return special(26, 0);

  if (Control{base, c.W(72)}.B(4495) != 0) {
    const uint32_t sub = c.H(274);
    const bool listed = In(c.B(2), {8, 0, 1, 16, 11});
    if ((In(sub, {13, 14}) && listed) || (In(sub, {11, 12}) && !listed)) {
      k.Call(__imp__sub_821DD810, c.a);
      return 0;
    }
  }

  const uint32_t sub = c.H(274);
  c.SetH(272, 0);
  c.SetH(288, 0);
  if (In(sub, {2, 5, 8, 4, 7, 10, 3, 6, 9, 11, 13, 15}) && c.B(170) >= 240) {
    c.SetW(300, 1);
    c.SetB(284, 4);
  }
  for (uint32_t o : {4492u, 438u, 296u, 104u, 292u}) c.SetH(o, 0);
  c.SetB(286, 0);
  return 1;
}

void PickSubstateHook(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  ctx.r3.u64 = PickSubstate(c, k, base);
}

void VerifyPickSubstate(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821E1930", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821E1930(original, base);
        return int64_t(original.r3.u32);
      },
      [&] {
        PPCContext rewrite = ctx;
        PickSubstateHook(rewrite, base);
        return int64_t(rewrite.r3.u32);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} state {} sub {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(272), c.H(274), c.B(2));
      },
      600);
}

// State 12's end: while +278 is 2-4, +8 turns by +256's sign at the robot's
// parameter +468 (+472 while +178 or +179 is set).
void Turn(const Control& c) {
  const uint32_t s278 = c.H(278);
  if (s278 < 2 || s278 > 4) return;
  const Control params{c.base, c.W(76)};
  const int32_t rate = int32_t(params.W(c.B(178) == 0 && c.B(179) == 0 ? 468 : 472));
  const int32_t amount = int32_t(c.W(256));
  if (amount > 0) {
    c.SetH(8, c.H(8) + int16_t(rate));
  } else if (amount < 0) {
    c.SetH(8, c.H(8) + int16_t(-rate));
  } else {
    c.SetH(8, c.H(8));
  }
}

// sub_821F0288, state 12: in the air. Substate 1 is the rise (or state 24
// for robot 2 with full gauges and +304 bit 2), substates 2-10 an attack in
// the air (three groups by the parameters at +3268 / +3364 / +3316), 11-16 a
// dash in the air (+337 / +4494 from the timer windows at +2144 / +2134 /
// +2154 in the robot's parameters), and the rest by +577 / +278: the landing
// (with a sound, to state 13 or back to the ground), +577 == 3, the fall
// (+278 == 2: state 17, 15, 24-26 or a substate from sub_821DEE00) and
// +278 == 4 (vertical speed +64 from +172 / +170). Most paths move +28 by
// +64 times Game +24940; at the end Turn runs.
void State12(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  const uint32_t frame = ctx.r1.u32 - 192;
  k.k.r1.u64 = frame;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  const auto data = [&] { return g.W(game::kGameRobotData); };
  const auto data_word = [&](uint32_t o) { return Control{base, data()}.W(o); };
  const uint32_t zero = *game::GuestPtr<be<uint32_t>>(base, 0x8200095C);
  const auto speed = [&](uint32_t table) {
    k.k.r4.u64 = c.W(248);
    k.Call(__imp__sub_821DD890, table);
    SetFloat(c, 372, k.k.f1.f64);
  };
  const auto set_580 = [&] { c.SetW(580, c.W(580) | 0x40008000); };
  const auto fall = [&] {
    const float y = float(std::fma(double(std::bit_cast<float>(g.W(24940))), double(std::bit_cast<float>(c.W(64))),
                                   double(std::bit_cast<float>(c.W(28)))));
    c.SetW(28, std::bit_cast<uint32_t>(y));
  };
  const auto sound = [&](uint32_t id, uint32_t table) {
    k.k.r4.u64 = id;
    k.k.r5.u64 = table;
    const uint32_t r = k.Call(__imp__sub_821A5C20, frame + 96);
    k.Call(__imp__sub_821ED918, c.a, r);
  };
  const auto pick_next = [&] {
    k.k.r5.u64 = 0;
    return k.Call(__imp__sub_821DEE00, c.a, 1) & 0xFFFF;
  };
  // Robot 2's special (state 24): gauges full, +180 255 and +304 bit 2.
  const auto robot2_special = [&] {
    return c.B(2) == 2 && c.B(180) == 255 && c.W(404) == 0 && c.W(408) == 0 && c.W(412) == 0 &&
           (c.H(304) & 2) != 0 && (g.B(24916) != 0 || g.B(24909) != 5);
  };
  const auto to_state24 = [&] {
    c.SetH(274, 0);
    c.SetB(286, 0);
    c.SetH(292, 0);
    c.SetH(272, 24);
  };
  // A class move (state 15) from +164 while flag 256 is set.
  const auto class_move_ready = [&] {
    return c.B(164) != 255 && (c.W(400) & 0x100) != 0 && (c.H(304) & 1) == 0 &&
           !(c.a == game_address + kControl2 && g.B(53014) != 0) &&
           !(c.a == game_address + 25020 && g.B(53013) != 0) && c.B(2) != 14;
  };
  const auto class_move = [&](bool set_284) {
    const uint32_t cls = c.B(164);
    for (uint32_t o : {292u, 290u, 288u, 274u}) c.SetH(o, 0);
    c.SetB(286, 0);
    c.SetH(272, 15);
    c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, cls)))));
    c.SetW(588, uint32_t(int32_t(int16_t(Table(base, kMoveTable1, cls)))));
    c.SetH(12, Table(base, kMoveTable2, cls));
    c.SetH(20, Table(base, kMoveTable3, cls));
    c.SetH(8, c.H(4490));
    k.Call(__imp__sub_821DD230, c.a, 64);
    if (set_284) c.SetB(284, 8);
    c.SetB(336, 18);
    c.SetH(598, 0);
    c.SetH(14, 0);
    c.SetH(304, c.H(304) | 4);
    c.SetH(16, c.H(4490) + c.H(20));
    c.SetH(306, c.B(164));
    k.Call(__imp__sub_821E0160, c.a, 2);
  };

  c.SetH(338, 0);
  c.SetB(576, 16);
  c.SetB(337, 0);
  c.SetB(4494, 0);
  const uint32_t sub = c.H(274);
  if (c.H(312) != 0) {
    c.SetB(576, 0);
    c.SetB(577, 0);
  } else if (sub == 1) {
    // The rise.
    c.SetB(440, 0);
    c.SetH(438, 0);
    set_580();
    if (g.W(kGameBusy) != 0) {
      c.SetB(576, 17);
      speed(data() + 864);
      const bool done = k.Call(__imp__sub_821DD180, c.a, 1) != 0;
      const uint32_t b577 = c.B(577);
      if (done) {
        if (b577 == 0) {
          c.SetH(292, 0);
          c.SetB(286, 1);
        } else {
          if (b577 == 2) {
            c.SetH(274, 0);
            c.SetH(272, 12);
            c.SetW(264, 0);
            c.SetB(286, c.H(278) == 2 ? 1 : 2);
          }
          c.SetH(292, 0);
        }
      } else if (b577 == 1) {
        c.SetB(441, 0);
        const uint32_t next = pick_next();
        c.SetH(274, next);
        if (next != 0) {
          c.SetB(286, 0);
          c.SetH(292, 0);
          c.SetH(296, 0);
          c.SetH(304, c.H(304) | 1);
        } else {
          c.SetH(274, 1);
        }
      }
    } else {
      c.SetB(576, 16);
      speed(data() + 864);
      c.SetB(441, 0);
      if (robot2_special()) {
        to_state24();
        c.SetH(438, 0);
        c.SetH(276, 0);
        c.SetW(64, zero);
        sound(0x719A9, 0x820280E8);
      } else {
        const uint32_t next = pick_next();
        c.SetH(274, next);
        if (next != 0) {
          c.SetB(286, 0);
          c.SetH(292, 0);
          c.SetH(296, 0);
          c.SetH(438, 0);
          c.SetH(304, c.H(304) | 1);
        } else if (c.H(278) == 2) {
          c.SetB(577, 1);
          c.SetH(274, 1);
        } else {
          c.SetH(274, 0);
          c.SetB(577, 2);
        }
      }
    }
    if (class_move_ready()) class_move(true);
    fall();
  } else if (sub >= 2 && sub <= 10) {
    // An attack in the air.
    uint32_t id, params;
    if (In(sub, {4, 7, 10})) {
      id = 0x68696A, params = 3316;
    } else if (In(sub, {3, 6, 9})) {
      id = 0x58595A, params = 3364;
    } else {
      id = 0x404142, params = 3268;
    }
    k.k.r5.u64 = data() + params;
    k.Call(__imp__sub_821DFFA8, c.a, id);
    k.k.r4.u64 = data() + params;
    k.Call(__imp__sub_821DF990, c.a);
    const uint32_t s = c.H(274);
    uint32_t table = 864;
    if (In(s, {5, 7, 6})) {
      table = 1184;
    } else if (In(s, {2, 4, 3})) {
      table = 1164;
    } else if (In(s, {8, 10, 9})) {
      table = 1204;
    }
    speed(data() + table);
    set_580();
    if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      const uint32_t b577 = c.B(577);
      if (b577 == 0) {
        c.SetB(286, 1);
        k.Call(__imp__sub_821E01E0, c.a);
        c.SetH(292, 0);
      } else if (b577 == 2) {
        c.SetH(290, 0);
        c.SetH(274, 0);
        c.SetH(272, 12);
        c.SetW(300, 4);
        c.SetB(286, 1);
        c.SetH(292, 0);
      } else if (k.Call(__imp__sub_821DEC58, c.a) != 0) {
        c.SetH(292, 0);
      } else {
        const uint32_t next = c.H(290);
        if (next == 0 || next == 1) {
          c.SetB(286, 2);
          c.SetH(292, 0);
        } else {
          if (next != c.H(274)) {
            c.SetH(274, next);
            c.SetH(438, 0);
            c.SetH(296, 0);
            c.SetB(440, c.B(440) + 1);
            k.Call(__imp__sub_821E01E0, c.a);
          }
          c.SetH(292, 0);
          c.SetH(290, 1);
        }
      }
    }
  } else if (sub >= 11 && sub <= 16) {
    // A dash in the air.
    const uint32_t params = c.W(76);
    uint32_t window;
    if (sub <= 12) {
      c.SetB(576, 140);
      window = params + 2144;
    } else if (sub <= 14) {
      c.SetB(576, 141);
      window = params + 2134;
    } else {
      c.SetB(576, 142);
      window = params + 2154;
    }
    set_580();
    c.SetW(584, 2);
    if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      const uint32_t b577 = c.B(577);
      if (b577 == 0) {
        c.SetB(286, 1);
        k.Call(__imp__sub_821E01E0, c.a);
      } else if (b577 == 2) {
        c.SetH(290, 1);
        c.SetH(274, 1);
        c.SetH(272, 12);
        c.SetH(278, 4);
        c.SetB(286, 1);
      } else {
        c.SetB(286, 2);
      }
      c.SetH(292, 0);
    }
    if (c.B(577) == 1) {
      const Control w{base, window};
      const int32_t t = c.S16(102);
      uint32_t flag = int32_t(w.H(0)) <= t && int32_t(w.H(2)) > t ? 1 : 0;
      if (int32_t(w.H(4)) <= t && int32_t(w.H(6)) > t) flag |= 2;
      c.SetB(337, flag);
      c.SetB(4494, t >= int32_t(w.H(6)) ? 255 : 0);
    }
  } else if (c.B(577) == 0) {
    // Landing.
    const uint32_t next = k.Call(__imp__sub_821DF6E0, c.a);
    if (int32_t(next) != 0) c.SetH(290, next);
    c.SetH(274, 0);
    c.SetW(584, 2);
    c.SetH(304, c.H(304) & ~1u);
    if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      if (c.B(2) == 8 || c.B(2) == 9) {
        sound(0x1105A9, 0x820280D0);
      } else {
        sound(0x1005A9, 0x820280DC);
      }
      const uint32_t n = c.H(290);
      if (c.B(173) <= 7 && n >= 11 && n <= 16) {
        // A dash on landing: state 13.
        c.SetH(274, n);
        c.SetH(272, 13);
        c.SetH(278, 2);
        set_580();
        c.SetH(304, c.H(304) & ~6u);
        c.SetB(286, 0);
        c.SetH(292, 0);
        c.SetW(64, data_word(3656));
        c.SetH(14, 0);
        c.SetH(16, c.H(8));
        c.SetB(336, 14);
        k.Call(__imp__sub_821E0160, c.a, 5);
      } else {
        c.SetH(278, 2);
        c.SetH(292, 0);
        c.SetH(290, 0);
        c.SetB(286, 1);
        c.SetH(304, c.H(304) & ~6u);
        set_580();
        c.SetB(336, 3);
        c.SetW(64, data_word(3644));
        k.k.r4.u64 = 3;
        c.SetW(584, k.Call(__imp__sub_821DD1E0, 0));
        c.SetH(12, 0);
      }
    }
    c.SetW(372, zero);
  } else if (c.B(577) == 3) {
    c.SetH(278, 0);
    if (k.Call(__imp__sub_821E1930, c.a) != 0) {
      c.SetW(584, 0);
      for (uint32_t o : {594u, 598u, 596u}) c.SetH(o, 0);
    } else if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      for (uint32_t o : {272u, 290u, 274u}) c.SetH(o, 0);
      c.SetB(286, 0);
      c.SetH(292, 0);
      c.SetW(300, 0);
      c.SetW(580, c.W(580) & ~0x04000000u);
    }
    c.SetW(584, 0);
    for (uint32_t o : {598u, 594u, 12u}) c.SetH(o, 0);
    c.SetW(372, zero);
  } else if (c.H(278) == 2) {
    // The fall.
    const uint32_t next = pick_next();
    c.SetB(577, 1);
    if (c.B(170) != 0 && (c.H(304) & 4) == 0) {
      c.SetH(274, 0);
      c.SetH(272, 17);
      c.SetH(290, 0);
      c.SetB(286, 4);
      c.SetH(292, 0);
      c.SetW(580, (c.W(580) & ~0x04000000u) | 0x8000);
      c.SetW(64, data_word(3652) ^ 0x80000000u);
      k.Call(__imp__sub_821E0160, c.a, 6);
      Turn(c);
      return;
    }
    if (class_move_ready()) {
      class_move(false);
      Turn(c);
      return;
    }
    const uint32_t robot = c.B(2);
    c.SetH(438, 0);
    c.SetB(441, 0);
    if (robot2_special()) {
      to_state24();
      c.SetH(276, 0);
      c.SetW(64, zero);
      sound(0x719A9, 0x820280E8);
    } else if (next != 0) {
      c.SetH(304, c.H(304) | 1);
      const auto special = [&](uint32_t state) {
        c.SetH(274, 0);
        c.SetW(584, 0);
        c.SetH(272, state);
        c.SetH(276, 0);
        c.SetB(286, 0);
        c.SetH(292, 0);
        c.SetH(4492, 0);
      };
      if (In(robot, {4, 11, 12}) && next == 8 && (c.H(304) & 2) != 0 && c.B(3322) == 0 && c.H(450) == 0) {
        special(25);
      } else if (robot == 14 && g.W(kGameBusy) == 0 && c.H(190) == 0 && next == 9 && c.W(412) == 0) {
        c.SetB(441, 0);
        to_state24();
        c.SetW(64, zero);
        c.SetH(438, 0);
        c.SetH(276, 0);
        c.SetB(336, 0);
      } else if (In(robot, {1, 5, 16}) && (c.W(2192) & 0xF) == 15 && next == 9) {
        special(26);
      } else {
        c.SetH(290, next);
      }
    }
    if ((c.H(304) & 6) == 0 && c.B(172) == 255) {
      c.SetH(304, c.H(304) | 2);
      c.SetW(64, data_word(3644));
      c.SetH(292, 0);
      c.SetB(284, 6);
    }
    fall();
    speed(data() + 864);
  } else if (c.H(278) == 4) {
    c.SetB(577, 2);
    const uint32_t b172 = c.B(172);
    if (b172 > 240 && b172 <= 248) {
      const double up = std::fma(double(int32_t(248 - b172)), GuestDouble(base, 0x82017340),
                                 double(std::bit_cast<float>(c.W(64))));
      const float v = float(up);
      c.SetW(64, std::bit_cast<uint32_t>(v));
      if (double(v) > GuestDouble(base, 0x820280C8)) c.SetW(64, *game::GuestPtr<be<uint32_t>>(base, 0x820280C0));
    }
    if ((c.H(304) & 8) == 0 && c.B(170) > 240) {
      const double down = -std::fma(double(int32_t(256 - c.B(170))), GuestDouble(base, 0x820173B0),
                                    -double(std::bit_cast<float>(c.W(64))));
      c.SetW(64, std::bit_cast<uint32_t>(float(down)));
    }
    fall();
    speed(data() + 864);
  }
  Turn(c);
}

void VerifyState12(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821F0288", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821F0288(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        State12(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} sub {} b577 {} s278 {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(274), c.B(577), c.H(278), c.B(2));
      },
      600);
}


// sub_821E2328: substate 0 of the movement state sub_821E4D58 handles (its
// number in the state table is not checked yet). It sets the buttons' move
// (+576 1 or 4) through sub_821DD230 and the speed (+372) from the robot's
// data by +577 and +400, then asks sub_821DD968 for the next substate:
// robot 17 hands some of them to sub_822583A8, Dordray's 16 goes to state 9,
// 11-16 and 2-10 become the substate (or sub_821DD810 while the other robot's
// +4495 is set), and with no substate a robot standing still with +170 at
// 231-251 goes to substate 17. At the end +8 turns by +256 (through
// sub_821BFF48 while Game +24932 is set).
void MoveSubstate0(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 128;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  const auto data = [&] { return g.W(game::kGameRobotData); };
  const auto busy = [&] { return g.W(kGameBusy) != 0; };
  const auto speed = [&](uint32_t table) {
    k.k.r4.u64 = c.W(248);
    k.Call(__imp__sub_821DD890, data() + table);
    SetFloat(c, 372, k.k.f1.f64);
  };
  // While busy: the speed from `table` and +376 from the data by +174.
  const auto busy_speed = [&](uint32_t table) {
    speed(table);
    c.SetW(376, Control{base, data()}.W((c.B(174) + 351) * 4));
  };
  const auto ask = [&](int32_t r6) {
    k.k.r6.s64 = r6;
    k.k.r5.s64 = -1;
    return int32_t(k.Call(__imp__sub_821DD968, c.a, 1));
  };
  const auto robot17 = [&](int32_t r) {
    const auto hand = [&](uint32_t r4) {
      k.k.r6.u64 = 0;
      k.k.r5.u64 = 0;
      k.k.r4.u64 = r4;
      k.Call(__imp__sub_822583A8, c.a);
    };
    if (r == 2) {
      hand(1);
    } else if (r == 5) {
      hand(0);
    } else if (r == 8) {
      hand(2);
    }
  };
  const auto to_state9 = [&] {
    c.SetH(274, 0);
    c.SetH(276, 0);
    c.SetH(272, 9);
    c.SetH(4492, 0);
    c.SetW(584, 0);
    c.SetB(286, 0);
    c.SetH(292, 0);
  };
  // Whether sub_821DD810 runs instead (the other robot's +4495 set).
  const auto other_blocks = [&](int32_t r, uint32_t robot) {
    if (Control{base, c.W(72)}.B(4495) == 0) return false;
    const bool listed = In(robot, {8, 0, 1, 16, 11});
    return (In(uint32_t(r), {13, 14}) && listed) || (In(uint32_t(r), {11, 12}) && !listed);
  };
  const auto block = [&] {
    k.Call(__imp__sub_821DD810, c.a);
    c.SetH(288, 0);
  };
  const auto in_k = [](int32_t r) { return In(uint32_t(r), {2, 5, 8, 4, 7, 10, 3, 6, 9, 11, 13, 15}); };
  // Dordray in his +450 mode: substate 13 / 11 straight away.
  const auto dordray_dash = [&](int32_t r) {
    c.SetH(272, 0);
    c.SetH(290, 0);
    c.SetB(577, 0);
    c.SetH(294, 0);
    c.SetW(584, 2);
    for (uint32_t o : {598u, 594u, 12u}) c.SetH(o, 0);
    c.SetB(4494, 255);
    c.SetH(296, 0);
    c.SetH(288, 0);
    c.SetH(274, uint32_t(r));
    c.SetH(278, 0);
    c.SetW(112, 0);
    c.SetH(292, 0);
  };
  // Substates 11-16.
  const auto dash = [&](int32_t r) {
    c.SetH(272, 0);
    c.SetH(290, 0);
    c.SetB(577, 0);
    c.SetH(292, 0);
    c.SetH(294, 0);
    c.SetW(584, 2);
    for (uint32_t o : {598u, 594u, 12u}) c.SetH(o, 0);
    c.SetB(4494, 255);
    c.SetH(296, 0);
    c.SetH(288, 0);
    c.SetH(274, uint32_t(r));
    c.SetH(4492, 0);
    c.SetH(592, 0);
  };
  // Substates 2-10 (9 and 10 become 8).
  const auto attack = [&](int32_t r) {
    const uint32_t n = r == 9 || r == 10 ? 8 : uint32_t(r);
    if (c.B(170) == 0) {
      c.SetH(278, 0);
    } else {
      c.SetW(300, 1);
      if (c.B(170) < 240) {
        c.SetH(272, 0);
        c.SetH(598, 0);
        c.SetW(584, 2);
        c.SetH(594, 0);
        c.SetH(12, 0);
        c.SetB(284, 4);
      }
    }
    c.SetH(290, 0);
    c.SetH(274, n);
    c.SetH(294, 0);
    c.SetH(296, 0);
    c.SetB(440, 0);
    c.SetH(438, 0);
    c.SetB(286, 0);
    c.SetH(292, 0);
  };
  // Standing still with +170 at 231-251: substate 17.
  const auto stand = [&](bool clear_290) {
    const uint32_t b170 = c.B(170);
    const int32_t turn = int16_t(c.H(4490) - c.H(8));
    if (b170 > 230 && b170 < 252 && (c.W(400) & 21) != 0 && turn < 20480 && turn > -20480 && c.B(168) == 0 &&
        c.B(167) == 0 && c.B(169) == 0) {
      if (clear_290) c.SetH(290, 0);
      c.SetH(272, 0);
      c.SetH(288, 0);
      c.SetH(274, 17);
      c.SetH(598, 0);
      c.SetH(594, 0);
      c.SetB(336, 0);
      c.SetW(112, 0);
      c.SetW(584, 0);
      c.SetB(286, 0);
      c.SetH(292, 0);
    }
  };
  // The move is over: back to the free state.
  const auto reset = [&] {
    for (uint32_t o : {272u, 288u, 290u, 274u}) c.SetH(o, 0);
    c.SetB(576, 0);
    c.SetB(577, 0);
  };
  const auto reset_rest = [&] {
    for (uint32_t o : {102u, 294u, 104u}) c.SetH(o, 0);
    c.SetW(584, 0);
    for (uint32_t o : {594u, 598u, 12u, 4492u}) c.SetH(o, 0);
    c.SetW(112, 0);
    c.SetW(264, 0);
  };

  c.SetB(4494, 0);
  c.SetH(104, 0);
  c.SetB(440, 0);
  c.SetH(438, 0);
  if (busy()) {
    c.SetB(576, 1);
    k.Call(__imp__sub_821DD230, c.a, 256);
  } else {
    const uint32_t b576 = c.B(576);
    const bool flags = (c.W(400) & 21) != 0;
    if (b576 == 1 && flags && c.W(584) < 2) {
      c.SetB(576, 4);
      c.SetB(100, 6);
      k.Call(__imp__sub_821DD230, c.a, 0);
      c.SetB(336, 10);
    } else if (b576 == 4 && flags) {
      k.Call(__imp__sub_821DD230, c.a, 0);
    } else {
      if (b576 == 4) c.SetB(100, 6);
      c.SetB(576, 1);
      k.Call(__imp__sub_821DD230, c.a, 1);
    }
  }
  const uint32_t b577 = c.B(577);
  c.SetH(598, 0);

  if (b577 == 0) {
    if ((c.W(400) & 63) != 0) {
      speed(764);
    } else if (busy()) {
      busy_speed(1284);
    } else {
      speed(744);
    }
    const uint32_t g2 = k.Call(__imp__sub_822AC098, 0);
    const uint32_t side = c.a != g2 + 25020 ? 1 : 0;
    if ((*game::GuestPtr<be<uint32_t>>(base, 0x83869E18 + side * 4) & 1) != 0) {
      c.SetB(286, 1);
      c.SetH(292, 0);
    } else if (const int32_t r = ask(-1); r == 0) {
      stand(false);
    } else {
      const uint32_t robot = c.B(2);
      if (robot == 17) {
        robot17(r);
      } else if (robot == 18) {
      } else if (robot == 0 && r == 16) {
        to_state9();
      } else if (robot == 0 && c.S16(450) > 0 && (r == 13 || r == 11)) {
        dordray_dash(r);
      } else if (r >= 11 && r <= 16) {
        if (other_blocks(r, robot)) {
          block();
          c.SetH(278, 0);
        } else {
          dash(r);
          c.SetW(300, in_k(r) && c.B(170) != 0 ? 1 : 0);
          c.SetW(112, 0);
        }
      } else if (r >= 2 && r <= 10) {
        attack(r);
      }
    }
  } else if (b577 == 2) {
    if (busy()) {
      busy_speed(1284);
    } else {
      speed(744);
    }
    if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      reset();
      reset_rest();
      c.SetH(278, 0);
      c.SetH(292, 0);
    } else {
      c.SetB(441, 0);
      const int32_t r = ask(0);
      const uint32_t robot = c.B(2);
      if (r == 0) {
        stand(false);
      } else if (robot == 17) {
        robot17(r);
      } else if (robot == 18) {
      } else if (robot == 0 && r == 16) {
        to_state9();
      } else if (other_blocks(r, robot)) {
        block();
        c.SetH(278, 0);
      } else {
        c.SetH(272, 0);
        c.SetH(290, 0);
        c.SetH(274, uint32_t(r));
        c.SetB(286, 0);
        c.SetH(292, 0);
        c.SetH(296, 0);
        c.SetW(584, 2);
        for (uint32_t o : {598u, 594u, 12u, 4492u, 592u}) c.SetH(o, 0);
        c.SetB(4494, 255);
        c.SetW(300, in_k(r) && c.B(170) != 0 ? 1 : 0);
        c.SetW(112, 0);
      }
    }
  } else {
    if (busy()) {
      busy_speed(1304);
    } else {
      speed(784);
    }
    if (k.Call(__imp__sub_821DD180, c.a, 1) != 0) {
      c.SetH(292, 0);
    } else {
      c.SetB(441, k.Call(__imp__sub_821DD180, c.a, 4) != 0 ? 1 : 0);
      const int32_t r = ask(-1);
      const uint32_t robot = c.B(2);
      if (r == 0) {
        stand(true);
      } else if (robot == 17) {
        robot17(r);
      } else if (robot == 18) {
      } else if (robot == 0 && r == 16) {
        to_state9();
      } else if (robot == 0 && c.S16(450) > 0 && (r == 13 || r == 11)) {
        dordray_dash(r);
      } else if (r >= 11 && r <= 16) {
        if (other_blocks(r, robot)) {
          block();
        } else {
          dash(r);
          c.SetW(112, 0);
          if (in_k(r) && c.B(170) >= 240) {
            c.SetW(300, 1);
            c.SetB(284, 4);
          } else {
            c.SetH(278, 0);
          }
        }
      } else if (r >= 2 && r <= 10) {
        if (c.B(577) == 2) {
          reset();
          c.SetH(292, 0);
          reset_rest();
        }
        attack(r);
      }
    }
  }

  // Turning.
  const uint32_t amount = c.W(256);
  if (!busy()) {
    c.SetB(4494, 0);
    c.SetH(8, amount + c.H(8));
  } else {
    k.k.r6.u64 = 0;
    k.k.r5.s64 = int16_t(amount);
    k.k.r4.s64 = int16_t(c.W(268));
    k.Call(__imp__sub_821BFF48, c.a);
    c.SetB(4494, 0);
  }
}

void VerifyMoveSubstate0(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821E2328", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821E2328(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        MoveSubstate0(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} b576 {} b577 {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.B(576), c.B(577), c.B(2));
      },
      600);
}


// sub_821E1FF0, run by sub_821E4D58 after its substate handler: a jump
// (state 12 substate 0) when +172 is 255, or a class move (state 19, the
// move tables indexed by +164) while flag 256 is set. Robot 6 doesn't jump
// out of substates 8-10. While Game +24932 is set only the class move is
// checked, without the +170 / +278 conditions.
void JumpOrClassMove(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 112;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const bool busy = Control{base, game_address}.W(kGameBusy) != 0;
  // State 19 with the move for the class at +164; returns the robot.
  const auto class_move = [&] {
    const uint32_t cls = c.B(164);
    const uint32_t robot = c.B(2);
    c.SetH(288, 0);
    c.SetH(290, 0);
    c.SetH(274, 0);
    c.SetH(272, 19);
    c.SetW(584, uint32_t(int32_t(int16_t(Table(base, kMoveTable0, cls)))));
    c.SetW(588, uint32_t(int32_t(int16_t(Table(base, kMoveTable1, cls)))));
    c.SetH(12, Table(base, kMoveTable2, cls));
    c.SetH(20, Table(base, kMoveTable3, cls));
    c.SetH(598, 0);
    c.SetH(292, 0);
    c.SetW(300, 0);
    return In(robot, {7, 8, 4, 11, 12}) && c.W(588) == 2;
  };

  if (!busy) {
    if (c.B(576) != 4) c.SetB(336, 0);
    if (c.B(172) == 255 && (c.B(4494) & 2) == 0) {
      if (c.B(2) == 6 && In(c.H(274), {8, 9, 10})) return;
      // The jump.
      for (uint32_t o : {288u, 290u}) c.SetH(o, 0);
      c.SetH(272, 12);
      c.SetH(274, 0);
      c.SetB(286, 0);
      c.SetH(292, 0);
      c.SetW(300, 0);
      c.SetB(0, 0);
      c.SetW(584, 2);
      for (uint32_t o : {12u, 598u, 594u}) c.SetH(o, 0);
      k.Call(__imp__sub_821E0160, c.a, 5);
      return;
    }
    if (c.B(164) == 255 || c.B(170) != 0 || (c.B(4494) & 4) != 0 || (c.W(400) & 0x100) == 0 || c.H(278) != 0) {
      return;
    }
    const bool still = class_move() || c.B(2) == 17;
    c.SetB(286, still ? 0 : 1);
    c.SetH(278, 0);
    k.Call(__imp__sub_821DD230, c.a, 8);
    c.SetB(284, 4);
    c.SetH(14, 0);
    c.SetH(18, 0);
    c.SetH(306, c.B(164));
    c.SetH(16, c.H(8) + c.H(20));
  } else {
    if (c.B(164) == 255 || (c.B(4494) & 4) != 0 || (c.W(400) & 0x100) == 0) return;
    c.SetB(286, class_move() ? 0 : 1);
    c.SetW(300, 0);
    const uint32_t r = k.Call(__imp__sub_821DD230, c.a, 1024);
    k.k.r4.u64 = c.H(20);
    k.Call(__imp__sub_821C01D8, r);
    c.SetB(284, 4);
    c.SetH(306, c.B(164));
  }
  k.Call(__imp__sub_821E0160, c.a, 2);
  c.SetB(336, 0);
}

void VerifyJumpOrClassMove(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821E1FF0", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821E1FF0(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        JumpOrClassMove(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} sub {} b172 {} b164 {} robot {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(274), c.B(172), c.B(164), c.B(2));
      },
      600);
}


// sub_821E4BC0: sub_821E4D58's other substates while +278 isn't 1. It sets
// the buttons' move through sub_821DD230 and the speed from the robot's data
// by substate group (2-4, 5-7, else substate 8), runs sub_821DF7B8 with the
// group's parameters (+2228 / +2212 / +2244) and a bit of the data's +114
// flags, then sub_821E3510 (substate 8), sub_821E39C0 (Game +24932 set) or
// sub_821E4088. The original passes registers from one call on to the
// next (sub_821DFDD8's result in r6), so they run through one context.
void MoveOtherSubstates(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 112;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  const auto data = [&] { return g.W(game::kGameRobotData); };
  const auto busy = [&] { return g.W(kGameBusy) != 0; };

  k.Call(__imp__sub_821DD230, c.a, busy() ? 512 : 2);
  k.k.r6.u64 = 38;
  k.k.r5.u64 = 37;
  k.k.r4.u64 = 36;
  k.k.r6.u64 = k.Call(__imp__sub_821DFDD8, c.a);
  const uint32_t sub = c.H(274);
  uint32_t flag = 0, params;
  const auto speed = [&](uint32_t table) {
    k.k.r4.u64 = c.W(248);
    k.Call(__imp__sub_821DD890, data() + table);
    SetFloat(c, 372, k.k.f1.f64);
  };
  if (In(sub, {2, 4, 3})) {
    speed(924);
    const uint32_t flags = Control{base, data()}.H(114);
    flag = busy() ? flags & 16 : flags & 4;
    params = c.W(76) + 2228;
  } else if (In(sub, {5, 7, 6})) {
    speed(944);
    const uint32_t flags = Control{base, data()}.H(114);
    flag = busy() ? flags & 8 : flags & 2;
    params = c.W(76) + 2212;
  } else {
    speed(964);
    c.SetH(274, 8);
    params = c.W(76) + 2244;
  }
  k.k.r4.u64 = flag;
  k.k.r5.u64 = params;
  k.Call(__imp__sub_821DF7B8, c.a);
  if (c.H(274) == 8) {
    k.k.r4.u64 = k.k.r6.u64;
    k.Call(__imp__sub_821E3510);
  } else {
    k.k.r5.u64 = k.k.r6.u64;
    k.k.r4.u64 = flag;
    k.Call(busy() ? __imp__sub_821E39C0 : __imp__sub_821E4088);
  }
}

void VerifyMoveOtherSubstates(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) verifier = new Verifier("sub_821E4BC0", base, {{game_address, 0xD000, "game"}});
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821E4BC0(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        MoveOtherSubstates(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} sub {} robot {}", control == game_address + kControl2 ? 2 : 1, c.H(274),
                           c.B(2));
      },
      10);
}


// sub_821DD2B0(control, r4, mode, r6, record): fills sub_821DD968's record.
// +8 / +12 from the hold timers +178 / +179 (held past 243: 1, held: -1,
// else 0; with r6 set and not (mode 0 and +441 clear): held 1), cleared in
// state 1; +4 is 2 while +278 is 1 (else 0) and +40 / +44 / +48 point at
// parameter records of the robot by +278 and state 1. The six flags: +20 /
// +16 / +24 from the hold timers +167 / +168 / +169 (held, or held 255 ticks,
// by r6 and +12), and while r4 is set (and +178 isn't held past 243)
// +32 / +28 / +36 from the same timers inside 250-253 / 253 when the range
// flags (+400) are set. While +441 is set in mode 0 with no substate
// waiting (+290), the current substate (2-10) is chosen again at +0 if its
// hold limit hasn't passed and sub_82257F50 agrees.
void FillChoiceRecord(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 112;
  const Control r{base, ctx.r7.u32};
  const int32_t mode = ctx.r5.s32;
  int32_t r6 = ctx.r6.s32;
  uint32_t use_ranges = ctx.r4.u32;
  r.SetW(0, 0);
  if (r6 == 0 || (mode == 0 && c.B(441) == 0)) {
    r6 = 0;
    const auto timer = [&](uint32_t o) -> uint32_t {
      const uint32_t t = c.B(o);
      return t > 243 ? 1 : t != 0 ? uint32_t(-1) : 0;
    };
    if (c.B(178) > 243) use_ranges = 0;
    r.SetW(8, timer(178));
    r.SetW(12, timer(179));
  } else {
    if (c.B(178) != 0) use_ranges = 0;
    r.SetW(8, c.B(178) != 0 ? 1 : 0);
    r.SetW(12, c.B(179) != 0 ? 1 : 0);
  }
  const Control p{base, c.W(76)};
  const bool state1 = c.H(272) == 1;
  if (state1) {
    use_ranges = ctx.r4.u32;
    r.SetW(12, 0);
    r.SetW(8, 0);
  }
  if (c.H(278) == 1) {
    r.SetW(40, p.a + (state1 ? 2308 : 2260));
    r.SetW(44, p.a + 2500);
    r.SetW(4, 2);
    r.SetW(48, p.a + 2404);
  } else {
    r.SetW(40, p.a + (state1 ? 2212 : 2164));
    r.SetW(44, p.a + 2452);
    r.SetW(4, 0);
    r.SetW(48, p.a + 2356);
  }
  for (uint32_t o : {24u, 20u, 16u, 36u, 32u, 28u}) r.SetW(o, 0);

  const auto held = [&] {
    if (c.B(167) != 0) r.SetW(20, 1);
    if (c.B(168) != 0) r.SetW(16, 1);
    if (c.B(169) != 0) r.SetW(24, 1);
  };
  const auto held_long = [&] {
    if (c.B(167) == 255) r.SetW(20, 1);
    if (c.B(168) == 255) r.SetW(16, 1);
    if (c.B(169) == 255) r.SetW(24, 1);
  };
  const bool quiet = mode >= 0 && c.B(441) == 0;
  if (r6 > 0) {
    if (!quiet) held();
  } else if (r6 == 0 || r.W(12) != 0) {
    held_long();
  } else if (!quiet) {
    held();
  }

  if (use_ranges != 0) {
    const uint32_t ranges = c.W(400);
    if (ranges & 3) {
      r.SetW(20, 0);
      if (c.B(167) >= 250 && c.B(167) < 254) r.SetW(32, 1);
    }
    if (ranges & 0xC) {
      r.SetW(16, 0);
      if (c.B(168) >= 250 && c.B(168) < 254) r.SetW(28, 1);
    }
    if (ranges & 0x30) {
      r.SetW(24, 0);
      if (c.B(169) == 253) r.SetW(36, 1);
    }
  }

  if (c.B(441) == 0 || mode != 0 || c.H(290) != 0) return;
  // The current substate again: its record, hold limit offset, weapon and variant.
  struct Again {
    uint32_t record, hold, weapon, variant;
  };
  Again a;
  switch (c.H(274)) {
    case 8: a = {40, 44, 2, 0}; break;
    case 9: a = {44, 44, 2, 1}; break;
    case 10: a = {48, 44, 2, 2}; break;
    case 2: a = {40, 28, 1, 0}; break;
    case 3: a = {44, 28, 1, 1}; break;
    case 4: a = {48, 28, 1, 2}; break;
    case 5: a = {40, 12, 0, 0}; break;
    case 6: a = {40, 12, 0, 1}; break;
    case 7: a = {40, 12, 0, 2}; break;
    default: return;
  }
  const uint32_t sub = c.H(274);
  if (int32_t(Control{base, r.W(a.record)}.H(a.hold)) <= c.S16(438)) return;
  k.k.r6.u64 = r.W(4);
  k.k.r5.u64 = a.variant;
  k.k.r4.u64 = a.weapon;
  if (k.Call(__imp__sub_82257F50, c.a) != 0) r.SetW(0, sub);
}

void VerifyFillChoiceRecord(PPCContext& ctx, uint8_t* base) {
  // The record is on the caller's stack, so each record address gets its own
  // verifier with the record declared.
  static std::map<uint32_t, Verifier*> verifiers;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  Verifier*& verifier = verifiers[ctx.r7.u32];
  if (!verifier) {
    verifier = new Verifier("sub_821DD2B0", base, {{game_address, 0xD000, "game"}, {ctx.r7.u32, 52, "record"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821DD2B0(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        FillChoiceRecord(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const Control c{base, control};
        return fmt::format("side {} state {} sub {} mode {} r6 {}", control == game_address + kControl2 ? 2 : 1,
                           c.H(272), c.H(274), ctx.r5.s32, ctx.r6.s32);
      },
      600);
}


// Small helpers the states share.

// sub_821DD180(control, mask): the side's motion flags (0x83869E18, one word
// per side; bit 1: the move is over) masked.
constexpr uint32_t kMotionFlags = 0x83869E18;

void MotionFlags(PPCContext& ctx, uint8_t* base) {
  const uint32_t game_address = game::LoadGuestAddress(base, game::kObjectTable);
  const uint32_t side = ctx.r3.u32 != game_address + 25020 ? 1 : 0;
  ctx.r3.u64 = *game::GuestPtr<be<uint32_t>>(base, kMotionFlags + side * 4) & ctx.r4.u32;
}

// sub_821DD230(control, mask): +594 from +12, turned by a quarter toward the
// nearest of 0 / 32768 unless the robot's parameter +112 has a bit of mask:
// 8193-24575 lose 16384, 24576-40960 lose 32768, 40961-57343 gain 16384.
void TurnTarget(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  const uint32_t a = c.H(12);
  uint32_t v = a;
  if ((Control{base, c.W(76)}.H(112) & ctx.r4.u32) == 0) {
    if (a > 8192 && a < 24576) {
      v = a - 16384;
    } else if (a >= 24576 && a <= 40960) {
      v = a - 32768;
    } else if (a > 40960 && a < 57344) {
      v = a + 16384;
    }
  }
  c.SetH(594, v);
}

// sub_821DD890(table, angle): a speed from five floats at table, by the
// angle folded to 0-32768, interpolated between the two values around it
// (one per 8192).
void SpeedByAngle(PPCContext& ctx, uint8_t* base) {
  const uint32_t table = ctx.r3.u32;
  int32_t a = int32_t(ctx.r4.u32 & 0xFFFF);
  if (a > 32768) a = 65536 - a;
  const uint32_t segment = a < 8192 ? 0 : a < 16384 ? 1 : a < 24576 ? 2 : 3;
  const int32_t frac = a - int32_t(segment) * 8192;
  const auto f = [&](uint32_t o) { return std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, o))); };
  const float low = f(table + segment * 4), high = f(table + segment * 4 + 4);
  const float step = (high - low) * float(double(frac));
  ctx.f1.f64 = double(float(std::fma(double(step), double(f(0x82027FEC)), double(low))));
}

// sub_821E0160(control, n): +112 grows by the robot's parameter n + 1911
// (n + 1924 while Game +24932 is set), not below 0.
void AddTo112(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 112;
  const uint32_t n = ctx.r4.u32;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const uint32_t index = n + (Control{base, game_address}.W(kGameBusy) != 0 ? 1924 : 1911);
  const int32_t v = int16_t(Control{base, c.W(76)}.H(index * 2)) + int32_t(c.W(112));
  c.SetW(112, v < 0 ? 0 : uint32_t(v));
  ctx.r3.u64 = game_address;
}

// sub_821E01E0(control): +112 grows by a parameter picked by the substate
// (2-4: 8, 5-7: 7, 8-10: 9, 11-12: 11, 13-14: 10, 15-16: 12; any other
// substate uses whatever its stack frame holds at +80), as AddTo112.
void AddTo112BySubstate(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  const uint32_t frame = ctx.r1.u32 - 112;
  k.k.r1.u64 = frame;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const uint32_t sub = c.H(274);
  uint32_t n;
  if (sub >= 2 && sub <= 4) {
    n = 8;
  } else if (sub >= 5 && sub <= 7) {
    n = 7;
  } else if (sub >= 8 && sub <= 10) {
    n = 9;
  } else if (sub == 11 || sub == 12) {
    n = 11;
  } else if (sub == 13 || sub == 14) {
    n = 10;
  } else if (sub == 15 || sub == 16) {
    n = 12;
  } else {
    n = *game::GuestPtr<be<uint32_t>>(base, frame + 80);
  }
  const uint32_t index = n + (Control{base, game_address}.W(kGameBusy) != 0 ? 1924 : 1911);
  const int32_t v = int16_t(Control{base, c.W(76)}.H(index * 2)) + int32_t(c.W(112));
  c.SetW(112, v < 0 ? 0 : uint32_t(v));
  ctx.r3.u64 = game_address;
}

// sub_821DF990(control, params): +4494 for an attack's substate. With +577
// clear: 255 / 0 from bit 1 of the group's parameter (+0 for 5-7, +16 for
// 2-4, else +32; 1 instead of 0 while +278 is 1). Otherwise it is 255
// until +102 reaches the timer window in the parameters (+10 / +26 / +42;
// a negative value counts while +577 is 1), then 0.
void AttackFlag4494(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  const Control p{base, ctx.r4.u32};
  const uint32_t b577 = c.B(577);
  const uint32_t sub = c.H(274);
  c.SetB(4494, 255);
  if (b577 == 0) {
    const uint32_t o = In(sub, {5, 7, 6}) ? 0 : In(sub, {2, 4, 3}) ? 16 : 32;
    const bool bit = (p.H(o) & 1) != 0;
    c.SetB(4494, bit ? 255 : c.H(278) == 1 ? 1 : 0);
    return;
  }
  uint32_t first, second;
  if (sub == 5) {
    first = second = 10;
  } else if (sub == 2) {
    first = second = 26;
  } else if (sub == 8) {
    first = 26, second = 42;
  } else {
    return;
  }
  const int32_t t102 = c.S16(102);
  if (b577 == 1) {
    const int32_t t = p.S16(sub == 8 ? 42 : first);
    if (t < 0 && !(t102 < -t)) c.SetB(4494, 0);
  } else if (b577 == 2) {
    if (p.H(first) >= 32768) {
      c.SetB(4494, 0);
    } else {
      const int32_t t = p.S16(second);
      if (t >= 0 && !(t102 < t)) c.SetB(4494, 0);
    }
  }
}

// The substate kind's refusal (kSubstateKinds): kinds 2, 5 and 8 wait for
// +460, +458 and +462.
uint32_t RefuseByKind(const Control& c, uint8_t* base, uint32_t result) {
  const int32_t kind = int32_t(*game::GuestPtr<be<uint32_t>>(base, kSubstateKinds + result * 4));
  if (kind == 2) return c.H(460) == 0 ? result : 0;
  if (kind == 5 && c.H(458) != 0) return 0;
  if (kind == 8 && c.H(462) != 0) return 0;
  return result;
}

// sub_821DF6E0(control): on landing, a dash (15 / 11 / 13) for a button held
// 253 ticks (+169, +167, +168) while its range flags (+400) are set.
void LandingDash(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  const uint32_t ranges = c.W(400);
  uint32_t r = 0;
  if (c.B(169) >= 253 && (ranges & 0x30) != 0) {
    r = 15;
  } else if (c.B(167) >= 253 && (ranges & 3) != 0) {
    r = 11;
  } else if (c.B(168) >= 253 && (ranges & 0xC) != 0) {
    r = 13;
  }
  ctx.r3.u64 = RefuseByKind(c, base, r);
}

// sub_821DF440(control, allow_169, specials, long_hold): the command states
// 15 and 19 read. The buttons count as held when their timers (+167 / +168
// / +169) are set (held 255 ticks with long_hold). With specials, while
// +588 is 0 or 3 and +190 is 0x4xx or xx04, a timer at 253 or more gives
// 15 / 11 / 13 (only robot 5's +169 while Game +24932 is set); without
// allow_169 that and +169 are ignored. Then 8 / 2 / 5 for +169 / +167 /
// +168 when sub_82257F50 allows the weapon and the robot isn't in that
// substate already; the result passes RefuseByKind.
void Command(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 160;
  const uint32_t allow_169 = ctx.r4.u32, specials = ctx.r5.u32;
  const int32_t long_hold = ctx.r6.s32;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const uint32_t b167 = c.B(167), b168 = c.B(168), b169 = c.B(169);
  const auto held = [&](uint32_t t) { return long_hold == 0 ? t != 0 : t == 255; };
  const bool h167 = held(b167), h168 = held(b168);
  bool h169 = held(b169);
  bool dash15 = false, dash11 = false, dash13 = false;
  const uint32_t w190 = c.H(190);
  if (specials != 0 && (c.W(588) == 0 || c.W(588) == 3) && ((w190 & 0xFF00) == 1024 || (w190 & 0xFF) == 4)) {
    if (Control{base, game_address}.W(kGameBusy) != 0) {
      dash15 = c.B(2) == 5 && b169 >= 253;
    } else if (b169 >= 253) {
      dash15 = true;
    } else if (b167 >= 253) {
      dash11 = true;
    } else if (b168 >= 253) {
      dash13 = true;
    }
  }
  if (allow_169 == 0) {
    dash15 = false;
    h169 = false;
  }
  const uint32_t r6 = c.H(278) == 1 ? 2 : 0;
  const auto weapon = [&](uint32_t w, uint32_t result) {
    k.k.r6.u64 = r6;
    k.k.r5.u64 = 0;
    k.k.r4.u64 = w;
    return k.Call(__imp__sub_82257F50, c.a) != 0 ? result : 0u;
  };
  uint32_t r = 0;
  if (dash15) {
    r = 15;
  } else if (h169 && c.H(274) != 8) {
    r = weapon(2, 8);
  } else if (dash11) {
    r = 11;
  } else if (h167 && c.H(274) != 2) {
    r = weapon(1, 2);
  } else if (dash13) {
    r = 13;
  } else if (h168 && c.H(274) != 5) {
    r = weapon(0, 5);
  }
  ctx.r3.u64 = RefuseByKind(c, base, r);
}

// sub_821DFFA8(control, buttons, params): an attack in the air. By the
// substate group it sets the button state (+576) from a byte of buttons
// (5-7: bits 8-15, 2-4: bits 16-23, 8-10: bits 0-7) and, while +438 is below
// the group's limit in params (+14 / +30 / +46) and sub_821DFBA0 agrees
// (params + 0 / 16 / 32), fires the weapon (0 / 1 / 2) through
// sub_822583A8 with the substate's variant and counts +438 up. Returns 0
// for other substates (1 for robot 18).
void AirAttack(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  const uint32_t frame = ctx.r1.u32 - 128;
  k.k.r1.u64 = frame;
  const uint32_t buttons = ctx.r4.u32, params = ctx.r5.u32;
  const uint32_t sub = c.H(274);
  uint32_t variant;
  if (In(sub, {5, 2, 8})) {
    variant = 0;
  } else if (In(sub, {7, 4, 10})) {
    variant = 2;
  } else if (In(sub, {6, 3, 9})) {
    variant = 1;
  } else {
    variant = *game::GuestPtr<be<uint32_t>>(base, frame + 80);
  }
  ctx.r3.u64 = 1;
  if (c.B(2) == 18) return;
  uint32_t weapon, button, limit, record;
  if (In(sub, {5, 7, 6})) {
    weapon = 0, button = (buttons >> 8) & 0xFF, limit = 14, record = params;
  } else if (In(sub, {2, 4, 3})) {
    weapon = 1, button = (buttons >> 16) & 0xFF, limit = 30, record = params + 16;
  } else if (In(sub, {8, 10, 9})) {
    weapon = 2, button = buttons & 0xFF, limit = 46, record = params + 32;
  } else {
    ctx.r3.u64 = 0;
    return;
  }
  c.SetB(576, button);
  if (c.S16(438) >= int32_t(Control{base, params}.H(limit))) return;
  k.k.r5.u64 = record;
  k.k.r4.u64 = c.B(577);
  if (k.Call(__imp__sub_821DFBA0, c.a) == 0) return;
  k.k.r6.u64 = 1;
  k.k.r5.u64 = variant;
  k.k.r4.u64 = weapon;
  k.Call(__imp__sub_822583A8, c.a);
  c.SetH(438, c.H(438) + 1);
}

// sub_821DEE00(control, allow_169, mode): the next attack in the air. The
// hold timers +178 / +179 give a / b (1 while held; 0 while the side's Game
// flag 53013 / 53014 is set); with mode, a button (+167 / +168 / +169) that
// is held counts, and one held under 253 ticks turns a / b from 0 to -1;
// without mode only buttons held 253 ticks count. +169 (if allowed), +167,
// +168 pick weapon 2 / 1 / 0; then variant 2 if a > 0, 1 if b > 0, 0 if
// neither is negative, each only if the robot isn't in that substate and
// sub_82257F50 agrees. The result passes RefuseByKind.
void NextAirAttack(PPCContext& ctx, uint8_t* base) {
  const Control c{base, ctx.r3.u32};
  Caller k{ctx, base};
  k.k.r1.u64 = ctx.r1.u32 - 128;
  const uint32_t allow_169 = ctx.r4.u32, mode = ctx.r5.u32;
  const uint32_t game_address = k.Call(__imp__sub_822AC098, 0);
  const Control g{base, game_address};
  int32_t a = c.B(178) != 0 ? 1 : 0, b = c.B(179) != 0 ? 1 : 0;
  if ((c.a == game_address + kControl2 && g.B(53014) != 0) || (c.a == game_address + 25020 && g.B(53013) != 0)) {
    a = 0;
    b = 0;
  }
  bool h167 = false, h168 = false, h169 = false;
  if (mode != 0) {
    const auto count = [&](uint32_t t, bool& held) {
      if (t == 0) return;
      held = true;
      if (t < 253) {
        if (b == 0) b = -1;
        if (a == 0) a = -1;
      }
    };
    count(c.B(167), h167);
    count(c.B(168), h168);
    count(c.B(169), h169);
  } else {
    h167 = c.B(167) >= 253;
    h168 = c.B(168) >= 253;
    h169 = c.B(169) >= 253;
  }
  // Substates s2 / s1 / s0 for variants 2 / 1 / 0 of the weapon.
  const auto pick = [&](uint32_t weapon, uint32_t s2, uint32_t s1, uint32_t s0) -> uint32_t {
    const auto fire = [&](uint32_t variant, uint32_t sub) {
      k.k.r6.u64 = 1;
      k.k.r5.u64 = variant;
      k.k.r4.u64 = weapon;
      return k.Call(__imp__sub_82257F50, c.a) != 0 ? sub : 0u;
    };
    if (a > 0 && c.H(274) != s2) return fire(2, s2);
    if (b > 0 && c.H(274) != s1) return fire(1, s1);
    if (b < 0 || a < 0 || c.H(274) == s0) return 0;
    return fire(0, s0);
  };
  uint32_t r = 0;
  if (allow_169 != 0 && h169) {
    r = pick(2, 10, 9, 8);
  } else if (h167) {
    r = pick(1, 4, 3, 2);
  } else if (h168) {
    r = pick(0, 7, 6, 5);
  }
  ctx.r3.u64 = RefuseByKind(c, base, r);
}

// One live check per helper; `result` reads what the helper returns.
void VerifySmall(const char* name, void (*original_fn)(PPCContext&, uint8_t*),
                 void (*rewrite_fn)(PPCContext&, uint8_t*), PPCContext& ctx, uint8_t* base, bool float_result,
                 uint32_t report_every) {
  static std::map<std::string, Verifier*> verifiers;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  Verifier*& verifier = verifiers[name];
  if (!verifier) verifier = new Verifier(name, base, {{game_address, 0xD000, "game"}});
  const auto result = [&](const PPCContext& k) {
    return float_result ? std::bit_cast<int64_t>(k.f1.f64) : int64_t(k.r3.u32);
  };
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        original_fn(original, base);
        return result(original);
      },
      [&] {
        PPCContext rewrite = ctx;
        rewrite_fn(rewrite, base);
        return result(rewrite);
      },
      [&] { return fmt::format("r3 {:08X} r4 {:08X}", ctx.r3.u32, ctx.r4.u32); },
      report_every);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821EE610) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821EE610(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::FreeState(ctx, base);
}

REX_HOOK_RAW(sub_821F2538) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821F2538(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyState19(ctx, base);
  oratan::decomp::State19(ctx, base);
}

REX_HOOK_RAW(sub_821F8168) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821F8168(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyDriver(ctx, base);
  oratan::decomp::Driver(ctx, base);
}

REX_HOOK_RAW(sub_821F1228) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821F1228(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyState15(ctx, base);
  oratan::decomp::State15(ctx, base);
}

REX_HOOK_RAW(sub_821E4F88) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E4F88(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyState18(ctx, base);
  oratan::decomp::State18(ctx, base);
}

REX_HOOK_RAW(sub_821DD968) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DD968(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyChooseSubstate(ctx, base);
  oratan::decomp::ChooseSubstateHook(ctx, base);
}

REX_HOOK_RAW(sub_821E1930) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E1930(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyPickSubstate(ctx, base);
  oratan::decomp::PickSubstateHook(ctx, base);
}

REX_HOOK_RAW(sub_821F0288) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821F0288(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyState12(ctx, base);
  oratan::decomp::State12(ctx, base);
}

REX_HOOK_RAW(sub_821E2328) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E2328(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyMoveSubstate0(ctx, base);
  oratan::decomp::MoveSubstate0(ctx, base);
}

REX_HOOK_RAW(sub_821E1FF0) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E1FF0(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyJumpOrClassMove(ctx, base);
  oratan::decomp::JumpOrClassMove(ctx, base);
}

REX_HOOK_RAW(sub_821E4BC0) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E4BC0(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyMoveOtherSubstates(ctx, base);
  oratan::decomp::MoveOtherSubstates(ctx, base);
}

REX_HOOK_RAW(sub_821DD2B0) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DD2B0(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyFillChoiceRecord(ctx, base);
  oratan::decomp::FillChoiceRecord(ctx, base);
}

REX_HOOK_RAW(sub_821DD180) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DD180(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821DD180", __imp__sub_821DD180, oratan::decomp::MotionFlags, ctx, base, false, 6000);
  }
  oratan::decomp::MotionFlags(ctx, base);
}

REX_HOOK_RAW(sub_821DD230) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DD230(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821DD230", __imp__sub_821DD230, oratan::decomp::TurnTarget, ctx, base, false, 6000);
  }
  oratan::decomp::TurnTarget(ctx, base);
}

REX_HOOK_RAW(sub_821DD890) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DD890(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821DD890", __imp__sub_821DD890, oratan::decomp::SpeedByAngle, ctx, base, true, 6000);
  }
  oratan::decomp::SpeedByAngle(ctx, base);
}

REX_HOOK_RAW(sub_821E0160) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E0160(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821E0160", __imp__sub_821E0160, oratan::decomp::AddTo112, ctx, base, false, 100);
  }
  oratan::decomp::AddTo112(ctx, base);
}

REX_HOOK_RAW(sub_821E01E0) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821E01E0(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821E01E0", __imp__sub_821E01E0, oratan::decomp::AddTo112BySubstate, ctx, base, false, 100);
  }
  oratan::decomp::AddTo112BySubstate(ctx, base);
}

REX_HOOK_RAW(sub_821DF990) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DF990(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821DF990", __imp__sub_821DF990, oratan::decomp::AttackFlag4494, ctx, base, false, 100);
  }
  oratan::decomp::AttackFlag4494(ctx, base);
}

REX_HOOK_RAW(sub_821DF6E0) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DF6E0(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821DF6E0", __imp__sub_821DF6E0, oratan::decomp::LandingDash, ctx, base, false, 100);
  }
  oratan::decomp::LandingDash(ctx, base);
}

REX_HOOK_RAW(sub_821DF440) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DF440(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821DF440", __imp__sub_821DF440, oratan::decomp::Command, ctx, base, false, 600);
  }
  oratan::decomp::Command(ctx, base);
}

REX_HOOK_RAW(sub_821DFFA8) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DFFA8(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821DFFA8", __imp__sub_821DFFA8, oratan::decomp::AirAttack, ctx, base, false, 100);
  }
  oratan::decomp::AirAttack(ctx, base);
}

REX_HOOK_RAW(sub_821DEE00) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821DEE00(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) {
    oratan::decomp::VerifySmall("sub_821DEE00", __imp__sub_821DEE00, oratan::decomp::NextAirAttack, ctx, base, false, 100);
  }
  oratan::decomp::NextAirAttack(ctx, base);
}
