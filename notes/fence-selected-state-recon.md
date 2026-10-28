# Fence / paddock "selected" state and the time pulse — raw evidence (2026-10-28)

Status: **the pulse driver is fully decoded; the per-material branch is
resolved; the selection switch exists but no reachable caller was found in
this build.** Binary `/DinoIsland.exe-ebea17` (Ghidra MCP + a byte scan of the
PE), all addresses VA. No `src/` change.

Complements `notes/fence-ground-beam-preview-recon.md`,
`notes/fence-construction-recon.md`, `notes/wall-raster-recon.md`.

**Lane collision:** `notes/fence-pulse-bank-recon.md` is dirty (another lane,
commit `da30fc6`). I did not edit it. §7 corrects three of its bank↔slot
assignments from the ctor disassembly; its formulas/constants are otherwise
right.

## Question

What exactly changes in the fence/wall rendering when an enclosure is
selected, what triggers it, and what is the exact time pulse (unselected and
selected) of the fence body and the `laser2.tga` lamp?

---

# 1. The pulse driver

## 1.1 Owner, call site, lifetime

`FUN_005f37a0` (gpkParkScene ctor, `D:\Dino\Dev\DISPLAY\Src\GraphPar\...`,
line 0x123) allocates **0x24 bytes**, calls `FUN_00600440`, and stores the
result at **`scene+0xe0`**:

```asm
005f37a0 ...                                   ; gpkParkScene ctor
PUSH 0x24 ; ... CALL 0x004052c0
CALL 0x00600440                                ; palette ctor
MOV [ESI+0xe0], EAX
```

`FUN_005f5450` (gpkParkScene::Render) calls the updater **once per park frame**:

```asm
005f5511 MOV ECX,[ESI+0xf4]
...
005f5534 MOV ECX,[ESI+0xe0]     ; = palette
005f553a PUSH EBX               ; dt
005f553b CALL 0x00600550        ; palette->Update(dt)
```

`0x600550`'s only two callers are the scene render (dt) and the palette ctor
(`FUN_00600440` calls it with dt = 0, i.e. evaluates the colours at t = 0).
**There is no selection gate**: the update runs every frame for every park.
One render path passes dt = 0 (`0x51e85b` `MOV ECX,[ESI]; PUSH 0x0; CALL
0x5f5450`), i.e. the pulse freezes on that path; the normal path passes the
park frame dt (`FUN_0051d180(dt, scene)` → `0x5f5450`).

dt is in **seconds**: the same park frame dt is used by the camera
(`notes/camera-controls-binary-recon.md` §table: `yaw += dt * 1.570796`), and
`0x600550` accumulates `time += dt` then feeds `sin(k*time)` with k = 4/6/10.

## 1.2 Object layout (0x24 bytes, ctor `FUN_00600440`)

```asm
00600447 LEA EDI,[ESI+0x4]   ; +0x04 -> material 1
00600451 LEA EBP,[ESI+0xc]   ; +0x0c -> material 3
0060045b LEA EBX,[ESI+0x14]  ; +0x14 -> material 2
00600465 LEA ECX,[ESI+0x1c]  ; +0x1c -> material 4
0060046d MOV dword [ESI],0   ; +0x00 = float time, starts at 0
00600473 CALL 0x00608250     ; gscMaterialBank ctor (sets DAT_00676ecc)
...
0060049d PUSH EBX ; PUSH 0x2 ; CALL 0x6083a0 / 0x6081b0   ; id 2 -> +0x14
006004bd PUSH EBP ; PUSH 0x3 ; ...                        ; id 3 -> +0x0c
006004e0 PUSH EBX(=+0x1c) ; PUSH 0x4 ; ...                ; id 4 -> +0x1c
00600500 PUSH EDI ; PUSH 0x1 ; ...                        ; id 1 -> +0x04
0060050f PUSH 0x0 ; CALL 0x00600550                       ; evaluate at t=0
```

