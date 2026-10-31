#include "dino_data.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace dino {
namespace {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::vector<std::string> read_lines(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open dino data: " + path);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

int parse_int(const std::string& text, const std::string& path) {
    char* end = nullptr;
    // Part 6-8 store negative influence values as their uint32 decimal form
    // (e.g. 4294967293 for -3), so parse into 64 bits and wrap to int32.
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || (end != nullptr && *end != '\0')) {
        throw std::runtime_error("dino data: expected integer in " + path +
                                 ": " + text);
    }
    return static_cast<int>(static_cast<std::uint32_t>(value));
}

std::size_t find_section(const std::vector<std::string>& lines,
                         const std::string& marker, const std::string& path) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find(marker) != std::string::npos) {
            return i;
        }
    }
    throw std::runtime_error("dino data: missing section '" + marker + "' in " +
                             path);
}

// Reads `count` whitespace-separated integers starting at line `start`.
std::vector<int> read_int_tokens(const std::vector<std::string>& lines,
                                 std::size_t start, std::size_t count,
                                 const std::string& path) {
    std::vector<int> values;
    values.reserve(count);
    for (std::size_t i = start; i < lines.size() && values.size() < count;
         ++i) {
        const char* cursor = lines[i].c_str();
        while (*cursor != '\0' && values.size() < count) {
            char* next = nullptr;
            const long value = std::strtol(cursor, &next, 10);
            if (next == cursor) {
                ++cursor;
                continue;
            }
            values.push_back(static_cast<int>(value));
            cursor = next;
        }
    }
    if (values.size() != count) {
        throw std::runtime_error("dino data: truncated table in " + path);
    }
    return values;
}

// A Part 2/3 table: count (20), 20 diagonal values, then a 20x20 matrix.
void read_merge_block(const std::vector<std::string>& lines,
                      const std::string& marker, const std::string& path,
                      DinoPartTable& out) {
    const std::size_t at = find_section(lines, marker, path) + 1;
    const int expected = 20;
    const std::vector<int> tokens =
        read_int_tokens(lines, at, 1 + expected + expected * expected, path);
    if (tokens[0] != expected) {
        throw std::runtime_error("dino data: bad table count for '" + marker +
                                 "' in " + path);
    }
    std::size_t cursor = 1 + static_cast<std::size_t>(expected);
    for (int row = 0; row < expected; ++row) {
        for (int col = 0; col < expected; ++col) {
            out[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] =
                tokens[cursor++];
        }
    }
}

// Part 3 holds eleven attribute groups x five body parts, each a 20x20 table.
void read_part_group(const std::vector<std::string>& lines,
                     const std::string& attribute, const std::string& path,
                     std::array<DinoPartTable, 5>& out) {
    static const char* kParts[5] = {"Head", "Arm", "Leg", "Body", "Tail"};
    for (int part = 0; part < 5; ++part) {
        read_merge_block(lines,
                         std::string(kParts[part]) + " : " + attribute, path,
                         out[static_cast<std::size_t>(part)]);
    }
}

// Part 1: `// Translation of the gene <Code>`, then a count, that many
// expressed values and a count x count dominance matrix. Agre/Stam/Cour use
// `matrix[alleleB][alleleA]` (the averaged translation is the matrix's own
// content; see docs/formats/gen.md).
DinoGeneTable read_gene_table(const std::vector<std::string>& lines,
                              const std::string& code,
                              const std::string& path) {
    const std::string marker = "// Translation of the gene <" + code + ">";
    const std::size_t at = find_section(lines, marker, path) + 1;
    if (at >= lines.size()) {
        throw std::runtime_error("dino data: truncated gene " + code);
    }
    const int count = parse_int(lines[at], path);
    if (count != 10) {
        throw std::runtime_error("dino data: gene " + code +
                                 " translation count is not 10");
    }
    const std::vector<int> values =
        read_int_tokens(lines, at + 1, 10 + 100, path);
    DinoGeneTable out;
    for (int i = 0; i < 10; ++i) {
        out.translation[static_cast<std::size_t>(i)] =
            values[static_cast<std::size_t>(i)];
    }
    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 10; ++col) {
            out.matrix[static_cast<std::size_t>(row)]
                      [static_cast<std::size_t>(col)] =
                values[static_cast<std::size_t>(10 + row * 10 + col)];
        }
    }
    return out;
}

// Part 1 rarity block: `// Rarities of the alleles of the gene <Code>`,
// a count, then one weight (1 rare .. 3 common) per allele.
std::vector<int> read_rarity_table(const std::vector<std::string>& lines,
                                   const std::string& code,
                                   const std::string& path) {
    const std::string marker =
        "// Rarities of the alleles of the gene <" + code + ">";
    const std::size_t at = find_section(lines, marker, path) + 1;
    if (at >= lines.size()) {
        throw std::runtime_error("dino data: truncated rarity " + code);
    }
    const int count = parse_int(lines[at], path);
    if (count <= 0 || count > 64) {
        throw std::runtime_error("dino data: rarity count in " + code);
    }
    std::vector<int> out = read_int_tokens(
        lines, at + 1, static_cast<std::size_t>(count), path);
    for (const int value : out) {
        if (value < 1 || value > 3) {
            throw std::runtime_error("dino data: rarity weight in " + code);
        }
    }
    return out;
}

// Part 1 block for a non-10-allele gene (dummy, skin, carapace).
DinoAlleleTable read_allele_table(const std::vector<std::string>& lines,
                                  const std::string& code,
                                  const std::string& path) {
    const std::string marker = "// Translation of the gene <" + code + ">";
    const std::size_t at = find_section(lines, marker, path) + 1;
    if (at >= lines.size()) {
        throw std::runtime_error("dino data: truncated gene " + code);
    }
    const int count = parse_int(lines[at], path);
    if (count <= 0 || count > 64) {
        throw std::runtime_error("dino data: unsupported allele count in " +
                                 code);
    }
    const std::vector<int> values =
        read_int_tokens(lines, at + 1,
                        static_cast<std::size_t>(count) +
                            static_cast<std::size_t>(count) * count,
                        path);
    DinoAlleleTable out;
    out.allele_count = count;
    out.translation.assign(values.begin(),
                           values.begin() + static_cast<std::size_t>(count));
    out.matrix.assign(values.begin() + static_cast<std::size_t>(count),
                      values.end());
    return out;
}

// Splits the lines in [start, end) into whitespace-separated tokens,
// discarding `//` comments. Used for the counted `N:`/`W:` tables in Parts
// 4-9.
std::vector<std::string> read_tokens(const std::vector<std::string>& lines,
                                     std::size_t start, std::size_t end) {
    std::vector<std::string> tokens;
    for (std::size_t i = start; i < end && i < lines.size(); ++i) {
        std::string line = lines[i];
        const std::size_t comment = line.find("//");
        if (comment != std::string::npos) {
            line.resize(comment);
        }
        std::size_t cursor = 0;
        while (cursor < line.size()) {
            while (cursor < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[cursor]))) {
                ++cursor;
            }
            std::size_t next = cursor;
            while (next < line.size() &&
                   !std::isspace(static_cast<unsigned char>(line[next]))) {
                ++next;
            }
            if (next > cursor) {
                tokens.push_back(line.substr(cursor, next - cursor));
            }
            cursor = next;
        }
    }
    return tokens;
}

