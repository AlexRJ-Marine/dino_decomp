# Paddock Feeding Dish switch (`SWITCH_FENCE_LUNCHBOX`)

Date: 2026-10-31. Read-only Ghidra pass on the retail `DinoIsland.exe`.

## Question

How does the paddock info panel's Feeding Dish switch place a LunchBox, and
what does it charge?

## Answer in one line

The switch **arms feature placement**; the next world ground click creates one
LunchBox (`.bfi` class `0x16`, tag `"LunchBox"`) at that ground point. The cap
is **16 dishes per fence**, the price is the `.bfi` price **100**, and the
charge goes to the **CONSTRUCTION** ledger (`0x585d20`, `park+0x120`, group B
`+0xa4/+0xa8`) after the actor is committed. The switch does not toggle or
remove a dish.

## 1. Switch handler `0x00531c80`

```asm
00531c88 CALL 0x0052aca0        ; reset preview/cursor state
00531c8d MOV ECX,[0x006749e4]   ; current UI list selection
00531cba CALL [vtable+4]        ; selection type
00531cbd CMP EAX,0x16           ; 0x16 = LunchBox feature class
00531cc4 SETZ BL
00531cce JZ 0x531d63            ; not LunchBox -> return
00531cd6 CALL 0x0052bbb0        ; enter feature-placement mode
```

`FUN_0052bbb0` sets `state+0x78` = selected feature record, `state+0x48` = 1
(feature mode), `state+0x88` = selected object. A second click resets the
preview and re-enters feature mode; there is **no toggle-off/remove** in the
switch.

UI: `SWITCH_FENCE_LUNCHBOX` in `FENCE_FEATURE_GROUP`; click `(620,243)`,
rect `(608,231,25x25)`, panel-relative `43,45`, label "Feeding Dish"
(`docs/platform/ui-coordinates.md`).

## 2. Placement click `FUN_0052b0d0` (vtable `0x642b4c` slot `+0x14`)

At `0x52b242` (feature mode):

```text
ground = FUN_005f8b20(scene)             ; mouse ground point
rect   = FUN_0052bcb0(ground)            ; cell = floor(ground) - paddock origin
if FUN_005812b0(scene, rect, orientation, size) != 0 -> warn (footprint)
if vtable+0x30() != 0 -> warn            ; 0x527150 cap
if FUN_0052bed0(cost) == 0 -> WARNING_NOT_ENOUGH_MONEY
FUN_0053acd0(...)                        ; create the feature actor
FUN_0053b0d0(...)                        ; AnimalBait only
[vtable+0x40]()                          ; feature commit (0x527370)
ledger = park+0x120
FUN_00585d20(ledger, featureRecord+0x1c); charge
```

- Position = the mouse click ground point, not a fixed dish slot. Each click
  creates **one** LunchBox.
- `LunchBox.bfi`: width 4, height 4, vertical 4.0, `model_scale` 1.0,
  price 100, class 0x16, tag `"LunchBox"`, actions
  `{Eat, EatForSmallies}`.
- `FUN_0052bed0` is only the budget gate (budget+8 < cost ->
  `&WARNING_NOT_ENOUGH_MONEY`); it does not charge.

## 3. Cap `0x00527150` (vtable `+0x30`)

```asm
count = (fence+0xdc - fence+0xd8) >> 2
CMP count, 0x10
JC ok          ; count < 16 allowed
MOV EAX,6      ; blocked -> WARNING_TOO_MANY_LUNCHOXES_IN_FENCE
```

The check runs before creation, so with 15 dishes the 16th is allowed and the
17th is blocked. **Effective max = 16**, not 15. There is no post-add
rollback.

### 3b. Placement validity before the cap

`0x52b242` also calls `FUN_005812b0(scene, rect, orientation, size)` before
the cap/budget gates. It tests the feature footprint rectangle (LunchBox
4x4 at orientation 0/2; 1/3 swaps a square) against the occupied-rect vector
at `state+0x34..+0x38` (`0x14`-byte records, min at `+4/+8`, max at
`+0xc/+0x10`) and returns 1 on overlap. The caller then shows warning code 1
= `&WARNING_BUSY_SPACE` (`0x66350c`) at `0x52c2e0`. It does **not** read the
terrain occupancy grid; the check is only the feature rect list. The exact
append of the new feature's rect into that vector was not isolated — OPEN.

