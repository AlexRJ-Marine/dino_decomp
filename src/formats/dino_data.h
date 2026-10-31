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
// matrix read by Agre/Stam/Cour (`matrix[alleleB][alleleA]`).
struct DinoGeneTable {
    std::array<int, 10> translation{};
    std::array<std::array<int, 10>, 10> matrix{};
};

// Part 1 gene block with an arbitrary allele count: the dummy position/type
// genes (48), skin type (8), skin subtype (3), carapace type (6) and subtype
// (3). Two expression rules are documented in docs/formats/gen.md:
// - dummy position/type and skin/carapace type: `translation[min(a, b)]`
//   (the lower allele is dominant);
// - skin/carapace subtype: `matrix[b][a]`.
struct DinoAlleleTable {
    int allele_count = 0;
    std::vector<int> translation;
    std::vector<int> matrix;  // allele_count^2, row-major; empty when absent

    int dominant(std::uint32_t a, std::uint32_t b) const;
    int expressed(std::uint32_t a, std::uint32_t b) const;
};

// The runtime phenotype attributes read at the saDinosaur ctor
// (`FUN_00617250` + `FUN_00619830`), including the dummy/skin/carapace
// accumulator (`0x619a60`/`0x619b80`) and the Part 9 clamp. All 20 Cpg04
// rows match the live ctor values; see docs/formats/translator-constants.md.
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
    int stamina = 0;         // +0x70 (post-accumulator)
    int courage = 0;         // +0x74
    // `def+0x188` (`0x617dfb`): max rest = gene-expression stamina + the
    // raw Body/Tail weight percentage bonus. This is the `+0x1d8` max used
    // by the rest gauge `FUN_00571540`.
    float max_rest = 0.0f;
    // `def+0x184`/`+0x186` (`0x617d47..0x617df4`):
    //   max_health = sqrt(weight) + 0.5*cbrt(length*width*height)
    //   regen      = max_health * stamina / RegenerationDivider + 1
    int max_health = 0;
    int regen = 0;
    // `def+0x19c` (`0x61766b..0x6176d4`): sum over the 18 genes of
    // `price[rarity(alleleA)-1] + price[rarity(alleleB)-1]`, with
    // price = {RareAlleleMarketPrice, Uncommon, Common}.
    int market_value = 0;
    // The five expressed part allele pairs (Head/Arm/Leg/Body/Tail); kept
    // for the runtime phenotype report and the part-gated formulas.
    std::array<int, 5> part_allele_a{};
    std::array<int, 5> part_allele_b{};
    // Raw (unscaled) Part 3 Weight table cells, for the rest-gauge bonus
    // (`0x6179d6`) and the runtime phenotype report.
    std::array<int, 5> weight_part_raw{};

    int weight_kg = 0;        // +0x18
    float food_consumption = 0.0f;  // +0x1c
    float violence_evolution = 0.0f;  // +0x28
    float walk_speed = 0.0f;          // +0x2c
    float run_speed = 0.0f;           // +0x30
    int jump = 0;                     // +0x34
    int obstacle_size = 0;            // +0x78 (GetObstacleSize)
    int obstacle_size_sq = 0;         // +0x7c
    int obstacle_radius = 0;          // +0x80 (danger-grid stamp radius)
    int conventional_appeal = 0;      // +0x194 (conventional need input)
    int originality = 0;              // store ORIGINALITY/Strangeness gauge
    int ornament_count = 0;           // non-empty slots in the 23-slot +0xE8 map
    int size_class = -1;              // +0x190
    // `+0x10` = `SizeCategory*Radius` (TranslatorConstants.ini):
    // 0 / 2 / 4 / 7 for size class 0..3. Retail copies it to `actor+0x60`;
    // the actor-target/eat radius modes read it.
    int size_radius = 0;              // +0x10 (SizeCategoryNRadius)
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

    // Part 1 allele tables for the dummy/skin/carapace genes. Gene order is
    // the docs/formats/gen.md order: 4 HDDi, 5 HDTy, 6 BDDi, 7 BDTy,
    // 8 TDDi, 9 TDTy, 10 SknTyp, 11 SknSub, 12 CarTyp, 13 CarSub.
    DinoAlleleTable hd_di{};
    DinoAlleleTable hd_ty{};
    DinoAlleleTable bd_di{};
    DinoAlleleTable bd_ty{};
    DinoAlleleTable td_di{};
    DinoAlleleTable td_ty{};
    DinoAlleleTable skin_type{};
    DinoAlleleTable skin_sub{};
    DinoAlleleTable carapace_type{};
    DinoAlleleTable carapace_sub{};

    // Part 4: three dummy subparts (Head/Body/Tail) x 48 positions x 6 slots.
    std::array<std::array<std::array<int, 6>, 48>, 3> dummy_disposition{};
    // Part 5: three subparts x 48 types x 5 slot type ids.
    std::array<std::array<std::array<int, 5>, 48>, 3> dummy_type{};
    // Part 6: two 50 x 18 influence tables (bonus and percentage).
    std::array<std::array<std::array<int, 18>, 50>, 2> dummy_influence{};
    std::array<int, 5> reference_width{100, 100, 100, 300, 150};
    // Part 7: skin map (8 x 3), the 582 body-length divisor and two 22 x 18
    // influence tables.
    std::array<std::array<int, 3>, 8> skin_map{};
    float skin_reference = 582.0f;
    std::array<std::vector<std::array<int, 18>>, 2> skin_influence{};
    // Part 8: carapace map (6 x 3) and two 16 x 18 influence tables.
    std::array<std::array<int, 3>, 6> carapace_map{};
    std::array<std::vector<std::array<int, 18>>, 2> carapace_influence{};
    // Part 9 min/max, used by the final `FUN_00619fb0` clamp.
    std::array<int, 20> min_attributes{};
    std::array<int, 20> max_attributes{};
    bool has_attribute_limits = false;

    // Part 1 rarity weights: one vector per gene (18), values 1..3 with
    // 1 = rare. `0x61766b` indexes it by allele, then the 3-entry price
    // table below.
    std::array<std::vector<int>, 18> rarity;
    std::array<int, 3> allele_price{500, 100, 50};  // Rare, Uncommon, Common
    std::array<int, 4> size_category_max{200, 620, 900, 3200};
    std::array<int, 4> size_category_radius{0, 2, 4, 7};
    float food_consumption_rate = 0.1f;
    float violence_evolution_rate = 0.4f;
    int physical_bonus_on_stamina = 100;  // translator +0x294
    int regeneration_divider = 1000;      // translator +0x290
    float walking_speed_coef = 0.1f;
    float running_speed_coef = 0.4f;
    float min_motricity_to_run = 0.01f;
    float jump_percentage = 50.0f;
    float perception_height_percentage = 1.0f;
    float intelligence_multiplier = 25.0f;
    float intelligence_power = 0.22f;
    float motricity_power = 0.85f;
    int visibility_divider = 10;
    int danger_visibility_divider = 40;
    int max_perception = 90;
    int conventional_limb_disparity_factor = 40;
    int conventional_limb_homogeneity_factor = 12;
    // `FUN_00618020` five show needs (TranslatorConstants.ini keys).
    float cute_need_beauty_factor = 0.6f;
    float danger_jaw_factor = 0.15f;
    float danger_aggressivity_factor = 0.15f;
    float danger_combined_attack_factor = 0.075f;
    float spectacular_need_size_factor = 60.0f;
    float max_dinosaur_size = 1346.0f;
    float conventional_need_factor = 0.5f;
    float taming_need_intelligence_factor = 0.45f;
    float taming_need_aggressivity_factor = 0.15f;

    bool valid = false;

    // Phenotype length in centimetres (0 when the table is invalid).
    int length_cm(const DinoGenotype& genotype) const;
    // Retail size class 0..3: <=200, <=620, <=900, else 3.
    int size_class(const DinoGenotype& genotype) const;
    // Full attribute set (`FUN_00619830` + `FUN_00617250`).
    DinoAttributes attributes(const DinoGenotype& genotype) const;
};

// `FUN_00618020`: the five floats a show judge feeds into the visitor
// preference dot product, in order: beauty, danger, spectacle, conventional
// appeal, tamability.
std::array<float, 5> dino_show_needs(const DinoAttributes& attributes,
                                     const DinoPhenotypeTable& table);

// `constants_path` may be empty; the documented retail values are then used.
DinoPhenotypeTable read_dino_phenotype(const std::string& proportion_path,
                                       const std::string& simu_trans_path,
                                       const std::string& constants_path = {});

}  // namespace dino
