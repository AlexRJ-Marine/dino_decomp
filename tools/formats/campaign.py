#!/usr/bin/env python3
"""Reader for the Dino Island campaign/mission layer.

``Data/Design/Scenario/`` is data-driven:

    CampaignList.ini          7 campaigns: Cpg01..Cpg06, Cpg08
    CampaignBase.ini          template ([BRIEFING], [LANDSCAPE], [MISSION],
                              [CONTEXT] with WorldFile/Budget/SellBackFactor)
    CpgNN/CpgNN.ini           includes ..\\CampaignBase.ini; sets island,
                              mission list, world file, budget, ending video
    CpgNN/CpgNN_MissionList.ini  count + mission names
    CpgNN/CpgNN_MissionN.ini  includes ..\\MissionBase.ini; objectives,
                              allowed buildings/food/features/shows/store,
                              lab/interface unlocks, visitor profiles

An objective is a command string such as ``cameraaltitude 120`` or
``genotypedinos hypsilo 2``. 28 distinct verbs occur across the 39 missions.

See ``docs/formats/campaign.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path

from overlay import parse_ini


class CampaignFormatError(ValueError):
    pass


@dataclass(frozen=True)
class ObjectiveSpec:
    """One objective verb, its internal id and evaluator function."""

    objective_id: int
    evaluator: str
    args: tuple[str, ...] = ()
    note: str = ""
    used: bool = True


# Recovered from the verb lookup FUN_005658d0 and the evaluator dispatch in
# FUN_005654f0 (snoObjective.cpp).  Verdicts are tri-state: 0 = pending,
# 1 = success, 2 = failure.  `used` is False for verbs the executable supports
# but no retail mission in CampaignList.ini uses.
OBJECTIVE_VERBS: dict[str, ObjectiveSpec] = {
    "treasury": ObjectiveSpec(0, "0x00567c00", ("target:int", "[deadline:int]"),
                              "treasury >= target; below target fails when deadline <= 0"),
    "marchandizing": ObjectiveSpec(1, "0x00567c40", ("target:int",),
                                   "merchandise income > target"),
    "dyingdinos": ObjectiveSpec(2, "0x00569010", ("max:int",),
                                "death counters sum <= max"),
    "aggressivedinos": ObjectiveSpec(3, "0x00568680", ("threshold:int", "percent:int")),
    "genotypedinos": ObjectiveSpec(4, "0x00568750", ("species:str", "count:int"),
                                   "count dinos matching the species/9-trait genotype"),
    "cutedinos": ObjectiveSpec(5, "0x00568770", ("count:int", "percent:int")),
    "hybriddinos": ObjectiveSpec(6, "0x00568840", ("count:int", "percent:int")),
    "notconnectedbuildings": ObjectiveSpec(7, "0x00567170", ("type:str", "count:int"),
                                           "FUN_00566620 mode 0"),
    "connectedbuildings": ObjectiveSpec(8, "0x00567150", ("type:str", "count:int"),
                                        "FUN_00566620 mode 1"),
    "visitors": ObjectiveSpec(9, "0x00566590", ("profile:str", "count:int"),
                              "visitor count >= count; profile may be 'all'"),
    "scaritydinos": ObjectiveSpec(10, "0x00568910", ("?",), used=False),
    "speeddinos": ObjectiveSpec(11, "0x00568ab0", ("flag:int", "percent:int")),
    "staminadinos": ObjectiveSpec(12, "0x00568b80", ("flag:int", "percent:int")),
    "cleverdinos": ObjectiveSpec(13, "0x00568c50", ("flag:int", "percent:int")),
    "sizedagilitydinos": ObjectiveSpec(14, "0x00569040", ("flag:int", "percent:int")),
    "franckstrangedinos": ObjectiveSpec(15, "0x00569110", ("count:int", "percent:int")),
    "employees": ObjectiveSpec(16, "0x005665f0", ("profile:str", "count:int"),
                               "employee count >= count; never fails"),
    "hungrydinos": ObjectiveSpec(17, "0x005689e0", ("flag:int", "percent:int")),
    "isolateddinos": ObjectiveSpec(18, "0x00568d20", ("count:int",)),
    "happyvisitors": ObjectiveSpec(19, "0x005665b0", ("profile:str", "percent:int"),
                                   "every visitor has satisfaction >= percent"),
    "keephappyvisitors": ObjectiveSpec(20, "0x005665d0", ("profile:str", "percent:int"),
                                       used=False),
    "carnivordinos": ObjectiveSpec(21, "0x00568de0", ("?",), used=False),
    "ornateddinos": ObjectiveSpec(22, "0x00568ef0", ("count:int",)),
    "cameraaltitude": ObjectiveSpec(23, "0x00569230", ("height:int",),
                                    "camera altitude >= height"),
    "happyshow": ObjectiveSpec(24, "0x00567710", ("profile:str", "percent:int")),
    "fightshow": ObjectiveSpec(25, "0x005677b0", ("?",), used=False),
    "scientificshow": ObjectiveSpec(26, "0x005678b0", ("threshold:int", "percent:int")),
    "happydinoshow": ObjectiveSpec(27, "0x00567b10", ("count:int", "duration:int")),
    "delaylimit": ObjectiveSpec(28, "0x00567bb0", ("limit:int",),
                                "clock <= load-time + limit; fails after"),
    "buyalldinos": ObjectiveSpec(29, "0x00569280", (),
                                 "dino store is empty"),
    "dinoexclusiveshow": ObjectiveSpec(30, "0x005692a0", ()),
    "prettystrangedino": ObjectiveSpec(31, "0x005694c0", ("count:int", "percent:int")),
    "bankrupt": ObjectiveSpec(32, "0x00567c80", (), "fails when the bankruptcy flag is set",
                              used=False),
}


def objective_spec(verb: str) -> ObjectiveSpec | None:
    return OBJECTIVE_VERBS.get(verb.lower())


def _merged(path: Path) -> dict[str, dict[str, str]]:
    """Merge #INCLUDEs; later section/key definitions override earlier ones."""
    out: dict[str, dict[str, str]] = {}
    for section in parse_ini(path):
        if section.name.upper() == "#INCLUDE":
            continue
        out.setdefault(section.name, {}).update(section.values)
    return out


