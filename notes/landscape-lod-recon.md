# Landscape object LOD and culling (R-2, WORLD-2)

Date: 2026-10-28. Binary: `DinoIsland.exe`, Ghidra project
`DinoIsland.exe-ebea17` through the MCP (`decompile_function`,
`list_xrefs`, `read_bytes`) plus capstone for functions Ghidra left
undefined (vtable targets). Source-file strings name the classes:
`DISPLAY\Src\GraphPark\gpkLandscape.cpp`, `gpkLandscapeSector.cpp`,
`gpkLandscapeObj.cpp`.

## Answer

Retail draws `.imp`/`.blf` landscape objects (trees, rocks) by **sector
distance LOD only**:

- 15x15 sectors of 64 units; centres `32 + 64*i` (i = 0..14) on both axes,
  Z = bilinear heightmap / 256 at the centre.
- An object belongs to sector `(int(y) >> 6) * 15 + (int(x) >> 6)`.
- Each frame a sector stores `d = |camera - centre|`; every object in it
  uses that `d`.
- Level `i` is the first with `d <= K * dist[i]`, `dist = {200, 400}`
  (`.blf` LOD1, LOD2); if none matches the object is not drawn.
- `K` comes from `Dino.ini [GRAPHIC] LODQuality`: HIGH 2.0, MEDIUM 1.0,
  LOW 0.5, AUTO/other 1.0. Retail ships `LODQuality=AUTO`.
- No frustum test was found on the sector or object path.

## Evidence

### Object constructor `0x603ad0` (gpkLandscapeObj, vtable `0x648704`)

```text
FUN_00508990(3)                         ; LOD node base, 3 slots
mesh = FUN_00607620(blf+0x1c, blf+0x34) ; lod1 mesh + texture dir
FUN_00508a50(mesh, 0x43480000)          ; add level, distance 200.0
mesh = FUN_00607620(blf+0x28, blf+0x34) ; lod2 mesh
FUN_00508a50(mesh, 0x43c80000)          ; add level, distance 400.0
FUN_00466470(scale,scale,scale); FUN_00466cf0(angle); FUN_00466430(pos)
```

`0x508a50` appends the child mesh to `+0x28` and the float to the distance
list `+0x38..+0x40`. `0x508990` sets the current level `+0x44 = -1`.

### LOD select `0x508c10` (vtable slot `+0x10`) / `0x508ba0`

```text
00508c21  mov eax, [ebp+4]              ; d from the draw context
00508c45  fld [0x65ebf4]                ; K
00508c50  fmul [esi + i*4]              ; K * dist[i]
00508c53  fcomp [esp+0x10]              ; compare with d
00508c5c  jne next                      ; K*dist[i] < d -> try next level
00508c5e  sel = i; break
00508c74  mov [ebx+0x44], al            ; -1 if no level matched
```

The decompiled twin `0x508ba0` reads
`if (d <= K * dist[i]) { sel = i; break; }`. Draw `0x508d40`/`0x508da0` and
the sector draw `0x5fea00` skip objects whose `+0x44 < 0`.

### K = LODQuality

- `0x5054f0` reads `[GRAPHIC] LODQuality`: `HIGH`→0, `MEDIUM`→1, `LOW`→2,
  else (`AUTO`)→3 (strings `0x65e898/8a0/8a8/8bc`).
- `0x51d1d0`: case 0→2.0 (`0x40000000`), 1 and 3→1.0, 2→0.5, then
  `0x508e00` stores it in `0x65ebf4` (default `0x3f800000`).
- `game/.../Data/Config/Dino.ini`: `LODQuality=AUTO`.

### Sector distance `0x609970` → `0x6099c0`

```text
00609982  call 0x447650          ; camera node position (same as the skybox)
00609987  centre(+0x34..+0x3c) - camera; sqrt(dx*dx + dy*dy + dz*dz)
006099b0  fstp [esi+0x40]        ; sector distance
006099c0  fld [ecx+0x40]; fstp [ctx+4]   ; handed to every child's LOD select
```

Sector class vtable `0x648628` (slot `+0x10` = `0x5fe850`, calls
`0x6099c0` then each object list's `+0x10`); base vtable `0x6487ac`.

### Grid `0x5fb8b0` and object filing `0x5fbf90`

- Two nested loops `for (v = 0x20; v < 0x3e0; v += 0x40)`: 15 centres per
  axis, outer = y, inner = x; `centre.z = bilinear(height) * 0.00390625`;
  each sector built by `0x5fd1e0(&centre, 4)`.
- `0x5fc132..0x5fc155`: `ftol(y) >> 6`, `* 15`, `+ (ftol(x) >> 6)`, index
  into the sector array `+0x28`, then `0x5fd640` (add object). Column 15
  (x >= 960) is not clamped and aliases the next row; the port keeps the
  formula and clamps only the final index.

### No frustum test

The landscape node (vtable `0x648600`) walks sectors in `0x5fc3c0`
(`+0xc`), `0x5fc410` (`+0x10`) and `0x5fc460` (`+0x18`); each skips a sector
only on byte `+0x48` (set at construction, cleared by every add-object
call = "sector is empty") and the node flags `+0x10/+0x11`. The sector
passes `0x5fe6e0`, `0x5fe850`, `0x5fea00` loop over object lists without any
view test. Retail relies on the 400-unit cut and D3D clipping.

## Port

- `src/world/vegetation.*`: `landscape_sector`, `landscape_lod`,
  `landscape_lod_quality`; vegetation is baked per (entry, sector, LOD):
  isl01 = 422 batches, LOD1 52,291 tris, LOD2 14,576 tris.
- `src/viewer/main.cpp`: per frame, sector distance from the eye selects the
  visible batch; `--lod-quality`, `--no-lod`. LODQuality defaults to
  `Data/Config/Dino.ini`.
- LOD2 textures are mostly cut-out (`palm1_lod2.tga` mean alpha 63/255), so
  vegetation now uses the `TextureAlpha*` AlphaTest (Ref 128, Greater).
  Which state profile retail binds to landscape meshes is still FMT-27.
- Port-only frustum culling: each batch has a bounding sphere tested against
  the six clip planes; `--no-cull` disables it. Image unchanged (max pixel
  delta <= 8, the same as run-to-run noise); 125 of 197 drawn batches are
  skipped at the retail park camera.

## Captures

- `notes/vegetation-lod-before-after.png`: pre-R-2 build (`f92e0ba`) vs R-2
  (`bb9020b`) at the park camera and orbits at 300/600 units.
- `notes/vegetation-lod-ab.png`: retail park camera (distance 65.6) and an
  orbit at 300 units, LOD rule vs `--no-lod`.
- First-frame stats (`vegetation frame:`), isl01:
  - distance 65.6: LOD1 658, LOD2 820, hidden 58.
  - orbit 300: LOD1 253, LOD2 819, hidden 464.

## Not done

- Runtime confirmation of the LOD switch in retail (x32dbg watch on `+0x44`
  or `0x65ebf4`).
- The ctx byte `[ctx+0]` gate in `0x5fe97e` (objects' LOD is only refreshed
  when it is set); its writer was not traced.
- Terrain itself is still one mesh; the retail terrain patch grid
  (`0x61e1f0`, 0x23c-byte cells) was not reversed.
