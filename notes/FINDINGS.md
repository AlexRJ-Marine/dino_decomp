# Dino Island — recon findings

Date: 2026-10-25. All claims below are backed by files in this workspace.

## 1. The ISO is the installer CD, not an installed game

- `Dino Island.iso`: 311,814,144 bytes, ISO9660, volume ID `DINO`, 152,253 sectors.
- 50 files, 297.7 MiB total.
- The game is **not** loose on the disc. `SETUP.EXE` is 261,820,943 bytes.
- `SETUP.EXE` is a **Wise Installation System** stub:
  - PE32, linker 6.0, timestamp 2001-08-13, overlay at `0x3A00`.
  - Strings: `STUB32.EXE`, `Wise0132.dll`, `WiseMain`, `Dino Island Installation`.
  - The whole game is in the Wise overlay (compressed DEFLATE stream + WiseScript).
- Extracted with **REWise v0.3.0** (built from Codeberg `CYBERDEV/REWise`).
  - Upstream `CMakeLists.txt` currently omits `src/input.c`; adding it fixes the link.
  - Output: `game/%MAINDIR%/`, 2,891 files, 397.5 MiB.
- `original/` also contains `AUTORUN.EXE`, `README.TXT`, `NECZ.NFO`, `BLUEMOON.NFO`.
  `README.TXT` says "CD 1", DirectX 8.1, Windows 95–XP.

## 2. The shipped executable is clean

`game/%MAINDIR%/DinoIsland.exe`

- PE32, 2,617,344 bytes, timestamp 2002-09-03, linker 6.0 (MSVC 6.0 / Visual C++ 6).
- Sections: `.text` 2,244,404 bytes, `.rdata` 152,668, `.data` 185,144, `.data1`, `.rsrc`.
- No overlay. No packer. No SafeDisc / SecuROM / StarForce strings.
- Imports: `DDRAW.dll!DirectDrawCreateEx`, `d3d8.dll!Direct3DCreate8`,
  `DINPUT8.dll!DirectInput8Create`, `WINMM` timers, `USER32`, `KERNEL32`, `ole32`.
- `dsound.dll!DirectSoundCreate8` is loaded dynamically (`LoadLibraryA`/`GetProcAddress`).
- No PDB, no debug directory, **RTTI disabled** (only `.?AVtype_info@@`).
- Assert strings retain original source paths, e.g.
  `D:\Dino\Dev\APPLI\Src\DinoAppli\WinMain.cpp`,
  `D:\Platform\Libs\mcLib\src\graph\mgEngineDx8.cpp`.

## 3. Engine identity

In-house Monte Cristo platform, statically linked:

- `mcLib`: core (`mc*`), graphics (`mg*`: engine Dx8, mesh, anim, texture, material,
  XFile/Flex import, particle), input (`mi*`), overlay (`mo*`), sound (`ms*`), math (`mm*`, `lk*`).
- `mcWorld`: resident world (`rs*`: Resident, Movable, Actor, Camera, Light, Billboard,
  Particle, MergeActor, Item), world (`wd*`).
- Game modules: `APPLI` (`daMainApplication`, `papParkApplication`), `CORE` (`lpScheduler`),
  `DISPLAY` (`gsc*`), `VIEW` (`pvw*`, `ivw*`, `mvw*`), `AI` (`ap*`),
  `ANIM` (`aac*`, `anm*`), `SOUND` (`sdm*`, `sdp*`, `sdt*`), `SIMU` (`sp*`).
- Rendering: Direct3D 8 + DirectDraw (2D/overlay) + DirectInput 8 + DirectSound 8.
- Video: MPEG-1 files, likely DirectShow (`ole32` only; no `mciSendString` import).

## 4. Assets

| Ext | Count | Size (MiB) | Notes |
| --- | ---: | ---: | --- |
| `.tga` | 649 | 127.34 | standard TGA, 16/24/32-bit |
| `.wav` | 231 | 125.15 | RIFF/WAVE PCM |
| `.mpg` | 7 | 77.46 | MPEG-1 program stream |
| `.raw` | 32 | 23.75 | terrain/height data |
| `.anim` | 299 | 19.83 | custom animation |
| `.jpg` | 42 | 7.15 | standard JPEG |
| `.skin` | 104 | 3.74 | custom skin |
| `.ini` | 338 | 3.38 | text config |
| `.png` | 66 | 2.26 | standard PNG |
| `.mesh` | 536 | 2.08 | custom mesh |
| `.fnt` | 5 | 1.01 | custom bitmap font |
| `.imp` | 78 | 0.26 | text/import descriptors |
| `.skel` | 64 | 0.20 | custom skeleton |
| `.loc` | 2 | 0.19 | UTF-16LE localized text |
| `.wld` | 8 | 0.05 | save/world |
| plus `.bai/.bfi/.bni/.bvi/.bbi/.bsi/.bwi/.blf/.bgd/.seq/.lnd/.shw/.gen` | ~400 | <1 | design/AI/config data |

