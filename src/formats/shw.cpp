#include "shw.h"

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

std::vector<int> parse_ints(const std::string& text, const std::string& path,
                            const std::string& section) {
    std::vector<int> values;
    const char* cursor = text.c_str();
    while (*cursor != '\0') {
        char* next = nullptr;
        const long value = std::strtol(cursor, &next, 10);
        if (next == cursor) {
            if (!std::isspace(static_cast<unsigned char>(*cursor))) {
                throw std::runtime_error("shw: bad number in " + section +
                                         " of " + path + ": " + text);
            }
            ++cursor;
            continue;
        }
        values.push_back(static_cast<int>(value));
        cursor = next;
    }
    return values;
}

}  // namespace

ShwShow load_shw(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("shw: cannot open " + path);
    }
    std::vector<std::pair<std::string, std::vector<std::string>>> sections;
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string text = trim(line);
        if (text.empty()) {
            continue;
        }
        if (text.rfind("//", 0) == 0) {
            section = trim(text.substr(2));
            sections.emplace_back(section, std::vector<std::string>{});
            continue;
        }
        if (section.empty()) {
            throw std::runtime_error("shw: value before the first section in " +
                                     path);
        }
        sections.back().second.push_back(text);
    }

    ShwShow show;
    show.path = path;
    int feature_count = -1;
    int team_count = -1;
    std::vector<int> cage_numbers;
    std::vector<int> team_values;
    for (const auto& entry : sections) {
        const std::string& name = entry.first;
        const std::vector<std::string>& values = entry.second;
        if (name == "Name") {
            if (values.empty()) {
                throw std::runtime_error("shw: empty Name in " + path);
            }
            show.name = values.front();
        } else if (name == "Cost") {
            show.cost = parse_ints(values.at(0), path, name).at(0);
        } else if (name == "Judge Kind") {
            show.judge_kind = parse_ints(values.at(0), path, name).at(0);
        } else if (name == "Available cage size") {
            std::vector<int> parsed;
            for (const std::string& row : values) {
                const std::vector<int> row_values =
                    parse_ints(row, path, name);
                parsed.insert(parsed.end(), row_values.begin(),
                              row_values.end());
            }
            if (parsed.size() != 4) {
                throw std::runtime_error("shw: cage size needs 4 values in " +
                                         path);
            }
            for (int i = 0; i < 4; ++i) {
                show.cage_size[static_cast<std::size_t>(i)] =
                    parsed[static_cast<std::size_t>(i)];
            }
        } else if (name == "Dimensions") {
            std::vector<int> parsed;
            for (const std::string& row : values) {
                const std::vector<int> row_values =
                    parse_ints(row, path, name);
                parsed.insert(parsed.end(), row_values.begin(),
                              row_values.end());
            }
            if (parsed.size() != 2) {
                throw std::runtime_error("shw: Dimensions needs 2 values in " +
                                         path);
            }
            show.width = parsed[0];
            show.height = parsed[1];
        } else if (name == "Wall type") {
            show.wall_type = parse_ints(values.at(0), path, name).at(0);
        } else if (name == "Number of features") {
            feature_count = parse_ints(values.at(0), path, name).at(0);
        } else if (name == "Features") {
            for (const std::string& row : values) {
                const std::vector<int> parsed = parse_ints(row, path, name);
                if (parsed.size() != 4) {
                    throw std::runtime_error("shw: feature row needs 4 values in " +
                                             path + ": " + row);
                }
                show.features.push_back(
                    {parsed[0], parsed[1], parsed[2], parsed[3]});
            }
        } else if (name == "Team assignments") {
            team_count = parse_ints(values.at(0), path, name).at(0);
        } else if (name == "Cage number") {
            const std::vector<int> parsed = parse_ints(values.at(0), path, name);
            cage_numbers.insert(cage_numbers.end(), parsed.begin(), parsed.end());
        } else if (name == "Team") {
            const std::vector<int> parsed = parse_ints(values.at(0), path, name);
            team_values.insert(team_values.end(), parsed.begin(), parsed.end());
        } else {
            throw std::runtime_error("shw: unknown section '" + name + "' in " +
                                     path);
        }
    }

    if (show.name.empty() || show.width <= 0 || show.height <= 0) {
        throw std::runtime_error("shw: incomplete header in " + path);
    }
    if (feature_count < 0 ||
        show.features.size() != static_cast<std::size_t>(feature_count)) {
        throw std::runtime_error("shw: feature count mismatch in " + path);
    }
    if (cage_numbers.size() != team_values.size()) {
        throw std::runtime_error("shw: cage/team count mismatch in " + path);
    }
    show.teams.reserve(cage_numbers.size());
    for (std::size_t i = 0; i < cage_numbers.size(); ++i) {
        show.teams.emplace_back(cage_numbers[i], team_values[i]);
    }
    if (team_count >= 0 &&
        show.teams.size() != static_cast<std::size_t>(team_count)) {
        throw std::runtime_error("shw: team count mismatch in " + path);
    }
    return show;
}

std::vector<std::string> load_feature_list(const std::string& features_dir) {
    const std::string path = features_dir + "/FeatureList.ini";
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("shw: cannot open " + path);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string text = trim(line);
        if (!text.empty() && text.rfind("//", 0) != 0) {
            lines.push_back(text);
        }
    }
    if (lines.empty()) {
        throw std::runtime_error("shw: empty FeatureList.ini");
    }
    const int count = std::atoi(lines.front().c_str());
    if (count <= 0 || lines.size() < static_cast<std::size_t>(count) + 1) {
        throw std::runtime_error("shw: bad FeatureList.ini count");
    }
    return std::vector<std::string>(lines.begin() + 1,
                                    lines.begin() + 1 + count);
}

}  // namespace dino
