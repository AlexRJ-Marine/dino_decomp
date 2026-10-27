# Dinosaur store panel (`DINO_ACHAT_SAVE_*`)

Status: **research complete; panel, gauges, preview, toast and mode catalogs
implemented.** This closes the "interactive store panel" part of CONS-1. The
capacity/budget rules and the headless purchase already exist; this note
covers the retail UI flow and the record fields the UI reads.

## State machine

`pvwStateDinosaurPurchase` (`FUN_00534430`, ctor) is registered as state id
`0x13` with the name `DINOSAUR_PURCHASE` (`FUN_0052BD10(0x13, ...)`).

- `FUN_00535170` — enter/activate. Resolves the panel widgets and wires:
  - `DINO_ACHAT_SAVE_OUTCOME_SAVE` -> `LAB_00534F60`
  - `DINO_ACHAT_SAVE_OUTCOME_CANCEL` -> `LAB_00534F80`
  - the name-list widget (`state+0x78`) -> `DAT_00535040`
  - then `FUN_00535460(0)` (fill rows) and `FUN_00535710(state+0x48)`.
- `LAB_00534F80` — cancel: `state+0x48 = -1`, `state+0x18 = 5` (leave),
  vtable+8 (state exit), "clic3" sound.
- `LAB_00534F60` — save:
  - if the callback argument is non-null, return (hover/focus event);
  - else jump to `FUN_00535600`.

## `FUN_00535600` — validate the selected row

```text
entry = store_vector.begin()[state+0x48]        // state+0x11 -> tool
price = entry+0x18
if (FUN_0052BED0(price)) {                       // budget gate + warning
    tool+0x2c = entry+0x28
    tool+0x30 = entry+0x00
    tool+0x38 = state+0x48
    tool+0x3c = 1                                // "from store" flag
    state+0x18 = 0x12                            // FENCE_INSERTION
    vtable+8()                                   // leave the store state
}
play "clic3"
```

`FUN_0052BED0` is the existing budget gate (`WARNING_NOT_ENOUGH_MONEY`). It
does **not** change state on failure: the panel stays open (the early return
only skips the copy). This matches the "failure keeps the current state"
rule already documented for `FENCE_INSERTION`.

## Store vector and row record

The state reads the store as `[state+0x44] -> +0x10 -> +0x18 -> +0x38`
(vector begin) / `+0x3c` (end); each element is a pointer to a store record.
The UI reads:

| Record offset | Use |
| --- | --- |
| `+0x00` | copied to `tool+0x30` (record pointer/handle) |
| `+0x18` | price (decimal text via `FUN_005972BA`) |
| `+0x28` | copied to `tool+0x2c`; passed to `FUN_005E4CF0` for the 3D preview |

`FUN_005E4CF0(entry+0x28)` builds the preview actor; the phenotype data comes
from the entry's dinosaur definition (`[state+0x70]+0x10`), whose `+0x70` is
the food kind (0..9, matching `.bsi`/`.bni`) and `+0x74` the size class (0..5;
the panel has four `DINO_ACHAT_SAVE_INDICATEURS_TAILLE1..4` and shows the
biped/quadruped pair separately).

## Row list

`FUN_00535460(state, preview_actor)` fills **six** fixed row widgets at
`state+0xd0` (two pointers each) from the store vector:

- `FUN_00517100(row_widget, index, store, [state+0x44]+8)` — set name/price;
- if a preview actor is passed, `FUN_00576550(preview+0x4fc, entry)` computes a
  score and `FUN_005171A0` writes it;
- rows past the end are cleared with `FUN_00517180`.

`FUN_00535710(state, index)` is the selection change:
- `state+0x48 = index`; `< 0` hides the detail widgets and clears the preview;
- otherwise shows them, sets the preview model from `entry+0x28`, refreshes
  the food-kind/size indicators from the definition (`+0x70`/`+0x74`), renders
  the preview (`FUN_005E2770`), writes the price text, and appends the name.

So the retail panel is a six-row viewport with a scrollbar, not a full list.

## Widget contract

From `ConstructionInterface.Ini` + `FUN_00534430`:

- root `DINO_ACHAT_SAVE_PANEL`, list `DINO_ACHAT_SAVE_NAMES_LIST`;
- `DINO_ACHAT_SAVE_SCROLLUP` / `..._SCROLLDOWN` / `..._THUMB` / `..._LIFT`
  (the scrollbar);
- `DINO_ACHAT_SAVE_PRICE_SLOT`, `DINO_ACHAT_SAVE_TEXT_ZONE_PANEL`
  (description), `DINO_ACHAT_SAVE_RENDER_VIEW`, `DINO_ACHAT_SAVE_DINO_PIC_OFF`;
