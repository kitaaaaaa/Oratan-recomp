// Hand-decompiled functions that hand a robot its input each tick.
//
// sub_821FB390 (called with a RobotControl, robot +0x6C, via sub_821EC830):
//   - Packs the side's lever channels (Game::levers, built by sub_822A71B0)
//     into RobotInput::held / pressed: first lever (left channel) in the low
//     byte, second lever (right channel) in the high byte.
//   - Picks the direction code: on battle step 0x13 from a per-side table
//     (sub_82200430 / sub_82200498), on step 0x14 it keeps last tick's;
//     otherwise by RobotControl::mode: the CPU (sub_82212A98, which also
//     replaces held / pressed), PlaybackInput (sub_821CBB28), none (mode 8),
//     or the player's levers through the two DirectionLookup tables.
//   - Sorts the code into a class 0-7 (sub_821F9BC8), points kChargeTable at
//     one of the robot's ChargeParams sets (or a fixed or quarter-rate one),
//     runs the charge meters (sub_821F9DE0) and then turns them into lever
//     vectors (sub_821FA290, LeverMovement), and afterwards counts the button
//     hold timers and latches the direction class on some presses.
//   Every robot goes through it the same way; what differs per robot is the
//   numbers in its parameters (charge rates, spread scales).
//
// sub_821FA290: each lever's eight meters as an (x, y) vector; the two
//   levers' y sum, y difference and x sum (scaled, eased in by the value at
//   kControlScale) and how much they changed; `spread`, the levers' squared x
//   difference times a robot parameter picked by state, buttons and a
//   reference value; in the third rate set the same for the y difference
//   with both buttons held, or a counter for some direction codes; and an
//   angle from the y and x sums (sub_822A73D8, original), as a quadrant when
//   spread is large.
//
// The CPU player's own steps (eight functions under sub_82212A98) and the
// angle function are called as the original code, with the same arguments
// and stack frames the original passes.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <string>

#include <fmt/format.h>

// Each guest float instruction is its own operation (see LeverMovement).
#pragma clang fp contract(off)

REX_EXTERN(__imp__sub_821FB390);
REX_EXTERN(__imp__sub_821F9BC8);
REX_EXTERN(__imp__sub_822A73D8);
REX_EXTERN(__imp__sub_822118F0);
REX_EXTERN(__imp__sub_8220F790);
REX_EXTERN(__imp__sub_822108B8);
REX_EXTERN(__imp__sub_8221B768);
REX_EXTERN(__imp__sub_82212128);
REX_EXTERN(__imp__sub_82218060);
REX_EXTERN(__imp__sub_82212330);
REX_EXTERN(__imp__sub_82210258);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
using game::be;

// Game fields not in game::Game yet (inside robots[0]'s record and past the
// mapped part); their meaning isn't known.
constexpr uint32_t kGameBusy = 24932;      // be<int32_t>, Game +0x6164: picks the third rate set
constexpr uint32_t kGameAiBypass = 52979;  // u8, Game +0xCEF3: the CPU only re-reads the direction
constexpr uint32_t kControl1 = 25020;      // Game offset of side 1's RobotControl
constexpr uint32_t kControl2 = 29516;      // and side 2's

uint32_t GameAddress(const uint8_t* base) {
  return game::LoadGuestAddress(base, game::kGamePointer);
}

// srw: shifts of 32 to 63 give 0.
uint32_t ShiftRight(uint32_t value, uint32_t shift) {
  shift &= 63;
  return shift >= 32 ? 0 : value >> shift;
}

const game::DirectionLookup& Lookup(const uint8_t* base, uint32_t address) {
  return *game::GuestPtr<game::DirectionLookup>(base, address);
}

// A packed lever word's direction code, with the tables' own shifts.
uint16_t DirectionCode(const uint8_t* base, uint32_t held) {
  const auto& first = Lookup(base, game::kDirectionLookupFirst);
  const auto& second = Lookup(base, game::kDirectionLookupSecond);
  const uint32_t high = first.code[ShiftRight(held, first.shift) & 15];
  const uint32_t low = second.code[ShiftRight(held, second.shift) & 15];
  return uint16_t((high << 8) | low);
}

// sub_82200430 (side 1) / sub_82200498 (side 2): this entry of the side's
// direction table, or 0xFFFF when the tables aren't in use.
uint16_t ScriptedDirection(const uint8_t* base, int side) {
  if (*game::GuestPtr<be<int32_t>>(base, game::kScriptedActive) < 0) return 0xFFFF;
  const uint32_t object = game::LoadGuestAddress(base, game::kObjectTable + 16 * 4);
  const uint32_t index = *game::GuestPtr<be<uint32_t>>(base, game::kScriptedIndex) + 0x20000 -
                         (side == 0 ? 28952u : 28832u);
  return *game::GuestPtr<be<uint16_t>>(base, object + 0x150000 + (index << 1));
}

