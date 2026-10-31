# Dino stats / economy — gap audit (2026-10-28)

Status: **the info-panel formulas, the live dino attribute/need/health model, the
food-source level economy and the dino sell value are decoded from the binary.**
Remaining uncertainty is listed per section and in §8 (Open). Binary
`/DinoIsland.exe-ebea17`, all addresses VA, image base 0x00400000. No `src/`
change; read-only audit. Scratch scripts live in `notes/harness/` (gitignored).

**Lane collision:** another lane committed the fence sell flow
(`notes/NOW.md` "ECONOMY: fence sell flow …") while this audit ran. I edited
nothing in `src/`, `docs/` or `tools/`; this note only adds a new file.

Already in the tree, not redone here:

| Topic | Where it is already proven |
| --- | --- |
| `0x57e720` four-stat shape (count / fill% / two buckets) | `notes/fence-panel-sell-recon.md` §A3 |
| Panel widget map, `0x5269e0` fill, paddock list `0x511890` | `notes/fence-panel-sell-recon.md` §A2, `notes/fence-construction-recon.md` ("scroll model", `FENCE_LIST_*`) |
| Fence sell value + refund + non-empty gate | `notes/fence-panel-sell-recon.md` §B |
| Budget ledger fields/setters/settlement rows | `notes/park-economy-recon.md` §1-§7, `docs/decomp/economy.md` |
| Store catalog, buy price, budget gate, six-row panel | `notes/store-panel-recon.md`, `notes/market-recon.md` |
| Phenotype attribute formulas + `TranslatorConstants.ini` | `notes/phenotype-attributes-runtime.md`, `notes/translator-recon.md` |
| Needs/thresholds/action loop | `notes/dinosaur-action-loop.md` (partly superseded here, §2.4) |
| Nutritionist hourly refill job | `notes/employee-ai-recon.md` §7 |

---

# 0. Short answers

| Question | Answer |
| --- | --- |
| Dino count in a paddock | `(*(int*)(stFence+0x9c) - *(int*)(stFence+0x98)) >> 2` — a `vector<dinosaur*>` (4-byte elements) written by `stFence::AddActor 0x57e8a0` / `RemoveActor 0x57e980`. |
| Occupation % | `(int)(*(float*)(stFence+0x78) * 100.0f)`, no clamp; `+0x78 = used/capacity`, `+0x7c` capacity, cost table 8/16/32/64 by `dino+0x1e0`. |
| Carnivore/herbivore food sufficiency | `(int)(Σ food_value * 100 / Σ demand)` per diet; **empty bucket = 100 %**; food_value = `slots_left * bsi.quantity * bsi.nutrition`, demand = `scale * dino.food_consumption`; `scale = 39616 / day_duration = 39616/3 = 13205.33` (§1.6). |
| Dino attribute gauges (live) | 11 values, sources in §2.2 (perception `+0x280`, beauty `+0x90`, attack `+0x29c/5`, aggression `+0xbc`, armor `+0x270`, speed `ftol(+0x268*20)`, agility `+0x98`, intelligence `+0x284`, courage `+0x2a4`, stamina `+0xc0`, originality `100-+0x1e4`), maxima from `AttributeRange.ini`. |
| Growth / aging | **No growth, no aging, no lifespan in this build** (negative result, §3.1). |
| Health | `+0x16` (int16) / max `+0x1d4` (int16) = `sqrt(weight) + 0.5*cbrt(L*W*H)`; regen `+0x1d6 = maxHP*stamina/1000 + 1` per 5 s; starvation damage `(int)(deficit*0.1)` per 10 s; state `+0x2bc` 0/1/2/4 at 100/67/34/0 %. |
| Food consumption | `food_gauge(+0x25c) -= 10 * food_consumption` every 10 s; `food_consumption(+0x6c) = weight*0.1*0.01`; FOOD need `+0x244 = ftol(food*99/max_food)`. |
| Food source state | `saFoodSource`: `+0x8` amount, `+0x10` level (0 full, 1 = 2/3, 2 = 1/3, 3 = **empty**); refill charges the `.bsi` **price** to the FOOD bucket (§4.3). |
| Dino sell value | `dino+0x1ec` = phenotype `+0x19c` = Σ over 18 genes of `price[rarity(A)] + price[rarity(B)]`; the three prices are `RareAlleleMarketPrice 500 / UncommonAlleleMarketPrice 100 / CommonAlleleMarketPrice 50`; refund = `ftol(value * SellBackFactor)` (§5.2). |
| Buy | price from the catalog triplet, budget gate `0x52bed0`, charge `0x585cc0` = DINOACHAT (`+0x9c/+0xa0`) at `0x535eeb` (§5.1). |

---

# 1. Info-panel stats — `stFence::GetStats` (`0x57e720`)

## 1.1 The function, verified instruction by instruction

