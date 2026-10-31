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
//     runs the charge meters (sub_821F9DE0) and then the robot's movement
//     code (sub_821FA290, still the original), and afterwards counts the
//     button hold timers and latches the direction class on some presses.
//   Every robot goes through it the same way; what differs per robot is the
//   ChargeParams in its parameters and sub_821FA290's use of the result.
//
// The CPU player's own steps (eight functions under sub_82212A98) and
// sub_821FA290 are called as the original code, with the same arguments the
// original passes.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "decomp.h"

#include <cstdint>
#include <string>

#include <fmt/format.h>

REX_EXTERN(__imp__sub_821FB390);
REX_EXTERN(__imp__sub_821F9BC8);
REX_EXTERN(__imp__sub_821FA290);
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
    const uint8_t action = game::GuestPtr<game::RobotControl>(base, control)->action;
    if (action != 17 && action != 18) {
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
        in.unk_58 = -1;
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
  const uint8_t action = c.action;
  if (action == 11 && int16_t(uint16_t(c.unk_1C2)) > 0) SetChargeTables(table, game::kChargeFixedC);
  if ((action == 4 || action == 11 || action == 12) && uint16_t(c.unk_1C2) >= 32768) {
    SetChargeTables(table, game::kChargeFixedB);
  }
  if (action == 0 && int16_t(uint16_t(c.unk_1C2)) > 0) {
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
    movement.r1.u64 = movement.r1.u32 - 160;
    movement.r3.u64 = input_address;
    movement.r4.u64 = control_address;
    movement.r5.u64 = params;
    __imp__sub_821FA290(movement, base);
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