- food icons `DINO_ACHAT_SAVE_FOODKIND_{FRUITS,PASTEQUE,FLOWER,PUMPKIN,HAY,COW,GOAT,OSTRICH,PIG,SHEEP}`
  and multipliers `DINO_ACHAT_SAVE_FOODMULTIPLY_1..5`;
- size/regime icons `..._INDICATEURS_TAILLE1..4`,
  `..._INDICATEURS_REG_ALIM_{HERBI,CARNI}`, `..._INDICATEURS_{BIPED,QUADRUPED}`;
- eleven attribute gauges + indicator strips
  `DINO_ACHAT_ATTRIBUTES_{ORIGINALITY,STAMINA,COURAGE,INTELLIGENCE,AGILITY,SPEED,ARMOR,AGGRESSIVENESS,ATTACK,BEAUTY,PERCEPTION}_{GAUGE,INDICS}`;
- `DINO_ACHAT_SAVE_OUTCOME_SAVE` / `..._CANCEL`.

## Port plan

1. A `StorePanel` model in `src/world/store.*`: the catalog entries
   (`Market.ini` or the mission `DinoStoreN`), the current scroll offset, the
   six visible rows and the selected index.
2. Render the six rows into `DINO_ACHAT_SAVE_NAMES_LIST` children (name +
   price). The retail row widgets are children of the list; expose
   `DINO_ACHAT_ROW_<i>_NAME/PRICE` runtime text and hit-test rows.
3. On row click: set the selection; update `PRICE_SLOT`, the food/size icons,
   the name and the 11 attribute gauges. The gauges read the selected
   `Market.ini` genotype through `DinoAttributes` (see
   `notes/phenotype-attributes-runtime.md`); `ORIGINALITY` (`+0x194`
   conventional appeal) remains 0.
4. On `SAVE`: if budget < price show `WARNING_NOT_ENOUGH_MONEY` and stay;
   otherwise remember the entry and enter the port's insertion mode.
5. On the next ground/paddock click: run `paddock_can_add`; if it fails show
   `WARNING_NO_ROOM_IN_FENCE` and stay in insertion; if it succeeds charge
   `price`, add the dino, call `BudgetLedger::spend_dinoachat(price)` and
   remove the row from the store list.
6. `CANCEL` leaves the panel and clears the selection.

Steps 4-5 reuse the existing `--buy-dino` core; only the source of the entry
changes (selected row instead of a CLI name). The `DINOACHAT` ledger bucket is
already fed by the headless purchase.

## Port status

Implemented:

- `ParkHud::set_store_rows`, `store_select`, `store_scroll_by`,
  `store_visible_row` and `store_row_at` in `src/ui/park.{h,cpp}`. The list is
  a six-row viewport over the catalog (row pitch = list height / 6); the
  selected row is drawn in yellow with its price right-aligned.
- Click actions: `DINO_ACHAT_SAVE_NAMES_LIST` -> `store select N`,
  the right 15 px strip (`LB_VSCROLL_WIDTH`) -> `store scroll up/down`
  (the retail VSCROLL children carry no INI geometry), SAVE -> `store buy N`
  (or `store no selection`), CANCEL -> close.
- `main.cpp`: `handle_store_buy` is the retail `0x00535600` budget gate. On
  success it stores the entry/price, hides the panel and arms
  `pending_store_index`; the next ground click runs `paddock_can_add`, shows
  `WARNING_NO_ROOM_IN_FENCE` and keeps the insertion state on failure, or
  adds the dinosaur at the picked point, charges `park_budget`, feeds
  `BudgetLedger::spend_dinoachat`, erases the catalog row and refreshes the
  list. `--ground-click X,Y` is the scripted scene click used by the tests.

Verified on Cpg04 (`test_cpp_store_panel_purchase`):

```text
park click 765,298 -> open DINO_ACHAT_SAVE_PANEL
park click 160,88  -> store select 0
park click 470,520 -> store buy 0
store: Allosaurus price=9000 selected, click a paddock
store: bought Allosaurus price=9000 budget=66000 paddock=7 dinos=21 \
       dinoachat=1/9000 used=32 total=368
```

and `test_cpp_store_panel_no_money`: budget 100 -> `WARNING_NOT_ENOUGH_MONEY`
and no purchase. `test_cpp_store_panel_scroll` scrolls the viewport one row
and buys `Allosaurus_Leoparus` (10,000).

