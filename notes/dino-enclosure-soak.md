# Dinosaur enclosure confinement — 600 s soak

Date: 2026-10-31. Port build `build/refactor/dino_port.exe`.

## Question

Does every imported park dinosaur stay inside its paddock rect for a long
run, with the retail fixed-rect confinement (`path+0x1c0`), under the live
action loop (Exploration/Eat/Sleep) and the exact mover?

## Retail contract

- `FUN_00569e00` (behaviour ctor) calls
  `FUN_0055ac30(this+0xa4 /* path */, occupancy, *(arg1+0x1c) /* paddock rect */)`;
  `FUN_0055ac30` stores that rect at `path+0x1c0`
  (`notes/dino-normal-action-target-gesture-recon.md` §3).
- `FUN_0055af70` (the mover step guard) compares the next cell against
  `[path+0x1c0]+0x1c/+0x20/+0x24/+0x28`. Nothing re-resolves the rect from the
  live position, so a boundary cell cannot re-home the dino.
- The port now adopts the spawn paddock **once** per dino and never updates it
  from the live position (previously it re-read `paddock_at(x,y)` every tick
  and fell back to the last index only when the cell was outside all rects).
- A diagnostic (`dino escape: <species> x= y= pen=`) fires once per dino if
  its live position leaves the fixed rect by more than 0.5 units. The mover
  already refuses any step outside the inset rect (`FUN_0055af70`).

## 600 s run

```text
dino_port.exe --frames 12000 --no-menu --no-hud --width 160 --height 120
  --no-vsync --no-save-config
  --lnd game/%MAINDIR%/Data/Design/landscape/isl04/isl04.lnd
  --asset-root game/%MAINDIR% --no-vegetation --no-water
  --wld-park game/%MAINDIR%/Data/Design/Scenario/Cpg04/Cpg04_Save.wld
  --place-lunchbox --dino-action-loop --fixed-dt 0.05
  --dino-action-speed 100 --dino-needs 20,90,50,90
```

12,000 frames x 0.05 s = **600 simulated seconds** (the 10-minute playable
slice) in 56.5 s wall time.

```text
exit=0
dino escape:   0
dino move: ... blocked: 0
dino eat:      1
dino explore: 20
dino sleep:   20 (start lines)
dino action:  41
```

Every one of the 20 imported Cpg04 dinosaurs adopted a paddock on the first
tick and stayed inside it; the mover never exhausted a blocked step.

## Automated check

`tests/test_app_cpp.py::test_cpp_dino_confinement_soak` runs 2,400 frames x
0.05 s = 120 s on the same park and asserts:

- exit 0,
- no `dino escape:` line,
- no `dino move: ... blocked` line,
- at least one `dino action:` and one `dino explore:` line (the loop ran).

Evidence: this note; `build/soak600.log` (gitignored).

## Rest-gauge build (2026-10-31, later that day)

Same 600 s command after the exact rest gauge was wired. The action-speed
fixture (100) makes Exploration 0.3 s, so FOOD/REPOS stay high and Sleep is
not selected; the confinement result is unchanged:

```text
exit=0
dino escape:   0
dino move: ... blocked: 0
dino eat:      1
dino explore: 20
dino sleep:    0
dino action:  21
```

The gauge itself is verified separately: `test_cpp_dino_rest_gauge_sleep`
(Cpg04, REPOS 10) shows the gauge climbing to max and REPOS 99 through the
0x1e instant sleep sentinel.
