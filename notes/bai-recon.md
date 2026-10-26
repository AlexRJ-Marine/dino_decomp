

## Tail read order (disassembly, 2026-10-26)

`0x53e060` reads the header, then a u8 at record `+0x2d` (file 0x56), then the
tail. The reader vtable calls are `call [edx+8]` with args pushed
`(dest, size, count)`. The observed order of the first tail reads:

```
+0x2d  u8
       4-byte scalar
       4-byte scalar
       count -> 12-byte records (3 u32)
       count -> 12-byte records (3 u32)
       count -> 8-byte records (2 u32)
       count -> 8-byte records (2 u32)
       ...
```

For `Eat.bai` this gives:

```
0x56 u8 1
0x57 count 1 -> 0x5b (4, 1, 200)
0x67 count 0
0x6b count 2 -> 0x6f (0,30), 0x77 (1,10)
0x7f count 0
0x83 ... more scalars/sections (not fully parsed)
```

A simple [12,12,8,8] parse consumes only part of most files (only 1/64 exact),
so there are further sections after the four arrays: scalars, then a vector of
0x58-byte records whose file form includes a 16-byte sub-vector and a string
(reader at 0x53e9a1..0x53ec06 reads `+0x28`, `+0x30` string, `+0x2c`, 16-byte
records, `+0x54`, then `+0x14/+0x18/+0x1c/+0x20/+0x24/+0x3c/+0x40`).

The vectors live in the 0xac-byte runtime record at `+0x58`, `+0x6c`
(12-byte), `+0x7c`, `+0x94` (8-byte) and `+0xa0` (0x58-byte). Finishing this
needs the loop bounds from the decompiler (currently ambiguous) or a runtime
dump of one action record.

## Correction: tail is fully solved (Ghidra + exact-EOF parse, 2026-10-26)

Everything above is superseded by the following. The file is self-describing:
every array is preceded by its count and the parse must end exactly at EOF.
64/64 retail files satisfy that invariant with the parser in
`tools/formats/bai.py`.

### Loader `0x0053e060` - real read order

```text
header (name[20], field_14, field_18, field_1c, flag_20, special_action,
        eat_flag, field_26, field_2a, field_2e, field_32, params[8])
0x56  u8   consume_flag                -> runtime +0x2d
      u32 n_pre;     n_pre    x 12 bytes  (attribute, comparison, threshold)
      u32 n_weight;  n_weight x 12 bytes  (attribute, unused, weight)
      u32 n_need;    n_need   x  8 bytes  (need, i32 delta)
      u32 n_grant;   n_grant  x  8 bytes  (attribute, value)
      u32 effect_class                 -> runtime +0x88
      u32 n_elem;    n_elem   x element (0x58-byte runtime record)
```

The loader rotates each pair when pushing into the runtime vector:

```text
0x0053e648  (attribute, comparison, threshold) -> (threshold, attribute, comparison)  +0x68
0x0053e70e  (attribute, unused, weight)        -> (unused, weight, attribute)         +0x54
0x0053e7cf  (need, delta)                      -> (delta, need)                       +0x78
0x0053e877  (attribute, value)                 -> (value, attribute)                  +0x90
```

Element file order vs runtime (0x58 bytes):

```text
file:     time, n_bones+bones, size, n_texture+string, n_events+(kind,value,percent),
          class, power, 5 floats, ramp_percent, ramp_value
runtime:  +0x00 time, +0x04 bone vector (vtable), +0x08 begin, +0x0c end,
          +0x10 cap, +0x14..+0x24 five floats, +0x28 size, +0x2c power,
          +0x30 string, +0x3c ramp_percent, +0x40 ramp_value,
          +0x44 event vector vtable, +0x48 begin, +0x4c end, +0x50 cap,
          +0x54 element_class
runtime event (16 bytes): {i32 value, u32 percent, u32 0, u32 kind}
```

### `FUN_0056e0e0` - preconditions and the consume gate

```text
comparison 0:  fail if threshold < value   -> require value <= threshold
comparison 1:  fail if value < threshold   -> require value >= threshold
other:         no constraint
```

`consume_flag == 1` first calls actor `vtable+0x2c` (float), then
`vtable+0x28` (index), and requires `actor[0x214 + index*4] != 0`. Retail
files with the flag: `Eat`, `EatBait`, `EatForSmallies`, `EatBaitForSmallies`,
`DinoAttackForCarniv`.

### `FUN_00569660` - element scoring and selection

```text
score = sum(GetAttribute(record.attribute) * record.weight)
      + rand() % action.field_14
```

