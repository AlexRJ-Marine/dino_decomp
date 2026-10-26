# Actions.bad recon (2026-10-26)

`Data/Design/Actions.bad` is the last undocumented Design file.

## Facts

- 9,244 bytes = `u32 5` + 5 ? 462 floats (5 ? 462 ? 4 = 9240).
- Referenced as `DATA\Design\Actions\bad` from park init `0x0051d1d0`
  (`+0x38`); the consuming loader is not yet located.
- Each block starts with a time-like value (`1/410`, 0.0053, 0.005, 0.0189,
  0.0103), two large coordinates, zeros, and a `0.7..2.3` value.
- The middle is near-zero (rotation/direction-like).
- The last 12 floats of each block are `0.8..2.0` playback scales, the same
  value family as `.bai` `field_18`.

Example tails:
```
block 0 [2.0,1.5,2.0,2.0,2.0,2.0,2.0,2.0,2.0,2.0,1.2,1.7]
block 4 [2.0,1.2,2.0,1.5,1.7,1.7,1.7,1.7,1.5,1.6,1.2,1.5]
```

## Hypothesis

Five blocks = five animation families; tail 12 = per-action playback speeds.
The first values may be stride/path data for a demo/behaviour replay.

## Tooling

- `tools/formats/actions_bad.py` ? count/block reader + tails.
- `tests/test_actions_bad.py` ? 5x462, spot values, tail range, exact size.
- `docs/formats/actions-bad.md` ? spec.

## Next

Find the reader (park init `+0x38` consumer) or trace the file at runtime to
name the fields.
