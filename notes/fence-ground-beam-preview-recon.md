# Paddock ground, fence light beams and enclosure preview — raw evidence (2026-10-28)

Status: **Q1 and Q3 answered from disassembly; Q2 answered except the exact
per-material state branch.** Binary `/DinoIsland.exe-ebea17` (Ghidra MCP), all
addresses VA. No `src/` change.

Complements `notes/fence-construction-recon.md`, `notes/wall-raster-recon.md`,
`notes/fence-wall-params-recon.md`, `notes/road-network-recon.md`.

## Questions

1. How does a built paddock get its sandy/dirt interior? Which layer/function,
   which rectangle/inset, which texture, and what marks it dirty?
2. How are the `laser2.tga` light beams of
   `Data/Graphics/paddock/fences/fence_paddock_lod1.mesh` rendered?
3. What exactly does the enclosure preview helper draw in modes 0/1/2/6/8
   (dots, spacing, size, start disc), and does the two-click tool queue
   anything else when the wall order is created?

---

# Q1 — the paddock interior ground

## 1.1 Correction: it is a 3D quad, not a `gpkLandscapeTextureManager` layer

The `gpkLandscapeTextureManager` (`gpkLandscapeTextureManager.cpp`, ctor
`0x603010`) really has a per-object **textured rectangle layer**:

| manager field | ctor call | vtable | draw | role |
| --- | --- | --- | --- | --- |
| `+0x24` | `FUN_00602320` (0x602320) | `0x6486ec` | `0x602380` | road paint (proven, road recon) |
| `+0x28` | `FUN_00605e70` (0x605e70) | `0x648750` | `0x605fc0` | building shadows |
| `+0x34` | `FUN_006020e0` (0x6020e0) | `0x6486e8` | `0x602120` | **per-object textured rect** |
| `+0x38` | `FUN_00605c20` (0x605c20) | `0x648748` | `0x605c80` | text/billboard overlay (gated by manager `+0x19`) |

`0x602120` iterates the vector at `*(layer+0x20)+4/+8` and per element:

```asm
0060212f FLD  [EBX+4]                     ; texture width
00602132 FMUL [0x6485f8]                  ; * 1/1024
00602138 MOV  EAX,[EBX+0x20]
0060213b ADD  EAX,4                       ; container +4 = begin
00602145 MOV  EBP,[EAX+4]                 ; end
00602148 MOV  ECX,[EAX]                   ; begin
00602150 SUB  EBP,ECX
00602152 SAR  EBP,2                       ; count
...
; per element: min/max of the two corner pairs via 0x5ff990 / 0x5ff9a0
; u = x/1024, v = (1023 - y)/1024          (0x3ff = 1023)
00602273 JZ   0x60229d                    ; layer+0x24 byte == 0 -> per-object path
; layer+0x24 != 0: fill with the layer colour at layer+0x25
0060229d CMP  dword [ESI+0x50],-1
006022a1 JNZ  0x6022cb
; obj+0x50 == -1 -> solid colour at obj+0x54  (0x4402d0)
006022cb ; else -> textured quad with texture at obj+0x4c (0x440490)
```

`0x5ff990`/`0x5ff9a0` are the *only* users of the object field:

```asm
005ff9a0 MOV EAX,[ECX+0x98]   ; +0x98 = pointer to the definition
005ff9a6 ADD EAX,8            ; corner pair 0
005ff990 MOV EAX,[ECX+0x98]
005ff996 ADD EAX,0x10         ; corner pair 1  (+8/+0xc and +0x10/+0x14)
```

**Which vector is it?** `layer+0x20` is the address passed down as the
manager ctor's 4th stack arg; the chain is:

```asm
005f3feb LEA ECX,[ESI+0xa8]   ; gpkParkScene+0xa8
005f3ff1 LEA EDX,[ESI+0x6c]   ; gpkParkScene+0x6c
005f3ffe CALL 0x5fb8b0        ; gpkLandscape ctor(..., param_5=scene+0x6c, ...)
005fb92e CALL 0x603010        ; texture manager ctor(arg4 = landscape param_5)
006031e3 CALL 0x6020e0        ; rect layer ctor(..., param_4 = manager arg4)
```

so `layer+0x20 = scene+0x6c`, and the draw reads `*(scene+0x70)` /
`*(scene+0x74)` = the scene's **building vector**, filled by
`gpkParkScene::AddBuilding` `0x5f5120` (`+0x70..+0x78`, object = `gpkBuilding`).
`gpkBuilding` is the only class that writes `+0x98`:

```asm
005fed42 MOV EDI,[ESP+0x20]      ; arg2 = the .bbi definition
005fed50 MOV [ESI+0x98],EDI      ; +0x98 = definition
005fecfe LEA ECX,[ESI+0x4c]      ; 2D texture slot at +0x4c
005fed01 CALL 0x44a130
```

