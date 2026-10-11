# LAUNCHER-001: Players cannot discover new releases in the launcher

- Severity: moderate usability limitation; does not prevent gameplay.
- Status: verified in tests and against the live release API; awaiting packaged release.
- Reported and updated: 2026-10-11 UTC.
- Affected: published v1.1.0 Windows x64 launcher and earlier versions.
- Related: [launcher guide](../launcher.md), [dashboard](../../dashboard.md).

## Symptoms and reproduction

Open the released launcher and its Help menu. There is no release check or
update notification; players must visit GitHub to discover a newer version.
The repository About description also incorrectly says there is no playable
release, and status/roadmap prose predates the v1.1.0 prebuilt distribution.

## Evidence and cause

The native Help dropdown and helper dispatcher have no update action. GitHub
metadata inspected on 2026-10-11 identifies v1.1.0 as the latest published release,
while About still describes a ROM-free source snapshot with no playable release.
This is missing release-discovery functionality and stale documentation, not a
download or ROM-import failure.

## Candidate remedy and attempts

Add a background helper that reads the public GitHub latest stable release API,
compares numeric version components, and writes an atomic frontend result.
The native UI checks at startup, offers Help > Check for updates…, and retains
a home-screen notice for newer releases. A fixed project URL opens the download
page only on request. The helper caps response size and request duration and
reports offline, invalid-data and rate-limit failures without blocking gameplay.
No ROM, save, profile or credential is sent. Original game/runtime behavior is unchanged.

Rewrite the README/dashboard around the completed v1 game, explicitly distinguish
current source additions from the v1.1.0 ZIP, and correct About to describe ROM import.
Retain historical campaign test conditions and specific outstanding issues.

The first native configure attempt failed because the added UI test target was
joined to the preceding CMake command without a newline. Corrected the target
separator before retrying; no runtime behavior or existing dependency pins changed.
Private evidence: `release-updates-20261011/queue/0005.stderr`.

The C# compiler then rejected deserialization-only fields under warnings-as-errors
(CS0649). Changed the API data contract to auto-properties, retaining required
JSON members. Evidence: `release-updates-20261011/queue/0007.stdout`.

The first launcher regression run exposed an unhandled InvalidDataException for
malformed release tags: on this .NET Framework it is not covered by IOException.
Added its explicit error classification. The regression preserves this failure
case and checks that malformed data produces an error, never an up-to-date claim.
Evidence: `release-updates-20261011/queue/0009.stdout`.

The native UI fixture exposed CRLF response lines being ignored by the binary
reader (the C# helper emits LF). Normalize a trailing CR on each protocol line so
either Windows or Unix line endings work. Retained failing UI artifacts under
`release-updates-20261011/ui-validation`; fresh checks use `ui-validation-2`.
The first PowerShell wrapper did not wait for the GUI-subsystem test executable,
so its exit code did not certify the UI checks. Subsequent execution explicitly
waits for process exit and requires the test's result.json as well.

## Verification and remaining limits

- Full Windows launcher build passed, including 635 launcher checks (28 update
  checker cases), 15 setup checks, six graphics-adapter checks and viewport checks.
- The native RmlUi integration run passed 42 assertions: startup background
  checks, update notice, Help entry, manual retry, duplicate suppression, current
  version, offline/error/timeout states, escaped text, compact layout and no
  gameplay modal interruption. Tests use an offscreen window and isolated profile.
- A real HTTPS request and the real frontend helper both found stable v1.1.0.
  The helper ran without a ROM and wrote only its update result, leaving gameplay
  status untouched. No newer release is currently published, so the positive
  notification uses a synthetic v1.2.0 fixture through the production UI.
- Captured the actual startup UI before ROM import for the README. Corrected and
  read back the GitHub About description to confirm the public metadata update.
- Private evidence: `release-updates-20261011/queue/0012.stdout`,
  `ui-validation-2/result.json`, `live-api-profile/frontend-update.txt`, and
  `github-before.json` / `github-after.json` under that same private directory.

## Resolution and release boundary

The missing release-discovery path is implemented and verified in current source;
the stale About wording is corrected on GitHub. README/dashboard and related status
pages describe the playable v1.1.0 release separately from newer source features.
No commit/tag/release has been created for this work. Existing v1.1.0 downloads
cannot acquire a checker retroactively; users must install a package containing
the new launcher once. Downloads open in the browser and updates are installed
manually. No game runtime or save format changed.

Hardware/proxy combinations beyond this machine remain unqualified. Offline and
malformed-data handling are fixture-tested; an actual GitHub rate limit was not
deliberately triggered. The checker reports those HTTP failures but never retries
in a loop. Reopen if a supported network/profile hangs launch, reports a stale
release as newer, or fails to display a valid newer stable release.

## Lessons

Keep player-facing release availability separate from source/development features
and formal research gates. Compare versions numerically, never treat a failed
request as proof of being current, and keep network requests off the UI thread.
Use the [GitHub latest-release API](https://docs.github.com/en/rest/releases/releases#get-the-latest-release)
without accepting remote executable paths or download instructions.
