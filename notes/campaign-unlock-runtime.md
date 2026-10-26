# Runtime: campaign unlock + the `0x56c0c0` clip-pair getters

Status: **campaign unlock solved; the `0x56c0c0` lead is now ruled out for
dinosaur actions.** Complements `notes/action-clip-lead.md`.

## Campaign progression is `run/save/dino.dat`

`dino.dat` is a 4-byte little-endian dword, not a save. The campaign database
reader `FUN_005612d0` (`snoCampaignDataBase.cpp`) builds the path
`<data>/dino.dat`, opens it, and reads a `u32` into `campaign_db+0x1c`; the
writer `FUN_005617a0` writes the same field back. A missing/short file
defaults to **1**.

- Fresh test profile: `01 00 00 00` → only the first campaign is available.
- Setting `0F 00 00 00` unlocked the first four campaigns; **Cpg04 ("Panic in
  the Savanna") then starts** through the normal
  Campaigns → row 3 → Start gear → mission row → Start flow.
- Evidence screenshot: `notes/campaign-park.png` (Cpg04 briefing, budget
  17,000, 20 dinosaurs, 4 briefing pages).
- The file is read at startup, so edit it before launching. `run/` is the
  test copy; `game/` and `original/` are untouched. The backup made during
  this experiment is `run/save/dino.dat.bak` (value 1); the test copy was
  restored to 1 after the run.

The campaign UI coordinates used by the harness (from `MenuInterface.Ini`):

```text
CAMPAIGNS main button         (395,189)
CAMPAIGN_LIST row i           (404, 183 + 34*i)
CAMPAIGN_START gear           (365,530)
CAMPAIGN_INFO mission row i   (259, 150 + 17*i)
CAMPAIGN_INFO Start           (521,449)
```

The mission briefing is modal (`1/4` in Cpg04); the park simulation does not
tick until it is dismissed with the green check at about `(322,402)`.

## `0x56c0c0` is not the dinosaur action→clip map

A live Cpg04 run logged `FUN_0056c0c0` (`asItemBehavior::RegisterForAction`)
with two real vtable bases, `0x6430AC` and `0x6430E4`. Their
`+0x20` / `+0x24` slots are:

| vtable | slot +0x20 | slot +0x24 |
| --- | --- | --- |
| `0x6430AC` | `0x56F290` → `MOV AX,0; RET` | `0x56F290` → `0` |
| `0x6430E4` | `0x56FE70` → `AX = [[[this+0x38]+0x28]+0x28]` (`u16`) | `0x56FE80` → `AX = 1` |

`FUN_0056c0c0` stores those two values at `[actor+0x28]+0x314/+0x316`. They are
**item/show animation state ids** (0/0 for one class, a state id/1 for the
other), not an action id → clip table. This matches the call sites: the
function is driven by `asItemBehavior` / show records, and its `param_3`
descriptor is the `.bfi`-style item data.

Consequence: the dinosaur action→clip mapping does **not** come from
`0x56c0c0`'s two `u16`s. The next trace should watch the `anmActor`'s current
clip when a known `saDinosaur` action (Eat/Sleep) starts, or find the
animator method that consumes `actor+0x2c0`/the `.bai` action element.

## Harness

`scripts/x32dbg/start-campaign.ps1` starts a chosen campaign/mission through
the retail menu and logs:

- `0x57dc60` stFence ctor (rectangle + cell-size pointer),
- `0x571540` `saDinosaur::Update` (`+0x2c0` action id, `+0x1e0` cost field),
- `0x56c0c0` clip-pair registration (vtable + args),
- `0x57eb50` `stFence::CanAddActor` (occupant, cost index, capacity/used).

The trace is timing-sensitive under x32dbg: install breakpoints only after the
mission page is reached, and dismiss the briefing before expecting
`saDinosaur::Update` hits. The run also overwrites `run/save/auto/auto.sav`;
restore it from `run/save/auto/slot001.sav` if a save test follows.

## Evidence

- `notes/campaign-park.png`, `notes/campaign-info.png`,
  `notes/campaign-selected.png`, `notes/campaign-running.png`.
- `notes/x32dbg-campaign.out.log` — `CLIPPAIR` lines with the real vtables;
  no `DINO`/`FENCE`/`CANADD` hits before the briefing was dismissed.
- Ghidra: `0x5612d0` (dino.dat reader), `0x5617a0` (writer), `0x56c0c0`,
  vtables `0x6430AC` / `0x6430E4`, getters `0x56F290` / `0x56FE70` /
  `0x56FE80`.