Conclusion: `+0x34` paints **building footprints**, not paddocks. A paddock
(the `stFence`) never enters this vector.

## 1.2 The paddock interior is the `gatAttraction` ground quad

The wall order created by the two-click tool (`0x53ac60`, class `0x17`) is
executed by the order dispatcher at `0x53c121`:

```asm
0053c115 MOV EAX,[ESI+0x4c]      ; style index
0053c11c CALL 0x586940           ; (unrelated)
0053c121 PUSH ESI
0053c122 MOV ECX,EDI
0053c124 CALL 0x53d0b0           ; wall/attraction order executor
```

```c
FUN_0053d0b0(pwdWorldManager, order /*class 0x17*/)
{
  def     = wall_defs + order[0x14] * 0xb4;            // Fence.bwi element
  stFence = FUN_00586920(order+0x18, order+0x20, def+0x0c, order+0x44);
  uVar2   = FUN_00520060(stFence, pwdWorld+0x158, pwdWorld+0x174, order+0x48);
  scene   = ...->+0x94;                                 // gpkParkScene
  uVar3   = FUN_005f4e70(def + 0x24, stFence, 0);       // AddAttraction
  uVar4   = FUN_004fecc0(stFence, ...);
  FUN_005e8820(stFence, uVar2, uVar3, uVar4);           // link resident
}
```

`gpkParkScene::AddAttraction` (`0x5f4e70`) creates the fence graphic and
pushes it into **two** scene vectors, then marks the basemap dirty:

```c
local_8 = FUN_00623640(def, stFence, 0, scene+0x3c, scene+0x44, scene+0x7c); // gatFence
// scene+0x50 vector  (rendered every frame by 0x5f5450)
// scene+0xac vector  (same object, second list)
*(u8*)(scene + 0xec) = 1;                              // basemap dirty
```

`gatFence` (`gatFence.cpp` ctor `0x623640`) is a thin `gatAttraction`:

```c
FUN_005ec7e0(this, def, stFence, ...);
this[0x25] = stFence;       // +0x94
this[0x24] = param_4;       // +0x90
*this = &PTR_FUN_00648aa0;  // gatFence vtable
```

`gatAttraction` (`gatAttraction.cpp` ctor `0x5ec7e0`) builds the **ground
quad** and loads its two textures:

```asm
005ec8fb LEA ECX,[ESI+0x34]            ; def+0x34: mcString (12 bytes)
005ec906 CALL 0x607200                 ; texture-cache lookup
005ec90f CALL 0x44a1b0                 ; bind into slot +0x64
005ec923 ADD ESI,0x40
005ec92c CALL 0x607200                 ; def+0x40: second texture
005ec935 CALL 0x44a1b0                 ; bind into slot +0x6c
005ec943 PUSH 5
005ec945 CALL 0x50b2f0 / 0x5e3fe0     ; state 5 = data\states\dino_attractionground.ini
```

Then the quad geometry:

```c
def        = param_2;                       // = element + 0x24
perimeter  = *(int*)(param_1[0x18] + 0x1c); // param_3 = stFence -> stPerimeter
// rect = stPerimeter +0x1c/+0x20/+0x24/+0x28 (the same 4 ints the rasteriser uses)
z = FUN_005d65c0(mid_y, mid_x) + 0.01f;     // terrain height at the rect centre
// 4 vertices, 2 UV sets; UV0 in {0,0.5}x{0,0.5} chosen by rand()&3,
// UV1 = UV0 + 0.5                       (dual-texture state)
FUN_0046a530();                              // close the static VB
```

Chain of the field offsets (`D` = file base = element+0x0c):

| access | runtime field | file | value in `Fence.bwi` |
| --- | --- | --- | --- |
| `def+0x34` | element+0x58 = D+0x4c | textures[2] | `data\graphics\textures\buildings\boue_pad.tga` |
| `def+0x40` | element+0x64 = D+0x58 | textures[3] | `data\graphics\textures\buildings\ground_occup_pad.tga` |
| `def+0x4c` | element+0x70 = D+0x64 | meshes[0] | `DATA\graphics\paddock\fences\fence_paddock_lod1.mesh` |
| `def+0x58` | element+0x7c = D+0x70 | meshes[1] | `DATA\graphics\paddock\fences\fence_lunchdoor.mesh` |
| `def+0x64` | element+0x88 = D+0x7c | texture dir | `DATA\graphics\TEXTURES\buildings` |

`def = element+0x24 = D+0x18` is proven by `0x53d0b0`
(`FUN_005f4e70(iVar7 + 0x24, ...)`) and by `gatWall` reading the mesh from
`def+0x4c` (see Q2). The runtime string order is `[tex0, tex1, tex2, tex3,
mesh0, mesh1, dir]` as 0xc-byte mcStrings starting at `D+0x34`, each with the
interned `char*` at `+8` (`0x53ddd0..0x53df40` reads the length into
`[D+0x34]`, the bytes into `*[D+0x34]`, then stores the interned string at
`[D+0x3c]`; the next record starts at `D+0x40`).

