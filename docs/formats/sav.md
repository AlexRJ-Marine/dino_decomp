# `.sav` save format

Status: **header, world-object graph, show catalog, simulation-state prefix,
lab genotype store and the AI/camera tail are solved; some simulation
semantics remain.**
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
...         12 reserved zero bytes
...         world manager: campaign ids, objectives, object lists,
            unlock arrays, show catalog
...         simulation manager: road network, visitors, park, stores
...         AI manager
...         scenario manager
```

The C++ header reader is `src/formats/sav.cpp` (`read_sav_header`); the
viewer scans `run/save/*.sav` and prints/renders the names.

The world block starts at **`0x18 + name_len`**:

| Save | `name_len` | World start | Show count | Show catalog |
| --- | ---: | ---: | ---: | ---: |
| `slot001.sav` | 20 | `0x2c` | `0x171` | 39 records |
| `auto/auto.sav` | 2 | `0x1a` | `0x12f` | 39 records |

The shared world-object layout (class table, fence/show/building/visitor/
dinosaur records, objective list and unlock arrays) is documented in
`docs/formats/wld.md` and parsed by `tools/formats/wld.py`.

The wrapper functions establish the order:

| Direction | Wrapper | Campaign | World | Sim | AI | Scenario |
| --- | --- | --- | --- | --- | --- | --- |
| save | `0x0051e7e0` | `0x0055d870` | `0x0053c9d0` | `0x00586b30` | `0x0051f400` | `0x005e5940` |
| load | `0x0051e6c0` | `0x0055d710` / `0x0055d980` | `0x0053c560` | `0x00586ad0` | `0x0051f290` | `0x005e5920` |

Stream methods: `[vtable + 4]` writes, `[vtable + 8]` reads.

## World manager section

The campaign manager writes two u32 ids (`0x0055d870`; `-1, -1` in free game).
`0x0053c560` then reads:

```text
u32 objective_count              # 6 in retail
objective_count x (u32 kind, u32 value, u32 target)

u32 fence_count     + drsFence records
u32 show_count      + drsShow records
u32 animal_count    + drsAnimal records
u32 employee_count  + drsEmployee records
u32 building_count  + (drsBuilding, u32 link) records
u32 visitor_count   + drsVisitor records
u32 dinosaur_count  + drsDinosaur records (18-gene genotype)

u32 feature_flags (62 bytes)
u32 building_flags (39 bytes)
u32 food_flags (10 bytes)
u32 misc_flags (6 bytes)

u32 show_catalog_count
show_catalog_count x show record
```

The class ids `0x12`–`0x18` and every record layout are in
`docs/formats/wld.md`. Retail free-game saves contain one `Entrance` building
at `(520,376)`, six default objective records, and all unlock bytes set to 1.
`slot001.sav` has two `drsVisitor` objects; `auto.sav` has none.

The show catalog is serialized **last** in this section by the show manager
writer `0x00558340` (reader `0x00558230`; constructor `0x00557a30`, path
`DATA\design\scenario\show\`). That is why the catalog can be located before
the simulation section.

## Show catalog

Locate the first `DATA\design\scenario\show\` path and read the count **8 bytes
before it** (the count is followed by the first path length).

```text
u32   path byte length (NUL included)
...   ASCII path, e.g. DATA\design\scenario\show\Cpg1_Race_1.shw
u32   name byte length (NUL included, UTF-16)
...   UTF-16LE display name
6 x u32 parameters (fully decoded, see docs/formats/shw.md):
  0 Cost
  1 constant 4
  2 cage-size flags packed as 4 bytes: sum(cage_size[i] << (8*i))
  3 arena width
  4 arena height
  5 judge kind
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
    u32 price                 # store price, matches FreeGame.ini [DINOSTORE]
    u32 path_bytes (ASCII, NUL included)
    ASCII path
    u32 description_bytes (ASCII, NUL included)
    ASCII description

u32 visitor_model[5]         # 3,3,3,3,3 in retail
u32 lab_count                # 8
lab_count x record:
    u8  present_flag         # 1 = empty; 0 = genotype + name follow
    if present_flag == 0:
        18 x (u32 allele_a, u32 allele_b)  # 0x90-byte genotype
        u32 name_bytes (ASCII, NUL included)
        ASCII name
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

This is the **simulation** visitor record. It is separate from the
`drsVisitor` world object in the world manager section.

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
seven needs are the visitor satisfaction/status values; the first three are
the runtime `+0x48/+0x4c/+0x50` needs (e.g. `slot001.sav` 97/96/99 and
99/98/99), the remaining four are 99 in retail saves. The C++ reader
`read_sav_simulation` exposes them as `SavVisitor.needs`.

### Park block (`0x293` bytes)

The block has a **0x30-byte header**, then the economy field stream written by
`0x00585780` and read by `0x00585240`. The stream is **not** a memory dump:
fields appear in writer order, so the file offsets differ from the runtime
object offsets (`docs/decomp/economy.md`). Stream offset 0 is `block + 0x30`.
The stream is **0x21a bytes** long, ending at `block + 0x24a`; the remaining
`0x49` bytes are the schedule-manager tail, a 12-byte park field and one
trailing byte.

```text
block  +0x000  u32  count / class tag (2; `FUN_0058aed0` reads it as a count)
block  +0x004  u32  header a: 0 in the manual save, 3 in the autosave
block  +0x008  u32  header b: 100 in both retail saves
block  +0x00c  f32  header c: 10.0 in the manual save, 0.0 in the autosave
block  +0x010  32 B header reserved (all zero in retail saves)

stream +0x000  u32  budget (75,000 / 75,125 in retail)
stream +0x004  f32  sellback factor (version >= 53)
stream +0x008  u32  FOOD total
stream +0x00c  u32  FOOD count
stream +0x010  u32  EFFECTIFS_SECU total
stream +0x014  u32  EFFECTIFS_SECU count
stream +0x018  u32  ticket income total (125 in the manual save)
stream +0x01c  u32  ticket income count (2 in the manual save)
stream +0x020  2 x u8  bankruptcy flags
stream +0x022  2 x u32  staff expense buckets
stream +0x02a  13 x u32  shop revenue, kind 0..12
stream +0x05e  13 x u32  shop sale counts, kind 0..12
stream +0x092  u32  DINOACHAT count / total
stream +0x09a  u32  CONSTRUCTION count / total
stream +0x0a2  u32  LABOACTION count / total
stream +0x0aa  u32  VENTES count / total
stream +0x0b2  2 x u32  yesterday staff buckets
stream +0x0ba  u32  yesterday FOOD total / count
stream +0x0c2  u32  yesterday SECU total / count
stream +0x0ca  13 x u32  yesterday shop revenue
stream +0x0fe  13 x u32  yesterday shop sale counts
stream +0x132  13 x u32  yesterday totals / group counters
stream +0x166  2 x u32  cumulative staff buckets
stream +0x16e  2 x (u32 total, u32 count)  cumulative FOOD / SECU
stream +0x17e  13 x u32  cumulative shop revenue
stream +0x1b2  13 x u32  cumulative shop sale counts
stream +0x1e6  13 x u32  cumulative totals / group counters
stream +0x21a  end of economy stream

block  +0x24a  u32  schedule day (`time manager +4`)
block  +0x24e  f32  schedule elapsed seconds (`+8`; 23.214 manual, 0.0 auto)
block  +0x252  f32  schedule current seconds (`+0xc`; same values)
block  +0x256  u32  task vector `+0x18` (0 in retail saves)
block  +0x25a  11 x u32  task slots (`task +0x70`; all zero in retail saves)
block  +0x286  12 B park field `spPark+0x148` (`00 00 ad ba` + two zero dwords)
block  +0x292  1 B  trailing byte (0x01 in both retail saves)
block  +0x293  end of park block
```

The schedule tail is written by `0x00594cb0`: `0x0058c260` writes the 12-byte
time manager (`+4`, `+8`, `+0xc`) and `0x0058c6b0` writes the task vector's
`+0x18` field followed by one `u32` per task. The vector length is **not**
stored; the reader reuses its constructor length (11 slots), so the 0x49-byte
tail only fits the empty form. The 12-byte field is the runtime `spPark+0x148`
block; the constructor zeroes bytes `+0x148/+0x149` and dwords `+0x14c/+0x150`,
which leaves `+0x14a/+0x14b` holding the debug fill `0xBAAD`. The final `0x01`
byte is present in both retail saves but no writer has been identified yet.

Both retail saves have no employees, so both staff buckets are 0. The manual
save's `125/2` pair is the **current-day ticket income** (two visitors), not a
staff bucket.

**Fill pattern.** The three computed totals (`+0x144/+0x148/+0x14c`, stream
`+0x132/+0x136/+0x13a` and again in the cumulative block at
`+0x1e6/+0x1ea/+0x1ee`) are only written by the panel refresh `0x00586070`.
A fresh save therefore contains the debug fill **`0xBAADF00D`** in those six
fields. `tools/formats/economy.py` masks it and reports the totals as
unsettled. The yesterday row counters are zero until the first day rollover.

**Loader bug.** For version >= 46 (`0x2e`) `0x00585240` reads the cumulative
block back into the *yesterday* runtime fields (`+0xbc..+0x16c`) instead of
`+0x170..+0x220`, and it consumes only 5 of the 13 first-block total dwords
before doing so. The retail loader is therefore misaligned by 32 bytes for
version-53 saves. The parser follows the **writer** layout and exposes the
two blocks separately (`yesterday_*` and `cumulative_*`); do not reproduce the
loader bug in the port. Evidence: `0x00585240` at `0x0058560b`
(`cmp [esp+0x18],0x2e`) reads `+0xbc..+0x16c`, while `0x00585780` writes
`+0x170..+0x220`.

`FUN_00585bd0` (constructor setter) writes the same runtime `+0x08` budget and
`+0x98` sellback fields, confirming the field identities.

### Dinosaur store

The 20 records are genotype references plus their store price:

```text
Allosaurus         9000   DATA\Design\Scenario\Genotype\allo.gen
Ankylosaurus       5500   DATA\Design\Scenario\Genotype\anky.gen
Brachiosaurus     42000   DATA\Design\Scenario\Genotype\brachio.gen
...
Velociraptor      15000   DATA\Design\Scenario\Genotype\velo.gen
```

The price field is proven, not guessed: all 20 names, prices and `.gen` paths
match `Data/Design/Scenario/FreeGame.ini` `[DINOSTORE]` in order (the same
values appear in `Market.ini` for those genotypes). The actual 18-gene table is
loaded from the `.gen` file when the record is read
(`FUN_00593a60` -> `FUN_00595f40` -> `FUN_00584c20`).

### Lab store

`FUN_00592060` reads five u32 (`3,3,3,3,3` in retail), then `FUN_00592a00`
reads `lab_count = 8` and one record per slot. The record byte is a presence
flag: `1` means empty and `0` means populated. All eight flags are `1` in both
retail saves, so the variable tail is only exercised by a synthetic test.

A populated slot (`flag == 0`) stores the full 18-gene genotype and a display
name. The writer is `FUN_00592b20`, which calls the genotype writer
`FUN_00585100` and then writes the string length + 1 (NUL included) and the
ASCII bytes:

```text
u8   flag                     # 0 = populated
18 x (u32 allele_a, u32 allele_b)   # 0x90 bytes, same gene order as .gen
u32  name_bytes               # string length + 1
...  ASCII name, NUL terminated
```

The genotype reader/writer `FUN_00584ed0` / `FUN_00585100` iterate **18**
records at runtime stride `0xc` and serialize the first two dwords of each
(8 bytes), so the file form is exactly 0x90 bytes. Version <= 21 used a
15 + 3 split with two extra dwords; version 53 uses the 18-gene form above.
The runtime lab record is `0xe8` bytes (flag at `+0`, string object at `+4`,
genotype at `+0x10`); the file only stores the flag, genotype and name.

`tools/formats/sav_graph.py` exposes `lab_records` (`LabRecord.flag`,
`.genes`, `.name`, `.empty`) and a `--labs` CLI dump.

## World manager section

`FUN_0053c9d0` (save) / `FUN_0053c560` (load) serialize several lists of world
objects plus the show manager. Each list is `u32 count` followed by objects
that use their own `vtable + 0x4c` serializer. `FUN_0053a140` registers seven
serializer classes:

| Class ID | Class | Constructor | Size | Read | Write |
| ---: | --- | --- | ---: | --- | --- |
| `0x12` | `drsDinosaur` | `0x005e8e30` | `0x11c` | `0x005e8ef0` | `0x005e8f80` |
| `0x13` | `drsVisitor` | `0x005e8c60` | `0x3c` | `0x005e8cf0` | `0x005e8d60` |
| `0x14` | `drsEmployee` | `0x005e8b40` | `0x34` | — | — |
| `0x15` | `drsAnimal` | `0x005e8a00` | `0x30` | — | — |
| `0x16` | `drsShow` | `0x005e74f0` | `0x70` | — | `0x005e7860` |
| `0x17` | `drsFence` | `0x005e87a0` | `0x5c` | — | `0x005e8910` |
| `0x18` | `drsBuilding` | `0x005e8590` | `0x34` | — | `0x005e86a0` |

The list order is fixed by the loader and is **not** the class-id order:
fence, show, animal, employee, building, visitor, dinosaur. The class is
chosen by the list; the inline class tag is redundant in version 50-53 files
(and stores a kind for fences/shows). Class `0x12` carries an 18-gene genotype
table (`FUN_00584ed0` / `FUN_00585100`).

### AI manager + camera tail

After the lab records:

```text
u32 ai_list_count[5]        # first four are 0 in retail
if non-auto:
    ai_list_count[4] x 17-byte AI entries
f32 camera_eye[3]          # 0..2
f32 camera_target[3]       # 3..5
f32 camera_progress        # 6, interpolation progress
f32 camera_reserved[3]     # 7..9, zero in retail
```

Manual: `(0,0,0,0,2)`, two 17-byte entries, camera eye
`(550.998, 512, 84.696)`, target `(512, 512, 32)`, progress `0.1`, then
`(0,0,0)`.
Auto: `(0,0,0,0,0)`, no entries, camera eye `(128, 128, 250)`, target
`(512, 512, 50)`, progress `0.1`, then `(0,0,0)`.

The split is proven against the verified start-camera framing: the manual
save's first two vectors are exactly the eye and target used by
`FUN_0051dad0`'s park handoff (`docs/platform/vertical-slice.md`).

The C++ viewer applies this tail on `--sav-park`: eye/target set the orbit
camera center, distance, yaw and pitch (unless `--camera-target`/`--distance`
is given). The manual save reproduces the start view; the auto save starts
far out at eye `(128,128,250)`. Guarded by `test_cpp_sav_park_import` and
`test_cpp_sav_park_auto_camera`.

The AI manager uses `0x0051f290` (load) / `0x0051f400` (save). The camera
block is the `camSwitcher` serializer (`0x005e5b80` / `0x005e5ba0`).

## Reader

`tools/formats/sav.py` — header and show catalog:

```text
python tools/formats/sav.py run/save/slot001.sav --shows
```

`tools/formats/sav_graph.py` — simulation-state prefix:

```text
python tools/formats/sav_graph.py run/save/slot001.sav --visitors --dinos --labs
```

`sav_graph` returns the road rectangles, visitors, park budget/fields, the
20 dinosaur-store records, the five visitor-model values, the lab records
(flag, 18 gene pairs, name), the five AI-list counts, the 17-byte AI entries
and the 10 camera floats. Tests: `tests/test_sav.py`,
`tests/test_sav_graph.py`.

### C++ economy import

`SavSimulation.economy` (`src/formats/wld.*`) decodes the full park economy
stream: sellback, current and yesterday FOOD/SECU/ticket/staff/shop/group
counters (`+0x30`..`+0x24a`). `--sav-park` copies the yesterday block into
`BudgetLedger` (poison `0xBAADF00D` -> 0), which drives the 14 budget-panel
rows; the current block is exposed for future settlement use. A synthetic
settled save (patched copy of `run/save/slot001.sav`) renders
`notes/budget-sav-rows.png` and is covered by `test_cpp_sav_budget_rows`
(expenses 8326, income 1460, net -6866).

## Evidence

- `notes/save-graph-recon.md` — decompiled function map, offsets and field
  evidence.
- `notes/save-recon.md` — original header/catalog recon.
- Ghidra: `FUN_0051e6c0`, `FUN_0051e7e0`, `FUN_0053a140`, `FUN_0053c560`,
  `FUN_0053c9d0`, `FUN_00558340`, `FUN_00585780` (economy writer),
  `FUN_00585240` (economy reader), `FUN_00585bd0`, `FUN_0058aed0`,
  `FUN_0058ccc0`, `FUN_00592060`, `FUN_00592a00` (lab reader),
  `FUN_00592b20` (lab writer), `FUN_00584ed0` / `FUN_00585100` (genotype
  reader/writer), `FUN_00593a60`.

## Open

- The park block's leading `2` is consumed by the reader `FUN_0058aed0` as a
  **record count**, not necessarily a class id; it then reads a type id +
  payload per record. The `class id` label and the meanings of the following
  fields (`0/3`, `100`, `10.0/0.0`) remain provisional.
- The single trailing `0x01` byte at `block + 0x292`.
- The second runtime staff bucket (`+0x20`) and the Guard role index; see
  `docs/decomp/economy.md`.
- The AI entry field split (17-byte records); the camera split is solved.
- Campaign autosaves populate the three simulation lists after the roads
  (a ~53 KB `auto.sav` after a campaign park has list0 = 20 objects). The
  free-game writer covers only the empty form; `sav_graph.py` and the C++
  reader report the populated variant. Decoding and writing those lists is
  open. The 20 `list0` records match the world block's 20 dinosaurs. The
  **packed prefix is 0x19 bytes**:

  ```text
  +0x00 u32 object id        (first: 63823)
  +0x04 u32 x                (517)
  +0x08 u32 y                (450)
  +0x0c u8  need_state[4]    (03 03 01 02)
  +0x10 u16 need values[2]   (a9 01, c8 00)
  +0x14 u8  flag             (0)
  +0x15 u32 name byte length (0x18, includes the NUL)
  +0x19 UTF-16LE name        ("Ankylosaur1\0")
  ```

  A variable tail follows the name. Its first 0x34 bytes are fixed: a u32
  (0) followed by four `[80,60,30]` int triples (the need thresholds), then
  five values (100 in the first dino, 0 in the next two). After that the tail
  is variable state (likely action/needs arrays); tail lengths are
  0x302..0x370 across the 20 records. `tools/formats/sav_campaign_scan.py`
  walks the list (ids 63823..63932, names `Ankylosaur1`..`Coelophysi2`) and
  prints the prefix plus the tail length. The four bytes at `+0x0c` range
  0..4 in the campaign records, so their exact "need state" encoding is still
  open. Decoding the variable tail is the remaining step.

## Writer round-trip (SAVE-0, Python)

`tools/formats/sav_write.py` rebuilds the **whole file** from the parsed
models in retail writer order: header, world block (fences with the
repeated 16-byte placement rect, shows, animals, employees, buildings,
visitors, dinosaurs, unlock flags), show catalog, and the simulation
section (roads, visitors, park economy stream `0x00585780` + schedule tail
`0x00594cb0`, dino store with its NUL-terminated strings, visitor model,
lab records, AI lists, camera, tail). It is byte-identical on
`run/save/slot001.sav`, `slot002.sav` and `auto/slot001.sav`;
`tests/test_sav_write.py` guards all three. `--whole` emits the file,
`--full` only the simulation, `--park` only the park block.

## C++ writer (SAVE-2)

`src/formats/wld.*` mirrors the Python writer and retains every byte in
typed records:

- `WldWorld::records` (`SavWorldRecords`): objectives, fences (object id,
  first/repeat `+0x14`, placement rectangle, waypoints, class tag, repeated
  16-byte rect, `+0x44`, `+0x48`), shows, animals, employees, buildings
  (with the per-object link), visitors, dinosaurs (18 gene pairs) and the
  four unlock flag arrays. `read_sav_world` parses the block a second time
  into this model and requires it to end exactly at `world_end`.
- `SavSimulation` keeps the park header/economy/schedule, visitors, dino
  store (raw UTF-16/ASCII strings with terminators), catalog, visitor
  model, labs, AI lists, camera and tail.
- `write_sav_world`, `write_sav_catalog`, `write_sav_park_block`,
  `write_sav_simulation` and `write_sav_whole` re-serialize all of it; no
  slice of the file is copied.

CLI:

```text
dino_viewer --sav-info run/save/slot001.sav        # header/world/simulation summary
dino_viewer --sav-roundtrip run/save/slot001.sav   # whole file identical
dino_viewer --sav-write IN.sav OUT.sav             # rebuild and emit
dino_viewer --sav-write IN.sav OUT.sav --sav-name renamed --sav-slot 3
dino_viewer --sav-write IN.sav OUT.sav --sav-budget 12345
```

The header (version, slot, UTF-16 name, 12 reserved bytes) is retained and
re-serialized too, so no section of the file is copied. `--sav-name` (ASCII)
and `--sav-slot` change only the header; the world/catalog/simulation body is
unchanged, and the emitted file round-trips through the same reader.

`--sav-roundtrip` reports `sav roundtrip: whole file identical` on every
retail save; `--sav-write` emits a file that the Python reference parses and
round-trips byte-identically. `test_cpp_sav_park_roundtrip` covers both paths.

**Retail acceptance (2026-10-27):** a C++-written save (`--sav-name cppwrite
--sav-slot 1`) installed as `run-test/save/slot001.sav` appears in the retail
LOAD GAME list as `cppwrite`; selecting it starts the park (Reset `hr=0`) and
renders it (`notes/sav-retail-accept.png`). The park clock matches the
save's `schedule_elapsed` (23.2 s -> 09:2x). The HUD cash (26000) does not
track the park block's `+0x30` budget (12345 in the test save), so that field
is not the live-money display; this is an open semantic question, not a writer
failure.
