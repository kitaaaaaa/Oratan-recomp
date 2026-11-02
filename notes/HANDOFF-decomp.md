# Decompilation handoff (2026-11-02)

The owner wants the whole game decompiled into readable, commented C++ that behaves exactly like
the original. File names match their contents; comments are accurate, short and plain. Work is
checked against the original before it is committed.

## Where things are

- `src/decomp/`: checked rewrites, one `REX_HOOK_RAW` per function. `robot_states.cpp` holds the
  driver (`sub_821F8168`), the movement states (0, 12, 15, 18, 19), sub_821E4D58's substates and
  the small state helpers (checked through `VerifySmall`). Other files: `robot_damage.cpp`,
  `robot_effects.cpp`, `robot_move_number.cpp`, `robot_weapon_gauges_and_ranges.cpp`,
  `robot_camera.cpp`, `robot_input.cpp`, `robot_motion.cpp`, ...
- `src/decomp_untested/`: written but unchecked (see its README). Not built.
- `notes/DEV_DIARY.md`: one entry per function: what it does, call counts, what the reference
  replay does not reach.
- `../work/DIARY.md` (outside the repo): the running log. `../work/decomp-tools/`: the scripts below.

## Robots by index (known so far)

0 Dordray, 4 / 11 / 12 the Apharmds (apb / aps / apc), 5 Bal-Keros, 9 Angelan, 10 Specineff,
15 Ajim, 17 Bradtos. Fei-Yen is the player in the reference replay; Angelan (9) is her opponent.

## How a function is done

1. Disassemble: `python dis.py <generated/default> sub_X` (from the recompiled sources' comments);
   `python info.py <generated/default> sub_X,sub_Y` gives size, callers and callees. Jump tables:
   read the `switch` in the generated `oratan_recomp.*.cpp`.
2. Write the rewrite. Callees run as the original (`__imp__sub_X`) through one register context
   (`Caller`), because the original sometimes passes leftover registers on (for example r6 kept
   across calls in sub_821E4BC0). Float code follows the generated code exactly (`fmadds` is
   `float(std::fma(...))`, `#pragma clang fp contract(off)`).
3. Hook it with a verifier (`Verifier::CheckCall`, see `decomp.h`): with
   `--decomp_verify_calls=true` every call runs the original and the rewrite from the same state
   and compares the declared memory ranges (plus a sample of the data section) and the result.
   Declare everything the function writes: stack records, pools outside the game block (for
   example the effect pools in `robot_effects.cpp`), the motion flags.
4. Build: `.\build.bat release` (PowerShell). Run the replay with the checks:
   `replay_once.ps1 -rec tests\replays\feiyen_training.otr -log X.log -extra "--audio_mute=true --decomp_verify_calls=true"`.
   Verify mode itself drifts from tick 12012 (10,351 drifting ticks): known, not the rewrites.
   A run takes about 7 minutes.
5. End to end without checks: must give 0 drifting ticks.
6. Plant a bug in a branch the replay reaches; the verifier must report mismatches. If a plant
   changes nothing, the branch isn't reached: plant elsewhere. Restore the file, then update its
   timestamp (Copy-Item keeps the old one, and ninja then skips the rebuild).
7. A temporary branch counter (a `TC()` macro after each `if`, printed every N calls) shows which
   branches the replay reaches; remove it before committing.
8. DEV_DIARY entry, commit (the owner allows a commit after each checked step), push.

Never commit the `tests/replays/replay_tick*.bin` dumps (replay drift output; leave them, don't
delete them). The untested rewrites stay in `src/decomp_untested/`, out of the build, until a
recording checks them. The VR work (`src/vr.*`, SDK patch 0007) is on hold and not committed yet:
the owner decides when. Always run the game with `--audio_mute=true`.

## What is left

- Not reached by the reference replay: the robots' specials (states 24-28), the other robots'
  branches everywhere, other stage kinds (footstep effects), anything under Game +24932 (which mode
  sets it is unknown). New recordings are needed, about 1-3 minutes each: one per robot using all
  moves and specials, a few fights on different stage kinds, getting knocked down / losing / fights
  past 50 seconds, and one fight per game mode to find what sets Game +24932.
- Skipped on purpose: drawing code (`sub_821FC8C0`, `sub_822518C8`, `sub_8224FE78`): the verifier
  and the replay hash don't see draw lists. Sound/string copies (`sub_821ED918`, `sub_821CFB90`).
- Not started: `sub_821E2FD8` (334), `sub_822583A8` (276, fires a weapon; writes object pools),
  `sub_821DFBA0`, `sub_821E3510` / `sub_821E39C0` / `sub_821E4088`, `sub_821F1BE0`, `sub_821F75E8`,
  state 25 (`sub_821F44A0`), and everything outside the robot code (menus, stages, rendering).