Attribute gauges: `StoreRow::attributes` (11 values) is built from the
selected Market genotype's `DinoAttributes`; `ParkHud::update_store_gauges`
fills each `DINO_ACHAT_ATTRIBUTES_*_GAUGE/INDICS`, and the draw loop renders
the proportional fill. Allosaurus: STAMINA 40, COURAGE 70, INTELLIGENCE 38,
AGILITY 50, SPEED 2, ARMOR 25, AGGRESSIVENESS 80, ATTACK 80/120, BEAUTY 20,
PERCEPTION 34, ORIGINALITY 0 (all part pairs homogeneous; `+0x194` is 100).
The Leoparus breed has distinct dummy alleles and shows ORIGINALITY 67.
Evidence: `notes/store-gauges.png`, `notes/store-gauges-originality.png`,
`test_cpp_store_attribute_gauges`.

3D preview: `DINO_ACHAT_SAVE_RENDER_VIEW` now draws the selected store
dinosaur's skinned LOD1 pose. `StoreRow.genotype` resolves the head
species/family; a per-genotype `StorePreview` caches the GPU mesh, texture
and model bounds. The draw reuses the scene `look_at`/`perspective` and
lighting with a side/three-quarter camera, clipped by scissor to the widget
rect. Allosaurus: `notes/store-preview.png`,
`test_cpp_store_3d_preview`.

Failed purchases: `FUN_0052bed0` (budget gate) and the fence-insertion message
at `0x00536017` both call `FUN_005e3d70` on the state's embedded message
object. That is the transient `MESSAGE_PANEL` toast (`LINE1..LINE4_MESSAGE`,
yellow, fading lines), **not** the modal `WARNING_PANEL`. The state is not
changed, so the store panel stays open. `ParkHud::show_message` fills the four
line widgets (word-wrapped to 26 characters; the retail lifetime constant is
not extracted yet, the port uses 4 s) and `ParkHud::update(dt)` expires it.
The port's `--warning` / `--request` / `--campaign-end` dialogs are
interface-level overlays, so they no longer call `hide_panels()`: retail keeps
the active state visible behind the dim `DIALOG_BACKGROUND`
(`notes/ui-recon.md`). Evidence: `notes/store-message.png`,
`test_cpp_store_message_keeps_panel`.

Catalog selection follows the mode, like the retail store vector:

- free game: `FreeGame.ini` `[DINOSTORE]` (20 entries; a subset of Market.ini);
- campaign mission: the mission's effective `[DINOSTORE]` block
  (its own, or the all-empty `MissionBase.ini` pages when it has none; e.g.
  Cpg04_Mission2 = 4 entries, Cpg04_Mission4/Cpg01 = empty);
- plain/debug park: `Market.ini` (54 entries).

`src/formats/market.cpp` now exposes `market_from_values(CampaignMission
::values)` for the flattened `DINOSTORE.DINOSTORE<N>` keys. `main.cpp` prints
`store catalog: <source> entries=N`; evidence `notes/store-mission-catalog.png`
and `test_cpp_store_catalog_mission` / `test_cpp_store_catalog_freegame`.

### Draw-order fix

The rows were initially invisible: `DINO_ACHAT_SAVE_WOOD_PANEL` (a sibling
child of the panel, ZPRIORITY 0.49) was drawn *after*
`DINO_ACHAT_SAVE_NAMES_LIST`. Root cause: `Overlay::load_interface` raised
text-only children to `parent + 0.001`, but lower ZPRIORITY is nearer in the
retail overlay, so the store list (0.46 -> 0.491) fell behind the wood
(0.490). The adjustment is now `parent - 0.001`; the six rows and the dark-red
list background render over the wood (`notes/store-panel.png`). The park
fence/dinosaur list snapshots still pass.


## Evidence

- Ghidra: `0x00534430` (state ctor / widget resolution), `0x00535170`
  (activate + callbacks), `0x00534F60` (save), `0x00534F80` (cancel),
  `0x00535600` (budget check + start insertion), `0x00535460` (row fill),
  `0x00535710` (selection refresh), `0x00593930` (store dtor),
  `0x00594270` (row removal).
- Strings: `DINOSAUR_PURCHASE` `0x006647A8`, `DINO_ACHAT_SAVE_*` at
  `0x00663D..0x006647`.
- Data: `Data/Overlay/ConstructionInterface.Ini`
  (`[DINO_ACHAT_SAVE_PANEL]`, `[DINO_ACHAT_SAVE_NAMES_LIST]`).
- Related: `docs/decomp/construction-validation.md`,
  `notes/fence-insertion-recon.md`.
