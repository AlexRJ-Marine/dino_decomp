# Employee AI and task loop

Date: 2026-10-25
Scope: static decompilation of `apEmployeeBehavior` / `spScheduleManager`.
Read-only; no game writes.

Status: **state machine and task-queue handoff decoded; task kind 0xb and the
schedule manager slots are still partial.**

## 1. Employee behavior object

`apEmployeeBehavior::Update` (`0x005220e0`) is the employee counterpart of
the visitor update. It points at the employee actor at `+0xc` and a job
manager at `+0x38`:

| Offset | Meaning |
| --- | --- |
| `+0x08` | local action/status |
| `+0x0c` | employee actor |
| `+0x2c` | current target object |
| `+0x34` | work-animation phase (kind 0xb) |
| `+0x38` | employee task manager |

The task manager (`+0x38`) keeps:

| Offset | Meaning |
| --- | --- |
| `+0x48` | job queue object |
| `+0x4c` | one-frame "job changed" flag |
| `+0x54` | current job id |
| `+0x58` | current job target |
| `+0x5c` | current job parameter |

The job queue at `+0x48` stores 0xc-byte entries at `+0xe8..+0xec` and
`+0xf0`.

## 2. Update dispatcher

`FUN_005220e0` mirrors the visitor dispatcher:

| Actor state (`actor+0x28`) | Update |
| ---: | --- |
| 0 | `0x005224c0` — take the next job |
| 1 | `0x00521490` — travel |
| 2 | `0x005219f0` + `0x00522390` — use/work |

`FUN_00522300` handles transitions:

- state 0: if the task check passes, move to state 1;
- state 1: on arrival move to state 2;
- state 2: when the work completes, clear the current job
  (`manager+0x54/+0x58`) and call `0x00580e60` with the job target.

## 3. Taking a job (`0x005224c0`)

If no current job (`manager+0x54 == 0`):

1. take the first 0xc-byte job from the queue `+0xe8..+0xec`;
2. store job id/target/parameter at `+0x54/+0x58/+0x5c`;
3. set the behavior target `+0x2c`;
4. if the target's `+0x34` flag is set, call `0x00521250(target+0xa4,
   target+0xa8)` to start the path.

If a current job exists, it keeps the target and, when the job is finished,
pushes the current entry back to the queue and clears the slot.

The job entry is `{u32 id, u32 target, u32 param}`.

## 4. Work action (`0x522390`)

For job/attraction kind **0xb** the employee performs a work animation:

- the target's `+0x28` duration controls the phase;
- when `0.51 * duration` has elapsed, the employee moves to the target's
  `+0x20/+0x24` cell (or the center of the target rectangle `+0xc8`);
- at `0.75 * duration`, set target `+0xb8 = 1`, `+0xc0 = 1`, and the
  behavior `+0x34 = 1`;
- `0x00521a40` updates the direction/path, then the actor position and tile.

`FUN_00522050` and `FUN_00522080` are the small handlers for this kind:
`0x522050` resets the target flags, `0x522080` sets the behavior phase to 2.

## 5. Scheduler

`spScheduleManager` (`0x00594b80`) creates:

- a 9-slot timing table (`0x0058c1c0(…, 9)`), storing minute/second conversions
  (1, 1/60, 1/3600);
- a 12-entry job/task vector (`0x0058c480`).

`spSimuManager::Update` calls `0x00594d00` every frame (`0x594c80` wrapper →
`0x58c210`/`0x58c620`), which serializes/updates these two structures.

## 6. Employee kinds

`EmployeeList.ini` has two roles:

```text
Nutritionist  (kind 0)
Guard         (kind 1)
```

The `.bei` profiles (other researcher) give height, walk speed, action speed,
meshes, textures and sounds. The task queue does not carry the role directly
in the decoded entries; the role is on the employee actor/profile.

## 7. Open

- Meaning of job kind `0xb` and the target's `+0x28` duration unit.
- How `spScheduleManager` assigns jobs to Nutritionist vs Guard.
- The target flags `+0xb8`/`+0xc0` and the `0x00580e60` completion call.
- Whether employees use the same attraction scoring as visitors for idle
  tasks.
- Staff wages/costs: not found in this path (see `docs/decomp/economy.md`).

## Evidence

- `FUN_005220e0` — employee update dispatcher.
- `FUN_005224c0` — job-queue pop and target selection.
- `FUN_00522390` — kind 0xb work animation/timer.
- `FUN_00522300` — state transitions.
- `FUN_00522050` / `0x00522080` — kind 0xb state handlers.
- `FUN_00594b80` / `0x0058c1c0` / `0x0058c480` — schedule manager.
- `Data/design/employees/EmployeeList.ini`, `*.bei` (read-only).
