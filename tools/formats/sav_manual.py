"""Convert a free-game autosave into a manually loadable save.

The retail park block marks an autosave with ``header_a == 3`` and
``header_c == 0.0``; a manual save uses ``header_a == 0`` and
``header_c == 10.0``. Patching only those two fields makes a free-game
autosave loadable through the retail LOAD GAME panel while preserving the
rest of the file byte-for-byte (verified with ``--sav-roundtrip``).

Campaign autosaves populate the simulation lists and are not supported by
``sav_graph.py`` yet, so this tool rejects them with a clear error.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from sav import read_sav  # noqa: E402
from sav_graph import read_sav_graph  # noqa: E402


def manualize(source: Path) -> bytes:
    data = bytearray(source.read_bytes())
    save = read_sav(source)
    graph = read_sav_graph(source)
    if graph.park is None:
        raise ValueError("no park block")
    base = save.state_offset + graph.park.offset
    struct.pack_into("<I", data, base + 0x04, 0)
    struct.pack_into("<f", data, base + 0x0C, 10.0)
    return bytes(data)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args(argv)
    try:
        rebuilt = manualize(args.source)
    except Exception as exc:  # noqa: BLE001 - one clean CLI error
        print(f"sav manualize failed: {exc}", file=sys.stderr)
        return 2
    args.output.write_bytes(rebuilt)
    print(f"sav manualized: {args.source} -> {args.output} "
          f"({len(rebuilt)} bytes, header_a=0 header_c=10.0)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
