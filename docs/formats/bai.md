# `.bai` action definitions

Status: **solved** for the 64 retail files. The packed header, every tail
array, the runtime record layout, the candidate gates, the element scorer, the
event effects, the behavior-node score transforms and the action-end
duration/ramp are decoded. 64/64 files parse with an exact-EOF invariant and
0 errors. The remaining open items (`stat_grants`, the end-ramp renderer, the
indexed multiplier's source object) are listed in
[Open fields](#open-fields).

## Inventory

`Data/Design/attractions/actions/*.bai` - 64 files, 111-812 bytes.
`ActionList.ini` lists them in load order (the loader strips `//` comments and
the leading count). The action index is the `ActionList.ini` line order:

```text
0 Exploration   1 RunAway    2 Sleep      3 Victory     4 Eat
5 Fall          6 Healed     7 Danse      8 Restore     9 Smell
10 Speed        11 GetInvincibility        12 GetEndurance
13 GetCourage   14 GetIntelligence         15 GetPerception
16 GetBonus     17 GetInvis  18 GetJump   19 GetQuad     20 GetSlide
21 GetScotch    22 GetArmor  23 Explode   24 KickTheirAss 25 HeadCharge
26 ObstAttackSize1          27 DinoAttackForHerbivore
28 DinoAttackForCarnivore   29 Devour     30 JumpFire    31 JumpHole
32 JumpHole2    33 Smellbad  34 Jump1m_Size1             35 Jump1m_Size2
36 Jump1m_Size3  37 Jump2m_Size1            38 MineExplosion
39 Teleport     40 ForceField 41 ReachWaypoint           42 Piege
43 GetSleep     44 ForceFieldSmall         45 JumpForBiggies
46 Jump2m_Size2  47 EatBait   48 LinkObstAttackSize1      49 Jump2m_Size3
50 Jump3m_Size1  51 Jump3m_Size2             52 Jump3m_Size3
53 Jump4m_Size1  54 Jump4m_Size2             55 Jump4m_Size3
56 ObstAttackSize2          57 ObstAttackSize3          58 ObstAttackSize4
59 LinkObstAttackSize2      60 LinkObstAttackSize3      61 LinkObstAttackSize4
62 EatForSmallies            63 EatBaitForSmallies
```

The loader is **`0x0053e060`**. It allocates a **0xac-byte** runtime record,
reads the header and tail through a reader vtable (`call [edx+8]`, arguments
`dest, size, count`), and rotates the 8/12-byte pairs as it pushes them into
the record's vectors. The file is self-describing: every count is stored, so
a parse must consume the file exactly. All 64 retail files satisfy this.

## File layout

Little-endian, packed. Several fields are **not 4-byte aligned**; that is why
a naive dword scan at `0x24` misreads `special_action`.

```text
0x00  char name[20]       # NUL-padded; long names truncated to 19 chars
0x14  u32  field_14       # runtime +0x00: element-score random range
0x18  f32  field_18       # runtime +0x04: base radius/scale
0x1c  u32  field_1c       # runtime +0x84: radius scale mode
0x20  u8   flag_20        # runtime +0x2c; reserved (copied, never read)
0x21  u32  special_action # runtime +0x60; unaligned; 0 normal, 1..6 groups
0x25  u8   eat_flag       # runtime +0x2e
0x26  u32  field_26       # runtime +0x30; unaligned; end-ramp percent
0x2a  f32  field_2a       # runtime +0x34; end-ramp value
0x2e  f32  field_2e       # runtime +0x38; action duration (seconds)
0x32  u32  field_32       # runtime +0x08; additive base score
0x36  u32  params[8]      # runtime +0x0c..+0x2b; score multipliers (percent)
0x56  u8   consume_flag   # runtime +0x2d
0x57  u32  n_pre
      n_pre    x 12-byte (attribute, comparison, threshold)
      u32  n_weight
      n_weight x 12-byte (attribute, unused, weight)
      u32  n_need
      n_need   x  8-byte (need, i32 delta)
      u32  n_grant
      n_grant  x  8-byte (attribute, value)
      u32  effect_class
      u32  n_elem
      n_elem   x element
```

The two 22-character action names are truncated in the file itself:
`DinoAttackForCarnivore` -> `DinoAttackForCarniv`,
`DinoAttackForHerbivore` -> `DinoAttackForHerbiv`. Loading is by
`ActionList.ini` order, so the truncation is harmless.

### `special_action` groups

The unaligned dword at `0x21` (runtime `+0x60`) matches the
`// Special action` groups in `ActionList.ini`:

| Value | Group | Files |
| ---: | --- | --- |
| `0` | normal | 50 |
| `1` | exploration | Exploration |
| `2` | runaway | RunAway |
| `3` | sleep | Sleep |
| `4` | dinosaur attack | DinoAttackForCarnivore/Herbivore |
| `5` | obstacle attack | ObstAttackSize1-4, LinkObstAttackSize1-4 |
| `6` | victory | Victory |

`special_action == 5` selects a separate element-picking branch in
`FUN_00569660`; it is independent of `effect_class` (see below).

## Runtime record (0xac bytes)

The header fields are **reordered** in the runtime record. This map is from
the loader (`0x0053e060`); the offsets are exact.

```text
+0x00  u32   field_14            # element-score jitter range
+0x04  f32   field_18            # base radius/scale
+0x08  u32   field_32            # additive base behavior score
+0x0c  u32   params[8]           # 0x0c..0x2b; percent score multipliers
+0x2c  u8    flag_20             # reserved; copied, never read
+0x2d  u8    consume_flag
+0x2e  u8    eat_flag
+0x30  u32   field_26            # end-ramp percent
+0x34  f32   field_2a            # end-ramp value
+0x38  f32   field_2e            # action duration (seconds)
+0x3c  char  name[20]
+0x50  vector effect_weights     # vtable +0x50, begin +0x54, end +0x58, cap +0x5c
+0x60  u32   special_action
+0x64  vector preconditions      # vtable +0x64, begin +0x68, end +0x6c, cap +0x70
+0x74  vector need_deltas        # vtable +0x74, begin +0x78, end +0x7c, cap +0x80
+0x84  u32   field_1c
+0x88  u32   effect_class
+0x8c  vector stat_grants        # vtable +0x8c, begin +0x90, end +0x94, cap +0x98
+0x9c  vector effects            # vtable +0x9c, begin +0xa0, end +0xa4, cap +0xa8
```

Each vector is 12 bytes (begin/end/capacity) with its vtable 4 bytes before
the begin pointer, so the five vectors exactly fill the gaps around
`special_action`, `field_1c` and `effect_class`.

## Header field consumers

The header fields fall into three groups: radius (`field_18`/`field_1c`),
behavior-node scoring (`field_32`, `params[8]`) and action-end timing
(`field_2e`, `field_26`, `field_2a`).

### Score: `field_32` and `params[8]`

Element selection (`FUN_00569660`) is only one use of the header. The behavior
graph also scores each action before running it. The base node scorer
`FUN_0056e820` (vtable `0x00643094`, slot `+0x10`) is only:

```text
*score += action.field_32            # runtime +0x08
```

Two node subclasses call that base and then apply a percentage:

```text
FUN_0056cb30   vtable 0x0064302c   ctor FUN_0056ca80
    *score += action.field_32
    if *score > 0: *score = *score * action.params[7] / 100

FUN_0056cc20   vtable 0x00643040   ctor FUN_0056cb80
    *score += action.field_32
    if *score > 0: *score = *score * action.params[i] / 100
```

So `params[7]` is a **flat percent multiplier**, while `params[i]` is an
**indexed percent multiplier**. The index comes from the node's `+0x10`
object: `[node+0x10] + 0xcc`; a null object selects index 0, otherwise
`[[object+0x28]+0x40]` supplies `i`.

`apDinosaurBehavior` (`0x00521b30`) constructs both nodes as its two
sub-behaviors: `+0x0c` = `FUN_0056cb80` (indexed) and `+0x10` =
`FUN_0056ca80` (`params[7]`). `field_32` is therefore an **additive base
score**, not a timeout: 900 for the jump/linked-obstacle family, 800 for
`ReachWaypoint`, 5000 for `Smell`, 50000 for `Victory`.

### Duration and end ramp: `field_2e`, `field_26`, `field_2a`

`field_2e` (`+0x38`) is the **action duration in seconds**:

- `FUN_0056c0c0` creates or updates the 0x18-byte show-behavior record and
  copies `action.field_2e` into record `+0x1c`.
- `FUN_0056d880` subtracts dt from record `+0x1c`; on expiry it calls
  `FUN_0056ddb0(record, action.field_26, action.field_2a)`.

`field_26` (`+0x30`) and `field_2a` (`+0x34`) are the **action-end ramp pair**
(percent, value). `FUN_0056ddb0`:

- does nothing when percent == 100 or the target record is null;
- appends the percent to the int vector at target `+0x14` and the value to the
  float vector at target `+0x24` (both capped at 10 entries);
- scales the target's `+0x0c` value by `percent / 100`.

The retail defaults are 50 / 30.0; `ReachWaypoint` stores 60 / 10.0.

### Reserved: `flag_20`

`flag_20` (`+0x2c`) is copied verbatim by the action clone `FUN_0056ac80`
(`puVar2[0xb] = puVar1[0xb]`, alongside `+0x2d`, `+0x2e` and `+0x30..+0x38`).
A byte-read scan over `.text` finds no comparison, arithmetic or branch on the
field in the behavior module; it is reserved/dead in this build.

### Element duration

The element `+0x28` float is a **duration in seconds**, not a visual size.
`FUN_00569a00` passes it as `param_5` to `FUN_005727b0`, which stores it in the
actor countdown `+0x260` (`0x00572840`); `FUN_00571540` decrements it by dt and
completes the action when it goes negative. `Eat` stores 0.0.

## Tail arrays

The loader rotates each pair while pushing it into the runtime vector. The
dataclasses in `tools/formats/bai.py` expose the **runtime** order.

### 1. Preconditions - file `(attribute, comparison, threshold)`

Runtime `+0x68`, 12-byte records `(threshold, attribute, comparison)`.

`saDinosaur::CanDoAction` (`FUN_0056e0e0`) is the consumer:

```text
comparison 0:  fail if threshold < GetAttribute(attribute)   -> require value <= threshold
comparison 1:  fail if value < threshold                     -> require value >= threshold
other:         no constraint
```

Before the attribute loop, `consume_flag == 1` gates the action on the actor's
inventory: it calls `vtable+0x2c` (a float), then `vtable+0x28` (an index),
and requires `actor[0x214 + index*4] != 0`. The five actions with
`consume_flag == 1` are `Eat`, `EatBait`, `EatForSmallies`,
`EatBaitForSmallies` and `DinoAttackForCarniv`; the flag means "requires a
consumable item".

### 2. Effect weights - file `(attribute, unused, weight)`

Runtime `+0x54`, 12-byte records `(unused, weight, attribute)`.

`FUN_00569660` computes the element-selection score:

```text
score = sum(GetAttribute(record.attribute) * record.weight)
      + rand() % action.field_14
```

The first dword (`unused`) is copied into runtime `+0x54` but the scorer never
reads it. It carries plausible thresholds (10/20/30/40/50/80/120/200) and is
kept as `unused` rather than guessed.

### 3. Need deltas - file `(need, i32 delta)`

Runtime `+0x78`, 8-byte records `(delta, need)`.

`FUN_0056e410` walks the vector for every enabled candidate and calls
`FUN_0056e5d0(need, delta, scratch_actor)`, which adds the delta to the need
slot at `+0x30 + 4*need`, caps it at 99, and scales it by a per-need factor
table indexed by `scratch_actor +0x98/+0x9c/+0xa0/+0xa4`. The modified need
state is then re-scored, so actions that satisfy needs score higher. Need ids:
`0 food`, `1 violence`, `2 security`, `3 rest`.

### 4. Stat grants - file `(attribute, value)`

Runtime `+0x90`, 8-byte records `(value, attribute)`. The loader rotation is
proven by the push order (`0x0053e877`), but **the consumer of the `+0x90`
vector has not been located**. The values mirror the event base values:
`Smell` grants twelve attributes at 100, `GetJump` grants jump 50,
`GetArmor` grants armor 50.

### 5. Effect elements

Runtime `+0xa0`, 0x58-byte records. The file stores the fields in a different
order than the runtime record:

```text
file element:
  u32  time                 # selection threshold, not a playback time
  u32  n_bones; u32 bones[n_bones]
  f32  duration             # seconds; copied to actor +0x260 on execution
  u32  n_texture; char texture[n_texture]   # length-prefixed, NUL included
  u32  n_events; n_events x (kind, i32 value, percent)
  u32  class                # params[0]
  u32  power                # params[1]
  u32  float0..float4       # params[2..6]
  u32  ramp_percent         # params[7]
  u32  ramp_value (f32)     # params[8]
```

Runtime element:

```text
+0x00  u32   time
+0x04  ...   bone vector vtable
+0x08  u32*  bones begin
+0x0c  u32*  bones end
+0x10  u32*  bones capacity
+0x14  f32   float0
+0x18  f32   float1
+0x1c  f32   float2
+0x20  f32   float3
+0x24  f32   float4
+0x28  f32   duration         # countdown copied to actor +0x260
+0x2c  u32   power
+0x30  string texture
+0x3c  u32   ramp_percent
+0x40  f32   ramp_value
+0x44  ...   event vector vtable
+0x48  event* events begin
+0x4c  event* events end
+0x50  event* events capacity
+0x54  u32   element_class
```

Each runtime event is 16 bytes: `{i32 value, u32 percent, u32 0, u32 kind}`.
The loader reads the file triple `(kind, value, percent)` and stores it in
that rotated form.

`FUN_00569a00` executes an element: it applies every event through
`FUN_005727b0`, copies the five floats to `actor+0x504`, calls
`FUN_0056ddb0(ramp_percent, ramp_value)`, and treats `element_class == 2` as a
fall (`FUN_0057af70`). `element_class == 1` occurs only on
`GetInvincibility`.

## Element selection

`FUN_00569660` is the picker. For a normal action (`special_action != 5`):

1. Score all effect weights plus `rand() % field_14` (see above).
2. Walk the first `count-1` elements and stop at the first element whose
   `time >= score`; if none matches, use the last element.
3. If `effect_class != 0`, push a pending-effect record into the park
   behavior's global vector (`DAT_006756d4`, owned by `FUN_0051f560`).

The `time` values are therefore **graded selection thresholds**: a higher
score (a smarter/smaller/bigger dinosaur, depending on the weights) selects a
later, stronger element. They are not playback times.

For `special_action == 5` (obstacle attack) the picker uses a different path:
it picks one of the five weights at actor `+0x288..+0x298` using
`rand() % total(+0x29c)`, looks up `FUN_00575de0(bucket + 0xb)`, schedules
`FUN_00573f40(actor+0x2d4, ..., bucket+0xb, ...)`, and returns
`rand() % weight[bucket]` as the element index.

## Event effects (`FUN_005727b0`)

The event `kind` selects a stat-effect block. The third field is a
**percentage of the actor's current value**, not a probability; the handler
computes:

```text
new = clamp(value + current * percent / 100, min, max)
```

`percent` can exceed 100 (`GetQuad` uses 125/150/200/300/400). Kinds observed
in retail `.bai` files:

| Kind | Label | Actor field | Effect |
| ---: | --- | --- | --- |
| 1 | violence | `+0x248` | add, clamp 0..99, `FUN_005714b0(1)` |
| 9 | jump | `+0x264` | set from `+0x84` percent + value, clamp |
| 10 | perception | `+0x280` | set from `+0x88` percent + value, clamp |
| 11 | intelligence | `+0x284` | set from `+0x8c` percent + value, clamp |
| 13 | five_weights | `+0x288..+0x298` | all five, total `+0x29c`, `+0x2a0` |
| 16 | armor | `+0x270` | set from `+0xb8` percent + value, clamp |
| 20 | courage | `+0x2a4` | set from `+0xc4` percent + value, clamp |
| 23 | food_gauge | `f32 +0x25c` | add, clamp 0..`+0x1dc` |
| 24 | stun | `int16 +0x16` | scale by percent, then `vtable+0xc(-value)` |
| 25 | rest_gauge | `f32 +0x258` | add, clamp 0..`+0x1d8` |
| 26 | scale_fields | `+0x274/+0x278/+0x27c` | value, value^2, value + `+0x34` |
| 27 | body_length_height | `+0x268/+0x26c` | set length/height from `+0x1f/+0x20` percent |
| 28 | flag_2a8 | byte `+0x2a8` | set 1 |

Kinds 23 and 28 are handled by the function but do not occur in the 64 retail
files. The event table is **not** an `ActionList.ini` action table: kind 9 is
the jump stat, while action id 9 is `Smell`. Several ids coincide, which made
the earlier "event kind is an action id" reading misleading.

## Attribute ids (`FUN_00572620`)

Exact jump table at `0x00572730`; the offset is the actor field read.

| Id | Label | Field |
| ---: | --- | --- |
| 0 | food | `+0x244` |
| 1 | violence | `+0x248` |
| 2 | security | `+0x24c` |
| 3 | rest | `+0x250` |
| 4 | length | `+0x50` (integer size class, cm) |
| 5 | height | `+0x64` (integer size class, cm) |
| 6 | field_68 | `+0x68` |
| 7 | length_f | `f32 +0x268`, rounded |
| 8 | height_f | `f32 +0x26c`, rounded |
| 9 | jump | `+0x264` |
| 10 | perception | `+0x280` |
| 11 | intelligence | `+0x284` |
| 12 | size | `+0x98` |
| 13 | weight_total | `+0x29c` |
| 14 | weight_2 | `+0x28c` (second of five `+0x288..+0x298`) |
| 15 | field_b4 | `+0xb4` |
| 16 | armor | `+0x270` |
| 18 | aggression | `+0xbc` |
| 19 | stamina | `+0xc0` |
| 20 | courage | `+0x2a4` |
| 22 | carnivore | `+0x200` |
| 23 | food_gauge | `f32 +0x25c` |
| 24 | stun | `int16 +0x16` |
| 29 | agility | `+0x1e0` |
| 30 | flag_528 | `+0x528 != 0` |

Ids 17, 21, 25-28 return 0 (default case). The `.bai` preconditions use id 4
for the length bands (`<=200`, `201..620`, `621..950`, `>=951`) and id 5 for
the force-field height bands (`>=250`, `<=250`).

## Effect classes

`effect_class` (runtime `+0x88`) is the pending-effect family:

| Value | Files | Meaning |
| ---: | ---: | --- |
| 0 | 44 | no pending effect |
| 1 | 1 | `KickTheirAss` (fight) |
| 2 | 1 | `MineExplosion` (explosion) |
| 3 | 1 | `Teleport` (teleport) |
| 5 | 17 | jump family + linked obstacle attacks |

Classes 1/2/3 are named after their only users; the class label is a working
hypothesis. Class 5 is confirmed: `FUN_0056d980` excludes class-5 candidates
from normal selection and `FUN_0056db70` re-enables them when a linked
obstacle is reachable. Class 5 is **not** the same field as
`special_action == 5`; `ObstAttackSize1-4` are in group 5 but have
`effect_class 0`, while `LinkObstAttackSize1-4` have both.

## Element classes

`element_class` (runtime `+0x54`) is 0 for 200 elements, 1 for
`GetInvincibility` (invincibility), and 2 for `Fall`, `JumpHole` (fall). The
class-2 path calls `FUN_0057af70`; class 1 is named after its only user.

## Worked examples

### `Eat.bai` (normal, consume)

```text
header:  field_14 0, field_18 3.1, field_1c 3, flag_20 0,
         special 0, eat 1, consume 1, field_26 50, field_2a/2e 30,
         field_32 0, params (100 x 8)
tail:    pre  length >= 200 (attr 4, cmp 1, threshold 200)
         weights  none
         needs    food +30, violence +10
         grants   none
         class    0
         elements one at t=0, bone 8, duration 0, class 0,
                  event (violence, +15, 100%)
```

A large dinosaur can eat; the action raises food and violence satisfaction,
and the element adds 15 violence.

### `GetArmor.bai` (scored elements)

```text
weights: intelligence x 1, jitter 10
elements: t=20/35/75/90/100, bone 35/45/45/45/45,
          events (armor, +10/+20/+30/+40/+50, 100%),
          texture blindage.tga on the last four, ramp 100%/0
grant:   armor 50
```

A smarter dinosaur scores higher and selects a later, stronger armor element.

### `Jump1m_Size1.bai` (pending jump, class 5)

```text
pre:      length <= 200
weights:  size x 1, jitter 10
header:   field_18 3.0, field_1c 4  -> radius = 3.0 * (actor+0x60 + 1.0)
elements: t=20/40/80/120/160
          events (stun, -1, 85%), (rest, -10, 100%) ...
          ramp 50%/10, 75%/5, 25%/20, ...
class:    5
```

`field_1c 4` means the action radius is scaled by `actor+0x60 + 1.0`
(`FUN_0056a490`), then squared and stored as 0.8x/1.2x bounds.

### `Smell.bai` (grants)

```text
header:  field_32 5000, field_18 0.1
grants:  7,8,9,11,12,13,14,16,18,19,20,23 = 100
element: t=0, bone 31, ramp 1%/30.0, floats 2.0 x 5
```

The action refreshes or raises twelve attributes at once; the consumer of the
grant vector is still open, so the exact application is not claimed.

### `Danse.bai` (percent drain)

```text
weights: size x 1, jitter 10
elements: t=40/80/120, bone 49, events (rest, 0, 99/95/90%)
```

The three elements drain 1%, 5% and 10% of the current rest gauge.

## Retail statistics

| Item | Count |
| --- | ---: |
| files | 64 |
| preconditions | 46 in 33 files |
| effect weights | 43 in 33 files |
| need deltas | 134 |
| stat grants | 24 |
| effect elements | 204 in 63 files |
| element events | 171 |
| effect class 0 / 1 / 2 / 3 / 5 | 44 / 1 / 1 / 1 / 17 |
| element class 0 / 1 / 2 | 200 / 1 / 3 |

Header value sets:

- `field_14`: `{0, 5, 10, 12, 50, 100}`
- `field_18`: `{0, 0.1, 1.0, 1.25, 1.4, 1.5, 1.7, 2.0, 2.5, 3.0, 3.1}`
- `field_1c`: 0 (9 files), 2 (1), 3 (31), 4 (23)
- `flag_20`: 0 (37 files), 1 (27)
- `field_26`: end-ramp percent - 50 (63), 60 (`ReachWaypoint`)
- `field_2a`: end-ramp value - 30.0 (63), 10.0 (`ReachWaypoint`)
- `field_2e`: action duration - 30.0 s (all 64)
- `field_32`: additive base score - 0 (most), 900 (jump/linked-obstacle
  family), 800 (`ReachWaypoint`), 5000 (`Smell`), 50000 (`Victory`)
- `params[8]`: percent score multipliers - 10-800; `Sleep` is `(10 x 7, 100)`,
  `Victory` all 100, jump actions carry 700, linked obstacles 690/800,
  `ReachWaypoint` 800/600

## Reader

`tools/formats/bai.py`:

```text
python tools/formats/bai.py <actions-dir>          # scan 64 files
python tools/formats/bai.py --json <actions-dir>   # machine-readable dump
python tools/formats/bai.py <one.bai>              # one file as JSON
```

Tests: `tests/test_bai.py` - 64-file scan, exact EOF, name truncation, group
map, header values, precondition/weight/need/grant runtime order, element
layout, event percentage semantics, attribute labels, `ActionList.ini`
coverage and truncation/trailing-byte rejection.

## Evidence

- Retail data: `Data/Design/attractions/actions/*.bai`, `ActionList.ini`
  (read-only).
- Ghidra: loader `0x0053e060`; header reads `0x0053e4fe..0x0053e5ed`; tail
  rotations `0x0053e648`, `0x0053e70e`, `0x0053e7cf`, `0x0053e877`; element
  reads `0x0053e9cf..0x0053ec10`.
- `FUN_0056e0e0` - `saDinosaur::CanDoAction` (preconditions, consume gate).
- `FUN_00569660` - element scorer/selector, pending-effect queue.
- `FUN_0056e410` / `FUN_0056e5d0` - need-delta scoring and application.
- `FUN_0056a490` - `field_18`/`field_1c` radius scaling, 0.8x/1.2x bounds.
- `FUN_0056e820` - base behavior-node score (`*= field_32`); vtable
  `0x00643094` slot `+0x10`.
- `FUN_0056cb30` / `FUN_0056cc20` - `params[7]` and indexed `params[i]`
  percent multipliers; ctors `FUN_0056ca80` / `FUN_0056cb80`.
- `FUN_0056c0c0` / `FUN_0056d880` / `FUN_0056ddb0` - `field_2e` duration
  countdown and `field_26`/`field_2a` end-ramp pair.
- `FUN_0056ac80` - action clone; copies `flag_20` (`+0x2c`) but never reads it.
- `FUN_00569a00` - element execution (events, floats, ramp, fall); passes the
  element duration to `FUN_005727b0`, which stores it in actor `+0x260`.
- `FUN_005727b0` - event effects (all 13 kinds).
- `FUN_00572620` + jump table `0x00572730` - attribute ids.
- `FUN_0056d980` / `FUN_0056db70` - class-5 jump/link filtering.
- `notes/bai-recon.md` - disassembly log and raw `Eat.bai` tail dump.
- `notes/bai-tail-parse.json` - full 64-file parse dump.

## Open fields

- `FUN_0056ddb0` appends the end-ramp pair to the target's int/float vectors
  (`+0x14`/`+0x24`) and scales `+0xc`, but the renderer that consumes the two
  capped vectors is not decoded. The target is reached through the
  `asItemBehavior` record `+0xc -> +0x30 -> +4`.
- The `+0x10` object used by the indexed `params[i]` multiplier is the
  **actor**: `FUN_0056d7d0` sets node `+0x10` to the actor, and the index is
  `actor+0xcc -> +0x28 -> +0x40`. The same expression is used by
  `FUN_00567190` as a dinosaur species/family filter, so `params[i]` is
  per-species/family tuning; the object at `actor+0xcc` is still unnamed.
- `stat_grants` (`+0x90`): the loader and the action clone (`FUN_0056ac80`,
  generic vector copy `FUN_0056b1e0`) touch it, but no instruction reads the
  8-byte `(value, attribute)` records. It appears to be unused in this build.
- The five weights at `+0x288..+0x298` and `+0x2a0` are only partly named;
  `FUN_005727b0` case 13 maintains them and the total.
