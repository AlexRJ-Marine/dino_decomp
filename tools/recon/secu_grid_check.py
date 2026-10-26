#!/usr/bin/env python3
"""Validate the retail dinosaur SECU/danger-grid formula from an x32dbg log.

The log must contain the breakpoints installed by
`scripts/x32dbg/start-campaign.ps1 -SecuTrace`:

    0x571B50  -> `STAMPactor=<dino>x=<x>y=<y>tr=<threat>r=<radius>`
    0x5714A5  -> `SECUactor=<dino>secu=<secu>p14=<health low/high>p1d4=<max>`

`0x571B50` is `FUN_00571b50` (stamp one dinosaur into the grid);
`0x5714A5` is the instruction after `FUN_00571440` stores `+0x24c`.
Every 1 s rebuild logs N stamps followed by N SECU values, in the same
dinosaur order.

The checker rebuilds the 128x128 grid of 8-unit cells from each stamp
block, applies

    secu = 99 + trunc((health - max_health) * 100 / max_health)
              - trunc((cell - threat) * 0.1)        (clamp >= 0)

and reports how many values match exactly.

Usage: python tools/recon/secu_grid_check.py notes/x32dbg-secu6.out.log
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

GRID_SIZE = 128
HEX = r"([0-9A-Fa-f]{8})"
SPACE = r"\s*"

STAMP_RE = re.compile(
    r"^STAMP" + SPACE + r"actor=" + HEX + SPACE + r"x=" + HEX + SPACE +
    r"y=" + HEX + SPACE + r"tr=" + HEX + SPACE + r"r=" + HEX
)
SECU_RE = re.compile(
    r"^SECU" + SPACE + r"actor=" + HEX + SPACE + r"secu=" + HEX + SPACE +
    r"p14=" + HEX + SPACE + r"p1d4=" + HEX
)


def parse_events(lines: list[str]) -> list[tuple]:
    events: list[tuple] = []
    for line in lines:
        match = STAMP_RE.search(line)
        if match:
            events.append(("S",) + tuple(int(g, 16) for g in match.groups()))
            continue
        match = SECU_RE.search(line)
        if match:
            events.append(("C",) + tuple(int(g, 16) for g in match.groups()))
    return events


def parse_pairs(events: list[tuple]) -> list[tuple[list, list]]:
    """Split into (stamp block, secu block) pairs of one rebuild."""
    pairs: list[tuple[list, list]] = []
    i = 0
    while i < len(events):
        if events[i][0] != "S":
            i += 1
            continue
        j = i
        stamps = []
        while j < len(events) and events[j][0] == "S":
            stamps.append(events[j])
            j += 1
        k = j
        secus = []
        while k < len(events) and events[k][0] == "C":
            secus.append(events[k])
            k += 1
        if (
            stamps
            and secus
            and {e[1] for e in stamps} == {e[1] for e in secus}
        ):
            pairs.append((stamps, secus))
        i = j if j > i else i + 1
    return pairs


def build_grid(stamps: list[tuple]) -> list[list[int]]:
    grid = [[0] * GRID_SIZE for _ in range(GRID_SIZE)]
    for _tag, _actor, x, y, threat, radius in stamps:
        for gx in range((x - radius) >> 3, ((x + radius) >> 3) + 1):
            if not 0 <= gx < GRID_SIZE:
                continue
            for gy in range((y - radius) >> 3, ((y + radius) >> 3) + 1):
                if 0 <= gy < GRID_SIZE:
                    grid[gx][gy] += threat
    return grid


def trunc_div(value: int, divisor: int) -> int:
    """C-style integer division (truncate toward zero)."""
    return int(value / divisor)


def expected_secu(
    cell: int, threat: int, health: int, max_health: int
) -> int:
    health_term = 0
    if max_health:
        health_term = trunc_div((health - max_health) * 100, max_health)
    secu = 99 + health_term - trunc_div((cell - threat) * 10, 100)
    return max(0, secu)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", help="x32dbg log with STAMP/SECU lines")
    parser.add_argument(
        "--show-mismatches", type=int, default=10,
        help="print at most N mismatches (default 10)",
    )
    args = parser.parse_args(argv)

    path = Path(args.log)
    events = parse_events(path.read_text(encoding="utf-8", errors="replace").splitlines())
    pairs = parse_pairs(events)
    if not pairs:
        print(f"no complete STAMP/SECU rebuild blocks in {path}")
        return 1

    deltas: Counter[int] = Counter()
    mismatches = []
    exact = total = 0
    for stamps, secus in pairs:
        grid = build_grid(stamps)
        info = {e[1]: e[2:] for e in stamps}
        for _tag, actor, secu, p14, p1d4 in secus:
            x, y, threat, _radius = info[actor]
            health = (p14 >> 16) & 0xFFFF
            max_health = p1d4 & 0xFFFF
            cell = grid[x >> 3][y >> 3]
            predicted = expected_secu(cell, threat, health, max_health)
            total += 1
            delta = secu - predicted
            deltas[delta] += 1
            if delta == 0:
                exact += 1
            elif len(mismatches) < args.show_mismatches:
                mismatches.append(
                    (hex(actor), x, y, threat, health, max_health, cell,
                     secu, predicted, delta)
                )

    print(f"blocks: {len(pairs)}  checks: {total}  exact: {exact}")
    print("delta histogram:", deltas.most_common(8))
    for row in mismatches:
        print(
            "MISMATCH actor=%s x=%d y=%d threat=%d health=%d/%d cell=%d "
            "logged=%d predicted=%d delta=%d" % row
        )
    return 0 if exact == total else 1


if __name__ == "__main__":
    sys.exit(main())
