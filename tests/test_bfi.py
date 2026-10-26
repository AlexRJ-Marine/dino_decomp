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
ACTIONS = GAME / "Data" / "Design" / "attractions" / "actions" / "ActionList.ini"
CONSTRUCTION = GAME / "Data" / "Overlay" / "ConstructionInterface.Ini"
INFOPANELS = GAME / "Data" / "Overlay" / "Infopanels.Ini"

ATTRS = {"agility", "courage", "combat", "protection", "perception", "intelligence", "agress"}


def test_scan_all_features():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    items = scan(FEATURES)
    assert len(items) == 62
    assert len({item.name for item in items}) == 62
    for item in items:
        # The strict parser only returns after consuming the file exactly.
        assert item.width >= 1 and item.height >= 1
        assert item.vertical >= 0
        assert item.field_11 >= 100
        assert item.field_15 >= 1
        assert item.price >= 10
        assert item.strings
        assert len(item.mesh_slots) == 8
        assert len(item.effects) == 7
        assert len(item.sounds) == 3
        assert len(item.attribute_sets) == 5
        assert item.score_steps == [200.0, 400.0, 600.0, 800.0, 1000.0]
        assert item.billboard_dir == "DATA\\graphics\\TEXTURES\\billboards\\"
        assert item.trailing_string == ""
        assert item.state_count in (1, 2, 3)


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
        for effect in item.effects:
            if effect.lower().endswith(".ini"):
                assert any(GAME.rglob(Path(effect).name)), (item.name, effect)
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
    assert items["ObstacleCar"].price == 800
    assert items["CageHigh"].price == 1200
    assert items["StaticBaitApple"].price == 10


def test_switch_keys_are_construction_buttons():
    if not FEATURES.exists() or not CONSTRUCTION.exists():
        pytest.skip("retail sample not present")
    text = CONSTRUCTION.read_text(encoding="cp1252", errors="replace")
    text += INFOPANELS.read_text(encoding="cp1252", errors="replace")
    for item in scan(FEATURES):
        if item.switch_key:
            assert f"[{item.switch_key}]" in text, (item.name, item.switch_key)


def test_judge_kinds():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    items = {item.name: item for item in scan(FEATURES)}
    expected = {
        "JudgeRace": 1,
        "JudgeObstacle": 2,
        "JudgeFight": 3,
        "JudgeDestruction": 4,
        "JudgeTeam": 5,
        "JudgeCapture": 6,
    }
    for name, kind in expected.items():
        assert items[name].judge_kind == kind, name
        assert items[name].judge
    for item in items.values():
        if item.class_tag != "Judge":
            assert item.judge_kind == 0


def test_food_kinds_match_bsi_and_bni():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    from actors import read_bni
    from bsi import read_bsi

    bsi = {
        path.stem.replace("Food", ""): read_bsi(path).kind_id
        for path in (GAME / "Data" / "Design" / "foodsource").glob("*.bsi")
    }
    bni = {
        path.stem.replace("Animal", ""): read_bni(path).food_kind
        for path in (GAME / "Data" / "Design" / "animal").glob("*.bni")
    }

    items = {item.name: item for item in scan(FEATURES)}
    for name, kind in bsi.items():
        item = items.get(f"StaticBait{name}")
        if item is not None:
            assert item.food_kind == kind, name
    for name, kind in bni.items():
        assert items[f"AnimalBait{name}"].food_kind == kind, name
    for item in items.values():
        if item.class_tag not in ("StaticBait", "AnimalBait"):
            assert item.food_kind == 0


def test_show_types_are_judge_kinds():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    items = scan(FEATURES)
    for item in items:
        assert set(item.show_types) <= {1, 2, 3, 4, 5, 6}
    # Obstacles are used in obstacle/destruction shows; waypoints in races.
    by_name = {item.name: item for item in items}
    assert by_name["ObstacleCar"].show_types == [2, 4]
    assert by_name["WaypointTeam1"].show_types == [1, 2]
    assert by_name["StaticBaitApple"].show_types == [1, 2, 5, 6]


