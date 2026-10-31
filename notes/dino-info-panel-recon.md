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
