import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from gen import GenFormatError, read_gen, scan  # noqa: E402

GAME = ROOT / "game" / "%MAINDIR%"
DESIGN = GAME / "Data" / "Design" / "Scenario" / "Genotype"
EDITION = GAME / "Data" / "Edition" / "Genotype"


def test_allo_genotype():
    if not (DESIGN / "allo.gen").exists():
        pytest.skip("retail sample not present")
    gen = read_gen(DESIGN / "allo.gen")
    assert gen.gene_count == 18
    assert gen.documented
    assert gen.alleles[14] == (1, 1)  # diet
    assert gen.alleles[15] == (8, 7)  # aggressiveness
    assert gen.alleles[16] == (2, 4)  # stamina
    assert gen.alleles[17] == (5, 6)  # courage


def test_bare_variant():
    if not (DESIGN / "allo2.gen").exists():
        pytest.skip("retail sample not present")
    gen = read_gen(DESIGN / "allo2.gen")
    assert gen.gene_count == 18
    assert not gen.documented
    assert len(gen.alleles) == 18


def test_scan_design_genotypes():
    if not DESIGN.exists():
        pytest.skip("retail game tree not present")
    total, bad, sums = scan(DESIGN)
    assert total == 57
    assert not bad, bad[:5]
    assert sums["genes"] == 57 * 18
    assert sums["documented"] == 20


def test_edition_genotypes_are_27_genes():
    if not EDITION.exists():
        pytest.skip("editor tree not present")
    total, bad, sums = scan(EDITION)
    assert total == 20
    assert not bad, bad[:5]
    assert sums["genes"] == 20 * 27


def test_rejects_truncated(tmp_path):
    p = tmp_path / "bad.gen"
    p.write_text("18\n0 0\n", encoding="cp1252")
    with pytest.raises(GenFormatError):
        read_gen(p)

def test_edition_files_are_sparse_27_gene():
    """Only slots 0-9 and 18 are used in the 20 edition files."""
    root = Path(__file__).resolve().parents[1]
    edition = root / "game" / "%MAINDIR%" / "Data" / "Edition" / "Genotype"
    if not edition.exists():
        pytest.skip("retail sample not present")
    files = sorted(edition.glob("*.gen"))
    assert len(files) == 20
    for path in files:
        data = read_gen(path)
        assert data.gene_count == 27
        for index, (a, b) in enumerate(data.alleles):
            if a or b:
                assert index <= 9 or index == 18, (path.name, index)
        a, b, c, d = (data.alleles[i] for i in range(4))
        assert a == b == c == d, path.name
    kentro = read_gen(edition / "kentro.gen")
    assert kentro.alleles[4:10] == [(3, 3), (46, 46)] * 3
    stego = read_gen(edition / "stego.gen")
    assert stego.alleles[4:10] == [(4, 4), (47, 47)] * 3
