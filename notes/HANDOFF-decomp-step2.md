# Handoff: decomp step 2 (`sub_8213CCD0` + check helpers, then game flow)

For the Claude instance on the owner's Windows PC. Written by the cloud session, which has no
game files or build. Follow the conventions in `notes/DEV_DIARY.md` section 7, set by the first
decompile (`src/decomp/robot_loader.cpp`). Do Part A first; Part B builds on it. Delete this file
when both are done, and record the results in the dev diary.

---

## Part A: check helpers, then `sub_8213CCD0`

### A1. Shared helpers for functions that write memory

`sub_821C5060` only returned a value, and its verifier saved and restored seven fields by hand.
Every function from now on writes guest memory, so turn that into a helper in
`src/decomp/decomp.h` / `decomp.cpp`. Roughly:

- **A named list of guest ranges** a function reads or writes, e.g.
  `{game::kSideInputs, 2 * sizeof(game::SideInput), "side inputs"}`.
- **A snapshot of those ranges:** capture, restore, and compare against another snapshot,
  reporting the first differing byte as range name + offset + both values.
- **A verifier object:** counts inputs and mismatches, logs the first ~20 mismatches with their
  inputs, and prints the standard summary line `decomp verify sub_X: N inputs, M mismatches`.
- **An optional whole-section check** for writes outside the declared ranges. Snapshot the
  XEX's data section (`0x82890000`, size `0x01171DFC`; the same range F9 saves) after the
  original runs and after the rewrite runs, from the same restored state, and compare. It's
  about 18 MB per capture, so only do it for a sample of inputs (say the first 50), then fall
  back to the declared ranges for the full sweep.
  - Other guest threads (audio, streaming) can change memory meanwhile. So only report bytes
    where the original's and the rewrite's results **differ from each other**, and re-run a
    mismatch once before counting it.

**Prove the helper on known-good code first.** Switch `robot_loader.cpp`'s verifier over to it.
It must still report 7,765,800 inputs and 0 mismatches, and the deliberately broken rewrite
must still fail.

### A2. Decompile `sub_8213CCD0`

**What we know:** called with a side in r3 when a side is chosen in the main menu. It gives that
side the "active" controller (`SideInput::controller`) and sets the other side's to −1. Where
"active" comes from is unknown; probably whichever controller pressed Start. Establish, as for
the first function:
- inputs (registers, globals);
- every address it writes;
- what it calls.

**Watch out for side effects.** If it calls anything with effects outside guest memory (a menu
sound, a controller rumble, a kernel/XAM call), a sweep of thousands of calls would trigger
thousands of them. In that case:
- the rewrite calls the same `__imp__` function at the same point;
- the verifier checks the rewrite *would* make the same call with the same arguments, without
  making it (for example by stubbing it out during the sweep);
- or limit the sweep and say so.

**One hook per function: merge it with local versus.** `src/local_versus.cpp` already hooks
`sub_8213CCD0`. It runs the original, then, with `local_versus` on, gives the other side the
second controller. There can only be one hook, so:
- move it into a decomp file, e.g. `src/decomp/menu.cpp` (by game area: the main menu's side
  selection);
- have it run the rewrite (or the original with `decomp_use_originals`), then call a function
  exported from `local_versus.cpp` (something like `oratan::OnSideChosen(base, side)`) holding
  today's local-versus logic;
- the verifier compares the rewrite with the original only, without the local versus step.
- **Behaviour must be unchanged in all four combinations** of `local_versus` × `decomp_use_originals`.

**Sweep:** every side value the caller can pass, plus −1 and a few out-of-range values if the
original handles them safely. Every "active controller" value (−1, 0–3, edge values). Every
starting combination of both sides' `controller` fields (−1 to 3). Compare all declared ranges,
with the whole-section check on the first sample.

**Play test:**
- choose 1P and 2P from the main menu with one controller, and with two;
- with `local_versus` on and off;
- with `decomp_use_originals` on and off.

---

## Part B: game flow and a reliable "fight in progress" flag

### The problem it solves

In split screen, Arcade's **Continue** and **Game Over** screens run inside the battle module
(module 1), so they get drawn twice side by side. Split screen is active whenever
`IsBattleModule()` is true, and no reliable "a fight is in progress" flag has been found
(`notes/RESULTS-2026-10-07.md`, "Still open"). The pause menu showing in both halves is fine.

### Leads

- **`Game::scene_mode` / `scene_param` (+0x240 / +0x242):** found by the first decompile. They
  mark special situations: (3, 21), and (0, 31) during the Ending.
- **The bytes around +0x614D** (`record_side`): `+0x614C` (24908) stays 1 through Continue;
  `+0x614E` (24910) only drops at Game Over.
- **The module switcher** `sub_822A6A30`, and the module byte's getter `sub_822A6B70`.
- **The game pointer getter** `sub_822AC098`. Callers that then write `+0x240` / `+0x242` are
  the code that changes scenes.

### B1. Map the states

Log the candidates through a full Arcade flow with the existing watcher, for example:

```
--watch_u32=839C21BC,*839EEA98+240,*839EEA98+614C
```

Each entry is a 32-bit word, so `+240` covers mode and param together, and `+614C` covers
`614C`–`614F`. Add other candidates as you find them.

Play through:
- the stage intro, the fight, round end;
- a win and the next stage's intro;
- a loss into Continue, continuing (yes), then losing again into Game Over;
- Training (module 2) fights and their pause menu;
- Score Attack, if quick.

Build a table of which values mark "fighting" (intro and round transitions included or not,
your call) versus Continue / Game Over / results screens.

### B2. Find the code

Find the writers of the fields that turn out to matter: callers of `sub_822AC098` storing at
those offsets, and what the module switcher does on each transition. Name the fields in
`game_memory.h` once their meaning is clear, with `static_assert`s, and turn the known values
into enums.

### B3. Fix split screen

In `src/local_versus.cpp`'s Draw hook, make split screen active only while a fight is in
progress, using the flag or state from B1/B2. Check that:
- Continue and Game Over are drawn once, full screen, in Arcade;
- intros, fights, round ends and pause behave as you decided in B1;
- Training and Score Attack still split;
- the vblank multiplier still switches correctly (the log shows `Guest vblank multiplier: 1`
  after leaving a split fight).

### B4. Decompile the switcher, if it's small

If `sub_822A6A30` is mostly a dispatch (a switch over modules calling per-module functions),
write a readable version that calls the same `__imp__` functions. Verify it as a **dry run**:
for every module value and transition, check that the rewrite would call the same function with
the same arguments as the original, without actually switching modules during the sweep. If it's
more than a dispatch, document what it does in the diary and leave the rewrite for later.

---

## Report back

- Part A: what `sub_8213CCD0` does, its verify line, the play tests, and how the helper API
  turned out. Also the robot_loader verifier's result after moving it onto the helper.
- Part B: the state table, the fields named in `game_memory.h`, whether split screen's Continue
  / Game Over problem is fixed, and whether the switcher was decompiled.
- Anything that should change in the conventions before the next step, which is recorded input
  replay and then `sub_822A71B0`.
