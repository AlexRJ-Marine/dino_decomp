# `.bbi` building definition

Status: **asset/UI mapping solved; header numbers partial.** 39/39 files parse,
390 mesh/skel/anim paths exist, 213 build-panel UI references (212 resolve).

## Inventory

`Data/Design/buildings/*.bbi` — 39 files, 802–1,396 bytes. `BuildingList.ini`
lists them in order (SmallSeats … DecoCirc). One file per build-menu entry.

## Layout

```text
0..120   121-byte header (30 x u32 + 1 flag byte)
121..    ten length-prefixed ASCII paths:
           skin, crash LOD1, destroy LOD1, construction LOD1,
           LOD2 x4, skel, anim
...      tail: interleaved numeric bytes and length-prefixed strings:
           texture directory, explosion .ini, sounds, CONSTRUC_* UI names
```

The ten paths are exactly the building's actor assets. The tail's strings are
found by scanning (they are interleaved with numeric profile data); all 390
mesh paths exist in the retail tree.

## Header (observed)

| Word | Example | Likely meaning |
| ---: | --- | --- |
| 0 | 4, 8, 12, 16, 24 | footprint width in grid cells |
| 1 | 4, 8, 12, 16 | footprint height in grid cells |
| 2 | 1.0, 10.0, 12.0 (float) | build/animation scale? |
| 3 | `0x6401`, `0x138801`, `0xC801` | packed flags/level/price byte |
| 4 | 2560, 12800 | construction time/ticks? |
| 5 | 7,680 … 1,280,000 | **build cost** (likely) |
| 6 | 0 … 102,400 | upkeep/income? |
| 7 | 0 … 128,000 | income/capacity? |
| 8..29 | flags/floats | not decoded |

Footprint examples: `Deco*` 4×4, `Barak*` 4×8, `SmallSeats` 4×12,
`Souv*` 8×12, `Rest*` 12×12, `Manege`/`Tampodino`/`Haunted`/`Tower` 16×16,
`Park*` 12×16, `Entrance` 24×12. Cost examples: `Entrance` 7,680,
`Deco*` 12,800, `SmallSeats` 128,000, `Rest*` 640,000, rides 1,280,000.

These are inferred from consistent patterns, not confirmed against the
construction UI; treat cost/footprint as the parity hypothesis.

## Tail strings

- Texture directory: `Data\Graphics\TEXTURES\buildings`.
- Explosion files (`explo_1.ini`, `explo_2.ini`).
- Sound names (`fries`, `boat`, …).
- Build-panel widget names: `CONSTRUC_<FAMILY>_BUTTON`,
  `CONSTRUC_<FAMILY>_SMALL_PROFILn`, `CONSTRUC_<FAMILY>_PROFILn_GAUGE`.
  These are the sections the construction panel uses for the selected
  building's attributes.

213 UI names across the 39 files; 212 exist in the interface INIs. The one
missing is `CONSTRUC_SECUANDFOOD_SECU_BUTTON` for `Security.bbi` (probably a
legacy/renamed section; the port should fall back to the file name).

## Reader

`tools/formats/bbi.py`:

```text
python tools/formats/bbi.py --dump <file.bbi>
python tools/formats/bbi.py --scan <buildings-dir>
```

Returns header words/flag, the ten mesh paths, tail strings and the numeric
tail. Tests: `tests/test_bbi.py` (39-file scan, Barak/Entrance values, all
paths, UI resolution, BuildingList mapping).

## Evidence

- `Data/Design/buildings/*.bbi`, `BuildingList.ini` (read-only).
- `Data/Overlay/ConstructionInterface.Ini` — UI name resolution.
- `tools/formats/bbi.py`, `tests/test_bbi.py`.

## Open

- Exact header fields (cost/footprint hypothesis, words 2/3/4/6/7 and 8–29).
- Numeric tail (attribute profile values for the `CONSTRUC_*_PROFIL_GAUGE`
  bars).
- The missing `Security` UI section.
- How `word0/word1` map to the 4-unit build grid.
