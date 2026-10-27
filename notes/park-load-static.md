# Park load: static map of the window between the AudioPark ctor and the first world-file open

Question: after the Start click the last `CreateFileA` is `data\Sound\ParkSoundDB.ini`
(inside `FUN_004fdb00`); which code between there and the first world-file open can
block or spin forever?

Binary `/DinoIsland.exe-ebea17`. All addresses are virtual (image base 0x400000).
Evidence is decompilation only; nothing in `src/`, `game/`, `original/` was touched.

---

## 1. Call chain (exact call sites)

```
FUN_00402460                       DinoAppli main loop (NOT a Ghidra function; entered
                                   once, loops internally: 0x4029f1 JNZ 0x4024c6)
  0x4024c6  FUN_00412230           D3D8 TestCooperativeLevel -> app+0x1a
  0x4024ef  PeekMessageA drain     while (PeekMessage(&msg,0,0,0,PM_REMOVE))
  0x402507    msg.message==0x12    -> WM_QUIT sets the quit flag
  0x402538  if app[0x48].state==1 && app[0x48].b10   ; one-shot "start" request
  0x40256a    app[0x48].b10 = 0                      ; never retried
  0x4025e6    CALL 0x00403a80                        ; build ParkAppli (APPLI/DinoAppli.cpp:0x5ef)
  0x4025f8    CALL 0x00402f10   if app[0x48].sel==2  ; load-game variant
  0x4025ff    CALL 0x00403e10   else                 ; new park
  0x4025eb    (return of FUN_00403a80)               ; <-- first bisect anchor
```

`FUN_00403a80` (creates the whole park):

```
0x403aab  new(0xbc)                          ; ParkAppli
0x403b16  CALL 0x0051cc90(park, app+0x50, app+0x5c, app+0x58, app+0x60)
0x403b76  CALL 0x00500710                    ; sound volume, no I/O
0x403b88  CALL 0x00500760                    ; sound volume, no I/O
0x403b90  CALL 0x00596630 -> FUN_00523360    ; ViewManager (0x1e8) + vtable[1]=0x005b9cf0
                                             ;   = overlay widget manager (moWidgetManager.cpp),
                                             ;     no file I/O, no waits
```

`FUN_0051cc90` (ParkAppli ctor, `this` = the 0xbc block; all sub-objects are plain field
setup, **no I/O and no waits except the audio one**):

| call site | callee | size | field | object |
| --- | --- | --- | --- | --- |
| 0x51ccab | FUN_0051ce50 | – | +0x18..0x38 zeroed | – |
| 0x51cce7 | FUN_0055d680 | 0x30 | +0xb4 | display/config — **zero calls in the body** |
| 0x51cd1e | FUN_005861b0 | 0x24 | +0x8 | SimulationManager (string copy only) |
| 0x51cd50 | FUN_0051f0a0 | 0x6c | +0x10 | BehaviorManager (vtables + `FUN_0058bd90(0x40)`) |
| 0x51cd85 | FUN_005f35a0 | 0xf8 | +0x0 | ParkScene (`money_3.ini` string) |
| **0x51cdbd** | **FUN_004fdb00** | 0xb0 | +0x14 | **AudioPark — the last `CreateFileA` is in here** |
| 0x51cdec | FUN_005d8df0 | 0x28 | +0x4 | AnimationManager (string copy only) |
| 0x51ce31 | FUN_0053a310 | 0xa4 | +0xc | ParkInterface (6 sub-object vtables) |
| 0x51ce3d | RET | | | **`0x51cdc2` (= return of the audio CALL) / `0x51cdc6` (both paths merge) = first code after the AudioPark ctor** |

`FUN_004fdb00` (sdpAudioPark.cpp) order of work:

```
FUN_0040c070 / FUN_005213e0                 hwnd + engine ptr
FUN_005b6f30                                msSoundEngine object (0x20) + timer vtable call
FUN_005b7010 -> FUN_005c0c70 + vtbl[1]      <- DirectSound device init happens HERE,
                                              i.e. BEFORE the sound DB is opened
FUN_00503030 / FUN_00500be0 / FUN_005b9070 / FUN_005b8fc0 / FUN_005b8fe0   settings
build "data\\Sound\\ParkSoundDB.ini" -> FUN_005b70a0
        FUN_00408530   mcTextFile reader ctor
        FUN_00408800   LoadTxtFile(path)          <-- LAST CreateFileA
        FUN_005b7a90   walk parsed resources, new 0x2c msSoundObject per entry, hash insert
"musLabo" string
```

`FUN_00403e10` (new park):

```
0x403e10  FUN_005073d0            clear a list
          FUN_0051d050            register 10 scheduler tasks (ParkInterfaceProcess,
                                  ParkInterfaceUpdate, SimulationManager, BehaviorManager,
                                  WorldManager, CampaignManager, AnimationManager,
                                  ParkScene, AudioPark, ParkInterfaceDraw)
          FUN_00525770            store app+0x4c into the view
0x403e65  CALL 0x0051d1d0         park init (papParkApplication.cpp)
```

`FUN_0051d1d0` (with the ordering that matters):

```
0x51d1e7  FUN_00507f30            store a global
0x51d20c  FUN_00508070            LOD index 0..3
0x51d21c  FUN_00507c60            if (app+0x31d00 != 0) return 1;   // WINDOWED -> no-op
                                  else validate the video mode with 6x FUN_00412320
0x51d22e  FUN_0040e200            only on the failure path
0x51d242  if (app+0x31d00 == 0) { // FULLSCREEN ONLY: loading page
0x51d2bf     FUN_00441ee0         "./DATA/OVERLAY/TEXTURES/loading_800x600.tga"
0x51d2e8..   FUN_0040ebe0/eb80/ec20/e5b0/e600   clear + quad
0x51d34d     FUN_0040eab0         present  (device vtbl+0x3c)
          }
0x51d37c  CMP [mode+0x34],3 / JA 0x51d908        ; invalid LOD -> error + return 0
0x51d3ba  FUN_00508e00
0x51d42c  CALL 0x0053a400         WorldManager wrapper
0x51d4dd  CALL 0x00586200         SimuManager ctor  (no I/O)
0x51d4fa  CALL 0x0051f560         BehaviorManager wiring (no I/O)
0x51d582  CALL 0x00408800         LoadTxtFile("DATA\\Graphics\\Focus\\Focus.ini")  <-- next I/O
0x51d5bb/613/671  FUN_00409550    BUILDING / Language lookups
0x51d6c4  FUN_005f37a0, FUN_004fe210, FUN_005d8e60
0x51d787  CALL 0x00408800         LoadTxtFile("data/config/dino.ini")
0x51d8b6  CALL 0x005234e0         ParkInterface widgets (~14 KB, no file I/O)
```

