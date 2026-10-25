# Visitor profile recon (`.bvi`) — 2026-10-25

## Inventory

`Data/Design/visitors/` — 24 files, 2.0–2.1 KB each, 10 personality types ×
Man/Woman (Kid has 3 variants each).

`VisitorList.ini`:

```text
// Total number of profiles
10
//ProfilType=0 SURFER      2 SurferMan, SurferWoman
//ProfilType=1 HOOLIGAN    2 HooliganMan, HooliganWoman
//ProfilType=2 PARENTS     2 ParentMan, ParentWoman
//ProfilType=3 BLASES      2 BlaseMan, BlaseWoman
//ProfilType=4 RETRAITE    2 OldMan, OldWoman
//ProfilType=5 RICHE       2 RicheMan, RicheWoman
//ProfilType=6 SCIENTIFICS 2 ScientistMan, ScientistWoman
//ProfilType=7 AMOUREUX    2 FanMan, FanWoman
//ProfilType=8 SPD         2 SpdMan, SpdWoman
//ProfilType=9 ENFANTS     6 KidMan, KidWoman, KidMan2, KidWoman2, KidMan3, KidWoman3
```

## Structure

- 96-byte header (24 × 32-bit).
- 28 length-prefixed ASCII strings (`DATA\...`): 26 mesh paths, texture
  directory, texture file.
- Tail 91–113 bytes, mixed binary, undecoded.

Header offsets:

```text
 0 u32 personality id
 4 u32 1
 8 u32 1
12 f32 height 1.7
16 f32 walk speed
20 u32 10
24 u32 2
28 5 x f32 preferences (-1..1)
48 12 x u32 header tail (576, 2.0, 1.0,1.0,1.0, 500, 300, -3000, 3000, 0,0,2)
```

## Personality table

```text
id name        speed  preferences
0  Surfer      2.5    0.0, 0.5, 1.0, -0.2, 0.0
1  Hooligan    2.2   -0.5, 1.0, 0.6,  0.0, 0.0
2  Parent      2.0    0.5, -0.8, 0.6, 0.2, 0.3
3  Blase       1.5    0.0, 0.3, 0.5, -0.5, 0.2
4  Old         1.2    0.5, -1.0, 0.1, 0.3, 0.7
5  Riche       2.0    0.3, 0.3, 0.5, -0.2, 0.3
6  Scientist   1.8    0.0, 0.0, 0.5, 0.6, 0.5
7  Fan         2.0    0.7, 0.0, 0.4, -0.6, 0.4
8  Spd         2.0    0.4, -0.5, 0.4, 0.0, 0.4
9  Kid         2.5    0.6, 0.0, 0.5, 0.0, 0.4
```

Man/Woman identical stats; only mesh/texture differ.

## Mesh table (26 per file)

```text
standing LOD1, standing LOD2
walk1 LOD1, walk1 LOD2
walk2 LOD1, walk2 LOD2
seated LOD1 x2
run1/2/3 LOD1 x2 each
happy1/2/3 LOD1 x2 each
unhappy1/2/3 LOD1 x2 each
```

The duplicates are two animation slots per state, not duplicate data. All
624 referenced mesh paths exist in the retail tree.

## Evidence

- `game/%MAINDIR%/Data/Design/visitors/*.bvi` (read-only).
- `VisitorList.ini` (same directory).
- `tools/formats/bvi.py`, `tests/test_bvi.py`, `docs/formats/bvi.md`.

## Open

- The five preference weights (likely attraction categories).
- Tail bytes (needs/rates).
- Spawn probabilities per personality.