// `50:` -> 50. The counted tables in Part 6-8 put the count before a colon.
int parse_count(const std::string& token, const std::string& path) {
    std::string text = token;
    if (!text.empty() && text.back() == ':') {
        text.pop_back();
    }
    return parse_int(text, path);
}

// Reads `count:` followed by `count` blocks of `width:` + width ints.
std::vector<std::vector<int>> read_counted_blocks(
    const std::vector<std::string>& tokens, std::size_t& at,
    const std::string& path) {
    if (at >= tokens.size()) {
        throw std::runtime_error("dino data: truncated counted block in " +
                                 path);
    }
    const int count = parse_count(tokens[at++], path);
    if (count < 0 || count > 512) {
        throw std::runtime_error("dino data: bad counted block size in " +
                                 path);
    }
    std::vector<std::vector<int>> rows;
    rows.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        if (at >= tokens.size()) {
            throw std::runtime_error("dino data: truncated counted row in " +
                                     path);
        }
        const int width = parse_count(tokens[at++], path);
        if (width < 0 || width > 256) {
            throw std::runtime_error("dino data: bad counted row width in " +
                                     path);
        }
        std::vector<int> row;
        row.reserve(static_cast<std::size_t>(width));
        while (static_cast<int>(row.size()) < width) {
            if (at >= tokens.size()) {
                throw std::runtime_error("dino data: truncated counted value in " +
                                         path);
            }
            row.push_back(parse_int(tokens[at++], path));
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

// Part 4 (`width` 6) and Part 5 (`width` 5): three subparts, each a `48`
// count and 48 rows.
template <std::size_t Width>
std::array<std::array<std::array<int, Width>, 48>, 3> read_dummy_rows(
    const std::vector<std::string>& tokens, std::size_t& at,
    const std::string& path) {
    std::array<std::array<std::array<int, Width>, 48>, 3> out{};
    for (int part = 0; part < 3; ++part) {
        if (at >= tokens.size()) {
            throw std::runtime_error("dino data: truncated dummy table");
        }
        const int count = parse_int(tokens[at++], path);
        if (count != 48) {
            throw std::runtime_error("dino data: dummy table is not 48 rows");
        }
        for (int row = 0; row < 48; ++row) {
            for (std::size_t col = 0; col < Width; ++col) {
                if (at >= tokens.size()) {
                    throw std::runtime_error(
                        "dino data: truncated dummy table row in " + path);
                }
                out[static_cast<std::size_t>(part)]
                   [static_cast<std::size_t>(row)][col] =
                    parse_int(tokens[at++], path);
            }
        }
    }
    return out;
}

float parse_float(const std::string& text) {
    return static_cast<float>(std::atof(text.c_str()));
}

// TranslatorConstants.ini `[SIMU_CONSTANTS]`; missing keys keep the
// documented retail default.
void read_translator_constants(const std::string& path,
                               DinoPhenotypeTable& table) {
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t equal = line.find('=');
        if (equal == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, equal));
        const std::string value = trim(line.substr(equal + 1));
        if (key == "FoodConsumptionRate") {
            table.food_consumption_rate = parse_float(value);
        } else if (key == "ViolenceEvolutionRate") {
            table.violence_evolution_rate = parse_float(value);
        } else if (key == "PhysicalBonusOnStamina") {
            table.physical_bonus_on_stamina = parse_int(value, path);
        } else if (key == "RareAlleleMarketPrice") {
            table.allele_price[0] = parse_int(value, path);
        } else if (key == "UncommonAlleleMarketPrice") {
            table.allele_price[1] = parse_int(value, path);
        } else if (key == "CommonAlleleMarketPrice") {
            table.allele_price[2] = parse_int(value, path);
        } else if (key == "RegenerationDivider") {
            table.regeneration_divider = parse_int(value, path);
        } else if (key == "WalkingSpeedCoef") {
            table.walking_speed_coef = parse_float(value);
        } else if (key == "RunningSpeedCoef") {
            table.running_speed_coef = parse_float(value);
        } else if (key == "MinMotricityToRun") {
            table.min_motricity_to_run = parse_float(value);
        } else if (key == "JumpPercentage") {
            table.jump_percentage = parse_float(value);
        } else if (key == "PerceptionHeightPercentage") {
            table.perception_height_percentage = parse_float(value);
        } else if (key == "IntelligenceMultiplier") {
            table.intelligence_multiplier = parse_float(value);
        } else if (key == "IntelligencePower") {
            table.intelligence_power = parse_float(value);
        } else if (key == "MotricityPower") {
            table.motricity_power = parse_float(value);
        } else if (key == "VisibilityDivider") {
            table.visibility_divider = parse_int(value, path);
        } else if (key == "DangerVisibilityDivider") {
            table.danger_visibility_divider = parse_int(value, path);
        } else if (key == "MaxPerception") {
            table.max_perception = parse_int(value, path);
        } else if (key == "ConventionalLimbDisparityFactor") {
            table.conventional_limb_disparity_factor =
                parse_int(value, path);
        } else if (key == "ConventionalLimbHomogeneityFactor") {
            table.conventional_limb_homogeneity_factor =
                parse_int(value, path);
        } else if (key == "CuteNeedBeautyFactor") {
            table.cute_need_beauty_factor = parse_float(value);
        } else if (key == "DangerJawFactor") {
            table.danger_jaw_factor = parse_float(value);
        } else if (key == "DangerAggressivityFactor") {
            table.danger_aggressivity_factor = parse_float(value);
        } else if (key == "DangerCombinedAttackFactor") {
            table.danger_combined_attack_factor = parse_float(value);
        } else if (key == "SpectacularNeedSizeFactor") {
            table.spectacular_need_size_factor = parse_float(value);
        } else if (key == "MaxDinosaurSize") {
            table.max_dinosaur_size = parse_float(value);
        } else if (key == "ConventionalNeedFactor") {
            table.conventional_need_factor = parse_float(value);
        } else if (key == "TamingNeedIntelligenceFactor") {
            table.taming_need_intelligence_factor = parse_float(value);
        } else if (key == "TamingNeedAgressivityFactor") {
            table.taming_need_aggressivity_factor = parse_float(value);
        } else if (key == "SizeCategory1MaxLength") {
            table.size_category_max[0] = parse_int(value, path);
        } else if (key == "SizeCategory2MaxLength") {
            table.size_category_max[1] = parse_int(value, path);
        } else if (key == "SizeCategory3MaxLength") {
            table.size_category_max[2] = parse_int(value, path);
        } else if (key == "SizeCategory4MaxLength") {
            table.size_category_max[3] = parse_int(value, path);
        } else if (key == "SizeCategory1Radius") {
            table.size_category_radius[0] = parse_int(value, path);
        } else if (key == "SizeCategory2Radius") {
            table.size_category_radius[1] = parse_int(value, path);
        } else if (key == "SizeCategory3Radius") {
            table.size_category_radius[2] = parse_int(value, path);
        } else if (key == "SizeCategory4Radius") {
            table.size_category_radius[3] = parse_int(value, path);
        }
    }
}

}  // namespace

