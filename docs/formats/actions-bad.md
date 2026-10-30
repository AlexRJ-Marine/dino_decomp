# `Actions.bad` — behaviour/action data

Status: **solved.** The file is a count plus five equal 462-float blocks. It is
opened by the translator (`FUN_00615e80` via `FUN_00417d10`) and each block is
read into a `0x81c`-byte per-family action record. The last 51 floats of every
block are the per-kind **record durations**; the 12-float tail is the last 12
durations, not an animation-scale table.

## Inventory

`Data/Design/Actions.bad` — 9,244 bytes = `4 + 5 × 462 × 4`.

The earlier "never opened" trace was wrong: `FUN_00615e80` opens it at startup
through `FUN_00417d10(path, "rb")`; the path stored at park-init `+0x38` is a
different copy. The loader is `FUN_00615e80` → reader `0x616550..0x6166c6`.

## Layout

```text
u32  count = 5
f32  block[0][462]
f32  block[1][462]
...
f32  block[4][462]
```

Per block, the reader consumes exactly 462 floats in this order:

```text
3 floats                    header / time step
51 × 2 floats               forward, lateral (one pair per kind)
51 floats                   scale (one per kind)
51 × 5 floats               per-kind params
51 floats                   duration (one per kind)   <- f[411..461]
```

Into the runtime record: `+0x0c` = the 51×2 table, `+0x354` = the scale table,
`+0x420` = the 51×5 table, `+0x1bc` = the duration table, and
`+0x288 + k*4 = 1.0 / dur[k]`. Total 12 + 408 + 204 + 1020 + 204 = 1848 bytes
= 462 floats.

`FUN_00617250` selects the record per dinosaur:
`P[0x1a8] = translator[0x3b0] + P[0x1ac] * 0x81c`; the family comes from
`simu_trans.dic` Part 10 by the legs allele, with bipeds snapped to 2/3 by
size category.

## Duration table (`f[411..461]`)

The rows below are the **file block order** (0..4). The Part 10 family index
used by `P[0x1ac]` maps to the blocks as **{0, 1, 3, 2, 4}**: family 2
(size class < 2) reads block 3 (1.2/1.5 s), family 3 (size class >= 2) reads
block 2 (2.0 s). This mapping is runtime-verified on the 20 Cpg04 dinos: their
size classes match the retail `DINORESET f1e0` values 20/20 and the measured
turns are 15 x 1.2 s (family 2) and 4 x 2.0 s (family 3); the lone family 4
uses block 4 (2.0/1.5). `--actions-bad-info` prints the **effective family**
rows (after the map).

| file block | dur[3] | dur[4] | dur[5] | dur[9] | dur[0x19] | dur[0x1c] | dur[0x27] |
|---|---:|---:|---:|---:|---:|---:|---:|
| 0 | 1.5 | 1.5 | 2.0 | 2.0 | 2.0 | 2.0 | 2.0 |
| 1 | 2.0 | 2.0 | 1.5 | 2.0 | 2.0 | 1.5 | 2.0 |
| 2 | 2.0 | 2.0 | 2.0 | 1.8 | 2.0 | 1.5 | 2.0 |
| 3 | 1.2 | 1.2 | 1.5 | 1.2 | 2.0 | 1.5 | 2.0 |
| 4 | 2.0 | 2.0 | 1.5 | 1.5 | 1.5 | 1.5 | 2.0 |

| Part 10 family | file block |
| --- | ---: |
| 0 HighQuadruped | 0 |
| 1 LightQuadruped | 1 |
| 2 (class < 2 biped) | 3 |
| 3 (class >= 2 biped) | 2 |
| 4 LowQuadruped | 4 |

The 2026-10-30 Cpg04 x32dbg probe measured exactly these turn values:
1.2 s for kinds 3/4 and 1.5 s for kind 5 on the LightBiped majority, 2.0 s on
the HeavyBiped group, and the lone kind-9 1.8 s record on a HeavyBiped.
`FUN_00575de0` uses `dur[kind]` for every kind except walk/run (kind 1/2 use
the stride formula).

## Reader

`tools/formats/actions_bad.py`:

```text
python tools/formats/actions_bad.py <Actions.bad>
```

`read_actions_bad` returns `count`, `block_size`, `blocks[5][462]`,
`durations` (`block[411:462]`) and `tails` (the last 12 floats, now known to be
durations). `src/formats/actions_bad.*` is the C++ port; `--actions-bad-info
FILE` prints the five family rows. Tests: `tests/test_actions_bad.py`,
`tests/test_app_cpp.py::test_cpp_actions_bad_durations`.

## Evidence

- `notes/dino-action-table-move-recon.md` — loader, record order, family table.
- `notes/dino-mover-runtime-probe.md` — measured 1.2/1.5/2.0 s turns.
- `Data/Design/Actions.bad` (read-only).
- `docs/formats/bai.md` — action definitions and event effects.

## Open

- The Part 10 family 2/3 -> file block swap is verified on Cpg04 (20/20 size
  classes and the measured turn groups). Families 0/1 have no Cpg04 dino, so
  their identity mapping is structural only.
- Kinds > 50 index past the 51-float table; the runtime probe saw `actk=0x39`
  with `sd=2.0`. The retail record has 519 dwords per family, so later kinds
  may read the next table or a default; not yet proven.
- The probe measured 1.4 s for kinds 0x1c/0x20 while the file says 1.5 s;
  possible `element+0x1d8` override.
- The first 411 floats are mapped structurally (forward/lateral/scale/params)
  but most per-kind semantics are still unnamed.
