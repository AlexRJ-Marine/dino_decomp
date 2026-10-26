#!/usr/bin/env python3
"""Reader for the mcLib render-state files in ``Data/States``.

Two layers:

* ``*.ini`` — **profiles** such as ``Standard.ini``, ``Texture.ini``,
  ``SkyBox.ini``, ``Shape2D`` variants. They set the pipeline-level states
  (fill, cull, z-buffer, alpha, fog, light) and pull in fragments with
  ``mgStateInclude <file>``.
* ``*.txt`` — **fragments** such as ``TextureParam.txt`` (per-texture-stage
  sampler/blend state), ``FogParam.txt`` and ``Misc.txt``.

Each line is ``mgState<Name> [stage] <value...>``; ``//`` starts a comment
and ``;`` is not special. Values are keywords, integers, floats, colours
(``r,g,b[,a]``), bit masks (``0x...``) or bit strings.

``Description.txt`` is the grammar reference (not a state file).

See ``docs/formats/render-states.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path

# States whose first value is a texture stage index.
STAGE_STATES = frozenset(
    {
        "mgStateTextureOp",
        "mgStateTextureArg1",
        "mgStateTextureArg2",
        "mgStateTextureArg3",
        "mgStateTextureAlphaOp",
        "mgStateTextureAlphaArg1",
        "mgStateTextureAlphaArg2",
        "mgStateTextureAlphaArg3",
        "mgStateTextureResultArg",
        "mgStateTextureCoordIndex",
        "mgStateTextureWrapU",
        "mgStateTextureWrapV",
        "mgStateTextureWrapW",
        "mgStateTextureBorderColor",
        "mgStateTextureMinFilter",
        "mgStateTextureMagFilter",
        "mgStateTextureMipFilter",
        "mgStateTextureMipMap",
        "mgStateTextureMipMapLODBias",
        "mgStateTextureMaxMipLevel",
        "mgStateTextureMaxAnisotropy",
        "mgStateTextureTransform",
        "mgStateTextureBumpEnvMat00",
        "mgStateTextureBumpEnvMat01",
        "mgStateTextureBumpEnvMat10",
        "mgStateTextureBumpEnvMat11",
        "mgStateTextureBumpEnvLScale",
        "mgStateTextureBumpEnvLOffset",
    }
)


class RenderStateError(ValueError):
    pass


@dataclass
class State:
    name: str
    args: list[str]
    source: str = ""
    line: int = 0

    @property
    def free(self) -> bool:
        """``Free`` releases the state instead of setting it."""
        return bool(self.args) and self.args[0].upper() == "FREE"

    @property
    def stage(self) -> int | None:
        if self.name in STAGE_STATES and self.args:
            try:
                return int(self.args[0])
            except ValueError:
                return None
        return None

    @property
    def value(self) -> list[str]:
        return self.args[1:] if self.stage is not None else self.args

    @property
    def text(self) -> str:
        return " ".join(self.value)

    def as_int(self) -> int:
        return int(self.text, 0)

    def as_float(self) -> float:
        return float(self.text)

    def as_bool(self) -> bool:
        return self.text.upper() in ("Y", "YES", "TRUE", "1")

    def as_color(self) -> tuple[int, ...]:
        return tuple(int(part) for part in self.text.split(","))


@dataclass
class RenderState:
    path: str
    states: list[State] = field(default_factory=list)
    includes: list[str] = field(default_factory=list)

    def all(self, name: str, stage: int | None = None) -> list[State]:
        return [
            state
            for state in self.states
            if state.name == name and (stage is None or state.stage == stage)
        ]

    def last(self, name: str, stage: int | None = None) -> State | None:
        found = self.all(name, stage)
        return found[-1] if found else None

    def summary(self) -> dict:
        return {
            "path": self.path,
            "includes": self.includes,
            "states": len(self.states),
            "names": sorted({state.name for state in self.states}),
        }


def _parse_lines(path: Path) -> tuple[list[State], list[str]]:
    states: list[State] = []
    includes: list[str] = []
    for lineno, raw in enumerate(
        path.read_text(encoding="cp1252", errors="replace").splitlines(), 1
    ):
        line = raw.split("//", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        name = parts[0]
        if not name.lower().startswith("mgstate"):
            raise RenderStateError(f"{path}:{lineno}: bad state name {name!r}")
        args = parts[1:]
        if name.lower() == "mgstateinclude":
            if not args:
                raise RenderStateError(f"{path}:{lineno}: include without a file")
            includes.append(args[0])
            continue
        states.append(State(name=name, args=args, source=str(path), line=lineno))
    return states, includes


def read_render_state(path: Path, *, expand: bool = True) -> RenderState:
    states, includes = _parse_lines(path)
    if expand:
        for name in includes:
            child = (path.parent / name).resolve()
            if not child.exists():
                raise RenderStateError(f"{path}: include {name!r} not found")
            states.extend(read_render_state(child).states)
    return RenderState(path=str(path), states=states, includes=includes)


def scan(directory: Path) -> list[RenderState]:
    """Read every .ini profile (the fragments are reached via includes)."""
    return [read_render_state(path) for path in sorted(directory.glob("*.ini"))]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", type=Path, help="a state file or the States directory")
    ap.add_argument("--no-expand", action="store_true")
    args = ap.parse_args(argv)
    if args.path.is_dir():
        data = [state.summary() for state in scan(args.path)]
    else:
        data = read_render_state(args.path, expand=not args.no_expand).summary()
    print(json.dumps(data, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
