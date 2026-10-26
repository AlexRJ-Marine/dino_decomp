#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../formats/bai.h"

namespace dino {

// The four dinosaur needs and the actor attributes that `.bai`
// preconditions/scorers can read. Offsets and ids: FUN_00572620.
enum class NeedAxis : int {
    Food = 0,
    Violence = 1,
    Secu = 2,
    Repos = 3,
};

struct DinoNeeds {
    std::array<int, 4> value{99, 99, 99, 99};  // FOOD/VIOLENCE/SECU/REPOS
    int size_cm = 300;        // attribute 4, actor+0x50
    int size_class = 3;       // attribute 12, actor+0x98
    int aggression = 50;      // attribute 18, actor+0xbc
    int stamina = 50;         // attribute 19, actor+0xc0
    int agility = 50;         // attribute 29, actor+0x1e0
    std::array<int, 31> extra{};  // other FUN_00572620 attributes

    int need(NeedAxis axis) const {
        return value[static_cast<std::size_t>(axis)];
    }
    int state(NeedAxis axis) const;
    int attribute(std::uint32_t id) const;
};

// GOOD/MID/BAD/full state from the [80,60,30] thresholds (FUN_005714b0).
// Non-monotonic: >80 = 3, 60..80 = 0, 30..60 = 1, <=30 = 2.
int need_state(int need_value);

// Per-need weights indexed by state (DAT_00643054 across four tables).
extern const int kNeedWeights[4][4];

// FUN_0056e5d0: cap the delta at 99, then weight * delta.
int need_term(const DinoNeeds& needs, NeedAxis axis, int delta);

// FUN_0056e410: sum the action's need deltas, add the optional node rest and
// threat terms, apply the node percentage, then add the `.bai` base score.
int score_action(const BaiAction& action, const DinoNeeds& needs,
                 int node_percent = 100, int node_rest = 0,
                 int threat_score = 0);

// FUN_0056e0e0: the `.bai` preconditions plus the consume/item gate. The item
// index comes from the target's vtable in retail; `inventory` states whether
// the caller has it.
bool preconditions_ok(const BaiAction& action, const DinoNeeds& needs,
                      bool inventory = false);

struct ActionChoice {
    int index = -1;
    std::string name;
    int score = 0;
};

// FUN_0056e670: highest score >= 1 among the candidate nodes. An empty
// `node_names` means "all actions"; retail scores only the nodes present in
// the dinosaur's behavior graph, so callers should pass one.
std::optional<ActionChoice> choose_action(
    const std::vector<BaiAction>& actions, const DinoNeeds& needs,
    const std::vector<std::string>& node_names = {},
    bool inventory = false);

}  // namespace dino
