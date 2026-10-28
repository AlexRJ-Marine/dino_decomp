# Employees and staff (port)

Status: **first slice implemented** — `.bei` profiles, Nutritionist hiring,
the hourly feeding queue, wages and the visible employee actor. The retail
contract is in `../decomp/employee-ai.md` and `../decomp/economy.md`; this page
describes the port and the deliberate simplifications.

## Data

- `Data/Design/Employees/EmployeeList.ini` lists 2 names:
  `Nutritionist` (kind 0) and `Guard` (kind 1).
- `Data/Design/Employees/<Name>.bei`: 60-byte header + interleaved
  numeric/length-prefixed body. The C++ reader is `src/formats/bei.{h,cpp}`,
  a port of `tools/formats/actors.py`.
- Header fields used by the port:
  - dword 0 = kind (0 nutritionist, 1 guard);
  - float 3 = height (1.7), float 4 = walk speed (2.0 / 1.0),
    float 5 = action speed (2.5);
  - dword 8 = **500**: both the hire cost (`0x588600` -> `0x585c00`,
    generic purchase) and the daily wage (`0x58ad00` -> `0x585d80`,
    role bucket `descriptor[0]`);
  - float 10 = **3.0**, used as the feed work duration (also the
    `.bei` field-24..59 candidate documented in `../formats/bei.md`).
- Mesh/texture assets are read from the body strings; the Nutritionist uses
  `feed_standing_lod1.mesh` (115 vertices, metres) and
  `DATA/graphics/TEXTURES/buildings/feed.tga`.

## Simulation

`src/world/employees.{h,cpp}`:

- `EmployeeSystem::hire(profiles, profile_index, x, y)`.
- `start_hour(profiles, sources, defs, instant = false)` runs at the park-clock
  new-hour boundary. It queues **every** placed food source (retail pushes
  every registered `{paddock, food source}` pair) and assigns the neediest
  first to each idle Nutritionist. Guards never take jobs, matching the exe
  (`0x5366c0` hardcodes employee index 0).
- `update(dt, ...)` walks the employee at the `.bei` walk speed to the source,
  plays the 3.0 s work duration, and refills the source to its `.bsi`
  quantity. The employee stands 0.8 world units beside the source so both
  actors are visible.
- `build_mesh` bakes every employee at its current position with the standing
  pose; the viewer uploads one texture and updates the VBO per frame.

The direct retail food-level write (`FUN_00576d50(owner, hour)`) is not
reversed field-by-field; the port models its visible effect as a refill to the
source capacity.

## Viewer

- `--employee-info` prints the two profiles and exits.
- `--hire-nutritionist N` (alias `--hire-nutritionists`) hires N
  Nutritionists at the Entrance cell, charging 500 each.
- `--feed-now` runs one hourly pass instantly at startup (test/debug).
- Save round-trip: `DINOPSAV` stores `employee <profile> <x> <y>` lines and
  restores the staff on load (`port employees: N`); `--fire-nutritionist N`
  removes the last N without a refund.
- HUD: `BUDGET_EFFECTIFS_FOOD_QUANTITY_NUMBER` = live nutritionist count;
  `BUDGET_EFFECTIFS_FOOD_QUANTITY_COST_NUMBER` = yesterday role-0 staff bucket
  after a day rollover.
- The employee actor is drawn in the park; see `notes/employees.png`.

## Evidence

- Tests: `tests/test_app_cpp.py::test_cpp_employee_info`,
  `test_cpp_employee_feeding_and_wages`.
- Cpg04 + `PigFood`: hire 2 = budget 74,000; one feed assigned/completed;
  settlement wages 1,000 -> budget 73,000.
- Retail basis: `docs/decomp/employee-ai.md`, `docs/decomp/economy.md`,
  `docs/formats/bei.md`, `tests/test_actors.py`.

## Open

- Interactive hire/fire buttons (`EFFECTIFS_NUTRI_EFFECTIFS_UP1/DOWN1`); no
  park-panel action is bound to them yet.
- Guard hiring (absent from the retail exe) and the EFFECTIFS_SECU pass.
- Exact per-paddock registration (`FUN_0058b5b0`) and the 51 %/75 % work
  flags (`+0xb8/+0xc0`); the port uses a single work timer.
- Walking animation (the port renders the standing pose only).
