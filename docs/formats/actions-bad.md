# `Actions.bad` ? behaviour/action data

Status: **structurally solved; semantics partly hypothesised.** The file is a
count plus five equal float blocks; each block ends in 12 animation-scale
values.

## Inventory

`Data/Design/Actions.bad` ? 9,244 bytes = `4 + 5 ? 462 ? 4`.

It is the `DATA\Design\Actions\bad` path stored by the park init
(`0x0051d1d0`, field `+0x38`). No loader has been located yet, so the reader
is structural.

## Layout

```text
u32  count = 5
f32  block[0][462]
f32  block[1][462]
f32  block[2][462]
f32  block[3][462]
f32  block[4][462]
```

The five blocks are equal and each has 462 floats (5 ? 462 = 2,310).

## What the values look like

- Block starts: a small time step (`1/410`, `0.0053`, `0.005`, `0.0189`,
  `0.0103`), then two large coordinates (e.g. `463.65, 522.85`), zeros, and a
  value around `0.7..2.3`.
- The middle is dominated by near-zero values (direction/rotation-like).
- The **last 12 floats of every block** are playback-scale values in
  `0.8..2.0` ? exactly the range and granularity of the `.bai` `field_18`
  animation scale (`1.0`, `1.2`, `1.5`, `1.7`, `2.0`, ?).

## Hypothesis

The five blocks match the five animation families
(`HighQuadruped`, `LightQuadruped`, `HeavyBiped`, `LightBiped`,
`LowQuadruped`), and the trailing 12 values are per-action playback speeds.
The leading coordinate pairs are probably stride/path data.

**Runtime result:** a full x32dbg trace from startup through Free Game start
and loading `slot001.sav` (which spawns a dinosaur) logs every `CreateFileA/W`
call. `Data/Design/Actions.bad` is **never opened**. The path is only stored
at park-init `+0x38`; the retail runtime has no consumer for it. Treat the
file as editor/demo data, not a live gameplay table.

## Reader

`tools/formats/actions_bad.py`:

```text
python tools/formats/actions_bad.py <Actions.bad>
```

`read_actions_bad` validates the block division and returns `count`,
`block_size`, `blocks[5][462]` and `tails` (the last 12 floats per block).
Tests: `tests/test_actions_bad.py` (5 ? 462, spot starts/tails, tail range,
exact size).

## Evidence

- `Data/Design/Actions.bad` (read-only).
- `docs/formats/bai.md` ? the `field_18` animation-scale value set.
- `tools/formats/actions_bad.py`, `tests/test_actions_bad.py`.

## Open

- The real meaning of the first 450 floats per block (editor/demo data; no
  retail loader).
- Whether the blocks are families in the `anim_trans.dic` order.
- Whether the 12 tail values index action ids 0..11 or another action list.
