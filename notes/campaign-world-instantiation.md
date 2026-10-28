# Campaign world instantiation — how `.wld` object lists become a populated park

Status: **solved** (Ghidra `DinoIsland.exe-ebea17` + the seven retail `.wld`
worlds + `build/campaign-autosave.sav`). Answers: which loader functions touch
`drsAnimal`/`drsEmployee`/`drsShow`, which constructors build the runtime
objects, where the initial positions/profiles come from, what the show's
`field_44`/`field_48` are, and what the port must keep.

Read with `docs/formats/wld.md`, `notes/world-objects-recon.md`,
`notes/campaign-list1-2-recon.md`.

## 0. TL;DR

1. A campaign `.wld` is **two** objects graphs, loaded in order:
   `pwdWorldManager` (the `drs*` placement records) **first**, then `spPark`
   (roads + the live simulation lists). Evidence: `FUN_0051e6c0`.
2. Each `drs*` record is *committed* (`FUN_005e7310`) which notifies one
   listener — the world manager — whose `AddObject` (`0x0053be00`) dispatches
   on `class_id-0x12` and **creates the runtime actor** (`saFoodSource`,
   `saEmployee`, the show arena…). The record's `field_1c` is the
   **pre-assigned registry id** of that actor.
3. The **positions/state are not in the placement record**; they are in the
   `.wld` simulation block (`spPark` lists), keyed by that id. For Cpg04 the
   `.wld` lists and `build/campaign-autosave.sav`'s list1/list2 are
   **byte-identical** (§7).
4. `drsShow.field_14` is the **`WallList.ini` wall style** (0=`Fence.bwi`,
   1=`Show.bwi`), *not* a show index. The show's content is its 71 waypoints
   (one placed `.bfi` feature each). `field_44`/`field_48` are **cached
   runtime ids** that the loader re-derives — not a catalog/`.shw` link.

## 1. Load order (`FUN_0051e6c0`)

```c
/* 0x0051e6c0  papParkApplication::LoadWorld(reader, version, campaign) */
if (version > 10 && campaign == 0) {
    FUN_0055dc80(); FUN_0055d980(); FUN_0055d710(reader, version);  /* campaign/mission ids */
}
FUN_0053c560(reader, version, campaign);   /* pwdWorldManager: drs* records  */
FUN_00586ad0(reader, version, campaign);   /* spPark: roads + sim lists       */
/* post pass: fences (+0x4c/+0x54) -> 0x51f950, shows (+0x5c/+0x64) -> 0x51f810 */
FUN_0051f290(...); FUN_005e5920(...); FUN_005f6950(); FUN_005d9310();
```

`FUN_00403e10` (the campaign mission-start handler — mission case 1, the
`WorldFile` branch: it calls `FUN_0051d1d0` = park app init: ten design DBs,
world-manager init, spPark creation, then reads the `.wld` version + mode and
calls `FUN_0051e6c0(reader, version, 1)` — campaign flag **1**).

The order is load-bearing: the sim list loop resolves each record's id with
`FUN_00572f40` (map `0x0067570c`) and calls the object's `Load`; a missing id
would resolve to null.

## 2. Commit + listener (how a record creates a runtime object)

`FUN_0053c560`'s list loop is identical for every list:

```c
/* read u32 class id -> mcRef factory (0x0041a7a0 creates by class id) */
mcRef ref;                                  /* 0x00447b00: ref = 0        */
FUN_00447bb0(&ref, class_id);               /* new drs* object            */
piVar3 = FUN_00447c30(&ref);                /* operator->                 */
(**(code **)(*piVar3 + 0x48))(reader, ver); /* drs*::Load                 */
FUN_005e7310(piVar3);                       /* == commit                  */
FUN_00447b30(&ref);                         /* release                    */
```

```c
/* 0x005e7310  drsObject::Commit(this) */
if (this->id_at_0x0c == 0) this->id_at_0x0c = FUN_00572ec0(this);   /* alloc id  */
for (i = 0; i < listeners.end - listeners.begin; ++i)
    (**(code **)(*listeners[i] + 8))(this);      /* manager->AddObject(this) */
this->byte_0x08 = 1;                             /* "committed"              */
```

