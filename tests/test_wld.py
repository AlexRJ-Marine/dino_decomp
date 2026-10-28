import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from sav import read_sav  # noqa: E402
from wld import WldFormatError, read_sav_world, read_wld, scan  # noqa: E402

SCENARIO = ROOT / "game" / "%MAINDIR%" / "Data" / "Design" / "Scenario"
CPG01 = SCENARIO / "Cpg01" / "Cpg01_Save.wld"
CPG02 = SCENARIO / "Cpg02" / "Cpg02_Save.wld"
CPG03 = SCENARIO / "Cpg03" / "Cpg03_Save.wld"
CPG04 = SCENARIO / "Cpg04" / "Cpg04_Save.wld"
CPG05 = SCENARIO / "Cpg05" / "Cpg05_Save.wld"
CPG07 = SCENARIO / "Cpg07" / "Cpg07_Save.wld"
SLOT001 = ROOT / "run" / "save" / "slot001.sav"
AUTO = ROOT / "run" / "save" / "auto" / "auto.sav"


def test_cpg01_entrance():
    if not CPG01.exists():
        pytest.skip("retail .wld files not present")
    wld = read_wld(CPG01)
    assert wld.version == 51
    assert wld.mode == 1
    assert wld.size == 200
    assert wld.world.campaign_id is None
    assert wld.world.mission_id is None
    assert wld.world.objectives == []
    assert wld.world.fences == []
    assert wld.world.shows == []
    assert wld.world.animals == []
    assert wld.world.employees == []
    assert len(wld.world.buildings) == 1
    b = wld.world.buildings[0]
    assert (b.object_id, b.linked_id, b.kind, b.cell_x, b.cell_y) == (1, 2, 9, 520, 376)
    assert b.kind_name == "Entrance"
    assert b.state == 1
    assert b.link == 0
    assert wld.world.visitors == []
    assert wld.world.dinosaurs == []
    assert [r.to_dict() for r in wld.roads] == [
        {"x0": 524, "y0": 392, "x1": 524, "y1": 452}
    ]
    assert wld.simulation_offset == 0x54
    assert wld.camera is not None
    assert wld.camera[:3] == pytest.approx((524.7033, 433.4687, 35.0), abs=1e-3)


def test_cpg04_populated_world():
    if not CPG04.exists():
        pytest.skip("retail .wld files not present")
    wld = read_wld(CPG04)
    w = wld.world
    assert wld.version == 53
    assert wld.mode == 1
    assert wld.size == 41397
    assert len(w.fences) == 9
    assert len(w.shows) == 1
    assert len(w.shows[0].waypoints) == 71
    assert len(w.animals) == 2
    assert len(w.employees) == 6
    assert len(w.buildings) == 57
    assert w.visitors == []
    assert len(w.dinosaurs) == 20
    assert len(wld.roads) == 14
    # A free-game-style campaign world has no save-only unlock arrays.
    assert w.feature_flags == b""
    assert w.building_flags == b""
    assert w.food_flags == b""
    assert w.misc_flags == b""
    assert (w.buildings[0].object_id, w.buildings[0].kind) == (1, 9)
    assert (w.buildings[0].cell_x, w.buildings[0].cell_y) == (440, 200)
    assert w.buildings[0].kind_name == "Entrance"
    assert w.dinosaurs[0].class_tag == 0x12
    assert len(w.dinosaurs[0].genotype) == 18
    assert wld.roads[0].to_dict() == {"x0": 380, "y0": 408, "x1": 656, "y1": 408}
    assert wld.simulation_offset == 0x2010
    # The .wld carries the live simulation lists, not just the placement graph.
    assert len(wld.simulation_dinos) == 20
    assert [(f.object_id, f.position) for f in wld.food_sources] == [
        (264, (461.5, 421.5)), (267, (461.5, 445.5))]
    assert [(e.object_id, e.cell_x, e.cell_y, e.state)
            for e in wld.employees] == [
        (13, 475, 340, 2), (15, 475, 340, 2), (337, 504, 364, 2),
        (375, 406, 411, 2), (411, 504, 364, 2), (63785, 450, 595, 2)]
    assert wld.attraction_count == 57
    assert wld.simulation_end == 0x6482


