# Overlay LIST / VSCROLL input and thumb math (retail binary)

Binary: **DinoIsland.exe-ebea17** (PE32, MSVC 6, image base 0x400000). All
addresses below are virtual addresses in that program. Evidence was produced
with Ghidra MCP only (`search_strings`, `list_xrefs`, `decompile_function`,
`disassemble`, `read_bytes`); no runtime capture was needed. Nothing in
`src/`, `game/` or `original/` was touched.

Status: the whole path is proven by disassembly. Open items are listed at the
end (mostly cosmetic / auto-repeat details).

---

## 0. Answer in one page

The retail listbox never scrolls itself: it owns an **auto-created VSCROLL
child named `LbVs`** plus a container child named `LbCtnr`, and each frame it
copies `VScroll.pos` into its own `firstLine` and re-derives
`VScroll.range = lineCount - visibleLines + 1`.

VScroll state (all ints unless noted):

| field | off | meaning | ctor default |
|---|---|---|---|
| drag | +0x64 (byte) | thumb is being dragged | 0 |
| nameThumb | +0x68 | "VsBtnThmb" | -1 |
| nameUp | +0x6c | "VsBtnUp" | -1 |
| nameDown | +0x70 | "VsBtnDown" | -1 |
| range | +0x74 | number of scroll positions | 1 |
| position | +0x78 | current position (0..range-1) | 0 |
| travel | +0x7c | thumb travel in pixels | 0 |
(`w`/`h` come from the base widget: +0x54 / +0x58.)

Layout (`0x5c4720`), `U`/`D` = up/down button side = scrollbar width `W`:

```
thumbH  = max(5, (H - (U + D + 5.0)) - 3*range)          // 5.0f at 0x642f94
travel  = (int)(H - U - D - thumbH)
thumb.y = up.y + up.h + position * travel / (range - 1)   // integer div
thumb.h = thumbH
thumb.w = W
if range == 1: thumb.h = H - (U+D); thumb.y = U; travel = 0 (disabled look)
```

Input (all directions clamp to `0 .. range-1`, see `0x5c4620`):

| event | delta | note |
|---|---|---|
| click Up | `-1` | `0x5c4650` |
| click Down | `+1` | `0x5c4650` |
| click track above thumb | `-3` | fixed 3, **not** a page |
| click track below thumb | `+3` | compares `thumb.y` with mouse Y |
| thumb press | none | sets drag flag, position snaps on the next move |
| thumb drag | `pos = (mouseY - (up.y+up.h)) * range / travel` | absolute, not relative |
| wheel | `pos -= delta/120` | one notch = 1 line; wheel is delivered through OnMouseMove |

The listbox computes `visible = (int)height / lineHeight` (C cast via `__ftol`,
i.e. round-to-nearest, then signed IDIV) and scrollbar width is simply
subtracted: VScroll at `x = W - LB_VSCROLL_WIDTH`, `w = LB_VSCROLL_WIDTH`, and
the line container is `W - LB_VSCROLL_WIDTH` wide.

HSCROLL (`0x5be620`) is the same class mirrored: events 0x36/0x37 instead of
0x38/0x39, x/width instead of y/height.

---

## 1. Classes, factory and vtable slots

`FUN_005ba6b0` maps the INI `TYPE=` string to a type id and `FUN_005ba8f0`
creates the object (size passed to `FUN_004052c0`, ctor called, then
`vtable+0x14` parse, `vtable+0x0c` create-children, `vtable+0x18` post-parse):

| id | type | size | ctor | vtable |
|---|---|---|---|---|
| 0 | WIDGET | 0x64 | 0x5bc220 | 0x6470c0 |
| 1 | BUTTON | 0x74 | 0x5c0380 | 0x64726c |
| 2 | EDIT | 0x74 | 0x5c3ed0 | 0x647350 |
| 3 | PROGRESS | 0x88 | 0x5bf900 | 0x647210 |
| 4 | GROUP | 0x68 | 0x5bec80 | 0x6471bc |
| 5 | HSCROLL | 0x80 | 0x5be620 | 0x647168 |
| 6 | VSCROLL | 0x80 | 0x5c45b0 | 0x6473a4 |
| 7 | LIST | 0xdc | 0x5bd5d0 | 0x647114 |
| 8 | COMBO | 0x80 | 0x5c4ba0 | 0x6473f8 |
| 9 | CURVE | 0x90 | 0x5c5140 | 0x64744c |
| 10 | RENDER | 0x74 | 0x5c56a0 | 0x6474a0 |

