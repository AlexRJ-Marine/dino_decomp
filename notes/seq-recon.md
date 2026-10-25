# Animation sequence recon (`.seq`) — 2026-10-25

## Inventory

10 files in `Data/Design/animations/`, one FX + one Sound per family
(`anim_trans.dic`: HighQuadruped, LightQuadruped, HeavyBiped, LightBiped,
LowQuadruped). 1,304 events total; all 10 parse with exact byte consumption.

```text
file                     kind   lead  seq events strings
HeavyBipedFX.seq         FX       -   41   95     42
HeavyBipedSound.seq      Sound  135   41  111     42
LightBipedFX.seq         FX       -   41   96     47
LightBipedSound.seq      Sound   60   41   91     47
HighQuadrupedFX.seq      FX       -   40  176     45
HighQuadrupedSound.seq   Sound  165   40  124     45
LightQuadrupedFX.seq     FX       -   41  180     45
LightQuadrupedSound.seq  Sound   75   41  139     45
LowQuadrupedFX.seq       FX       -   40  174     44
LowQuadrupedSound.seq    Sound   82.5 40  118     44
```

## Layout

```text
<optional f32 lead>   Sound only
u32 sequence_count
u32 event_count
u32 string_count
string_count x (u32 length + ASCII/NUL)
```

Strings are shared names: sounds (`step#8`, `roar#2`), particle files
(`poussiere_deplacement_small.ini`, `KO_small.ini`), bones
(`Bip01_R_Toe0`, `Bip01_Pelvis`).

### FX

```text
seq_count x (u32 id, u32 event_count)
  event_count x 6 bytes: u16 frame, u16 type, u16 string index
  time = frame / 30
```

LightBipedFX: 41*8 + 96*6 = 904 bytes after strings (exact).
Sequence 2 events: (0, type1, string2 Bip01_R_Toe0), (15, type1, string3
Bip01_L_Toe0) -> 0.0/0.5 s.

### Sound

```text
seq_count x (u32 id, u32 event_count)
  event_count x 16 bytes: u32 type|string<<16, u32 flags, f32 time, f32 value
```

LightBipedSound: 41*8 + 91*16 = 1784 bytes after strings (exact).
Sequence 2 events: (type0, string0, time4.0, value60.0), (type15, string0,
time4.0, value60.0). Time is seconds (3.2–4.0), not frame/30.
`lead` (60/75/82.5/135/165) is family-level, meaning open.

## Loaders

- `FUN_00605420` — FX sequencer parser (`gpkFXSeqFamily.cpp`); reads counts,
  strings, per-sequence id/count, then 6-byte events.
- `FUN_00500c20` — Sound loader; 16-byte records and frame/30 conversion.
- `FUN_006000c0` creates 5 FX sequencers, `FUN_00500c20` 5 Sound ones, with
  the literal paths at `0x6707c0`.. and `0x65e1f0`...

## Evidence

- `Data/Design/animations/*.seq`.
- `tools/formats/seq.py`, `tests/test_seq.py`, `docs/formats/seq.md`.

## Open

- FX type codes, Sound type/flags/value semantics, `lead`.
- Sequence id -> animation clip name mapping.