// sub_821F9BC8: sorts the direction code into a class 0-7. Codes it doesn't
// know leave both class bytes as they are.
void ClassifyDirection(game::RobotInput& in) {
  int cls;
  switch (uint16_t(in.direction)) {
    case 0: case 1: case 7: case 255: case 256: case 263: case 1792: case 1793: case 65280:
      cls = 0; break;
    case 772: case 773: case 1027: case 1028: case 1029: case 1279: case 1283: case 1284:
    case 65284:
      cls = 1; break;
    case 257: case 511: case 65281:
      cls = 2; break;
    case 258: case 259: case 513: case 514: case 515: case 767: case 769: case 770: case 65282:
      cls = 3; break;
    case 771: case 1023: case 65283:
      cls = 4; break;
    case 1799: case 2047: case 65287:
      cls = 5; break;
    case 1286: case 1287: case 1541: case 1542: case 1543: case 1791: case 1797: case 1798:
    case 65286:
      cls = 6; break;
    case 1285: case 1535: case 65285:
      cls = 7; break;
    default:
      return;
  }
  in.direction_class2 = uint8_t(cls);
  in.direction_class = uint8_t(cls);
}

// sub_821CBB28: input from the side's PlaybackInput record.
void PlaybackDirection(uint8_t* base, uint32_t control) {
  const uint32_t game_address = GameAddress(base);
  uint32_t source = game::kPlaybackInput;
  if (control != game_address + kControl1) source += game::kPlaybackInputStride;
  const auto& playback = *game::GuestPtr<game::PlaybackInput>(base, source);
  auto& in = game::GuestPtr<game::RobotControl>(base, control)->input;
  const uint32_t held = playback.held;
  in.held = held;
  in.pressed = uint32_t(playback.held) & ~uint32_t(playback.previous);
  in.direction = DirectionCode(base, held);
}

// Calls an original step with (r3, r4) = (control, ai) on `c`, which carries
// the registers from one call to the next as in the original.
void Step(void (*fn)(PPCContext&, uint8_t*), PPCContext& c, uint8_t* base, uint32_t control,
          uint32_t ai) {
  c.r3.u64 = control;
  c.r4.u64 = ai;
  fn(c, base);
}

// sub_82212A98: the CPU player's input. `c` is the caller's frame.
void AiInput(PPCContext c, uint8_t* base, uint32_t control) {
  const uint32_t game_address = GameAddress(base);
  auto& in = game::GuestPtr<game::RobotControl>(base, control)->input;
  if (*game::GuestPtr<uint8_t>(base, game_address + kGameAiBypass) != 0) {
    in.direction = DirectionCode(base, in.held);
    return;
  }
  c.r1.u64 = c.r1.u32 - 128;  // the original's frame
  const uint32_t ai_address =
      control == game_address + kControl1 ? game::kAiControl1 : game::kAiControl2;
  auto& ai = *game::GuestPtr<game::AiControl>(base, ai_address);
  const uint32_t previous = ai.held;
  ai.hold_extra = 0;
  Step(__imp__sub_822118F0, c, base, control, ai_address);
  Step(__imp__sub_8220F790, c, base, control, ai_address);
  if (ai.timer < 0) {
    const int32_t plan_before = ai.plan;
    __imp__sub_822108B8(c, base);  // with the registers the previous step left
    const uint8_t robot = game::GuestPtr<game::RobotControl>(base, control)->robot_id;
    if (robot != 17 && robot != 18) {
      if (ai.plan == plan_before) {
        ai.repeats = ai.repeats + 1;
      } else {
        ai.repeats = 0;
      }
      // (The original also tests plan == 126 with 8 repeats, which 4 already covers.)
      if (ai.repeats >= 4) {
        ai.repeats = 0;
        ai.unk_14 = 0;
        ai.duration = 1;
        ai.unk_18 = 33;
        ai.unk_34 = 1;
      }
    }
    Step(__imp__sub_8221B768, c, base, control, ai_address);
  }
  Step(__imp__sub_82212128, c, base, control, ai_address);
  if (ai.unk_44 == 1 && ai.unk_1C == 0) Step(__imp__sub_82218060, c, base, control, ai_address);
  Step(__imp__sub_82212330, c, base, control, ai_address);
  Step(__imp__sub_82210258, c, base, control, ai_address);
  const int32_t duration = ai.duration;
  if (duration > 0 && ai.timer < 0) {
    ai.timer = 0;
  } else {
    ai.timer = ai.timer + 1;
    if (ai.timer > duration) {
      ai.unk_1C = 0;
      ai.timer = -1;
      ai.duration = -1;
    }
  }
  // (The original stores 0xFFFF in `direction` here when unk_14 is 0; the
  // store below always replaces it.)
  const uint32_t held = ai.held;
  in.pressed = held & ~previous;
  in.held = held;
  in.hold_extra = ai.hold_extra;
  in.direction = DirectionCode(base, held);
}

// A charge meter by its halfword index from the RobotInput start (first
// lever: 18 + direction, second: 26 + direction). The direction is a byte
// of the code and can be past 7; the original indexes on regardless.
be<uint16_t>& Meter(uint8_t* base, uint32_t input, uint32_t index) {
  return game::GuestPtr<be<uint16_t>>(base, input)[index];
}
int32_t Rate(const uint8_t* base, uint32_t table, uint32_t index) {
  return game::GuestPtr<be<int32_t>>(base, table)[index];
}
uint16_t Minus(uint16_t value, int32_t rate) { return uint16_t(uint32_t(value) - uint32_t(rate)); }

// Drains all eight meters from `first`; those below `floor` go to 0.
void Drain(uint8_t* base, uint32_t input, uint32_t first, const game::ChargeTable& t,
           int32_t floor) {
  for (uint32_t i = 0; i < 8; ++i) {
    auto& meter = Meter(base, input, first + i);
    const int16_t value = int16_t(Minus(meter, Rate(base, t.decay, i)));
    meter = uint16_t(value);
    if (value < floor) meter = 0;
  }
}