Every vtable is 0x54 bytes (21 virtuals, 0x00..0x50). The slots used here:

| slot | meaning | base | LIST | VSCROLL |
|---|---|---|---|---|
| 0x0c | create internal children | 0x587340 | **0x5bd880** | **0x5c48a0** |
| 0x10 | copy state from EQUIVALENT widget | 0x5bc420 | 0x5bd7f0 | 0x5be690 |
| 0x14 | parse INI (base keys) | 0x5bca20 | 0x5bdca0 | 0x5bebe0 |
| 0x18 | post-parse | 0x608100 | 0x5c55a0 | **0x5c4b20** (VS_POSITION/VS_RANGE) |
| 0x24 | set position | 0x5c5900 | 0x5c6490 | 0x5c6490 |
| 0x28 | set size | 0x5c5ad0 | 0x5c64b0 | 0x5c64b0 |
| 0x2c | update layout | 0x5c5b30 | **0x5c64d0** | **0x5cb9d0** |
| 0x30 | per-frame update / hover tree | 0x5c5b60 | **0x5c64f0** | **0x5cb9f0** |
| 0x40 | mouse move (ctx arg) | 0x5c5d80 | **0x5c6b30** | **0x5cbb30** |
| 0x44 | mouse down (button,ctx) | 0x5c5e20 | **0x5c6b60** | **0x5cbbc0** |
| 0x48 | mouse up (button,ctx) | 0x5c5e90 | **0x5c6b90** | **0x5cbbf0** |
| 0x4c | widget event (id, a, b, ...) | 0x5c5f00 | **0x5c6bc0** | **0x5cbc30** |

Base widget fields used: `+0x38` flags (bit0 visible, bit1 enable, bit4
user_movable, bit5 drag_and_drop), `+0x44` hovered child widget, `+0x4c` x,
`+0x50` y, `+0x54` w, `+0x58` h (floats), `+0x60` listener, `+0x68`
`std::list` of children. `FUN_005bc5b0(x,y)` is the rectangle hit test.

Helpers (all take `this` in ECX): `FUN_005bbcb0` = move (vtable+0x24, adds
parent origin), `FUN_005bbdc0` = set size (vtable+0x28), `FUN_005bbfd0` =
send event to self (vtable+0x4c), `FUN_005bb960` = set name (event 0x1a).

---

## 2. VSCROLL is auto-created; the INI section only decorates it

`LIST::CreateChildren` = `0x5bd880`:

```
005bd896 CALL 0x005ba000        ; alloc 0x64 + FUN_005bc220  -> plain WIDGET child
005bd8a1 MOV [ESI+0x6c],EAX     ; its auto-generated name string
005bd8ab CALL 0x004139d0        ; find widget by name -> EBX (container)
005bd8bf CALL 0x005ba150        ; alloc 0x80 + FUN_005c45b0  -> VSCROLL child
005bd8c5 MOV [ESI+0x68],EAX     ; its auto-generated name string
005bd8d4 CALL 0x004139d0        ; -> EDI (the scroll bar)
...
005bd8f2..005bd92f  FUN_005bb960(EDI, "LbVs")    ; rename to LbVs
005bd98f..005bd9ac  FUN_005bb960(EBX, "LbCtnr")  ; rename to LbCtnr
```

Strings: `"LbCtnr"` @0x66d7c8, `"LbVs"` @0x66d7d0 (both only reachable from
this function).

`VSCROLL::CreateChildren` = `0x5c48a0` creates the three buttons and names
them:

