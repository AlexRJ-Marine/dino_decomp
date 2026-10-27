# Campaign objective verbs — decoded (2026-10-25)

Scope: static decompilation of `snoObjective.cpp` in `DinoIsland.exe`.
No writes to `game/` or `original/`.

## Parser and dispatch

`FUN_005654f0` (`snoObjective::Load`) tokenises one `ObjectiveN` value, looks
the verb up with `FUN_005658d0`, and stores:

| Offset | Meaning |
| ---: | --- |
| `+0x10` | first string argument (species/profile/building type) |
| `+0x1c` | second string argument (rare) |
| `+0x28` | first numeric argument (`atoi`, `FUN_005981b2`) |
| `+0x2c` | second numeric argument |
| `+0x30` | evaluator function pointer |
| `+0x34` | objective id (lookup result) |
| `+0x38` | application/park context |

The evaluator is called as `int f(objective*)`. The verdict is tri-state:

- `0` — pending / not yet;
- `1` — success;
- `2` — failure.

`FUN_005658d0` is a long string-compare chain; unrecognised verbs return `33`
and `FUN_005654f0` hits the `unknown objective` assert (`snoObjective.cpp`
line `0x146`).

## Verb table

| Id | Verb | Evaluator | Meaning |
| ---: | --- | --- | --- |
| 0 | `treasury` | `0x00567c00` | treasury >= arg1; if below, fails when arg2 <= 0, else pending |
| 1 | `marchandizing` | `0x00567c40` | merchandise income > arg1 |
| 2 | `dyingdinos` | `0x00569010` | death counters `+0x14c + +0x150` <= arg1 |
| 3 | `aggressivedinos` | `0x00568680` | pattern[1] `+0xbc`, count arg1, threshold arg2 |
| 4 | `genotypedinos` | `0x00568750` | count matching species + 10-field genotype >= arg2 |
| 5 | `cutedinos` | `0x00568770` | pattern[0] `+0x90`, count arg1, threshold arg2 |
| 6 | `hybriddinos` | `0x00568840` | pattern[2] `+0x1e4`, count arg1, threshold arg2 |
| 7 | `notconnectedbuildings` | `0x00567170` | `FUN_00566620` mode 0 |
| 8 | `connectedbuildings` | `0x00567150` | `FUN_00566620` mode 1 |
| 9 | `visitors` | `0x00566590` | visitor count >= arg2 (`all` or profile name) |
| 10 | `scaritydinos` | `0x00568910` | pattern[3] `+0x1e8` (unused) |
| 11 | `speeddinos` | `0x00568ab0` | pattern[6] `int(+0x7c)`, count arg1, speed arg2 |
| 12 | `staminadinos` | `0x00568b80` | pattern[7] `+0xc0`, count arg1, threshold arg2 |
| 13 | `cleverdinos` | `0x00568c50` | pattern[9] `+0x8c`, count arg1, threshold arg2 |
| 14 | `sizedagilitydinos` | `0x00569040` | pattern[5] `+0x98` and pattern[8] `+0x1e0`; count fixed at 1 |
| 15 | `franckstrangedinos` | `0x00569110` | `+0x1b0==1`, `+0x1b4==0`, `+0x190>=2`, `+0x6c<arg1`, `+0x3c>arg2` |
| 16 | `employees` | `0x005665f0` | employee count >= arg2; returns 0 or 1 only |
| 17 | `hungrydinos` | `0x005689e0` | pattern[4] `+0x244` food; count -1 = all |
| 18 | `isolateddinos` | `0x00568d20` | pattern[1] `+0xbc`, fail-list count (-2) |
| 19 | `happyvisitors` | `0x005665b0` | every visitor satisfaction >= arg2 |
| 20 | `keephappyvisitors` | `0x005665d0` | unused |
| 21 | `carnivordinos` | `0x00568de0` | `+0x1b0==1`, `+0x70 < arg1` (unused) |
| 22 | `ornateddinos` | `0x00568ef0` | count dinos with a non-empty 23-slot `+0xE8` array |
| 23 | `cameraaltitude` | `0x00569230` | camera altitude >= arg1 |
| 24 | `happyshow` | `0x00567710` | any show, profile score >= arg2 |
| 25 | `fightshow` | `0x005677b0` | fight show, hooligan score >= arg1 (unused) |
| 26 | `scientificshow` | `0x005678b0` | show score > arg1 and a dino `+0x1e4` > arg2 |
| 27 | `happydinoshow` | `0x00567b10` | any show, score >= arg1 and >= arg2 dinos |
| 28 | `delaylimit` | `0x00567bb0` | clock <= load-time + arg1, then fails |
| 29 | `buyalldinos` | `0x00569280` | dino store vector is empty |
| 30 | `dinoexclusiveshow` | `0x005692a0` | a show has a dino that no other show has |
| 31 | `prettystrangedino` | `0x005694c0` | `+0x40 > arg1`, `+0x194 < arg2` |
| 32 | `bankrupt` | `0x00567c80` | fails when `sim+0x95` is set |

