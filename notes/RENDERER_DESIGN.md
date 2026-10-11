# Renderer rework: design

Goal: render at exactly 1920×1080 (or any size) with no up- or downscaling. Phase 1 (what the
game asks of the GPU) is in `RENDERER_API.md`.

## Why the current renderer can't do 1.5×

The current renderer emulates the Xbox 360 GPU (Xenos) faithfully, including its 10 MB of
on-chip memory (EDRAM). Resolution scaling is limited to whole numbers by two parts of it:

1. **Resolves go through an EDRAM-layout buffer.** Every copy from a render target to a
   texture first lays the pixels out the way EDRAM stores them, then a compute shader writes
   them into guest memory. Scaling is applied per EDRAM sample.
2. **Scaled textures live in a "scaled copy of guest memory".** When a resolved image is
   later used as a texture, the texture cache finds it by guest memory address, with each guest
   texel owning exactly scale × scale host texels. At 1.5× a guest texel would own 2.25 host
   texels, so the addressing breaks.

Everything else already works with any scale. The shader translator applies the scale as a
float (pixel position, texture offsets), and viewports and scissors are plain multiplications.

## The decision: keep the shader translator, replace render targets and resolves

The original plan was a full native renderer in the style of Unleashed Recompiled, including
converting all 1,416 shaders ourselves. Phase 1 changed that:

- The shaders are compiled Xbox 360 microcode (no source), and the SDK already contains a
  complete, tested Xenos-to-DXBC translator (about 10,000 lines). It already translates every
  shader this game uses, correctly. Writing a new one would take months and gain nothing.
- Texture loading (Xbox 360 tiled layouts and formats), guest memory uploads, vertex fetching
  and the pipeline cache all work and don't limit resolution.
- Only the render-target and resolve layer ties resolution to EDRAM.

So the new renderer is a **native render-target mode** inside the existing D3D12 backend.

### 1. Render targets are ordinary host textures

Each render target the game binds (EDRAM base, pitch, format, multisampling) gets a host
texture of exactly guest size × scale, rounded. The scale can be fractional (1.5 for 720p →
1080p). Render targets that share EDRAM aren't kept in sync (no EDRAM ownership model). Checking
that the game never depends on that is task A below.

Predicated tiling needs no special handling. Each of the 4 tiles draws into the same host
target, offset by the window offset, and is resolved into its band of the destination
texture, exactly as on the console.

### 2. Resolves are host copies into native textures

A resolve copies a rectangle of a render target into a **native resolve texture** at the
same scale. That covers multisample resolve, the 7e3 → texture format conversion and the
exponent bias. Native resolve textures are keyed by guest destination address and format. The
optional clear that comes with a resolve clears the host render target.

Nothing is written back to guest memory, unless task A finds a resolve the CPU reads (for
example, average luminance read back for auto exposure). Those few small resolves would also
take the old path.

### 3. Textures that are resolve outputs bind the native texture

When a draw samples a texture whose base address is a native resolve texture, the texture
cache binds that host texture instead of loading guest memory. Normalised texture coordinates
need no change. Unnormalised coordinates and texel offsets are scaled by the translator, which
already does this for integer scales.

### 4. Presentation

The final frame is resolved to the front buffer address as usual. With a native resolve
texture there, the presenter shows it 1:1 at 1920×1080.

## Tasks

A. **Measure** (instrumentation in the SDK, `gpu_frame_stats` + `gpu_frame_dump_every`). For
   every draw: render targets, depth, and the texture addresses it samples. For every resolve:
   source, destination, size and format. Answers: which textures are resolve outputs; whether
   any render targets share EDRAM with different formats while their contents are still needed;
   whether any resolve output is read by the CPU.
B. **Fractional scale plumbing.** Allow a float scale (or a ratio such as 3/2) where the
   translator, viewport and scissor code use the integer scale today.
C. **Native render-target cache.** A new `RenderTargetCache` path next to the existing
   host-render-target and ROV paths: creation, binding, clears, depth.
D. **Native resolves.** Copy and convert shaders; resolve-texture registry.
E. **Texture cache hook.** Bind native resolve textures by address.
F. **Settings.** "Native render targets" plus a resolution (1080p, 1440p, 4K) in F1. The old
   path stays the default until the new one matches it.
G. **Parity testing.** Title, menus, every stage, effects, split screen.

## Status (2026-10-09)

**Working.** Settings: F1 → Resolution → 1920 x 1080 / 2560 x 1440 / 3840 x 2160, then
restart (or `--native_render_targets=true --native_resolution=1080`). All three run at 60 fps on
an RTX 2080. Tested: title, menus, attract mode, Training battle. At 1× (native mode,
`native_resolution=0`) the image is identical to EDRAM emulation.

What turned out to matter, beyond the plan above:

- **The scene is anamorphic.** The game renders its 3D scene at 1024×1024 (4× MSAA, 4 tiles of
  256 rows) and stretches it onto the 1280×720 frame. So scale is chosen *per render target*:
  everything 1040 pixels wide (scene, its depth, the shadow map, the tone-mapped image) gets
  1920/1024 × 1080/1024; the 1280×720 frame gets 1.5; the bloom/luminance chain stays 1×.
  256 rows × 1080/1024 = 270 exactly, so tiles have no seams (also at 1440 and 2160).
