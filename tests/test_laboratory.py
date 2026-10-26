import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from laboratory import read_laboratory  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
LAB = GAME / "Data" / "Design" / "laboratory" / "Laboratory.ini"


def test_general_and_beams():
    if not LAB.exists():
        pytest.skip("retail sample not present")
    lab = read_laboratory(LAB)
    assert lab.max_upgrade_level == 4
    assert lab.max_beam_strength == 3
    assert [(b.level, b.altered_genes, b.survival_chance, b.radiation_damage)
            for b in lab.beams] == [
        (1, 6, 100, 10),
        (2, 2, 100, 20),
        (3, 2, 100, 40),
    ]


def test_upgrade_levels():
    if not LAB.exists():
        pytest.skip("retail sample not present")
    lab = read_laboratory(LAB)
    assert [(l.level, l.reproduction, l.max_gene_modifications,
             l.cloning_failure, l.beam_max_strength, l.fridge_slots)
            for l in lab.levels] == [
        (1, False, 0, 100, 0, 4),
        (2, True, 2, 40, 1, 8),
        (3, True, 4, 20, 2, 8),
        (4, True, 20, 0, 3, 8),
    ]


def test_costs():
    if not LAB.exists():
        pytest.skip("retail sample not present")
    lab = read_laboratory(LAB)
    assert lab.costs == {
        "BREED": 2000,
        "CLONING": 4000,
        "MUTATION1": 1000,
        "MUTATION2": 2000,
        "MUTATION3": 3000,
        "RAREALLELEMODIFICATION": 1000,
        "UNCOMMONALLELEMODIFICATION": 500,
        "COMMONALLELEMODIFICATION": 250,
    }