So the **global material bank** `DAT_00676ecc` gets four animated materials:

| bank id | palette slot | animator block | colour |
| ---: | ---: | ---: | --- |
| 1 | `+0x04` | block 4 | grey → white (sin 10 t) |
| 2 | `+0x14` | block 2 | blue (sin 6 t) |
| 3 | `+0x0c` | block 1 | red (sin 6 t) |
| 4 | `+0x1c` | block 3 | blue-white (sin 4 t) |

This mapping is the **corrected** one (§7).

## 1.3 Exact time functions (`FUN_00600550`)

Colour packing: `FUN_00439430(out, p2, p3, p4, p5)` writes
`out[0]=p4, out[1]=p3, out[2]=p2, out[3]=p5` → **B,G,R,A** in memory.
`FUN_00443ba0(slot,&colour)` writes the dword at material **+0x00**; the
material struct offsets +0x04/+0x08/+0x0c are written by `0x443b60`/`0x443b80`
/`0x443bc0` (D3DMATERIAL8 order Ambient/Specular/Emissive is *inferred*, only
+0 is proven to be the colour that goes to the D3D material).

Let `t` = accumulated seconds. All four write **alpha = 0xFF**.

```text
id 3 (palette+0x0c)  x6 = (sin(6t)+1)/2 ; c = trunc(127*x6)
                     RGB = (0xFF-c, 0x40, 0x40)        ; red
id 2 (palette+0x14)  same c
                     RGB = (0x40, 0x40, 0xFF-c)        ; blue
id 4 (palette+0x1c)  x4 = (sin(4t)+1)/2
                     r = g = 0x50 - trunc(54*x4)
                     b     = 0xFF - trunc(170*x4)      ; blue-white
id 1 (palette+0x04)  x10 = (sin(10t)+1)/2
                     lerp(0x808080FF, 0xFFFFFFFF, x10) ; grey 128..255
                     then +0x04/+0x08/+0x0c = 0        ; one-off, every frame
```

Constants (read from the image):

| VA | value | use |
| --- | ---: | --- |
| `0x006486a0` | 6.0 | id 3 / id 2 phase |
| `0x0062620c` | 4.0 | id 4 phase |
| `0x006470bc` | 10.0 | id 1 phase |
| `0x0064869c` | 127.0 | id 3 / id 2 amplitude |
| `0x00648698` | 54.0 | id 4 r/g amplitude |
| `0x00648694` | 170.0 | id 4 b amplitude |
| `0x0062588c` | 1.0 | +1 before /2 |
| `0x0062544c` | 0.5 | /2 |

Periods: id 1 `2π/10 = 0.6283 s`; id 2 / id 3 `2π/6 = 1.0472 s`;
id 4 `2π/4 = 1.5708 s`.

## 1.4 Worked values (RGB 0..255)

| t (s) | id 1 grey | id 3 red | id 2 blue | id 4 blue-white |
| ---: | --- | --- | --- | --- |
| 0.00 | 191 | (192,64,64) | (64,64,192) | (53,53,170) |
| 0.25 | 229 | (129,64,64) | (64,64,129) | (31,31,99) |
| 0.50 | 130 | (183,64,64) | (64,64,183) | (29,29,93) |

(trunc toward zero, as `__ftol`.)

Cross-check of the mapping: the construction helper binds **bank 2 (valid)**
and **bank 3 (invalid)** (`FUN_005f8010`, `+0xb4`/`+0xbc`). Measured preview
colours are blue `(38,38,151)` and maroon `(148,38,38)`
(`notes/fence-ground-beam-preview-recon.md`, retail-capture section) — i.e.
bank 2 at mid phase `(64,64,192)` and bank 3 at `(192,64,64)` after lighting.
So the id↔colour mapping above is consistent with the captures, and the
preview discs pulse too (1.047 s).

---

# 2. Which material the fence uses — the `FUN_005f3250` branch

