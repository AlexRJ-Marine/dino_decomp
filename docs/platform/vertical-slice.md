# Vertical-slice scene (island + dinosaur)

Status: **working** — isl01 terrain, 1,536 placed trees, and an animated
allosaurus standing on the terrain in one C++/SDL3/GL scene.

## Command

```powershell
$G = "game/%MAINDIR%"
.\build\mingw\dino_viewer.exe `
  --lnd "$G/Data/Design/landscape/isl01/isl01.lnd" --asset-root $G `
  --mesh "$G/Data/Graphics/Actors/Dinos/allo/allo_lod1.mesh" `
  --skel "$G/Data/Graphics/Actors/Dinos/allo/allo.skel" `
  --skin "$G/Data/Graphics/Actors/Dinos/allo/allo_lod1.skin" `
  --anim "$G/Data/Graphics/Actors/Dinos/animations/HeavyBiped/HeavyBiped_Walk.anim" `
  --dino-texture "$G/Data/Graphics/Textures/actors/dinos/allo.tga" `
  --dino-pos 512,512 --dino-angle 200 --time 1200 `
  --yaw 35 --pitch 16 --distance 17
```

Options added for the combined scene:

```text
--dino-texture FILE   texture for the skinned mesh (separate from the basemap)
--dino-pos X,Y        spawn on the terrain in .imp coordinates (0..1024)
--dino-angle DEG      Z rotation of the placed mesh
--skin-bind MODE      mesh (default) or skin; see docs/formats/skin.md
```

## Camera

When a dinosaur is placed, the orbit camera targets the spawn point at
`z = terrain + 3` and defaults to a 22-unit distance. Otherwise it frames the
whole island. Mouse drag / arrows orbit, wheel zooms, `R` resets.

## Contact shadow

A 2.6-unit translucent disc is baked at the spawn's terrain height
(`u_alpha = 0.35`, alpha blending, depth write off) so the model reads as
standing on the ground. It moves with `--dino-pos`.

## Skinning note

The `.skin` full mesh is in a crouched bind pose. The viewer uses the
`.mesh` rest pose by default (`--skin-bind mesh`) with the same rigid weights
and inverse-bind matrices; this yields the standing dinosaur. `--skin-bind
skin` reproduces the crouched bind for comparison. Details and evidence:
`docs/formats/skin.md`.

## Vertical-slice status

| Requirement | Status |
| --- | --- |
| One island with terrain + textures | done |
| Placed objects with textures | done (1,536 trees, 9 draw batches) |
| Skinned dinosaur animation | done (`HeavyBiped_Walk`/`Neutral`/`Run`) |
| Player moves the camera | orbit camera done; free-fly next |
| Place a path/fence | not started (SIM-1) |
| 10 minutes without crashing | pending a long-run test |

## Evidence

- `notes/isl01-dino-meshbind.png` — standing allosaurus on isl01.
- `notes/isl01-dino-walk.png` — `HeavyBiped_Walk` at t=1200.
- `tests/test_viewer_cpp.py`:
  `test_cpp_skin_bind_modes`, `test_cpp_island_dino_validate`,
  `test_cpp_island_dino_window`.
