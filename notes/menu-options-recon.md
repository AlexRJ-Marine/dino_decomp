# Main-menu Options page (OPTIONS_PAGE, menu index 5) — binary recon

Status: page controller, all handlers and the backing config located and
read. Live/apply behaviour proven from the code. Two items remain open
(marked below).

Scope: `DinoIsland.exe` (`/DinoIsland.exe-ebea17`, MD5 5c835242...).
This is the **main-menu** Options page. It is a different object from the
in-park options controller `FUN_0050c1a0` (see `notes/ui-recon.md` §In-park
options).

Retail data: `game/%MAINDIR%/Data/Overlay/MenuInterface.Ini` and
`game/%MAINDIR%/Data/Config/Dino.ini`.

## 1. Page object and widget map

`FUN_00518830` = OPTIONS_PAGE constructor (`Date: 2026-10-28`, called once
from the menu view `FUN_00517c60` @ `0x00517d9e`, together with all other
menu pages). It resolves each INI element by name through the menu manager
(`0x006744e0`) and stores the resulting control object:

```
00518888 PUSH 0x662044            ; "OPTIONS_MUSIC_HSCROLL"
0051889c CALL 0x005bacd0
005188a1 MOV EAX,[EAX+0x34]       ; -> interface
005188b1 CALL 0x004139d0          ; -> control object
005188c4 MOV [ESI+0x68],EAX       ; page field
```

| page offset | INI element |
|---|---|
| `+0x3c` | `OPTIONS_PAGE` (page interface; `+0x38` on it = visible/enabled flags) |
| `+0x68` | `OPTIONS_MUSIC_HSCROLL` |
| `+0x84` | `OPTIONS_SOUNDS_HSCROLL` |
| `+0x80` | `OPTIONS_GAME_SPEED_HSCROLL` |
| `+0x70` | `OPTIONS_GRAPH_QUALITY_INDICATION` |
| `+0x6c` | `OPTIONS_GRAPH_TEXTURE_QUALITY_INDICATION` |
| `+0x88` | `OPTIONS_SCREEN_SIZE_INDICATION` |
| `+0x74` | `OPTIONS_GRAPH_MIXING_INDICATION` |
| `+0x78` | `OPTIONS_MUSIC_MIXING_INDICATION` |
| `+0x7c` | `OPTIONS_GRAPH_TEXTURE_DEPTH_INDICATION` |
| (locals) | `OPTIONS_OK_BUTTON`, `OPTIONS_CANCEL_BUTTON` (not stored) |

**Important:** the page binds the `*_INDICATION` elements, not the matching
`*_BUTTON` elements. In the INI the `*_BUTTON` entries are plain `WIDGET`
icons; every `*_INDICATION` is `TYPE = "BUTTON"`, `TEXTURE =
"OPTIONS_TEXT_ZONE"`, `TEXT = "&Ok"` — i.e. the clickable text zone that
shows the current value. The `_BUTTON` names have **no xref in the binary**.

Other page fields used by the page:

| offset | meaning |
|---|---|
| `+0x38` | display/view context (gsc object). `+0x04` overlay renderer, `+0x10` -> `dinoConfig`, `+0x18` -> audio manager |
| `+0x40` | current screen-mode index |
| `+0x44` | screen-mode container sub-object (`+0x04/+0x08` = string array begin/end -> page `+0x48/+0x4c`; `+0x14/+0x18` = mode array begin/end -> page `+0x58/+0x5c`; `+0x20` = capability byte -> page `+0x64`) |
| `+0x64` | "hardware T&L mixing supported" flag (see §5) |

Handler binding uses two listener slots on `[[widget+0x60]]`:
`+0x24` is the *value-changed* callback (called with event code `0x36`),
`+0x20` is the *click* callback (called with code `0`). The callback's first
argument is the firing widget; `widget+0x5c` is its owner page.

```
00518ab9 CALL 0x005bc570              ; begin-edit on the listener block
00518ac2 MOV [ECX+0x24],0x518c10      ; music hscroll value-changed
00518acc CALL 0x005bbf20              ; end-edit (also passes page as userdata)
```

## 2. Function map

