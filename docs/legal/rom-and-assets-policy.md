# ROM and game-data handling

Users supply their own supported ROM. The project does not provide ROMs or
game assets. Validate the selected file locally; do not scan unrelated folders
or transmit the file.

## Repository and submissions

Keep ROMs, extracted assets, generated game code, unrestricted memory captures,
save states, and game-data-bearing logs or dumps out of commits and GitHub
attachments, releases, caches, and CI output. Encoding or archiving a file does
not change its contents or classification. Preserve third-party notices and
do not copy material without an applicable permission/license basis.

Project-authored source, synthetic fixtures, schemas, supported-ROM metadata,
and reviewed hashes/aggregate results can be tracked. A recorded hash does
not authorize publishing its underlying input.

For bug reports, attach the launcher's exported support ZIP using the
[playtesting guide](../playtesting.md). Crash/hang dump export is planned;
the current launcher exports filtered text logs and does not produce dumps.
Any future dump format needs an explicit contents/size contract and verified
filtering before it becomes a supported GitHub attachment.

## Local inputs and output

Use ignored locations such as `roms/`, `generated/`, `src/generated/`,
`corpus/private/`, `captures/private/`, and `artifacts/private/`, or an explicit
location outside the checkout. Keep generated bodies and private evidence
separate from source. Avoid printing their contents or machine-specific paths.
Detailed corpus controls are in the [test-corpus policy](test-corpus-policy.md).

Packaging must exclude ROMs, extracted assets, private test bodies, and
unapproved generated output. Diagnostic collection is opt-in and filters
game data, saves, personal paths, and identifiers before export.

## Enforcement

Ignore rules, tracked-path checks, repository content scanning, and synthetic
canaries enforce the boundary. Scan source, packages, debug symbols, and output
before publication; automated scanning does not replace maintainer review.
Boundary changes and release approval remain owner decisions.

Report accidental disclosure through [security reporting](../../SECURITY.md).
Do not paste exposed bytes into an issue as evidence.

## Owner-requested prebuilt beta (2026-10-09)

The `beta/simplified-install` branch implements the owner-requested prebuilt-game
model: the compiled native game program may be included in the local review beta.
ROMs, extracted game assets, saves, generated source bodies and debug dumps remain
excluded. The owner will review and test the beta before any public release.
Generated binaries are still never committed to Git. This exception concerns
local beta packaging and does not change third-party license obligations or
authorize uploading a release. See `docs/prebuilt-beta.txt` for scope.
