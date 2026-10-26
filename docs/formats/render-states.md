# `Data/States` — mcLib render-state profiles and fragments

Status: **solved structurally.** 34 profiles and 18 fragments parse; the
grammar is documented by the retail `Description.txt`. This is the original
fixed-function/D3D8 pipeline state set, so it is the reference for the port's
material system.

## Two layers

| Layer | Files | Role |
| --- | ---: | --- |
| Profiles (`*.ini`) | 34 | pipeline-level state: fill, cull, z-buffer, alpha blend/test, fog, light, and includes |
| Fragments (`*.txt`) | 18 | per-texture-stage sampler/blend state (`TextureParam*`), fog (`FogParam*`), misc (`Misc.txt`) |
| Grammar (`Description.txt`) | 1 | reference text, not loaded |

Profiles `mgStateInclude` fragments (and other profiles). Includes are
relative to `Data/States` and expand inline in order.

## Grammar

```text
mgStateFillMode        Solid
mgStateZBufferFunc     LessEqual
mgStateTextureOp       0   Modulate
mgStateFogDistColor    255,220,165
mgStateInclude         Misc.txt
mgStateTextureOp       Free
```

- `//` starts a comment.
- A state is `mgState<Name>` followed by its values.
- Texture states take a **stage** integer (0..7) as the first value.
- `Free` (case-insensitive) releases the state instead of setting it — e.g.
  `Censor.ini` frees the stencil function and texture op.
- Values are keywords (`Solid`, `CCW`, `LessEqual`, `Modulate`, `Clamp`,
  `Y`/`N`), integers, floats, bit masks (`0x00000000`), bit strings
  (`0000000000000000`) and colours (`r,g,b[,a]`).

`Description.txt` lists the accepted values per state (e.g.
`mgStateTextureOp` supports `Disable`, `SelectArg1/2`, `Modulate`,
`Modulate2x/4x`, `Add`, `AddSigned`, `Subtract`, `AddSmooth`,
`BlendDiffuseAlpha`, `BlendTextureAlpha`, `BlendFactorAlpha`,
`BlendTextureAlphaPM`, `BlendCurrentAlpha`, `Premodulate`,
`ModulateAlpha_AddColor`, `ModulateColor_AddAlpha`,
`ModulateInvAlpha_AddColor`, `ModulateInvColor_AddAlpha`, `BumpEnvMap`,
`BumpEnvMapLumminance`, `DotProduct3`, `MultiplyAdd`, `Lerp`).

Four names used by the retail files are missing from or misspelled in
`Description.txt`: `mgStateAlphablendOp` (doc `mgStateAlphaBlendOp`),
`mgStatePointSizeMax` (doc `mgStatePointSizeMAx`), `mgStateStencilFunc` and
`mgStateTextureMaxAnisotropy` (not documented).

## Profiles

`Standard.ini` is the canonical opaque lit profile:

```text
mgStateInclude Misc.txt
FillMode Solid      CullMode CCW        ShadeMode Gouraud
Clip Y              ZBuffer Y           ZBufferWrite Y
ZBufferFunc LessEqual                   ZBufferBias 0
Texture Y           Include TextureParam.txt
AlphaBlend Y        SrcAlpha / InvSrcAlpha / Add
AlphaTest Y         Ref 128             Func LessEqual
Fog Y               Include FogParam.txt
Light Y             ColorVertex N       NormalizeNormals Y
Specular N          DiffuseMatSource Color1
SpecularMatSource Color2                AmbientMatSource Color2
EmissiveMatSource Material
```

The other profiles are variants: `Texture.ini` / `TextureAlpha.ini` /
`TextureLighted.ini` / `TextureAlphaLighted.ini`, `SkyBox.ini`,
`WaterSurface.ini`, `Shape*.ini` (2D/3D, colour, alpha, texture),
`Font.ini`, `ParticleAdd/Alpha/Sub.ini`, `Helper.ini`, `Waypoint.ini`,
`Censor.ini`, `DetailMap.ini`, and the `Dino_*` minimap/shadow/wall/tile
special cases.

## Fragments

### `TextureParam.txt` (default 3D texture stages)

```text
stage 0: Op Modulate, Arg1 Texture, Arg2 Diffuse, Arg3 Diffuse
         AlphaOp Modulate, AlphaArg1 Texture, AlphaArg2 Diffuse
         Result Current, CoordIndex 0
         WrapU/V/W Wrap, BorderColor 0,0,0,0
         MinFilter Linear, MagFilter Linear
         MipMap N, MipFilter Point, MipMapLODBias 0.0, MaxMipLevel 0
         MaxAnisotropy 1, Transform Disable, BumpEnv* 0.0
stage 1: Op Disable, AlphaOp Disable
```

So the default 3D material is `texture × diffuse` with no mipmaps, linear
min/mag, repeat wrap, and stage 1 off.

### `TextureParam_WaterSurface.txt`

Three stages: `SelectArg1` (base texture), `Modulate2x` (`Texture × Current`)
at stage 1, `Disable` at stage 2 — the water dual-texture blend.

### `FogParam.txt`

```text
FogDistStart 1500
FogDistEnd   2000
FogDistColor 255,220,165
```

### `Misc.txt`

```text
Dither Y, LinePattern 0 0000000000000000
Stencil N, Fail/ZFail/Pass Keep, Func Always, Ref 0, Mask 0, WriteMask 0
LocalViewer Y
PointSize 1.0, Min 1.0, Max 1.0, Sprite N, Scale N, ScaleA/B/C 0.0
```

## Reader

`tools/formats/render_states.py`:

```text
python tools/formats/render_states.py <States-dir>
python tools/formats/render_states.py Standard.ini
```

`read_render_state` expands `mgStateInclude` recursively (missing include
raises), returns `RenderState.states` (ordered `State(name, args, source,
line)`) and `.includes`. `State` exposes `stage` (for texture states),
`free`, `text`, `as_int`, `as_float`, `as_bool`, `as_color`.
`RenderState.all(name, stage)` / `.last(name, stage)` query it. Tests:
`tests/test_render_states.py` (all 34 profiles, Standard include expansion,
water stages, stage classification, documented names).

## Port hand-off

- Reproduce `Standard` for opaque lit meshes, `TextureAlpha` for the current
  texture-modulated pipeline, `SkyBox`, `WaterSurface` and `Shape2D`
  variants for the HUD.
- The port's current GLSL pipeline corresponds to stage 0 `Modulate`
  (`texture × diffuse`) with `Wrap` and linear filtering; retail disables
  mipmaps for 3D materials, which is why the meshes look aliased.
- Fog is a linear distance fog 1500..2000 in world units with colour
  `(255,220,165)`.

## Evidence

- `Data/States/Description.txt`, `*.ini`, `*.txt` (read-only).
- `tools/formats/render_states.py`, `tests/test_render_states.py`.

## Open

- Which code path selects each profile (the renderer names them by string).
- The `Dino_*` profile names' exact pairing with park actors.
- Why four states drifted from `Description.txt`.
