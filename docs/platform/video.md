# Ending videos (MPEG-1)

Status: **decoder, frame export and windowed playback ported.** MPEG-1 program
streams are read with the vendored PL_MPEG header; `--video-info`,
`--video-frame` and `--play-video` use real retail movies. The campaign
`CPGEND` close hands off to the ending video. MP2 audio and the intro movie are
still open.

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
    rgb }`, decoded forward from the start with audio disabled;
  - `MpgStream` → one `next(MpgFrame&)` per video frame for playback.
- `src/viewer/main.cpp`:
  - `--video-info FILE` prints the probe and exits;
  - `--video-frame FILE --video-frame-index N --video-out OUT.bmp` decodes
    frame N, flips it to top-down and writes a 24-bit BMP;
  - `--play-video FILE [--video-play-start N] [--video-play-frames N]` opens
    the window, draws the letterboxed video at its frame rate, and stops at
    the end/ESC or after N frames. `--screenshot` saves the last frame.
  - When the campaign `CPGEND` closes on the final mission, `advance_campaign`
    plays the campaign's `EndingVideo` (`tennis.mpg` for Cpg02) through the
    same player. Audio is not wired yet.

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

Playback:

```powershell
.\build\port\dino_viewer.exe --play-video "game\%MAINDIR%\Data\Video\tennis.mpg" `
  --video-play-start 300 --video-play-frames 1 --screenshot notes\harness\play.bmp
```

Expect `video play: ... 640x480 24.000 fps`, `video played: 1 frames`, and a
meaningful screenshot. The campaign completion path (`Cpg02_Mission5`) prints
`campaign video: ...\tennis.mpg` and `video played: ...` on `CPGEND` close.

## Open

- MP2 audio (PL_MPEG has an audio decoder; not wired).
- Play the intro movie before the menu.
- A/V sync, seeking and fullscreen scaling polish.

## Evidence

- `third_party/pl_mpeg.h`, `third_party/README.md`.
- `src/formats/mpg.{h,cpp}`, `src/viewer/main.cpp`.
- `tests/test_video.py` (probe, frame export, playback),
  `tests/test_campaign_cpp.py::test_cpp_campaign_completion_video`,
  `notes/video-tennis-frame.png`.
