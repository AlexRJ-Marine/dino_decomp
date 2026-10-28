"""The application layer is the port, not a viewer (2026-10-28 rename).

`src/app` (formerly `src/viewer`) builds `dino_port.exe`. The rename landed as
one commit and must not half-revert: no tracked build input may reference the
old identifier. Historical notes under `notes/` are evidence and are exempt;
`tools/viewer/` really is an asset viewer and keeps its name.
"""
from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Directories whose tracked files must use the new names. `notes/` is the
# evidence archive (append-only, never rewritten) and `third_party/` is
# vendored code.
CHECKED = ("src/", "scripts/", "tests/", "docs/", ".opencode/")
CHECKED_FILES = {
    "CMakeLists.txt",
    "CMakePresets.json",
    "README.md",
    "AGENTS.md",
    "TASKS.md",
    "pytest.ini",
    ".gitignore",
    "opencode.jsonc",
}

# Old identifier -> what replaced it.
BANNED = {
    "dino_viewer": "dino_port",
    "DINO_VIEWER_EXE": "DINO_EXE",
    "src/viewer": "src/app",
    "src\\viewer": "src/app",
    "test_viewer_cpp": "test_app_cpp",
    "sdl3-viewer": "app-shell",
}

# This file names the old identifiers by design; the config migration in
# `src/app/cli.cpp` reads the legacy file name once (`load_config`).
ALLOWED = {
    "tests/test_naming.py",
    "src/app/cli.cpp",
}

# Compatibility shims that intentionally mention the old name for one cycle:
# `.gitignore` keeps the legacy config file ignored, and the tests still read
# `DINO_VIEWER_EXE` when a contributor's shell sets it. The token is allowed,
# nothing else in those files is.
LEGACY_ALLOWED = {
    ".gitignore": {"dino_viewer"},
    "tests/test_app_cpp.py": {"DINO_VIEWER_EXE"},
    "tests/test_campaign_cpp.py": {"DINO_VIEWER_EXE"},
    "tests/test_video.py": {"DINO_VIEWER_EXE"},
}


def _tracked_files() -> list[str]:
    out = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout
    return out.splitlines()


def test_no_legacy_viewer_identifiers():
    hits: list[str] = []
    for name in _tracked_files():
        if name in ALLOWED:
            continue
        if not (name.startswith(CHECKED) or name in CHECKED_FILES):
            continue
        path = ROOT / name
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for old, new in BANNED.items():
            if old in text and old not in LEGACY_ALLOWED.get(name, set()):
                hits.append(f"{name}: {old!r} -> {new!r}")
    assert not hits, "old viewer identifiers survived the rename:\n" + "\n".join(hits)


def test_app_files_exist():
    assert (ROOT / "src" / "app" / "main.cpp").is_file()
    assert (ROOT / "tests" / "test_app_cpp.py").is_file()
    assert (ROOT / "docs" / "platform" / "app-shell.md").is_file()
    assert not (ROOT / "src" / "viewer").exists()