### 3c. Feature mode persists after a placement

The tail after the charge is `0x52b347 JMP 0x52b372`; in feature mode
(`state+0x5c == 0`) it returns at `0x52b586` without writing `+0x48`,
`+0x78` or `+0x88`. `FUN_0053acd0`, `FUN_0053b0d0`, `vtable+0x40`
(`0x527370`) and `FUN_005812b0` do not clear `+0x48` either. Each click
re-reads the ground point and re-runs the cap (`0x52b2d2`) and budget gate
(`0x52b3f0`), so dish-after-dish placement without re-clicking the switch is
intended. Mode ends on right-click/leave (`0x52b562` -> `state+0x18 = 5`,
`vtable+0x8` = `0x5269c0`), Esc (`0x52be60`), or a selection/enter refresh
(`0x52bb70`).

## 4. Dish list push `FUN_0057e440` (stFence vtable `0x6432ac` slot `+0x0c`)

`FUN_0057e440` detects the feature class tag `"LunchBox"` (`0x66303c`) and
appends a 4-byte pointer to the 0xb4 stFence feature wrapper:

```asm
0057e4db MOV EAX,[ESI+0xdc]   ; end
0057e4e7 LEA ECX,[ESI+0xd8]   ; vector
0057e4f9 MOV [EAX],EDX        ; push wrapper pointer
0057e4fe ADD EAX,4
0057e501 MOV [ECX+4],EAX      ; end++
```

The dish position lives in the feature actor (the ground point), not in the
`stFence+0xd8` record. Removal is `FUN_0057e630` (generic feature erase), not
the switch.

## 5. Charge `0x00585d20`

`FUN_00585d20(ledger, amount)`:
`ledger+4 = 1`, `ledger+0xa4++` (count), `ledger+0xa8 += amount` (total),
`ledger+8 -= amount` (remaining budget). `0x585d20` is the **CONSTRUCTION**
group B setter (`+0xa4/+0xa8`); `park+0x120` is the ledger object.

## Port recipe

```text
switch click (620,243):
    pending_build_tool = 7        // feature mode; no dish yet

next world ground click:
    (mx,my) = pick_terrain(...) + world/2
    fence = paddock at (mx,my) (or the selected fence)
    if not inside fence -> warn
    cell = floor(mx),floor(my); rect = cell..cell+3   // 4x4 footprint
    if rect overlaps an existing feature rect -> WARNING_BUSY_SPACE
    if LunchBox count in fence >= 16 -> WARNING_TOO_MANY_LUNCHOXES
    if budget < 100 -> WARNING_NOT_ENOUGH_MONEY
    create Feeder (LunchBox.bfi: quantity 10000, nutrition 100, actions
        {Eat,EatForSmallies}, radius 2) at (mx,my)
    append to park_session.feeders / fence dish list
    budget -= 100; ledger.spend_construction(100)
    keep feature mode (the switch is not re-clicked per dish)
```

The port's `.bfi` display path (`--place-lunchbox`) already creates the
`Feeder` + `bowl.mesh` draw; the interactive path reuses it at the clicked
point and adds the overlap/cap/CONSTRUCTION checks. The switch itself only
arms `pending_build_tool = 7`; the per-frame routing calls
`ensure_build_ready()` and applies the click through `apply_build_tool_click`.
The `--lunchbox-prefill N` test hook fills N dishes in a 3-column, 4-unit
row grid in the first paddock for the cap test.

## Evidence

- Ghidra: `0x00531c80`, `0x0052bbb0`, `0x0052b0d0`, `0x0052bed0`,
  `0x0053acd0`, `0x0053b0d0`, `0x00527150`, `0x00527030`, `0x0057e440`,
  `0x0057e630`, `0x00585d20`.
- `Data/Design/attractions/features/LunchBox.bfi`;
  `docs/platform/ui-coordinates.md` (`SWITCH_FENCE_LUNCHBOX`);
  `docs/decomp/economy.md` (`0x585d20` = CONSTRUCTION).
- `notes/show-live-actor-recon.md` for the feature actor creation.
