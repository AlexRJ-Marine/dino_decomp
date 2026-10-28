# `FENCE_CONSTRUCTION` — enclosure/wall tool (2026-10-28)

Status: **state machine, validators, warning table and cost path decoded.**
The selected-list row semantics, the wall rasteriser and two extended vtable
hooks remain open.

Complements `notes/fence-insertion-recon.md` (paddock capacity / dinosaur
insertion) and `docs/decomp/construction-validation.md`. This is the tool that
opens from `OPEN_CONSTRUCTION_FENCE`, draws a paddock perimeter in two clicks
and places the fence-page feature (LunchBox / observation area).

## State class

| Item | Address |
| --- | --- |
| constructor | `0x5265b0` |
| state name | `FENCE_CONSTRUCTION` (`0x663024`) |
| panel | `FENCE_CONSTRUCTION_AND_INFO_PANEL` (`0x663000`) |
| vtable | `0x642b4c` |

The vtable has 16 slots. The first eight are the common `pvwState` interface;
the rest are the fence-tool hooks:

| Slot | Address | Role |
| --- | --- | --- |
| +0x00 | `0x526900` | dtor |
| +0x04 | `0x526930` | enter |
| +0x08 | `0x5269c0` | leave (hides the panel) |
| +0x0c | `0x52ad00` | update/preview |
| +0x10 | `0x52b590` | mouse move/notification (cursor point, preview) |
| +0x14 | `0x52b0d0` | click/apply |
| +0x18 | `0x608100` | no-op |
| +0x1c | `0x52be60` | key handler (`N`/`J` rotate, Esc leaves) |
| +0x20 | `0x508d30` | base no-op |
| +0x24 | `0x52bfc0` | rotate: `+0x3c = (+0x3c +/- 1) & 3` |
| +0x28 | `0x527030` | feature limit: LunchBox class + 16-entry cap -> `&WARNING_TOO_MANY_LUNCHOXES_IN_FENCE` |
| +0x2c | `0x40c1d0` | helper |
| +0x30 | `0x527150` | perimeter preview/validate (orientation, 16-edge limit) |
| +0x34 | `0x526e70` | feature preview: LunchBox uses `+0x1b8` as height, others `0x5f85f0`/`0x5f8790` |
| +0x38 | `0x526f20` | perimeter apply (validate, cost, budget, wall record) |
| +0x3c | `0x608100` | no-op |
| +0x40 | `0x527370` | wall-record builder / orientation |

### Bound panel widgets

`0x5265b0` resolves the panel and its stats widgets:

```text
+0x84  FENCE_FEATURE_PRICE
+0x8c  FENCE_CONSTRUCTION_AND_INFO_PANEL
+0x90  FENCE_STATS_DINO_QUANTITY_INDICS_BACK_CHIFFRE
+0x94  FENCE_STATS_OCCUPATION_INDICS_BACK_CHIFFRE
+0x98  FENCE_STATS_CARNI_DISTRIBUTION_INDICS_BACK_CHIFFRE
+0x9c  FENCE_STATS_HERBI_DISTRIBUTION_INDICS_BACK_CHIFFRE
+0xa0  FENCE_NAME_INDICS
```

`SWITCH_OBSERVATION_AREA` gets callbacks `LAB_00531b60` / `LAB_00531b80`.

### Fields

| Offset | Meaning |
| ---: | --- |
| `+0x38` | click-consumed flag |
| `+0x3c` | feature orientation 0..3 (rotated by `0x52bfc0`) |
| `+0x40` | preview/helper collection (cleared by `0x52caa0`) |
| `+0x44` | shared construction tool: `+0x8` park scene, `+0x10` park, `+0x14` scene/terrain |
| `+0x48` | feature-placement mode (set by `0x52bbb0` on list selection) |
| `+0x4a` | two-click perimeter mode; first corner at `+0x64/+0x68` |
| `+0x4c` | selected feature index |
| `+0x50` | selected feature footprint size (passed as the margin) |
| `+0x54` | feature preview visible |
| `+0x5c` | wall preview visible |
| `+0x64/+0x68` | first perimeter corner |
| `+0x78` | selected feature record / terrain origin |
| `+0x7c` | park scene grid |
| `+0x88` | selected feature object (`+0x4c` = class-tag string) |

## Flow

### Enter / refresh

