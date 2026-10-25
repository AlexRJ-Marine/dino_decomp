# Sound, music and movie layer

Status: **decompiled** for the sound database, music selection and the
DirectShow movie pipeline. Renderer internals, subtitles and 3D attenuation
remain partial.

## Sound

The exe imports `WINMM` only for timers. DirectSound is loaded dynamically by
`0x005ca960`:

```text
dsound.dll  -> DirectSoundCreate8
winmm.dll   -> mmioOpenA, mmioAscend, mmioDescend, mmioRead, mmioWrite,
               mmioSeek, mmioSetInfo, mmioGetInfo, mmioAdvance, mmioClose
```

| Address | Purpose |
| --- | --- |
| `0x005b6f30` | `msSoundEngine` constructor |
| `0x005b70a0` | load a sound DB; allocates `0x005b71f0`, parses `0x005b7a90` |
| `0x005b7d00` | `msSoundDatabase` lookup/instantiate by section name |
| `0x005b8240` / `0x005b82a0` | lookup/validate a live sound handle |
| `0x005b8e00` / `0x005b8fc0` / `0x005b8fe0` | volume / frequency / pan |
| `0x005c2d70` | DirectSound buffer play/stop wrapper |
| `0x005c3130` / `0x005c3260` | buffer volume/frequency setters |

`0x004fdb00` constructs `sdpAudioPark`, loads
`data\Sound\ParkSoundDB.ini`, creates the engine, music manager and sound DB,
sets volumes 100 / 80, and defaults the music name to `musLabo`.
`0x004fd910` plays the `"music"` entry through the audio-park lookup
(`0x005213e0` / `0x005b7d00`) and starts it with `0x005b8240` /
`0x005b8e00` / `0x005b82a0`.

Sound DB format: `docs/formats/sounddb.md`.

## Music

Music is a normal sound-DB entry with `USE_STREAMING=1`, `LOOP=1`,
`CATEGORY=1`. The island `.lnd` stores the section name (for example
`musEquateur`); see `docs/formats/lnd.md` for the island table. The five park
themes are PCM 22,050 Hz stereo WAVs of about five minutes; `musLabo.wav` is
the 18.9-second menu/lab loop.

## Movies

The movie layer is `mcLib` `mvVideo` / `mvMovie` / `mvSubtitle` and uses
**DirectShow**, not a custom MPEG decoder.

| Address | Purpose |
| --- | --- |
| `0x005c0210` / `0x005c0260` | `mvVideo` constructor/destructor |
| `0x005c7c50` / `0x005c7cd0` | `mvMovie` COM init/shutdown |
| `0x005c7ed0` | build graph: `CoCreateInstance`, `RenderFile`, choose renderer |
| `0x005c80b0` | renderer type 1 (windowed) |
| `0x005c8ba0` | allocate renderer type 2 (`0x1e0`) or 3 (`0x1b0`) |
| `0x005c8dc0` / `0x005c8ea0` / `0x005c8f10` | open / play / play-on-window |
| `0x005c9010` / `0x005c90f0` | subtitle destructors |

The menu wrapper is `0x00517a40`:

```text
Create mvVideo
mvVideo::Open(data/video/<name>, renderer=3)
mvVideo::Play()
```

`0x00517ab0` polls the playing flag (`0x005c0350`).

Startup (`0x00401d40`, `0x00402600`):

- `UseIntroVideo=1` -> `data/video/intro.mpg`; otherwise menu music;
- `data/video/generic.mpg` is the menu/campaign background;
- campaign endings use `EndingVideo` / `EndingVideoName`.

## Open

- DirectShow renderer type 2/3 internals and the D3D texture upload path.
- `.sbt`/subtitle file format (`mvSubtitle.cpp`).
- 3D attenuation curves for `USE_3D=1` (`3D_MIN_DISTANCE`/`MAX`).
- Menu music transitions and the music-manager queue (`0x00503030`).

## Evidence

`notes/media-recon.md` — function-by-function evidence and retail inventory.
`docs/formats/sounddb.md` — INI key table and music map.
`tools/formats/sounddb.py`, `tests/test_sounddb.py`.
