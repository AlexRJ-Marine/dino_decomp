# Ending videos (MPEG-1)

Status: **decoder, frame export, windowed playback and MP2 audio ported.**
MPEG-1 program streams are read with the vendored PL_MPEG header;
`--video-info`, `--video-frame` and `--play-video` use real retail movies. The
campaign `CPGEND` close hands off to the ending video. The intro movie is still
open.

## Retail files

`Data/Video/`:

| File | Used by |
| --- | --- |
| `tennis.mpg` | Cpg02 ending (`EndingVideo=tennis.mpg`) |
| `basket.mpg`, `trampoline.mpg`, `trappe.mpg`, `mixe.mpg` | the other campaign endings |
| `generic.mpg`, `intro.mpg` | generic/intro movies |

All are MPEG-1 program streams. Example probe:

```text
video: tennis.mpg width=640 height=480 duration=34.417 fps=24.000 audio=44100Hz streams=1
```

All seven movies carry exactly one MPEG-1 Audio Layer II track at 44.1 kHz
(`plm_get_num_audio_streams` = 1, `plm_get_samplerate` = 44100).

## Port contract

- `third_party/pl_mpeg.h` — MIT, vendored unmodified
  (`third_party/README.md`).
- `src/formats/mpg.{h,cpp}`:
  - `probe_mpg(path)` → `MpgInfo { width, height, duration, frame_rate,
    audio_streams, audio_samplerate }`;
  - `decode_mpg_frame(path, index)` → `MpgFrame { width, height, index, time,
    rgb }`, decoded forward from the start with audio disabled;
  - `MpgStream` → one `next(MpgFrame&)` per video frame for playback, plus
    `has_audio()`, `audio_samplerate()` and `take_audio()` (interleaved
    stereo float decoded since the previous call).
- `src/viewer/main.cpp`:
  - `--video-info FILE` prints the probe and exits;
  - `--video-frame FILE --video-frame-index N --video-out OUT.bmp` decodes
    frame N, flips it to top-down and writes a 24-bit BMP;
  - `--play-video FILE [--video-play-start N] [--video-play-frames N]` opens
    the window, draws the letterboxed video, and stops at the end/ESC/click or
    after N frames. `--screenshot` saves the last frame. Frames are paced on
    their PTS (`frame.time - first_time`) rather than a fixed delay, so the
    audio queue and video stay aligned.
  - **Audio:** `play_video` opens an `SDL_AUDIO_F32` 2-channel stream at
    `video.audio_samplerate()`, queues `take_audio()` after each `next()`,
    and applies the SFX volume (`--mute`/`opt.sfx_volume`). The menu/island
    music stream is paused for the movie and resumed afterwards, so the
    retail soundtrack is not doubled by the menu theme. The stream is
    destroyed on skip/end.
  - When the campaign `CPGEND` closes on the final mission, `advance_campaign`
    plays the campaign's `EndingVideo` (`tennis.mpg` for Cpg02) through the
    same player, with its soundtrack.

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

Expect `video play: ... 640x480 24.000 fps`, `video audio: 44100 Hz stereo`
(when an audio device opens), `video played: 1 frames`, and a meaningful
screenshot. The campaign completion path (`Cpg02_Mission5`) prints
`campaign video: ...\tennis.mpg` and `video played: ...` on `CPGEND` close.

## Open

- Play the intro movie before the menu.
- A long-run A/V drift check (the PTS pacing + SDL queue are in place; a
  20-minute soak is not done). Seeking and fullscreen scaling polish.
- Does retail attenuate the movie with the SFX slider or the music slider?
  The port uses the SFX slider; confirm with an options A/B.

## Evidence

- `third_party/pl_mpeg.h`, `third_party/README.md`.
- `src/formats/mpg.{h,cpp}`, `src/viewer/main.cpp`.
- `tests/test_video.py` (probe, frame export, playback),
  `tests/test_campaign_cpp.py::test_cpp_campaign_completion_video`,
  `tests/test_viewer_cpp.py::test_cpp_mpg_cinematic_audio_probe`,
  `test_cpp_retail_menu_cinematics` (audio stream assertion),
  `notes/video-tennis-frame.png`.
