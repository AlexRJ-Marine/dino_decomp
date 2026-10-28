# Entrance dock and starting road (R-16)

Date: 2026-10-28. Sources: retail saves, `DinoIsland.exe` via the Ghidra MCP
(`DinoIsland.exe-ebea17`) and capstone, the retail `.mesh`/`.bbi` files.

## Symptom

A retail park starts with the boat moored at a pink gangway and a pier with
posts and gate pillars, and a dirt road running inland from the gate
(`notes/entrance-retail-reference.png`). The port showed only the boat, too
far offshore, and no road (`notes/entrance-model-default.png`).

## 1. The dock is the rest of `entrance_lod1.mesh`

`.mesh` offset `0x0D` is a real **chunk count**, not a constant 1. The port's
reader (and `docs/formats/mesh.md`) stopped after the first chunk.

`entrance_lod1.mesh` (35,899 bytes) has 4 chunks:

| Chunk | Verts | Tris | Materials | Bounds x (m) |
| --- | ---: | ---: | --- | --- |
| `boat` | 296 | 231 | `boat.tga` | -32.9 .. -24.0 |
| `radar` | 16 | 6 | `boat.tga` | -29.6 .. -27.2 |
| `ponton2` (gangway) | 20 | 10 | `boat.tga`, `ponton.tga` | -25.0 .. -20.3 |
| `ponton` (pier, posts, gate) | 522 | 330 | `ponton.tga` | -22.3 .. 8.7 |

Chunk layout (validated on **all 536 retail meshes**, each ending exactly at
EOF; counts 1: 516, 2: 13, 3: 2, 4: 5):

```text
u32 name_len, name (NUL)
u32 vertex_count, f32[3] x vertex_count
u32 index_count, u32 x index_count
u8 uv_flag, u32 uv_count, f32[2] x uv_count      ; flag 0 -> count 0
u8 normal_flag, u32 normal_count, f32[3] x count ; flag 0 -> count 0
u8 colour_flag, u32 colour_count, u8[4] x count (if flag)
u32 material_count, { 21 bytes, string texture, string name } x count
u32 triangle_count (= index_count / 3), u16 material x triangle_count
16 zero bytes
```

This is the same trailer the `.skin` reader already decodes (`skin.cpp`),
minus the bone list. Multi-chunk files include `lunchdoor_lod1`,
`station_lod1`, `coin*_lod1`, `manege_lod2` and `helper_building`.

## 2. The road: `FUN_0051dad0` + `FUN_00586b80`

### Retail saves

Every retail free-game save on isl01 holds one Entrance and one road:

```text
run/save/slot002.sav  (2.5 s in, budget untouched 75000)
sav building: kind=9 cell=520,376 state=1
sav road: 524,392-524,452
```

`slot001.sav` (both `run/` and `run-test/`) is identical, and campaign
`Cpg01` stores the same rectangle.

### Door rect `FUN_00586b80` (jump table `0x586d0c` → `0x586e64`)

`spBuilding+0x18..+0x24` comes from four `.bbi` floats `D+0x5c..+0x68`
(file offsets 89, 93, 97, 101) rotated by the orientation. With
`w0 = width-1`, `w1 = height-1` and truncating `__ftol`:

| state | handler | x0 | y0 | x1 | y1 |
| --- | --- | --- | --- | --- | --- |
| 0 | `0x586d4e` | f0 | f1 | f2 | f3 |
| 1 | `0x586d13` | w1-f1 | f0 | w1-f3 | f2 |
| 2 | `0x586db8` | w0-f0 | w1-f1 | w0-f2 | w1-f3 |
| 3 | `0x586d80` | f1 | w0-f0 | f3 | w0-f2 |

(added to the cell origin). Every retail `.bbi` has
`(w, h/2+0.5, w, h/2-0.5)`, a door on the +x edge; Entrance is
`(24, 7, 24, 6)`, Security `(6, 1, 6, -1)`.

### Access road `FUN_0051dad0` (park start)

```text
for each building def with def+0x78 == 10 (Entrance):
    r = building.rect(+0x18..+0x24); state = orientation
    cx = snap4((r.x0 + r.x1) / 2); cy = snap4((r.y0 + r.y1) / 2)
         ; snap4(v) = ((v + (v >> 31 & 3)) >> 2) * 4
    state 0: (cx-8, cy)  .. (cx+52, cy)
    state 1: (cx, cy-8)  .. (cx, cy+52)
    state 2: (cx-52, cy) .. (cx+8, cy)
    state 3: (cx, cy-52) .. (cx, cy+8)
    FUN_0058d110(&start, &end, 1)       ; add the road, no cost
```

The decompiler shows `puStack_198 + 0xd` on a pointer-typed variable; the
byte offsets are `+0x34` (52) and `-8`.

isl01: cell (520,376), state 1 → door `(520+4, 376+24)-(520+5, 376+24)` →
centre `(524,400)` → road **(524,392)-(524,452)**, exactly the saved
rectangle. On the port's 4-unit grid this is column 131, rows 98..113
(16 cells).

The loop runs under a flag (`local_94`) that was not traced; campaign
`.wld` files already store the road, so the port lays it only for a
Free Game start (and, opt-in, for a `--building` Entrance with `--build
--entrance-road`).

## Port

- `src/formats/mesh.*`: `load_mesh_chunks` (exact chunk walk, throws on
  trailing bytes), `split_by_material`; a cleared UV/normal flag now
  consumes its zero count. `load_mesh` still returns chunk 0.
- `src/viewer/main.cpp` `place_building`: a static `.mesh` is drawn as
  one part per chunk and material, each with its own texture. Entrance:
  `verts=854 tris=577 parts=5`.
- `src/formats/bbi.*`: `BuildingDef::door` (file 89..101).
- `src/world/build.*`: `building_door`, `entrance_access_road`.
- Free Game start prints `entrance road: 524,392-524,452 path_cells=16`
  and paints the road into the terrain texture.

## Evidence

- `notes/entrance-dock-road.png`: retail reference / port before / port
  after at a matching camera.
- `tests/test_mesh.py::test_entrance_mesh_chunks`,
  `test_all_meshes_chunk_layout`.
- `tests/test_viewer_cpp.py::test_cpp_free_game_entrance_road`,
  `test_cpp_entrance_dock_and_road_render`,
  `test_cpp_building_placement_transform` (now `verts=854 tris=577
  parts=5`).

## Not done

- Retail runtime trace of `FUN_0051dad0` and its `local_94` gate.
- The port's road paint is lighter/narrower than the retail dirt road
  (road texture style is R-11 territory).
- The gangway's pink comes from its material; retail renders it pink too.
