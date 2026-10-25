import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from campaign import OBJECTIVE_VERBS, load_campaigns, read_campaign, scan  # noqa: E402
from loc import read_loc  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
SCENARIO = GAME / "Data" / "Design" / "Scenario"


def test_campaign_table():
    if not (SCENARIO / "CampaignList.ini").exists():
        pytest.skip("retail sample not present")
    campaigns = load_campaigns(SCENARIO)
    assert [c.name for c in campaigns] == [
        "Cpg01", "Cpg02", "Cpg03", "Cpg04", "Cpg05", "Cpg06", "Cpg08",
    ]
    assert [(c.island, c.budget) for c in campaigns] == [
        ("Isl01", 60000), ("Isl02", 50000), ("Isl03", 35000),
        ("Isl04", 20000), ("Isl05", 30000), ("Isl06", 50000),
        ("Isl07", 20000),
    ]
    assert sum(len(c.missions) for c in campaigns) == 39


def test_mission_counts():
    if not (SCENARIO / "CampaignList.ini").exists():
        pytest.skip("retail sample not present")
    campaigns = load_campaigns(SCENARIO)
    assert {c.name: len(c.missions) for c in campaigns} == {
        "Cpg01": 14, "Cpg02": 5, "Cpg03": 5, "Cpg04": 4,
        "Cpg05": 4, "Cpg06": 4, "Cpg08": 3,
    }


def test_cpg01_first_and_fourth_mission():
    if not (SCENARIO / "Cpg01").exists():
        pytest.skip("retail sample not present")
    campaign = read_campaign(SCENARIO / "Cpg01" / "Cpg01.ini")
    first = campaign.missions[0]
    assert first.objectives[0].command == "cameraaltitude 120"
    fourth = campaign.missions[3]
    assert [o.command for o in fourth.objectives] == [
        "genotypedinos hypsilo 2",
        "genotypedinos coelo 2",
        "dyingdinos 0",
    ]
    assert "PointOfView" in fourth.allowed("BUILDINGS")
    assert fourth.sections["FRIDGE"]["EGGS0"] == "Coelophysis coelo.gen"


def test_objectives_and_verbs():
    if not (SCENARIO / "CampaignList.ini").exists():
        pytest.skip("retail sample not present")
    data = scan(SCENARIO)
    assert data["campaigns"] == 7
    assert data["missions"] == 39
    assert sum(data["verbs"].values()) == 89
    assert len(data["verbs"]) == 28
    assert data["verbs"]["genotypedinos"] == 14
    assert data["verbs"]["happyvisitors"] == 10


def test_objective_verb_table():
    """The decompiled verb -> id/evaluator table covers every mission verb."""
    assert len(OBJECTIVE_VERBS) == 33
    ids = sorted(spec.objective_id for spec in OBJECTIVE_VERBS.values())
    assert ids == list(range(33))
    assert OBJECTIVE_VERBS["treasury"].objective_id == 0
    assert OBJECTIVE_VERBS["genotypedinos"].evaluator == "0x00568750"
    assert OBJECTIVE_VERBS["bankrupt"].objective_id == 32
    # Supported but unused by any CampaignList mission.
    unused = {v for v, spec in OBJECTIVE_VERBS.items() if not spec.used}
    assert unused == {
        "scaritydinos", "keephappyvisitors", "carnivordinos", "fightshow",
        "bankrupt",
    }
    if not (SCENARIO / "CampaignList.ini").exists():
        pytest.skip("retail missions not present")
    unknown = {
        objective.verb
        for campaign in load_campaigns(SCENARIO)
        for mission in campaign.missions
        for objective in mission.objectives
        if objective.verb not in OBJECTIVE_VERBS
    }
    assert not unknown, sorted(unknown)


def test_world_files_and_cpg07():
    if not (SCENARIO / "CampaignList.ini").exists():
        pytest.skip("retail sample not present")
    campaigns = load_campaigns(SCENARIO)
    for campaign in campaigns:
        world = SCENARIO / campaign.name / campaign.world_file
        assert world.exists(), campaign.world_file
        assert world.stat().st_size >= 200
    # Cpg07 has data but is absent from CampaignList.ini.
    assert (SCENARIO / "Cpg07" / "Cpg07_Save.wld").exists()
    assert "Cpg07" not in [c.name for c in campaigns]


def test_mission_loc_ids_resolve():
    if not (SCENARIO / "CampaignList.ini").exists():
        pytest.skip("retail sample not present")
    known = set()
    for name in ("MenuEng.loc", "ParkEng.loc"):
        loc = read_loc(GAME / "Data" / "Overlay" / name)
        known.update(record.field_id for record in loc.records)
    missing = set()
    for campaign in load_campaigns(SCENARIO):
        for mission in campaign.missions:
            for text in (mission.title, mission.description, mission.hint,
                         *(o.description_id for o in mission.objectives)):
                if text and text not in known:
                    missing.add(text)
    assert not missing, sorted(missing)[:5]
