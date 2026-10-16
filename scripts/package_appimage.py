#!/usr/bin/env python3
"""Package the Linux AppImage.

The image contains this repository's tracked source and a first-run wrapper.
It contains no ROM, no game assets and no code generated from a ROM: the game
is built on the user's machine from their own ROM on first launch.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PACKAGING = ROOT / "packaging/linux"
LAUNCHER = PACKAGING / "launcher"
# Files that are never needed to build the game from a ROM.
EXCLUDED_PREFIXES = (".github/", "docs/images/", "evidence/", "launcher/windows/")


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def make_icon(destination: Path) -> None:
    """Extract the largest PNG frame from the application icon (no extra tools)."""
    data = (ROOT / "launcher/ui/assets/app.ico").read_bytes()
    reserved, kind, count = struct.unpack_from("<HHH", data)
    if reserved != 0 or kind != 1 or count == 0:
        raise ValueError("Unexpected application icon format.")
    best = None
    for index in range(count):
        width, _, _, _, _, _, size, offset = struct.unpack_from("<BBBBHHII", data, 6 + index * 16)
        width = width or 256
        frame = data[offset:offset + size]
        if frame.startswith(b"\x89PNG\r\n\x1a\n") and (best is None or width > best[0]):
            best = (width, frame)
    if best is None:
        raise ValueError("The application icon has no PNG frame.")
    destination.write_bytes(best[1])


def build_launcher(destination: Path) -> None:
    """Compile the first-run window against the system GTK 3 (no game data involved)."""
    flags = subprocess.check_output(["pkg-config", "--cflags", "--libs", "gtk+-3.0"], text=True).split()
    destination.parent.mkdir(parents=True, exist_ok=True)
    # gnu11 keeps the binary off glibc 2.38-only C23 symbols such as __isoc23_strtol.
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-O2", "-s", "-Wall", "-Wextra",
                    "-Wno-deprecated-declarations", str(LAUNCHER / "jfg-launcher.c"), "-o", str(destination),
                    *flags], check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/appimage")
    parser.add_argument("--appimagetool", help="Path to appimagetool (default: $APPIMAGETOOL or PATH)")
    parser.add_argument("--allow-dirty", action="store_true",
                        help="Package even if the working tree has uncommitted changes")
    args = parser.parse_args()

    if git("status", "--porcelain").strip() and not args.allow_dirty:
        raise ValueError("The working tree has uncommitted changes; commit them or pass --allow-dirty.")
    revision = git("rev-parse", "--short=12", "HEAD").decode().strip()
    if args.allow_dirty and git("status", "--porcelain").strip():
        revision += "-dirty"

    output = args.output.resolve()
    appdir = output / "JFG.AppDir"
    if appdir.exists():
        shutil.rmtree(appdir)
    source = appdir / "usr/share/jfg-recomp/src"
    source.mkdir(parents=True)
    for raw in git("ls-files", "-z").split(b"\0"):
        if not raw:
            continue
        name = raw.decode("utf-8")
        if name.startswith(EXCLUDED_PREFIXES):
            continue
        path = ROOT / name
        if path.is_symlink() or not path.is_file():
            continue
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)

    (appdir / "usr/share/jfg-recomp/source-id").write_text(revision + "\n")
    shutil.copy2(PACKAGING / "AppRun", appdir / "AppRun")
    (appdir / "AppRun").chmod((appdir / "AppRun").stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    shutil.copy2(PACKAGING / "jfg-recomp.desktop", appdir / "jfg-recomp.desktop")
    make_icon(appdir / "jfg-recomp.png")
    build_launcher(appdir / "usr/bin/jfg-launcher")
    background = appdir / "usr/share/jfg-recomp/launcher/background.jpg"
    background.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(LAUNCHER / "background.jpg", background)
    print("AppDir ready: " + str(appdir))

    tool = args.appimagetool or os.environ.get("APPIMAGETOOL") or shutil.which("appimagetool")
    if not tool:
        print("appimagetool not found. Download it from https://appimage.github.io/appimagetool/ ,\n"
              "make it executable and rerun with --appimagetool PATH. You can also test the\n"
              "AppDir directly: " + str(appdir / "AppRun"))
        return 0
    image = output / f"JFG-{revision}-x86_64.AppImage"
    environment = dict(os.environ, ARCH="x86_64")
    # --appimage-extract-and-run lets appimagetool itself run without FUSE.
    subprocess.run([tool, "--appimage-extract-and-run", str(appdir), str(image)],
                   check=True, env=environment)
    print("Built: " + str(image))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