`0x526930` reads the current UI list selection (`0x447b90`/`0x447c30`) and
compares it with `0x17` (23), the **wall element type** created by
`0x53ac60` (the show-element factory `0x53acd0` also creates types
0x16/0x17; see `notes/dino-target-vtable-recon.md`). On a match it calls
`0x5269e0` (title/price), `0x52c1f0` (paddock dino stats) and `0x526d70`
(feature list); otherwise `0x52bb70` resets the preview
(`+0x48 = +0x4a = 0`, `0x5f8590(8.0f)`, `0x5f6a10(1)`). So the fence tool
refreshes its panel only when the selected element is a wall, and resets the
preview when the selection is anything else.

- `0x5269e0` formats the panel title from the selected feature name,
  appends `%u` (`0x64d894`) and `"%"` (`0x663038`), and copies the result
  into the name/price widgets.
- `0x52c1f0` walks the park paddock list (`+0x8c..+0x90`, 0x1f4-stride) and
  updates the dino-quantity/occupation stat widgets.
- `0x526d70` walks the 62 `.bfi` records (`gdt+0x7c`, 0x1d8 stride) and adds
  the names whose `+0x1d4` byte is set to the fence feature list. The loader
  `0x53ed30` reads the `.bfi` final byte into `+0x1d4` and hardcodes
  `+0x1d5 = 1`; the byte is the `tail_flag` (1 only for `LunchBox`, verified
  across all 62 files). So the list has **one row (LunchBox)**; the walls /
  observation area are the separate `SWITCH_OBSERVATION_AREA` toggle, not a
  list row.

Selecting a feature calls `0x52bbb0`: reads the UI selection into `+0x78`,
sets `+0x48 = 1` (feature mode) and prepares the preview.

### Mouse move

`0x52b590` stores the ground point into `+0x64/+0x68` and calls
`0x5f8700`/`0x5f8790` to move the feature preview; when the notification's
`+0x10` is non-zero it toggles the preview through `vtable+0x24` (rotate).

### Click (`0x52b0d0`)

```text
arg != 0:
    close preview / leave (0x52b501 -> 0x52b562 -> state 5 + vtable+8)

+0x48 (feature mode):
    0x52b242:
        ground = 0x5f8b20(scene)
        rect   = 0x52bcb0(ground)
        if 0x5812b0(scene, rect, orientation, size) != 0 -> warn
        if vtable+0x30() != 0 -> warn                    // 0x527150
        if 0x52bed0(cost) == 0 -> WARNING_NOT_ENOUGH_MONEY
        vtable+0x3c()                                    // no-op
        0x53acd0(...)   // create .bfi feature actor
        0x53b0d0(...)   // animal-bait actor (AnimalBait only)
        vtable+0x40()

+0x4a == 0 (first perimeter corner):
    ground = 0x5f8a80(scene)
    point  = floor(ground)
    code   = 0x58a190(scene, point, margin)
    if code != 0: 0x52c2e0(code) -> warn
    +0x64/+0x68 = point
    0x5f85c0(...); 0x5f8700(...)
    +0x4a = 1

else (second corner):
    0x52bbf0(&rect)          // sort the two corners, max-1
    if vtable+0x38() != 0:   // 0x526f20 -> apply perimeter
        0x5f8830(); tool+0xc->0x5f6a10(0); +0x48 = 1; 0x52cc20()
    else:
        reset preview (+0x78/+0x48/+0x4a = 0, 0x5f8590(8.0f))
```

The two modes are exclusive: `+0x48` is set by feature selection, `+0x4a`
is the first-corner state for the wall perimeter.

## Perimeter apply (`0x526f20`)

```text
if 0x58a4e0(scene, rect, margin) != 0:
    0x52c2e0(code) -> warn, return 0

cost = 0x581b90(p1, p2, config)          // perimeter length -> price
if 0x52bed0(cost) == 0:                  // WARNING_NOT_ENOUGH_MONEY
    return 0
0x585d20(park_budget, cost)              // CONSTRUCTION: budget -= cost,
                                         // count++, total += cost
wall = 0x53ac60(tool+0x8, p1, p2)        // type 0x17 record with the two
                                         // corners at +0x18..+0x24
select(wall); 0x5269e0()                 // refresh the panel
return 1
```

`0x585d20` is the same CONSTRUCTION budget setter used by the road commit
(`+4 = 1`, `+0xa4++`, `+0xa8 += cost`, `+8 -= cost`).

## Perimeter preview (`0x527150`)