int DinoAlleleTable::dominant(std::uint32_t a, std::uint32_t b) const {
    const std::uint32_t index = std::min(a, b);
    if (index < translation.size()) {
        return translation[index];
    }
    return 0;
}

int DinoAlleleTable::expressed(std::uint32_t a, std::uint32_t b) const {
    const std::size_t count = static_cast<std::size_t>(allele_count);
    if (matrix.size() < count * count || a >= count || b >= count) {
        return 0;
    }
    return matrix[static_cast<std::size_t>(b) * count + a];
}

int DinoFamilyTable::index_of(const std::string& name) const {
    for (std::size_t i = 0; i < species.size(); ++i) {
        if (species[i] == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string DinoFamilyTable::family_of(const std::string& name) const {
    const int index = index_of(name);
    if (index < 0 || index >= static_cast<int>(default_family.size())) {
        return std::string();
    }
    const int family = default_family[static_cast<std::size_t>(index)];
    if (family < 0 || family >= static_cast<int>(families.size())) {
        return std::string();
    }
    return families[static_cast<std::size_t>(family)];
}

DinoFamilyTable read_dino_family_table(const std::string& anim_trans_path,
                                       const std::string& simu_trans_path) {
    const std::vector<std::string> lines = read_lines(anim_trans_path);
    DinoFamilyTable table;

    std::size_t at = find_section(lines, "// Dinosaurs", anim_trans_path) + 1;
    if (at >= lines.size()) {
        throw std::runtime_error("dino data: truncated dinosaur list");
    }
    const int species_count = parse_int(lines[at++], anim_trans_path);
    if (species_count <= 0 || species_count > 100) {
        throw std::runtime_error("dino data: bad dinosaur count");
    }
    for (int i = 0; i < species_count && at < lines.size(); ++i) {
        table.species.push_back(lines[at++]);
    }

    at = find_section(lines, "// Families", anim_trans_path) + 1;
    if (at >= lines.size()) {
        throw std::runtime_error("dino data: truncated family list");
    }
    const int family_count = parse_int(lines[at++], anim_trans_path);
    if (family_count <= 0 || family_count > 32) {
        throw std::runtime_error("dino data: bad family count");
    }
    for (int i = 0; i < family_count && at < lines.size(); ++i) {
        table.families.push_back(lines[at++]);
    }

    // Part 10 is the final block: a count followed by one family index per
    // base dinosaur, in the same order as the dinosaur list above.
    const std::vector<std::string> simu = read_lines(simu_trans_path);
    const std::size_t needed =
        static_cast<std::size_t>(species_count) + 1;
    if (simu.size() < needed) {
        throw std::runtime_error("dino data: truncated Part 10");
    }
    const std::size_t base = simu.size() - needed;
    const int part_count = parse_int(simu[base], simu_trans_path);
    if (part_count != species_count) {
        throw std::runtime_error("dino data: Part 10 count mismatch");
    }
    for (int i = 0; i < species_count; ++i) {
        table.default_family.push_back(
            parse_int(simu[base + 1 + static_cast<std::size_t>(i)],
                      simu_trans_path));
    }
    if (table.species.size() != table.default_family.size()) {
        throw std::runtime_error("dino data: family table size mismatch");
    }
    return table;
}

int read_gen_head_type(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open genotype: " + path);
    }
    std::vector<int> values;
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t comment = line.find("//");
        if (comment != std::string::npos) {
            line.resize(comment);
        }
        char* cursor = line.data();
        char* end = cursor + line.size();
        while (cursor < end) {
            char* next = nullptr;
            const long value = std::strtol(cursor, &next, 10);
            if (next == cursor) {
                ++cursor;
                continue;
            }
            values.push_back(static_cast<int>(value));
            cursor = next;
        }
    }
    if (values.size() < 3 || values[0] < 1) {
        return -1;
    }
    return std::min(values[1], values[2]);
}

DinoGenotype read_gen_genotype(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open genotype: " + path);
    }
    std::vector<int> values;
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t comment = line.find("//");
        if (comment != std::string::npos) {
            line.resize(comment);
        }
        char* cursor = line.data();
        char* end = cursor + line.size();
        while (cursor < end) {
            char* next = nullptr;
            const long value = std::strtol(cursor, &next, 10);
            if (next == cursor) {
                ++cursor;
                continue;
            }
            values.push_back(static_cast<int>(value));
            cursor = next;
        }
    }
    // The first value is the gene count (18); the rest are A/B pairs.
    if (values.size() < 37 || values[0] != 18) {
        throw std::runtime_error("genotype: expected 18 genes in " + path);
    }
    DinoGenotype genotype{};
    for (std::size_t gene = 0; gene < genotype.size(); ++gene) {
        genotype[gene] = {
            static_cast<std::uint32_t>(values[1 + gene * 2]),
            static_cast<std::uint32_t>(values[2 + gene * 2])};
    }
    return genotype;
}

bool genotype_is_carnivore(const DinoGenotype& genotype) {
    return genotype[14].first == 1 && genotype[14].second == 1;
}

