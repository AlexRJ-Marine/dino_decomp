import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from anim_trans import (  # noqa: E402
    MERGE_TABLES,
    read_anim_trans,
    read_graphic_trans,
    read_proportion,
    scan,
)

GAME = ROOT / "game" / "%MAINDIR%"
GENOTYPE = GAME / "Data" / "Design" / "Genotype"
ANIM_DIR = GAME / "Data" / "Graphics" / "Actors" / "Dinos" / "animations"


def test_anim_trans_sections():
    if not (GENOTYPE / "anim_trans.dic").exists():
        pytest.skip("retail sample not present")
    data = read_anim_trans(GENOTYPE / "anim_trans.dic")
    assert len(data.dinosaurs) == 20
    assert data.dinosaurs[:3] == ["allo", "anky", "brachio"]
    assert data.dinosaurs[-1] == "velo"
    assert data.families == [
        "HighQuadruped", "LightQuadruped", "HeavyBiped", "LightBiped",
        "LowQuadruped",
    ]
    assert len(data.animations) == 51
    assert data.animations[0] == "Neutral"
    assert data.animations[-1] == "Slip"
    assert set(data.tables) == set(MERGE_TABLES)
    assert all(len(table) == 20 for table in data.tables.values())


def test_merge_matrix_values():
    if not (GENOTYPE / "anim_trans.dic").exists():
        pytest.skip("retail sample not present")
    data = read_anim_trans(GENOTYPE / "anim_trans.dic")
    for name in ("Head merge translation table", "Body merge translation table",
                 "Tail merge translation table"):
        values = {v for row in data.tables[name] for v in row}
        assert values == {0, 50}, name
        assert all(data.tables[name][i][i] == 0 for i in range(20))
    arm = data.tables["ArmLeg merge translation table"]
    assert {v for row in arm for v in row} == {0, 1, 50, 99}
    assert all(arm[i][i] == 0 for i in range(20))
    # Directional 10/10 split plus two special pairs.
    group_a = {"allo", "coelo", "deino", "gigano", "hypsilo", "pachy",
               "peleca", "trex", "trood", "velo"}
    group_b = {"anky", "brachio", "diplo", "igua", "kentro", "parasau",
               "proto", "salta", "stego", "trice"}
    for row_name, row in zip(data.dinosaurs, arm):
        for col_name, value in zip(data.dinosaurs, row):
            if row_name == col_name:
                assert value == 0, (row_name, col_name)
            elif row_name in group_a and col_name in group_b:
                assert value == (99 if (row_name, col_name) == ("trex", "stego") else 1)
            elif row_name in group_b and col_name in group_a:
                assert value == 99, (row_name, col_name)
            elif row_name in group_a and col_name in group_a:
                assert value == 50, (row_name, col_name)
            elif row_name in group_b and col_name in group_b:
                assert value == (1 if (row_name, col_name) == ("anky", "trice") else 50)


def test_animation_names_exist_in_every_family():
    if not ANIM_DIR.exists():
        pytest.skip("retail animation tree not present")
    data = read_anim_trans(GENOTYPE / "anim_trans.dic")
    for family in data.families:
        folder = ANIM_DIR / family
        stems = {p.stem.split("_", 1)[1] for p in folder.glob("*.anim")}
        assert set(data.animations) <= stems, family


def test_graphic_trans_widths():
    if not (GENOTYPE / "graphic_trans.dic").exists():
        pytest.skip("retail sample not present")
    assert read_graphic_trans(GENOTYPE / "graphic_trans.dic") == [100, 100, 100, 300, 150]


def test_proportion_table():
    if not (GENOTYPE / "proportion.dat").exists():
        pytest.skip("retail sample not present")
    data = read_anim_trans(GENOTYPE / "anim_trans.dic")
    table = read_proportion(GENOTYPE / "proportion.dat", data.dinosaurs)
    assert len(table.scales) == 20
    assert all(len(row) == 5 for row in table.scales)
    assert table.dinosaurs == data.dinosaurs
    # Legs are the reference column.
    assert all(abs(row[3] - 1.0) < 1e-6 for row in table.scales)
    # Known extremes: T-rex arms, Diplodocus tail, Brachiosaurus head.
    assert table.scale("trex", "arm") == pytest.approx(0.235, abs=1e-5)
    assert table.scale("diplo", "tail") == pytest.approx(2.0927, abs=1e-4)
    assert table.scale("brachio", "head") == pytest.approx(1.1375, abs=1e-4)


def test_scan_dictionaries():
    if not GENOTYPE.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(GENOTYPE)
    assert total == 3
    assert not bad, bad
    assert sums["dinosaurs"] == 20
    assert sums["families"] == 5
    assert sums["animations"] == 51
    assert sums["tables"] == 4
    assert sums["proportions"] == 20
