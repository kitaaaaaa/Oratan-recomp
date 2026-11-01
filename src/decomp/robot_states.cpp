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
#include <cstdint>
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
REX_EXTERN(__imp__sub_822AC098);
REX_EXTERN(__imp__sub_821F1BE0);
REX_EXTERN(__imp__sub_821DFC50);
REX_EXTERN(__imp__sub_821DD890);
REX_EXTERN(__imp__sub_821DF990);
REX_EXTERN(__imp__sub_821DF200);
REX_EXTERN(__imp__sub_821DF440);
REX_EXTERN(__imp__sub_821E01E0);
REX_EXTERN(__imp__sub_821F8168);
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
