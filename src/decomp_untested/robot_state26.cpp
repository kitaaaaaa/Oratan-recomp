// sub_821F5258: movement state 26's handler (table at 0x82027C80). By the
// robot (+2, robot_id), state 26 being each robot's special move: 0 (dor) a charge along the facing with a ground-following fall / landing (ground checks through
// sub_8222CB80, a vertical push at +64 clamped by constants), 2 (cyp) a flight that
// follows the other robot's height (+6 eased toward it), 6 (rai) a timed push from
// the robot's data (sub_821DD890), 1/5/16 (bbd, bks, bbs) and 10 (spf) short timed steps, any other robot
// resets the state. In the third rate set (Game +0x6164) robots 0 and 2
// instead push the velocity through the matrix helpers (sub_822B5A18 ...
// sub_822B61B0). Fields are written by offset (mostly unnamed). Callees are
// the original code with one register context running through the calls.

#include <rex/hook.h>

#include "../game_memory.h"
#include "decomp.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <string>

#include <fmt/format.h>

#pragma clang fp contract(off)

REX_EXTERN(__imp__sub_821F5258);
REX_EXTERN(__imp__sub_821A5C20);
REX_EXTERN(__imp__sub_821BFF48);
REX_EXTERN(__imp__sub_821DD180);
REX_EXTERN(__imp__sub_821DD890);
REX_EXTERN(__imp__sub_821ED918);
REX_EXTERN(__imp__sub_8222CB80);
REX_EXTERN(__imp__sub_822A74E8);
REX_EXTERN(__imp__sub_822A74F8);
REX_EXTERN(__imp__sub_822A7508);
REX_EXTERN(__imp__sub_822A7558);
REX_EXTERN(__imp__sub_822A75A0);
REX_EXTERN(__imp__sub_822B5908);
REX_EXTERN(__imp__sub_822B5A18);
REX_EXTERN(__imp__sub_822B5A78);
REX_EXTERN(__imp__sub_822B61B0);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;
using Fn = void (*)(PPCContext&, uint8_t*);

struct S26 {
  uint8_t* base;
  uint32_t a;       // control block
  uint32_t game;
  uint32_t frame;
  PPCContext k;     // running registers