// One lever's meters for direction byte `held` (255 = none held).
void ChargeLever(uint8_t* base, uint32_t input, uint32_t first, uint32_t held,
                 const game::ChargeTable& t) {
  const int32_t threshold = t.threshold;
  if (held == 255) {
    Drain(base, input, first, t, threshold);
    return;
  }
  const uint32_t opposite = (held - 4) & 7;
  auto& drop = Meter(base, input, first + opposite);
  drop = Minus(drop, Rate(base, t.drop, opposite));
  for (uint32_t i = 0; i < 8; ++i) {
    if (i == held) continue;
    auto& meter = Meter(base, input, first + i);
    const int16_t value = int16_t(Minus(meter, Rate(base, t.decay, i)));
    meter = uint16_t(value);
    if (value < 0) meter = 0;
  }
  auto& build = Meter(base, input, first + held);
  const uint16_t value = build;
  if (int16_t(value) >= 1536) {
    build = 1536;
  } else if (int16_t(value) > 1024) {
    build = uint16_t(value + 1);
  } else {
    build = uint16_t(uint32_t(Rate(base, t.build, held)) + value);
  }
  for (uint32_t i = 0; i < 8; ++i) {
    if (i == held) continue;
    auto& meter = Meter(base, input, first + i);
    if (int16_t(uint16_t(meter)) < threshold) meter = 0;
  }
}

// Both levers drain to no lower than 0 (the lockout cases).
void DrainBoth(uint8_t* base, uint32_t input, const game::ChargeTable& t) {
  for (uint32_t i = 0; i < 8; ++i) {
    for (uint32_t first : {18u, 26u}) {
      auto& meter = Meter(base, input, first + i);
      const int16_t value = int16_t(Minus(meter, Rate(base, t.decay, i)));
      meter = uint16_t(value);
      if (value < 0) meter = 0;
    }
  }
}

// sub_821F9DE0: the charge meters. `hold` is sub_821FB390's second flag.
void UpdateCharge(uint8_t* base, uint32_t input_address, bool hold) {
  const uint32_t game_address = GameAddress(base);
  auto& in = *game::GuestPtr<game::RobotInput>(base, input_address);
  const auto& t = *game::GuestPtr<game::ChargeTable>(base, game::kChargeTable);
  const bool busy = *game::GuestPtr<be<int32_t>>(base, game_address + kGameBusy) != 0;
  if (!busy) {
    const uint8_t lockout = in.lockout;
    if (lockout != 0) {
      if (!hold) {
        in.lockout = 0;
        return;
      }
      if (lockout > 1) in.lockout = uint8_t(lockout - 1);
      in.unk_0C = 0;
      if (in.lockout >= 128) return;
      DrainBoth(base, input_address, t);
      return;
    }
    if (hold) {
      in.lockout = 255;
      in.unk_0C = 0;
      return;
    }
    const uint16_t code = in.direction;
    if (code == 518 || code == 262 || code == 774 || code == 517 || code == 519) {
      const uint8_t count = in.unk_0C;
      if (count > 1) {
        in.unk_0C = uint8_t(count - 1);
      } else if (count == 0) {
        in.unk_0C = 255;
        in.angle = -1;
      }
      in.lockout = 0;
      DrainBoth(base, input_address, t);
      return;
    }
  }
  const uint16_t code = in.direction;
  in.unk_0C = 0;
  in.lockout = 0;
  ChargeLever(base, input_address, 18, code >> 8, t);
  ChargeLever(base, input_address, 26, code & 0xFF, t);
}

// --- sub_821FA290: the lever vectors -----------------------------------------
//
// Bit-exact with the recompiled original: each PowerPC float instruction is
// one call below, in the original's order (single-precision results are
// rounded through float, fmadd is std::fma), and the file is compiled with
// contraction off so no two of them get fused.

double Mul(double a, double b) { return a * b; }
double Add(double a, double b) { return a + b; }
double Sub(double a, double b) { return a - b; }
double Fma(double a, double b, double c) { return std::fma(a, b, c); }                // fmadd
double NegFmaSub(double a, double b, double c) { return -std::fma(a, b, -c); }       // fnmsub
double Single(double a) { return double(float(a)); }                                 // frsp
double SingleSqrt(double a) { return double(float(std::sqrt(a))); }                  // fsqrts

// fctiwz as the recompiler does it: NaN and too small give INT_MIN, too big INT_MAX.
int32_t TruncToInt(double v) {
  if (std::isnan(v)) return INT32_MIN;
  if (v >= double(INT32_MAX)) return INT32_MAX;
  if (v < -2147483648.0) return INT32_MIN;
  return int32_t(v);
}

double GuestDouble(const uint8_t* base, uint32_t address) {
  return std::bit_cast<double>(uint64_t(*game::GuestPtr<be<uint64_t>>(base, address)));
}
double GuestFloat(const uint8_t* base, uint32_t address) {
  return double(std::bit_cast<float>(uint32_t(*game::GuestPtr<be<uint32_t>>(base, address))));
}

