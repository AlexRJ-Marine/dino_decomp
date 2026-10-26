#include "dino_data.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
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

}  // namespace dino
