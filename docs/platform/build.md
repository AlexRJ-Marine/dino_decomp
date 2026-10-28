# Path and fence placement (SIM-1)

Status: **working** - grid placement, save/load, undo, terrain picking,
retail fence/path visuals, the retail paddock-capacity model, the
`<island>_occupation.raw` buildability mask and building placement through
the retail construction panel.

## Grid

- World is the 1024-unit island square; the default cell is **4 units**
  (256×256 cells).
- **Paths** own cells.
- **Fences** own cell edges. Only the *south* edge of a cell and the *east*
  edge are stored, so a shared edge has one canonical owner:
  - south edge `(x, y)` == north edge of `(x, y-1)`
  - east edge `(x, y)` == west edge of `(x+1, y)`
- `BuildGrid` keeps a change stack for undo; only actual toggles are recorded,
  so `undo()` restores the opposite value and fixes the counts.

## Interaction

```text
1        path tool
2        fence tool
0        tool off
U        undo last placement
click    place/remove at the terrain point under the cursor
```

A click is a left press/release with less than 5 px of drag, so orbit/pan
still works. Placement picks the terrain by ray-marching the heightfield from
the camera through the mouse pixel and bisecting the last step (24 iterations);
the hit must be inside the island bounds. The pick ray uses the **same 80°
vertical FOV as the terrain renderer** (`draw_scene`); the old 33° value was
the single-dino viewer and made placements drift away from the cursor toward
the screen edges. The fence tool chooses the nearest
cell edge (west/east/south/north) and toggles its canonical edge.

The `1` key tool above is the port's original per-cell toggle. The retail
way to build paths is the road tool below.

## Retail fence tool (`FENCE_CONSTRUCTION`)

`OPEN_CONSTRUCTION_FENCE` opens the `FENCE_CONSTRUCTION` state
(`0x5265b0`, panel `FENCE_CONSTRUCTION_AND_INFO_PANEL`). The player selects a
fence feature (LunchBox / observation area) or draws a paddock perimeter:

- first click stores a corner after the busy/inside check (`0x58a190`);
- second click builds the sorted rectangle (`0x52bbf0`), previews it
  (`0x527150`; its `>= 16` check is the LunchBox dish list, not an edge cap),
  validates with the selected feature's margin
  (`0x58a4e0`), charges `CONSTRUCTION` (`0x581b90` cost, `0x52bed0` gate,
  `0x585d20` deduction) and records the wall (`0x53ac60`, type 0x17);
- feature mode (selected list row) spawns the `.bfi` actor (`0x53acd0`) and
  the animal-bait actor (`0x53b0d0`);
- warnings `BUSY_SPACE` / `UNEVEN_FLOOR` / `INSIDE_PERIMETER` /
  `PERIMETER_TOO_SMALL` / `ROAD_TOO_SHORT` come from `0x52c2e0`; `N`/`J`
  rotate the placement and Esc leaves.

The port matches the retail click flow: both clicks snap to the nearest
4-unit anchor (`0x5f8c70` -> `0x5faaa0`, round-half-down) and the first click
stores that anchor; the second builds the sorted rectangle (four sides,
canonical edges) and registers a `Paddock` with its capacity.
The cursor validity disc uses the captured retail colours (blue
`(38,38,136)`, red `(80,38,38)`): blue when the 4x4 cell at the snapped
anchor passes the occupancy scan and is not strictly inside an existing
paddock, red otherwise (`fence_preview_disc`). Exact two-click rules are now
ported: `0x52bbf0` sorts the two anchors and subtracts 1 from the high
corner; a side shorter than `6*margin-1` (margin 4 from `Fence.bwi`) is
`PERIMETER_TOO_SMALL`; the inset interior is scanned for a busy occupancy
class; cost is `((x1+y1)-(x0+y0))*2/4*30` (30 per perimeter cell, all 24
`.bwi` files), charged to CONSTRUCTION, and the four wall lines are stamped
into the occupancy grid as class 7. The pending corner shows
`&MESSAGE_FENCE_PERIMETER_COST` ("Cost of enclosure: N"). Still open: the
0.01 height-slope gate, LunchBox row, Observation Area (`PointOfView`
building, 4x4, 500). Fence selling is ported (below).

