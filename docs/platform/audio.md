# Audio layer

Status: **AUDIO-1 complete** — SDL3 audio playback from the retail WAV files.

## Format

`src/formats/wav.cpp` parses RIFF/WAVE PCM:

- 8- or 16-bit, mono or stereo.
- `fmt ` and `data` chunks, word-aligned.
- `frames = data_size / block_align`, `duration = frames / sample_rate`.

Retail set: 231 files, all PCM 16-bit (225 mono 22,050 Hz, 4 stereo 22,050 Hz,
2 mono 44,100 Hz). The Python reader (`tools/formats/wav.py`) was validated
against Python's `wave` module; the C++ reader uses the same rules.

## Playback

`dino_viewer` opens an SDL3 audio device stream with the WAV's channel count
and sample rate, queues the PCM data, and loops by appending when the queued
buffer drops below half the clip. Audio is optional: if no device is
available, the viewer prints a warning and continues.

```powershell
.\build\mingw\dino_viewer.exe `
  --wav "game/%MAINDIR%/Data/Sound/Actors/Dinos/avoidance.wav"
```

Options:

```text
--wav FILE    play a retail WAV
--once        play once (default loops)
--mute        start muted
```

`M` toggles mute (`dino_keys.cfg` action `mute`). The key map now includes:

```text
mute=M
```

## Evidence

```text
wav ...avoidance.wav: 1 ch, 44100 Hz, 16-bit, 4410 frames, 0.100s
audio: ...avoidance.wav, 44100 Hz, 1 ch, 0.100s (loop)
```

Files:

- `notes/audio-smoke.txt` — validate + playback output.
- `tests/test_viewer_cpp.py`:
  - `test_cpp_wav_validate`
  - `test_cpp_wav_window` (muted 2-frame window smoke test)

## Next

- Music/voice channel mixing and the original `Sound/*.ini` metadata.
- Use SDL3 audio for the game's sound events once the simulation layer emits
  them (Phase 3+).
