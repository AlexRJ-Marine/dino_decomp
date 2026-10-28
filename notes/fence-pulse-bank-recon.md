# Fence/helper pulse banks — raw binary evidence (2026-10-28)

Status: **formulas proven; the per-material branch used by the fence laser is
still open** (the recon pass `notes/fence-selected-state-recon.md` is resolving
it). Binary `/DinoIsland.exe-ebea17`, VAs.

The user confirms the fence lighting **pulses**, so these constants are the
source of truth (a single screenshot cannot show them).

## Owner and call site

`FUN_00600440` is the scene material manager ctor (called from the scene ctor
`FUN_005f37a0` at `0x5f3841`). It creates the `gscMaterialBank`
(`FUN_00608250`) and registers four animated materials:

| bank id | manager slot | update block |
| ---: | ---: | ---: |
| 2 | `manager+0x04` | block 4 |
| 3 | `manager+0x0c` | block 1 |
| 4 | `manager+0x14` | block 2 |
| 1 | `manager+0x1c` | block 3 |

Registration: `FUN_006081b0(id, slot)` after `FUN_00443880(slot)`; the ids are
`2,3,4,1` for slots `+4,+0xc,+0x14,+0x1c` (disassembly `0x600440`).

`FUN_00600550(manager, dt)` is called **every scene frame** from the scene
render `FUN_005f5450` (`0x5f553b`, `ECX = scene+0xe0`). It accumulates
`manager->time += dt` (float seconds) and rewrites the four materials.

The colour setter is `FUN_00439430(out, p2, p3, p4, p5)`:
`byte0 = p4, byte1 = p3, byte2 = p2, byte3 = p5` — i.e. the engine material
colour is stored **B,G,R,A** (D3D convention), matching the port's helper
formula.

## Block 1 — bank id 3 (sin 6 t)

```asm
0060056a FMUL 6.0            ; t * 6
00600570 FSIN
00600572 FADD 1.0
00600578 FMUL 0.5
0060057e FMUL 127.0
00600584 CALL __ftol         ; x = trunc((sin(6t)+1)*0.5*127), 0..127
...
PUSH 255 (A), PUSH 64 (R), PUSH 64 (G), PUSH 255-x
```

Colour bytes in memory order (B,G,R,A) = `(64, 64, 255-x, 255)` → **red**,
R ramps 128..255, G/B fixed 64. Period `2*pi/6 = 1.047 s`.

## Block 2 — bank id 4 (sin 6 t)

Same time/scale as block 1. Colour bytes `(255-x, 64, 64, 255)` → **blue**,
B ramps 128..255, R/G fixed 64. Period 1.047 s. This is the additive branch's
material; a blue additive pulse over the dark post matches the captured
cyan-white lamps (`laser2.tga` is a white/grey glow).

## Block 3 — bank id 1 (sin 4 t)

```asm
006005ef FMUL 4.0
006005f5 FSIN
006005fd FMUL 0.5
00600603 FLD ST0
00600605 FMUL 54.0
0060060b CALL __ftol         ; c2 = trunc((sin(4t)+1)*0.5*54), 0..54
00600610 FMUL 170.0
0060061f CALL __ftol         ; c3 = trunc((sin(4t)+1)*0.5*170), 0..170
BL = 0x50 - c2               ; 80-c2
CL = 0xFF - c3               ; 255-c3
```

Colour bytes `(255-c3, 80-c2, 80-c2, 255)` → **blue-white**: B 85..255,
R=G 26..80. Period `2*pi/4 = 1.571 s`. This is the material behind
`dino_selectedwall` (`TextureParam_WallSelected` stage 1 Modulate2x) in the
per-material default branch of `FUN_005f3250`.

## Block 4 — bank id 2 (sin 10 t)

```asm
00600644 FMUL 10.0
0060064a .. MOV AL,0x80      ; colour (128,128,128,255)
00600655 FSIN
00600680 FADD 1.0
00600686 FMUL 0.5            ; alpha = (sin(10t)+1)/2
CALL 0x600710                ; lerp grey->white by alpha
```

White/grey pulse, period `2*pi/10 = 0.628 s`; the alpha is the lerp weight
(no additive colour ramp proven).

## Exact float constants (file offsets)

| VA | value |
| --- | ---: |
| `0x006486a0` | 6.0 |
| `0x0062620c` | 4.0 |
| `0x006470bc` | 10.0 |
| `0x0064869c` | 127.0 |
| `0x00648698` | 54.0 |
| `0x00648694` | 170.0 |
| `0x0062588c` | 1.0 |
| `0x0062544c` | 0.5 |

## Per-material assignment (`FUN_005f3250`)

Called only from the gatWall ctor `FUN_005ee970` (`0x5eedd6` fence mesh,
`0x5eeec8` lunchdoor mesh) with `param_4 = 4` (material bank id) and
`param_5 = 7` (state id). Per mesh material entry:

* `byte[material_object+2] == 0xff` → `FUN_006083a0(4)` +
  `FUN_00608110(4)` (block 2, the **blue** pulse) + state 7 `dino_additif`
  (One/One, ZWrite N, fog N).
* otherwise → bank 1 (`FUN_00608110(1)`, block 3, the blue-white pulse) +
  state 8 `dino_selectedwall` (SrcAlpha/InvSrcAlpha, ZWrite Y, Modulate2x).

**Open**: which branch each of the two fence materials takes at runtime (the
byte is not the file header's last byte), and whether selection changes that
byte, the material id or adds the `ATTRACTION_LIST_SELECTION_LIGHT` effects.
The captured **unselected** lamps are cyan-white
(`notes/harness/fence-rev-beams-post.png`: brightest `(228,255,255)`, lower lamp
`(227,251,254)`, upper lamp `(163,194,242)`), consistent with the blue additive
block 2 (id 4) or the blue-white block 3 (id 1, `dino_selectedwall`). The
additive state 7 (One/One, ZWrite N) versus state 8 (SrcAlpha, ZWrite Y,
Modulate2x) and the pulse period (1.047 s vs 1.571 s) will distinguish them:
the fixed-camera time series in `notes/harness/fence-pulse-summary.txt` and the
selection recon will settle it.

## Evidence

* `FUN_00600440` (manager ctor, registrations), `FUN_00600550` (per-frame
  update, blocks 1..4), `FUN_005f5450` call site at `0x5f553b`,
  `FUN_00439430` (BGRA byte setter), `FUN_005f3250` (per-material branch),
  `FUN_006081b0`/`FUN_00608110` (bank insert/lookup), `FUN_00608250`/
  `FUN_006082f0` (bank create/destroy).
* Floats read from the PE image at the VAs above.
* Capture cross-check: `notes/harness/fence-rev-beams-post.png`,
  `notes/harness/fence-rev2-beams-2.png`.
