import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from campaign import (  # noqa: E402
    OBJECTIVE_VERBS,
    TRAIT_PATTERN,
    load_campaigns,
    read_campaign,
    read_mission,
    scan,
)
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


def test_objective_trait_pattern():
    """The 10 genotype-comparison slots map to actor trait fields."""
    assert sorted(TRAIT_PATTERN) == list(range(10))
    assert TRAIT_PATTERN[0] == ("+0x90", "cuteness/beauty")
    assert TRAIT_PATTERN[1] == ("+0xbc", "aggression")
    assert TRAIT_PATTERN[4] == ("+0x244", "food need")
    assert TRAIT_PATTERN[6] == ("int(+0x7c)", "speed")
    assert TRAIT_PATTERN[9] == ("+0x8c", "intelligence")
    # Each trait verb names the slot it constrains.
    assert "pattern[0]" in OBJECTIVE_VERBS["cutedinos"].note
    assert "pattern[1]" in OBJECTIVE_VERBS["aggressivedinos"].note
    assert "pattern[2]" in OBJECTIVE_VERBS["hybriddinos"].note
    assert "pattern[6]" in OBJECTIVE_VERBS["speeddinos"].note
    assert OBJECTIVE_VERBS["speeddinos"].args == ("count:int", "speed:int")
    assert OBJECTIVE_VERBS["hungrydinos"].args == ("count:int", "percent:int")
    assert OBJECTIVE_VERBS["scientificshow"].args == ("score:int", "strangeness:int")
    assert "no other show" in OBJECTIVE_VERBS["dinoexclusiveshow"].note


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


def test_no_mission_overrides_difficulty_variants():
    """All 39 missions inherit the base difficulty placeholders."""
    if not (SCENARIO / "CampaignList.ini").exists():
        pytest.skip("retail sample not present")
    for campaign in load_campaigns(SCENARIO):
        for mission in campaign.missions:
            context = mission.sections["CONTEXT"]
            assert context["BUDGETMEDIUM"] == "0", mission.name
            assert context["BUDGETHARD"] == "0", mission.name
            assert context["SELLBACKFACTORMEDIUM"] == "1.0", mission.name
            assert context["SELLBACKFACTORHARD"] == "1.0", mission.name
            profiles = mission.sections["PROFILES"]
            assert profiles["VISPROFILESMEDIUM"] == "", mission.name
            assert profiles["VISPROFILESHARD"] == "", mission.name


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

BUILDING_GROUPS = {
    0: {"Farm"},
    1: {"PointOfView", "SmallSeats", "MediumSeats", "LargeSeats"},
    # Cpg07 (cut content) puts BarakSport in the entrance group.
    2: {"ParkNat", "ParkCirc", "ParkKute", "ParkSport", "ParkViol",
        "BarakSport"},
    3: {"BarakNat", "BarakCirc", "BarakKute", "BarakSport", "BarakViol"},
    4: {"SouvNat", "SouvCirc", "SouvKute", "SouvSport", "SouvViol"},
    5: {"RestNat", "RestCirc", "RestKute", "RestSport", "RestViol"},
    6: {"Tampodino", "Granderoue", "Manege", "Tower", "Haunted"},
    7: {"Station", "DecoNat", "DecoKute", "DecoCirc", "DecoSport", "DecoViol"},
    8: {"DecoNat", "DecoKute", "DecoCirc", "DecoSport", "DecoViol"},
}
FEATURE_GROUPS = {
    0: {"LunchBox"},
    1: {"CageSmall", "CageMedium", "CageHigh", "CageHuge"},
    2: {"JudgeRace", "JudgeObstacle", "JudgeFight", "JudgeDestruction",
        "JudgeTeam", "JudgeCapture"},
    3: {"StaticBaitApple", "StaticBaitFlower", "StaticBaitPumpkin",
        "StaticBaitWatermelon", "StaticBaitHay", "AnimalBaitOstrich",
        "AnimalBaitGoat", "AnimalBaitSheep", "AnimalBaitPig", "AnimalBaitCow"},
    4: {"WaypointTeam1", "WaypointTeam2", "WaypointTeam3", "WaypointTeam4",
        "TileSmell", "TileSmellbad", "TileSmellBad"},
    5: {"ObstacleBeam1", "ObstacleBeam2", "ObstacleBeam3", "ObstacleBeam4",
        "ObstacleBeam5", "ObstacleRock", "ObstacleHole", "ObstacleHole2",
        "ObstacleFire", "ObstaclePiege", "ObstacleMine",
        "ObstacleForceField", "ObstacleTrappe"},
    6: {"TileSlide", "TileJump", "TileCourage", "TilePerception",
        "TileScotch", "TileQuad", "TileEndurance", "TileShield",
        "TileIntelligence", "TileSpeed", "TileHeal"},
    7: {"Danse", "TileInvisibility", "TileKickAss", "TileSleep", "Teleport"},
    8: {"ObstacleCar", "ObstacleBusSchool", "ObstacleMiniCar",
        "ObstacleBusLondon"},
}


