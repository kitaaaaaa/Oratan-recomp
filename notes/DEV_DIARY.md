# Oratan-recomp dev diary

A record of bringing **Cyber Troopers Virtual-On Oratorio Tangram M.S.B.S. Ver.5.66** (Xbox 360,
Xbox Live Arcade, title ID 58410985) to Windows as a native program, and of what we learned
about how the game works inside. It is written to be read by people, but it keeps exact
addresses and offsets so the work can be picked up again.

Guest addresses (the game's own memory addresses, as on the Xbox 360) are written like
`0x839EEA98`. Functions are named after their address, like `sub_82123758`.

---

## 1. What this project is

The Xbox 360 runs PowerPC code. Instead of emulating that CPU, the project uses the
**ReXGlue SDK** (v0.10.0) to *statically recompile* the game: every function in the game's
executable (`default.xex`) is translated once, ahead of time, into C++ and compiled into a
normal Windows program. The Xbox 360's operating system calls (file access, controllers,
threads, the GPU) are provided by a runtime derived from the Xenia emulator.

What runs is therefore the original game logic, compiled for x86-64, plus:

- an **Xbox 360 kernel and system library** layer (files, threads, controllers, achievements),
- the **Xenos GPU emulation**, which replays the game's GPU command stream on Direct3D 12 or
  Vulkan.

Our own code (`src/`) hooks individual game functions to fix, extend or observe them. Changes
to the SDK itself live in `sdk-patches/`. No game files are in the repository; players supply
their own copy.

## 2. What we've accomplished

In roughly the order it happened:

| Area | Result |
|---|---|
| Recompilation | The whole game runs: Arcade start to finish, Training, menus, at 60 fps |
| Game files | Boots from extracted files or straight from the raw XBLA package (unpacked on first launch) |
| Presentation | Window title and branding; Alt+Enter windowed/fullscreen; F1 Options window |
| Options | Render resolution 1×–4×, VSync (SDK patch 0001), fullscreen, restart button |
| Local versus | Controller 2 takes over the CPU opponent, including Arcade bosses |
| Split screen | Each player's view at once: staggered, side by side, or one monitor per player (SDK patch 0002); LIVE MONITOR camera when player 2 isn't playing |
| Hidden content | Training stage select page 2 with **Distorted Shrine** and **Tangram**; **Bradtos** playable via Ajim's slot |
| Fixes | Achievements menu no longer softlocks (SDK patch 0003) |
| Tools | Crash reporter (names the faulting game function), memory capture (F9), value watcher, image dump |
| Models | Fei-Yen exported game-accurately (OBJ, rigged glTF/FBX, symmetric A-pose) |
| Releases | 0.1.0 and 0.2.0 zips on GitHub |

Things that were tried and **dropped** (so they are not tried again blindly):

- **High frame rate** by drawing interpolated frames between game updates. The camera and
  stage interpolated fine, but the robots are drawn from data we never found, so they ghosted.
  Abandoned at the user's request; see `work\HIGH_FRAMERATE_NOTES.md`.
- **The attract-mode tutorial stage** (`man`): it is only scenery for a scripted demo and has no
  terrain/collision grid, so it cannot load as a battle stage.
- **Tangram as a playable robot** (its motion file is empty) and **Bal-Keros / Bal-Baros
  outside their own stages** (crash on dash / invisible shot).
- **Space Carrier, Tangram Core** and the menu backdrops as stages (crash).

## 3. How the game works

The fixed addresses and structure layouts below are also in `src/game_memory.h` as C++ types
(with compile-time offset checks); new findings go in both.

### 3.1 Lineage

The executable is a port of the **Dreamcast Ver.5.45 code base** (internal module names OPEN,
GAME, NETVS, CHRSEL, ENDING, CSTM, MTEST; the string `M.S.B.S.VER.5.45` is still inside),
running on a 2009 Xbox 360 engine from Sega that also uses Project DIVA-style 2D tools
(`aet`/`spr` layout files). Model and texture data are still in the Dreamcast formats
(little-endian, PowerVR textures), converted at load time.

### 3.2 Main loop

