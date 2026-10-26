#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace dino {

// One retail `.shw` show scenario (`Data/Design/Scenario/Show/*.shw`).
// Text sections: Name, Cost, Judge Kind, Available cage size, Dimensions,
// Wall type, Number of features, Features, Team assignments, Cage/Team.
struct ShwShow {
    std::string path;
    std::string name;
    int cost = 0;
    int judge_kind = 0;
    std::array<int, 4> cage_size{};
    int width = 0;
    int height = 0;
    int wall_type = 0;
    std::vector<std::array<int, 4>> features;  // index, rotation, x, y
    std::vector<std::pair<int, int>> teams;    // cage number, team
};

// Parses one file; throws std::runtime_error on a malformed/unknown section.
ShwShow load_shw(const std::string& path);

// FeatureList.ini order (62 entries) used by the feature tuples.
std::vector<std::string> load_feature_list(const std::string& features_dir);

}  // namespace dino
