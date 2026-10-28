# Retail Load Game `.sav` -> island (menu load path)

Status: **solved.** The main-menu Load Game panel does not ask for an island:
the save header stores a **mode** and an **island index** (or campaign id),
and retail derives the `.lnd` from it. Evidence: disassembly plus all four
shipped samples.

## Click -> load

`FUN_00519ba0` (`LOAD_GAME_PAGE` ctor) binds the widgets:

| Widget | Page field | Handler |
| --- | --- | --- |
| `LOAD_GAME_NAMES_LIST` | `[page+0x74]` | `0x00519e00` (selection), `0x00519eb0` (paint) |
| `LOAD_LOAD_BUTTON` | `[page+0x70]` | `0x00519d70` |
| `LOAD_CANCEL_BUTTON` | — | `0x00518080` |

`0x00519d70` requires `event==0` and a selected row (`[panel+0x6c] >= 0`),
plays `clicDefault`, then calls the panel virtual `[EDI+8]`. The app
dispatcher `0x00402460` takes the `page+0x24` command:

```text
004025eb  MOV EAX,[EBP+0x48]   ; page
004025ee  MOV ECX,[EAX+0x24]   ; command
004025f1  CMP ECX,0x2
004025f6  JNZ 0x004025ff
004025f8  CALL 0x00402f10      ; .sav slot load
004025ff  CALL 0x00403e10      ; new island / campaign start
```

`FUN_00402f10` builds `"save/" + entry` (`0x64b614`) and opens it with
`FUN_00417d10`. `FUN_00402bc0` is the same builder with the autosave branch
(`"save/auto/auto.sav"`, `0x64b61c`).

## Header fields

`FUN_00402f10` reads version/name, then `FUN_00506390` selects the block:

```text
read u32 -> [req+0x40]          ; mode
read u32 -> [req+0x44]          ; unused in these paths
if (mode == 1) read u32 -> [req+0x24]   ; campaign id; [req+0x20] = 999
else           read u32 -> [req+0x20]   ; island index; [req+0x24] = 999
```

Island resolution `0x004030a1`-`0x004030d5`:

```text
0040309e  MOV ECX,[EAX+0x40]    ; mode
004030a3  JZ  0x004030c1        ; mode 0 -> island list
004030a5  DEC ECX / JNZ default ; mode 1 -> campaign list
004030c1  MOV ECX,[EAX+0x20]    ; island index
004030c4  MOV EDX,[EAX+0x10]    ; descriptor vector
004030c7  MOV EAX,[EDX+ECX*4]   ; descriptor
004030ce  ADD EAX,0xc           ; -> "islNN"
```

`FUN_00506530` builds the descriptor vector from
`Data/Design/Landscape/IslandList.ini` (one `islNN` per line, string at
descriptor `+0x0c`). `FUN_0051dad0` then derives the landscape path from
characters 3-4 of `islNN`:

```text
Data\Design\Landscape\islNN\islNN.lnd
```

(Mode 1 resolves the campaign id through the campaign database:
`[[[app+0x50]+0x24]+idx*4]+0x54` at `0x004030a5`.)

## Exact layout

```text
0x00  u32  version (53)
0x04  u32  slot/index (0)
0x08  u32  name byte length (UTF-16LE, NUL included)
0x0c  ...  UTF-16LE name
p = 0x0c + name_len
p+0x00  u32  mode            0 = free game/island, 1 = campaign
p+0x04  u32  unused (0)      not read by the paths examined
p+0x08  u32  island index    mode 0: index into IslandList.ini (0 = isl01)
                             mode 1: campaign id
p+0x0c  ...  world manager starts here (the old "12 reserved bytes")
```

Samples (read-only check with Python `struct`):

| file | `name_len` | `p` | mode | p+4 | p+8 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `slot001.sav` | 0x14 | 0x20 | 0 | 0 | 0 -> isl01 |
| `slot002.sav` | 0x0a | 0x16 | 0 | 0 | 0 -> isl01 |
| `slot003.sav` | 0x14 | 0x20 | 0 | 0 | 0 -> isl01 |
| `auto/auto.sav` | 0x02 | 0x0e | 0 | 0 | 0 -> isl01 |

The world block starting at `p+0x0c` matches `docs/formats/sav.md`
(`0x18 + name_len`); the port's `read_sav_header` simply stops after the
name and the world parser seeks past the 12 bytes.

## Port implication

- `SavHeader` should expose `mode` and `island_index`/`campaign_id` instead
  of treating the 12 bytes as reserved (no layout change).
- For a manual/free-game save (`mode 0`) the Load Game menu can select
  `IslandList.ini[island_index]` and call `load_island` before the existing
  `.sav` world/simulation import. **No island choice dialog is needed.**
- For a campaign autosave (`mode 1`) the id resolves through
  `CampaignList.ini` and should use the campaign start path, not the
  free-game island list.
- The three shipped manual saves are all isl01, so a free-game load can be
  tested against them as-is.

## Confidence

- **High:** header order/offsets (`FUN_00402f10` + `FUN_00506390` + writer
  `FUN_00403340`/`FUN_005063f0`), island index -> `IslandList.ini` ->
  `Data\Design\Landscape\islNN\islNN.lnd`, sample values.
- **Medium:** the static link from `0x00519d70`'s `CALL [EDI+8]` to the
  `0x00402460` dispatcher (the command value 2 was not traced to the widget
  instruction-by-instruction), though `FUN_00402f10` is the only branch that
  parses a `.sav` header.
- **Unknown:** the `u32` at `0x04` and `p+4`; both are zero in every sample
  and not read by the paths examined.
