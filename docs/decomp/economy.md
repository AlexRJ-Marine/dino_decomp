# Park budget and daily economy

Status: **decompiled**; the budget counters, the day trigger and the complete
budget-panel row mapping are solved. The panel refresh `0x00510430` reads a
specific counter for every one of its 14 rows, so the old “`spPark+0x1c`
index = row” question is answered: `+0x1c` is a two-entry staff-expense
bucket array, not a 14-entry row array.
The day trigger: `0x00524630` fires `0x0058ad00` when the park clock's
end-of-day flag is set (`docs/decomp/park-time.md`).

## Economy object

`spPark` creates the economy object with `FUN_00585150` and stores the pointer
at **`spPark+0x120`** (`spPark` constructor `0x005874a0`). The 0x293-byte save
park block serializes this object after a 0x28-byte object header, so every
offset below is relative to the object and to `save block +0x28` (the older
notes' “park block `+0x08`” is the same field as `save block +0x30`).

## Budget

The current budget is at **`+0x08`**. `FUN_00585bd0` sets it; the save reader
`FUN_00585240` reads it. The retail saves contain:

- manual: 75,125
- autosave: 75,000

The object carries the current-day economy counters:

| Offset | Meaning |
| ---: | --- |
| `+0x08` | budget |
| `+0x0c` / `+0x10` | FOOD total / count |
| `+0x14` / `+0x18` | EFFECTIFS_SECU total / count |
| `+0x1c` / `+0x20` | staff expense bucket by descriptor role (0/1; Guard index open) |
| `+0x24 + k*4` | shop revenue by shop kind `k` (0..12) |
| `+0x58 + k*4` | shop sale count by shop kind `k` (0..12) |
| `+0x8c` / `+0x90` | ticket (ENTREES) income total / count |
| `+0x94` / `+0x95` | bankruptcy flags |
| `+0x9c` / `+0xa0` | DINOACHAT (group A) count / total |
| `+0xa4` / `+0xa8` | CONSTRUCTION (group B) count / total |
| `+0xac` / `+0xb0` | LABOACTION (group C) count / total |
| `+0xb4` / `+0xb8` | VENTES generic-income count / total |
| `+0xbc` / `+0xc0` | yesterday copy of staff buckets 0 / 1 |
| `+0xc4` .. `+0x138` | yesterday copies of FOOD, EFFECTIFS_SECU and sales amounts/counts |
| `+0x13c` / `+0x140` | yesterday ticket income total / count |
| `+0x144` / `+0x148` / `+0x14c` | total expenses / total income / net |
| `+0x150` .. `+0x16c` | yesterday copies of the four group counters |

The “current” and “yesterday” blocks are rolled by `0x00585e10`: at the end of
a day it copies each current-day counter into its matching yesterday slot
(`+0xc4..+0x16c`) and adds it into the cumulative block (`+0x170..+0x220`).

## Budget API

| Address | Operation |
| --- | --- |
| `0x00585bd0` | set budget, clear bankruptcy flags, reset counters |
| `0x00585c00` | `budget -= amount` (generic purchase) |
| `0x00585c40` | `budget += amount`; VENTES counters `+0xb4` / `+0xb8` |
| `0x00585c90` | `budget -= amount`; FOOD counters `+0x0c` / `+0x10` |
| `0x00585cc0` | `budget -= amount`; DINOACHAT group A `+0x9c` / `+0xa0` |
| `0x00585cf0` | `budget -= amount`; LABOACTION group C `+0xac` / `+0xb0` |
| `0x00585d20` | `budget -= amount`; CONSTRUCTION group B `+0xa4` / `+0xa8` |
| `0x00585d50` | `budget -= amount`; EFFECTIFS_SECU `+0x14` / `+0x18` |
| `0x00585d80` | `budget -= amount`; staff bucket `+0x1c + role*4` |
| `0x00585db0` | `budget += amount`; shop revenue `+0x24 + kind*4`, count `+0x58 + kind*4` |
| `0x00585de0` | `budget += amount`; ENTREES `+0x8c` / `+0x90` |

Every setter raises the one-frame dirty flag at `+0x04`.

Known call sites:

- employee hire `0x00588600` → `0x00585c00` (cost `descriptor[8]`)
- daily settlement `0x0058ad00` → `0x00585d50`, `0x00585d80`,
  `0x00585de0`
- sale/purchase UI `0x00533a00` … `0x005341b0` → `0x00585c40`
- show/economy `0x0060ac40`, `0x0060c490`, `0x0060d9a0` →
  `0x00585c40` / `0x00585cf0`

## Daily settlement (`0x0058ad00`)

1. For each employee in `spPark+0xe4..0xe8`: read the descriptor at
   `entry+0x50`, charge its wage (`descriptor[8]`) with
   `0x585d80(wage, role)`, and use `descriptor[0]` as the role index of the
   `+0x1c + role*4` staff bucket.
2. For each actor in `spPark+0x90..0x94`: charge `obj->profile->+0x20` with
   `0x585d50` (EFFECTIFS_SECU counters).
3. For each visitor in `spPark+0x80..0x84`: add the ticket value with
   `0x585de0`.
4. `0x585e10` rolls the current counters into the yesterday/cumulative blocks
   and updates bankruptcy:
   - budget < 0 → set `+0x94`;
   - still < 0 while flagged → set `+0x95`;
   - budget ≥ 0 → clear `+0x94`.
5. Reset the actors' daily counters (`+0x38` → `+0x3c`) and the shop counters.

**Trigger.** `0x00524630` (park daily update) checks the schedule-manager
time field `+0x29` (end of day). When set, it calls `0x0058ae60` (daily actor
reset), `0x0058c2d0` (day counter `+0x04 += 1`, clear flags, reset elapsed
time) and then `0x0058ad00`. The clock itself is documented in
`docs/decomp/park-time.md`; the day ends at hour 11.

## Port settlement (P4c)

`src/viewer/main.cpp` runs `run_daily_settlement()` when `ParkClock` sets
`end_of_day`, before `next_day()`:

```text
ticket_price = Entrance .bbi visitor price (offset 25, 50 in retail)
income       = visitor_count * ticket_price
wages        = 0                     // no employee actors yet
park_budget += income - wages
```

`--settle-day` runs one settlement at startup for tests. 8 visitors on the
24-cell loop gives `income=400`, `budget=75400`. The exact per-visitor retail
ticket value is still inferred: the manual save's current-day ticket pair is
`125/2`, not `2 * 50`; difficulty/age multipliers are not decoded. Employee
wages (`0x585d80`) and the EFFECTIFS_SECU pass (`0x585d50`) are implemented as
zero until the staff/animal actors exist.

## Budget panel (`0x00510430`)

The panel constructor `0x0050fd30` resolves 14 rows in this order and stores
two text widgets per row:

- amount column `+0x5c + row*4`
- count column `+0x24 + row*4`

The refresh `0x00510430` calls `0x00586070` first; that function recomputes
`+0x144` (total expenses), `+0x148` (total income) and `+0x14c` (net), and
returns a pointer to `+0xbc`. The refresh then fills each row from these
counters (all sources are the **yesterday** block, so the panel shows the last
settled day until the next rollover):

| Row | Label | Amount source (read) | Count source (read) | Current-day origin |
| ---: | --- | ---: | ---: | --- |
| 0 | FOOD | `+0xc4` | `+0xc8` | `+0x0c` / `+0x10` |
| 1 | EFFECTIFS_FOOD | `+0xbc` | live employee count | role-0 bucket `+0x1c` |
| 2 | EFFECTIFS_SECU | `+0xcc` | `+0xd0` | `+0x14` / `+0x18` |
| 3 | DINOACHAT | `+0x154` | `+0x150` | `+0xa0` / `+0x9c` |
| 4 | LABOACTION | `+0x164` | `+0x160` | `+0xb0` / `+0xac` |
| 5 | CONSTRUCTION | `+0x15c` | `+0x158` | `+0xa8` / `+0xa4` |
| 6 | ENTREES | `+0x13c` | `+0x140` | `+0x8c` / `+0x90` |
| 7 | RESTOS | `+0xd8` | `+0x10c` | `+0x24+1*4` / `+0x58+1*4` |
| 8 | BARAQUES | `+0xdc` | `+0x110` | `+0x24+2*4` / `+0x58+2*4` |
| 9 | SOUVENIRS | `+0xe0` | `+0x114` | `+0x24+3*4` / `+0x58+3*4` |
| 10 | BOITNOIRE | `+0xe4` | `+0x118` | `+0x24+4*4` / `+0x58+4*4` |
| 11 | POINTDEVUE | `+0xe8` | `+0x11c` | `+0x24+5*4` / `+0x58+5*4` |
| 12 | GRADINS | `+0xec` | `+0x120` | `+0x24+6*4` / `+0x58+6*4` |
| 13 | VENTES | `+0x16c` | `+0x168` | `+0xb8` / `+0xb4` |
| — | TOTAL_LEFT | `+0x144` | — | total expenses |
| — | TOTAL_RIGHT | `+0x148` | — | total income |
| — | TOTAL_PRICE | `+0x14c` | — | net |

Shop kinds 0 and 7..12 are not shown by the panel; only kinds 1..6 are wired
to a row. The row labels themselves come from the widget names validated in
`docs/platform/ui-original-menu.md`.

## Staff roles and costs

`FUN_00541940` loads `employeelist.ini` into a table of `0xb0`-byte employee
graphics/profile records at `manager+0xf8`. The retail list has exactly two
entries and the matching `.bei` `kind_id` values are:

- `0` — Nutritionist (`Nutritionist.bei`, `feed_*` meshes)
- `1` — Guard (`Guard.bei`, `secu_*` meshes)

`FUN_0053cf10` creates one employee from a `drsEmployee` world object. The
runtime employee stores a descriptor pointer at `+0x50`; `FUN_00588600`
pushes the employee into `spPark+0xe4` and charges `descriptor[8]` as the
hire cost. The daily pass charges the same `descriptor[8]` as the wage, so
one value is used for both. The budget panel's EFFECTIFS_FOOD count is the
number of employees whose `descriptor[0] == 0`.

The role *index* in `descriptor[0]` is consistent with Nutritionist = 0
(the first `employeelist.ini` entry). The Guard index is untested: both retail
saves have no employees, so both staff buckets are 0. (The 125/2 pair seen in
the raw save belongs to the serialized ticket income, not to the staff
buckets; see “Relation to `.sav`”.)

## Cumulative counters

`0x585e10` keeps:

- current-day counters at `+0x0c..+0xb8`;
- yesterday counters at `+0xc4..+0x16c`;
- cumulative counters at `+0x170..+0x220`.

It copies current → yesterday, adds current into cumulative, then checks
bankruptcy. `0x586070` recomputes the three displayed totals from the
yesterday block only:

- expenses = yesterday staff buckets (`+0xbc/+0xc0`) + yesterday FOOD +
  yesterday EFFECTIFS_SECU + yesterday DINOACHAT/CONSTRUCTION/LABOACTION;
- income = the 13 yesterday shop revenues + yesterday VENTES + yesterday
  ENTREES;
- net = income − expenses.

## Relation to `.sav`

The save park block is **not** a raw memory dump. It has a 0x30-byte header,
then the economy field stream written by `0x00585780` in **writer order**:
budget, sellback, FOOD, EFFECTIFS_SECU, tickets, two flag bytes, the two staff
buckets, 13 shop revenues, 13 sale counts, the four group pairs, the yesterday
block and the cumulative block. The stream is 0x21a bytes; the park block's
remaining 0x49 bytes are the schedule tail and a 12-byte field. The loader
`0x00585240` reads the same stream in the same order, **except** that for
version >= 46 it reads the cumulative block into the yesterday fields and
starts it 32 bytes early (it consumes only 5 of the 13 first-block total
dwords). That is a retail loader bug; the parser follows the writer layout.
The reader is `sav_graph._read_park_stream`.

Consequences for save inspection:

- the budget is the first stream dword (`block + 0x30`);
- the manual save's current-day ticket pair is `125 / 2` (two visitors), which
  is stream `+0x18/+0x1c`, not runtime `+0x20/+0x24`;
- both staff buckets are 0 in both retail saves (no employees);
- the yesterday block starts at stream `+0xb2` and the cumulative block at
  stream `+0x166`; both end with a 13-dword totals group whose entries 2..4
  are the computed expenses/income/net.

**Fresh-save caveat.** The yesterday block `+0xc4..+0x16c` is zeroed at park
creation and only written by the first day rollover (`0x00585e10`). The three
computed totals `+0x144/+0x148/+0x14c` are only written by the panel refresh
(`0x00586070`), so a fresh save dumps the debug fill **`0xBAADF00D`** in those
three fields. `tools/formats/economy.py` masks the fill and reports the totals
as unsettled. Only a day roll populates the 14 row counters; opening the panel
updates the totals but not the rows.

A save taken after one in-game day is the quickest way to validate the row
mapping at runtime.

## Evidence

- Ghidra: `0x00585150` (economy ctor), `0x00585bd0`, `0x00585c00`,
  `0x00585c40`, `0x00585c90`, `0x00585cc0`, `0x00585cf0`, `0x00585d20`,
  `0x00585d50`, `0x00585d80`, `0x00585db0`, `0x00585de0`, `0x00585e10`,
  `0x00586070` (totals), `0x0058ad00` (daily settlement), `0x0050fd30`
  (budget panel ctor), `0x00510430` (panel refresh), `0x00541940`
  (`employeelist.ini`), `0x0053cf10` / `0x00588600` (employee creation).
- `notes/park-economy-recon.md` — decompiled functions, counter map and the
  panel row derivation.
- `docs/formats/sav.md` — park block layout and offsets.
- `tools/formats/sav_graph.py` — exposes the budget from retail saves.
- `tools/formats/economy.py` — decodes the 14 panel rows from a save and
  masks the `0xBAADF00D` fill; `tests/test_economy.py` checks it against the
  retail save.
