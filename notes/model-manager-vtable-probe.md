# Model-manager vtable and the `.bfi` display_type 3 branch (runtime probe)

Date: 2026-10-31. Retail `DinoIsland.exe` under the windowed x32dbg harness,
Cpg04 campaign park load. Read-only; no gameplay or format code changed.

## Question

`FUN_005f4e10` dispatches the construct/display hook through a manager virtual:

```
0x5f4e2a  CALL dword ptr [EAX+0x28]
```

What are the vtable and the `[vtable+0x28]` implementations, what are the exact
arguments (record block and scene handle), and which branch builds the
`.bfi` `display_type 3` ground tile?

## Answer (short)

Two manager vtables were observed on 88 calls during the Cpg04 load:

| manager vtable | `[vtable+0x28]` | calls | context |
| --- | --- | ---: | --- |
| `0x00648A68` | `0x00622DF0` | 71 | show/generic feature display factory |
| `0x00648AA0` | `0x006236C0` | 17 | LunchBox feature builder (park/fence owners) |

At `0x5f4e2a` (before the `CALL`):

```
ECX      = manager object        (EAX = [ECX] = vtable)
[esp]    = record block          (design+0x84, the `.bfi` display block)
[esp+4]  = feature actor         (return of FUN_00589020)
[esp+8]  = scene handle          (FUN_005f4e10's this+0xdc)
```

At the callee entry (`0x622DF0` / `0x6236C0`): `ECX = manager`,
`[esp+4] = record`, `[esp+8] = actor`, `[esp+0xc] = scene`. The return address
is `0x005F4E2D`.

The record fields are relative to the block base `rec = design+0x84`:

| field | address | runtime check |
| --- | --- | --- |
| price (file `0x19`) | `rec-0x5c` (design+0x28) | LunchBox 100, CageMedium 800, Beam5 500 |
| waypoint team (file `0x1d`) | `rec-0x58` (design+0x2c) | WaypointTeam1 = 1, Team2 = 2 |
| **display type** (file `0x21`) | `rec-0x54` (**design+0x30**) | 0 mesh/judge, 1 cage, 3 tile |
| class tag `char*` | `rec-0x50` (design+0x34) | "Tile", "Waypoint", "StaticBait", "Cage", "Judge", "" |

**Correction to the earlier note:** the `.bfi` display type is at
**record+0x30**, not record+0x28. Record+0x28 is the price. The loader
(`0x53f1c1` reads file `0x21`; `0x53f1e4` stores it at `record+0x30`); the
runtime values above match the files exactly.

The `0x622DF0` factory does **not** switch on display_type directly. It
switches on the actor class tag `[[actor+0x28]+0x28]`, and the display_type 3
classes route to the ground-tile builder `0x00622240`
(`gatShowTileFeature.cpp`):

| class tag | compare/branch | target | object | ground base |
| --- | --- | --- | --- | --- |
| "Cage" `0x663520` | `0x622e36` | `0x622e38` | alloc 0x80, `0x5edb30` | - |
| "Tile" `0x665074` | `0x622eb1` | `0x6231af` | alloc 0x58, **`0x622240`** | direct |
| "Teleport" `0x665050` | `0x622ee4` | `0x6231af` | alloc 0x58, **`0x622240`** | direct |
| "Waypoint" `0x66505c` | `0x622f17` | `0x622f1d` | alloc 0x7c, `0x6240c0` | calls `0x622240` at `0x6240e1` |
| "StaticBait" `0x665068` | `0x62301d` | `0x62301f` | alloc 0xb0, **`0x623fa0`** | calls `0x622240` at `0x623fc0` |
| "AnimalBait" `0x664f54` | `0x62309b` | `0x62309d` | alloc 0x5c, `0x623f20` | base class |
| else (Judge / empty) | `0x6230ef` | `0x62314c` | alloc 0xe0, `0x621770` | - |

So for the static baits (`StaticBaitFlower/Apple/Hay/Pumpkin/Watermelon`,
class tag "StaticBait", display_type 3) the branch is
`0x00622DF0 -> 0x0062301F -> 0x00623FA0`, whose first action is
`CALL 0x00622240` (the 4x4 ground quad/UV base). `0x623FA0` then sets
`vtable 0x648B04` and builds its feeder subobject at `+0x58`.
`0x622240` builds the flat quad from the placement rectangle scaled by the
0.5 constant at `0x0062544C`; the exact UV rectangle is still to be decoded,
but it lives in `0x622240`, not in the manager virtual.

