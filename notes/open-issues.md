# Open issues — status and hand-off (2026-10-26)

Everything else in the tree is documented and tested. These are the last
unknowns, with what was tried and the concrete next step.

## 1. `Actions.bad` first 450 floats per block

- **Solved:** 9,244 B = `u32 5` + 5 × 462 floats; the 5 blocks match the five
  animation families; the last 12 floats per block are playback scales
  (0.8–2.0, same family as `.bai` `field_18`).
- **Open:** the first 450 floats of each block.
- **Tried:** size-prefixed block guesses (no match); float statistics
  (time-like first value, two large coordinates, near-zero rotation-like
  middle) — consistent with a baked path, not proven.
- **Next:** find the consumer of park init `+0x38`
  (`DATA\Design\Actions\bad`, set in `0x51d1d0`) or trace the file open under
  x32dbg, then dump the parsed object.

## 2. `.lnd` `width` / `height`

- **Values:** 520×376, 312×504, 704×448, 440×200, 480×200, 628×172, 120×492,
  548×96, 268×276, 488×420.
- **Ruled out:** heightmap bbox (`>0`/`>1` and every threshold 0..199), the
  occupation-mask bbox, the basemap size (1024²), and an island-preview image
  size (no matching TGA).
- **Next:** decompile the `.lnd` reader — the path is built at `0x51dad0`
  (`Data\Design\Landscape\isl`), and the `width`/`height` fields are read
  early in the header; find where the loaded values are stored and used
  (terrain mesh crop, pathfinding grid, or editor rectangle).

## 3. `.bfi` body integers

- **Solved:** 12-field header `0x00..0x2c`, class tag at `0x2d`, all strings
  (actions, 57 meshes, 49 textures, effect INIs, switch keys) and the
  footprint (proven by the `.shw` tiling).
- **Open:** the integer fields after the class tag (action bindings, scores,
  durations).
- **Next:** the `.bfi` loader `0x53ed30` uses the same reader pattern as the
  `.bai` loader; disassemble the calls after the class-tag read and
  reconstruct the field order (the other agent did exactly this for `.bai`).

## 4. `.bwi` field semantics and `SWITCH_*` consumer

- **Solved:** full struct, all 24 files, assets, height law, cost class,
  electrified flag, 0xb4-byte runtime record.
- **Open:** what `0x08` (100), `0x0c` (10/15/30) and `0x10` (30) mean, and
  where the two `SWITCH_WALL_*` / `SWITCH_FENCE_*` strings are consumed.
- **Tried:** the switch strings do not occur in any INI, `.loc`, or the exe;
  they are likely runtime keys or dead editor data.
- **Next:** find the wall record's consumers (construction budget setter and
  the wall renderer); if the strings are never read, mark them dead.

## 5. Dinosaur behavior-graph node types

- **Solved:** `apDinosaurBehavior` (`0x521b30`), two sub-behaviors
  (`0x56cb80`/`0x56ca80`), node-graph reader `0x56ce80` / writer `0x56d5a0`,
  41 node types, node record shape.
- **Open:** the semantics of the 41 branches.
- **Note:** the parallel agent is already in this area (`.bai` consumers
  `FUN_0056e820`/`0x56cb30`/`0x56cc20`); coordinate before diving in.
- **Next:** decode each `switch` branch of `0x56ce80`; the BAD FOOD/REPOS →
  action 4/2 pair is one of them.

## 6. Edition demo `.shw` judge value

- **Solved:** schema and all 3 files (`NoName / width / height / value /
  count / features`, 32×72).
- **Open:** the fourth value is 2 in `demo8` and 6 in the other two; it is
  not the retail judge kind (1..4).
- **Next:** locate the edition-show loader (not the retail `0x558340`
  writer/`0x558230` reader) or accept it as a legacy editor field.

## Cross-agent opens (not mine)

- `.bai` stat-grant vector: stored and cloned, no reader (parallel agent).
- `.bai` end-ramp renderer and the source of the indexed `params[i]`
  multiplier (parallel agent).
- `RadiationDamage` (`spLaboratory +0x3c`) exact consumer (mutation action).