DinoAttributes DinoPhenotypeTable::attributes(const DinoGenotype& genotype) const {
    DinoAttributes out;
    if (!valid || proportions.size() < 20) {
        return out;
    }
    static constexpr int kGene[5] = {0, 1, 1, 2, 3};
    int allele_a[5] = {};
    int allele_b[5] = {};
    for (int part = 0; part < 5; ++part) {
        const auto& gene = genotype[static_cast<std::size_t>(kGene[part])];
        if (gene.first >= 20 || gene.second >= 20) {
            return out;
        }
        allele_a[part] = static_cast<int>(gene.first);
        allele_b[part] = static_cast<int>(gene.second);
        out.part_allele_a[static_cast<std::size_t>(part)] = allele_a[part];
        out.part_allele_b[static_cast<std::size_t>(part)] = allele_b[part];
    }

    int raw_length[5] = {};
    int raw_width[5] = {};
    int raw_height[5] = {};
    for (int part = 0; part < 5; ++part) {
        const auto& table = static_cast<std::size_t>(part);
        raw_length[part] =
            length[table][static_cast<std::size_t>(allele_b[part])]
                         [static_cast<std::size_t>(allele_a[part])];
        raw_width[part] =
            width[table][static_cast<std::size_t>(allele_b[part])]
                        [static_cast<std::size_t>(allele_a[part])];
        raw_height[part] =
            height[table][static_cast<std::size_t>(allele_b[part])]
                         [static_cast<std::size_t>(allele_a[part])];
    }

    const auto blend = [&](int part, int column, float merge) {
        const float a = proportions[static_cast<std::size_t>(allele_a[part])]
                                    [static_cast<std::size_t>(column)];
        const float b = proportions[static_cast<std::size_t>(allele_b[part])]
                                    [static_cast<std::size_t>(column)];
        return (1.0f - merge) * a + merge * b;
    };
    const float merge_head =
        static_cast<float>(head_merge[static_cast<std::size_t>(allele_b[0])]
                                    [static_cast<std::size_t>(allele_a[0])]) *
        0.01f;
    const float merge_arm_leg =
        static_cast<float>(
            arm_leg_merge[static_cast<std::size_t>(allele_b[1])]
                         [static_cast<std::size_t>(allele_a[1])]) *
        0.01f;
    const float merge_tail =
        static_cast<float>(tail_merge[static_cast<std::size_t>(allele_b[4])]
                                    [static_cast<std::size_t>(allele_a[4])]) *
        0.01f;

    const float body = static_cast<float>(raw_length[3]);
    if (raw_length[0] == 0 || raw_height[1] == 0 || raw_height[2] == 0 ||
        raw_length[4] == 0) {
        return out;
    }
    float scale[5] = {};
    scale[0] = blend(0, 0, merge_head) * (body / static_cast<float>(raw_length[0]));
    scale[1] = blend(1, 1, merge_arm_leg) * (body / static_cast<float>(raw_height[1]));
    scale[2] = blend(1, 2, merge_arm_leg) * (body / static_cast<float>(raw_height[2]));
    scale[3] = 1.0f;
    scale[4] = blend(4, 4, merge_tail) * (body / static_cast<float>(raw_length[4]));

    const auto truncate = [](float value) { return static_cast<int>(value); };
    for (int part = 0; part < 5; ++part) {
        out.part_length[part] = truncate(static_cast<float>(raw_length[part]) *
                                         scale[part]);
        out.part_width[part] = truncate(static_cast<float>(raw_width[part]) *
                                        scale[part]);
        out.part_height[part] = truncate(static_cast<float>(raw_height[part]) *
                                         scale[part]);
    }

    // +0x1ac family: the ArmLeg merge picks the expressed legs allele, and the
    // phenotype uses row A (unlike the attribute tables, which use [B][A]).
    out.family = arm_leg_merge[static_cast<std::size_t>(allele_a[1])]
                              [static_cast<std::size_t>(allele_b[1])] < 50
                    ? default_family[static_cast<std::size_t>(allele_a[1])]
                    : default_family[static_cast<std::size_t>(allele_b[1])];
    out.quadruped = !(out.family == 2 || out.family == 3);
    const int body_half = out.part_length[3] / 2;
    const int front = out.quadruped ? out.part_length[0] + body_half
                                    : out.part_length[0] + out.part_length[3];
    const int back = out.quadruped ? out.part_length[4] + body_half
                                   : out.part_length[4];
    out.length_cm = front + back;  // +0x00 = +0x04 + +0x08
    out.width_cm = out.part_width[3] + std::max(out.part_width[1], out.part_width[2]);
    out.height_cm =
        out.part_height[0] + std::max(out.part_height[1], out.part_height[2]);
    out.size_class = -1;
    for (int category = 0; category < 4; ++category) {
        if (out.length_cm <= size_category_max[static_cast<std::size_t>(category)]) {
            out.size_class = category;
            break;
        }
    }
    if (out.size_class < 0) {
        out.size_class = 3;
    }
    // Retail overrides the family for bipeds: `(class < 2) + 2`
    // (HeavyBiped 2 / LightBiped 3); quadruped keeps the legs family.
    if (!out.quadruped) {
        out.family = out.size_class < 2 ? 2 : 3;
    }

    int weight_sum = 0;
    int speed_sum = 0;
    for (int part = 0; part < 5; ++part) {
        const auto table = static_cast<std::size_t>(part);
        const auto b = static_cast<std::size_t>(allele_b[part]);
        const auto a = static_cast<std::size_t>(allele_a[part]);
        out.weight_part_raw[table] = weight[table][b][a];
        weight_sum += truncate(static_cast<float>(weight[table][b][a]) * scale[part]);
        speed_sum += truncate(static_cast<float>(speed[table][b][a]) * scale[part]);
    }
    out.weight_kg = weight_sum;
    const float motricity =
        out.weight_kg > 0
            ? static_cast<float>(speed_sum) /
                  static_cast<float>(std::pow(static_cast<double>(out.weight_kg),
                                              motricity_power))
            : 0.0f;
    out.walk_speed = static_cast<float>(out.part_length[3]) * walking_speed_coef *
                     motricity;
    out.run_speed = motricity >= min_motricity_to_run
                        ? static_cast<float>(out.part_length[3]) *
                              running_speed_coef * motricity
                        : 0.0f;

    const auto express = [&](const DinoGeneTable& table, int gene) {
        const int a = static_cast<int>(genotype[static_cast<std::size_t>(gene)].first);
        const int b = static_cast<int>(genotype[static_cast<std::size_t>(gene)].second);
        if (a < 0 || a > 9 || b < 0 || b > 9) {
            return 0;
        }
        return table.matrix[static_cast<std::size_t>(b)]
                           [static_cast<std::size_t>(a)];
    };
    out.aggression = express(agre, 15);
    out.stamina = express(stam, 16);
    out.courage = express(cour, 17);


    int perception_sum = 0;
    int intelligence_sum = 0;
    int beauty_sum = 0;
    int armor_sum = 0;
    int agility_sum = 0;
    for (int part = 0; part < 5; ++part) {
        const auto table = static_cast<std::size_t>(part);
        const auto b = static_cast<std::size_t>(allele_b[part]);
        const auto a = static_cast<std::size_t>(allele_a[part]);
        perception_sum += perception[table][b][a];
        intelligence_sum += intelligence[table][b][a];
        beauty_sum += beauty[table][b][a];
        armor_sum += armor[table][b][a];
        agility_sum += agility[table][b][a];
    }
    out.perception = perception_sum + truncate(perception_height_percentage *
                                               static_cast<float>(out.height_cm) /
                                               100.0f);
    out.perception = std::min(out.perception, max_perception);
    // Intelligence is an encephalization quotient, not the Part 3 sum. The
    // product is int64 here; retail multiplies int32 (wrapping on overflow).
    const std::int64_t volume =
        static_cast<std::int64_t>(out.length_cm) * out.width_cm * out.height_cm;
    out.intelligence =
        volume > 0
            ? truncate(intelligence_multiplier *
                       static_cast<float>(intelligence_sum) /
                       static_cast<float>(std::pow(static_cast<double>(volume),
                                                   intelligence_power)))
            : 0;
    out.intelligence = std::clamp(out.intelligence, 0, 200);
    out.beauty = beauty_sum;
    out.armor = armor_sum;
    out.agility = agility_sum;
    out.jump = truncate(jump_percentage * static_cast<float>(out.agility) / 100.0f);

    const auto attack_value = [&](int part, int part_a, int part_b) {
        return attack[static_cast<std::size_t>(part)]
                     [static_cast<std::size_t>(part_b)]
                     [static_cast<std::size_t>(part_a)];
    };
    const auto scaled_attack = [&](int value, int part) {
        return truncate(static_cast<float>(value) *
                        std::sqrt(scale[part]));
    };
    out.attack_head = scaled_attack(attack_value(0, allele_a[0], allele_b[0]), 0);
    out.attack_body = scaled_attack(attack_value(3, allele_a[0], allele_b[0]), 0);
    out.attack_arm = scaled_attack(attack_value(1, allele_a[1], allele_b[1]), 1);
    out.attack_leg = scaled_attack(attack_value(2, allele_a[1], allele_b[1]), 2);
    out.attack_tail = scaled_attack(attack_value(4, allele_a[4], allele_b[4]), 4);
    if (out.quadruped) {
        out.attack_leg = 0;
    }
    out.attack_total = out.attack_head + out.attack_body + out.attack_arm +
                       out.attack_leg + out.attack_tail;

    // +0x78/+0x7c/+0x80 are derived from the final height before the
    // accumulator; the accumulator then applies slot 16 to +0x78 and the
    // radius/square are recomputed from it (`FUN_00617250`).
    out.obstacle_size =
        (visibility_divider / 2 + out.height_cm) / visibility_divider;

    // `+0x194` conventional appeal (`FUN_00617250`): 40 / distinct part
    // allele pairs + 12 * homozygous parts. The accumulator then applies
    // slot 17 (subtracted) and the slot-6 percentage.
    {
        static constexpr int kPartGene[5] = {0, 1, 1, 2, 3};
        std::array<std::pair<std::uint32_t, std::uint32_t>, 5> pairs{};
        int homogeneous = 0;
        for (int part = 0; part < 5; ++part) {
            const auto& gene =
                genotype[static_cast<std::size_t>(kPartGene[part])];
            pairs[static_cast<std::size_t>(part)] = gene;
            if (gene.first == gene.second) {
                homogeneous += conventional_limb_homogeneity_factor;
            }
        }
        int distinct = 0;
        for (int i = 0; i < 5; ++i) {
            bool seen = false;
            for (int j = 0; j < i; ++j) {
                if (pairs[static_cast<std::size_t>(j)] ==
                    pairs[static_cast<std::size_t>(i)]) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                ++distinct;
            }
        }
        out.conventional_appeal =
            conventional_limb_disparity_factor / std::max(1, distinct) +
            homogeneous;
    }

    // `FUN_00619a60` (additive) and `FUN_00619b80` (percentage) walk the 23
    // dummy slots; the skin and carapace additions follow in `FUN_00617250`.
    // The final value is `trunc(P * pct/100 + 1e-4) + trunc(acc)` for the int
    // fields and `P * pct * 0.01 + acc * 0.01` for the two speeds.
    const auto express_dominant = [&](const DinoAlleleTable& table,
                                      int gene) {
        return table.dominant(
            genotype[static_cast<std::size_t>(gene)].first,
            genotype[static_cast<std::size_t>(gene)].second);
    };
    const auto express_sub = [&](const DinoAlleleTable& table, int gene) {
        return table.expressed(
            genotype[static_cast<std::size_t>(gene)].first,
            genotype[static_cast<std::size_t>(gene)].second);
    };

    // 23-slot body-part map (`FUN_0061a230`): Head 0..5, Body 6..9, Arm 10,
    // Leg 11, Tail 12..17, then copies of slots 5/10/11/16/17.
    static constexpr int kSlotPart[23] = {
        0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 1, 2,
        4, 4, 4, 4, 4, 4, 0, 1, 2, 4, 4};
    // Per-part selector column (`T[0x114]`): Head 8, Arm 9, Leg 10,
    // Body 18 (none), Tail 11.
    static constexpr int kSelector[5] = {8, 9, 10, 18, 11};

    std::array<int, 23> slot_row{};
    {
        const DinoAlleleTable* positions[3] = {&hd_di, &bd_di, &td_di};
        const DinoAlleleTable* types[3] = {&hd_ty, &bd_ty, &td_ty};
        for (int pair = 0; pair < 3; ++pair) {
            const int pos = std::clamp(
                express_dominant(*positions[pair], 4 + pair * 2), 0, 47);
            const int type = std::clamp(
                express_dominant(*types[pair], 5 + pair * 2), 0, 47);
            const auto& disposition =
                dummy_disposition[static_cast<std::size_t>(pair)]
                                 [static_cast<std::size_t>(pos)];
            const auto& type_row =
                dummy_type[static_cast<std::size_t>(pair)]
                          [static_cast<std::size_t>(type)];
            for (int col = 0; col < 6; ++col) {
                const int tag = disposition[static_cast<std::size_t>(col)];
                if (tag <= 0 || tag > 5) {
                    continue;
                }
                slot_row[static_cast<std::size_t>(pair * 6 + col)] =
                    type_row[static_cast<std::size_t>(tag - 1)];
            }
        }
        slot_row[18] = slot_row[5];
        slot_row[19] = slot_row[10];
        slot_row[20] = slot_row[11];
        slot_row[21] = slot_row[16];
        slot_row[22] = slot_row[17];
    }

    // `ornateddinos`: the actor's 23-slot +0xE8 map is non-empty when any
    // Part-6 dummy row is selected. The five duplicate slots count too.
    out.ornament_count = 0;
    for (const int row : slot_row) {
        if (row > 0) {
            ++out.ornament_count;
        }
    }

    std::array<float, 18> acc{};
    std::array<int, 18> pct{};
    pct.fill(100);
    for (int slot = 0; slot < 23; ++slot) {
        const int row = slot_row[static_cast<std::size_t>(slot)];
        if (row <= 0 || row >= 50) {
            continue;
        }
        const int part = kSlotPart[slot];
        const float ratio =
            static_cast<float>(out.part_width[static_cast<std::size_t>(part)]) /
            static_cast<float>(reference_width[static_cast<std::size_t>(part)]);
        const auto& add =
            dummy_influence[0][static_cast<std::size_t>(row)];
        const auto& percent =
            dummy_influence[1][static_cast<std::size_t>(row)];
        for (int k = 0; k < 7; ++k) {
            acc[static_cast<std::size_t>(k)] +=
                static_cast<float>(add[static_cast<std::size_t>(k)]) * ratio;
            pct[static_cast<std::size_t>(k)] +=
                percent[static_cast<std::size_t>(k)] - 100;
        }
        const int selector = kSelector[part];
        if (selector != 18) {
            acc[static_cast<std::size_t>(selector)] +=
                static_cast<float>(add[static_cast<std::size_t>(selector)]) *
                ratio;
            pct[static_cast<std::size_t>(selector)] +=
                percent[static_cast<std::size_t>(selector)] - 100;
        }
        for (int k = 12; k < 18; ++k) {
            acc[static_cast<std::size_t>(k)] +=
                static_cast<float>(add[static_cast<std::size_t>(k)]) * ratio;
            pct[static_cast<std::size_t>(k)] +=
                percent[static_cast<std::size_t>(k)] - 100;
        }
    }

    // Skin (`P+0x174`, Part 7): the 8x3 map selects a 22-row influence
    // pair; only column 0 is scaled by the body-length ratio.
    {
        const int type = std::clamp(express_dominant(skin_type, 10), 0, 7);
        const int sub = std::clamp(express_sub(skin_sub, 11), 0, 2);
        const int row = skin_map[static_cast<std::size_t>(type)]
                                [static_cast<std::size_t>(sub)];
        const float ratio =
            static_cast<float>(out.part_length[3]) / skin_reference;
        if (row >= 0 && row < 22) {
            for (int k = 0; k < 18; ++k) {
                acc[static_cast<std::size_t>(k)] +=
                    static_cast<float>(
                        skin_influence[0][static_cast<std::size_t>(row)]
                                      [static_cast<std::size_t>(k)]) *
                    (k == 0 ? ratio : 1.0f);
                pct[static_cast<std::size_t>(k)] +=
                    skin_influence[1][static_cast<std::size_t>(row)]
                                  [static_cast<std::size_t>(k)] -
                    100;
            }
        }
    }
    // Carapace (`P+0x178`, Part 8).
    {
        const int type =
            std::clamp(express_dominant(carapace_type, 12), 0, 5);
        const int sub = std::clamp(express_sub(carapace_sub, 13), 0, 2);
        const int row = carapace_map[static_cast<std::size_t>(type)]
                                    [static_cast<std::size_t>(sub)];
        const float ratio =
            static_cast<float>(out.part_length[3]) / skin_reference;
        if (row >= 0 && row < 16) {
            for (int k = 0; k < 18; ++k) {
                acc[static_cast<std::size_t>(k)] +=
                    static_cast<float>(
                        carapace_influence[0][static_cast<std::size_t>(row)]
                                          [static_cast<std::size_t>(k)]) *
                    (k == 0 ? ratio : 1.0f);
                pct[static_cast<std::size_t>(k)] +=
                    carapace_influence[1][static_cast<std::size_t>(row)]
                                      [static_cast<std::size_t>(k)] -
                    100;
            }
        }
    }

    const auto normalise = [&](int value, int k) {
        return static_cast<int>(static_cast<float>(value) *
                                    (static_cast<float>(
                                         pct[static_cast<std::size_t>(k)]) *
                                     0.01f) +
                                1e-4f) +
               static_cast<int>(acc[static_cast<std::size_t>(k)]);
    };
    out.weight_kg = normalise(out.weight_kg, 0);
    out.walk_speed =
        out.walk_speed *
            (static_cast<float>(pct[2]) * 0.01f) +
        acc[2] * 0.01f;
    out.run_speed =
        out.run_speed *
            (static_cast<float>(pct[2]) * 0.01f) +
        acc[2] * 0.01f;
    out.perception = normalise(out.perception, 3);
    out.intelligence = normalise(out.intelligence, 4);
    out.beauty = normalise(out.beauty, 5);
    out.agility = normalise(out.agility, 6);
    out.attack_head = normalise(out.attack_head, 7);
    out.attack_body = normalise(out.attack_body, 8);
    out.attack_arm = normalise(out.attack_arm, 9);
    out.attack_leg = normalise(out.attack_leg, 10);
    out.attack_tail = normalise(out.attack_tail, 11);
    out.armor = normalise(out.armor, 12);
    out.aggression = normalise(out.aggression, 13);
    out.stamina = normalise(out.stamina, 14);
    out.courage = normalise(out.courage, 15);
    out.obstacle_size = normalise(out.obstacle_size, 16);
    // `+0x194`: the percentage pass divides by slot 6, the additive pass
    // subtracts slot 17 (`0x619d90` then `0x619c70`).
    if (pct[6] != 0) {
        out.conventional_appeal = static_cast<int>(
            static_cast<float>(out.conventional_appeal) * 100.0f /
                static_cast<float>(pct[6]) + 1e-4f) -
            static_cast<int>(acc[17]);
    }
    out.conventional_appeal =
        std::clamp(out.conventional_appeal, 0, 100);
    // The store shows Strangeness/ORIGINALITY: a conventional body is low.
    out.originality = 100 - out.conventional_appeal;
    // `def+0x19c` (`0x61766b..0x6176d4`): sum over the 18 genes of
    // `price[rarity(A)-1] + price[rarity(B)-1]`; the retail record skips a
    // gene only when its rarity table is absent.
    out.market_value = 0;
    for (int gene = 0; gene < 18; ++gene) {
        const std::vector<int>& weights =
            rarity[static_cast<std::size_t>(gene)];
        const int a = genotype[static_cast<std::size_t>(gene)].first;
        const int b = genotype[static_cast<std::size_t>(gene)].second;
        if (a < 0 || b < 0 ||
            a >= static_cast<int>(weights.size()) ||
            b >= static_cast<int>(weights.size())) {
            continue;
        }
        const std::size_t ca = static_cast<std::size_t>(
            std::clamp(weights[static_cast<std::size_t>(a)] - 1, 0, 2));
        const std::size_t cb = static_cast<std::size_t>(
            std::clamp(weights[static_cast<std::size_t>(b)] - 1, 0, 2));
        out.market_value += allele_price[ca] + allele_price[cb];
    }

    // `FUN_00619fb0` clamps the final values with the Part 9 table.
    if (has_attribute_limits) {
        const auto clamp_attr = [&](int& value, int index) {
            value = std::clamp(
                value, min_attributes[static_cast<std::size_t>(index)],
                max_attributes[static_cast<std::size_t>(index)]);
        };
        clamp_attr(out.weight_kg, 0);
        clamp_attr(out.jump, 3);
        clamp_attr(out.perception, 4);
        clamp_attr(out.intelligence, 5);
        clamp_attr(out.beauty, 6);
        clamp_attr(out.agility, 7);
        clamp_attr(out.attack_head, 8);
        clamp_attr(out.attack_body, 9);
        clamp_attr(out.attack_arm, 10);
        clamp_attr(out.attack_leg, 11);
        clamp_attr(out.attack_tail, 12);
        clamp_attr(out.armor, 14);
        clamp_attr(out.aggression, 17);
        clamp_attr(out.stamina, 18);
        clamp_attr(out.courage, 19);
    }

    // `def+0x188` (`0x617dfb`, after the accumulator and the `0x619fb0`
    // clamp): max rest = final stamina + x. `0x6179d6..0x617a0a` computes
    //   x = (weight[1][g1b][g1a] + weight[2][g1b][g1a])
    //       * PhysicalBonusOnStamina / weight_sum
    // Part order is Head/Arm/Leg/Body/Tail, so tables 1/2 are Arm/Leg and
    // both share the raw gene-1 allele pair; the tables are used unscaled
    // and the division truncates.
    if (weight_sum > 0) {
        const int g1a = genotype[1].first;
        const int g1b = genotype[1].second;
        if (g1a >= 0 && g1a < 20 && g1b >= 0 && g1b < 20) {
            const auto b = static_cast<std::size_t>(g1b);
            const auto a = static_cast<std::size_t>(g1a);
            const int raw = weight[1][b][a] + weight[2][b][a];
            const int bonus =
                (raw * physical_bonus_on_stamina) / weight_sum;
            out.max_rest = static_cast<float>(out.stamina + bonus);
        }
    }
    // `0x617d47..0x617d7c`: max health = sqrt(weight) +
    // 0.5 * cbrt(length*width*height), all final phenotype values.
    const double body_volume = static_cast<double>(out.length_cm) *
                               static_cast<double>(out.width_cm) *
                               static_cast<double>(out.height_cm);
    out.max_health = static_cast<int>(
        std::sqrt(static_cast<double>(out.weight_kg)) +
        0.5 * std::cbrt(body_volume));
    // `0x617dd9..0x617df4`: regen = maxHP*stamina / RegenerationDivider + 1.
    out.regen = (out.max_health * out.stamina) /
                    std::max(1, regeneration_divider) +
                1;
    out.food_consumption = static_cast<float>(out.weight_kg) *
                           food_consumption_rate * 0.01f;
    out.violence_evolution = static_cast<float>(out.aggression) *
                             violence_evolution_rate * 0.01f;
    out.obstacle_size_sq = out.obstacle_size * out.obstacle_size;
    out.obstacle_radius =
        (out.obstacle_size * visibility_divider) / danger_visibility_divider;
    // Retail sums the five accumulated attack parts after `FUN_00619fb0`.
    out.attack_total = out.attack_head + out.attack_body + out.attack_arm +
                       out.attack_leg + out.attack_tail;
    out.combined_attack = (out.aggression + out.attack_total) / 6;
    return out;
}

