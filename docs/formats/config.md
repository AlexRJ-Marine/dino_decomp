# `Data/Config` ? startup, input, camera and user settings

Status: **solved structurally** for the text files; the camera blocks are
limits, not the initial framing.

## Inventory

| File | Bytes | Role |
| --- | ---: | --- |
| `Dino.ini` | 230 | user settings (video/sound/language/speed) |
| `Start.Ini` | 288 | engine startup (window, 3D device, key table) |
| `camera.ini` | 1,885 | five camera motion configs |
| `KeyMapper.ini` | 851 | game command -> key bindings |
| `key/KeyFrancais.ini` | 7,698 | DIK/MIK key table (French AZERTY) |
| `GraphicOption.ini` | 294 | editor graphics flags |
| `ErrorListe.ini` | 323 | display error strings (`NL` = newline) |

## `Dino.ini`

```ini
[DINO]    UseIntroVideo=1  Language=Eng  GameSpeed=50
[GRAPHIC] Resolution=800x600x16  TextureDepth=16  TextureQuality=AUTO
          LODQuality=AUTO  HardwareTNL=1
[SOUND]   UseSound=1  HardwareSound=0  FXVolume=97  MusicVolume=97
```

`GameSpeed=50` is the user setting; the park clock uses its own
`time scale` (60) from `spScheduleManager`.

## `Start.Ini`

Tab/space-separated `key value` lines (no sections):

```text
mcDataLibPath   ..\..	utorials
mgWindowName    mcWorld
mgWindow        0,168,800,600        // obsolete
mgFullScreen    1
mgFullScreenSize 800,600
mgSet3D         -1,"HAL","SW",16
mgDebugDraw     0
mgBkColor       0,0,0
miKeyLoad       data\config\Key\KeyFrancais.ini
moWindow        0,168,800,600
```

`mgSet3D -1,"HAL","SW",16` requests a 16-bit HAL device with a software
fallback. The key table is selected by `miKeyLoad`.

## `camera.ini`

Five identical `[Config1..5]` blocks (one per camera mode):

| Key | Value | Meaning |
| --- | ---: | --- |
| `MoveSpeedMin/Max` | 1 / 20 | pan speed |
| `ProgressSpeedMin/Max` | 0.05 / 0.2 | dolly speed |
| `RotationSpeedMin/Max` | 1.570796 / 1.570796 | radians per second (?/2) |
| `ElevationMin/Max` | 3 / 500 | camera height range |
| `DistanceMin/Max` | 10 / 300 | orbit distance range |
| `TargetHardnessMin/Max` | 6 / 2 | target smoothing (reversed: 6 -> 2) |
| `PositionHardnessMin/Max` | 6 / 2 | position smoothing |

The blocks are byte-identical in the retail file. A new Free Game uses
`[Config1]` and `progress = 0.1` to derive the start camera:

```text
distance  = lerp(DistanceMin 10,  DistanceMax 300, 0.1) = 39.0
elevation = lerp(ElevationMin 3,  ElevationMax 500, 0.1) = 52.7
eye       = island centre + (39, 0, 52.7)
```

Confirmed by the saved camera `(551, 512, 84.7) -> (512, 512, 32)`; see
`notes/ui-recon.md` ("Start camera solved"). The per-frame park FOV is 80?.

## `KeyMapper.ini`

17 rows of `command event key [modifier]`:

| Command | Event | Keys |
| --- | --- | --- |
| `CameraRotateLeft` | `KeyRepeat` | `Fl?cheGauche +Shift`, `Suppr` |
| `CameraRotateRight` | `KeyRepeat` | `Fl?cheDroit +Shift`, `PageDown` |
| `CameraRotateUp` | `KeyRepeat` | `Fl?cheHaut +Shift`, `Home` |
| `CameraRotateDown` | `KeyRepeat` | `Fl?cheBas +Shift`, `Fin` |
| `CameraKeyUp/Down` | `KeyRepeat` | `Fl?cheHaut` / `Fl?cheBas` |
| `CameraKeyLeft/Right` | `KeyRepeat` | `Fl?cheGauche` / `Fl?cheDroit` |
| `RotateFeatureLeft/Right` | `KeyDown` | `Pad+` / `Pad-` |
| `Escape` | `KeyDown` | `Esc` |
| `ShowRasters` | `KeyDown` | `r` |
| `PauseGame` | `KeyDown` | `p` |

Key names are French (`Fl?che` = arrow, `Suppr` = Delete, `Fin` = End) and
resolve through the key table.

## `key/KeyFrancais.ini`

144 rows, tab-separated, header:

```text
//valeur DIK  code MIK  code DIK  Nom  Pad  Ctrl?  Ascii?  Ascii  Ascii(Shift)  Ascii(AltGraph)
0x10  MIK_Q  DIK_Q  a  P  0  1  a  A
```

- Columns: DIK value, MIK name, DIK name, display name, pad flag, ctrl flag,
  ascii flag, plain/shift/AltGr characters.
- The layout is **AZERTY**: `DIK_Q` is named `a`, `DIK_W` is `z`, etc.
- DirectInput scancodes cover 0x01..0xED (keyboard + media keys).

## `GraphicOption.ini` / `ErrorListe.ini`

`GraphicOption.ini` is an editor/debug set: fill/cull/Gouraud, texture Y,
skybox `none`, fog Y from 10 to 50, immediate draw, z-buffer, alpha blend and
test. `ErrorListe.ini` holds display error strings with `NL` as the newline
marker (unsupported mode, no hardware TNL, missing CD).

## Reader

`tools/formats/config.py`:

```text
python tools/formats/config.py <Config-dir>
python tools/formats/config.py camera.ini
python tools/formats/config.py KeyMapper.ini
```

`read_config_bundle` returns bindings, key rows, cameras, `Start.Ini` and
`Dino.ini` values. `read_keytable` pads the variable-length rows to 10
columns. Tests: `tests/test_config.py` (17 bindings, 144 keys, AZERTY Q?a,
five camera configs, startup and user settings).

## Evidence

- `Data/Config/*`, `Data/Config/key/KeyFrancais.ini` (read-only).
- `tools/formats/config.py`, `tests/test_config.py`.

## Open

- Which `[ConfigN]` belongs to which camera mode (the park uses Config1).
- Whether `progress` is a saved camera field or fixed at 0.1 for a new game.
- `TargetHardness`/`PositionHardness` semantics (reversed min/max).