```
005c48b6 CALL 0x005ba050  -> [ESI+0x6c]      then FUN_005bb960(.., 0x66e0fc "VsBtnUp")
005c48fa CALL 0x005ba050  -> [ESI+0x70]      then FUN_005bb960(.., 0x66e0f0 "VsBtnDown")
005c493e CALL 0x005ba050  -> [ESI+0x68]      then FUN_005bb960(.., 0x66e0e4 "VsBtnThmb")
```

Consequence, and the answer to the EQUIVALENT question: **the scroll bar and
its buttons always exist, whether or not the INI declares them.**
`FUN_005ba8f0` resolves `EQUIVALENT=` with `FUN_00414dc0/FUN_004139d0` and, when
the name already exists, does *not* create a new object - it calls
`vtable+0x14` (parse) on the existing one (that is exactly what the INI header
means by "so this object is only updated"). That is why
`[ISLAND_LIST_VS] TYPE=VSCROLL EQUIVALENT="LbVs"` only supplies the textures
and why `[.._BTN_UP] EQUIVALENT="VsBtnUp"` works.

`LB_VSCROLL_WIDTH` is therefore the only geometry input the INI really controls
(the bar is then placed and sized by the list, section 6).

---

## 3. INI field to state mapping

### LIST (parse `0x5bdca0`, base parse `0x5bca20` first)

| key | target | setter |
|---|---|---|
| LB_VSCROLL_WIDTH | list+0x70 | inline, default 0 |
| LB_LINE_HEIGHT | list+0x74 | event 0x3e via `FUN_005bee50` |
| LB_NB_COLUMNS | list+0x78 | event 0x43 via `FUN_005bedd0` |
| LB_COLUMN<n>_WIDTH | list+0x98+4n | event 0x44 via `FUN_005bedf0`, loop `LB_COLUMN%d_WIDTH` |
| LB_FIRST_LINE_SEPARATOR / LB_LINE_SEPARATOR / SELECTED_* colours | +0x64 flag, +0x88..+0x97 | events 0x3f, 0x40, 0x4c, 0x4d (cosmetic; not fully mapped) |


List ctor defaults (`0x5bd5d0`): +0x70=0, +0x74=-1, +0x78=1, +0x7c=0 (first
line), +0x80=-1, +0x84=-1 (selected line), +0x98..+0xd4 = 0, +0xd8 = empty
`std::list` of line widgets.

List event ids (jump table at `0x5c7108`):

```
0x3a..0x40  colours / flags      0x41 [list+0x84]=selected line
0x42        (default/no-op)      0x43 [list+0x78]=columns
0x44        [list+0x98+4i]=w     0x45 add line   0x46 add line (variant)
0x47        delete line          0x48 clear lines
0x49        [list+0x7c]=first line (this is the VScroll->list sync)
0x4a/0x4b   +0x64 flags          0x4c/0x4d colours
```

### VSCROLL

`vtable+0x14` = `0x5bebe0` only forwards to the base parse; the VS keys are
read by `vtable+0x18` = `0x5c4b20`:

```
005c4b3c FUN_00409370(section,"VS_POSITION")   ; key present?
005c4b4d FUN_00409370(section,"VS_RANGE")
005c4b61 FUN_004094d0(section,"VS_POSITION", 0)     ; default 0
005c4b72 FUN_004094d0(section,"VS_RANGE",    1)     ; default 1
005c4b7c if (pos < 0) pos = 0
005c4b80 if (pos >= range) pos = range-1
005c4b91 FUN_005bbfd0(0x38, pos, range, 0, 0)       ; event to self
```

`EQUIVALENT` copy (`0x5be690`) copies +0x64, +0x74, +0x78, +0x7c from the
source widget (so a duplicated VS keeps position/range).

The HS mirror is `0x5bec00` (HS_POSITION / HS_RANGE, default 0 / 1, event
0x36).

---

## 4. VSCROLL layout (`FUN_005c4720`, also `0x5be7e0` for H)

Raw disassembly of the interesting part:

```
005c476a FLD [ESI+0x54] / PUSH / FSTP [ESP]   ; setSize(up,   W, W)
005c477c CALL 0x005bbdc0
005c4781 PUSH 0 / PUSH 0 / CALL 0x005bbcb0    ; move(up, 0, 0)
005c478c FLD [ESI+0x54] ... CALL 0x005bbdc0   ; setSize(down, W, W)
005c47a1 FLD [ESI+0x58]; FSUB [EBX+0x58]      ; H - down.h
005c47af CALL 0x005bbcb0                      ; move(down, 0, H-down.h)
005c47b4 CMP [ESI+0x74],0x1                   ; range == 1 ?
005c47bc..005c47e4  thumb.h = H-(up.h+down.h); thumb.y = up.h
005c47f5 CALL 0x005bbfd0(0x39, 0,0,0,0)       ; travel = 0
005c4800 FLD [EDI+0x58]; FADD [EBX+0x58]      ; up.h + down.h
005c4806 FLD [ESI+0x58]                       ; H
005c4809 LEA EAX,[EAX+EAX*2]                  ; range*3
005c480e FADD [0x00642f94]                    ; + 5.0f
005c481a FISUB [ESP+0x10]                     ; - 3*range
005c481e FST  [ESP+0x10]                      ; thumbH
005c4822 FCOMP [0x00642f94]  -> min 5.0f
005c4837..005c4848  H - (up.h+down.h+thumbH) -> __ftol -> event 0x39
005c4865 CALL 0x005bbdc0                      ; setSize(thumb, W, thumbH)
005c486a MOV EAX,[ESI+0x7c]                   ; travel
005c4870 IMUL EAX,[ESI+0x78]                  ; travel * position
005c4875 DEC ECX                              ; range-1
005c4876 IDIV ECX
005c4883 FADD [EDI+0x58]                      ; + up.h
005c488b CALL 0x005bbcb0                      ; move(thumb, 0, ...)
```

Pseudo-C:

```c
void vscroll_update_geometry(vscroll *s) {
    widget *up    = find_widget(s->name_up);
    widget *down  = find_widget(s->name_down);
    widget *thumb = find_widget(s->name_thumb);

    set_size(up,    s->w, s->w);   move(up,   0, 0);
    set_size(down,  s->w, s->w);   move(down, 0, s->h - s->w);

    if (s->range == 1) {                 /* nothing to scroll */
        set_size(thumb, s->w, s->h - (up->h + down->h));
        move(thumb, 0, up->h);
        send_event(s, 0x39, 0);          /* travel = 0 */
        return;
    }
    float thumb_h = (s->h - (up->h + down->h + 5.0f)) - (float)(s->range * 3);
    if (thumb_h < 5.0f) thumb_h = 5.0f;
    int travel = (int)(s->h - up->h - down->h - thumb_h);
    send_event(s, 0x39, travel);         /* s->travel = travel */

    set_size(thumb, s->w, thumb_h);
    move(thumb, 0, up->h + (s->travel * s->position) / (s->range - 1));
}
```

Notes / facts worth keeping:

* the up/down buttons are **squares of side = scrollbar width**; the INI W/H
  of the buttons is overwritten every layout;
* the "5.0" and the `3*range` are literally in the binary (`0x642f94` =
  5.0f, `LEA EAX,[EAX+EAX*2]` = 3*range);
* `travel` is the thumb travel in pixels, *not* the visible line count, and it
  is recomputed on every geometry/position change;
* `range == 1` (empty list, or list that fits) gives a full-height thumb and
  `travel = 0`; the bar is still hit-testable but clicking it only re-sends
  positions that clamp to 0.

In the list, the bar is positioned by `sync_list_vscroll` (`0x5bda70`):

```
setSize(VS, (float)list->vscroll_w, list->h);
move(VS, list->w - list->vscroll_w, 0);
move(ctnr, 0, 0);
setSize(ctnr, list->w - VS->w, list->h);
if (list->columns == 1)
    set_column_width(list, 0, (int)(list->w - list->vscroll_w));  /* event 0x44 */
```

(`FLD [ESI+0x54]; FISUB [ESI+0x70]` = w - vscrollWidth; `0x5bdab3..0x5bdae9`.)

---

## 5. VSCROLL input

### 5.1 Mouse move / wheel - `FUN_005cbb30` (slot 0x40)

