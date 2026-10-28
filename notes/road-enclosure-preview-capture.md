# Retail road / enclosure preview capture

Status: **captured 2026-10-28** from the retail `DinoIsland.exe` under the
windowed virtual-input harness. All four requested screenshots exist in
`notes/harness/` (gitignored):

| File | Content |
| --- | --- |
| `notes/harness/road-preview-valid.png` | Road tool armed, straight valid run, solid blue strip |
| `notes/harness/road-preview-invalid.png` | Road tool armed, too-short run, red cell |
| `notes/harness/enclosure-preview-valid.png` | Enclosure tool armed, blue ground disc outside a paddock |
| `notes/harness/enclosure-preview-invalid.png` | Enclosure tool armed, red ground disc inside the paddock |

This capture changed no tracked `notes/*.png`, `game/`, `original/` or source
file.

## Harness used

- Runtime: `run-window/`, refreshed with
  `.\scripts\setup-test-runtime.ps1 -Target run-window` so its `DINPUT8.dll`
  is the current proxy build (164945 bytes, 2026-10-27 22:01).
- Executable: `run-window/DinoCapture.exe`, a byte copy of the patched
  `run-window/DinoIsland.exe` (2617344 bytes). The rename only isolates the
  process from another lane's concurrent `run-test` `DinoIsland` harness runs;
  it is the same retail binary and the same `DINPUT8.dll` / dgVoodoo wrappers.
- Virtual input only: `DINO_VIRTUAL_MOUSE=1`, writes `dino_input.txt`
  (`x y 1` / `x y 0`) and posts `WM_MOUSEMOVE` / `WM_LBUTTONDOWN` /
  `WM_LBUTTONUP`. No `SetCursorPos`, `mouse_event`, `SendInput` or physical
  click was used.
- Other environment: `DINO_FORCE_INPUT_ACTIVE=1`, `DINO_LOG_ERRORS=1`,
  `DINO_AUTO_DISMISS_ERRORS=1`; `DINO_MOUSE_CLICK_CAPTURE` removed.
- Window: **native dgVoodoo `WS_POPUP` 800x600** (`-NoWindowStyle`), i.e. the
  client rectangle equals the window rectangle. This was required for the park
  UI (see "Harness finding" below).
- Focus: the game stayed in the background for every final capture.
  `park window metrics ... fg=0xD0660` in the road run; the foreground handle
  was never the game. The proxy's D3D8 Reset may log
  `activated game window (fg=<other hwnd>)` when the game is not foreground;
  in these runs `SetActiveWindow`/`SetFocus` did not take the OS foreground
  (the logged `fg` is still the other window). The scripts call
  `SetForegroundWindow(previousForeground)` only to restore the user's window
  after the run. No `-Focus` click was used in the final road or enclosure
  runs.

Exact commands:

```powershell
.\scripts\setup-test-runtime.ps1 -Target run-window
powershell -NoProfile -ExecutionPolicy Bypass -File notes\harness\road-probe.ps1 `
  -GameDir run-window -ExeName DinoCapture.exe -ShotPrefix road7 `
  -ParkWaitSec 40 -NoWindowStyle -ScanStarts

powershell -NoProfile -ExecutionPolicy Bypass -File notes\harness\fence-campaign-probe.ps1 `
  -ShotPrefix cpg-fence3     # red circle
powershell -NoProfile -ExecutionPolicy Bypass -File notes\harness\fence-campaign-probe.ps1 `
  -ShotPrefix cpg-fence4     # blue circle after two zoom-out clicks
```

`notes/harness/road-probe.ps1` and `notes/harness/fence-campaign-probe.ps1`
are the capture drivers. `road-probe.ps1` has a `-ScanStarts` mode that tries a
grid of start cells and dismisses the retail warning dialog
(pixel `(400,300) == (56,65,104)`, check button `400,375`) until a start cell
is accepted.

## Road tool: exact sequence (Free Game, IGUANAO, easy)

| Step | Control / action | Client point | Result |
| --- | --- | --- | --- |
| 1 | `FREE_GAME_BUTTON` | 395,234 | Free Game page |
| 2 | `ISLAND_LIST` row 0 (IGUANAO) | 269,183 | island selected |
| 3 | `FREE_EASY_BUTTON` | 494,451 | easy |
| 4 | `FREE_START_BUTTON` | 526,531 | park load; wait 40 s |
| 5 | `OPEN_TRANSPORT` | 765,248 | infrastructure panel opens |
| 6 | `CONSTRUC_ROAD_BUTTON` | 645,86 | Road tool armed (button highlighted) |
| 7 | ground start click | 300,200 | start cell accepted |
| 8 | mousemove only | 420,200 | **valid preview**, `Cost of road: 70` |
| 9 | mousemove only | 302,202 | **invalid preview**, `Cost of road: 0` |
| 10 | mousemove only | 420,100 | vertical valid preview, `Cost of road: 120` |

