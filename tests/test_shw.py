import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav import read_sav  # noqa: E402
from shw import ShwFormatError, read_shw, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
SHOWS = GAME / "Data" / "Design" / "Scenario" / "Show"
SAMPLE = SHOWS / "Cpg1_Race_1.shw"
SAVE = ROOT / "run" / "save" / "slot001.sav"


def test_sample_show():
    if not SAMPLE.exists():
        pytest.skip("retail sample not present")
    show = read_shw(SAMPLE)
    assert show.name == "&cpg1_race_1"
    assert show.cost == 1800
    assert show.judge_kind == 1
    assert show.cage_size == (1, 0, 0, 0)
    assert show.dimensions == (19, 39)
    assert show.wall_type == 1
    assert show.feature_count == 14
    assert show.features[0] == (4, 0, 0, 18)
    assert show.teams == [(0, 1), (1, 0)]


def test_scan_all_shows():
    if not SHOWS.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(SHOWS)
    assert total == 43
    assert not bad, bad[:5]
    assert sums["features"] == 1_679
    assert sums["teams"] == 174


def test_save_catalog_matches_shows():
    if not SAVE.exists() or not SHOWS.exists():
        pytest.skip("retail save/game tree not present")
    save = read_sav(SAVE)
    assert len(save.shows) == 39
    for record in save.shows:
        path = SHOWS / Path(record.path).name
        assert path.exists(), record.path
        show = read_shw(path)
        assert record.params[0] == show.cost
        assert record.params[3] == show.dimensions[0]
        assert record.params[4] == show.dimensions[1]
        assert record.params[1] == 4


def test_rejects_unknown_section(tmp_path):
    p = tmp_path / "bad.shw"
    p.write_text("// Nope\n1\n", encoding="cp1252")
    with pytest.raises(ShwFormatError):
        read_shw(p)
