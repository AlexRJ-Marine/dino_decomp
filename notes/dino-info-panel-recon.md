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
- **The hit test is a pixel-ID buffer.** `FUN_005e0da0(scene, x)` at
  `0x5e0da0` is exactly:

  ```c
  return *(u32 *)(scene->id_buffer(+0x8c)
                  + (scene->row(+0x10) * scene->width(+0x04) + x) * 4);
  ```

  `0x539750` calls it with the mouse pixel, adds the returned actor to the
  selection manager, clears the previous selection and reopens the panel
  (`panel+0xc0`). So the retail pick is a **rendered object-ID color/ID
  buffer**, not a ray or bounding box. The remaining renderer lead is the
  pass that fills `scene+0x8c`; the port would need a matching offscreen
  ID pass to select actors with retail semantics.

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
- `ParkHud::show_dino_info` opens `DINO_INFO_PANEL`/`DINO_INFO_DINO_PIC`,
  fills `DINO_NAME_SLOT`, the `ATTRIBUTES_<NAME>_GAUGE/_INDICS` set and the
  four `DINO_INFO_BESOINS_*` state icons (state 3 -> GOOD, per `0x5138b0`).
  `--dino-panel NAME` is the test opener; the 800x600 screenshot
  `build/dino-panel.png` shows the anky panel (`test_cpp_dino_info_panel`).
- **Selection is wired (2026-10-31).** `apply_park_click` calls
  `pick_dino_index`: the port keeps the last posed CPU mesh per dinosaur
  (`ImportedDino::pick_mesh`), builds the same 80-degree camera ray as
  `pick_terrain`, and takes the nearest triangle hit in front of the
  heightfield. A hit opens the panel (`dino pick: anky id=1` ->
  `dino panel open:`); an empty click closes it. The 800x600 capture
  `build/pick-panel.png` shows the clicked anky with the panel.
  `test_cpp_dino_click_opens_panel`.
- The retail hit test is a pixel-ID buffer; the port's ray/mesh test is the
  same selection for a dinosaur, but it only contains dinosaur meshes. A
  dinosaur behind a building/vegetation can still be picked (terrain
  occlusion is tested). The retail buffer draws every object, so a full
  object-ID pass remains the exact solution; no click radius or sellability
  rule is invented.
- Still open: the other three tabs (needs/visitor appreciation/calendar),
  the sell-text widget and the teleport/next-dino buttons.

## Evidence

- Ghidra `0x511d80`, `0x5e2940`, `0x5e2be0`, `0x532d8c`.
- `Data/Overlay/Infopanels.Ini` sections 3731-4900.
- `docs/platform/ui-coordinates.json` screen `park-dino-info`.
