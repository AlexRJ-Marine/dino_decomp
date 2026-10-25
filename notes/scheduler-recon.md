# Simulation scheduler and AI update order (DEC-2) — recon

Date: 2026-10-25
Scope: static decompilation of `DinoIsland.exe` (image base `0x00400000`).
No writes to `game/` or `original/`; this note is evidence only.

Status: **decompiled, not yet dynamically verified**. The update order below
comes from the task registration and the tick implementations, not from a
runtime trace.

## 1. `lpScheduler` (CORE / Gameloop)

Source path in asserts: `D:\Dino\Dev\CORE\Src\Gameloop\lpScheduler.cpp`.

| Address | Function | Notes |
| --- | --- | --- |
| `0x00506ec0` | `lpScheduler` constructor | creates clock `0x00441b40`, timer/profiler `0x00506bc0`, stats object `0x005b93e0(30.0)` |
| `0x00506f80` | destructor | frees the task vector, clock and stats |
| `0x00507050` | `AddTask(name, callback, context)` | appends a 24-byte task record, enabled = 1 |
| `0x005073d0` | `ClearTasks()` | destroys all task records |
| `0x00507490` | `SetTaskEnabled(name, bool)` | name match is case-insensitive `strcmp` (`0x00405740`) |
| `0x00507500` | `Reset()` | resets clock/profiler and stores the new timestamp |
| `0x00507530` | **`Update()`** | the frame tick; see below |
| `0x00507740` | task-vector growth/reallocation | vector element stride `0x18` |
| `0x00507920` | task-record copy | 24 bytes: name string, enabled, context, callback |

Task record layout (from `0x00507050` / `0x00507530`):

```text
+0x00  mcString name
+0x0c  u8       enabled
+0x10  void*    context
+0x14  void   (*callback)(float dt, float dt2, void* context)
```

`lpScheduler::Update` (`0x00507530`):

```text
dt  = timer->delta_seconds()            // 0x00506e30; caps at 0.2 s
now = clock->now()                       // 64-bit counter
dt2 = (now - last_time) * 0.001
last_time = now
for task in tasks (insertion order):
    if task.enabled:
        start = rdtsc()
        task.callback(dt, dt2, task.context)
        profiler[task].samples += 1
        profiler[task].ticks   += rdtsc() - start
```

The callback receives two time deltas and its context; the simulation tasks
use the first argument as `dt`. The animation task ignores both and reads its
own globals. `lpScheduler::Update` is called from the main loop at
`0x004024ea`.
`lpScheduler::Reset` is called on new/load paths at `0x00403e10`,
`0x00402bc0` and `0x00402f10`.

## 2. Registered task order

`0x0051d050` registers the park tasks from the park start/load paths
(`0x00403e10`, `0x00402f10`). This is the **frame order**, because `Update`
walks the vector in insertion order.

| # | Task name | Callback | Entry point |
| ---: | --- | --- | --- |
| 1 | `ParkInterfaceProcess` | `0x00596550` | overlay manager vtable `+0x10` |
| 2 | `ParkInterfaceUpdate` | `0x0051d1b0` | `0x00524630` (`moWidgetManager` update) |
| 3 | `SimulationManager` | `0x0051d160` | `0x005869e0` (`spSimuManager::Update`) |
| 4 | `BehaviorManager` | `0x0051d170` | `0x0051fab0` (`apBehaviorManager::Update`) |
| 5 | `WorldManager` | `0x0051d140` | `0x0053b480` (`pwdWorldManager::Update`) |
| 6 | `CampaignManager` | `0x0051d150` | `0x0055dcc0` |
| 7 | `AnimationManager` | `0x0051d1c0` | `0x005d92d0` |
| 8 | `ParkScene` | `0x0051d180` | `0x005f5450` then `0x005f5600` |
| 9 | `AudioPark` | `0x0051d1a0` | `0x004feff0` |
| 10 | `ParkInterfaceDraw` | `0x00596570` | overlay manager vtable `+0x14` |

The menu application (`0x00403c80`) registers a separate three-task list at
`0x00596500`:

