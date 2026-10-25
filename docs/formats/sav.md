# `.sav` save format

Status: **header, show catalog, simulation-state prefix and the AI/camera tail
are solved; world object contents remain partial.**
The retail save is a sequence of manager-level serializers, not one fixed
struct.

## Inventory

- `run/save/slot001.sav` — 7,376 bytes, manual save, name `agenttest`.
- `run/save/auto/auto.sav` — 7,116 bytes, autosave.
- `run/save/auto/slot001.sav` — 7,116 bytes, identical to `auto.sav`.
- `run/save/dino.dat` — 4 bytes, `01 00 00 00`.

## File layout

```text
0x00  u32   version / flags (53 in both retail saves)
0x04  u32   slot/index (0)
0x08  u32   save-name byte length (NUL included)
0x0c  ...   UTF-16LE save name
...         campaign manager: 2 x u32 (0, 0 in retail saves)
...         world manager: object lists + show manager
...         simulation manager: road network, visitors, park, stores
...         AI manager
...         scenario manager
```

The wrapper functions establish the order:

| Direction | Wrapper | Campaign | World | Sim | AI | Scenario |
| --- | --- | --- | --- | --- | --- | --- |
| save | `0x0051e7e0` | `0x0055d870` | `0x0053c9d0` | `0x00586b30` | `0x0051f400` | `0x005e5940` |
| load | `0x0051e6c0` | `0x0055d710` / `0x0055d980` | `0x0053c560` | `0x00586ad0` | `0x0051f290` | `0x005e5920` |

Stream methods: `[vtable + 4]` writes, `[vtable + 8]` reads.

