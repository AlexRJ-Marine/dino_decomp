# Move-record action mapping - FUN_0056eec0 dispatcher (Cpg04)

Date: 2026-10-31. Retail `DinoIsland.exe`, windowed x32dbg harness (`run-test/`,
virtual input, background-safe). Raw log (gitignored):
`notes/harness/dino-move-profile.log` (2.4 MB, 60 s of running park; the probe
also ran once as `notes/harness/dino-move-profile-run1.log`).

## Command

```powershell
.\scripts\x32dbg\start-campaign.ps1 -CampaignRow 3 -Seconds 30 -UnlockCampaigns `
    -MoverTrace -MoveProfile -SimSeconds 60 -MoverHotMs 2000 `
    -Log notes\harness\dino-move-profile.log
```

`-MoveProfile` (added to `scripts/x32dbg/start-campaign.ps1`) installs
`0x56c0c0` element registration plus the five `FUN_0056eec0` case entry points;
`-MoverTrace` installs the queue/turn probes and dismisses the briefing with
`Esc` (no in-game clicks).

## Answer in one line

`[profile+0x60]` **is the `.bai` `special_action` field** (1..6), the dispatcher
case is `special_action - 1`, and `effect_class` does **not** predict it. Only
the six special actions reach this dispatcher; every normal action
(`special_action == 0`) falls through at `0x56f1c7` and never emits here.

## The chain (verified statically and at runtime)

`FUN_0056eec0(EBX = behaviour)`:

```text
[EBX+0x18]..[EBX+0x1c]   vector of active elements; ESI = the last one
[ESI+0x14]               action object (vtable 0x0064302C), +0x28 = saDinosaur
[ESI+0x10]               element state: 0 = start/emit, 3 = execute/unregister
[ESI+0xc]                element record; +4 = move profile (0xac-stride record)
[[[ESI+0xc]+4]+0x60]     dispatcher value  -> DEC -> jump table 0x56f1e0
```

`0x56c0c0` (`REGACT`, `__thiscall` on the behaviour) registers an element with
`param_2` = the action object, `param_3` = the element record and stores
`param_3+4` = the profile. Its caller is `FUN_0056e670` (the action selector,
`ret=0x56E6E3`), so every registration follows a chooser decision.

Observed `REGACT` field: `[element+4]` is a pointer to the 0xac-stride action
record, and `[record+0x60]` takes the values **1, 2, 3, 4** in this park.

## Why +0x60 is `special_action`

The three single-instance profiles sit at consecutive 0xac-stride addresses:

| `+0x60` | profile pointer (run 1 / run 2) | offset from the first |
| ---: | --- | ---: |
| 1 | `0x097C75B0` / `0x097975B0` | +0 |
| 2 | `0x097C765C` / `0x0979765C` | +0xAC |
| 3 | `0x097C7708` / `0x09797708` | +0x158 |

`ActionList.ini` has 64 entries; the only run of `special_action` values
`1, 2, 3` is indices 0-2 (`Exploration`, `RunAway`, `Sleep`), and those are
exactly the actions whose `.bai` files carry `special_action` 1/2/3
(`tools/formats/bai.py` already documents `special_action` as runtime `+0x60`).
The remaining observed value, 4, is `special_action` 4 = `DinoAttackForHerbivore`
(27) / `DinoAttackForCarnivore` (28) - both are per-instance records (10
distinct pointers in the log), which is why they do not sit in the static run.

