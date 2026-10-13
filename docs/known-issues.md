# Known issues

See the [issue investigation register](development/issues/README.md) for evidence,
attempted remedies and verified resolutions.

Check this page before opening a report. It lists confirmed open limitations;
[GitHub issues](https://github.com/TK22-26/JetForceGemini-Recomp/issues)
hold the ongoing discussion, and fixed problems are recorded in the
[runtime fix log](development/boot-gameplay-fix.md).

## Critical: Goldwood combat slowdown

The owner's recorded drone fight drops to approximately 15 game updates per
second in its busiest section. This is an actual update-cadence deficit, separate
from the fixed modern camera-distance stepping. Detailed logging alone does not
explain it. A native elapsed-time candidate is now available in the local Modern Controls
Preview after hidden replay qualification. Remaining slow updates and owner live
verification keep this issue critical and open; this is not a released fix. See the [timing investigation](features/pc-controls.md#2026-10-11-critical-goldwood-timing-investigation)
for measurements, experiments and the remaining acceptance work.

## Windows 10 setup

**v1.1.0 uses a prebuilt game and one-time ROM import.** The WSL/developer-tool
setup problems below concern the old v1.0-and-earlier local-build installer or
source builds; ordinary v1.1.0 installation no longer requires those steps.

[Issue 4](https://github.com/TK22-26/JetForceGemini-Recomp/issues/4) reports
setup failures on Windows 10 with Windows PowerShell 5.1:

- Disabled firmware virtualization can cause repeated restart instructions.
- Older WSL installations may reject the requested Ubuntu distribution.
- Missing Linux packages can stop setup at the package check.
- Long RT64 dependency paths could fail checkout, and a retry could skip
  incomplete submodules.

Since 0.4.0-preview.2, setup uses short build and dependency cache paths,
enables Git long paths for dependency downloads, retries submodule
initialization and preserves native stderr and exit codes under PowerShell 5.1.
These changes have automated coverage, but the reporter's full Windows 10
sequence still needs a retest. Firmware-virtualization detection, older WSL
distributions and prerequisite probes such as `Test-LinuxPackages` remain open,
so repeated restarts alone will not fix every failure. If you are comfortable
installing prerequisites yourself, use [manual build setup](development/rom-bootstrap.md).

## Release and source versions

v1.1.0 ships a prebuilt game from its recorded source revision. Current source
and development builds can contain newer features, including the controls
overhaul and launcher update checker, which are not yet in that published ZIP.
Include your launcher version and game build when reporting. The new checker's
availability and validation are recorded in [LAUNCHER-001](development/issues/launcher-update-discovery.md).
[Getting started](getting-started.md) explains release and source-build setup.

## Controllers

The v1.1.0 release supports keyboard and Xbox/XInput controllers. The current
controls overhaul adds compatible SDL gamepad discovery and Xbox/PlayStation
layouts. XInput rumble is available; native SDL/DualSense rumble, gyro and
adaptive triggers are not implemented. Original local multiplayer is working
per maintainer confirmation; coverage of every device/adapter combination remains open.

[Experimental PC controls](features/pc-controls.md) are opt-in and
single-player only. Steep camera angles near scenery can hide part of the
player, pointer targets on pause subpages remain incomplete, and coverage across
weapons, characters and modes is incomplete. The start/save-selection screens and
main pause page have mouse targets in the current modern-controls preview.

## Gameplay and rendering coverage

The maintainer completed the campaign with optional infinite-health and enemy
auto-kill mods and a save with all Tribals unlocked; saving and loading are
verified. The maintainer confirms the full game, original multiplayer and
optional content are playable. The retained assisted run is not an exhaustive
unassisted or hardware-compatibility test; original-console accuracy remains under validation. Player-shadow processing
is verified for Juno only, and Juno's water splash is untested. Report a
specific failure with its location and steps using the [playtesting guide](playtesting.md).

## Support reports

Native crash stacks and freeze capture require a game built by 0.4.0-preview.2
or later. Capture is best effort: forced termination, early startup failures,
corrupted stacks or access restrictions can leave partial logs. Reports are
text stack snapshots, not memory dumps, and nothing uploads automatically.

## Navigation mod

The opt-in [navigation mod](features/navigation-mod.md) can walk, jump, open
chests, collect NPC rewards and cross exits, but autonomous campaign
completion is not established. Lupus hover, moving platforms, crawling,
shooting and explosive gates and unsupported scripted traversal still need
manual play. Normal gameplay leaves the mod off.

## Background-music static

Goldwood music static in the experimental native-timing preview was traced to stale compressed sample data. The local preview now loads requested samples synchronously before consumption. Source-integrity checks and normal-Play integration pass, and the owner confirmed on 2026-10-13 that the audio is fixed in the delivered local preview. See [AUDIO-001: cause, remedy and verification limits](development/issues/intermittent-music-crackle.md). No complete audio fix is publicly released.

## Modern controls: transitions, jumping and focus

Candidate corrections for modern area-entry facing, jump momentum/air steering and focus-loss camera reset are verified in focused tests and delivered in the local v1.1 preview. Continuously active modern input reproduced a 180-degree Goldwood entry error; the corrected heading matches the original control path. The owner confirmed all three fixes in live play on 2026-10-13, and they are committed as 7b1ea4b; broader device and unusual-state coverage remains open. See [causes, regression evidence and live checks](development/issues/modern-controller-state-transitions.md).

## Modern camera: terrain bounce and pond-lip contact

The revised local Modern Controls preview corrects two reproduced causes: coarse
distance halving and a native height correction above clear finite pond geometry.
The reconstructed Goldwood area-47 pond route now holds full camera distance
through the prior 200-to-100 pull-ins; mouse, low-angle terrain, real obstruction
and original Controller Mapping regressions pass. Owner live verification and
broader scenery/moving-model coverage remain pending. Unknown geometry retains
the native response. This is not a complete swept-volume camera overhaul.

Use the Desktop JFG Camera Capture shortcut to record the next live reproduction.
See [CAMERA-005](development/issues/camera-collision-bounce.md) for causes, failed
and successful attempts, evidence, current package and limits. CAMERA-002 remains
open for broader scenery-driven pull-ins.
