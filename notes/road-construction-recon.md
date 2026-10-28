# Road construction (`ROAD_CONSTRUCTION`) recon

Status: **decompiled** (static, Ghidra 12.2 + capstone sweeps of the
unpatched `DinoIsland.exe`); not yet runtime-traced. Labels follow
`notes/handoff.md`: everything below is *decompiled* unless marked
*inferred*.

## Entry

```text
OPEN_TRANSPORT -> CONSTRUCTION_ROAD_METRO_PANEL
  CONSTRUC_ROAD_BUTTON   handler 0x52e100 (click) / 0x52e210
  CONSTRUC_FARM_BUTTON   handler 0x52e1d0 / 0x52e350   (.bbi category 8)
  CONSTRUC_METRO_BUTTON  handler 0x52e1f0 / 0x52e4d0   (.bbi category 9)
  CONSTR_PRICE_SLOT      price readout
  PUTBACK_PANEL_BUTTON   0x52fe10
```

`FUN_0052de80` is the state constructor: `FUN_0052bd10(0x10,
"ROAD_CONSTRUCTION")`, vtable `0x642c60`, binds the widgets above and scans
the 500-byte building definitions (`gdt+0x8c..+0x90`) for category 8 (Farm)
and 9 (Metro), storing their indices at `+0x64`/`+0x68`.

Vtable `0x642c60`:

| Slot | Address | Role |
| ---: | --- | --- |
| 0 | `0x52e650` | dtor |
| 1 | `0x52e6b0` | enter (clears `+0x48/+0x49/+0x4a`, `+0x6c` = preview helper) |
| 2 | `0x52e710` | leave |
| 3 | `0x52e750` | per-frame update (`ret 4`) — **not a Ghidra function** |
| 4 | `0x52ee40` | mouse move |
| 5 | `0x52e9e0` | mouse button (`ret 8`: button, point) — **not a Ghidra function** |
| 6..9 | `0x608100`, `0x52be60`, `0x508d30`, `0x52bfc0` | shared base slots |

State flags: `+0x48` road tool armed, `+0x49` segment in progress (start
placed), `+0x4a` farm/metro ghost active. `+0x50/+0x54` current point.

## Preview helper (`helper_road.mesh`, object at state `+0x6c`)

- `FUN_005f8670`: road mode (`+0xcc = 9`), snap step `+0x14 = 4.0f`,
  end `+0x7c/+0x80` := start `+0x74/+0x78`.
- `FUN_005f8700(point)`: snap the cursor (`FUN_005f8c70` -> `FUN_005faaa0`,
  4-unit step) and store it as the helper position.
- `0x5fa5b0..0x5fa64a` (road update): end := snapped cursor, then
  **axis lock**: if `|end.y - start.y| < |end.x - start.x|` then
  `end.y = start.y` else `end.x = start.x`. Roads are straight,
  axis-aligned segments. The min/max that follows only builds the preview
  quad; the stored start/end stay unordered.
- `FUN_005f8830`: start := end, mode 0 (segment finished, chain from here).
- `FUN_005f8a70(ok)`: preview colour flag `+0x34` (valid / invalid).

## Per-frame update `0x52e750` (while `+0x48` and `+0x49`)

```text
FUN_0052f0d0(&start, &end)            ; ftol of helper +0x74/+0x7c
len  = (end.x - start.x) + (end.y - start.y)
cost = (len * gdt[+0x1a4]) >> 2       ; shr, unsigned
show &MESSAGE_ROAD_COST "%d" cost     ; FUN_005e4970 cursor message
ok   = spRoadNetwork::Validate(start, end, fences) == 0     ; 0x58cfd0
helper.SetValid(ok)                   ; 0x5f8a70
```

