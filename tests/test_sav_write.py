"""SAVE-0: the park block re-serializes byte-identically from the parsed model.

Runs against the real retail saves in ``run/save`` when present; skips
otherwise.  The park record is the first writer-ordered structure needed by a
retail `.sav` writer, so this guards the field order in
``tools/formats/sav_write.py``.
"""
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav_write import rewrite_sav  # noqa: E402

SAVES = [
    ROOT / "run" / "save" / "slot001.sav",
    ROOT / "run" / "save" / "slot002.sav",
    ROOT / "run" / "save" / "auto" / "slot001.sav",
]


@pytest.mark.parametrize("path", SAVES, ids=lambda p: f"{p.parent.name}-{p.name}")
def test_park_block_roundtrip(path):
    if not path.exists():
        pytest.skip("retail save not present")
    original, rebuilt, park_offset = rewrite_sav(path)
    assert len(rebuilt) == len(original), (
        f"length changed: {len(original)} -> {len(rebuilt)}")
    if rebuilt != original:
        diff = next(i for i, (a, b) in enumerate(zip(original, rebuilt))
                    if a != b)
        raise AssertionError(
            f"first diff at 0x{diff:x} (park at 0x{park_offset:x}): "
            f"{original[diff:diff + 8].hex()} != {rebuilt[diff:diff + 8].hex()}")
