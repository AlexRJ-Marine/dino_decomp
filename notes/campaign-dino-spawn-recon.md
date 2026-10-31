# Campaign `.wld` list0 dinosaur spawn (2026-10-31)

## The records

The campaign `.wld` simulation lists start after the road rectangles with a
`u32` count and one `saDinosaur` record per live animal. The record layout is
the same as `tools/formats/sav_campaign.py::read_campaign_dino`; the C++ port
now decodes it (`src/formats/wld.cpp::read_campaign_dino_state`) into
`WldDinoState`:

| Field | Offset / source |
| --- | --- |
| `object_id`, `x`, `y` | +0x00/+0x04/+0x08 |
| `state_bytes[4]` | +0x0c |
| `field_10`/`field_12` | +0x10/+0x12 (u16) |
| `flag` | +0x14 |
| name (UTF-16LE) | +0x19, length at +0x15 |
| `field_38` | after the name |
| thresholds[12] | +0x30 when `flag == 0` |
| gauges[0xB0] | `saDinosaur` memory image |
| `food`/`violence`/`security`/`rest` | gauges +0x30/+0x34/+0x38/+0x3c |
| `need_states[4]` | gauges +0x98 |
| `activity` / `action_id` | gauges +0xa8 / +0xac |
| `CampaignPath` prefix | after +0x1d0: position at +0, direction at +0x18 |

`x,y` are cell coordinates; the path position is the live float position and
the direction is the facing. The genotype bank is the world block's
`WorldDinosaur` list; `WldDinoState.object_id` matches
`WorldDinosaur.field_1c` (for Cpg04: 63823 = Ankylosaur1, 63830 =
Allosaurus1, ...).

## Spawn (`Game::spawn_world_dinos`)

- match each `WorldDinosaur` record to its `WldDinoState` by `field_1c`;
- spawn at `path_x/path_y`, facing `atan2(dir_y, dir_x)` (not the old generic
  paddock-centre spiral);
- find the containing paddock by `paddock_at_map`; a campaign load restores
  the saved population even if the port's approximate capacity model would
  reject it;
- apply the saved FOOD/VIOLENCE/SECU/REPOS gauges (and set `food_gauge`/
  `rest_gauge` to the same percentage), then `DinoActionLoop::restore` with
  the saved action id. `--dino-needs` is an explicit test override and wins
  over the `.wld` values.
- `--dino-fill-spawn` is a test fixture that ignores the list0 data and keeps
  the old paddock-order layout; the dino/action/danger tests use it where
  they need the old cluster.

## Evidence

Cpg04 (`Cpg04_Save.wld`):

```text
Ankylosaur1  object 63823  path (516,450)  dir (1,0)        needs 41/80/99/95
Allosaurus1  object 63830  path (432,520)  dir (-.404,.915) needs 41/80/93/86
Allosaurus2  object 63835  path (479.17,373.70)            needs 40/0/77/82
```

`test_cpp_campaign_spawn_positions` asserts the first two lines and 20
`source=wld` spawns. Full suite 548 passed.

## Raw `saDinosaur` gauges (2026-10-31)

`WldDinoState` also exposes the raw actor image fields that the same gauge
block carries (image base +0x214; see `notes/dino-stats-economy-gap-audit.md`
and `notes/dino-cache-writers-recon.md`):

| Port field | Gauge offset | Actor offset | Cpg04 values (Anky / Allo1 / Allo2) |
| --- | --- | --- | --- |
| `food_gauge` (f32) | +0x48 | +0x25c | 1786.68 / 839.58 / 819.59 |
| `rest_gauge` (f32) | +0x44 | +0x258 | 97.90 / 56.22 / 53.12 |
| `threat` (i32) | +0x8c | +0x2a0 | 53 / 68 / 68 |
| `threat_radius` (i32) | +0x68 | +0x27c | 5 / 10 / 10 |

These are parsed only; `Game::spawn_world_dinos` still sets
`DinoNeeds::food_gauge/rest_gauge` from the 0..99 need percentage. Wiring the
raw float gauges (max food = weight, max rest = `rest_base`) is the next step
for exact Eat/Sleep timing. The saved threat caches matter because retail seeds
the danger grid from them; the port currently leaves `DinoNeeds::threat` at the
CLI default, so `RunAway` cannot trigger from the Cpg04 save.

