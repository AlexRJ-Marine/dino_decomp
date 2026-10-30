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
        const std::size_t float_index =
            static_cast<std::size_t>(family) * ActionsBad::kBlockFloats +
            ActionsBad::kDurationOffset;
        const std::size_t byte_offset = 4 + float_index * sizeof(float);
        std::memcpy(table.durations[static_cast<std::size_t>(family)].data(),
                    data.data() + byte_offset,
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

}  // namespace dino