| address | role |
|---|---|
| `0x00518830` | OPTIONS_PAGE ctor (bind widgets + callbacks) |
| `0x005191c0` | page init / "on show" (vtable slot 1): sets every control from current state |
| `0x00519560` | page close (vtable slot 2), called by OK **and** Cancel |
| `0x00519190` | page dtor |
| `0x00518c10` | music HSCROLL changed |
| `0x00518cc0` | sounds HSCROLL changed |
| `0x00518db0` | game-speed HSCROLL changed |
| `0x00518d70` -> `0x00519640` | graph (LOD) quality cycle |
| `0x00518d90` -> `0x005196f0` | texture quality cycle |
| `0x00518e60` -> `0x005197a0` | screen size cycle |
| `0x00518e80` -> `0x00519890` | graph mixing (HardwareTNL) toggle |
| `0x00518ea0` | music mixing (HardwareSound) toggle |
| `0x00518ff0` | texture depth toggle |
| `0x00518080` | OK **and** Cancel click (same pointer for both) |
| `0x005199b0` | quality label setter (`&High`/`&Medium`/`&Low`/`&Auto`) |

Supporting mcLib code:

| address | role |
|---|---|
| `0x005bbfd0` | widget->fireEvent(code, a, b, ...) (calls widget vtable `+0x4c`) |
| `0x005be6e0` | HSCROLL set position: clamp to `[0, range-1]`, fire event `0x36` |
| `0x005be710` | HSCROLL left-click logic (arrows / thumb / track) |
| `0x005be7e0` | HSCROLL layout (thumb x from position/range) |
| `0x005c7740` | HSCROLL drag + wheel |
| `0x005c7820` | HSCROLL event mirror (event `0x36` stores pos/range, `0x37` track width) |
| `0x005be690` | HSCROLL copies range/pos/track from its INI template |
| `0x005bebf0` | HSCROLL INI parse of `HS_POSITION`/`HS_RANGE` |
| `0x005ba6b0` | INI `TYPE` -> class id (5 = HSCROLL) |

## 3. Values and storage

All target state lives in one config struct, `dinoConfig`, reached as
`[[page+0x38]+0x10]`. Layout recovered from its loader `FUN_005054f0` and
saver `FUN_00505bd0` (both keyed by `dino.ini` sections):

| offset | type | INI key | values |
|---|---|---|---|
| `+0x00` | int | `[GRAPHIC] Resolution` w | pixels |
| `+0x04` | int | h | pixels |
| `+0x08` | int | bpp | `0x10` or `0x20` only (else 800/600/16) |
| `+0x0c` | int | `TextureDepth` | `0x10` or `0x20` |
| `+0x10` | byte | `HardwareTNL` | 0/1 |
| `+0x11` | byte | `[SOUND] HardwareSound` | 0/1 |
| `+0x12` | byte | `UseSound` | 0/1 |
| `+0x14` | byte | `[DINO] UseIntroVideo` | 0/1 |
| `+0x15` | byte | `DebugInfo` | 0/1 |
| `+0x18` | int | `[DINO] GameSpeed` | 0..100 (clamped on load) |
| `+0x1c` | mcString | `Language` | e.g. "Eng" |
| `+0x28` | int | `[SOUND] FXVolume` | 0..100 |
| `+0x2c` | int | `MusicVolume` | 0..100 |
| `+0x30` | int | `[GRAPHIC] TextureQuality` | 0 HIGH, 1 MEDIUM, 2 LOW, 3 AUTO |
| `+0x34` | int | `LODQuality` | same enum |

Load defaults when a key is missing (`FUN_005054f0`): 800x600x16,
TextureDepth 16, TextureQuality AUTO, LODQuality AUTO, HardwareTNL 0,
UseSound 1, HardwareSound 0, FXVolume 50, MusicVolume 50, GameSpeed 10,
UseIntroVideo 1, DebugInfo 0. The shipped `Dino.ini` has
`GameSpeed=50, HardwareTNL=1, HardwareSound=0, FXVolume=97,
MusicVolume=97, TextureQuality=AUTO, LODQuality=AUTO, TextureDepth=16`.

The audio volumes are **not** read from the config at page entry; they are
read from the audio manager (see §4) and copied back into `+0x28/+0x2c` at
application shutdown.

Decompiler caution: Ghidra prints `param_1[6] = 0x32;` after the GameSpeed
clamp in `FUN_005054f0`. The instruction stream at `0x00505607..0x00505623`
has no such store — it is an artifact. GameSpeed really keeps the INI value
(clamped 0..100, default 10).

