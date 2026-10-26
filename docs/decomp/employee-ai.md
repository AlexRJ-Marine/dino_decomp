# Employee AI and task loop

Status: **decompiled**; the job queue is the hourly feeding schedule, only
Nutritionists are hired, and "kind `0xb`" is the target object's definition
category (a food source), not a job id. The only open points are the 75 %
work-animation flag and the `0xb`/`LunchDoor` category overlap.

## Behaviour

`apEmployeeBehavior::Update` (`0x005220e0`) is the employee counterpart of the
visitor update. It references the employee actor at `+0x0c` and a task manager
at `+0x38`.

| Actor state | Update |
| ---: | --- |
| 0 | take the next job (`0x005224c0`) |
| 1 | travel (`0x00521490`) |
| 2 | work/use the target (`0x005219f0` + `0x00522390`) |

`FUN_00522300` performs transitions; on work completion it clears the current
job and calls `0x00580e60(target)`.

## Job queue

The task manager keeps `+0x48` (job queue), `+0x4c` (one-frame changed flag),
`+0x54` (owner), `+0x58` (target) and `+0x5c` (unused/parameter). The queue
stores 0xc-byte entries **`{u32 owner, u32 target, u32 hour}`** — there is no
job-type enum and no `id` field.

**Only producer:** `FUN_005953f0`, the virtual `+0x0c` of the kind-8 task
manager (the "Farm" park object). Every frame it checks the new-hour byte
`[time+0x28]`, clears the queue at end-of-day `[time+0x29]`, iterates its
registered `{owner, food source}` vector at `+0xd8..+0xdc`, and pushes
`{owner, fs, hour}` at `0x5954a4`. `FUN_0058c450` supplies the hour slot.

Registration: `FUN_0058b5b0` walks every food source in
`spSimuManager+0x128..+0x12c`, takes `[fs+0xc8]` (the owning paddock), finds
the nearest kind-8 manager by cell distance and calls `FUN_00595610(tm, owner,
fs)`. `FUN_00595750` then marks every employee `+0x4c=1`, `+0x54=0`,
`+0x58=0`, `+0x28=0`.

`FUN_005224c0`:

1. pops the first job when no current job exists;
2. stores owner/target and sets behavior target `+0x2c`;
3. calls `0x00521250(target+0xa4, target+0xa8)` to start the path;
4. if a job is already current, keeps it and pushes it back when finished.

On completion `FUN_00522300` calls `FUN_00580e60(owner, hour)`, which marks
`owner[0x59+8*hour] = 1` and updates the food level from
`owner[0x5c+8*hour]` (`FUN_00576d50`).

## Work action (`0x00522390`)

The target discriminator is the object's definition `[target+0x40] +
0x6c` — the `.bbi` category (file offset 49). Observed values in AI code:
6 show, 8 task-manager building, 9 station, **0xb food source**, 0xc
decoration. `FUN_0053c4d0` matches `def+0x6c == 0xb` **and**
`[fs+0xc8] == paddock`.

For a kind-0xb target the employee plays a work animation driven by the
target definition's `def+0x28` (`.bbi` file offset 37) duration:

- at `0.51 * duration` move to the target cell (`+0x20/+0x24`) or the centre
  of the target rectangle (`+0xc8`);
- at `0.75 * duration` set target `+0xb8 = 1`, `+0xc0 = 1`, behavior phase 1;
- `0x00521a40` updates direction/path; the actor position and tile follow.

`+0xb8`/`+0xc0` live on the food source. Best reading: `+0xb8` =
available/returned, `+0xc0` = claimed/serviced. `0x00522050` resets them to
`1/0` when the employee leaves; `0x00522080` sets phase 2.

**Caveat:** `FUN_00522390` guards the 75 % branch with `behavior+0x34 == 0`,
but the employee ctor `0x521e40` initialises `+0x34 = 2`. The branch is
unreachable unless another writer resets `+0x34`; this is the one open
behaviour detail.

## Schedule manager

`spScheduleManager` (`0x00594b80`) creates:

- a 9-slot timing table (`0x0058c1c0(…, 9)`) with minute/second conversions
  (1, 1/60, 1/3600);
- a 12-entry task vector (`0x0058c480`).

`0x00594d00` is called from `spSimuManager::Update`; it updates/serializes
these structures (`0x58c210`, `0x58c620`).

## Employee roles

`EmployeeList.ini` defines two roles:

```text
Nutritionist  (kind 0)
Guard         (kind 1)
```

The role is `.bei` `def+0x00`, reached through the actor's definition pointer
`actor+0x50` (`FUN_005872c0` proves `+0x50` is the `.bei` record by reading
walk speed at `+0x10`). The `.bei` profiles supply height, walk/action speed,
meshes, textures and sounds.

**Only Nutritionists are ever hired in this exe.** The hire handler
`0x5366c0` (widget `EFFECTIFS_NUTRI_EFFECTIFS_UP1`, installed by the panel
ctor `0x536160`) calls `FUN_0053b2e0(..., 0)` with **employee index 0
hardcoded**. The new employee is pushed by `0x591140` (`tm+0xc8`, capacity
`[tm->def+0x30]`) and bound by `FUN_005766f0` (`emp+0x48 = tm`,
`emp+0x45 = 1`). The `EFFECTIFS_SECU_EFFECTIFS_UP1` widget exists in
`Infopanels.Ini:5651` but not in the executable, and `0x536160` resolves only
the `EFFECTIFS_NUTRI_*` literals. Guards are only a wage bucket
(`EFFECTIFS_SECU`), never queue jobs.

## Open

- `behavior+0x34` is 2 from the ctor, making the 75 % work branch
  unreachable unless another writer resets it.
- Kind `0xb` is a placed food source (`0x5958b0`, owner at `+0xc8`), but the
  same numeric value is `.bbi` category 11 (`LunchDoor`); the naming overlap
  is unresolved even though the AI checks `def+0x6c == 0xb` and
  `[fs+0xc8] == paddock`.
- `behavior+0x30` holds the same manager pointer as `+0x38`.
- Staff wages/costs (not in this path; see `docs/decomp/economy.md`).

## Evidence

- `notes/employee-ai-recon.md` — decompiled functions and fields.
- `0x005220e0`, `0x005224c0`, `0x00522390`, `0x00522300`,
  `0x00522050`, `0x00522080`, `0x00594b80`, `0x0058c1c0`,
  `0x0058c480`.
- `Data/design/employees/EmployeeList.ini`, `*.bei`.
