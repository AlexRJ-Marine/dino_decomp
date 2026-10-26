# Decoded features — port contracts

Status: **reference for the decoded-but-unported systems.** Each section gives
the retail runtime contract, the data files, what the port already has, the
first slice worth implementing, and where the evidence lives. The deep
research stays in `docs/decomp/` and `docs/formats/`; this page is the bridge
from research to implementation.

Feature status in the matrix: **decoded** (no port implementation).

## Shows, judges and the schedule

**Retail contract**

- `spScheduleManager` (`0x594b80`) owns a 9-slot timing table and a 12-entry
  show task vector. On each hour boundary `0x58c760` scans the slots; a show
  with `+0xd3 != 0` (running), `+0xe0 == 2` and `+0xe4` starts through
  `0x57b110`/`0x57b130`. `0x58c390` computes the show window; `0x58dfc0`
  builds the render records (it is not the clock).
- Score pipeline: genotype → `FUN_00618020` five phenotype floats (beauty,
  danger, spectacle, conventional appeal, tamability); per participant
  `FUN_00576550` dot with the visitor profile's five preference weights,
  clamped ±100; `FUN_00581770` takes the max over participants, adds the
  `+0x4f8/+0x4f9` bonuses, clamps ±130 and smooths at 0.07/frame. The five
  weight vectors are the **active visitor profiles' `.bvi` preferences**, not
  show data; every show uses the same profile list.
- Six judge kinds (`FUN_0057c260`): Race, Obstacle, Fight, Destruction, Team,
  Capture. Base ctor `0x582d70` (vtable `0x6433a4`); the subclass vtables
  (`0x64350c` … `0x643458`) are byte-identical and differ only by data
  (`judge_kind`, arena mesh `effects[5]`, weights). `stShowStatistics`
  (`0x57f5e0`) holds five arrays of 0xDB floats at `+0x28..+0x3c`.
- `.bfi` `+0xE8` attribute lists and `+0x104` score steps are **dead editor
  metadata**; no reader exists in the show path.

**Data** `.shw` scenario grids (43), `.bfi` features (62), `.seq` FX/sound
events, `ShowsN` mission lists, the save show catalog.

**Port status** none. `ATTRACTION_LIST_PANEL` and
`PREDEFINED_SHOW_PANEL` render static art; `SHOW_AMOUNT` is 0.

**First slice** load a `.shw`/`.bfi`, place one judge arena from `effects[5]`,
score one dinosaur from `DinoAttributes` with the active profile weights, and
show the five category scores. Schedule/live actors come later.

**Evidence** `../decomp/show-score.md`, `../decomp/park-time.md`,
`../formats/shw.md`, `../formats/bfi.md`, `../formats/seq.md`.

## Employees and staff

**Retail contract**

- `apEmployeeBehavior::Update` (`0x5220e0`) is choose → travel → work, the
  same shape as visitors. The task manager keeps a `0xc`-byte queue of
  `{owner, target, hour}` at `+0x48`.
- The only producer is `FUN_005953f0` (kind-8 task manager `+0x0c`): on the
  new-hour byte it pushes `{paddock, food source, hour}` for every registered
  pair. `FUN_0058b5b0` registers each food source with the nearest manager;
  `FUN_00595750` wakes the employees.
- Kind `0xb` is the target's `.bbi` category (a placed food source), not an
  employee job id. Work duration is `def+0x28`; `+0xb8/+0xc0` are
  available/claimed flags.
- Roles are `EmployeeList.ini` + `.bei` `kind_id`: Nutritionist 0, Guard 1.
  Only Nutritionists are hired; the hire handler `0x5366c0` hardcodes index 0.
  Guards exist only as the `EFFECTIFS_SECU` wage bucket.
- Wages use the role buckets `+0x1c/+0x20`; hire cost is the descriptor
  `[8]`.

**Data** `EmployeeList.ini`, `.bei` profiles (2), `EFFECTIFS_NUTRI_*` /
`EFFECTIFS_SECU_*` panels.

**Port status** first slice implemented (`src/formats/bei.*`,
`src/world/employees.*`): Nutritionist hiring, the hourly queue seeded from
every placed food source, walk + 3 s work + refill, daily wages in the ledger,
and a visible employee actor. The interactive `EFFECTIFS_NUTRI_*` buttons and
the save round-trip are still open. See `employees.md`.

**First slice** spawn one nutritionist, seed the hourly queue from the placed
food sources, path to the source, play the work duration and mark the source
serviced; feed the wage bucket into `BudgetLedger` at day rollover. **Done**
(`--hire-nutritionist`, `--feed-now`, `notes/employees.png`).

**Evidence** `../decomp/employee-ai.md`, `../formats/bei.md`,
`../decomp/economy.md`, `../../notes/employee-ai-recon.md`.

## Animals (`.bni`)

**Retail contract**

- Five profiles in `Data/Design/animal/` (`AnimalList.ini`): Ostrich, Sheep,
  Goat, Pig and Cow. The 48-byte header carries variant, size, limit, food
  value, quantity, food kind and a mesh list + texture directory. Food kinds
  match `.bsi` (plants/animals).
- Animals are actors (`drsAnimal`, class `0x14` in `.sav`/`.wld`) that live in
  the food chain: a carnivore eats an animal source, an animal source is a
  placed `.bni` target. The `.bni` food value/quantity are the meal values.