## 4. HSCROLL behaviour (common to all three sliders)

INI: `TYPE = "HSCROLL"`, `HS_POSITION = 5`, `HS_RANGE = 32`, with children
`..._BTN_LEFT` (`EQUIVALENT = "HsBtnLeft"`), `..._SCROLLRIGHT`
(`"HsBtnRight"`), `..._BTN_THUMB` (`"HsBtnThmb"`).

Parsing (`0x005bec14`):
```
PUSH "HS_POSITION"; CALL 0x409370 (key exists?)
PUSH "HS_RANGE";    CALL 0x409370
pos   = 0x4094d0(section,"HS_POSITION",0)
range = 0x4094d0(section,"HS_RANGE",1)
if (pos < 0) pos = 0;  if (pos >= range) pos = range-1;
```
`+0x74 = range`, `+0x78 = position`, `+0x7c = track pixel width`,
`+0x100 = dragging flag`.

Position to value (this is the program's only mapping; identical in all three
handlers, e.g. music at `0x00518c23`):
```
00518c2d MOV ECX,[EAX+0x78]     ; position
00518c30 MOV EDX,[EAX+0x74]     ; range
00518c3a FILD qword [ESP+0xc]   ; (double)position
00518c46 FMUL [0x006256b8]      ; * 100.0
00518c4c FIDIV [ESP+0xc]        ; / range
00518c50 FADD [0x0062544c]      ; + 0.5
00518c56 CALL 0x005982d4        ; __ftol -> int
```
`value = (int)(pos * 100.0f / range + 0.5f)` — round to nearest, **divide by
`range`, not `range-1`**. With the retail `HS_RANGE = 32` the maximum
position is 31, so the top value is **97**, never 100. The shipped
`Dino.ini` confirms it (`MusicVolume=97`, `FXVolume=97`).

Reverse mapping, done only by the page init (`0x0051925c`):
`pos = (int)(value * 0.01f * range + 0.5f)`, then clamped to `range-1` by
`FUN_005be6e0`.

Input, all handled inside the HSCROLL class:

* **left arrow** (`FUN_005be710`, clicked == `+0x6c`): `pos-1`, lower clamp 0;
* **right arrow** (clicked == `+0x70`): `pos+1`, upper clamp `range-1`;
* **thumb** (clicked == `+0x68`): sets `+0x100 = 1`, starts drag (no value
  change on press);
* **track click** (anything else): compares mouse x with the thumb centre
  (`thumb+0x4c`) and does **`pos+3` / `pos-3`**, clamped;
* **drag** (`FUN_005c7740`, while `+0x100`): absolute
  `pos = ((mouseX - thumbOffset) * range) / trackWidth`, clamped by
  `FUN_005be6e0`;
* **wheel** (`FUN_005c7740`, `param_2[4]` = wheel delta):
  `pos = pos - delta/120` (so one notch = ±1; wheel *up* lowers the value).

Each change re-fires event `0x36` through `0x005bbfd0(code, pos, range, 0, 0)`;
`FUN_005c7820` copies `pos`/`range` into `+0x78/+0x74` and re-lays the thumb.
So the value is applied on every click/drag step — **live**, never buffered
until OK.

Fill colour/arrow repeat: no repeat timer found; a held arrow produces one
step per click.

## 5. Control table