`void __thiscall GetStats(stFence* this, float scale, int out[4])`, `0x57e720..0x57e89c`.
Retail uses an x87 accumulator chain; I re-derived the stack to confirm which
sum lands in which output (the previous note's conclusion is correct):

| out | formula | asm anchor |
| --- | --- | --- |
| `out[0]` | `(*(int*)(+0x9c) - *(int*)(+0x98)) >> 2` | `0x57e729..0x57e73e` |
| `out[1]` | `__ftol(*(float*)(+0x78) * 100.0f)` | `0x57e740..0x57e754`, `0x6256b8 = 100.0f` |
| `out[2]` | `carn_demand == 0 ? 100 : (int)(carn_food*100/carn_demand)` | guard `0x57e832..0x57e83c`, divide `0x57e83e..0x57e84d` |
| `out[3]` | `herb_demand == 0 ? 100 : (int)(herb_food*100/herb_demand)` | guard `0x57e85d..0x57e868`, divide `0x57e86a..0x57e87b` |

Dino loop (`0x57e763..0x57e798`):

```c
sumCarn = 0; sumHerb = 0;                    // x87 slots A (diet==1) and B (diet!=1)
for (d = begin(+0x98); d != end(+0x9c); ++d) {
    v = scale * *(float*)(*d + 0x6c);        // food consumption
    if (*(int*)(*d + 0x200) == 1) sumCarn += v;   // FADDP      -> slot A
    else                          sumHerb += v;   // FADDP ST2  -> slot B
}
```

Slot A is the one the `out[2]` divide consumes: `FSTP [esp+0x24]` pops the
herbivore *food* sum, then `FLD ST1` / `FCOMP 0` guards slot A, `FMUL 100` /
`FDIV ST0,ST1` divides the carnivore food by slot A, and `__ftol` pops — leaving
slot B on top for the `out[3]` guard at `0x57e85d`.

Feature loop (`0x57e7a6..0x57e822`):

```c
herbFood = 0; carnFood = 0;                  // ST0 and ST1 (pushed as two zeros)
for (f = begin(+0xd8); f != end(+0xdc); ++f) {
    bsi = *(int*)(*f + 0xb0);                // .bsi record
    if (!bsi) continue;
    n = 0;
    for (i = 0; i < 10; ++i)                 // 10 slots, stride 8, at feature+0x60
        if (*(unsigned char*)(*f + 0x60 + i*8) == 0) ++n;
    if (*(int*)(bsi + 0x0c) < 5) herbFood += n * *(float*)(bsi+0x04) * *(float*)(bsi+0x00);
    else                        carnFood += n * *(float*)(bsi+0x04) * *(float*)(bsi+0x00);
}
```

Cross-checks:
* the diet test is `CMP EBP,1` with `EDI=1` (`0x57e78a`), the kind test is
  `CMP dword [ECX+0xc],5; JGE` (`0x57e7ee`) — `>= 5` is the carnivorous `.bsi`
  kind range (`docs/formats/bsi.md`).
* `__ftol` at `0x5982d4` is a *pop*, which is what puts the herbivore demand on
  top for the `out[3]` divide — this is where an earlier reading could go wrong;
  the resulting order (out[2] = carn, out[3] = herb) matches the widget stores
  (`+0x98` ← `0x526c7e` CARNI, `+0x9c` ← `0x526d4a` HERBI).

## 1.2 Inputs and where each comes from

| Input | Source | Proven by |
| --- | --- | --- |
| `stFence+0x98..+0x9c` | `vector<dinosaur*>`, 4-byte elements | `0x57e8a0` push, `0x57e980` erase, sell gate `0x533312` |
| `stFence+0x78` | `used / capacity` (float, can exceed 1 in principle; not clamped) | `0x57e8a0`/`0x57e980`: `[0x80] ± cost[dino+0x1e0]`, `[0x78] = [0x80]/[0x7c]` |
| `stFence+0x7c` | capacity = rect cells | `0x57dc60` ctor |
| `stFence+0x80` | used cost units | same |
| `stFence+0x84..+0x90` | 8/16/32/64 cost table | ctor, `0x57e980` |
| `dino+0x1e0` | size class = def+0x190 (0..3) | `0x5710c0` cache map, `docs/decomp` size-class note |
| `dino+0x6c` | **food consumption** = def+0x1c = `weight * FoodConsumptionRate(0.1) * 0.01` | `0x617e7f..0x617e8d` (§3.4) |
| `dino+0x200` | diet: 0 = herbivore, 1 = carnivore = def+0x1b0 | `0x617e90..0x617ea7` (kind default 0/5), `dinosaur-action-loop.md` |
| `stFence+0xd8..+0xdc` | `vector<feature*>` = the paddock's LunchBox dishes (cap 15, `0x527150`) | `0x57e630`, `0x527030` |
| `feature+0xb0` | the `.bsi` record | `0x57e7cf` |
| `feature+0x60 + i*8` | 10 portion slots; a **zero first byte counts as one available portion** | `0x57e7d9..0x57e7ec` (`CMP byte [EAX],BL` with `BL=0`, stride 8, count 10) |
| `.bsi+0x00` / `+0x04` / `+0x0c` | nutrition / quantity / kind | `0x57e7f8..0x57e81a` |

## 1.3 Two consumers, one scale

* Fence info panel `0x5269e0` (snapshot at open only): `0x57e720(stFence, scale, out)`,
  then `%u` / `%u%%` into `+0x90/+0x94/+0x98/+0x9c`.
* Paddock list `OPEN_PADDOCK_LIST_PANEL` row fill inside `0x511890`
  (vtable `+0xc`, per frame): calls `0x57e720` at `0x511984` with the same scale
  and writes `FENCE_LIST_QUANTITY_SLOT` … `FENCE_LIST_CARNIQUANTITY_SLOT`
  (`fence-construction-recon.md`). **Warning:** that note maps `out[2]` to
  `FENCE_LIST_HERBIQUANTITY_SLOT` and `out[3]` to `…CARNI…`, i.e. the retail
  *list* panel looks label-swapped relative to the info panel. Verify against a
  capture before porting the list labels (§8.3).

## 1.4 Edge cases (all proven in the disassembly)

* `carn_demand == 0` → `out[2] = 100`; `herb_demand == 0` → `out[3] = 100`
  (an all-herbivore paddock shows 100 % carnivore sufficiency).
* `n == 0` (all portions eaten) → that bucket contributes 0, so the result is
  `0 %`, not 100.
* `feature+0xb0 == 0` → the feature is skipped entirely.
* No clamp: values above 100 % are possible (`out[2] = 212` for one 2000 kg
  carnivore + one Cow dish with the scale of §1.6). The UI appends `%`.
* `out[1]` uses `__ftol`, i.e. truncation toward zero, and no clamp at 100.
* `out[0]` is a `>>2` of the vector byte distance — any element that is not a
  4-byte pointer would corrupt it; the vector is `vector<dinosaur*>`.

## 1.5 What the count really is

`drsFence+0x38` and `+0x4c` both alias the `stFence` (`0x5e8820`); the dino
vector is filled by `stFence::AddActor` from the fence-insertion path
(`0x535f56`) and from the scenario loader (`0x55df50`), so the count is
"dinosaurs currently inside this paddock", independent of the visual actors.

## 1.6 The `scale` argument — resolved statically (was Open)

```asm
00526a42 MOV EDX,[EBP+0x44]   ; the construction tool
00526a45 PUSH 0x471ab000      ; 39616.0f
00526a4a MOV EAX,[EDX+0x10]
00526a4d MOV ECX,[EAX+0x14]   ; spPark
00526a50 MOV EDX,[ECX+0xc4]   ; spPark+0xc4 = spScheduleManager pair
00526a56 MOV ECX,[EDX+0x4]    ; 2nd half = FUN_0058c1c0 object
00526a59 CALL 0x0058c460      ; x * [this+0x14]
```

`FUN_0058c1c0(obj, duration, id)` (`0x58c1c0`) stores

```text
+0x00 id (=9)      +0x04 duration      +0x08 duration/60     +0x0c duration/3600
+0x14 1/duration   +0x18 60/duration   +0x1c 3600/duration
```

so `scale = 39616 * (1/duration)`. The duration comes from

```text
FUN_00586200(this, simuMgr, cfg+0x110, cfg+0x1a0, cfg+0x1a8)      0x586200
  -> FUN_005874a0(spPark, cfg+0x110, cfg+0x1a0, cfg+0x1a8)        0x5874a0
     -> FUN_00594b80(param_4 = cfg+0x1a8, param_3 = cfg+0x1a0)    0x587a6d
        -> FUN_0058c1c0(*(float*)(cfg+0x1a8), 9)
```

`cfg` is the game-data object (`0x53d4d0` loader); `cfg+0x1a8` is the first
float of the `GeneralParams.bgd` 4x4 grid written by `0x542f80`
(`+0x1a0` = 40.0, `+0x1a4` = 10, then 16 floats). The retail file value is
**3.0** (verified: `Data/Design/generalparams/GeneralParams.bgd` tail =
`… 60.0, 3.0, 3.0, 3.0, 3.0, 2.0 …`).

```text
scale = 39616.0 / 3.0 = 13205.333
```

Confidence: **high for the chain** (all four call sites disassembled, argument
order re-checked from the raw `PUSH` order at `0x587a65..0x587a6d`), **medium for
"3.0 is a day length"** — the field is unnamed and only `GeneralParams.bgd`
supplies it. Runtime probe to close it: read
`[[[[tool+0x10]+0x14]+0xc4]+4]+0x14` on a live park (expect `0.3333333`) and
`[spPark+0xc4]`'s `+0x04` object's `+0x04` (expect `3.0`).

## 1.7 Worked example (Cpg04 paddock, one dish of each kind)

Paddock `(452,312)-(467,343)`, capacity 32, 1 Allosaurus (2000 kg, carnivore)
and 1 Triceratops-class herbivore of 3000 kg:

```text
scale        = 13205.33
carn demand  = 13205.33 * (2000*0.001) = 26410.7
herb demand  = 13205.33 * (3000*0.001) = 39616.0
Cow dish     = 10 * 7 * 800            = 56000  -> carn = 56000*100/26411 = 212 %
Hay dish     = 10 * 50 * 250           = 125000 -> herb = 125000*100/39616 = 315 %
```

This is the calibration to compare against a retail screenshot of the same
paddock (§9).

---

# 2. Dino attributes / gauges UI

## 2.1 Two different panels with the same 11 gauges

| Panel | Class / ctor | Value source |
| --- | --- | --- |
| Store (`DINO_ACHAT_SAVE_PANEL`) | state 0x13 `DINOSAUR_PURCHASE`, ctor `0x534430`, gauges registered at `0x534ba5..0x534da3` | the **selected `Market.ini` genotype** → phenotype (`DinoPhenotypeTable::attributes`) |
| Live (`DINO_INFO_PANEL`) | `ivwDinoInfo` ctor `0x511d80` (0x1c4 bytes), created by `FUN_0050b970` param_1[6] | the **live `saDinosaur` caches** (`0x5396c0` enter → `0x5135d0` show → `0x5e2940` fill) |

The store side is already ported (`StoreRow::attributes`,
`ParkHud::update_store_gauges`, `notes/store-panel-recon.md`). The live side is
**not** in the port.

## 2.2 Live gauge fill — `0x5e2940` → `0x5e2be0(index, value)`

`0x5e2940` runs on the info panel's `+0x60`/`+0x14` callback (installed by
`0x5135d0`) and on every tab switch. `dino = *(int*)(panel+0x30)`:

