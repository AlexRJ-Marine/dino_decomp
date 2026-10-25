# `MenuSoundDB.ini` / `ParkSoundDB.ini`

Status: **solved.** 2/2 retail databases parse; all 229 filename entries
resolve except one documented retail typo.

The sound database is a plain INI keyed by sound name. The engine is
`msSoundDatabase` (`0x005b7d00`) and the DirectSound wrapper is loaded
dynamically from `dsound.dll` (`0x005ca960`). The parser is
`tools/formats/sounddb.py`; evidence is in `notes/media-recon.md`.

## Inventory

| File | Sections | Streaming | Looping | Missing files |
| --- | ---: | ---: | ---: | ---: |
| `Data/Sound/MenuSoundDB.ini` | 5 | 1 | 1 | 0 |
| `Data/Sound/ParkSoundDB.ini` | 224 | 5 | 12 | 1 |

The park DB's 5 streaming/looping entries are the island music themes. The
menu DB's single streaming/looping entry is `[music]` (`musLabo.wav`).

## Layout

```text
; comment
[section]                 # sound name used by game code
FILENAME        = "data/Sound/.../file.wav"
USER_PARAM      = n
PRIORITY        = 0..100
CATEGORY        = n
USE_VOLUME      = 0/1
USE_FREQUENCY   = 0/1
USE_STEREO      = 0/1
USE_ALL_FX      = 0/1
USE_STREAMING   = 0/1
USE_3D          = 0/1
LOOP            = 0/1
KEEP_IN_MEMORY  = 0/1
LOCK_HARD       = 0/1     # documented but unused
LOCK_SOFT       = 0/1     # documented but unused
START_DELAY     = n
3D_MIN_DISTANCE = f
3D_MAX_DISTANCE = f
```

Defaults follow the file header: `PRIORITY=0`, `CATEGORY=-1`,
`USE_*=0`, `LOOP=0`, `START_DELAY=0`, `3D_MIN_DISTANCE=1.0`,
`3D_MAX_DISTANCE=1000000000.0`.

`#INCLUDE` / `[#INCLUDE]` may include another INI recursively. The retail
files do not use it; the parser supports it with cycle detection.

## Music sections

`ParkSoundDB.ini`:

| Section | File | Loop | Streaming | Category |
| --- | --- | ---: | ---: | ---: |
| `musTempere` | `data/Sound/Musics/musTempere.wav` | 1 | 1 | 1 |
| `musAtoll` | `data/Sound/Musics/musAtoll.wav` | 1 | 1 | 1 |
| `musEquateur` | `data/Sound/Musics/musEquateur.wav` | 1 | 1 | 1 |
| `musAfricaine` | `data/Sound/Musics/musAfricaine.wav` | 1 | 1 | 1 |
| `musLabo` | `data/Sound/Musics/musLabo.wav` | 1 | 1 | 1 |

`MenuSoundDB.ini`:

| Section | File | Loop | Streaming |
| --- | --- | ---: | ---: |
| `music` | `data/Sound/Musics/musLabo.wav` | 1 | 1 |

The island `.lnd` selects the park theme by name:

| Island | Music |
| --- | --- |
| isl01 | `musEquateur` |
| isl02 | `musAtoll` |
| isl03 | `musTempere` |
| isl04 | `musAfricaine` |
| isl05 | `musAtoll` |
| isl06 | `musAfricaine` |
| isl07 | `musTempere` |
| isl08 | `musEquateur` |
| isl09 | `musEquateur` |
| isl10 | `musTempere` |

All five theme WAVs are PCM 22,050 Hz stereo, about five minutes each.
`musLabo.wav` is PCM 22,050 Hz mono, 18.9 s.

## Retail data bug

`[bell]` points to `data/sound/Attractions/Attractions/bell.wav`, which is not
present. The same sound exists as `Data/Sound/Buildings/bell.wav`, used by
`[warning]`. This is the only missing filename in either database.

## Reader

```text
python tools/formats/sounddb.py --dump <file.ini>
python tools/formats/sounddb.py --scan <Data/Sound> --root <game root>
python tools/formats/sounddb.py --dump <file.ini> --missing
```

`read_sounddb(path, root)` returns a `SoundDb` with `entries`, `by_name()`,
`music`, and a `summary()` that checks every `FILENAME` against the game root.

Tests: `tests/test_sounddb.py` (both retail DBs, the `[bell]` typo, includes,
unknown keys, invalid booleans).
