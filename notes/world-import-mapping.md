# P2 — retail park import mapping (`.wld` / `.sav` → port)

Status: **P2 complete.** Roads/fences/buildings/visitors/budget/clock are
implemented, and dinosaurs spawn from the world genotype. The files carry no
per-dinosaur position, so records are distributed inside the fence
rectangles.

## Roads → path cells

The simulation road list is after the world-object block:

```text
u32 count
count × (s32 x0, s32 y0, s32 x1, s32 y1)   // world units, 0..1024
```

Cpg04 (`Data/Design/Scenario/Cpg04/Cpg04_Save.wld`) has 14 rects, all
axis-aligned single-cell lines:

```text
(380,408)-(656,408)   (412,456)-(504,456)   (412,376)-(412,456)
(444,348)-(496,348)   (444,208)-(444,408)   (448,456)-(448,592)
...
```

Mapping to `BuildGrid`:

```text
cell_x = floor(wx / grid.cell_size)   # raw world 0..1024
cell_y = floor(wy / grid.cell_size)
```

The old example `380 -> 223` was wrong: it used `(wx - origin_x)/cell`,
which adds half the world. For isl01 (1024 world units, 4-unit cells) the
correct cells are `380 -> 95`, `408 -> 102`, `656 -> 164`. Mark every cell in
the inclusive rectangle; a line rect (`y0 == y1`) is one cell high. The port
implementation is `dino::import_roads` in `src/world/build.cpp`, used by
`--wld-park`.

## Fences → grid edges

`drsFence` (class `0x17`) carries a world-unit **start/end rectangle** in the
placed base:

```text
u32 object_id
u32 field_14
s32 start_x, start_y
s32 end_x, end_y
u32 waypoint_count
waypoint_count × 0x18 bytes
u32 class_tag (0x17)
u32 field_14 (repeat)
s32 start_x, start_y (repeat)
s32 end_x, end_y (repeat)
u32 field_44
u32 field_48
```

Cpg04's nine fences are rectangles such as `(452,312)-(467,343)` and
`(460,356)-(491,403)`. Decompose each rectangle into cell edges: south edge
of `(x, y0)` for `x` in `x0..x1`, north edge of `(x, y1)`, west edge of
`(x0, y)`, east edge of `(x1, y)`; the port's canonical edge convention
stores south/east only, so map north/west to the neighbour's south/east.

The 0x18-byte waypoint records are **not world coordinates** (values like
`(flags=63668, key=0, 25, 0, 12, 12)`), and the writer stores the last three
fields in the order `+0x14, +0x0c, +0x10` rather than the struct order. They
are auxiliary path/segment data; the start/end rectangle is sufficient for a
first import. Treat exact waypoint semantics as open (cosmetic).

`field_44`/`field_48` look like linked object ids (e.g. `63529/63530/63531`
for one fence), not geometry. The object ids in a `.wld` are arbitrary
editor ids, not sequential indices.

## Visitors

The save visitor list is a count followed by 0x50-byte records:

```text
u32 object_id
s32 x, y                 // world units
u32 flags                // 0x10001 in slot001
u32 unknown_10
u32 state                // 0 choose, 1 travel, 2 use, 3 leave
f32 fx, fy               // world position
f32 dx, dy               // heading
u32 unknown_28
u32 zero_2c
u32 visitor_id
7 × u32 needs            // +0x48..: need0, need1, need2, 99, 99, 99, 99
```

`run/save/slot001.sav` decodes two visitors:

```text
id 38  (524,433)  state 1  needs (97,96,99,99,99,99,99)  dir (-0.02, 1.00)
id 39  (524,393)  state 1  needs (99,98,99,99,99,99,99)  dir (-0.12,-0.99)
```

So P2 can spawn the exact visitors at their saved world positions, with
their saved state and needs. The profile index is not in this record; assign
one from the free-game profile table (or leave it 0) until the writer of
`actor+0x68` is found.

## Budget and clock

The park block in the save is `0x293` bytes: a `0x30` header + the economy
stream + the schedule tail. `tools/formats/economy.py` and
`docs/decomp/economy.md` expose the budget pair and the daily settlement;
`sav_graph.ParkState` exposes `day`, `elapsed_seconds` and the 12-byte task
field. The port's `PortState.budget`, `day` and `elapsed` map directly:
read the budget pair instead of resetting to the difficulty budget, and seed
`ParkClock` with the saved day/hour.

## Dinosaurs - species from the genotype

The `drsDinosaur` world record carries only:

```text
object_id, field_1c, field_24, field_28, field_2c, class_tag,
18 x (allele A, allele B)
```

The five ids are sequential object links (e.g. `63822/63823/63824/63826/
63825`), not positions; `field_1c` is the link used by the simulation list.

Cpg04's simulation section (offset `0x2010`, after the 14 road rects) starts
with `u32 count = 20`, and each record begins with:

```text
u32 link_id          # equals the world dino's field_1c (63823 for the first)
u32 ?                # 517
u32 ?                # 450
u32 ?                # 0x02010303
u32 ?                # 0x00c801a9 (425/200 as two u16)
u8  0                # pad byte, so the next field is unaligned
u32 name_bytes       # 0x18, at record +0x15
utf16 name           # "Ankylosaur1" at record +0x19
...                  # phenotype (80/60/30 x4), needs (100 x5), care state
```

So the simulation block holds the **individual dinosaur care state** (name,
phenotype, needs), linked to the world record by `field_1c`. It does **not**
contain a world position either: retail dinosaurs are assigned to a paddock
and respawn at load. The port therefore distributes them inside the saved
fence rectangles.

The species comes from the world genotype: gene 0 is the head-type pair and
the dominant allele is `min(A, B)`. Each of the 20 base `<species>.gen` files
has a distinct head type 0..19, so the mapping is one-to-one:

```text
0 allo   1 anky   2 brachio 3 coelo  4 deino
5 diplo  6 gigano 7 hypsilo 8 igua   9 kentro
10 pachy 11 parasau 12 peleca 13 proto 14 salta
15 stego 16 trex 17 trice 18 trood 19 velo
```

Hybrids use the dominant body plan (`[18,0]` → allo, `[3,18]` → coelo).
Cpg04 resolves to 20/20: allo 7, anky 2, coelo 4, deino 5, trood 2. The
animation family is `simu_trans.dic` Part 10 (LightBiped for most of these,
LowQuadruped for anky, HeavyBiped for allo).

## Integration order

1. `read_wld`/`read_sav_world`: also return fences and visitor records (C++
   `WldWorld` currently stops at buildings + roads).
2. `BuildGrid`: apply roads as path cells and fence rectangles as edges.
3. `VisitorSystem::spawn`: restore saved visitors (world pos, state, needs)
   instead of random path cells.
4. `PortState`: read budget/day/elapsed from the save instead of defaults.
5. Dinosaurs: spawn the genotype at a default position until the position
   field is located.
