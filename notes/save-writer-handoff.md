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
  `notes/sav-retail-accept.png`;
- the park clock (09:28) matches the save's `schedule_elapsed` (23.2 s), so
  the world/camera/clock came from the save.

Campaign autosaves (~53 KB, after a campaign park) populate the simulation
lists after the roads. `list0` (`saDinosaur`) is decoded and modeled in
`tools/formats/sav_campaign.py`; the C++ reader validates each record length
and keeps list0 plus the still-undecoded list1/list2 tail raw. Whole-file
round-trips are byte-identical (`--sav-roundtrip`,
`build/campaign-autosave.sav`), and `test_cpp_sav_park_roundtrip` no longer
skips the class. Remaining for full campaign parity: typed C++ list0, decode
list1/list2, and expose the campaign park/visitor state (the partial C++
`--sav-info` reports zeros for those).

HUD cash resolved: it is the same field as the park block `+0x30`
(`spPark->economy+0x08`; `FUN_00526010` reads it at `0x526041`, see
`notes/hud-budget-source-recon.md`). The `0x58b1b0` guard is not a
save-vs-autosave switch: `BL` is the park reader's third call-site argument,
`0` for every `.sav` LOAD (manual and autosave) and `1` for the campaign
`.wld` loader, whose stream has no park block. A `.sav` LOAD restores the
budget (`FUN_00585240` also self-gates on the flag), so the port restoring
`sav_simulation.budget` into `park_budget`/`buy_budget` matches retail. The
26000 seen after the earlier load is not explained by this guard; candidates
are a later scenario/economy writer or a campaign `.wld` start
(`notes/hud-budget-restore-flag-recon.md` §8).

Reproduction (the exact test save is 7,374 bytes, sha256
`0e5559a0cbd36120731396c51205192074a5f7016dcf1150b8e5119f6d8da738`):

```powershell
.\build\port\dino_viewer.exe --sav-write run\save\slot001.sav `
  build\savwrite\cppwrite-slot001.sav --sav-name cppwrite --sav-slot 1 `
  --sav-budget 12345
# install as run-test/save/slot001.sav, then
# notes\harness\load-acceptance.ps1 -WatchSeconds 40
```

The LOAD GAME panel listed `cppwrite`; selecting that row and LOAD started
the park (Reset `hr=0`) and the 40 s screenshot shows the park with clock
09:28 (the save's `schedule_elapsed` is 23.2 s). The acceptance run restores
the original `slot001.sav` in its `finally` block.

Campaign save caveat: `FUN_0058b200` skips the visitor list, park economy
(`FUN_00585780`), schedule (`FUN_00594cb0`) and the `spPark+0x148` tail on the
campaign branch. A campaign autosave therefore has **no 0x293 park block**;
after list0/list1/list2 it continues with the `spPark+0x90` list (57 records
in the sample), `+0xa0`, `+0x40`, the store/lab writer and the camera tail.
`--sav-park` cannot import a campaign save; campaign population must come from
the `CpgNN_Save.wld` world records (and eventually the campaign save's
list1/list2 state). The 57-list record size is the next decode target.
