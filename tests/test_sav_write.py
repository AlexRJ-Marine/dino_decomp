"""SAVE-0: the whole retail save re-serializes byte-identically.

Runs against the real retail saves in ``run/save`` when present; skips
otherwise.  Covers the header, the world block (fences with the repeated placement
rect, shows, animals, employees, buildings, visitors, dinosaurs, flags),
the show catalog, and the whole simulation section (roads, visitors,
park economy + schedule, dino store, visitor model, lab, AI, camera).
"""
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav_write import rewrite_sav_whole  # noqa: E402

SAVES = [
    ROOT / "run" / "save" / "slot001.sav",
    ROOT / "run" / "save" / "slot002.sav",
    ROOT / "run" / "save" / "auto" / "slot001.sav",
]


@pytest.mark.parametrize("path", SAVES, ids=lambda p: f"{p.parent.name}-{p.name}")
def test_whole_file_roundtrip(path):
    if not path.exists():
        pytest.skip("retail save not present")
    original, rebuilt, _offset = rewrite_sav_whole(path)
    assert len(rebuilt) == len(original), (
        f"length changed: {len(original)} -> {len(rebuilt)}")
    if rebuilt != original:
        diff = next(i for i, (a, b) in enumerate(zip(original, rebuilt))
                    if a != b)
        raise AssertionError(
            f"first diff at 0x{diff:x}: "
            f"{original[diff:diff + 8].hex()} != {rebuilt[diff:diff + 8].hex()}")