`FUN_005f3250(this, meshPath, texDir, matBankId=4, stateId=7)` is called twice
from the `gatWall` ctor `FUN_005ee970` (`0x5eedd6` body mesh, `0x5eeec8`
lunch-door mesh) with `(4, 7)`.

`0x5f3250` builds **two render passes** in one block:

| pass | draw item | material vector | state vector |
| ---: | ---: | ---: | ---: |
| A (`gatWall+0x14 == 0`) | `this+0x04` | `this+0x08` | `this+0x28` |
| B (`gatWall+0x14 != 0`) | `this+0x14` | `this+0x18` | `this+0x38` |

(`FUN_0045cc30` copies material proxies, `FUN_0045cab0` copies state proxies;
`0x5f3530` picks `(this+4,this+0x24)` or `(this+0x14,this+0x34)` from the byte
`gatWall+0x14`.)

Per mesh material entry the loop does:

```asm
005f3334 CALL 0x00443be0        ; out = *(material_data + 8)
005f3339 CMP byte [EAX+2],0xff  ; EAX is the address of the out local
005f333d JNZ 0x5f339b           ; -> default branch
; ---- 0xff branch (laser) ----
005f3346..005f3358  material bank 4 -> [this+0x08][i]
005f3360..005f3372  state 7 (dino_additif) -> [this+0x28][i]
005f337b..005f3390  material bank 4 -> [this+0x18][i]
005f3398..005f33cc  state 7 -> [this+0x38][i]
; ---- default branch ----
005f339b..005f33b1  material bank 1 -> [this+0x18][i]
005f33b9..005f33cc  state 8 (dino_selectedwall) -> [this+0x38][i]
```

Note the asymmetry: the 0xff material is overridden in **both** passes, the
default material only in **pass B**.

## 2.1 Resolving the 0xff test

`FUN_00443be0` (disassembly):

```asm
00443be0 MOV EAX,ECX                ; slot
00443be8 MOV EDX,[EAX+4]            ; material id
00443beb MOV ECX,[DAT_00674528+0x6c]; material cache
00443bef CALL 0x0043c370            ; id -> material data
00443bf4 MOV ECX,[EAX+8]            ; data[2]
00443bf7 MOV EAX,[ESP+4]            ; = &out
00443bfb MOV [EAX],ECX              ; *out = data[2]
00443bfd RET 4
```

so the test is `byte 2 of the dword at material_data+8 == 0xFF`.

Census over **all 416 `.mesh` material headers** in the retail tree (byte
scan for `?? ?? ?? FF | FF FF FF FF | CD CC CC 3D | ??.. | 00 00 00 FF`):

* 414 materials have the 4th header dword `0xFFE6E6E6` (byte 2 = `0xE6`);
* exactly **2** have `0xFFFF0000` (byte 2 = `0xFF`) — the second material of
  `fence_paddock_lod1.mesh` and `fence_lunchdoor.mesh`, i.e. **`laser2.tga`**.

The test therefore selects the pure-red-authored laser material and nothing
else in the game. `material_data+8` is the runtime copy of the `.mesh` 21-byte
header's 4th dword (`+0x0c`, the "colour" field of `docs/formats/mesh.md`);
the exact runtime field name is inferred (proven only by this unique
correlation). This **disproves** the texture-alpha hypothesis in
`notes/fence-pulse-bank-recon.md` §"Branch hypothesis".

Consequence:

* **laser** (`laser2.tga`, colour `0xFFFF0000`) → bank **4** + state **7**
  (`Dino_additif.ini`: One/One Add, ZWrite N, ZFunc Less, AlphaTest N, Fog N,
  Light Y, stage0 Modulate(Texture,Diffuse), stage1 off).
* **fence body** (`fence_paddock.tga`, colour `0xFFE6E6E6`) → bank **1** +
  state **8** (`Dino_SelectedWall.ini`) — but only in pass B.

---

# 3. State ids (correction) and the selected-wall state

