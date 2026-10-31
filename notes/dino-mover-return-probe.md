# Dino mover return-address probe - which call site emits which record

Date: 2026-10-31. Retail `DinoIsland.exe` under the windowed x32dbg harness
(`run-test/`, virtual input, background-safe). Closes Open #1 of
`notes/dino-mover-runtime-probe.md`.

Status: **answered from the return address.** The per-frame `0x34`/`0x35`
markers and the `0x27 + 0x20 + 0x20` lunge profile come from **different
functions**: the markers from the mover tick step `FUN_00574450`, the lunge
from the move-profile case 0 builder `FUN_005741c0` (called only from
`0x56ef84`). The item-element sites `0x56c65f`/`0x56c70e` do not fire in a
60 s window but do fire once a dino reaches an item (~112 s into Cpg04), and
the same window kills the old "emitter never runs / no Eat record" claim.

## Commands

```powershell
# 1. the note's command, unchanged
.\scripts\x32dbg\start-campaign.ps1 -CampaignRow 3 -Seconds 30 -UnlockCampaigns `
    -MoverTrace -SimSeconds 60 -MoverHotMs 2000 `
    -Log notes\harness\dino-mover-return.log

# 2. supplementary longer window (same harness, only the window changed)
.\scripts\x32dbg\start-campaign.ps1 -CampaignRow 3 -Seconds 30 -UnlockCampaigns `
    -MoverTrace -SimSeconds 180 -MoverHotMs 2000 `
    -Log notes\harness\dino-mover-return-180.log
```

Both loaded the Cpg04 park (`FENCE this=...x0=0x1c4,y0=0x138,x1=0x1d3,y1=0x157`),
printed `mover trace: recorded a 2000 ms mover-tick window`, and exited without
leaving a process. No focus was stolen and no physical input was used.

## Logpoint

`ret={p:[esp]}` is already in `HEAD` (`2f37f4b`, 2026-10-31 03:35) for both
`MOVSTART` (`0x573f40`) and `MOVAQUEUE` (`0x573dc0`), together with every
existing field; `git status` shows `scripts/x32dbg/start-campaign.ps1`
identical to `HEAD`. No edit was needed for this probe and no other trace mode
was touched.

The logged value is the **return** address. The call instruction is
`ret - 5` (x86 `call rel32`), which is how the addresses below map to the
call-site list in `dino-mover-runtime-probe.md`.

## Logs

