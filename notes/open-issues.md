# Open issues — status and hand-off (2026-10-26)

Everything else in the tree is documented and tested. These are the last
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

## 2. `.lnd` `width` / `height`

- **Solved (dead data):** the only full `.lnd` reader is `FUN_0051dad0`
  (island load, called from the Free Game start `0x403e10`).  It reads the
  eight resource strings, then the float, width, height and the 0/1/2 value,
  then the music name and the 25 vegetation strings.  The four header values
  are read into stack scratch and **never read again**: the destination slots
  are overwritten/abandoned before the music string (capstone scan: no later
  access to the float/width/height slots).  The second `.lnd` reader
  `FUN_00506950` only reads the name-key string.
- **Ruled out earlier:** heightmap bbox (`>0`/`>1` and every threshold
  0..199), the occupation-mask bbox, central-mass bboxes, the basemap size
  (1024^2), a fixed 2-unit cell grid, and a preview-image size.
- **Conclusion:** `width`, `height`, the float and the 0/1/2 value are
  editor/tool metadata.  The retail runtime never consumes them, so the
  values need no further decoding.
- **Correction:** the old lead about the terrain builder `0x5fb8b0` reading
  dword `+0x24/+0x25/+0x26` was wrong.  Those are detail-map parameters set
  by `FUN_005f3f10` to `20000`, `1.0` and `0.05`; they are not the `.lnd`
  header trio.  Also note the `.lnd` float is **not** a proven runtime water
  level ? the reader discards it.  The port's water plane uses it as a
  convention; WORLD-3 should confirm the retail water height separately.

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
  0xb4-byte runtime record, and the `SWITCH_*` keys are **dead data** (no
  lookup table anywhere, no prefix in the exe).
- **Solved:** `0x0c` (10/15/30) is the construction cost. The build path at
  `0x52b2dd` passes the wall wrapper's `+0x1c` (the embedded wall record's
  `+0x0c`) to `FUN_00585d20`, the CONSTRUCTION budget setter.
- **Inferred:** `0x08` (100) and `0x10` (30) are uniform across all walls and
  fit hit points/build time; no second consumer was found.
- **Remaining:** only the seven texture-scale floats' exact roles.
  `scale[5] == height/8` is the vertical tiling; the other six are constant
  defaults across all 24 walls. The `gatWall.cpp` geometry builder
  `FUN_005ee970` does not read them; the material/UV setup is the likely
  consumer.

## 5. Dinosaur behavior-graph node types

- **Solved:** `apDinosaurBehavior` (`0x521b30`), two sub-behaviors
  (`0x56cb80`/`0x56ca80`), node-graph reader `0x56ce80` / writer `0x56d5a0`,
  41 node types, node record shape.
- **Open:** the semantics of the 41 branches.
- **Note:** the parallel agent is already in this area (`.bai` consumers
  `FUN_0056e820`/`0x56cb30`/`0x56cc20`); coordinate before diving in.
- **Next:** decode each `switch` branch of `0x56ce80`; the BAD FOOD/REPOS →
  action 4/2 pair is one of them.