So, for a paddock, the ground pair is exactly the two `*_pad` textures:

* `boue_pad.tga` 128x128 RGB, unique colours 3688, range (110,75,4)..(248,185,62)
  — the brown mud/dirt matching the retail interior;
* `ground_occup_pad.tga` 256x256 RGBA, 256 unique values, almost all
  `(0,0,0,0)` with sparse white pixels — the second stage of the dual-texture
  state (occupancy/detail overlay).

`Show.bwi` is the same schema with `boue_att.tga` + `ground_occup_att.tga` and
`fence_show_lod1.mesh` (the arena shares the code path).

## 1.3 Rectangle / inset

**No inset.** The quad is built from the perimeter rect itself
(`stPerimeter+0x1c..+0x28`), i.e. the same rectangle the wall rasteriser uses.
The wall cells (1-unit Bresenham lines, type 7) sit on the perimeter line, so
the ground polygon reaches under the wall line; the visible dirt edge is the
inner face of the wall geometry in the capture. Height = terrain height at the
rect centre + 0.01 (a single flat quad, not per-corner terrain).

Evidence: `notes/harness/crop-paddock-ground.png` (retail, built campaign
paddock): the dirt ends exactly at the wall line and its top/bottom edges are
the fence run.

## 1.4 Dirty / repaint

`scene+0xec` is the only "repaint" flag:

```asm
005f555f MOV  AL,[ESI+0xec]         ; gpkParkScene::Render (0x5f5450)
005f5565 TEST AL,AL
005f5567 JZ   0x5f5586
005f5569 MOV  ECX,[ESI+0xf4]        ; gpkLandscape
005f556f CALL 0x5fc3a0              ; ECX=[ECX+0x48]; JMP 0x603430  (manager compose)
005f5574 MOV  ECX,[ESI+0xe8]
005f557a CALL 0x5fb210              ; second manager refresh
005f557f MOV  byte [ESI+0xec],0
```

`0x5fc3a0` is `MOV ECX,[ECX+0x48] ; JMP 0x00603430`: the manager is
`gpkLandscape+0x48` (`param_1[0x12]` from the landscape ctor) and `0x603430`
is the compose pass (basemap: roads, building rects, shadows, text). The flag
is set by `AddBuilding` (`0x5f5120`), `AddAttraction` (`0x5f4e70`) and the
other `Add*` scene calls. There is no per-paddock dirty marker: the paddock
ground is *static scene geometry* built once in the `gatAttraction` ctor, and
the `gatFence` object is in the `scene+0x50` attraction vector that
`gpkParkScene::Render` (`0x5f5450`) draws every frame, so it appears on the
first frame after the fence exists. The `scene+0xec` recompose only refreshes
the basemap layers (roads/buildings/shadows).

---

# Q2 — fence light beams (`laser2.tga`)

## 2.1 Material table of `fence_paddock_lod1.mesh`

67 vertices, 105 indices, 35 triangles, one chunk (`fence_paddock_lod1`),
bounds `(-0.257,-2.618,-0.076) .. (0.703,1.967,8.771)` — a **single 4-unit
fence segment** (one post + rails), not a whole paddock.

| # | texture | material name | 21-byte header (hex) | tris |
| ---: | --- | --- | --- | ---: |
| 0 | `fence_paddock.tga` | `2 - Default` | `969696ff ffffffff cdcccc3d e6e6e6ff 000000ff 00` | 27 |
| 1 | `laser2.tga` | `9 - Default` | `969696ff ffffffff cdcccc3d 0000ffff 000000ff 01` | 8 |

Header decode (MSVC/D3D colour convention, byte order B,G,R,A):

```text
+0x00 u32 diffuse   = 0xff969696  -> (150,150,150,255)
+0x04 u32 ambient   = 0xffffffff  -> white
+0x08 f32 power     = 0.1f        (0x3dcccccd)
+0x0c u32 colour    = 0xe6e6e6ff  (fence) / 0x0000ffff (0xffff0000 = RED, laser)
+0x10 u32 specular  = 0x000000ff  -> black
+0x14 u8  flag      = 0 (fence) / 1 (laser)
```

The laser's 4th dword is a pure **red** (`00 00 FF FF` = D3DCOLOR
`0xFFFF0000`); the fence body's is near-white. `fence_lunchdoor.mesh` has the
same two materials (31 + 8 tris). `fence_show_lod1.mesh` (show arena) has **no**
laser material.

## 2.2 The beam geometry

The 8 triangles of material 1 are **two cross-shaped billboards** per segment:

| beam | triangles | geometry |
| --- | --- | --- |
| top | 27,28 | quad in the XY plane at z = 6.3289; x -0.2451..0.2466, y -1.9535..1.9465 |
| top | 29,30 | quad in the YZ plane at x = 0.0008; z 6.0830..6.5747, y -1.9535..1.9465 |
| low | 31,32 | quad in the XY plane at z = 2.2675; x -0.1251..0.3666 |
| low | 33,34 | quad in the YZ plane at x = 0.1208; z 2.0216..2.5134 |

So each beam is a cross of two orthogonal quads (0.49 wide, the full 3.9-unit
segment long): visible from every camera angle. Heights are 6.33/8 = **79%**
and 2.27/8 = **28%** of the 8-unit post. UVs run u 0..1 across the width and
v -0.6715..-0.0242 along the segment (v_tex = 1+v = 0.33..0.98). No scroll is
stored in the mesh.

## 2.3 Who draws it

`gatWall` (`gatWall.cpp` ctor `0x5ee970`) creates **two** mesh instances per
wall segment from the wall definition:

```c
// first:  def+0x4c = meshes[0], def+0x64 = texture dir, state id 4, texture id 7
FUN_005f3250(def+0x4c, def+0x64, 4, 7, resourceMgr);
// second: def+0x58 = meshes[1] (used when the pair is non-null)
FUN_005f3250(def+0x58, def+0x64, 4, 7, resourceMgr);
```

`gatAttraction` creates one `gatWall` per `stWall` segment
(`FUN_005ecdf0`, from `stFence+0x1c` -> `stPerimeter` -> vector at
`+0x10..+0x18`), and `gatFence`'s render (`0x5ed2d0`) calls each wall's
`vtable+0x18` = `0x5ef1d0`.

`0x5ef1d0` per segment (loop over `stWall+0x18` cells):

```asm
005ef2c1 ... build the segment transform from the two endpoints and the terrain
005ef314 CALL 0x466430                 ; set translation
005ef331 MOV EDX,[ECX+0x18]            ; stWall cell vector
005ef337 CMP dword [cell+0x18],1       ; cell kind
005ef33d MOV EAX,[EDI+0xb0]            ; second instance block
005ef346 CMP dword [EAX],0             ; present?
005ef34e TEST ECX,[EAX+8]
005ef352 MOV CL,[EDI+0x14]
005ef355 MOV EAX,[EDI+0xbc]            ; instance list
005ef363 MOV ECX,[EAX+4]               ; second instance (laser mesh)
...
005ef37f CALL 0x5f3530(instance, transform, &flag)
```

So the beams are **part of the fence mesh instance**: the same mesh's material
1, drawn when a cell's kind byte is 1 and the second mesh instance exists.
`FUN_005f3530` chooses between two sub-objects of the instance
(`+4 vs +0x14`) by the byte `gatWall+0x14`; the base ctor `FUN_00508740`
writes `param_1[5] = 0` (`+0x14..+0x17`), so the byte is **0** and the
sub-object `+4` is used.

## 2.4 Render state

`FUN_005f3250` walks the instance's 8-byte material entries and assigns a
**material** (gscMaterialBank) and a **state** (state bank) per entry:

```asm
005f3334 CALL 0x443be0                 ; material object from the entry
005f3339 CMP  byte [EAX+2],0xff
005f333d JNZ  0x5f339b                 ; default branch
; --- byte[+2] == 0xff: gscMaterialBank 4 + state 7 (Dino_additif) ---
005f3346 PUSH 4 ; CALL 0x6083a0 (gscMaterialBank); CALL 0x608110 (Get id 4)
005f3358 CALL 0x4439d0                 ; assign material
005f3360 PUSH 7 ; CALL 0x50b2f0 (state bank); CALL 0x5e3fe0 (Get id 7)
005f3372 CALL 0x46d700                 ; assign state
; --- default: gscMaterialBank 1 + state 8 (Dino_SelectedWall) ---
005f339e PUSH 1 ; ... 005f33a0 CALL 0x6083a0 / 0x608110
```

`0x608250`/`0x6082f0` (gscMaterialBank.cpp) own `DAT_00676ecc`; `DAT_006755fc`
is the state bank registered by `FUN_00600800`.

Candidate states / .ini:

* state 7 = `data\states\dino_additif.ini`: `AlphaBlend Y One/One Add`,
  `ZBufferWrite N`, `ZBufferFunc Less`, `AlphaTest N`, **`Fog N`**,
  `Light Y`, `EmissiveMatSource Material`, texture stage 0 =
  `TextureParam.txt` (Modulate(Texture,Diffuse), Wrap, linear, no mip).
* state 8 = `data\states\dino_selectedwall.ini` (opaque, ZWrite Y,
  SrcAlpha/InvSrcAlpha, Tone stage 1 Modulate2x).