// The constants it reads (values as found: 1.414..., 2.0, 1/6144, 1.0, -1.0,
// 0.05, -0.05, 0.0, 70, 1/5120, 0.01, 0.7, 100, 70, 4096, 0.2, -0.2, 0.65).
struct MovementConstants {
  double diagonal, straight, spread_scale, spread_max, spread_min, dead_hi, dead_lo, zero;
  double scale_low_end, sum_scale, scale_ramp, scale_low, scale_high_end, scale_ramp_start;
  double change_scale, spread_on, spread_on_neg, spread_angle;
  explicit MovementConstants(const uint8_t* base)
      : diagonal(GuestDouble(base, 0x8203AE48)),
        straight(GuestDouble(base, 0x82007C00)),
        spread_scale(GuestDouble(base, 0x8203AE40)),
        spread_max(GuestFloat(base, 0x82000958)),
        spread_min(GuestFloat(base, 0x82001370)),
        dead_hi(GuestDouble(base, 0x82016318)),
        dead_lo(GuestDouble(base, 0x8203AE38)),
        zero(GuestFloat(base, 0x8200095C)),
        scale_low_end(GuestFloat(base, 0x82017830)),
        sum_scale(GuestDouble(base, 0x8203AE30)),
        scale_ramp(GuestDouble(base, 0x820217A0)),
        scale_low(GuestDouble(base, 0x8203AE28)),
        scale_high_end(GuestFloat(base, 0x82001514)),
        scale_ramp_start(GuestDouble(base, 0x8203AE20)),
        change_scale(GuestDouble(base, 0x8203AE18)),
        spread_on(GuestDouble(base, 0x820173B0)),
        spread_on_neg(GuestDouble(base, 0x82018FD8)),
        spread_angle(GuestDouble(base, 0x8203AE10)) {}
};

struct Vector {
  double x, y;
};

// One lever's charge meters (directions 0-7) as an (x, y) vector: straight
// directions weigh `straight`, diagonals `diagonal`.
Vector LeverVector(const be<uint16_t> (&meters)[8], const MovementConstants& k) {
  double m[8];
  for (int i = 0; i < 8; ++i) m[i] = double(int16_t(uint16_t(meters[i])));
  const double a = Mul(m[1], k.diagonal), b = Mul(m[3], k.diagonal);
  const double c = Mul(m[7], k.diagonal), d = Mul(m[5], k.diagonal);
  const double p = Fma(m[0], k.straight, a), q = Fma(m[2], k.straight, b);
  const double t1 = Sub(p, b), t2 = Add(q, a);
  const double u = NegFmaSub(m[4], k.straight, t1), v = Sub(c, t2);
  const double w = Sub(u, d), z = Fma(m[6], k.straight, v);
  return {Single(Add(w, c)), Single(Add(z, d))};
}

// Clamped to [min, max], then squared keeping the sign, 0 inside the dead zone.
double SignedSquare(double v, const MovementConstants& k) {
  if (v > k.spread_max) {
    v = k.spread_max;
  } else if (v < k.spread_min) {
    v = k.spread_min;
  }
  if (v > k.dead_hi) return Single(Mul(v, v));
  if (v < k.dead_lo) return -Single(Mul(v, v));
  return k.zero;
}

// Follows a change: jumps to a bigger one, else eases toward 0 by 256.
void TrackChange(be<uint16_t>& field, int32_t change) {
  const int16_t current = int16_t(uint16_t(field));
  if ((change > 0 && current < change) || (change < 0 && current > change)) {
    field = uint16_t(change);
  } else if (current > 256) {
    field = uint16_t(current - 256);
  } else if (current < -256) {
    field = uint16_t(current + 256);
  } else {
    field = 0;
  }
}

// A robot parameter as the original reads it: the low half of a word, signed.
int32_t Param(const uint8_t* base, uint32_t params, uint32_t offset) {
  return int16_t(uint16_t(uint32_t(*game::GuestPtr<be<uint32_t>>(base, params + offset))));
}
bool Within(int32_t v, int32_t limit) { return v > -limit && v < limit; }

// The parameter that scales `spread`, by state, buttons and the second reference value.
int32_t SpreadParam(const uint8_t* base, uint32_t params, const game::RobotControl& c,
                    int32_t yaw, bool busy, bool spread_input) {  // yaw: relative2
  const auto P = [&](uint32_t offset) { return Param(base, params, offset); };
  const bool no_buttons = c.input.hold_0002 == 0 && c.input.hold_0200 == 0;
  const bool flagged = (uint32_t(c.movement_flags) & 21) != 0;
  if (!busy) {
    if (c.movement_state == 19 && c.movement_substate == 0) {
      return c.unk_090 ? P(452) : Within(yaw, 6144) ? P(448) : P(444);
    }
    if (flagged) return Within(yaw, 4096) ? P(464) : Within(yaw, 6144) ? P(460) : P(456);
    if (no_buttons) return c.unk_090 ? P(428) : Within(yaw, 6144) ? P(424) : P(420);
    return c.unk_090 ? P(440) : Within(yaw, 6144) ? P(436) : P(432);
  }
  if (spread_input) {
    if (c.movement_state == 19 && c.movement_substate == 0) {
      return c.unk_090 ? P(716) : Within(yaw, 6144) ? P(708) : P(700);
    }
    if (flagged) return Within(yaw, 4096) ? P(740) : Within(yaw, 6144) ? P(732) : P(724);
  } else if (flagged) {
    // (In this case the original tests 6144 before 4096, so 724 is never picked.)
    return Within(yaw, 6144) ? P(732) : Within(yaw, 4096) ? P(724) : P(740);
  }
  if (no_buttons) return c.unk_093 ? P(668) : Within(yaw, 6144) ? P(660) : P(652);
  return c.unk_093 ? P(692) : Within(yaw, 6144) ? P(684) : P(676);
}

