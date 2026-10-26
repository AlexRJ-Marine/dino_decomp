#include "dino_data.h"

#include <algorithm>
#include <cctype>
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
                      std::array<std::array<int, 20>, 20>& out) {
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

int DinoPhenotypeTable::length_cm(const DinoGenotype& genotype) const {
    if (!valid || proportions.size() < 20) {
        return 0;
    }
    const auto allele_a = [&](std::size_t gene) {
        return static_cast<int>(genotype[gene].first);
    };
    const auto allele_b = [&](std::size_t gene) {
        return static_cast<int>(genotype[gene].second);
    };
    const int a0 = allele_a(0), b0 = allele_b(0);
    const int a2 = allele_a(2), b2 = allele_b(2);
    const int a3 = allele_a(3), b3 = allele_b(3);
    if (a0 < 0 || a0 >= 20 || b0 < 0 || b0 >= 20 || a2 < 0 || a2 >= 20 ||
        b2 < 0 || b2 >= 20 || a3 < 0 || a3 >= 20 || b3 < 0 || b3 >= 20) {
        return 0;
    }
    // FUN_00619830: for each part, scale = blend(proportion) * body / table,
    // then the stored part length is table * scale (float32), truncated.
    // The body itself is the raw Part 3 Body Length cell.
    const float mh = static_cast<float>(head_merge[b0][a0]) / 100.0f;
    const float head_ratio =
        mh * proportions[static_cast<std::size_t>(b0)][0] +
        (1.0f - mh) * proportions[static_cast<std::size_t>(a0)][0];
    const float mt = static_cast<float>(tail_merge[b3][a3]) / 100.0f;
    const float tail_ratio =
        mt * proportions[static_cast<std::size_t>(b3)][4] +
        (1.0f - mt) * proportions[static_cast<std::size_t>(a3)][4];
    const int body = body_length[b2][a2];
    const int table_head = head_length[b0][a0];
    const int table_tail = tail_length[b3][a3];
    const float body_f = static_cast<float>(body);
    const int head = static_cast<int>(
        static_cast<float>(table_head) *
        (head_ratio / static_cast<float>(table_head) * body_f));
    const int tail = static_cast<int>(
        static_cast<float>(table_tail) *
        (tail_ratio / static_cast<float>(table_tail) * body_f));
    return head + body + tail;
}

int DinoPhenotypeTable::size_class(const DinoGenotype& genotype) const {
    if (!valid) {
        return -1;
    }
    const int length = length_cm(genotype);
    if (length <= 200) {
        return 0;
    }
    if (length <= 620) {
        return 1;
    }
    if (length <= 900) {
        return 2;
    }
    return 3;
}

DinoPhenotypeTable read_dino_phenotype(const std::string& proportion_path,
                                       const std::string& simu_trans_path) {
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
    read_merge_block(lines, "Tail : merge ratio", simu_trans_path,
                     table.tail_merge);
    read_merge_block(lines, "Head : Length", simu_trans_path,
                     table.head_length);
    read_merge_block(lines, "Body : Length", simu_trans_path,
                     table.body_length);
    read_merge_block(lines, "Tail : Length", simu_trans_path,
                     table.tail_length);
    table.valid = true;
    return table;
}

}  // namespace dino
