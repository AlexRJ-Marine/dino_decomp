# Road network and road/terrain look (R-20)

Date: 2026-10-28. Binary `DinoIsland.exe`, Ghidra project
`DinoIsland.exe-ebea17` via the MCP plus capstone. Builds on
`notes/road-construction-recon.md` (tool state, costs, warnings, preview).

## 1. `spRoadNetwork` stores segments, not cells

A road is a segment `{x0, y0, x1, y1, ..., +0x14 horizontal}` built by
`0x5943f0` (`horizontal = y0 == y1`). The network keeps a list sorted by
`x0` (`+0x08..+0x0c`). `AddRoad` `0x58d110` runs, in order:

### Collinear merge `0x58d890`

For the new segment N against each existing E of the same orientation:

```text
vertical:   stop when N.x0 < E.x0 (sorted); for E.x0 == N.x0:
            E.y0 <  N.y0: if N.y0-4 <= E.y1 -> N.start = E.start,
                          N.end = max(N.end, E.end), remove E
            E.y0 >= N.y0: if N.y1 < E.y1 { if E.y0 <= N.y1+4 -> N.end = E.end,
                          remove E } else remove E (contained)
horizontal: stop when N.x1+4 < E.x0; for E.y0 == N.y0:
            N.x0 <= E.x0: N.end = max(N.end, E.end), remove E
            else if N.x0-4 <= E.x1: N.start = E.start,
                          N.end = max(N.end, E.end), remove E
```

So touching (gap 0..4 units) and overlapping runs become one segment.

### T-junction snap `0x58d9d0`

```text
N vertical, E horizontal (stop when N.x0+4 < E.x0):
  N.x0 outside [E.x0, E.x1] and N.y0 <= E.y0 <= N.y1:
      E.x0 == N.x0+4 -> E.x0 -= 4      ; E reaches N's column
      E.x1 == N.x0-4 -> E.x1 += 4
      (E removed and re-inserted sorted, scan restarts at index 1)
  N.x0 inside E's span:
      E.y0 == N.y1+4 -> N.y1 += 4      ; N reaches E's row
      E.y0 == N.y0-4 -> N.y0 -= 4
N horizontal, E vertical with N.x0-4 <= E.x0 (stop when N.x1+4 < E.x0):
  E.x0 outside [N.x0, N.x1] and E.y0 <= N.y0 <= E.y1:
      E.x0 == N.x0-4 -> N.x0 -= 4 ; E.x0 == N.x1+4 -> N.x1 += 4
  E.x0 inside N's span:
      E.y0 == N.y0+4 -> E.y0 -= 4 ; E.y1 == N.y0-4 -> E.y1 += 4
      (E re-inserted sorted, scan restarts)
```

A road that ends one cell short of another is extended onto it, so the two
rectangles overlap.

### Insert `0x58dbd0` / remove `0x58d390`

Insert before the first segment with a larger `x0`. `0x594610` records
crossings (end-open flags `+0x10/+0x11`); it does not change geometry.

## 2. Road paint `0x602380`

`0x594810` returns the segment as `(x0, y0)`, `(x1+3, y1+3)`. Per segment in
list order, with `sx = tex_w/1024`, `sy = tex_h/1024`:

```text
u0 = x0*sx            v0 = (h-1) - (y1+3)*sy
u1 = floor((x1+3)*sx) v1 = floor((h-1) - y0*sy)        ; 0x59a79b
1024x1024: FUN_004402d0(u0,v0 .. u1+1,v1+1, RGB(170,130,90))   border
           FUN_004402d0(u0+1,v0+1 .. u1,v1, RGB(253,198,137))  fill
else:      FUN_004402d0(u0,v0 .. u1+1,v1+1, RGB(253,198,137))
```

`FUN_004402d0` (last flag 1) adds 1 to the far corner. Each segment is a
4-texel strip: 1 border, 2 fill, 1 border, with its ends outlined too.

**Junctions (corrected 2026-10-28).** The third argument of `FUN_004402d0`
is the quad's depth: borders at `0x3f7f3b64` = 0.99700, fills at
`0x3f7ef9db` = 0.99600. `FUN_00440000` draws them with the 2D colour state
(`DAT_00674528+0x80` `+0x94`, opaque colour), and `ShapeColor2D.ini` has
`ZBuffer Y`, `ZBufferWrite Y`, `ZBufferFunc LessEqual`. The compose pass
`FUN_00603430` sets the viewport z range 0..1 and draws the basemap at
z 1.0 first. So wherever two roads overlap, a fill beats a border
regardless of draw order: junctions and crossings join seamlessly and only
the outer edges keep the brown border. The first port of this pass let a
later segment's border cut across earlier fills; `notes/road-junction-fix.png`
shows both.

The old port painted per cell and merged borders between neighbouring
cells, so joins and ends looked different.

## 3. Terrain: `detailmap.ini`

`0x5f3f10` binds `DATA\states\detailmap.ini` just before it builds
`gpkLandscape` (`0x5fb8b0`). `0x600800` registers the same file as material
10. The state is unlit, unfogged, `ZBufferFunc Less`, and:

```text
stage 0: SelectArg1(Texture)              basemap (roads painted in), set 0
stage 1: AddSigned(Texture, Current)      detail map, coordinate set 1
```

The landscape vertex writer (`0x61fa1f..0x61facf`, 0x1c-byte vertices) emits

```text
uv0 = ( x/1024 + 1/2048, -(y/1024) - 1/2048)   ; 0x6740d0 = 1/1024, 0x648944
uv1 = ( x/16,            -(y/16) )             ; 0x6740d4 = 1/16
```

from `0x5fb8b0`'s parameter block (`local_18 = 1/1024`, `local_14 = 1/16`,
copied to the globals in `0x61e330`). The detail map therefore repeats every
16 world units.

The old port lit and fogged the basemap and had no detail layer.

## Port

- `src/world/build.*`: `BuildGrid::roads`, `add_road` (merge, snap,
  sorted insert); `place_road` and `import_roads` (wld/sav, Entrance) go
  through it; `paint_roads` draws segments as above and keeps the per-cell
  look only for cells that no complete segment covers.
- `src/world/port_save.*`: `road x0 y0 x1 y1` lines.
- `src/viewer/main.cpp`: terrain uses the detail map (AddSigned, world/16,
  unlit, no fog). The detail texture is mipmapped. Retail asks for no mip
  maps; without them the world/16 tiling aliases into noise at park-camera
  distance, while the dgVoodoo2 captures look filtered.
  `--terrain-lit` restores the old look for A/B.

## Not done / differences

- Road endpoints: the port still accepts runs toward -x/-y (normalised). The
  retail signed-length cost quirk would refuse them; not runtime-checked.
- Road selling (`REQUEST_SELL_ROAD`, `0x58d4b0`) needs a sell tool the port
  does not have yet (R-8).
- The 1/2048 half-texel basemap offset is not reproduced.
- Retail colours come from a 16-bit (R5G6B5) back buffer; the port is 32-bit.

## Evidence

- `notes/terrain-detail-compare.png`: retail / old port / new port at the
  entrance and the `p5-side-by-side` park camera.
- `notes/road-network-before-after.png`: chained and junction roads, old
  per-cell paint vs retail segments (its right column still shows the
  junction border bug).
- `notes/road-junction-fix.png`: that bug vs the depth-tested fix.
- `--selftest`: merge, T-junction snap, end extension, rectangle paint.
- `tests/test_viewer_cpp.py::test_cpp_road_network_retail_segments`,
  `test_cpp_terrain_detail_map`.