// sub_822A73D8 (original): an angle from (f1, f2), upper half of the result.
uint32_t Angle(const PPCContext& frame, uint8_t* base, double f1, double f2) {
  PPCContext call = frame;
  call.f1.f64 = f1;
  call.f2.f64 = f2;
  __imp__sub_822A73D8(call, base);
  return call.r3.u32;
}

// sub_821FA290. `frame` is its caller's; it adds its own 176-byte frame.
void LeverMovement(PPCContext frame, uint8_t* base, uint32_t input_address,
                   uint32_t control_address, uint32_t params) {
  frame.r1.u64 = frame.r1.u32 - 176;
  frame.fpscr.disableFlushMode();
  const uint32_t game_address = GameAddress(base);
  auto& c = *game::GuestPtr<game::RobotControl>(base, control_address);
  game::RobotInput& in = c.input;
  const MovementConstants k(base);

  const Vector first = LeverVector(in.charge[0], k);
  const Vector second = LeverVector(in.charge[1], k);
  const double spread = SignedSquare(Single(Mul(Single(Sub(second.x, first.x)), k.spread_scale)), k);

  const double sum_y = Single(Add(second.y, first.y));
  const double diff_y = Single(Sub(first.y, second.y));
  const double sum_x = Single(Add(second.x, first.x));
  const double scale = GuestFloat(base, control_address + game::kControlScale);
  double out_x = Single(Mul(sum_x, k.sum_scale));
  double out_y = Single(Mul(-sum_y, k.sum_scale));
  double out_diff = Single(Mul(-diff_y, k.sum_scale));
  if (scale < k.scale_low_end) {
    out_y = Single(Mul(out_y, k.scale_low));
    out_diff = Single(Mul(out_diff, k.scale_low));
    out_x = Single(Mul(out_x, k.scale_low));
  } else if (scale < k.scale_high_end) {
    const double f = Fma(Sub(scale, k.scale_ramp_start), k.scale_ramp, k.scale_low);
    out_y = Single(Mul(f, out_y));
    out_diff = Single(Mul(f, out_diff));
    out_x = Single(Mul(f, out_x));
  }

  const double old_y = double(float(in.sum_y));
  const double old_x = double(float(in.sum_x));
  TrackChange(in.sum_y_change, TruncToInt(Mul(Single(Sub(old_y, out_y)), k.change_scale)));
  TrackChange(in.sum_x_change, TruncToInt(Mul(Single(Sub(old_x, out_x)), k.change_scale)));
  const double old_length = SingleSqrt(Single(Fma(old_x, old_x, Single(Mul(old_y, old_y)))));
  const double new_length = SingleSqrt(Single(Fma(out_x, out_x, Single(Mul(out_y, out_y)))));
  TrackChange(in.length_change, TruncToInt(Mul(Single(Sub(old_length, new_length)), k.change_scale)));
  in.sum_y = float(out_y);
  in.spread_sign = 0;
  in.diff_y = float(out_diff);
  in.sum_x = float(out_x);

  // The two reference values relative to the robot's (absolute while Game
  // +0x6164 is set); the second picks among the spread parameters, the first
  // among the third set's lift parameters.
  const bool busy = *game::GuestPtr<be<int32_t>>(base, game_address + kGameBusy) != 0;
  const auto control_u16 = [&](uint32_t offset) {
    return uint16_t(*game::GuestPtr<be<uint16_t>>(base, control_address + offset));
  };
  const int32_t relative2 = int16_t(busy ? control_u16(game::kControlRef2)
                                         : uint16_t(control_u16(game::kControlRef2) - c.facing));
  const int32_t relative1 = int16_t(busy ? control_u16(game::kControlRef1)
                                         : uint16_t(control_u16(game::kControlRef1) - c.unk_006));

  // The levers pointing opposite ways sideways.
  const bool spread_input = (first.x < k.spread_on_neg && second.x > k.spread_on) ||
                            (first.x > k.spread_on && second.x < k.spread_on_neg);
  const bool sideways = std::fabs(out_y) < k.scale_ramp && spread_input;
  if (sideways) in.direction_class2 = 255;
  const int32_t parameter = SpreadParam(base, params, c, relative2, busy, sideways);
  const int32_t spread_amount = TruncToInt(Single(Mul(Single(double(parameter)), spread)));
  in.spread = spread_amount;
  if (sideways) {
    if (spread_amount > 0) {
      in.spread_sign = 1;
    } else if (spread_amount < 0) {
      in.spread_sign = -1;
    }
  }

  in.unk_0E = 255;
  if (busy) {
    if (in.hold_0200 != 0 && in.hold_0002 != 0) {
      const double lift = SignedSquare(Single(Mul(Single(Sub(second.y, first.y)), k.spread_scale)), k);
      const auto P = [&](uint32_t offset) { return Param(base, params, offset); };
      int32_t value;
      if ((uint32_t(c.movement_flags) & 21) != 0) {
        value = Within(relative1, 4096) ? P(736) : Within(relative1, 6144) ? P(728) : P(720);
      } else if (c.unk_092 != 0) {
        value = P(664);
      } else {
        value = (relative1 > -9216 && relative1 < 3072) ? P(656) : P(648);
      }
      in.y_spread = TruncToInt(Single(Mul(Single(double(value)), lift)));
    } else {
      const uint16_t state = c.movement_state;
      if (state == 0 || state == 1 || state == 21) {
        const uint16_t code = in.direction;
        const int32_t v = in.unk_0F;
        if (code == 262 || code == 774 || code == 518 || code == 517 || code == 519) {
          if (v < 0) {
            in.unk_0F = 1;
          } else if (v < 127) {
            in.unk_0F = int8_t(v + 1);
          }
          in.unk_0E = 0;
        } else if (code == 1537 || code == 1539 || code == 1538 || code == 1282 || code == 1794) {
          if (v > 0) {
            in.unk_0F = -1;
          } else if (v > -127) {
            in.unk_0F = int8_t(v - 1);
          }
          in.unk_0E = 1;
        } else if (v > 40 || v < -40) {
          in.unk_0F = int8_t(v / 2);  // rounds toward 0, as srawi + addze
        } else if (v > 10) {
          in.unk_0F = int8_t(v - 2);
        } else if (v < -10) {
          in.unk_0F = int8_t(v + 2);
        } else {
          in.unk_0F = 0;
        }
      }
    }
  }

  in.latched_class = 255;
  in.angle = -1;
  const double spread_size = std::fabs(spread);
  if (!(spread_size < k.spread_angle) || in.lockout == 0) {
    if (spread_size > k.spread_on) {
      const uint32_t a = (uint32_t(int32_t(Angle(frame, base, out_y, out_x)) >> 1)) & 0x7FFF;
      if (a > 4096 && a <= 8192) {
        in.angle = 16384;
      } else if (a > 8192 && a < 24576) {
        in.angle = 32768;
      } else if (a >= 24576 && a < 28672) {
        in.angle = 49152;
      } else {
        in.angle = 0;
      }
    } else if (in.direction != 0xFFFF) {
      in.angle = int32_t(Angle(frame, base, out_y, out_x) & 0xFFFF);
    }
  }

  if (in.direction == 0xFFFF) {
    const uint8_t ticks = in.no_direction_ticks;
    in.same_direction_ticks = 0;
    if (ticks < 255) in.no_direction_ticks = uint8_t(ticks + 1);
    return;
  }
  if (in.direction != in.previous_direction) {
    in.same_direction_ticks = 0;
  } else if (in.same_direction_ticks < 255) {
    in.same_direction_ticks = uint8_t(in.same_direction_ticks + 1);
  }
  const uint8_t same = in.same_direction_ticks;
  in.no_direction_ticks = 0;
  if (same < 11 && uint32_t(int32_t(in.angle)) <= 65535) {
    in.held_angle = int32_t(in.angle);
  } else {
    in.held_angle = -1;
  }
}

