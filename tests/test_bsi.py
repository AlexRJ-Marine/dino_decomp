import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from bsi import read_bsi, read_food_list, scan  # noqa: E402
from overlay import parse_ini  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
FOOD = GAME / "Data" / "Design" / "foodsource"


def test_apple_profile():
    if not (FOOD / "AppleFood.bsi").exists():
        pytest.skip("retail sample not present")
    food = read_bsi(FOOD / "AppleFood.bsi")
    assert food.nutrition == pytest.approx(200.0)
    assert food.quantity == pytest.approx(2.0)
    assert food.price == 200
    assert food.kind_id == 1
    assert food.flags == 256
    assert len(food.assets) == 4
    assert food.feeder_mesh.endswith("feed_apple.mesh")
    assert food.ui_name == "FOOD_DISTRIBUTION_VEGETARIAN_FRUITS_BUTTON"


def test_scan_all_food():
    if not FOOD.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(FOOD)
    assert total == 10
    assert not bad, bad[:5]
    assert sums["assets"] == 40
    assert sums["ui"] == 10


def test_food_list_and_kinds():
    if not FOOD.exists():
        pytest.skip("retail game tree not present")
    names = read_food_list(FOOD / "FoodSourceList.ini")
    assert names == [
        "FlowerFood", "AppleFood", "WatermelonFood", "PumpkinFood", "HayFood",
        "OstrichFood", "GoatFood", "SheepFood", "PigFood", "CowFood",
    ]
    for index, name in enumerate(names):
        food = read_bsi(FOOD / f"{name}.bsi")
        assert food.kind_id == index, name


def test_assets_and_ui_resolve():
    if not FOOD.exists():
        pytest.skip("retail game tree not present")
    sections = set()
    for path in (GAME / "Data" / "Overlay").glob("*.Ini"):
        sections.update(section.name for section in parse_ini(path, expand=False))
    missing = set()
    ui_missing = set()
    for path in FOOD.glob("*.bsi"):
        food = read_bsi(path)
        for asset in food.assets:
            if asset.startswith("DATA\\") and not (GAME / asset).exists():
                missing.add(asset)
        if food.feeder_mesh and not (GAME / food.feeder_mesh).exists():
            missing.add(food.feeder_mesh)
        if food.ui_name not in sections:
            ui_missing.add((path.stem, food.ui_name))
    assert not missing, sorted(missing)[:5]
    assert not ui_missing, sorted(ui_missing)
