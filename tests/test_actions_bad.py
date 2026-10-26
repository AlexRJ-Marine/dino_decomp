import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from actions_bad import read_actions_bad  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
BAD = GAME / "Data" / "Design" / "Actions.bad"


def test_actions_bad_blocks():
    if not BAD.exists():
        pytest.skip("retail sample not present")
    data = read_actions_bad(BAD)
    assert data.count == 5
    assert data.block_size == 462
    assert len(data.blocks) == 5
    assert all(len(block) == 462 for block in data.blocks)
    assert data.blocks[0][:3] == pytest.approx([1 / 410, 463.64999, 522.85101])
    assert data.blocks[0][-12:] == pytest.approx(
        [2.0, 1.5, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 1.2, 1.7]
    )
    assert data.blocks[4][-12:] == pytest.approx(
        [2.0, 1.2, 2.0, 1.5, 1.7, 1.7, 1.7, 1.7, 1.5, 1.6, 1.2, 1.5]
    )


def test_tail_speeds_are_animation_scales():
    """Every tail value is in the `.bai` field_18 scale range."""
    if not BAD.exists():
        pytest.skip("retail sample not present")
    data = read_actions_bad(BAD)
    for tail in data.tails:
        assert all(0.5 <= value <= 3.5 for value in tail), tail


def test_file_size():
    if not BAD.exists():
        pytest.skip("retail sample not present")
    assert BAD.stat().st_size == 4 + 5 * 462 * 4
