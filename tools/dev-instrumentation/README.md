# Renderer development instrumentation

Debugging tools used while building the native renderer (`sdk-patches/0004`). They are not
part of normal builds; apply them when needed and remove them again before committing.

## SDK: `sdk-instrumentation.patch`

Apply in the SDK tree on top of patches 0001–0004 (`git apply`), rebuild the SDK, copy
`rexruntime.dll` and `rexgpu-xenos.dll` into the game's build folder.

- `--gpu_frame_stats=true`: every 300 frames, logs draws and resolves per frame, render-target
  configurations and the number of distinct shaders.
- With `gpu_frame_stats`: **F11** writes every draw and resolve of the next 2 frames to
  `gpu_frame_dump.txt` (in the working directory): render targets, EDRAM bases, window offset,
  sampled texture addresses/sizes/formats, shader hashes; resolves with source, destination
  address/size/format and written range. `--gpu_frame_dump_every=N` dumps one frame every N
  frames without a key press (e.g. during attract mode).
- **F8** (native mode): cycles the output through every native texture (resolve outputs), and
  logs each one's address, size and format. F8 past the last one returns to the normal image.
- `NativeTextures: reducing …` log lines: which textures get box-filtered copies for 1x draws.
- `NativeClearAliases`: an earlier attempt at forwarding Direct3D 9 clears between render
  targets of different scales. It does **not** work: the clear vertices it reads from guest
  memory have already been overwritten when the GPU thread processes the draw. Kept only for
  reference; the shadow pass is handled by keeping it at 1x instead.
- `--d3d12_debug=true` also writes Direct3D 12 debug-layer messages to the log
  (`D3D12 debug layer (id): …`), no debugger needed.

Keys already used by the game and runtime: F1 options, F3 debug overlay, F4 settings,
F7 achievements, F9 snapshot, F10 game D3D trace, F11 frame dump, backtick console,
PgUp/PgDn stage pages.

## Game: `game-d3d-trace.patch` + `d3d_trace.cpp`

Hooks the game's 146 Direct3D entry points (see `notes/RENDERER_API.md`). Copy `d3d_trace.cpp`
to `src/`, apply the patch (adds it to CMake and marks frame boundaries in `frame_stats.cpp`),
rebuild. Run with `--d3d_trace_file=<path without spaces>`; **F10** records 30,000 calls.

## Scripts

- `d3dtrace.py <trace.bin>`: calls per frame per function with sample arguments.
- `framedump.py <gpu_frame_dump.txt>`: render passes, resolves, and which sampled textures come
  from which resolve.