`FUN_00600800` registers the park states, ids in the image:

| id | file |
| ---: | --- |
| 3 | `data\states\dino_wall.ini` |
| 4 | `data\states\dino_shadow_wall.ini` |
| 5 | `dino_attractionground.ini` |
| 7 | `dino_additif.ini` |
| 8 | `dino_selectedwall.ini` |
| 11 | `helper.ini` |
| 13 | `dino_HelperBuilding.ini` |

**Correction:** state 4 is `Dino_Shadow_Wall`, not `Dino_Wall`. The earlier
brief's "unselected body uses dino_wall.ini (state 4)" mixes the two; the
`FUN_005f3250` argument `4` is a **material bank id**, not a state id, and the
state argument is `7`.

`Dino_SelectedWall.ini` + `TextureParam_WallSelected.txt`:

* ZBuffer Y / Write Y / Func **Less** (not LessEqual);
* AlphaBlend **Y**, SrcAlpha / InvSrcAlpha;
* AlphaTest **N**;
* Fog Y; Light Y, all four `*MatSource = Material`;
* stage 0: `SelectArg1(Texture)` — **no diffuse modulation**;
* stage 1: `Modulate2x(Diffuse, Current)` = `2 * diffuse * texture`, alpha
  `SelectArg2(Current)`; stage 2 off.

`Dino_Wall.ini` + `Dino_WallTextureParam.txt` (state 3) for comparison:
opaque, AlphaTest Y 128 Greater, stage 0 `Modulate(Texture,Diffuse)`,
stage 1 off.

So pass B is not a tint: with the pulsing diffuse `0.5..1.0`, stage 1
`Modulate2x` produces `1.0x..2.0x` the texture — the wall body **brightens and
breathes**, saturating to white at the top of the pulse. State 8's alpha blend
also makes the texture's alpha meaningful where state 3 alpha-tests it.

---

# 4. The selection switch and what triggers it

## 4.1 Chain

```text
gatFence vtable+0x24 = 0x5ed240   (gatAttraction slot, address 0x648ac4)
  -> for each wall in gatFence+0x54:
       wall->vtable+0x24 = 0x508980 = MOV byte [wall+0x14], arg
```

`0x5ed240` (decompiled) loops `[this+0x44..+0x48]` and calls `[wall vtable+0x24]`
with its argument; `0x508980` is a one-instruction setter
`*(u8*)(this+0x14) = arg`. `gatWall+0x14` is created as 0 by the base ctor
`FUN_00508740` (`param_1[5] = 0`).

## 4.2 Who calls the switch

`0x5ed240` has exactly **one** direct call in the image: the thunk

```asm
00623380 MOV EAX,[ESP+4] ; PUSH EAX ; CALL 0x005ed240 ; RET 4
```

which itself is only referenced from the vtable at **0x648a8c** (vtable base
0x648a60). A scan of `.text` for the immediate `0x648a60` (including `LEA`)
finds **no** reference, so that vtable is never installed and the thunk is
dead.

`0x5ed280` (the same class's vtable+0x08) is also only reachable through a
vtable; it gates on `stFence+0x14` and then calls `wall->vtable+0x08 =
0x508d30`, which is `{ return; }` — inert.

## 4.3 The generic "activate/deselect" object that does call `vtable+0x24`

`FUN_005e74f0` (ctor, 0x70 bytes, vtable **0x648214**, created from the
ParkWorld init `FUN_0053a140` at `0x53a27f`; class id 0x16 at
`DAT_00676db4`, source path 0x66ff90 = `...\APPLI\Src\DinoResident\drsShow.cpp`,
name getter returns `"drsShow"`). Note the class identity: it is the **show
resident**, so the most likely retail consumer of this activate path is a
*running show's* arena graphic (gatShow derives from gatAttraction and shares
`+0x24 = 0x5ed240`), not necessarily a paddock click.

