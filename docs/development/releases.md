# Windows release packaging

Version 1.1.0 publishes a prebuilt game bundle. The runtime is compiled locally
from the maintainer's supported ROM; generated sources, ROMs, extracted game
assets, saves, debug symbols, and local captures remain outside Git and release
assets. Players import their own ROM once. Release assets include only the
allowlisted compiled program, host libraries, documentation, and licenses.

1. Build and validate a clean native runtime, retaining its matching `.support`
   identity and private PDB. Retain the exact runtime source revision.
2. Update the launcher/CMake version and `CHANGELOG.md`. Commit the reviewed
   changes, then run `scripts/build_launcher.ps1 -Test` from the clean revision.
3. Run `scripts/package_beta.py --launcher <launcher> --runtime <runtime-dir>
   --vc-runtime <x64-VC143-redist-dir> --output <new-output-dir> --version 1.1.0`.
   Despite its historical filename, this packager accepts stable versions.
   Stable packaging requires the launcher build receipt to match clean source.
4. Validate the generated ZIP, inventory, and SHA-256 file. Keep the three
   artifacts together. The runtime identity may name an earlier reviewed
   commit when a release changes only the launcher.
5. Push the reviewed source to main, create the matching version tag, and
   upload the three artifacts to a draft GitHub release with changelog notes.
   Never overwrite a published release or tag.
6. The release workflow validates the tagged source through ROM-free CI, then
   downloads and verifies the draft bundle before publishing it. It fails
   closed if the playable runtime, checksums, source identity, or required
   libraries are missing. The old launcher-only package is not a v1.1 release.

The local build retains the game's compiled program but supplies no ROM or
extracted asset files. See `THIRD_PARTY_NOTICES.md` and the bundled notices for
the separate project, runtime, and redistributable license scopes.
