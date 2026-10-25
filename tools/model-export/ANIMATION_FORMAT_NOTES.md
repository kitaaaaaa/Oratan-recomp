# VOOT animation — FEI.mt format, runtime, and exact extraction (2026-10-24)

Result: the Fei-Yen animations the game plays can be reproduced exactly: per-tick bone rotations
read from the running game (39 distinct animations in one play session: idle, run, sprint,
backward, strafing, dashing, turning, jumping and landing), turned into game-accurate clips for a
VRChat avatar (every mesh-carrying bone within 0.0 deg of the game, joint positions within 0.2 cm). The keyframe
container (`XXX.mt`) is decoded down to the curves; how the game turns curve values into joint
rotations is NOT decoded (see "Not decoded"), which is why the exact route reads the game's own
output instead.

## What is decoded and what is not
- Decoded: the file layout (two block tables), the block header, channel entries, key records,
  the curve evaluator (cubic Hermite, bfloat16 values), the time base, which animation the game
  selects each tick, the robot record layout used here, the draw-list output.
- Measured, not derived: bone rotations (draw-list world matrices, see below), animation lengths,
  loop periods, travel speeds.
- Not decoded: which channel drives which joint and in what Euler order, the hierarchy the game
  composes them in, the two-bone IK solve (probably `sub_82241118`), per-state modifiers (speed,
  lean, direction), header fields `S`, `n2`, `C`, `D`, the 16-word tail at 0xF34, and what the
  second table (B) is used for.

## The file: `media/rom/data.farc` → `XXX.mt` (little-endian; Fei-Yen = FEI.mt, 803,616 bytes)
Extract with `farc.py` (see MODEL_FORMAT_NOTES.md). The game loads the whole file (0xC4320 bytes =
the file size) and turns every table word into a guest pointer (file offset + load base; in the
traced run FEI.mt sat at 0xA4D7F340, so table word 0x247CC reads as 0xA4DA3B0C).

| Region | Contents |
|---|---|
| 0x00..0x0F | header (not identified) |
| 0x10..0x9E3 | **table A**: 629 × u32 offsets of animation blocks, index 0..628 |
| 0x9E4..0xF33 | **table B**: 340 × u32 offsets of more blocks (every one parses as a valid block) |
| 0xF34..0xF73 | 16 words: small offsets/limits (0xFC4, 0x1954, 0xC430C, 0x1238 ...); read by `sub_8223C1C0` (u32) and `sub_822347D8` (u16); purpose unknown |
| 0x1ED4.. | block data (first block at 0x1ED4, last at 0xC1A3C) |

629 table-A words give only **452 distinct blocks**: indices share blocks, in a repeating pattern
of three. Indices {1,4,7,39}, {2,5,8,40}, {3,6,9,41} each share one block; {10,42}, {11,43},
{12,44} likewise; {53,55} and {54,56}; the largest group has 16 indices. The sharing indices are
played differently (different speed, direction, lean): 5 and 8 are the same block, yet the game
moves her at 7.06 and 4.22 m/s and the recorded poses differ. So an index is (block, play mode),
not a block.

### Block header (28 bytes, at the table word's offset)
`u32 n` length in ticks of 1/60 s (idle = 96 → 1.6 s; range over the file 1..200),
`u32 S`, `u32 A` file offset of the channel-entry array, `u32 B` end of it (= start of the
constant array), `u32 n2`, `u32 C`, `u32 D`.
Channel count = (B − A) / 2: 71 for 387 blocks, 70 for 124, 69 for 111, 73 for 7 (Fei-Yen has
more joint channels than rig bones, so several channels belong to one bone). `C − A` = 66 and
`D − B` = 4 in 622 of 629 blocks (70 and 0 in the rest). `S` and `n2` vary per block (idle: S 960,
n2 80; run: S 1008, n2 32; strafe: S 192, n2 28; jump rise and fall: n2 200); their meaning is
not known.

### Channel entries and keys
Entry `i` is a u16 at address `a = A + 2*i`:
- `0` → the channel is 0.0.
- `< 0x2000` → a curve; its key list starts at file offset `a − 2*entry` (entries count u16 units
  back from their own address, so curves are stored before the array).
