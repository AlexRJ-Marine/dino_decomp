#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dino {

// One `CanDoAction` gate. Runtime order is (threshold, attribute, comparison);
// comparison 0 means value <= threshold, 1 means value >= threshold.
struct BaiPrecondition {
    std::uint32_t threshold = 0;
    std::uint32_t attribute = 0;
    std::uint32_t comparison = 0;
};

// One term of the element score. Runtime order is (unused, weight, attribute).
struct BaiWeight {
    std::uint32_t unused = 0;
    std::uint32_t weight = 0;
    std::uint32_t attribute = 0;
};

struct BaiNeedDelta {
    std::int32_t delta = 0;
    std::uint32_t need = 0;
};

struct BaiGrant {
    std::uint32_t value = 0;
    std::uint32_t attribute = 0;
};

struct BaiEvent {
    std::uint32_t kind = 0;  // effect id, not an ActionList index
    std::int32_t value = 0;
    std::int32_t percent = 0;
};

// 0x58-byte runtime element; the file stores the same fields in this order.
struct BaiElement {
    std::uint32_t time = 0;
    std::vector<std::uint32_t> bones;
    float duration = 0.0f;
    std::string texture;
    std::vector<BaiEvent> events;
    std::array<std::uint32_t, 9> params{};
};

// One `.bai` action. The vectors are exposed in runtime order.
struct BaiAction {
    std::string name;
    std::uint32_t field_14 = 0;  // element-score jitter range
    float field_18 = 0.0f;       // base radius/scale
    std::uint32_t field_1c = 0;  // radius scale mode
    std::uint8_t flag_20 = 0;    // reserved
    std::uint32_t special_action = 0;  // 0 normal, 1..6 special groups
    std::uint8_t eat_flag = 0;
    std::uint32_t field_26 = 0;  // end-ramp percent
    float field_2a = 0.0f;       // end-ramp value
    float field_2e = 0.0f;       // action duration seconds
    std::uint32_t field_32 = 0;  // additive behavior score
    std::array<std::uint32_t, 8> params{};  // percent score multipliers
    std::uint8_t consume_flag = 0;
    std::vector<BaiPrecondition> preconditions;
    std::vector<BaiWeight> effect_weights;
    std::vector<BaiNeedDelta> need_deltas;
    std::vector<BaiGrant> stat_grants;
    std::uint32_t effect_class = 0;
    std::vector<BaiElement> elements;
};

BaiAction load_bai(const std::string& path);
std::vector<std::string> load_action_list(const std::string& path);
std::vector<BaiAction> load_action_table(const std::string& asset_root);

}  // namespace dino
