# Data/Config recon (2026-10-26)

Read-only analysis of `Data/Config`.

## Files

```
Dino.ini          230 B   user settings
Start.Ini         288 B   engine startup
camera.ini       1885 B   5 identical camera blocks
KeyMapper.ini     851 B   17 command bindings
key/KeyFrancais.ini 7698 B  144 DIK/MIK rows (AZERTY)
GraphicOption.ini 294 B   editor graphics flags
ErrorListe.ini    323 B   error strings
```

## Dino.ini

`[DINO]` UseIntroVideo 1, Language Eng, GameSpeed 50. `[GRAPHIC]`
Resolution 800x600x16, TextureDepth 16, TextureQuality AUTO, LODQuality
AUTO, HardwareTNL 1. `[SOUND]` UseSound 1, HardwareSound 0, FX/Music 97.

## Start.Ini

`mgWindow 0,168,800,600`, `mgFullScreen 1`, `mgFullScreenSize 800,600`,
`mgSet3D -1,"HAL","SW",16`, `miKeyLoad data\config\Key\KeyFrancais.ini`,
`moWindow 0,168,800,600`.

## camera.ini

`[Config1..5]` identical: MoveSpeed 1..20, ProgressSpeed 0.05..0.2,
RotationSpeed pi/2..pi/2, Elevation 3..500, Distance 10..300,
TargetHardness 6..2, PositionHardness 6..2.

## KeyMapper.ini

17 rows, commands camera rotate/key moves, rotate feature (pad +/-), escape,
show rasters (r), pause (p). Events KeyRepeat/KeyDown; modifiers +Shift.

## KeyFrancais.ini

144 tab rows: DIK value, MIK, DIK, name, pad, ctrl, ascii flag, ascii,
shift, altgr. AZERTY: DIK_Q -> `a`, DIK_W -> `z`, DIK_A -> `q`. Values
0x01..0xED.

## Tooling

- `tools/formats/config.py` ? all readers + bundle.
- `tests/test_config.py` ? bindings, key table, cameras, startup, Dino.ini.
- `docs/formats/config.md` ? spec.

## Start framing (cross-reference)

The park uses `[Config1]` + `progress 0.1`: distance 39.0, elevation 52.7,
eye = centre + (39,0,52.7), yaw 0, FOV 80. Confirmed by
`run/save/slot001.sav` and `notes/ui-recon.md` ("Start camera solved").

## Open

- Config block -> camera mode mapping (park = Config1).
- Whether `progress` is saved per camera or fixed at 0.1 for a new game.
- Hardness min>max meaning.
