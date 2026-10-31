# Park dinosaur behavior node set (2026-10-31)

Read-only Ghidra pass on the retail `DinoIsland.exe`. This note corrects the
"four-name node set" assumption in `src/world/dino_actions.*`.

## Question

Which action nodes does a park dinosaur's `apDinosaurBehavior` actually have,
and how do they become available?

## Answer in one line

The graph is **action×target nodes with runtime availability**, built from an
in-memory per-dino descriptor vector. The port's
`{Exploration, Eat, EatForSmallies, Sleep}` list is a placeholder: the
code-built `DB+0x158` table is **Exploration, RunAway, Sleep, Victory**, and
`DB+0x174` is the attack pair. No `.bhv`/graph file is read; the graph
serializer is savegame I/O.

## 1. Two different objects (the notes conflated them)

- `FUN_0051fe20` -> `FUN_0056bc40` -> `FUN_00569e00` allocates the
  **0x274-byte AI dino / feature behavior** (assert `apBehaviorManager.cpp`).
  `FUN_0053cfb0` calls it with `designDB+0x158` and `designDB+0x174`
  (`0x0053cffe`). Read/write: `FUN_0056a170`/`FUN_0056a2f0`
  (vtable `0x642fa0`).
- `FUN_0051f5c0` -> `FUN_00521b30` is the **0x18-byte
  `apDinosaurBehavior`**. It owns two graph sub-behaviors `FUN_0056cb80`
  (vtable `0x643040`) and `FUN_0056ca80` (vtable `0x64302c`), base
  `FUN_0056cc90` (vtable `0x643094`, node list head `+0x08`). Caller
  `FUN_0053cd50` passes `*(dino+0x24)`, `*(dino+0x2c)`, `*(dino+0x28)`; the
  last two are the sub-behavior descriptors.

## 2. `designDB+0x158` / `+0x174` are code-built action-ID tables

- `FUN_005431c0` (DB ctor `FUN_0053d4d0`, call `0x53d671`) sets
  `+0x158=10000`, `+0x15c=10000`, `+0x160=0`, resolves the literal names
  **Exploration, RunAway, Sleep, Victory** (`0x6650c4`, `0x6650bc`,
  `0x65e6b8`, `0x6650b4`) and stores their IDs in
  `+0x168/+0x16c`.
- `FUN_005434a0` (`0x53d678`) resolves **DinoAttackForHerbivore,
  DinoAttackForCarnivore** (`0x6650e8`, `0x6650d0`) into `+0x184/+0x188`.
- The names resolve through the `ActionList.ini` action DB (`0x664ffc`,
  parsed by `FUN_0053e060`); retail file
  `Data/Design/attractions/actions/ActionList.ini` (64 entries).

## 3. Node construction

- `FUN_0056a9a0` (`0x00569eee`, `ECX`=0x274 object, args `big+0x60`,
  `DB+0x158`): for each of the **4 IDs** creates a 0xC action entry
  (`FUN_00569640`) in `[big+0x28]`, then for each record in `[big+0x18]`
  calls `FUN_0056e300(record, actionEntry, big+0x60)`.
- `FUN_0056e300` -> `FUN_0056e860` allocates/links the 0x40-byte nodes:
  `+0x00/+0x04` links, **`+0x08` availability byte**, `+0x0c` score,
  `+0x10` target distance, `+0x14` int, vectors at **`+0x18` and `+0x28`**,
  `+0x38` action-def pointer, `+0x3c` target/action object.
- `apDinosaurBehavior` path: `FUN_00521d80`/`FUN_00521d10` (mode 1/2) call
  `FUN_0056e190(dino+0x24)`, which iterates the descriptor vector
  `[dino+0x28, dino+0x2c)` and appends the same 0x40-byte nodes via
  `FUN_0056e860`. This graph is **per-dino**, not the DB table.

## 4. Availability is runtime

`FUN_0056d880` -> `FUN_0056d980`: each node `+0x08 := 0`; distance is
computed into `+0x10`; if the action class (`*(*(node+0x38)+4)+0x88`) != 5
and the record flag == 1 it tests target geometry then
`FUN_0056e0e0(actiondef, target)`; class-5 nodes are forced 0 afterwards.
`FUN_0056db70` can wake class-5 nodes when the requested target coordinate
matches. `FUN_0056e410` scores only `+0x08 == 1` nodes; others get
`+0x0c = 0x80000000`.

## 5. Park action set

- `DB+0x158` (the 0x274 object) = **Exploration(1), RunAway(2), Sleep(3),
  Victory(4)**, one node per action × action-record/target.
- `DB+0x174` = **DinoAttackForHerbivore(28), DinoAttackForCarnivore(29)**;
  whether they become graph nodes is OPEN (no node-builder call found).
- `Smell`, `GetCourage`, `ObstAttack*`, `Eat`, `EatForSmallies` are in
  `ActionList.ini` but not in either DB table. They are not 0x274 nodes.