void SetChargeTables(game::ChargeTable& t, uint32_t table) {
  t.drop = table;
  t.decay = table;
  t.build = table;
}

// sub_821FB390.
void RobotInputTick(PPCContext& ctx, uint8_t* base) {
  const uint32_t control_address = ctx.r3.u32;
  const uint32_t game_address = GameAddress(base);
  auto& g = *game::GuestPtr<game::Game>(base, game_address);
  auto& c = *game::GuestPtr<game::RobotControl>(base, control_address);
  game::RobotInput& in = c.input;
  const uint32_t input_address = control_address + uint32_t(offsetof(game::RobotControl, input));

  // The side's channels: first lever, second lever, extra.
  const bool side2 = control_address == game_address + kControl2;
  const uint32_t first_channel = side2 ? 4 : 0, second_channel = side2 ? 5 : 1;
  const uint32_t extra_channel = side2 ? 6 : 2;
  auto* channels = reinterpret_cast<game::InputChannel*>(g.levers);
  const uint32_t params = c.params;

  const uint16_t last_direction = in.direction;
  in.held = (uint32_t(channels[second_channel].now & 0xFF) << 8) |
            uint32_t(channels[first_channel].now & 0xFF);
  in.pressed = (uint32_t(channels[second_channel].pressed & 0xFF) << 8) |
               uint32_t(channels[first_channel].pressed & 0xFF);
  in.previous_direction = last_direction;

  const int16_t step = int16_t(uint16_t(g.scene_step));
  if (step == 20) {
    // the direction stays as it was
  } else if (step == 19) {
    if (control_address == game_address + kControl1) {
      in.direction = ScriptedDirection(base, 0);
    } else if (side2) {
      in.direction = ScriptedDirection(base, 1);
    }
  } else {
    switch (c.mode) {
      case 1:
      case 4: {
        PPCContext frame = ctx;
        frame.r1.u64 = frame.r1.u32 - 160;  // sub_821FB390's frame
        AiInput(frame, base, control_address);
        break;
      }
      case 8:
        in.held = 0;
        in.pressed = 0;
        in.direction = 0xFFFF;
        in.hold_extra = 0;
        break;
      case 5:
        PlaybackDirection(base, control_address);
        break;
      default: {
        // The player: the tables at fixed shifts (4 and 12).
        const uint32_t held = in.held;
        const uint32_t high = Lookup(base, game::kDirectionLookupFirst).code[(held >> 4) & 15];
        const uint32_t low = Lookup(base, game::kDirectionLookupSecond).code[(held >> 12) & 15];
        in.direction = uint16_t((high << 8) | low);
        break;
      }
    }
  }

  if (in.direction != in.previous_direction) {
    in.direction_changed = c.movement_state == 19 ? 1 : 2;
  } else {
    in.direction_changed = 0;
  }

  const uint8_t last_class = in.direction_class;
  in.direction_class = 255;
  ClassifyDirection(in);

  // Each lever's sign from its byte of the code.
  const uint16_t code = in.direction;
  int first_sign = 0, second_sign = 0;
  switch (code & 0xFF00) {
    case 0x100: case 0x200: case 0x300: first_sign = 1; break;
    case 0x500: case 0x600: case 0x700: first_sign = -1; break;
  }
  switch (code & 0xFF) {
    case 1: case 2: case 3: second_sign = -1; break;
    case 5: case 6: case 7: second_sign = 1; break;
  }
  const bool signs = (first_sign < 0 && second_sign <= 0) || (first_sign == 0 && second_sign < 0);
  const bool busy = *game::GuestPtr<be<int32_t>>(base, game_address + kGameBusy) != 0;
  bool hold = false, lever = false;
  if (busy) {
    hold = first_sign < 0 && second_sign < 0;
    lever = signs;
  } else if (code == 1537 || code == 1538 || code == 1539 || code == 1282 || code == 1794) {
    hold = true;
    in.direction_class = last_class;
    lever = true;
  } else {
    lever = signs;
  }

  // Charge rates for this tick.
  auto& table = *game::GuestPtr<game::ChargeTable>(base, game::kChargeTable);
  uint32_t set = params + 120;
  if (busy) {
    set = params + 320;
  } else if ((uint32_t(c.movement_flags) & 0x3F) != 0) {
    set = params + 220;
  }
  const auto& rates = *game::GuestPtr<game::ChargeParams>(base, set);
  table.drop = set + uint32_t(offsetof(game::ChargeParams, drop));
  table.threshold = rates.threshold;
  table.build = set + uint32_t(offsetof(game::ChargeParams, build));
  table.decay = set + uint32_t(offsetof(game::ChargeParams, decay));
  const uint8_t robot = c.robot_id;
  if (robot == 11 && int16_t(uint16_t(c.unk_1C2)) > 0) SetChargeTables(table, game::kChargeFixedC);
  if ((robot == 4 || robot == 11 || robot == 12) && uint16_t(c.unk_1C2) >= 32768) {
    SetChargeTables(table, game::kChargeFixedB);
  }
  if (robot == 0 && int16_t(uint16_t(c.unk_1C2)) > 0) {
    table.build = game::kChargeQuarterBuild;
    table.decay = game::kChargeQuarterDecay;
    table.drop = game::kChargeQuarterDrop;
    const auto& normal = *game::GuestPtr<game::ChargeParams>(base, params + 120);
    auto* build = game::GuestPtr<be<int32_t>>(base, game::kChargeQuarterBuild);
    auto* decay = game::GuestPtr<be<int32_t>>(base, game::kChargeQuarterDecay);
    auto* drop = game::GuestPtr<be<int32_t>>(base, game::kChargeQuarterDrop);
    for (int i = 0; i < 8; ++i) {
      build[i] = int32_t(normal.build[i]) / 4;  // rounds toward 0, as srawi + addze
      decay[i] = int32_t(normal.decay[i]) / 4;
      drop[i] = int32_t(normal.drop[i]) / 4;
    }
  }
  if (c.unk_1BE != 0) SetChargeTables(table, game::kChargeFixedA);

  if (in.unk_0C != 0) in.unk_0D = uint8_t(-in.unk_0C);
  if (lever) {
    const uint8_t timer = in.lever_timer;
    if (timer > 1) {
      in.lever_timer = uint8_t(timer - 1);
    } else if (timer == 0) {
      in.lever_timer = 255;
    }
  } else {
    in.lever_timer = 0;
  }

  UpdateCharge(base, input_address, hold);
  {
    PPCContext movement = ctx;
    movement.r1.u64 = movement.r1.u32 - 160;  // sub_821FB390's frame
    LeverMovement(movement, base, input_address, control_address, params);
  }

  // Button timers (held / pressed as the movement code left them).
  if (in.fired_0100 != 0) {
    const uint8_t fired = uint8_t(in.fired_0100 - 1);
    const bool held_0100 = (uint32_t(in.held) & 0x100) != 0;
    in.fired_0100 = fired;
    if (held_0100 && fired == 0) in.fired_0100 = 1;
  }
  if (in.fired_0001 != 0) in.fired_0001 = uint8_t(in.fired_0001 - 1);
  if (in.fired_both != 0) in.fired_both = uint8_t(in.fired_both - 1);
  if (in.hold_0001 > 1) in.hold_0001 = uint8_t(in.hold_0001 - 1);
  if (in.hold_0100 > 1) in.hold_0100 = uint8_t(in.hold_0100 - 1);
  const uint32_t pressed = in.pressed;
  if (pressed & 0x100) {
    in.hold_0100 = 255;
  } else if ((uint32_t(in.held) & 0x100) == 0) {
    in.fired_0100 = 0;
    in.hold_0100 = 0;
  }
  if (pressed & 0x001) {
    in.hold_0001 = 255;
  } else if ((uint32_t(in.held) & 0x001) == 0) {
    in.fired_0001 = 0;
    in.hold_0001 = 0;
  }
  if (in.hold_0100 > 251 && in.hold_0001 > 251) {
    in.hold_0001 = 0;
    in.hold_0100 = 0;
    in.fired_both = 255;
  } else if ((uint32_t(in.held) & 0x001) == 0) {
    in.fired_both = 0;
  }
  if (in.hold_0100 <= 253 && in.hold_0100 != 0) {
    in.fired_0100 = 255;
    in.hold_0100 = 0;
  }
  if (in.hold_0001 <= 253 && in.hold_0001 != 0) {
    in.fired_0001 = 255;
    in.hold_0001 = 0;
  }
  if (c.mode == 0) {
    if (channels[extra_channel].pressed & 0x20) {
      in.hold_extra = 255;
    } else if (channels[extra_channel].now & 0x20) {
      if (in.hold_extra > 1) in.hold_extra = uint8_t(in.hold_extra - 1);
    } else {
      in.hold_extra = 0;
    }
  }
  if (pressed & 0x002) {
    in.hold_0002 = 255;
  } else if (uint32_t(in.held) & 0x002) {
    if (in.hold_0002 > 1) in.hold_0002 = uint8_t(in.hold_0002 - 1);
  } else {
    in.hold_0002 = 0;
  }
  if (pressed & 0x200) {
    in.hold_0200 = 255;
  } else if (uint32_t(in.held) & 0x200) {
    if (in.hold_0200 > 1) in.hold_0200 = uint8_t(in.hold_0200 - 1);
  } else {
    in.hold_0200 = 0;
  }

  // Latch the direction class when a button press meets the state's rule.
  const uint8_t second = in.hold_0200, first = in.hold_0002;
  const uint16_t state = c.movement_state;
  bool latch = false;
  const bool just_one = (second == 255 && first == 0) || (second == 0 && first == 255);
  if (just_one && in.lockout == 0 &&
      (state == 0 || state == 1 || state == 19 || state == 12)) {
    latch = true;
  } else {
    if (!busy) {
      const bool fresh = (second > 244 && first == 0) || (second == 0 && first > 244);
      if (fresh && in.lockout == 0 && (state == 0 || state == 1)) latch = true;
    } else {
      const bool fresh = (second > 240 && first == 0) || (second == 0 && first > 240);
      if (fresh && state == 0) latch = true;
    }
    if (!latch && ((second != 0) != (first != 0)) && (state == 5 || state == 16 || state == 10)) {
      latch = true;
    }
    if (!latch && in.direction_class != 255 && in.lockout == 0) {
      const uint8_t a = in.hold_0002, b = in.hold_0200;
      if (((a == 255 && b == 0) || (a == 0 && b == 255)) && state == 2) latch = true;
    }
  }
  if (latch) in.latched_class = in.direction_class;

  if (second > 249 && first > 249) {
    if (in.both_held_timer > 1) in.both_held_timer = uint8_t(in.both_held_timer - 1);
    if (in.both_held_timer == 0) in.both_held_timer = 255;
  } else if (second != 0 && first != 0) {
    if (in.both_held_timer > 1) in.both_held_timer = uint8_t(in.both_held_timer - 1);
  } else {
    in.both_held_timer = 0;
  }
}