- `≥ 0x2000` → the entry itself is a constant.
Values (constants and keys) are **bfloat16 with the sign moved to bit 0**: `bits = (v >> 1) |
((v & 1) << 15)`, float = that value << 16 as the high half of an IEEE-754 float32. 0 stays 0.
Key record, 8 bytes: `u16 time` (ticks of 1/60 s), `u16 value`, `u16 tangent_in`,
`u16 tangent_out`. The list ends where the next record's time field is 0xFFFF.

### Curve evaluation (`sub_8223F270`; `mt.py` reproduces it)
Find the key pair around `t` (seconds): walk forward while `t > time(next)/60`. With
`t0, t1` the pair's times, `d = t1 − t0`, `s = (t − t0)/d`, `p0/p1` the values, `m0` the
first key's out tangent and `m1` the second key's in tangent:

    h00 = 2s³ − 3s² + 1   h10 = d(s³ − 2s² + s)   h01 = −2s³ + 3s²   h11 = d(s³ − s²)
    value = h00·p0 + h10·m0 + h01·p1 + h11·m1

(tangents are per second of the key spacing, `d` multiplies them). At exactly a key time the
key's value is returned. Constants and zero entries are returned as is.
Use: `import mt` (set `FEI_MT` to the extracted `.mt`, default `farc_data/FEI.mt`), then
`b = mt.animation(index)` and `b.values(t)` → all channel values at `t` seconds (`b.n`, `b.count`).

## Runtime (guest memory; Xbox 360 addresses, big-endian) facts
- Evaluator chain per tick: `sub_82240808` selects the block (it reads the table word
  `file + 0x10 + 4*index`; every other tick in the traced slow-motion run), `sub_8223F478`
  fetches a channel entry, `sub_8223F270` evaluates the curve. `sub_82241118` contains what looks
  like the two-bone leg solve (not decoded).
- Game struct pointer at `*(0x839EEA98)`; **robot record 0** (the player) at `game + 0x6150`,
  4,496 bytes (0x1190). `+0x00` robot number (8 = Fei-Yen), `+0xC8` pointer to the loaded
  `.mt`, `+0x4B8` a bone table of 24-byte entries, quaternion nodes from `+0xA4C`. The node
  rotations include each bone's rest orientation, so fitting them directly to curve output failed;
  the draw list (below) is the clean source.
- Draw list (see MODEL_FORMAT_NOTES.md): base `*(0x839EFB78)`, count `*(0x839EFB7C)`, 0x15C-byte
  records; `+0x4C` = part address + 0x2C, `+0x60` = the part's world matrix. Every part's matrix
  is already `animated pose × heading`, so the delta against the part's rest matrix is exactly the
  rotation the game applied to that part.
- World is left-handed (x right, y up, z forward); the game's facing is NOT the pelvis: the
  robot holds an angled stance with the head dead on its target (idle: pelvis ≈ +21°, torso ≈
  +44° from the head's direction).

## Tracing the game (`Oratan-trace`, see tools/dev-instrumentation/motion-trace/)
A copy of the project with the generated `REX_LOAD_*`/`REX_STORE_*` macros instrumented
(`__func__` is the recompiled guest function's name) and a per-tick hook. It watches:
the motion file range (found through the robot record), and the robot record. Default run
logs only table reads (cheap), `ORATAN_TRACE_FULL=1` logs every key read and record write.
`ORATAN_TRACE_MAXMB` caps the event log (default 3000). Output in `trace_out/`:
- `trace_events.bin`: 20-byte records, `u32 tick, u32 ea, u64 value, u16 fn, u8 kind (0 read,
  1 write), u8 size`; `trace_names.txt` maps fn ids to function names; `trace_info.txt` has the
  motion-file address and robot-record address.
- `trace_bones.bin`: per tick `u32 tick` + two 4,496-byte robot records (player, opponent),
  big-endian as in guest memory (13,991 records, 120 MB, in the recorded session).
- `trace_parts.bin`: per tick `u32 tick, u16 kept` then `kept × (u32 object address, 64-byte
  matrix)`; only Fei-Yen's part objects (material index < 47) from the draw list (55 MB).
- `trace_objs.bin`: once per object `u32 address, u32 size`, then the object header and its
  vertices, used to identify which rig part an object is.
Recorded in the game's slow-motion mode (one animation step per 2 ticks, tick numbers
3019..16531). The idle loop's measured period (160 ticks) against its stored length (96 frames
= 1.6 s) gives **100 ticks per animation-second** for that run.

