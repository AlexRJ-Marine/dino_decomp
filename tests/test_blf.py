import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from blf import BlfFormatError, read_blf, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
PALM1 = GAME / "Data" / "Design" / "Landscape" / "Objects" / "palm1.blf"


def test_palm1_blf():
    if not PALM1.exists():
        pytest.skip("retail sample not present")
    blf = read_blf(PALM1)
    assert (blf.version, blf.lod_count, blf.unknown, blf.texture_count) == (2, 2, 0, 2)
    assert blf.lods == [
        r"DATA\graphics\landscape\trees\palmtrees\palm1_lod1.mesh",
        r"DATA\graphics\landscape\trees\palmtrees\palm1_lod2.mesh",
    ]
    assert blf.texture_dir == r"DATA\graphics\TEXTURES\buildings"


def test_all_blf_and_lod_targets_exist():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 24
    assert not bad, bad[:5]
    assert sums["lods"] == 48
    for path in sorted(GAME.rglob("*.blf")):
        blf = read_blf(path)
        for lod in blf.lods:
            assert (GAME / lod.replace("\\", "/")).exists(), lod


def test_rejects_bad_version(tmp_path):
    p = tmp_path / "bad.blf"
    p.write_bytes(b"\x03\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00")
    with pytest.raises(BlfFormatError):
        read_blf(p)
