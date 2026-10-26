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

// Diet gene (14) expression: `Diet` matrix [[1,1],[1,2]] with translation
// 1 = herbivore, 2 = carnivore, so only a (1,1) pair is carnivorous.
bool genotype_is_carnivore(const DinoGenotype& genotype);

// One 20x20 allele-blend table (`table[alleleB][alleleA]`).
using DinoPartTable = std::array<std::array<int, 20>, 20>;

// Part 1 gene expression table: 10 expressed values and the 10x10 dominance
// matrix read by Agre/Stam/Cour (`matrix[alleleA][alleleB]`).
struct DinoGeneTable {
    std::array<int, 10> translation{};
    std::array<std::array<int, 10>, 10> matrix{};
};

// The runtime phenotype attributes read at the saDinosaur ctor
// (`FUN_00617250` + `FUN_00619830`). The dummy/skin/carapace accumulator
// (`0x619a60`/`0x619b80`) is not applied yet, so a few values carry the
// documented small residual; see docs/formats/translator-constants.md.
struct DinoAttributes {
    int length_cm = 0;   // +0x00
    int width_cm = 0;    // +0x0c
    int height_cm = 0;   // +0x14
    std::array<int, 5> part_length{};  // +0x98, Head/Arm/Leg/Body/Tail
    std::array<int, 5> part_width{};   // +0xac
    std::array<int, 5> part_height{};  // +0xc0

    int perception = 0;      // +0x38
    int intelligence = 0;    // +0x3c
    int beauty = 0;          // +0x40
    int combined_attack = 0; // +0x44
    int agility = 0;         // +0x48
    int armor = 0;           // +0x68
    int attack_head = 0;     // +0x4c
    int attack_body = 0;     // +0x50 (retail reads the head pair)
    int attack_arm = 0;      // +0x54
    int attack_leg = 0;      // +0x58 (zero for quadrupeds)
    int attack_tail = 0;     // +0x5c
    int attack_total = 0;    // +0x60
    int aggression = 0;      // +0x6c
    int stamina = 0;         // +0x70
    int courage = 0;         // +0x74

    int weight_kg = 0;        // +0x18
    int food_consumption = 0; // +0x1c
    float violence_evolution = 0.0f;  // +0x28
    float walk_speed = 0.0f;          // +0x2c
    float run_speed = 0.0f;           // +0x30
    int jump = 0;                     // +0x34
    int size_class = -1;              // +0x190
    int family = -1;                  // +0x1ac
    bool quadruped = true;            // +0x1b4
};

// Phenotype tables used by the paddock size class (`saDinosaur+0x1e0`) and
// the store/lab attribute gauges. `proportion.dat` supplies the five part
// ratios for the 20 base species; `simu_trans.dic` Parts 2/3/10 supply the
// merges, the per-part attribute tables and the default families;
// `TranslatorConstants.ini` supplies the coefficients. `FUN_00619830` scales
// each part table value by the allele-blended proportion (see
// notes/phenotype-length-runtime.md).
struct DinoPhenotypeTable {
    std::vector<std::array<float, 5>> proportions;
    DinoPartTable head_merge{};
    DinoPartTable arm_leg_merge{};
    DinoPartTable tail_merge{};
    std::array<DinoPartTable, 5> length{};
    std::array<DinoPartTable, 5> width{};
    std::array<DinoPartTable, 5> height{};
    std::array<DinoPartTable, 5> weight{};
    std::array<DinoPartTable, 5> speed{};
    std::array<DinoPartTable, 5> perception{};
    std::array<DinoPartTable, 5> intelligence{};
    std::array<DinoPartTable, 5> beauty{};
    std::array<DinoPartTable, 5> armor{};
    std::array<DinoPartTable, 5> agility{};
    std::array<DinoPartTable, 5> attack{};
    DinoGeneTable agre{};
    DinoGeneTable stam{};
    DinoGeneTable cour{};
    std::array<int, 20> default_family{};

    std::array<int, 4> size_category_max{200, 620, 900, 3200};
    std::array<int, 4> size_category_radius{0, 2, 4, 7};
    float food_consumption_rate = 0.1f;
    float violence_evolution_rate = 0.4f;
    float walking_speed_coef = 0.1f;
    float running_speed_coef = 0.4f;
    float min_motricity_to_run = 0.01f;
    float jump_percentage = 50.0f;
    float perception_height_percentage = 1.0f;
    float intelligence_multiplier = 25.0f;
    float intelligence_power = 0.22f;
    float motricity_power = 0.85f;

    bool valid = false;

    // Phenotype length in centimetres (0 when the table is invalid).
    int length_cm(const DinoGenotype& genotype) const;
    // Retail size class 0..3: <=200, <=620, <=900, else 3.
    int size_class(const DinoGenotype& genotype) const;
    // Full attribute set (`FUN_00619830` + `FUN_00617250`).
    DinoAttributes attributes(const DinoGenotype& genotype) const;
};

// `constants_path` may be empty; the documented retail values are then used.
DinoPhenotypeTable read_dino_phenotype(const std::string& proportion_path,
                                       const std::string& simu_trans_path,
                                       const std::string& constants_path = {});

}  // namespace dino
