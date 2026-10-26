# Visitor/obstacle perception reaction (`apBehaviorManager`)

Status: **partial** — the notification/target-selection helpers and the two
reaction starters are decoded; the target/policy `GetTarget` vtable slot and
the list producers remain.

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
