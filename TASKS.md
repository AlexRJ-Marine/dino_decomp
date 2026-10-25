# Task queue

One task at a time. A task is done only when its acceptance criteria are met,
tests pass, and `notes/STATE.md` records the evidence path.

## Phase 0 — environment

### ENV-1: Verify the agent environment
- [ ] `scripts/check-env.ps1` passes.
- [ ] `scripts/start-ghidra.ps1` starts the Ghidra MCP server.
- [ ] `ghidra` MCP tools list `DinoIsland.exe`.
- [ ] `scripts/build.ps1` builds the C++ skeleton.
- **Evidence:** `notes/env-check.md` with command output.

## Phase 1 — formats (quick wins first)

### FMT-1: `.mesh` header and file inventory
- [x] Parse the shared header across all 536 `.mesh` files.
- [x] Record version bytes, hash, sentinel, chunk count, name, payload count.
- [x] Reject malformed files with byte offsets.
- **Acceptance:** `pytest` passes; `notes/mesh-scan.json` has 536 valid entries.
- **Depends on:** ENV-1.
- **Evidence:** `notes/mesh-scan.json`, `docs/formats/mesh.md`, `tests/test_mesh_header.py`.

### FMT-2: `.mesh` geometry layout
- [x] Determine vertex stride, position/normal/UV offsets, index format, submeshes.
- [x] Cross-check against a known mesh (`cow_lod1.mesh`, `allo_lod1.mesh`) in Ghidra.
- **Acceptance:** parser reads a mesh and exports a valid OBJ with correct
  triangle count and bounding box; Blender opens it.
- **Depends on:** FMT-1.
- **Evidence:** `tools/formats/mesh.py`, `docs/formats/mesh.md`,
  `tests/test_mesh.py`; 536/536 meshes, 47,171 vertices, 35,190 triangles.

### FMT-3: `.skin` skinning data
- Parse bone list, inverse bind matrices, per-vertex weights/indices.
- **Acceptance:** all 104 `.skin` files parse; weights sum to 1.0 within 1e-3.
- **Depends on:** FMT-2.

### FMT-4: `.skel` skeleton
- Parse bone hierarchy, names, local transforms.
- **Acceptance:** all 64 `.skel` files parse; hierarchy is acyclic.
- **Depends on:** FMT-1.

### FMT-5: `.anim` animation
- Parse tracks, keyframe times, values, interpolation.
- **Acceptance:** all 299 `.anim` files parse; times are monotonic; a clip
  applied to a skeleton produces plausible motion.
- **Depends on:** FMT-3, FMT-4.

### FMT-6: Simple asset loaders
- `.tga` (16/24/32-bit), `.wav` (PCM), `.mpg` (MPEG-1), `.png`, `.jpg`.
- **Acceptance:** batch-convert 10 samples of each; dimensions/duration match.
- **Depends on:** ENV-1.

### FMT-7: Text/config formats
- `.loc` (UTF-16LE key/value), `.lnd` (length-prefixed strings), `.fnt`
  (bitmap glyphs), `.ini` (already text), `.shw`/`.gen`/`.imp`.
- **Acceptance:** parser round-trips a sample; font renders a reference string.
- **Depends on:** ENV-1.

## Phase 2 — first viewer

### VIEW-1: CMake + SDL3 + OpenGL window
- Window, input, delta time, clean shutdown.
- **Acceptance:** `scripts/build.ps1` builds; window opens for 5 seconds in a
  headless screenshot test or manual run.
- **Depends on:** ENV-1.

### VIEW-2: Load one mesh + texture
- Use the FMT-2 parser/output; render with a simple lit shader and camera.
- **Acceptance:** `cow_lod1.mesh` + its TGA renders; screenshot in `notes/`.
- **Depends on:** FMT-2, FMT-6, VIEW-1.

### VIEW-3: Skinned animation playback
- Load `.skel` + `.skin` + `.anim`, apply skinning, play a clip.
- **Acceptance:** `allo` walk/idle plays smoothly; screenshot/gif in `notes/`.
- **Depends on:** FMT-3, FMT-4, FMT-5, VIEW-2.

## Phase 3 — world

### WORLD-1: Island terrain
- Load `.lnd` + `.raw`; build a heightmap mesh; place landscape objects.
- **Acceptance:** island `isl01` loads with correct bounds and textures.
- **Depends on:** FMT-6, FMT-7, VIEW-2.

### WORLD-2: Object placement and frustum culling
- Read scenario/landscape placement data; instance meshes; cull.
- **Acceptance:** 1,000+ instances render at interactive FPS.
- **Depends on:** WORLD-1.

### WORLD-3: Skybox, water, lighting
- Match the original look closely enough for comparison screenshots.
- **Acceptance:** side-by-side screenshot against the original.
- **Depends on:** WORLD-1.

## Phase 4 — simulation

### SIM-1: Path/fence placement
- Grid-based placement, validation, undo.
- **Acceptance:** place/remove a path and a fence; state survives save/load.
- **Depends on:** WORLD-2.

### SIM-2: Visitor agents
- Spawn visitors, pathfinding, needs, attraction use.
- **Acceptance:** 50 visitors enter, use one attraction, and leave.
- **Depends on:** SIM-1.

### SIM-3: Dinosaurs
- Load genotypes, needs, behaviors, animation state machine.
- **Acceptance:** one dinosaur eats, sleeps, and reacts to a visitor.
- **Depends on:** SIM-2.

### SIM-4: Economy and attractions
- Money, ticket price, staff, attraction income.
- **Acceptance:** a 10-minute session is playable without crashes.
- **Depends on:** SIM-2.

## Phase 5 — presentation

### UI-1: Menus and HUD
- Main menu, park HUD, info panels.
- **Acceptance:** start a new game, open/close panels, save.
- **Depends on:** SIM-4.

### MEDIA-1: Audio and video
- DirectSound-equivalent playback, music, MPEG intro.
- **Acceptance:** music loops, SFX trigger, intro plays.
- **Depends on:** VIEW-1.

### SAVE-1: Save/load
- Reproduce enough of `.wld`/save semantics or define a new format.
- **Acceptance:** save, restart, load, state matches.
- **Depends on:** SIM-4.

## Phase 6 — parity and release

### PAR-1: Reference comparison harness
- Run original under dgVoodoo2/DXVK; capture screenshots and traces.
- **Acceptance:** deterministic comparison scenes documented.
- **Depends on:** VIEW-2.

### PAR-2: Performance and packaging
- 60 FPS at 1080p on the target machine; reproducible builds.
- **Acceptance:** zip build runs on a clean Windows VM.
- **Depends on:** UI-1, SAVE-1.

## Backlog / research

- DEC-1: Map loader functions for `.mesh`, `.skin`, `.skel`, `.anim`, `.fnt`.
- DEC-2: Map the simulation scheduler (`lpScheduler`) and AI update order.
- DEC-3: Map the `.wld` save format.
- DEC-4: Recover the original assert-string source map into `docs/architecture.md`.
- DEC-5: Determine whether `Pathmap.bin` is generated at runtime.
