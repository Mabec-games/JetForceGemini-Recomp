#!/usr/bin/env python3
"""Create the offline dependency bundle for the Linux AppImage.

The bundle lets the first-run build work with two small downloads (about 6 MB) instead of
about 4 GB. Two things are never bundled and AppRun downloads them: IDO, the
original N64 compiler (proprietary), and the Jet Force Gemini decompilation
(reconstructed Rare/Nintendo code without a license).

It contains only open-source material, each with its own license files:
- three pinned dependency repositories (RT64, N64Recomp, N64ModernRuntime)
  with all submodules, as shallow git repositories at exactly the revisions in
  dependencies.lock.json, plus the decompilation's three tool submodules;
- git-redirects.tsv, mapping each submodule's upstream URL to its bundled copy,
  so the build's own "git submodule update" calls stay offline;
- objdiff-cli (MIT/Apache-2.0), which the decompilation's tool cache expects;
- Python wheels for the decompilation's requirements.txt, for several CPython
  versions;
- ido.json: where AppRun downloads IDO from and its expected SHA-256;
- licenses/: readable copies of all bundled license files.

No ROM, game asset or ROM-derived file is involved.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from build_from_rom import NAMES  # noqa: E402

OBJDIFF_BASE = "https://github.com/encounter/objdiff"
IDO_URL = "https://github.com/decompals/ido-static-recomp/releases/download/{version}/ido-5.3-recomp-linux.tar.gz"
BUNDLE_REF = "refs/heads/jfg-bundle"
DEFAULT_PYTHONS = ("3.11", "3.12", "3.13", "3.14")


def git(*args: str, cwd: Path | None = None, config: dict[str, str] | None = None,
        allow_no_match: bool = False) -> str:
    options: list[str] = []
    for key, value in {"protocol.file.allow": "always", "uploadpack.allowAnySHA1InWant": "true",
                       "advice.detachedHead": "false", **(config or {})}.items():
        options += ["-c", f"{key}={value}"]
    command = ["git", *options, *(["-C", str(cwd)] if cwd else []), *args]
    result = subprocess.run(command, check=False, capture_output=True, text=True,
                            env=dict(os.environ, GIT_TERMINAL_PROMPT="0"))
    if allow_no_match and result.returncode == 1 and not result.stderr.strip():
        return ""  # "git config --get-regexp" found nothing.
    if result.returncode:
        raise RuntimeError(f"{' '.join(command)} failed:\n{result.stderr.strip()}")
    return result.stdout.strip()


def submodules(repo: Path) -> list[tuple[str, str]]:
    """(name, path) for each submodule declared in repo's .gitmodules."""
    if not (repo / ".gitmodules").is_file():
        return []
    raw = git("config", "-f", ".gitmodules", "--get-regexp", r"^submodule\..*\.path$", cwd=repo,
              allow_no_match=True)
    entries = []
    for line in raw.splitlines():
        key, path = line.split(" ", 1)
        entries.append((key[len("submodule."):-len(".path")], path))
    return entries


def shallow_repository(source: Path, destination: Path, commit: str, origin: str) -> None:
    destination.mkdir(parents=True)
    git("init", "-q", cwd=destination)
    git("fetch", "-q", "--depth", "1", source.resolve().as_uri(), commit, cwd=destination)
    git("checkout", "-q", "--detach", "FETCH_HEAD", cwd=destination)
    git("update-ref", BUNDLE_REF, "HEAD", cwd=destination)
    git("remote", "add", "origin", origin, cwd=destination)


def bundle_submodules(source: Path, destination: Path, deps_root: Path, redirects: dict[str, str]) -> None:
    """Recreate source's submodules (recursively) in destination as shallow copies."""
    for name, path in submodules(destination):
        entry = git("ls-tree", "HEAD", "--", path, cwd=destination).split()
        if len(entry) < 3 or entry[1] != "commit":
            continue  # Declared in .gitmodules but not part of this revision (git skips it too).
        commit = entry[2]
        url = git("config", f"submodule.{name}.url", cwd=source)  # Already resolved by "init".
        git("config", f"submodule.{name}.url", url, cwd=destination)
        git("config", f"submodule.{name}.active", "true", cwd=destination)
        git("submodule", "update", "--init", "--depth", "1", "--", path, cwd=destination,
            config={f"url.{(source / path).resolve().as_uri()}.insteadOf": url})
        sub = destination / path
        if git("rev-parse", "HEAD", cwd=sub) != commit:
            raise RuntimeError(f"{sub} is not at the recorded commit {commit}")
        git("update-ref", BUNDLE_REF, "HEAD", cwd=sub)
        gitdir = Path(git("rev-parse", "--absolute-git-dir", cwd=sub))
        redirects.setdefault(url, gitdir.relative_to(deps_root).as_posix())
        bundle_submodules(source / path, sub, deps_root, redirects)


