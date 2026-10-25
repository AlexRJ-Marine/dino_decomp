import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from lnd import LndFormatError, read_lnd, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
ISL01 = GAME / "Data" / "Design" / "landscape" / "isl01" / "isl01.lnd"


def test_isl01():
    if not ISL01.exists():
        pytest.skip("retail sample not present")
    lnd = read_lnd(ISL01)
    assert lnd.name_key == "&Isl01_Name"
    assert lnd.basemap.endswith("basemap_01.tga")
    assert lnd.detailmap.endswith("detailmap_01.tga")
    assert lnd.heightmap.endswith("heightmap_01.raw")
    assert lnd.skybox.endswith("isl01")
    assert lnd.water.endswith("water.tga")
    assert lnd.occupation.endswith("isl01_occupation.raw")
    assert lnd.unknown_float == pytest.approx(16.0)
    assert (lnd.width, lnd.height, lnd.unknown_int) == (520, 376, 1)
    assert lnd.music == "musEquateur"
    assert len(lnd.vegetation) == 25
    assert lnd.vegetation[0].endswith("isl01_ImpPalm1.imp")
    assert lnd.vegetation[-1] == "None"


def test_scan_all_lnd():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 10
    assert not bad, bad[:5]
    assert sums["vegetation"] == 250


def test_rejects_truncated_lnd(tmp_path):
    p = tmp_path / "bad.lnd"
    p.write_bytes(b"\x08\x00\x00\x00abc")
    with pytest.raises(LndFormatError):
        read_lnd(p)
