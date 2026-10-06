# Oratan-recomp

A static recompilation of **Cyber Troopers Virtual-On Oratorio Tangram
M.S.B.S. Ver.5.66** (Xbox Live Arcade, 2009) to native Windows, built on the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

The game's PowerPC code is translated ahead of time into C++ and compiled for
x86-64. ReXGlue supplies the Xbox 360 runtime (kernel, file system, audio, and
the Xenos GPU layer derived from Xenia).

> [!IMPORTANT]
> This repository contains **no game code or data**. You need your own copy of
> the XBLA release. The recompiled C++ is generated locally from your copy and
> is never committed.

> [!WARNING]
> Early work in progress. Expect crashes, graphical bugs, and missing features.

## Status

| Phase | State |
|---|---|
| XEX analysis / codegen | Done: 0 analysis errors, ~14.3k functions |
| Builds and links | Done |
| Boots to title, menus, attract mode | Working |
| Gameplay | Testing |

## Requirements

- Windows 10/11, x64
- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/community/) with
  the **Desktop development with C++** workload (for the MSVC STL and Windows SDK)
- Clang 20 or newer ([LLVM releases](https://github.com/llvm/llvm-project/releases))
- CMake 3.25 or newer, and Ninja
- [ReXGlue SDK v0.10.0](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0)
  (`rexglue-sdk-0.10.0-win-amd64.zip`)

## Building

1. Extract your copy of the game into `assets/` (see
   [assets/README.md](assets/README.md)).
2. Unzip the ReXGlue SDK somewhere and set `REXSDK` to its `win-amd64` folder:
   ```bat
   set REXSDK=C:\path\to\rexglue-sdk\win-amd64
   ```
3. Run:
   ```bat
   build.bat release
   ```
   The first build runs `rexglue codegen` on `assets/default.xex` and compiles
   the result, which takes a while.
4. The game is at `out\build\win-amd64-release\oratan.exe`. Run it with the
   game folder as its argument, or copy `assets/` next to the exe:
   ```bat
   out\build\win-amd64-release\oratan.exe assets
   ```

`build.bat` also picks up portable tools placed in a sibling `..\tools\`
folder (`llvm\`, `cmake\`, `rexsdk\win-amd64\`).

## Project layout

| Path | What |
|---|---|
| `oratan_manifest.toml` | Codegen manifest (entry XEX, setjmp/longjmp) |
| `oratan_config.toml` | Manual function boundaries and hooks for this game |
| `src/` | The host app: boot defaults, hooks, patches |
| `generated/rexglue.cmake` | SDK integration (managed by `rexglue`) |
| `generated/default/` | Recompiled code (generated locally, gitignored) |

## Legal

This project is not affiliated with SEGA or Microsoft. Virtual-On is a
trademark of SEGA. Do not ask for or share game files here.

## Credits

- [ReXGlue](https://github.com/rexglue/rexglue-sdk), the recompilation SDK
- [Xenia](https://github.com/xenia-project/xenia), whose emulator code the runtime builds on
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), which pioneered this approach
