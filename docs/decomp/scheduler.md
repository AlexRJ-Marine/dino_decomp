# Scheduler and AI update order

Status: **decompiled** from `DinoIsland.exe`; not yet verified by a runtime
trace. Addresses are virtual addresses in the unpatched binary (image base
`0x00400000`).

The game does not use a dependency graph or an event loop per subsystem. A
single `lpScheduler` owns a fixed list of named tasks and calls them in
registration order every frame with the elapsed seconds.

## Frame pipeline

```text
main loop (0x004024ea)
  lpScheduler::Update (0x00507530)
    for each enabled task, in this order:

      park:
        ParkInterfaceProcess   -> overlay process
        ParkInterfaceUpdate    -> 0x00524630
        SimulationManager      -> 0x005869e0   actors, needs, park economy
        BehaviorManager        -> 0x0051fab0   perception, reactions, AI
        WorldManager           -> 0x0053b480   world/scenario objects
        CampaignManager        -> 0x0055dcc0
        AnimationManager       -> 0x005d92d0
        ParkScene              -> 0x005f5450, 0x005f5600
        AudioPark              -> 0x004feff0
        ParkInterfaceDraw      -> overlay draw

      menu:
        MenuInterfaceProcess   -> overlay process
        MenuInterfaceUpdate    -> 0x00517930
        MenuInterfaceDraw      -> overlay draw
```

`lpScheduler` task record:

```text
+0x00  mcString name
+0x0c  u8       enabled
+0x10  void*    context
+0x14  void   (*callback)(float dt, float dt2, void* context)
```

`Update` computes the frame deltas (the first is capped at 0.2 s), stores the
new timestamp, and profiles each callback with `rdtsc`. Both deltas are
passed to the task; the simulation tasks use the first one.
`SetTaskEnabled` (`0x00507490`) matches by name and is the pause/resume
mechanism used by the park interface.

## Park task registration

`0x0051d050` (called from the park start/load paths, `0x00403e10` and
`0x00402f10`) registers the ten tasks above. The names are the exact strings used by the profiler and by
`SetTaskEnabled`:

`ParkInterfaceProcess`, `ParkInterfaceUpdate`, `SimulationManager`,
`BehaviorManager`, `WorldManager`, `CampaignManager`, `AnimationManager`,
`ParkScene`, `AudioPark`, `ParkInterfaceDraw`.

The menu application registers only `MenuInterfaceProcess`,
`MenuInterfaceUpdate`, `MenuInterfaceDraw` (`0x00596500`).

## Simulation step

`spSimuManager::Update` (`0x005869e0`) runs for every sim actor:

```text
0x00571320(actor)          clear one-shot flags
0x00571540(actor, dt)      needs, path, animation state
every 5 s:  0x005725e0(actor)
every 10 s: 0x00571370(actor, 10), 0x00571410(actor, 10), 0x00571350()
```

Then `0x00589cd0(manager, dt)` runs the park-wide step. Its ordering is:

1. rebuild dirty caches and the occupancy grid;
2. once per second (`0x00675a20`): park clock, periodic actor bookkeeping,
   building bookkeeping, grid scan, attraction callbacks;
3. every frame: visitors (`0x0057b220`), food/stock (`0x0057e3a0`),
   buildings (`0x00576320` with 10 / 7.5 / 12.5 s pulses), economy
   (`0x0058a970`), counters (`0x00585c80`), shows (`0x0058dfc0`),
   dinosaur store (`0x00594d00`).

## Behaviour step

`apBehaviorManager::Update` (`0x0051fab0`) performs, in order:

1. dinosaur perception/notification (`0x0051fa10`, `0x00521d80`);
2. generic actor update (`actor->vtable[+0xc](dt)`);
3. clear the one-frame actor flag at `+0x4f9`;
4. per-actor reaction checks (`0x00521d10`, `0x0051f8d0`, `0x00521d60`)
   followed by the actor update; `0x005f6fb0` is an integer-vector `find`,
   not a visibility test;
5. deferred behavior events (`0x00520880`);
6. visitor ticks (`0x00521df0`) and visitor AI (`0x00522820`);
7. employee AI (`0x005220e0`).

`apDinosaurBehavior` (`0x00521b30`, vtable `0x00642abc`) is a small state
machine rather than a per-frame planner:

| State | Meaning |
| ---: | --- |
| 0 | idle |
| 1 | dinosaur is reacting to an actor |
| 2 | dinosaur is reacting to a visitor |
| 3 | initial |

`0x00521d80` sets state 1, `0x00521d10` sets state 2, and `0x00521d60`
returns state 2 to idle and stops the action. `0x005213f0` promotes state 1
to state 2 when the target matches. Actions are started through
`0x0056ac80` / `0x0056e190`; `0x00521790` writes the target cell and inserts a
path record.

## Dinosaur needs

`saDinosaur::Update` (`0x00571540`) decays two need slots:

```text
need_A(+0x96) -= rate(+0x126) * decay_table[type*4 + 0x354]
clamp to [0, max(+0x76)]

every 10 s:
    need_B(+0x97) -= 10 * rate(+0x1b)
    timer(+0x248) = max(0, timer - 10)
```

`+0x130` selects the current need/action index; `0x00571190` advances it and
sets the action to `0x1f` (idle/complete). The index-to-`.bai` mapping is
still open.

## Related functions

| Address | Function |
| --- | --- |
| `0x00506ec0` | `lpScheduler` constructor |
| `0x00507050` | `AddTask` |
| `0x00507490` | `SetTaskEnabled` |
| `0x00507530` | `Update` (frame tick) |
| `0x0051d050` | park task registration |
| `0x00596500` | menu task registration |
| `0x005869e0` | `spSimuManager::Update` |
| `0x00589cd0` | park-wide simulation step |
| `0x0051fab0` | `apBehaviorManager::Update` |
| `0x00521b30` | `apDinosaurBehavior` constructor |
| `0x00521d80` / `0x00521d10` / `0x00521d60` | dinosaur reaction state transitions |
| `0x00571540` | `saDinosaur::Update` (needs) |
| `0x00571190` | advance/complete the current need action |
| `0x005725e0` / `0x00571370` / `0x00571410` | 5 s and 10 s need passes |

## Evidence

- `notes/scheduler-recon.md` — decompiled code excerpts and field notes.
- `docs/decomp/function-status.md` — status table.
- Ghidra: `lpScheduler.cpp`, `spSimuManager.cpp`, `spDinosaurStore.cpp`,
  `saDinosaur.cpp`, `apBehaviorManager.cpp`, `apDinosaurBehavior.cpp`,
  `apVisitorBehavior.cpp`, `apEmployeeBehavior.cpp` assert paths.
