import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from freegame import read_freegame  # noqa: E402
from market import read_market  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
SCENARIO = GAME / "Data" / "Design" / "Scenario"
FREE = SCENARIO / "FreeGame.ini"
MARKET = SCENARIO / "Market.ini"
SHOWS = SCENARIO / "Show"


def test_context_and_lab():
    if not FREE.exists():
        pytest.skip("retail sample not present")
    data = read_freegame(FREE)
    assert data.budgets == {"easy": 75000, "medium": 50000, "hard": 25000}
    assert data.sellback == {"easy": 1.0, "medium": 0.5, "hard": 0.2}
    assert data.laboratory == {
        "REPRODUCTION": 3,
        "MODIFICATION": 3,
        "CLONING": 3,
        "MUTATION": 3,
        "FRIDGE": 3,
    }
    assert data.interface == {
        "LABORATORY": 1,
        "SHOW": 1,
        "FENCE": 1,
        "BUILDING": 1,
        "INFRASTRUCTURE": 1,
        "DINOPURCHASE": 1,
        "EGGSALE": 1,
    }
    assert data.selling == {"DINOSAUR": 1}


def test_visitor_profiles():
    if not FREE.exists():
        pytest.skip("retail sample not present")
    data = read_freegame(FREE)
    assert {key: len(value) for key, value in data.profiles.items()} == {
        "easy": 6,
        "medium": 6,
        "hard": 6,
    }
    assert data.profiles["easy"][0] == ("retraite", 30, 100)
    assert data.profiles["medium"][-1] == ("riche", 10, 100)
    assert data.profiles["hard"][2] == ("spd", 10, 100)
    for entries in data.profiles.values():
        for name, first, second in entries:
            assert first > 0 and second == 100


def test_shows_and_dinostore():
    if not FREE.exists():
        pytest.skip("retail sample not present")
    data = read_freegame(FREE)
    assert sorted(data.shows) == list(range(10))
    assert data.shows[8] == [] and data.shows[9] == []
    total = sum(len(names) for names in data.shows.values())
    assert total == 39
    for names in data.shows.values():
        for name in names:
            assert (SHOWS / f"{name}.shw").exists(), name

    assert len(data.dinostore) == 20
    market = {entry.genotype: entry for entry in read_market(MARKET).entries}
    for entry in data.dinostore:
        assert entry.genotype in market
        assert entry.price == market[entry.genotype].price
        assert entry.name == market[entry.genotype].name
