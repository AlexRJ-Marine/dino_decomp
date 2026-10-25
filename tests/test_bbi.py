import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from bbi import read_bbi, read_building_list, scan  # noqa: E402
from overlay import parse_ini  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
BUILDINGS = GAME / "Data" / "Design" / "buildings"
CONSTRUCTION = GAME / "Data" / "Overlay" / "ConstructionInterface.Ini"
UI_INIS = ["ConstructionInterface.Ini", "ParkInterface.Ini", "Infopanels.Ini",
           "ListInterface.Ini", "LabInterfaceMain.Ini", "LabInterfaceBreed.Ini",
           "LabInterfaceModify.Ini", "LabInterfaceFridge.Ini",
           "LabInterfaceClone.Ini"]


def test_barak_profile():
    if not (BUILDINGS / "BarakCirc.bbi").exists():
        pytest.skip("retail sample not present")
    building = read_bbi(BUILDINGS / "BarakCirc.bbi")
    assert len(building.meshes) == 10
    assert building.header_words[0] == 4
    assert building.header_words[1] == 8
    assert "CONSTRUC_BARAQUES3_BUTTON" in building.ui_names
    assert "explo_1.ini" in building.tail_strings
    assert "fries" in building.tail_strings


def test_entrance_profile():
    if not (BUILDINGS / "Entrance.bbi").exists():
        pytest.skip("retail sample not present")
    building = read_bbi(BUILDINGS / "Entrance.bbi")
    assert len(building.meshes) == 10
    assert building.header_words[0] == 24
    assert "boat" in building.tail_strings


def test_scan_all_buildings():
    if not BUILDINGS.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(BUILDINGS)
    assert total == 39
    assert not bad, bad[:5]
    assert sums["meshes"] == 390
    assert sums["ui"] == 213


def test_mesh_paths_and_list():
    if not BUILDINGS.exists():
        pytest.skip("retail game tree not present")
    names = read_building_list(BUILDINGS / "BuildingList.ini")
    assert len(names) == 39
    assert all((BUILDINGS / f"{name}.bbi").exists() for name in names)
    missing = set()
    for path in BUILDINGS.glob("*.bbi"):
        for mesh in read_bbi(path).meshes:
            if not (GAME / mesh).exists():
                missing.add(mesh)
    assert not missing, sorted(missing)[:5]


def test_ui_names_resolve():
    if not BUILDINGS.exists() or not CONSTRUCTION.exists():
        pytest.skip("retail game tree not present")
    sections = set()
    for name in UI_INIS:
        path = GAME / "Data" / "Overlay" / name
        if path.exists():
            sections.update(section.name for section in parse_ini(path, expand=False))
    missing = set()
    for path in BUILDINGS.glob("*.bbi"):
        for ui in read_bbi(path).ui_names:
            if ui not in sections:
                missing.add((path.stem, ui))
    assert missing == {("Security", "CONSTRUC_SECUANDFOOD_SECU_BUTTON")}