std::array<float, 5> dino_show_needs(const DinoAttributes& attributes,
                                     const DinoPhenotypeTable& table) {
    std::array<float, 5> needs{};
    // `FUN_00618020`: beauty, danger, spectacle, conventional, tamability.
    needs[0] = static_cast<float>(attributes.beauty) *
               table.cute_need_beauty_factor;
    needs[1] = static_cast<float>(attributes.attack_head) *
                   table.danger_jaw_factor +
               static_cast<float>(attributes.aggression) *
                   table.danger_aggressivity_factor +
               static_cast<float>(attributes.attack_total -
                                  attributes.attack_head) *
                   table.danger_combined_attack_factor;
    const double volume = static_cast<double>(attributes.length_cm) *
                          static_cast<double>(attributes.width_cm) *
                          static_cast<double>(attributes.height_cm);
    const double size = volume > 0.0 ? std::cbrt(volume) : 0.0;
    needs[2] = static_cast<float>(
        size * table.spectacular_need_size_factor /
        (table.max_dinosaur_size > 0.0f ? table.max_dinosaur_size : 1.0f));
    needs[3] = static_cast<float>(attributes.conventional_appeal) *
               table.conventional_need_factor;
    needs[4] = static_cast<float>(attributes.intelligence) *
                   table.taming_need_intelligence_factor +
               static_cast<float>(100 - attributes.aggression) *
                   table.taming_need_aggressivity_factor;
    return needs;
}