Notes:

- The first scan start at `200,200` was rejected with
  `Unable to build: Space already occupied.` The warning dialog was dismissed
  with the green check at `400,375`; `300,200` is a free start.
- The preview is laid out along the **longer axis** of start -> cursor, so the
  `420,200` run is a horizontal strip and `420,100` is a vertical strip.
- The road panel stays open while the tool is armed; the preview and the
  cursor cost toast are drawn over the park. The game cursor itself is not
  visible in these screenshots (retail hides it in the park); the preview's
  end point is the cursor.

## Road preview: what it looks like

`road-preview-valid.png` (800x600):

- One straight, **solid-looking blue strip**, about 120 px long by 14 px high
  at this camera, from the start cell to the cursor. It is **not segmented
  into visibly separate boxes**: the interior is one uniform flat colour. There
  are only faint darker seams at the 4-unit joins: 1800 of 1920 sampled bar
  pixels are the main blue, 120 are one seam shade and 120 another.
- No road/helper texture pattern is visible; it is a flat colour influenced by
  the retail pulsing material bank, blended additively over the grass.
- Dominant fill in the saved shot: **(64,64,135)** (2166 pixels in a
  130x25 crop around the strip). Seam shades: **(63,63,133)** and
  **(61,61,129)**. This is the dark
  navy phase of `gscMaterialBank` material 2 `RGB(64,64,255-x)`, here
  `x = 120`. A second capture at a different phase (`road6-valid.png`) read
  **(64,64,249)** (`x = 6`), confirming the decompiled pulse
  `x = trunc((sin(6t)+1)*0.5*127)`, 0..127. The **dark blue** in the saved
  shot is therefore retail's pulsing helper colour, not a missing texture.
- The toast reads `Cost of road: 70` (7 intervals, 10 per cell, matching the
  decoded `10 * (N-1)` rule).

`road-preview-invalid.png`:

- The same tool and start cell, moved back to the start cell (`302,202`):
  a **red single 4-unit cell**, about 14x14 px, with `Cost of road: 0`.
  This is the `WARNING_ROAD_TOO_SHORT` case: `start == end`, so the preview is
  one cell rather than a run.
- Dominant fill in the saved shot: **(134,64,64)** (274 fill pixels). This is
  material 3 `RGB(255-x,64,64)` at `x = 121`; a different-phase capture
  (`road6-short.png`) read **(251,64,64)** (`x = 4`). Both lie on the same
  pulsing 64-base red ramp.
- The colour change is immediate on the mousemove; no extra click is needed.

`road-preview-invalid.png` is therefore a valid `TOO_SHORT` invalid capture but
not a multi-cell red run. A red multi-cell run needs a busy cell or a position
strictly inside a perimeter; the free-game start road at `200,200` is a road
cell that may be crossed (the run toward it stays blue), so it is not a busy
run cell. The campaign Cpg04 paddock would provide the `INSIDE_PERIMETER`
case, but this capture stopped at the single-cell `TOO_SHORT` evidence.

## Enclosure tool: exact sequence (campaign Cpg04, row 3)

The enclosure tool could **not** be armed into a preview in Free Game: the
free park has no existing paddock, so `FENCE_CONSTRUCTION` enters its "no
selected fence" branch and the first ground click does not produce the disc.
It is reachable in the shipped campaign park, which has 9 fences:

| Step | Control / action | Client point | Result |
| --- | --- | --- | --- |
| 1 | `CAMPAIGN_BUTTON` | 395,189 | campaign list |
| 2 | `CAMPAIGN_LIST` row 3 (`y = 166 + 3*34 + 17`) | 409,285 | PANIC IN THE SAVANNA / Cpg04 |
| 3 | `CAMPAIGN_START_BUTTON` | 364,530 | briefing page |
| 4 | `CAMPAIGN_INFO_START_BUTTON` | 521,449 | Cpg04 park load; wait 45 s |
| 5 | briefing green check x5 | 322,402 | dismiss the four briefing pages |
| 6 | `OPEN_PADDOCK_LIST_PANEL` | 492,584 | paddock list (5 visible paddocks) |
| 7 | `FENCE_SNAPSHOT_SLOT1` | 207,507 | selects Pretoria; camera jumps to it |
| 8 | `OPEN_CONSTRUCTION` | 764,135 | construction bar |
| 9 | `OPEN_CONSTRUCTION_FENCE` | 618,145 | `FENCE_CONSTRUCTION` state, tool armed |
| 10 | mousemove only | 330,250 | **red disc** on the paddock floor |
| 11 | `NAVIGATION_DEZOOM` x2 | 707,354 | widens the view; tool stays armed |
| 12 | mousemove only | 100,300 / 350,450 / 400,500 | **blue disc** on the grass outside the paddock |