- If the park wall/segment vector (`park+0xd8 .. +0xdc`) has 16 or more
  entries, returns **6** (no warning string is defined for 6 in `0x52c2e0`).
- Reads the two points, calls `0x581610` (edge vector) and `0x581520`
  (orientation), then sets `+0x14` to 0..3:
  `-4` -> 2, x == width-3 -> 0, `-4` on y -> 3, y == height-3 -> 1.
- `0x527370` is the record/orientation builder for the same wall list and
  checks the `"Lunchbox"` class tag (`0x6630d0`).

## Validators

| Function | Test | Result |
| --- | --- | --- |
| `0x58a190(scene, point, margin)` | point-rect inside an obstacle at `scene+0x50..+0x54` (rect `+0x1c..+0x28`) | 3 |
| | cell scan `0x58a390` | 0 free |
| `0x58a4e0(scene, rect, margin)` | width/height smaller than `4*margin - 1` | 4 |
| | rect inside an obstacle | 3 |
| | cell scan `0x58a390` | 0 free |
| `0x5812b0(scene, point, orientation, size)` | overlap against `scene+0x34..+0x38` rects (`+4/+8/+0xc/+0x10`) | 1 |

`0x52c2e0(code)` maps the codes to the retail messages:

| Code | String |
| ---: | --- |
| 1 | `&WARNING_BUSY_SPACE` (`0x66350c`) |
| 2 | `&WARNING_UNEVEN_FLOOR` (`0x6634f4`) |
| 3 | `&WARNING_INSIDE_PERIMETER` (`0x6634d8`) |
| 4 | `&WARNING_PERIMETER_TOO_SMALL` (`0x6634b8`) |
| 5 | `&WARNING_ROAD_TOO_SHORT` (`0x6634a0`) |
| 6 | none |

Failure keeps the panel open; the warning is displayed through `0x5e3d70`.

## Cost

`0x581b90(p1, p2, config)` is the perimeter price. The actual x86 is:

```asm
; EAX = p1.x + p1.y
NEG EAX
SHL EAX,0x1f
SUB EAX,ECX          ; ECX = p1.x + p1.y
ADD EAX, p2.x + p2.y
SHL EAX,1
DIV config[0]        ; cell size
IMUL EAX,config[4]   ; price per cell
```

The `NEG/SHL 31/SUB` sequence is the signed `- (p1.x + p1.y)` with the
compiler's sign correction; it is not a `0x7fffffff` multiplier (Ghidra
renders it that way). Test values confirm the formula:

```text
p1=(1,1) p2=(5,5) cell=4 price=10
  (5+5 - 1-1) * 2 / 4 * 10 = 20
```

So:

```text
cost = ((p2.x + p2.y) - (p1.x + p1.y)) * 2 / cell * price_per_cell
```

which is the rectangle **perimeter** in cells times the per-cell price
(`config[0]` = 4, `config[4]` = the `FENCE_FEATURE_PRICE` value). The caller
`0x526f20` sorts the corners through `0x52bbf0` before calling.

## Key handler (`0x52be60`)

- `N` (0x4e) -> `vtable+0x24(1)` (rotate clockwise)
- `J` (0x4a) -> `vtable+0x24(0)` (rotate counter-clockwise)
- `Esc` (0x01) -> state 5 + `vtable+8` (leave); if already 5, `tool+0x1c`
  `0x50be00(8)`.

## Wall perimeter (`stPerimeter` / `stWall`)

The wall record created by `0x526f20` (`0x53ac60`, type 0x17, two corners) is
consumed by the `stPerimeter` rasteriser:

| Function | Role |
| --- | --- |
| `0x584090` | `stPerimeter` ctor: stores the rectangle at `+0x1c..+0x28`, the wall type at `+0x2c`, vtable `0x643530`, and calls `0x582210` four times for the four sides |
| `0x582210` | `stPerimeter::AddSegment`: allocates a 0x24-byte `stWall`, calls `0x584200`, and appends the pointer to the vector at `+0x10..+0x18` (assert path `stPerimeter.cpp:0x5e`) |
| `0x584200` | `stWall` rasteriser: stores corners `+0..+0xc`, wall type `+0x10`, vtable `0x64353c`, allocates `length/cell` 0x28-byte cell records at `+0x18` (`0x584700`), walks the line (Bresenham; `0x584520` axis case, `0x55d590`/`0x55d600` horizontal/vertical, generic loop for diagonals), and finalises each cell with `0x5849a0` |
| `0x584700` | cell object ctor: `+0x00` owner id (`*(param_2+8)`), `+0x04` vtable `0x642f74`, `+0x08/+0x0c/+0x10` vector begin/end/cap of 8-byte `(x,y)` pairs, `+0x14` wall type (`param_5`), `+0x18` zero at ctor (not read by `0x5849a0`), `+0x1c` add/remove flag, `+0x20` grid, `+0x24` source object (`param_2`) |
| `0x584520` | axis-aligned callback: appends a cell every `cell_size` along the segment (`+0x10` index, `+0x14` count-1), then `0x5848c0` |
| `0x5849a0` | occupancy commit: for flag 0/1 sets `u16 = (u16 & 0x0fff) \| (type << 12)`; otherwise clears the high nibble (`u16 &= 0x0fff`) |
| `0x580f00` | show-arena ctor: calls `0x584090` with the arena corners, cell size and `.shw` `Wall type`, then `0x576860` zero-fills the show score vector to the profile count |

The occupancy grid stores one `u16` per world unit; the **high nibble is the
occupant type** (`DAT_006757b4` for walls, `DAT_00675a60` for roads per the
road recon), the low 12 bits are flags. `0x5849a0` stamps or clears that
nibble for every cell of the segment. This is why the road/fence validators
test `u16 >> 12` and why a player-built wall blocks a road even though the
port models fence edges separately.

So a show arena and a player-built paddock share the same `stPerimeter` ->
four `stWall` segments -> grid cells model. The port stores fence **edges**
instead of rasterised wall cells, which is the documented approximation.

### Paddock list panel

`FUN_00511270` is the `FENCE_LIST_PANEL` ctor:

- `FUN_005e3240(5, FENCE_LIST_PANEL, FENCE_LIST_SCROLL_UP/DOWN)` -> five
  visible rows.
- It resolves `FENCE_BACKGROUND_SLOT`, `FENCE_NAME_SLOT`,
  `FENCE_SNAPSHOT_SLOT` per row and a 0x14-byte row record array, then fills
  each row's `FENCE_LIST_DINO_INDICATEURS_PANEL_SLOT`,
  `FENCE_LIST_QUANTITY_SLOT`, `FENCE_LIST_POURCENT_SLOT`,
  `FENCE_LIST_HERBIQUANTITY_SLOT` and `FENCE_LIST_CARNIQUANTITY_SLOT`.
- `param_1[0x11] = park+0x14+0x9c` is the paddock vector; `0x5e36e0(row,0,0,0)`
  initialises the row state.

Panel vtable `0x642874`: `+0` dtor `0x511740`, `+4` show/hide `0x511860`
(calls base `0x5e38c0` + `0x5bbeb0`), `+8` `0x511d50`, `+0xc` `0x511890`,
`+0x10` `0x587340`, `+0x14` `0x511d40`. `0x511d50` returns whether the
selected row differs from the paddock count.

The list base is `FUN_005e3240(count, panel, scroll_up, scroll_down)`: it
stores the visible-row count (`5`), resolves the list/scroll widgets, and
allocates `count * 0x30` row records at `+0x1c`. `FUN_005e36e0(row, a, b, c)`
fills one row's three widgets at `+0x14/+0x18/+0x1c/+0x20` (the fence panel
passes `FENCE_BACKGROUND_SLOT`, `FENCE_NAME_SLOT`, `FENCE_SNAPSHOT_SLOT`), and
`FUN_005e38c0` toggles a row's visibility and installs the selection callback
`LAB_005e3930` on `[row+0x18]`.

The per-frame fill is `0x511890` (vtable `+0xc`): it calls the base
`0x5e39c0`, computes the visible range from the paddock count
(`[panel+0xc]`) and scroll (`[panel+0x10]`), then for each visible row takes
the paddock from `[panel+0x44]+4`, calls `0x57e720(paddock, ...)` and fills
`FENCE_NAME_SLOT` through `0x5bb940`, followed by `%u` strings for the
quantity/percent/carni/herbi widgets. `0x511d50` (vtable `+8`) is the dirty
check: it returns whether the selected row (`[panel+0xc]`) differs from the
paddock count.

`0x57e720(paddock, scale, out)` produces the row stats:

```text
out[0] = occupant count            ([paddock+0x9c] - [paddock+0x98]) / 4
out[1] = count scaled by the passed factor
out[2]/out[3] = herbivore/carnivore percentages
    each occupant contributes scale * [dino+0x6c]; occupants with
    [dino+0x200] == 1 go to the first bucket; an empty bucket is 100%
```

