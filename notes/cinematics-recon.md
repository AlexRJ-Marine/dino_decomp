# Cinematics page (`CINEMATICS_PAGE`, menu index 6)

Status: **solved and ported.** The page lists the campaigns' ending videos,
not a video folder, and START plays the selected campaign's `EndingVideo`.
Evidence: Ghidra disassembly at the addresses below plus the retail data.

## List population

`FUN_00518210` only resolves the page widgets by section name. The list is
filled by the page's enter virtual `FUN_00518400` (page vtable `0x64291c`,
slot +4 = `0x642920`). It walks the **campaign database**, not a hard-coded
table:

```text
005184c2  MOV EAX,[ECX+8]      ; [page+0x38] -> +8 = snoCampaignDataBase*
005184c5  MOV ECX,[EDI+0x4c]   ; CINEMATICS_LIST widget
005184c8  ADD EAX,0x20
005184cf  MOV ESI,[EAX+8]      ; campaign vector end   (db+0x28)
005184d2  MOV EDX,[EAX+4]      ; campaign vector begin (db+0x24)
005184d5  SUB ESI,EDX
005184da  SAR ESI,2            ; count = (end-begin)/4
005184dd  CALL 0x5bf050        ; mcListBox clear
...
0051854c  PUSH 0x661ed8        ; "ISLAND_LIST_BUTTON" row graphic
005185ab  CALL 0x5bb940        ; list message 0x21 (row graphic)
0051859a  ADD ECX,0x3c         ; item + 0x3c = EndingVideoName
0051859e  MOV ECX,0x6744e0
005185a3  CALL 0x40bfe0        ; loc resolve -> const char*
005185a9  CALL 0x5bb9b0        ; list message 0x1b (row text)
```

`[page+0x38]` is `&menuInterface[0x88]` (passed from `FUN_00517410` at
`0x51768f`); `+8` is the campaign DB built by `FUN_005612d0` from
`CampaignList.ini`. Each campaign object has:

- `+0x30` = `[BRIEFING] EndingVideo` (file name)
- `+0x3c` = `[BRIEFING] EndingVideoName` (loc id)

Retail rows (7 campaigns, `CampaignList.ini` first line `7`):

| Row | Campaign | `EndingVideoName` | `EndingVideo` |
| ---: | --- | --- | --- |
| 0 | Cpg01 | `&Video_Basket` "MAGIC T-REX" | `basket.mpg` |
| 1 | Cpg02 | `&Video_Tennis` "TIE-BREAK" | `tennis.mpg` |
| 2 | Cpg03 | `&Video_Trampoline` "BOING !" | `trampoline.mpg` |
| 3 | Cpg04 | `&Video_Trappe` "THE TRAP" | `Trappe.mpg` |
| 4 | Cpg05 | `&Video_Labo` "TCHERNOPIG" | `mixe.mpg` |
| 5 | Cpg06 | `""` | `""` |
| 6 | Cpg08 | `""` | `""` |

`&Video_Approach` ("STEP BY STEP") and `&Video_FartContest` ("FRAGRANCE")
exist in `MenuEng.loc` but are referenced by no campaign in this build
(dead entries). `generic.mpg` is only the sentinel request `0x29A` = 666
(`0x402656`); it is not a list row.

## START / INTRO

`CINEMATICS_START_BUTTON`'s handler `0x518360` only publishes the selected
row and plays the click sound:

```text
00518360  MOV EAX,[ESP+4]
0051836a  MOV EBX,[EAX+0x5c]    ; page object
0051836f  CMP ECX,0             ; click only
00518374  MOV ECX,[EBX+0x38]    ; video controller
0051837c  MOV EDX,[ECX]
0051837e  MOV ECX,[EBX+0x40]    ; selected row
00518381  MOV [EDX+0x10],ECX    ; publish index
00518384  MOV EDX,0x661e58      ; "clicDefault"
```

The app frame handler builds the path and plays it:

```text
00402647  MOV EAX,[EBP+0x48]    ; video controller
0040264a  MOV ESI,[EAX+0x20]    ; requested index
00402656  CMP ESI,0x29a         ; sentinel -> generic.mpg
004026fa  MOV EAX,[EBP+0x50]    ; campaign DB
00402700  MOV EAX,[ECX+ESI*4]   ; campaign[i]
00402703  MOV ECX,[EAX+0x30]    ; EndingVideo
0040271a  MOV EDX,[0x64b610]    ; "data/video/"
00402958  CALL 0x517a40         ; mvVideo::play(path)
```

`CINEMATICS_INTRO_BUTTON` is **inert**: it is `TYPE = "WIDGET"` (a static
picture, `INTRO_BUTTON` -> `menu_outcome4.tga`), its section name does not
exist in `DinoIsland.exe`, and `FUN_00518210` resolves only PAGE, LIST,
START and CANCEL. `intro.mpg` is played once at startup from
`[DINO] UseIntroVideo` (`0x401d4c`, `0x64b628`).

## Gating

The row handler `FUN_00518790` greys rows whose text is empty and rows past
the `campaign_db+0x1c` limit (`dino.dat`): `MOV ECX,[EAX+0x1c] / DEC ECX` ->
enable when `row < value-1`, because an ending video unlocks only after the
campaign is finished. `0x402854` raises the value to `max(value, index+2)` and
persists it. See `notes/campaign-unlock-runtime.md` §Correction: the dword is
a **count**, not a mask. The port matches both rules
(`Menu::cinematic_unlocked`).

## Port status (2026-10-28)

- `src/ui/menu.{h,cpp}`: `cinematic` selection, `cinematic_requested`,
  `cinematic_video()`.
- `src/viewer/main.cpp`: page-6 list draws `EndingVideoName` rows (empty rows
  blank/grey); START resolves `Data/Video/<EndingVideo>` and calls the
  existing `play_video` MPEG player, then returns to the menu.
- Test: `tests/test_viewer_cpp.py::test_cpp_retail_menu_cinematics_page`
  (row labels, empty rows, basket/trampoline/mixe playback, row 5 and a
  fresh-profile `dino.dat=1` no-op).
- Screenshot: `build/campaign/menu-cine.png` (local, not tracked).

## Open

- `CINEMATICS_INTRO_BUTTON` animation: the INI points at `INTRO_BUTTON` in
  `MenuGraphicDB`; the port draws the static frame.