No `.pak`, `.dat` archive, or DLLs. Loose files under `Data/`.

## 5. Custom format clue

`.mesh`, `.skin`, `.anim`, `.skel` share a header:

```
01 03 00 00 00            # version? (1.3)
xx xx xx xx               # per-file checksum/hash
78 56 34 12               # 0x12345678 sentinel
01 00 00 00               # chunk count?
09 00 00 00 "cow_lod1\0"  # name length + name
5d 00 00 00 ...           # count + payload
```

`.mesh` files are named `<name>_lod1.mesh`, `<name>_part1_lod1.mesh`, etc.
`.bai` starts with a literal action name ("Eat"). `.bfi/.bni/.bvi/.bbi/.bsi/.bwi/.blf/.bgd`
are compact binary parameter blocks. `.lnd` uses length-prefixed strings (name + texture paths).
`.loc` is UTF-16LE with BOM. `.fnt` has a glyph table followed by pixel data.

## 5b. `.mesh` geometry (proven)

- 536/536 meshes parsed: 47,171 vertices, 35,190 triangles.
- Body: positions (3×f32), index_count (u32), indices (u32), UV block
  (flag + count + 2×f32), normal block (flag + count + 3×f32), material tail.
- Parser/exporter: `tools/formats/mesh.py`; spec: `docs/formats/mesh.md`.
- Ghidra: magic check in `FUN_00477b50`; loader path via `FUN_004714b0`.

## 6. Current state

- `game/` holds the extracted retail tree. Treat as read-only reference.
- `tools/` holds the extraction/triage scripts (`inventory.py`, `extract_file.py`,
  `pe_triage.py`, `exe_intel.py`, `exe_strings.py`, `game_inventory.py`, `sample_formats.py`).
- `notes/` holds raw logs (`wise-file-list.txt`, `wise-extract.log`, `exe-intel.txt`,
  `exe-strings.txt`, `iso-manifest.json`).
- Nothing has been decompiled yet.

## Overlay UI / original menu assets (2026-10-25)

- The original menus are data-driven. `Data/Overlay/MenuInterface.Ini` (47,955
  bytes) documents its own schema in the header: widget types (`WIDGET`,
  `BUTTON`, `COMBO`, `EDIT`, `CURVE`, `GROUP`, `HSCROLL`, `LIST`, `PROGRESS`,
  `RENDER`, `VSCROLL`, `WINDOW`), per-object fields (`TYPE`, `PARENT`,
  `PRIORITY`, `ZPRIORITY`, `EQUIVALENT`, `X`, `Y`, `WIDTH`, `HEIGHT`,
  `VISIBLE`, `ENABLE`, `TEXTURE`, `TEXT_COLOR`, `TEXT_HEIGHT`, `TEXT_FONT`,
  `TEXT_JUSTIFY`, `TEXT`, `HINT`, `USER_ID`), and button sub-types
  (`NORMAL`, `SWITCH`, `CHECK_BOX`).
- `MenuGraphicDB.ini` (9,685 bytes) defines named atlas rectangles:
  `[NAME] X, Y, WIDTH, HEIGHT, FILENAME, NB_OBJECTS, SKIP_LIST` (e.g.
  `[MENU_DUMMY]` uses `./DATA/OVERLAY/TEXTURES/menu_choix_dino.tga`).
- `RadialMenu.Ini` (12,093 bytes) defines the radial build menu.
- `ParkInterface.Ini` (54,052 bytes) is the in-park HUD/panel layout; the other
  `*Interface*.Ini` files cover lab/list/info/construction/mission panels.
- Localised strings: `MenuEng.loc` (56 records) / `ParkEng.loc` (702), parsed
  by `tools/formats/loc.py`.
- Menu art: `Overlay/textures/campagne_800x600.tga`,
  `cinematics_800x600.tga`, island maps, and reference JPGs in
  `Overlay/pics/Menu_hors_jeu/` (main, free game, campaign, options).
- The x32dbg Free Mode path clicks 395,234 (Free Mode), 260,183 (IGUANO),
  494,451 (easy), 526,531 (Start) � see `scripts/x32dbg/play-freegame.ps1`.
- Current port menu is placeholder only; handoff/questions in
  `docs/platform/ui-original-menu.md`.

