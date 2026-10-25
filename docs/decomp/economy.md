# Park budget and daily economy

Status: **decompiled**; the fixed category labels and the day trigger are
solved. The exact `spPark+0x1c` index-to-label mapping and the UI ledger
rows are still partial.
The day trigger: `0x00524630` fires `0x0058ad00` when the park clock's
end-of-day flag is set (`docs/decomp/park-time.md`).

## Budget

`spPark` stores the current budget at **`+0x08`**. `FUN_00585bd0` sets it;
`FUN_00585240` (park save/load) reads it. The retail saves contain:

- manual: 75,125
- autosave: 75,000

The same park block carries the economy counters:

| Relative to block `+0x28` | Meaning |
| ---: | --- |
| `+0x08` | budget |
| `+0x0c` | float `1.0` |
| `+0x14`/`+0x18` | expense group total / count |
| `+0x1c + category*4` | per-category expense counters |
| `+0x8c`/`+0x90` | ticket income total / count |
| `+0x9c`/`+0xa0` | expense group A total / count |
| `+0xa4`/`+0xa8` | expense group B total / count |
| `+0xac`/`+0xb0` | expense group C total / count |
| `+0xb4`/`+0xb8` | generic income total / count |
| `+0x94`/`+0x95` | bankruptcy flags |

## Budget API

| Address | Operation |
| --- | --- |
| `0x00585c00` | `budget -= amount` |
| `0x00585c40` | `budget += amount`; generic income counters |
| `0x00585cc0` | `budget -= amount`; expense group A |
| `0x00585cf0` | `budget -= amount`; expense group C |
| `0x00585d20` | `budget -= amount`; expense group B |
| `0x00585d50` | `budget -= amount`; attraction expense group |
| `0x00585d80` | `budget -= amount`; per-category shop expense |
| `0x00585de0` | `budget += amount`; ticket income counters |

Every setter raises the one-frame dirty flag at `+0x04`.

Known call sites:

- visitor entry `0x005883f0` → `0x00585de0` (ticket income)
- daily settlement `0x0058ad00` → `0x00585d80`, `0x00585d50`,
  `0x00585de0`
- sale/purchase UI `0x00533a00` … `0x005341b0` → `0x00585c40`
- show/economy `0x0060ac40`, `0x0060c490`, `0x0060d9a0` →
  `0x00585c40` / `0x00585cf0`

## Daily settlement (`0x0058ad00`)

1. For each shop/stall: charge the category price (`0x585d80`).
2. For each attraction: charge the gate/operation price (`0x585d50`).
3. For each visitor: add the ticket value (`0x585de0`).
4. `0x585e10` rolls current-day counters into last-day/cumulative blocks and
   updates bankruptcy:
   - budget < 0 → set `+0x94`;
   - still < 0 while flagged → set `+0x95`;
   - budget ≥ 0 → clear `+0x94`.
5. Reset the shops' daily counters (`+0x38` → `+0x3c`) and attraction counters.

**Trigger.** `0x00524630` (park daily update) checks the schedule-manager
time field `+0x29` (end of day). When set, it calls `0x0058ae60` (daily actor
reset), `0x0058c2d0` (day counter `+0x04 += 1`, clear flags, reset elapsed
time) and then `0x0058ad00`. The clock itself is documented in
`docs/decomp/park-time.md`; the day ends at hour 11.

## Fixed category labels

The budget panel constructor `0x0050fd30` resolves 14 rows in this order
(the widget pointers are assigned in the same order for the cost and quantity
columns):

| Row | Widget stem | Meaning |
| ---: | --- | --- |
| 0 | `BUDGET_FOOD` | food / feeding cost |
| 1 | `BUDGET_EFFECTIFS_FOOD` | nutritionist staff |
| 2 | `BUDGET_EFFECTIFS_SECU` | guard staff |
| 3 | `BUDGET_DINOACHAT` | dinosaur purchase |
| 4 | `BUDGET_LABOACTION` | laboratory action |
| 5 | `BUDGET_CONSTRUCTION` | construction |
| 6 | `BUDGET_ENTREES` | admissions / tickets |
| 7 | `BUDGET_RESTOS` | restaurants |
| 8 | `BUDGET_BARAQUES` | stalls |
| 9 | `BUDGET_SOUVENIRS` | gift shops |
| 10 | `BUDGET_BOITNOIRE` | "black box" (lab device) |
| 11 | `BUDGET_POINTDEVUE` | viewpoint attraction |
| 12 | `BUDGET_GRADINS` | grandstands |
| 13 | `BUDGET_VENTES` | sales |

The `spPark` per-category array at `+0x1c` has 13/14 counters; the exact
index-to-row mapping is still to be confirmed against a populated save.
The `VISITEURS` widget is the visitor total, not a spend row.

## Cumulative counters

`0x585e10` keeps:

- current-day counters at `+0x0c..+0xb8`;
- last-day/cumulative counters at `+0x150..+0x220`.

It copies current → last-day, adds current into cumulative, then checks
bankruptcy. The UI budget ledger reads these blocks.

## Relation to `.sav`

The save park block uses the same relative offsets, so the counters can be
read directly from a save. Both retail saves are fresh parks (zero counters,
non-zero budget). Two saves taken after different purchases are the quickest
way to label the fixed categories.

## Evidence

- `notes/park-economy-recon.md` — decompiled functions and call sites.
- `docs/formats/sav.md` — park block layout and offsets.
- `tools/formats/sav_graph.py` — exposes the budget from retail saves.