```
005cbb35..005cbb45  up = find_widget(this->name_up)
005cbb4a MOV CL,[ESI+0x64]                  ; drag flag
005cbb55 MOV EDI,[ESI+0x74]                 ; range
005cbb58 CMP EDI,0x1 / JLE skip             ; only if range > 1
005cbb5d FLD [EAX+0x58]; FADD [EAX+0x50]    ; up.y + up.h
005cbb63 PUSH EDI / CALL __ftol             ; (int)(up.y+up.h)
005cbb6b MOV EAX,[EBX+0x4]                  ; ctx->y
005cbb6e SUB EAX,ECX                        ; y - top
005cbb72 IMUL EAX,EDI                       ; * range
005cbb76 IDIV [ESI+0x7c]                    ; / travel
005cbb7a CALL 0x005c4620                    ; clamp + event 0x38
005cbb7f MOV ECX,[EBX+0x10]                 ; ctx->wheel
005cbb86 MOV EDX,[ESI+0x74]                 ; range
005cbb89 MOV EAX,0x88888889 ... SAR EDX,6   ; ECX / 120
005cbb93 MOV ECX,[ESI+0x78]; SUB ECX,EDX    ; pos - wheel/120
005cbba5 CALL 0x005c4620
005cbbad CALL 0x005c5d80                    ; base mouse move (forwards to child)
```

```c
void vscroll_on_move(vscroll *s, mouse_ctx *ctx) {
    if (s->dragging && s->range > 1) {
        int top = (int)(up->y + up->h);
        vscroll_set_pos(s, (ctx->y - top) * s->range / s->travel);
    }
    if (ctx->wheel != 0)
        vscroll_set_pos(s, s->position - ctx->wheel / 120);
    base_on_move(s, ctx);           /* forward to s->hovered child */
}
```

`mouse_ctx` layout observed: `+0x00` x, `+0x04` y (ints, widget-local),
`+0x08/+0x0c` delta, `+0x10` wheel delta. 120 = `WHEEL_DELTA`; one notch =
one line (never a page).

### 5.2 Mouse down - `FUN_005cbbc0` -> `FUN_005c4650`

```c
void vscroll_on_down(vscroll *s, int button, mouse_ctx *ctx) {
    if (button == 0) vscroll_click(s, ctx->y);   /* left only */
    base_on_down(s, button, ctx);
}

void vscroll_click(vscroll *s, int mouse_y) {    /* 0x5c4650 */
    widget *up = find(s->name_up), *down = find(s->name_down),
           *thumb = find(s->name_thumb);
    int p = s->position, r = s->range;

    if (s->hovered == up)        p = clamp(p - 1, 0, r - 1);
    else if (s->hovered == down) p = clamp(p + 1, 0, r - 1);
    else if (s->hovered == thumb) { s->dragging = 1; return; }  /* no move */
    else p = clamp(p + (thumb->y < mouse_y ? 3 : -3), 0, r - 1);
    send_event(s, 0x38, p, r);                    /* clamped in 0x5c4620 */
}
```

`0x5c4620` is the single clamp point:

```c
void vscroll_set_pos(vscroll *s, int pos) {       /* FUN_005c4620 */
    if (pos < 0) pos = 0;
    if (s->range <= pos) pos = s->range - 1;
    send_event(s, 0x38, pos, s->range);
}
```

Event 0x38 handler (`0x5cbc30`, slot 0x4c):

```
005cbc31.. if (id == 0x38) { this->position = a; this->range = b;
                             FUN_00587340(); FUN_005c4720(); }   /* relayout */
else if (id == 0x39) { this->travel = a; }
FUN_005c5f00(...)      /* base OnEvent -> listener at +0x60 */
```

So every position change re-runs the geometry (thumb rect) and then notifies
the parent/listener.

### 5.3 Mouse up - `FUN_005cbbf0`

```
005cbc08 MOV AL,[ESI+0x64]; if (drag) [ESI+0x64] = 0;   /* clear drag */
FUN_005c5e90(...)                                        /* base */
```

### 5.4 Auto-repeat - `FUN_005cb9f0` (slot 0x30), code at 0x5cba00