`FUN_0053a400` is only a factory: `new(0x10)` world wrapper, `new(0x1fc) FUN_0053d4d0`,
`new(0x28) FUN_00557a30("DATA\\design\\scenario\\show\\")`, `DAT_00675734 = wm+400`.
**No file I/O in it.** The I/O is in the sub-manager setters that `FUN_0053d4d0` calls:
`FUN_0053d9a0` (design paths[0] = `DATA\design\attractions\features\`) does
`0x53da6e CALL 0x00417d10` on **`walllist.ini`** — that is the first world-data open,
not anything inside `FUN_0053a400`.

---

## 2. What can actually block

### 2.1 There is no loader thread (definitive)

The binary imports **no `CreateThread` / `_beginthreadex` at all**. The complete set of
blocking primitives in the image is:

| API | call sites | location |
| --- | --- | --- |
| `WaitForSingleObject` | 0x5cd457, 0x5ce6cf, 0x5ce9ec | streaming-audio cluster only |
| `MsgWaitForMultipleObjects` | 0x5cc604 | streaming-audio cluster only |
| `WaitForMultipleObjects` | (IAT only, no code xref) | – |
| `timeSetEvent` | 0x5cc7a0 | that cluster's "thread" (winmm timer) |
| `MessageBoxA` | 0x414091, 0x423412, 0x596e23 | mcLib error reporter |

None of the 0x5cc000–0x5cf000 audio-streaming functions is reachable from
`FUN_004fdb00` (which only calls 0x5b6f30 / 0x5b7010 / 0x5b70a0 / 0x500be0 / 0x503030),
so **a thread join cannot be the blocker**: the park is built synchronously on the main
thread. `FUN_00408530` / `FUN_004086e0`, which appear on both the audio and the park-init
paths, are *not* locks — they are the mcTextFile reader constructor/destructor
(`FUN_00408800` = `LoadTxtFile`, string + array teardown).

### 2.2 The only modal blocker: the mcLib error reporter

`FUN_004232a0` (handler installed by the error-manager ctor `FUN_00423460`, object
`DAT_00674f78`):

```c
EnterCriticalSection(&errLock);
FUN_00587340("ErrorLog.txt", ...);          /* STUB: empty function in the retail build */
FUN_00413db0(...);                          /* severity/file/line record (not verified) */
if (*(char *)(DAT_00674528 + 0x31950) == 1) {          /* WINDOWED */
    MessageBoxA(GetActiveWindow(), text, caption, 0x12012);   /* modal: blocks forever */
} else if (msg[0]) { /* ring buffer only, invisible */ }
LeaveCriticalSection(&errLock);
```

* `DAT_00674528 + 0x31d00` is the **Windowed** flag: it is copied verbatim into
  `D3DPRESENT_PARAMETERS.Windowed` (`FUN_0040fee0`, `[app+0x31e2c] = [app+0x31d00]`)
  and into `[app+0x31950]` by `FUN_00411630`.
* `FUN_00413f20` (the default chain handler) **always** dialogs, no windowed check.
* `FUN_00596bb0` posts `MessageBoxA(hwnd, 0, "Fatal Error", 0x11030)`.

Consequences:

1. **Forcing `Windowed=1` switched modal error dialogs ON.** In fullscreen the same
   error is silent (the dev `ErrorLog.txt` writer `FUN_00587340` is compiled to `ret`),
   so nothing is ever logged on disk. `GetActiveWindow()` + the harness restoring the
   previous foreground window makes the dialog easy to miss.
2. A dialog explains every observation at once: no further `CreateFileA`, no `Present`,
   frozen DirectInput state counters (the game loop is inside the modal pump), and a
   process that is alive and idle rather than dead.

The same flag also gates the rest of the park start:

* `FUN_00507c60` (`0x51d21c`): `if (Windowed) return 1;` — with `Windowed=1` the
  video-mode validation never runs (otherwise it calls `FUN_00412320` up to six times,
  each doing a device release + `CreateDevice`/`Reset` in mgEngineDx8, with
  "Unable to restore previous video mode" error paths).
* `FUN_0051d1d0` (`0x51d242`): with `Windowed=1` the **loading page is skipped**, so
  `loading_800x600.tga` is not reopened at park start (it was already loaded from the
  menu, `notes/background-input-recon.md` §2). `ParkSoundDB.ini` can therefore legally
  be the last `CreateFileA` even on a healthy run — the next open should be
  `walllist.ini` (0x53da6e), then `Focus.ini` (0x51d582), then `dino.ini` (0x51d787).

### 2.3 The code that runs immediately after the last successful open (ranked #2)

`FUN_005b70a0` else-branch = `FUN_00408530` (reader) + `FUN_00408800` (`LoadTxtFile`) +
`FUN_005b7a90` (walk the parsed tree). `FUN_005b7a90` news one 0x2c `msSoundObject`
per entry (224 entries in `ParkSoundDB.ini`) and hashes it:

```c
uVar6 = FUN_00406ac0(hashbuf) % (uint)((this[8] - this[4]) >> 2);   /* inside FUN_005b7a90 */
```

No waits, but a zero bucket count would trap (INT divide by zero) and the insert is
O(n) per entry. This is literally the code that executes right after the last observed
file open, so it is the first thing to exclude.

### 2.4 D3D8 (fullscreen path only)

`FUN_0040eab0` present (`device vtbl+0x3c`), and `FUN_00412320`
("Change Video Mode :" / `SetDisplayMode` + `CreateDevice`) are reachable only from the
`Windowed == 0` branches (`0x51d21c`, `0x51d2bf`..`0x51d34d`). Already mitigated by the
`Windowed`/`Reset` shim; not the current blocker.

### 2.5 The "loading loop" is a poll, not a wait

After the park exists, the per-frame path is `0x402627` → `FUN_00517ab0` →
`FUN_005c0350`, which is just

```c
return (obj->f9c != 0) ? *(byte *)(obj->f9c + 0x28) : 0;   /* a flag read */
```

so the loop iterates and returns; it cannot hang there. The state object
`app_data+0x48` is `{+0x4 state (1 menu / 3 park), +0x8 view, +0x10 one-shot start
request, +0x20 pending command (0x29a = movie), +0x24 load-vs-new selector}`. There is
**no progress counter and no completion wait** anywhere in the park application code.

---

## 3. Conclusion

Ranked blockers for "no `CreateFileA` after `ParkSoundDB.ini`, main thread idle":

1. **A modal `MessageBoxA` from the mcLib error reporter** (`0x423412` in
   `FUN_004232a0`, or the always-dialog default `FUN_00413f20` at `0x414091`, or
   `FUN_00596bb0` at `0x596e23`) — now enabled because the harness forces `Windowed=1`.
   Confidence: medium-high; it is the only blocking primitive reachable in the window,
   and the retail error log is a stub so nothing else records it.
2. **The sound-DB post-load in `FUN_005b7a90`** (whole function, 0x5b7a90), which is the code
   that runs immediately after the last observed open. Confidence: low-medium.
3. D3D8 `Reset`/`Present` — only on the fullscreen path. Confidence: low for the
   current windowed run.

Ruled out by construction: thread join / background loader (no `CreateThread` import;
all waits live in the unreachable 0x5cc–0x5ce audio cluster), a spin in
`FUN_0053d4d0`/`FUN_0053a400` (pure object construction), `FUN_0055d680` (16 stores,
zero calls), and the post-`0x51d42c` helpers `FUN_00586200` / `FUN_0051f560` /
`FUN_005234e0` (no I/O, no waits).

## 4. Next runtime breakpoint

**`bp` on the `MessageBoxA` slot: `0x00414091`, `0x00423412`, `0x00596e23`**
(equivalently a hardware break on `[0x0062526c]`). One hit names the exact failing
assert on the stack and closes the question; no hit within one Start click eliminates
the entire class, because those three sites are the only modal primitives in the
program.

Fallback bisect ladder if the dialog is not it:
`0x0051cdc6` (did the AudioPark ctor return?) → `0x00403b1b` (did the park ctor return?)
→ `0x0051d42c` (did `FUN_0051d1d0` reach the world manager?) → `0x0053da6e` (the
`walllist.ini` `LoadTxtFile`).

## 5. Follow-ups

- Re-run with `Windowed=1` and the dialog breakpoints; if one hits, capture
  `local_114` (`"%s(%d):\n%s\nPress Retry for Debug"`) and the assert file/line.
- If no dialog: break at `0x0051cdc6` and `0x0053da6e` to split audio-ctor vs
  world-manager, then single-step `FUN_005b7a90`'s loop for the `%` trap.
- Consider clearing `[app+0x31950]` in the shim (keep the D3D `Windowed` flag but
  silence the modal reporter) so a hidden error cannot park the main thread.
