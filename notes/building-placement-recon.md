# Building placement recon (Ghidra)

Status: **solved** for position, orientation and Z. Two model-record fields
(`model[0]` slope-alignment flag, `model[1]` Z offset) are still unmapped to
`.bbi` bytes.

## Call chain

`drsBuilding` (34 bytes, vtable `0x648268`, factory `0x005e8730`)
→ `FUN_0053d1a0` (world object → graphics)
→ `spPark::AddBuilding` `FUN_00589100`
→ building ctor `FUN_00586b80`
→ graphic `gpkBuilding` `FUN_005fecf0`
→ placement `FUN_0061ffc0`.

## Position

`FUN_0053d1a0` at `0x0053d1f2`:

```text
iVar2 = *(int*)(*(int*)(param_1 + 0x34) + 0x8c);        // 500-byte def table
iVar1 = iVar2 + *(int*)(param_2 + 0x1c) * 500;          // +0x1c = kind
iVar2 = *(int*)(iVar2 + 0x78 + kind*500);               // +0x78 = category
uVar3 = FUN_00586950(param_2 + 0x14, *(u32*)(param_2+0x30), iVar1 + 0xc,
                     *(u32*)(param_2+0x24));
uVar4 = FUN_005f5120(iVar1 + 0x7c, uVar3);
```

`FUN_00589100` dispatches on `def+0x6c` (= record `+0x78`); category 10 is
the Entrance (`FUN_00595c70`, a bare base-ctor wrapper). The entrance→10 enum
also appears at `0x00566ea9`.

`FUN_00586b80` stores `cell_x`/`cell_y` at `this[2]`/`this[3]`
(`0x586c1f`/`0x586c24`) and the orientation (`drsBuilding+0x30`) at
`this[0x12]`. Footprint words are the `.bbi` header dwords:

```text
0x586c4e  state 0 or 2: this[4] = cell_x + word0 - 1
                        this[5] = cell_y + word1 - 1
0x586c3e  state 1 or 3: this[4] = cell_x + word1 - 1
                        this[5] = cell_y + word0 - 1
```

`gpkBuilding` `FUN_005fecf0` at `0x005fed5c`–`0x005fed7f` (FPU order, constant
`0x0062544c = 0.5f`):

```text
[ESI+0x58] = (this[2] + this[4]) * 0.5f     // X = mean of the x span
[ESI+0x5c] = (this[3] + this[5]) * 0.5f     // Y = mean of the y span
FUN_0061ffc0(landscape, node, model[0], X, Y, model[1], rect[0x48]);
```

No axis swap. For isl01 Entrance (kind 9, cell `520,376`, state 1):

```text
even-state footprint 24 x 12; state 1 swaps -> x 520..531, y 376..399
world = (X 525.5, Y 387.5)
```

Cross-checks: `isl01.lnd` width/height = `520/376`, `isl02` = `312/504`,
`isl03` = `704/448` — exactly the campaign Entrances. Cpg01 road `x=524` vs
X 525.5; Cpg02 road `y=508` vs Y 509.5; Cpg03 road ends at `(716,452)` vs
centre `(715.5,453.5)`. All road ends are multiples of 4.

## Z

`FUN_0061ffc0` (same code in `FUN_00620270`):

```text
iVar6 = *param_1;                                   // heightfield
map = (X * [hf+0x14], Y * [hf+0x14]);               // 512 map / 1024 world = 0.5
h = bilinear(u16[hf+0xc], w=[hf+4], h=[hf+8]) * 0.00390625   // 1/256
FUN_004663f0(pos = (X, Y, h));
if (model[0]==0) { yaw basis only }                 // 0x4664c0/0x4664e0
else if (model[0]==1) FUN_0061c510(X,Y,...)         // slope-aligned basis
FUN_00466ba0(translate(0,0,model[1]));              // per-model Z offset
```

`FUN_005c0b80` is the `mgHeightmap` bilinear sampler; `mgHeightmap` layout is
`{vtbl, w@4, h@8, u16* data@0xc}` (`FUN_005c05a0`). The extended object used
here adds cell size `+0x10` and inverse `+0x14` (0.5 → 512 map over the
1024 world).

Validation: save camera target `(512,512,32)` equals the bilinear height at
map `(256,256)` = **32.0** exactly. isl01 Entrance Z = **69.4** (the port's
old `(520,376)` corner sample gave 76.0; the port now prints 69.4 at the
footprint centre).

The Entrance has **no special Z rule**. Category 9 (`Station`), not 10, has
the only in-`AddBuilding` special case (`FUN_0058d6c0`). The Entrance does get
a load-time special case in `FUN_0051dad0` (road spawning): it finds the
building with `def+0x78 == 10`, takes the centre of its second rect
(`spBuilding+0x18..+0x24`), snaps it with `((v>>2)*4)` and calls
`FUN_0058d110(start,end,1)` on the 4-unit build grid.

## Scale, orientation and model choice

No scale and no axis conversion in the placement path. `FUN_005fecf0` →
`FUN_00603e30` loads the four `.bbi` paths raw. The transform is
`translate(X,Y,h) · Rz(90°×state) · translate(0,0,model[1])`; case 0 of
`FUN_0061ffc0` writes rows 0/1 as state 0 `(1,0)`, 1 `(0,1)`, 2 `(-1,0)`,
3 `(0,-1)`.

The model origin is the placement point; the model is not re-centred. The
`.bbi` primary model is the **first path**, a `.skin`:

- `entrance_lod1.mesh` bbox x `-32.9..-24.0`, y `-16.4..14.1`, z `-3.2..10.8`
  (pivot ~28 units off the boat body).
- `entrance_lod1.skin` full mesh is centred near the origin (cm:
  x `-513..373`, y `-2230..825`, z `-718..658`).

Using the `.mesh` LOD1 at index 3 therefore produces a visible offset; the
port loads the primary `.skin` instead (cm → m, divide by 100).

## State and footprint

`state` is **not** construction/destroyed; it is the 90° orientation (0..3)
and swaps the footprint axes for odd values. It is consumed only as
`rect[0x48]`/`this[0x12]` in `FUN_00586b80` and the jump table `0x586d0c`.
The factory default (`0x005e8730`) is 1. Construction/destruction is tracked
separately in the sim object (`+0x64..+0x68` flags in `FUN_00586b80`).

`word0/word1` are world units, not grid cells: they are added directly to
`cell_x/cell_y`. Entrance 24×12 = 6×3 cells of the 4-unit build grid. The
4-unit grid is proven by the `(v>>2)*4` snap and by every Cpg01–03 road
endpoint being a multiple of 4.

## Port changes

- `src/formats/wld.cpp` reads every `drsBuilding` from a `.wld`/`.sav`.
- `src/world/terrain.cpp` now samples the 512 map as `world*0.5` (sample 0 at
  -512, spacing 2), matching `FUN_005c0b80`.
- `src/viewer/main.cpp` `place_building` uses the footprint centre,
  `state*90°` yaw, the primary `.skin` and the heightfield Z.
- `src/formats/skin.cpp` gained `load_skin_auto` for building skins with zero
  skin bones / a non-stem block name, plus color and material parsing.

## Open

- `model[0]` (yaw-only vs slope-align) and `model[1]` (Z offset) are the first
  two dwords of the model record at `record+0x7c`; they are not mapped back to
  `.bbi` header/tail bytes. The entrance is likely `0,0`; its hull currently
  sinks ~7 units into the terrain, so `model[1]` may be non-zero.
- Heightfield `+0x10/+0x14` (cell size / inverse) inferred as 2.0/0.5 from the
  512 map over the 1024 world; the constructor was not located.