### Enclosure preview (helper modes 1 and 2)

`0x5f8770`/`0x5f8700` set the helper mode; `0x5f8850` draws it
(`notes/fence-ground-beam-preview-recon.md` §3):

- **Before the first click (mode 1, `0x5f98e0`):** one `helper_sphere` of
  scale 8 (diameter 8, a disc from above) at the snapped anchor + step/2,
  i.e. the cell centre. Blue `(38,38,136)` or maroon `(80,38,38)`. The disc
  disappears once the corner is set.
- **After the first click (mode 2, `0x5f9b00`):** a filled grid of
  `helper_sphere` dots on a **4-unit** grid strictly between the two snapped
  anchors. The first/last row and column are half-size (diameter 2), the
  interior is diameter 4; z is the terrain height at each cell corner. Valid
  `(38,38,151)`, invalid `(148,38,38)` (retail captures
  `notes/harness/fence-rev-template-valid.png` / `-invalid.png`). The grid
  turns maroon on the same `0x58a4e0` failures as the cost rect (side < 23,
  inside a paddock, inset occupancy scan). Implemented in
  `fence_template_mesh`, drawn only after the first corner. Leaving the tool
  (Esc, panel close, another tool) must hide **both** the disc and the grid;
  the port's cleanup originally checked only the disc, so Esc after the first
  corner left the blue dots on screen.

### Paddock ground

A built paddock gets its sandy interior from a **`gatAttraction` ground quad**
created with the `stFence` (`0x53d0b0` -> `0x5f4e70` -> `0x5ec7e0`), not from
a landscape texture-manager layer (`notes/fence-ground-beam-preview-recon.md`
§1):

- rect = the `stPerimeter` rectangle itself (no inset), z = terrain height at
  the centre + 0.01, flat (does not follow the terrain);
- stage 0 `boue_pad.tga` (sand) x stage 1 `ground_occup_pad.tga` (mostly-white
  mottle, ragged alpha border), both **Wrap** and sampled from one of the four
  128x128 quadrants (`rand() & 3` at creation; stage 1 uses the next quadrant,
  UV+0.5);
- state `dino_attractionground.ini`: unlit, fog Y, ZWrite N,
  SrcAlpha/InvSrcAlpha, dual texture. Repainted immediately because
  `AddAttraction` sets `scene+0xec` and the scene render recomposes.
- Loaded parks: retail creates the quads for every `stFence` in the saved
  park, so a campaign save shows its enclosure floors immediately. The port's
  `import_park_layout` rebuilt the meshes **before** the `paddocks` vector was
  populated, so loaded enclosures had no floor until the next build; a second
  `rebuild_build_meshes()` after the paddock list is built fixes it
  (`test_cpp_campaign_enclosure_ground_loaded`: 9 Cpg04 paddocks / 36 verts).
- Capture: interior mean RGB (160.9,129.0,62.0) 0.5 s after the build
  (`notes/harness/fence-rev-ground-after.png`); the port reproduces it in
  `build/paddock-ground4.png`.
- Port pitfall fixed: `upload_mesh` adds +1 to V (engine bottom-left UVs) and
  the terrain detail pass leaves `u_uv2_world = 1/16`; the ground V is stored
  as `v-1`, the ground textures are uploaded repeat-wrapped, and the extra
  draw resets `u_uv2_world` per item. Without all three the quad samples the
  atlas border (transparent) and is invisible.
- Open: show arenas use `Show.bwi` with `boue_att.tga` /
  `ground_occup_att.tga` (`boue_pad`/`ground_occup_pad` for the other 23 wall
  styles). The port's `Paddock` does not yet carry the wall style, so every
  perimeter currently uses the `Fence.bwi` pair; the show tool's arena fences
  are the remaining case to split.

### Fence lamps (`laser2.tga`)