## Shared evaluators

### `FUN_00566080` — visitors (ids 9/19/20)

Arguments `(profile-string, a, b, c)`; the `all` string takes the total
visitor path, otherwise `FUN_00565e60` maps the name to a visitor profile
index and the per-profile counter is used.

- `visitors <profile> <n>` passes `(n, 0, 1)`: pending while count < n,
  success once count >= n.
- `happyvisitors <profile> <pct>` passes `(0, pct, 1)`: success when every
  visitor value in the per-visitor satisfaction array is >= pct.
- `keephappyvisitors` passes `(0, pct, 0)` (unused in retail).

### `FUN_00566620` — buildings (ids 7/8)

Arguments `(type-string, mode, count)`. The type string is mapped to a
building kind (`blackbox`, `farm`, `show`, `lunchdoor`, `fence`, …); the
function counts buildings of that kind and, for mode 1, those with the
`+0x34` enabled byte set and kind `+0x6c == 0`. It returns success as soon as
`count` is reached; a negative count means "all".

`Cpg03_Mission5` uses the unusual `connectedbuildings buildings 50`, and
`fence`/`lunchdoor`/`show`/`farm`/`blackbox` occur elsewhere.

### `FUN_00567cb0` — genotype/trait matcher (ids 3,5,6,10,11,12,13,14,17,18)