- The application object's run loop is `sub_82112020`; its virtual table (`0x82000EA8`) has
  **Update** at slot 2 (`sub_821236E0`) and **Draw** at slot 4 (`sub_82123758`).
- **Update** advances the game by one tick (1/60 s). **Draw** only renders and presents; it does
  not change game state, which is why it can be called more than once per tick (split screen
  does this).
- Presenting goes `sub_82188C50` → `sub_8219F548` → `sub_8219EF88` → `sub_822D3180` (Direct3D
  Present) → `VdSwap`. The game paces itself by waiting for one vertical blank per present.

### 3.3 Modules

The game is a set of modules; the current one is the byte at `0x839C21BC` (switcher
`sub_822A6A30`):

| # | Module | Used for |
|---|---|---|
| 0 | OPEN | Title, main menu, attract demo |
| 1 | GAME | Arcade and Score Attack battles, Continue, Game Over |
| 2 | NETVS | Training (and online) battles, their select screens |
| 3 | CHRSEL | Arcade character select |
| 4 | ENDING | Endings |
| 5 | CSTM | Customize |
| 6 | MTEST | Removed test menu |

### 3.4 Battle state

- Everything about a fight hangs off one structure, the **game** pointer at `0x839EEA98`.
- Two **robot records** start at `game+0x6150`, 4496 bytes each (side 1 = DNA, side 2 = RNA). The
  first byte is the robot number. `+0x84` is the position. `+0x74` (upper 16 bits) is the
  facing. From `+0x4B8` there's a bone table: 24-byte entries of three angles (32-bit, 65536 =
  360°) and a position.
- `game+25021` / `+29517`: "this side is CPU-controlled". `game+53125`: which camera to draw from
  (0 = side 1, 1 = side 2, 2 = LIVE MONITOR).
- The camera used for drawing is a 4×4 matrix at `game+0x00`.

### 3.5 Input

Controllers are read by `sub_821135C0` into `0x829BF2A8` (60 bytes per controller). Each side
has an input record at `0x829BEFA8` (316 bytes apart); `+300` says which controller that side
reads. `sub_822A71B0` turns both sides' input into robot command words every tick.

### 3.6 Stages and robots

- **Stage number** of the next fight: `0x83876DF4`. **Stage file names**: pointer table at
  `0x8201F530` (stage → `carrier`, `duct`, …; ends at stage 21). **Stage records** (display
  name, announcer lines, music): 300-byte entries at `0x82898AF8`.
- **Robot names** for file loading: pointer table at `0x8201F588` (0 `dor` … 18 `tgm`).
- **Training stage select**: a 17-entry carousel table at `0x8203FCA8` (24 bytes: stage, preview
  image position and sheet, name, panel image). **Training character select**: a 2 × 9 byte
  grid at `0x82010698`, portraits at `0x820106B0`.
- The robot loader `sub_821C5C30` passes every robot through `sub_821C5060`, which swaps
  Bal-Bados (robot 1) for its stage-specific forms: Bal-Baros (16) on the water stage (5),
  Bal-Keros (5) on the final stage (21). Earlier notes had the two forms the other way round.
  This is the first hand-decompiled function; see section 7.

## 4. How the 3D models are stored

All 3D data lives in `media/rom/data.farc`, a compressed **FARC** archive (`FArC`: a list of
name, offset, compressed size, uncompressed size; each file is gzip-compressed). For each robot
there are:

| File | Contents |
|---|---|
| `xxx1c.mrg` | the in-game model, player 1 colours |
| `xxx2c.mrg` | the same, player 2 colours |
| `xxx1a.mrg` / `xxx2a.mrg` | a smaller model set (not what battles draw) |
| `xxx1c.mg2` / `xxx2c.mg2` | materials and textures |
| `XXX.mt` | motion (animation) data |

Stages have `name.mrg` (terrain grid + scenery model) and `name.tex` (textures). All of it is
**little-endian** (Dreamcast heritage).

### 4.1 `.mrg`: geometry

A robot is not one mesh but a set of **parts** (Fei-Yen's in-game model has 1,329 parts, plus
alternates; a typical frame draws 36–60 of them: armour plates, joints, hair, weapon, and
lower-detail or hyper-mode versions of each).

