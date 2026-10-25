import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from actors import read_bei, read_bni, scan_bei, scan_bni  # noqa: E402
from bsi import read_bsi  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
ANIMALS = GAME / "Data" / "Design" / "animal"
EMPLOYEES = GAME / "Data" / "Design" / "employees"
FOOD = GAME / "Data" / "Design" / "foodsource"


def test_cow_animal():
    if not (ANIMALS / "CowAnimal.bni").exists():
        pytest.skip("retail sample not present")
    cow = read_bni(ANIMALS / "CowAnimal.bni")
    assert cow.variant == 3
    assert cow.size == pytest.approx(2.5)
    assert cow.limit == 50
    assert cow.food_value == 1000
    assert cow.quantity == pytest.approx(0.5)
    assert cow.food_kind == 9
    assert cow.speed == pytest.approx(10.0)
    assert len(cow.meshes) == 6
    assert cow.texture_dirs == ["DATA\\graphics\\TEXTURES\\actors\\animals"]


def test_animal_list_and_food_kinds():
    if not ANIMALS.exists() or not FOOD.exists():
        pytest.skip("retail game tree not present")
    expected = {
        "CowAnimal": (9, 1000),
        "PigAnimal": (8, 600),
        "SheepAnimal": (7, 400),
        "GoatAnimal": (6, 250),
        "OstrichAnimal": (5, 200),
    }
    for name, (kind, value) in expected.items():
        animal = read_bni(ANIMALS / f"{name}.bni")
        assert animal.food_kind == kind, name
        assert animal.food_value == value, name
        food_name = name.removesuffix("Animal") + "Food"
        food = read_bsi(FOOD / f"{food_name}.bsi")
        assert animal.food_kind == food.kind_id, name


def test_scan_animals():
    if not ANIMALS.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan_bni(ANIMALS)
    assert total == 5
    assert not bad, bad[:5]
    assert sums["meshes"] == 33


def test_employees():
    if not (EMPLOYEES / "Guard.bei").exists():
        pytest.skip("retail sample not present")
    guard = read_bei(EMPLOYEES / "Guard.bei")
    assert guard.kind_id == 1
    assert guard.height == pytest.approx(1.7, abs=1e-4)
    assert guard.walk_speed == pytest.approx(1.0)
    assert guard.action_speed == pytest.approx(2.5)
    assert len(guard.meshes) == 4
    assert guard.sounds == ["tweet", "singing"]

    nurse = read_bei(EMPLOYEES / "Nutritionist.bei")
    assert nurse.kind_id == 0
    assert nurse.walk_speed == pytest.approx(2.0)
    assert len(nurse.meshes) == 5


def test_scan_employees():
    if not EMPLOYEES.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan_bei(EMPLOYEES)
    assert total == 2
    assert not bad, bad[:5]
    assert sums["meshes"] == 9
    assert sums["sounds"] == 4


def test_actor_assets_exist():
    if not ANIMALS.exists() or not EMPLOYEES.exists():
        pytest.skip("retail game tree not present")
    missing = set()
    for path in ANIMALS.glob("*.bni"):
        for mesh in read_bni(path).meshes:
            if not (GAME / mesh).exists():
                missing.add(mesh)
    for path in EMPLOYEES.glob("*.bei"):
        for mesh in read_bei(path).meshes:
            if not (GAME / mesh).exists():
                missing.add(mesh)
    assert not missing, sorted(missing)[:5]