def _list_file(path: Path) -> list[str]:
    lines = [line.strip() for line in path.read_text(encoding="cp1252").splitlines() if line.strip()]
    if not lines:
        raise CampaignFormatError(f"{path}: empty list")
    try:
        count = int(lines[0])
    except ValueError as e:
        raise CampaignFormatError(f"{path}: bad count {lines[0]!r}") from e
    names = lines[1:]
    if len(names) != count:
        raise CampaignFormatError(f"{path}: {count} declared, {len(names)} names")
    return names


@dataclass
class Objective:
    description_id: str
    command: str
    verb: str
    args: list[str] = field(default_factory=list)


@dataclass
class Mission:
    name: str
    path: str
    title: str = ""
    description: str = ""
    hint: str = ""
    objectives: list[Objective] = field(default_factory=list)
    sections: dict[str, dict[str, str]] = field(default_factory=dict)

    def allowed(self, section: str) -> list[str]:
        return [value for key, value in self.sections.get(section, {}).items()
                if key.startswith(section.upper()[:-1]) and value.strip()]

    def summary(self) -> dict:
        return {
            "name": self.name,
            "title": self.title,
            "objectives": [
                {"verb": o.verb, "args": o.args, "description": o.description_id}
                for o in self.objectives
            ],
            "buildings": self.allowed("BUILDINGS"),
            "foodsources": self.allowed("FOODSOURCES"),
            "features": self.allowed("FEATURES"),
        }


@dataclass
class Campaign:
    name: str
    path: str
    island: str = ""
    world_file: str = ""
    budget: int = 0
    sell_back: float = 1.0
    missions: list[Mission] = field(default_factory=list)
    sections: dict[str, dict[str, str]] = field(default_factory=dict)

    def summary(self) -> dict:
        return {
            "name": self.name,
            "island": self.island,
            "world_file": self.world_file,
            "budget": self.budget,
            "sell_back": self.sell_back,
            "missions": [m.name for m in self.missions],
        }


def read_mission(path: Path) -> Mission:
    sections = _merged(path)
    briefing = sections.get("BRIEFING", {})
    objectives = []
    obj_section = sections.get("OBJECTIVES", {})
    for index in range(1, 6):
        command = obj_section.get(f"OBJECTIVE{index}", "").strip()
        if not command:
            continue
        parts = command.split()
        objectives.append(Objective(
            description_id=obj_section.get(f"OBJECTIVE{index}_DESC", ""),
            command=command,
            verb=parts[0].lower(),
            args=parts[1:],
        ))
    return Mission(
        name=path.stem,
        path=str(path),
        title=briefing.get("TITLE", ""),
        description=briefing.get("DESCRIPTIONTEXT", ""),
        hint=briefing.get("HINTTEXT", ""),
        objectives=objectives,
        sections=sections,
    )


def read_campaign(path: Path) -> Campaign:
    sections = _merged(path)
    name = path.stem
    directory = path.parent
    mission_list = sections.get("MISSION", {}).get("MISSIONLIST", "")
    if not mission_list:
        raise CampaignFormatError(f"{path}: no MissionList")
    mission_names = _list_file(directory / mission_list)
    missions = []
    for mission_name in mission_names:
        mission_path = directory / f"{mission_name}.ini"
        if not mission_path.exists():
            raise CampaignFormatError(f"{path}: missing mission {mission_path.name}")
        missions.append(read_mission(mission_path))
    context = sections.get("CONTEXT", {})
    return Campaign(
        name=name,
        path=str(path),
        island=sections.get("LANDSCAPE", {}).get("ISLAND", ""),
        world_file=context.get("WORLDFILE", ""),
        budget=int(context.get("BUDGET", "0") or 0),
        sell_back=float(context.get("SELLBACKFACTOR", "1.0") or 1.0),
        missions=missions,
        sections=sections,
    )


def load_campaigns(root: Path) -> list[Campaign]:
    names = _list_file(root / "CampaignList.ini")
    campaigns = []
    for name in names:
        path = root / name / f"{name}.ini"
        if not path.exists():
            raise CampaignFormatError(f"missing campaign {path}")
        campaigns.append(read_campaign(path))
    return campaigns


def scan(root: Path) -> dict:
    campaigns = load_campaigns(root)
    verbs: dict[str, int] = {}
    for campaign in campaigns:
        for mission in campaign.missions:
            for objective in mission.objectives:
                verbs[objective.verb] = verbs.get(objective.verb, 0) + 1
    return {
        "campaigns": len(campaigns),
        "missions": sum(len(c.missions) for c in campaigns),
        "verbs": dict(sorted(verbs.items())),
        "detail": [c.summary() for c in campaigns],
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--scan", type=Path)
    ap.add_argument("--dump", type=Path, help="CpgNN.ini")
    args = ap.parse_args(argv)
    if args.scan:
        print(json.dumps(scan(args.scan), indent=2))
        return 0
    if args.dump:
        campaign = read_campaign(args.dump)
        data = campaign.summary()
        data["missions_detail"] = [m.summary() for m in campaign.missions]
        print(json.dumps(data, indent=2))
        return 0
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
