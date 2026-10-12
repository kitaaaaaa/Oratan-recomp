// Hand-decompiled functions that turn player input into robot commands.
//
// sub_822A71B0: once per tick, builds both sides' robot command words.
//   - Stores kInputSource into Game::input_sources[0..3] (sub_82113A98).
//   - While kCommandCapture is active, only stores side 0's command word in
//     it and stops there: no robot commands this tick.
//   - Otherwise builds each side's command word from its PlayerActions
//     (sub_822AABE0). If the online source has a session (sub_8213D830, an
//     STL map lookup kept as the original), it replaces them: its own words
//     when it is ready (sub_8213D890 / sub_8213D8E0), 0 when not.
//   - Splits each word into the side's lever channels with their history
//     (sub_822A7070), and stores the words, without command::kStart, in
//     Game::robot_commands.
//   Nothing it calls reaches outside guest memory.
//
// The same hook runs local versus' steps around it (BeforeRobotCommands /
// AfterRobotCommands), with the rewrite or the original alike.

#include <rex/hook.h>
#include <rex/logging.h>

#include "../game_memory.h"
#include "../local_versus.h"
#include "decomp.h"

#include <cstdint>
#include <string>

#include <fmt/format.h>

REX_EXTERN(__imp__sub_822A71B0);
REX_EXTERN(__imp__sub_8213D830);

