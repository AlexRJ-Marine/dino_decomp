# GetCourage / ObstAttackSize1 cache writers

Status: **writer structure solved.** The cache values are copied from the
`saDinosaur` phenotype/part-record block; the exact `+0x44`/`+0x78`/`+0x80`
semantics still depend on the part-record accumulator.

## Completion (`FUN_00571190`, switch at `dino+0x2c0`)

Action IDs are the `ActionList.ini` IDs. The function runs when an action
finishes, then sets the action id to `0x1f`.

```c
case 0x0d:                         // GetCourage
    dino[+0x29c] = dino[+0x0b0];   // attack total (phenotype +0x60)
    dino[+0x2a0] = dino[+0x094];   // threat/courage cache
    // copy five dwords from +0x270..+0x280 to +0x288..+0x298
    for (i = 0; i < 5; ++i)
        dino[0x288 + 4*i] = dino[0x270 + 4*i];
    break;
case 0x1a:                         // ObstAttackSize1
    dino[+0x274] = dino[+0x0c8];
    dino[+0x278] = dino[+0x0cc];
    dino[+0x27c] = dino[+0x0d0];   // danger-grid stamp radius
    break;
```

So the two caches used by the danger grid are:

```text
stamp value  dino+0x2a0 = dino+0x94     (set by action 13)
stamp radius dino+0x27c = dino+0xd0     (set by action 26)
```

The five values copied by action 13 are the already-cached combat stats at
`+0x270` (GetBonus), `+0x274`/`+0x278`/`+0x27c` (ObstAttackSize1) and
`+0x280` (Speed/GetInvincibility).

## Effect application (`FUN_005727b0`, cases 0x0d and 0x1a)

Action 13 first computes the five stat slots `+0x9c..+0xac` from the action
parameters and the min/max tables at `dino+0x143`/`+0x144`:

```c
for (i = 0; i < 5; ++i) {
    value = (dino[0x27 + i] * param_4) / 100 + param_3;
    value = clamp(value, def_min[i], def_max[i]);   // +0x20..+0x30
    dino[0x9c + i] = value;
    sum += value;
}
dino[+0xa7] = sum;
dino[+0xa8] = dino[+0x2f] + sum;
```

Action 26 computes `+0x9d` (size), `+0x9e = size*size` and `+0x9f` from
`dino+0x34` plus the action parameter. The completion then overwrites
`+0x274/+0x278/+0x27c` from the actor fields `+0xc8/+0xcc/+0xd0`, so the
action effect's `+0x9d..` values are the per-action stat slots, not the cache.

## Mapping to the phenotype copy

`saDinosaur`'s constructor (`0x56FED0`) copies the `0x1b8`-byte phenotype
block (`0x6e` dwords) from the translator to `dino+0x14` (byte `+0x50`).
Therefore:

| Actor field | Phenotype field | Known meaning |
| --- | --- | --- |
| `+0x94` | `+0x44` | cache value; exact part-record field still open |
| `+0xb0` | `+0x60` | Attack total |
| `+0xc8` | `+0x78` | obstacle size (part-record field) |
| `+0xcc` | `+0x7c` | obstacle size squared/aux |
| `+0xd0` | `+0x80` | obstacle radius (danger-grid stamp) |
| `+0x270..+0x280` | `+0x220..+0x230` | five cached combat stats |

The `+0x44`/`+0x78`/`+0x80` fields are not in the documented Part 3 table
(`docs/formats/translator-constants.md`); they are part of the accumulator /
part-record block, which is the open CONS-1 item. The runtime Cpg04 values
(threat `53/68/40`, radius `5/10/3`) confirm the cache is populated before
the harness traces the park.

## Port impact

`src/world/dino_actions.cpp` already accepts `threat_score`; once
`DinoAttributes` exposes the part-record fields, `DinoActionLoop` can set:

```text
threat_cache   = phenotype +0x44
obstacle_radius = phenotype +0x80
```

and the danger-grid stamp/chooser threat term can use them instead of 0.

Evidence addresses: `0x571190` (completion), `0x5727b0` (ApplyActionEffect),
`0x56FED0` (phenotype copy), `0x571B50` (grid stamp).