LICENSE_PREFIXES = ("license", "licence", "copying", "notice", "copyright", "unlicense", "authors")
LICENSE_DEPTH = 3  # Some bundles (e.g. RT64's Windows libraries) keep licenses in subfolders.
# Repositories that are downloaded on first run instead of bundled (see the module docstring).
DOWNLOADED = {"Jet-Force-Gemini"}
EXTRA_DIR = ".extra"
# Prebuilt binaries that ship without their license text, and where that text lives.
EXTRA_LICENSES = {
    "rt64/src/contrib/dxc": ("https://raw.githubusercontent.com/microsoft/DirectXShaderCompiler/main/LICENSE.TXT",
                             "Prebuilt DirectX Shader Compiler (microsoft/DirectXShaderCompiler), which ships "
                             "without its license text; LICENSE.TXT is that project's license."),
}


# Python packages whose wheels ship no license file (both MIT, per their repositories).
PYTHON_LICENSE_SOURCES = {
    "crunch64": "https://raw.githubusercontent.com/decompals/crunch64/main/LICENSE",
    "pylibyaml": "https://raw.githubusercontent.com/philsphicas/pylibyaml/main/LICENSE",
}


def collect_licenses(repo: Path, destination: Path) -> int:
    """Copy license files of repo and of each submodule, up to LICENSE_DEPTH folders below each."""
    roots = [repo]
    if (repo / ".git").exists():
        roots += [repo / line.split()[1] for line in
                  git("submodule", "status", "--recursive", cwd=repo).splitlines() if line.strip()]
    copied: set[Path] = set()
    for root in roots:
        for folder, subfolders, files in os.walk(root):
            depth = len(Path(folder).relative_to(root).parts)
            subfolders[:] = [] if depth >= LICENSE_DEPTH else [name for name in subfolders if name != ".git"]
            for file_name in sorted(files):
                source = Path(folder, file_name)
                if file_name.lower().startswith(LICENSE_PREFIXES) and source.is_file() and source not in copied:
                    target = destination / source.relative_to(repo)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, target)
                    copied.add(source)
    return len(copied)


def collect_wheel_licenses(wheels: Path, destination: Path) -> int:
    """License files from each wheel; for wheels without any, the license its metadata declares."""
    import zipfile
    from email.parser import Parser
    count = 0
    for wheel in sorted(wheels.glob("*.whl")):
        project = wheel.name.split("-")[0].lower()
        with zipfile.ZipFile(wheel) as archive:
            found = False
            for name in archive.namelist():
                parts = name.split("/")
                if ".dist-info" in parts[0] and parts[-1].lower().startswith(LICENSE_PREFIXES):
                    found = True
                    target = destination / project / parts[-1]
                    if not target.exists():
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(archive.read(name))
                        count += 1
            target = destination / project / "LICENSE-FROM-METADATA.txt"
            if not found and not (destination / project).exists():
                metadata_name = next(n for n in archive.namelist() if n.endswith(".dist-info/METADATA"))
                metadata = Parser().parsestr(archive.read(metadata_name).decode("utf-8", "replace"))
                declared = [f"License: {metadata['License']}"] if metadata["License"] else []
                declared += [f"License-Expression: {metadata['License-Expression']}"] \
                    if metadata["License-Expression"] else []
                declared += [c for c in metadata.get_all("Classifier") or [] if c.startswith("License ::")]
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(f"{metadata['Name']} {metadata['Version']} ships no license file. Its "
                                  f"package metadata declares:\n" + "\n".join(declared or ["(nothing)"]) +
                                  f"\nHome page: {metadata['Home-page'] or metadata['Project-URL'] or '-'}\n")
                count += 1
    return count


def strip_worktree(repo: Path) -> None:
    """Keep only the git storage; restore_worktrees() checks the files out again."""
    for child in repo.iterdir():
        if child.name != ".git":
            shutil.rmtree(child) if child.is_dir() and not child.is_symlink() else child.unlink()


