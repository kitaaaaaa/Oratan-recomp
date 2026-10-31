# Reference replays

Recordings made with `--replay_record`: controller reads and per-tick game-state
fingerprints only, no game content. They double as gameplay regression tests and as the
input for decomp checks on functions that depend on input history.

| File | What it covers | Ticks | Recorded |
|---|---|---|---|
| `feiyen_training.otr` | Boot, Training as Fei-Yen: every movement state (standing, walking, dashing forward / sideways / back, jumping, crouching, guarding, turbo) with left, right and centre weapon, taps and holds; quit to the title | 23,336 | 2026-10-30 |

One robot is enough for the input path: every robot uses the same inputs, and the attacks
only differ by robot and movement state. `sub_822A71B0` (input to command words) never reads
which robot is playing.

## Save state they need

A replay starts from boot, so the save data must match the one it was recorded with
(unlocks and options change the menus). The runtime keeps it in
`<Documents>\Virtual-On OT\` (Windows' Documents folder, which may be redirected):

| File | Bytes | SHA-256 |
|---|---|---|
| `B13EBABEBABEBABE\58410985\00000001\SYSTEM\SYSTEM.DAT` | 9,908 | `9211C9450C54FDF10CFF9E5A04CD4BC02E6A5CDA43AD099B488CB5A973F9D7F8` |
| `B13EBABEBABEBABE\58410985\Headers\00000001\SYSTEM.header` | 328 | `5FFA80168591891EDEBFF82A28324FF5FF14769181C6CAFB68E031649790068B` |
| `achievements\58410985.toml` | 368 | `BDCE6F3BB76A07BA12BE67BE917CA516D05C260119F9F05175D9DF1E717B04AF` |

The save files are the owner's and are not committed; a copy of that state is kept
outside the repository. Before replaying, check `SYSTEM.DAT` against the hash above and put
the copy back if it differs (playing can update it; the session above didn't). Display
settings don't matter (see `notes/DEV_DIARY.md` section 8).

## Running

```
oratan.exe --replay_play="tests\replays\feiyen_training.otr" --log_file=replay.log
```

Expected: `replay: stopped at tick 23336 (end of recording): 0 drifting ticks, 0 unmatched
reads`. Add `--decomp_verify_calls=true` to compare each decompiled input-history function
with the original on every call (`decomp verify sub_822A71B0: N calls, 0 mismatches`).
