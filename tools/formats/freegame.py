#!/usr/bin/env python3
"""Reader for the free-game setup ``Data/Design/Scenario/FreeGame.ini``.

The file includes ``MissionBase.ini`` (the campaign/mission schema) and then
overrides the free-game values:

```ini
[CONTEXT]
Budget=75000              BudgetMedium=50000        BudgetHard=25000
SellBackFactor=1.0        ...Medium=0.5             ...Hard=0.2

[PROFILES]
VisProfiles="retraite 30 100 surfer 30 100 ..."      (6 profiles)
VisProfilesMedium="..."                              (6)
VisProfilesHard="..."                                (6)

[SHOWS]
Shows0="Cpg1_Race_1 Cpg1_Race_2 Cpg1_Race_3"
...
Shows9=""

[DINOSTORE]
DinoStore0="Allosaurus 9000 allo.gen ..."

[LABORATORY]
Reproduction=3 Modification=3 Cloning=3 Mutation=3 Fridge=3
```

The included mission sections are empty except ``[INTERFACE]`` (all seven
features enabled) and ``[SELLING]`` (``Dinosaur=1``).

See ``docs/formats/freegame.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path

from market import MarketEntry
from overlay import parse_ini

DIFFICULTIES = ("easy", "medium", "hard")


class FreeGameError(ValueError):
    pass


@dataclass
class FreeGame:
    path: str
    budgets: dict[str, int]
    sellback: dict[str, float]
    profiles: dict[str, list[tuple[str, int, int]]]
    shows: dict[int, list[str]]
    dinostore: list[MarketEntry]
    laboratory: dict[str, int]
    interface: dict[str, int] = field(default_factory=dict)
    selling: dict[str, int] = field(default_factory=dict)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "budgets": self.budgets,
            "sellback": self.sellback,
            "profiles": {
                key: [list(item) for item in value]
                for key, value in self.profiles.items()
            },
            "shows": {str(key): value for key, value in self.shows.items()},
            "dinostore": [entry.summary() for entry in self.dinostore],
            "laboratory": self.laboratory,
            "interface": self.interface,
            "selling": self.selling,
        }


def _triplets(value: str, what: str) -> list[tuple[str, int, int]]:
    tokens = value.split()
    if len(tokens) % 3:
        raise FreeGameError(f"{what}: {len(tokens)} tokens, not a multiple of 3")
    out = []
    for index in range(0, len(tokens), 3):
        name, first, second = tokens[index:index + 3]
        out.append((name, int(first), int(second)))
    return out


def _store_entries(value: str, store: int, what: str) -> list[MarketEntry]:
    tokens = value.split()
    if len(tokens) % 3:
        raise FreeGameError(f"{what}: {len(tokens)} tokens, not a multiple of 3")
    out = []
    for index in range(0, len(tokens), 3):
        name, price, genotype = tokens[index:index + 3]
        out.append(
            MarketEntry(store=store, name=name, price=int(price), genotype=genotype)
        )
    return out


def read_freegame(path: Path) -> FreeGame:
    sections = {section.name.upper(): section for section in parse_ini(path)}
    context = sections.get("CONTEXT")
    profiles = sections.get("PROFILES")
    shows_section = sections.get("SHOWS")
    store_section = sections.get("DINOSTORE")
    lab_section = sections.get("LABORATORY")
    for name, section in (
        ("CONTEXT", context),
        ("PROFILES", profiles),
        ("SHOWS", shows_section),
        ("DINOSTORE", store_section),
        ("LABORATORY", lab_section),
    ):
        if section is None:
            raise FreeGameError(f"{path}: missing [{name}]")

    budgets = {
        "easy": context.as_int("BUDGET"),
        "medium": context.as_int("BUDGETMEDIUM"),
        "hard": context.as_int("BUDGETHARD"),
    }
    sellback = {
        "easy": context.as_float("SELLBACKFACTOR"),
        "medium": context.as_float("SELLBACKFACTORMEDIUM"),
        "hard": context.as_float("SELLBACKFACTORHARD"),
    }

    parsed_profiles = {
        "easy": _triplets(profiles.get("VISPROFILES"), "[PROFILES]/VisProfiles"),
        "medium": _triplets(
            profiles.get("VISPROFILESMEDIUM"), "[PROFILES]/VisProfilesMedium"
        ),
        "hard": _triplets(
            profiles.get("VISPROFILESHARD"), "[PROFILES]/VisProfilesHard"
        ),
    }

    shows: dict[int, list[str]] = {}
    for key, value in shows_section.values.items():
        if not key.startswith("SHOWS"):
            continue
        page = int(key[5:])
        shows[page] = value.split()
    if sorted(shows) != list(range(10)):
        raise FreeGameError(f"{path}: expected Shows0..Shows9, got {sorted(shows)}")

    dinostore: list[MarketEntry] = []
    for key in sorted(
        store_section.values, key=lambda item: int(item[9:])
    ):
        if not key.startswith("DINOSTORE"):
            raise FreeGameError(f"{path}: unexpected store key {key}")
        dinostore.extend(
            _store_entries(store_section.values[key], int(key[9:]), key)
        )

    laboratory = {key: int(value) for key, value in lab_section.values.items()}
    interface = {
        key: int(value)
        for key, value in sections.get("INTERFACE", lab_section).values.items()
    }
    selling = {
        key: int(value)
        for key, value in sections.get("SELLING", lab_section).values.items()
    }
    return FreeGame(
        path=str(path),
        budgets=budgets,
        sellback=sellback,
        profiles=parsed_profiles,
        shows=shows,
        dinostore=dinostore,
        laboratory=laboratory,
        interface=interface,
        selling=selling,
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path)
    args = ap.parse_args(argv)
    print(json.dumps(read_freegame(args.path).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