`gdt[+0x1a4]` is the u32 `10` of `GeneralParams.bgd`: the game-data ctor
`0x53d4d0` calls the loader `0x542f80` on itself (`ecx = esi`), which reads 4
bytes into `[esi+0x1a4]` at `0x543128`. The state reaches the same object as
`[[state+0x44]+8]+0x34` (*inferred* from the shared building list offsets).
Because start/end are snapped to 4 units, a straight road of N cells costs
**`10 * (N - 1)`**.

*Inferred quirk:* `len` is signed and not abs'd, so a segment drawn toward
-x/-y gives a negative product that `shr` turns into ~1e9 and the money gate
rejects. Needs a runtime check before anyone relies on it; the port uses
`|dx| + |dy|`.

## Mouse button `0x52e9e0` (`button`, `point`)

Left button (`button == 0`), road tool armed:

1. **No segment yet** (`+0x49 == 0`): take the snapped helper position;
   `spRoadNetwork::ValidateStart(start, fences)` (`0x58ce80`); on 0 store it,
   `FUN_005f8670` + `FUN_005f8700`, set `+0x49 = 1`; otherwise
   `FUN_0052c2e0(code)`.
2. **Segment in progress**: endpoints via `0x52f0d0`;
   `code = 0x58cfd0(start, end, fences)`; non-zero -> `FUN_0052c2e0(code)`
   and stay. Then `cost` as above; `FUN_0052bed0(cost)` (affordability,
   shows `WARNING_NOT_ENOUGH_MONEY`, does not deduct); on success:
   - `spRoadNetwork::AddRoad(start, end, 1)` (`0x58d110`, allocates a 0x2c
     road via `0x5943f0`, splits/merges overlapping roads `0x58d890`,
     `0x58d9d0`, inserts sorted);
   - `0x5207b0` on `[+0x44]+0x18` (AI/behaviour side; *inferred*: path
     network changed);
   - `[[+0x44]+0xc]+0xec = 1` (scene dirty, *inferred*);
   - **`FUN_00585d20(cost)` — the CONSTRUCTION budget bucket** (answers the
     open question in `docs/decomp/construction-validation.md`: roads do
     charge CONSTRUCTION);
   - `FUN_005f8830` (chain: start := end), `FUN_005f8670`, `FUN_005f8700`,
     hide the cost message (`0x5e4a90`). `+0x49` stays 1.

Other button (`0x52ed6a`):

- segment in progress -> cancel it (`+0x49 = 0`, helper start-only
  `0x5f8650`), tool stays armed;
- tool armed, no segment -> disarm (`+0x48 = 0`, `0x52cc20`);
- farm/metro ghost -> cancel ghost;
- otherwise -> leave the state (`[+0x18] = 5`, vtable `+8`).

## Validation (`spRoadNetwork`, `0x58ce80` / `0x58cfd0`)

Return codes feed `FUN_0052c2e0`, the shared construction warning helper:

| Code | Warning | Rule |
| ---: | --- | --- |
| 3 | `&WARNING_INSIDE_PERIMETER` | the road's box lies strictly inside a fence rectangle: `f.x0 < x0 && f.y0 < y0 && x1+3 < f.x1 && y1+3 < f.y1` (fences at `[entry+0x1c]+0x1c..+0x28`) |
| 5 | `&WARNING_ROAD_TOO_SHORT` | `start == end` (`0x58cfd0` only) |
| 1 | `&WARNING_BUSY_SPACE` | any cell of `[x0..x1+3] x [y0..y1+3]` (inside the map) whose occupancy `u16 >> 12` is neither 0 nor the road type `DAT_00675a60` (runtime-initialised; reads 0 in the file) — functor vtable `0x6435cc`, `FUN_0058cf90` |

The occupancy grid is `[spRoadNetwork]`: `+0` width, `+4` height, `+0x10`
column pointers, `u16` per world unit. Roads may cross or extend roads; they
may not cover buildings or other occupants. There is **no slope test** for
roads (`WARNING_UNEVEN_FLOOR`, code 2, belongs to the building path), and
`WARNING_PERIMETER_TOO_SMALL` (4) is the fence path.

