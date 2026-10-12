# Handoff: decomp step 3 (`sub_822A71B0`, checked by replays)

For the Claude instance on the owner's Windows PC. Written by the cloud session, which has no
game or build. Follow the conventions in `notes/DEV_DIARY.md` section 7. Do
`HANDOFF-netplay.md` part 0 (replay regression) first: this work relies on replays, and the
replay code was just reorganised. Delete this file when done, and record the results in the
dev diary.

## Goal

Decompile `sub_822A71B0`, which turns both sides' controller input into robot command words
once per tick. Once it's readable, we know exactly how input becomes dashes, jumps, weapons,
turbo moves and cancels. That knowledge feeds the speed option, netplay, porting Oratan's
movement to other games, and understanding `SideInput` and the robot records.

## 1. Reference recordings (do this first)

Record a small library with `--replay_record`. It doubles as the test set for this function and
as gameplay regression tests:

- **Training, one recording per robot** (or as many as practical): every button, dashes in all
  directions, jumps, turbo (left, right, centre) attacks, dash attacks, crouch and guard,
  V-turbo / special moves, and holding versus tapping.
- **One Arcade run** to the first or second stage.
- **One local versus session** with two controllers, so side 2's input path is exercised by a
  person, not the AI.
- **One long free-play session.**

Notes:
- **Save state:** a replay from boot assumes the same save data and settings, since unlocks
  and options change the menus. Find where the game's save and settings live, record with a
  known state (a separate clean save folder if the runtime allows it, or a documented backup),
  and write down how to reproduce it.
- **Where they live:** recordings are only controller bytes and fingerprints, with no game
  content, so they can be committed: put them in `tests/replays/`, with a `README.md` listing
  what each covers, its tick count, and the save state it needs.
- **Check each one:** replay every recording once right after making it (0 drifting ticks)
  before relying on it.

## 2. Scout the function

Read the generated code and disassembly, as for the earlier functions:

- **Size and callees.** If it's large, work bottom-up: decompile its leaf callees first, each
  checked the same way as below, and leave the outer function calling `__imp__` versions until
  its parts are done.
- **What it reads:** the controller buffer (`game::kControllerStates`), the side input records
  (`SideInput`: its 300 unknown bytes probably hold input history such as previous buttons,
  hold timers and stick state), the robot records, and anything else.
- **What it writes:** the command words (where, and what each bit means), and any history it
  updates.
- **Name what you learn** in `game_memory.h`: `ControllerState` fields, `SideInput` fields, the
  command word bits as an enum, all with `static_assert`s.

## 3. One hook: merge with local versus

`src/local_versus.cpp` hooks `sub_822A71B0`. Before the call it gives a side without a
controller one (`AssignMissingControllers`). After it, it applies `view_camera` and clears or
restores the CPU flags. As with `sub_8213CCD0`:

- move the hook into a decomp file, e.g. `src/decomp/robot_control.cpp`;
- export those two steps from `local_versus.cpp` (for example
  `oratan::BeforeRobotCommands(base)` / `oratan::AfterRobotCommands(base)`), and call them from
  the new hook around the rewrite (or around the original with `decomp_use_originals`);
- the verifier compares the rewrite with the original only, without those steps;
- **netplay depends on this:** `LocalVersusOn()` (local versus on, or netplay active) gates
  those steps. Keep that behaviour.

## 4. Check it with replays instead of a sweep

The function depends on input history, so constructed inputs don't fit. Check it on **every
call** while replaying the reference recordings:

- Add a mode to `decomp::Verifier`, or a thin wrapper, that uses **the current game state as the
  input**: capture the declared ranges at entry, run the original, restore, run the rewrite,
  compare. The PC session already suggested a `Verifier` input source fed by replays; this is
  that.
- **Declared ranges:** the controller buffer, both `SideInput` records, both robot records
  (whole), and everything else it writes. Keep the whole-section check on the first 50 calls of
  each session.
- **Developer setting:** something like `decomp_verify_calls` (check every call, not just the
  first), so it can stay off normally.
- Run every reference recording with it on and report the totals
  (`decomp verify sub_822A71B0: N calls, M mismatches`). As always, prove the check with a
  deliberately broken rewrite first.
- **Finally:** replay every recording with the rewrite active and the check off. Each must still
  match its recorded fingerprints with 0 drifting ticks. That's the end-to-end proof.

## Report back

- What the function does: the command word format, what `SideInput` holds, and the callees
  decompiled or left as `__imp__`.
- The recordings made, with their verify totals and end-to-end replay results.
- New `game_memory.h` names.
- How the call-by-call check turned out as a `Verifier` feature, and whether a "same call, same
  arguments" dry-run check was needed for any callee.