| widget | handler | writes | values | label | live? |
|---|---|---|---|---|---|
| `OPTIONS_MUSIC_HSCROLL` | `0x518c10` | audio manager music volume (`FUN_004fda60`, byte at `[mgr+0xc]+1`); `dinoConfig+0x2c` only at shutdown | 0..97 (0..100 after `+0.5` rounding) | own thumb position | live |
| `OPTIONS_SOUNDS_HSCROLL` | `0x518cc0` | audio manager FX volume (`FUN_004fda30`, byte at `[mgr+0xc]+0`); `+0x28` at shutdown | 0..97 | thumb | live |
| `OPTIONS_GAME_SPEED_HSCROLL` | `0x518db0` | `dinoConfig+0x18` (int) | 0..97 | thumb | live (scheduler reads it) |
| `OPTIONS_GRAPH_QUALITY_INDICATION` | `0x519640` | `dinoConfig+0x34` (`LODQuality`) | 0 High, 1 Medium, 2 Low, 3 Auto; click sets `v = (v==0) ? 3 : v-1` | `&High` / `&Medium` / `&Low` / `&Auto` (loc keys) | stored only |
| `OPTIONS_GRAPH_TEXTURE_QUALITY_INDICATION` | `0x5196f0` | `dinoConfig+0x30` (`TextureQuality`) | same enum + same cycle | same 4 labels | stored only |
| `OPTIONS_SCREEN_SIZE_INDICATION` | `0x5197a0` | `dinoConfig+0x00/+0x04/+0x08` (12-byte mode) | next entry in the mode list, index `+0x40` = `(i+1) % n` | `"%dx%dx%d"` of the entry | stored only |
| `OPTIONS_GRAPH_MIXING_INDICATION` | `0x519890` | `dinoConfig+0x10` (`HardwareTNL`) | 0/1, toggled only if `page+0x64`; else forced 0 | `&Hardware` / `&Software` | stored only (INI hint `&Restart`) |
| `OPTIONS_MUSIC_MIXING_INDICATION` | `0x518ea0` | `dinoConfig+0x11` (`HardwareSound`) | 0/1, toggled only if `page+0x64`; else forced 0 | `&Hardware` / `&Software` | stored only |
| `OPTIONS_GRAPH_TEXTURE_DEPTH_INDICATION` | `0x518ff0` | `dinoConfig+0x0c` | toggles `0x10` <-> `0x20` | literal `32 bits` / `16 bits` | stored only |
| `OPTIONS_OK_BUTTON` | `0x518080` | page `+0x18 = 0`, click sound, `vtable[2] = 0x519560` | — | — | closes page |
| `OPTIONS_CANCEL_BUTTON` | `0x518080` (identical) | identical | — | — | closes page |

Cycle snippet (`FUN_00519640`, LOD quality; texture quality is identical with
`+0x30`):
```
iVar2 = config->lod_quality;      // +0x34
if (iVar2 == 1) config->lod_quality = 0;
else if (iVar2 == 2) config->lod_quality = 1;
else if (iVar2 == 3) config->lod_quality = 2;
else                 config->lod_quality = 3;
FUN_005199b0(page->graph_quality_indication, config->lod_quality);
```

Texture depth toggle (`0x00519023`):
```
EDX = config->texture_depth;
EDX = (EDX == 0x10) ? 0x20 : 0x10;   // see NEG/SBB/AND sequence
config->texture_depth = EDX;
setText(indication, EDX == 0x10 ? "16 bits" : "32 bits");
```
(`32 bits` @ `0x0066208c`, `16 bits` @ `0x00662094`; both go through the
localisation replacer `0x0040bd60` first.)

HardwareTNL / HardwareSound toggle:
```
if (page->has_hw_mixing) value = !value;   // else value = 0
setText(indication, value ? "&Hardware" : "&Software");
play "clicDefault" if OPTIONS_PAGE widget flag +0x38 bit 0 set
```
`page+0x64 = FUN_00508120()` (called from `FUN_0051c690` during init):
for each adapter it calls `IDirect3D8::GetDeviceCaps(adapter, HAL=1, &caps)`
and returns 1 if `caps.DevCaps & 0x00010000`
(`D3DDEVCAPS_TLVERTEXSYSTEMMEMORY`) is set on any adapter, else 0. When 0 the
page forces `HardwareTNL = 0` and clicking cannot turn it back on.

Screen size list (`FUN_0051c6b0`, called from init through `0x0051c690`):
the list is **not** an INI list. It is the video driver's static mode table
(`DAT_00674528 + 0x4e0`, stride `0x4f44`, entry pointer at `+0x4fe0`),
filtered by `width > 799 && height > 599`. Each kept entry produces the
string `"%dx%dx%d"`; the depth comes from
`(&DAT_006748b0)[FUN_00413020(fmt)]`, i.e. a format-code -> bits table
(16/32). The mode struct is `{int w, int h, int bpp}`. The list therefore
depends on the current video device, not on `Dino.ini`; entries below 800x600
are unavailable.

Init selects the entry matching the current config (`FUN_0051c9d0` compares
w, h, bpp). If none matches, index 0 is selected **and the config is
overwritten with the first list entry** (`0x005194d2`), so a bogus
`Resolution=` silently becomes the smallest supported mode.

