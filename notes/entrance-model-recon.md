# Entrance model: the static LOD1 `.mesh`, not the `.skin` bind mesh

Status: **solved and ported.** The isl01 Entrance boat was still slightly off
after the terrain-orientation fix because the port rendered the `.skin` full
mesh instead of the static LOD1 `.mesh`.

## Retail reference

The retail screenshot `notes/entrance-retail-reference.png` was captured by
loading a copy of `slot001.sav` whose camera block was patched to:

```text
eye    (525.5, 300.0, 70.0)
target (525.5, 387.5, 18.0)   # footprint centre / terrain height
```

It shows the boat **in the water at the end of the pier**, with the pier
reaching the beach. The camera target is at the beach end of the pier, not
at the boat.

## Evidence

`entrance_lod1.skin` and `entrance_lod1.mesh` contain the same 296-vertex
pier+boat geometry, but in different spaces:

| Asset | Units | Bounding box | Centre |
| --- | --- | --- | --- |
| `entrance_lod1.skin` full mesh | centimetres | `(-512.7,-2230.4,-717.9)..(372.7,824.5,658.0)` | `(-0.7,-7.0,-0.3) m` |
| `entrance_lod1.mesh` | metres | `(-32.9,-16.4,-3.2)..(-24.0,14.1,10.8)` | `(-28.4,-1.2,3.8) m` |

The `.mesh` is the same geometry shifted ~28 m in X into the placement-pivot
space. The engine's `gpkBuilding` (`FUN_005fecf0`) places the model at the
footprint centre `(525.5,387.5)` and the terrain height; the pier then runs
from the pivot to the beach and the boat sits at the water end. The `.skin`
full mesh is the centimetre bind mesh centred on its own origin, so placing it
at the pivot puts the boat on the beach and drops the pier.

The `.bbi` order is: normal LOD1 `.skin`, crash, destroy, then either the
static normal LOD1 `.mesh` (Entrance, seats, doors) or the construction model.
The port now selects index 3 when it is a normal `.mesh`, and falls back to
the animated `.skin` for buildings whose index 3 is the construction model
(RestCirc, Farm, Manege, SouvCirc).

## Verification

- `notes/entrance-model-default.png`: port, default model, matching the
  retail reference layout (boat west of the pivot, pier to the beach).
- `notes/entrance-retail-reference.png`: retail, same camera.
- `test_cpp_building_placement_transform` asserts `entrance_lod1.mesh`.
- The `--building-model skin|mesh` override remains for inspection.
