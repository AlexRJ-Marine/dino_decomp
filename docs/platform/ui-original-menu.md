# Original menu — research handoff (UI-2)

Status: **not implemented**. The current start screen and controls panel are
**placeholder UI**, deliberately standalone. They do not use retail menu data,
layout, art, sounds, actions or the original state machine.

## What the placeholder does (and does not do)

`src/viewer/main.cpp`:

- `--menu` / interactive launch: draws `DINO ISLAND`, `Press ENTER to begin`,
  `Esc quits  Tab help` over the island; ENTER/SPACE starts, ESC quits.
- `Tab` / `--help-panel`: a controls panel.
- `--no-menu` skips it (tests use `--frames`, which also skips it).

It is a dark rectangle + `.fnt` text. There are **no** retail backgrounds, no
buttons, no hover/pressed states, no menu sounds, no profile/scenario flow, no
save/load panels.

## Retail assets already located

| Path | What it is |
| --- | --- |
| `Data/Overlay/MenuInterface.Ini` | Menu layout: object types (`WIDGET`, `BUTTON`, `COMBO`, `EDIT`, `CURVE`, `GROUP`, `HSCROLL`, `LIST`, `PROGRESS`, `RENDER`, `VSCROLL`, `WINDOW`), parameters (`TYPE`, `PARENT`, `X/Y/WIDTH/HEIGHT`, `VISIBLE`, `ENABLE`, `TEXTURE`, `TEXT_COLOR`, `TEXT_HEIGHT`, `TEXT_FONT`, `TEXT_JUSTIFY`, `TEXT`, `HINT`, `USER_ID`), button sub-types (`NORMAL`, `SWITCH`, `CHECK_BOX`), states and actions. |
| `Data/Overlay/MenuGraphicDB.ini` | Named texture rectangles: `[NAME] X, Y, WIDTH, HEIGHT, FILENAME, NB_OBJECTS, SKIP_LIST`. Used for buttons/scrollbars/icons. |
| `Data/Overlay/RadialMenu.Ini` | Radial build menu definitions. |
| `Data/Overlay/MenuEng.loc` | 56 localised strings (`&FreeGame`, `&Isl01_Name`, …); parser in `tools/formats/loc.py`. |
| `Data/Overlay/ParkInterface.Ini` | In-park HUD/panels (54 KB). |
| `Data/Overlay/*Interface*.Ini` | Lab, list, info, construction, mission panels. |
| `Data/Overlay/MenuGraphicDB.ini` + `textures/*.tga` | Menu backgrounds (`campagne_800x600.tga`, `cinematics_800x600.tga`, island maps). |
| `Data/Overlay/pics/Menu_hors_jeu/*.jpg` | Reference screenshots of the original menus (main, free game, campaign, options). |

The comment header of `MenuInterface.Ini` documents the widget schema
verbatim; it is the primary spec for a parser.

## Questions for the reverse-engineering agent

1. **State machine**: what screens exist, the entry state at startup, and the
   transitions (intro → main → free game / campaign → park; pause; options;
   save/load). Which overlay functions drive them (`mo*` classes)?
2. **Layout semantics**: confirm `MenuInterface.Ini` section/parameter parsing
   (parents, priorities, anchoring on resolution change, `EQUIVALENT`),
   and how `MenuGraphicDB.ini` rectangles map to button frames.
3. **Rendering contract**: how the engine draws an overlay widget (texture
   atlas, text font, colors, z-order), and the overlay `mo*` vtables if we
   want to hook rather than reimplement.
4. **Input/actions**: mouse/keyboard hit-testing, hover/pressed/disabled
   states, sounds, and the action IDs bound to Free Mode / IGUANO / easy /
   Start (the x32dbg harness clicks them at 395,234 / 260,183 / 494,451 /
   526,531 on its window — see `scripts/x32dbg/play-freegame.ps1`).
5. **Localisation**: how `TEXT` fields in the Ini reference `MenuEng.loc`
   records (`&...` IDs) and how missing IDs are handled.
6. **Save/load**: the save panel flow and `.sav` paths (`run/save/slotNNN.sav`
   is produced by the original with `run/save`/`run/save/auto` present).
7. **Audio/video**: menu music/SFX and intro-video skip behavior
   (`Data/Config/Dino.ini` `UseIntroVideo`).

## Plan when the research lands

1. Parse `MenuInterface.Ini` + `MenuGraphicDB.ini` into an overlay widget tree
   (new `src/ui/`), keeping the placeholder only behind `--menu`.
2. Implement hit-testing + actions and the main-menu transition table.
3. Swap the placeholder start screen for the retail layout at 800×600 and
   1024×768 (QOL-2 scaling), then wire Free Game → island selection → park.
4. Keep `--no-menu`/`--menu` for tests; the placeholder will be removed or
   renamed once parity is reached.

No retail assets are copied into `src/`; the menu will read them from
`game/%MAINDIR%` like the rest of the port.