  float F(uint32_t addr) const {
    return std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, addr)));
  }
  double D(uint32_t addr) const {
    return std::bit_cast<double>(uint64_t(*game::GuestPtr<be<uint64_t>>(base, addr)));
  }
  double CF(uint32_t o) const { return double(F(a + o)); }
  void SetCF(uint32_t o, double v) {
    *game::GuestPtr<be<uint32_t>>(base, a + o) = std::bit_cast<uint32_t>(float(v));
  }
  uint8_t B(uint32_t o) const { return *game::GuestPtr<uint8_t>(base, a + o); }
  void SetB(uint32_t o, uint32_t v) { *game::GuestPtr<uint8_t>(base, a + o) = uint8_t(v); }
  uint16_t H(uint32_t o) const { return *game::GuestPtr<be<uint16_t>>(base, a + o); }
  void SetH(uint32_t o, uint32_t v) { *game::GuestPtr<be<uint16_t>>(base, a + o) = uint16_t(v); }
  uint32_t W(uint32_t o) const { return *game::GuestPtr<be<uint32_t>>(base, a + o); }
  void SetW(uint32_t o, uint32_t v) { *game::GuestPtr<be<uint32_t>>(base, a + o) = v; }

  uint32_t Call(Fn fn) {
    fn(k, base);
    return k.r3.u32;
  }
  bool Dash() {
    k.r4.u64 = 1;
    k.r3.u64 = a;
    return Call(__imp__sub_821DD180) != 0;
  }
  double AngleA(uint32_t angle) {
    k.r3.u64 = angle;
    Call(__imp__sub_822A7558);
    return k.f1.f64;
  }
  double AngleB(uint32_t angle) {
    k.r3.u64 = angle;
    Call(__imp__sub_822A75A0);
    return k.f1.f64;
  }
  double Ground(double x, double z) {
    k.r7.u64 = frame + 80;
    k.r6.u64 = frame + 88;
    k.r3.u64 = a;
    k.f1.f64 = x;
    k.f2.f64 = z;
    Call(__imp__sub_8222CB80);
    return k.f1.f64;
  }
  void Voice(uint32_t buffer) {
    k.r5.u64 = 0x82028250;
    k.r4.u64 = 0xB17A9;
    k.r3.u64 = frame + buffer;
    const uint32_t r = Call(__imp__sub_821A5C20);
    k.r4.u64 = r;
    k.r3.u64 = a;
    Call(__imp__sub_821ED918);
  }
  void Reset() {  // 0x821F6070 onward, without 290/274/292
    SetW(112, 0);
    SetH(4492, 0);
    SetH(12, 0);
    SetH(598, 0);
    SetH(594, 0);
    SetW(584, 0);
    SetH(104, 0);
    SetH(294, 0);
    SetH(102, 0);
    SetB(577, 0);
    SetB(576, 0);
    SetH(288, 0);
    SetH(272, 0);
  }
  void BigReset(bool with_292, bool with_416) {
    SetH(272, 0);
    SetH(288, 0);
    SetH(290, 0);
    SetH(274, 0);
    SetB(576, 0);
    SetB(577, 0);
    if (with_292) SetH(292, 0);
    SetH(102, 0);
    SetH(294, 0);
    SetH(104, 0);
    SetW(584, 0);
    SetH(594, 0);
    SetH(598, 0);
    SetH(12, 0);
    SetH(4492, 0);
    SetW(112, 0);
    if (with_416) SetW(416, 0);
  }
  // The matrix push used in the third rate set (frame +96..+120).
  void MatrixPush(double amount) {
    Call(__imp__sub_822B5A18);
    Call(__imp__sub_822B5908);
    k.r3.s64 = int16_t(H(8));
    Call(__imp__sub_822A74F8);
    k.r3.s64 = int16_t(H(6));
    Call(__imp__sub_822A74E8);
    k.r3.s64 = int16_t(H(10));
    Call(__imp__sub_822A7508);
    *game::GuestPtr<be<uint32_t>>(base, frame + 120) = std::bit_cast<uint32_t>(float(amount));
    k.r4.u64 = frame + 112;
    k.r3.u64 = frame + 96;
    Call(__imp__sub_822B61B0);
    const double x = CF(380), y = CF(384), z = CF(388);
    SetCF(380, double(float(x + double(F(frame + 96)))));
    SetCF(384, double(float(y + double(F(frame + 100)))));
    SetCF(388, double(float(z + double(F(frame + 104)))));
    Call(__imp__sub_822B5A78);
  }
  // velocity += amount * (-A(facing), B(facing)), single precision.
  void PushFacingS() {
    const double a1 = AngleA(H(8));
    SetCF(380, double(float(-std::fma(a1, CF(372), -CF(380)))));
    const double b1 = AngleB(H(8));
    SetCF(388, double(float(std::fma(b1, CF(372), CF(388)))));
  }
  // The landing ground test: below the "no ground" limit and below the robot.
  double NoGround() const { return double(F(0x820022B4)); }
  // A clamped downward step at +64, then added to y (+28).
  void Settle(double ground, double scale_addr_d, uint32_t min_addr) {
    const double y = CF(28);
    const double drop = double(float(y - ground));
    const double step = double(float(-drop * D(uint32_t(scale_addr_d))));
    SetCF(64, step);
    const double floor = double(F(min_addr));
    if (step < floor) SetCF(64, floor);
  }
};

