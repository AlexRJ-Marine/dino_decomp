# Dinosaur behavior graph recon (2026-10-26)

Follow-up to the action/need research. The `apDinosaurBehavior` object owns a
**node graph** with its own serializer; this is where the action chooser
lives.

## Objects

- `apDinosaurBehavior` ctor `0x00521b30` (assert file
  `apDinosaurBehavior.cpp`):
  - `+0x04` actor (dinosaur)
  - `+0x0c` sub-behavior built by `0x0056cb80` (vtable `0x643040`)
  - `+0x10` sub-behavior built by `0x0056ca80` (vtable `0x64302c`)
  - `+0x08` a node list counter, `+0x14` mode (`1`/`2` select which
    sub-behavior `0x00521cc0` dispatches to)
- Both sub-behaviors derive from `0x0056cc90`, which initialises a circular
  linked list at `+0x08` (list head) and stores the actor at `+0x28`.

## Node graph

The graph is serialized by:

- `0x0056ce80` ? reader; takes a node type `0..0x28` (41 types) and fills the
  node's fields.
- `0x0056d5a0` ? writer; walks the list and writes each node.

Node record shape (from the writer):

```text
+0x00  linked-list next
+0x04  int  (type / id)
+0x08  int
+0x0c  int
+0x10  int
+0x14  vector<int>   (count + ints)
+0x28  vector<int>   (count + ints)
+0x3c  int
+0x40  child link (node pointer)
+0x44  actor field +0x60
```

The reader's `switch (type)` covers 41 node types; type `0x25` (37) reads a
vector and is the largest branch. The node types are conditions/actions of
the dinosaur behavior tree (need checks, target selection, action choice).

## Vtables

```
0x00642abc  apDinosaurBehavior        0x521c60 0x521cc0 0x521ea0 0x521fa0 0x5220a0 0x522050 0x522080
0x00643040  sub-behavior A            0x56cbb0 0x56cae0 0x56cb00 0x56cbe0 0x56cc20
0x0064302c  sub-behavior B            0x56cab0 0x56cae0 0x56cb00 0x56cb20 0x56cb30
```

`0x56cae0`/`0x56cb00` are the shared load/save thunks into `0x56ce80`/
`0x56d5a0`. `0x56cb30` scales an int by `actor[+4][+0x28] / 100`;
`0x56cc20` combines a value with `actor[+0x10][+0xcc]` and divides by 100.

## Next step

Decode the 41 node-type branches of `0x0056ce80` to recover the condition and
action nodes. The BAD FOOD/REPOS -> actions 4/2 mapping should be one of the
condition/action pairs (need gauge `+0x244`/`+0x250` -> action id 4/2).
