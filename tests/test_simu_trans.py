import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from anim_trans import read_anim_trans  # noqa: E402
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