def test_state_counts_and_mesh_slots():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    by_name = {item.name: item for item in scan(FEATURES)}
    assert by_name["CageHigh"].state_count == 1
    assert by_name["ObstacleCar"].state_count == 2
    assert by_name["JudgeRace"].state_count == 2
    assert by_name["TileHeal"].state_count == 1
    assert by_name["CageHigh"].mesh_slots[6].lower().endswith("cages.tga")
    assert by_name["ObstacleCar"].mesh_slots[0].lower().endswith("carintact_lod1.mesh")
    assert by_name["ObstacleCar"].mesh_slots[1].lower().endswith("carintact_lod2.mesh")
    assert by_name["ObstacleCar"].mesh_slots[3].lower().endswith("car_lod1.mesh")
    assert by_name["ObstacleCar"].mesh_slots[4].lower().endswith("car_lod2.mesh")
    assert by_name["WaypointTeam1"].mesh_slots[6].lower().endswith("piste_red.tga")
    assert by_name["WaypointTeam1"].mesh_slots[7].lower().endswith("piste_red.tga")
    # Judge arena meshes live in effects[5].
    assert by_name["JudgeRace"].effects[5].lower().endswith("course.mesh")
    assert by_name["JudgeFight"].effects[5].lower().endswith("boxe.mesh")


def test_actions_resolve_case_insensitively():
    if not ACTIONS.exists():
        pytest.skip("retail sample not present")
    names = {
        line.strip().lower()
        for line in ACTIONS.read_text(encoding="cp1252").splitlines()
        if line.strip()
    }
    for item in scan(FEATURES):
        for action in item.actions:
            assert action.lower() in names, (item.name, action)


def test_attribute_vocabulary_and_weights():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    for item in scan(FEATURES):
        for group in item.attribute_sets:
            for attr in group:
                assert attr.lower() in ATTRS, (item.name, attr)
        # slots 3/4 are always empty in the retail data
        assert item.attribute_sets[3] == []
        assert item.attribute_sets[4] == []
    by_name = {item.name: item for item in scan(FEATURES)}
    assert by_name["ObstacleBeam1"].attribute_sets[0] == ["agility"]
    assert by_name["ObstacleBeam4"].attribute_sets[0] == ["agility"] * 5
    assert by_name["ObstacleCar"].attribute_sets[1] == ["combat"] * 2
    assert by_name["ObstacleRock"].attribute_sets[1] == ["combat"] * 4


def test_duration_pair_and_tail_flags():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    for item in scan(FEATURES):
        ratio = item.duration_frames / item.duration
        # Seconds and animation frames; the frame rate is 10, 12 or 15 fps
        # depending on the action (bait 12, TileSmellbad 10, the rest 15).
        assert ratio == pytest.approx(10.0) or ratio == pytest.approx(12.0) or ratio == pytest.approx(15.0)
    by_name = {item.name: item for item in scan(FEATURES)}
    assert by_name["LunchBox"].tail_flag == 1
    assert all(item.tail_flag == 0 for item in by_name.values() if item.name != "LunchBox")
    assert by_name["ObstacleBeam5"].category == 1
    assert by_name["LunchBox"].category == 4
    assert by_name["TileHeal"].category == 2


def test_waypoint_teams_and_display_types():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    by_name = {item.name: item for item in scan(FEATURES)}
    for team in range(1, 5):
        assert by_name[f"WaypointTeam{team}"].waypoint_team == team
    for item in by_name.values():
        if item.class_tag == "Cage":
            assert item.display_type == 1
        elif item.class_tag in ("Tile", "StaticBait", "AnimalBait", "Waypoint", "Teleport"):
            assert item.display_type == 3
        else:
            assert item.display_type == 0
        if item.class_tag != "Waypoint":
            assert item.waypoint_team == 0


def test_real_file_roundtrip():
    if not FEATURES.exists():
        pytest.skip("retail sample not present")
    item = read_bfi(FEATURES / "ObstacleCar.bfi")
    assert item.name == "ObstacleCar"
    assert item.class_tag == ""
    assert item.width == 2 and item.height == 4
    assert item.price == 800
    assert item.state_count == 2
    assert item.judge_kind == 0
    assert item.food_kind == 0
    assert item.actions[0] == "ObstAttackSize1"
    assert item.actions[-1] == "JumpForBiggies"
    assert item.texture_dir.lower().endswith("textures\\buildings")
    assert item.effects[:2] == ["explo_2verre.ini", "explo_1verre.ini"]
    assert item.switch_key == "SWITCH_FEATURE_OBSTACLE_CAR2"
    assert item.sounds == ["", "carAttack", ""]
