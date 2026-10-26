#pragma once

#include <string>
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

}  // namespace dino
