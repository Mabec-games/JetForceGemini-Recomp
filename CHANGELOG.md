# Changelog

## Unreleased

- Added experimental Linux x86-64 support for the native runtime: POSIX guest
  memory mapping and device-register trapping, an SDL2 window with keyboard,
  mouse and gamepad input, a `fork`/`exec` process supervisor, and
  `scripts/build_from_rom.py` for Linux. See [Linux support](docs/development/linux.md).
  The Windows launcher and its companion tools are unchanged and Windows-only.

## 1.1.0 — 2026-10-09

- Fixed the embedded game's aspect ratio. Gameplay stays in a centered 4:3
  display area when the launcher is wide, resized, maximized, or fullscreen.
  Cutscenes retain their own top and bottom bars instead of expanding across
  the launcher window. The original game's graphics and widescreen option
  are unchanged.
- Added one-time ROM import. Select a supported ROM once; the launcher verifies
  and copies it into the local game profile. Future launches use that copy,
  so the original file can be moved or deleted. Invalid imports preserve the
  previous copy. Existing valid ROM selections are imported on upgrade.
- Removed the central ROM card after a successful import. **Game > Select ROM…**
  remains available to select another file.
- Replaced the first-run local build with a prebuilt Windows x64 game bundle.
  Players no longer need Git, Python, Visual Studio Build Tools, WSL, or a
  first-run compilation. Required Microsoft runtime DLLs are included.
- Added a clear message when only software graphics are available, avoiding
  the stalled startup and excessive memory use observed in the test VM.
- Added package integrity checks, a ROM-import regression test, and viewport
  coverage for 4:3, 16:9, ultrawide, portrait, and odd-sized windows.

Download and extract the complete v1.1.0 ZIP into a new folder. Open
`JFG-Launcher.exe`, use **Select ROM…** if prompted, and click **Play**. Existing
saves and settings remain in the same profile. The ZIP contains the compiled
program and its runtime libraries; **no ROM or game assets are included**.

The simplified installation approach was inspired by the
[Zelda64Recomp team and its plug-and-play model](https://github.com/Zelda64Recomp/Zelda64Recomp#plug-and-play).
This is an independent implementation.

## 1.0.0 — 2026-10-09

Initial stable release with the unified launcher, configurable controllers,
live map and inventory, and the recorded rendering and audio fixes. The v1.0.0
installer built the game locally from the player's ROM and required developer
tools. See the [v1.0.0 release](https://github.com/TK22-26/JetForceGemini-Recomp/releases/tag/v1.0.0).
