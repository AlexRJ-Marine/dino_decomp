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