- Header: `2, 0x10, …`, then a table of `(offset, id)` pairs. Each part starts at
  `offset + 0x10`. The first entry points back into the table and is not a part.
- **Part header** (0x38 bytes):

  | Offset | Field |
  |---|---|
  | +0x00 | link: file offset of an **alternate version** of this part (0 = none) |
  | +0x04 | position (3 floats) |
  | +0x10 | rotation (3 × 32-bit angles, 2³² = 360°) |
  | +0x1C | scale (3 floats) |
  | +0x28 | material number (into the `.mg2` material table) |
  | +0x2C | flags |
  | +0x30 | bounding radius |
  | +0x34 | vertex count |

- **Vertices**, 16 bytes each: position x, y, z (floats) and a **normal packed into one 32-bit
  word** as three signed 10-bit numbers (bits 0, 10, 20; divide by 512).
- Then a polygon count and the **polygons**: a colour word, two more words, a flags word whose
  byte 2 is the vertex count, then for each vertex a 16-bit vertex index and a 16-bit U and V.
  Polygons are **triangle strips**. UVs are in texel units (divide by the texture size; effect
  textures tile).
- The normal and hyper-mode (gold) versions of a part can have identical headers and geometry
  and differ only in UVs, which point at pink or gold areas of the same texture.

### 4.2 `.mg2`: materials and textures

- A material table: 20-byte entries (four PowerVR control words and a texture number, −1 for
  untextured).
- A table of texture offsets, then the textures themselves in the Dreamcast **PVR** format
  (`PVRT`, sometimes preceded by `GBIX`): 16-bit colour (ARGB1555, RGB565, ARGB4444), twiddled
  or rectangular, or **8-bit paletted**.
