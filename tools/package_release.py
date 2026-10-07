"""Assemble the player-facing release zip from a release build.

  python tools/package_release.py 0.1.0

Produces dist/Oratan-recomp-<version>-win64.zip containing:
  Oratan-recomp/
    oratan.exe, rexruntime.dll, rexgpu-xenos.dll
    Visual C++ runtime DLLs (app-local, so no redistributable install is needed)
    assets/HOW TO ADD THE GAME.txt   (players put their game files here)
    README.txt

No game files are included; players supply their own.
"""

import os
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "out" / "build" / "win-amd64-release"
BINARIES = ["oratan.exe", "rexruntime.dll", "rexgpu-xenos.dll"]
VC_RUNTIME = ["msvcp140.dll", "msvcp140_atomic_wait.dll", "vcruntime140.dll", "vcruntime140_1.dll"]

ASSETS_TXT = r"""Put your Virtual-On Oratorio Tangram game in THIS folder, then start oratan.exe.

Easiest: copy the game's package file straight in here. It is a single file
named

  2A944528D84678B7C9F0270A564B8E653520EF72

found on your Xbox 360 under Content\0000000000000000\58410985\000D0000\
(or in Xenia's content folder). It is unpacked automatically the first time
you start the game.

Already-extracted files (default.xex and the media folder) work too.
"""

README_TXT = """\
Virtual-On Oratorio Tangram (Ver.5.66) - PC recompilation  v{version}
=====================================================================

1. Copy your own copy of the game into the "assets" folder
   (see assets\\HOW TO ADD THE GAME.txt).
2. Double-click oratan.exe.

Controls:
  Xbox controllers work as on the console.
  F1         Options (resolution, VSync, fullscreen, local versus)
  Alt+Enter  Switch between fullscreen and a window
  LB / RB    In Training: on the stage select, show the hidden stages
             (Distorted Shrine, Tangram); on the character select, with the
             cursor on AJIM, switch that slot to BRADTOS

Alpha: the game is playable from start to finish. Bug reports and logs are
welcome at
https://github.com/kitaaaaaa/Oratan-recomp/issues
A log is written to the "logs" folder next to oratan.exe.

This is an unofficial fan project, not affiliated with SEGA or Microsoft.
No game files are included.
"""


def find_vc_runtime(name: str) -> Path:
    system32 = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
    path = system32 / name
    if not path.exists():
        sys.exit(f"missing {path}; install the latest VC++ redistributable")
    return path


def main() -> int:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    version = sys.argv[1]

    stage = ROOT / "dist" / "Oratan-recomp"
    if stage.exists():
        shutil.rmtree(stage)
    (stage / "assets").mkdir(parents=True)

    for name in BINARIES:
        src = BUILD / name
        if not src.exists():
            sys.exit(f"missing {src}; build the release preset first")
        shutil.copy2(src, stage / name)
    for name in VC_RUNTIME:
        shutil.copy2(find_vc_runtime(name), stage / name)

    (stage / "assets" / "HOW TO ADD THE GAME.txt").write_text(ASSETS_TXT, newline="\r\n")
    (stage / "README.txt").write_text(README_TXT.format(version=version), newline="\r\n")

    out = ROOT / "dist" / f"Oratan-recomp-{version}-win64.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in sorted(stage.rglob("*")):
            z.write(f, f.relative_to(stage.parent))
    print(f"{out} ({out.stat().st_size / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
