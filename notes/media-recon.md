# Media layer recon: sound, music and movies

Date: 2026-10-25
Scope: static decompilation plus retail-file inventory. Read-only; no game
writes.

Status: **sound database, music selection and the DirectShow movie pipeline
decoded.** DirectShow renderer internals and 3D attenuation remain partial.

## 1. Inventory

| Group | Files | Notes |
| --- | ---: | --- |
| WAV | 231 | 225 mono / 4 stereo 22,050 Hz + 2 mono 44,100 Hz (FMT-6) |
| Music WAV | 5 | `Data/Sound/Musics/*.wav`, all PCM 22,050 Hz |
| MPEG | 7 | `Data/Video/*.mpg` (intro, generic, basket, mixe, tennis, trampoline, trappe) |
| Sound DB | 2 | `Data/Sound/MenuSoundDB.ini` (5), `ParkSoundDB.ini` (224) |

Music durations:

| File | Channels | Rate | Duration |
| --- | ---: | ---: | ---: |
| `musLabo.wav` | 1 | 22,050 | 18.9 s |
| `musTempere.wav` | 2 | 22,050 | ~5 min |
| `musAtoll.wav` | 2 | 22,050 | ~5 min |
| `musEquateur.wav` | 2 | 22,050 | ~5 min |
| `musAfricaine.wav` | 2 | 22,050 | ~5 min |

## 2. Sound engine

The exe does **not** import `DSOUND.dll`. `FUN_005ca960` loads it at runtime:

```text
LoadLibraryA("dsound.dll") -> GetProcAddress("DirectSoundCreate8")
LoadLibraryA("winmm.dll")  -> mmioOpenA/Ascend/Descend/Read/Write/Seek/
                              SetInfo/GetInfo/Advance/Close
```

The engine classes are in `msSoundEngine.cpp` / `msSoundDatabase.cpp`:

| Address | Purpose |
| --- | --- |
| `0x005b6f30` | `msSoundEngine` constructor (DirectSound object from `DAT_006749ac`) |
| `0x005b70a0` | load a sound DB (`FUN_005b71f0` allocates, `0x005b7a90` parses) |
| `0x005b7d00` | `msSoundDatabase` lookup/instantiate by name |
| `0x005b8240` / `0x005b82a0` | lookup/validate a live sound handle |
| `0x005b8e00` / `0x005b8fc0` / `0x005b8fe0` | volume/frequency/pan setters |
| `0x005c2d70` / `0x005c3130` / `0x005c3260` | DirectSound buffer wrappers |

The sound DB header documents every key (`FILENAME`, `PRIORITY`, `CATEGORY`,
`USE_VOLUME`, `USE_FREQUENCY`, `USE_STEREO`, `USE_ALL_FX`, `USE_STREAMING`,
`USE_3D`, `LOOP`, `KEEP_IN_MEMORY`, `LOCK_HARD`, `LOCK_SOFT`, `START_DELAY`,
`3D_MIN_DISTANCE`, `3D_MAX_DISTANCE`). See `docs/formats/sounddb.md`.

## 3. Park audio and music

`FUN_004fdb00` is the `sdpAudioPark` constructor:

- loads `data\Sound\ParkSoundDB.ini` via `0x005b70a0`;
- creates `msSoundEngine` (`0x005b6f30`), a music manager (`0x00503030`) and
  a sound DB object (`0x00500be0`);
- sets volumes: `+0x1a = 100`, `+0x69 = 0x50`;
- sets the default music name to `musLabo`.

`FUN_004fd910` is the music playback helper. It builds the string `"music"`,
calls the audio-park lookup (`0x005213e0` / `0x005b7d00`), stores the handle,
and calls `0x005b8240`, `0x005b8e00`, `0x005b82a0` (play + volume). The
`"music"` section comes from `MenuSoundDB.ini`; the park themes are separate
sections in `ParkSoundDB.ini`.

Music selection is data-driven from the island `.lnd`:

| Island | `.lnd` music |
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

All five `mus*` entries in `ParkSoundDB.ini` have `USE_STREAMING=1`,
`LOOP=1`, `PRIORITY=4`, `CATEGORY=1`. The menu `music` entry uses
`musLabo.wav` with the same streaming/loop flags.

## 4. Movie pipeline

The movie code is `mcLib` `mvVideo.cpp` / `mvMovie.cpp` / `mvSubtitle.cpp`.
It is a **DirectShow** wrapper, not a custom MPEG decoder.

| Address | Purpose |
| --- | --- |
| `0x005c0210` | `mvVideo` constructor; allocates a 0x44-byte `mvMovie` |
| `0x005c0260` | `mvVideo` destructor |
| `0x005c7c50` | `mvMovie` constructor; `CoInitialize` |
| `0x005c7cd0` | `mvMovie` destructor; `SetWindowLongA`, `CoUninitialize` |
| `0x005c7ed0` | build the DirectShow graph: `CoCreateInstance` (CLSID `0x006476e0`, IID `0x006475f0`), `RenderFile`, choose renderer |
| `0x005c80b0` | renderer type 1 (windowed) |
| `0x005c8ba0` | allocate renderer type 2 (`0x1e0`) or type 3 (`0x1b0`) |
| `0x005c8dc0` | set movie filename |
| `0x005c8ea0` | play |
| `0x005c8f10` | play on a window (`hwnd`, flags `0x401`) |
| `0x005c9010` / `0x005c90f0` | subtitle destructor |

The menu wrapper is `0x00517a40` (`mvwMenuInterface.cpp`):

```text
FUN_00517e60(1)
Create mvVideo (0x005c0210)
mvVideo::Open(path, renderer=3)   # 0x005c02c0
mvVideo::Play()                    # 0x005c0330
```

`0x00517ab0` polls `mvVideo::IsPlaying` (`0x005c0350`).

Startup paths (`0x00401d40` / `0x00402600`):

- `Dino.ini [DINO] UseIntroVideo=1` → `data/video/intro.mpg` via
  `0x00517a40`; otherwise play the menu music (`0x004fd910`);
- `data/video/generic.mpg` is played by the menu/campaign path at
  `0x00402685`;
- campaign endings use `CampaignBase.ini` `EndingVideo` /
  `EndingVideoName` (see `docs/formats/campaign.md`).

Renderer type 3 is the in-game D3D renderer used by the menu; types 1 and 2
are the fallback/windowed paths. The exact renderer internal layout and the
subtitle file format are still open.

## 5. Retail data bug

`ParkSoundDB.ini` `[bell]` points to
`data/sound/Attractions/Attractions/bell.wav`, which does not exist. The file
is present as `Data/Sound/Buildings/bell.wav` and is used by `[warning]`.
This is the only missing filename in either DB.

## 6. Evidence

- `FUN_005ca960` — dynamic `dsound.dll` / `winmm.dll` loader.
- `FUN_005b6f30`, `0x005b70a0`, `0x005b7d00` — sound engine/database.
- `FUN_004fdb00`, `0x004fd910` — `sdpAudioPark` and music playback.
- `FUN_005c0210`..`0x005c90f0` — `mvVideo` / `mvMovie` / `mvSubtitle`.
- `FUN_00517a40`, `0x00517ab0` — menu movie wrapper.
- `0x00401d40`, `0x00402600` — startup intro/generic video paths.
- Files: `ParkSoundDB.ini`, `MenuSoundDB.ini`, `Data/Video/*.mpg`,
  `Data/Sound/Musics/*.wav`, island `.lnd` files.

Parser: `tools/formats/sounddb.py`; tests: `tests/test_sounddb.py`.