| idx | widget | value | dino field | phenotype `def+` |
| ---: | --- | --- | --- | ---: |
| 0 | `ATTRIBUTES_PERCEPTION` | `dino+0x280` | cache | 0x38 |
| 1 | `…_BEAUTY` | `dino+0x90` | direct | 0x40 |
| 2 | `…_ATTACK` | `dino+0x29c / 5` (magic `0x66666667`, `0x5e2974..0x5e2986`) | cache | 0x60 |
| 3 | `…_AGGRESSIVENESS` | `dino+0xbc` | direct | 0x6c |
| 4 | `…_ARMOR` | `dino+0x270` | cache | 0x68 |
| 5 | `…_SPEED` | `__ftol(*(float*)(dino+0x268) * 20.0f)` (`0x648154 = 20.0f`) | cache | 0x2c |
| 6 | `…_AGILITY` | `dino+0x98` | direct | 0x48 |
| 7 | `…_INTELLIGENCE` | `dino+0x284` | cache | 0x3c |
| 8 | `…_COURAGE` | `dino+0x2a4` | cache | 0x74 |
| 9 | `…_STAMINA` | `dino+0xc0` | direct | 0x70 |
| 10 | `…_ORIGINALITY` | `max(0, 100 - dino+0x1e4)` | direct | 0x194 |

The "cache" block is `saDinosaur::reset_attribute_caches` **`0x5710c0`**, called
from the need reset `0x570fd0` (and after action completion, `0x571190`):

```text
+0x264←+0x84  +0x268←+0x7c  +0x26c←+0x80  +0x270←+0xb8  +0x274←+0xc8
+0x278←+0xcc  +0x27c←+0xd0  +0x280←+0x88  +0x284←+0x8c  +0x2a4←+0xc4
+0x29c←+0xb0  +0x2a0←+0x94  +0x288..+0x298 ← +0x9c..+0xac  (+0x2a8=0)
```

Because `dino+0x50` is the 0x1b8-byte definition copy (`0x5712f9 MOVSD.REP`,
`0x571301`), `dino+X = def+(X-0x50)`; the map above is exactly the phenotype
attribute set (`phenotype-attributes-runtime.md`), which is why
`0x270←0xb8` is armor etc.

## 2.3 Gauge registration, maxima and colours — `0x5e2150`

`FUN_005e2150(gaugeName, indicsName, index, max)` resolves the widgets, then:

```text
FUN_005bfbe0(0)                      ; current = 0
FUN_005bfc00(max)                    ; PB_MAX_PROGRESS from AttributeRange.ini
FUN_005bfbc0(0); FUN_005bfc20(...)   ; min
FUN_005bfc70(20.0f, 0xea, 0x00, 0, 0xff)   ; band colours are FIXED 20/40/60/80/100
FUN_005bfc70(40.0f, 0xea, 0x65, 6, 0xff)
FUN_005bfc70(60.0f, 0xea, 0xe6, 6, 0xff)
FUN_005bfc70(80.0f, 0x88, 0xee, 0, 0xff)
FUN_005bfc70(100.0f, 0x00, 0xd7, 4, 0xff)
```

`0x5e2be0(index, value)` clamps the value to the registered max
(`[rec+0x14]+0x7c`) and writes the `%d` text.

`Data/Overlay/AttributeRange.ini` `[DINO]`:

```text
Perception=100  Beauty=100  Strangeness=100  Attack=120  Aggressiveness=100
Armor=100  WalkSpeed=100  Agility=100  Intelligence=100  Courage=100  Stamina=100
```

**Two retail quirks to reproduce (or consciously fix):**
1. The ORIGINALITY gauge is registered with the **`Stamina`** key
   (`0x5124xx` reads `s_Stamina_0065e638` twice), not `Strangeness`. Both are
   100.0 in retail, so it is invisible unless a mod changes the INI.
2. The ATTACK value is divided by 5, and the SPEED value is `walk_speed * 20`.

## 2.4 Needs tab (`DINO_INFO_BESOINS_*`)

`0x5138b0` (tab 1) hides the 12 need icons (`+0xd0..+0xfc`, 3 states × 4 needs),
then shows one per need from `dino+0x2ac / +0x2b0 / +0x2b4 / +0x2b8`, each
clamped: `if (state >= 3) state = 0`. Groups:

```text
+0xd0/+0xd4/+0xd8  = FOOD      good/mid/bad
+0xdc/+0xe0/+0xe4  = REPOS     good/mid/bad
+0xe8/+0xec/+0xf0  = VIOLENCE  good/mid/bad
+0xf4/+0xf8/+0xfc  = PEUR      good/mid/bad   (the "SECU" axis)
```

The four states are produced by `0x5714b0(axis)` from the needs
`+0x244/0x248/0x24c/0x250` and the per-need threshold arrays at
`dino+0x3c+axis*4` (each `{80,60,30}`, counts `DAT_00643114 = {3,3,3,3}`):

```c
int i = count-1;
while (i >= 0 && need > thr[i]) --i;
if (i == -1) i = count;                 // need > 80 -> 3
state[axis] = i;
```

Effective mapping after the panel clamp: `need > 60 → GOOD`, `30 < need <= 60 →
MID`, `need <= 30 → BAD`.

`FUN_00572e10` picks the "most urgent" need for the single mood indicator:
returns 0 food, 3 peur(`+0x2b8`), 2 violence(`+0x2b4`), 1 repos(`+0x2b0`),
4 = none; state 3 is skipped (used as "unset").

Cross-check that nails the axis order: the four writers and the four groups
agree — `0x571370`→`+0x244`→case 0→`+0x2ac`→group `+0xd0` = FOOD;
`0x571410`→`+0x248`→case 1→`+0x2b0`→group `+0xe8` = VIOLENCE;
`0x571440`→`+0x24c`→case 2→`+0x2b4`→group `+0xf4` = PEUR;
`0x571540`→`+0x250`→case 3→`+0x2b8`→group `+0xdc` = REPOS.

`0x513ac0` (tab 3, the CALENDAR button) hides 10 gauges, then calls
`FUN_0057aaf0(dino)` per paddock in `[[panel+0xcc]+0x14]+0x40..0x44` and shows
one gauge per paddock that contains the dinosaur. `0x513b40` (tab 2, the
FOOD_PREF button) fills six visitor-profile rows with `0x517100`/`0x576550`
(profile interest) — the same row filler the store uses; the button name and the
filled data do not obviously match, cosmetic.

## 2.5 Food-kind / size icons on tab 1

`0x5138b0` also shows one of ten `DINO_INFO_FOODKIND_*` icons from `dino+0x70`
(values 0..9) and one of six `DINO_INFO_FOODMULTIPLY_1..6` icons from
`dino+0x74` (values 1..6). Those are `def+0x20` / `def+0x24`, i.e.

```text
def+0x20 = food kind (0..9, .bsi/.bni ids)
def+0x24 = food quantity multiplier (1..6)
```

Both are written by the phenotype builder: `0x617ea7 P[0x20] = EDI` (0 for
herbivore, 5 for carnivore, then searched upward) and `0x617eaa P[0x24] = 1`.

---

# 3. Growth, aging, lifespan, health

## 3.1 Growth / aging — negative result

Tested, not guessed:

* The definition copy in the ctor (`0x5712f9`, `MOVSD.REP` of `0x6e` dwords)
  happens once; a whole-image byte scan for stores to `+0x00`/`+0x0c`/`+0x14`
  (length/width/height) and `+0x1e0` (size class) finds **no writer outside the
  phenotype builder `0x617xxx`** (`notes/harness/scan_bytes.py 19c` etc.).
* There is no "age" field: `TranslatorConstants.ini` has no age key, and the
  only per-dino timers are the action countdown (`+0x260`, `0x571190`) and the
  5 s / 10 s park passes (`0x5869e0`).
* `SizeCategory1..4MaxLength` (200/620/900/3200) are **birth** categories from
  the phenotype length; the info panel shows `dino+0x1e0` unchanged for life.

Conclusion: **no growth, no aging, no natural death from age** in this build.
Confidence high (store census + no key + no timer).

## 3.2 Health

```text
+0x14 (int16) = (uint)def+0x18 / 10          ; "mass/10" (0x570fa5 region)
+0x16 (int16) = health                        ; 0x570fa0 sets = max
+0x1d4 (int16)= max health = (int)(sqrt(weight) + 0.5*cbrt(L*W*H))   ; 0x617d47..0x617d84
+0x1d6 (int16)= regen = (int)(maxHP*stamina / RegenerationDivider) + 1   ; 0x617dd9..0x617df4
+0x2bc        = health state 0..4             ; 0x570fa5, 0x5724d0
```

`0x5724d0(delta)` (the `saDinosaur` vtable slot `+0x0c`, `0x643124`):