## Static call chain

Ghidra `/DinoIsland.exe-ebea17`:

```
0x5f4e10  PUSH ESI
          MOV ESI,ECX                  ; this
          MOV ECX,[ESP+0x8]            ; arg1 = manager
          MOV EDX,[ESI+0xdc]           ; scene
          MOV EAX,[ECX]                ; manager vtable
          PUSH EDX                     ; virtual arg3
          MOV EDX,[ESP+0x14]           ; arg3 of FUN_005f4e10 = actor
          PUSH EDX                     ; virtual arg2
          MOV EDX,[ESP+0x14]           ; arg2 = record block
          PUSH EDX                     ; virtual arg1
0x5f4e2a  CALL [EAX+0x28]              ; ECX=manager, [esp]=record
0x5f4e2d  MOV ECX,[ESI+0xf4]
          PUSH EAX                     ; created display object
          CALL 0x5fc260                ; add to the scene container
          RET 0x10
```

The single caller is `FUN_0053acd0` at `0x53aeee`:

```
0x53aed7  MOV ECX,[ESI+0x5c]      ; arg4 for FUN_005f4e10
0x53aeda  MOV EAX,[ESI+0x3c]      ; arg1 = manager
0x53aede  MOV ECX,[ESP+0x14]      ; FUN_005f4e10 `this`
0x53aee2  LEA EDX,[EBP+0x84]      ; arg2 = design+0x84 (record block)
0x53aee8  PUSH EDI                ; arg3 = feature actor
0x53aee9  MOV ECX,[ECX+8]         ; FUN_005f4e10 ECX = this
0x53aeec  PUSH EDX
0x53aeed  PUSH EAX
0x53aeee  CALL 0x005f4e10
```

Runtime values during the Cpg04 load: `FUN_005f4e10` `this` = `0x097E3AC8`
(same for all calls), scene = `this+0xdc` = `0x0985A290`, scene container =
`this+0xf4` = `0x0AEFA390`. The manager is `owner[0x3c]` (the first stack arg
of `FUN_005f4e10`), not `FUN_005f4e10`'s `this`.

## Runtime probe

Harness: `scripts/x32dbg/model-manager-probe.ps1` (new, harness lane; virtual
input only, no cursor/foreground calls, no screenshots). It reuses the proven
`start-campaign.ps1` menu path and installs logpoints while the debuggee is
paused before the mission Start click.

Commands (final evidence runs):

```powershell
.\scripts\x32dbg\model-manager-probe.ps1 -Mode Entry -CampaignRow 3 `
    -MissionRow 0 -UnlockCampaigns -SimSeconds 10 -Seconds 40 `
    -Log notes\harness\model-manager-entry3.log

.\scripts\x32dbg\model-manager-probe.ps1 -Mode Fn -Fn 0x622DF0 `
    -CampaignRow 3 -MissionRow 0 -UnlockCampaigns -SimSeconds 10 -Seconds 40 `
    -Log notes\harness\model-manager-fn-622df0.log
```

The entry logpoint is at `0x5f4e2a`; the Fn logpoint is at the discovered
`0x622DF0`. Both auto-continue (`; run`) and only log, so the park loads
normally. Two earlier iterations are kept as `model-manager-entry.log` and
`model-manager-entry2.log`; they captured the same vtables but read the
display fields from the wrong base (`ECX` is the manager, not the record), so
they are superseded by `entry3`.

### Entry results (88 calls, Cpg04 row 3, mission 0)

```
total=88
--- vt/fn ---
  71 00648A68 00622DF0
  17 00648AA0 006236C0
--- display/tag ---
  50 disp=0 tag=""            (44 Beam5 + 4 MiniCar + 2 Beam2)
  17 disp=0 tag="LunchBox"
  12 disp=3 tag="Waypoint"
   4 disp=3 tag="Tile"
   2 disp=3 tag="AnimalBait"
   2 disp=1 tag="Cage"
   1 disp=0 tag="Judge"
```

All 71 show-feature calls share manager `0x0B6D1F00` (vtable `0x648A68`) and
scene `0x0985A290`. Heap object addresses (manager, actor, scene) change per
run; the vtable and code addresses do not. The 17 LunchBox calls share one record block
(`0x098C8344`) but use 9 distinct manager objects (`0x0B6ACBB8`,
`0x0B6D4940`, `0x0B6DB440`, `0x0B94DE10`, `0x0E116528`, `0x0E203CD8`,
`0x0E2BBB38`, `0x0E37BDC8`, `0x0E567B60`); Cpg04 has 9 fences, so these are
likely fence-mounted LunchBox features (not proven by this probe).

