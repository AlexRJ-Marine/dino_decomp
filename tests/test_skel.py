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
    assert s.roots == (0,)
    assert s.bones[0].name == "Scene_Root"
    assert s.bones[1].name == "Bip01"
    assert s.bones[-1].name == "Dum_dosbassin"
    assert all(b.flag == 1 for b in s.bones)
    assert all(len(b.matrix) == 16 for b in s.bones)
    assert all(-1 <= b.parent < s.bone_count for b in s.bones)
    assert s.trailing == b""


def test_allo_hierarchy():
    if not ALLO.exists():
        pytest.skip("retail sample not present")
    s = read_skel(ALLO)
    pelvis = s.by_name("Bip01_Pelvis")
    spine = s.by_name("Bip01_Spine")
    assert pelvis.parent == s.by_name("Bip01").index
    assert spine.parent == pelvis.index
    assert s.bones[1].children == (2, 3)
    assert s.bones[3].children == (4, 79)


def test_allo_world_transforms():
    if not ALLO.exists():
        pytest.skip("retail sample not present")
    s = read_skel(ALLO)
    lo, hi = s.world_bounds()
    size = tuple(hi[i] - lo[i] for i in range(3))
    # A 10 m-long, 5 m-tall biped. The exact numbers catch matrix-order bugs:
    # transposing the 4x4 or composing parent @ local changes them drastically.
    assert size == pytest.approx((1.8145, 10.1893, 5.3174), abs=0.01)
    pos = s.world_positions()
    assert pos[0] == pytest.approx((0.0, 0.0, 0.0), abs=1e-5)
    assert pos[3][2] == pytest.approx(3.5887, abs=0.01)  # pelvis height
    assert pos[11][2] > pos[3][2]  # head above pelvis
    assert pos[11][1] < pos[-1][1]  # head in front of tail tip


def test_prop_skeletons():
    base = GAME / "Data" / "Graphics" / "buildings" / "coin" / "coinkute_lod1.skel"
    seats = GAME / "Data" / "Graphics" / "attractions" / "seats" / "seats3_lod1.skel"
    if not base.exists() or not seats.exists():
        pytest.skip("retail samples not present")
    a = read_skel(base)
    b = read_skel(seats)
    assert a.bone_count == 2
    assert [x.name for x in a.bones] == ["Scene_Root", "coinkute_lod1"]
    assert [x.parent for x in a.bones] == [-1, 0]
    assert b.bone_count == 28
    assert b.bones[2].name == "DummyD01"
    assert b.bones[-1].name == "DummyD26"
    assert b.bones[1].parent == 0
    assert all(x.parent == 0 for x in b.bones[1:])


def test_scan_all_skeletons():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 64
    assert not bad, bad[:5]
    assert sums["bones"] == 2_113
