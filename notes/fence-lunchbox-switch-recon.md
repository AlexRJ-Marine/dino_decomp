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
    pending_lunchbox_place = true   // feature mode; no dish yet

next world ground click:
    (mx,my) = pick_terrain(...) + world/2
    fence = paddock at (mx,my) (or the selected fence)
    if not inside fence -> warn
    if LunchBox count in fence >= 16 -> WARNING_TOO_MANY_LUNCHOXES
    if budget < 100 -> WARNING_NOT_ENOUGH_MONEY
    create Feeder (LunchBox.bfi: quantity 10000, nutrition 100, actions
        {Eat,EatForSmallies}, radius 2) at (mx,my)
    append to park_session.feeders / fence dish list
    budget -= 100; ledger.spend_construction(100)
    pending_lunchbox_place = false
```

The port's `.bfi` display path (`--place-lunchbox`) already creates the
`Feeder` + `bowl.mesh` draw; the interactive path reuses it at the clicked
point and adds the CONSTRUCTION charge/cap.

## Evidence

- Ghidra: `0x00531c80`, `0x0052bbb0`, `0x0052b0d0`, `0x0052bed0`,
  `0x0053acd0`, `0x0053b0d0`, `0x00527150`, `0x00527030`, `0x0057e440`,
  `0x0057e630`, `0x00585d20`.
- `Data/Design/attractions/features/LunchBox.bfi`;
  `docs/platform/ui-coordinates.md` (`SWITCH_FENCE_LUNCHBOX`);
  `docs/decomp/economy.md` (`0x585d20` = CONSTRUCTION).
- `notes/show-live-actor-recon.md` for the feature actor creation.