Loads genotype designs from `.\Data\Design\scenario\genotype\`, then compares
**10 dinosaur trait fields** against a 10-int pattern. The evaluator passes
`(speciesFilter, pattern, mode=2, requiredCount, -1, 1)`; `mode 2` selects
the comparison side, `requiredCount` is arg1, and `-1` means "return success
after the count check". The matched list is built from the park dinosaurs.

| Slot | Actor field | Trait | Verbs |
| ---: | --- | --- | --- |
| 0 | `+0x90` | cuteness/beauty | `cutedinos` |
| 1 | `+0xbc` | aggression | `aggressivedinos`, `isolateddinos` |
| 2 | `+0x1e4` | hybrid/strangeness | `hybriddinos` |
| 3 | `+0x1e8` | scarcity/rarity | `scaritydinos` |
| 4 | `+0x244` | food need | `hungrydinos` |
| 5 | `+0x98` | size | `sizedagilitydinos` |
| 6 | `int(+0x7c)` | speed | `speeddinos` |
| 7 | `+0xc0` | stamina | `staminadinos` |
| 8 | `+0x1e0` | agility | `sizedagilitydinos` |
| 9 | `+0x8c` | intelligence | `cleverdinos` |

The field loads are at `0x567f42`–`0x567fa3` (`+0x244`, `+0x98`, `+0x90`,
`+0xbc`, `+0x1e4`, float `+0x7c`, `+0x1e0`, `+0xc0`, `+0x8c`, `+0x1e8`).
`genotypedinos all 5` passes an empty pattern (all slots 0), so it filters by
species only; `genotypedinos hypsilo 2` filters by species.

`isolateddinos` passes `requiredCount = -2` and `param_6 = 1`; `-2` selects
the *rejected* list (dinos that do not match), which is then checked for
isolation tags — hence "isolated" rather than "aggressive".

Correction (2026-10-27, `0x568d20`): arg0 is written to pattern[1] and `-2`
resolves to the **matched** count, so every dino with aggression >= arg0 goes
through the isolation second phase; the port models isolation as "alone in its
paddock".

### `FUN_00567190` — shows (ids 24/25/27)

Signature `(objective, showType, profile, threshold, requiredCount,
minParticipants)`. The show-type strings map to ids 1..7:

```text
race 1   obstacle 2   fight 3   destruction 4
team 5   capture 6    fence 7   all = any
```

It iterates shows of that type, gets the show's per-profile score array
(`show+0xEC -> +0x6C`), counts entries `>= threshold`, and requires at least
`requiredCount` matches while each show has at least `minParticipants`
dinosaurs (`show+0xB4 - show+0xB0 >> 2`).

- `happyshow <profile> <score>`: type `all`, count 1.
- `happydinoshow <score> <participants>`: type `all`, profile `all`, count 1,
  min participants = arg2.
- `fightshow <score>`: type `fight`, profile `hooligan`, count 1 (unused).

`scientificshow` (0x5678b0) requires a show whose per-type score is
`> arg1` and a dinosaur in the show's `show+0xAC` list with `+0x1E4 > arg2`.
`dinoexclusiveshow` (0x5692a0) walks every show's `show+0xAC` dinosaur vector
and succeeds when a dinosaur appears in exactly one show.

## Simple evaluator evidence

- `treasury` (`0x00567c00`): `CMP treasury, arg1; JGE -> 1`, else
  `arg2 <= 0 ? 2 : 0`. This is why every retail line is `treasury X 1`.
- `marchandizing` (`0x00567c40`): sum of `spParkEconomy+0x1c/+0x20/+0x24`
  compared with arg1.
- `dyingdinos` (`0x00569010`): `arg1 >= [+0x14c] + [+0x150]` -> 1 else 2.
- `employees` (`0x005665f0`): `(end-begin)>>2 >= arg2` -> 1 else 0.
- `cameraaltitude` (`0x00569230`): camera altitude via `FUN_005e5a50`;
  returns 1 when `altitude >= arg1`.
- `delaylimit` (`0x00567bb0`): `FUN_0058c350` reads the clock; success while
  `clock <= arg1 + arg2` (the loader replaces arg2 with the load-time clock).
- `buyalldinos` (`0x00569280`): success iff the dino-store vector
  (`sim+0x38`) is empty.
- `bankrupt` (`0x00567c80`): `AL = [sim+0x95]`, `NEG/SBB/AND 2` -> 2 if set,
  else 0.

## Data quirks found

- `Cpg07_Mission1` says `dyinddinos 5` (missing `g`). Cpg07 is the unlisted
  cut campaign, so the `unknown objective` path is not reachable in retail.
- `Cpg06_Mission1` has `Objective3=genotypedinos diplo 1""` (no opening
  quote, trailing `""`). `atoi("1\"\"")` still yields 1, so it works.
- `scaritydinos`, `keephappyvisitors`, `carnivordinos`, `fightshow` and
  `bankrupt` are implemented but unused by the 89 CampaignList objectives.

## Open

- Human-readable names for the show score field, the `+0x1e4` strangeness
  field, and the `franckstrangedinos` / `prettystrangedino` fields.
- Difficulty variants and objective weighting.

## Evidence

- `FUN_005654f0` — `snoObjective::Load` (unknown-objective assert at `0x146`).
- `FUN_005658d0` — verb → id lookup.
- Evaluator addresses in the table above.
- `FUN_00566080`, `FUN_00566620`, `FUN_00567cb0`, `FUN_00567190` — shared
  visitor/building/genotype/show helpers.
- `notes/campaign-recon.md`, `tools/formats/campaign.py`,
  `tests/test_campaign.py`.
