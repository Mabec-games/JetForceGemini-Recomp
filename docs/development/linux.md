# Linux (x86-64) support

**Status: experimental.** The ROM-free libraries, tests, CI and the Linux host
layer are built and tested on Linux. The full game path (ROM generation, RT64,
live window, input and audio) has been ported but has only been
compile-checked and unit-tested for its host mechanisms so far; it has not yet
been played end to end on Linux. Please report what you find.

Linux uses the same recompiled game, the same fail-closed device model and the
same save format as Windows. Only the host layer differs.

## Requirements

- 64-bit x86 Linux with glibc 2.31 or newer (Ubuntu 22.04+, Debian 12+, Fedora 36+).
- A Vulkan 1.1 capable GPU and driver (Mesa or the vendor driver).
- An X11 session. Wayland sessions run through XWayland, which SDL selects
  automatically unless you set `SDL_VIDEODRIVER` yourself: RT64's Vulkan
  backend currently creates its surface from an Xlib window.
- The supported US `.z64` ROM (not provided).

```sh
sudo apt install build-essential gcc-multilib binutils-mips-linux-gnu \
  python3-venv python3-pip pkg-config cmake ninja-build git wget \
  libsdl2-dev libx11-dev libxrandr-dev libvulkan-dev libasound2-dev
```

## Build and play

```sh
git clone https://github.com/TK22-26/JetForceGemini-Recomp.git
cd JetForceGemini-Recomp
python3 scripts/build_from_rom.py --rom ~/games/my-copy.z64 --play
```

This runs the same recipe as Windows: it clones the pinned dependencies,
rebuilds the matching ELF from your ROM, generates CPU and audio code, builds
RT64 and compiles `jfg-native-boot`. Everything generated stays under
`$XDG_CACHE_HOME/jfg/b/<checkout-id>` (default `~/.cache/jfg/b`). Do not
upload that directory.

Saves live in `$XDG_DATA_HOME/JFGRecomp/profiles/default`
(default `~/.local/share/JFGRecomp/profiles/default`). To launch an existing
build directly:

```sh
jfg-native-boot --rom my-copy.z64 --save jfg.flash --controller-pak controller-1.pak --play
```

## Controls

- **Window:** F11 toggles fullscreen. The window keeps the console's 4:3
  presentation. Closing it quits the game.
- **Keyboard and mouse:** the same default keyboard map as Windows, including the
  experimental mouse-look controls when enabled in `experimental-controls.ini`.
- **Gamepads:** any SDL game controller (Xbox, PlayStation, Switch Pro, 8BitDo
  and others) in the first four slots, using the XInput button order.
- The launcher UI is Windows-only for now. To remap controls, edit
  `controller-N.ini` and `pc-input.ini` next to the profile and point
  `JFG_CONTROLLER_CONFIG` at the first of them; the file format is shared with
  Windows (keys are Windows virtual-key codes).

## How the host layer works

| Concern | Windows | Linux |
| --- | --- | --- |
| Guest address space | `CreateFileMapping` + `MapViewOfFileEx` | `memfd_create` + fixed `mmap` inside one `PROT_NONE` reservation |
| Device-register traps | guard pages + x86 trap flag in a vectored exception handler | `PROT_NONE` pages + trap flag from `SIGSEGV`/`SIGTRAP` handlers |
| Crash ledger | `SetUnhandledExceptionFilter` | `SIGSEGV`/`SIGBUS`/`SIGFPE`/`SIGILL`/`SIGABRT` handlers (module-relative address) |
| Process isolation | `CreateProcess` + inherited file mapping token | `fork`/`exec` + inherited `memfd` token, `PR_SET_PDEATHSIG` |
| Window, keyboard, mouse | Win32 window + raw input | SDL2 window; SDL scancodes translated to the same virtual-key codes |
| Gamepads | XInput | `SDL_GameController` |
| Renderer | RT64 on D3D12 | RT64 on Vulkan |

The two device-trap handlers share one platform-neutral implementation of the
device emulation; only fault capture and re-arming differ. `tests/mmio_trap_posix_tests.cpp`
checks the trap-flag mechanism without a ROM.

## Not yet supported on Linux

- The Windows launcher, live map and inventory companion windows.
- The Phase 7 RT64 evidence runner and the support-capture helper.
- Wayland-only systems without XWayland.
