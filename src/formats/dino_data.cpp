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
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || (end != nullptr && *end != '\0')) {
        throw std::runtime_error("dino data: expected integer in " + path +
                                 ": " + text);
    }
    return static_cast<int>(value);
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
        weight_sum += truncate(static_cast<float>(weight[table][b][a]) * scale[part]);
        speed_sum += truncate(static_cast<float>(speed[table][b][a]) * scale[part]);
    }
    out.weight_kg = weight_sum;
    out.food_consumption = static_cast<float>(weight_sum) *
                           food_consumption_rate * 0.01f;
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
    out.violence_evolution = static_cast<float>(out.aggression) *
                             violence_evolution_rate * 0.01f;

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
    out.perception = std::min(out.perception, 90);  // MaxPerception
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
    out.combined_attack = (out.aggression + out.attack_total) / 6;
    return out;
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

    // Part 10: count (20) then one default family index per base dinosaur.
    const std::size_t part10 =
        find_section(lines, "// Part 10", simu_trans_path) + 1;
    const std::vector<int> families =
        read_int_tokens(lines, part10, 21, simu_trans_path);
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