int DinoPhenotypeTable::length_cm(const DinoGenotype& genotype) const {
    return attributes(genotype).length_cm;
}

int DinoPhenotypeTable::size_class(const DinoGenotype& genotype) const {
    return attributes(genotype).size_class;
}

DinoPhenotypeTable read_dino_phenotype(const std::string& proportion_path,
                                       const std::string& simu_trans_path,
                                       const std::string& constants_path) {
    DinoPhenotypeTable table;

    std::ifstream in(proportion_path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open proportions: " +
                                 proportion_path);
    }
    std::uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in || count == 0 || count > 256) {
        throw std::runtime_error("proportions: bad count in " +
                                 proportion_path);
    }
    table.proportions.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<float, 5>& row = table.proportions[i];
        in.read(reinterpret_cast<char*>(row.data()),
                static_cast<std::streamsize>(row.size() * sizeof(float)));
        if (!in) {
            throw std::runtime_error("proportions: truncated record in " +
                                     proportion_path);
        }
    }

    const std::vector<std::string> lines = read_lines(simu_trans_path);
    read_merge_block(lines, "Head : merge ratio", simu_trans_path,
                     table.head_merge);
    read_merge_block(lines, "ArmLeg : merge ratio", simu_trans_path,
                     table.arm_leg_merge);
    read_merge_block(lines, "Tail : merge ratio", simu_trans_path,
                     table.tail_merge);
    read_part_group(lines, "Length", simu_trans_path, table.length);
    read_part_group(lines, "Width", simu_trans_path, table.width);
    read_part_group(lines, "Height", simu_trans_path, table.height);
    read_part_group(lines, "Weight", simu_trans_path, table.weight);
    read_part_group(lines, "Speed", simu_trans_path, table.speed);
    read_part_group(lines, "Perception", simu_trans_path, table.perception);
    read_part_group(lines, "Intelligence", simu_trans_path, table.intelligence);
    read_part_group(lines, "Beauty", simu_trans_path, table.beauty);
    read_part_group(lines, "Armor", simu_trans_path, table.armor);
    read_part_group(lines, "Agility", simu_trans_path, table.agility);
    read_part_group(lines, "Attack", simu_trans_path, table.attack);
    table.agre = read_gene_table(lines, "Agre", simu_trans_path);
    table.stam = read_gene_table(lines, "Stam", simu_trans_path);
    table.cour = read_gene_table(lines, "Cour", simu_trans_path);
    table.hd_di = read_allele_table(lines, "HDDi", simu_trans_path);
    table.hd_ty = read_allele_table(lines, "HDTy", simu_trans_path);
    table.bd_di = read_allele_table(lines, "BDDi", simu_trans_path);
    table.bd_ty = read_allele_table(lines, "BDTy", simu_trans_path);
    table.td_di = read_allele_table(lines, "TDDi", simu_trans_path);
    table.td_ty = read_allele_table(lines, "TDTy", simu_trans_path);
    table.skin_type = read_allele_table(lines, "SknTyp", simu_trans_path);
    table.skin_sub = read_allele_table(lines, "SknSub", simu_trans_path);
    table.carapace_type =
        read_allele_table(lines, "CarTyp", simu_trans_path);
    table.carapace_sub = read_allele_table(lines, "CarSub", simu_trans_path);
    // Rarity weights, one vector per gene in the runtime genotype order
    // (docs/formats/gen.md). `0x61766b` uses them for the `+0x19c` value.
    static constexpr const char* kGeneCodes[18] = {
        "Head", "ArmLeg", "Body", "Tail", "HDDi", "HDTy", "BDDi",
        "BDTy", "TDDi", "TDTy", "SknTyp", "SknSub", "CarTyp", "CarSub",
        "Diet", "Agre", "Stam", "Cour"};
    for (int gene = 0; gene < 18; ++gene) {
        table.rarity[static_cast<std::size_t>(gene)] = read_rarity_table(
            lines, kGeneCodes[gene], simu_trans_path);
    }

    // Parts 4-6: the dummy disposition/type tables and their influence
    // tables. Part 4/5 have three subparts (Head/Body/Tail), each introduced
    // by a `48` count; Part 6 is `50:` x (`18:` + 18 ints), twice, followed
    // by the five reference widths.
    const std::size_t part4 =
        find_section(lines, "// Part 4", simu_trans_path);
    const std::size_t part5 =
        find_section(lines, "// Part 5", simu_trans_path);
    const std::size_t part6 =
        find_section(lines, "// Part 6", simu_trans_path);
    const std::size_t part7 =
        find_section(lines, "// Part 7", simu_trans_path);
    const std::size_t part8 =
        find_section(lines, "// Part 8", simu_trans_path);
    const std::size_t part9 =
        find_section(lines, "// Part 9", simu_trans_path);
    const std::size_t part10 =
        find_section(lines, "// Part 10", simu_trans_path);

    {
        std::vector<std::string> tokens =
            read_tokens(lines, part4 + 1, part5);
        std::size_t at = 0;
        table.dummy_disposition =
            read_dummy_rows<6>(tokens, at, simu_trans_path);
        if (at != tokens.size()) {
            throw std::runtime_error(
                "dino data: trailing tokens in Part 4");
        }
    }
    {
        std::vector<std::string> tokens =
            read_tokens(lines, part5 + 1, part6);
        std::size_t at = 0;
        table.dummy_type = read_dummy_rows<5>(tokens, at, simu_trans_path);
        if (at != tokens.size()) {
            throw std::runtime_error(
                "dino data: trailing tokens in Part 5");
        }
    }
    {
        std::vector<std::string> tokens =
            read_tokens(lines, part6 + 1, part7);
        std::size_t at = 0;
        const auto additive = read_counted_blocks(tokens, at,
                                                   simu_trans_path);
        const auto percentage = read_counted_blocks(tokens, at,
                                                     simu_trans_path);
        if (additive.size() != 50 || percentage.size() != 50) {
            throw std::runtime_error(
                "dino data: Part 6 influence tables are not 50 rows");
        }
        for (int i = 0; i < 50; ++i) {
            if (additive[static_cast<std::size_t>(i)].size() != 18 ||
                percentage[static_cast<std::size_t>(i)].size() != 18) {
                throw std::runtime_error(
                    "dino data: Part 6 influence row is not 18 wide");
            }
            for (int k = 0; k < 18; ++k) {
                table.dummy_influence[0][static_cast<std::size_t>(i)]
                                     [static_cast<std::size_t>(k)] =
                    additive[static_cast<std::size_t>(i)]
                            [static_cast<std::size_t>(k)];
                table.dummy_influence[1][static_cast<std::size_t>(i)]
                                     [static_cast<std::size_t>(k)] =
                    percentage[static_cast<std::size_t>(i)]
                              [static_cast<std::size_t>(k)];
            }
        }
        for (int i = 0; i < 5; ++i) {
            if (at >= tokens.size()) {
                throw std::runtime_error(
                    "dino data: truncated Part 6 reference widths");
            }
            table.reference_width[static_cast<std::size_t>(i)] =
                parse_int(tokens[at++], simu_trans_path);
        }
        if (at != tokens.size()) {
            throw std::runtime_error(
                "dino data: trailing tokens in Part 6");
        }
    }
    {
        std::vector<std::string> tokens =
            read_tokens(lines, part7 + 1, part8);
        std::size_t at = 0;
        const auto map = read_counted_blocks(tokens, at, simu_trans_path);
        if (map.size() != 8) {
            throw std::runtime_error("dino data: skin map is not 8 rows");
        }
        for (int i = 0; i < 8; ++i) {
            if (map[static_cast<std::size_t>(i)].size() != 3) {
                throw std::runtime_error(
                    "dino data: skin map row is not 3 wide");
            }
            for (int k = 0; k < 3; ++k) {
                table.skin_map[static_cast<std::size_t>(i)]
                              [static_cast<std::size_t>(k)] =
                    map[static_cast<std::size_t>(i)]
                       [static_cast<std::size_t>(k)];
            }
        }
        if (at >= tokens.size()) {
            throw std::runtime_error(
                "dino data: missing Part 7 skin reference");
        }
        table.skin_reference = parse_float(tokens[at++]);
        for (int table_index = 0; table_index < 2; ++table_index) {
            const auto influence =
                read_counted_blocks(tokens, at, simu_trans_path);
            if (influence.size() != 22) {
                throw std::runtime_error(
                    "dino data: skin influence table is not 22 rows");
            }
            table.skin_influence[static_cast<std::size_t>(table_index)]
                .resize(22);
            for (int i = 0; i < 22; ++i) {
                if (influence[static_cast<std::size_t>(i)].size() != 18) {
                    throw std::runtime_error(
                        "dino data: skin influence row is not 18 wide");
                }
                for (int k = 0; k < 18; ++k) {
                    table.skin_influence
                        [static_cast<std::size_t>(table_index)]
                        [static_cast<std::size_t>(i)]
                        [static_cast<std::size_t>(k)] =
                        influence[static_cast<std::size_t>(i)]
                                 [static_cast<std::size_t>(k)];
                }
            }
        }
        if (at != tokens.size()) {
            throw std::runtime_error(
                "dino data: trailing tokens in Part 7");
        }
    }
    {
        std::vector<std::string> tokens =
            read_tokens(lines, part8 + 1, part9);
        std::size_t at = 0;
        const auto map = read_counted_blocks(tokens, at, simu_trans_path);
        if (map.size() != 6) {
            throw std::runtime_error(
                "dino data: carapace map is not 6 rows");
        }
        for (int i = 0; i < 6; ++i) {
            if (map[static_cast<std::size_t>(i)].size() != 3) {
                throw std::runtime_error(
                    "dino data: carapace map row is not 3 wide");
            }
            for (int k = 0; k < 3; ++k) {
                table.carapace_map[static_cast<std::size_t>(i)]
                                  [static_cast<std::size_t>(k)] =
                    map[static_cast<std::size_t>(i)]
                       [static_cast<std::size_t>(k)];
            }
        }
        for (int table_index = 0; table_index < 2; ++table_index) {
            const auto influence =
                read_counted_blocks(tokens, at, simu_trans_path);
            if (influence.size() != 16) {
                throw std::runtime_error(
                    "dino data: carapace influence table is not 16 rows");
            }
            table.carapace_influence
                [static_cast<std::size_t>(table_index)]
                .resize(16);
            for (int i = 0; i < 16; ++i) {
                if (influence[static_cast<std::size_t>(i)].size() != 18) {
                    throw std::runtime_error(
                        "dino data: carapace influence row is not 18 wide");
                }
                for (int k = 0; k < 18; ++k) {
                    table.carapace_influence
                        [static_cast<std::size_t>(table_index)]
                        [static_cast<std::size_t>(i)]
                        [static_cast<std::size_t>(k)] =
                        influence[static_cast<std::size_t>(i)]
                                 [static_cast<std::size_t>(k)];
                }
            }
        }
        if (at != tokens.size()) {
            throw std::runtime_error(
                "dino data: trailing tokens in Part 8");
        }
    }
    {
        std::vector<std::string> tokens =
            read_tokens(lines, part9 + 1, part10);
        if (tokens.size() < 40) {
            throw std::runtime_error(
                "dino data: Part 9 min/max table is truncated");
        }
        for (int i = 0; i < 20; ++i) {
            table.min_attributes[static_cast<std::size_t>(i)] =
                static_cast<int>(parse_float(
                    tokens[static_cast<std::size_t>(i)]));
            table.max_attributes[static_cast<std::size_t>(i)] =
                static_cast<int>(parse_float(
                    tokens[static_cast<std::size_t>(20 + i)]));
        }
        table.has_attribute_limits = true;
    }

    // Part 10: count (20) then one default family index per base dinosaur.
    const std::vector<int> families =
        read_int_tokens(lines, part10 + 1, 21, simu_trans_path);
    if (families[0] != 20) {
        throw std::runtime_error("dino data: Part 10 count mismatch");
    }
    for (int i = 0; i < 20; ++i) {
        table.default_family[static_cast<std::size_t>(i)] =
            families[static_cast<std::size_t>(i + 1)];
    }

    read_translator_constants(constants_path, table);
    table.valid = true;
    return table;
}

}  // namespace dino