```c
if (dino+0x2a8) return;                 // invincible (action 0x1c / 28)
max = *(short*)(dino+0x1d4);
*(short*)(dino+0x16) -= delta;          // negative delta heals
hp  = *(short*)(dino+0x16);
pct = hp*100/max;
if (pct >= 67)      { state = 0; hp = min(hp,max); }
else if (pct >= 34) { state = 1; }
else if (hp >= 1)   { state = 2; }
else                { state = 4; hp = 0; effect 5.0f via 0x573f40(...,0x15); flags +0x516/+0x4d }
```

Regen: `0x5725e0` (every 5 s) → `if (food > 0.01 && rest > 0.01 && action != 4)
vtable+0xc(-*(short*)(dino+0x1d6))` — i.e. heal `regen` per 5 s.

## 3.3 Starvation damage

From `0x571370` (see §3.4): when the food gauge reaches 0 the dino takes

```text
damage = max(1, (int)((food_before - amount) * 0.1f))    ; 0x642ab8 = 0.1f
if (food_before >= 0.1f && food_before - amount <= 0) { +0x4d = 1; +0x517 = 1; }  // hungry event
vtable+0xc(damage)                                       // -> 0x5724d0
```

So a starving dino loses ~10 % of its max HP per 10 s tick once the bar is empty,
and dies at 0 → state 4.

## 3.4 Sickness

No evidence: no `Sick`/`Malad`/`Disease` string, no dedicated state field, and the
health state machine only has good/hurt/badly-hurt/(3 unused)/dead. Recorded as a
**negative result** (string search + `0x5724d0` census).

---

# 4. Feeding / nutrition

## 4.1 The per-tick driver — `FUN_005869e0` (park simu update)

```c
g_675a10 += dt; every10 = (g_675a10 >= 10.0); if (every10) g_675a10 = 0;
g_675a14 += dt; every5  = (g_675a14 >=  5.0); if (every5)  g_675a14 = 0;
for (d in spPark+0x68..+0x6c) {          // all dinosaurs
    d->clear_one_frame_flags();          // 0x571320
    d->update(dt);                       // 0x571540: rest gauge + action countdown
    if (every5)  d->0x5725e0();          // regen
    if (every10) {
        d->consume_food(10.0f);          // 0x571370
        d->violence_decay(10.0f);        // 0x571410
        d->refresh_need_states();        // 0x571350 -> 0x5714b0 x4
    }
}
```

## 4.2 Food gauge and the FOOD need — `0x571370`

```c
amount = arg * *(float*)(dino+0x6c);          // arg = 10.0
if (*(float*)(dino+0x25c) > amount) {
    if (food >= 0.1f && food - amount <= 0) { ...hungry flags... }
    damage = max(1, (int)((food - amount) * 0.1f));   // 0x642ab8
    vtable+0xc(damage);                                // health
    amount = *(float*)(dino+0x25c);                    // clamp to the bar
}
*(float*)(dino+0x25c) -= amount;
*(int*)(dino+0x244) = __ftol(food * 99.0f / *(float*)(dino+0x1dc));   // 0x643144 = 99
```

with the definition constants (phenotype builder, `0x617e6f..0x617e8d`):

```text
def+0x18  = weight (kg)
def+0x1c  = food_consumption = weight * FoodConsumptionRate(0.1) * 0.01
def+0x18c = max_food = weight (raw float copy)
def+0x188 = max_rest (float) = stamina + x   (x = [ESP+0x1c], not pinned)
```

so `food_consumption = weight * 0.001` per second, i.e. **1 % of the bar per
10 s** for every dino (the bar is `weight`), and `FOOD need = 99 * food/max`.

## 4.3 Food sources — the `.bsi` instance object

`saFoodSource` (`SimuActor/saFoodSource.cpp`, the assert file string is
`0x665dbf`), created by `FUN_00576c20(def)`, cleared by `0x576c50`/`0x576d00`:

```text
+0x00 u8   dirty/"has food" flag
+0x04 u8*  pointer to a byte copied from the flag (actor visibility)
+0x08 f32  current amount
+0x0c def* .bsi record
+0x10 u32  level: 0 full, 1 = 2/3, 2 = 1/3, 3 = EMPTY
+0x14..+0x17 flags
```

`FUN_00576d20(def)` = "empty it": `+0x0c = def`, `+0x08 = 0`, `+0x10 = 3`.

`FUN_00576d50(level)` = `SetFoodLevel`:

```c
dino?/fs+0x10 = level;
if (level == 0) fs+0x08 = def[+4];
else if (level == 1) fs+0x08 = def[+4] * 0.66666f;
else if (level == 2) fs+0x08 = def[+4] * 0.33333f;
else assert "Invalid food quantity" (saFoodSource.cpp:0xb8);
fs+0x00 = 1; *fs+0x04 = 1; fs+0x15 = 1;
price = def[+8];                                  // .bsi price
(*(budget->vtable[0]))(price);                    // = 0x585c90 spend_food
```

`0x585c90` is the budget vtable slot 0 (the budget's vtable lives in `.data`;
`0x643540` is the slot that holds `0x585c90`), i.e.
`budget -= price; +0x0c += price; +0x10++` = the **FOOD** bucket. So **every
refill costs the `.bsi` price and lands in FOOD**.

Refill trigger (already in `notes/employee-ai-recon.md` §7, re-checked here):

```text
Farm object (kind-8 task manager) FUN_005953f0, on the new-hour byte:
   for each {owner, food source} in +0xd8..+0xdc: push {owner, fs, hour}
Nutritionist completes -> FUN_00522300 -> FUN_00580e60(owner, hour):
   owner[0x59 + hour*8] = 1;                    // "this hour filled"
   FUN_00576d20(owner+0xb0);                    // empty a default source
   FUN_00576d50(*(int*)(owner+0x5c + hour*8));  // set the hour's level
```

So the hourly table per owner is 8 bytes/hour: `{u8 done, pad, u16 pad, u32 level}`
at `+0x58 + hour*8` (done at `+0x59`, level at `+0x5c`), hours 0..~23.

## 4.4 Consumption vs. the stat formula

Two different notions coexist and both are retail-true:

* `GetStats` (§1) counts the 10 **portion slots** at `feature+0x60`. 0 = portion
  still there. This is what the panel shows as food sufficiency.
* `saFoodSource` tracks `amount`/`level` for the visual and the refill price.
  `SetFoodLevel` never touches `feature+0x60` (no store to `+0x60` near
  `0x576xxx` in the byte scan), so **the slot writer is still Open** (§8.2).

