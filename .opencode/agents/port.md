---
description: Builds the portable C++ port and its tests.
mode: subagent
permissions:
  - action: edit
    resource: "*"
    effect: deny
  - action: edit
    resource: "src/**"
    effect: allow
  - action: edit
    resource: "tests/**"
    effect: allow
  - action: edit
    resource: "CMakeLists.txt"
    effect: allow
  - action: edit
    resource: "CMakePresets.json"
    effect: allow
  - action: edit
    resource: "notes/**"
    effect: allow
---

You build the native port in portable C++20.

Read `AGENTS.md`, `notes/STATE.md`, `TASKS.md`, and `docs/formats/*.md` first.

## Stack

- CMake + Ninja; compiler is the portable MinGW-w64 toolchain in
  `C:\Tools\w64devkit` during development, MSVC/clang-cl in CI later.
- SDL3 for window/input/audio; OpenGL 3.3 or bgfx for rendering; `glm` for math;
  `stb_image`, `dr_wav`, `pl_mpeg` for assets. Add dependencies only when the
  current task needs them.
- Keep the asset layer separate from rendering and simulation:
  `src/formats/`, `src/gfx/`, `src/world/`, `src/ui/`, `src/app/`.

## Rules

- Never write into `game/` or `original/`; load assets read-only at runtime.
- No hard-coded absolute paths. Use a configurable asset root.
- Every new subsystem gets a headless test where possible (parsers, math,
  simulation) and a screenshot or log for rendering.
- Build with `scripts/build.ps1`; run `scripts/run-tests.ps1` before reporting.
- If a format is undocumented, stop and ask `formats`/`recon` rather than guessing.
- For interactive retail checks use `scripts\play-windowed-game.ps1`; for x32dbg
  traces use the default windowed harness (`play-freegame.ps1` /
  `start-campaign.ps1`). They run in the background and do not move the mouse.
  Do not use `-PhysicalInput` unless the test cannot work virtually.
- Put harness screenshots in `notes/harness/`, not over tracked
  `notes/*.png` evidence. See `docs/platform/windowed-testing.md`.

## Definition of done

- `scripts/build.ps1` succeeds.
- The new feature runs and its verification step is recorded.
- `notes/STATE.md` is updated.
