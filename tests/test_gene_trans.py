import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "formats"))

from gene_trans import GeneTransFormatError, read_gene_trans  # noqa: E402

DIC = ROOT / "game" / "%MAINDIR%" / "Data" / "Design" / "Genotype" / "simu_trans.dic"


@pytest.fixture(scope="module")
def tables():
    if not DIC.exists():
        pytest.skip("retail sample not present")
    return read_gene_trans(DIC)


def test_eighteen_genes(tables):
    assert list(tables) == [
        "Head", "ArmLeg", "Body", "Tail", "HDDi", "HDTy", "BDDi", "BDTy",
        "TDDi", "TDTy", "SknTyp", "SknSub", "CarTyp", "CarSub", "Diet",
        "Agre", "Stam", "Cour",
    ]
    assert [tables[c].allele_count for c in tables] == [
        20, 20, 20, 20, 48, 48, 48, 48, 48, 48, 8, 3, 6, 3, 2, 10, 10, 10,
    ]


def test_simple_genes_have_no_matrix(tables):
    for code in ("Head", "ArmLeg", "Body", "Tail"):
        table = tables[code]
        assert table.translation == []
        assert table.matrix is None
        assert len(table.rarities) == 20
        assert table.express(0, 1) is None


def test_dominant_min_rule(tables):
    for code in ("HDDi", "HDTy", "BDDi", "BDTy", "TDDi", "TDTy", "SknTyp", "CarTyp", "Diet"):
        table = tables[code]
        n = table.allele_count
        for a in range(n):
            for b in range(n):
                assert table.express(a, b) == table.express(min(a, b), min(a, b)), code


def test_average_rule(tables):
    for code in ("SknSub", "CarSub"):
        table = tables[code]
        n = table.allele_count
        for a in range(n):
            for b in range(n):
                assert table.express(a, b) == table.express((a + b) // 2, (a + b) // 2), code
    assert tables["SknSub"].express(0, 2) == 1
    assert tables["CarSub"].express(0, 1) == 0


def test_stat_values(tables):
    assert tables["Diet"].express(1, 1) == 2
    assert tables["Agre"].express(8, 7) == 80
    assert tables["Stam"].express(2, 4) == 40
    assert tables["Cour"].express(5, 6) == 70
    assert tables["SknTyp"].express(2, 5) == 2


def test_rarities(tables):
    assert tables["SknTyp"].rarities == [3, 3, 2, 2, 2, 2, 1, 1]
    assert tables["Agre"].rarities == [3, 3, 2, 2, 2, 2, 2, 2, 1, 1]


def test_rejects_missing_rarities(tmp_path):
    p = tmp_path / "bad.dic"
    p.write_text(
        "// Translation of the gene <X>\n0\n// Rarities of the alleles of the gene <X>\n2\n3\n",
        encoding="cp1252",
    )
    with pytest.raises(GeneTransFormatError):
        read_gene_trans(p)
