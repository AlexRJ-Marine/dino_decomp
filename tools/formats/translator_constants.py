#!/usr/bin/env python3
"""Reader for ``TranslatorConstants.ini``.

The file has one ``[SIMU_CONSTANTS]`` section and 41 keys used by
``saSimuGenotypeTranslator`` (``0x00615e80``) to turn a genotype into the
dinosaur phenotype and its visitor needs.

The loader stores the values in the translator object at these offsets:

    +0x228 MaxQuadrupedRange          +0x264 FoodConsumptionRate
    +0x22c MinHeightForHeavyBiped     +0x268 ViolenceEvolutionRate
    +0x230 MinWeightForHeavyBiped     +0x26c WalkingSpeedCoef
    +0x244 SizeCategory1MaxLength     +0x270 RunningSpeedCoef
    +0x248 SizeCategory2MaxLength     +0x274 MinMotricityToRun
    +0x24c SizeCategory3MaxLength     +0x278 JumpPercentage
    +0x250 SizeCategory4MaxLength     +0x27c PerceptionHeightPercentage
    +0x254 SizeCategory1Radius        +0x280 VisibilityDivider
    +0x258 SizeCategory2Radius        +0x284 DangerVisibilityDivider
    +0x25c SizeCategory3Radius        +0x288 IntelligenceMultiplier
    +0x260 SizeCategory4Radius        +0x28c HitPointsDivider
                                      +0x290 RegenerationDivider
                                      +0x294 PhysicalBonusOnStamina
                                      +0x298 ConventionalLimbDisparityFactor
                                      +0x29c ConventionalLimbHomogeneityFactor
                                      +0x2a0 ConventionalNeedFactor
                                      +0x2a4 DangerJawFactor
                                      +0x2a8 DangerCombinedAttackFactor
                                      +0x2ac DangerAggressivityFactor
                                      +0x2b0 CuteNeedBeautyFactor
                                      +0x2b8 TamingNeedIntelligenceFactor
                                      +0x2bc TamingNeedAgressivityFactor
                                      +0x2c0 SpectacularNeedSizeFactor
                                      +0x2c4 MaxDinosaurSize
                                      +0x2c8 RareAlleleMarketPrice
                                      +0x2cc UncommonAlleleMarketPrice
                                      +0x2d0 CommonAlleleMarketPrice
                                      +0x2d4 MotricityPower
                                      +0x2d8 IntelligencePower
                                      +0x2dc MaxPerception

``+0x2b4`` is a 4-byte gap in the object (not loaded from the file).

See ``docs/formats/translator-constants.md``.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field
from pathlib import Path


class TranslatorConstantsError(ValueError):
    pass


SECTION = "SIMU_CONSTANTS"

# Key -> translator-object offset (byte).
STRUCT_OFFSETS: dict[str, int] = {
    "MaxQuadrupedRange": 0x228,
    "MinHeightForHeavyBiped": 0x22C,
    "MinWeightForHeavyBiped": 0x230,
    "SizeCategory1MaxLength": 0x244,
    "SizeCategory2MaxLength": 0x248,
    "SizeCategory3MaxLength": 0x24C,
    "SizeCategory4MaxLength": 0x250,
    "SizeCategory1Radius": 0x254,
    "SizeCategory2Radius": 0x258,
    "SizeCategory3Radius": 0x25C,
    "SizeCategory4Radius": 0x260,
    "FoodConsumptionRate": 0x264,
    "ViolenceEvolutionRate": 0x268,
    "WalkingSpeedCoef": 0x26C,
    "RunningSpeedCoef": 0x270,
    "MinMotricityToRun": 0x274,
    "JumpPercentage": 0x278,
    "PerceptionHeightPercentage": 0x27C,
    "VisibilityDivider": 0x280,
    "DangerVisibilityDivider": 0x284,
    "IntelligenceMultiplier": 0x288,
    "HitPointsDivider": 0x28C,
    "RegenerationDivider": 0x290,
    "PhysicalBonusOnStamina": 0x294,
    "ConventionalLimbDisparityFactor": 0x298,
    "ConventionalLimbHomogeneityFactor": 0x29C,
    "ConventionalNeedFactor": 0x2A0,
    "DangerJawFactor": 0x2A4,
    "DangerCombinedAttackFactor": 0x2A8,
    "DangerAggressivityFactor": 0x2AC,
    "CuteNeedBeautyFactor": 0x2B0,
    "TamingNeedIntelligenceFactor": 0x2B8,
    "TamingNeedAgressivityFactor": 0x2BC,
    "SpectacularNeedSizeFactor": 0x2C0,
    "MaxDinosaurSize": 0x2C4,
    "RareAlleleMarketPrice": 0x2C8,
    "UncommonAlleleMarketPrice": 0x2CC,
    "CommonAlleleMarketPrice": 0x2D0,
    "MotricityPower": 0x2D4,
    "IntelligencePower": 0x2D8,
    "MaxPerception": 0x2DC,
}


@dataclass
class TranslatorConstants:
    path: str
    section: str
    raw: dict[str, str] = field(default_factory=dict)
    values: dict[str, float] = field(default_factory=dict)

    def __getitem__(self, key: str) -> float:
        return self.values[key]

    def summary(self) -> dict:
        return {
            "path": self.path,
            "section": self.section,
            "keys": len(self.values),
            "values": {k: round(v, 6) for k, v in self.values.items()},
        }


def read_translator_constants(path: Path) -> TranslatorConstants:
    section = ""
    raw: dict[str, str] = {}
    values: dict[str, float] = {}
    for number, line in enumerate(
        path.read_text(encoding="cp1252").splitlines(), start=1
    ):
        text = line.strip()
        if not text or text.startswith(";"):
            continue
        if text.startswith("["):
            if not text.endswith("]"):
                raise TranslatorConstantsError(f"{path}:{number}: bad section {text!r}")
            section = text[1:-1]
            continue
        if "=" not in text:
            raise TranslatorConstantsError(f"{path}:{number}: expected key=value")
        key, value = (part.strip() for part in text.split("=", 1))
        if section != SECTION:
            raise TranslatorConstantsError(
                f"{path}:{number}: key outside [{SECTION}]"
            )
        try:
            number_value = float(value)
        except ValueError as e:
            raise TranslatorConstantsError(
                f"{path}:{number}: bad number {value!r}"
            ) from e
        raw[key] = value
        values[key] = number_value
    missing = sorted(set(STRUCT_OFFSETS) - set(values))
    extra = sorted(set(values) - set(STRUCT_OFFSETS))
    if missing or extra:
        raise TranslatorConstantsError(f"{path}: missing={missing} extra={extra}")
    return TranslatorConstants(
        path=str(path), section=section, raw=raw, values=values
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", type=Path, required=True)
    args = ap.parse_args(argv)
    print(json.dumps(read_translator_constants(args.dump).summary(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
