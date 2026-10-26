# Render-state files recon (2026-10-26)

Read-only analysis of `Data/States`. No writes to `game/` or `original/`.

## Layout

- 34 `*.ini` **profiles** (Standard, Texture, TextureAlpha, TextureLighted,
  TextureAlphaLighted, SkyBox, WaterSurface, Shape*, Font, ParticleAdd/
  Alpha/Sub, Helper, Waypoint, Censor, DetailMap, the Dino_* specials).
- 18 `*.txt` **fragments**: TextureParam*.txt, FogParam*.txt, Misc.txt.
- `Description.txt` is the grammar/manual, not a state file.

Profiles include fragments and other profiles with `mgStateInclude <file>`;
includes are relative to `Data/States`.

## Grammar

```
mgState<Name> [stage|Free] <values...>
// comment
```

- Texture states take a stage integer (0..7) first.
- `Free` releases the state (used by Censor.ini).
- Values: keywords (`Solid`, `CCW`, `LessEqual`, `Modulate`, `Clamp`,
  `Y`/`N`), integers, floats, `0x` masks, 16-char bit strings, `r,g,b[,a]`.

`Description.txt` documents the accepted values per state. Four names used in
the files are missing/misspelled there:

```
mgStateAlphablendOp            (doc: mgStateAlphaBlendOp)
mgStatePointSizeMax            (doc: mgStatePointSizeMAx)
mgStateStencilFunc             (undocumented)
mgStateTextureMaxAnisotropy    (undocumented)
```

## Key fragments

TextureParam.txt (default 3D):
```
stage 0: Op Modulate, Arg1 Texture, Arg2 Diffuse, Arg3 Diffuse,
         AlphaOp Modulate, AlphaArg1 Texture, AlphaArg2 Diffuse,
         Result Current, CoordIndex 0, WrapU/V/W Wrap, Border 0,0,0,0,
         Min/MagFilter Linear, MipMap N, MipFilter Point, LODBias 0.0,
         MaxMipLevel 0, MaxAnisotropy 1, Transform Disable, BumpEnv* 0
stage 1: Op Disable, AlphaOp Disable
```

TextureParam_WaterSurface.txt: stage 0 SelectArg1, stage 1 Modulate2x
(Texture x Current), stage 2 Disable.

FogParam.txt: start 1500, end 2000, colour 255,220,165.

Misc.txt: Dither Y, LinePattern 0 + zero bits, Stencil N with all Keep/Always/
0, LocalViewer Y, PointSize 1.0/Min/Max, sprite/scale off.

Standard.ini: Solid/CCW/Gouraud, clip Y, z Y/Y/LessEqual/0, Texture Y +
TextureParam, AlphaBlend Y SrcAlpha/InvSrcAlpha/Add, AlphaTest Y ref 128
LessEqual, Fog Y + FogParam, Light Y ColorVertex N NormalizeNormals Y
Specular N, sources Color1/Color2/Color2/Material.

## Tooling

- `tools/formats/render_states.py` — parser + include expansion, stage/free
  handling, typed getters.
- `tests/test_render_states.py` — 34 profiles, Standard expansion, water
  stages, stage classification, documented names.
- `docs/formats/render-states.md` — spec.

## Port hand-off

Default 3D material = texture x diffuse, repeat wrap, linear min/mag, no
mipmaps. Fog = linear 1500..2000, colour (255,220,165). Standard profile is
the opaque lit reference; Shape2D/Font profiles cover the HUD.

## Open

- Profile selection in the engine (by name string).
- Dino_* profile pairing with actors.
- The four doc drifts.
