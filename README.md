# Mabecs Jet Force Gemini Recomp (Linux)

![Jet Force Gemini running natively on Linux](docs/images/mabec-gameplay.jpg)

An unofficial Linux edition of [Jet Force Gemini Recomp](https://github.com/TK22-26/JetForceGemini-Recomp)
by TK22-26 and the JFG Recomp contributors. Double-click the AppImage and choose
your Jet Force Gemini (USA) ROM; the game is built on your computer once, then a
**Play** button appears. Every later start opens the launcher on the Play screen.

![The launcher's Play screen](docs/images/mabec-launcher-play.jpg)

## Download

**[JFG-Linux-Ver3](https://github.com/Mabec-games/JetForceGemini-Recomp/releases/tag/JFG-Linux-Ver3) is the latest and recommended version.**

| | **Ver3** (latest) | Ver2 | Ver1 |
|---|---|---|---|
| Launcher with Play screen, music, full screen and mouse aim options | ✅ | – | – |
| Keyboard and mouse aiming | ✅ | – | – |
| AppImage size | 148 MB | 144 MB | 3.7 MB |
| Downloaded during the first build | about 6 MB | about 6 MB | about 4 GB |
| First build takes | about 15–30 minutes | about 15–30 minutes | about 15–30 minutes on a fast connection |

```
chmod +x JFG-Linux-mabec-Ver3.appimage
./JFG-Linux-mabec-Ver3.appimage
```

All versions use the same saves.

## The launcher

![The launcher building the game](docs/images/mabec-launcher.jpg)

- **First start:** choose your ROM. The launcher checks it, lists any missing
  build tools with the install command, and shows the build's progress.
- **Play screen:** appears when the build is done and on every later start.
- **Window ⟷ Full screen:** how the game starts. F11 still switches while playing.
- **Mouse aim (play with keyboard & mouse):** see below.
- **Play without inverted mouse look:** with mouse aim on, moving the mouse
  forward looks up. Off by default, which keeps the game's original inverted look.
- **Music:** *NOW PLAYING* in the top corner, with a mute button. It loops while
  the launcher is open and stops when the build finishes or you press Play.

All choices are remembered.

## Keyboard and mouse

Tick **Mouse aim (play with keyboard & mouse)** to play with keyboard and mouse.
This turns on the game's built-in, experimental keyboard-and-mouse support
(single-player) with its **Normal** layout. Player 1 then uses keyboard and
mouse; untick it to play with a gamepad, which keeps its normal controls.

**Tip: hold the right mouse button to aim.** While it's held, the mouse moves the
aiming reticle directly, which is much easier than aiming the original way.
Release it to go back to turning the camera.

| Key / mouse | N64 button | In the game |
|---|---|---|
| Mouse | – | Look around / turn the camera |
| **Right mouse button (hold)** | R | **Aim**: the mouse aims directly |
| **Left mouse button** | Z | **Fire** |
| W, A, S, D | Control stick | Move (and strafe while aiming) |
| Shift (hold) | – | Move at full speed |
| Space | C-Up | Jump |
| Mouse wheel up / down | L / B | Change weapon |
| Tab | Start | Pause menu |
| Enter, or the Z key | A | A button |
| J, K, L | C-Left, C-Down, C-Right | C buttons |
| Arrow keys | D-pad | D-pad |
| F11 | – | Switch window / full screen |

In menus, the arrow keys move, **Enter** confirms and **Backspace** goes back.
The mouse is only captured during gameplay; it is released in menus, cutscenes,
the pause menu and when the window loses focus.

## You need

- **Your own Jet Force Gemini (USA) `.z64` ROM.** The launcher checks it.
- **Common build tools.** If any are missing, the launcher lists them with the
  install command for your distribution.
- **Python 3.11–3.14** for the bundled Python packages (other versions download
  them).

## What's different from upstream

- The launcher described above, with Play screen, music, full screen and
  keyboard-and-mouse options.
- Linux: mouse aim works (the game's controller settings were never passed on
  Linux), and a crash at startup with keyboard and mouse is fixed.
- Fixes the Linux build failing on GCC (reported upstream as
  [issue #15](https://github.com/TK22-26/JetForceGemini-Recomp/issues/15)).
- Stalled downloads stop after 60 seconds instead of hanging, and downloads try
  IPv4 first, for routers that never answer IPv6 lookups.
- Includes [NoahVl's Linux AppImage improvements](https://github.com/NoahVl/JetForceGemini-Recomp/tree/claude/linux-appimage)
  (GTK 3 requirement check, first compiler errors on a failed build).
- Window title: "Mabecs Jet Force Gemini Recomp".

**Music:** Ver3's launcher plays *Jet Force Gemini - Main Theme (Remake by Bryan EL)*,
a fan remake by Bryan EL of the game's main theme; the original music is by Rare.

No ROM, game data or code generated from a ROM is included in this repository or
its releases. Two things are never bundled and are downloaded on first launch:
IDO, the original N64 compiler, which is proprietary, and the Jet Force Gemini
decompilation, which is reconstructed game code without a license. The bundled
open-source components' license files are in the AppImage under
`usr/share/jfg-recomp/offline/licenses`. Not affiliated with or endorsed by the
upstream project, Rare or Nintendo.

---

*The original project's README follows.*

# Jet Force Gemini Recomp

[![Build status](https://github.com/TK22-26/JetForceGemini-Recomp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/TK22-26/JetForceGemini-Recomp/actions/workflows/ci.yml)

A native PC recompilation of Jet Force Gemini for Windows x64.
Experimental Linux x86-64 support is documented in [Linux support](docs/development/linux.md).
**v1.1.0** includes a prebuilt game: extract the ZIP, import your supported
North American ROM once, and click **Play**. No developer tools or compilation
are required.

**Campaign status: fully playable from start to finish** through maintainer
playtesting. See the playtesting notes below for the conditions of that run.

[Get started](docs/getting-started.md) · [Changelog](CHANGELOG.md) ·
[Known issues](docs/known-issues.md) · [Report a problem](docs/playtesting.md) ·
[Contribute](CONTRIBUTING.md)

## Installation

Download **JFG-1.1.0-windows-x64.zip** from the
[v1.1.0 release](https://github.com/TK22-26/JetForceGemini-Recomp/releases/tag/v1.1.0),
extract the entire ZIP, and open `JFG-Launcher.exe`. Use **Select ROM…** to import
your supported US `.z64` ROM. Once import finishes, the original file can be
moved or deleted; the launcher uses its private local copy. Existing saves
stay in your game profile. **Game > Select ROM…** remains available afterward.

The installation approach is inspired by the **Zelda64Recomp team** and
[Zelda64Recomp's plug-and-play distribution model](https://github.com/Zelda64Recomp/Zelda64Recomp#plug-and-play).
Credit to that project for this setup experience; this is an independent
implementation, with no endorsement or affiliation implied.

**We do not ship game assets or ROMs.** The release includes the compiled game
program and runtime libraries. Textures, models, audio, and other game data are
read from your locally imported ROM, which is never uploaded. Original game
content remains the property of its respective owners.

Version 1.1.0 also restores a centered 4:3 game area so cutscene bars remain
correct inside a wide launcher window. See the [changelog](CHANGELOG.md).

## What works

- Full campaign completion, verified through maintainer playtesting.
- Optional live map and live inventory tracking in a custom companion interface.
- Native rendering and audio.
- Keyboard controls and configurable Xbox/XInput controllers.
- **Experimental PC controls (single-player):** mouse-and-keyboard look/aim and
  dual-stick controller support, including movement and strafing while aiming.
  Enable experimental controls for your selected device in the launcher's
  Controller Mapping window. See [PC controls status and remaining work](docs/planning/pc-controls-status.md).
- Verified saving and loading, with a launcher for setup and subsequent launches.

Original-console accuracy remains under validation.
Playtesting continues across more PCs, controllers, and gameplay.
See [recent gameplay fixes](docs/development/boot-gameplay-fix.md) and
[development progress](docs/dashboard.md) for details.

## Campaign playtest progress

**Campaign status: fully playable from start to finish**
through maintainer playtesting.

The verification run used optional infinite-health and enemy auto-kill testing
mods, plus a testing save with all Tribals unlocked. Saving and loading are
also verified.

## Live map and inventory

Optional companion tools provide a custom interface for exploring the game
and tracking your progress:

- **Live map:** see your position, room exits, items, NPCs, enemies, Tribals,
  and doors. Select an object to inspect its requirements and rewards; zoom,
  pan, or fit the map to the current room.
- **Live inventory:** track weapons, keys, quest items, and shared ship parts.
  View Juno, Vela, or Lupus, or follow the active character automatically.

![Custom live map showing the player, room geometry, exits, pickups, and an interaction inspector](docs/images/live-map.png)

*Live map in the custom companion interface.*

The inventory's game icons are extracted locally from each player's own ROM;
those artwork files are not bundled with the project or launcher. The interface
is custom project code. Game content remains the property of its respective
owners.

See the [launcher and live tools guide](docs/development/launcher.md) for details.

## Getting started

Start with the [getting started guide](docs/getting-started.md) for requirements,
downloads, and setup. You need a supported US big-endian `.z64` ROM;
the project does not provide ROMs or game assets.

The [released bundle](https://github.com/TK22-26/JetForceGemini-Recomp/releases)
is ready to play after ROM import. Developers can still build from source;
the guide explains both paths. Older v1.0 setup issues are documented in
[known issues](docs/known-issues.md#windows-10-setup).

## Help by playtesting

Play normally and report problems: crashes, freezes, progression blockers,
incorrect controls, graphics or audio, and save/load failures. Check
[known issues](docs/known-issues.md) first and attach the launcher's support
ZIP when available. Successful sessions do not need a report.

See [playtesting and reporting](docs/playtesting.md). If you want to contribute
code, [CONTRIBUTING.md](CONTRIBUTING.md) explains the pull request workflow.

## Future additions

The goal is a complete, faithful PC version of Jet Force Gemini with optional
modern enhancements. Planned work and remaining validation include:

- **Original local multiplayer and optional content:** broader coverage of the
  modes and activities beyond the main campaign.
- **Ultrawide support:** wider aspect ratios with correct gameplay presentation,
  menus, and HUD layout.
- **Higher frame-rate presentation:** smoother visuals while preserving the
  original gameplay timing.
- **Mod support:** documented interfaces and tools for community modifications.
- **A more polished PC experience:** smoother setup, broader controller coverage,
  and better crash and freeze diagnostics.

Compatibility and stability come first. These are development goals; release
dates have not been set.

## Build and development status

- [Build status and CI runs](https://github.com/TK22-26/JetForceGemini-Recomp/actions/workflows/ci.yml)
- [Development progress](docs/dashboard.md)
- [Build the playable game from your ROM](docs/development/rom-bootstrap.md)
- [Build and test the project source](docs/development/setup.md)
- [Documentation index](docs/README.md) and [project roadmap](docs/planning/JFG_RECOMP_MASTER_PLAN.md)

CI checks source, tools, and tests without a ROM. A passing build does not
establish that every level or gameplay feature works.

## License

Original project code and documentation use the [MIT License](LICENSE).
Dependencies retain their [third-party notices](THIRD_PARTY_NOTICES.md).
Jet Force Gemini game content and trademarks belong to their respective owners.
