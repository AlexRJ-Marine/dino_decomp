# Hybrid decompilation

Status: **HYB-1/2/3/4 complete** — the original exe runs with the matrix,
normal, both vector, `.skin` and `.anim` readers replaced by recovered C++.

## Mechanism

`DinoIsland.exe` imports exactly one function from `DINPUT8.dll`:

```text
DINPUT8.dll: DirectInput8Create
```

We build a 32-bit proxy DLL named `DINPUT8.dll` and place it in `run/`.
Windows loads it from the application directory before the system DLL.

- `DllMain` loads the real `C:\Windows\SysWOW64\dinput8.dll` and forwards
  `DirectInput8Create` to it, so the game keeps working input.
- `DllMain` then installs an inline detour on the first target function.
- The DLL appends to `dino_hook.log` in the process working directory.

Only `run/` is touched. `original/` and `game/` are never modified. Delete
`run/DINPUT8.dll` to revert to the original code path.

## Replacements

### `FUN_004663a0` — 64-byte matrix reader

`0x4663a0` reads 64 bytes (16 floats) from the retail memory stream:

```c
return stream->vtable->read(buffer, 4, 0x10) == 0x10;
```

It is used by the `.skel`, `.skin` and `.anim` readers.

- Retail VA: `0x4663a0` (RVA `0x663a0`, image base `0x400000`).
- Retail prologue: `8B C1 6A 10 8B` (`mov eax,ecx; push 0x10; ...`).
- Patch: `E9 <rel32>` over the first 5 bytes, jumping to
  `hook_4663a0` in the proxy DLL.
- Replacement signature matches the original `__thiscall`:
  `bool hook(void* buffer, Stream* stream)`.

### `FUN_004809a0` — 12-byte normal reader

`0x4809a0` reads 12 bytes (3 floats) for a normal:

```c
return stream->vtable->read(buffer, 4, 3) == 3;
```

- Retail VA: `0x4809a0` (RVA `0x809a0`).
- Retail prologue: `8B C1 6A 03 8B`.
- Same 5-byte detour, replacement `hook_4809a0`.
- Used by the skin per-block normal arrays.

The proxy is verified to keep the game playable. During a park session the
matrix reader is called ~281,000 times and the normal reader ~95,000 times.

### `FUN_0047d460` / `FUN_0047cc90` — vector readers (opt-in)

Both are `__thiscall bool f(void* this, Stream* stream)` with `RET 4`:

```text
read u32 count, resize the 16-byte vector wrapper {vptr, first, last, end}
to the count (0x47d500 for u16 / 0x47cac0 for u32 insert), then read
count elements one at a time.
```

`0x47cc90` is the `.skel` child-index list (u32 per bone); `0x47d460` reads a
u16 array at `+0x78` of a 0xb8-byte mesh record. The 8-byte prologue
`51 53 56 57 8B 7C 24 14` needs an 8-byte patch (`E9 rel32` + 3 NOP).
Implementation: `hook_47d460`/`hook_47cc90` in `src/hook/dinput8_proxy.cpp`.
**Runtime-verified on a Cpg04 campaign load:** 2000+ calls each and the park
renders normally (`notes/hyb3-campaign.png`). Installed by default. Full ABI:
`notes/hybrid-vector-recon.md`.

### `FUN_0047d880` / `FUN_004839e0` — `.skin` and `.anim` readers

`0x47d880` reads the `.skin` per-bone table: count, name, u32 indices, f32
weights, 64-byte inverse-bind matrix into 0x6C records. `0x4839e0` reads one
`.anim` bone: name, u64 key times, matrix count, 64-byte matrices, into a
0x2C record. Both use the engine vector helpers (`0x47c210`/`0x47f8c0`,
`0x47a7c0`/`0x47daa0`, `0x484d30`/`0x486a60`, `0x4855f0`) and the mcString
reserve `0x4058b0`; the matrices go through the already-hooked `0x4663a0`.
6-byte prologues (`81 EC 94 00 00 00`, `83 EC 48 53 55 56`) need
`E9 rel32` + 1 NOP.

Implementation: `hook_47d880`/`hook_4839e0` in `src/hook/dinput8_proxy.cpp`,
installed by default. **Runtime-verified on the free-game park:** 2221 skin
bone records and 8911+ animation keys read, with the park rendering normally
(`notes/park-hook-all.png`, `notes/hyb4-counter.log`). Per-call logging is
disabled because these readers can run under the loader lock; the counters
are printed from `hook_4663a0` every 10,000 calls. Full ABIs:
`notes/hybrid-skin-recon.md`, `notes/hybrid-anim-recon.md`.

## Build/run

```powershell
.\scripts\build-hook.ps1
.\scripts\play-freegame-direct.ps1 -Seconds 55 -GameDir run -Shot notes\park-hook.png
```

`scripts/build-hook.ps1` compiles the 32-bit DLL with the w64devkit `g++ -m32`
and copies it to `run/DINPUT8.dll`. `scripts/play-freegame-direct.ps1` launches
`run/DinoIsland.exe`, clicks Free Mode → IGUANO → easy → Start, waits for the
park, screenshots, and prints `run/dino_hook.log`.

## Evidence

| File | Content |
| --- | --- |
| `notes/park-hook.png` | playable park reached with both hooks active |
| `notes/hook-4663a0.log` | first hook only: calls 1 → 281,000 |
| `notes/hook-4663a0-4809a0.log` | both hooks: 281k matrix + 95k normal calls |
| `src/hook/dinput8_proxy.cpp` | proxy + replacements |
| `scripts/build-hook.ps1` | 32-bit DLL build |
| `scripts/play-freegame-direct.ps1` | direct-launch park smoke test |

The combined log shows:

```text
DINPUT8 proxy attached
DINPUT8 real DLL loaded
hook: FUN_004663a0 patched
hook: FUN_004809a0 patched
hook_4663a0: calls=1
hook_4809a0: calls=1
...
hook_4663a0: calls=281000
hook_4809a0: calls=97000
```

## Safety/revert

- `run/DinoIsland.exe` remains the no-CD-patched copy; the hook does not
  modify it.
- `run/DINPUT8.dll` is the only added runtime file.
- Revert: `Remove-Item run\DINPUT8.dll` (and optionally `run\dino_hook.log`).
- The proxy is 32-bit and only loads when the game is launched from `run/`.

## Next

1. `0x47d460`/`0x47cc90` are **runtime-verified** (Cpg04, 2000+ calls each,
   park renders). The ABI is in `notes/hybrid-vector-recon.md`; the detours
   are installed by default.
2. `0x4809e0` is a 2-float (UV) reader using stream vtable slot `+4`; its only
   caller is `0x471cd0` and it is not exercised by the park run. Promote it
   once that path is identified.
3. `0x47d880` (`.skin`) and `0x4839e0` (`.anim`) are **runtime-verified**:
   2221 skin bones and 8911+ animation keys, park renders. Next: `0x477890`
   (`.skel` skeleton) and `0x4809e0` (UV reader).
4. Keep `docs/decomp/function-status.md` current; mark each entry `replaced`
   only after the running game exercises it.