## Storage

A road is a rectangle `[start .. end+3]` in world units (0..1024), a 4-wide
strip. `spRoadNetwork::Save` (`0x58cd60`) writes the count plus 16-byte
rectangles, matching `docs/formats/sav.md`.

## Selling a road (2026-10-28)

`&REQUEST_SELL_ROAD` (`0x663cfc`) is the road branch of the shared sell
handler (`0x532d9c..0x533a70`, the same block as the fence/building/show/
feature/dino branches). `0x58d4b0` resolves the road under the cursor; when it
returns null the request is ignored. The refund reuses the build formula:

```text
length = (x1 - x0) + (y1 - y0)      // road rectangle at +0x0/+0x4/+0x8/+0xc
refund = (length * GeneralParams[+0x1a4]) >> 2
```

`GeneralParams[+0x1a4] = 10`, so the refund is the same per-cell price as the
build (no sellback multiplier in this branch). The value is formatted into the
localised `&REQUEST_SELL_ROAD` text with `%d`, then the removal callback runs.
There is no slope or occupancy gate for selling.

## Panel wiring (2026-10-28)

`0x52de80` is the `ROAD_CONSTRUCTION` state ctor (`FUN_0052bd10(0x10,
"ROAD_CONSTRUCTION", ...)`):

```text
+0x70  CONSTRUCTION_ROAD_METRO_PANEL
+0x74  CONSTR_PRICE_SLOT
+0x78  PUTBACK_PANEL_BUTTON        -> LAB_0052fe10
       CONSTRUC_ROAD_BUTTON        -> LAB_0052e100  (arms the road tool)
       CONSTRUC_FARM_BUTTON        -> LAB_0052e210 / LAB_0052e1d0
       CONSTRUC_METRO_BUTTON       -> LAB_0052e1f0 / LAB_0052e4d0
```

The ctor scans the scenario building list (0x500-byte stride) for kinds 8/9
and stores their indices at `+0x64`/`+0x68` (Farm/Metro). `0x52e6b0` (enter)
resets `+0x48/+0x49/+0x4a`, sets `+0x6c` from `tool+0xc+0x40`, shows the
panel and calls `0x52c1f0`.

## Road button body `0x52e100`

The `CONSTRUC_ROAD_BUTTON` notification handler (only acts when the
notification argument is 0):

```text
state = owner+0x5c
0x5f8650(state+0x6c)            // clear the preview helper
0x5f8700(state+0x6c, state+0x50) // set the preview start point
0x52cc10(state+0x40, 0)          // reset the tool selection
state+0x48 = 1; state+0x49 = 0
0x5f6a10(tool+0xc, 1)            // enable the helper
update CONSTR_PRICE_SLOT via 0x500560
```

So the retail button arms the straight-run state, places the helper at the
current ground point and refreshes the price slot; it does not commit a road.

## Not yet decoded

- Runtime confirmation of the price chain and the negative-direction quirk.

## Evidence

- Strings: `ROAD_CONSTRUCTION` `0x663784`, `CONSTRUC_ROAD_BUTTON` `0x663720`,
  `&MESSAGE_ROAD_COST` `0x663798`, `&WARNING_ROAD_TOO_SHORT` `0x6634a0`,
  `&REQUEST_SELL_ROAD` `0x663cfc`, `helper_road.mesh` `0x670458`.
- Functions: `0x52de80`, `0x52e6b0`, `0x52e750`, `0x52e9e0`, `0x52ee40`,
  `0x52f0d0`, `0x52c2e0`, `0x58ce80`, `0x58cfd0`, `0x58cf90`, `0x58d110`,
  `0x5f8670`, `0x5f8700`, `0x5f8830`, `0x5f8a70`, `0x5f8c70`,
  `0x5fa5b0`, `0x53d4d0`, `0x542f80`.