- A `PVPL` block holds the 256-colour palette for the paletted textures. **The 1P/2P colour
  schemes differ by palette.** Palette entry 0 (pure green) is the **transparency key** (for
  example around Fei-Yen's heel lights).
- Fei-Yen's body is one 512×512 paletted texture atlas.

### 4.3 How the game uses them at run time

- On loading, the game **byte-swaps** the model into Xbox 360 order (floats per 32 bits, polygon
  indices and UVs per 16 bits, colours untouched) and turns each part's link offset into a
  pointer.
- Some parts are **changed while the game runs**: Fei-Yen's skirt, for example, has its
  vertices, UVs and colours updated in memory, so the file copy is not what is drawn.
- Each frame the engine builds a **draw list**: base pointer at `0x839EFB78`, count at
  `0x839EFB7C`, 0x15C-byte records holding the modelview matrix (+0x00), a pointer to the part
  (+0x4C, pointing at the part's +0x2C), the world matrix (+0x60) and the view matrix (+0xA0).
  Reading the parts and world matrices from this list gives an exact copy of what the game
  drew. That is how Fei-Yen was exported.
- **Skeleton:** the model files contain no bone hierarchy. It is driven by the motion data and
  game code. We derived Fei-Yen's skeleton by comparing 34 captured poses: parts that never
  move relative to each other form one bone (21 bones), and each joint's pivot is the point
  that stays fixed between parent and child in every pose.
- **Coordinates:** the game's world is **left-handed** (x right, y up, z forward).

The full export pipeline and scripts are in `work\MODEL_FORMAT_NOTES.md`.

### 4.4 How the robots animate (2026-10-24)

Fei-Yen's animations were extracted exactly and turned into game-accurate clips for a VRChat
avatar. Full format, runtime facts, the tracing tool and the pipeline are in
`tools/model-export/ANIMATION_FORMAT_NOTES.md`; in short:

- **`XXX.mt`** is a table of **animation blocks**: table A at 0x10 (629 offsets), table B at 0x9E4
  (340 more). A block holds one array of channels (about 70 for Fei-Yen); a channel is a constant
  or a **cubic Hermite curve** of keys (u16 time in 1/60 s, bfloat16 value with the sign in bit 0,
  tangents). The curve evaluator is fully decoded. Indices share blocks in a repeating pattern of
  three (452 distinct blocks for 629 indices), played at different speeds and directions.
- **Not decoded:** which channel drives which joint and in what order, and the leg IK. So the
  exact route reads the game's *output* instead of re-deriving it.
- **Each tick**, `sub_82240808` reads one word of table A: the file offset read is
  `0x10 + 4 * animation index`. Logging those reads says which animation plays when; logging the
  draw list (4.3) gives every part's exact world matrix. `tools/dev-instrumentation/motion-trace/`
  is the instrumented build (macro-level load/store logging in the generated code, per-tick hook).
- **Result:** 39 animations recorded in one session, 13,686 ticks; per bone the rotation is the
  part's world matrix against its rest matrix, converted to the rig's frame. The robot's facing is
  its **head**, not its pelvis (the idle is an angled stance with the head dead on target).
- A Humanoid avatar cannot hold the game's joints exactly (3-axis shoulder, hinge elbows and
  knees); a hidden correction bone per limb carries what the Humanoid pose leaves over, which
  brings every bone to 0.0° of the game.

## 5. How rendering works

### 5.1 The layers

1. **Game code** decides what to draw: robots, stage, effects, 2D layouts.
2. An **engine graphics layer** (`0x822B0000`–`0x822BFE20`, about 200 functions; the most-used
   ones are called from about 500 places) keeps matrix stacks and records drawing work. It does
   not call Direct3D itself.
3. The **Xbox 360 Direct3D library**, compiled into the game (`0x822BFE20`–`0x822E8000`, about
   700 functions). The game calls 147 of them directly. They write **GPU command packets**
   (PM4) into a ring buffer.
4. **Xenos GPU emulation** (SDK, `rexgpu-xenos.dll`) reads those packets and reproduces them on
   Direct3D 12, converting the game's GPU shaders as it meets them.

### 5.2 Shaders

The game's shaders are in `media/shader_hlsl.farc` (uncompressed FARC): **1,416** compiled
Xbox 360 shaders (79 vertex, 1,337 pixel). They are permutations of only **12 programs**:

| Program | Vertex / pixel | Purpose |
|---|---|---|
| `vo3d` | 48 / 1152 | the main 3D shader (robots, stages), with feature switches |
| `default` | 5 / 80 | general purpose |
| `tone_map` | 3 / 48 | HDR tone mapping |
| `shadowmap` | 2 / 24 | shadows |
| `simple` | 4 / 12 | simple geometry |
| `reduce_tex` | 9 / 9 | downsampling (bloom, luminance) |
| `vo2d` | 2 / 4 | 2D |
| `pp_gauss`, `ripple`, `silhouette`, `color_correct`, `font` | 1–2 each | effects, text |

### 5.3 What a frame does on the GPU

Measured on the title and menus with `--gpu_frame_stats=true`:

- about 460 draws per frame (up to 9,744);
- about 35 **resolves** per frame: copies from the GPU's on-chip memory into textures. Most form
  a **downsampling chain** (1024 → 512 → … → 32 pixels) feeding HDR tone mapping and bloom;
- about 20 different render-target set-ups, some with 4× multisampling;
- 11 vertex and 12 pixel shaders in use.

In a battle (Training, Public Port, about a minute):

- about 1,150–1,350 draws per frame;
- 42 resolves per frame: a 1024×1024 shadow map, the HDR downsampling chain (1024 → 32) and
  the final 1280×720 image;
- the main 3D pass renders with **4× multisampling** into the Xbox 360's **10-bit floating-point
  HDR format** ("7e3", `2_10_10_10_FLOAT`), which PC GPUs don't have natively;
- 21 vertex and 27 pixel shaders in use.

### 5.4 Why resolution is limited

The Xbox 360 GPU renders into **10 MB of on-chip memory (EDRAM)** and copies ("resolves") the
result out. The game is built around a 1280×720 frame. The emulation can raise the resolution
only by **whole multiples** (2×, 3×, 4×), because copies between on-chip memory and textures
must line up with pixels. A native 1920×1080 frame (1.5×) does not fit that scheme, and would
not fit in 10 MB on the real hardware either. This is what led to the renderer rework below.

### 5.5 Split screen

Split screen calls **Draw** once per player each tick with the camera byte (`game+53125`) set
to that player's view, tags each presented frame with its view (`SetNextSwapView`, carried
alongside `VdSwap`), and the SDK's D3D12 backend composes the tagged frames into one image
(SDK patch 0002). The guest's vertical-blank rate is raised so two presents per tick keep the
game at full speed.

