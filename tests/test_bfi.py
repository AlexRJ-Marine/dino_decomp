import sys
from collections import Counter
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from bfi import CLASS_TAGS, read_bfi, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
FEATURES = GAME / "Data" / "Design" / "attractions" / "features"
LIST = FEATURES / "FeatureList.ini"


def test_scan_all_features():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    items = scan(FEATURES)
    assert len(items) == 62
    assert len({item.name for item in items}) == 62
    for item in items:
        assert item.width >= 1 and item.height >= 1
        assert item.vertical >= 0
        assert item.field_11 >= 100
        assert item.field_15 >= 1
        assert item.field_19 >= 10
        assert item.strings


def test_class_tags():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    tags = Counter(item.class_tag for item in scan(FEATURES))
    assert tags == {
        "Tile": 26,
        "": 10,
        "Judge": 6,
        "AnimalBait": 5,
        "StaticBait": 5,
        "Cage": 4,
        "Waypoint": 4,
        "LunchBox": 1,
        "Teleport": 1,
    }
    assert set(tags) - {""} == set(CLASS_TAGS)


def test_feature_list_matches_files():
    if not LIST.exists():
        pytest.skip("retail sample not present")
    lines = [
        line.strip()
        for line in LIST.read_text(encoding="cp1252").splitlines()
        if line.strip()
    ]
    assert lines[0] == "62"
    names = lines[1:]
    assert len(names) == 62
    assert sorted(names) == sorted(item.name for item in scan(FEATURES))


def test_assets_exist():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    meshes = textures = 0
    for item in scan(FEATURES):
        for relative in (*item.meshes, *item.textures):
            path = GAME / relative.replace("\\", "/")
            assert path.exists(), (item.name, relative)
        meshes += len(item.meshes)
        textures += len(item.textures)
    assert meshes == 57
    assert textures == 49


def test_switch_keys_and_cages():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    items = {item.name: item for item in scan(FEATURES)}
    with_keys = [name for name, item in items.items() if item.switch_key]
    assert len(with_keys) == 60
    assert items["JudgeCapture"].switch_key == ""
    assert items["JudgeTeam"].switch_key == ""
    assert items["CageMedium"].width == 8
    assert items["CageHigh"].width == 12
    assert items["CageHuge"].width == 16
    assert items["CageHuge"].vertical == 8.0
    assert items["ObstacleBusLondon"].width == 4
    assert items["ObstacleBusLondon"].height == 8
    assert items["ObstacleCar"].width == 2
    assert items["ObstacleCar"].field_11 == 200
    assert items["ObstacleBusLondon"].field_11 == 500