def redirect_config(deps_root: Path, redirects: dict[str, str]) -> dict[str, str]:
    return {f"url.{(deps_root / path).as_uri()}.insteadOf": url for url, path in redirects.items()}


def restore_worktrees(deps_root: Path, names: list[str], redirects: dict[str, str]) -> None:
    """What AppRun does on first run (kept in sync with packaging/linux/AppRun)."""
    config = redirect_config(deps_root, redirects)
    for name in names:
        git("reset", "-q", "--hard", cwd=deps_root / name)
        git("submodule", "update", "--init", "--recursive", "--force", cwd=deps_root / name, config=config)


def verify(deps_root: Path, manifest: dict) -> None:
    for name, info in manifest["repositories"].items():
        if info.get("downloaded"):
            continue
        if git("rev-parse", "HEAD", cwd=deps_root / name) != info["commit"]:
            raise RuntimeError(f"{name} restored at the wrong commit")
        status = git("submodule", "status", "--recursive", cwd=deps_root / name)
        if any(line[:1] in "-+U" for line in status.splitlines()) or len(status.splitlines()) != info["submodules"]:
            raise RuntimeError(f"{name} submodules did not restore cleanly:\n{status}")
        if git("status", "--porcelain", "--ignore-submodules=none", cwd=deps_root / name):
            raise RuntimeError(f"{name} restored with modified files")


