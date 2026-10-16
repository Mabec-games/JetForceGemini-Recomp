#!/usr/bin/env python3
"""Create the offline dependency bundle for the Linux AppImage.

The bundle lets the first-run build work without the network, except for the
IDO compiler (proprietary, so it is never bundled; AppRun downloads it).

It contains only open-source material, each with its own license files:
- the four pinned dependency repositories with all submodules, as shallow git
  repositories at exactly the revisions in dependencies.lock.json;
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


def collect_licenses(repo: Path, destination: Path) -> int:
    """Copy the license files of repo and of every submodule (top level of each)."""
    count = 0
    roots = [repo] + [repo / line.split()[1] for line in
                      git("submodule", "status", "--recursive", cwd=repo).splitlines() if line.strip()]
    for root in roots:
        for item in sorted(root.iterdir()):
            if item.is_file() and item.name.lower().startswith(LICENSE_PREFIXES):
                target = destination / root.relative_to(repo.parent) / item.name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(item, target)
                count += 1
    return count


def collect_wheel_licenses(wheels: Path, destination: Path) -> int:
    import zipfile
    count = 0
    for wheel in sorted(wheels.glob("*.whl")):
        project = wheel.name.split("-")[0]
        with zipfile.ZipFile(wheel) as archive:
            for name in archive.namelist():
                parts = name.split("/")
                if ".dist-info" in parts[0] and parts[-1].lower().startswith(LICENSE_PREFIXES):
                    target = destination / project / parts[-1]
                    if not target.exists():
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(archive.read(name))
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

    for entry in lock["repositories"]:
        if entry["id"] not in NAMES:
            continue
        name = NAMES[entry["id"]]
        source = args.deps.resolve() / name
        if git("rev-parse", "HEAD", cwd=source) != entry["commit"]:
            raise RuntimeError(f"{source} is not at the pinned commit {entry['commit']}")
        print(f"Bundling {name} …", flush=True)
        shallow_repository(source, deps_root / name, entry["commit"], entry["url"])
        bundle_submodules(source, deps_root / name, deps_root, redirects)
        status = git("submodule", "status", "--recursive", cwd=deps_root / name)
        if any(line[:1] in "-+U" for line in status.splitlines()):
            raise RuntimeError(f"{name} has submodules that are missing or at the wrong commit:\n{status}")
        manifest["repositories"][name] = {"commit": entry["commit"], "url": entry["url"],
                                          "submodules": len(status.splitlines())}

    (output / "git-redirects.tsv").write_text(
        "".join(f"{url}\t{path}\n" for url, path in sorted(redirects.items())))
    requirements = output / "requirements.txt"
    shutil.copy2(deps_root / "Jet-Force-Gemini/requirements.txt", requirements)
    names = list(manifest["repositories"])
    licenses = output / "licenses"
    license_count = sum(collect_licenses(deps_root / name, licenses / "repositories") for name in names)
    print(f"Collected {license_count} license files from the repositories and submodules.", flush=True)
    for name in names:
        strip_worktree(deps_root / name)
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
    print(f"Collected {collect_wheel_licenses(wheels, licenses / 'python')} license files from the Python "
          "packages.", flush=True)
    (licenses / "README.txt").write_text(
        "License files of everything bundled in this offline edition. The repositories are stored as git\n"
        "data under ../deps (restored on first run); repositories/ holds each one's and each submodule's\n"
        "license files. python/ holds the Python packages' licenses (the wheels are in ../wheels).\n"
        "IDO is not bundled: it is proprietary and downloaded on first run.\n")
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
