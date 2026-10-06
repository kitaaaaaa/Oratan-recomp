# SDK patches

Changes applied to [ReXGlue SDK v0.10.0](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0)
before building it from source. The release zip ships the patched runtime DLLs.

| Patch | What |
|---|---|
| `0001-present-vsync.patch` | Adds the `present_vsync` setting (F1 → VSync): presents without tearing but never blocks, so the game keeps full speed in fullscreen. The stock SDK always presents with tearing allowed. |
| `0002-split-screen.patch` | Split screen: the game tags each frame swap with the view it was drawn from (`rex/system/split_screen.h`); the D3D12 backend places tagged views side by side and presents them as one image. Adds `guest_vblank_multiplier` so a title drawing two frames per tick keeps its speed. |

To build the patched SDK:

```
git clone --recursive https://github.com/rexglue/rexglue-sdk.git
cd rexglue-sdk
git checkout v0.10.0
git apply ../Oratan-recomp/sdk-patches/*.patch
cmake --preset win-amd64 -DCMAKE_INSTALL_PREFIX=<prefix>
cmake --build out/build/win-amd64 --config Release --target install
```

On Windows, check that Git checked out the symlinked files in
`thirdparty/libmspack` as real files (`git config core.symlinks true`), or the
build fails in `lzxd.c`.

Then pass `-DCMAKE_PREFIX_PATH=<prefix>` when configuring this project.
