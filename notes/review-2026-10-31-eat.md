# Review - Eat/refill slice (2026-10-31)

Independent review of commits `2ca3834`, `6b0dbe9`, `a08f57f`, `904cdc5`,
`0b530d7`, `f00942d`. The reviewer reproduced the Cpg04 Eat at action speeds
10 and 1, the employee feed and the dino pick, and read the `.bai` fields
independently.

## Blocker (fixed)

**`DinoActionLoop::finish_current()` made Eat unbounded.** After the arrival
meal the action ended immediately; the chooser re-selected Eat and the dino
drained a whole LunchBox in seconds (measured: 334 meals / 14.4 s at action
speed 10; 214 / 19.2 s at speed 1). `eat_applied` only guarded one action
instance.

Fix: remove `finish_current()` (and the method). The meal still fires once at
arrival + mover idle; the `.bai field_2e` cap ends the action. Cpg04 now logs
one meal in 900 frames at speed 10 (`quantity=9970`), and
`test_cpp_lunchbox_eat` asserts `1 <= meals <= 6` to catch a regression.

## Should-fix

- **Stand-cell cache tied to the feeder (fixed).** `ImportedDino::eat_feeder`
  stores the feeder vector index; a different nearest feeder invalidates
  `eat_stand_x/y`.
- **Port-save phenotype lost (fixed).** `--load-port` re-created dinos with
  `attributes = nullptr`, so `part_length[3]`/`attack_head`/`size_radius`
  were 0 and the Eat gate/bite became zero. `DINOPSAV` now appends
  `size_class`, `size_radius`, `body_length_cm`, `attack_head`, `length_cm`,
  `height_cm` after the id (backward-compatible optional tail), and the load
  restores them. `test_cpp_port_save_dino_state` asserts
  `dino info size: class=1 radius=2 body=226 height=234 length=606 head=30`
  after a load.
- **Stale claims (fixed).** `park_session.cpp` (stand cell is implemented),
  `dino_actions.h` (meal at arrival, not finish), `employees.h` (level
  producer solved), `notes/lunchbox-eat-recon.md`,
  `notes/dino-food-gauge-recon.md`, `docs/platform/feeding.md` (stand cell
  457,321, not the bowl centre), `notes/dino-eat-gate-recon.md` (no early
  end; d² wording).
- **Test coverage (fixed).** Cadence assertion as above; the
  `remaining=5.000 level=0` integration assertion remains weak (both true at
  placement); the 100/66/33 and level-3 rejection are pinned by the
  `set_food_level` selftest instead.
- **Mover-idle condition.** The port clears the mover before testing
  `state() == 0`, so the retail wait for `actor+0x4b4 == 0` is forced rather
  than observed. The element/meal timing note records this.
- **Stand-cell paddock rect (open).** Candidates are not constrained by the
  `set_bounds` rect; a feeder near a fence can yield a cell the mover refuses.
  Retail's `0x0055af70` rect test may or may not be part of the stand-cell
  predicate; not changed without evidence.

## Nits

- `finish_action` formatting fixed.
- `eat_arrival_radius` modes 1/2 return the base radius; only modes 3/4 occur
  in the Eat family (marked in the header).
- `--no-build` is order-dependent (a later `--build*` re-enables build mode).
- `game_input.cpp` `else` now runs paddock selection on every non-build click
  (needed for the pick test; low risk).
- `make_shared<const Mesh>` per dino per frame is a picking copy; a reusable
  buffer would avoid it.
- `dino_pick.cpp` uses `std::max` without `<algorithm>` (transitive today).

## Verification

- Previous full suite: **537 passed** (8 warnings, 5m10s) on the fence build
  before this review fix.
- After the fix: targeted `port_save_dino_state`, `port_save_danger_cache`,
  `lunchbox_eat` pass; fast suite **384 passed**; `verify -Fast` green.
- A fresh full suite is the remaining gate for the blocker/save changes.