`DAT_00676dac` is the listener container (created + filled by
`FUN_0053a400`, the `pwdWorldManager` init; it pushes the manager itself as the
single listener). `FUN_005e7400` is the matching "destroy" notify
(`listener+4`). No other listener is ever registered.

```c
/* 0x0053be00  pwdWorldManager::AddObject(drsObject* o) */
int cls = o->vtable[1](o);           /* +0x04 = class id getter */
int k   = cls - 0x12;
if (k < 0 || k > 6) return;
switch (k) { ... }                   /* jump table 0x0053c154 */
```

| case | class | handler | creates |
| ---: | --- | ---: | --- |
| 0 | 0x12 `drsDinosaur` | `0x0053be27` → `FUN_0053cd50` | saDinosaur + 4 handles (`0x005e8ea0`) |
| 1 | 0x13 `drsVisitor` | `0x0053bee0` → `FUN_0053ce70` | saVisitor (`0x005e8cc0` sets handles) |
| 2 | 0x14 `drsEmployee` | `0x0053bf4e` → **`FUN_0053cf10`** | **saEmployee** |
| 3 | 0x15 `drsAnimal` | `0x0053bfbc` (inline) | **saFoodSource** |
| 4 | 0x16 `drsShow` | `0x0053c05f` → **`FUN_0053cfb0`** | arena + render + behaviour + sound |
| 5 | 0x17 `drsFence` | `0x0053c0c9` → `FUN_0053d0b0` | wall mesh |
| 6 | 0x18 `drsBuilding` | `0x0053c133` → `FUN_0053d1a0` | building actor |

Class table installed by `0x0053a140` (sizes/ctors already in
`docs/formats/wld.md` §2). Vtables (read from each ctor's
`MOV [ESI],imm32`): `drsDinosaur 0x006483f8`, `drsVisitor 0x006483a8`,
`drsEmployee 0x00648358`, `drsAnimal 0x00648308`, `drsShow 0x00648214`,
`drsFence 0x006482b8`, `drsBuilding 0x00648268`.
`+0x04` = class id, `+0x08` = name, `+0x48` = `Load`, `+0x4c` = `Save`.

## 3. `drsAnimal` → `saFoodSource` (2 records → list1 264/267)

Case 3 of `AddObject`, `0x0053bfbc`:

```c
/* drsAnimal layout: +0x0c object_id, +0x1c field_1c, +0x20 field_20 */
if (o->byte_0x08) {                     /* committed -> tear down old handles */
    FUN_005594b0(spPark+0x30, o->+0x24);
    FUN_005f4a90(spPark+0x08, o->+0x2c);
    FUN_005868e0(spPark+0x0c, o->+0x24);
}
index   = o->field_20;                                  /* animal profile idx  */
record  = *(int *)(designDB + 0xe8) + index * 0x78;     /* .bni table, 0x78    */
fs      = FUN_005868c0(worldMgr, record + 0xc, o->field_1c);   /* create       */
FUN_005f4910(spPark, record + 0x30, fs);                /* render obj (meshes) */
FUN_005e8a50(o, fs, 0, render);                         /* drsAnimal::Set      */
```

`FUN_005868c0` is a 3-instruction stub: `this = [worldMgr+0x14]` (= spPark,
`ECX`), then `FUN_00588860(spPark, record+0xc, field_1c)`.

```c
/* 0x00588860  spPark::AddFoodSource(spPark* this, profile, id) */
obj = mcMalloc(0x50, ...); if (!obj) return 0;
obj = FUN_00576f10(obj, profile, this+0x60, this+0x5c, this+0x74, id); /* ctor */
push obj into vector [this+0xcc, this+0xd0, this+0xd4);   /* = spPark list1  */
this->byte_0 = 1;
return obj;
```