---

# 5. Market / store economy

## 5.1 Buy

Already decoded (`notes/store-panel-recon.md`, `notes/market-recon.md`); the
money side, re-verified:

* catalog: `Market.ini` / mission `[DINOSTORE]` / `FreeGame.ini` `[DINOSTORE]`,
  entry = `{name, price, .gen}`;
* budget gate `FUN_0052bed0` (`&WARNING_NOT_ENOUGH_MONEY`, state unchanged);
* charge at `0x535eeb` inside `FENCE_INSERTION` (state 0x12):
  `0x585cc0(price)` = `budget -= price; +0x9c += price; +0xa0++` = **DINOACHAT**
  (panel row 3, yesterday `+0x154/+0x150`);
* catalogue row removal `0x594270`, dino creation `0x589020`.

## 5.2 Sell a dinosaur — state `SELLING` (`0x532cc0`)

Branch at `0x532d9c`:

```asm
00532d86 MOV EAX,[EBP+0x44]       ; the tool
00532d89 MOV ECX,[EAX+0x8]        ; scene/registry
00532d8c MOV AL,[ESP+0x27]        ; "picked a dino" flag from the pick loop
00532d90 MOV EDX,[ECX+0x34]
00532d93 TEST byte [EDX+0x65],AL  ; sellable flag
00532d96 JZ 0x532ea7              ; -> &WARNING_SELLING_UNAVAILABLE_DINO
00532d9c PUSH 0x663df8            ; &REQUEST_SELL_DINO  "Sell this dinosaur for "
...
00532dee MOV EAX,[EBP+0x50]       ; the selected drsDinosaur
00532df7 MOV ECX,[EAX+0x34]       ; saDinosaur
00532dfa FILD dword [ECX+0x1ec]   ; <-- the value
00532e00 FMUL float [EDX+0x98]    ; * budget SellBackFactor
00532e06 CALL 0x5982d4            ; __ftol
00532e10 PUSH 0x64c6cc            ; "%d"
```

Confirm: `0x533a00` (validate callback) = `0x585c40(dino+0x1ec)` (the budget is
the global `DAT_0067572c`; the setter itself reads `[budget+0x98]`) then
`0x53a900(delete)` + `0x53b3d0(effect)`; `0x585c40` applies `[budget+0x98]`
and credits `+0x8`, `+0xb4++`, `+0xb8 += amount` = **VENTES** (panel row 13).
So the dialog number and the credit agree:
`refund = trunc((float)dino_value * SellBackFactor)`.

### Where `dino+0x1ec` comes from

`dino+0x1ec = def+0x19c` (definition copy base `0x50`). `def+0x19c` is written by
the phenotype builder `FUN_00617250` at `0x61765f`/`0x6176c6`:

```c
P[0x198] = 0; P[0x19c] = 0;
for (g = 0; g < 18; ++g) {                    // runtime genotype: 3 dwords/gene
    rec = gene[g].record;                     // alleleA, alleleB, record*
    if (rec->count == 0) continue;
    tbl = rec->table;                         // per-allele class table
    P[0x198] += 4 - tbl[A] - tbl[B];           // "purity" counter
    P[0x19c] += priceTable[tbl[A]] + priceTable[tbl[B]];   // priceTable = [ebp+0x2c8]
}
```

`priceTable` is the translator's 3-entry table at `+0x2c8`; the only
price-like constants in `TranslatorConstants.ini` are

```text
RareAlleleMarketPrice     = 500
UncommonAlleleMarketPrice = 100
CommonAlleleMarketPrice   = 50
```

so `market value = Σ over 36 alleles of {50,100,500}`, max **18,000**, minimum
**1,800** (all common). This is a *genotype rarity* value, independent of the
`Market.ini` purchase price — a 9,000 Allosaurus can sell for less.

Confidence: **high** for "the sell price is `dino+0x1ec`" and for the loop
structure; **medium** for the table identity (it requires a runtime dump of
`[ebp+0x2c8..0x2d0]` to prove the order is Rare/Uncommon/Common, §8.4).

## 5.3 Non-empty / other gates

* Selling a **paddock** blocks when `stFence+0x98 != +0x9c`
  (`&WARNING_SELLING_NON_EMPTY_FENCE`), unchanged.
* Selling a **dino** is blocked by the `+0x65` byte flag test above
  (`&WARNING_SELLING_UNAVAILABLE_DINO` 0x663dd4). The flag's producer is Open
  (§8.5) — it is not the paddock, not the diet and not the health state.
* Shows block while running (`&WARNING_SELLING_RUNNING_SHOW`).

---

# 6. Daily economy — what the port has vs. what retail does

Port `src/world/economy.cpp` matches the retail buckets 1:1
(`notes/park-economy-recon.md` §7): FOOD, EFFECTIFS_FOOD, EFFECTIFS_SECU,
DINOACHAT, LABOACTION, CONSTRUCTION, ENTREES, RESTOS/BARAQUES/SOUVENIRS/
BOITNOIRE/POINTDEVUE/GRADINS, VENTES, plus `expenses()`/`income()`/`net()`.

Verified call sites that matter for dinosaurs:

| Bucket | Setter | Retail caller | Port |
| --- | --- | --- | --- |
| DINOACHAT | `0x585cc0` | `0x535eeb` (dino placed) | `spend_dinoachat` — wired |
| VENTES | `0x585c40` | `0x533a00` (dino sold), `0x5333xx` (fence sold) | `add_vente` is called by both sells: `Game::sell_dino` and `sell_paddock_now`; the fence log prints `ventes=count/total` (`test_cpp_fence_sell_empty`: 1/1890). |
| FOOD | `0x585c90` (budget vtable[0]) | `0x576d50` (food-source refill) | `spend_food` wired: the nutritionist level-0 refill charges the `.bsi` price (`test_cpp_employee_feeding_and_wages`, PigFood 700). |
| EFFECTIFS_SECU | `0x585d50` | `0x58ad00` over `park+0x90..+0x94` | not wired (known SIM-4 gap) |
| EFFECTIFS_FOOD | `0x585d80` | `0x58ad00` over `park+0xe4..+0xe8` (employees, `cat 0`) | not wired (known SIM-4 gap) |
| ENTREES | `0x585de0` | visitor entry + `0x58ad00` | wired |

