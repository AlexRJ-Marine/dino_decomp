import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav import read_sav  # noqa: E402
from shw import (  # noqa: E402
    ShwFormatError,
    read_demo_shw,
    read_feature_names,
    read_shw,
    scan,
)

GAME = ROOT / "game" / "%MAINDIR%"
SHOWS = GAME / "Data" / "Design" / "Scenario" / "Show"
FEATURES = GAME / "Data" / "Design" / "attractions" / "features"
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
        packed = sum(value << (8 * index) for index, value in enumerate(show.cage_size))
        assert record.params[2] == packed, record.path
        assert record.params[5] == show.judge_kind, record.path


def test_rejects_unknown_section(tmp_path):
    p = tmp_path / "bad.shw"
    p.write_text("// Nope\n1\n", encoding="cp1252")
    with pytest.raises(ShwFormatError):
        read_shw(p)

def test_feature_indices_resolve_and_cages_match_teams():
    """Tuples are (FeatureList index, rotation, x, y)."""
    if not SHOWS.exists():
        pytest.skip("retail sample not present")
    names = read_feature_names(FEATURES)
    assert len(names) == 62
    assert names[0] == "CageSmall"
    assert names[3] == "CageHuge"
    assert names[4] == "JudgeRace"
    assert names[26] == "WaypointTeam1"
    assert names[42] == "ObstacleBeam5"
    cages = teams = 0
    for path in sorted(SHOWS.glob("*.shw")):
        show = read_shw(path)
        width, height = show.dimensions
        file_cages = 0
        judges = 0
        for feature, rotation, x, y in show.features:
            assert 0 <= feature < len(names)
            assert 0 <= rotation <= 3
            assert 0 <= x < width and 0 <= y < height
            if feature <= 3:
                file_cages += 1
            if 4 <= feature <= 10:
                judges += 1
        assert file_cages == len(show.teams), path.name
        assert judges >= 1, path.name
        cages += file_cages
        teams += len(show.teams)
    assert cages == 174
    assert teams == 174

def test_judge_kind_and_cage_flags():
    """Judge Kind 1..4 selects the judge feature; cage flags select cages."""
    if not SHOWS.exists():
        pytest.skip("retail sample not present")
    judge_feature = {1: 4, 2: 5, 3: 7, 4: 6}  # Race, Obstacle, Fight, Destruction
    for path in sorted(SHOWS.glob("*.shw")):
        show = read_shw(path)
        used_judges = {f[0] for f in show.features if 4 <= f[0] <= 10}
        assert judge_feature[show.judge_kind] in used_judges, path.name
        used_cages = {f[0] for f in show.features if f[0] <= 3}
        flags = {index for index, value in enumerate(show.cage_size) if value}
        assert used_cages == flags, path.name
        assert show.wall_type == 1, path.name

def test_edition_demo_shows():
    """Data/Edition/Show uses an older, smaller schema."""
    demo_dir = GAME / "Data" / "Edition" / "Show"
    if not demo_dir.exists():
        pytest.skip("retail sample not present")
    demos = {path.name: read_demo_shw(path) for path in demo_dir.glob("*.shw")}
    assert set(demos) == {"demo.shw", "demo8.shw", "demofinale.shw"}
    assert all(show.name == "NoName" for show in demos.values())
    assert {show.features.__len__() for show in demos.values()} == {20, 5, 8}
    for show in demos.values():
        assert (show.width, show.height) == (32, 72)
        assert show.judge in (2, 6)
        assert all(
            0 <= feature < 62 and 0 <= rotation <= 3
            and 0 <= x < show.width and 0 <= y < show.height
            for feature, rotation, x, y in show.features
        )

def test_footprints_tile_the_arena():
    """`.bfi` footprints + rotation + (x,y) anchor tile every arena exactly."""
    if not SHOWS.exists() or not FEATURES.exists():
        pytest.skip("retail sample not present")
    from bfi import scan as scan_bfi

    names = read_feature_names(FEATURES)
    footprints = {item.name: item for item in scan_bfi(FEATURES)}
    total_cells = 0
    total_features = 0
    overhang = []
    for path in sorted(SHOWS.glob("*.shw")):
        show = read_shw(path)
        width, height = show.dimensions
        grid: set[tuple[int, int]] = set()
        for feature, rotation, x, y in show.features:
            item = footprints[names[feature]]
            fw, fh = item.width, item.height
            if rotation % 2:
                fw, fh = fh, fw
            cells = {
                (x + dx, y + dy)
                for dx in range(fw)
                for dy in range(fh)
            }
            assert not (cells & grid), (path.name, feature, x, y)
            grid |= cells
            total_cells += len(cells)
            total_features += 1
            outside = [
                cell
                for cell in cells
                if not (0 <= cell[0] < width and 0 <= cell[1] < height)
            ]
            if outside:
                # Judges are allowed to overhang the arena edge by one cell.
                assert names[feature].startswith("Judge"), (path.name, feature)
                overhang.append((path.name, feature))
    assert total_features == 1679
    assert total_cells == 29500
    assert len(overhang) == 3
