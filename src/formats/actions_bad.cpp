#include "actions_bad.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace dino {

ActionsBad load_actions_bad(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("actions bad: cannot open " + path);
    }
    const std::vector<std::uint8_t> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    if (data.size() != static_cast<std::size_t>(ActionsBad::kFileBytes)) {
        throw std::runtime_error("actions bad: wrong size in " + path);
    }
    std::uint32_t count = 0;
    std::memcpy(&count, data.data(), sizeof(count));
    if (count != ActionsBad::kFamilies) {
        throw std::runtime_error("actions bad: block count in " + path);
    }
    ActionsBad table;
    table.valid = true;
    for (int family = 0; family < ActionsBad::kFamilies; ++family) {
        const std::size_t block =
            static_cast<std::size_t>(family) * ActionsBad::kBlockFloats;
        const std::size_t duration_byte =
            4 + (block + ActionsBad::kDurationOffset) * sizeof(float);
        std::memcpy(table.durations[static_cast<std::size_t>(family)].data(),
                    data.data() + duration_byte,
                    sizeof(float) * ActionsBad::kKinds);
        const std::size_t scale_byte =
            4 + (block + ActionsBad::kScaleOffset) * sizeof(float);
        std::memcpy(table.scales[static_cast<std::size_t>(family)].data(),
                    data.data() + scale_byte,
                    sizeof(float) * ActionsBad::kKinds);
    }
    return table;
}

float action_duration(const ActionsBad& table, int family, int kind) {
    // Part 10 family index -> Actions.bad block. The two biped families are
    // swapped relative to the file's block order (runtime-verified on
    // Cpg04; see the header comment).
    static constexpr int kBlockForFamily[ActionsBad::kFamilies] = {0, 1, 3,
                                                                  2, 4};
    if (!table.valid || family < 0 || family >= ActionsBad::kFamilies ||
        kind < 0 || kind >= ActionsBad::kKinds) {
        return 0.0f;
    }
    return table.durations[static_cast<std::size_t>(
        kBlockForFamily[family])][static_cast<std::size_t>(kind)];
}

float action_scale(const ActionsBad& table, int family, int kind) {
    static constexpr int kBlockForFamily[ActionsBad::kFamilies] = {0, 1, 3,
                                                                   2, 4};
    if (!table.valid || family < 0 || family >= ActionsBad::kFamilies ||
        kind < 0 || kind >= ActionsBad::kKinds) {
        return 0.0f;
    }
    return table.scales[static_cast<std::size_t>(
        kBlockForFamily[family])][static_cast<std::size_t>(kind)];
}

}  // namespace dino