namespace oratan::decomp {
namespace {

namespace game = oratan::game;
namespace command = game::command;

// sub_82187C28: a player's action record; any side but 0 or 1 gets side 0's.
const game::PlayerActions& PlayerActionsOf(const uint8_t* base, int32_t side) {
  const auto* records = game::GuestPtr<game::PlayerActions>(base, game::kPlayerActions);
  return records[(side == 0 || side == 1) ? side : 0];
}

// sub_822AABE0: a side's command word from its player's actions.
uint16_t BuildCommand(const uint8_t* base, int32_t side) {
  struct Mapping {
    uint32_t action;
    uint16_t bit;
  };
  static constexpr Mapping kMappings[] = {
      {2, command::kStart},           {7, command::kExtra0},
      {9, command::kLeftButton1},     {8, command::kLeftButton0},
      {11, command::kRightButton1},   {10, command::kRightButton0},
      {3, command::kLeftDirection0},  {4, command::kLeftDirection1},
      {5, command::kLeftDirection2},  {6, command::kLeftDirection3},
      {12, command::kRightDirection0}, {13, command::kRightDirection1},
      {14, command::kRightDirection2}, {15, command::kRightDirection3},
  };
  const game::PlayerActions& actions = PlayerActionsOf(base, side);
  uint16_t word = 0;
  for (const Mapping& m : kMappings) {
    if (actions.Has(m.action)) word |= m.bit;
  }
  return word;
}

// sub_8213D830: whether the online source has a session (original code).
bool OnlineHasSession(PPCContext& ctx, uint8_t* base) {
  PPCContext call = ctx;
  __imp__sub_8213D830(call, base);
  return (call.r3.u32 & 0xFF) != 0;
}

// sub_8213D890: whether the online source's command words can be used.
bool OnlineReady(const uint8_t* base) {
  const auto* online = game::GuestPtr<game::OnlineInput>(base, game::kOnlineInput);
  return online->active != 0 && online->state >= 0 && online->ready != 0;
}

// sub_8213D8E0: a side's online command word, or 0 when not ready.
uint16_t OnlineCommand(const uint8_t* base, int32_t side) {
  const auto* online = game::GuestPtr<game::OnlineInput>(base, game::kOnlineInput);
  return OnlineReady(base) ? uint16_t(online->commands[side]) : 0;
}

// Copies the bits of `word` listed in `moves` (from, to) into a new value.
struct BitMove {
  uint16_t from;
  uint16_t to;
};
template <size_t N>
uint16_t MoveBits(uint16_t word, const BitMove (&moves)[N]) {
  uint16_t out = 0;
  for (const BitMove& m : moves) {
    if (word & m.from) out |= m.to;
  }
  return out;
}

void SetChannel(game::InputChannel& channel, uint16_t now) {
  const uint16_t previous = channel.now;
  channel.previous = previous;
  channel.now = now;
  channel.pressed = uint16_t(now & ~previous);
}

// sub_822A7070: splits a side's command word into its lever channels.
void UpdateLevers(game::Game& g, uint16_t word, int32_t side) {
  static constexpr BitMove kLeft[] = {
      {command::kLeftDirection0, 0x20}, {command::kLeftDirection1, 0x10},
      {command::kLeftDirection2, 0x80}, {command::kLeftDirection3, 0x40},
      {command::kLeftButton0, 0x02},    {command::kLeftButton1, 0x01},
  };
  static constexpr BitMove kRight[] = {
      {command::kRightDirection0, 0x20}, {command::kRightDirection1, 0x10},
      {command::kRightDirection2, 0x80}, {command::kRightDirection3, 0x40},
      {command::kRightButton0, 0x02},    {command::kRightButton1, 0x01},
  };
  static constexpr BitMove kExtra[] = {
      {command::kStart, 0x10},
      {command::kExtra0, 0x20},
      {command::kExtra1, 0x08},
      {command::kExtra2, 0x04},
  };
  game::LeverInput& levers = g.levers[side];
  SetChannel(levers.left, MoveBits(word, kLeft));
  SetChannel(levers.right, MoveBits(word, kRight));
  SetChannel(levers.extra, MoveBits(word, kExtra));
}

// sub_822A71B0.
void BuildRobotCommands(PPCContext& ctx, uint8_t* base) {
  // Like the original, no check for a missing Game (it runs only once one exists).
  game::Game& g =
      *game::GuestPtr<game::Game>(base, game::LoadGuestAddress(base, game::kGamePointer));
  for (auto& source : g.input_sources) source = game::kInputSource;

  auto* capture = game::GuestPtr<game::CommandCapture>(base, game::kCommandCapture);
  if (capture->active != 0) {
    capture->command = BuildCommand(base, 0);
    return;
  }

  uint16_t words[2] = {BuildCommand(base, 0), BuildCommand(base, 1)};
  if (OnlineHasSession(ctx, base)) {
    words[0] = OnlineCommand(base, 0);
    words[1] = OnlineCommand(base, 1);
  }
  for (int32_t side = 0; side < 2; ++side) UpdateLevers(g, words[side], side);
  for (int32_t side = 0; side < 2; ++side) {
    g.robot_commands[side] = uint16_t(words[side] & ~command::kStart);
  }
}

// Compares the rewrite with the original on this call (decomp_verify_calls).
void VerifyCall(PPCContext& ctx, uint8_t* base) {
  static Verifier* verifier = nullptr;
  const uint32_t game_address = game::LoadGuestAddress(base, game::kGamePointer);
  if (!game_address) return;
  if (!verifier) {
    auto field = [&](size_t offset) { return game_address + uint32_t(offset); };
    verifier = new Verifier(
        "sub_822A71B0", base,
        {{field(offsetof(game::Game, levers)), uint32_t(sizeof(game::Game::levers)),
          "game levers"},
         {field(offsetof(game::Game, robot_commands)),
          uint32_t(sizeof(game::Game::robot_commands)), "game robot commands"},
         {field(offsetof(game::Game, input_sources)), uint32_t(sizeof(game::Game::input_sources)),
          "game input sources"},
         {game::kCommandCapture, uint32_t(sizeof(game::CommandCapture)), "command capture"},
         {game::kPlayerActions, uint32_t(2 * sizeof(game::PlayerActions)), "player actions"},
         {game::kOnlineInput, uint32_t(sizeof(game::OnlineInput)), "online input"},
         {game::kControllerStates, uint32_t(4 * sizeof(game::ControllerState)),
          "controller states"},
         {game::kSideInputs, uint32_t(2 * sizeof(game::SideInput)), "side inputs"}});
  }
  verifier->CheckCall(
      [&] {
        PPCContext original = ctx;
        __imp__sub_822A71B0(original, base);
        return int64_t(0);
      },
      [&] {
        PPCContext rewrite = ctx;
        BuildRobotCommands(rewrite, base);
        return int64_t(0);
      },
      [&] {
        const game::Game& g = *game::GetGame(base);
        return fmt::format("commands before {:#06x} {:#06x}", uint16_t(g.robot_commands[0]),
                           uint16_t(g.robot_commands[1]));
      },
      600);
}

}  // namespace
}  // namespace oratan::decomp

REX_HOOK_RAW(sub_822A71B0) {
  oratan::BeforeRobotCommands(base);
  if (REXCVAR_GET(decomp_use_originals)) {
    __imp__sub_822A71B0(ctx, base);
  } else {
    if (REXCVAR_GET(decomp_verify_calls)) oratan::decomp::VerifyCall(ctx, base);
    oratan::decomp::BuildRobotCommands(ctx, base);
  }
  oratan::AfterRobotCommands(base);
}