| field | use |
| --- | --- |
| `+0x04` | owner/context (`0x5e73c0`) |
| `+0x08` | enable byte |
| `+0x10` | **target object** |
| `+0x44..+0x50`, `+0x4c` (0x18-byte records) | loaded table (`0x5e78b0`, a text-file loader) |
| `+0x5c/+0x60` | second table |

vtable slots:

* `+0x38` = `0x5e7440` — **Activate**:
  `if (*(u8*)(this+8) && this+0x10) { target->vtable[+0x24](1); notify all in
  DAT_00676dac via vtable[+0x10]; }`
* `+0x3c` = `0x5e7490` — **Deactivate**: same with `(0)` and
  `vtable[+0x14]`.

If the target is a `gatFence`/`gatAttraction`, `+0x24` is `0x5ed240`, so this
call is what would flip every wall to pass B.

**Not proven / likely dead for paddocks:**
* no direct caller of `0x5e7440` / `0x5e7490`;
* the only `call [reg+0x38]` / `[reg+0x3c]` in the whole park-view range are
  `0x52b1cf` / `0x52b2f6`, and those are `pvwStateFenceConstruction`'s **own**
  virtual slots (`CALL [EDX+0x38]` where EDX = `[ESI]`, ESI = the state), not
  this object;
* no `call [reg+0x24]` anywhere reaches a gatFence/gatAttraction except the
  in-function call inside `0x5ed240`.

So: the selected-wall pass is **wired but no reachable caller was found in
this build**, and the one class that does drive `vtable+0x24` is the show
resident. A paddock-click path to pass B is therefore **not proven** and is
Open (§8); the only *proven* consumer of the SelectedWall material/state pair
would be whatever activates a drsShow target.

`stFence` side note: `stFence+0x14` exists (tested by `0x5ed280`) but that
method's payload is a no-op, so it has no visible effect.

---

# 5. The `ATTRACTION_LIST_SELECTION_LIGHT1..10` strings

They are **2D overlay widgets, not 3D lights**:

* `Data/Overlay/ListInterface.Ini:2319..2500` defines
  `[ATTRACTION_LIST_SELECTION_LIGHT_GROUP]` and ten children
  `ATTRACTION_LIST_SELECTION_LIGHT1..10`, each `PARENT =
  "ATTRACTION_LIST_SELECTION_LIGHT_GROUP"`, `TEXTURE = "SELECTION_LIGHT"`.
* `Data/Overlay/ParkGraphicDB.Ini:2088` defines `[SELECTION_LIGHT]` (atlas
  rectangle into a menu texture).
* The only executable reference is `FUN_00514ad0@0x514b9b/0x514c4d`, the
  attraction-list panel constructor (a `pvw`/`ivw` list panel): it looks the
  ten widget names up by string and stores the widget pointers (the
  "selected row" role is inferred from the name and the INI layout, the ctor
  only resolves them).

They have nothing to do with the fence's 3D lighting.

`OPEN_PADDOCK_LIST_PANEL` / `PADDOCK_AMOUNT` are likewise HUD widgets:
`ParkInterface.Ini:570/598`, wired by the park interface ctor
`FUN_00527d30`, callback `LAB_00528df0`.

---

# 6. Does the pulse change with selection?

No. The pulse is unconditional (one call per park frame, §1.1) and only writes
material colours. Selection changes **which material** the body points at:

* unselected (pass A): body = the mesh's authored material proxy
  (`fence_paddock.tga`, header diffuse `0xFF969696`, static); laser = bank 4 +
  state 7 (pulsing, 1.5708 s).
* selected (pass B): body = bank 1 + state 8 (grey 128..255, 0.6283 s, and
  stage-1 `Modulate2x` doubling); laser unchanged (bank 4 + state 7).

Both billboard heights of the lamp are the **same material** (material index 1
of the same mesh, 8 triangles = two crossed quads per height), drawn in one
material pass — so they always pulse **in phase** and with the same colour.
The lamps are one global material, so every paddock in the park pulses in
phase (one palette per `gpkParkScene`).