- **EDRAM transfers are still needed between targets at the same scale.** Direct3D 9's Clear
  fills part of a target through a 4× MSAA half-pitch alias of the same EDRAM; without the
  transfer the bloom accumulates stale glow. Transfers are kept when both targets are at 1×
  and dropped otherwise.
- **Resolve destinations use the adjusted base** (RB_COPY_DEST_BASE plus the rectangle's tile
  offset), because the game resolves rectangles outside the destination pitch and samples
  them as separate small textures (the 8×16 luminance texture).
- Sub-rectangles of a native texture that are sampled as their own texture get a small derived
  copy, refreshed when the source changes.

Code: SDK `native_textures.{h,cpp}` (native textures, resolve shaders compiled at start-up with
D3DCompile), hooks in the render-target cache (`GetNativeScale`, `FilterNativeTransfers`, native
resolve), texture cache (bindless SRV override, front buffer) and command processor (per-draw
viewport/scissor scale). Game: `options_menu.cpp`, `settings.cpp`.

Fixed after user testing (2026-10-10):

- **Shadows** showed last frame's screen on the ground: the shadow mask shares its EDRAM layout
  (base 0, 1040 wide, 1x) with the tone-mapped scene, and is cleared through a 4x MSAA alias.
  Render targets drawn with a depth buffer in that layout now get their own key
  (`native_class` 1) and stay 1x, so the EDRAM transfer that carries the clear works exactly as
  on the console. Tone mapping (no depth) stays scaled. Resolves use the class drawn last.
  Forwarding the clear with values read from the clear draw's vertices does not work: that
  guest memory has already been reused when the GPU thread gets to the draw.
- **Blurry, moiré output**: the box-filtered copies (for the bloom chain) were also made for the
  composite draw, halving the final image horizontally. They are now only made for draws into
  1x render targets.
- Options: Anti-aliasing (Off / 2x / 4x, scene only), Anisotropic filtering (Off–16x).

The SDK changes are `sdk-patches/0004-native-render-targets.patch`. Debugging tools used
along the way (frame dumps, texture viewer, D3D trace) are in `tools/dev-instrumentation/`.

Not done yet: precompiled resolve shaders instead of runtime D3DCompile (D3DCompiler_47.dll
ships with Windows 10+); resolved depth buffers' byte order is unverified (not sampled in any
scene tested).

## Lighting (2026-10-10, uncommitted)

Added on top of the native renderer, as one full-screen pass over the HDR scene (before bloom and
tone mapping), so the game's own lighting and effects are untouched.

**Sharp shadow mapping.** The game already renders a shadow pass per robot: the robot from the
light's point of view, with depth, into a 1024×1024 target (it uses only the silhouette, blurred,
as a soft blob on the ground). The light-space depth buffer is a proper shadow map. The robots are
drawn with the same index buffers in the shadow pass (light matrix in c36–c39) and in the scene
(camera matrix in c36–c39); light × inverse(camera) is the same for every mesh because the model
matrix cancels out, which gives a camera-clip → light-clip transform per robot per frame. For each
scene pixel (from the 1080p scene depth) the pass looks up both light maps (3×3 PCF) and darkens
the HDR colour. Result: robots shadow themselves and cast sharp shadows on the floor and on each
other. Verified with a CPU readback of the depth buffers analysed offline.

**Ambient occlusion.** From the same depth buffer and the camera projection (taken from the same
matrices: near plane 0.5, focal lengths with exactly the 16:9-in-square ratio). Alchemy-style
obscurance, 12 samples, normals reconstructed from depth.

Settings: F1 → Sharp shadows / Ambient occlusion (native resolutions only, apply immediately).
cvars `native_shadows`, `native_shadow_strength` (0.5), `native_ao_strength` (1.0),
`native_ao_radius` (1.5), `native_shadow_debug` (1 = shadow factor, 2 = AO). Cost: none measurable
(60 fps at 1080p).

Not done / untested: mirror matches (same robot twice — the k-th scene draw of a mesh is paired
with its k-th shadow-pass draw, which assumes both passes draw the robots in the same order);
stages other than Public Port; lighting from projectiles/explosions (would need the game's
dynamic light data).

**Update 2026-10-11: self-shadowing off by default.** It looked wrong on robots with skirts
(Fei-Yen, Angelan): strange lower-body shadows. Robot draws (scene meshes shared with the shadow
pass) now write their shadow-pass number + 1 into the stencil (the game doesn't use stencil in the
scene; the override is skipped and a warning logged if it ever does). The depth resolve keeps the
stencil, and a robot ignores its own light map unless `native_self_shadow` is on — it is still
shadowed by the other robot, and both still cast floor shadows. The pairing of scene draws with
shadow-pass draws restarts per tile (the scene is drawn in 4 tiles). A depth-gap threshold was
tried first and rejected: it also removed floor shadows near the robots. Debug view 3 shows the
robot mask.
