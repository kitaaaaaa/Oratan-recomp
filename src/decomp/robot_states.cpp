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