**Data** `.bni` (5), `AnimalList.ini`, `.bsi` food sources, save animal
records.

**Port status** `.bsi` sources and the dinosaur Eat gate work; no live animal
actors are spawned. `PigFood` is a food source, not a pig.

**First slice** spawn one animal actor at a `.bsi` animal source, let a
carnivore target it, resolve the meal (food value/quantity) and remove the
actor; then add roaming.

**Evidence** `../formats/bni.md`, `../formats/bsi.md`,
`../formats/sav.md` (world object graph), `tests/test_actors.py`.

## Laboratory gameplay

**Retail contract**

- `Laboratory.ini` has four upgrade levels: reproduction 0/1/1/1, clone failure
  100/40/20/0, beam cap 0/1/2/3, fridge 4/8/8/8 and gene-modification cap
  0/2/4/20. Three mutation beams change 3/2/2 genes at increasing radiation
  damage; eight costs cover breed/clone/mutation.
- `spLaboratory` (`0x5912d0`) reads the INI into arrays sized by
  `MaxUpgradeLevel`/`MaxBeamStrength` (`+0x14..+0x34`); the lab panels are
  `LabInterfaceBreed/Clone/Fridge/Modify.Ini`.
- Genotypes are 18 genes (3 ints per gene at runtime); the translator
  `0x6180e0`/`0x617250` produces the phenotype and the five show floats.
  Fridge slots store `{flag, genotype, name}` records (0xe8 runtime).

**Port status** panels render (`DINO_LAB_MAIN_CENTER`, `LAB_BREED_CENTER`,
`LAB_MODIFY_CENTER`, `LAB_CLONE_CENTER`, `LAB_FRIDGE_CENTER`);
`DinoAttributes` is ported, but there is no lab state machine, breeding,
cloning, beam mutation or fridge store.

**First slice** a `Laboratory` object parsed from the INI with upgrade level,
fridge slots and the three costs; breed two `.gen` genotypes and clone with
the failure percentage; wire the breed panel's validate button to the result.
Beams and gene editing come next.

**Evidence** `../formats/laboratory.md`,
`../formats/translator-constants.md`, `../formats/simu-trans.md`,
`../formats/gen.md`, `../../notes/laboratory-params-recon.md`,
`tests/test_laboratory.py`.

## Campaign objectives

**Retail contract**

- `snoObjective::Load` (`0x5654f0`) tokenises an `ObjectiveN` command, looks
  the verb up with `0x5658d0` and stores arguments at `+0x28/+0x2c`, the
  evaluator at `+0x30` and the id at `+0x34`. Evaluators return
  **0 pending / 1 success / 2 failure**.
- 33 verbs (ids 0..32). Simple evaluators: `treasury`, `marchandizing`,
  `dyingdinos`, `visitors`, `employees`, `cameraaltitude`, `delaylimit`,
  `buyalldinos`, `bankrupt`. Trait verbs use a 10-slot pattern over actor
  fields (`+0x90` cuteness, `+0xbc` aggression, `+0x1e4` hybrid, `+0x1e8`
  scarcity, `+0x244` food, `+0x98` size, `+0x7c` speed, `+0xc0` stamina,
  `+0x1e0` agility, `+0x8c` intelligence). Show verbs use the show query
  `0x567190`.
- Mission selection uses `mission+0x50` for budget/sellback/profiles;
  buildings/features/shows/store lists are difficulty-independent.

**Port status** the C++ reader (`src/formats/campaign.{h,cpp}`) parses all 7
campaigns, 39 missions, 89 objectives and the 33-verb table; CLI
`--campaign-info`/`--campaign-mission`. The evaluator
(`src/world/campaign_objectives.{h,cpp}`) implements the simple/trait verbs
and the tri-state aggregate; `src/world/campaign_runtime.{h,cpp}` evaluates
once per second and stops on success/failure. `--start-mission FILE` opens
`MISSION_PANEL` with the briefing, five objective lines and ON/OFF lights,
and shows `CPGEND_PANEL` on the result. Live dinosaurs are filled from the
imported phenotype snapshots (cuteness/beauty, aggression, size, speed,
stamina, agility, intelligence, food); hired staff fill the `employees`
objective from their `.bei` role names. Hybrid/scarcity/strangeness/ornaments
are still empty. `DINOPSAV` stores the active mission id and `--load-port`
resumes it; a successful mission advances to the next one in the campaign,
while campaign completion/ending videos remain open;
show/building/isolation/hungry verbs stay pending.

**First slice** parse the objective commands, implement the simple evaluators
against the port's park state (budget, visitors, dinos, buildings), run them
on a one-second timer and show the tri-state in `MISSION_PANEL`. All of that
is done; the next step is filling `CampaignState.dinos` from
`DinoAttributes` and wiring `CAMPAIGN_LIST`/`CAMPAIGN_START` in the menu.

**Evidence** `../formats/campaign.md`, `../../notes/objectives-recon.md`,
`../decomp/function-status.md`, `tests/test_campaign.py`.

## Why these are grouped

They all sit on the same missing layer: live park actors (show participants,
employees, animals, lab products) and a mission state machine. The port has
the park, visitors, dinosaurs, food sources, economy and UI; these five
systems are the remaining simulator side of the campaign game.