```c
/* 0x00576f10  saFoodSource::saFoodSource(this, profile, a2, a3, a4, id) */
FUN_00577600(DAT_00675738, a2, a3, a4, id);   /* saActor base, registers id   */
this->+0x3c = 0;
this->+0x40 = profile;                        /* .bni record + 0xc            */
this->vtable = 0x006431d0;
this->+0x14 = (u16)_ftol(*(float*)(profile + 0x20));
this->+0x16 = *(u16*)(profile + 0x14);
this->+0x48 = this->+0x4c = 0;
```

```c
/* 0x00577600  saActor::saActor(this, u16 kind, a, b, c, int id) */
this->+0x00 = &saActor_vtable;  this->+0x10 = 1;  this->+0x12 = 1;
this->+0x18(u16) = kind;  this->+0x1c = a;  this->+0x20 = b;  this->+0x24 = c;
this->+0x08 = this->+0x0c = 0;   this->+0x11 = this->+0x13 = 0;
if (id == 0) { this->+0x04 = FUN_00572ec0(this); }   /* allocate */
else         { this->+0x04 = id; FUN_00572fa0(id, this); }  /* adopt     */
```

So for Cpg04:

| `.wld` animal | `field_1c` → actor id | `field_20` → profile | saFoodSource `+0x14`/`+0x16` |
| --- | ---: | ---: | --- |
| id 263 | 264 | 0 | 10 / 250 |
| id 266 | 267 | 0 | 10 / 250 |

`+0x14`/`+0x16` are two `.bni`-derived constants (they end up in the saved
record at `+0x10`/`+0x12`); 250 is the **food value of `GoatAnimal`**
(`.bni` file `+0x14`; `AnimalList.ini` order is Ostrich 200, Sheep 400,
Goat 250, Pig 600, Cow 1000). 10 is the `speed`/`10.0` float. See §10 for the
one runtime read that pins `index 0` to a file name.

The animal table is built by `FUN_00542920` (`Data/design/animal`): it opens
`animallist.ini` as a **text file** and loads each named `.bni` into a
`0x78`-byte record (`+0x30` = the mesh-string vector). Employee/food
equivalents: `FUN_00541940` (`employeelist.ini`, stride `0xb0`, table at
`designDB+0xf8`), `FUN_005420d0` (`foodsourcelist.ini`), `FUN_0053d9a0`
(`walllist.ini`, stride `0xb4` = walls, table at `designDB+0x6c`).

## 4. `drsEmployee` → `saEmployee` (6 records → list2 13/15/337/375/411/63785)

```c
/* 0x0053cf10  pwdWorldManager::CreateEmployee(this, drsEmployee* e) */
profile = *(int *)(designDB + 0xf8) + e->field_20 * 0xb0;  /* employeelist.ini order */
emp     = FUN_00586890(worldMgr, profile + 0xc, e->field_1c);
actor   = FUN_00520550(emp, spPark+0x110, spPark+0xd8);      /* walker/view    */
graph   = FUN_005f46d0(profile + 0x3c, emp, designDB, designDB+0x30);
sound   = FUN_004fe820(profile + 0x90, emp);
FUN_005e8b90(e, actor, graph, sound);        /* e+0x24/0x28/0x2c/0x30          */
```

```c
/* 0x00588600  spPark::AddEmployee(this, profile, id) */
obj = mcMalloc(0x60, ...);
obj = FUN_005765e0(obj, this+0x60, this+0x5c, this+0x74, profile, id);
push obj into [this+0xe4, +0xe8, +0xec);       /* = spPark list2            */
FUN_00585c00(this->+0x120, *(int*)(profile + 0x20));  /* staff budget -= wage */
```

```c
/* 0x005765e0  saEmployee::saEmployee(this, a2, a3, a4, profile, id) */
FUN_00579300(a2, a3, a4, id);      /* saActor base + this->+0x50 = profile        */
this->vtable = 0x006431a0;
this->+0x28 = 0;  this->+0x34 = 1.0f;  this->+0x38 = 0;
this->byte_0x44 = 0;  this->byte_0x45 = 0;  this->+0x48 = 0;  this->byte_0x4c = 0;
/* 0x00579300 adds: this->+0x3c = (rand() % 200) * 0.01 */
```