The 10 distinct show record blocks are `098C5704, 098C5E64, 098C6974,
098C70D4, 098C851C, 098C86F4, 098C9964, 098CA29C, 098CA64C, 098CBE44`
(matching the 10 feature types in the Cpg04 show histogram).

Sample lines (verbatim):

```
MODELMGR|mgr=0B6D1F00|vt=00648A68|fn=00622DF0|rec=098C851C|disp=3|wteam=1|price=10|tagp=097A1A30|tag="Waypoint"|actor=0B657870|cls="Waypoint"|scene=0985A290|self=097E3AC8|t=645417093
MODELMGR|mgr=0B6D1F00|vt=00648A68|fn=00622DF0|rec=098CBE44|disp=3|wteam=0|price=300|tagp=034A00B8|tag="Tile"|actor=0E38FAA8|cls="Tile"|scene=0985A290|self=097E3AC8|t=645417109
MODELMGR|mgr=0B6D1F00|vt=00648A68|fn=00622DF0|rec=098C9964|disp=3|wteam=0|price=30|tagp=034A1188|tag="AnimalBait"|actor=0E2634F8|cls="AnimalBait"|scene=0985A290|self=097E3AC8|t=645417171
MODELMGR|mgr=0B6D1F00|vt=00648A68|fn=00622DF0|rec=098C5704|disp=1|wteam=0|price=800|tagp=034DBC20|tag="Cage"|actor=0B60EB30|cls="Cage"|scene=0985A290|self=097E3AC8|t=645417062
MODELMGR|mgr=0B6D1F00|vt=00648A68|fn=00622DF0|rec=098C5E64|disp=0|wteam=0|price=50|tagp=034D9378|tag="Judge"|actor=0B633190|cls="Judge"|scene=0985A290|self=097E3AC8|t=645417062
MODELMGR|mgr=0B6D1F00|vt=00648A68|fn=00622DF0|rec=098C70D4|disp=0|wteam=0|price=500|tagp=034D5650|tag=""|actor=0B634FF0|cls=""|scene=0985A290|self=097E3AC8|t=645417078
MODELMGR|mgr=0E203CD8|vt=00648AA0|fn=006236C0|rec=098C8344|disp=0|wteam=0|price=100|tagp=097A1A70|tag="LunchBox"|actor=0B907438|cls="LunchBox"|scene=0985A290|self=097E3AC8|t=645417031
```

### Fn results (71 calls at `0x622DF0`)

```
MODELCALL|mgr=0AED1F00|vt=00648A68|rec=098E5704|disp=1|wteam=0|price=800|tag="Cage"|actor=0AE0EB30|cls="Cage"|scene=0987A290|ret=005F4E2D|t=645466890
```

`ret=005F4E2D` is the instruction after the virtual call in `FUN_005f4e10`,
confirming the caller. The callee sees the record at `[esp+4]`, the actor at
`[esp+8]` and the scene at `[esp+0xc]`; the manager is `ECX`.

### Vtable dumps (image bytes, fixed image base)

`0x00648A68` (primary):

```
+0x00 00622C10   +0x04 005084B0   +0x08 00623390   +0x0c 00508D30
+0x10 00508580   +0x14 006235B0   +0x18 005ED2D0   +0x1c 005ED360
+0x20 005ECFB0   +0x24 00623380   +0x28 00622DF0   +0x2c 00623290
+0x30 005ED320   +0x34 006235E0   +0x38 00623690   +0x3c 005084B0
+0x40 005ED280   +0x44 00508D30   +0x48 00508580   +0x4c 00587340
+0x50 005ED2D0   +0x54 005ED360   +0x58 005ECFB0   +0x5c 005ED240
+0x60 006236C0   +0x64 00623880   +0x68 005ED320
```

`0x00648AA0` (primary for the LunchBox managers):

```
+0x00 00623690   +0x04 005084B0   +0x08 005ED280   +0x0c 00508D30
+0x10 00508580   +0x14 00587340   +0x18 005ED2D0   +0x1c 005ED360
+0x20 005ECFB0   +0x24 005ED240   +0x28 006236C0   +0x2c 00623880
+0x30 005ED320
```

`0x648AA0 = 0x648A68 + 0x38` and slot `0x648A68+0x38 = 0x623690 =
[0x648AA0]`, so the two tables are adjacent in `.rdata`; the runtime
`[manager]` value decides which one is the primary vtable.

