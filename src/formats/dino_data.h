#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dino {

// Species/animation-family tables recovered from
// `Data/Design/Genotype/anim_trans.dic` (species and family order) and
// `simu_trans.dic` Part 10 (default family per base dinosaur).
struct DinoFamilyTable {
    std::vector<std::string> species;   // 20 base species == graphics folders
    std::vector<std::string> families;  // 5 animation family folders
    std::vector<int> default_family;    // one index per species

    int index_of(const std::string& name) const;
    std::string family_of(const std::string& name) const;
};

DinoFamilyTable read_dino_family_table(const std::string& anim_trans_path,
                                       const std::string& simu_trans_path);

// Dominant head-type gene (0..19) from a design `.gen` file. The 20 base
// `<species>.gen` files map one-to-one onto the 20 head types, which is how a
// saved 18-gene genotype selects its body mesh. Returns -1 on a parse error.
int read_gen_head_type(const std::string& path);

// Full 18-gene genotype from a design `.gen` file (allele A, allele B).
using DinoGenotype =
    std::array<std::pair<std::uint32_t, std::uint32_t>, 18>;
DinoGenotype read_gen_genotype(const std::string& path);

// Phenotype length tables used by the paddock size class (`saDinosaur+0x1e0`).
// `proportion.dat` supplies the five part ratios (head, arm, body, leg, tail)
// for the 20 base species; `simu_trans.dic` Part 2 supplies the Head/Tail merge
// ratios and Part 3 the Head/Body/Tail length tables. FUN_00619830 scales each
// part table value by the allele-blended proportion and normalises by the Body
// length; the final lengths are truncated (see notes/phenotype-length-runtime.md).
struct DinoPhenotypeTable {
    std::vector<std::array<float, 5>> proportions;
    std::array<std::array<int, 20>, 20> head_merge{};
    std::array<std::array<int, 20>, 20> tail_merge{};
    std::array<std::array<int, 20>, 20> head_length{};
    std::array<std::array<int, 20>, 20> body_length{};
    std::array<std::array<int, 20>, 20> tail_length{};
    bool valid = false;

    // Phenotype length in centimetres (0 when the table is invalid).
    int length_cm(const DinoGenotype& genotype) const;
    // Retail size class 0..3: <=200, <=620, <=900, else 3.
    int size_class(const DinoGenotype& genotype) const;
};

DinoPhenotypeTable read_dino_phenotype(const std::string& proportion_path,
                                       const std::string& simu_trans_path);

}  // namespace dino