def download(url: str, destination: Path) -> str:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=120) as response, destination.open("wb") as out:
        shutil.copyfileobj(response, out)
    return hashlib.sha256(destination.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--deps", type=Path, required=True,
                        help="Existing dependency cache with the pinned repositories and their submodules "
                             "(the 'd' folder of a completed build)")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--python", action="append", dest="pythons",
                        help=f"CPython version to fetch wheels for (default: {' '.join(DEFAULT_PYTHONS)})")
    args = parser.parse_args()
    lock = json.loads((ROOT / "dependencies.lock.json").read_text())
    output = args.output.resolve()
    if output.exists():
        shutil.rmtree(output)
    deps_root = output / "deps"
    redirects: dict[str, str] = {}
    manifest: dict = {"repositories": {}}

    licenses = output / "licenses"
    license_count = 0
    for entry in lock["repositories"]:
        if entry["id"] not in NAMES:
            continue
        name = NAMES[entry["id"]]
        source = args.deps.resolve() / name
        if git("rev-parse", "HEAD", cwd=source) != entry["commit"]:
            raise RuntimeError(f"{source} is not at the pinned commit {entry['commit']}")
        if name in DOWNLOADED:
            # Reconstructed game code without a license: never bundled, AppRun downloads it. Its
            # open-source tool submodules are bundled as standalone repositories the build is
            # redirected to.
            print(f"Bundling only the tool submodules of {name} (the repository itself is downloaded) …",
                  flush=True)
            extra = deps_root / EXTRA_DIR / name
            tools = []
            for sub_name, path in submodules(source):
                entry_line = git("ls-tree", "HEAD", "--", path, cwd=source).split()
                if len(entry_line) < 3 or entry_line[1] != "commit":
                    continue
                url = git("config", f"submodule.{sub_name}.url", cwd=source)
                if submodules(source / path):
                    raise RuntimeError(f"{name}/{path} has nested submodules; bundling them is not supported")
                shallow_repository(source / path, extra / path, entry_line[2], url)
                redirects.setdefault(url, (extra / path).relative_to(deps_root).as_posix())
                license_count += collect_licenses(extra / path, licenses / "repositories" / name / path)
                strip_worktree(extra / path)
                tools.append(path)
            requirements_text = git("show", "HEAD:requirements.txt", cwd=source)
            manifest["repositories"][name] = {"commit": entry["commit"], "url": entry["url"],
                                              "downloaded": True, "bundled_submodules": tools}
            continue
        print(f"Bundling {name} …", flush=True)
        shallow_repository(source, deps_root / name, entry["commit"], entry["url"])
        bundle_submodules(source, deps_root / name, deps_root, redirects)
        status = git("submodule", "status", "--recursive", cwd=deps_root / name)
        if any(line[:1] in "-+U" for line in status.splitlines()):
            raise RuntimeError(f"{name} has submodules that are missing or at the wrong commit:\n{status}")
        manifest["repositories"][name] = {"commit": entry["commit"], "url": entry["url"],
                                          "submodules": len(status.splitlines())}
        license_count += collect_licenses(deps_root / name, licenses / "repositories" / name)
        strip_worktree(deps_root / name)
    print(f"Collected {license_count} license files from the repositories and submodules.", flush=True)

    (output / "git-redirects.tsv").write_text(
        "".join(f"{url}\t{path}\n" for url, path in sorted(redirects.items())))
    requirements = output / "requirements.txt"
    requirements.write_text(requirements_text + "\n")
    names = [name for name, info in manifest["repositories"].items() if not info.get("downloaded")]
    with tempfile.TemporaryDirectory() as scratch:
        print("Self-test: restoring the files from the bundled git storage …", flush=True)
        trial = Path(scratch) / "deps"
        shutil.copytree(deps_root, trial, symlinks=True)
        restore_worktrees(trial, names, redirects)
        verify(trial, manifest)

    tools = lock["release_tools"]
    objdiff = tools["objdiff"]
    print(f"Fetching objdiff-cli {objdiff} …", flush=True)
    objdiff_dir = output / "objdiff"
    download(f"{OBJDIFF_BASE}/releases/download/{objdiff}/objdiff-cli-linux-x86_64", objdiff_dir / "objdiff-cli")
    (objdiff_dir / "objdiff-cli").chmod(0o755)
    for license_name in ("LICENSE-MIT", "LICENSE-APACHE"):
        download(f"https://raw.githubusercontent.com/encounter/objdiff/{objdiff}/{license_name}",
                 objdiff_dir / license_name)

    ido = tools["ido-static-recomp"]
    with tempfile.TemporaryDirectory() as scratch:
        print(f"Recording the checksum of IDO {ido} (downloaded on first run, not bundled) …", flush=True)
        url = IDO_URL.format(version=ido)
        digest = download(url, Path(scratch) / "ido.tar.gz")
    (output / "ido.json").write_text(json.dumps({"version": ido, "url": url, "sha256": digest}, indent=2) + "\n")

    wheels = output / "wheels"
    # Some requirements are published only as source (e.g. intervaltree). Build the whole set once for
    # this Python; pure-Python results ("py3-none-any") then also satisfy the other versions below.
    print("Building wheels for packages published only as source …", flush=True)
    subprocess.run([sys.executable, "-m", "pip", "wheel", "--quiet", "--disable-pip-version-check",
                    "-r", str(requirements), "-w", str(wheels)], check=True)
    for version in args.pythons or DEFAULT_PYTHONS:
        print(f"Fetching wheels for CPython {version} …", flush=True)
        subprocess.run([sys.executable, "-m", "pip", "download", "--quiet", "--disable-pip-version-check",
                        "--find-links", str(wheels),
                        "--only-binary=:all:", "--implementation", "cp", "--python-version", version,
                        "--platform", "manylinux_2_28_x86_64", "--platform", "manylinux_2_17_x86_64",
                        "--platform", "manylinux2014_x86_64", "--platform", "linux_x86_64",
                        "-r", str(requirements), "-d", str(wheels)], check=True)
    shutil.copytree(objdiff_dir, licenses / "objdiff", ignore=shutil.ignore_patterns("objdiff-cli"))
    for path, (url, note) in EXTRA_LICENSES.items():
        download(url, licenses / "repositories" / path / Path(url).name)
        (licenses / "repositories" / path / "NOTE.txt").write_text(note + "\n")
    for project, url in PYTHON_LICENSE_SOURCES.items():
        download(url, licenses / "python" / project / "LICENSE")
    print(f"Collected {collect_wheel_licenses(wheels, licenses / 'python')} license files from the Python "
          "packages.", flush=True)
    (licenses / "README.txt").write_text(
        "License files of everything bundled in this offline edition. The repositories are stored as git\n"
        "data under ../deps (restored on first run); repositories/ holds each one's and each submodule's\n"
        "license files. python/ holds the Python packages' licenses (the wheels are in ../wheels).\n"
        "Not bundled, downloaded on first run: IDO (proprietary) and the Jet Force Gemini decompilation\n"
        "(reconstructed game code without a license; only its open-source tool submodules are bundled).\n")
    manifest["wheels"] = sorted(path.name for path in wheels.iterdir())
    manifest["pythons"] = list(args.pythons or DEFAULT_PYTHONS)
    manifest["objdiff"] = objdiff
    manifest["ido"] = ido
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Offline bundle ready: " + str(output))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
