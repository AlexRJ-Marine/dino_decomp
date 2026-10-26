# Open issues — status and hand-off (2026-10-26)

This tracker covers **format/asset unknowns only**. Gameplay/simulation
status lives in `notes/STATE.md`, `TASKS.md` and `docs/decomp/`; a field can be
fully decoded here while its game logic is still unported.

Everything else in the format tree is documented and tested. These are the last
unknowns, with what was tried and the concrete next step.

## 1. `Actions.bad` first 450 floats per block

- **Solved:** 9,244 B = `u32 5` + 5 × 462 floats; the 5 blocks match the five
  animation families; the last 12 floats per block are playback scales
  (0.8–2.0, same family as `.bai` `field_18`).
- **Resolved (runtime):** the retail game never opens `Actions.bad`. A full
  x32dbg `CreateFileA/W` trace from startup through Free Game start and a
  dinosaur-bearing `slot001.sav` load shows `.bai`/`.bwi`/`.bfi`/`.anim`/
  `.mesh` opens but no `.bad`. The park-init `+0x38` path is stored but never
  consumed; the file is editor/demo data. Its internal floats remain
  undocumented but are not a live gameplay table.

## 2. `.lnd` `width` / `height` / float / 0/1/2

- **Correction (runtime): no longer dead data.** `FUN_0051dad0` reads the
  four values into stack scratch and does not use them directly, but the
  retail data and runtime prove they are consumed elsewhere:
  - `width`/`height` are the free-game **Entrance cell**.
  - the `0/1/2` value is the Entrance **orientation state**.
  - the float is the **water surface Z**.
- Runtime `FUN_00586b80` (building ctor): isl01 `(520,376) state 1`, isl02
  `(312,504) state 2`, exactly `isl01.lnd 520/376/1` and
  `isl02.lnd 312/504/2`. All seven campaign `Cpg0*_Save.wld` Entrance
  records match their island `.lnd` width/height/0-1-2.
- Runtime `FUN_00602cf0` water quad: isl01 `16.0`, isl02 `20.0`, matching the
  `.lnd` float.
- **Open:** the exact code path that carries the four values from the `.lnd`
  stack scratch to the Entrance/landscape objects. The `.lnd` reader may be a
  second copy or the values may be re-read by the scenario setup.
- **Evidence:** `notes/calibration-2026-10-26.md`,
  `notes/x32dbg-calibration-isl01b.out.log`,
  `notes/x32dbg-calibration-isl02.out.log`.

## 3. `.bfi` body integers

- **Solved:** the full body is a fixed sequential stream; all 62 files parse
  to an exact EOF with `tools/formats/bfi.py`.  The class tag does not change
  the body shape (it only sets a boolean at record `+0x34`).
- **Solved fields:** judge kind (1 Race..6 Capture), show-type list, food
  kind (`.bsi`/`.bni` ids 0..9), price (header `0x19`), waypoint team
  (header `0x1d`), display type (header `0x21`), action names, 8 mesh/texture
  slots, texture dir, 7 effect INIs, billboard dir, attribute-weight lists
  (agility/courage/combat/protection/perception/intelligence/agress),
  score steps 200..1000, 3 sound names, duration pair, switch key and tail
  flag.  Full layout in `docs/formats/bfi.md`.
- **Remaining (semantics only):** exact use of `field_3c`/`field_40`,
  `anim_flag`/`anim_speed`, `field_11`/`field_15`, the 3-value
  `field_154` block, and the scoring arithmetic for the attribute lists.
- **Evidence:** `notes/bfi-recon.md` ("Resolution"), `0x5475c0`/`0x54a630`
  record copy, `0x621770` mesh renderer.

## 4. `.bwi` field semantics

- **Solved:** full struct, all 24 files, assets, height law, electrified flag,
  0xb4-byte runtime record, the `SWITCH_*` keys are **dead data**, and
  `0x0c` is the construction cost (proven via the `0x52b2dd` build path and
  the `FUN_00585d20` budget setter).
- **Solved:** the seven texture-scale floats are `1.0, 0.5, 0.25, 0.6, 2.0,
  height/8, 0.125`; `scales[5]` is the vertical tiling (`height/8`) and the
  other six are constant material defaults. The geometry builder `0x5ee970`
  does not read them, so they belong to the texture layer and are cosmetic.
- **Inferred:** `0x08` = hit points, `0x10` = build time (uniform 100/30, no
  second consumer).
- **Evidence:** `docs/formats/bwi.md`, `notes/bwi-recon.md`.

## 5. Dinosaur behavior-graph node types

- **Resolved:** the old "41 node types" premise was a misread. `0x29` is the
  graph **serialization version**, not a type count; there is no 41-way
  type switch and no serialized type enum.
- The graph is a runtime object list: base node `0x56cc90` (0x40 bytes) with
  a kind dword at +8, scalars at +0xc/+0x10/+0x14, two int vectors
  (+0x18/+0x28) and an attached action object/vtable at +0x38/+0x3c.
- Builders `0x56e190`/`0x56e300` attach nodes to usable action objects;
  `0x56e410` scores nodes with kind byte 1 through the object's `vtable+0x14`
  and the `.bai` effect weights; `0x56e0e0` applies the `.bai` preconditions.
- Serializer `0x56ce80`/`0x56d5a0` accepts versions 0x25..0x2b (0x29
  current, 0x2b adds one header field); it is a save-state reader, not a
  behaviour-type table.
- **Evidence:** `notes/dinosaur-behavior-graph.md` ("Node record and
  evaluator"), `docs/formats/bai.md`.