`fence_paddock_lod1.mesh` material 1 (`laser2.tga`, 8 triangles) is two
cross-shaped additive billboards per 4-unit segment, at **79%** and **28%** of
the post height (`notes/fence-ground-beam-preview-recon.md` §2). Retail draws
them as the same fence mesh's material 1 with per-material state selection
(`0x5f3250`: `dino_additif`, One/One, ZWrite N, fog N). There are **no beams
along the rail**; the two billboard heights are the post lamps (top tip and
~2/3 post height), captured as near-white cyan (capture
`notes/harness/fence-rev-beams-post.png`, peak `214,255,255`). The port splits the fence
mesh by material (`split_by_material`) and draws material 1 additively with
no depth write.

The colour is a **time pulse**, not a static tint (`FUN_00600550`, called
from the scene render `0x5f553b`; `notes/fence-pulse-bank-recon.md` and
`notes/fence-selected-state-recon.md`):

- **Laser (bank 4, id 4):** `x4 = (sin(4t)+1)/2`, RGB
  `(80-54*x4, 80-54*x4, 255-170*x4)` in 0..255, additive, period
  **1.571 s**. The mesh material's white ambient dominates, so the retail
  lamps read near-white with a subtle blue pulse. The port adds the scene
  ambient to the bank colour (`1.62/1.64/1.68` peak-ish after diffuse) and
  applies it with the `u_tint` uniform.
- **Selected paddock (bank 1, id 1):** clicking the fence (after leaving the
  construction tool) selects the paddock; its body switches to
  `Dino_SelectedWall` (state 8, stage 1 `Modulate2x`) with the grey
  `128..255` sin 10 t ramp, so the body brightens **1.0x..2.0x** at period
  **0.628 s**. The lamps themselves stay additive but saturate against the
  brighter posts. Right-click or a click on empty ground deselects.
  `--select-paddock N` and `--pulse-time SEC` freeze/select for tests.
- Retail measurements: unselected body static; selected body `204 -> 399`
  (stone `384 -> 733`, rail `336 -> 535`) at 0.63 s, sand unchanged
  (`notes/harness/fence-pulse-run1-frames.tsv`). The port's selected pass is
  split into `BuildMeshes::fences_selected` so only that enclosure pulses.

Still open: the retail paddock **info panel** opened by the selection click
(as opposed to the pulse) is not ported, and the selection trigger path
(`0x5ed240`, no static caller found) is implemented from the live retail
behaviour rather than the static xref scan.

### Leaving a placement tool

The fence panel has **no close button** (`FENCE_CONSTRUCTION_AND_INFO_PANEL`
contains only the stats and construction sub-panels). Retail leaves the state
with the common key handler: `pvwStateFenceConstruction` vtable **+0x1c =
`0x52be60`** handles Esc, which calls the state's leave **+0x08 = `0x5269c0`**
(hides the panel). The port now maps Esc to the same release:

- Esc with an active placement state clears it: fence (`build_tool 6`), road
  (`4`), building (`3`), show (`5`) and store selection; the corresponding
  panel is hidden. A selected enclosure is deselected as well (retail uses Esc
  for that too).
- The building/road panels' **put-back/close** widgets and the HUD opener
  toggles also deactivate the matching tool (the fence opener is covered by
  its panel, hence Esc).
- A **successful placement does not auto-leave** the state in retail: the fence
  apply `0x526f20` only updates the info/name and keeps the style selected.
  The port keeps the tool armed after a build, so press Esc (or switch to
  another tool) before clicking the placed enclosure.
- Right-click or a click on empty ground deselects the enclosure highlight.

Retail Esc in the **default park** is a **no-op**: no panel, no dialog, no
pause, no quit (live probe: clock keeps running, pixel diff 0). With the
construction strip open but no placement tool, Esc also does nothing (the
strip stays). Esc **closes the Options panel** (it is the overlay-close key;
`PauseGame` is **P**, not Esc). The port now matches: Esc closes
`OPTIONS_PANEL` if visible, otherwise leaves an active placement/selection
state, otherwise does nothing in the park. The standalone viewer (no park)
still uses Esc to exit.

