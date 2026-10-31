# Retail mouse cursors (2026-10-31)

## Retail data

`Data/Graphics/Cursors/` holds the real cursor art and `.ini` records:

| State | `.ini` | `[GRAPH] Sequence` | Size |
| --- | --- | --- | --- |
| default park/menu | `CursorMain.ini` | `CursorMain_wood.tga` | 16x16 |
| sell mode | `CursorSell.ini` | `money.tga` | 32x32 |
| fence placement | `CursorInsertFence.ini` | `put_in_enclos.tga` | 32x32 |

`Base.ini` sets `Path="data\graphics\cursors"`, `Speed=10`, `SizeX/SizeY=32`;
each cursor `.ini` overrides `Sequence`/`Size`/`Speed`. There is **no hotspot
field**, so the hotspot is the top-left `(0,0)`.

## Port

`src/app/cursor.{h,cpp}` loads the three TGAs with `dino::load_tga`, creates
an SDL colour cursor from the RGBA surface (`SDL_CreateColorCursor(surface,
0, 0)`), and installs it with `SDL_SetCursor`. Modes:

- `Main` — default, menu and park;
- `Sell` — `park_sell_mode`;
- `Fence` — `build_tool == 2` or `6` (enclosure/fence placement).

`init_cursors` runs at the end of `Game::boot_platform`; `shutdown_cursors`
runs at the start of `Game::shutdown`; `game.cpp` selects the mode each frame
after `update_preview`. Missing images fall back to the SDL default cursor and
log the exact file/error.

## Why the mouse looked wrong

Two separate causes:

1. The port did not install the retail colour cursor, so the OS arrow showed.
2. A stale construction tool from a previous park could leave the road helper
   texture under the mouse and commit paths on the first click. Menu-started
   games now reset `build_tool`, `road_has_start`, the fence corners,
   selections, sell mode, pending widgets, feeder draws and show runtime
   (`Game::update_menu_loading`); the park then starts in the default state
   like retail.

There is no automated pixel test for the OS cursor (PrintWindow does not
capture the hardware cursor); the TGA load and mode selection are exercised
by every interactive launch.
