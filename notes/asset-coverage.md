# Asset coverage audit (2026-10-26)

Every file in `game/%MAINDIR%` (2,891 files) mapped to its spec/task. This is
the index for "is anything still undocumented?".

## By extension

| Ext | Files | Spec / status |
| --- | ---: | --- |
| `.tga` | 649 | `docs/formats/tga.md` |
| `.mesh` | 536 | `docs/formats/mesh.md` |
| `.ini` | 338 | UI: `overlay-ini.md`; data: `campaign.md`, `sounddb.md`, `laboratory.md`; lists in `docs/formats/*.md` |
| `.anim` | 299 | `docs/formats/anim.md` |
| `.wav` | 231 | `docs/formats/wav.md` |
| `.skin` | 104 | `docs/formats/skin.md` |
| `.imp` | 78 | `docs/formats/imp.md` |
| `.gen` | 77 | `docs/formats/gen.md` |
| `.png` | 66 | UI/preview art (not a data format) |
| `.skel` | 64 | `docs/formats/skel.md` |
| `.bai` | 64 | `docs/formats/bai.md` (complete) |
| `.bfi` | 62 | `docs/formats/bfi.md` (header + strings) |
| `.shw` | 46 | 43 retail `shw.md` + 3 edition demos (`read_demo_shw`) |
| `.jpg` | 42 | UI/preview art |
| `.bbi` | 39 | `docs/formats/bbi.md` |
| `.raw` | 32 | `docs/formats/raw.md` |
| `.bvi` | 24 | `docs/formats/bvi.md` |
| `.blf` | 24 | `docs/formats/blf.md` |
| `.bwi` | 24 | `docs/formats/bwi.md` |
| `.txt` | 18 | 17 `Data/States` fragments (`render-states.md`) + root `Readme.txt` |
| `.seq` | 10 | `docs/formats/seq.md` |
| `.bsi` | 10 | `docs/formats/bsi.md` |
| `.lnd` | 10 | `docs/formats/lnd.md` (width/height still open) |
| `.wld` | 8 | `docs/formats/wld.md` |
| `.mpg` | 7 | `docs/decomp/media.md` (DirectShow) |
| `.fnt` | 5 | `docs/formats/fnt.md` |
| `.bni` | 5 | `docs/formats/bni.md` |
| `.dic` | 3 | `simu-trans.md`, `anim-trans.md`, `gene_trans.py` |
| `.loc` | 2 | `docs/formats/loc.md` |
| `.bei` | 2 | `docs/formats/bei.md` |
| `.bgd` | 1 | `docs/formats/general-params.md` |
| `.dat` | 1 | `proportion.dat` in `anim-trans.md` |
| `.bad` | 1 | **open** — `Data/Design/Actions.bad`, 2,311-int blob |
| `.bin` | 1 | `Data/Pathmap.bin`, zero bytes (unused) |
| `.pdf` | 1 | retail manual (not parsed) |
| `.exe` | 1 | the original binary |
| `.ico`/`.url`/`.lnk` | 6 | installer/OS artifacts |

## By directory

| Dir | Content | Status |
| --- | --- | --- |
| `Data/Design/animal` | `.bni` + list | FMT-13 |
| `Data/Design/animations` | 10 `.seq` | FMT-15 |
| `Data/Design/attractions` | 64 `.bai`, 62 `.bfi`, 24 `.bwi` + lists | FMT-18/22/23 |
| `Data/Design/buildings` | 39 `.bbi` + list | FMT-11 |
| `Data/Design/employees` | 2 `.bei` + list | FMT-14 |
| `Data/Design/foodsource` | 10 `.bsi` + list | FMT-12 |
| `Data/Design/generalparams` | `GeneralParams.bgd` | FMT-26 |
| `Data/Design/Genotype` | 3 `.dic`, `proportion.dat`, 3 `.ini` | FMT-17/19/20/24 |
| `Data/Design/laboratory` | `Laboratory.ini` | FMT-25 |
| `Data/Design/landscape` | 10 `.lnd`, 78 `.imp`, 10 `.raw`, 24 `.blf` | WORLD-1, FMT-21 |
| `Data/Design/Scenario` | 5 INIs, 7 campaign dirs (63 INIs, 57 `.gen`, 43 `.shw`, 8 `.wld`) | FMT-8/9/16/28/29 |
| `Data/Design/visitors` | 24 `.bvi` + list | FMT-10 |
| `Data/Config` | Dino/Start/camera/KeyMapper/key table | FMT-30 |
| `Data/States` | 34 profiles + 17 fragments | FMT-27 |
| `Data/Overlay` | menu/park UI INIs, loc, pics/textures | UI-2 / FMT-7 |
| `Data/Edition` | 20 `.gen`, 3 demo `.shw` | FMT-9 (edition), FMT-8b |
| `Data/Graphics` | meshes, skins, textures, skybox, focus | formats above |
| `Data/Sounds` | 231 `.wav` + `*.ini` sound DBs | FMT-6, MEDIA-1 |

## Remaining unknowns

1. `Data/Design/Actions.bad` — 9,244 bytes, first `u32 = 5`, then floats;
   no strings, loader not located. Likely an old action/behavior table.
2. `.lnd` `width`/`height` (520x376, 312x504, ...): not the heightmap or
   occupation bbox, not a height threshold, not a basemap size. Purpose open.
3. Edition demo `.shw` judge/type value (2 or 6).
4. `.bfi` integer fields after the class tag.
5. `.bwi` field semantics (`0x08/0x0c/0x10`) and the `SWITCH_*` consumer.
6. The 41 dinosaur behavior-graph node types (`notes/dinosaur-behavior-graph.md`).
7. Show runtime catalog fields and lab `RadiationDamage` semantics.
