---
name: Dino hook loop
description: Build, install and verify a DINPUT8 inline detour in the retail DinoIsland.exe (hybrid decompilation). Use when editing src/hook/dinput8_proxy.cpp, adding a reader/function replacement, or checking the hook counters.
---

# Dino hook loop

The proxy is 32-bit `run/DINPUT8.dll`, built from `src/hook/dinput8_proxy.cpp`.
It forwards the single imported `DirectInput8Create` and installs inline
detours plus the `.mesh` vtable hook. Every hook is verified by running the
retail game and reading counters from `dino_hook.log`.

## 1. Build

```powershell
.\scripts\build-hook.ps1
```

This compiles with w64devkit `g++ -m32` and writes `run/DINPUT8.dll`. It must
show `DirectInput8Create` in the export list.

## 2. Install into the test runtime

```powershell
.\scripts\setup-test-runtime.ps1            # run-test (x32dbg / virtual input)
# .\scripts\setup-test-runtime.ps1 -Target run-window   # normal play copy
```

`run-test/` and `run-window/` use a junction to `game/.../Data` and their own
`save/`. The setup script copies the DLL and dgVoodoo2 wrappers from `run/`.

## 3. Run the free-game park (headless)

```powershell
.\scripts\play-freegame-direct.ps1 -Seconds 25 -Shot notes\harness\park-hook.png
```

Default is windowed + virtual input in `run-test/`; it keeps the game in the
background and restores the previous foreground window. Use `-GameDir run`
plus `-PhysicalInput` only for the legacy fullscreen path.

## 4. Verify

```powershell
Select-String -Path run-test\dino_hook.log -Pattern 'patched|counters:|total calls'
```

Expected for the free-game park (values drift slightly with animation time):

```text
hook: FUN_004663a0 patched
...
counters: mesh=121 skin=2221 anim=8911 skel=21 vec16=2221 vec32=1680 fnt=5
```

- Hooks install by default; a `prologue mismatch` log means the exe changed or
  the address is wrong.
- Do not log per call inside the asset readers: they run under the loader lock
  and `CreateFileA` deadlocks. Counters print from `hook_4663a0` every 10,000
  calls, plus totals at DLL detach.
- If a run is left open, kill the exact PID, not all `DinoIsland` processes
  (another lane may be testing).

## 5. Evidence

- Screenshot to `notes/harness/` (gitignored) for the run; copy a final,
  stable shot into `notes/` only when it is trackable evidence.
- Update `notes/hybrid-*-recon.md`, `docs/decomp/hybrid.md`,
  `docs/decomp/function-status.md`, `notes/hyb*-counter.txt`.
- Record the completion with `scripts/state.ps1 -Done "..."`.

## Contracts that bite

- Function signatures are `__thiscall`; the callee pops stack args. Get the
  `RET n` right or the caller stack is corrupted.
- 5/6/8/12-byte patches must match the verified prologue exactly; add a new
  `install_detourN` with a prologue check instead of weakening an existing one.
- Engine vectors are 16-byte `{vptr, first, last, end}`; use the engine insert
  helpers, not `std::vector`.
- `.mesh` and `.skin` share `0x477f30`; branch on `this+4` magic
  (`0x12345678` mesh, `0x02468ACE` skin) and forward `.skin` to the original.
