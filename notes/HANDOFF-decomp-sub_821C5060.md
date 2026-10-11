# Handoff: decompile `sub_821C5060` (first hand-decompiled function)

For the Claude instance on the owner's Windows PC, which has the game files, the generated code
and the build. Written by a cloud session that has none of those. Delete this file when done,
and put what you found in `notes/DEV_DIARY.md` and a short results note.

## Goal

Replace the recompiled `sub_821C5060` with a readable C++ version that behaves identically,
**proven** by comparing it with the original on every input it can get. This is the project's
first decompiled function, so it also sets the conventions the next ones (`sub_8213CCD0`,
`sub_822A6A30`, then `sub_822A71B0`) will follow.

What we know: the robot loader `sub_821C5C30` passes every robot through `sub_821C5060`, which
swaps Bal-Bados for its stage-specific forms: Bal-Keros (robot 5) underwater, Bal-Baros
(robot 16) in zero gravity (`DEV_DIARY.md` 3.6, comment in `src/select_pages.cpp`). Bal-Bados'
own robot number and the stage numbers involved are not known yet.

## Step 0: merge and build the header first

Branch `ccr-e6e61bf3-k79306` has one commit that isn't on `main`: `cd009e8`, which adds
`src/game_memory.h` (the mapped guest data as C++ types) and moves `local_versus.cpp`,
`select_pages.cpp`, `frame_stats.cpp` and `debug_tools.cpp` onto it. It was only syntax-checked
(clang targeting MinGW, against the SDK headers), never built.

1. Merge it into `main` and build.
2. Quick regression check. Behaviour should be unchanged:
   - local versus: player 2 takes over the CPU robot; unticking it mid-match hands it back;
   - split screen in a Training battle, with swap sides;
   - Training LB/RB: stage page 2, Ajim ↔ Bradtos;
   - `--watch_u32`, F9 snapshot (writes `snapN_game.bin` in a battle);
   - the "game module N" log line on mode changes.
3. Fix any compile errors in place. Keep the header's `static_assert`s; if one fails, the layout
   is wrong, so find out why rather than deleting the check.

## Step 1: work out what the function really does

Read the generated C++ for `sub_821C5060` (in `generated/default/`) and its disassembly
(`oratan.exe --dump_image=work/image.bin`, then `python tools/disasm.py work/image.bin
0x821C5060 200`). Also look at its call site in `sub_821C5C30`. Establish:

- **Inputs:** which argument registers it reads (r3, r4, …), and which guest memory it reads.
  The stage might be an argument or a global, such as `game::kNextStage` (`0x83876DF4`).
- **Output:** the return value (r3) and any memory it writes.
- **Calls:** whether it calls other functions. If it does, note them; the readable version can
  call `__imp__sub_XXXXXXXX` for any it doesn't replace.
- **The table or switch:** which robot numbers and stage numbers it maps.

If it turns out not to be small and self-contained (large, many calls, writes lots of state),
stop and report back instead of decompiling it. We'll pick a different first function.

## Step 2: write the readable version

Conventions for this and all later decompiled functions:

- **Location:** `src/decomp/`, one file per game area. This one goes in
  `src/decomp/robot_loader.cpp`. Add it to `ORATAN_SOURCES` in `CMakeLists.txt`.
- **Shape:** a plain C++ function with a descriptive name, for example
  `uint8_t oratan::decomp::StageFormOf(uint8_t robot, int32_t stage)`. It takes and returns
  ordinary values; the hook does the register and memory plumbing. Add a comment with the
  original address and what it does, in the style of the rest of `src/`.
- **Data:** use `src/game_memory.h` for any guest memory it touches. Add anything new you learn:
  Bal-Bados' number in `robot_id`, the underwater and zero-gravity stages in `stage_id`, new
  addresses, all with comments.
- **Hook:** `REX_HOOK_RAW(sub_821C5060)` reads the inputs from `ctx`/memory, calls the readable
  function, and writes the result back exactly as the original does (r3, plus any memory
  writes). A hook that never calls `__imp__sub_821C5060` replaces the original.
- **Two developer cvars**, shared by all decompiled functions (put them in a small
  `src/decomp/decomp.h` / `decomp.cpp`):
  - `decomp_use_originals` (bool, default false): every decomp hook calls the original
    `__imp__` function instead. This is the escape hatch if a rewrite misbehaves.
  - `decomp_verify` (bool, default false): each decomp hook runs its verifier (step 3) the
    first time it's called, and logs the result.
- **Copyright:** don't commit generated code or disassembly listings, same as now. Describe
  the logic in words in comments and notes; the readable C++ is our own.

## Step 3: prove it matches

Run the original and the rewrite on the same inputs and compare. For this function that can be
**exhaustive**:

- Inside the hook, the first time it's called with `decomp_verify` on, take a copy of the live
  `ctx` (its stack pointer is valid there), and for every combination of inputs:
  - set the input registers (and any global it reads; save it first and restore it afterwards);
  - call `__imp__sub_821C5060(copy, base)` and record r3 and anything it writes;
  - call the readable version with the same inputs;
  - compare, and log every mismatch with its inputs.
- **Ranges:** every robot number the loader can pass (0–18 at least; wider if the original
  range-checks), and every stage number from −1 to well past 21. If out-of-range values can
  crash the original, limit the sweep to the values it handles, and say so in the notes.
- If the function writes memory, save those bytes before each original call and restore them
  after it, and compare what each version wrote.
- Log one summary line, for example
  `decomp verify sub_821C5060: 627 inputs, 0 mismatches`.
- Then play it as well: load Bal-Bados on its own stage (Bal-Keros underwater, Bal-Baros in
  zero gravity) and on a normal stage, in Arcade and Training, with `decomp_use_originals` off.

## Report back

- What the function does, in a few sentences: signature, inputs, the mapping it encodes.
- The verify summary line and the gameplay checks.
- Anything new added to `game_memory.h`.
- Anything that made this harder than expected, so the conventions can be adjusted before the
  next function.
- Then delete this handoff file.
