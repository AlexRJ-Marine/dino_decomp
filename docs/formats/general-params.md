# `GeneralParams.bgd` — global park parameters

Status: **structurally solved.** 170 bytes, all fields decoded and validated
against the loader; the gameplay meaning of each field is partly inferred.

## Inventory

`Data/Design/generalparams/GeneralParams.bgd` — 170 bytes. Loaded by
`0x542f80`, the last of the ten design databases that `0x53d4d0` opens from
the config paths:

```text
0  walls           1  actions        2  features       3  buildings
4  landscape objs  5  visitors       6  employees      7  food sources
8  animal          9  general params
```

## Layout

Packed little-endian. Strings are `u32` byte count (including NUL) + bytes,
so the stream is unaligned.

```text
0x00  f32  3000.0
0x04  f32  1800.0
0x08  f32  3.0
0x0c  f32  1.5
0x10  string ""              (length 1, just the NUL)
      string "rocket"
      string "trumpet"
      string "startBlast"
      string "whitle"        (read separately, stored as one string)
      f32  8.0
      f32  120.0
      f32  40.0
      u32  10
      f32  60.0
      f32[16]  4 x 4 grid:
               3 3 3 3
               3 2 2 2
               3 2 2 2
               3 2 2 2
      f32[4]   6.0, 10.5, 14.0, 20.0
```

### Runtime offsets

The loader writes the record fields at these park-application offsets:

| File field | Runtime offset |
| --- | ---: |
| first four floats | `+0x110`, `+0x114`, `+0x118`, `+0x11c` |
| four sounds | a 4-entry string vector (`+0x140` region) |
| fifth string (`whitle`) | buffer at `+0x144`, length at `+0x14c` |
| `8.0` | `+0x154` |
| `120.0` | `+0x150` |
| `40.0` | `+0x1a0` |
| `10` | `+0x1a4` |
| `60.0` | `+0x1f8` |
| 4 x 4 grid | `+0x1a8..+0x1c4` |
| 4 floats | `+0x1e8..+0x1f4` |

## Meaning (hypotheses)

- The four sound names (`rocket`, `trumpet`, `startBlast`) plus `whitle` are
  the celebration/fireworks sound set; the first entry is empty so a show can
  suppress the launch sound.
- `3000` / `1800` are large time-like values (park day length or show
  schedule horizon in the same units as the clock).
- `3.0` / `1.5` are rate multipliers.
- `8.0`, `120.0`, `40.0`, `60.0` look like distances/heights (camera or
  visibility limits).
- `10` is an integer limit; the 4 x 4 grid and the final four values
  (`6, 10.5, 14, 20`) are a separate float block, most likely a 4-step ramp
  whose last four values are the tier maxima.

These readings are not proven; the file is small enough to verify at runtime
by watching the park-application fields.

## Reader

`tools/formats/general_params.py`:

```text
python tools/formats/general_params.py GeneralParams.bgd
```

Returns `GeneralParams` with `sounds[4]`, `whistle`, the five scalars, the
4 x 4 `matrix_1a8` and `vector_1e8`. Validates every string and requires the
file to end exactly after the last float. Tests:
`tests/test_general_params.py`.

## Evidence

- `Data/Design/generalparams/GeneralParams.bgd` (read-only).
- Ghidra: `0x542f80` (reader), `0x53d4d0` (ten-database loader).
- `tools/formats/general_params.py`, `tests/test_general_params.py`.

## Open

- Meaning of every numeric field.
- Whether the sound vector is played by the show/fireworks code or by the
  park ambience.
- What the 4 x 4 grid and the final four values index (show steps, visitor
  spending tiers, difficulty ramp).
