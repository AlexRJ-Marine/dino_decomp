# Ending videos (MPEG-1)

Status: **decoder and frame export ported.** MPEG-1 program streams are read
with the vendored PL_MPEG header; `--video-info` and `--video-frame` decode
real retail movies. Real-time playback, audio and the campaign `CPGEND` wiring
are still open.

## Retail files

`Data/Video/`:

| File | Used by |
| --- | --- |
| `tennis.mpg` | Cpg02 ending (`EndingVideo=tennis.mpg`) |
| `basket.mpg`, `trampoline.mpg`, `trappe.mpg`, `mixe.mpg` | the other campaign endings |
| `generic.mpg`, `intro.mpg` | generic/intro movies |

All are MPEG-1 program streams. Example probe:

```text
video: tennis.mpg width=640 height=480 duration=34.417 fps=24.000
```

## Port contract

- `third_party/pl_mpeg.h` — MIT, vendored unmodified
  (`third_party/README.md`).
- `src/formats/mpg.{h,cpp}`:
  - `probe_mpg(path)` → `MpgInfo { width, height, duration, frame_rate }`;
  - `decode_mpg_frame(path, index)` → `MpgFrame { width, height, index, time,
    rgb }`, decoded forward from the start with audio disabled.
- `src/viewer/main.cpp`:
  - `--video-info FILE` prints the probe and exits;
  - `--video-frame FILE --video-frame-index N --video-out OUT.bmp` decodes
    frame N, flips it to top-down and writes a 24-bit BMP.

The retail movies stay in `game/`; only decoded frames are written to local
output paths. `notes/video-tennis-frame.png` is frame 300 of `tennis.mpg`.

## Verification

```powershell
.\build\port\dino_viewer.exe --video-info "game\%MAINDIR%\Data\Video\tennis.mpg"
.\build\port\dino_viewer.exe --video-frame "game\%MAINDIR%\Data\Video\tennis.mpg" `
  --video-frame-index 300 --video-out notes\harness\tennis-300.bmp
```

Expect `width=640 height=480 duration=34.417 fps=24.000` and a 640×480 BMP.
`tests/test_video.py` pins both commands; frame 300 must contain more than
1,000 colours (a real scene, not a blank frame).

## Open

- Real-time playback in the viewer (texture upload + timed frame decode).
- MP2 audio (PL_MPEG has an audio decoder; not wired).
- Play the campaign ending video when `CPGEND` closes (the ending text and
  unlock bit are already wired in `campaign-flow.md`).
- Intro movie before the menu.

## Evidence

- `third_party/pl_mpeg.h`, `third_party/README.md`.
- `src/formats/mpg.{h,cpp}`, `src/viewer/main.cpp`.
- `tests/test_video.py`, `notes/video-tennis-frame.png`.
