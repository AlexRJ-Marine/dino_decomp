# Park budget and daily economy (SIM-4 recon)

Date: 2026-10-25
Scope: static decompilation of the `spPark` economy API. No writes to `game/`.

Status: **budget/spend/income API decoded; daily settlement ordering and
counter fields mapped; UI category labels and the complete panel row mapping
solved (see section 7; sections 1–6 keep the original wording).** Sections 1–6
predate the 2026-10-26 save-stream correction: their `+0x0c`/`+0x14`/`+0x24`
rows are **runtime** offsets and do not describe the serialized save order
(see “Save stream” at the end of section 7).

## 1. Budget field

`spPark` keeps the current budget at **`+0x08`**. The setter is
`FUN_00585bd0(budget, sellback)`; the save reader `FUN_00585240` reads the
same field, and `sav_graph.py` exposes it as `park.budget`.

The save park block also carries the economy counters:

| Relative to `park block + 0x28` | Meaning |
| ---: | --- |
| `+0x08` | current budget |
| `+0x0c` | float `1.0` (sellback factor / difficulty) |
| `+0x14`/`+0x18` | expense group total / count |
| `+0x1c + cat*4` | per-category expense counters |
| `+0x8c`/`+0x90` | ticket income total / count |
| `+0x9c`/`+0xa0` | expense group A total / count |
| `+0xa4`/`+0xa8` | expense group B total / count |
| `+0xac`/`+0xb0` | expense group C total / count |
| `+0xb4`/`+0xb8` | generic income total / count |
| `+0x94`/`+0x95` | bankruptcy flags |

The retail save values are consistent with this: the manual save has budget
75,125 and the autosave 75,000; the remaining counters are zero in the fresh
park.

## 2. Budget operations

| Address | Operation |
| --- | --- |
| `0x00585c00` | `budget -= amount`; sets dirty flag `+0x04` |
| `0x00585c40` | `budget += amount`; `+0xb4 += 1`, `+0xb8 += amount` |
| `0x00585cc0` | `budget -= amount`; `+0x9c += 1`, `+0xa0 += amount` |
| `0x00585cf0` | `budget -= amount`; `+0xac += 1`, `+0xb0 += amount` |
| `0x00585d20` | `budget -= amount`; `+0xa4 += 1`, `+0xa8 += amount` |
| `0x00585d50` | `budget -= amount`; `+0x14 += amount`, `+0x18 += 1` |
| `0x00585d80` | `budget -= amount`; `+0x1c + category*4 += amount` |
| `0x00585de0` | `budget += amount`; `+0x8c += amount`, `+0x90 += 1` |

All eight setters raise the `+0x04` dirty flag. They are the only writers of
the budget field in the normal park path.

Call sites observed:

- `0x00585de0` is called from `FUN_005883f0` when a visitor enters the park
  (ticket purchase), and again from the daily settlement `FUN_0058ad00`.
- `0x00585d80` is called from `FUN_0058ad00` for each shop/stall: the stall's
  `+0x50` structure supplies a price and category.
- `0x00585d50` is called from `FUN_0058ad00` for each attraction's gate price.
- `0x00585c40` is called from the sale/purchase UI (`0x00533ac0`,
  `0x00533a00`, `0x00533f30`, `0x00533de0`, `0x00534040`, `0x005341b0`),
  and from the show managers (`0x0060ac40`).
- `0x00585cf0` is called from the show/economy paths (`0x0060c490`,
  `0x0060d9a0`).
- `0x00585d20` has several purchase call sites; the fixed categories are not
  yet named.

## 3. Daily settlement (`0x0058ad00`)

`FUN_0058ad00` is the end-of-day/refresh pass:

1. For each shop/stall in `park+0xe4..0xe8`:
   `0x585d80(price, category)` (expense by category).
2. For each attraction in `park+0x90..0x94`:
   `0x585d50(gate_price)` (attraction expense).
3. For each visitor in `park+0x80..0x84`:
   `0x585de0(ticket_value)` (ticket income).
4. `0x585e10` rolls the daily counters into the cumulative/last-day fields and
   updates bankruptcy:
   - if budget < 0 and not already flagged: `+0x94 = 1`;
   - if budget is still < 0 while flagged: `+0x95 = 1`;
   - clears `+0x94` when the budget is non-negative again.
