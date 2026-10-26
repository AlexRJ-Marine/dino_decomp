#include "dino_actions.h"

#include <algorithm>

namespace dino {
namespace {

int axis_index(NeedAxis axis) {
    return static_cast<int>(axis);
}

}  // namespace

const int kNeedWeights[4][4] = {
    {2, 15, 300, 1},  // FOOD
    {1, 10, 60, 1},   // VIOLENCE
    {2, 15, 65, 1},   // SECU
    {5, 20, 90, 1},   // REPOS
};

int need_state(int need_value) {
    if (need_value > 80) {
        return 3;
    }
    if (need_value > 60) {
        return 0;
    }
    if (need_value > 30) {
        return 1;
    }
    return 2;
}

int DinoNeeds::state(NeedAxis axis) const {
    return need_state(need(axis));
}

int DinoNeeds::attribute(std::uint32_t id) const {
    switch (id) {
        case 0:
        case 1:
        case 2:
        case 3:
            return value[id];
        case 4:
            return size_cm;
        case 12:
            return size_class;
        case 18:
            return aggression;
        case 19:
            return stamina;
        case 29:
            return agility;
        default:
            break;
    }
    if (id < extra.size()) {
        return extra[id];
    }
    return 0;
}

int need_term(const DinoNeeds& needs, NeedAxis axis, int delta) {
    const int index = axis_index(axis);
    if (index < 0 || index > 3) {
        return 0;
    }
    const int current = needs.value[static_cast<std::size_t>(index)];
    if (current + delta > 99) {
        delta = 99 - current;
    }
    const int weight = kNeedWeights[index][needs.state(axis)];
    if (delta < 1 && weight < 1) {
        return -(weight * delta);
    }
    return weight * delta;
}

int score_action(const BaiAction& action, const DinoNeeds& needs,
                 int node_percent, int node_rest, int threat_score) {
    int score = 0;
    for (const BaiNeedDelta& delta : action.need_deltas) {
        if (delta.need <= 3) {
            score += need_term(needs, static_cast<NeedAxis>(delta.need),
                               delta.delta);
        }
    }
    if (node_rest != 0) {
        score += need_term(needs, NeedAxis::Repos, node_rest / 100);
    }
    if (threat_score != 0) {
        score += need_term(needs, NeedAxis::Secu, threat_score / 10);
    }
    if (node_percent != 100 && score > 0) {
        score = node_percent * score / 100;
    }
    return static_cast<int>(action.field_32) + score;
}

bool preconditions_ok(const BaiAction& action, const DinoNeeds& needs,
                      bool inventory) {
    for (const BaiPrecondition& precondition : action.preconditions) {
        const int value = needs.attribute(precondition.attribute);
        const int threshold = static_cast<int>(precondition.threshold);
        if (precondition.comparison == 0 && value > threshold) {
            return false;
        }
        if (precondition.comparison == 1 && value < threshold) {
            return false;
        }
    }
    if (action.consume_flag != 0 && !inventory) {
        return false;
    }
    return true;
}

std::optional<ActionChoice> choose_action(
    const std::vector<BaiAction>& actions, const DinoNeeds& needs,
    const std::vector<std::string>& node_names, bool inventory) {
    std::optional<ActionChoice> best;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        const BaiAction& action = actions[i];
        if (!node_names.empty() &&
            std::find(node_names.begin(), node_names.end(), action.name) ==
                node_names.end()) {
            continue;
        }
        if (!preconditions_ok(action, needs, inventory)) {
            continue;
        }
        const int score = score_action(action, needs);
        if (score < 1) {
            continue;
        }
        if (!best || score > best->score) {
            best = ActionChoice{static_cast<int>(i), action.name, score};
        }
    }
    return best;
}

}  // namespace dino
