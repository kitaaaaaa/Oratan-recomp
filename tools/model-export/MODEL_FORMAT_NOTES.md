# VOOT model export — formats and pipeline (2026-10-07)

Result: Fei-Yen exported game-accurately to work\export\FeiYen (OBJ + MTL + PNG), from an F9
capture on the Customize screen (work\caps\cap0_*). User confirmed: sabre is in her LEFT hand;
gold parts are HYPER-mode versions of the pink parts (normal mode = pink).

## Files (media/rom/data.farc, compressed FArC: name\0, u32 BE offset, csize, usize; gzip)
Extract: `python work\farc.py <data.farc> work\farc_data`. Per robot XXX:
- `xxx1c.mrg` / `xxx1c.mg2`: in-game model + textures (1P colours). `2c` = 2P colours.
  `1a`/`2a` = a smaller model set (111 objects for Fei-Yen; not what the game draws in battle).
- `XXX.mt`: motion data (compressed keyframes; NOT decoded — not needed, see pipeline).
All data little-endian.

### .mrg (geometry)
- Header: u32 2, u32 0x10, u32, u32. Table at 0x10 of (u32 offset, u32 id); object = offset+0x10.
  First entry points into the table (not an object).
- Object: +0 link (file offset of an ALTERNATE version of this part, +0x10; 0 = none),
  +4 pos xyz f32, +0x10 rot xyz int32 (2^32 = 360°), +0x1C scale xyz, +0x28 material index
  (into .mg2 material table), +0x2C flags, +0x30 radius, +0x34 vertex count.
  Vertices: 16 bytes: x,y,z f32 + normal packed signed 10:10:10 (bits 0,10,20; /512).
  Then u32 polygon count; polygon: u32 colour, u32, u32, u32 flags (byte 2 = vertex count),
  then n × (u16 vertex index, u16 u, u16 v) padded to 4 bytes. Polygons are TRIANGLE STRIPS.
  UVs are in texels (divide by texture size; effects tile).
- Normal and hyper versions of a part can have identical headers; only UVs differ
  (pink vs gold tiles of the atlas).

### .mg2 (materials + textures)
- Header u32 2, 0x10, offset of PVPL palette, ... At 0x10: u32 material count, then 20-byte
  materials (4 PowerVR control words + texture index, -1 = untextured), then u32 texture count
  + offsets (+0x10). Textures are Dreamcast PVRT (optionally GBIX first): formats ARGB1555 /
  RGB565 / ARGB4444; data 0x01 twiddled, 0x09 rectangle, 0x07 8-bit paletted twiddled.
- `PVPL` block (256 ARGB1555) = palette for the paletted textures; differs per colour variant.
  Fei-Yen body = texture 0, 512x512 paletted atlas.

## Runtime (guest memory) facts used
- Models loaded byte-swapped (floats per u32, polygon index/UV per u16, colours untouched);
  object link words become pointers. Found by header signature + polygon UV comparison.
- Draw list: base *(0x839EFB78), count *(0x839EFB7C), 0x15C-byte records: +0x4C = object
  address + 0x2C for model parts, +0x60 = world matrix 4x4 (row vectors, translation row 3),
  +0x00 modelview, +0xA0 view.
- Live palettes are ARGB8888 (5-bit << 3) e.g. at 0xADCC5000.
- World is LEFT-handed (x right, y up, z forward): export negates z (and normals' z) and
  reverses winding for right-handed OBJ.

## Pipeline (work\)
1. Run game with `--capture_dir=<dir>`; press F9 with the robot on screen → capN_data.bin +
   capN_mem.bin (memory 0xA4000000-0xAE000000). Restart button drops the option.
2. `python locate2.py caps\capN_mem.bin fei1c` → addr2obj_fei1c.pkl.
3. `MEM=caps/capN_mem.bin DATA=caps/capN_data.bin python feiexport.py <outdir>` → OBJ/MTL/PNG.
4. Check: `python texrender.py <outdir>/groups.pkl out.png` (renders in game's left-handed space).
Scripts: farc.py, mrg.py, pvr.py, png4.py, drawlist.py, locate2.py, feiexport.py, texrender.py.

## Rig (feiyen_rigged.glb)
- Skeleton is derived from the game, not guessed: 34 F9 captures (customize + 33 Training poses:
  running, jumping, falling, boosting, arm gun, sabre). Parts are matched across captures by
  content (nv, material, polygon count, header pos), since load addresses change per session.
- Parts that never move relative to each other = one bone (21 bones). For each part pair a
  least-squares fixed pivot over all poses is solved (residual ~0.000 for real joints); tree built
  outward from the pelvis choosing zero-residual parents whose geometry touches the pivot.
- Export: glTF 2.0 binary, single skinned mesh, each part weighted 100% to its bone, rest pose =
  Customize capture, z negated (left->right handed), rotated to face +Z (sabre hand = her LEFT),
  feet at y=0. Verified by reading the .glb back and posing it (glbcheck.py).
- Scripts: poses.py, skeleton.py, tree.py, rigexport.py, glbcheck.py; data rigdata.pkl, tree.pkl.