Port approximations: wall lines are in the occupancy grid, but the port also
keeps the edge model for rendering and paddock capacity, so edge-based
crossings remain as a fallback.

### Selling a fence (shared sell state)

Retail's HUD **Sell** switch (`OPEN_SELLING`, screen (591,584)) enters the
shared state 0x14 `SELLING`. A click on an enclosure resolves it like the
selection click (`0x53c2d0`) and opens `REQUEST_PANEL` with
`&REQUEST_SELL_FENCE` + the value. Validate runs `0x533de0`:

1. refuse when `stFence+0x98 != +0x9c` (dinosaurs inside) with
   `&WARNING_SELLING_NON_EMPTY_FENCE` — "The enclosure cannot be sold: It
   still contains dinosaurs.";
2. remove the `"Lunchbox"` feature actors;
3. credit `int(GetSellValue * SellBackFactor)`;
4. delete the fence graphics and the 12-unit ground cells / wall occupancy.

`GetSellValue` (`0x581710`) is the build formula on the **margin-inset**
perimeter: `((x1-m + y1-m) - (x0+m + y0+m)) * 2/cell * price`, m=4 from
`Fence.bwi`. Live retail: build 870 → sell 630; the port's 2130 build sells
for 1890. `SellBackFactor` is `FreeGame.ini` `SellBackFactor` /
`SellBackFactorMedium` / `SellBackFactorHard` = 1.0/0.5/0.2, or the campaign
`CONTEXT.SELLBACKFACTOR`.

The port implements the fence branch (`sell_paddock_now`): removes the
canonical edges, the class-7 wall lines and the paddock (which drops its
ground quad), credits the budget and rebuilds the meshes. Cancel keeps the
fence; the state stays armed. Esc leaves sell mode and closes the request.
The same HUD state will dispatch roads, buildings, dinosaurs, features and
shows next; their branches are decoded in `notes/fence-panel-sell-recon.md`.

Note: the old "16-edge limit" was a misreading — the `>= 16` check in
`0x527150` is the `stFence` **LunchBox dish list** (max 15 dishes), not a
park enclosure cap. The port's 16-paddock gate is removed.

### Paddock info panel (selection)

Selecting a fence (the same pick as the highlight) enters the retail
FENCE_CONSTRUCTION state 15 and shows the **same**
`FENCE_CONSTRUCTION_AND_INFO_PANEL` root; the info half is
`FENCE_STATS_PANEL`. `opvFenceInfoFill` (`0x5269e0`) is a **snapshot** — its
only callers are the state enter and the successful perimeter apply, so the
numbers do not refresh per frame.

`stFence::GetStats` (`0x57e720`):

- `out[0]` = `(+0x9c - +0x98)/4` occupant count;
- `out[1]` = `(int)(fill[+0x78] * 100)` percent of capacity units;
- per-diet demand `scale * dino[+0x6c]`, carnivore when `dino[+0x200] == 1`;
- per-diet food = sum over the `stFence+0xd8` LunchBox dishes of
  `available_slots * quantity(.bsi+0x04) * nutrition(.bsi+0x00)`, kind `< 5`
  herbivore / `>= 5` carnivore;
- `out[2]/out[3] = food*100/demand`, or **100%** when that diet has no
  animals; `scale = 39616 * [[spPark+0xc4]+4]+0x14` (exact schedule value
  still open).

The port fills the four `..._CHIFFRE` widgets and the name field on
selection (`refresh_fence_info_panel`), with `EnclosureN` regenerated in
creation order (retail never saves the name). No LunchBox dishes are ported
yet, so a diet with animals reads 0% and an empty diet 100%; the dish ratio
waits for the schedule scale. Screenshot: `build/info-panel-zoom.png`
(matches the retail empty-paddock capture).

**Open (noted from playtest):** retail changes the mouse cursor/icon while
the sell state is armed; the port still uses the normal park cursor. This is
tracked as SELL-1 in `TASKS.md`.