Which branch the laser takes depends on the runtime material object's byte
`+2`, not on the file header; **open** (see Open). What is proven: the beams
are the same mesh's material 1, they carry a pure-red colour dword and a
white/alpha `laser2.tga` (64x64 RGBA, 188 unique values, grey core
(143,143,143,149)), and the only additive path in the renderer is state 7
(`Dino_additif`), which matches the pink/white additive look in
`notes/harness/crop-fence-beams.png`.

No UV scroll: both `TextureParam.txt` (state 7) and
`Dino_WallTextureParam.txt` (state 4/8) set `mgStateTextureTransform 0
Disable`, and the mesh UVs are static. The only per-frame animation in the
gate path is `FUN_005ed130` (called after the walls by `0x5ed2d0`), which
computes a camera-distance fog factor
`clamp(dist-50,0,120)/120*0.5+0.05` — a distance fade, not a time pulse.

---

# Q3 — enclosure preview helper (modes 0/1/2/6/8)

## 3.1 Object and fields

Helper ctor `FUN_005f8010` (gpkHelperInterface.cpp), 0xf8 bytes, owned by
`gpkParkScene+0x40` (created in `FUN_005f3f10` from
`FUN_005f8010(scene, terrain, landscape, scene+0x44)`). Tools reach it at
`state+0x7c`.

| offset | type | meaning | ctor / setters |
| ---: | --- | --- | --- |
| `+0x08/+0x0c/+0x10` | f32 x3 | last snapped point (world) | `0x5f8700`, `0x5f8790`, mode-1 draw |
| `+0x14` | f32 | **step / grid size** (default 8.0) | mode setters |
| `+0x18` | u8 | "centred" flag passed to the snap (0 for fence modes) | mode setters |
| `+0x1c/+0x20/+0x24` | f32 x3 | first-click snapped corner | mode-1 draw |
| `+0x34` | u8 | **valid flag** (`1` = valid) | `0x5f8a70(helper, code==0)` |
| `+0x38..+0x60` | f32 x2 x4 | the 4 perimeter corner points | `0x52bbf0` / `0x58a4e0` path |
| `+0x68/+0x6c` | f32 | dot/footprint size X/Y (default 8.0) | mode setters |
| `+0x70` | int | perimeter segment id 0..3 (0 = -x?, 1 = +y, 2 = +x, 3 = -y) | `0x5f8790`, `0x5f8e20` |
| `+0x74/+0x78` | f32 | last snapped point for modes 8/9 | `0x5fa500`, `0x5f8670` |
| `+0x7c/+0x80` | f32 | copies of +0x74/+0x78 | `0x5f8670` |
| `+0x84/+0x94` | 16 B | render slots (material/state) | ctor |
| `+0xa4` | ptr | `data\graphics\focus\helper_building.mesh` | ctor |
| `+0xa8` | ptr | `data\graphics\focus\helper_sphere.mesh` (unit sphere ±0.5) | ctor |
| `+0xac` | ptr | `data\graphics\focus\helper_road.mesh` | ctor |
| `+0xb0` | ptr | `data\graphics\focus\helper_sell_arrow.mesh` | ctor |
| `+0xb4` | slot | gscMaterialBank id 2 (valid) | ctor `0x5f83cc..0x5f83e3` |
| `+0xbc` | slot | gscMaterialBank id 3 (invalid) | ctor `0x5f83e8..0x5f83fd` |
| `+0xcc` | int | **mode** | mode setters |
| `+0xd0` | ptr | park scene (ground point at `+0x18/+0x1c`) | ctor |
| `+0xd4` | ptr | terrain (heightmap, `+0x14` = cell scale) | ctor |

Colour: `FUN_005f9730` binds `this+0xb4` when `+0x34 != 0` (valid) and
`this+0xbc` when 0 (invalid). Those are **gscMaterialBank ids 2 and 3**, i.e.
the same material bank the wall renderer uses. The retail capture
`notes/harness/enclosure-preview-valid.png` shows a flat blue disc
(RGB ~(38,38,136) at 5 samples); `enclosure-preview-invalid.png` is the
maroon counterpart (see the older note). The material colours themselves were
not dumped (Open).

## 3.2 Mode setters

| function | mode | fields written |
| --- | --- | --- |
| `0x5f8590(step)` | **1** | `+0xcc=1, +0x14=step, +0x68=+0x6c=step, +0x18=0, +0x34=0` |
| `0x5f85c0(step)` | **2** | `+0xcc=2`, same fields |
| `0x5f85f0(w,h,?,mode,step)` | any | `+0xcc=mode, +0x68=w, +0x6c=h, +0x14=step` |
| `0x5f8620(size*,step,byte)` | **6** | `+0xcc=6, +0x68=size.x, +0x6c=size.y, +0x14=step, +0x18=byte` |
| `0x5f8650()` | **8** | `+0xcc=8, +0x14=4.0f, +0x18=0` |
| `0x5f8670()` | **9** | `+0xcc=9, +0x7c=+0x74, +0x80=+0x78, +0x14=4.0f` |
| `0x5f86a0/0x5f86d0` | 0xb/0xc | size + step |
| `0x5f8830()` | 0 | clears (copies +0x7c/+0x80 back) |

