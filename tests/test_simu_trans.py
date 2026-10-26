import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from anim_trans import read_anim_trans  # noqa: E402
from anim_trans import read_graphic_trans  # noqa: E402
from simu_trans import FAMILIES, read_simu_trans  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
GENOTYPE = GAME / "Data" / "Design" / "Genotype"
SIMU = GENOTYPE / "simu_trans.dic"
ANIM = GENOTYPE / "anim_trans.dic"


def test_parts_and_genes():
    if not SIMU.exists():
        pytest.skip("retail sample not present")
    data = read_simu_trans(SIMU)
    assert len(data.genes) == 18
    assert [n for n, _ in data.part_headers] == list(range(1, 11))
    assert data.part_headers[1][1].startswith("merge ratio")
    assert data.part_headers[9][1].startswith("Default family")


def test_merge_tables_match_anim_trans():
    if not (SIMU.exists() and ANIM.exists()):
        pytest.skip("retail samples not present")
    data = read_simu_trans(SIMU)
    anim = read_anim_trans(ANIM)
    assert set(data.merge) == {"Head", "ArmLeg", "Body", "Tail"}
    for name, table in data.merge.items():
        assert table.matrix == anim.tables[f"{name} merge translation table"], name
        assert len(table.diagonal) == 20
        assert len(table.matrix) == 20 and all(len(row) == 20 for row in table.matrix)


def test_attribute_tables():
    if not SIMU.exists():
        pytest.skip("retail sample not present")
    data = read_simu_trans(SIMU)
    assert len(data.attributes) == 55
    assert all(len(t.diagonal) == 20 for t in data.attributes.values())
    assert all(
        len(t.matrix) == 20 and all(len(row) == 20 for row in t.matrix)
        for t in data.attributes.values()
    )
    head_length = data.attributes[("Head", "Length")]
    assert head_length.diagonal[:5] == [224, 167, 662, 54, 91]
    assert head_length.matrix[0][:5] == [224, 195, 443, 138, 157]
    assert data.attributes[("Body", "Length")].diagonal[0] == 200
    assert data.attributes[("Tail", "Length")].diagonal[0] == 318
    assert data.attributes[("Head", "Beauty")].diagonal[0] == 2
    assert data.attributes[("Head", "Attack")].diagonal[0] == 80


def test_default_families():
    if not SIMU.exists():
        pytest.skip("retail sample not present")
    data = read_simu_trans(SIMU)
    assert data.default_families == [
        2, 4, 0, 3, 3, 0, 2, 3, 1, 1,
        3, 1, 3, 4, 0, 1, 2, 4, 3, 3,
    ]
    assert data.family_of(0) == "HeavyBiped"       # allo
    assert data.family_of(10) == "LightBiped"      # pachy
    assert data.family_of(17) == "LowQuadruped"    # trice
    counts = {FAMILIES[i]: data.default_families.count(i) for i in range(5)}
    assert counts == {
        "HighQuadruped": 3,
        "LightQuadruped": 4,
        "HeavyBiped": 3,
        "LightBiped": 7,
        "LowQuadruped": 3,
    }


def test_dummy_disposition_and_type():
    if not SIMU.exists():
        pytest.skip("retail sample not present")
    data = read_simu_trans(SIMU)
    assert set(data.dummy_disposition) == {"Head", "Body", "Tail"}
    assert set(data.dummy_type) == {"Head", "Body", "Tail"}
    for part in ("Head", "Body", "Tail"):
        rows = data.dummy_disposition[part]
        assert len(rows) == 48 and all(len(row) == 6 for row in rows)
        rows = data.dummy_type[part]
        assert len(rows) == 48 and all(len(row) == 5 for row in rows)
    assert data.dummy_disposition["Head"][0] == [0, 0, 0, 0, 0, 0]
    assert data.dummy_disposition["Head"][1] == [3, 0, 0, 3, 3, 3]
    assert data.dummy_type["Head"][0] == [0, 0, 0, 0, 0]
    assert data.dummy_type["Head"][1] == [7, 8, 9, 8, 0]


def test_skin_and_carapace_tables():
    if not SIMU.exists():
        pytest.skip("retail sample not present")
    data = read_simu_trans(SIMU)
    assert data.skin_map == [
        [0, 0, 0], [1, 2, 3], [4, 5, 6], [7, 8, 9],
        [10, 11, 12], [13, 14, 15], [16, 17, 18], [19, 20, 21],
    ]
    assert data.skin_unknown == 582
    assert len(data.skin_influence) == 2
    for table in data.skin_influence:
        assert len(table) == 22 and all(len(row) == 18 for row in table)
    assert data.skin_influence[0][0] == [0] * 18
    assert data.skin_influence[1][0][:17] == [100] * 17
    assert data.skin_influence[1][0][17] == 101

    assert data.carapace_map == [
        [0, 0, 0], [1, 2, 3], [4, 5, 6],
        [7, 8, 9], [10, 11, 12], [13, 14, 15],
    ]
    assert len(data.carapace_influence) == 2
    for table in data.carapace_influence:
        assert len(table) == 16 and all(len(row) == 18 for row in table)
    assert data.carapace_influence[0][0] == [0] * 18
    assert data.carapace_influence[1][0] == [100] * 18


def test_min_max_attributes():
    if not SIMU.exists():
        pytest.skip("retail sample not present")
    data = read_simu_trans(SIMU)
    assert data.min_attributes == [0.0] * 20
    assert len(data.max_attributes) == 20
    assert data.max_attributes[0] == 100000.0
    assert data.max_attributes[1] == 100000.0
    assert data.max_attributes[2] == 100.0
    assert data.max_attributes[18] == 65536.0
    assert data.max_attributes[19] == 100.0
    assert data.max_attributes[3] == 100.0
    assert data.max_attributes[4:18] == [200.0] * 14
    assert data.max_attributes[18] == 65536.0
    assert data.max_attributes[19] == 100.0


def test_dummy_influence_and_reference_widths():
    if not SIMU.exists():
        pytest.skip("retail sample not present")
    data = read_simu_trans(SIMU)
    assert len(data.dummy_influence) == 2
    for table in data.dummy_influence:
        assert len(table) == 50
        assert all(len(row) == 18 for row in table)
    assert data.dummy_influence[0][0] == [0] * 18
    assert data.dummy_influence[1][0] == [100] * 18
    assert data.dummy_influence[0][49][:6] == [40, 0, 0, 2, 2, 1]
    assert data.member_reference_widths == [100, 100, 100, 300, 150]
    if (GENOTYPE / "graphic_trans.dic").exists():
        assert data.member_reference_widths == read_graphic_trans(
            GENOTYPE / "graphic_trans.dic"
        )

def test_attribute_and_part_order():
    """Part 3 file order, used by the translator's 11 attribute groups."""
    if not SIMU.exists():
        pytest.skip("retail sample not present")
    data = read_simu_trans(SIMU)
    assert data.attribute_order == [
        "Length",
        "Width",
        "Height",
        "Weight",
        "Perception",
        "Intelligence",
        "Beauty",
        "Armor",
        "Speed",
        "Attack",
        "Agility",
    ]
    assert data.part_order == ["Head", "Arm", "Leg", "Body", "Tail"]
    for attribute in data.attribute_order:
        assert (data.part_order[0], attribute) in data.attributes
