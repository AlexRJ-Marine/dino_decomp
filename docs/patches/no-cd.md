# No-CD patch — `DinoIsland.exe`

Status: **startup CD check bypassed and verified under x32dbg + dgVoodoo2.**

This patch is for the exact retail build in this workspace. Do not apply it to a
different executable. It is a personal preservation/interoperability patch; do
not redistribute the patched binary or the game assets.

## Target

| Field | Value |
|---|---|
| File | `DinoIsland.exe` (2,617,344 bytes) |
| Original MD5 | `5c8352429b0ede4a0d3ff4c8743f59cb` |
| Original SHA-256 | `7ba16f38db4fe9461e700e6005e29a37c0d7a6d9441a233880a7c4405bbd717c` |
| Patched MD5 | `7b090049115af62e6eaf32682ca0f4c3` |
| Patched SHA-256 | `6ad30c0a801b3b86d4dfb1bab0d87cb411f76dfd1b9bb8c5ae9d8776f0029422` |
| File offset | `0x1BFC` |
| Virtual address | `0x401BFC` (image base `0x400000`) |

Apply it only to a copy under `run/`. Never patch `game/` or `original/`.

## Symptom

Without a CD-ROM drive that passes the volume check, the game displays the
`&InsertCD` dialog and exits before the main menu.

## What the check does

The startup code scans drive letters `C:` through `Z:`:

```asm
00401B55  mov  edi, dword ptr [GetDriveTypeA]
00401B60  lea  eax, [esi + 0x61]      ; 'a' + index
00401B6A  push eax
00401B6B  push 0x64BCB4               ; "%c:\\"
00401B71  call 0x5972BA               ; sprintf
00401B80  push edx
00401B81  call edi                    ; GetDriveTypeA
00401B83  cmp  eax, 5                 ; DRIVE_CDROM
00401B86  jne  0x401BE7               ; next drive
...
00401BBC  call dword ptr [GetVolumeInformationA]
...
00401BF9  cmp  esi, 0x1A              ; scanned all 26 drives?
00401BFC  jne  0x401D4C               ; CD found -> success path
00401C02  test al, al
00401C04  jne  0x401D4C
00401C0A  push 0x64BDD0               ; "&InsertCD"
```

When no CD is found, `esi == 26` and the code falls through to the
`&InsertCD` dialog. The branch at `0x401BFC` is the “a CD was found” shortcut to
the normal startup path at `0x401D4C`.

## Patch

At file offset `0x1BFC`, replace:

```text
0F 85 4A 01 00 00     jne 0x401D4C
```

with:

```text
E9 4B 01 00 00 90     jmp 0x401D4C ; nop
```

`E9` is a rel32 jump. The displacement is `0x401D4C - 0x401C01 = 0x14B`, so the
bytes are `E9 4B 01 00 00`; the trailing `90` preserves the original six-byte
length.

This forces the normal startup path even when the drive scan finds no CD.

## Apply

From the workspace root:

```powershell
# Patch a copy (recommended workflow; run/ is already a copy)
.\scripts\patch_nocd.ps1 -Target run\DinoIsland.exe

# Or use the Python patcher
.\.venv\Scripts\python.exe tools\patch_nocd.py --apply run\DinoIsland.exe

# Revert
.\.venv\Scripts\python.exe tools\patch_nocd.py --revert run\DinoIsland.exe

# Verify
.\.venv\Scripts\python.exe tools\patch_nocd.py --verify run\DinoIsland.exe
```

An IPS patch is provided at `patches/dinoisland-nocd.ips`. It applies at
offset `0x1BFC` and replaces six bytes.

The patcher creates `DinoIsland.exe.orig` next to the target if a backup does
not already exist. It refuses to patch a file whose bytes or SHA-256 do not
match the known target unless `--force` is used.

## Verification

Verified in this workspace with x32dbg `headless.exe` and dgVoodoo2:

- The patched copy no longer shows `&InsertCD`.
- The game reaches `Begin3D Success`.
- It loads `Data/States/*.ini`, `Data/Overlay/MenuGraphicDB.ini`, and menu
  textures (`menu_outcome2.tga`, `menu_outcome4.tga`).
- x32dbg log: `notes/x32dbg-nocd.out.log`.

The patch bypasses the **startup drive scan only**. It does not patch any other
protection because none was found in this build. If a later prompt appears, it
is a separate check; verify that the running executable is the patched copy
(SHA-256 `6ad30c0a...`), not `game/DinoIsland.exe`.

## Files

| Path | Purpose |
|---|---|
| `patches/dinoisland-nocd.ips` | IPS patch for offset `0x1BFC` |
| `tools/patch_nocd.py` | Apply/revert/verify and generate IPS |
| `scripts/patch_nocd.ps1` | Thin PowerShell wrapper |
| `notes/x32dbg-nocd.out.log` | Verification trace |

## Legal

This is a local interoperability/preservation patch for a game the user owns.
Do not distribute the patched executable, the original game files, or extracted
assets. Keep `game/` and `original/` out of version control.