## 6. Initial values on page entry (`0x005191c0`)

`0x005191c0` is the page's activation routine (vtable slot 1, at
`0x00642944`; the ctor itself only binds widgets). Every time the page is
shown it re-reads the live state:

```
FUN_0051c540()                       ; base activate (enable page input)
FUN_0051c690(page+0x44)              ; rebuild screen-mode list + caps byte

mov  ecx,[page+0x68]                 ; music hscroll
mov  edi,[ecx+0x74]                  ; its range
mov  ecx,[[page+0x38]+0x18]          ; audio manager
call 0x004fdab0                      ; get music volume (byte)
pos = (int)(vol * 0.01f * range + 0.5f)
FUN_005be6e0(pos, range)             ; set slider (fires event 0x36)
```
then the same for FX volume (`0x004fda50`) and game speed
(`config+0x18`), then:

* `HardwareTNL` label from `config+0x10` (and forced 0 if `page+0x64 == 0`);
* `HardwareSound` label from `config+0x11`;
* texture-depth label from `config+0x0c` (`0x10` -> "16 bits", else "32 bits");
* screen-size label + config fix-up (see above);
* texture-quality label from `config+0x30`, LOD-quality label from
  `config+0x34` (`FUN_005199b0`).

Background: `BACKGROUND_OPTIONS` (0x0066209c) looked up in
`MenuGraphicDB.ini` and pushed into the view object at `[page+0x38]+0x04`.

Because init fires event `0x36` on the sliders, the "changed" handlers run
during page entry too — they write the same value back to the audio manager
and play the `clicDefault` UI sound. (Audible on entering the page in
retail: one click per slider if the value differs; can also be a no-op.)

## 7. OK vs Cancel, persistence

* OK and Cancel share one handler (`0x00518080` — the same address is stored
  in both widgets' `+0x20`; `00518bd6` and `00518bf1`). There is no revert.
* `0x00518080` on click: `page+0x18 = 0`, play sound `clicDefault`, then
  `call [page vtable + 0x8]` = `FUN_00519560`.
* `FUN_00519560`: look up `OPTIONS_OK_BUTTON` (focus cleanup), broadcast
  event `0x2d` with arg 1 to every widget (`FUN_005c0480(1)`), free the
  screen-mode string vector (`page+0x48..0x4c`), set `page+0x5c = page+0x58`,
  then `FUN_0051c560()` = unregister input / leave the page
  (`FUN_005bbe60(0)`, `FUN_005bbe80(0)`, `FUN_005e4710()`).
* No apply step, no mode switch, no renderer rebuild, no file write in either
  button path.
* **Persistence is at application shutdown, not on OK.** The app's shutdown
  `FUN_00402250` copies audio state back into the config
  (`0x0040230c..0x0040233e`: `HardwareSound = !getHwFlag()`,
  `FXVolume = getFxVolume()`, `MusicVolume = getMusicVolume()`), then calls
  `FUN_00505bd0(config, "data/")` which rewrites `Data/Config/Dino.ini`
  (`[DINO]`, `[GRAPHIC]`, `[SOUND]` sections, one `key=value` line each) and
  exits. A killed process loses the changes; a normal quit keeps them.
* At start-up `FUN_005054f0` parses `dino.ini` into the config struct and
  `FUN_004039f0` applies `UseSound`/`HardwareSound`/`FXVolume`/`MusicVolume`
  to the audio manager. Resolution/quality/depth/GameSpeed are only stored in
  the struct at that point.

## 8. Keyboard / other input

Nothing is bound on this page beyond mouse. The page has no key handler: its
vtable (`0x00642940`: dtor `0x519190`, activate `0x5191c0`, close `0x519560`,
plus default stubs) contains no input or key entry, and no keyboard string or
key-map lookup appears in the page or in its handlers. ESC is handled by the
menu view's global key map (not traced here).

## 9. Port contract

1. Settings model = the 12 fields of the table in §3, loaded from
   `Data/Config/Dino.ini` with the defaults listed there, written back on
   quit (and only on quit).
2. Slider: `pos` in `[0, HS_RANGE-1]` (31 for retail files), thumb x =
   `(width * pos) / (range-1)` (see `FUN_005be7e0`), value =
   `floor(pos*100/range + 0.5)`. Do not "fix" the `/range`; the retail
   maximum is 97 and `Dino.ini` stores 97. Init maps
   `pos = floor(value*0.01*range + 0.5)`.
3. Slider input: arrows +-1, track click +-3, thumb drag absolute,
   wheel -delta/120 (wheel up decreases). Clamp to `[0, range-1]` after
   every step. Apply immediately.
4. Volume targets are the audio manager (0..100 byte each); GameSpeed is
   `config.game_speed` (percent, higher = faster; consumed by the scheduler).
5. Quality buttons cycle `HIGH(0) -> AUTO(3) -> LOW(2) -> MEDIUM(1) -> HIGH`;
   labels are localised keys `&High`, `&Medium`, `&Low`, `&Auto`.
6. Texture depth toggles 16/32 bits, label literal `"16 bits"`/`"32 bits"`.
7. HardwareTNL / HardwareSound toggle 0/1 with labels `&Hardware`/`&Software`;
   HardwareTNL is forced to 0 when the device caps test fails (port: treat as
   "always allowed" unless a T&L check is modelled).
8. Screen size cycles through the host display's modes >= 800x600, label
   `"%dx%dx%d"`; snap the config to entry 0 if the stored mode is invalid.
9. OK and Cancel are the same action: leave the page, discarding nothing and
   reverting nothing. Graphics changes therefore appear only after a
   restart/new render target; the INI hint `&Restart` on the mixing row states
   this.

## 10. Uncertainty / open

* **Where resolution / TextureDepth / TextureQuality / LODQuality /
  HardwareTNL are consumed** (i.e. the exact moment they take effect) was not
  traced. The page never calls a mode-set or renderer-reconfigure function,
  and the config is only read at start-up (`FUN_005054f0`,
  `FUN_004039f0`) as far as found. "Needs restart (or at least re-creating
  the 3D view)" is inferred from that plus the `&Restart` hint, not proven by
  a runtime test.
