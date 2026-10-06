# Oratan-recomp

A native PC version of **Cyber Troopers Virtual-On Oratorio Tangram
M.S.B.S. Ver.5.66**, made by recompiling the 2009 Xbox Live Arcade release.
It is not an emulator: the game's code was translated into a regular Windows
program.

> [!WARNING]
> Early alpha. It boots, reaches the menus and plays the attract mode.
> Expect bugs; please [report them](https://github.com/kitaaaaaa/Oratan-recomp/issues).

## How to play

You need your own copy of the Xbox Live Arcade game. No game files are
included here.

1. **Download** the latest `Oratan-recomp-…-win64.zip` from
   [Releases](https://github.com/kitaaaaaa/Oratan-recomp/releases) and unzip it
   anywhere.
2. **Add your game** to the `assets` folder inside it (see below).
3. **Double-click `oratan.exe`.**

Nothing needs to be installed. Windows 10 or 11 (64-bit) with a DirectX 12
graphics card is required.

### Adding your game

Copy the game's Xbox Live Arcade package file into `assets`. It is a single
file named `2A944528D84678B7C9F0270A564B8E653520EF72`, found on an Xbox 360
under `Content\0000000000000000\58410985\000D0000\` (or in Xenia's
`content` folder). It is unpacked automatically the first time you start the
game.

Already-extracted files (`default.xex` and the `media` folder) work too.

### Controls

- Xbox controllers work as on the console.
- **F1** opens Options (resolution, fullscreen).
- **Alt+Enter** switches between fullscreen and a window. To move the game to
  another monitor: Alt+Enter, drag the window over, Alt+Enter again. The
  choice is remembered.

A log is written to the `logs` folder next to `oratan.exe`; attach it to bug
reports.

## Status

| | |
|---|---|
| Boots, title screen, menus, attract mode | Working |
| Gameplay | Testing |
| Xbox Live features | Not supported |

## Building from source

Only needed if you want to work on the project.

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/vs/community/)
   with **Desktop development with C++**, plus [LLVM/Clang 20+](https://github.com/llvm/llvm-project/releases),
   [CMake 3.25+](https://cmake.org/download/) and Ninja, and unzip the
   [ReXGlue SDK v0.10.0](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0)
   (`rexglue-sdk-0.10.0-win-amd64.zip`).
2. Put your game files in `assets/`.
3. Configure and build (the first build recompiles the game, which takes a few minutes):
   ```
   cmake --preset win-amd64-release -DCMAKE_PREFIX_PATH=<path to SDK>\win-amd64
   cmake --build out\build\win-amd64-release
   ```
   `build.bat` does the same and also finds portable tools in a sibling
   `..\tools\` folder.
4. `python tools/package_release.py <version>` makes the release zip.

| Path | What |
|---|---|
| `oratan_manifest.toml` | Recompiler settings (entry XEX, setjmp/longjmp) |
| `oratan_config.toml` | Hand-fixed function boundaries for this game |
| `src/` | The PC side: startup, paths, patches |
| `tools/` | Analysis and packaging scripts |
| `generated/default/` | Recompiled code (generated locally, not committed) |

## Legal

Unofficial fan project, not affiliated with or endorsed by SEGA or Microsoft.
Virtual-On is a trademark of SEGA. Do not ask for or share game files here.

## Credits

- [ReXGlue](https://github.com/rexglue/rexglue-sdk), the recompilation SDK
- [Xenia](https://github.com/xenia-project/xenia), whose emulator code the runtime builds on
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), which pioneered this approach