Walk the first `count-1` elements; select the first with `time >= score`,
otherwise the last. `time` is a **selection threshold**, not playback time.
If `effect_class != 0`, push `{class, actor, param_4, element_index}` into the
park-behavior pending vector `DAT_006756d4` (initialized by `FUN_0051f560`).
`special_action == 5` uses the separate five-weight bucket path:
`rand() % actor+0x29c`, walk `+0x288..+0x298`, `FUN_00575de0(bucket+0xb)`,
`FUN_00573f40(actor+0x2d4, ..., bucket+0xb, ...)`, return
`rand() % weight[bucket]` as the element index.

### `FUN_0056e410` / `FUN_0056e5d0` - need deltas

The vector is read as `(delta, need)` and passed to
`FUN_0056e5d0(need, delta, scratch_actor)`, which adds the delta to need slot
`+0x30 + 4*need`, caps at 99, and scales it by a per-need factor table
(`DAT_00643054/64/74/84` indexed by `+0x98/+0x9c/+0xa0/+0xa4`). The function
also applies need 3 unconditionally from `actor+0x10 / 100000`.

### `FUN_00569a00` - element execution

Applies every event through `FUN_005727b0`, copies the five floats to
`actor+0x504`, calls `FUN_0056ddb0(ramp_percent, ramp_value)`, and treats
`element_class == 2` as a fall (`FUN_0057af70`). `element_class == 1` occurs
only on `GetInvincibility`.

### `FUN_005727b0` - event effects

The third file field is a **percentage of the current value**, not a
probability:

```text
new = clamp(value + current * percent / 100, min, max)
```

`GetQuad` proves it: percents 125/150/200/300/400 with value 0. Kinds are
**effect ids**, not `ActionList.ini` action ids (kind 9 = jump stat, while
action 9 = `Smell`):

| kind | effect | field |
| ---: | --- | --- |
| 1 | violence | `+0x248` |
| 9 | jump | `+0x264` |
| 10 | perception | `+0x280` |
| 11 | intelligence | `+0x284` |
| 13 | five weights + total | `+0x288..+0x2a0` |
| 16 | armor | `+0x270` |
| 20 | courage | `+0x2a4` |
| 23 | food gauge | `f32 +0x25c` (not in retail) |
| 24 | stun | `int16 +0x16` |
| 25 | rest gauge | `f32 +0x258` |
| 26 | scale fields | `+0x274/+0x278/+0x27c` |
| 27 | body length/height | `+0x268/+0x26c` |
| 28 | flag | byte `+0x2a8` (not in retail) |

### `FUN_00572620` + jump table `0x00572730` - attribute ids

The jump table was read directly; ids 4/5 are **integer size-class** stats
(`+0x50`/`+0x64`), while 7/8 are the rounded float body dimensions
(`+0x268`/`+0x26c`). Id 13 is the weight total `+0x29c`, id 14 the second of
the five weights `+0x28c`. Full table in `docs/formats/bai.md`.

### `FUN_0056a490` - radius from `field_18`/`field_1c`

`field_18` is the base radius; `field_1c` picks the multiplier:

```text
0  none
1  x length  (+0x268)
2  x height  (+0x26c)
3  x actor+0xf4 * 0.01
4  x (actor+0x60 + 1.0)
```

The squared radius is stored with 0.8x/1.2x bounds.

### Class-5 jump filtering

`FUN_0056d980` excludes `effect_class == 5` candidates from normal selection;
`FUN_0056db70` re-enables them when a linked obstacle is reachable. Class 5 is
independent of `special_action == 5`: `ObstAttackSize1-4` are in the special
group with `effect_class 0`, `LinkObstAttackSize1-4` have both.

### Retail parse

64/64 files, exact EOF, 0 errors: 46 preconditions in 33 files, 43 weights in
33 files, 134 need deltas, 24 grants, 204 elements in 63 files, 171 events.
Effect classes 0/1/2/3/5 = 44/1/1/1/17; element classes 0/1/2 = 200/1/3.
`field_1c` 0=9, 2=1, 3=31, 4=23; `flag_20` 0=37, 1=27. Full dump:
`notes/bai-tail-parse.json`.

### Header consumers - score, duration and end ramp

The remaining header fields are **not** timing hints. They split into a
behavior-node score transform, an action duration and an end-of-action ramp.

#### `field_32` (`+0x08`) - additive base score

The base behavior-node scorer is tiny:

```c
void FUN_0056e820(int *score, int node) {
  *score = *score + *(int *)(*(int *)(node + 4) + 8);   // action.field_32
}
```

`FUN_0056e820` is vtable `0x00643094` slot `+0x10` (`0x006430a4`). So
`field_32` is an additive score, not a timeout. Retail values fit that:
900 for the jump/linked-obstacle family, 800 `ReachWaypoint`, 5000 `Smell`,
50000 `Victory`.

