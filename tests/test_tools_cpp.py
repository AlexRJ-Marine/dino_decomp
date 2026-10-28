"""dino_tools: the headless CLI.

`dino_tools --selftest` runs the same 194 checks as `dino_port --selftest` but
links `dino_core` alone, so it proves the format/world libraries stay free of
SDL and OpenGL and gives a refactor a gate that needs no window, no display
and no retail data.

These tests are deliberately not marked slow: the binary runs in ~70 ms.
"""
from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pefile
import pytest

ROOT = Path(__file__).resolve().parents[1]
# dino_tools is built next to the port; DINO_EXE points at that directory.
PORT_EXE = Path(
    os.environ.get("DINO_EXE") or ROOT / "build" / "mingw" / "dino_port.exe"
)
TOOLS_EXE = PORT_EXE.with_name("dino_tools.exe")


def _run(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(TOOLS_EXE), *args], capture_output=True, text=True, timeout=120
    )


def test_tools_selftest_passes():
    if not TOOLS_EXE.exists():
        pytest.skip("dino_tools.exe not built")
    result = _run("--selftest")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "selftest: PASS" in result.stdout
    assert "FAIL" not in result.stdout


def test_tools_help_lists_selftest():
    if not TOOLS_EXE.exists():
        pytest.skip("dino_tools.exe not built")
    result = _run("--help")
    assert result.returncode == 0
    assert "--selftest" in result.stdout


def test_tools_rejects_unknown_option():
    if not TOOLS_EXE.exists():
        pytest.skip("dino_tools.exe not built")
    result = _run("--not-an-option")
    assert result.returncode == 2
    assert "unknown option" in result.stderr


def test_tools_does_not_link_sdl():
    """dino_core must stay SDL/GL-free; dino_tools is the proof."""
    if not TOOLS_EXE.exists():
        pytest.skip("dino_tools.exe not built")
    pe = pefile.PE(str(TOOLS_EXE))
    try:
        imports = {
            entry.dll.decode("ascii", "replace").lower()
            for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])
        }
    finally:
        pe.close()
    assert imports, "dino_tools.exe has no import directory; not a PE?"
    sdl = sorted(name for name in imports if "sdl" in name)
    assert not sdl, f"dino_tools must stay headless, imports SDL: {sdl}"