`drsEmployee` is the simple base only (`0x0c` id, `0x1c` field_1c —
`0x005e8bc0` = `simple_base_load`), so **`+0x20` (profile index) is never
written on the load path** — it is zeroed by the ctor (`0x005e8b40`) and only
the editor's placement tool would set it. Therefore **all six Cpg04 employees
are profile index 0 = `Nutritionist`** (`EmployeeList.ini`: 2 / Nutritionist /
Guard). Each one also docks its wage (`profile+0x2c`) from the staff budget at
creation.

Actor ids come from `field_1c` and are exactly the ids list2 uses:

| `.wld` employee (object_id) | `field_1c` = saEmployee id |
| --- | ---: |
| 12 | 13 |
| 14 | 15 |
| 336 | 337 |
| 374 | 375 |
| 410 | 411 |
| 63784 | 63785 |

## 5. `drsShow` → arena + behaviour (1 record)

```c
/* 0x0053cfb0  pwdWorldManager::CreateShow(this, drsShow* s) */
wall = *(int *)(designDB + 0x6c) + s->field_14 * 0xb4;   /* WallList.ini, 0xb4 */
arena  = FUN_005868f0(&s->start /*+0x18*/, &s->end /*+0x20*/, wall + 0xc, s->field_44);
behav  = FUN_0051fe20(arena, designDB+0x158, designDB+0x174, s->field_48);
graph  = FUN_005f4b60(wall + 0x24, arena);
sound  = FUN_004fea70(designDB+0x120, arena, visitorProfile+0x94);
FUN_005e7630(s, arena, behav, graph, sound);              /* drsShow::Set      */
for (i = 0; i < s->waypoint_count; ++i)
    FUN_0053acd0(s, &s->waypoints[i], 0);                 /* one .bfi feature  */
```

```c
/* 0x005e7630  drsShow::Set(this, arena, behav, graph, sound) */
this->+0x5c = arena; this->+0x64 = behav; this->+0x60 = graph; this->+0x68 = sound;
this->+0x44 = *(int *)(arena + 0x70);     /* <-- field_44 overwritten */
this->+0x48 = *(int *)(behav + 0x34);     /* <-- field_48 overwritten */
FUN_005e93c0(arena, graph, sound);
```

Answers to the field questions:

- **`field_14` = 1 is the wall style**, `WallList.ini` index 1 = `Show.bwi`
  (index 0 = `Fence.bwi`, which is what all 9 Cpg04 fences use). It is used as
  the show's arena wall/ground profile (`designDB+0x6c`, stride `0xb4` =
  `.bwi`), the same value the `.shw` "wall type" carries. **It is not a show
  index.**
- **`field_44` = 195 and `field_48` = 63526 are runtime registry ids**, cached
  back into the record at save time from `arena+0x70` / `behaviour+0x34`
  (the `drsShow` ctor `0x005e74f0` zeroes both). They are passed into the two
  constructors as a *preferred id* and are then overwritten by `Set`. They are
  **not** a catalog index and **not** a `.shw` reference. (194→195 = arena
  gets `show_id+1`; the fences show the same pattern: `field_44 =
  object_id+1`, `field_48 = object_id+2`.)
- The show **has no `.shw`/catalog link at all in a `.wld`** (a campaign
  world carries no show catalog — `FUN_00558230` only reads one when the
  campaign flag is 0, i.e. for `.sav`). Its content is the waypoint list:
  one waypoint = one placed feature actor.
