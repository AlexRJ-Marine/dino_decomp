# Employee AI and task loop

Status: **decompiled**; job kind `0xb`, role assignment and staff costs remain
partial.

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
`+0x54` (job id), `+0x58` (target) and `+0x5c` (parameter). The queue stores
0xc-byte entries `{u32 id, u32 target, u32 param}`.

`FUN_005224c0`:

1. pops the first job when no current job exists;
2. stores id/target/param and sets behavior target `+0x2c`;
3. calls `0x00521250(target+0xa4, target+0xa8)` to start the path;
4. if a job is already current, keeps it and pushes it back when finished.

## Work action (`0x00522390`)

For target kind `0xb`, the employee plays a work animation driven by the
target's `+0x28` duration:

- at `0.51 * duration` move to the target cell (`+0x20/+0x24`) or the center
  of the target rectangle (`+0xc8`);
- at `0.75 * duration` set target `+0xb8 = 1`, `+0xc0 = 1`, behavior phase 1;
- `0x00521a40` updates direction/path; the actor position and tile follow.

`0x00522050` resets the kind-0xb target flags; `0x00522080` sets phase 2.

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

The `.bei` profiles supply height, walk/action speed, meshes, textures and
sounds. The decoded job entries do not carry the role; it lives on the
employee actor/profile.

## Open

- Job kind `0xb` semantics and the `+0x28` duration unit.
- Nutritionist vs Guard job assignment in `spScheduleManager`.
- Target flags `+0xb8`/`+0xc0` and the `0x00580e60` completion contract.
- Staff wages/costs (not in this path; see `docs/decomp/economy.md`).

## Evidence

- `notes/employee-ai-recon.md` — decompiled functions and fields.
- `0x005220e0`, `0x005224c0`, `0x00522390`, `0x00522300`,
  `0x00522050`, `0x00522080`, `0x00594b80`, `0x0058c1c0`,
  `0x0058c480`.
- `Data/design/employees/EmployeeList.ini`, `*.bei`.