void Dordray0(S26& s, bool busy) {
  if (busy) {
    s.SetB(576, 162);
    s.SetW(584, 2);
    s.MatrixPush(double(s.F(0x82010844)));
    const uint8_t v = s.B(577);
    if (v == 0) {
      if (s.Dash()) {
        s.SetH(292, 0);
        s.SetB(336, 7);
        s.SetB(286, 1);
        s.SetH(276, 0);
        s.SetB(568, 30);
        s.Voice(128);
      }
    } else if (v == 1) {
      s.SetB(337, 3);
      if (s.Dash()) {
        if (s.H(276) > 100) s.SetB(286, 2);
        s.SetH(292, 0);
      }
      s.SetH(276, s.H(276) + 1u);
    } else {
      goto other;
    }
    return;
  }
  {
    const uint32_t mode = s.H(278);
    double push;
    if (mode == 4) {
      s.SetB(576, 162);
      push = s.D(0x82016310);
    } else {
      s.SetB(576, mode == 1 ? 161 : 160);
      push = 0;
    }
    s.SetW(584, 2);
    const double vx = s.CF(380);
    const double a1 = s.AngleA(s.H(8));
    if (mode != 4) push = mode == 1 ? s.D(0x820176F0) : s.D(0x820216F8);
    s.SetCF(380, double(float(-std::fma(a1, push, -vx))));
    const double b1 = s.AngleB(s.H(8));
    const double vz = std::fma(b1, push, s.CF(388));
    const double x = double(float(s.CF(24) + s.CF(380)));
    s.SetCF(388, double(float(vz)));
    const double z = double(float(s.CF(32) + s.CF(388)));
    const double ground = s.Ground(x, z);
    const uint8_t v = s.B(577);
    if (mode == 4) {
      const double g = double(float(ground + s.D(0x82017448)));
      if (v == 0) {
        s.SetCF(388, 0.0f);
        s.SetCF(380, 0.0f);
        if (g < s.NoGround() && g < s.CF(28)) {
          const double y = s.CF(28);
          s.Settle(g, 0x82028008, 0x82021628);
          s.SetCF(28, double(float(y + s.CF(64))));
          if (s.Dash()) {
            s.SetH(292, 0);
            s.SetB(286, 1);
            s.SetH(276, 0);
            s.SetB(568, 31);
            s.Voice(160);
          }
        } else {
          s.SetCF(388, 0.0f);
          s.SetCF(380, 0.0f);
          s.SetB(286, 2);
          s.SetH(292, 0);
          s.SetW(300, 0);
        }
        return;
      }
      if (v == 1) {
        s.SetB(337, 3);
        bool hold = false;
        if (g < s.NoGround() && g < s.CF(28)) {
          const double y = s.CF(28);
          s.Settle(g, 0x82017310, 0x82001370);
          if (!(s.CF(64) > double(s.F(0x82000924)))) {
            hold = true;
            s.SetCF(28, double(float(s.CF(64) + y)));
            if (s.B(336) == 7) {
              const uint32_t params = s.W(76);
              const double scale = s.CF(game::kControlScale);
              if (scale < double(s.F(params + 1476)) || scale < double(s.F(params + 1480)) ||
                  scale < double(s.F(params + 1484))) {
                s.SetB(336, 9);
              }
            }
            if (s.Dash()) {
              if (s.H(276) > 80) s.SetB(286, 2);
              s.SetH(292, 0);
            }
            s.SetH(276, s.H(276) + 1u);
          }
        }
        if (!hold) {
          s.SetCF(380, 0.0f);
          s.SetCF(388, 0.0f);
          s.SetB(286, 2);
          s.SetH(292, 0);
        }
        return;
      }
    } else {
      if (v == 0) {
        if (ground < s.NoGround() && ground < s.CF(28) + s.D(0x820172E0)) {
          s.SetCF(28, ground);
          if (s.Dash()) {
            const uint32_t m = s.H(278);
            s.SetH(292, 0);
            s.SetB(286, 1);
            s.SetH(276, 0);
            s.SetB(568, m == 1 ? 32 : 30);
            s.Voice(192);
          }
        } else {
          s.SetCF(388, 0.0f);
          s.SetCF(380, 0.0f);
          s.SetB(286, 2);
          s.SetH(292, 0);
        }
        return;
      }
      if (v == 1) {
        s.SetB(337, 3);
        if (ground < s.NoGround() && ground < s.CF(28) + s.D(0x820172E0)) {
          const uint32_t t = s.H(276);
          s.SetCF(28, ground);
          if (t > 60) {
            s.SetB(286, 2);
            s.SetH(292, 0);
          }
          if (s.Dash()) s.SetH(292, 0);
          s.SetH(276, s.H(276) + 1u);
        } else {
          s.SetCF(380, 0.0f);
          s.SetCF(388, 0.0f);
          s.SetB(286, 2);
          s.SetH(292, 0);
        }
        return;
      }
    }
  }
other:
  if (s.B(577) == 2) {
    s.SetCF(388, 0.0f);
    s.SetCF(380, 0.0f);
    s.SetB(337, 0);
    if (s.Dash()) {
      s.SetH(274, 0);
      s.SetB(286, 0);
      s.SetH(294, 0);
      s.SetW(584, 0);
      s.SetH(594, 0);
      s.SetH(598, 0);
      s.SetH(12, 0);
      s.SetH(4492, 0);
      s.SetB(284, 4);
      s.SetW(112, 0);
      s.SetH(272, 0);
      s.SetH(288, 0);
      s.SetH(290, 0);
      s.SetH(292, 0);
      s.SetW(300, 0);
    }
  }
}