Calls the base update, then if the flags at +0x38 have bit 3 set it reads the
real mouse position from the global input context, compares it with the
widget's absolute rect (`global + 0x60/+0x68/+0x6c/+0x70/+0x74`), and if the
pointer is outside it keeps stepping: `hovered == up -> pos-1`,
`hovered == down -> pos+1` (via `0x5c4620`). This is the "hold the arrow and
slide out of the bar" behaviour; there is no visible timer in this function.

### 5.5 Event summary

| event | direction | payload | effect |
|---|---|---|---|
| 0x38 | anyone -> VS | pos, range | set position/range, relayout |
| 0x39 | VS -> itself | travel px | store travel used by drag math |
| 0x36/0x37 | same, HSCROLL | pos,range / travel | mirrored |
| 0x49 | LIST -> itself | first line | `list+0x7c = pos` |

---

## 6. LIST <-> VSCROLL integration

### 6.1 Per-frame update - `FUN_005c64f0` (slot 0x30)

```
005c64f6 CALL 0x005bdc80       ; vs = find_widget(list+0x68)   ("LbVs")
005c64ff MOV EAX,[EDI+0x78]    ; vs->position
005c6503 CALL 0x005bf8e0       ; list event 0x49 -> list+0x7c = position
005c6508 MOV ECX,[ESI+0x58]    ; height (float)
005c6511 CALL 0x005bda20       ; lineHeight
005c651c CALL __ftol           ; (int)height  (round to nearest)
005c6522 IDIV EBX              ; visible = (int)height / lineHeight
005c6528 CALL 0x005bda40       ; n = std::list size (line count)
005c652d CMP EAX,EBX / JGE 0x5c6547
   ; n < visible:
005c6531 PUSH 1 / PUSH 0 / CALL 0x005c4620   ; VS.set(0, range=1)
005c653c PUSH 0 / CALL 0x005bf8e0            ; firstLine = 0
   ; n >= visible:
005c654a SUB EAX,EBX / INC EAX               ; maxFirst = n - visible + 1
005c654d CMP EAX,ECX(VS.range) / JZ 0x5c655b
005c6551 PUSH EAX / PUSH 0 / CALL 0x005c4620 ; VS.set(0, maxFirst)
005c655d CALL 0x005bf060                     ; re-y the lines if firstLine changed
```

```c
void list_update(list *l) {
    vscroll *vs = find_widget(l->name_vs);            /* "LbVs" */
    l->first_line = vs->position;                     /* event 0x49 */
    int visible  = (int)l->h / l->line_height;         /* see note below */
    int n        = list_count(l);
    if (n < visible) {
        vscroll_set(&vs->pos_range, 0, 1);             /* event 0x38 */
        l->first_line = 0;
    } else if (n - visible + 1 != vs->range) {
        vscroll_set(&vs->pos_range, 0, n - visible + 1); /* resets to top! */
    }
    if (l->first_line != l->cached_first) list_reposition_lines(l);
}
```

Important behaviours:

* `visible = (int)(float)height / lineHeight`. The cast is `__ftol`
  (round-to-nearest, default FPU mode), then `idiv` (truncating). For integer
  heights this is plain floor: `ISLAND_LIST` 285/34 = **8** lines,
  `CAMPAIGN_LIST` 285/34 = **8**, `LOAD_GAME_NAMES_LIST` 280/20 = **14**.
* **When the line count changes the range changes, and the position is forced
  back to 0** (`PUSH 0` before `0x5c4620`). Rebuilding a list (new save files,
  new islands) therefore jumps to the top unless the range happens to be
  identical.
* If the list fits (`n <= visible`) range becomes 1 and the bar is disabled
  (full-height thumb, travel 0). There is no partial-row handling: a line is
  either fully inside `visible` or it is not counted.

### 6.2 Scroll position -> line index

`firstLine = vs->position` (1:1, no scaling). Line *i* is drawn at
`y = list.y + (i - firstLine) * lineHeight` (`FUN_005bf060`; it offsets every
line widget with `-(firstLine*lineHeight)`).

### 6.3 Click to line - `FUN_005bdb10` (called from list slot 0x44 `0x5c6b60`)