// decomp_verify: ClassifyDirection against sub_821F9BC8 for every code, with
// the class bytes starting at a marker so "left as is" is checked too.
void VerifyClassify(uint8_t* base, uint32_t input_address) {
  const uint32_t at = input_address;
  Verifier verifier("sub_821F9BC8", base,
                    {{at + 2, 2, "direction classes"}, {at + 0x1E, 2, "direction"}});
  auto& in = *game::GuestPtr<game::RobotInput>(base, at);
  for (uint32_t code = 0; code < 0x10000; ++code) {
    verifier.Check(
        [&] {
          in.direction = uint16_t(code);
          in.direction_class = 0x5A;
          in.direction_class2 = 0xA5;
        },
        [&] {
          PPCContext c{};
          c.r3.u64 = at;
          __imp__sub_821F9BC8(c, base);
          return int64_t(0);
        },
        [&] {
          ClassifyDirection(in);
          return int64_t(0);
        },
        [&] { return fmt::format("code {}", code); });
  }
  verifier.Finish();
}

// Compares the rewrite with the original on this call (decomp_verify_calls).
void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = GameAddress(base);
  if (!game_address) return;
  if (!verifier) {
    // The CPU's steps draw random numbers, so the generator is part of the
    // input too (generator 1, the one sub_822A7460 uses).
    const uint32_t generators = game::LoadGuestAddress(base, game::kRandomGenerators);
    const uint32_t random_state = game::LoadGuestAddress(base, generators + 16 * 1 + 4);
    verifier = new Verifier(
        "sub_821FB390", base,
        {{game_address, 0xD000, "game"},
         {game::kChargeTable, 0x70, "charge table"},
         {game::kAiControl2, game::kAiControl1 - game::kAiControl2 + 0x120, "CPU control"},
         {game::kRandomCalls, 4, "random calls"},
         {random_state, game::kRandomStateSize, "random state"}});
  }
  const uint32_t control = ctx.r3.u32;
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_821FB390(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        RobotInputTick(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const auto& c = *game::GuestPtr<game::RobotControl>(base, control);
        return fmt::format("side {} mode {} direction {:#06x}",
                           control == game_address + kControl2 ? 2 : 1, c.mode,
                           uint16_t(c.input.direction));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_821FB390) {
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_821FB390(ctx, base);
    return;
  }
  static bool swept = false;
  if (REXCVAR_GET(decomp_verify) && !swept) {
    swept = true;
    oratan::decomp::VerifyClassify(
        base, ctx.r3.u32 + uint32_t(offsetof(oratan::game::RobotControl, input)));
  }
  if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
  oratan::decomp::RobotInputTick(ctx, base);
}
