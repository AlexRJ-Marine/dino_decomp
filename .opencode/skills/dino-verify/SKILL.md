---
name: Dino verify
description: Build and test the portable viewer quickly without colliding with another agent. Use when choosing a build directory, running a fast or full test subset, or reproducing a viewer test.
---

# Dino verify

## Fast loop

```powershell
.\scripts\build.ps1 -BuildDir build\<lane>
.\scripts\test.ps1 -Exe build\<lane>\dino_port.exe -Fast
```

`-Fast` skips the `slow` tests (everything in `tests/test_app_cpp.py`, which
spawns `dino_port.exe`). Format/unit tests run in a few seconds.

## Targeted viewer tests

```powershell
.\scripts\test.ps1 -Exe build\<lane>\dino_port.exe -Pattern phenotype
.\scripts\test.ps1 -Exe build\<lane>\dino_port.exe -File tests/test_app_cpp.py -Pattern wld_park
```

The viewer is a separate process, so `DINO_EXE` must point at the exe
you just built. `scripts/test.ps1` sets it.

## Full suite before commit

```powershell
.\scripts\run-tests.ps1
# or, with the read-only and whitespace checks:
.\scripts\verify.ps1
.\scripts\verify.ps1 -Fast
```

`verify.ps1` also fails when `original/` or `game/` has local changes and runs
`git diff --check`.

## Build isolation

- One build directory per lane: `build/port`, `build/hybrid`, `build/check`.
- Never build into `build/mingw` while another lane is running its viewer;
  the link fails with "Permission denied".
- If a build is locked, find the exact process:
  `Get-Process dino_port | Select Id,Path` and stop only the one using your
  build directory. Do not kill all `DinoIsland`/`dino_port` processes.
- `dino_port.exe` supports `--frames N --no-menu --no-hud --no-vsync
  --no-save-config` for headless runs and `--screenshot path.bmp/png`.

## Common failures

- `viewer not built` / skipped tests: `DINO_EXE` points at a missing exe.
- `FileNotFoundError` for `game/%MAINDIR%`: run from the repo root; tests
  resolve paths from `Path(__file__).parents[1]`.
- Long stalls: a test is waiting on a modal window; use `--frames 2` and
  `--no-menu` for logic-only checks.