`effect_class` is **not** the field: `Exploration`, `RunAway`, `Sleep` and the
`DinoAttack` pair all have `effect_class == 0`, yet their `+0x60` is 1/2/3/4.
(`effect_class` is 5 for the jump family and 1/2/3 for KickTheirAss /
MineExplosion / Teleport, none of which appear in this dispatcher's value set.)

## Action -> case -> emitted records

`REGACT p60` counts over 60 s (per frame while the element is selected, so these
are selection frames, not action starts): `1 -> 45`, `2 -> 1453`, `3 -> 17`,
`4 -> 73`.

| `+0x60` | action (id) | case | element state | records emitted (kind, mode, duration, target) | log hits |
| ---: | --- | --- | ---: | --- | ---: |
| 1 | **Exploration** (0) | 0 `0x56ef12` | 0 | `0x27` mode 2 **2.0 s** target=self; `0x27` mode 2 **2.5 s** target=action point (inline push, `X[0x258]+0.5`); `0x34` mode 1 target=action point; `0x20` mode 2 **1.5 s / 2.0 s** (`X[0x23c]`; 25x 1.5, 20x 2.0) target=action point; `0x20` mode 2 **5.0-20.0 s** (random) target=self | 45 lunges |
| 2 | **RunAway** (1) | 1 `0x56ef8e` | 0 | either `0x19` mode 2 **2.0 s** target=self, or `0x35` mode 1 duration 0 target=steering cell | 189 + 30 |
| 3 | **Sleep** (2) | 2 `0x56f087` | 3 | `0x1c` mode 2 **1.5 s** target=self | 14 |
| 3 | **Sleep** (2) | 2 `0x56f087` | 0 | `0x1d` mode 2 **1.5 s** target=self **and** `0x1e` mode 2 duration `0x7f7fc99e` target=self | 17 + 17 |
| 4 | **DinoAttackForHerbivore/Carnivore** (27/28) | 3 / 4 `0x56f1c7` | any | **none** - the jump table goes straight to the epilogue | 73 regs |
| 5 | `ObstAttackSize1-4`, `LinkObstAttackSize1-4` (26, 56-61, 48, 59-61) | 4 `0x56f1c7` | any | none here; their gestures come from `FUN_0056fb70` (`ret=0x56FC99`, kinds `0x0b..0x0f` = the `special_action == 5` idle/random arm) | not hit |
| 6 | **Victory** (3) | 5 `0x56f152` | 0 / 3 | context gesture with duration `[0x006430A8]` = `0x7f7fc99e` | not hit |

Case 0's `FUN_005741c0` was verified statement by statement: it emits
`0x27` through `0x573f40`, then pushes a second `0x27` record by hand with
`X[0x258]+0.5`, then a `0x34` marker through `FUN_00575e50`, then two `0x20`
records through `0x573dc0` (one `X[0x23c]`, one `rand()%16 + 5.0`).

Raw examples:

```text
REGACT|t=616955812|beh=0B2BB6B8|act=0EA557A0|el=0B567D88|p4=00000000|p5=0|dino=0E8A20C8|el4=097975B0|p60=1|edi=0E986BE8|node=0E986BE0|n38=0E16EE48|n8=BAADF001|nc=00000006|n2c=0E986D08|n30=0E986D08|a0=0064302C|a4=0B2BB65C|ret=0056E6E3
MP0|t=616955828|dino=0E8A20C8|esi=0B2F2A80|ebx=0B2BB6B8|act=0EA557A0|state=0|el=0B567D88|el4=097975B0|p60=1|vb=0AFFA398|ve=0AFFA39C
MOVSTART|t=616955828|mv=0E8A2390|dino=0E8A20C8|...|a1=0E8A239C|a2=0E8A239C|ax=43D5D000|ay=44047A97|a3=40000000|a4=00000002|a5=00000027|a6=00000000|a7=00000000|ret=005741F3
MP4|t=616963968|dino=0E28B090|esi=0E747D18|ebx=0E3CBB90|act=0E930110|state=0|el=0B500C38|el4=0979765C|p60=2|vb=0E9880A0|ve=0E9880BC
MOVSTART|t=617005859|...|a3=40000000|a4=00000002|a5=00000019|a6=00000000|a7=00000000|ret=0056F082
MOVSTART|t=616971859|...|a3=3FC00000|a4=00000002|a5=0000001D|a6=00000000|a7=00000000|ret=0056F135
```

`a3` is the duration (`40000000` = 2.0 s, `3FC00000` = 1.5 s, `7F7FC99E` = the sentinel),
`a4` the mode, `a5` the kind, `a6` the instant flag, `a7` allowTurn.

## Emission sites seen in the same 60 s window

Grouped by return address at `0x573f40`/`0x573dc0` (2072 records):

| ret | owner | kinds (count) |
| --- | --- | --- |
| `0x5746A1` / `0x574672` | `FUN_00574450` blocked re-path | `0x35` (1442) / `0x34` (6) |
| `0x56F082` | case 1 (RunAway) | `0x19` (189) |
| `0x5741F3` / `0x574303` / `0x57433C` | case 0 lunge `FUN_005741c0` | `0x27` (45), `0x20` (45), `0x20` (45) |
| `0x56FC99` | `FUN_0056fb70`, `special_action == 5` arm | `0x09` (2), `0x0b` (13), `0x0c` (7), `0x0d` (10 + 3), `0x0e` (7 + 4), `0x0f` (6 + 8) |
| `0x56FA98` | `FUN_0056f9xx` element actions | `0x12` (8), `0x13` (24), `0x14` (14), `0x16` (1), `0x17` (2) |
| `0x56F1C0` | case 1 run-marker branch | `0x35` (30) |
| `0x56F94F` | walk-to-cell before an element action | `0x35` (18) |
| `0x575B7E` | `FUN_00575ac0` re-path | `0x34`/`0x35` (3/15) |
| `0x56F104` / `0x56F135` / `0x56F150` | case 2 (Sleep) | `0x1c` (14) / `0x1d` (17) / `0x1e` (17) |
| `0x56FA02` / `0x56FDD3` / `0x574FFF` / `0x56E79E` | cancels (`mode 0`) | `0x00` (13/10/9/1) |
| `0x5725C1` | periodic AI pass | `0x15` (2) |

So `FUN_0056eec0` is **not** where ordinary actions get their move records; the
`0x34/0x35` walk/run markers are re-issued every frame by the mover re-path
(`FUN_00574450`), and element actions go through `FUN_0056f9xx` /
`FUN_0056fb70`.

## Notes / corrections

- `[ESI+0x14]` is **not** an action id: it is an action object (vtable
  `0x0064302C`) whose `+0x28` is the `saDinosaur`. The action identity has to
  come from the profile pointer (`[element+0xc]+4 = [ESI+0xc]+4`), or from the
  registration call in `FUN_0056e670`.
- `REGACT` fires per frame while an element is selected, not once per action
  start, so its counts are selection frames (`p60=2` 1453 vs 219 actual
  emissions).
- `state == 3` does not emit for case 0/1 (it runs `FUN_00569a00` +
  `FUN_0056c260`, i.e. execute/unregister); for case 2 it is the only branch
  that emits `0x1c`.
- The `0x7f7fc99e` duration on `0x1e` (Sleep) and on the case-5 context gesture
  is the mover sentinel, not a real 3.4e38 s timeout: such a record only ends on
  arrival.

## Open

1. Separate `DinoAttackForHerbivore` (27) from `DinoAttackForCarnivore` (28):
   both are `special_action == 4` and both take the no-move case, so `+0x60`
   cannot distinguish them. The action object (`[ESI+0x14]`) differs per
   instance; its vtable is shared, so a field inside it or the chooser call
   site would be needed.
2. `Victory` (case 5) and the `ObstAttack` group (case 4) did not run in this
   mission window; their record lists are unverified at runtime.
3. Whether the two `0x20` records of the case-0 lunge are a "look around then
   return" or a generic profile list; the durations (fixed `X[0x23c]`, then
   random 5-21 s) suggest a "linger" tail.
