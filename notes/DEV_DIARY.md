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
  Bal-Bados for its stage-specific forms (Bal-Keros underwater, Bal-Baros in zero gravity).

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

---

*Working notes with every detail and the full history: `work\DIARY.md`. Model formats and
pipeline: `work\MODEL_FORMAT_NOTES.md`. Abandoned high-frame-rate work:
`work\HIGH_FRAMERATE_NOTES.md`.*