1. `MenuInterfaceProcess` — vtable `+0x10`
2. `MenuInterfaceUpdate` — `0x00517930`
3. `MenuInterfaceDraw` — vtable `+0x14`

`0x00507490` is the pause mechanism: it disables tasks by name. The in-park
navigation/options code (`0x005255d0`, `0x00524630`) uses it to stop/resume
groups such as `SimulationManager` and `BehaviorManager`.

## 3. `spSimuManager::Update` (`0x005869e0`)

```text
global_10s += dt;   if >= 10: reset, ten_second_tick = true
global_5s  += dt;   if >=  5: reset, five_second_tick = true

for actor in sim_actor_manager.actors (+0x68 .. +0x6c):
    0x00571320(actor)            // clear pending one-shot flags
    0x00571540(actor, dt)        // per-actor update (needs/animation/path)
    if five_second_tick:
        0x005725e0(actor)        // passive need feedback
    if ten_second_tick:
        0x00571370(actor, 10.0)  // decay need A
        0x00571410(actor, 10.0)  // decay timer/need B
        0x00571350()             // update four global slots

0x00589cd0(this, dt)             // park-wide simulation step
```

Timers are globals, not per manager instance:

```text
0x00675a10  +dt, 10.0 s period   (simulation 10 s tick)
0x00675a14  +dt,  5.0 s period   (simulation 5 s tick)
0x00675a20  +dt,  1.0 s period   (park-wide 1 s tick, inside 0x00589cd0)
0x00675a2c  +dt, 10.0 s period   (building/visitor substep)
0x00675a30  +dt,  7.5 s period   (building/visitor substep)
0x00675a34  +dt, 12.5 s period   (building/visitor substep)
```

## 4. `spSimuManager::UpdatePark` (`0x00589cd0`)

This is the park-wide step called by `spSimuManager::Update` after the actor
loop. It is a long function; the important ordering is:

1. If a dirty flag (`manager[0]`) is set, rebuild the cached actor/building
   vectors and the occupancy grid (`+0x5c`).
2. Add `dt` to the 1 s timer (`0x00675a20`). Once per second:
   - `0x005774f0()` — park clock / scheduler bookkeeping
   - `0x00571b50()` for each sim actor — periodic actor bookkeeping
   - `0x00571440()` for each sim actor — periodic actor counters
   - `0x005763b0()` for each building/attraction
   - scan the occupancy/height grid and update the max cell value at
     `manager[0x5c]+0x14`
   - virtual `+0x14` on the `manager[0x114]` list (attraction/building update)
3. Per-frame:
   - `0x0057b220(dt)` for the `+0x40` list (visitor flow)
   - `0x0057e3a0(dt)` for the `+0xa0` list (food/stock/animal pens)
   - `0x00576320(dt, ten_s, seven5_s, twelve5_s)` for each building with
     `building[0x6c] == 0`; the three flags are the 10 / 7.5 / 12.5 s pulses
   - `0x0058a970(dt)` — economy / ticket / income
   - `0x00585c80()` — park-level counters
   - `0x0058dfc0(dt)` — shows/attractions
   - `0x00594d00(dt)` — dinosaur/visitor store bookkeeping

The exact responsibilities of the last four functions are not yet proven;
they are called in this order every frame.

## 5. `apBehaviorManager::Update` (`0x0051fab0`)

Lists in the behavior manager (all vectors of 4-byte pointers):

```text
+0x04  scene actors (rendering/physics side)
+0x14  sim actors with behavior (dinosaurs, visitors, employees)
+0x24  actors that need a generic update
+0x38  visitors
+0x4c  employees
+0x5c  deferred behavior-event queue
```

Order inside one update:

1. **Dinosaur perception / notification**
   - for each sim actor `a` with behavior `b = a+0x270`:
     - if `b.state == 1` and `b[200] != b[0xcc]`:
       `0x0051fa10(b, a, dt)` — notify/attack nearby actors.
     - if `b[0xb8] != b[0xbc]`: scan scene actors, use `0x005f6fb0` as a
       filter/visibility test, and call `0x00521d80(a, b, target)` when hit.