## 3.3 Snap and height (`0x5f8c70`, `0x5faaa0`)

`0x5f8c70(helper, out, point, step, centredByte)`:

1. `0x5faaa0(&out, point, step, centredByte)`:
   `x0 = x - fmod(x,step)` (inline `FLD x; FLD step; CALL fmod` at
   `0x5faadd..0x5faaea`, then `FSUBR`), same for y; candidates
   `(x0,y0) (x0,y0+step) (x0+step,y0) (x0+step,y0+step)`, nearest wins
   (ties resolve toward `+step`; the port already implements this as
   "half-down"). If `centredByte`: subtract `step/2` before and add it after.
2. Terrain height at the snapped point: bilinear on the u16 heightmap
   (`terrain+0xc`, `+0x4/+0x8` size, `+0x14` = scale), `* 1/256`
   (`0x58a4xx` formula).
3. Result = `(snapped x, snapped y, height)`.

Fence modes use `centredByte = 0` (the helper ctor and mode setters write
`+0x18 = 0`).

## 3.4 Draw mode switch `FUN_005f8850` (gpkHelperInterface.cpp)

```c
switch (mode) {
case 1:  FUN_005f98e0(); return;              // start disc
case 2:  FUN_005f9b00(); return;              // perimeter grid
case 3:  ... single mesh, yaw 0/90/180/270 from +0x70 ...
case 5: case 6: case 7: FUN_005f9ed0(); return;  // footprint box
case 8: case 0xd: FUN_005fa500(); return;     // building cube
case 9:  FUN_005fa5b0(); return;
case 10: ... one sphere at the int point from +0xd0 ...
case 0xb: FUN_005fa950();
case 0xc: FUN_005fa8a0(); return;
}
```

### Mode 1 — start disc (`0x5f98e0`)

Takes the scene's stored ground point (`scene+0x18/+0x1c`, floats), snaps it
with `step`, stores it to `+0x1c/+0x20/+0x24` and `+0x08/+0x0c/+0x10`, then:

```c
pos  = (snapped.x + step*0.5, snapped.y + step*0.5, terrainHeight(snapped));
size = (+0x68, +0x6c, min(+0x6c,+0x68));
FUN_005f9730(&pos, &size);       // ONE helper_sphere instance
```

Instance count = **1**. The mesh is a unit-diameter sphere, so the disc
diameter = `step`. The tool calls mode 1 with **8.0** from the reset path
(`0x52bb70`, and `0x52b219` after a successful apply uses 4.0): radius **4
world units** (one build cell) centred 4 units +x/+y from the snapped anchor.
That is the blue disc in `notes/harness/enclosure-preview-valid.png`.

### Mode 2 — perimeter grid (`0x5f9b00` → `0x5f9bb0`)

The tool's first click calls `0x5f85c0(step)` with

```asm
0052b177 MOV EDX,[ECX+EAX*4+0x94]   ; walldef + ix*0xb4 + 0x94 = D+0x88
0052b185 FILD qword [ESP+0x1c]      ; (float) that int
0052b18c CALL 0x005f85c0            ; mode 2, step = 4.0
```

`D+0x88` is the `.bwi` u32 at file 0x88 = **4** for all 24 walls, so
**step = 4.0** and `+0x68 = +0x6c = 4.0`.

`0x5f9bb0(firstPoint, currentPoint, size)` walks the grid:

```c
dx = (p2.x <= p3.x) ? +step : -step;
dy = (p2.y <= p3.y) ? +step : -step;
for (y = p2.y; strictly between p2.y and p3.y; y += dy)
  for (x = p2.x; strictly between p2.x and p3.x; x += dx) {
      z = bilinearHeight(x, y);
      pos  = (x + step/2, y + step/2, z);
      size = (x==p2.x || y==p2.y || nextX>=p3.x || nextY>=p3.y)
             ? (size/2) : size;               // half on the border rows/cols
      FUN_005f9730(&pos, &size);              // one helper_sphere
  }
```

Instance count = `(ceil(span_x/step)) * (ceil(span_y/step))`; the last row and
last column are exclusive of the cursor corner. Interior spheres have diameter
4 (touching), border spheres diameter 2. Note the helper's **step (4) is the
build cell**, so a built enclosure's preview covers exactly its cells.

### Mode 6 — footprint box (`0x5f9ed0`)

Used by the feature-preview hook `0x526e70` (fence vtable `+0x34`):

```asm
00526ea5 MOV EDX,[EDI+0x1b8]      ; LunchBox height (int -> float)
00526ec8 PUSH <size vector>       ; feature size argument of the hook
00526ec9 CALL 0x5f8620            ; mode 6, w,h, step=height, byte=0
```

