# Hybrid decompilation

Status: **HYB-1 complete** — the original exe runs with the first retail
function replaced by recovered C++.

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

## First replacement: `FUN_004663a0`

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

The proxy is verified to keep the game playable and the function is called
hundreds of thousands of times during a park session.

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
| `notes/park-hook.png` | playable park reached with the hook active |
| `notes/hook-4663a0.log` | proxy attach, patch, calls 1 → 281,000 |
| `src/hook/dinput8_proxy.cpp` | proxy + replacement |
| `scripts/build-hook.ps1` | 32-bit DLL build |
| `scripts/play-freegame-direct.ps1` | direct-launch park smoke test |

The log shows:

```text
DINPUT8 proxy attached
DINPUT8 real DLL loaded
hook: FUN_004663a0 patched
hook_4663a0: calls=1
...
hook_4663a0: calls=281000
```

## Safety/revert

- `run/DinoIsland.exe` remains the no-CD-patched copy; the hook does not
  modify it.
- `run/DINPUT8.dll` is the only added runtime file.
- Revert: `Remove-Item run\DINPUT8.dll` (and optionally `run\dino_hook.log`).
- The proxy is 32-bit and only loads when the game is launched from `run/`.

## Next

1. Replace the next leaf/util function (e.g. the 4-byte stream reader or a
   matrix helper) with C++ and verify the park still loads.
2. Work up to `0x477890` (`.skel`), `0x47d880` (`.skin` records), and
   `0x4839e0` (`.anim` bone records); these need the stream/container object
   contracts, not just the file format.
3. Keep `docs/decomp/function-status.md` current; mark each entry `verified`
   only after the running game exercises the replacement.