```
005c6b6d TEST EDI,EDI / JNZ skip          ; left button only
005c6b74 SHR EAX,0x5 / TEST AL,1 / JNZ    ; skip if +0x38 bit5 (DRAG_AND_DROP)
005c6b7c CALL 0x005bdb10
005bdb26 CALL find(vs) / CMP [ESI+0x44]   ; if the scroll bar is hovered: return
005bdb37 CALL __ftol(height); IDIV [ESI+0x74]   ; visible
005bdb40 MOV EBP,[ESI+0x7c]               ; firstLine
005bdb4d CALL 0x005bded0                  ; n = line count
005bdb52 ADD EBP,EDI; CMP EBP,EAX; JLE    ; if firstLine+visible > n ->
005bdb62 SUB EDI,EBP                      ;     visible = n - firstLine
005bdb66.. IDIV [ESI+0x74]                ; line = firstLine + (y-list.y)/lineHeight
005bdb8b CALL 0x005bee10                  ; event 0x41 -> list+0x84 = line
005bdb97 CALL __ftol([ESI+0x4c])          ; list.x
005bdb9c MOV ECX,[EBP]                    ; ctx->x
005bdba1..005bdbbe  accumulate [list+0x98+4i] until localX < sum
005bdbc6 CALL 0x005bee30                  ; event 0x42 (selected column; no-op here)
```

```c
void list_on_click(list *l, int button, mouse_ctx *ctx) {
    if (button != 0 || (l->flags & 0x20)) { base_on_down(...); return; }
    if (l->hovered == vs) { base_on_down(...); return; }  /* clicked the bar */
    int visible = (int)l->h / l->line_height;
    if (l->first_line + visible > n) visible = n - l->first_line;
    int line = l->first_line + (ctx->y - (int)l->y) / l->line_height;
    if (line < l->first_line + visible) l->selected_line = line;
    /* column: first i with cumulative LB_COLUMNi_WIDTH > ctx->x - l->x */
    l->selected_column = ...;   /* event 0x42, ignored by this build */
    base_on_down(...);
}
```

Selection is stored at `list+0x84` (default -1). There is no proportional
clamp when the list shrinks: `0x47` (delete line) clears the selection
(`FUN_005bee10(-1)`) when the deleted index equals it, and `0x48` (clear)
clears `+0x84`/`+0x80`. `list+0x80` is a second "current/highlight" slot used
only for line repositioning and reset to -1 on delete/clear.

### 6.4 Wheel over a list

`LIST` slot 0x40 (`0x5c6b30`):

```
005c6b38 MOV EAX,[EDI+0x10]      ; ctx->wheel
005c6b3d JZ done                 ; no wheel -> base only
005c6b3f CALL 0x005bdc80         ; vs = find(list+0x68)
005c6b44 CMP [ESI+0x44],EAX      ; already hovered?
005c6b4a PUSH EDI / MOV ECX,EAX / CALL 0x005bbf40   ; vs->OnMouseMove(ctx)
005c6b54 CALL 0x005c5d80         ; base
```

So the wheel is routed *into* the VSCROLL's mouse-move handler; the list only
forwards when the bar is not already the hovered child.

---

## 7. HSCROLL (brief)

Same implementation, mirrored to x. `0x5be620` ctor (0x80, vtable 0x647168),
create children `0x5be960`, layout `0x5be7e0`, clamp+send `0x5be6e0`, click
`0x5be710`, move/wheel `0x5c7740`, down `0x5c77d0`, up `0x5c7800`, event
`0x5c7820`.

* events are **0x36** (position,range) and **0x37** (travel);
* `thumb_w = max(5, (W - (left.w + right.w + 5)) - 3*range)`,
  `travel = W - left.w - right.w - thumb_w`,
  `thumb.x = left.w + pos*travel/(range-1)`, `thumb.y = 0`,
  buttons are `H x H` squares;
* drag uses `ctx->x - (left.x + left.w)`, same `*range/travel`;
* track click is also +-3, wheel also `delta/120`.
  (Evidence: `0x5c7740` decompile shows `((*ctx - top) * range) / travel` and
  `pos - ctx[4]/0x78`.)