def test_mission_content_groups():
    """BUILDINGS/FEATURES group indices are content categories."""
    if not SCENARIO.exists():
        pytest.skip("retail sample not present")
    features = {
        line.strip()
        for line in (GAME / "Data/Design/attractions/features/FeatureList.ini")
        .read_text(encoding="cp1252").splitlines()
        if line.strip()
    }
    drinks = {"TileSmellBad"}  # retail typo used by four Cpg02 missions
    show_names = {path.stem.lower() for path in (SCENARIO / "Show").glob("*.shw")}
    genotypes = {path.name for path in (SCENARIO / "Genotype").glob("*.gen")}
    typo_missions = []
    for path in sorted(SCENARIO.glob("Cpg0*/Cpg*_Mission*.ini")):
        mission = read_mission(path)
        for key, value in mission.sections.get("BUILDINGS", {}).items():
            group = int(key[-1])
            for name in value.split():
                assert name in BUILDING_GROUPS[group], (path.name, key, name)
        for key, value in mission.sections.get("FEATURES", {}).items():
            group = int(key[-1])
            for name in value.split():
                assert name in FEATURE_GROUPS[group], (path.name, key, name)
                assert name in features or name in drinks, name
                if name in drinks:
                    typo_missions.append(path.name)
        for key, value in mission.sections.get("SHOWS", {}).items():
            for name in value.split():
                assert name.lower() in show_names, (path.name, key, name)
        for key, value in mission.sections.get("DINOSTORE", {}).items():
            tokens = value.split()
            assert len(tokens) % 3 == 0, (path.name, key)
            for index in range(0, len(tokens), 3):
                assert tokens[index + 1].isdigit()
                assert tokens[index + 2] in genotypes, tokens[index + 2]
        for key, value in mission.sections.get("FRIDGE", {}).items():
            tokens = value.split()
            assert len(tokens) % 2 == 0, (path.name, key)
            for genotype in tokens[1::2]:
                assert genotype in genotypes, genotype
    assert len(typo_missions) == 4

def test_campaign_briefing_assets():
    """Ending videos exist; the island snapshots are not shipped."""
    if not (SCENARIO / "CampaignList.ini").exists():
        pytest.skip("retail sample not present")
    campaigns = load_campaigns(SCENARIO)
    videos = {path.name.lower() for path in GAME.rglob("*.mpg")}
    snapshots = {path.name.lower() for path in GAME.rglob("*.tga")}
    used_snapshots = 0
    used_videos = 0
    for campaign in campaigns:
        briefing = campaign.sections.get("BRIEFING", {})
        snapshot = briefing.get("ISLANDSNAPSHOT", "")
        if snapshot:
            used_snapshots += 1
            assert snapshot.lower() not in snapshots, snapshot  # dead field
        ending = briefing.get("ENDINGVIDEO", "")
        if ending and ending.lower() not in ("", "none"):
            used_videos += 1
            assert ending.lower() in videos, ending
    assert used_snapshots == 7
    assert used_videos == 5