2. **Generic actor update**
   - for each actor in `+0x24`: virtual `vtable[+0xc](dt)`.
3. Clear the one-frame `actor+0x4f9` flag for each scene actor.
4. **Behavior-specific reactions**
   - for each sim actor `a`, behavior `b = a+0x270`:
     - if `b.state != 1`, skip the reaction checks but still run the actor
       update at the end.
     - `b+0xd6 == 1`: visitors looking at the dinosaur; scan scene actors,
       `0x005f6fb0`, then `0x00521d10(a, b, target)` (state 2).
     - `b+0xd8 == 1`: `0x0056bcb0(a)` then `0x0056ab00(a)`.
     - `b+0xdb == 1`: walk `b+0x104..0x10c`; if the target actor's
       `+0x15 == 0` hidden flag, `0x00521d60(b)` (stop reaction).
     - `b+0xd9` or `b+0xda`: `0x0051f8d0(a, b)` — force reaction to a target
       list.
     - virtual `actor.vtable[+0xc](dt)` — actor update.
5. If `+0x5c` queue non-empty: `0x00520880()` — process deferred events.
6. **Visitor behavior**: for each visitor in `+0x04`:
   `0x00521df0(visitor, dt)`.
7. For each visitor behavior in `+0x38` whose actor `+0x6c == 0`:
   `0x00522820(behavior, dt)`.
8. **Employee behavior**: for each employee behavior in `+0x4c`:
   `0x005220e0(behavior, dt)`.

## 6. `apDinosaurBehavior` state machine (`0x00521b30`)

Source path: `D:\Dino\Dev\AI\Src\ParkBehavior\apDinosaurBehavior.cpp`.
Object size 0x30. Vtable `0x00642abc`.

| Offset | Meaning |
| --- | --- |
| `+0x08` | actor pointer |
| `+0x0c` | "visitor seen" action controller |
| `+0x10` | "dinosaur seen" action controller |
| `+0x14` | state: 0 idle, 1 dinosaur reaction, 2 visitor reaction, 3 initial |
| `+0x28` | current actor target |
| `+0x2c` | current action target |
| `+0x34` | result/status |

State transitions:

| Address | Behavior |
| --- | --- |
| `0x00521d80` | state = 1; `0x0056d7d0` + `0x0056ac80` select/start the dinosaur action |
| `0x00521d10` | state = 2; starts the visitor-facing action |
| `0x00521d60` | state 2 → 0; `0x0056b550(action, 1)` stops the action |
| `0x005213f0` | event callback: if state 1 and target matches, state = 2; if state 2 and target matches, call vtable `+0x10` (stop) |
| `0x00521760` | stop: action state = 2, actor action pointer = 0 |
| `0x00521790` | move/action setup: writes target grid coords, animation, and inserts a 20-byte path record |

The behavior is event-driven: the per-frame work observed here is the actor
update plus the visibility/notification passes in
`apBehaviorManager::Update`; the reaction targets and actions are latched
into the state machine. No separate high-level planner was found in the
module.

## 7. `saDinosaur::Update` / needs (`0x00571540`)

The dinosaur's phenotype is copied into the actor at `+0x14` (0x6e dwords =
440 bytes; see the genotype research). The update function uses these fields:

| Offset | Meaning (observed) |
| --- | --- |
| `+0x94` | integer need/status value |
| `+0x96` | float need A (decays every frame) |
| `+0x97` | float need B (decays every 10 s) |
| `+0x98` | action countdown (seconds) |
| `+0x76` | max value for need A |
| `+0x126` | need-A decay rate |
| `+0x130` | current need/action type |
| `+0x140` | pointer to the five per-need display values |
| `+0x248` | second timer/need value |
| `+0x2c0` | current action/need index; `0x1f` = idle/complete |
| `+0x515` | "action finished" flag |
| `+0x4d` | one-frame "state changed" flag |

Per-frame part (`0x00571540`):

