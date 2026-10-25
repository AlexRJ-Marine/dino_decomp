# Building definition recon (`.bbi`) — 2026-10-25

## Inventory

39 files in `Data/Design/buildings/`, 802–1,396 bytes, one per
`BuildingList.ini` entry (SmallSeats … DecoCirc). 390 mesh/skel/anim paths,
all existing. 213 `CONSTRUC_*` UI names, 212 present in the interface INIs.

## Layout

- 121-byte header (30 u32 + 1 flag byte).
- 10 length-prefixed ASCII paths: skin, crash LOD1, destroy LOD1,
  construction LOD1, LOD2 x4, skel, anim.
- Tail: interleaved numeric bytes and strings (texture dir, `explo_*.ini`,
  sounds, `CONSTRUC_*` widget names). Strings are found by scanning; the
  numeric tail is the profile/cost data.

## Header table (first 8 words)

```text
name          w0  w1  w2(float) w3        w4    w5       w6     w7
SmallSeats     4  12   10.0     0x6401   2560   128000  12800   0
MediumSeats    4  12   10.0     0x6401   2560   153600  12800   0
LargeSeats     4  12   10.0     0x6401   2560   204800  12800   0
PointOfView    4   4   12.0     0x6401   2560   128000   1280   0
Security      12  12    1.0     0x138801 12800   25600      0   0
Farm          12  12    1.0     0xC801   2560   128000      0   0
Rest*         12  12    1.0     0x6401   2560   640000   7680   102400
Entrance      24  12    1.0     0x6401   2560     7680  12800   0
Manege        16  16    1.0     0x6401   2560  1280000   1280   128000
Souv*          8  12    1.0     0x6401   2560   512000   6400   51200
Barak*         4   8    1.0     0x6401   2560   128000   1280   12800
Station        4  12    1.0     0x6401   2560   512000      0   0
Tampodino     16  16    1.0     0x6401   2560  1280000   1280   128000
Haunted       16  16    1.0     0x6401   2560  1280000   2560   128000
Granderoue    16  16    1.0     0x6401   2560  1280000   1280   128000
Tower         16  16    1.0     0x6401   2560  1280000   1280   128000
Park*         12  16    1.0     0x6401   2560   128000      0   0
LunchDoor      4   4    1.0     0x6401   2560    25600      0   0
Deco*          4   4    1.0     0x6401   2560    12800      0   0
```

Hypothesis: `w0 × w1` = footprint in grid cells; `w5` = build cost;
`w3` = packed level/flags (`0x6401` = level 1, 100); `w4` = build time;
`w6/w7` = upkeep/income. Not confirmed against the UI.

## Tail

Example `BarakCirc`: `explo_1.ini`, `explo_2.ini`, `fries`,
`CONSTRUC_BARAQUES3_BUTTON`, `CONSTRUC_BARAQUES3_SMALL_PROFIL1`,
`CONSTRUC_BARAQUES3_PROFIL1_GAUGE`, … The `CONSTRUC_*` sections are the
build-panel attribute widgets for that building family.

Only `CONSTRUC_SECUANDFOOD_SECU_BUTTON` (Security.bbi) is absent from all
interface INIs; likely legacy/renamed.

## Evidence

- `Data/Design/buildings/*.bbi`, `BuildingList.ini`.
- `Data/Overlay/ConstructionInterface.Ini` — UI name resolution.
- `tools/formats/bbi.py`, `tests/test_bbi.py`, `docs/formats/bbi.md`.