The show catalog is serialized **last** in the world-manager section by the
show manager writer `0x00558340` (reader `0x00558230`; constructor
`0x00557a30`, path `DATA\design\scenario\show\`). That is why the show catalog
can be located before the simulation section.

## Show catalog

Locate the first `DATA\design\scenario\show\` path and read the count **8 bytes
before it** (the count is followed by the first path length).

```text
u32   path byte length (NUL included)
...   ASCII path, e.g. DATA\design\scenario\show\Cpg1_Race_1.shw
u32   name byte length (NUL included, UTF-16)
...   UTF-16LE display name
6 x u32 parameters
```

Both retail saves contain 39 records. Example:

```text
The Crazy Course   params 1800, 4, 1, 19, 39, 1
The Ultimate Race  params 42000, 4, 256, 51, 39, 2
```

Parameter meanings remain open.

## Simulation manager section

This is the blob returned by `sav.read_sav().state` (immediately after the show
catalog). Version 53:

```text
u32 road_count
road_count x (s32 x0, s32 y0, s32 x1, s32 y1)

u32 list0_count              # 0 in retail
u32 list1_count              # 0
u32 list2_count              # 0

u32 visitor_count
visitor_count x 0x50-byte visitor

u32 park_count               # 1
park_count x 0x293-byte park block

u32 dino_store_count         # 20
dino_store_count x record:
    u32 name_bytes (UTF-16LE, NUL included)
    UTF-16LE name
    u32 store_id / price
    u32 path_bytes (ASCII, NUL included)
    ASCII path
    u32 description_bytes (ASCII, NUL included)
    ASCII description

u32 visitor_model[5]         # 3,3,3,3,3 in retail
u32 lab_count                # 8
lab_count x u8 present_flag  # 1 = empty; 0 = variable genotype data
u32 ai_counts[5]             # AI lists; fifth has the entries
if non-auto: entries x 17 bytes
f32 camera[10]               # camSwitcher state (40 bytes)
```

Retail offsets inside `state`:

| File | road | visitor count | park count | park block | dino store | lab | AI + camera |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `slot001.sav` | `0x0000` | `0x0020` | `0x00c4` | `0x00c8` | `0x035b` | `0x0b65` | `0x0b71` (94 B) |
| `auto/auto.sav` | `0x0000` | `0x0020` | `0x0024` | `0x0028` | `0x02bb` | `0x0ac5` | `0x0ad1` (60 B) |

The retail saves have one road rectangle `(524, 392) - (524, 452)`. The three
object lists are empty. The manual save has two visitors and a budget of
75,125; the autosave has none and a budget of 75,000.

### Visitor record (`0x50` bytes)

```text
+0x00 u32 object id         4 / 6 in the manual save
+0x04 s32 tile x            524
+0x08 s32 tile y            433 / 393
+0x0c u32 flags             0x00010001
+0x10 u32 unknown           0xbaadf00d when unset
+0x14 u32 state             1
+0x18 f32 x
+0x1c f32 y
+0x20 f32 direction x
+0x24 f32 direction y
+0x28 u32 unknown
+0x2c u32 zero
+0x30 u32 visitor id         38 / 39
+0x34 u32 needs[7]
```

The first dword is an object-registry ID, not the personality profile. The
seven needs are the visitor satisfaction/status values; their semantic names
are open.

### Park block (`0x293` bytes)

`FUN_00585240` (park reader) starts at `block + 0x28`; the first `0x28` bytes
are an object header.

```text
relative +0x08   u32  budget (75,000 / 75,125 in retail)
relative +0x0c   f32  1.0
relative +0x10   u32
relative +0x14   u32
relative +0x18   u32
relative +0x1c   u32
relative +0x20   u32  125 manual / 0 auto (total visitors or income)
relative +0x24   13 x u32; first is 2 manual / 0 auto
relative +0x58   13 x u32
relative +0x94   2 x u8 (version-gated)
relative +0x98   version > 52 field
...
```

`FUN_00585bd0` (park constructor setter) writes the same `+0x08` budget and
`+0x98` field, confirming the budget offset.

### Dinosaur store

The 20 records are genotype references, not the gene values:

```text
Allosaurus         9000   DATA\Design\Scenario\Genotype\allo.gen
Ankylosaurus       5500   DATA\Design\Scenario\Genotype\anky.gen
Brachiosaurus     42000   DATA\Design\Scenario\Genotype\brachio.gen
...
Velociraptor      15000   DATA\Design\Scenario\Genotype\velo.gen
```

The `store_id` values look like purchase prices, but that is still a
hypothesis. The actual 18-gene table is loaded from the `.gen` file when the
record is read (`FUN_00593a60` → `FUN_00595f40` → `FUN_00584c20`).

### Lab store

`FUN_00592060` reads five u32 (`3,3,3,3,3` in retail), then `FUN_00592a00`
reads `lab_count = 8` and one presence byte per record. All eight flags are
`1` (empty). A zero flag is followed by a variable genotype record whose exact
layout is not decoded.

## World manager section

`FUN_0053c9d0` (save) / `FUN_0053c560` (load) serialize several lists of world
objects plus the show manager. Each list is `u32 count` followed by objects
that use their own `vtable + 0x4c` serializer. `FUN_0053a140` registers seven
serializer classes:

| Class ID | Constructor | Size | Read | Write |
| ---: | --- | ---: | --- | --- |
| `0x12` | `0x005e8e30` | `0x11c` | `0x005e8ef0` | `0x005e8f80` |
| `0x13` | `0x005e8c60` | `0x3c` | `0x005e8cf0` | `0x005e8d60` |
| `0x14` | `0x005e8b40` | `0x34` | — | — |
| `0x15` | `0x005e8a00` | `0x30` | — | — |
| `0x16` | `0x005e74f0` | `0x70` | — | `0x005e7860` |
| `0x17` | `0x005e87a0` | `0x5c` | — | `0x005e8910` |
| `0x18` | `0x005e8590` | `0x34` | — | `0x005e86a0` |

Class `0x12` carries an 18-gene genotype table (`FUN_00584ed0` /
`FUN_00585100`). The other class names and the object-list order are open.

### AI manager + camera tail

After the lab flags:

```text
u32 ai_list_count[5]        # first four are 0 in retail
if non-auto:
    ai_list_count[4] x 17-byte AI entries
f32 camera[10]              # 40 bytes
```

Manual: `(0,0,0,0,2)`, two 17-byte entries, camera `(550.998, 512, 84.696,
512, 512, 32, 0.1, 0, 0, 0)`.
Auto: `(0,0,0,0,0)`, no entries, camera `(128, 128, 250, 512, 512, 50, 0.1,
0, 0, 0)`.

The AI manager uses `0x0051f290` (load) / `0x0051f400` (save). The camera
block is the `camSwitcher` serializer (`0x005e5b80` / `0x005e5ba0`); the exact
field split of the ten floats remains open.

## Reader

`tools/formats/sav.py` — header and show catalog:

```text
python tools/formats/sav.py run/save/slot001.sav --shows
```

`tools/formats/sav_graph.py` — simulation-state prefix:

```text
python tools/formats/sav_graph.py run/save/slot001.sav --visitors --dinos
```

`sav_graph` returns the road rectangles, visitors, park budget/fields, the
20 dinosaur-store records, the five visitor-model values, the lab flags, the
five AI-list counts, the 17-byte AI entries and the 10 camera floats. Tests:
`tests/test_sav.py`, `tests/test_sav_graph.py`.

## Evidence

- `notes/save-graph-recon.md` — decompiled function map, offsets and field
  evidence.
- `notes/save-recon.md` — original header/catalog recon.
- Ghidra: `FUN_0051e6c0`, `FUN_0051e7e0`, `FUN_0053a140`, `FUN_0053c560`,
  `FUN_0053c9d0`, `FUN_00558340`, `FUN_00585240`, `FUN_00585bd0`,
  `FUN_0058aed0`, `FUN_0058ccc0`, `FUN_00592060`, `FUN_00592a00`,
  `FUN_00593a60`.

## Open

- World object list semantics and class names.
- Park fields beyond budget; the `0x28`-byte block header.
- Dinosaur `store_id` meaning.
- The AI/camera field split; the byte layout is solved.
- Populated lab genotype records.
- Show parameter semantics.
