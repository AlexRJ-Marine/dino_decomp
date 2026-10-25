import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from skel import read_skel, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
ALLO = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "allo" / "allo.skel"


def test_allo_skeleton():
    if not ALLO.exists():
        pytest.skip("retail sample not present")
    s = read_skel(ALLO)
    assert s.bone_count == 80
    assert len(s.bones) == 80
    assert s.global_scale == pytest.approx(0.01, rel=1e-4)
    assert s.bones[0].name == "Scene_Root"
    assert s.bones[1].name == "Bip01"
    assert s.bones[-1].name == "Dum_dosbassin"
    assert all(b.flag == 1 for b in s.bones)
    assert all(len(b.values) == 19 for b in s.bones)
    assert s.trailing == b""


def test_prop_skeletons():
    base = GAME / "Data" / "Graphics" / "buildings" / "coin" / "coinkute_lod1.skel"
    seats = GAME / "Data" / "Graphics" / "attractions" / "seats" / "seats3_lod1.skel"
    if not base.exists() or not seats.exists():
        pytest.skip("retail samples not present")
    a = read_skel(base)
    b = read_skel(seats)
    assert a.bone_count == 2
    assert [x.name for x in a.bones] == ["Scene_Root", "coinkute_lod1"]
    assert b.bone_count == 28
    assert b.bones[2].name == "DummyD01"
    assert b.bones[-1].name == "DummyD26"


def test_scan_all_skeletons():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 64
    assert not bad, bad[:5]
    assert sums["bones"] == 2_113
