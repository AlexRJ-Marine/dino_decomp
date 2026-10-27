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

Remaining save item: **retail acceptance**. The harness park load still stalls
in `FUN_0051d1d0` before the world files are opened (see
`notes/background-input-recon.md` and the TOOL-4 notes). Until that loop is
fixed, the writer cannot be confirmed by loading in retail.