void Cypher2(S26& s, bool busy) {
  s.SetW(584, 2);
  s.SetB(576, 60);
  const uint8_t v = s.B(577);
  if (!busy) {
    if (v == 0) {
      const double ground = s.Ground(s.CF(24), s.CF(32));
      const double g = double(float(ground + s.D(0x820176E0)));
      const double y = s.CF(28);
      s.Settle(g, 0x82028008, 0x82021628);
      s.SetCF(28, double(float(y + s.CF(64))));
      s.SetCF(388, 0.0f);
      s.SetCF(380, 0.0f);
      if (s.Dash()) {
        s.SetH(276, 0);
        s.SetB(286, 1);
        s.SetB(568, 30);
        s.SetB(337, 3);
        s.SetH(292, 0);
      }
      return;
    }
    if (v != 1) return;
    s.SetB(568, 30);
    s.SetB(337, 3);
    s.SetCF(372, s.H(276) < 8 ? double(s.F(0x820024FC)) : double(s.F(0x82010844)));
    const double mine = s.CF(28);
    const double theirs = double(s.F(s.W(72) + 28));
    const int32_t tilt = int16_t(s.H(6));
    if (mine < theirs) {
      if (tilt < 4096) s.SetH(6, tilt + 256);
    } else if (mine > theirs) {
      if (tilt > -4096) s.SetH(6, tilt - 256);
    } else if (tilt > 256) {
      s.SetH(6, tilt - 256);
    } else if (tilt < -256) {
      s.SetH(6, tilt + 256);
    } else {
      s.SetH(6, 0);
    }
    const double a6 = s.AngleA(s.H(6));
    s.SetCF(28, double(float(std::fma(a6, s.D(0x82006A88), s.CF(28)))));
    const int32_t stage = *game::GuestPtr<be<int32_t>>(s.base, game::kNextStage);
    const double cap = stage == 10 ? double(s.F(0x82007C10)) : double(s.F(0x82001514));
    if (s.CF(28) > cap) s.SetCF(28, cap);
    s.PushFacingS();
    const double x = double(float(s.CF(24) + s.CF(380)));
    const double z = double(float(s.CF(32) + s.CF(388)));
    const double ground = s.Ground(x, z);
    const uint32_t t = (uint32_t(s.H(276)) + 1) & 0xFFFF;
    s.SetH(276, t);
    if (t > 330 || !(ground < s.NoGround())) {
      s.SetH(272, 12);
      s.SetCF(388, 0.0f);
      s.SetCF(380, 0.0f);
      s.SetH(274, 0);
      s.SetB(286, 2);
      s.SetW(300, 4);
      s.SetB(337, 0);
      s.SetH(6, 0);
      s.SetH(10, 0);
    } else if (!(ground < s.CF(28))) {
      s.SetCF(28, ground);
    }
    if (s.Dash()) s.SetH(292, 0);
    return;
  }
  s.SetH(276, 0);
  if (v == 0) {
    s.SetCF(372, double(s.F(0x82000958)));
    if (s.Dash()) {
      s.SetB(286, 1);
      s.SetB(568, 30);
      s.SetB(337, 3);
      s.SetH(292, 0);
    }
  } else if (v == 1) {
    s.SetB(568, 30);
    s.SetB(337, 3);
    if (*game::GuestPtr<be<uint16_t>>(s.base, s.W(72) + 316) == 0) {
      uint64_t r4 = s.H(4488), r5 = s.H(4490);
      if (int16_t(r4) > 4096) r4 = 4096; else if (int16_t(r4) < -4096) r4 = uint64_t(-4096);
      if (int16_t(r5) > 4096) r5 = 4096; else if (int16_t(r5) < -4096) r5 = uint64_t(-4096);
      s.k.r6.u64 = 0;
      s.k.r5.u64 = r5;
      s.k.r4.u64 = r4;
      s.k.r3.u64 = s.a;
      s.Call(__imp__sub_821BFF48);
    }
    const uint32_t t = (uint32_t(s.H(276)) + 1) & 0xFFFF;
    s.SetH(276, t);
    if (t > 330) {
      s.SetB(286, 2);
      s.SetH(292, 0);
    } else if (s.Dash()) {
      s.SetH(292, 0);
    }
    s.SetCF(372, double(s.F(0x82028218)));
  } else if (s.Dash()) {
    s.BigReset(false, false);
    s.SetH(292, 0);
  }
  const uint8_t now = s.B(577);
  if (now == 0 || now == 1) s.MatrixPush(s.CF(372));
}

