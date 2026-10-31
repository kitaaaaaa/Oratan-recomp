# Oratan-recomp

A native PC version of **Cyber Troopers Virtual-On Oratorio Tangram
M.S.B.S. Ver.5.66**, made by recompiling the 2009 Xbox Live Arcade release with ReXGlue, adding enhanced features like higher resolutions and split-screen multi-player. 

> [!NOTE]
> Alpha. The game is playable from start to finish. Please
> [report bugs](https://github.com/kitaaaaaa/Oratan-recomp/issues).

>[WARNING: THIS VERSION IS NOT A SIMULATORY SYSTEM - ALL REGISTERED USER DATA WILL BE TRANSMITTED TO THE DEPLOYED VIRTUAROID SYSTEM MEMORY BANK]

## How to play

You need your own copy of the Xbox Live Arcade game. No game files are
included here.

1. **Download** the latest `Oratan-recomp-…-win64.zip` from
   [Releases](https://github.com/kitaaaaaa/Oratan-recomp/releases) and unzip it
   anywhere.
2. **Add your game** to the `assets` folder inside it (see below).
3. **Double-click `oratan.exe`.**

Windows 10 or 11 (64-bit) with a DirectX 12
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
- **F1** opens Options (resolution, anti-aliasing, anisotropic filtering, VSync,
  fullscreen, local versus).
- **Alt+Enter** switches between fullscreen and a window. To move the game to
  another monitor: Alt+Enter, drag the window over, Alt+Enter again. The
  choice is remembered.

A log is written to the `logs` folder next to `oratan.exe`; attach it to bug
reports.

### Graphics

**F1 → Resolution** offers the original 1280×720 and native **1920×1080**,
**2560×1440** and **3840×2160**. The original game draws its 3D scene into a
1024×1024 image and stretches it onto the 720p screen. The native modes draw
the scene and the HUD at exactly the chosen size instead, so the image is
sharp rather than upscaled. Shadows stay at the original resolution (the game
clears them through a trick that only works there). Changing the resolution
needs a restart; the Options window offers a **Restart now** button.

With a native resolution:

- **Anti-aliasing:** Off, 2× MSAA or 4× MSAA (the game's own setting). Applies
  to the 3D scene and needs a restart.
- **Anisotropic filtering:** Off to 16×, applies immediately. Keeps floors and
  other surfaces seen at shallow angles sharp.

All native modes run at 60 fps on a mid-range DirectX 12 graphics card (tested
on an RTX 2080, including 4K).

### Local two-player versus (experimental)

The Xbox Live Arcade release only had online versus. With two controllers
connected:

1. Press **F1** and tick **Player 2 controls the opponent** and **Split screen
   during battles**. **Split display** picks the layout: staggered (player 1
   top-left, player 2 bottom-right), side by side, or one monitor per player
   (the window spans two side-by-side monitors, like the arcade's twin
   cabinets). **Swap sides** puts player 1 on the right.
   With one monitor per player, **Alt+Enter** switches between spanning the
   two monitors and a normal window you can move. To use other monitors (say
   2 and 3 of three), press Alt+Enter, drag the window over them, and press
   Alt+Enter again. The monitors are remembered.
2. Start any match (Training or Arcade) with controller 1. Controller 2 takes
   over the opponent robot, and each player gets their own view side by side.

Controller 2 also takes over Arcade bosses when they appear such as Ajim, Bradtos and Tangram. A
player-controlled Bradtos can open up to expose its core with a guard-turbo
attack, though not every time. P2 as Tangram has not been tested.

Unticking **Player 2 controls the opponent** mid-match hands the opponent back
to the CPU. With split screen on but player 2 control off, the second view
shows the LIVE MONITOR camera from Observer mode. With split screen on, Arcade's Continue and Game Over screens are
also shown twice, side by side.

### Online versus (experimental)

Two players on different PCs can play versus over the internet or a local
network. The game has to start in step on both PCs, so starting or leaving a
session restarts it.

1. **Player 1 hosts:** press **F1**, open **Netplay**, pick **Host**, and press
   **Host (restarts the game)**. The game restarts and waits for player 2. The
   panel lists this PC's addresses to give player 2.
2. **Player 2 joins:** press **F1**, open **Netplay**, pick **Join**, type the
   host's address and port (for example `192.168.1.20:7000`), and press
   **Join (restarts the game)**.
3. Once connected, both games start together and play from the title screen.
   Player 2 drives the opponent, and each player sees their own robot.

Both PCs need the same game files, save data and settings. Over the internet,
the host forwards the UDP port (7000 by default) to their PC, or both players
use a VPN such as ZeroTier or Tailscale. **Input delay** (set by the host)
hides network lag: 3 suits a local network, 4-5 a connection of about 40 ms.
The panel says when the game had to wait for the other player often, and
warns if the two games fall out of sync. **Leave netplay** restarts the game
offline. If the other player disconnects, the game carries on offline after 10
seconds.

### Hidden stages and Bradtos (Training)

The game has stages and a boss that its menus never offer. In Training:

- **Stage select:** press **LB** or **RB** (or Page Up / Page Down) to switch to
  a second page. The two slots after RANDOM become **DISTORTED SHRINE** (the
  mid-boss stage) and **TANGRAM** (the final stage). They have no preview art
  of their own and show the SANCTUARY 2 and SPACE STATION cards. Press again to
  switch back.
- **Character select:** with the cursor on **AJIM**, press **LB** or **RB** to
  switch that slot to **BRADTOS** (and back). The slot keeps a generic picture
  while it is BRADTOS. As the CPU opponent, BRADTOS plays its Arcade intro and
  defeat scenes.

Other unlisted stages and robots (SPACE CARRIER, TANGRAM CORE, the TANGRAM
robot, and BAL-BADOS' stage forms BAL-KEROS and BAL-BAROS on other stages)
crash or misbehave and are left out.

## Status

| | |
|---|---|
| Single player Arcade mode (start to finish) and Training | Working, 60 fps |
| Other modes (Score Attack, Customize), saving | Working, minimally tested |
| Native 1080p / 1440p / 4K, anti-aliasing, anisotropic filtering | Working, 60 fps |
| VSync, windowed mode | Working |
| Local two-player versus with split screen | Working, experimental, see above |
| Hidden stages (Distorted Shrine, Tangram) and Bradtos in Training | Working, see above |
| Online versus (netplay, F1 → Netplay) | Experimental, tested on one PC; see above |
| Original Xbox Live / System Link menus | Not supported (netplay replaces them) |

## Building from source

Only needed if you want to work on the project.

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/vs/community/)
   with **Desktop development with C++**, plus [LLVM/Clang 20+](https://github.com/llvm/llvm-project/releases),
   [CMake 3.25+](https://cmake.org/download/) and Ninja.
2. Build ReXGlue SDK v0.10.0 from source with the patches in
   [sdk-patches/](sdk-patches/README.md), installing it to a folder of your
   choice (the SDK prefix). The stock prebuilt SDK zip also works, minus the
   VSync option; its prefix is the `win-amd64` folder inside the zip.
3. Put your game files in `assets/`.
4. Configure and build (the first build recompiles the game, which takes a few minutes):
   ```
   cmake --preset win-amd64-release -DCMAKE_PREFIX_PATH=<SDK prefix>
   cmake --build out\build\win-amd64-release
   ```
   `build.bat` does the same and also finds portable tools in a sibling
   `..\tools\` folder.
5. `python tools/package_release.py <version>` makes the release zip.

| Path | What |
|---|---|
| `oratan_manifest.toml` | Recompiler settings (entry XEX, setjmp/longjmp) |
| `oratan_config.toml` | Hand-fixed function boundaries for this game |
| `src/` | The PC side: startup, paths, patches |
| `tools/` | Analysis and packaging scripts |
| `generated/default/` | Recompiled code (generated locally, not committed) |

## Legal

Unofficial fan project, not affiliated with or endorsed by SEGA or Microsoft.
Virtual-On is a trademark of SEGA. Character design by Hajime Katoki.
No copyrighted game data is provided.

## Credits

- [ReXGlue](https://github.com/rexglue/rexglue-sdk), the recompilation SDK
- [Xenia](https://github.com/xenia-project/xenia), whose emulator code the runtime builds on
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), which pioneered this approach