5. Resets each shop's daily `+0x38` counter into `+0x3c`, and clears the
   attraction counters.

`0x58ad00` is reached from the park interface update `0x00524630`. The exact
trigger (day rollover vs. a manual refresh) is still open.

## 4. Cumulative counters (`0x00585e10`)

`0x585e10` keeps two copies of the economy counters:

- current-day values at `+0x0c..+0xb8`;
- last-day/cumulative values at `+0x150..+0x220`.

It copies the current values into the last-day block, adds them into the
cumulative block (`+0x170..+0x220`), and then evaluates the bankruptcy flags.
The UI budget ledger reads these blocks; the exact row labels are still open.

## 5. Relation to the save graph

The save park block (0x293 bytes) contains the current-day counters at the
same relative offsets decoded by `sav_graph.py`. A save taken after a day of
play would therefore expose the category totals directly; both current retail
saves are fresh parks, so the counters are zero and only the budget is
non-zero.

This is the first place to verify the category labels: take two saves after
different purchases and diff `+0x9c..+0xb8` and the `+0x1c` array.

## 6. Open

- Names of the fixed expense categories (`+0x9c`, `+0xa4`, `+0xac`, `+0xb4`).
- Whether `0x58ad00` runs on a day boundary, a timer, or a UI refresh.
- The exact ticket value source (`visitor profile` vs `park` field).
- The UI budget-ledger row mapping.
- Bankruptcy consequences (`+0x95`).

## Evidence

- Ghidra: `0x00585bd0`, `0x00585c00`, `0x00585c40`, `0x00585cc0`,
  `0x00585cf0`, `0x00585d20`, `0x00585d50`, `0x00585d80`, `0x00585de0`,
  `0x00585e10`, `0x005883f0`, `0x0058ad00`, `0x00585240`.
- Save evidence: `notes/save-graph-recon.md`, `tools/formats/sav_graph.py`
  (budget at park block `+0x30`).

## 7. Budget-panel row mapping (solved)

The panel is not a flat `spPark+0x1c[14]` array. `FUN_0050fd30` builds a
`0xa4`-byte panel object; `FUN_00510430` refreshes it and reads one exact
counter per row. This closes the �index-to-label� question: the old `+0x1c`
array is only a two-entry staff-expense bucket array.

Panel layout (`0x0050fd30`):

- `+0x1c` economy object (`*(park + 0x120)`)
- `+0x20` title text
- `+0x24 + row*4` count text, rows 0..13
- `+0x5c + row*4` amount text, rows 0..13
- `+0x98` / `+0x9c` / `+0xa0` total expenses / income / net
- `+0x94` back-pointer to `spPark`

`0x00586070` recomputes `+0x144` (expenses), `+0x148` (income) and `+0x14c`
(net) and returns `economy + 0xbc`; `0x00510430` uses that as `EDI`.
All row sources are the **yesterday** block (`+0xbc..+0x16c`), so the panel
shows the last settled day:

| Row | Label | Amount | Count |
| ---: | --- | ---: | ---: |
| 0 | FOOD | `+0xc4` | `+0xc8` |
| 1 | EFFECTIFS_FOOD | `+0xbc` | count of employees with `def[0] == 0` |
| 2 | EFFECTIFS_SECU | `+0xcc` | `+0xd0` |
| 3 | DINOACHAT | `+0x154` | `+0x150` |
| 4 | LABOACTION | `+0x164` | `+0x160` |
| 5 | CONSTRUCTION | `+0x15c` | `+0x158` |
| 6 | ENTREES | `+0x13c` | `+0x140` |
| 7 | RESTOS | `+0xd8` | `+0x10c` |
| 8 | BARAQUES | `+0xdc` | `+0x110` |
| 9 | SOUVENIRS | `+0xe0` | `+0x114` |
| 10 | BOITNOIRE | `+0xe4` | `+0x118` |
| 11 | POINTDEVUE | `+0xe8` | `+0x11c` |
| 12 | GRADINS | `+0xec` | `+0x120` |
| 13 | VENTES | `+0x16c` | `+0x168` |
| � | totals | `+0x144` / `+0x148` / `+0x14c` | � |

