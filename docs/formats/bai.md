# `.bai` action definitions

Status: **header decoded; variable tail open.** 64/64 files parse, action names
match `ActionList.ini`, and the special-action group is decoded. The tail is
preserved as raw bytes until its arrays are reconstructed.

## Inventory

`Data/Design/attractions/actions/*.bai` — 64 files, 111–812 bytes.
`ActionList.ini` lists them in load order (the loader strips the `//` comment
lines and the leading count). The action index is the `ActionList.ini` line
order and is used by `saDinosaur` (`docs/decomp/dinosaur-actions.md`).

The loader is `0x0053e060`; it builds 0xac-byte runtime records and reads the
files through `FUN_00548ec0` / `FUN_00548f20`.

## Header

All files share a packed little-endian header. Several fields are not 4-byte
aligned, which is why a naive dword scan misreads the params array.

```text
0x00  char name[20]      # NUL-padded; truncated to 19 chars for long names
0x14  u32  field_14
0x18  f32  field_18
0x1c  u32  field_1c
0x20  u8   flag_20
0x21  u32  special_action   # unaligned; 0 normal, 1..6 special groups
0x25  u8   eat_flag
0x26  u32  field_26         # unaligned; 50 normal, 60 ReachWaypoint
0x2a  f32  field_2a
0x2e  f32  field_2e
0x32  u32  field_32
0x36  u32  params[8]
0x56  ...  variable tail
```

The loader reads these in order: a byte at `0x20`, a dword at `0x21`, a byte
at `0x25`, a dword at `0x26`, then the two floats. The overlapping alignment
is why a plain `u32` scan at `0x24` produces nonsense.

The two 22-character names are truncated in the file itself:
`DinoAttackForCarnivore` → `DinoAttackForCarniv`, `DinoAttackForHerbivore` →
`DinoAttackForHerbiv`. The loader matches by `ActionList.ini` order, so the
truncation is harmless.

### `special_action` — special-action group

The unaligned dword at `0x21` matches the `// Special action` groups in
`ActionList.ini`:

| Value | Group | Files |
| ---: | --- | --- |
| `0` | normal | Eat, Danse, GetArmor, … |
| `1` | exploration | Exploration |
| `2` | runaway | RunAway |
| `3` | sleep | Sleep |
| `4` | dinosaur attack | DinoAttackForCarnivore/Herbivore |
| `5` | obstacle attack | ObstAttack*, LinkObstAttack* |
| `6` | victory | Victory |

### Other header fields (observed)

| Field | Values | Working hypothesis |
| --- | --- | --- |
| `field_14` | 0, 5, 10, 12, 50, 100 | cooldown / priority |
| `field_18` | 0, 0.1, 1.0, 2.1–3.1, 10.0 | speed / animation scale |
| `field_1c` | 0–4 | variant / animation count |
| `flag_20` | 0 or 1 | `1` for Danse, Fall, ForceField, GetArmor, … |
| `eat_flag` | 0 or 1 | `1` only for Eat and EatBait |
| `field_26` | 50 (63 files), 60 (ReachWaypoint) | speed/priority |
| `field_2a` | 30.0 (63 files), 10.0 (ReachWaypoint) | radius / duration |
| `field_2e` | 30.0 in all 64 | radius / duration |
| `field_32` | 0, 800, 900, 5000, 50000 | range / score threshold |
| `params[8]` | 10–1000 | per-action weights (open) |

The `params[8]` array is the most distinctive part: `Sleep` uses
`(10,10,10,10,10,10,10,100)`, `Victory` and most actions use 100s, and attacks
use mixed values such as `(100, 30, 30, 500, 200, 100, 100, 50)`.

## Tail

The tail starts at `0x56` with a `u8` followed by several count-prefixed
arrays. The loader pushes 12-byte and 8-byte records into vectors at
`+0x58`, `+0x6c`, `+0x7c`, `+0x94` and a 0x58-byte vector at `+0xa0`; the
exact field names and order are still open.

## Reader

`tools/formats/bai.py`:

```text
python tools/formats/bai.py <actions-dir>          # scan 64 files
python tools/formats/bai.py --json Eat.bai          # one file
```

Tests: `tests/test_bai.py` (64-file scan, name truncation, group map,
Eat/Sleep/Victory/ReachWaypoint values, ActionList coverage).

## Evidence

- `Data/Design/attractions/actions/*.bai` and `ActionList.ini` (read-only).
- Ghidra: `0x0053e060` (loader), `0x00548ec0` / `0x00548f20` (record read).
- `docs/decomp/dinosaur-actions.md` — action IDs and `.bai` mapping.

## Open

- Tail arrays (animation/event records) and their 0xac-byte runtime mapping.
- Semantics of `field_14`, `field_18`, `field_1c`, `field_2a`/`field_2e`,
  `field_32` and the eight params.
- The low byte of `field_20`.
