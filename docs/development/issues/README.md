# Issue investigations and resolutions

Keep an issue record from the first reproducible report through its final verified
resolution. This is the persistent engineering history for bugs, regressions and
significant limitations. It complements [known issues](../../known-issues.md), the
[runtime fix log](../boot-gameplay-fix.md), and feature implementation notes; link
those sources rather than copying conflicting accounts.

## Recording policy

- Start or update a record while investigating, not only after success. Use the
  [template](template.md). Give the issue a stable descriptive filename and ID.
- Record symptoms and reproduction, affected version/profile/platform, severity,
  evidence provenance, observed facts, hypotheses, attempts and their results.
- Retain failed or inconclusive attempts when they explain a decision. Correct
  invalid evidence explicitly; do not silently promote a hypothesis to a cause.
- Record the actual final code/behavior change, why it fixes the demonstrated
  cause, regression checks, remaining gaps, and where it is available. A patch,
  successful build, or completed replay alone is not a verified resolution.
- Separate **reported**, **reproduced**, **candidate**, **verified in tests**,
  **awaiting live verification**, and **resolved/released**. State the specific
  scope of a pass. Record a commit/release only once one actually exists.
- Keep public records free of ROMs, generated game code, personal save data and
  identifying machine paths. Reference private artifact IDs and hashes as needed.
- Preserve useful findings from older feature/fix logs through links. Historical
  tests apply to their recorded build, not automatically to the current tree.

This practice applies to future work throughout the repository, including
launcher/UI, controls, runtime, rendering, audio, setup and tooling. Documentation
does not authorize reopening an exhausted investigation or reset guard budgets.

## Active and historical issue index

The historical rows below index existing evidence; they do not claim a new retest.
Detailed per-issue records are added as those issues receive further work.

| ID | Issue | Current recorded status | Investigation / resolution |
|---|---|---|---|
| LAUNCHER-001 | No launcher update discovery; stale release description | Verified in tests and live API; awaiting release | [Cause, implementation and checks](launcher-update-discovery.md) |
| PERF-001 | Goldwood drone-combat update slowdown | **Critical, open; partial candidate improvement** | [Evidence and attempts](goldwood-combat-slowdown.md) |
| CAMERA-003 | Modern area entry faces the exit | Verified in focused tests and owner live play; local preview | [Reproduction and work](modern-controller-state-transitions.md) |
| MOVE-003 | Dual-stick jump loses direction and air steering | Verified in focused tests and owner live play; local preview | [Reproduction and work](modern-controller-state-transitions.md) |
| CAMERA-004 | Focus loss lowers camera angle | Verified in focused tests and owner live play; local preview | [Reproduction and work](modern-controller-state-transitions.md) |
| CAMERA-001 | Small repeated forward camera steps | Verified in isolated replay; delivered preview, live verification pending | [Fixed modern camera boom](../../features/pc-controls.md#2026-10-11-goldwood-capture-camera-stepping-and-native-pause-save) |
| CAMERA-002 | Large scenery-driven camera pull-ins | Open; distinct from small boom-distance steps | [Capture findings](../../features/pc-controls.md#2026-10-11-goldwood-capture-camera-stepping-and-native-pause-save) |
| INPUT-001 | Main-menu clicks eaten; resize cursor remains in client | Corrected and regression-tested; physical focus verification remains | [Win32 activation/cursor cause and fix](../../features/pc-controls.md#live-menu-click-and-cursor-correction-2026-10-10) |
| INPUT-002 | Hover traverses intermediate save slots | Corrected with semantic native selection; tested | [Pointer audit](../../features/pc-controls.md#modern-shoulder-camera-and-mouse-audit-2026-10-10) |
| INPUT-003 | Fire drops mouse aim; aim entry snaps view | Corrected and tested; broader live coverage remains | [Capture ownership and camera handoff](../../features/pc-controls.md#modern-shoulder-camera-and-mouse-audit-2026-10-10) |
| INPUT-004 | Missing pause mouse targets / modern Save | Main page and Save tested and delivered; subpage targets remain open | [Native Save and pointer work](../../features/pc-controls.md#2026-10-11-goldwood-capture-camera-stepping-and-native-pause-save) |
| MOVE-001 | Upward view moves idle character | Reproduced; corrected and zero-displacement replay verified | [Camera translation cause](../../features/pc-controls.md#upward-camera-running-speed-and-scripted-input-follow-up-2026-10-10) |
| MOVE-002 | Modern movement 23.08% too fast | Native axis scaling/cap restored and tested | [Movement cap cause and fix](../../features/pc-controls.md#upward-camera-running-speed-and-scripted-input-follow-up-2026-10-10) |
| INPUT-005 | Mouse ownership during pickups/cutscenes | Shared lock fix tested; exact shotgun chest awaits live verification | [Input-lock audit](../../features/pc-controls.md#upward-camera-running-speed-and-scripted-input-follow-up-2026-10-10) |
| ANIM-001 | Backward movement uses wrong clip and leans | Native selector inputs corrected and tested | [Animation cause and fix](../../features/pc-controls.md#native-backward-and-directional-locomotion-2026-10-10) |
| ANIM-002 | Stationary turning has no planted-foot pivot | Open research; no confirmed reusable pivot or installed asset | [Feasibility findings](../../features/pc-controls.md#stationary-pivot-feasibility-review-2026-10-11) |

For earlier runtime defects, retain the [existing fix log](../boot-gameplay-fix.md).
Other currently reported limitations remain in [known issues](../../known-issues.md).