The corresponding current-day sources are `+0x0c/+0x10`, `+0x1c`, `+0x14/+0x18`,
`+0xa0/+0x9c`, `+0xb0/+0xac`, `+0xa8/+0xa4`, `+0x8c/+0x90`,
`+0x24+kind*4` / `+0x58+kind*4` for kinds 1..6, and `+0xb8/+0xb4`.
`0x585db0` is the shop-sale setter (`+0x24+kind*4` amount, `+0x58+kind*4`
count, budget += amount); `0x585c90` is the FOOD setter (`+0x0c/+0x10`).
Neither was in the original eight-setter table.

### Employee roles

`0x00541940` loads `employeelist.ini` into `0xb0`-byte graphics/profile
records at `manager+0xf8`. The retail list has two entries; the matching
`.bei` `kind_id` values are `0` = Nutritionist and `1` = Guard. `0x0053cf10`
creates an employee; `0x00588600` pushes it into `spPark+0xe4..0xe8`, charges
`entry->+0x50->[8]` (hire cost) and `0x58ad00` charges the same value as the
daily wage. The panel's EFFECTIFS_FOOD count is the number of entries with
`entry->+0x50->[0] == 0`.

Caveat: both retail saves have no employees, so both runtime staff buckets
are 0 and the Guard role index is untested. The 125/2 pair seen in the raw
save is the **current-day ticket income** (serialized at stream `+0x18/+0x1c`),
not `+0x20/+0x24`; see “Save stream” below.

The `spPark+0x90..0x94` pass uses `obj->+0x40->+0x20` and feeds
`+0x14/+0x18` (EFFECTIFS_SECU); the exact actor set of that vector is still
worth a runtime check, but the panel-side mapping is proven.

### Save stream (writer `0x00585780`, reader `0x00585240`)

The save park block is not a raw object dump. It has a 0x30-byte header and
then a sequential stream in writer order:

```
+0x08, +0x98, +0x0c, +0x10, +0x14, +0x18, +0x8c, +0x90,
+0x94(u8), +0x95(u8), +0x1c x2, +0x24 x13, +0x58 x13,
+0x9c..+0xb8 (4 pairs), +0xbc x2, +0xc4, +0xc8, +0xcc, +0xd0,
+0xd4 x13, +0x108 x13, +0x13c..+0x16c (13), +0x170 x2,
+0x178..+0x184, +0x188 x13, +0x1bc x13, +0x1f0..+0x220
```

The stream starts at `block + 0x30`. The budget is the first dword. The manual
save's current-day ticket pair is 125/2 at stream `+0x18/+0x1c`. The stream is
0x21a bytes and ends with a 180-byte cumulative block. The three computed
totals at runtime `+0x144/+0x148/+0x14c` are the 2nd..4th dwords of the
13-dword totals group (stream `+0x132/+0x136/+0x13a`, and again in the
cumulative block at `+0x1e6/+0x1ea/+0x1ee`); they contain the debug fill
`0xBAADF00D` in a fresh save because the panel refresh never wrote them. The
yesterday counters are zero until the first day roll.

**Reader asymmetry.** `0x00585240` reads the cumulative block (version >= 46)
into the yesterday fields `+0xbc..+0x16c`, and starts it 32 bytes early
because it consumes only 5 of the 13 first-block total dwords. This is a
retail loader bug; `sav_graph._read_park_stream` follows the writer layout and
exposes `yesterday_*` / `cumulative_*` separately. The final 0x49 bytes of
the 0x293-byte park block are the schedule tail (0x3d bytes) and a 12-byte
field. `tools/formats/economy.py` uses the yesterday block, matching the panel
refresh.

### Evidence

- Ghidra: `0x0050fd30`, `0x00510430`, `0x00586070`, `0x00585c90`,
  `0x00585db0`, `0x00585780` (save writer), `0x00585240` (save reader),
  `0x00541940`, `0x0053cf10`, `0x00588600`.
- Retail `employeelist.ini` (2 entries) and `.bei` `kind_id` values.