```text
if need_type != 0:
    need_A -= rate * table[0x354 + need_type*4]
    clamp need_A to [0, max_A]
    need_display = int(need_A)
    state_changed = true
    factor = switch(need_type):
        1,2        -> stat[0x9a] * 0.2
        9,10,11    -> stat[0xa2]
        12         -> stat[0xa3]
        13         -> stat[0xa4]
        14         -> stat[0xa5]
        15         -> stat[0xa6]
        16,0x25    -> stat[0x99]
        25         -> stat[0x2f]
        default    -> unchanged
    for i in 0..4:
        need_display[i] = factor * table[0x420 + need_type*0x14 + i*4]

if action != 0x1f:
    countdown -= dt
    if countdown < 0:
        0x00571190()     // advance/complete the action
```

`0x00571190` copies a stat block into the live need block for action indices
9, 10, 11, 13, 16, 20, 26, 27, 28; for index 13 it copies five dwords. It
then sets action = 0x1f and raises `0x515` and `0x4d`.

Periodic passes from `spSimuManager::Update`:

```text
every 5 s, 0x005725e0(actor):
    if need_A > 0.01 and need_B > 0.01 and actor[0xaf] != 4:
        actor.vtable[+0xc](-*(int16*)(actor+0x1d6))

every 10 s, 0x00571370(actor, 10.0):
    need_B -= 10.0 * actor[0x1b]
    if the old value was above the threshold and the new value crosses it:
        actor+0x4d = 1; actor+0x517 = 1
    actor.vtable[+0xc](max(1, int(delta)))
    actor[0x91] = int(need_B)

every 10 s, 0x00571410(actor, 10.0):
    actor[0x248] = max(0, actor[0x248] - int(10.0))

every 10 s, 0x00571350():
    for i in 0..3: 0x005714b0(i)
```

The semantic names of need A / need B / the 0x2c0 action indices are not yet
confirmed. They are generic status/need slots; the action indices correspond
to the `.bai` actions (Eat, Sleep, …) but the index→file mapping needs a
runtime trace or a cross-reference of the action manager.

## 8. What this gives the port

The per-frame port order should be:

```text
ui process -> ui update
simulation (actors: needs/path/animation; park: economy/visitors/shows)
behavior (perception, reactions, actor update, visitor/employee AI)
world manager
campaign manager
animation manager
scene update -> scene render
audio
ui draw
```

The scheduler is a simple fixed-order, variable-dt task list. Menus use a
three-task subset (`process`, `update`, `draw`) and the park uses ten tasks.
There is no priority or dependency graph; insertion order is the contract.

## Open questions

- Semantic names for the dinosaur need slots and the `0x2c0` action indices.
- Which exact task subset `0x00507490` disables when the game is paused.
- The `0x00589cd0` sub-functions `0x0058a970`, `0x00585c80`, `0x0058dfc0`,
  `0x00594d00` are ordered but not individually decoded.
- `0x005f6fb0` is used as a target filter in both dinosaur and visitor
  perception; its contract (distance/visibility/LOS) is still open.
- Confirmation by a runtime trace (breakpoints on 0x00507050 callbacks) is
  still desirable.

## Evidence

- Ghidra decompilation of `0x00506ec0`, `0x00507050`, `0x00507530`,
  `0x0051d050`, `0x00596500`, `0x005869e0`, `0x00589cd0`, `0x0051fab0`,
  `0x00521b30`, `0x00521d80`, `0x00521d10`, `0x00521d60`, `0x00571540`,
  `0x00571190`, `0x005725e0`, `0x00571370`, `0x00571410`.
- Strings: `D:\Dino\Dev\CORE\Src\Gameloop\lpScheduler.cpp`,
  `D:\Dino\Dev\AI\Src\ParkBehavior\apDinosaurBehavior.cpp`,
  `D:\Dino\Dev\SIMU\Src\SimuPark\spSimuManager.cpp`,
  `D:\Dino\Dev\SIMU\Src\SimuActor\saDinosaur.cpp`.
- Task names live at `0x0065eb0c`..`0x0065eba4`; registration at `0x0051d050`
  and `0x00596500`.
