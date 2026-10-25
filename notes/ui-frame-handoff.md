# Frame-strip handoff for `src/ui/overlay.cpp`

Short version of the follow-up research in `notes/ui-recon.md`; the full
spec is in `docs/formats/overlay-ini.md` ("Frame strips").

1. `NB_OBJECTS` is the number of logical frames.
2. `SKIP_LIST` is a bitmask (`mask |= 1 << frame`). Keeping it as a set of
   skipped indices is fine, but only as a mask for selection.
3. **Skipped frames are not stored in the atlas.** The stored frames are the
   complement, packed compactly from the entry's `(X, Y)`.
4. Example: `[BUDGET_PANEL_BUTTON]` has `X=0 Y=62 WIDTH=31 HEIGHT=31
   NB_OBJECTS=8 SKIP_LIST=1 2 3`. Its five frames are at x = 0, 31, 62, 93,
   124 — *not* at the original slots 0, 4, 5, 6, 7 scaled by `W`.
5. Packing direction: horizontal if the used frames fit the texture width,
   otherwise vertical. `MAIN_MENU_BUTTON` (213×23, skip 0 1 2 3) is vertical:
   the first stored slot is transparent and the three visible bars are at
   y ≈ 25, 48, 71. `ISLAND_LIST_BUTTON` behaves the same way at Y = 55.
6. The first stored slot can be blank/disabled, so **do not assume frame 0 is
   normal**. The observed normal frames are:
   - menu/list bars: 5 (frame 4 is blank; 6 hover, 7 selected)
   - right-panel icons: 4 (frame 0 is the gray disabled art; 5 hover, 6
     pressed, 7 yellow selected)
   - difficulty icons: 4 (5 hover, 6 pressed, 7 yellow ring selected)
   - Start/Cancel: 0 (4..7 are the coloured variants)
   Allow a per-control normal-frame override until the engine state table is
   recovered.
7. `TEXTURE_FRAME` (0..255) offsets the state frame; no retail menu button
   sets it, so it defaults to 0.

Reference images: `notes/ui-atlas-frames.png`,
`notes/ui-atlas-outcome4-full.png`, `notes/hud-preview.png`.

The Python oracle `tools/viewer/overlay_preview.py` implements all of this;
its `--hud` output matches the retail 800×600 screenshot panel-for-panel
(MAD 8.8 on the right panel, 12.0 on the gauge).