Draw: one mesh at `(x + w/2, y + h/2, terrainHeight(x,y))` with the footprint
swapped for segment ids 1/3, scale `(w, h, min(w,h))`.

### Mode 8 — building cube (`0x5fa500`)

Takes `scene+0x18/+0x1c`, snaps with `step = 4.0`, stores `+0x74/+0x78`, and
draws **one** `helper_sphere` at

```c
pos  = (x + (step-1)*0.5, y + (step-1)*0.5, z)   // +(1.5,1.5) for step 4
size = (step, step, step)                         // 4x4x4
```

i.e. the 4x4 building preview cube's centre is offset by 1.5 units (half a
cell minus half a unit). Modes 0xb/0xc are the same pattern with different
meshes (`0x5fa950`, `0x5fa8a0`), mode 9 (`0x5fa5b0`) uses the road helper.

## 3.5 Concrete example — rect (100,100)-(116,116), mode 2

First click at (100,100) → mode 2, step 4, size 4, corners 100/116.
Instances (16 = 4x4), diameter in parentheses:

```text
row y=100 (half):  (102,102)[2] (106,102)[2] (110,102)[2] (114,102)[2]
row y=104:         (102,106)[2] (106,106)[4] (110,106)[4] (114,106)[2]
row y=108:         (102,110)[2] (106,110)[4] (110,110)[4] (114,110)[2]
row y=112 (half):  (102,114)[2] (106,114)[2] (110,114)[2] (114,114)[2]
z at each dot     = bilinear terrain height at (x,y) (cell corner, not centre)
```

Mode 1 on the same anchor: one instance at (104,104) (8-unit step,
`100 + 8/2`) with size (8,8,8), radius 4 — the 2x2-cell start disc.

---

# Does the two-click tool queue anything else?

`0x526f20` (apply) is: `0x58a4e0` validate → `0x581b90` cost → `0x585d20`
budget → `0x53ac60` **order** (class 0x17: style index, two corners) →
select + `0x5269e0` panel refresh. It writes **no** grid cells, no landscape
flag and no decal. The grid cells (type 7) and the paddock ground are created
later by the order executor `0x53d0b0` (via `stFence` → `stPerimeter`
`0x584090` → `stWall` raster) and `AddAttraction` (the ground quad + the
`scene+0xec` basemap dirty flag). See Q1.

---

# Open

* **Laser state branch.** `FUN_005f3250` chooses state 7 (`Dino_additif`) +
  gscMaterialBank 4 when the *runtime material object's* byte `+2` is `0xff`,
  else state 8 (`Dino_SelectedWall`) + material 1. The mapping from the
  `.mesh` 21-byte header (last byte 0/1) to that runtime byte is not decoded;
  no static proof which branch material 1 (`laser2.tga`) takes. A breakpoint
  on `0x5f3339` / `0x5f339b` at first fence draw would settle it.
* The helper's valid/invalid colours: `+0xb4/+0xbc` are gscMaterialBank ids
  2/3 (proven); their stored colours were not dumped. Retail capture says
  blue (valid, ~(38,38,136)) vs maroon (invalid).
* `0x5f9730` draws the mesh with `this+0x84` and `this+0x94` (states 11/13 =
  `helper.ini` / `dino_HelperBuilding.ini`); which of the two slots is which
  stage, and whether the `pose_valid/pose_invalid` textures are bound there,
  is not proven (the ctor loads the three focus textures but the slots are
  filled from the state bank ids 0xb/0xd).
* `ground_occup_pad.tga` is nearly all `(0,0,0,0)`; the dual-texture
  Modulate(stage1) would darken the ground if bound to stage 1. The exact
  stage binding (which of the two slots goes to which stage) is open.
* No UV scroll / time pulse found in the fence or laser path; only a
  camera-distance fog factor in `0x5ed130`. A second capture pair at the same
  camera pose would prove/disprove a pulse.

# Follow-up

1. Runtime: break on `0x5f3339` and on `0x5f9730`, dump the material byte and
   the state ids in a built paddock; screenshot the preview disc colours.
2. Dump `gscMaterialBank` entries 1..4 after `FUN_005f8010` (colours) with a
   debugger; compare with the capture.
3. Port: implement the paddock ground as a static quad from the fence rect,
   `z = terrain(centre)+0.01`, state `Dino_AttractionGround`, two textures
   `boue_pad.tga` + `ground_occup_pad.tga`; implement preview mode 1 (8-unit
   sphere) and mode 2 (4-unit grid, half-size border).

# Retail-capture cross-check (2026-10-28, later harness run)

This resolves several Open items above; it does not rewrite them.

