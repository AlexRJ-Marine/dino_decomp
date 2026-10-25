import math
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from imp import ImpFormatError, read_imp, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
PALM1 = (GAME / "Data" / "Design" / "landscape" / "isl01"
         / "isl01_ImpPalm1.imp")


def test_palm1_placements():
    if not PALM1.exists():
        pytest.skip("retail sample not present")
    imp = read_imp(PALM1)
    assert imp.model == r"DATA\Design\Landscape\Objects\palm1.blf"
    assert len(imp.instances) == 196
    first = imp.instances[0]
    assert first.x == pytest.approx(582.0)
    assert first.y == pytest.approx(777.0)
    assert first.scale == pytest.approx(1.2198095321655273, rel=1e-7)
    assert first.angle == pytest.approx(3.6757218837738037, rel=1e-7)


def test_all_imp_instances_in_world():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GAME)
    assert total == 78
    assert not bad, bad[:5]
    assert sums["instances"] == 17_059
    assert sums["models"] == 17


def test_rotations_are_radians():
    if not GAME.exists():
        pytest.skip("retail game tree not present")
    angles = []
    for path in sorted(GAME.rglob("*.imp")):
        angles.extend(inst.angle for inst in read_imp(path).instances)
    assert angles
    assert min(angles) >= 0.0
    assert max(angles) <= 2.0 * math.pi + 1e-3


def test_rejects_truncated_imp(tmp_path):
    p = tmp_path / "bad.imp"
    p.write_bytes(b"\x08\x00\x00\x00a.blf\x00\x00\x02\x00\x00\x00")
    with pytest.raises(ImpFormatError):
        read_imp(p)