The only other per-frame term in the fence path remains `FUN_005ed130`: it
builds a quad at the fence position scaled by
`clamp(cameraDistance-50,0,120) * (1/120) * 0.5 + 0.05` (0.05..0.55) and draws
it with the fence's texture slots — a distance fade, not a time pulse.

---

# 7. Corrections to `notes/fence-pulse-bank-recon.md` (dirty, not edited)

Its table assigns bank ids to palette slots as `2→+4, 3→+0xc, 4→+0x14, 1→+0x1c`.
The ctor disassembly (`0x600447`..`0x60050a`, quoted in §1.2) shows the ids are
pushed in the order **2, 3, 4, 1** against the slots **+0x14, +0x0c, +0x1c,
+0x04**, i.e.:

```text
+0x04 = id 1 (grey/white sin 10t)   [its table: id 2]
+0x0c = id 3 (red sin 6t)            [correct]
+0x14 = id 2 (blue sin 6t)           [its table: id 4]
+0x1c = id 4 (blue-white sin 4t)     [its table: id 1]
```

So the **laser** (bank 4) uses the **sin 4t blue-white** pulse and the
**selected wall** (bank 1) uses the **sin 10t grey→white** pulse — not the
reverse. Its per-block formulas and float constants are correct.

---

# 8. Open

* **Reachability of pass B.** No caller of `0x5ed240` survives outside the
  never-installed vtable at `0x648a60`; the drsShow activate/deselect
  (`0x5e7440`/`0x5e7490`) also has no caller found. Either the feature is dead
  in this build, or the call goes through a table I did not find. A runtime
  breakpoint on `0x5ed240` / on reads of `gatWall+0x14` while clicking a
  paddock would settle it in one run.
