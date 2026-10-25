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
| 3 | `aggressivedinos` | `0x00568680` | trait count (open) |
| 4 | `genotypedinos` | `0x00568750` | count matching species + 9-trait genotype >= arg2 |
| 5 | `cutedinos` | `0x00568770` | trait count (open) |
| 6 | `hybriddinos` | `0x00568840` | trait count (open) |
| 7 | `notconnectedbuildings` | `0x00567170` | `FUN_00566620` mode 0 |
| 8 | `connectedbuildings` | `0x00567150` | `FUN_00566620` mode 1 |
| 9 | `visitors` | `0x00566590` | visitor count >= arg2 (`all` or profile name) |
| 10 | `scaritydinos` | `0x00568910` | unused |
| 11 | `speeddinos` | `0x00568ab0` | trait count (open) |
| 12 | `staminadinos` | `0x00568b80` | trait count (open) |
| 13 | `cleverdinos` | `0x00568c50` | trait count (open) |
| 14 | `sizedagilitydinos` | `0x00569040` | trait count (open) |
| 15 | `franckstrangedinos` | `0x00569110` | trait count (open) |
| 16 | `employees` | `0x005665f0` | employee count >= arg2; returns 0 or 1 only |
| 17 | `hungrydinos` | `0x005689e0` | trait count (open) |
| 18 | `isolateddinos` | `0x00568d20` | count (open) |
| 19 | `happyvisitors` | `0x005665b0` | every visitor satisfaction >= arg2 |
| 20 | `keephappyvisitors` | `0x005665d0` | unused |
| 21 | `carnivordinos` | `0x00568de0` | unused |
| 22 | `ornateddinos` | `0x00568ef0` | count (open) |
| 23 | `cameraaltitude` | `0x00569230` | camera altitude >= arg1 |
| 24 | `happyshow` | `0x00567710` | show result (open) |
| 25 | `fightshow` | `0x005677b0` | unused |
| 26 | `scientificshow` | `0x005678b0` | show result (open) |
| 27 | `happydinoshow` | `0x00567b10` | show result (open) |
| 28 | `delaylimit` | `0x00567bb0` | clock <= load-time + arg1, then fails |
| 29 | `buyalldinos` | `0x00569280` | dino store vector is empty |
| 30 | `dinoexclusiveshow` | `0x005692a0` | show result (open) |
| 31 | `prettystrangedino` | `0x005694c0` | trait count (open) |
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

### `FUN_00567cb0` — genotypes (id 4)

Loads genotype designs from `.\Data\Design\scenario\genotype\`, then compares
nine dinosaur trait fields (`+0x90`, `+0xbc`, `+0x1e4`, `+0x1e8`, `+0x244`,
`+0x98`, `+0xc0`, `+0x8c`, `+0x1e0`, …) against an optional nine-int pattern.
`genotypedinos all 5` uses a null pattern (any genotype); `genotypedinos
hypsilo 2` filters by species. This is the bridge to the `.gen` data
(`docs/formats/gen.md`).

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

- Exact predicates for the trait verbs (aggressive/cute/hybrid/speed/
  stamina/clever/size-agility/hungry/isolated/ornate/pretty-strange) and the
  nine genotype fields.
- Show evaluators (happyshow/fightshow/scientificshow/happydinoshow/
  dinoexclusiveshow).
- `scaritydinos` and `carnivordinos` argument semantics.
- Difficulty variants and objective weighting.

## Evidence

- `FUN_005654f0` — `snoObjective::Load` (unknown-objective assert at `0x146`).
- `FUN_005658d0` — verb → id lookup.
- Evaluator addresses in the table above.
- `FUN_00566080`, `FUN_00566620`, `FUN_00567cb0` — shared counter helpers.
- `notes/campaign-recon.md`, `tools/formats/campaign.py`,
  `tests/test_campaign.py`.