## Rendering (2026-10-27): roads are painted into the landscape texture

Runtime: a retail Cpg04 park load (14 roads) under x32dbg with a
`CreateFileA` log (`scripts/x32dbg/start-campaign.ps1 -PhysicalInput
-UnlockCampaigns -FileTrace -NoTrace`; log
`notes/harness/x32dbg-cpg04-files.out.log`, `PARKREADY` reached) opens **no
road texture**: the only terrain images are `basemap_04.tga` and
`detailmap_04.tga`. `focus/road.tga` is the placement helper's 32x32 glow;
`buildings/ground.tga` / `groundalpha.tga` are referenced nowhere.

Static: `gpkLandscapeTextureManager` (source string `0x670b08`, ctor block
`0x603159..0x603224`) renders the island texture (`FUN_00603430`, re-run
when the scene dirty flag `+0xec` is set, which the road commit raises) from
four layers: `+0x24` road layer `0x602320`, `+0x28` building shadows
`0x605e70` (`buildings\shadow.tga`), `+0x34` `0x6020e0`, `+0x38`
`0x605c20` (optional).

Road layer draw `FUN_00602380` (vtable `0x6486ec`), per road rectangle of
`spRoadNetwork` (`[[park+0x14]+0xc0]`, read via `0x594810`):

```text
sx = tex_w / 1024, sy = tex_h / 1024
u0 = x0*sx, u1 = x1*sx, v = (tex_h - 1) - y*sy      (Y flipped)
if tex is 1024x1024:
    fill rect          RGB(170,130,90)  (bytes 5a 82 aa ff, BGRA)   outline
    fill rect inset 1  RGB(253,198,137) (0xFFFDC689)                 surface
else:
    fill rect          RGB(253,198,137)
```

Opaque colour fills (`FUN_004402d0`); the detail map then modulates the
terrain as everywhere else. So a 4-unit retail road is a sandy strip with a
1-texel brown border in the 1024 basemap; no road geometry is drawn.

## Placement preview look (2026-10-27)

- Shape (`0x5fa708..0x5fa891`): the preview walks the run in 4-unit steps;
  per step it samples the heightmap bilinearly (`u16 * 0.00390625`) and
  draws `helper_road.mesh` (20 verts: a bottomless unit box, -0.5..0.5,
  side UVs span the full texture) scaled `+0x14 = 4.0` on all axes, centred
  at `(x + 1.5, y + 1.5, h)`, via `0x5f9730`.
- Colour: the helper picks material 2 (valid) or 3 (invalid) from
  `gscMaterialBank` (`[0x676ecc]`, created in `0x600440`); `0x600550`
  animates them every frame: `x = trunc((sin(6t) + 1) * 0.5 * 127)`,
  material 2 = RGB(64,64,255-x), material 3 = RGB(255-x,64,64).
- Render state `Helper.ini`: texture `focus/road.tga` (32x32 white-grey
  gradient) modulated by the material, alpha blend One/One (additive).
- Port: `road_helper_mesh` + the same tint and additive blend
  (`src/world/build.cpp`, `src/viewer/main.cpp`).

## Port preview divergence (2026-10-28)

`8c23d92` briefly changed `road_helper_mesh` to one stretched box spanning
the run; the current source keeps retail's one `helper_road.mesh` per 4-unit
step (`0x5fa708..0x5fa891`) and adds `DrawItem::depth_write` for the preview.
The per-cell boxes saturate to white in the additive One/One pass when the
box's back/side faces also write; depth-writing the nearest face keeps the
retail shape. The port draws the tinted helper unlit and opaque, which
reproduces the captured retail fills exactly: valid `RGB(64,64,255-x)`
(capture `(64,64,135)`), invalid `RGB(255-x,64,64)` (capture `(134,64,64)`).
`notes/road-enclosure-preview-capture.md` has the full capture.
