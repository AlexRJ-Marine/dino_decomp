# Playing-video exit / skip (retail behaviour)

Status: **solved (static)**. One player for every MPEG in the game
(`mvVideo`, mcLib), driven asynchronously by the menu view. It is **not** a
blocking modal loop and it is **not** stopped by a key: the only skip input is
**any mouse button** (left, right or middle). Natural end and skip both return
to the page that started the video (Cinematics = page 6, or Main Menu = page 0
for the startup intro); music resumes and the menu UI is redrawn.

Evidence: Ghidra on `/DinoIsland.exe-ebea17` (MD5 5c8352429b0ede4a0d3ff4c8743f59cb).
No runtime trace was needed; every claim below is a direct instruction/xref.

## The player object and the single play entry point

`0x517a40` = MenuView video wrapper `play(path)`:

```text
00517a40 PUSH ESI / MOV ESI,ECX
00517a43 PUSH 0x1
00517a45 MOV ECX,[ESI+0xac]
00517a4b CALL 0x517e60            ; video mode ON (hide/disable menu UI)
00517a50 MOV ECX,[0x674528]
00517a56 CALL 0x40eb80            ; renderer begin/clear
00517a73 CALL 0x4052c0 (0x2c)     ; alloc wrapper
00517a7e CALL 0x5c0210            ; new mcLib mvVideo
00517a90 MOV [ESI+0x9c],EAX       ; store player on the menu view
00517a96 CALL 0x5c02c0            ; mvVideo::open(path, mode 3)
00517aa1 CALL 0x5c0330            ; mvVideo::start
```

- `0x517ab0` = `isPlaying()`: returns 0 if `[view+0x9c]==0`, else the
  player status byte from `0x5c0350` (nonzero = still playing).
- Only these 4 call sites exist for `0x517a40`: `0x401ddf` (startup intro),
  `0x4026e7` (command 0x29A = "generic.mpg"), `0x40279d` (campaign row
  `EndingVideo`), `0x402958` (fourth path, same helper).
- Only one xref to the mvVideo constructor `0x5c0210` and one to the
  movie-object constructor `0x5c7c50`: **there is no second video player.**

## The per-frame update: where the skip test lives

`0x517930` = MenuView / MenuInterface update (`MenuInterfaceUpdate`,
registered by `0x596500` as the overlay callback `0x596560`). It runs every
frame while the menu is up:

```text
00517944 MOV ECX,[ESI+0x9c]        ; player
0051794e JZ  0x517a0c              ; none -> normal menu path
00517954 CALL 0x5c0350             ; playing?
0051795b JZ  0x517a0c              ; no  -> normal menu path
00517961 MOV EAX,[0x006750ec]      ; mcInput mouse state (DIMOUSESTATE*)
0051796a MOV CL,[EAX+0x0e]         ; middle button
0051796d MOV DL,[EAX+0x0d]         ; right button
00517974 MOV DL,[EAX+0x0c]         ; left button
00517977 ADD ECX,EDX               ; sum of the three buttons
0051797f JZ  0x51799f              ; 0 -> no skip, pump a frame
```

Nonzero (any button down) falls into `0x517981`-`0x5179f0`:

```text
00517985 MOV EAX,[ECX] / PUSH 0 / CALL [EAX]   ; player vtable[0](0) = dtor
005179ca CALL 0x405400                          ; operator delete
005179d5 MOV [ESI+0x9c],0                       ; player cleared
005179e1 PUSH 0 / CALL 0x517e60                 ; video mode OFF (UI back)
005179f0 RET 4
```

No other input is read on this path. `0x6750ec` is the DirectInput mouse
state filled by `0x4456c0` (`DIMOUSESTATE`: `+0xc` left, `+0xd` right,
`+0xe` middle, `+0xf` fourth button **not** checked). Mouse motion, wheel and
the whole keyboard array `0x6750f0` are not consulted.

## No keyboard skip