Evidence:

Retail itself draws a **filled circular ground disc**, not a ring
(`notes/road-enclosure-preview-capture.md`): solid blue ~RGB(38,38,136) when
placeable and dark maroon ~RGB(80,38,38) when not, with a hard edge, no rim,
snapped to the build cell under the cursor (capture: cursor 350,450 -> disc
centre ~407,455). After the first corner the pending rectangle is outlined
with small `helper_sphere` dots every 8 units along its four sides (retail
helper mode 1), the same blue/maroon colours, turning maroon when the retail
`0x58a4e0` checks fail (side < 23, inside a paddock, inset busy scan). The
port's preview helper is being aligned to this shape;
the earlier ring was a stand-in.

## Retail road tool (`ROAD_CONSTRUCTION`)

```text
OPEN_TRANSPORT (Infrastructure) -> CONSTRUC_ROAD_BUTTON (Road)
  left click    place the start cell
  left click    commit a straight run start -> cursor, then chain from its end
  right click   cancel the run; a second right click turns the tool off
```

Decoded from state `0x10` (`notes/road-construction-recon.md`) and
implemented in `src/world/build.cpp` (`road_segment`, `road_cost`,
`check_road_start`, `check_road`, `place_road`) with the viewer as build tool
`4`:

- **Shape:** the run keeps the longer axis of start -> cursor (`0x5fa5b0`),
  so roads are straight, one cell (4 units) wide. The cursor is snapped to
  the **nearest** 4-unit anchor (`0x5faaa0`, round-half-down) before the axis
  lock; the road and fence tools both use it, the other build tools use the
  floored cell. This snapped cell decides where the direction flips between
  the two axes.
- **Cost:** `(|run| world units * price) >> 2`, i.e. `price` per cell beyond
  the first; `price` is `GeneralParams.bgd` `+0x1a4` = **10**
  (`src/formats/general_params.*`). Charged to `CONSTRUCTION` like
  buildings; the money gate runs after placement validation. A retail probe
  shows the cost is direction-independent (+x 100, -x 100, +y 100, -y 190
  for a longer world run), so the raw signed `len` is ordered/absolved before
  the charge; the port uses `|dx| + |dy|`.
- **Validation** (shared warning helper `0x52c2e0`):
  `WARNING_INSIDE_PERIMETER` when the run lies strictly inside a paddock
  rectangle, `WARNING_ROAD_TOO_SHORT` when start == end,
  `WARNING_BUSY_SPACE` when a cell is blocked ground (occupation mask), under
  a building footprint, or the run crosses a fence edge. Existing road cells
  never block, so runs can cross and extend roads. Roads have no slope rule.
- **Network:** roads are `spRoadNetwork` segments, not cells. `AddRoad`
  (`0x58d110`) merges collinear runs that overlap or touch within 4 units
  (`0x58d890`), extends a run that stops one cell short of another road onto
  it (`0x58d9d0`) and keeps the list sorted by `x0`. The port mirrors this in
  `dino::add_road` (`BuildGrid::roads`); the road tool, `.wld`/`.sav` roads
  and the Entrance road all go through it, and port saves keep `road` lines.
- **Look:** retail draws no road geometry. Its landscape texture manager
  paints each segment into the island texture (road layer `0x602380`) as the
  rectangle `(x0,y0)..(x1+3,y1+3)`: on 1024x1024 maps a RGB(170,130,90)
  rectangle with a RGB(253,198,137) rectangle inset by one texel (1 border,
  2 fill, 1 border, ends outlined). The quads are depth-tested
  (`ShapeColor2D`, LessEqual): fills at z 0.996 beat borders at z 0.997, so
  junctions and crossings join without border lines. The port's
  `paint_roads` does the same per segment (cells no segment covers
  keep the old per-cell look). The terrain then applies the `detailmap.ini`
  AddSigned detail layer, which gives roads their grain
  (`docs/platform/terrain.md`). A Cpg04 file trace confirms retail loads no
  road texture at all. Evidence: `notes/road-network-recon.md`.
