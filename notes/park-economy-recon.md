# Park budget and daily economy (SIM-4 recon)

Date: 2026-10-25
Scope: static decompilation of the `spPark` economy API. No writes to `game/`.

Status: **budget/spend/income API decoded; daily settlement ordering and
counter fields mapped; exact UI category labels still open.**

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