void Raiden6(S26& s) {
  s.SetB(576, 163);
  s.SetW(584, 2);
  s.k.r4.u64 = 0;
  s.k.r3.u64 = game::LoadGuestAddress(s.base, s.game + game::kGameRobotData) + 824;
  s.Call(__imp__sub_821DD890);
  s.SetCF(372, s.k.f1.f64);
  const bool dashed = s.Dash();
  const uint8_t v = s.B(577);
  if (dashed) {
    if (v == 0) {
      s.SetW(416, 0);
      s.SetH(292, 0);
      s.SetB(286, 1);
    } else {
      if (v != 1) s.BigReset(true, true);
      s.SetH(292, 0);
    }
  } else if (v == 1) {
    const uint32_t params = s.W(76);
    const int32_t current = int32_t(s.W(416));
    const int32_t limit = *game::GuestPtr<be<uint16_t>>(s.base, params + 3640);
    if (current >= limit) {
      s.SetW(416, uint32_t(limit));
      s.SetB(286, 2);
      s.SetH(292, 0);
    } else {
      s.SetW(416, uint32_t(current + *game::GuestPtr<be<uint16_t>>(s.base, params + 3628)));
    }
  } else if (v == 2) {
    const double t = double(float(double(int64_t(int16_t(s.H(102))))));
    const double n = double(float(double(int64_t(int32_t(s.W(612) + 1)))));
    const double ratio = double(float(t / n));
    s.SetCF(372, double(float((s.D(0x82001388) - ratio) * s.CF(372))));
  }
  s.PushFacingS();
}

// sub_821F5258.
void State26(PPCContext& ctx, uint8_t* base) {
  S26 s{base, ctx.r3.u32, game::LoadGuestAddress(base, game::kGamePointer), ctx.r1.u32 - 288, ctx};
  s.k.r1.u64 = s.frame;
  s.k.fpscr.disableFlushMode();
  const uint32_t zero = *game::GuestPtr<be<uint32_t>>(base, 0x8200095C);
  for (uint32_t o : {120u, 116u, 112u}) *game::GuestPtr<be<uint32_t>>(base, s.frame + o) = zero;
  const bool busy = *game::GuestPtr<be<int32_t>>(base, s.game + 24932) != 0;
  const uint8_t robot = s.B(2);
  switch (robot) {
    case 0:
      Dordray0(s, busy);
      break;
    case 2:
      Cypher2(s, busy);
      return;
    case 6:
      Raiden6(s);
      return;
    case 1:
    case 5:
    case 16:
      s.SetB(576, 62);
      s.SetW(584, 2);
      if (s.Dash()) {
        if (s.B(577) == 0) {
          s.SetB(286, 1);
        } else {
          s.SetB(286, 2);
          s.SetW(264, 0);
          s.SetH(272, 12);
          s.SetB(284, 8);
          s.SetH(290, 0);
          s.SetH(274, 0);
        }
        s.SetH(292, 0);
      }
      return;
    case 10: {
      s.SetB(576, 61);
      s.SetW(584, 2);
      const bool dashed = s.Dash();
      const uint8_t v = s.B(577);
      if (dashed) {
        if (v == 0) {
          s.SetH(276, 0);
          s.SetB(286, 1);
          s.SetB(568, 30);
          s.SetB(337, 1);
        } else if (v == 1) {
          if (s.H(276) < 4) {
            s.SetB(286, 2);
            s.SetB(337, 0);
          }
          s.SetH(276, 0);
        } else {
          s.Reset();
          s.SetH(290, 0);
          s.SetH(274, 0);
        }
        s.SetH(292, 0);
      } else if (v == 1 && s.B(181) != 0) {
        s.SetH(276, s.H(276) + 1u);
      }
      return;
    }
    default:
      s.Reset();
      s.SetH(290, 0);
      s.SetH(274, 0);
      s.SetH(292, 0);
      return;
  }
  // Action 0's common end.
  const uint32_t params = s.W(76);
  s.SetW(412, *game::GuestPtr<be<uint16_t>>(base, params + 3512));
  s.SetH(8, s.W(256) + s.H(8));
}

void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    const uint32_t generators = game::LoadGuestAddress(base, game::kRandomGenerators);
    const uint32_t random_state = game::LoadGuestAddress(base, generators + 16 * 1 + 4);
    verifier = new Verifier("sub_821F5258", base,
                            {{game_address, 0xD000, "game"},
                             {game::kRandomCalls, 4, "random calls"},
                             {random_state, game::kRandomStateSize, "random state"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821F5258(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        State26(rewrite, base);
        return int64_t(0);
      },
      [&] {
        return fmt::format("robot {} b577 {}", *game::GuestPtr<uint8_t>(base, control + 2),
                           *game::GuestPtr<uint8_t>(base, control + 577));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821F5258) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821F5258(ctx, base);
    return;
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::State26(ctx, base);
}