- The port's `node_names` therefore has the wrong membership/mechanism:
  retail nodes are action×target with runtime gates, not a static name
  subset. The `apDinosaurBehavior` per-dino descriptor vector still needs
  its producer decoded (the item/feature 0x274 table is not it).

## 6. No `.bhv` file

`FUN_0056ce80`/`FUN_0056d5a0` are savegame read/write (vtable slots), not
initial construction. The node lists likely persist in `.sav` dino records;
this is OPEN pending a call-site/save diff.

## 7. Follow-up: per-dino IDs and the exact node set

- `drsDinosaur` (ctor `0x005e9000`, 0x11c bytes, vtable `0x6483f8`) zeroes
  and then deserializes three dwords: `+0x24`, `+0x28`, `+0x2c`
  (`0x5e8f92`/`0x5e8f9d`/`0x5e8fac`). They are **save/world-resident IDs**
  (behavior-registry keys), not pointers/descriptors; the embedded design
  object starts at `+0x3c`.
- `FUN_0053cd50`: `FUN_00586520(&drs+0x3c, drs+0x1c)` builds the sim actor,
  then `FUN_0051f5c0(owner=simActor, a2=[drs+0x24], a3=[drs+0x2c],
  a4=[drs+0x28])` -> `FUN_00521b30`: master id `[drs+0x24]`, subA
  `FUN_0056cb80` gets `[drs+0x2c]`, subB `FUN_0056ca80` gets `[drs+0x28]`.
  These are registered via `FUN_00572ec0`/`FUN_00572fa0`.
- The node descriptors live on the **0x274 AI object**, not `drsDinosaur`:
  `FUN_0051f950`/`FUN_0051f810` pass `[entry+0x4c]=apDinosaurBehavior` and
  `[entry+0x54]=0x274 object`; `FUN_00521d80`/`0x521d10` call
  `FUN_0056e190(sub, aiObj+0x24)`. Both sub-behaviors read the **same**
  `aiObj+0x24` descriptor container and get the same action set; they
  differ only in mode/activation list (subA mode 2/manager `+0xf4`, subB
  mode 1/manager `+0xb8`).
- `FUN_0056a9a0` is the only node-producing path for that container and
  loops the **`DB+0x158` four-entry table**: **Exploration, RunAway, Sleep,
  Victory** (`FUN_005431c0` literal names resolved through
  `ActionList.ini`). A park dino's own graph is exactly those four,
  replicated per action record/target. It does **not** include `Eat` or
  `EatForSmallies`; those exist only on the food-item behavior.
- `DB+0x174` = `{DinoAttackForHerbivore(28), DinoAttackForCarnivore(29)}`,
  stored at `aiObj+0x268`; no xref to the node builder, OPEN (likely
  show/attack selection only).
- Availability is unchanged: `FUN_0056d980` sets `+0x08` from target geometry
  + `FUN_0056e0e0`; class 5 is forced off; `FUN_0056db70` can wake it;
  `FUN_0056e410` scores only `+0x08 == 1`.

### Exact port decision

`node_names` for the park dino must be
`{Exploration, RunAway, Sleep, Victory}`. `Eat`/`EatForSmallies` must be
removed from the dino's chooser and modeled on the **food-item behavior**
path (the `.bfi` feature's action list + the dino's target policy). The
current port's `{Exploration, Eat, EatForSmallies, Sleep}` is a confirmed
placeholder: it changes membership and bypasses the item behavior.

## Port consequence

Do **not** expand to all 64 actions and do **not** keep the four-name
allowlist as if it were retail. The next slice is the per-dino descriptor
producer (`FUN_0053cd50` args `dino+0x24/+0x28/+0x2c`) and the
`apDinosaurBehavior` mode-1/mode-2 action sets, then the runtime
availability gate (`+0x08`, target range, `FUN_0056e0e0`).

## Evidence

- Ghidra: `FUN_0051fe20`, `FUN_0056bc40`, `FUN_00569e00`, `FUN_00521b30`,
  `FUN_0056cb80`, `FUN_0056ca80`, `FUN_0056cc90`, `FUN_0053cd50`,
  `FUN_0053cfb0`, `FUN_005431c0`, `FUN_005434a0`, `FUN_0053d4d0`,
  `FUN_0053e060`, `FUN_0056a9a0`, `FUN_0056e300`, `FUN_0056e860`,
  `FUN_0056e190`, `FUN_0056d980`, `FUN_0056e0e0`, `FUN_0056db70`,
  `FUN_0056e410`.
- `Data/Design/attractions/actions/ActionList.ini`;
  `Data/Design/Landscape/IslandList.ini` only for the save header (separate).
- `notes/dinosaur-behavior-graph.md`, `notes/dino-action-chooser-recon.md`,
  `notes/dino-ai-gap-audit.md`.