The snapshot is a normal row widget; its texture/render source is not
isolated yet.

### Preview look (retail capture, 2026-10-28)

The enclosure tool draws a **filled circular ground disc**, not a ring:
solid blue ~RGB(38,38,136) when placeable and dark maroon ~RGB(80,38,38)
when not (inside the selected paddock), hard edge, no bright rim, z-tested
on the ground. It snaps to the build cell under the cursor (capture: cursor
350,450, disc centre ~407,455 after two zoom-out clicks). The disc is darker
than the road helper blue (64,64,249 at a different pulse phase), so the two
previews do not share a colour. Evidence:
`notes/road-enclosure-preview-capture.md`,
`notes/harness/enclosure-preview-valid.png`,
`notes/harness/enclosure-preview-invalid.png`.

## Selling a fence

`&REQUEST_SELL_FENCE` (`0x663d4c`) is one branch of the shared sell handler
(`0x532d9c..0x533a70`, which also serves `REQUEST_SELL_DINO`,
`REQUEST_SELL_FEATURE`, `REQUEST_SELL_SHOW`, `REQUEST_SELL_BUILDING` and
`REQUEST_SELL_ROAD`). The fence branch:

```text
object = 0x53c2d0(first selected row)
occupants = [object+0x4c]+0x98 .. +0x9c
if occupants is not empty:
    show &WARNING_SELLING_NON_EMPTY_FENCE (0x663d28)
    return without removing

value = 0x58acc0(park, object)
        object->vtable+0x14()                       // base value
      + sum(entry[+0x40]+0x18 for park+0xb0.. entries whose +0xc4 == object)
refund = int(value * *(float*)(park+0x94 + 0x98))   // park sellback factor
format "&REQUEST_SELL_FENCE" + "%d" (0x64c6cc)
0x533a70(...)                                       // remove the object
```

So a paddock with a dinosaur cannot be sold, and an empty one refunds
`base value * sellback`. The port currently removes fences without either
gate or refund.

## Open

- Runtime-confirm that the focused object in `0x526930` is the wall element
  (type `0x17` = the `0x53ac60` wall record / show-element type). Statically
  it matches; the selection object's vtable+4 was not dumped live.
- ~~Exact decomposition of the `0x581b90` cost magic~~ **resolved:** the
  signed `NEG/SHL 31/SUB` is `-(p1.x+p1.y)`; cost is
  `((p2.x+p2.y)-(p1.x+p1.y))*2/cell*price` (perimeter cells * price).
- ~~`0x527030` / `0x526e70` extended hooks~~ **resolved:** `0x527030` is the
  LunchBox 16-entry cap (`&WARNING_TOO_MANY_LUNCHOXES_IN_FENCE`), and
  `0x526e70` is the feature preview update.
- `0x584700` `+0x18` / `0x5849a0` finalisation beyond the mapped fields.

## Evidence

- Ghidra: `0x5265b0` ctor; vtable `0x642b4c` slots through `0x642bac`;
  `0x526930` enter, `0x5269c0` leave, `0x52ad00` update, `0x52b590` move,
  `0x52b0d0` click, `0x52be60` keys; `0x52bfc0` rotate; `0x526f20` apply;
  `0x527150` preview; `0x527370` wall builder; `0x52c2e0` warnings;
  `0x58a190`/`0x58a4e0`/`0x5812b0` validators; `0x581b90` cost;
  `0x585d20` CONSTRUCTION budget; `0x53ac60` wall record; `0x53acd0`
  feature actor; `0x53b0d0` animal bait; `0x5269e0` title; `0x52c1f0`
  stats; `0x526d70` feature list.
- Strings: `FENCE_CONSTRUCTION` `0x663024`,
  `FENCE_CONSTRUCTION_AND_INFO_PANEL` `0x663000`,
  `FENCE_FEATURE_PRICE` `0x662fec`, `FENCE_NAME_INDICS` `0x662fd8`,
  `FENCE_STATS_*` `0x662f7c`/`0x662fa8`/`0x662f14`/`0x662f48`,
  `SWITCH_OBSERVATION_AREA` `0x662efc`, `WARNING_*` `0x6634a0..0x66350c`,
  `LunchBox` `0x66303c`, `Lunchbox` `0x6630d0`.
- `docs/decomp/construction-validation.md`, `notes/fence-insertion-recon.md`.