- The show's *identity* for the building link is the drs id: Cpg04 buildings
  196/198/63561/63565/63569 have `link = 194` (the show's `object_id`), not
  `field_44`.

Waypoint semantics (`FUN_0053acd0`, `param_3` = `drsShow`+`0x2c` element,
stride `0x18`):

```c
featdef = *(int *)(designDB + 0x7c) + wp[2] * 0x1d8;   /* FeatureList .bfi  */
if (featdef->tag_0x34 == "Judge") { /* remove the previous judge features */ }
f = FUN_00589020(owner+0x0e, wp[3], wp[4], wp[5], featdef+0xc, wp[0]);
wp[0] = *(int *)(f + 0x38);                 /* cache the feature actor id */
FUN_005f4e10(owner+0x0f, featdef+0x84, f, owner+0x17);
FUN_004fef10(owner+0x10, f, featdef+0x18c);
```

| dword | name | meaning |
| ---: | --- | --- |
| 0 (`+0x00`) | flags | feature actor registry id (201..286, written back) |
| 1 (`+0x04`) | key | second id counter (7978..8063) |
| 2 (`+0x08`) | x | **`.bfi` FeatureList index** (1..57 in Cpg04; <62) |
| 3 (`+0x0c`) | y | **rotation** 0..3 |
| 4 (`+0x10`) | z | **cell x** relative to `start` (0..72 < 80 wide) |
| 5 (`+0x14`) | extra | **cell y** relative to `start` (0..31 < 36 tall) |

Cpg04's show: `start (420,416)`, `end (499,451)`, **71** waypoints (not 70 —
the count dword is 71), feature histogram `{1:2, 5:1, 11:2, 15:4, 26:6, 27:6,
37:2, 42:44, 44:2, 57:2}`, 44 of them index 42 (`ObstacleBeam5`). No `.shw`
in `Data/Design/Scenario/Show` has 71 features, so the placed show is **not**
a copy of any `.shw` — it is an authored layout.

## 6. Where the initial positions come from (the simulation block)

`FUN_00586ad0` (version 53, campaign flag 1):

```c
if (version > 1) FUN_0058aed0(reader, version, campaign);  /* spPark lists   */
FUN_00593a60(...); FUN_00592060(...);
if (version < 2) FUN_0058ccc0(...);                        /* legacy roads   */
```

`FUN_0058aed0` first calls `FUN_0058ccc0` (the road rectangles), then:

```text
u32 road_count ; road_count x (s32 x0,y0,x1,y1)
u32 dino_count ; dino_count x (u32 id + saDinosaur::Load)
if version > 2:
    u32 food_count     ; x (u32 id + saFoodSource::Load)      <- list1
    u32 employee_count ; x (u32 id + saEmployee::Load)        <- list2
    (campaign flag 0: u32 visitor_count + records; flag 1 & v>=0x18: skipped)
    u32 count          ; ...                                    (57 per-building attractions)
u32 count ; ...
u32 count ; ...
... FUN_00585240 / FUN_00594c80 ... then 12..40 bytes of camera
```

Every id is resolved with `FUN_00572f40` (the same registry the drs commit
filled). So the actors created in §3/§4 are the objects whose positions are
read here.

Cpg04 `.wld` (parsed with `tools/formats/sav_campaign.py` at
`simulation_offset` = `0x2010`, i.e. after the roads):

| list | at | count | ids |
| --- | ---: | ---: | --- |
| list0 `saDinosaur` | `0x2014` | 20 | 63823, 63830, 63835 … 63932 (= the `drsDinosaur` `field_1c`s) |
| list1 `saFoodSource` | `0x6324` | 2 | 264, 267 |
| list2 employee | `0x6380` | 6 | 13, 15, 337, 375, 411, 63785 |
| next list | `0x6486` | **57** | = the building count (one attraction record per building) |

list1 records (`.wld` = autosave):

| id | cell (+0x08/+0x0c) | position (+0x28/+0x2c) | direction (+0x30/+0x34) | flags | u16s |
| ---: | --- | --- | --- | --- | --- |
| 264 | (461,461) | (461.5, 421.5) | (-1.0, 0.0) | 1,0,1,0 | 10 / 250 |
| 267 | (461,461) | (461.5, 445.5) | (-1.0, 0.0) | 1,0,1,0 | 10 / 250 |