- **Feedback:** failures show the retail toast text; while a run is pending a
  preview follows the cursor drawn like the retail helper (`Helper.ini`,
  One/One additive): `focus/road.tga` times the pulsing `gscMaterialBank`
  colour (`0x600550`, x = trunc((sin 6t + 1) * 0.5 * 127)): material 2
  RGB(64,64,255-x) when valid and affordable, material 3 RGB(255-x,64,64)
  when not. Retail draws one `focus/helper_road.mesh` bottomless box per
  4-unit step, scaled 4x4x4 and centred at (cell + 1.5, cell + 1.5, ground
  height) (`0x5fa708..0x5fa891`). The port keeps that per-cell shape
  (`road_helper_mesh`/`append_mesh_scaled`), writes depth so back/side faces
  do not add a second layer, and draws the tinted helper **unlit and opaque**.
  This reproduces the captured retail fills exactly: valid `RGB(64,64,255-x)`
  (capture: `(64,64,135)`), invalid `RGB(255-x,64,64)` (capture:
  `(134,64,64)`). The toast shows `Cost of road: N`.
- **CLI:** `--construct CONSTRUC_ROAD_BUTTON` (or the two panel clicks) plus
  `--road-click X,Y` (build cells, repeatable) drive the same code path.

Port approximations: the occupancy grid now carries roads (class 2), walls
(class 7) and building footprints (class 1), and `check_road`/construction
validate against it; the edge model remains for rendering and paddock
capacity. Removing a fence is not gated on its occupants (retail refuses a
non-empty paddock with `&WARNING_SELLING_NON_EMPTY_FENCE` and refunds
`value * sellback` on the empty one). Road selling is decoded: the refund
reuses the build formula `((x1-x0)+(y1-y0))*price/4`; the port does not
implement the sell request yet. Evidence:
`tests/test_viewer_cpp.py::test_cpp_road_tool`, `--selftest` road checks,
`notes/road-tool.png`.

## Buildability mask

Each island ships `Data/Design/Landscape/<island>/<island>_occupation.raw`:
a 1024×1024 `u8` map over the same 1024-unit world (1 unit per cell). The
values are `0` (free/buildable) and `255` (blocked: water, cliff, vegetation
or an existing building — the 77,923 free isl01 cells exclude the Entrance
footprint). `Terrain::buildable(world_x, world_y)` maps the centred world
coordinate to the mask (`mx = x + 512`, `my = y + 512`) and returns true only
for `0`.

- Path, fence and construction placement check the mask; a blocked cell
  prints `placement: blocked cell`, and the CLI placement prints
  `place-path: blocked cell X,Y` / `place-fence: ...` and skips it.
- The fence check tests the stored cell and the cell across the edge: edge 0
  is the south edge (shared with `(x, y-1)`), edge 1 is the east edge (shared
  with `(x+1, y)`). A neighbour outside the grid is ignored, so a boundary
  edge with a free inside cell is allowed.
- Removing an existing path/fence is not gated (only adding is), so a tile
  covered by a later building can still be removed.
- `try_construct` checks the budget first (retail `0x5306e1`), then every cell
  of the `.bbi` footprint.
- Retail `.wld`/`.sav` roads and fences are imported unfiltered (they are
  already valid in the source data).
- Missing masks (e.g. a synthetic test island) keep the previous permissive
  behaviour.

## Visuals

- On a retail island, path cells are painted into the terrain texture (see
  the road tool above). Only bare terrain without a basemap falls back to
  terrain-conforming quads at `height + 0.05` textured with
  `buildings/ground.tga`, which retail never references.
- Fence segments instance the retail paddock fence
  `Data/Graphics/paddock/fences/fence_paddock_lod1.mesh`, scaled so one
  segment spans one cell. The mesh has two materials
  (`fence_paddock.tga` 27/35 triangles, `laser2.tga` 8 triangles); the port
  currently draws all triangles with material 0 (`fence_paddock.tga`) because
  per-triangle material splitting is not implemented yet.
