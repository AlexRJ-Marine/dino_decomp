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

- `0x0056ce80` — versioned graph reader; `param_3` is the **format
  version** (0x29 = v41), not a node-type switch. It reads the graph header
  and the per-node type/fields.
- `0x0056d5a0` — versioned writer; walks the node list and writes each node.

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

The reader's `param_3` comparison is a **version check** (`0x29` = 41), not a
41-way node-type switch. The per-node type is read inside the record and
dispatched through the node vtables/registry. The node types are
conditions/actions of the dinosaur behavior tree.

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

Decode the per-node type dispatch (the reader's `0x29` is the format version,
not a type count) to recover the condition and action nodes. The BAD
FOOD/REPOS -> actions 4/2 mapping should be one of the condition/action pairs
(need gauge `+0x244`/`+0x250` -> action id 4/2).

## Node record and evaluator (2026-10-26)

The "41 node types" reading was wrong: `0x29` is the serialization version,
not a type count.  There is no 41-way type switch.  The graph is a runtime
object list with a versioned serializer:

Writer `0x56d5a0`, per node (base `0x56cc90`, 0x40 bytes):

```
+0x08 dword   node flags (base ctor zeroes it; low byte is a kind flag)
+0x0c dword   written first
+0x10 dword   written last
+0x14 dword
+0x18 vector  (vtable + begin/end/cap; writer stores begin at +0x1c)
+0x28 vector  (writer stores begin at +0x2c)
+0x38 pointer object / action data
+0x3c pointer vtable of the attached object
```

Reader `0x56ce80` branches on the format version: `< 0x29` is the legacy
path (first field at +0xc, node vectors at +0x1c/+0x2c), `>= 0x29` also
reads the graph flags at +0x14/+0x15 and +0x1c/+0x20 and uses the same node
records.  `0x2b` adds an extra header field.  So the reader accepts versions
0x25..0x2b; `0x29` is the current version.

Node construction:

- `0x56e860` copies a source struct into a node: kind byte at +8, +0xc/+0x10
  /+0x14 scalars, two int vectors, and the attached object/vtable at
  +0x38/+0x3c.
- `0x56e190` and `0x56e300` are the two builders: they walk a vector of
  action objects, query each object's virtual `+0xc` predicate, and create a
  node for the ones that are usable.
- `0x56e410` is the scorer: it walks the node list, processes nodes whose
  kind byte is 1, asks the attached object for its action (`vtable+0x14`),
  accumulates effect weights through `0x56e5d0`, applies the actor's
  `+0x14` percentage, and stores the resulting score at node+0xc.
- `0x56e0e0` gates an action on its precondition pairs
  `(threshold, attribute, comparison)`; `0x56e820` adds the object's
  base score at `+8`; `0x56cb30`/`0x56cc20` scale by the actor and profile
  percentages.

The BAD FOOD/REPOS -> action 4/2 mapping is therefore not a node-type
constant: it comes from the need gauges feeding the action objects and the
precondition/score tables in the `.bai` action definitions (already
documented in `docs/formats/bai.md`).  The runtime chooses the highest
scoring usable node; no per-type switch table exists in the binary.

Consequence: there is no file-format table left to decode here.  The node
"kind" byte is a runtime flag written by the builders (0/1), not an enum
serialized in the retail data.
