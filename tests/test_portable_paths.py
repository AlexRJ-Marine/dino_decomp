"""Tracked code must run on any contributor's machine.

Scripts, sources and build files may not hard-code a tool install path or a
user's home directory. Tools are found through `scripts/tools.ps1`
(`$env:DINO_TOOLS`, default `C:\\Tools`); the repo root is derived from the
script location. Notes, logs and docs are evidence and are not checked.
"""
from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CHECKED_SUFFIXES = {".ps1", ".py", ".sh", ".cpp", ".h", ".txt", ".json", ".jsonc", ".cmake"}
CHECKED_DIRS = ("scripts/", "tools/", "src/", "tests/")
CHECKED_FILES = {"CMakeLists.txt", "CMakePresets.json", "opencode.jsonc"}

# The one place the default tools root is defined.
ALLOWED = {
    "scripts/tools.ps1",
    # IDE convenience preset; scripts/build.ps1 is the supported entry point.
    "CMakePresets.json",
    # Agent permission default for the conventional tools root.
    "opencode.jsonc",
    "tests/test_portable_paths.py",
}

PATTERNS = [
    ("tool root", re.compile(r"[A-Za-z]:[\\/]+Tools[\\/]", re.I)),
    ("home dir", re.compile(r"[A-Za-z]:[\\/]+Users[\\/]+(?!Public\b)[^\\/\s\"'*$%{]+", re.I)),
    ("wsl home dir", re.compile(r"/mnt/[a-z]/Users/", re.I)),
]


def _tracked() -> list[str]:
    out = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True)
    files = []
    for name in out.stdout.splitlines():
        if name in ALLOWED or not (ROOT / name).is_file():
            continue
        if name in CHECKED_FILES or (name.startswith(CHECKED_DIRS) and Path(name).suffix in CHECKED_SUFFIXES):
            files.append(name)
    return files


def test_no_machine_specific_paths():
    files = _tracked()
    assert files, "no tracked scripts found"
    hits = []
    for name in files:
        text = (ROOT / name).read_text(encoding="utf-8", errors="replace")
        for lineno, line in enumerate(text.splitlines(), 1):
            for kind, rx in PATTERNS:
                if rx.search(line):
                    hits.append(f"{name}:{lineno}: {kind}: {line.strip()[:120]}")
    assert not hits, (
        "Machine-specific paths in tracked files; use scripts/tools.ps1 "
        "(DINO_TOOLS) or a path relative to the repo:\n" + "\n".join(hits)
    )