- `0x5960c0` (main WndProc): `WM_KEYDOWN 0x100` -> `0x5bb6e0`, which forwards
  the key to the overlay widget tree (`0x5bbf90`, message code 10). That path
  never touches `[view+0x9c]` and never calls `0x5c0340/0x5c0370`.
- `WM_LBUTTONDOWN/UP` (`0x5bb640/0x5bb600`) only set widget button state; the
  actual skip is the DirectInput level read in `0x517930`.
- The movie subclasses the app window (`0x5c8f10` installs `0x5c8fa0`, which
  forwards to `0x5c7e00`). `0x5c7e00` handles only the player's own registered
  message and `CallWindowProcA`s everything else back to `0x5960c0`: no key
  handler in the movie code.
- `WM_DESTROY` -> `PostQuitMessage` is the only quit path; there is no Esc
  handling in the frame handler (`0x402647` switches on a UI command id, not
  a key code).
- There is no "press key / click to skip" string and no skip widget in
  `Data/Overlay/MenuInterface.Ini`; the CINEMATICS_INTRO_BUTTON is a static
  picture (see `notes/cinematics-recon.md`). **No visible skip hint exists.**

## Frame pump / end of video (what happens when it just finishes)

`0x517930` second half:

```text
0051799f CALL 0x5c0360             ; wait for next decoded frame
005179aa TEST AL,AL
005179ac JNZ  0x5179f3             ; frame ready
005179ae ... destroy player (same as skip)   ; timeout / stalled player
005179f3 CALL 0x5c0370             ; SetEvent(req)+ResetEvent(frame)
005179f8 PUSH 0,0,0 / CALL 0x40eab0 ; render the movie frame
```

- `0x5c0360` -> `0x5c8d60` -> `WaitForSingleObject(movie+0x168, timeout)`;
  timeout is `movie+0x10` ms or 1000 ms when zero (`0x5ce9e0` treats
  `0x102 WAIT_TIMEOUT` as "no frame").
- `0x5c0370` -> `0x5cea00`: `SetEvent(movie+0x16c)` + `ResetEvent(movie+0x168)`
  i.e. "request the next frame". The decoder thread (`0x5cd440`) waits on
  `movie+0x16c` and signals `movie+0x168` after decoding a frame (`0x5cd92c`).
  So `0x5c0360` is a frame-ready wait, not the end-of-stream test.
- End of stream is seen through the status byte `0x5c0350` becoming 0; the
  update then takes `0x517a0c` (draw the menu instead of the movie).

`0x517ad0` = MenuInterfaceDraw (vtable slot +0x14 of the menu object). When
the player exists but the status byte is 0 it frees the player and re-enables
the UI, then draws the page:

```text
00517ad3 MOV ECX,[ESI+0x9c] / TEST / JZ 0x517b35
00517add CALL 0x5c0350 / TEST AL / JNZ 0x517b35   ; still playing: draw movie only
00517af4 CALL [EAX] (dtor) ... 00517b14 MOV [ESI+0x9c],0
00517b30 CALL 0x517e60 (0)                        ; video mode OFF
00517b35 ... if no player -> draw the menu page (0x40e5b0 ...)
```

The page itself is never changed by play/skip/end. The overlay stays on the
page that started the video, so the result is **return to the same page**, with
the same list selection.

## Music / UI state

`0x402627`-`0x402644` in the app frame handler (`LAB_00402460`, virtual frame
method referenced from vtable `0x6252f8`) is an edge detector on
`isPlaying()`:

```text
0040262a CALL 0x517ab0        ; playing this frame?
0040262f MOV BL,AL
00402635 MOV AL,[EBP+0x3e]    ; was playing last frame?
0040263c MOV ECX,[EBP+0x54]
0040263f CALL 0x4fd910        ; menu music restart ("music", 0x65e154)
00402644 MOV [EBP+0x3e],BL
```

