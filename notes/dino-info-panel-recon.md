# Live dino info panel (`DINO_INFO_PANEL`) — widget map and open pick

Date: 2026-10-31. Read-only Ghidra + `Data/Overlay/Infopanels.Ini`.

## Retail panel

`ivwDinoInfo` ctor `0x511d80` (0x1c4 bytes), created from `FUN_0050b970`
param_1[6]. The overlay tree is in `Data/Overlay/Infopanels.Ini`:

```text
DINO_INFO_PANEL
├─ DINO_INFO_DINO_PIC            (portrait, selection-driven)
├─ DINO_INFO_MAINWOOD_PANEL
├─ DINO_INFO_MAIN_MENU
├─ DINO_INFO_VAL_CALCU_PANEL     (x=0 y=183 117x358, hidden by default)
│  ├─ ATTRIBUTES_<NAME>_SYMBOL
│  ├─ ATTRIBUTES_<NAME>_GAUGE_PANEL
│  │  ├─ ATTRIBUTES_<NAME>_GAUGE   (PROGRESS)
│  │  └─ ATTRIBUTES_<NAME>_INDICS  (text)
│  └─ ... one block per gauge ...
└─ DINO_INFO_BESOINS_PANEL
   ├─ DINO_INFO_BESOINS_FOOD_GROUP
   │  ├─ DINO_INFO_BESOINS_FOOD_ICON
   │  ├─ _BADSTATE / _MIDSTATE / _GOODSTATE
   └─ VIOLENCE / SECU / REPOS groups (same shape)
```

Gauge names: `PERCEPTION`, `BEAUTY`, `ATTACK`, `AGGRESSIVENESS`, `ARMOR`,
`SPEED`, `AGILITY`, `INTELLIGENCE`, `COURAGE`, `STAMINA`, `ORIGINALITY`.
The live fill (`0x5e2940`) formulas are already ported in `--dino-info`
(`src/app/game_boot_park.cpp`), in retail index order.

Tabs/controls (`docs/platform/ui-coordinates.json`, screen `park-dino-info`):

| control | click |
| --- | --- |
| `DINO_INFO_DINO_SELECTION_ICON` | 144,78 |
| `DINO_INFO_VAL_CALCU_BUTTON` | 44,131 |
| `DINO_INFO_BESOINS_BUTTON` | 74,131 |
| `DINO_INFO_FOOD_PREF_BUTTON` | 44,161 |
| `DINO_INFO_CALENDAR_BUTTON` | 74,161 |
| `DINO_NAME_SLOT` | 46,87 |
| `DINO_INFO_TELEPORTATION_ICON` | 142,30 |

The tab buttons toggle `DINO_INFO_VAL_CALCU_PANEL`,
`DINO_INFO_BESOINS_PANEL` and the other two sub-panels; the names are loaded
by the port's Overlay (they are in `ParkHud`'s panel list).

## Selection chain (park view)

The world-pick handler is an inline park-view method at `0x5396c0`
(no Ghidra boundary):

```text
005396c4 CALL 0x0052bde0        ; clear/arm the pick
005396c9 MOV EAX,[0x006749e4]   ; global selection manager
005396db CALL 0x00447b90        ; iterate its list
005396e4 CALL 0x00447c30        ; -> picked object
005396ed MOV [ESI+0x48],EAX     ; park view's selected object
005396fd MOV EAX,[ESI+0x48]
00539700 MOV EDX,[ECX+0x18]     ; info view object
00539703 MOV [EDX+0xc0],EAX     ; ivwDinoInfo selected actor
00539712 CALL 0x005135d0        ; FUN_005135d0(info, 1) -> show/fill
```

- `FUN_005135d0(panel, visible)` stores the flag, and on visible reads
  `panel+0xc0` (the selected actor), sets the panel's references, then calls
  `FUN_005e2940` (the 11 live gauges) and the tab fill
  (`FUN_005138b0`/`0x513b40`/`0x513ac0` for tabs 1/2/3).
- `0x5397f0` closes: `panel+0xc0` is left, `FUN_005135d0(panel, 0)`, and
  the park state returns to 8.
- `0x539750` is the alternate branch: it re-resolves the picked object via
  `FUN_005e0da0`, clears the previous selection, and calls
  `FUN_005135d0(panel, 0)` then `panel+0xc0 = new` and `(panel, 1)`.
- `FUN_0052bde0` only clears `[this+0x38]`; `FUN_005e4700` sets
  `scene+0x14 = 1` (a pick request). The actual screen-space hit test runs
  in the scene/renderer update and fills the `0x006749e4` selection
  manager; its producer (the only write to the global is the manager ctor
  `FUN_004036e0` at `0x40383b`) is the open engine lead.

So the UI side is fully mapped: the panel opens with the actor at
`panel+0xc0`, and the 11 gauges/needs are filled from it. Only the
scene-pick hit test remains before the interactive panel can be wired
without guessing its selection semantics.

## Sell text is not an Infopanels child

`0x532d8c` writes `&REQUEST_SELL_DINO` and
`ftol(dino+0x1ec*SellBackFactor)` into `[EBP+0x74]`/`[EBP+0x68]` after
testing `dino+0x65` (see `notes/dino-sell-recon.md`). No `SELL`/price widget
exists in `Infopanels.Ini`; `EBP` is the park-HUD/object-info object, not the
`ivwDinoInfo` overlay tree. The exact widget that receives those strings and
the world-pick that selects the dinosaur are still untraced.

## Port status

- `--dino-info NAME` prints every live value (11 gauges in `0x5e2940` order,
  4 needs/states, food/rest/health, value/refund); `test_cpp_dino_info`.
- The overlay tree is loadable by name; the panel is not opened or filled
  interactively yet. The selection world pick and the sell-text widget are
  the two open leads; do not invent a click radius or a sellable rule.

## Evidence

- Ghidra `0x511d80`, `0x5e2940`, `0x5e2be0`, `0x532d8c`.
- `Data/Overlay/Infopanels.Ini` sections 3731-4900.
- `docs/platform/ui-coordinates.json` screen `park-dino-info`.
