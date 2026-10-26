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

## UI click sounds

`MenuSoundDB.ini` is parsed at startup (5 entries). Menu button clicks play
`clicDefault` (`clic#3.wav`), `QUIT_BUTTON` plays `exitClic` (`clic#9.wav`),
and park HUD button actions also play `clicDefault`. Each click opens a short
SDL3 device stream for the WAV; a new click cuts the previous one. Audio is
best-effort: a missing device logs a warning and disables the player.

Options:

```text
--ui-sounds        enable click sounds in screenshot/test runs
--no-sounds        disable UI click sounds
--list-sounds      print the MenuSoundDB entries and exit
--play-sound NAME  play one entry at startup (smoke test)
```

`--list-sounds` output:

```text
sound clicDefault: data/Sound/Interface/clic#3.wav
sound clic: data/Sound/Interface/clic#2.wav
sound exitClic: data/Sound/Interface/clic#9.wav
sound music: data/Sound/Musics/musLabo.wav
sound mixer: data/Sound/Interface/mixer.wav
```

Evidence: `tests/test_viewer_cpp.py::test_cpp_menu_sounds`.

## Island music

`ParkSoundDB.ini` is parsed at startup (224 entries). `load_island` looks up
the `.lnd` `music` name (`musEquateur`, `musAtoll`, `musTempere`,
`musAfricaine`, `musLabo`) and streams that 26–28 MB WAV in 64 KB chunks,
looping when the queue drains below one second. The player keeps one theme in
memory at a time.

Options:

```text
--music            play the island theme (interactive default)
--no-music         disable it
--music-volume N   gain 0..100 (default 97)
```

The music starts on the initial `--lnd` load and again after a menu-started
island swap. The menu plays the `MenuSoundDB` `music` entry (the same
`musLabo.wav`) until Start; entering a lab centre switches to `musLabo`, and
leaving the lab restores the island theme. `M` mutes both music and clicks.
Evidence: `tests/test_viewer_cpp.py::test_cpp_park_music`,
`test_cpp_menu_and_island_music_switch`.

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