For the campaign run, `run-window/save/dino.dat` was backed up, written with
`0F 00 00 00` to unlock rows 1-4, and restored byte-identically at the end.

## Enclosure preview: what it looks like

`enclosure-preview-valid.png` (step 12, cursor `350,450` after 2x zoom out):

- A **flat, solid blue circular disc on the ground**, not a ring and not a
  rectangle. It has a hard edge, no texture, no gradient, and it is partially
  occluded by terrain/fence posts (normal z-testing). The disc lies on the
  orange grass **outside** the selected paddock.
- Screen bounding box of the fill: x 371-443, y 419-494 (about 72x75 px).
  Disc centre is about **(407,455)** while the cursor target is `350,450`:
  the preview is **snapped to the underlying build cell**, so the disc centre
  can be offset from the raw cursor (here by about 57 px after two zoom-out
  clicks). The exact snap step was not measured.
- Dominant fill: **(38,38,136)** (4268 connected pixels). The edge is a hard
  cutoff; no bright rim was measured.
- This is the "able to place" state. The blue colour is much darker/navy
  than the road helper blue (64,64,249); do not reuse the road material colour
  for the circle.

`enclosure-preview-invalid.png` (step 10, cursor `330,250`):

- The same flat circular disc, **dark red/maroon**, on the sandy paddock floor
  **inside** the selected paddock. The cursor arrow sits in the middle.
- Screen bounding box of the fill: x 309-367, y 231-277 (about 59x47 px,
  an ellipse under perspective). Dominant fill: **(80,38,38)** (all 2027
  connected fill pixels); no bright rim.
- This is the "unable to place" state. The pattern matches the road rule:
  a rectangle/disc inside an existing perimeter is rejected
  (`WARNING_INSIDE_PERIMETER`, code 3). *Inferred* from the two positions:
  inside the selected paddock -> red; outside on grass -> blue. The exact
  fence rule was not traced further in this run.

## Harness finding: the park UI coordinate frame

The first virtual-input attempts clicked `OPEN_TRANSPORT` (`765,248`) and did
nothing after the park loaded, even though the menu clicks worked and the
click log showed the DirectInput button down/up. Root cause was the bordered
test window, not input activation:

- `scripts/Set-WindowedStyle.ps1` adds a normal caption/border. The client
  origin is offset by about **(3,29)** px from the window origin.
- *Inferred mechanism:* the proxy's `hook_view_mouse_move` corrects the
  coordinates for the view vtable it patches (the menu view), while this park
  view's hit test used the uncorrected window coordinate, so a click at client
  `765,248` landed about 29 px low (the gap below the Infrastructure icon).
  `OPEN_TRANSPORT` and the other park HUD buttons never opened.
- Keeping dgVoodoo's native `WS_POPUP` window (`-NoWindowStyle`, client 800x600
  and frame offset `(0,0)`) made the park UI exact: the Infrastructure panel,
  Road button, paddock list, Construction/Enclosure and navigation buttons all
  worked while the game stayed in the background.
- This is a harness/UI-offset issue, not a gameplay discrepancy. The final
  captures use the native popup window.

## Evidence index

| Claim | Evidence | Reproduce |
| --- | --- | --- |
| Road valid preview is solid blue `(64,64,135)` at the captured pulse phase | `notes/harness/road-preview-valid.png` (`road7-valid.png`) | `road-probe.ps1 ... -ShotPrefix road7 -ScanStarts` |
| Road invalid preview is red `(134,64,64)` for `TOO_SHORT` | `notes/harness/road-preview-invalid.png` (`road7-short.png`) | same run, move back to the start cell |
| Enclosure valid preview is a blue ground disc `(38,38,136)` | `notes/harness/enclosure-preview-valid.png` (`cpg-fence4-z6.png`) | `fence-campaign-probe.ps1 -ShotPrefix cpg-fence4` |
| Enclosure invalid preview is a red ground disc `(80,38,38)` | `notes/harness/enclosure-preview-invalid.png` (`cpg-fence3-red.png`) | `fence-campaign-probe.ps1 -ShotPrefix cpg-fence3` |
| Bordered window offsets park UI clicks ~29 px | this note, "Harness finding" | compare the failed `road4-*`/`road5-*` shots with `road7-*` |
