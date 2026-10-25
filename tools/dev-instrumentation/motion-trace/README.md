# Motion trace (Fei-Yen animation extraction)

Records which animation the game plays each tick and the exact world matrix of every part of
the player's robot. Used to extract Fei-Yen's animations (see
`tools/model-export/ANIMATION_FORMAT_NOTES.md`). Never apply it to the working tree: it
hand-edits the generated code and stubs out code generation. Work on a copy.

## Build

1. Copy the whole project (`Oratan-recomp`) to a new folder, e.g. `Oratan-trace`. The game
   files in `assets` can be linked rather than copied.
2. `python patch_trace.py <that folder>`: instruments `generated/default/oratan_pch.h` (every
   `REX_LOAD_*` / `REX_STORE_*` logs through `oratan_trace_event` when the address is inside the
   watched ranges, tagged with the recompiled function's name), adds the `trace::OnTickEnd`
   hook to `src/tick_hooks.*`, adds `src/motion_trace.cpp` to `CMakeLists.txt`, and replaces the
   code-generation command in `generated/rexglue.cmake` with a stamp-file `touch` (otherwise the
   build regenerates the code and throws the instrumentation away).
3. Copy `motion_trace.cpp` to `<folder>/src/` and build as usual (`build.bat`).

The patch is repeatable: it refuses to run twice (`already patched`).

## Run

Start the game from the build folder, choose Training with Fei-Yen as the player's robot, play,
quit normally. Output goes to `trace_out/` next to `oratan.exe` (deleted at start by
`run_trace.bat`-style launchers; keep your own copy).

- Default: only the animation-table reads of `FEI.mt` are logged (cheap) plus per tick the
  robot records and the draw-list matrices.
- `ORATAN_TRACE_FULL=1`: also logs every read of the whole motion file and every write to the
  robot record (large; `ORATAN_TRACE_MAXMB` caps the event log, default 3000).
- Slow-motion mode makes the log easier to read (the extraction scripts assume 100 ticks per
  animation-second for that run; measure it from a looping animation, see the notes).

Output files and their formats are listed in `ANIMATION_FORMAT_NOTES.md` ("Tracing the game").
`trace_analyze.py <trace_out> [--tick N]` prints which functions read the file and write the
robot record, and one tick's reads next to the writes. `segments.py`, `parts_extract.py` and
`make_anims.py` (in `tools/model-export/`) turn a trace into animations.