#### `params[7]` and `params[i]` - percent score multipliers

Two node subclasses call the base scorer and then scale the result:

```c
// vtable 0x0064302c, ctor FUN_0056ca80
void FUN_0056cb30(int *score, int node) {
  FUN_0056e820(score, node);
  if (0 < *score)
    *score = (*(int *)(*(int *)(node + 4) + 0x28) * *score) / 100;  // params[7]
}

// vtable 0x00643040, ctor FUN_0056cb80
int FUN_0056cc20(int self, int *score, int node) {
  FUN_0056e820(score, node);
  if (0 < *score) {
    int i = *(int *)(*(int *)(self + 0x10) + 0xcc);   // index source
    if (i == 0) i = 0;
    else        i = *(int *)(*(int *)(i + 0x28) + 0x40);
    *score = (*(int *)(*(int *)(node + 4) + 0xc + i * 4) * *score) / 100;
  }
}
```

`apDinosaurBehavior` (`0x00521b30`) constructs both as its two sub-behaviors:

```text
+0x0c = FUN_0056cb80(param_2, param_4)   -> object vtable 0x00643040 (indexed)
+0x10 = FUN_0056ca80(param_2, param_5)   -> object vtable 0x0064302c (params[7])
```

The indexed node's `+0x10` object is still unnamed; only the index path
`[node+0x10]+0xcc -> +0x28 -> +0x40` is known. When `+0xcc` is null the
index is 0.

#### `field_2e` (`+0x38`) - action duration

`field_2e` is a duration in seconds. `FUN_0056c0c0` creates/updates the
0x18-byte show-behavior record and copies it in:

```c
*(undefined4 *)(param_2 + 0x1c) = *(undefined4 *)(*(int *)(param_3 + 4) + 0x38);
```

`FUN_0056d880` decrements record `+0x1c` by dt; when it expires it calls
`FUN_0056ddb0(record, action.field_26, action.field_2a)`.

#### `field_26` (`+0x30`) / `field_2a` (`+0x34`) - action-end ramp

`FUN_0056ddb0(record, percent, value)`:

```text
percent == 100 or record == 0      -> no-op
append percent to int vector  record+0x14   (cap 10)
append value   to float vector record+0x24  (cap 10)
record+0x0c = record+0x0c * percent / 100
if the vector is already at 10 entries -> record+0x0c = 0
```

Retail defaults 50 / 30.0; `ReachWaypoint` 60 / 10.0. The renderer that
consumes the two capped vectors is still open.

#### `flag_20` (`+0x2c`) - reserved

`FUN_0056ac80` clones an action record field by field and copies the byte:

```c
*(undefined1 *)(puVar2 + 0xb) = *(undefined1 *)(puVar1 + 0xb);  // +0x2c
*(undefined1 *)((int)puVar2 + 0x2d) = *(undefined1 *)((int)puVar1 + 0x2d);
*(undefined1 *)((int)puVar2 + 0x2e) = *(undefined1 *)((int)puVar1 + 0x2e);
puVar2[0xc] = puVar1[0xc];  // +0x30
...
```

A needle scan over `.text` for byte-sized `[reg+0x2c]` operands finds only
the two unrelated application hits `0x00403a4d`/`0x00403b6f`; no behavior
code compares or branches on `flag_20`. It is stored and cloned but dead in
this build.

#### `stat_grants` (`+0x90`) - no consumer found

The loader pushes the `(value, attribute)` pairs and `FUN_0056ac80` clones the
vector through the generic `FUN_0056b1e0` copy. A scan of the action/behavior
range for `[record+0x90]`/`[record+0x94]` finds no loop over 8-byte records
and no `>> 3` stride: the only hits are the clone and unrelated actor fields.
Like `flag_20`, the stat-grant vector appears to be stored but unused in this
build.

#### Element `+0x28` - duration

`FUN_00569a00` passes element `+0x28` as `param_5` to `FUN_005727b0`
(`FUN_005727b0(puVar7[3], *puVar7, puVar7[1], *(undefined4 *)(iVar4 + 0x28), iVar4 + 0x30)`).
`FUN_005727b0` stores it in the actor countdown:

```asm
00572840  898660020000   MOV dword ptr [ESI + 0x260], EAX
```

`FUN_00571540` subtracts dt from `+0x260`:

```asm
00571a08  fld dword ptr [esi + 0x260]
00571a0e  fsub dword ptr [esp + 0x58]
00571a12  fst dword ptr [esi + 0x260]
```

so the element field is a countdown duration in seconds. `Eat` stores 0.0.
