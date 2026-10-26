#!/usr/bin/env python3
"""Reader for ``Data/Design/laboratory/Laboratory.ini``.

Small INI with four block families:

```text
[GENERAL]        MaxUpgradeLevel, MaxBeamStrength
[BEAM_INFO_n]    NumAlteredGenes, MutationSurvivalChance, RadiationDamage
[UPGRADE_LEVEL_n] ReproductionAvailable, MaxGeneModifications,
                  CloningFailurePercentage, MutationBeamMaxStrength,
                  FridgeSlots
[COST]           Breed, Cloning, Mutation1..3, {Rare,Uncommon,Common}
                 AlleleModification
```

See ``docs/formats/laboratory.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path

from overlay import OverlayFormatError, parse_ini


class LaboratoryError(ValueError):
    pass


@dataclass
class BeamInfo:
    level: int
    altered_genes: int
    survival_chance: int
    radiation_damage: int


@dataclass
class UpgradeLevel:
    level: int
    reproduction: bool
    max_gene_modifications: int
    cloning_failure: int
    beam_max_strength: int
    fridge_slots: int


@dataclass
class Laboratory:
    path: str
    max_upgrade_level: int
    max_beam_strength: int
    beams: list[BeamInfo] = field(default_factory=list)
    levels: list[UpgradeLevel] = field(default_factory=list)
    costs: dict[str, int] = field(default_factory=dict)

    def summary(self) -> dict:
        return {
            "path": self.path,
            "max_upgrade_level": self.max_upgrade_level,
            "max_beam_strength": self.max_beam_strength,
            "beams": [beam.__dict__ for beam in self.beams],
            "levels": [level.__dict__ for level in self.levels],
            "costs": self.costs,
        }


def read_laboratory(path: Path) -> Laboratory:
    sections = {section.name.upper(): section for section in parse_ini(path)}
    general = sections.get("GENERAL")
    if general is None:
        raise LaboratoryError(f"{path}: missing [GENERAL]")
    max_level = general.as_int("MAXUPGRADELEVEL")
    max_beam = general.as_int("MAXBEAMSTRENGTH")

    beams: list[BeamInfo] = []
    for index in range(1, max_beam + 1):
        section = sections.get(f"BEAM_INFO_{index}")
        if section is None:
            raise LaboratoryError(f"{path}: missing [BEAM_INFO_{index}]")
        beams.append(
            BeamInfo(
                level=index,
                altered_genes=section.as_int("NUMALTEREDGENES"),
                survival_chance=section.as_int("MUTATIONSURVIVALCHANCE"),
                radiation_damage=section.as_int("RADIATIONDAMAGE"),
            )
        )

    levels: list[UpgradeLevel] = []
    for index in range(1, max_level + 1):
        section = sections.get(f"UPGRADE_LEVEL_{index}")
        if section is None:
            raise LaboratoryError(f"{path}: missing [UPGRADE_LEVEL_{index}]")
        levels.append(
            UpgradeLevel(
                level=index,
                reproduction=bool(section.as_int("REPRODUCTIONAVAILABLE")),
                max_gene_modifications=section.as_int("MAXGENEMODIFICATIONS"),
                cloning_failure=section.as_int("CLONINGFAILUREPERCENTAGE"),
                beam_max_strength=section.as_int("MUTATIONBEAMMAXSTRENGTH"),
                fridge_slots=section.as_int("FRIDGESLOTS"),
            )
        )

    cost_section = sections.get("COST")
    if cost_section is None:
        raise LaboratoryError(f"{path}: missing [COST]")
    costs = {key: value for key, value in cost_section.values.items()}
    for key in (
        "BREED",
        "CLONING",
        "MUTATION1",
        "MUTATION2",
        "MUTATION3",
        "RAREALLELEMODIFICATION",
        "UNCOMMONALLELEMODIFICATION",
        "COMMONALLELEMODIFICATION",
    ):
        if key not in costs:
            raise LaboratoryError(f"{path}: missing COST/{key}")
        costs[key] = int(costs[key])
    return Laboratory(
        path=str(path),
        max_upgrade_level=max_level,
        max_beam_strength=max_beam,
        beams=beams,
        levels=levels,
        costs=costs,
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path)
    args = ap.parse_args(argv)
    print(json.dumps(read_laboratory(args.path).summary(), indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except OverlayFormatError as error:
        raise SystemExit(str(error)) from error
