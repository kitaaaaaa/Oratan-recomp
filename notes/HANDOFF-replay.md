# Handoff: test input recording and replay (`src/replay.cpp`)

For the Claude instance on the owner's Windows PC. Written by the cloud session, which has no
game or build. Independent of `HANDOFF-decomp-step2.md` (different files and hooks), so do it
before, after, or between parts of that. Delete this file when done, and record the results in
`notes/DEV_DIARY.md`.

## What it is

A developer tool that records every controller read from boot to a file, and replays it
instead of the real controllers. Every game tick it also fingerprints the game state, so a replay
that drifts reports the first tick and which part of the state differs. It's the groundwork for
checking `sub_822A71B0` (input → robot commands, which depends on input history) and for
netplay, which needs the game to be deterministic.

**How it works:**
- Update `sub_821236E0` counts ticks from boot.
- After each read by `sub_821135C0`, the controller buffer `game::kControllerStates`
  (4 × 60 bytes) is saved to the file (recording) or overwritten from it (replay), read for read.
- After each tick, four regions are hashed: the battle state (first `replay_hash_bytes`, default
  `0xD000`), each robot record, and the side inputs + module byte + next stage.
- The full file format and logic are in the comments at the top of `src/replay.cpp`.

**Tested in the cloud** with the real `replay.cpp` against a toy game in fake guest memory:
- a recording replayed with different live input ends identical, with 0 drifts;
- injected randomness is caught at the exact tick and region, with a state dump;
- a different number of controller reads per tick is reported at the first tick where the
  counts differ;
- a truncated file stops cleanly.

It's syntax-checked against the SDK headers, but not built with the real SDK and not run with
the game.

## Settings

| Setting | Meaning |
|---|---|
| `--replay_record=FILE` | Record from boot to FILE |
| `--replay_play=FILE` | Replay FILE from boot instead of the controllers |
| `--replay_dump_tick=N` | Also write the hashed regions after tick N to `replay_tickN_<record\|replay>.bin` next to FILE (a replay also does this automatically at its first drift) |
| `--replay_hash_bytes=N` | How much of the battle state to fingerprint (default `0xD000`) |

Close the game with the window's X, so the recording gets its end marker. A crash still keeps
everything up to the last flush, every 600 ticks.

## Tests

1. **Build.** `src/replay.cpp` is already in `CMakeLists.txt`.
2. **Record:** `--replay_record=work/r1.otr`. Boot, go to Training, fight for about a minute,
   quit to the title, close the window. The log should say `replay: recorded N ticks`.
3. **Replay:** `--replay_play=work/r1.otr`, hands off the controller. Does the session replay by
   itself, through the menus and the fight? The log ends with
   `replay: stopped at tick N (...): D drifting ticks, U unmatched reads`.
4. **If it drifts,** find out why. The first drift is what matters; later ones usually follow
   from it.
   - Record again with `--replay_dump_tick=<first drift tick>`, then diff that file against the
     `replay_tick<N>_replay.bin` the replay wrote. The differing offsets say what changed.
   - Typical causes:
     - a random seed taken from the clock: look for the RNG and its seeding;
     - loading that finishes on a different tick: the drift starts right after a loading screen;
     - controller reads outside the Update thread: there's a log warning for this;
     - another thread writing game state.
   - **Unmatched reads** mean the game read the controllers a different number of times than when
     recording. That's a timing dependency worth understanding.
   - If only the **game** region drifts while both robots match, the first `0xD000` bytes may
     include heap pointers or timers that legitimately differ between runs. Narrow it down with
     the dumps and lower `replay_hash_bytes` or skip that range, and say which.
5. **Once a replay matches,** replay the same file with different display settings: another
   resolution, the native renderer on and off, split screen on. These shouldn't change gameplay,
   so the replay should still match. That turns replays into a gameplay regression test.
6. Optional: `--guest_vblank_multiplier=4` may replay faster, since the game paces itself on
   vertical blanks.

## Fixing a drift

If the cause is a clock-seeded random number generator, or another value read from outside the
game, the fix is to record that value too, as a new record type, and feed it back on replay.
Bump `kVersion` when the file format changes. If the cause is load timing, say so before working
around it: that's a design question, because netplay would hit the same problem. Options include
starting replays at fight start instead of boot.

## Report back

- Whether a Training session replays without drift. If not, the first drift tick, the regions,
  and the cause.
- Any fixes made, such as a recorded seed.
- Whether the display-setting replays still match.
- Then delete this file and add a "Replays" note to the dev diary.
