# Dinosaur danger grid and SECU

Status: retail mechanism decoded, validated and ported. The cache writers
(GetCourage / ObstAttackSize1) and the chooser's per-action target threat
remain.

## Retail

`FUN_00589CD0` rebuilds the danger grid **once per second** (`DAT_00675a20`
accumulator, threshold 1.0):

1. `FUN_005774F0` clears the 128 x 128 cells. Cells are 8 raw world units
   (`0..1024`, the `.wld`/`.raw` coordinate space) and 0x20 bytes each.
2. Every dinosaur in `park+0x68` calls `FUN_00571B50`: it adds `dino+0x2a0`
   to every cell in `[(x-r)>>3, (x+r)>>3] x [(y-r)>>3, (y+r)>>3]`, with
   `r = dino+0x27c`.
3. Every dinosaur calls `FUN_00571440`, which recomputes SECU (`+0x24c`)
   from the cell under its position:

```text
secu = 99 + trunc((health - max_health) * 100 / max_health)
          - trunc((grid[x>>3][y>>3] - threat) * 0.1)      // clamp >= 0
```

`health`/`max_health` are the `int16 +0x16` / `int16 +0x1d4` pair (damage
is subtracted by `FUN_005724D0`); `0.1` is the float at `0x675704`, written
by the dinosaur ctor at `0x56fec0`. Because SECU is recomputed, `.bai`
SECU need deltas are transient.

The grid is dinosaurs-only: visitors and employees do not stamp it. The
chooser has a separate per-action target term
(`threat = -cell(target) + cell(actor) - actor+0x2a0`, target from the
action policy vtable `+0x14`, radius test `FUN_00572DD0`); see
`docs/decomp/dinosaur-actions.md`.

Validated on Cpg04 against live retail data: **862/862 SECU values over 45
one-second rebuilds** match the formula, including damaged dinosaurs.
Evidence: `notes/x32dbg-secu6.out.log`, `notes/x32dbg-secu-sample.txt`,
`notes/secu-grid-validation.txt`,
`tools/recon/secu_grid_check.py`, `notes/dino-action-chooser-recon.md`.

## Port

- `src/world/danger_grid.{h,cpp}`: `DangerGrid` (`clear`, `stamp`, `at`,
  cell access) and `secu_from_grid`. The port uses the same 8-unit cells,
  the same radius box and the same truncating arithmetic.
- `src/app/main.cpp`: once per second it clears the grid, stamps every
  imported dinosaur from `DinoNeeds::threat` / `threat_radius`, and pushes
  the recomputed value through `DinoActionLoop::set_secu`. Positions are
  raw units (`ImportedDino::x/y`; the renderer subtracts
  `world_size/2`). The grid is "live" when **any** dinosaur has a nonzero
  threat/radius or `health != max_health`; then every dinosaur (including
  zero-attribute ones standing in another dinosaur's cell) gets the retail
  recompute. With every cache at 0 the port keeps the configured SECU, so
  the `--dino-needs` chooser fixtures stay meaningful; this also means
  `.bai` SECU deltas are not overwritten in that default case. `--dino-threat`,
  `--dino-radius` and `--dino-health` make the grid live. Retail leaves
  `+0x27c`/`+0x2a0` at 0 until action 26 (ObstAttackSize1) and action 13
  (GetCourage) finish, so imported dinosaurs start with the caches at 0.
  `DinoActionLoop::finish_action` now implements those writers from
  `DinoNeeds::obstacle_radius` / `combined_attack` (filled from
  `DinoAttributes`), so normal play populates the grid after the relevant
  action completes.
- CLI: `--dino-threat N`, `--dino-radius N`, `--dino-health CUR,MAX`,
  `--secu-probe` (one line per dinosaur per rebuild; it prints even when
  the grid is not live).
- The port clamps the stored need to 0..99 (`set_secu`), while retail only
  clamps at 0; with valid positions and `health <= max_health` the formula
  never exceeds 99. `at()` returns 0 outside the 128x128 grid (retail reads
  unchecked); valid 0..1024 positions never hit it. `max_health == 0` is a
  port guard (retail would divide by zero) covered by the selftest.
- `--selftest` covers stamping/radius and the formula (full health,
  threat, damage, clamp, `max_health = 0`).
- `tests/test_app_cpp.py::test_cpp_danger_grid_secu` runs Cpg04 with 20
  dinosaurs and checks every probe line against the formula, the raw
  0..1024 coordinates (`x > 512`), the half-health offset, health-only
  activation, a non-exact 70/169 ratio and the 0 clamp.
- The port save now persists the four need values, `size_cm`, the two
  danger caches (`threat`, `threat_radius`) and their phenotype sources
  (`combined_attack`, `obstacle_radius`) as an optional tail. Older saves
  omit the tail and reload with the caches at 0.
- `tests/test_app_cpp.py::test_cpp_port_save_danger_cache` covers the
  save/load round-trip.

## Remaining

- The chooser's per-action target position (policy vtable `+0x14`) and the
  visitor/obstacle reaction path.
