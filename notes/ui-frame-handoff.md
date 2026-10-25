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
6. The first stored slot is the **normal** state (frame 4) and may be fully
   transparent; frame 0 is the **disabled** art. The state mapping is
   **0 disabled, 4 normal, 5 hover, 6 pressed, 7 selected**. Frame 4 may be
   fully transparent (menu/list bars): draw it anyway and let the page art
   show. `SKIP_LIST` removes animation frames 1–3, not states. The Python
   oracle validates this: Start/Cancel match retail exactly (MAD 0.0), gauge
   icons MAD 3.65.
7. `TEXTURE_FRAME` (0..255) offsets the state frame; no retail menu button
   sets it, so it defaults to 0.

Reference images: `notes/ui-atlas-frames.png`,
`notes/ui-atlas-outcome4-full.png`, `notes/hud-preview.png`.

The Python oracle `tools/viewer/overlay_preview.py` implements all of this;
its `--hud` output matches the retail 800×600 screenshot panel-for-panel
(MAD 8.8 on the right panel, 3.65 on the gauge).

## Review of `src/viewer/main.cpp` frame helpers (read-only)

`ui_frame_rect` currently:
- computes `used = objects - skip.size()`, assuming skipped frames are
  removed from the **end**;
- lays frames out **row-major by texture columns**
  (`columns = texture.width / frame_width`);
- scans for the first inked row and uses it as the strip origin.

All three are wrong for the retail data. Replace with:

```text
used = [i for i in 0..objects-1 if i not in skip]      // keep logical ids
if x + len(used)*w <= texture.width:  horizontal
else:                                 vertical

slot = index_of(frame in used)                        // frame is 0/4/5/6/7
if horizontal: src = (x + slot*w, y, w, h)
else:          src = (x, y + slot*h, w, h)
```

Do **not** search for "first ink" to find the normal frame, and do not use
`ui_last_visible_frame` for selection. Use the state mapping (0/4/5/6/7).
`ui_rect_has_ink` is still useful only for skipping sprites that are fully
transparent, but a transparent normal frame (menu/list bar) is intentional:
draw it (or skip drawing) and let the page art show.

The current helper also drops `skip` ordering and cannot represent the
vertical strips (`MAIN_MENU_BUTTON`, `ISLAND_LIST_BUTTON`). The Python
`Graphic.frame_rect` / `state_frame` are the reference implementation.

Paste-ready shape for `src/viewer/main.cpp`:

```cpp
static int frame_for_state(int state) {  // 0 disabled, 1 normal, 2 hover,
    switch (state) {                     // 3 pressed, 4 selected
        case 0: return 0;
        case 2: return 5;
        case 3: return 6;
        case 4: return 7;
        default: return 4;
    }
}

static UiSourceRect ui_frame_rect(const UiTexture& tex, const dino::ui::Graphic& g,
                                  int logical_frame) {
    std::vector<int> used;
    for (int i = 0; i < g.objects; ++i)
        if (std::find(g.skip.begin(), g.skip.end(), i) == g.skip.end())
            used.push_back(i);
    if (used.empty()) return {g.rect.x, g.rect.y, g.rect.width, g.rect.height, false};

    auto it = std::find(used.begin(), used.end(), logical_frame);
    const int slot = it == used.end() ? 0 : int(it - used.begin());
    const bool horizontal =
        g.rect.x + int(used.size()) * g.rect.width <= tex.width;
    UiSourceRect r{g.rect.x, g.rect.y, g.rect.width, g.rect.height, true};
    if (horizontal) r.x += slot * g.rect.width;
    else            r.y += slot * g.rect.height;
    if (r.x + r.w > tex.width || r.y + r.h > tex.height) r.valid = false;
    return r;
}
```

`graphic.skip` can stay a `std::vector<int>`; just use it as a set. The
normal row on the Free Game page is frame 4 and is transparent, so the page
art shows through — that is the retail look, not a missing sprite.