list2 records:

| id | cell | position | direction | state (+0x28) | +0x44/+0x45/+0x4c |
| ---: | --- | --- | --- | ---: | --- |
| 13 | (475,340) | (474.575, 340.001) | (0.99999, -0.00235) | 2 | 0/1/0 |
| 15 | (475,340) | (474.575, 340.001) | (0.99999, -0.00235) | 2 | 0/1/0 |
| 337 | (504,364) | (503.840, 364.000) | (-1.0, 0.0) | 2 | 0/1/0 |
| 375 | (406,411) | (405.999, 410.575) | (0.00235, 0.99999) | 2 | 0/1/0 |
| 411 | (504,364) | (503.840, 364.000) | (-1.0, 0.0) | 2 | 0/1/0 |
| 63785 | (450,595) | (449.999, 594.574) | (0.00234, 0.99999) | 2 | 0/1/0 |

For the animals the `+0x08/+0x0c` pair is **(461,461)** for both while the y
positions differ — treat `+0x28/+0x2c` as the authoritative position and
`+0x08/+0x0c` as a zone/anchor cell that only employees keep in sync.

### Record → runtime field map (proved from the save/load functions)

`saActor` base (`0x00577600` ctor / `0x00577d60` save / `0x00577cc0` load):

| runtime | record (list1/list2 after the u32 id) | meaning |
| ---: | ---: | --- |
| `+0x04` | — | registry id (the u32 the manager reads, adopted/allocated by the ctor) |
| `+0x08` | `+0x04` | cell/anchor x |
| `+0x0c` | `+0x08` | cell/anchor y |
| `+0x10..13` | `+0x0c` | 4 state bytes |
| `+0x14` (u16) | `+0x10` | disk-cached profile constant |
| `+0x16` (u16) | `+0x12` | disk-cached profile constant |

`saFoodSource` (`0x00576fd0` save / `0x00576f70` load): `+0x28/+0x2c` = x,y;
`+0x30/+0x34` = dirx,diry; `+0x4c`; `+0x44` (record `+0x14..+0x28`, 0x2C total).

`saEmployee` (`0x005793d0` save / `0x00579380` load): `+0x28` = state;
`+0x2c/+0x30` = x,y; `+0x34/+0x38` = dirx,diry; then `+0x44`, `+0x45`, `+0x4c`
(record `+0x14..+0x2a`, 0x2B total).

## 7. Proof that the campaign start state *is* the `.wld`

`build/campaign-autosave.sav` (53,338 B) holds the Cpg04 world block and the
simulation lists **verbatim**:

- world block: same show (`object_id 194`, `field_14 1`, `field_44 195`,
  `field_48 63526`, 71 waypoints), same 2 animals, same 6 employees,
  same 20 `drsDinosaur` ids — every field identical to
  `Cpg04_Save.wld`;
- `s.find(bytes from the .wld)` → list1 block (count + 2 records, 92 B) at
  **`0x6942`** and the list2 block (count + 6 records, 202 B) at **`0x699E`**,
  byte-identical. So "6 employees at those cells" is the `.wld` state, not a
  played-in state.

Seven-file validation with the same parser (roads → list0 → list1 → list2 →
next):

```text
Cpg01 v51 dinos=0 food=0 emp=0 next=1
Cpg02 v51 dinos=0 food=0 emp=1 (55463) next=4
Cpg03 v50 dinos=0 food=0 emp=0 next=4
Cpg04 v53 dinos=20 food=2 (264,267) emp=6 (13,15,337,375,411,63785) next=57
Cpg05 v52 dinos=0 food=0 emp=2 (29,31) next=8
Cpg06 v51 dinos=0 food=0 emp=0 next=1
Cpg08 v51 dinos=0 food=0 emp=0 next=1
```

`next` == the world block's building count in all seven files, which
identifies the previously undecoded "57-record attraction list" (see
`notes/campaign-list1-2-recon.md` §6) as **one `snoAttraction` record per
building**.

