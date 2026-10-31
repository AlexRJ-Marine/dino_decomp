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

## Port plan

1. Finish `DinoAttributes::market_value` (rarity weights + price table
   `Rare/Uncommon/Common` 500/100/50) and verify it against the 20 Cpg04
   `DINORESET f1ec` values.
2. Add `ParkSession::sell_dino(index)`:
   - `refund = (int)(attributes.market_value * sellback)`;
   - `budget += refund`, ledger `add_vente(refund)`;
   - free the paddock capacity (`paddock_remove`);
   - remove the dino from `imported_dinos` (and the parallel draw list).
3. Expose it as a test hook first (`--sell-dino N|SPECIES`), then the
   interactive hit test/REQUEST_SELL_DINO panel.

## Open

- The exact caller/UI of `FUN_00533a00` (`REQUEST_SELL_DINO` panel path) is
  not traced here; the economy path above is the block to port.
- Whether the carcass/actor list removal frees the paddock capacity
  (`saDinosaur` cost index) is implied by `FUN_0053a900` but not traced
  through `spPark`.

## Evidence

- Ghidra `FUN_00533a00`, `FUN_00585c40`, `FUN_0053a900`, `FUN_0053b3d0`.
- `notes/dino-stats-economy-gap-audit.md` §5.2/G7/G8.
