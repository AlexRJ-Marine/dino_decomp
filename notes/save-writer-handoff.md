# Save-writer handoff (SAVE-2, world block) — COMPLETE

State: `dc8418e` — the C++ writer re-serializes the **whole file** from typed
models and is byte-identical on `run/save/slot001.sav`, `slot002.sav` and
`auto/slot001.sav`:

```text
dino_viewer --sav-roundtrip run/save/slot001.sav
sav roundtrip: whole file identical

dino_viewer --sav-write run/save/slot001.sav build/out.sav
sav written: 7376 bytes -> build/out.sav
```

What landed (the previous plan, for the record):

1. `SavWorldRecords` in `src/formats/wld.h` mirrors `tools/formats/wld.py`:
   objectives, fences, shows, animals, employees, buildings, visitors,
   dinosaurs and the four unlock flag arrays.
2. `read_sav_world` parses the block a second time into that model and
   requires it to end exactly at `world_end`.
3. `write_sav_world` mirrors `sav_write.py::write_world`; `write_sav_whole`
   splices header + world + catalog + simulation. `world_raw` no longer
   exists, so nothing is copied.
4. `--sav-write IN OUT` emits the rebuilt save; the roundtrip test also writes
   each retail save and compares bytes. The C++ output was cross-checked with
   `tools/formats/sav_write.py` (Python parses it and round-trips it
   byte-identically).

`--sav-budget N` sets the park block `+0x30` field for field tests (the HUD
cash is a different value; see below).

## Retail acceptance (2026-10-27) - PASSED

The TOOL-4 park load was fixed (`notes/background-input-recon.md` §4b); the
retail LOAD GAME path now reaches a park. A C++-written save with
`--sav-name cppwrite --sav-slot 1` was installed as
`run-test/save/slot001.sav`:

- the retail LOAD GAME list shows `cppwrite`;
- selecting it starts the park (D3D8 Reset `hr=0`) and renders it:
  `notes/harness/load-cppwrite.png`;
- the park clock (09:28) matches the save's `schedule_elapsed` (23.2 s), so
  the world/camera/clock came from the save.

Open semantic item: the HUD cash (26000) is not the park block `+0x30`
`budget` (12345 in that test save; `26000` occurs only as show 27's first
parameter). Identify the live-money field before using `--sav-budget` for
gameplay assertions.