## 8. Port contract

Minimum data the port must retain **and** apply to reproduce retail's Cpg04
park. Everything here is already parsed by `src/formats/wld.{h,cpp}`
(`SavWorldRecords`, `SavSimulation`) and by `tools/formats/sav_campaign.py`;
the gap is wiring, not parsing.

1. **World block (`WldWorld.records`)** — keep as-is and use:
   - `shows[i]`: `object_id`, `field_14` (WallList index), `start`, `end`,
     `field_44`/`field_48` (opaque ids), `waypoints[71]`.
   - `animals[i]`: `object_id`, `field_1c` (**actor id**), `field_20`
     (**animal profile index**).
   - `employees[i]`: `object_id`, `field_1c` (**actor id**); profile index 0.
   - `dinosaurs[i]`: `object_id`, `field_1c`, `field_24/28/2c`, genotype.
   - `buildings[i]`: `kind`, `cell`, `state`, `link` (a `drs*` object id —
     resolve `link == show.object_id` for the show's seating).
2. **Simulation lists** — the `.wld` reader must ALSO parse past the roads
   (it currently stops there). Required: `road_count` + rects,
   `dino_count` + `saDinosaur` records, `food_count` + `0x2C` records,
   `employee_count` + `0x2B` records, then the per-building list. Reuse
   `sav_campaign.read_campaign_list0/1/2` (generic on any blob) or
   `campaign_dino_end`.
3. **Runtime actors the port must create** (ids from the `.wld`, so cross
   references resolve):
   - `saFoodSource{ id = animal.field_1c, profile = animal.field_20,
     x,y,dirx,diry }` from list1 (match by id);
   - `saEmployee{ id = employee.field_1c, profile = 0,
     state, x, y, dirx, diry, +0x44/+0x45/+0x4c }` from list2;
   - one show arena per `drsShow`: rect = (`start`,`end`), wall style =
     `field_14`, and **71 features** from the waypoints
     (`feature = wp.x`, `rotation = wp.y`, `cell = start + (wp.z, wp.extra)`,
     `FeatureList` order = `tools/formats/bfi.py`). Do **not** try to match a
     `.shw`; do **not** read `field_44`/`field_48` as a catalog/`.shw` id.
4. **Show panel/HUD**: a campaign `.wld` has no show catalog. If the port
   lists the arena in the show panel it must synthesise the row from the
   record (rect + wall style + feature count) or leave it unnamed; the
   existing `ParkShow`/`ShowRuntime` path is catalog-entry driven and cannot
   own this show unchanged. (Visual parity of that row still needs a retail
   screenshot — SHOW-4.)
5. **Determinism notes**: sort/derive nothing from directory order —
   `drsAnimal.field_20`/`drsEmployee.field_20` are indices into tables built
   from `animallist.ini` / `employeelist.ini`; use those ini orders (port
   already loads `BuildingList.ini` the same way).
6. **Engine-state initialisation** retail performs that a pure renderer can
   skip but a simulation must copy:
   - `saEmployee` ctor seeds `+0x3c = (rand()%200)*0.01` (per-employee RNG
     offset) and docks `profile+0x2c` (wage) from the staff budget
     (`spPark+0x120`) once per employee;
   - visitors: none at campaign start (campaign flag 1 skips the list).

## 9. Corrections to existing docs

- `docs/formats/wld.md` "Open: Fence/show `field_14`" → **resolved**:
  `WallList.ini` index. Cpg04: 9 fences use 0 = `Fence.bwi`, the show uses
  1 = `Show.bwi`.
- `docs/formats/wld.md` "Open: `field_44`/`field_48`" → **resolved**: cached
  runtime ids (`arena+0x70`, `behaviour+0x34`), re-derived at load.
- `docs/formats/wld.md` "`.wld` simulation object lists after the roads" →
  **decoded** for versions 50–53 (§6/§7).
- `notes/campaign-list1-2-recon.md` §6 "57-record list not decoded" → it is
  the per-building attraction list (count == building count in all 7 files).
- Cpg04's show has **71** waypoints, not 70.

## 10. Open / the runtime reads still worth doing

`drsAnimal.field_20 = 0` for both Cpg04 animals, so both are the same species,
and the derived `+0x14/+0x16` constants are (10, 250). 250 is `GoatAnimal`'s
food value, but `AnimalList.ini` lists `OstrichAnimal` first (200), so the
index→file mapping is not settled by static reading. Exact breakpoints
(x32dbg, retail campaign Cpg04 mission 1):

| what | break at | registers | watch |
| --- | --- | --- | --- |
| animal record | `0x0053c01a` (case 3 of `AddObject`) | `ESI` = `drsAnimal`, `EBX` = `.bni` record base | `dd EBX`, `dd *(EBX-0x0c+0xe8)`-style: dump 5×`0x78` from the array base `[(*(int*)(worldMgr+0x34))+0xe8]` and match `+0x14` (food value) against 200/400/250/600/1000 |
| animal profile index | same breakpoint | `ESI` | `dd ESI+0x20` (expect 0, 0) |
| food-source outcome | `0x005888a3` (just after the ctor) | `EAX` = new `saFoodSource` | `dd EAX+0x14` (u16 10), `dd EAX+0x16` (u16 250) |
| employee profile index | `0x0053cf1b` (case 2 first insn) | `EBP` = `drsEmployee` | `dd EBP+0x20` (expect 0 ×6 → `Nutritionist`) |
| show wall style | `0x0053c066` (case 4 first insn) | `ESI` = `drsShow` | `dd ESI+0x14` (expect 1 → `Show.bwi`) |
| show id caches | `0x0053c0bf` (right after `CALL 0x0053cfb0`) | `ESI` = `drsShow` | `dd ESI+0x44`, `dd ESI+0x48` (expect 195, and a fresh id) |

`worldMgr` is `parkApp + 0x80`; at these breakpoints the design DB pointer is
already in a register (`EDI` in `AddObject`, `[ECX+0x34]`), so prefer the
register over a global address.

## 11. Evidence

| Claim | Evidence |
| --- | --- |
| load order world block → sim | `0x0051e6c0`; `0x00403e10` passes campaign 1 |
| commit + listener | `0x005e7310`, `DAT_00676dac`, `0x0053a400`, `0x005e7400` |
| `AddObject` dispatch | `0x0053be00`, jump table `0x0053c154` (7 entries) |
| animal create | `0x0053bfbc` (case 3), `0x005868c0`, `0x00588860`, `0x00576f10`, `0x00577600` |
| employee create | `0x0053cf10`, `0x00586890`, `0x00588600`, `0x005765e0`, `0x00579300` |
| show create | `0x0053c05f` (case 4), `0x0053cfb0`, `0x005868f0`→`0x00588ac0`, `0x0051fe20`, `0x005e7630`, `0x0053acd0` |
| actor save/load layouts | `0x00577d60`, `0x00577cc0`, `0x00576fd0`, `0x00576f70`, `0x005793d0`, `0x00579380`, `0x00576660`, `0x005766b0` |
| design DB tables | `0x0053d4d0` (10 DBs), `0x00542920` (animal, `animallist.ini`), `0x00541940` (employee), `0x0053d9a0` (walls, `walllist.ini`) |
| `field_14` = WallList | `0x0053cfb0` (`designDB+0x6c` + `field_14*0xb4`) + `WallList.ini` index 1 = `Show.bwi` |
| `field_44/48` re-derived | `0x005e7630` |
| sim list layout | `0x00586ad0`, `0x0058aed0`, `0x0058ccc0` |
| data | `Cpg04_Save.wld` (0x2010…), `build/campaign-autosave.sav` (0x6942, 0x699E) |

Files: `tools/formats/wld.py`, `tools/formats/sav_campaign.py`,
`src/formats/wld.{h,cpp}`, `src/viewer/main.cpp` (`load_campaign_park`).