- Both are baked into one mesh per type and rebuilt only when placement
  changes.

## Paddock capacity

A `.wld` fence rectangle is a paddock (`stFence`, `0x57dc60`/`0x57eb50`):

```text
capacity = ((x1-x0+1) / cell) * ((y1-y0+1) / cell)   // float division
cost      = 8 << index, index 0..3 -> 8 / 16 / 32 / 64
can_add   = dinosaur not already inside && capacity - used >= cost
add       = append occupant, used += cost, fill = used / capacity
```

`src/world/build.cpp` exposes `Paddock`, `paddock_capacity`, `paddock_cost`,
`paddock_size_class`, `paddock_can_add` and `paddock_add`. The size class is
the **phenotype length category** (`DinoPhenotypeTable` in
`src/formats/dino_data.*`): `length = int(HeadLen * head_ratio / HeadLen *
body) + BodyLen + int(TailLen * tail_ratio / TailLen * body)` (each part
truncated), with `proportion.dat` blended by the Part 2 merge and the
Part 3 Head/Body/Tail Length; thresholds 200/620/900 (class 3 above 900). Imported
dinosaurs fill paddocks in record order; Cpg04 gives 9 paddocks, total
capacity 492, and 20 dinosaurs consume **336**, matching retail exactly.
`DINO_DEBUG_PHENO=1` prints each dinosaur's genotype, length and class.

`--selftest` checks the capacity maths, the cost table, the duplicate rule,
filling and the no-room rejection.

Open: the purchase path (store charge to `DINOACHAT`, store-row removal) and
the `WARNING_NO_ROOM_IN_FENCE` panel path. The per-individual
`dinosaur+0x1e0` size class is solved and ported; see
`notes/paddock-size-class-runtime.md`.
Per-paddock numbers and the all-campaign scan:
`notes/paddock-capacity-verification.md`.

## Building construction (P4b)

The retail panel path is wired end-to-end:

```text
OPEN_CONSTRUCTION (right panel) -> OPEN_BUILDING -> category tab
  -> CONSTRUC_*_BUTTON (from the `.bbi` tail) -> click the ground
```

`ParkHud::click` exposes the category tabs (`CONSTRUCTION_BUILDINGS_BUTTON`,
`CONSTRUCTION_BARAQUES_BUTTON`, ...) and every `CONSTRUC_*_BUTTON` as a
`construct <widget>` action; the viewer resolves the widget to its
`BuildingList.ini` index by scanning the `.bbi` `tail_strings`.

The charge is **`.bbi` file offset 21** (`D+0x18`), proven by
`pvwStateBuildingConstruction` (`0x5306e1` gate, `0x5307ac`
`FUN_00585d20`); file offset 25 is the visitor entry price, not the build
cost. The port checks the footprint against existing buildings and the
island bounds, then deducts the cost from `park_budget` (the HUD
`BUDGET_AMOUNT` follows immediately). A failed check prints the retail
category name (`WARNING_NOT_ENOUGH_MONEY`) and places nothing.

```text
--construct NAME        select by BuildingList name or CONSTRUC_*_BUTTON
--construct-pos X,Y     place the selected building (repeatable, map units)
--park-budget N         override the start budget (tests/debug)
```

`--park-click 748,177 --park-click 582,28 --park-click 577,243
--construct-pos 600,300` selects `RestCirc` through the panel and places it:
cost 2,500, budget 75,000 -> 72,500. A second placement at the same cell is
rejected as overlapping.

## Dinosaur purchase

The store catalog (`Market.ini`, 54 entries) and the paddock model are wired
together for a headless purchase:

```text
--buy-dino NAME        a Market.ini display name (e.g. Zarbosaurus)
--buy-paddock N        optional paddock index; default first with room
```

The flow mirrors `pvwStateFenceInsertion`/`stFence`:

1. resolve the full 18-gene genotype from the catalog, map its dominant head
   gene to the 20 base species and derive the phenotype size class;
