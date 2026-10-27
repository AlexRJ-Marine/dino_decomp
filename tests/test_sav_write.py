"""SAVE-0: the simulation section re-serializes byte-identically.

Runs against the real retail saves in ``run/save`` when present; skips
otherwise.  Covers roads, the three empty lists, visitor records, the park block
(economy stream + schedule), the dino store (string NUL terminators),
the visitor model, lab records, AI lists, camera and tail.  The header and
world block are still copied raw (not yet proven).
"""
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav_write import rewrite_sav_full  # noqa: E402

SAVES = [
    ROOT / "run" / "save" / "slot001.sav",
    ROOT / "run" / "save" / "slot002.sav",
    ROOT / "run" / "save" / "auto" / "slot001.sav",
]


@pytest.mark.parametrize("path", SAVES, ids=lambda p: f"{p.parent.name}-{p.name}")
def test_simulation_roundtrip(path):
    if not path.exists():
        pytest.skip("retail save not present")
    original, rebuilt, state_offset = rewrite_sav_full(path)
    assert len(rebuilt) == len(original), (
        f"length changed: {len(original)} -> {len(rebuilt)}")
    if rebuilt != original:
        diff = next(i for i, (a, b) in enumerate(zip(original, rebuilt))
                    if a != b)
        raise AssertionError(
            f"first diff at 0x{diff:x} (simulation at 0x{state_offset:x}): "
            f"{original[diff:diff + 8].hex()} != {rebuilt[diff:diff + 8].hex()}")