So the dino-specific daily-economy gap is exactly **the FOOD refill charge**
(every refill costs the `.bsi` price), **the VENTES credit for object sales**
(the new fence sell credits the raw cash only) and the two staff buckets.
Nothing else dino-related is missing from the ledger.

---

# 7. Port gap list

| # | Gap | Retail oracle | Where to implement |
| ---: | --- | --- | --- |
| G1 | Paddock 4 stats (count / fill% / carn & herb sufficiency) | `0x57e720` + `0x58c460`/`0x58c1c0` scale | new `dino::PaddockStats paddock_stats(const Paddock&, span<const DinoRuntime*>, span<const PlacedFood*>)` in `src/world/build.{h,cpp}` (next to `paddock_capacity`); call from `src/ui/park.{h,cpp}` panel and from the paddock list rows |
| G2 | Live dino info panel (11 gauges + 4 need icons + food kind/quantity + mood) | `0x511d80` ctor, `0x5135d0` tabs, `0x5138b0`, `0x5e2940`, `0x5710c0`, `0x5714b0`, `0x572e10` | `src/ui/park.{h,cpp}` (widgets + `update_dino_gauges`) + `src/viewer/main.cpp` (dino-selected state / hit test) |
| G3 | Dino runtime fields the panel needs | `+0x16/+0x1d4/+0x1d6/+0x244..0x250/+0x25c/+0x264..0x2a4/+0x2ac..0x2b8/+0x2bc` | `src/world/dino_actions.{h,cpp}` (`DinoNeeds`): add `max_food`, `food_gauge`, `max_rest`, `rest_gauge`, `health_max`, `regen`, `health_state`, the 11 attribute caches, the 4 need states |
| G4 | Retail food decay (replaces the tuned `food_decay = 0.5/s`) | `0x571370` + `0x5869e0` 10 s cadence | `DinoActionLoop::tick` in `src/world/dino_actions.cpp`: accumulate a 10 s timer, `gauge -= 10*consumption`, `need = 99*gauge/max`, clamp |
| G5 | Health damage/regen/death | `0x5724d0`, `0x5725e0` | same file: `apply_damage(delta)` with the 67/34/0 thresholds and state 4, `heal(regen)` on the 5 s pass |
| G6 | Food-source level + price | `0x576c20/0x576d20/0x576d50`, budget vtable[0] `0x585c90` | `src/world/food.h` (`FoodSource::level`, `amount`, not `remaining` alone) and `src/world/employees.cpp` (`source.level = level; source.amount = quantity*factor; economy.spend_food(def.price)`) |
| G7 | Dino market value | def `+0x19c`, `0x61765f..0x6176c6`, `*AlleleMarketPrice` | `src/formats/dino_data.{h,cpp}`: `DinoAttributes::market_value` + a `PhenotypeTable::allele_price[3]` read from `TranslatorConstants.ini`; assert on a real `.gen` |
| G8 | Dino sell flow | `0x532d9c..0x532ea7`, `0x533a00`, `0x585c40` | `src/viewer/main.cpp` sell mode: extend the existing fence branch with a dino hit test, `REQUEST_SELL_DINO` text, `add_vente(trunc(value*sellback))`, delete the dino |
| G9 | FOOD bucket wiring | `0x576d50` | `src/world/economy.cpp` needs no change; call `spend_food` from G6 |
| G10 | `need_state` panel clamp | `0x5138b0` (`state >= 3 → 0`) | `src/world/dino_actions.cpp::need_state` stays 0..3 for `.bai`; add a separate `panel_state(state)` returning 0 when 3 |
| G11 | Sale income must reach the ledger | `0x585c40` = VENTES (`+0xb4/+0xb8`) | `src/viewer/main.cpp` `sell_paddock_now` (and the new dino sell): `budget_ledger.add_vente(value)` next to `park_budget += value` |

Notes on intent:
* G4/G1 share `food_consumption`; compute it once in `dino_data` and store it on
  the runtime dino to avoid drift.
* The port's `DinoNeeds::value` order is FOOD/VIOLENCE/SECU/REPOS, which matches
  `+0x244/+0x248/+0x24c/+0x250`; the *panel* order is FOOD/REPOS/VIOLENCE/PEUR
  (`+0x2ac/+0x2b0/+0x2b4/+0x2b8`), so G2 must reorder.

## 7.1 Smallest verifiable slice — "live dino stats panel + food sufficiency + dino sell value"

Order (each step is testable on its own):

1. **`DinoAttributes` additions** (`src/formats/dino_data.*`): `food_consumption`,
   `max_food`, `max_health`, `regen`, `max_rest`, `food_kind`, `food_quantity`,
   `market_value`. Constants: `FoodConsumptionRate 0.1`, `RegenerationDivider
   1000`, `HitPointsDivider 10`, `*AlleleMarketPrice`, `1/3` for the HP cube
   root. Test: `test_cpp_phenotype_hp_food` on `allo.gen` — weight 2000 →
   `food_consumption = 2.0`, `max_food = 2000`, `max_health =
   (int)(sqrt(2000)+0.5*cbrt(12m*…))`, `regen = maxHP*stamina/1000+1`.
2. **`paddock_stats`** (§G1) with `scale = 13205.333f` as a named constant
   `kFenceFoodScale = 39616.0f / 3.0f`. Tests:
   `two herbivores + 1 hay + 1 carnivore + 1 cow` → four exact ints; empty
   paddock → `0, 0, 100, 100`; all portions eaten → `0 %`.
3. **`--paddock-stats` CLI** printing the four values per paddock for a real
   park; compare against a retail `FENCE_STATS_PANEL` capture of the same
   paddock (the calibration in §1.7).
4. **Live panel** (§G2): reuse the store's `PROGRESS` widgets; feed the gauge
   values from the caches (§2.2) and the need icons from §2.4. Test:
   `--dino-info PROBE` printing the 11 values + 4 states, cross-checked against
   the store gauges of the same genotype (perception/beauty/agility/stamina
   must agree; attack is `total/5`, speed is `walk*20`).
5. **Dino sell** (§G8): `sell dino N` action + `REQUEST_SELL_DINO` text +
   `add_vente`. Test: buy `Allosaurus` (9000, catalog), sell it → budget delta =
   `trunc(market_value * sellback)`, `ventes_total` fed, dino gone, paddock
   capacity released.

