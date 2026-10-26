# Vertical-slice scene (island + dinosaur)

Status: **working** — isl01 terrain, 1,536 placed trees, and an animated
allosaurus standing on the terrain in one C++/SDL3/GL scene, with orbit and
free-fly cameras.

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

## From the retail menu

The same slice can be reached through the real menu screens: `--start-dino`
preloads the allosaurus assets, and the Free Game Start handoff places it at
the centre of the selected island after loading it.

```powershell
$G = "game/%MAINDIR%"
.\build\mingw\dino_viewer.exe --asset-root $G `
  --menu-page 1 --menu-click 200,180 --menu-click 494,451 `
  --menu-click 526,531 --menu-loading-time 0 --start-dino
```

This loads isl01/easy (75,000), places the allosaurus at (512,512), and
starts the walking animation. Screenshot: `notes/menu-start-dino.png`.

## Camera

Ground-target mode (default): **left-drag** orbits, wheel zooms, arrows
orbit, `R` resets. `W/A/S/D` walk the look-at point over the island and
`Q/E` raise/lower it, so the camera can travel freely without entering
free-fly. When a dinosaur is placed, the target is the spawn point at
`z = terrain + 3`; otherwise it frames the whole island.

The menu-started park uses the recovered Free Game camera: target is the
island centre at terrain height, horizontal distance 39, elevation 52.7,
yaw 0 (`Data/Config/camera.ini` progress 0.1; retail save eye
`(551,512,84.7)`, target `(512,512,32)` on isl01). The park scene renders
with the original FOV **80°**; mesh-only views keep 33°. `--start-dino` uses a
close demo camera (distance 22) so the allosaurus is visible.

Free-fly mode: `--free-camera` or `C` to toggle. Controls:

```text
W/S          forward/back along the view direction
A/D          strafe
E/Q          up/down
Shift        sprint (140 u/s vs 40 u/s)
left-drag    mouse look
```

The camera is clamped to `terrain height + 1.6` while it is over the island,
so it cannot sink through the ground. All actions are rebindable in
`dino_keys.cfg` (`move_forward`, `move_back`, `move_left`, `move_right`,
`move_up`, `move_down`, `sprint`, `toggle_camera`).

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
| Player moves the camera | orbit + free-fly done |
| Place a path/fence | done (SIM-1: grid, ray-pick, undo, save/load) |
| 10 minutes without crashing | done: 60,000 frames at ~100 FPS, 134.1 MB flat |

## Evidence

- `notes/isl01-dino-meshbind.png` — standing allosaurus on isl01.
- `notes/isl01-dino-walk.png` — `HeavyBiped_Walk` at t=1200.
- `tests/test_viewer_cpp.py`:
  `test_cpp_skin_bind_modes`, `test_cpp_island_dino_validate`,
  `test_cpp_island_dino_window`.
