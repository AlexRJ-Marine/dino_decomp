# HUD `BUDGET_AMOUNT` source (ParkInterface)

Status: solved statically (Ghidra `DinoIsland.exe-ebea17`). No runtime check yet.

## Answer

- The always-on park HUD text is refreshed by **`FUN_00526010`** (ParkInterface
  HUD refresh), called unconditionally from the scheduler update
  `FUN_00524630` at `0x524f04` and `0x525088` (thunk `0x51d1b0`).
- `BUDGET_AMOUNT` shows `spPark->economy->budget`, i.e.

  ```
  cash = *(int *)( *(int *)( *(int *)( *(int *)(this + 0xd0) + 0x14 ) + 0x120 ) + 0x08 )
                  \__________________ ParkView manager ______/   spPark   economy
  ```

  formatted with `"%d"` (string at `0x64c6cc`).
- `spPark+0x120` is the economy object created by `FUN_00585150` in the park
  ctor `FUN_005874a0` (`0x5879ab`); `+0x08` is the budget documented in
  `docs/decomp/economy.md`.
- The widget text setter is **`FUN_005bb940`**:
  `(**(code**)(*widget + 0x4c))(0x1b, wide_text, 0, 0, 0)`.

## Widget slots (correction of the earlier `+0xbc` assumption)

`FUN_005234e0` resolves names in order and stores the *previous* resolve just
before the next name push. Disassembly shows the true mapping:

| Field | Name | Store site |
| --- | --- | --- |
| `+0xac` | GAUGE_PANEL | `0x523e5b` |
| `+0xb0` | RIGHT_PANEL | (same block) |
| `+0xb4` | NAVIGATION_PANEL | `0x523e...` |
| `+0xb8` | TIME_PANEL | `0x523e...` |
| `+0xbc` | **DIALOG_BACKGROUND** | `0x523f2b` |
| `+0x1cc` | **BUDGET_AMOUNT** | `0x523f5f` |
| `+0x1d0` | VISITOR_AMOUNT | `0x523f93` |
| `+0x1d4` | DINO_AMOUNT | `0x523fc7` |
| `+0x1d8` | SHOW_AMOUNT | `0x523ffb` |
| `+0x1dc` | PADDOCK_AMOUNT | `0x524022` |
| `+0x1e0` | PARK_CURRENT_TIME | `0x524063` |
| `+0x1e4` | PARK_CURRENT_DAY | (tail) |
| — | OPTIONS_MENU_EXIT_ICON | used directly, not stored |

`+0xbc` is the dialog background, so searching xrefs to `+0xbc` for the HUD
update finds nothing.

## Update body (`FUN_00526010`)

Every value follows the same 5-step pattern:
`FUN_00404530(&DAT_006743c8)` (clear scratch mcString) -> `FUN_005972ba(buf, fmt,
value)` (sprintf) -> `FUN_00425540(buf)` -> `FUN_0040bd60(&DAT_006743c8,
this+0x9c)` (narrow -> wide) -> `MOV ECX,[this+widget]; CALL 0x005bb940` (set
text). `this+0x90` is the narrow scratch, `this+0x9c` the wide scratch.

```c
void ParkInterface_UpdateHud(ParkInterface *this) {
    int *park = *(int **)( *(int *)(this + 0xd0) + 0x14 );   // spPark

    set_text(this->w_budget,  "%d",      econ(park)->budget);          // 0x526079
    set_text(this->w_visitor, "%u", (park->vis_end  - park->vis_begin ) >> 2); // 0x80/0x84
    set_text(this->w_dino,    "%u", (park->dino_end - park->dino_begin) >> 2); // 0x68/0x6c
    set_text(this->w_show,    "%u", (park->show_end - park->show_begin) >> 2); // 0x40/0x44
    set_text(this->w_paddock, "%u", (park->pad_end  - park->pad_begin ) >> 2); // 0xa0/0xa4
    clock = FUN_0058c350();      // hour/min/day
    set_text(this->w_time, "%.2d:%.2dAM/PM", ...);
    set_text(this->w_day,  "%d", clock.day + 1);
}
```

Widget set sites: `0x526083` (budget), `0x5260d5` (visitor), `0x526121`
(dino), `0x52619b` (show), `0x526282` (paddock), `0x526340` (time),
`0x526417` (day). Budget value read: `0x526041-0x526051`
(`[reg+0x14]` -> `[reg+0x120]` -> `[reg+0x08]`, PUSH `0x64c6cc` = `"%d"`).

