#include "dino_actions.h"

#include <algorithm>
#include <utility>

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

DinoActionLoop::DinoActionLoop(const std::vector<BaiAction>* actions,
                               DinoNeeds needs, DinoActionLoopConfig config)
    : actions_(actions), needs_(needs), config_(std::move(config)) {
    for (int& value : needs_.value) {
        value = std::max(0, std::min(99, value));
    }
    // Decide on the first tick so a forced need state is visible immediately.
    decision_ = config_.decision_interval;
}

int DinoActionLoop::action_id() const {
    return action_index_ < 0 ? 0x1f : action_index_;
}

const std::string& DinoActionLoop::action_name() const {
    static const std::string idle = "idle";
    if (action_index_ < 0 || actions_ == nullptr ||
        static_cast<std::size_t>(action_index_) >= actions_->size()) {
        return idle;
    }
    return (*actions_)[static_cast<std::size_t>(action_index_)].name;
}

double DinoActionLoop::action_progress() const {
    if (action_index_ < 0 || duration_ <= 0.0) {
        return 0.0;
    }
    return std::min(1.0, elapsed_ / duration_);
}

void DinoActionLoop::decide() {
    if (actions_ == nullptr) {
        return;
    }
    const auto choice =
        choose_action(*actions_, needs_, config_.node_names, true);
    if (!choice) {
        return;
    }
    action_index_ = choice->index;
    elapsed_ = 0.0;
    const BaiAction& action = (*actions_)[static_cast<std::size_t>(action_index_)];
    duration_ = std::max(0.1, static_cast<double>(action.field_2e) *
                                  config_.action_scale);
    decision_ = 0.0;
}

void DinoActionLoop::finish_action() {
    if (actions_ != nullptr && action_index_ >= 0 &&
        static_cast<std::size_t>(action_index_) < actions_->size()) {
        for (const BaiNeedDelta& delta :
             (*actions_)[static_cast<std::size_t>(action_index_)].need_deltas) {
            if (delta.need > 3) {
                continue;
            }
            int& value = needs_.value[delta.need];
            value = std::max(0, std::min(99, value + delta.delta));
        }
    }
    action_index_ = -1;
    elapsed_ = 0.0;
    duration_ = 0.0;
    decision_ = config_.decision_interval;
}

void DinoActionLoop::tick(double dt) {
    if (dt <= 0.0) {
        return;
    }
    const double rates[4] = {config_.food_decay, config_.violence_decay,
                             config_.secu_decay, config_.repos_decay};
    for (std::size_t i = 0; i < needs_.value.size(); ++i) {
        int& value = needs_.value[i];
        if (value <= 0 || rates[i] <= 0.0) {
            decay_accum_[i] = 0.0;
            continue;
        }
        decay_accum_[i] += rates[i] * dt;
        while (decay_accum_[i] >= 1.0 && value > 0) {
            --value;
            decay_accum_[i] -= 1.0;
        }
    }

    if (action_index_ >= 0) {
        elapsed_ += dt;
        if (elapsed_ >= duration_) {
            finish_action();
        }
    }

    decision_ += dt;
    if (action_index_ < 0 && decision_ >= config_.decision_interval) {
        decide();
    }
}

}  // namespace dino