def test_cpg02_03_05_versions():
    if not (CPG02.exists() and CPG03.exists() and CPG05.exists()):
        pytest.skip("retail .wld files not present")
    cpg02 = read_wld(CPG02)
    assert cpg02.version == 51
    assert (len(cpg02.world.fences), len(cpg02.world.buildings), len(cpg02.world.dinosaurs)) == (
        2,
        4,
        0,
    )
    assert len(cpg02.roads) == 2

    cpg03 = read_wld(CPG03)
    assert cpg03.version == 50
    assert (len(cpg03.world.fences), len(cpg03.world.buildings), len(cpg03.world.dinosaurs)) == (
        1,
        4,
        0,
    )
    assert len(cpg03.roads) == 3

    cpg05 = read_wld(CPG05)
    assert cpg05.version == 52
    assert (
        len(cpg05.world.fences),
        len(cpg05.world.animals),
        len(cpg05.world.employees),
        len(cpg05.world.buildings),
    ) == (2, 0, 2, 8)
    assert len(cpg05.roads) == 3


def test_cpg07_is_legacy():
    if not CPG07.exists():
        pytest.skip("retail .wld files not present")
    with pytest.raises(WldFormatError):
        read_wld(CPG07)


def test_all_campaign_worlds():
    if not SCENARIO.exists():
        pytest.skip("retail scenario files not present")
    good, bad = scan(SCENARIO)
    assert len(good) == 7
    assert len(bad) == 1
    assert bad[0][0].endswith("Cpg07_Save.wld")
    assert {w.version for w in good} == {50, 51, 52, 53}
    assert all(w.camera is not None for w in good)


def test_sav_world_block_slot001():
    if not SLOT001.exists():
        pytest.skip("retail save not present")
    save = read_sav(SLOT001)
    world = read_sav_world(SLOT001)
    name_len = struct.unpack_from("<I", save.data, 8)[0]
    assert world.offset == 0x18 + name_len == 0x2C
    assert world.end == save.count_offset
    assert (world.campaign_id, world.mission_id) == (0xFFFFFFFF, 0xFFFFFFFF)
    assert world.objectives == [
        (4, 30, 100),
        (0, 30, 100),
        (2, 30, 100),
        (9, 30, 100),
        (1, 30, 100),
        (6, 30, 100),
    ]
    assert world.fences == []
    assert world.shows == []
    assert world.animals == []
    assert world.employees == []
    assert len(world.buildings) == 1
    assert world.buildings[0].kind_name == "Entrance"
    assert (world.buildings[0].cell_x, world.buildings[0].cell_y) == (520, 376)
    assert len(world.visitors) == 2
    assert world.visitors[0].object_id == 3
    assert world.visitors[1].object_id == 5
    assert world.dinosaurs == []
    assert world.feature_flags == b"\x01" * 62
    assert world.building_flags == b"\x01" * 39
    assert world.food_flags == b"\x01" * 10
    assert world.misc_flags == b"\x01" * 6


def test_world_class_table():
    from wld import WORLD_CLASSES

    assert WORLD_CLASSES == {
        0x12: "drsDinosaur",
        0x13: "drsVisitor",
        0x14: "drsEmployee",
        0x15: "drsAnimal",
        0x16: "drsShow",
        0x17: "drsFence",
        0x18: "drsBuilding",
    }


def test_cpg04_genotypes_use_the_18_gene_order():
    if not CPG04.exists():
        pytest.skip("retail .wld files not present")
    wld = read_wld(CPG04)
    ranges = (
        [(0, 19)] * 4
        + [(0, 47)] * 6
        + [(0, 7), (0, 2), (0, 5), (0, 3), (0, 1)]
        + [(0, 9)] * 3
    )
    assert len(ranges) == 18
    for dino in wld.world.dinosaurs:
        assert dino.class_tag == 0x12
        assert len(dino.genotype) == 18
        for (a, b), (lo, hi) in zip(dino.genotype, ranges):
            assert lo <= a <= hi and lo <= b <= hi


def test_sav_world_block_auto(retail_auto_save):
    save = read_sav(retail_auto_save)
    world = read_sav_world(retail_auto_save)
    name_len = struct.unpack_from("<I", save.data, 8)[0]
    assert world.offset == 0x18 + name_len == 0x1A
    assert world.end == save.count_offset
    assert len(world.buildings) == 1
    assert world.visitors == []
