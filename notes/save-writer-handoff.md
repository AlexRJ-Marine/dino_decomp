# Save-writer handoff (SAVE-2, world block)

State: `6f2d686` — the C++ writer reproduces the park block, the whole
simulation section, and the whole-file assembly (header + world raw + catalog +
simulation) on `slot001.sav`, `slot002.sav`, `auto/slot001.sav`. The Python
golden reference is `tools/formats/sav_write.py --whole` (byte-identical).

The only remaining code slice is `write_sav_world` (replace `world_raw`):

1. `src/formats/wld.h` — extend `WldWorld` with the records `wld.py` reads:
   fences/shows (object_id, first +0x14, start/end, waypoints, class_tag,
   second +0x14 for fences, `extra16`, +0x44, +0x48), animals (object_id,
   +0x1c, +0x20 when version > 0x32), employees (2 u32), buildings
   (object_id, linked_id, class_tag, kind, cell_x, cell_y, state, link),
   visitors (+0x1c/class/20/24/28), dinosaurs (object_id, +0x1c, +0x24/+0x28/
   +0x2c when version >= 0x25, class_tag, 18 pairs), campaign/mission ids,
   objectives (version > 9), and the four flag byte arrays.
2. `read_sav_world` — fill them (version 53 covers the retail saves; keep the
   version guards identical to `wld.py`).
3. `write_sav_world` — mirror `write_world` in `sav_write.py`; splice into
   `write_sav_whole` instead of `world_raw`; change the CLI string to
   `whole file identical`; extend `test_cpp_sav_park_roundtrip`.

After that, save work is only retail acceptance (blocked on the
`FUN_0051d1d0` loading-loop stall or one approved `-PhysicalInput` run).