Used by `OPTIONS_*_HSCROLL` (`HS_POSITION=5`, `HS_RANGE=32`).

---

## 8. Port contract

```cpp
struct ScrollBar {                 // VSCROLL or HSCROLL
    bool  dragging = false;
    int   range    = 1;            // number of positions (>=1)
    int   pos      = 0;            // 0..range-1
    int   travel   = 0;            // px of thumb movement
    int   width    = 0;            // bar thickness (LB_VSCROLL_WIDTH)
    int   height   = 0;            // bar length (list height)
    float btn      = 0;            // button side = width
};

void layout(ScrollBar& s) {                       // 0x5c4720
    float up = s.width, down = s.width;           // square buttons
    if (s.range == 1) {
        s.travel = 0; thumb = {0, up, s.width, s.height - 2*up};
        return;
    }
    float th = (s.height - (up + down + 5.0f)) - 3.0f * s.range;
    if (th < 5.0f) th = 5.0f;
    s.travel = (int)(s.height - up - down - th);
    thumb = {0, up + s.pos * s.travel / (s.range - 1), s.width, th};
}

void set_pos(ScrollBar& s, int p) {               // 0x5c4620
    if (p < 0) p = 0;
    if (p >= s.range) p = s.range - 1;
    s.pos = p; layout(s);  notify_listener();
}
```

Per-event behaviour to reproduce:

| input | action |
|---|---|
| LMB down on Up | `set_pos(pos-1)` (clamp) |
| LMB down on Down | `set_pos(pos+1)` (clamp) |
| LMB down on track above thumb | `set_pos(pos-3)` |
| LMB down on track below thumb | `set_pos(pos+3)` (compare `thumb.y < mouseY`) |
| LMB down on thumb | `dragging = true`, no position change |
| mouse move while dragging and `range > 1` | `set_pos((mouseY - (upY+upH)) * range / travel)` |
| wheel (`delta` / 120) | `set_pos(pos - delta/120)` |
| LMB up | `dragging = false` |
| `range == 1` | no scrolling, thumb fills the track |

Listbox:

```cpp
visible    = (int)list.h / list.line_height;      // int cast then idiv
list.first = vs.pos;                              // every update
maxFirst   = n - visible + 1;                     // n = line count
if (n < visible) { vs.set(0, 1); list.first = 0; }
else if (maxFirst != vs.range) vs.set(0, maxFirst);   // resets to top

vs.w = LB_VSCROLL_WIDTH; vs.h = list.h;
vs.x = list.w - LB_VSCROLL_WIDTH; vs.y = 0;
ctnr.w = list.w - vs.w; ctnr.h = list.h; ctnr.x = ctnr.y = 0;
if (columns == 1) column[0] = (int)(list.w - LB_VSCROLL_WIDTH);
```

Two details that a naive port gets wrong:

1. the drag is **absolute** (the thumb top jumps to the cursor Y), not a
   relative delta;
2. the track click is a fixed **3 lines**, not `visible` lines;
3. the drag divides by `range` while the thumb placement divides by
   `range-1`; both end up consistent at pos 0 and pos range-1 after clamping.

---

## 9. Uncertainty / open

* The auto-repeat path (`0x5cb9f0`/`0x5cba00`) depends on flags bit 3 of
  `+0x38` and on a global input context; I did not identify which INI key sets
  bit 3 nor the caller cadence. Treat "hold to repeat" as unverified.
* `list+0x80` (second line index) and the container's `event 0x2a` call in
  `0x5bd880` are not fully explained; they do not affect scrolling.
* Event 0x42 (selected column) falls through to the base handler in this build,
  i.e. column selection is computed but not stored by the listbox.
* `FUN_00587340` (called on every 0x38) was not decompiled; it looks like a
  dirty/invalidate call, not state.
* `LB_FIRST_LINE_SEPARATOR` / colour keys were not fully mapped to bits; they
  are cosmetic and outside the scroll path.
* No keyboard (arrow/page) handling was found in the LIST/VSCROLL code; only
  mouse and wheel.