Evidence plan per step: unit test on real data (`.gen`, `.bsi`,
`AttributeRange.ini`, `TranslatorConstants.ini`), plus one retail cross-check
(screenshot or x32dbg read). Runtime probes to settle the Open items below.

---

# 8. Open

1. **`def+0x188` max rest** — the formula is `stamina + x` with `x = [esp+0x1c]`
   in `0x617dfb`; `x` was not traced. Low impact (rest is not in the requested
   formulas) but needed for a faithful rest gauge.
2. **The `feature+0x60` slot writer** — proven shape (10 × 8 bytes, byte 0 ==
   0 means "portion available") and consumer, but no writer was found. Byte
   scan of the whole `.text` for stores to `+0x60` in the 0x57xxxx range did not
   locate the eat handler. Candidate: the `Eat` action's effect application
   (`0x569a00` per `dinosaur-action-loop.md`) may mark the slot through a
   feature virtual. Runtime probe: break on write to `feature+0x60` while a dino
   eats.
3. **Paddock-list label swap** — `fence-construction-recon.md` maps `out[2]` to
   the HERBI slot and `out[3]` to CARNI, the info panel uses the opposite. One
   retail capture of `OPEN_PADDOCK_LIST_PANEL` with a mixed paddock settles it.
4. **`priceTable` at translator `+0x2c8`** — dump 12 bytes at runtime in
   `0x6176ac` to confirm it is `[Rare, Uncommon, Common]` (or the reverse) and
   therefore that the sell value is money.
5. **Sellable flag `[obj+0x65]`** in the dino sell gate — producer unknown.
   Probe: break at `0x532d93` on a live park and dump the byte.
6. **`spPark` duration == 3.0** — the call chain is proven; the *meaning* of
   `GeneralParams.bgd` grid[0] is not. A runtime read of the two floats closes
   it (§1.6).
7. **`0x572e10` return 4** — "no need is urgent". The panel's
   `FUN_005bbeb0(i == iVar4)` loop over 4 widgets silently shows nothing for 4;
   which widget group that is was not dumped. Cosmetic.
8. **`amount` vs slots double bookkeeping** — whether `SetFoodLevel` also resets
   the 10 slots (via the missing writer) is unknown; if it does, the refill
   level directly changes the panel's food sufficiency.

---

# 9. Evidence

Ghidra (`DinoIsland.exe-ebea17`) — new in this pass:

* Stats: `0x57e720` (full disassembly `0x57e720..0x57e89c`), `0x58c460`,
  `0x58c1c0`, `0x594b80`, `0x5874a0` (call site `0x587a65..0x587a6d`),
  `0x586200` (call site `0x51d4dd`), `0x542f80` (GeneralParams loader).
* Live panel: `0x5394d0` (state 6 ctor), vtable `0x642e78`, `0x5396c0` enter,
  `0x5397f0` leave, `0x539840` update, `0x5398c0`/`0x539cb0` (teleport icon),
  `0x5135d0` (show/tab), `0x512f80`/`0x5130d0`/`0x513220`/`0x513370` (tab
  buttons set `+0x98 = 0..3`), `0x5138b0` (needs + food icons),
  `0x513b40` (profile rows), `0x513ac0` (calendar/paddock), `0x5137d0`
  (per-frame dispatch), `0x511d80` (info panel ctor, all widget offsets),
  `0x50b970` (owner of the panel), `0x5e2150` (gauge registration),
  `0x5e2940` (gauge fill), `0x5e2be0` (clamp + text), `0x5e2be0` callers.
* Attributes: `0x5710c0` (cache reset), `0x56fed0` (saDinosaur ctor,
  definition copy `0x5712f9`), `0x570fa0`, `0x570fd0`, `0x571540`, `0x571190`,
  `0x571320`, `0x571350`, `0x571370`, `0x571410`, `0x571440` (PEUR),
  `0x5714b0` (need state), `0x5724d0` (damage), `0x5725e0` (regen),
  `0x572e10` (most urgent need), `0x5869e0` (per-tick driver).
* Phenotype constants: `0x617d47..0x617e8d` (HP / regen / rest / food / diet /
  kind), `0x61765f..0x6176c6` (value), `0x643124` (vtable),
  `0x643114 = {3,3,3,3}`.
* Food source: `0x576c20`, `0x576c50`, `0x576c90`, `0x576cd0`, `0x576d00`,
  `0x576d20`, `0x576d50`, `0x580e60`, `0x585c90` (budget vtable slot 0,
  referenced from `.data` `0x643540`).
* Sell/buy: `0x532cc0` (dino branch `0x532d86..0x532ea2`), `0x533a00`,
  `0x533a70`, `0x533de0`, `0x585c40`, `0x585cc0`, `0x535eeb`, `0x58acc0`,
  `0x581710`.

Data (read-only):

* `Data/Overlay/AttributeRange.ini` (`[DINO]` 11 maxima).
* `Data/Design/Genotype/TranslatorConstants.ini` (FoodConsumptionRate,
  RegenerationDivider, HitPointsDivider, `*AlleleMarketPrice`, size categories).
* `Data/Design/generalparams/GeneralParams.bgd` (first grid float = 3.0).
* `Data/Design/foodsource/*.bsi` (`docs/formats/bsi.md`), `Market.ini`.
* `Data/Overlay/ConstructionInterface.Ini` (`DINO_INFO_*` layout).

Constants (this pass):

```text
0x6256b8 = 100.0f    0x6254a0 = 0.0f     0x6254a4 = 0.1f
0x642ab8 = 0.1f      0x643144 = 99.0f     0x626a7c = 0.01f
0x62544c = 0.5f      0x6488e8 = 1/3 (double)
0x648154 = 20.0f     0x471ab000 = 39616.0f
```

Scratch tools (gitignored): `notes/harness/scan_bytes.py` (raw displacement
scan used for the `+0x19c` / `+0x184` / `+0x2bc` writer census),
`notes/harness/scan_disp.py` (capstone linear sweep, unreliable across data).

## Handoff

`notes/evidence-index.md` was dirty in another lane when this audit was
committed, so **no evidence-index row was added**. Row to add:
`paddock 4 stats (count/fill%/carn+herb sufficiency)` → `0x57e720`,
`0x58c460`/`0x58c1c0`, `notes/dino-stats-economy-gap-audit.md` §1 →
`test_cpp_paddock_stats` (to be written with G1).