So on the 1->0 edge (natural end, skip or stall) the menu music is restarted
by the frame handler. `0x517a40` also calls `0x517e60(1)` (video mode on) and
the skip/end path calls `0x517e60(0)` (video mode off), i.e. the menu UI is
hidden/disabled during playback and re-enabled afterwards.

## Startup intro (`intro.mpg`)

`0x401d4c`-`0x401de4` in the init/run function `LAB_00401540`:

```text
00401b17 PUSH 0 / CALL 0x403c80   ; open menu page 0 (main menu) first
00401d4c MOV ECX,[EBP+0x60] / MOV AL,[ECX+0x14]  ; [DINO] UseIntroVideo
00401d5a ... build "data/video/" + [0x64b628]    ; 0x64b628 -> "intro.mpg"
00401ddf CALL 0x517a40                            ; same player, non-blocking
```

`0x5054f0` reads `UseIntroVideo` from `Dino.ini` into `config+0x14`
(`0x5055ac` string ref). `play()` returns immediately; the function finishes
startup and the frame loop drives the player through the same `0x517930`
update. Therefore the intro is skipped by the same **mouse click**, and when
it ends/is skipped the main menu (page 0) is shown. Ctrl/Esc are not special.
(`0x64b628` is a `char*` initialized to `"intro.mpg"` at `0x64bc20`.)

## Campaign-ending video (CPGEND)

- The CPGEND panel (`0x5234e0`, strings `CPGEND_PANEL`,
  `CPGEND_TEXT_ZONE`, `CPGEND_OUTCOME_ICON` at `0x662a74`...) builds the
  in-game outcome panel; it has **no** call to the video player and no
  xref to `0x517a40`.
- Campaign `EndingVideo` / `EndingVideoName` are read by `0x564cc0`
  (`CampaignBase.ini`, `[BRIEFING]`, `0x564f05` / `0x564ed8`) and used only
  by the menu Cinematics list. The ending MPEG is watched from that page
  (row unlock `row < dino.dat-1`), so it uses the same player and the same
  mouse-click exit. No campaign-specific exit exists.
- `generic.mpg` (`0x64b62c` -> `0x64bc14`) is the fallback for UI command
  `0x29A` = 666 (`0x402656`); same player, same exit.

## Port contract

1. **Do not block on the MPEG.** Keep the frame loop; poll the decoder each
   frame (retail: wait for "frame ready"/end with a timeout).
2. **Skip:** while a cinematic is alive, if LMB, RMB or MMB is down -> stop
   immediately, destroy the player, hide the movie, return to the *same*
   page (page 6 Cinematics, or page 0 for the intro). No page advance.
3. **End:** when the decoder reports end-of-stream (or a stall/timeout),
   take the same return path. Resume menu music on the playing 1->0 edge
   (`0x40263c` -> `0x4fd910`).
4. **Input edge case:** retail is level-triggered, so a still-held launch
   click would skip on the next poll. In practice menu buttons act on
   mouse-up (`0x5bb600`), so the click is already released before playback.
   In the port, ignore the press that launched the video; start testing the
   buttons on the following frame.
5. **Keyboard:** Esc and other keys must not be the only exit; retail has no
   key skip. Keeping Esc as an *extra* convenience is a port-only deviation
   (document it), but the primary path must be any mouse click.
6. **UI:** hide/disable the menu UI while playing, restore on exit; no skip
   hint is drawn in retail.

## Uncertainty

- The exact role of `0x40eab0(0,0,0)` (frame blit vs. clear) is inferred from
  the renderer vtable call at `0x40eab0`; it does not affect the exit rules.
- `movie+0x10` (frame-wait timeout, default 1000 ms) is never set by the
  menu code we traced; the 1 s fallback is from `0x5c8d60`. A >1 s decoder
  stall would destroy the player in retail.
- No runtime test was run for this note; the skip input is proven only by the
  instruction stream (`0x517961`-`0x5179d0`). A live confirmation would be a
  click on any button during `basket.mpg` and during `intro.mpg`.