| run | log | size | records | window | frame rate (2 s `MOVTICK`) |
| --- | --- | ---: | ---: | ---: | --- |
| 60 s (note's command) | `notes/harness/dino-mover-return.log` | 759 KB | 673 (575 MOVSTART + 98 MOVAQUEUE) | 65.6 s | 1098 hits = **27.5 fps** |
| 180 s (supplementary) | `notes/harness/dino-mover-return-180.log` | 4.35 MB | 6934 (6524 + 410) | 185.5 s | 1352 hits / 2.484 s = **27.2 fps** |

Both logs are gitignored. Analysis scripts (gitignored):
`notes/harness/parse_mover_return.py`, `marker_vs_tick.py`,
`marker_series.py`, `profile_durations.py`; full output in
`notes/harness/*.analysis.txt`.

The 60 s window contains the gestures and turns but almost no walking, so it
cannot show the repeated marker on its own. The 180 s window reaches the same
walking phase as the old 2.2 MB log (run3) and reproduces its long series; it
is the run that answers the per-frame question.

## Counts per return address

### 60 s run (the note's exact command)

| tag | call | ret | function | total | kinds | target==pos | target!=pos |
| --- | --- | --- | --- | ---: | --- | ---: | ---: |
| MOVAQUEUE | 0x5742fe | 0x574303 | FUN_005741c0 (move-profile case 0) | 42 | 0x20=42 | 0 | 42 |
| MOVAQUEUE | 0x574337 | 0x57433c | FUN_005741c0 (move-profile case 0) | 42 | 0x20=42 | 42 | 0 |
| MOVAQUEUE | 0x56f14b | 0x56f150 | FUN_0056eec0 case body | 14 | 0x1e=14 | 14 | 0 |
| MOVSTART | 0x56f07d | 0x56f082 | FUN_0056eec0 case 1 | 347 | 0x19=347 | 347 | 0 |
| MOVSTART | 0x5741ee | 0x5741f3 | FUN_005741c0 (move-profile case 0) | 42 | 0x27=42 | 42 | 0 |
| MOVSTART | 0x56fc94 | 0x56fc99 | FUN_0056fb70 | 41 | 0x09=1, 0x0a=2, 0x0b=6, 0x0c=7, 0x0d=12, 0x0e=8, 0x0f=5 | 0 | 41 |
| MOVSTART | 0x56fa93 | 0x56fa98 | move-profile case body (no Ghidra function) | 32 | 0x12=6, 0x13=11, 0x14=13, 0x16=1, 0x17=1 | 32 | 0 |
| MOVSTART | 0x56f94a | 0x56f94f | move-profile case body (no Ghidra function) | 19 | 0x35=19 | 0 | 19 |
| MOVSTART | 0x56f9fd | 0x56fa02 | move-profile case body (no Ghidra function) | 15 | 0x00=15 | 15 | 0 |
| MOVSTART | 0x56f130 | 0x56f135 | FUN_0056eec0 | 14 | 0x1d=14 | 14 | 0 |
| MOVSTART | 0x57466d | 0x574672 | FUN_00574450 (mover tick step) | 12 | 0x34=12 | 0 | 12 |
| MOVSTART | 0x575b79 | 0x575b7e | FUN_00575ac0 | 12 | 0x34=7, 0x35=5 | 0 | 12 |
| MOVSTART | 0x56f0ff | 0x56f104 | FUN_0056eec0 | 12 | 0x1c=12 | 12 | 0 |
| MOVSTART | 0x56f1bb | 0x56f1c0 | FUN_0056eec0 case body | 12 | 0x35=12 | 0 | 12 |
| MOVSTART | 0x56fdce | 0x56fdd3 | FUN_0056fb70 | 9 | 0x00=9 | 9 | 0 |
| MOVSTART | 0x574ffa | 0x574fff | FUN_00574ad0 | 4 | 0x00=4 | 4 | 0 |
| MOVSTART | 0x56e799 | 0x56e79e | FUN_0056e670 | 2 | 0x00=2 | 2 | 0 |
| MOVSTART | 0x57469c | 0x5746a1 | FUN_00574450 (mover tick step) | 1 | 0x35=1 | 0 | 1 |
| MOVSTART | 0x5725bc | 0x5725c1 | FUN_005724d0 | 1 | 0x15=1 | 1 | 0 |

Zero: `0x56f98a` (ret `0x56f98f`), `0x56c65f` (`0x56c664`), `0x56c70e`
(`0x56c713`), `0x5209d6` (`0x5209db`), `0x520a91` (`0x520a96`), `0x520b85`
(`0x520b8a`); also `0x569660` (`EMIT|` = 0).

### 180 s run (supplementary)

| tag | call | ret | function | total | kinds | target==pos | target!=pos |
| --- | --- | --- | --- | ---: | --- | ---: | ---: |
| MOVAQUEUE | 0x5742fe | 0x574303 | FUN_005741c0 (move-profile case 0) | 135 | 0x20=135 | 0 | 135 |
| MOVAQUEUE | 0x574337 | 0x57433c | FUN_005741c0 (move-profile case 0) | 135 | 0x20=135 | 135 | 0 |
| MOVAQUEUE | 0x56c42f | 0x56c434 | FUN_0056c3b0 (item executor finish) | 69 | 0x1b=69 | 69 | 0 |
| MOVAQUEUE | 0x56f14b | 0x56f150 | FUN_0056eec0 case body | 48 | 0x1e=48 | 48 | 0 |
| MOVAQUEUE | 0x56c455 | 0x56c45a | FUN_0056c3b0 (item executor finish) | 23 | 0x24=23 | 23 | 0 |
| MOVSTART | 0x57469c | 0x5746a1 | **FUN_00574450 (mover tick step)** | **2561** | 0x35=2561 | 0 | 2561 |
| MOVSTART | 0x57466d | 0x574672 | **FUN_00574450 (mover tick step)** | **1953** | 0x34=1953 | 0 | 1953 |
| MOVSTART | 0x56f07d | 0x56f082 | FUN_0056eec0 case 1 | 1346 | 0x19=1346 | 1346 | 0 |
| MOVSTART | 0x5741ee | 0x5741f3 | FUN_005741c0 (move-profile case 0) | 135 | 0x27=135 | 135 | 0 |
| MOVSTART | 0x56fc94 | 0x56fc99 | FUN_0056fb70 | 86 | 0x09=3, 0x0a=1, 0x0b=16, 0x0c=9, 0x0d=23, 0x0e=12, 0x0f=22 | 0 | 86 |
| MOVSTART | 0x56fa93 | 0x56fa98 | move-profile case body (no Ghidra function) | 64 | 0x12=24, 0x13=22, 0x14=16, 0x16=1, 0x17=1 | 64 | 0 |
| MOVSTART | 0x575b79 | 0x575b7e | FUN_00575ac0 | 57 | 0x34=38, 0x35=19 | 0 | 57 |
| MOVSTART | 0x56f94a | 0x56f94f | move-profile case body (no Ghidra function) | 54 | 0x35=54 | 0 | 54 |
| MOVSTART | 0x56f130 | 0x56f135 | FUN_0056eec0 | 48 | 0x1d=48 | 48 | 0 |
| MOVSTART | 0x56f0ff | 0x56f104 | FUN_0056eec0 | 43 | 0x1c=43 | 43 | 0 |
| MOVSTART | 0x56f1bb | 0x56f1c0 | FUN_0056eec0 case body | 34 | 0x35=34 | 0 | 34 |
| MOVSTART | 0x56f9fd | 0x56fa02 | move-profile case body (no Ghidra function) | 33 | 0x00=33 | 33 | 0 |
| MOVSTART | 0x569770 | 0x569775 | FUN_00569660 (element->move emitter) | 24 | **0x08=24** | 0 | 24 |
| MOVSTART | 0x574ffa | 0x574fff | FUN_00574ad0 | 21 | 0x00=21 | 21 | 0 |
| MOVSTART | 0x56c70e | 0x56c713 | item element path (0x56c490 region) | 21 | 0x00=21 | 21 | 0 |
| MOVSTART | 0x56fdce | 0x56fdd3 | FUN_0056fb70 | 20 | 0x00=20 | 20 | 0 |
| MOVSTART | 0x56c65f | 0x56c664 | item element path (0x56c490 region) | 18 | 0x35=18 | 0 | 18 |
| MOVSTART | 0x56e799 | 0x56e79e | FUN_0056e670 | 3 | 0x00=3 | 3 | 0 |
| MOVSTART | 0x5725bc | 0x5725c1 | FUN_005724d0 | 3 | 0x15=3 | 3 | 0 |

Zero: `0x56f98a` (`0x56f98f`), `0x5209d6` (`0x5209db`), `0x520a91`
(`0x520a96`), `0x520b85` (`0x520b8a`).

## The four requested kinds

### 60 s run

| kind | total | sites (count) | target==pos |
| --- | ---: | --- | ---: |
| `0x34` | 19 | `0x574672` 12 (actk=1), `0x575b7e` 7 | 0 |
| `0x35` | 37 | `0x56f94f` 19, `0x56f1c0` 12, `0x575b7e` 5, `0x5746a1` 1 (actk=2) | 0 |
| `0x27` | 42 | `0x5741f3` 42 | **42** |
| `0x20` | 84 | `0x574303` 42 (lunge, target!=pos), `0x57433c` 42 (return, target==pos) | 42 |

### 180 s run

| kind | total | sites (count) | target==pos |
| --- | ---: | --- | ---: |
| `0x34` | 1991 | `0x574672` 1953 (actk=1), `0x575b7e` 38 (actk=1) | 0 |
| `0x35` | 2686 | `0x5746a1` 2561 (actk=2), `0x56f94f` 54, `0x56f1c0` 34, `0x575b7e` 19 (actk=2), `0x56c664` 18 (item) | 0 |
| `0x27` | 135 | `0x5741f3` 135 | **135** |
| `0x20` | 270 | `0x574303` 135 (lunge), `0x57433c` 135 (return) | 135 |

Every `0x27` is followed by exactly one lunge `0x20` and one return `0x20`
(135/135/135). In 96 of 135 groups all three land in the same millisecond; in
the rest the 15 ms `GetTickCount` boundary splits the group (the records
themselves are back-to-back in the same function).

Durations (`notes/harness/profile_durations.py`): `0x27` = 2.000 s in all
42/135 records; lunge `0x20` = 1.5 s or 2.0 s; return `0x20` = 5-20 s
(60 s sample: 5, 10, 11, 14 s; 180 s sample: 8, 12, 15, 20 s). That matches
the decompiled `profile+0x23c` and `rand()%16 + 5.0`.

## The per-frame re-issue is `FUN_00574450`

Longest same-target run per return address (180 s, gap <= 100 ms):

| ret | dino | records | longest run | median dt |
| --- | --- | ---: | ---: | ---: |
| `0x5746a1` (0x35) | 0x0ce4b090 | 1837 | 851 | **15 ms** |
| `0x574672` (0x34) | 0x0cf7d148 | 1293 | **1273** | **15 ms** |
| `0x5746a1` (0x35) | 0x0b4d1b98 | 719 | 719 | **15 ms** |
| `0x574672` (0x34) | 0x0d4620c8 | 477 | 476 | **15 ms** |

No other return address has a run longer than 1. So the note's "re-issued
every frame" series is the mover tick's own record re-publication, not the
action layer:

```text
FUN_00574350 (mover tick, 0x574350)
  -> FUN_00574450 (0x574450, step)
       actk (mover+0x1f4) == 1 -> FUN_00573f40(mover+0xc, cell, 0, mode 1, 0x34, 1, 1)   @0x57466d
       actk == 2               -> FUN_00573f40(mover+0xc, cell, 0, mode 1, 0x35, 1, 1)   @0x57469c
       -> FUN_00575ac0 (0x575ac0) -> 0x34 or 0x35 @0x575b79
```

This is not literally every mover frame: a dino held `actk==1` for all 58
frames of the 2 s `MOVTICK` window yet emitted 3 markers in 65 s
(`notes/harness/marker_vs_tick.py`). The record is re-issued while the
cell-crossing branch in `FUN_00574450` holds, which can persist for hundreds
of consecutive frames (the 1273/851/719 runs above) but is not unconditional.
The old log's long runs have `actk==1` (912 and 115 `0x34`) and `actk==2`
(1077 and 644 `0x35`), exactly the two branches.

The other `0x35` sites are occasional: `0x56f94f`/`0x56f1c0` are action-layer
move-profile case bodies (54 and 34 records, actk mixed 0x00-0x39, longest
run 1) and `0x56c664` is the item executor (18 records during eating).

## The lunge profile is `FUN_005741c0`, called from `0x56ef84`

`list_xrefs(0x5741c0)` returns exactly one call: `0x56ef84`, which is the
harness's `MP0` case entry of the move-profile dispatcher (`-MoveProfile`).
The decompiled body is the whole profile:

```c
// FUN_005741c0(dino, Vec2* lunge_target)
FUN_00573f40(iVar1, iVar1, *(...+0x1a8)+600), 2, 0x27, 0, 0);
//   ^ 0x27, mode 2, target == mover+0xc (position), allowTurn = 0, instant = 0
... local_8 = 0x34; local_10 = 0.0; local_4 = 1; local_c = 1; local_b = 1;
FUN_00575e50(&local_18);          // direct queue append, NOT through 0x573f40/0x573dc0
FUN_00573dc0(lunge_target, lunge_target, *(...+0x1a8)+0x23c, 2, 0x20, 0, 0);
FUN_00573dc0(lunge_target, mover+0xc, rand()%16 + 5.0, 2, 0x20, 0, 1);
```

Measured: the `0x27` is always duration **2.0 s** (the `+0x1a8+600` slot),
position-targeted, `allowTurn=0`; the first `0x20` is duration **1.5/2.0 s**
(`+0x23c`) to the lunge point; the return `0x20` is **5-20 s**
(`rand()%16 + 5.0`). The return target is the dino's position by
construction. A `0x34` record is appended directly inside the same function
with `FUN_00575e50`, so it does **not** appear as a `MOVSTART`/`MOVAQUEUE`
hit; the trace's `0x27 + 0x20 + 0x20` trio is the visible part of the profile.

## Item elements and Eat: window length, not a dead path

The 60 s window has `0x56c65f`/`0x56c70e` = 0 and `EMIT|` = 0, which is what
the old note recorded. The 180 s window reaches the feeding phase:

| event | 60 s | 180 s | first occurrence |
| --- | ---: | ---: | --- |
| `0x569660` (`EMIT|`) | 0 | **24** | ~112 s into the window |
| `0x569770` kind `0x08` (Eat) | 0 | **24** | t=636591859 |
| `0x580e60` (`SETLEVEL`) | 0 | **7** | - |
| `0x522300` (`EATSM`, state 2) | 144 | 335 | - |
| item `0x56c65f` (`0x56c664`, kind 0x35) | 0 | **18** | t=636588343 |
| item `0x56c70e` (`0x56c713`, kind 0x00) | 0 | **21** | - |
| `0x56c42f`/`0x56c455` queue (`0x1b`/`0x24`) | 0 | 69 / 23 | - |

Reading: the old probe's "no dino Eat record, `SetLevel` never fires, emitter
never runs" is a **60 s sampling artifact**. In Cpg04 the dinos do not reach
an item until about two minutes into the mission. The item-element call sites
are real and active during that phase; they are not the walk/run re-issue
path, though (`0x56c664` has 18 records total, longest run 1).

## Sample lines

`0x27` profile start, 60 s log (dino 0x0f6e20c8, t=636121093):

```text
MOVSTART|t=636121093|mv=0F6E2390|dino=0F6E20C8|id=-1163067382|act=31|cd=00000000|food=444C999C|st=0|actk=0|pend=0|px=43D5D000|py=44047A97|fx=BEDE0108|fy=BF66AECC|qb=0F6E2610|qe=0F6E2610|walk=402658A9|run=412658A9|a1=0F6E239C|a2=0F6E239C|ax=43D5D000|ay=44047A97|a3=40000000|a4=00000002|a5=00000027|a6=00000000|a7=00000000|ret=005741F3
```

Lunge and return queued in the same millisecond (`0x20` twice):

```text
MOVAQUEUE|t=636121093|mv=0F6E2390|dino=0F6E20C8|...|a2=0019FD60|ax=43D50000|ay=44020000|a3=40000000|a4=00000002|a5=00000020|a6=00000000|a7=00000000|ret=00574303
MOVAQUEUE|t=636121093|mv=0F6E2390|dino=0F6E20C8|...|a2=0F6E239C|ax=43D5D000|ay=44047A97|a3=41300000|a4=00000002|a5=00000020|a6=00000000|a7=00000001|ret=0057433C
```

(`0x41300000` = 11.0 s return duration; the second record returns to the
dino position.)

Per-frame walk/run re-issue, 180 s log:

```text
MOVSTART|t=636485640|mv=0D4F4CA0|dino=0D4F49D8|...|st=1|actk=1|pend=1|px=4403A417|py=43C0C0C3|...|ax=4402C000|ay=43BD8000|a3=00000000|a4=00000001|a5=00000034|a6=00000001|a7=00000001|ret=00574672
MOVSTART|t=636483921|mv=0D4BEAD0|dino=0D4BE808|...|st=1|actk=2|pend=2|px=4401224E|py=43BC0E86|...|ax=44010000|ay=43BC0000|a3=00000000|a4=00000001|a5=00000035|a6=00000001|a7=00000001|ret=005746A1
```

Eat record, 180 s log (kind `0x08`, `ret=00569775` = call `0x569770` inside
`FUN_00569660`):

```text
MOVSTART|t=636591859|mv=0B4D1E60|dino=0B4D1B98|id=-1163067389|act=31|cd=00000000|food=4415E5C2|st=1|actk=2|pend=2|px=43F12998|py=43C00000|fx=3F800000|fy=00000000|qb=0B49A4F0|qe=0B49A508|walk=40201CB9|run=41201CB9|a1=0B4D1E6C|a2=0019FC0C|ax=43F48000|ay=43C08000|a3=40000000|a4=00000002|a5=00000008|a6=00000000|a7=00000001|ret=00569775
```

## Static cross-check

`list_xrefs(0x573f40)` in Ghidra returns **24** callers: the 16 in the note,
six the note did not list, and the two `FUN_00569660` emitter sites
(`0x569770`/`0x569841`). The six missing:

| call | caller | observed in 180 s |
| --- | --- | ---: |
| `0x5741ee` | FUN_005741c0 | 135 (`0x27`) |
| `0x57466d` | FUN_00574450 | 1953 (`0x34`) |
| `0x57469c` | FUN_00574450 | 2561 (`0x35`) |
| `0x575b79` | FUN_00575ac0 | 57 (`0x34`/`0x35`) |
| `0x574ffa` | FUN_00574ad0 | 21 (`0x00`) |
| `0x5725bc` | FUN_005724d0 | 3 (`0x15`) |

The two `FUN_00569660` sites fire only on the item/Eat path (`0x08` = 24).
The item sites `0x56c65f`/`0x56c70e` are in the note's list and in the xref
list; they are simply not hit until a dino uses an item.

## Port implication

- **Do not add a separate action-layer `0x34`/`0x35` emitter.** Retail's
  re-issue is the mover tick itself (`FUN_00574350` -> `FUN_00574450`): while
  the current record kind is 1 (walk) or 2 (run), it re-queues the same cell
  record as `0x34`/`0x35` at ~15 ms. The port's `DinoMover::tick` /
  `publish_activity` is the right home; `0x34`/`0x35` are walk/run aliases,
  not a "jump" kind (the `dino_motion.h` comment "0x34/0x35 jump" should be
  corrected when code is next touched).
- **The lunge is one atomic profile**, not a single record: `0x27` (2 s, at
  the dino position, `allowTurn=0`) + a direct-queued `0x34` + `0x20` to the
  lunge point (`profile+0x23c`, 1.5/2.0 s) + `0x20` back to the position
  (`rand()%16+5` s). `FUN_005741c0` is entered only from the move-profile
  dispatcher case 0 (`0x56ef84`), so the port's attack/lunge action should
  produce the same 3-record group, including the return leg.
- **Keep the item/Eat path.** `0x569660` and the item executor do run in
  Cpg04 once a dino reaches food (~112 s). The 60 s negative in
  `notes/dino-mover-runtime-probe.md` section 6 must not be used to delete
  the Eat kind or the item-element emitter.
- **Ported (2026-10-31):** at the meal the port now queues the observed
  kind-8 record (`mode 2`, `family_duration(8, 2.0)`, target = the feeder) in
  `ParkSession::tick_dino`, matching the 180 s `ret=0x569775` record.

## Harness state / stale lock

- Before the probe, an abandoned run from another lane was still alive:
  PowerShell PID 46832 (`-GenotypeTrace -Seconds 5`, started 02:30) ->
  `headless.exe` PID 44164 -> `DinoIsland.exe` PID 4300. The debuggee was
  stopped at a breakpoint, `Responding=False`, CPU flat for ~4 h, with the
  game window foreground and `run-test/` held. Its log was never written
  (`notes/harness/x32dbg-ctor.out.log` absent); `build\ctor-trace-harness.log`
  ends at `shot campaign-running`. Stopped by exact PID (46832, then 44164;
  4300 exited with the debugger). No other DinoIsland/headless process has
  run since.
- `run-test/dino_input.txt` contained a stale `322 402 0` (the abandoned
  briefing click); truncated to empty before both runs.
- `run-test/save/dino.dat` was 4 bytes (`8,0,0,0`) before and after; the
  harness backup/restore completed (`LastWriteTime` = run end).
- No build directory was created or modified; no source files, no commits.

## Open

1. The 60 s window did not contain a walking phase, so the old note's
   per-frame series could not be reproduced there; the 180 s window did.
   A future probe can shorten the Eat wait, but the return-address attribution
   is already unambiguous.
2. `0x56f98a` and the three `FUN_00520880` sites (`0x5209d6/0x520a91/0x520b85`)
   never fired in either window; they are not on the Cpg04 park-dino path.
3. The `0x56eec0`-`0x56fa9x` region has no Ghidra function symbol; the
   "move-profile case body" label is from the harness `-MoveProfile` map
   (`0x56ef84/0x56f044/0x56f0d3/0x56f130/0x56f02c`), not a decompiled
   function header.