* **Pass A's render state for the body.** In pass A only the laser gets a
  state override; the body's state proxy is the mesh's own (probably -1 = "no
  override", so the previously-set state persists). Whether retail therefore
  draws the unselected body with `Dino_Wall` (state 3, from the gatWall's own
  slot `+0x9c`) is not proven.
* **Material struct field names.** Only `+0x00` is proven to be the colour the
  pulse writes; `+0x04/+0x08/+0x0c` = Ambient/Specular/Emissive is inferred
  from the D3DMATERIAL8 order and from the id-1 block zeroing all three.
* **`material_data+8` field name.** Proven to behave as the `.mesh` header
  colour dword by the 416-material census; not decoded from the loader.
* **Which click sets `drsShow+0x10`.** The loader `0x5e78b0` fills the tables
  from a text file; the code that assigns the target and the enable byte was
  not found in this pass.
* Retail `.mesh` bodies: are they lit by more than one light? The lamp's
  white look (capture `(228,255,255)`) despite a blue diffuse needs the
  material's `Ambient` (0xFFFFFFFF from the mesh header) to dominate; that is
  consistent but not proven.

---

# Retail capture cross-check (live game, 2026-10-28)

The fixed-camera time series (`notes/harness/fence-pulse-run1-frames.tsv`,
12 frames over 547/562 ms at the same pose) **proves pass B is reachable in
this build**:

* **Unselected:** the fence body, rail, stone base and the sampled lamp
  pixels are static (body `(65,57,79)` constant; rail `(158,161,13)`
  constant; stone `(137,129,112)` constant). The lamp tip's blue channel
  varies by ~1/255, consistent with the additive bank-4 ramp under the
  dominant white ambient.
* **Selected** by a left click on the fence post at view `(321,130)`: an
  info panel opens and the **whole fence** starts pulsing at
  `w ≈ 10 rad/s` (period **0.63 s**): body `204 → 399`, stone
  `384 → 733`, rail `336 → 535`, lamps saturate to `(255,255,255)`; the sand
  inside/outside the fence is unchanged (`(173,137,67)` constant).
* Esc closed the panel and the fence returned to static.

So the paddock-click -> `gatWall+0x14` -> pass B path exists at runtime; the
static xref scan in §4.2 missed the caller (likely the park interface ->
`drsShow` activate path). The port implements the selection click.

Screenshots: `notes/harness/fence-pulse-unsel-01..12-client.png`,
`fence-pulse-sel-01..12-client.png`, `fence-pulse-sel1-panel.png`.

# 9. Follow-up experiments

1. x32dbg: breakpoint `0x5f3339` on the first fence draw; read the `0xFF`
   branch taken per material (expected: body = not-taken, laser = taken) and
   the per-material state ids actually bound.
2. x32dbg: read `gatWall+0x14` for the walls of a built paddock, then click
   the paddock / the HUD paddock-list button and re-read. Breakpoint
   `0x5ed240` and `0x5e7440`.
3. Time series at a fixed camera: sample the lamp pixel for 3 s and fit
   `A + B*sin(4t+phi)` (id 4) vs `sin(10t)` (id 1); the period distinguishes
   them without a debugger.
4. Port: implement the four material pulses driven by one park clock
   (`time += dt` seconds) and a `selected` flag per attraction that selects
   pass A/B; the selected body must use `Modulate2x(Diffuse, Current)` in
   stage 1, not a simple tint.

# 10. Evidence

* Binary: `FUN_005f37a0` (scene ctor, `+0xe0`, alloc 0x24), `FUN_00600440`
  (palette ctor), `FUN_00600550` (per-frame update), `FUN_005f5450@0x5f553b`
  (call site), `FUN_0051d180`/`0x51e85b` (dt / dt = 0 paths), `FUN_00600710`
  (colour lerp), `FUN_00439430` (BGRA pack), `FUN_00443ba0` / `0x443b60` /
  `0x443b80` / `0x443bc0` / `0x443be0` (material field accessors),
  `FUN_00608250`/`0x6083a0`/`0x608110`/`0x6081b0` (bank create/lookup/insert),
  `FUN_005f3250` (per-material branch), `FUN_005ee970` (gatWall ctor),
  `0x5f3530` (pass picker), `0x488360`/`0x488390` (instance draw),
  `0x45cab0`/`0x45cc30` (state / material vector copy), `0x5ef1d0` (gatWall
  render), `0x5ed2d0` (gatFence render), `0x5ed240` (set walls' pass),
  `0x508980` (byte setter), `0x508740` (base ctor, `+0x14 = 0`), `0x5ed280`
  (stFence+0x14 gate), `0x5ed130` (distance fade quad), `FUN_00600800` (state
  registry), `FUN_0050b2f0`/`FUN_005e3fe0`/`0x46d700` (state bank/lookup/assign),
  `FUN_005e74f0` (drsShow ctor), `0x5e7440`/`0x5e7490` (activate/deselect),
  `0x5e73c0`/`0x5e78b0`/`0x5e7ec0`, `FUN_0053a140@0x53a27f` (ParkWorld init),
  `FUN_00527d30` (park interface ctor), `FUN_00514ad0` (attraction list panel),
  `0x623380` (dead thunk), vtable 0x648214 / 0x648a60 / 0x648aa0 / 0x6485a8.
* Data: `Data/States/Dino_Wall.ini`, `Dino_Shadow_Wall.ini`,
  `Dino_SelectedWall.ini`, `Dino_additif.ini`, `Dino_WallTextureParam.txt`,
  `TextureParam_WallSelected.txt`, `Data/Overlay/ListInterface.Ini`
  (2319..2500), `ParkGraphicDB.Ini` (2088), `ParkInterface.Ini` (570, 598).
* PE byte scans (local script, read-only): material-header census 416 headers
  / 2 with `0xFF FF 00 00`; immediate/vtable reachability scans for
  `0x648a60`, `0x5ed240`, `0x5e7440`, `0x5e7490`, `call [reg+0x24/0x38/0x3c]`.