## Save format and Pathmap (2026-10-25)

- `.sav` is a serialized world: header (version 53, slot, UTF-16 save name),
  a **39-record show catalog** (ASCII `.shw` path + UTF-16 name + 6 u32
  params), then an opaque park-state object graph (3,023 bytes manual /
  2,829 auto). Header + catalog parser: `tools/formats/sav.py`,
  `tests/test_sav.py`; details in `notes/save-recon.md`,
  `docs/formats/sav.md`.
- `Data/Pathmap.bin` is **0 bytes** in the retail tree and remains 0 bytes
  after a Free Mode park session; the executable has no `Pathmap` string.
  DEC-5 is therefore "not generated/populated on the tested path" until a
  campaign/editor path proves otherwise.

## Updates to the overlay findings

- `RadialMenu.Ini` is **legacy/unused**: no executable references and its 15
  icon names are absent from every GraphicDB. Do not implement it.
- The original menu is now implemented (UI-2 Phase A); the data-driven layout,
  frame strips and preview oracle are in `docs/formats/overlay-ini.md`,
  `notes/ui-recon.md` and `tools/viewer/overlay_preview.py`.
- `.shw` show scenarios are plain text (43 files) with `//` sections: name,
  cost, judge kind, cage size, dimensions, wall type, 4-int feature grid and
  cage->team slots. 43/43 parse (1,679 features, 174 team slots). Reader:
  `tools/formats/shw.py`, spec `docs/formats/shw.md`, tests `test_shw.py`.
  Save catalog cost/dimensions match all 39 records.
- `.gen` genotypes are text: gene count + two ints per gene. 57 runtime files
  under `Data/Design/Scenario/Genotype` have 18 genes (head/legs/body/tail,
  dummies, skin, carapace, diet, aggression, stamina, courage); 20 editor
  files under `Data/Edition/Genotype` have 27. Reader `tools/formats/gen.py`,
  spec `docs/formats/gen.md`, tests `test_gen.py`.