1. **Ground stage binding.** `ground_occup_pad.tga` is **not** nearly all
   black: each 128x128 quadrant is mostly white with grey mottling and a
   ragged transparent border (whole-atlas mean RGB 186.8, alpha 186.8; 48-70%
   of each quadrant is opaque white). The built interior reads
   (160.9,129.0,62.0), which is `boue_pad` (168.4,131.4,59.5) x the mostly
   white mottle, so stage 0 = `boue_pad` and stage 1 = `ground_occup_pad`
   Modulate. The black pixels are only the quadrant borders. Capture:
   `notes/harness/fence-rev-ground-after.png`; port check
   `build/paddock-ground4.png` (samples 126..195, 115..156, 53..75).
2. **Preview colours.** `fence-rev-template-valid.png` measures the dot grid
   RGB **(38,38,151)**; `-invalid.png` measures **(148,38,38)**. The
   pre-first-click start disc measures **(38,38,82)** in the same run, darker
   than the road-tool disc (38,38,136); whether that is a different material
   bank or a blend is still open. The grid is the load-bearing shape.
3. **No disc after the first click.** The pre-click cell disc disappears when
   the corner is stored; only the mode-2 grid remains.
4. **Lamps, not beams.** Close-up: the `laser2` billboards read as static
   cyan-white lamps **on the posts** (top peak 214,255,255; lower lamp at
   ~2/3 post height), not a beam between posts and not a glowing rail. No
   scroll. The ~5% luminance fade over 0.75 s is not proven to be a time
   pulse; the only known per-frame term is `0x5ed130`'s distance factor.
   Capture: `notes/harness/fence-rev-beams-post.png`.
5. **Port quirks needed for the quad.** `upload_mesh` adds +1 to V (engine
   bottom-left UVs), so the ground stores `v-1`; the ground atlases use
   `mgStateTextureWrapV Wrap` and are uploaded `GL_REPEAT`; the terrain detail
   pass leaves `u_uv2_world = 1/16`, so the extras loop resets it per item.
   Without all three the quad samples the transparent atlas border.

# Evidence

* Binary: `0x5ec7e0` gatAttraction ctor (ground quad + textures),
  `0x5ecdf0` wall-per-segment builder, `0x5ed2d0` gatFence render,
  `0x5ee970` gatWall ctor, `0x5ef1d0` gatWall render, `0x5f3250` mesh
  instance + per-material state, `0x5f3530` instance sub-object pick,
  `0x5f4e70` AddAttraction, `0x5f5120` AddBuilding, `0x5f5450` scene render
  (dirty flag), `0x5fc3a0` compose trampoline, `0x603010`/`0x603159` texture
  manager ctor, `0x603430` compose, `0x6020e0`/`0x602120` rect layer,
  `0x602320`/`0x602380` road layer, `0x605e70`/`0x605fc0` shadow layer,
  `0x605c20`/`0x605c80` text layer, `0x5faaa0` snap, `0x5f8c70` snap+height,
  `0x5f8850` helper draw switch, `0x5f98e0` mode 1, `0x5f9b00`/`0x5f9bb0`
  mode 2, `0x5f9ed0` mode 6, `0x5fa500` mode 8, `0x5f9730` mesh draw,
  `0x5f8010` helper ctor, `0x5f8590`/`0x5f85c0`/`0x5f85f0`/`0x5f8620`/
  `0x5f8650`/`0x5f8670` mode setters, `0x5f8a70` valid flag, `0x5f8a80`
  get point, `0x52ad00` two-click update, `0x52b0d0` click, `0x526f20`
  apply, `0x526e70` feature preview, `0x53d0b0` wall order executor,
  `0x53c121` order dispatcher.
* `0x608250`/`0x6082f0` gscMaterialBank create/destroy (`DAT_00676ecc`);
  `0x600800` state registry (state ids; `DAT_006755fc`).
* Data: `Fence.bwi` (textures `wall_03`, `wallcrach_3`, `boue_pad`,
  `ground_occup_pad`; meshes `fence_paddock_lod1`, `fence_lunchdoor`),
  `Show.bwi` (`boue_att`, `ground_occup_att`, `fence_show_lod1`),
  `Data/States/dino_attractionground.ini`, `dino_additif.ini`,
  `dino_wall.ini`, `Dino_WallTextureParam.txt`, `dino_selectedwall.ini`,
  `helper.ini`, `dino_HelperBuilding.ini`, `Data/Graphics/focus/Focus.ini`.
* `fence_paddock_lod1.mesh` material/geometry dump (this note §2.1/2.2).
* Captures: `notes/harness/cpg-fence-enclosure.png`,
  `cpg-fence2/3/4-enclosure.png`, `crop-paddock-ground.png`,
  `crop-fence-beams.png`, `enclosure-preview-valid.png`,
  `enclosure-preview-invalid.png`.
* Cross-refs: `notes/fence-construction-recon.md`,
  `notes/wall-raster-recon.md`, `notes/fence-wall-params-recon.md`,
  `docs/formats/mesh.md`, `docs/formats/bwi.md`.