## Extraction pipeline (`work\`, scripts in `tools/model-export/`)
1. `segments.py <trace_out> <out.pkl>`: the animation index for each tick = (table-A offset
   read by `sub_82240808` − 0x10)/4; runs of one index become `[index, first tick, last tick]`
   (191 runs, 39 distinct animations in the recorded session; make_anims builds a clip for each).
2. `parts_extract.py <trace_out> <out.npz>`: identifies each traced object as a rig part by
   signature (vertex count, material, first vertices) against `rigexport2.py`'s data (160 traced
   objects, 59 matched to rig parts, one of them with an identical-signature twin), then per tick, per bone:
   `D = rest⁻¹ · M` (rest matrix of the bone's largest part; M = its world matrix), take the
   rotation part by polar decomposition, change basis with `T = S · Ry` (the rig's axis
   conversion), and rotate by `W_b`, the per-bone Kabsch rotation from the rig-rest geometry to
   the A-pose geometry (`feiyen_apose_v2.glb`): `Q_b = Δ_b · W_b⁻¹`. The per-tick pelvis
   yaw (heading) is removed and stored separately, together with the pelvis displacement.
   Output: 33 bones with data (some parts are never drawn), 13,686 ticks.
3. `make_anims.py <npz> <segments.pkl> <outdir> [--tick-rate 100]`: per animation, the longest
   recorded run; drops the first ≤6 ticks (blend-in from the previous pose); loop detection
   (best autocorrelation of thigh/shin/torso/upper-arm rotations over periods 20..500 ticks,
   correlation > 0.6 and vertical range < 6 units; animation 37 uses the override period 55);
   a loop is one period starting where the signal best matches itself a period later;
   resamples to 30 fps with per-bone slerp; writes `anim_<id>.txt` (`META duration loop|once
   fwd side`, then per frame `POSE`, one `bone qx qy qz qw` line per bone, `ROOTY`) and
   `anims.json`. Heading is put back as the swing about the clip's reference direction
   (loops: linear drift removed, since circle-strafing turns the whole body steadily), and the
   reference direction is the **head's** yaw, not the pelvis's.
   Speeds: `fwd`/`side` in m/s from the pelvis displacement (21.117 game units = 1.5 m).

Run it (from `tools/model-export/` or `work\`; `VOOT_WORK` = the work folder holding `farc_data\`,
the rig pickles and `export\FeiYen\feiyen_apose_v2.glb`):

    set VOOT_WORK=...\work
    python segments.py       <trace_out> seg.pkl
    python parts_extract.py  <trace_out> bones.npz
    python make_anims.py     bones.npz seg.pkl anims          (add --tick-rate 100)

Re-running these on the recorded session reproduces `bones.npz` and all 39 `anim_*.txt` exactly.

## Catalogue of the recorded Fei-Yen animations (indices into table A)
Duration at 100 ticks/s; "loop" = one period; speeds are travel speed in the pelvis frame.
Roles marked * were checked in play (idle, run, backward, the strafes) or confirmed by the owner (jump rise and fall); the rest are read from the data and are guesses.

| Index | Kind | Length | Speed (fwd / side m/s) | Role |
|---|---|---|---|---|
| 0 | loop | 1.60 s | 0 / 0 | *standing idle (the rocking stance; also the customize-screen idle) |
| 1, 4, 7 | once | 0.14 s | 8.6 / 2.7; 2.5 / −8.6; 5.7 / 0 | dash starts (one block, three directions) |
| 2 | once | 0.58 s | 7.0 / 0 | forward dash |
| 3, 6 | once | 0.02–0.06 s | | dash fragments |
| 5 | loop | 0.46 s | 7.06 / 0 | sprint |
| 8 | loop | 0.48 s | 4.22 / 0 | *run |
| 9 | once | 0.22 s | 2.1 / 0 | run stop |
| 10, 12 | once | 0.22–0.24 s | −1.2/−0.7; −2.3/1.4 | back-step starts/stops |
| 11 | loop | 0.48 s | −6.47 / 0 | *backward run |
| 20, 21 | loop, once | 0.36 s, 0.12 s | 10.5 / 0; 8.4 / 0 | boost dash and its end |
| 33, 35, 36, 38 | once | 0.18–0.54 s | | strafe starts/stops/turns |
| 34 | loop | 0.54 s | 0 / +4.18 | *strafe left |
| 37 | loop | 0.55 s | 0 / −3.25 | *strafe right |
| 40, 43–50 | mixed | 0.09–1.2 s | | other lateral and turning moves; 46 and 49 are 1.2 s loops during which the heading turns steadily (792° and −1100° over the longest recorded runs) |
| 51 | once | 0.02 s | | crouch before take-off |
| 52 | once | 1.04 s | 0 / 0 | *jump rise |
| 53 | once | 1.90 s | 0 / 0 | *fall |
| 54 | once | 0.17 s | 0 / 0 | landing |
| 55 | once | 0.15 s | | fast dive (after 52 with no 53) |
| 56 | once | 0.03 s | | landing recovery |
| 81, 82, 83 | once, loop, once | 0.02 s, 1.28 s, 0.11 s | | 82 is a low guard-like loop |
Jumps and falls change the pelvis height by up to 118 game units (the world altitude); that
height is not part of the pose (set `ROOTY` to 0 for them).

## Using the data in another engine (what the VRChat/Unity work found)
- The extracted rotations are exact world rotations of the game's own parts, so the ground truth
  is the part, not a skeleton. The game's joints do not match a Humanoid rig: the shoulder
  turns about 3 axes (a Humanoid shoulder has 2); the elbow is a clean hinge (its axis stays
  within 1-5° over a clip); the knee's axis wanders by up to ~30° in the run, which a Humanoid
  knee (bend + twist) cannot follow; the upper arm part is a 20-vertex cap, and the owner notes
  it cannot roll without taking the forearm with it.
- Humanoid muscle clips keep thighs, feet and hands within 6° but leave up to 62° at the
  shoulders and 12–14° at shins, forearms and upper arms. Fix used: each limb bone gets a hidden
  leaf child (at the same pivot) carrying the rotation the Humanoid pose leaves over, and the
  mesh is skinned to it → every bone 0.0° from the game. Joint positions stay within 0.2 cm
  because the leftover error is nearly all roll about the bone's own axis.
- Humanoid avatar twist parameters (`upperArmTwist` etc.) decide where twist lands. Upper
  segments 1, lower segments 0 gave the least error; 1/1 left forearms 57° and shins 54° off.
- Muscle values beyond ±1 are NOT clamped by Unity's animator. Quaternion curves must keep one
  sign convention across all clips (w ≥ 0), or a crossfade of two clips cancels them out.
- A blend tree over velocity parameters jumps the whole pose in one frame when a key is
  released; use one state per motion with crossfades and let the state's playback speed follow
  the velocity (`speedParameter`).
- Clips for avatars whose root motion is dropped must bake rotation, height and position into
  the pose (`loopBlendOrientation`, `loopBlendPositionY`, `loopBlendPositionXZ` = true).
- VRChat: head tracking pins the head to the fixed view point, so an animation that moves the
  head makes the neck stretch; animate the head (Tracking Control: Animation) or give a tracked
  mode its own clips whose body moves under a head that stays put.

## Open questions
Channel→joint assignment and Euler order; what `S`, `n2`, `C`, `D` mean; table B's role and the
sharing pattern (why index 5 and 8 share a block but not a speed); the IK; opponent robots'
files (the same container is used by all `XXX.mt`; only FEI.mt was examined); saber, hit, fall
and knock-down animations were not played in the recorded session and are not in the catalogue.

## Scripts
`mt.py` (reader), `segments.py`, `parts_extract.py`, `make_anims.py` in `tools/model-export/`;
`trace_analyze.py`, `motion_trace.cpp`, `patch_trace.py` in
`tools/dev-instrumentation/motion-trace/`. Reuses `rigexport2.py`, `glbcheck.py`, `farc.py`.