* The `caps` field tested in `FUN_00508120` is `Caps+0x1C & 0x10000`, which is
  `D3DDEVCAPS_TLVERTEXSYSTEMMEMORY` in the D3DCAPS8 layout. Why that flag
  (rather than `HWTRANSFORMANDLIGHT`) gates the mixing toggle is not obvious;
  it is reported as measured.
* The exact `page+0x38` object type (gsc display context) was not named from
  RTTI/asserts; only its `+0x04/+0x10/+0x18` members are proven.
* `page+0x18` (cleared on OK) was not identified; it is not read by any page
  handler.
* No runtime probe was run for this note; everything above is static code.

## Evidence index

| claim | evidence |
|---|---|
| page = menu index 5, ctor binds `_INDICATION` widgets | `FUN_00518830` (`0x00518830`), string xrefs of the 12 names |
| page index/registration | `FUN_0051c510(5,"OPTIONS",...)` @ `0x00518842` |
| slider value formula | `0x00518c23..0x00518c5b` (music), `0x00518ccd..0x00518d06` (sounds), `0x00518dc3..0x00518df6` (speed) |
| init formula | `0x00519256..0x00519304` |
| HS_POSITION/HS_RANGE parse | `0x005bec14..0x005bec62`; constants 0 and 1 |
| slider clamp | `FUN_005be6e0` (`0x005be6e0`) |
| slider mouse logic | `FUN_005be710` (arrows/track), `FUN_005c7740` (drag/wheel), `FUN_005c7820` (event mirror) |
| quality enum + labels | `FUN_005199b0`, strings `0x6620b0..0x6620c8`; loader compare `0x00505885..0x00505940` |
| config layout + defaults | `FUN_005054f0` (load), `FUN_00505bd0` (save), `Data/Config/Dino.ini` |
| audio apply at start | `FUN_004039f0` (`0x004039f0`) |
| save at shutdown | `FUN_00402250` (`0x0040230c..0x004023ec`) -> `FUN_00505bd0` |
| OK == Cancel | ctor `0x00518bd6`/`0x00518bf1`, handler `0x00518080` |
| mode list from driver | `FUN_0051c6b0` (`0x0051c6b0`), `"%dx%dx%d"` @ `0x662434`, filter `w>799 && h>599` |
| caps gate | `FUN_00508120`, `0x00508162` test `[ESP+0x2c] & 0x10000` |

