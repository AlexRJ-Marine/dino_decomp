# Visitor/obstacle perception reaction (`apBehaviorManager`)

Status: **superseded for the list path — the target id lists have no writer in
this build** (`notes/perception-id-lists-recon.md`): `beh+0xb8`/`+0xc8`/
`+0xf4`/`+0x108` are always empty, so the notification/selection bodies never
run. The starters themselves (`0x521d10`/`0x521d80`) and their direct entry
points remain usable by the show/attraction controller; the target policy
`+0x14` is ported (`notes/dino-target-vtable-recon.md`). A Cpg04 runtime trace
saw `0x521d80` fire 20 times with actor targets and `0x521d10` 0 times, so the
direct path is live (`notes/target-vtable-runtime.md`). The older partial
notes below are kept for the ABI.

## `FUN_0051fa10(behavior, actor, dt)` — dinosaur notification pass

Called from `apBehaviorManager::Update` (`0x51fab0`) while the dinosaur
behavior state is 1 and `behavior[200] != behavior[0xcc]`.

```text
vector = behavior+0x04 .. behavior+0x08   // 4-byte actor pointers
for each entry e:
    id = *(e + 8)                          // actor id
    if std::find(actor+200, actor+0xcc, id) != actor+0xcc:
        FUN_0055ae30(*(e + 8))
        FUN_00521dd0(dt)
```

`FUN_005f6fb0` is the integer-vector `find` used throughout the behavior
manager. `actor+200`/`actor+0xcc` are the dino's known-target vector
begin/end. `FUN_00521dd0` is the deferred-event enqueue (not the reaction
starter); the reaction starts through the `0x521d80`/`0x521d10` paths below.

## `FUN_0051f8d0(actor, behavior)` — target-list stop pass

```text
behavior = actor+0x270
for each id in behavior+0xf4 .. behavior+0xf8:
    find the scene actor whose +8 == id
    if target_actor+0xc->+0x15 == 0:       // visible, not hidden
        FUN_0055ae30(id)
        FUN_00521d60(behavior)             // stop state 2
```

This is the "looked-at visitor disappeared/left" cleanup. The scene actor
list is `behavior+0x04` (the same vector used by `0x51fa10`); `+0x0c` is the
actor's visual/visibility object and `+0x15` is its hidden flag.

## Reaction starters

`FUN_00521d80(behavior, actor, target)` — dinosaur reaction, state 1:

```text
behavior+0x14 = 1
FUN_0056d7d0(actor)
(behavior+0x10)->+4 = target+4
FUN_0056e190(target+0x24)     // attach the action nodes
FUN_0056ac80(behavior+0x10)   // start the action controller
```

`FUN_00521d10(behavior, actor, target)` — visitor reaction, state 2:

```text
behavior+0x14 = 2
FUN_0056d7d0(actor)
(behavior+0x0c)->+4 = target+4
FUN_0056e190(target+0x24)
FUN_0056ac80(behavior+0x0c)
```

`FUN_00521d60(behavior)` is the stop counterpart: state 2 -> 0 and
`FUN_0056b550(action, 1)`.

## Chooser target provider (partial)

`FUN_0056e410` walks the behavior node list (`behavior+8` sentinel). For
each enabled node it calls the object at `node+0x3c`:

```text
(node[0xf]->vtable[+0x14])(&x, &y)   // GetTarget position
node[0xf]->vtable[+0x18]()           // target radius
node[0xf]->vtable[+0x1c]()           // target actor/object
```

- `node+0x38` (`node[0xe]`) points to the `asItemBehavior` wrapper;
  `node[0xe]+4` is the action record whose `+0x78/+0x7c` need deltas,
  `+0x60` special_action and `+0x88` effect_class are read.
- `FUN_0056d980` computes `(target.x - actor.x)^2 + (target.y - actor.y)^2`
  and stores it at `node+0x10`; it also compares
  `node[0xf]+0x28` (target actor) against the scene actor list.
- `FUN_00572dd0(actor, x, y)` is the radius box test:
  `|x - actor.x| <= actor+0x27c` and the same for y.
- The chooser threat term is `-cell(target) + cell(actor) - actor+0x2a0`,
  added as `need_term(SECU, value / 10)`.

The provider vtable itself is still unidentified. The actor vtable
`0x642f70` slot `+0x14` (`0x55c6b0`) is a grid/occupation lookup, not a
position getter, so the provider is a separate target object. The next
step is a one-shot runtime probe at `0x56e506` (ECX = actor,
`local_14[0xf]` = provider) to read the provider's vtable and therefore
the `GetTarget`/`GetRadius`/`GetObject` function addresses.

## What is still open

- The **target/policy vtable `+0x14`** (`GetTarget`) used by the chooser's
  threat term and by obstacle/visitor actions. The port currently passes
  `threat_score = 0` in `DinoActionLoop`.
- The producers of the `behavior+0xf4` target list and the `actor+200`
  known-target vector.
- How the hidden flag (`actor+0xc->+0x15`) is set on despawn/visibility.
- Whether the perception pass also feeds the SECU danger grid (the grid is
  currently dinosaurs-only, `docs/platform/danger-grid.md`).

## Port impact

`src/world/dino_actions.cpp` already evaluates the chooser's target box and
threat term with a `threat_score` input; once the `GetTarget` slot and the
target list producers are decoded, the visitor/obstacle reaction can be wired
without changing the action chooser. Evidence addresses: `0x51fab0`,
`0x51fa10`, `0x51f8d0`, `0x521d10`, `0x521d80`, `0x521d60`, `0x51f8d0`,
`0x5f6fb0`, `0x55ae30`.