## 6. The renderer rework (in progress)

**Goal:** draw the game with a native PC renderer instead of replaying Xbox 360 GPU commands,
so it can render at any exact resolution (1920×1080 included) without the EDRAM limits.

**Approach under assessment** (the same idea as Unleashed Recompiled's renderer):

1. Intercept the game at the Direct3D level, at its 147 entry points, or higher at the engine
   graphics layer if its recorded commands are simple enough.
2. Convert the 12 shader programs (1,416 compiled variants) to PC shaders ahead of time.
3. Re-create render targets, resolves (render-to-texture), textures (including Xbox 360 tiled
   layouts) and multisampling with native resources at the chosen resolution.
4. Keep the existing Xenos emulation as the default until the new renderer draws everything
   correctly.

**Biggest risk:** the resolve and render-to-texture chain (about 35 per frame), which must be
rebuilt as native passes at the new resolution.

**Phase 1 (mapping the Direct3D calls), 2026-10-08:** traced a title-screen frame and a
battle frame. The game makes about 4,400 (title) and 11,300 (battle) Direct3D calls per frame
across only ~55 distinct functions; the rest of the 147 are set-up. The main frame is drawn in
4 tiles (predicated tiling) because 1280×720 with 4× multisampling doesn't fit in EDRAM, which
a native renderer can drop. All resolves go through one function. Draws use two paths:
indexed triangle strips (robots, stages) and immediate quads (2D, full-screen passes). The
function-by-function catalogue is in `notes\RENDERER_API.md`; most roles are now confirmed
from the code, including the render states (matched to the GPU register bits). Shaders have no
separate setter function: the game seems to write them into the device directly and the GPU
loads them at draw time. Remaining in phase 1: find where shaders are bound, and list the
set-up functions.

---

**Native rendering works (2026-10-09).** F1 → Resolution now offers 1920×1080, 2560×1440
and 3840×2160 rendered natively (60 fps). See `notes\RENDERER_DESIGN.md` → Status for how it
works and what is left.

## 7. Hand-decompiled functions

Recompiled game functions can be replaced with readable C++ that behaves identically, one
function at a time (`src/decomp/`). Conventions, set with the first one:

- One file per game area (`robot_loader.cpp`, `menu.cpp`, `game_flow.cpp`), a plain C++
  function with a descriptive name and the original address in a comment, and a `REX_HOOK_RAW`
  that does the register and memory plumbing. One hook per function: other features that
  need the same function are called from that hook (local versus' `OnSideChosen`).
- Small helpers the function calls (getters, flag setters, a stricmp) are rewritten inline
  under their own names when they only touch memory; the sweep covers them too.
- Guest data through `src/game_memory.h`; anything learned goes into it, with `static_assert`s.
- `decomp_use_originals` (calls the original code instead) and `decomp_verify` (compares the
  rewrite with the original over every input it can construct, on first use, and logs
  `decomp verify sub_X: N inputs, M mismatches`).
- The check is `decomp::Verifier` (`decomp.h`): give it the guest ranges the function reads
  or writes, then per input a setup, the original, the rewrite and a description. It
  restores the ranges between and after runs, compares results and ranges, re-runs a
  mismatch (it counts only if the difference repeats), logs the first 20, and prints the
  summary. The first 50 inputs also compare the whole data section (0x82890000, 18 MB) to
  catch writes outside the declared ranges; bytes that change by themselves (2, a counter at
  0x839F65AA) are found by watching the section for 100 ms first and skipped. A sweep blocks
  the game thread for a moment (the main menu's Start press, the first robot load).
- Each new verifier is proven with a deliberately broken rewrite before it is trusted.
- No generated code or disassembly is committed; the logic is described in words.

**`sub_821C5060`, Bal-Bados' form for the stage** (2026-10-11). Takes a robot number (r3) and
returns it unchanged unless it is Bal-Bados or one of its forms. For those it returns plain
Bal-Bados in two situations marked by two game fields (`Game::scene`/`scene_step` at
+0x240/+0x242: (3, 21), the battle's Continue screen, or (0, 31) during the Ending module),
otherwise Bal-Baros if the next stage is the water stage, Bal-Keros if it is the final stage,
else Bal-Bados. When the scene is 0 it also checks the stage stored in a per-side record (`Game::side_records`, 300 bytes each
at +0x96E4, side chosen by the byte at +0x614D). It reads the game pointer and module byte
through two one-line getters and writes no memory. Verified: 7,765,800 input combinations
(robots −2…40, every module, stages −2…40, edge values of the two fields, both sides, five side
stages), 0 mismatches; a deliberately broken rewrite gave 27,138 mismatches, so the check works.
The verifier saves and restores every byte it changes. Moved onto `Verifier` later: still
7,765,800 inputs, 0 mismatches; a planted bug gave 11,916.

**`sub_8213CCD0`, the main menu's side choice** (2026-10-11, `menu.cpp`). Called with the side
(0 = 1P, 1 = 2P). Gives the active controller (`kActiveController`, 0x8289030C, handed out
when Start is pressed on the title screen; any value above 3 means none) to that side's
`SideInput::controller` and -1 to the other. With no active controller, side 0 gets pad 0 and
side 1 pad 1 whatever the choice. Any side other than 0 or 1 leaves both at -1. Then every
controller except the active one is disabled: `ControllerMasks` (0x829B2268) holds three bit
masks per controller; disabling clears the bit in all three (the first is "enabled", the
others aren't known). It calls only memory-touching helpers (sub_82114C88, sub_82114CD0,
sub_82114A00), so a sweep has no outside effects. Verified: 81,000 inputs (sides incl. the
32-bit extremes, active values incl. 4 and 0x80000000, both sides' starting pads -1..3, mask
patterns), 0 mismatches; a planted bug gave 1,125, exactly the inputs it affects. Local
versus' old hook on this function became `oratan::OnSideChosen`, called after the rewrite or
the original. Play tested with two pads: rewrite with local versus off, original with local
versus on (2P).

**`sub_822A6A30`, the module switcher** (2026-10-11, `game_flow.cpp`). Not a dispatch: it takes
a module file name ("GAME.BIN"), a scene and a step, copies the name into `Game::module_file`
(+0xCEB0), sets `Game::scene`/`scene_step`, sets two bytes (+0xCEF9 = 1, +0xCEF7 = 0, meaning
unknown), and looks the name up case-insensitively in `kModuleFileNames` (0x8298AC9C:
OPEN, GAME, NETVS, CHRSEL, ENDING, CSTM, MTEST) to set the module byte (0xFF if none
matches). The module itself starts later from the frame loop. Most callers go through
sub_822A6BA0 (scene 0, step 0). The rewrite also returns the original's r3 (the last name
comparison). Verified: 720 inputs (the seven names, other spellings and unknown names written
to spare guest stack, scenes and steps incl. sign edges), 0 mismatches; planted bugs gave 125
(including 5 writes outside the declared ranges, caught by the section check).

**Game flow: scenes and steps** (2026-10-11). `Game::scene` is the running module's scene, an
index into that module's scene table (`sub_822AF658` runs one such table); `scene_step` counts
through it. Logged through Arcade (win, loss, Continue yes, Game Over), Score Attack and
Training:

| Module | Scene | Steps | What |
|---|---|---|---|
| OPEN | 1 | 0..6, 0x11 | title and main menu |
| CHRSEL | 0 | 4, 5 | Arcade / Score Attack character select |
| NETVS | 5, 0 | 0..6 | Training select screens |
| GAME / NETVS | 2 | 9 | one frame before a stage |
| GAME / NETVS | 3 | 9, 0xA | stage load |
| | | 0xB | stage intro (~4 s) |
| | | 0xC..0xE | "ready" (~2 s) |
| | | 0xF | fighting (pause doesn't change it) |
| | | 0x10, 0x11 | fight decided |
| | | 0x12, 0x13 | ~1.5 s later |
| | | 0x14 | results (~6.5 s); a win goes back to step 9 for the next stage |
| | | 0x15, 0x16 | Continue screen (yes: CHRSEL) |
| | | 0x17, 0x18 | Game Over, then OPEN |
| GAME / NETVS | 2 | 0 | after quitting a fight from the pause menu |
| NETVS | 4 | 0 | Training's after-fight menu |

Named in `game_memory.h` as `scene_id`, `battle_step` and `IsFightInProgress` (battle module,
scene 3, step before 0x15). Bytes next to `record_side`: +0x614C is 1 in Arcade / Score Attack
battles (0 in Training), +0x614E is 1 from a run's first fight and drops at Game Over,
+0x614F counts the stages started in the run.

Split screen now uses `IsFightInProgress`: Continue, Game Over, Training's menus and quits are
drawn once (with one monitor per player they show the same picture on both monitors, by
design), and the vblank multiplier follows (logged `Guest vblank multiplier: 1` at Continue).
Side effect noticed in play: the 2P view now shows the winner's victory pose at the results.
Still open: the results text is 1P's ("you win/you lose"); a PERFECT by 2P registers (music)
but its screen doesn't say so.

Next step: recorded input replay, then `sub_822A71B0`. Suggestions for the conventions before
then: a replay gives real inputs to sweep with, which a `Verifier` input generator could take;
and functions that call the scene code or the renderer will need the "same call, same
arguments" dry-run check (not yet written, nothing has needed it).

## 8. Replays

`src/replay.cpp` (developer): `--replay_record=FILE` records every controller read from boot
plus a per-tick fingerprint of the game state; `--replay_play=FILE` feeds the reads back
instead of the controllers and reports the first tick where the fingerprint differs
(`replay: stopped at tick N (...): D drifting ticks, U unmatched reads`). Format and logic are
described at the top of the file. Paths with spaces need quoting on the command line.

**Results** (2026-10-11):

- **The game is deterministic from boot.** A Training session (6,829 ticks: boot, menus,
  a fight of about a minute, quit to the title) replays by itself with 0 drifting ticks and 0
  unmatched reads. The controllers are read exactly once per tick, always on the Update thread.
  The fingerprints really move (about 7,000 distinct game states in a 9,700-tick session), so
  a match isn't empty data agreeing.
- **Display settings don't change gameplay.** The same recording matches with 1440p
  instead of 1080p, with the native renderer off, and with split screen on (which runs fights
  at a vblank multiplier of 2). Replays work as a gameplay regression test.
- **Heap pointers in the robot records.** The first native-renderer-off replay "drifted" from
  the round start on, but only six words differed: `Robot` +0xB4, +0xC8, +0x2D8 in both
  robots, each exactly 0xC1A0 higher, still the only difference mid-fight. They are guest heap
  pointers (allocation placement differs with the renderer; gameplay doesn't). Named
  `heap_pointer_*` in `game_memory.h` and zeroed before hashing; file format version 2.
- **Boot waits on vblanks.** `--guest_vblank_multiplier=4` replays 2.5x faster but drifts from
  tick 1025: part of the boot sequence waits a number of vblanks, so it finishes in fewer ticks
  and every later input lands on the wrong tick. Not worked around: fast replays, and
  netplay, would want to start from a known point (fight start) instead of boot, or record the
  ticks the boot took. Fights themselves aren't affected (split screen's 2x matches).
- **Closing the window hard-exits** (`ReXApp::OnClosing` calls `_Exit`), so the end record is
  never written. Recording now flushes every tick, and a replay treats the end of the file as
  the end of the recording.

## 9. Netplay

`src/netplay.cpp` (experimental, command line only): lockstep versus over UDP, built as a live
replay. Each machine sends its own controller's buffer every tick, `netplay_delay` ticks ahead;
both write the same merged buffer (host slot 0, joiner slot 1) and exchange state fingerprints.
`--netplay_host=PORT`, `--netplay_join=ADDRESS:PORT`, `--netplay_pad=N`, `--netplay_delay=N`,
`--netplay_timeout=MS`, `--netplay_sim_latency=MS`, `--netplay_sim_loss=PCT`. The Update and
controller-read hooks are in `tick_hooks.cpp` (netplay, then replay), the fingerprint in
`state_hash.cpp`.

**Results** (2026-10-12, two copies on one PC, two controllers):

- **Replays still match** after the refactor (6,829 ticks, 0 drifts), and **live input doesn't
  leak in**: a replay played while mashing the real controller still matched. The game sees the
  controllers only through `kControllerStates`.
- **Fixed: one press counted up to 4 times.** The read (`sub_821135C0`) keeps each controller's
  history in that buffer: "just pressed" (+12) = held (+4) and not held at the previous read
  (+8), and it skips unchanged XInput packets by the number at +28. After netplay put the
  shared, delayed input there, the next read compared the local controller against input
  `delay + 1` ticks old, so a press was "new" on several ticks (the menus moved 2-4 steps).
  Netplay now gives the read this machine's own buffer back before each read
  (`BeforeControllerRead`). Replays never hit this: a recording overwrites the buffer with its
  own consistent history.
- **Fixed: the unfocused window got no controller input** (SDL drops pad events in the
  background by default). SDK patch 0006 turns background pad input on, always (the owner's
  choice).
- **In sync throughout:** 12,345 ticks of menus, Training and Arcade with every fingerprint
  compared, no desync.
- **Simulated internet** (40 ms each way, 2% loss): in sync at every delay, and felt smooth at
  3. Stalls per 30 s: delay 3, host 9-47 and joiner 451-1,577 (still 60 ticks/s; waits fit in
  frame time); delay 4, joiner 0 but a host burst of 1,731; delay 5, host 34-52, joiner 0. The
  waits fall on whichever machine runs ahead. 4-5 suits a ~40 ms connection.
- **Disconnects:** closing one copy, in the menus or mid-fight, makes the other wait
  `netplay_timeout` (10 s), log "the other player stopped answering; continuing offline", and
  carry on; mid-fight the opponent's robot goes back to the CPU.
- Not tested yet: two PCs (LAN or internet).

**F1 Netplay panel** (2026-10-24, cloud session; not yet built or run with the game). Netplay has
to run from boot, so the panel's **Host** / **Join** / **Leave** buttons restart the game with
`--netplay_host=PORT` / `--netplay_join=ADDRESS:PORT` added or removed (`RelaunchArgs` in
`relaunch.cpp`, which rewrites the command line with `CommandLineToArgvW` and re-quotes it).
Those two are never saved, so a normal launch is offline; `netplay_port`, `netplay_address`,
`netplay_delay` and `netplay_pad` are remembered. While waiting or joining it shows the state and
a **Cancel** button (`CancelNetplayWait`: the game carries on offline); once connected, the
peer, player, delay, the last 30 s report's stalls and any desync (`GetNetplayStatus`); after a
failure or the end, the reason. The host sees its IPv4 addresses (`LocalIpv4Addresses`). Checked
in the cloud: status and both cancel paths with the real netplay code, a two-process session
still in sync, and the command line rewrite round-tripping through the Windows parsing rules
(spaces, quotes, trailing backslashes, both `--opt=v` and `--opt v`). Open question: whether the
F1 window draws while the host waits at boot (the game thread is blocked then).

**Asked for next** (owner, 2026-10-12): with local versus or netplay, player 2 should get their
own VR select (Training, Arcade, Score Attack) instead of a copy of player 1's, and Training's
select cursor should say "PLAYER 2" instead of "AI". An F1 Netplay panel would replace the
command line. Later: VR headset support and a first-person view.

---

*Working notes with every detail and the full history: `work\DIARY.md`. Model formats and
pipeline: `work\MODEL_FORMAT_NOTES.md`. Animation format and extraction:
`work\ANIMATION_FORMAT_NOTES.md`. Abandoned high-frame-rate work:
`work\HIGH_FRAMERATE_NOTES.md`.*
