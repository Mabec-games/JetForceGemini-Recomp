"""Short, checkout-specific build storage; no drive mapping or registry edits."""
from __future__ import annotations
import hashlib
import os
from pathlib import Path

MAX_BINARY_ROOT = 80


def check_binary_path(path: Path) -> Path:
    path = path.resolve()
    if len(str(path)) > MAX_BINARY_ROOT:
        raise ValueError('Windows build output path is too long for MSBuild. '
                         'Use --build-root with a short writable local folder, for example D:/JFG-builds. '
                         'Existing build files were preserved.')
    if str(path).startswith('\\\\'):
        raise ValueError('Windows builds require a local drive, not a network share.')
    return path


def windows_cache(source: Path, override: Path | None = None) -> Path:
    if override is None:
        local = os.environ.get('LOCALAPPDATA')
        if not local:
            raise ValueError('LOCALAPPDATA is unavailable; specify --build-root with a writable local folder.')
        base = Path(local) / 'JFG/b'
    else:
        base = override
    key = hashlib.sha256(os.path.normcase(str(source.resolve())).encode('utf-8')).hexdigest()[:12]
    cache = base.resolve() / key
    # Leave room for a run ID, native build directory and upstream intermediates.
    check_binary_path(cache / 'w' / ('0' * 12) / 'n')
    return cache


def linux_cache(source: Path, override: Path | None = None) -> Path:
    """Per-checkout build cache under XDG_CACHE_HOME (or ~/.cache)."""
    if override is None:
        root = os.environ.get('XDG_CACHE_HOME')
        base = (Path(root) if root else Path.home() / '.cache') / 'jfg/b'
    else:
        base = override
    key = hashlib.sha256(str(source.resolve()).encode('utf-8')).hexdigest()[:12]
    return base.resolve() / key


def host_cache(source: Path, override: Path | None = None) -> Path:
    return windows_cache(source, override) if os.name == 'nt' else linux_cache(source, override)