`VISITOR_AMOUNT` and `DINO_AMOUNT` share the same helper pattern but not a
helper function; they are just more iterations of this loop. `FUN_00526010`
also drives the counts/time, so it is the whole top HUD refresh.

## Relation to the save (port contract)

- The save park block `+0x30` **is** the same field: the economy serializer
  `FUN_00585780` writes `+0x08` first and the reader `FUN_00585240` reads it
  back into the live object. The save reader is called on the **live** economy:
  `0x58b1b9  MOV ECX,[EBP+0x120]; CALL 0x585240` inside `FUN_0058aed0`
  (park loader), with `EBP` = `spPark`. So the HUD cash and the save park block
  `+0x30` are the *same* budget, not two different budgets.
- `26000` is not a static config value: it is absent from
  `Data/Design/Scenario/*.ini` (`FreeGame.ini` budgets are 75000/50000/25000,
  `Cpg04.ini` 20000) and from `Cpg04_Save.wld`. It is therefore a *live*
  economy value, not a different field.
- The economy restore on load is **conditional**:
  `0x58b1b0  TEST BL,BL; JZ read; CMP EDI,0x17; JA skip` -- when the loader
  flag `BL` is non-zero **and** the save version is > 0x17 (retail saves are
  version 46), the `FUN_00585240` economy read is skipped and the live budget
  keeps whatever it already had. Candidate explanation for the 26000 HUD after
  loading a 12345 save; the byte-level meaning of `BL` is **unproven**
  (it is the low byte of the loader's first argument, set two calls above).
- The absolute budget setters that can leave a pre-load value in place:
  `FUN_005874a0` (`0x5879b1`, park ctor, `set_budget(0, 1.0)`),
  `FUN_0055d8e0` (scenario context: `set_budget(scenario+0x7c,
  *(float*)(scenario+0x80))`), called from `FUN_0051da20` / `FUN_0051da70`
  (new game / free game), and `FUN_00561bb0` (`0x5620fa`, `budget += delta`).

## Port

- HUD cash = `Economy::budget` (`economy + 0x08`, int, printed with `%d`); the
  economy object is `spPark + 0x120`. Do not add a separate "HUD budget".
- Do not treat the save park block `+0x30` as a different budget: it is the
  serialized image of the same `+0x08`, first dword after the 0x30-byte block
  header.
- The budget panel (`0x510430`) is a different widget set (yesterday/cumulative
  counters); it does not feed the top HUD.

## Evidence

- `0x523f21`/`0x523f5f` -- `PUSH 0x66299c` (`BUDGET_AMOUNT`) then
  `MOV [ESI+0x1cc],EAX`; `0x523f2b` stores `+0xbc` for `DIALOG_BACKGROUND`.
- `0x526041-0x526083` -- economy read, `%d`, `ECX=[ESI+0x1cc]`,
  `CALL 0x005bb940`.
- `0x526094-0x526121`, `0x526160-0x52619b`, `0x5261da-0x526282` -- visitor,
  dino, show, paddock counts and their widget slots.
- `0x524f04`, `0x525088` -- `FUN_00524630` -> `FUN_00526010`.
- `0x58b1b0-0x58b1c2` -- conditional economy read in the park loader.
- `0x5879ab`/`0x5879b1` -- park ctor stores economy at `+0x120`, budget 0.
- `0x5620d9-0x5620fa` -- `FUN_00561bb0` budget delta.
- `0x55d904-0x55d90b` -- `FUN_0055d8e0` scenario absolute budget.

Uncertainty: the `BL` loader flag and the exact origin of 26000 need one
runtime check (breakpoint at `0x526079`, read `[[[esp+..]+0x120]+8]` / watch
`0x58585bd0` during LOAD GAME).

## 9. Correction (2026-10-27, follow-up recon)

The "`BL` = loader flag, version > 0x17 skips the budget" reading was too
broad. `notes/hud-budget-restore-flag-recon.md` proved `BL` is the park
reader's **third call-site argument**: `0` for the `.sav` LOAD path
(`FUN_00402f10`) and the save/autosave wrapper, `1` only for the campaign
`.wld` loader (`FUN_00403e10`), whose stream has no park block. A manual or
autosave `.sav` LOAD therefore sets `BL = 0` and **restores the budget**
(`FUN_00585240` also self-gates on the flag). The port restoring
`sav_simulation.budget` matches retail for `.sav` LOAD. The 26000 after the
earlier test is not explained by this guard; see the follow-up note §8 for the
remaining candidates (a later scenario/economy writer or a campaign `.wld`
start) and the one-line runtime watch at `0x58b1b0`.
