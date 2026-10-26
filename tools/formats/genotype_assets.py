#!/usr/bin/env python3
"""Readers for the two genotype graphic tables.

``Data/Design/Genotype/SkinCarapaceData.ini``
    The skin/carapace texture lists indexed by the `simu_trans.dic` Part 7/8
    maps:

    ```text
    DATA\\graphics\\TEXTURES\\actors\\elements_sup\\bonus\\peau\\
    22
    void
    bonus01.tga
    ...
    DATA\\graphics\\TEXTURES\\actors\\elements_sup\\bonus\\armure\\
    16
    void
    hurt.tga
    blood.tga
    armor_01_s.tga
    ...
    ```

    One texture per skin type (22); three per carapace type (16 x 3 =
    nomina/hurt/blood).

``Data/Design/Genotype/AdornmentData.ini``
    The dummy/adornment models:

    ```text
    49                       head/body/tail adornments, each an LOD pair
    horn1\\horn_l_01_lod1.mesh
    horn1\\horn_l_01_lod2.mesh
    ...
    4                        claw types
    4                        sizes per type
    claw1\\claw_xl_01.mesh
    ...
    ```

    All model paths are relative to ``Data/Graphics/Actors/elements_sup/``.

See ``docs/formats/genotype-assets.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path


class GenotypeAssetsError(ValueError):
    pass


@dataclass
class SkinCarapaceData:
    path: str
    skin_dir: str
    skins: list[str]
    carapace_dir: str
    carapaces: list[list[str]]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "skin_dir": self.skin_dir,
            "skins": self.skins,
            "carapace_dir": self.carapace_dir,
            "carapaces": self.carapaces,
        }


@dataclass
class AdornmentData:
    path: str
    pairs: list[tuple[str, str]]
    claw_types: int
    claw_sizes: int
    claws: list[list[str]]

    @property
    def meshes(self) -> list[str]:
        out = [name for pair in self.pairs for name in pair]
        out.extend(name for row in self.claws for name in row)
        return out

    def summary(self) -> dict:
        return {
            "path": self.path,
            "pairs": len(self.pairs),
            "claw_types": self.claw_types,
            "claw_sizes": self.claw_sizes,
            "claws": self.claws,
        }


def _tokens(path: Path) -> list[str]:
    out: list[str] = []
    for line in path.read_text(encoding="cp1252", errors="replace").splitlines():
        text = line.split("//", 1)[0].strip()
        if text:
            out.append(text)
    return out


def read_skin_carapace(path: Path) -> SkinCarapaceData:
    tokens = _tokens(path)
    index = 0
    skin_dir = tokens[index].rstrip("\\/")
    index += 1
    count = int(tokens[index])
    index += 1
    skins = tokens[index:index + count]
    index += count
    if len(skins) != count:
        raise GenotypeAssetsError(f"{path}: truncated skin list")
    carapace_dir = tokens[index].rstrip("\\/")
    index += 1
    count = int(tokens[index])
    index += 1
    flat = tokens[index:index + count * 3]
    index += count * 3
    if index != len(tokens):
        raise GenotypeAssetsError(f"{path}: {len(tokens) - index} trailing tokens")
    carapaces = [flat[i * 3:(i + 1) * 3] for i in range(count)]
    if any(len(row) != 3 for row in carapaces):
        raise GenotypeAssetsError(f"{path}: truncated carapace list")
    return SkinCarapaceData(
        path=str(path),
        skin_dir=skin_dir,
        skins=skins,
        carapace_dir=carapace_dir,
        carapaces=carapaces,
    )


def read_adornment(path: Path) -> AdornmentData:
    tokens = _tokens(path)
    index = 0
    pair_count = int(tokens[index])
    index += 1
    flat = tokens[index:index + pair_count * 2]
    index += pair_count * 2
    if len(flat) != pair_count * 2:
        raise GenotypeAssetsError(f"{path}: truncated adornment pairs")
    pairs = [(flat[i * 2], flat[i * 2 + 1]) for i in range(pair_count)]
    claw_types = int(tokens[index])
    index += 1
    claw_sizes = int(tokens[index])
    index += 1
    flat = tokens[index:index + claw_types * claw_sizes]
    index += claw_types * claw_sizes
    if index != len(tokens):
        raise GenotypeAssetsError(f"{path}: {len(tokens) - index} trailing tokens")
    claws = [
        flat[i * claw_sizes:(i + 1) * claw_sizes] for i in range(claw_types)
    ]
    return AdornmentData(
        path=str(path),
        pairs=pairs,
        claw_types=claw_types,
        claw_sizes=claw_sizes,
        claws=claws,
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path)
    args = ap.parse_args(argv)
    name = args.path.name.lower()
    if name == "skincarapacedata.ini":
        data = read_skin_carapace(args.path)
    elif name == "adornmentdata.ini":
        data = read_adornment(args.path)
    else:
        raise SystemExit(f"unknown file {args.path.name}")
    print(json.dumps(data.summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
