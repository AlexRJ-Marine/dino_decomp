# `Laboratory.ini` — lab upgrades, mutation beams and costs

Status: **solved.** Plain INI, 45 values in four block families. The reader
validates every expected key.

## Inventory

`Data/Design/laboratory/Laboratory.ini` — 1,050 bytes, cp1252, `[SECTION]`
and `Key=Value` lines, no includes.

## Blocks

### `[GENERAL]`

| Key | Value |
| --- | ---: |
| `MaxUpgradeLevel` | 4 |
| `MaxBeamStrength` | 3 |

### `[BEAM_INFO_1..3]` — mutation beam strengths

| Beam | `NumAlteredGenes` | `MutationSurvivalChance` | `RadiationDamage` |
| ---: | ---: | ---: | ---: |
| 1 | 6 | 100 | 10 |
| 2 | 2 | 100 | 20 |
| 3 | 2 | 100 | 40 |

Beam 1 changes the most genes at the lowest damage; beams 2 and 3 change two
genes with progressively more radiation damage. Survival is 100% in all
three, so the beam risk is the damage, not lethality.

### `[UPGRADE_LEVEL_1..4]`

| Level | `ReproductionAvailable` | `MaxGeneModifications` | `CloningFailurePercentage` | `MutationBeamMaxStrength` | `FridgeSlots` |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | 0 | 0 | 100 | 0 | 4 |
| 2 | 1 | 2 | 40 | 1 | 8 |
| 3 | 1 | 4 | 20 | 2 | 8 |
| 4 | 1 | 20 | 0 | 3 | 8 |

Level 1 cannot breed (`0`), cannot clone (100% failure) and cannot mutate.
Reproduction unlocks at level 2; the cloning failure drops 100 → 40 → 20 → 0;
the beam strength cap rises 0 → 3; the gene-modification cap is 0/2/4/20.

### `[COST]`

| Key | Value |
| --- | ---: |
| `Breed` | 2,000 |
| `Cloning` | 4,000 |
| `Mutation1` | 1,000 |
| `Mutation2` | 2,000 |
| `Mutation3` | 3,000 |
| `RareAlleleModification` | 1,000 |
| `UncommonAlleleModification` | 500 |
| `CommonAlleleModification` | 250 |

These are the park-budget costs shown by the lab panels
(`LabInterfaceBreed/Clone/Fridge/Modify.Ini`).

## Reader

`tools/formats/laboratory.py` reuses `overlay.parse_ini` and returns a
`Laboratory` with `beams`, `levels` and `costs`. Missing sections or keys
raise. Tests: `tests/test_laboratory.py` (all values).

## Evidence

- `Data/Design/laboratory/Laboratory.ini` (read-only).
- `Data/Overlay/LabInterface*.Ini` — the panels that display these actions.
- `tools/formats/laboratory.py`, `tests/test_laboratory.py`.

## Open

- Which key the lab UI reads for the upgrade price (not in this file; the
  upgrade itself is a building/construction cost).
- Whether `RadiationDamage` is a probability, a hit-point loss or a stat
  penalty. The value is integer percent-like (10/20/40).
