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


def test_duration_table():
    """f[411..461] is the per-family record duration table.

    The first mover recon read f[111] and saw 1.0 s for every turn; the
    runtime probe measured 1.2/1.5/2.0 s, which matches this table.
    """
    if not BAD.exists():
        pytest.skip("retail sample not present")
    data = read_actions_bad(BAD)
    assert len(data.durations) == 5
    assert all(len(row) == 51 for row in data.durations)
    # File block rows (0..4). The Part 10 family 2/3 indices map to blocks
    # 3/2; the C++ `--actions-bad-info` prints the effective family rows.
    expected = [
        (1.5, 1.5, 2.0, 2.0, 2.0, 2.0, 2.0),  # HighQuadruped
        (2.0, 2.0, 1.5, 2.0, 2.0, 1.5, 2.0),  # LightQuadruped
        (2.0, 2.0, 2.0, 1.8, 2.0, 1.5, 2.0),  # HeavyBiped
        (1.2, 1.2, 1.5, 1.2, 2.0, 1.5, 2.0),  # LightBiped
        (2.0, 2.0, 1.5, 1.5, 1.5, 1.5, 2.0),  # LowQuadruped
    ]
    kinds = (3, 4, 5, 9, 0x19, 0x1c, 0x27)
    for family, row in enumerate(expected):
        assert tuple(data.durations[family][k] for k in kinds) == pytest.approx(
            row
        )


def test_rest_gauge_scale_table():
    """f[105..155] is the rest-gauge scale table used by FUN_00571540.

    Walk 0.5, run 2.0, sleep sentinel 0x1e -10 (gauge increases), and the
    sleep transition/wake records 0x1d/0x1c 0. All five blocks agree.
    """
    if not BAD.exists():
        pytest.skip("retail sample not present")
    data = read_actions_bad(BAD)
    assert len(data.scales) == 5
    for row in data.scales:
        assert len(row) == 51
        assert (row[0], row[1], row[2]) == pytest.approx((-2.0, 0.5, 2.0))
        assert row[0x1C] == 0.0
        assert row[0x1D] == 0.0
        assert row[0x1E] == pytest.approx(-10.0)
        assert row[0x27] == 0.0  # exploration turn is a duration, no scale


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
