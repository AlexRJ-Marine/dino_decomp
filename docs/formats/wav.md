# `.wav` format

Status: **solved** — 231/231 retail files parse; values match Python's `wave`
module.

## Inventory

| Kind | Count |
| --- | ---: |
| PCM, mono, 22,050 Hz, 16-bit | 225 |
| PCM, stereo, 22,050 Hz, 16-bit | 4 |
| PCM, mono, 44,100 Hz, 16-bit | 2 |
| Total | 231 |
| Frames | 38,715,314 |
| Duration | 1,755.246 s |

All files are standard RIFF/WAVE with `fmt ` (16 bytes) and `data` chunks.
No compressed or float formats are present.

## Layout

```text
"RIFF" u32 size "WAVE"
chunks:
  "fmt " u32 size
      u16 format_tag     (1 = PCM)
      u16 channels       (1 or 2)
      u32 sample_rate
      u32 byte_rate
      u16 block_align
      u16 bits_per_sample (16)
  "data" u32 size
      PCM bytes
```

Chunks are word-aligned: odd-sized chunks are followed by one pad byte.
`frames = data_size / block_align`; `duration = frames / sample_rate`.

## Validation

`tools/formats/wav.py`:

- `read_wav(path, load_data=False)` reads headers only (fast scan).
- `read_wav(path)` loads `data` and, for 16-bit PCM, `samples` as an
  `array('h')`.

Tests compare against Python's `wave` module:

| File | Format | Frames | Duration |
| --- | --- | ---: | ---: |
| `Data/Sound/Actors/Dinos/avoidance.wav` | mono, 44,100, 16-bit | 4,410 | 0.100 s |
| `Data/Sound/Musics/musAfricaine.wav` | stereo, 22,050, 16-bit | 6,640,988 | 301.179 s |

Full scan: 231 files, 0 errors, 38,715,314 frames
(`tests/test_wav.py`).

## Viewer

`tools/viewer/asset_viewer.py --wav <file.wav>` prints the format and duration;
`--play` uses `winsound` on Windows.