2. find a paddock with `capacity - used >= 8 << class` (a missing/duplicate
   entry prints `WARNING_NO_ROOM_IN_FENCE` and does not charge);
3. check `park_budget >= price` (`WARNING_NOT_ENOUGH_MONEY`);
4. spawn the dinosaur, add its cost to `used`, and deduct the price.

Cpg04: `--buy-dino Zarbosaurus --park-budget 10000` -> price 4,900,
budget 5,100, 21 dinosaurs, paddock used 352 (336 + the class-2 cost).
Both failures show the retail warning panel when the park HUD is loaded
(`WARNING_NO_ROOM_IN_FENCE` -> "There is not enough room in the
enclosure.", `WARNING_NOT_ENOUGH_MONEY` -> "Not enough money!";
`notes/buy-warning.png`).

The interactive `DINO_ACHAT_SAVE_PANEL` uses the same core: six-row
viewport/selection (`src/ui/park.*`), SAVE checks the budget and arms the
insertion, the ground click runs `paddock_can_add`, charges the price, feeds
`BudgetLedger::spend_dinoachat`, places the dinosaur and removes the catalog
row (`notes/store-panel-recon.md`, `notes/store-panel.png`).

## Feeding

`--place-food NAME|KIND` and `--food-pos CELL_X,CELL_Y` place a `.bsi` food
source; the `.bai` consume gate then hides Eat unless the dinosaur's paddock
holds a matching source, and each meal consumes a portion. See
`docs/platform/feeding.md`.

## CLI (also used by tests)

```text
--build                  start with the path tool
--build-cell N           cell size (default 4)
--build-load FILE        load a placement file
--build-save FILE        save placements on exit
--place-path X,Y         add a path cell (cell coordinates, repeatable)
--place-fence X,Y,E      add a fence edge, E=0 south / 1 east (repeatable)
--pick-test X,Y          print the terrain cell under a pixel on frame 0
--selftest               run grid/visitor logic checks and exit
```

`--selftest` builds a grid, toggles paths/fences, checks undo and world→cell
mapping, spawns visitors and runs the SIM-2b scorer, checks the dinosaur
action chooser and the paddock capacity/cost/duplicate/fill rules, and checks
the baked mesh counts. It prints `selftest: PASS (N checks)`; pytest asserts
the count.

Placement file (`notes/build-test.txt` style):

```text
cell_size 4
size 256 256
path 128 128
fence 128 128 0
path 129 128
```

`--build-load` recreates the grid from `size` × `cell_size`; unknown or
out-of-range cells are ignored.

## Evidence

```text
build: cell 4.0, grid 256x256, 2 path cells, 1 fences; mesh 2 tiles, 1 segments
```

- `notes/build-preview.png` — L-shaped path + fence line, allosaurus on the
  path.
- `notes/build-preview2.png` — wider view of the same layout.
- `tests/test_viewer_cpp.py`:
  - `test_cpp_build_save_load` (counts, file contents, reload)
  - `test_cpp_pick_terrain` (ray pick returns a cell)
  - `test_cpp_occupation_mask_blocks_placement` (blocked isl01 cell rejected)
- `src/world/build.cpp`, `src/world/build.h`, `src/world/terrain.cpp`
  (`Terrain::buildable`, mask load).
- `notes/raw-recon.md`, `notes/occupation-masks.png` — the `.raw` mask
  (0 = buildable, 255 = blocked).

## Open items

- Validation rules beyond the occupation mask: steep slopes and dinosaur
  pens.
- Costs/limits and a HUD (needs the overlay/UI layer).
- Path connectivity / visitors (SIM-2).
- Per-triangle fence material split (material 1 is `laser2.tga`, 8 triangles).
- Retail fence warning codes, perimeter price and the LunchBox/observation
  feature path (decoded; `docs/decomp/construction-validation.md`).
- The paddock fence is 8.8 m tall; other wall types (`.bwi`: LowWood,
  HighSteel, …) need their own mesh generation.
