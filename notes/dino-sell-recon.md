# Dino sell flow — decompiled economy path

Date: 2026-10-31. Read-only Ghidra pass; no runtime probe in this pass.

## Proven

- `FUN_00533a00(panel)` (`0x533a00`):
  ```c
  FUN_00585c40(*(int *)(*(int *)(panel+0x50) + 0x1ec));  // dino market value
  FUN_0053a900(*(int *)(panel+0x50));                    // remove from actor list
  FUN_0053b3d0();                                        // refresh scene
  ```
  The dinosaur pointer is `panel+0x50`; `dino+0x1ec` is the definition copy of
  `def+0x19c` (the genotype-rarity value; see
  `notes/dino-stats-economy-gap-audit.md` §5.2).
- `FUN_00585c40(value)` (`0x585c40`):
  ```c
  i = __ftol();                    // the value passed on the FP stack
  economy[0xb4] += 1;              // sale count
  economy[0xb8] += i;              // VENTES total
  economy[0x08] += i;              // budget
  economy[0x04] = 1;               // dirty
  ```
  This is the VENTES/budget credit; no park counter is touched.
- The value is the **gene-rarity** `def+0x19c`, not the `Market.ini`
  purchase price. The audit's refund form is
  `ftol(value * SellBackFactor)` (the factor is applied before
  `FUN_00585c40`; the FPU argument is lost in the decompiled signature).
- Removal: `FUN_0053a900` walks the actor list, unregisters the dino and
  deletes it; `FUN_0053b3d0` refreshes the scene.

## Port status (2026-10-31)

1. [x] `DinoAttributes::market_value` — rarity weights parsed for all 18
   genes + price table 500/100/50; all 20 Cpg04 `DINORESET f1ec` values
   match (`notes/phenotype-attributes-runtime.md`).
2. [x] `Game::sell_dino(index)`:
   - `refund = (int)(market_value * sellback)`;
   - `budget += refund`, `ledger.add_vente(refund)` (VENTES count/total);
   - `paddock_remove` subtracts the size-class capacity cost;
   - removes the dino and its `imported_dino_draws` entry.
   Called from `boot_sell_dino()` **after** `boot_park_session()` so the
   park-session budget reset cannot wipe the credit.
3. [x] Test hook `--sell-dino NAME` (last matching imported dino, so earlier
   indices do not shift). Cpg04 log:
   `sell dino: anky value=2400 refund=2400 budget=3400 ventes=1/2400
   pen=4 used=48 sellback=1.00 dinos=19` with `--park-budget 1000`
   (`test_cpp_dino_sell`).
4. [x] Stable `ImportedDino::id` (2026-10-31): every import/buy gets a
   unique id from `ParkSession::next_imported_dino_id`; paddock occupants,
   the info-panel diet lookup and `sell_dino` use it. The `.psav` writer
   appends the id; older saves (no id) restore the 0-based import index so
   their occupant lists still resolve. Campaign Cpg04 ids are 1..20.
5. [ ] Interactive `REQUEST_SELL_DINO` hit test/panel path (`0x532d9c..`);
   the stable id is now in place, only the UI selection remains.

## Open

- The exact caller/UI of `FUN_00533a00` (`REQUEST_SELL_DINO` panel path) is
  not traced here; the economy path above is the block to port.
- Whether the carcass/actor list removal frees the paddock capacity
  (`saDinosaur` cost index) is implied by `FUN_0053a900` but not traced
  through `spPark`.

## Evidence

- Ghidra `FUN_00533a00`, `FUN_00585c40`, `FUN_0053a900`, `FUN_0053b3d0`.
- `notes/dino-stats-economy-gap-audit.md` §5.2/G7/G8.
