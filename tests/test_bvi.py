import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from bvi import read_bvi, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
VISITORS = GAME / "Data" / "Design" / "visitors"


def test_fan_profile():
    if not (VISITORS / "FanMan.bvi").exists():
        pytest.skip("retail sample not present")
    profile = read_bvi(VISITORS / "FanMan.bvi")
    assert profile.personality_id == 7
    assert profile.personality == "Fan"
    assert profile.height == pytest.approx(1.7, abs=1e-4)
    assert profile.walk_speed == pytest.approx(2.0, abs=1e-4)
    assert profile.preferences == (0.7, 0.0, 0.4, -0.6, 0.4)
    assert len(profile.meshes) == 26
    assert profile.texture.endswith("fan.tga")


def test_scan_all_visitors():
    if not VISITORS.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(VISITORS)
    assert total == 24
    assert not bad, bad[:5]
    assert sums["meshes"] == 624


def test_all_mesh_paths_exist():
    if not VISITORS.exists():
        pytest.skip("retail game tree not present")
    missing = set()
    for path in VISITORS.glob("*.bvi"):
        for mesh in read_bvi(path).meshes:
            if not (GAME / mesh).exists():
                missing.add(mesh)
    assert not missing, sorted(missing)[:5]


def test_visitor_list_mapping():
    listing = VISITORS / "VisitorList.ini"
    if not listing.exists():
        pytest.skip("retail sample not present")
    lines = [line.strip() for line in listing.read_text(encoding="cp1252").splitlines()]
    assert lines[0].startswith("// Total number")
    assert lines[1] == "10"
    names = [line for line in lines if line.endswith(("Man", "Woman")) or "Man2" in line or "Woman2" in line or "Man3" in line or "Woman3" in line]
    assert len(names) == 24
    for name in names:
        assert (VISITORS / f"{name}.bvi").exists(), name


def test_personality_stats_unique():
    if not VISITORS.exists():
        pytest.skip("retail game tree not present")
    stats = {}
    for path in sorted(VISITORS.glob("*Man.bvi")):
        profile = read_bvi(path)
        stats[profile.personality_id] = (profile.personality, round(profile.walk_speed, 2))
    assert len(stats) == 10
    assert stats[4] == ("Old", 1.2)
    assert stats[0] == ("Surfer", 2.5)
