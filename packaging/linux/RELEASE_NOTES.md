# Linux AppImage — experimental preview

**You must supply your own ROM.** This download contains no game data, no ROM and no code generated from a ROM. On first launch it asks for your **US `.z64` ROM** (32 MiB, big-endian), checks it, and **builds the game on your own machine**. Later launches start straight away.

## Status: untested end to end
The Linux port has been compile-checked and its host mechanisms unit-tested, but **the full game has not yet been played on Linux**. Expect problems and please report what you see. This is not an official release.

## Use
```sh
chmod +x JFG-*-x86_64.AppImage
./JFG-*-x86_64.AppImage            # asks for your ROM, builds (once), then plays
./JFG-*-x86_64.AppImage --rom ~/my-copy.z64
```
Options: `--rom FILE`, `--build-only`, `--rebuild`, `--version`, `--help`. If your system lacks FUSE, add `--appimage-extract-and-run`.

## Requirements
64-bit x86 Linux, a Vulkan-capable GPU/driver, an X11 session (XWayland is fine), network access for the first build, and these tools (the app prints the exact command if any are missing):
```sh
sudo apt install build-essential gcc-multilib binutils-mips-linux-gnu python3-venv python3-pip pkg-config cmake ninja-build git wget libsdl2-dev libx11-dev libxrandr-dev libvulkan-dev libasound2-dev
```
On Arch / CachyOS / Manjaro (multilib repo enabled; `ninja` is the package literally named `ninja`, `samurai` also works):

```
sudo pacman -S --needed base-devel git cmake ninja python python-pip pkgconf wget sdl2 libx11 libxrandr vulkan-icd-loader vulkan-headers alsa-lib lib32-glibc lib32-gcc-libs
paru -S mips-linux-gnu-binutils   # AUR; provides mips-linux-gnu-as/objcopy
```

The first build downloads the pinned dependencies and takes several minutes. Saves go in `~/.local/share/JFGRecomp`; the build cache (safe to delete) is in `~/.cache/jfg/appimage`.

Source revision: `@REVISION@`. More detail: `docs/development/linux.md`.

SHA-256: see `@SHA256FILE@` attached to this release.
