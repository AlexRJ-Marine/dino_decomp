# `.seq` animation event sequences

Status: **solved** for both layouts (FX and Sound); event type codes and the
Sound value/flags are partial.

## Inventory

Ten files in `Data/Design/animations/`, one FX and one Sound per animation
family (`anim_trans.dic`: HighQuadruped, LightQuadruped, HeavyBiped,
LightBiped, LowQuadruped):

| Family | FX seq/events/strings | Sound lead | Sound seq/events/strings |
| --- | --- | ---: | --- |
| HeavyBiped | 41 / 95 / 42 | 135.0 | 41 / 111 / 42 |
| LightBiped | 41 / 96 / 47 | 60.0 | 41 / 91 / 47 |
| HighQuadruped | 40 / 176 / 45 | 165.0 | 40 / 124 / 45 |
| LightQuadruped | 41 / 180 / 45 | 75.0 | 41 / 139 / 45 |
| LowQuadruped | 40 / 174 / 44 | 82.5 | 40 / 118 / 44 |

## Common layout

```text
<optional f32 lead>     Sound files only
u32 sequence_count      (40/41)
u32 event_count         (91..180)
u32 string_count        (42..47)
string_count x (u32 length + ASCII/NUL)
```

The strings are shared names: sounds (`step#8`, `roar#2`), particle files
(`poussiere_deplacement_small.ini`, `KO_small.ini`), and bones
(`Bip01_R_Toe0`, `Bip01_Pelvis`). FX events reference bones/particles; Sound
events reference sound/particle names.

## FX records

```text
sequence_count x (u32 id, u32 event_count)
  event_count x 6 bytes:
    u16 frame
    u16 type
    u16 string index
    time = frame / 30
```

The sizes match exactly: LightBipedFX `41*8 + 96*6 = 904` bytes after the
string table.

Example (LightBipedFX sequence 2): `(frame 0, type 1, string 2)`,
`(frame 15, type 1, string 3)` — the right/left toe bones at 0.0/0.5 s.
Sequence ids are animation-sequence ids (0..40), not clip names.

## Sound records

```text
sequence_count x (u32 id, u32 event_count)
  event_count x 16 bytes:
    u32 type (low 16) / string index (high 16)
    u32 flags
    f32 time
    f32 value
```

Size check: LightBipedSound `41*8 + 91*16 = 1784` bytes after the table.
Example (sequence 2): `(type 0, string 0, time 4.0, value 60.0)` and
`(type 15, string 0, time 4.0, value 60.0)`. Frame timing is in **seconds**
here (3.2–4.0), unlike FX's frame/30. The `lead` float (60/75/82.5/135/165)
is family-level; meaning open.

## Reader

`tools/formats/seq.py`:

```text
python tools/formats/seq.py --dump <file.seq>
python tools/formats/seq.py --scan <animations-dir>
```

Validates the exact byte consumption per file. Tests: `tests/test_seq.py`
(10-file scan, FX/Sound examples, string indices, counts).

## Evidence

- `Data/Design/animations/*.seq` (read-only).
- `FUN_00605420` — FX sequencer parser (`gpkFXSeqFamily.cpp`): reads counts,
  strings, then per-sequence id/count and 6-byte events.
- `FUN_00500c20` — Sound loader (frame/30, 16-byte records).
- `FUN_006000c0` — creates the five FX sequencers; `FUN_00500c20` the five
  Sound ones.
- `anim_trans.dic` — family mapping (20 dinos → 5 families).
- `tools/formats/seq.py`, `tests/test_seq.py`.

## Open

- FX `type` codes (1, 3, 15, …) and Sound `type`/`flags` semantics.
- Sound `value` (48–60) and the `lead` float.
- Sequence id → animation clip name mapping.
- Whether Sound `time` is absolute seconds or an offset.
