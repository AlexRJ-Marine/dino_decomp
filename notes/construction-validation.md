# P4 — construction validation and economy loop

Status: **budget gate and the two construction warnings located; the cell
rules are the remaining piece.** Complements `docs/decomp/economy.md` and
`docs/platform/buildings.md`.

## Affordability gate (`0x52bed0`)

```text
if (budget < cost) {
    show "&WARNING_NOT_ENOUGH_MONEY";
    return 0;
}
return 1;
```

The budget is reached through the park/world chain
`[[[[this+0x44]+0x10]+0x14]+0x120]+8`; `cost` is the second argument. This is
the common pre-build check used before a wall, building or feature is
created. It does **not** deduct; the caller's build path does (the wall path
passes the wall record's `+0x0c` to `FUN_00585d20`, the CONSTRUCTION budget
setter).

## Fence / no-room validator (`0x535be0`)

`0x535be0` is a virtual method (data xref `0x642d8c`) that is not registered
as a function in the current Ghidra project; its body references
`&WARNING_NO_ROOM_IN_FENCE` at `0x536017`/`0x53602c`. It checks the
construction tool's current fence/paddock state (`this+0x44`, byte `+0x3d`)
and the budget before allowing the placement. Manual start: `0x535be0`
(`sub esp,0x3c; push ebx/ebp/esi; mov esi,ecx`).

Port implication: the "room in fence" case is a **fence/paddock** rule, not
the generic cell rule; the port should surface it when a paddock has no
buildable cell for a dinosaur or when a fence edit would split/close a
paddock without room.

## Cell rules already solved elsewhere

- `.raw` occupation mask 1024×1024, `0` = buildable land, `255` =
  blocked/water (`docs/formats/raw.md`). Cell = `world/4` for the 4-unit
  grid.
- Building footprint: `.bbi` bytes 0/4 are **world units**, not cells;
  `state` 0..3 rotates 90° and swaps the footprint
  (`docs/platform/buildings.md`).
- Cost: building/feature prices are `.bbi` byte 25 / `.bfi` header `0x19`;
  wall cost is `.bwi` `+0x0c` (10/15/30). Path tiles are free in the port's
  current build mode.
- Daily economy: `docs/decomp/economy.md` (`0x58ad00` settlement, wages,
  ticket income, shop revenue).

## Port order for P4

1. Load the occupation mask and expose `cell_buildable(x,y)`.
2. Building placement: footprint cells all buildable and free; cost from
   `.bbi`; `if (!afford(cost)) warning` else deduct.
3. Fence/path placement: canonical edge/cell, cost check for walls.
4. Daily settlement after the clock rollover, using the documented
   `spPark` counters.

## Open

- The exact cell predicate inside `0x535be0` (which cells are reserved for
  the construction tool) needs a manual disassembly or a runtime check.
- Whether path tiles charge the CONSTRUCTION budget (the wall path is the
  only proven setter).
