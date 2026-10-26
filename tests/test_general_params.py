import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from general_params import read_general_params  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
PARAMS = GAME / "Data" / "Design" / "generalparams" / "GeneralParams.bgd"


def test_header():
    if not PARAMS.exists():
        pytest.skip("retail sample not present")
    data = read_general_params(PARAMS)
    assert (data.field_110, data.field_114) == (3000.0, 1800.0)
    assert (data.field_118, data.field_11c) == (3.0, 1.5)
    assert data.sounds == ["", "rocket", "trumpet", "startBlast"]
    assert data.whistle == "whitle"


def test_tail():
    if not PARAMS.exists():
        pytest.skip("retail sample not present")
    data = read_general_params(PARAMS)
    assert (data.field_154, data.field_150) == (8.0, 120.0)
    assert data.field_1a0 == 40.0
    assert data.field_1a4 == 10
    assert data.field_1f8 == 60.0
    assert data.matrix_1a8 == [
        3.0, 3.0, 3.0, 3.0,
        3.0, 2.0, 2.0, 2.0,
        3.0, 2.0, 2.0, 2.0,
        3.0, 2.0, 2.0, 2.0,
    ]
    assert data.vector_1e8 == [6.0, 10.5, 14.0, 20.0]


def test_file_size():
    if not PARAMS.exists():
        pytest.skip("retail sample not present")
    assert PARAMS.stat().st_size == 170