## What `0x622DF0` does (the display_type 3 dispatcher)

`0x622DF0` is the factory body; `EDI` is the feature actor and
`ESI` is the manager. It reads the class tag string at
`[[actor+0x28]+0x28]` and dispatches (addresses above). For the ground-tile
classes the chain is:

```
Tile / Teleport:
  0x622EB1/0x622EE4  JNZ 0x6231AF
  0x6231AF           operator new(0x58)
  0x6231E9           CALL 0x622240      <- ground quad/UV builder

Waypoint:
  0x622F17           JZ 0x622FF0 (not waypoint)
  0x622F1D           operator new(0x7c)
  0x622F50           CALL 0x6240C0
  0x6240E1           CALL 0x622240      (base)

StaticBait:
  0x62301D           JZ 0x62306E (not static bait)
  0x62301F           operator new(0xB0)
  0x623060           CALL 0x623FA0
  0x623FC0           CALL 0x622240      (base)
  0x623FDE           object vtable = 0x648B04

AnimalBait:
  0x62309B           JZ 0x6230E9 (not animal bait)
  0x62309D           operator new(0x5C)
  0x6230DB           CALL 0x623F20
```

`0x622240` (`gatShowTileFeature.cpp`) is the flat-quad base for all display_type
3 classes. It stores its argument objects at `+0x50`/`+0x54` and builds the
quad from the placement rectangle at `[actor+0x04..0x10]` (centre =
`(x0+x1)*0.5`, `(y0+y1)*0.5` with the 0.5 constant at `0x0062544C`), calls
`0x61FFC0`, then runs the material/UV setup (`FUN_004693F0`, `FUN_00469610`).
For the static baits the `.bfi` footprint is 4/4 and the ground texture is
mesh slot 6 (`animaux.tga`); whether `0x622240` reads the record footprint
directly or the actor's rect is still to be pinned down (the rect is built
earlier by `FUN_00589020`). The exact UV rectangle is likewise still open.

## Open

1. **StaticBait placement path.** Cpg04's show has no `StaticBait` record, so
   `0x62301F -> 0x623FA0` is proven statically but not observed at runtime.
   The construction-kit owner's manager class is not captured: it may be the
   generic `0x648A68` table (show/world owner) or another table. A placement
   run of a StaticBait with the entry probe would settle it. The port branch
   itself is `0x622DF0` -> `0x62301F` -> `0x623FA0` -> `0x622240`.
2. **UV rectangle: resolved (2026-10-31 follow-up).** `0x622240` builds
   one quad per feature: width/height from `[actor+0x28]` (4x4), centre from
   the actor footprint, orientation from `actor+0x24`, Z from the scene
   transform. Vertices/UVs: `(-w/2,+h/2)` (0,1), `(-w/2,-h/2)` (1,1),
   `(+w/2,+h/2)` (0,0), `(+w/2,-h/2)` (1,0); triangles (v0,v1,v2) and
   (v1,v3,v2). Full texture, no atlas sub-rect. The port uses this exact
   vertex/UV order for the `display_type 3` quad.
3. **LunchBox owners.** The 17 LunchBox calls use 9 distinct manager objects;
   the fence-to-manager mapping is inferred from the 9 Cpg04 fences, not
   directly watched. The LunchBox vtable `0x648AA0`/`0x6236C0` checks the
   actor class "LunchBox" and builds a 0x138-byte object with `0x624670`;
   it does not enter the ground-tile branch.
4. The `.bfi` file `display_type` is at **0x21**; the runtime record stores it
   at **record+0x30** (record+0x28 is the price). The port's
   `BfiFeature.display_type` reads file 0x21, which is correct.

## Harness state

- No `DinoIsland.exe` / `headless.exe` process left after the runs.
- `run-test/save/dino.dat` was backed up and restored by the script
  (4-byte file; `-UnlockCampaigns`).
- Logs: `notes/harness/model-manager-entry.log` (first iteration),
  `model-manager-entry2.log`, `model-manager-entry3.log` (clean entry),
  `model-manager-fn-622df0.log` (fn call log), plus `.err` files.
- New harness script: `scripts/x32dbg/model-manager-probe.ps1` (`-Mode Entry`
  / `-Mode Fn -Fn <addr>`); virtual input only, background safe.
- Committed with the static-bait quad/UV update (`3f...`/this commit); the
  probe note is `notes/model-manager-vtable-probe.md` and the harness script
  `scripts/x32dbg/model-manager-probe.ps1`.
