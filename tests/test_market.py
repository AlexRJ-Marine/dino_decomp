import sys
from collections import Counter
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from market import read_market  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
MARKET = GAME / "Data" / "Design" / "Scenario" / "Market.ini"
GENOTYPES = GAME / "Data" / "Design" / "Scenario" / "Genotype"


def test_market_entries():
    if not MARKET.exists():
        pytest.skip("retail sample not present")
    market = read_market(MARKET)
    assert len(market.entries) == 54
    assert market.stores == [0, 2, 3, 4, 5, 6]  # store page 1 is absent
    assert all(entry.price > 0 for entry in market.entries)
    assert len(set(entry.genotype for entry in market.entries)) == 54
    counts = Counter(entry.store for entry in market.entries)
    assert counts == {0: 10, 2: 14, 3: 14, 4: 9, 5: 5, 6: 2}
    first = market.entries[0]
    assert (first.store, first.name, first.price, first.genotype) == (
        0,
        "Allosaurus",
        9000,
        "allo.gen",
    )
    assert market.by_genotype()["trex.gen"].price == 18000
    assert market.by_genotype()["rex.gen"].name == "Rex"


def test_market_genotypes_exist():
    if not MARKET.exists() or not GENOTYPES.exists():
        pytest.skip("retail sample not present")
    market = read_market(MARKET)
    for entry in market.entries:
        assert (GENOTYPES / entry.genotype).exists(), entry.genotype


def test_design_genotypes_not_in_market():
    """Three design .gen files are not for sale (mission/cut data)."""
    if not MARKET.exists() or not GENOTYPES.exists():
        pytest.skip("retail sample not present")
    market = read_market(MARKET)
    used = {entry.genotype for entry in market.entries}
    design = {path.name for path in GENOTYPES.glob("*.gen")}
    assert design - used == {"anky2.gen", "peleca3.gen", "proto2.gen"}