- `simu_trans.dic` holds the 18-gene expression tables: per gene a
  translation list, an optional N?N dominance matrix and rarity weights.
  Dominant genes use translation[min(a,b)]; SknSub/CarSub/Agre/Stam/Cour use
  translation[(a+b)//2]. Reader `tools/formats/gene_trans.py`, tests
  `test_gene_trans.py`; details in `docs/formats/gen.md`.
- `.bvi` visitor profiles: 24 files, 10 personalities listed by
  `VisitorList.ini`, 96-byte header (personality id, height 1.7, walk speed,
  five preference weights), 26 mesh paths + texture. All 624 mesh paths
  exist. Reader `tools/formats/bvi.py`, spec `docs/formats/bvi.md`, tests
  `test_bvi.py`; preference and tail semantics remain open.
- `.bbi` building definitions: 39 files matching `BuildingList.ini`, 121-byte
  header, 10 mesh/skel/anim paths and a tail with texture/explosion/sound and
  `CONSTRUC_*` build-panel widget names. 390 asset paths exist; 212/213 UI
  names resolve (Security button missing). Header suggests footprint w0 x w1
  and cost w5. Reader `tools/formats/bbi.py`, spec `docs/formats/bbi.md`,
  tests `test_bbi.py`.
- `.bsi` food sources: 10 files (Flower..Cow) with a 29-byte header
  (nutrition, quantity, price, kind id), four food/part meshes + texture dir,
  a feeder ``feed_*.mesh`` and a ``FOOD_DISTRIBUTION_*_BUTTON``. All assets
  and buttons resolve. Reader `tools/formats/bsi.py`, spec
  `docs/formats/bsi.md`, tests `test_bsi.py`.
- `.bni` animals (5) and `.bei` employees (2) share an actor-profile layout:
  fixed header + interleaved numeric/string body. Animals: size, food value,
  quantity, food kind (matches `.bsi`), meshes + texture. Employees: kind,
  height 1.7, walk/action speed, meshes, textures, sounds. All 42 mesh paths
  exist. Readers in `tools/formats/actors.py`, specs `docs/formats/bni.md`,
  `bei.md`, tests `test_actors.py`.
- `.seq` animation events: 5 FX + 5 Sound files (one per family). Common
  42-47-string table of sounds/particles/bones; FX events are 6 bytes
  (frame/30, type, string index), Sound events 16 bytes (type|string, flags,
  time, value). 1,304 events, exact byte sizes. Reader
  `tools/formats/seq.py`, spec `docs/formats/seq.md`, tests `test_seq.py`.
- Campaign layer: `CampaignList.ini` (7), `CampaignBase.ini`,
  `CpgNN.ini` (island, mission list, WorldFile, budget),
  `MissionList.ini`, `MissionBase.ini`, 39 mission files with 89 objective
  commands (28 verbs). All 206 loc IDs resolve. The 8 `CpgNN_Save.wld`
  initial worlds share the `.sav` serializer; Cpg07 is unlisted cut content.
  Reader `tools/formats/campaign.py`, spec `docs/formats/campaign.md`, tests
  `test_campaign.py`.
- Visitor scoring weights: `DAT_00675638` is set once by `FUN_0051f560` from
  profile record `+0xD0`, which is the constant `.bvi` block at file
  `0x38..0x50` = [1, 1, 1, 500, 300, -3000, 3000] (identical in all 24
  files). Score = rand()%6000 - 3000 + sum(attr*(99-need)^2) - 300*distance
  - 500*price. `FUN_0059aef0` is pow(x,2). The five variable profile weights
  (file 0x1C..0x2C) land at record +0x24..+0x34 and are NOT used by the
  scorer; their consumer is open. Evidence: notes/visitor-weights-runtime.md,
  tools/recon/dump-visitor-weights.ps1, tests/test_bvi.py.
- Campaign objectives: `snoObjective::Load` (`0x5654f0`) maps 33 verbs to
  ids 0-32 via `0x5658d0` and stores a tri-state evaluator (0 pending,
  1 success, 2 failure). Simple evaluators decoded (treasury, marchandizing,
  dyingdinos, employees, visitors/happyvisitors, connected buildings, camera
  altitude, delay limit, buy-all-dinos, bankrupt); trait/show predicates open.
  Cpg07's cut mission has a `dyinddinos` typo. Table in
  docs/formats/campaign.md, evidence in notes/objectives-recon.md.
- `anim_trans.dic` / `graphic_trans.dic`: plain-text genotype dictionaries.
  anim_trans has 20 dinosaurs, 5 animation families, 51 clip names and four
  20x20 merge tables (Head/Body/Tail = identity+50, ArmLeg = directional
  0/1/50/99 with a 10/10 species split). All 51 clip names exist in all 5
  family folders. graphic_trans has 5 reference widths (100,100,100,300,150).
  Reader tools/formats/anim_trans.py, spec docs/formats/anim-trans.md.
- `proportion.dat`: u32 20 + 20 x 5 floats in anim_trans dinosaur order.
  Columns are part scale factors [head, arm, body, leg, tail]; the leg column
  is 1.0 everywhere. T-rex arm 0.235, Diplodocus tail 2.093, Brachio head
  1.1375. Pairs with the .gen head/armleg/body/tail species genes.
  Reader tools/formats/anim_trans.py --dump-proportion.
- Objective trait/show predicates: dinosaur-trait verbs build a 10-slot
  pattern compared against actor +0x90 (cute), +0xbc (aggression), +0x1e4
  (hybrid), +0x1e8 (scarcity), +0x244 (food), +0x98 (size), +0x7c (speed),
  +0xc0 (stamina), +0x1e0 (agility), +0x8c (intelligence). Show query
  0x567190 maps race/obstacle/fight/destruction/team/capture/fence and
  per-profile score arrays. carnivordinos, ornateddinos, scientificshow and dinoexclusiveshow are
  decoded; all 33 evaluator predicates are solved. Only field names and
  objective weighting remain. See notes/objectives-recon.md.
- Campaign difficulty: FUN_00561bb0 reads mission+0x50 (0/1/2) and selects
  Budget/SellBackFactor/VisProfiles plus their Medium/Hard variants. Only
  those three keys vary; Buildings/Features/FoodSources/Shows/DinoStore are
  difficulty-independent. All 33 objective predicates are now decoded
  (scientificshow = show score > arg1 and dino +0x1e4 > arg2;
  dinoexclusiveshow = a dino unique to one show).
- LOD2/LOD3 skin bind: inv(inverse_bind) equals the skeleton rest for 58/60
  dinosaur LOD skins. Only pachy LOD2/LOD3 differ, and only in the neck/head
  chain (Neck2 7.772 cm / rot 1.094, Neck1 7.633 cm, Head/Ponytail 4.316 cm).
  Using the skin inverse_bind on the .mesh adds a fixed head offset (4.316 cm
  at rest, 10.905 cm on a walk frame). Fix: use inv(skeleton rest) for the
  .mesh bind and inverse_bind for the .skin bind. Evidence:
  notes/lod-bind-recon.md, tools/recon/analyze_lod_bind.py, tests/test_skin.py.
