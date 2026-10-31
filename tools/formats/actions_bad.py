#!/usr/bin/env python3
"""Reader for ``Data/Design/Actions.bad`` (behaviour/action data).

The file is a **count + equal float blocks** table:

```text
u32  count = 5
f32  block[0][462]
f32  block[1][462]
...
f32  block[4][462]
```

9,244 bytes = 4 + 5 x 462 x 4. The block count matches the five animation
families (`HighQuadruped`, `LightQuadruped`, `HeavyBiped`, `LightBiped`,
`LowQuadruped`); each block ends in **12 playback-speed values** in the same
range as the `.bai` `field_18` animation scale (0.8..2.0 in the retail file).

The exact meaning of the rest of each block is not proven; the reader
exposes the raw floats so a port or a runtime trace can use them.

See ``docs/formats/actions-bad.md``.
"""
from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass
from pathlib import Path


class ActionsBadError(ValueError):
    pass


@dataclass
class ActionsBad:
    path: str
    count: int
    block_size: int
    blocks: list[list[float]]

    @property
    def tails(self) -> list[list[float]]:
        """The last 12 floats of every block (animation-scale candidates)."""
        return [block[-12:] for block in self.blocks]

    @property
    def durations(self) -> list[list[float]]:
        """The 51 per-kind record durations: block f[411..461].

        ``FUN_00615e80`` reads each block as 3 header floats, 51x2
        forward/lateral, 51 scale, 51x5, then these 51 durations; the mover's
        turn/gesture duration is ``durations[family][kind]``.
        """
        return [block[411:462] for block in self.blocks]

    @property
    def scales(self) -> list[list[float]]:
        """The 51 per-kind rest-gauge scales: block f[105..155].

        ``FUN_00571540`` reads them as ``block+0x354+kind*4`` and applies
        ``gauge -= progress * scale[kind]``; walk 0.5, run 2.0, the sleep
        sentinel `0x1e` -10. All five blocks agree.
        """
        return [block[105:156] for block in self.blocks]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "count": self.count,
            "block_size": self.block_size,
            "blocks": len(self.blocks),
            "tails": self.tails,
        }


def read_actions_bad(path: Path) -> ActionsBad:
    data = path.read_bytes()
    if len(data) < 8:
        raise ActionsBadError(f"{path}: too short")
    count = struct.unpack_from("<I", data, 0)[0]
    if count == 0:
        raise ActionsBadError(f"{path}: zero blocks")
    payload = len(data) - 4
    if payload % (4 * count) != 0:
        raise ActionsBadError(
            f"{path}: {payload} payload bytes not divisible by {count} blocks"
        )
    block_size = payload // (4 * count)
    blocks = []
    off = 4
    for _ in range(count):
        blocks.append(list(struct.unpack_from(f"<{block_size}f", data, off)))
        off += block_size * 4
    return ActionsBad(
        path=str(path),
        count=count,
        block_size=block_size,
        blocks=blocks,
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path)
    args = ap.parse_args(argv)
    print(json.dumps(read_actions_bad(args.path).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
