#include "dino_actions.h"

#include <algorithm>
#include <cstdlib>
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

int chooser_threat_score(const ActionTarget& target, int actor_x, int actor_y,
                         int actor_threat, int actor_radius,
                         const DangerGrid& grid) {
    if (target.x < 0) {
        return 0;
    }
    int threat = -grid.at(static_cast<float>(target.x),
                          static_cast<float>(target.y));
    // FUN_00572dd0: target inside [actor +/- threat_radius] on both axes.
    if (std::abs(target.x - actor_x) <= actor_radius &&
        std::abs(target.y - actor_y) <= actor_radius) {
        threat += grid.at(static_cast<float>(actor_x),
                          static_cast<float>(actor_y)) -
                  actor_threat;
    }
    return threat;
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
    const std::vector<std::string>& node_names, bool inventory,
    const ActionThreatProvider& threat_provider) {
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
        const int threat =
            threat_provider ? threat_provider(action) : 0;
        const int score = score_action(action, needs, 100, 0, threat);
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

double DinoActionLoop::remaining() const {
    if (action_index_ < 0 || duration_ <= 0.0) {
        return 0.0;
    }
    return std::max(0.0, duration_ - elapsed_);
}

void DinoActionLoop::restore(const DinoNeeds& needs, int action_id,
                             double action_left) {
    needs_ = needs;
    for (int& value : needs_.value) {
        value = std::max(0, std::min(99, value));
    }
    if (actions_ != nullptr && action_id >= 0 &&
        static_cast<std::size_t>(action_id) < actions_->size()) {
        action_index_ = action_id;
        duration_ = std::max(0.0, action_left);
    } else {
        action_index_ = -1;
        duration_ = 0.0;
    }
    elapsed_ = 0.0;
    decision_ = config_.decision_interval;
    decay_accum_.fill(0.0);
    food_gauge_timer_ = 0.0;
    health_timer_ = 0.0;
    if (needs_.max_food > 0.0f) {
        // The port save stores the FOOD need, not the gauge; keep the two
        // consistent until the next 10 s pass overwrites the need.
        needs_.food_gauge =
            needs_.max_food * static_cast<float>(needs_.value[0]) / 99.0f;
    }
    if (needs_.max_rest > 0.0f) {
        // `FUN_00570fd0` initialises the gauge to max; a restored need maps
        // back through the same `ftol(gauge*99/max)` division.
        needs_.rest_gauge =
            needs_.max_rest * static_cast<float>(needs_.value[3]) / 99.0f;
    }
}

void DinoActionLoop::apply_health(int delta) {
    if (needs_.max_health <= 0) {
        return;
    }
    // `FUN_005724d0`: health -= signed delta, clamp [0, max]; state bands
    // 0/1/2 at >= 67 / 34..67 / < 34 %, and 4 (dead) at health < 1.
    int health = needs_.health - delta;
    health = std::clamp(health, 0, needs_.max_health);
    needs_.health = health;
    const int percent = health * 100 / needs_.max_health;
    if (percent < 67) {
        if (percent < 34) {
            needs_.health_state = health < 1 ? 4 : 2;
        } else {
            needs_.health_state = 1;
        }
    } else {
        needs_.health_state = 0;
    }
}

void DinoActionLoop::update_rest(int kind, float progress, float scale) {
    if (kind == 0 || needs_.max_rest <= 0.0f || scale == 0.0f) {
        return;
    }
    // `0x571905`: gauge -= [dino+0x498] * block[kind]; clamp to
    // [0, +0x1d8]. A negative scale (the sleep sentinel 0x1e is -10)
    // increases the gauge. `0x57195c` stores the truncating quotient.
    const float gauge =
        needs_.rest_gauge - progress * scale;
    needs_.rest_gauge = std::clamp(gauge, 0.0f, needs_.max_rest);
    needs_.value[3] = std::clamp(
        static_cast<int>(needs_.rest_gauge * 99.0f / needs_.max_rest), 0, 99);
}

void DinoActionLoop::decide() {
    if (actions_ == nullptr) {
        return;
    }
    const auto choice = choose_action(*actions_, needs_, config_.node_names,
                                      config_.inventory,
                                      config_.threat_provider);
    if (!choice) {
        return;
    }
    action_index_ = choice->index;
    elapsed_ = 0.0;
    const BaiAction& action = (*actions_)[static_cast<std::size_t>(action_index_)];
    duration_ = std::max(0.1, static_cast<double>(action.field_2e) *
                                  config_.action_scale);
    last_score_ = choice->score;
    last_needs_ = needs_.value;
    // `FUN_00569A00`: the element events run when the action starts.
    apply_action_events(action, needs_);
    decision_ = 0.0;
}

void apply_action_events(const BaiAction& action, DinoNeeds& needs) {
    for (const BaiElement& element : action.elements) {
        for (const BaiEvent& event : element.events) {
            switch (event.kind) {
                case 1:  // VIOLENCE delta (`actor+0x248`)
                    needs.value[1] += event.value;
                    break;
                case 25:  // rest gauge -> REPOS
                    needs.value[3] =
                        event.percent != 0 ? event.percent : event.value;
                    break;
                default:
                    // 0x14 courage, 0x0a perception, 0x17/0x18 food/health
                    // are actor/stat writers not modelled by DinoNeeds.
                    break;
            }
        }
    }
    for (int& value : needs.value) {
        value = std::max(0, std::min(99, value));
    }
}

void DinoActionLoop::finish_current() {
    if (action_index_ >= 0) {
        finish_action();
    }
}

void DinoActionLoop::finish_action() {    if (actions_ != nullptr && action_index_ >= 0 &&
        static_cast<std::size_t>(action_index_) < actions_->size()) {
        const BaiAction& action =
            (*actions_)[static_cast<std::size_t>(action_index_)];
        last_finished_ = action.name;
        // Retail does not apply the `.bai` need deltas (the element events
        // already ran at action start); only the completion writers below.
        // `FUN_00571190`: action 13 (GetCourage) copies phenotype +0x44 into
        // the danger-grid threat cache +0x2a0; action 26 (ObstAttackSize1)
        // copies phenotype +0x80 into the stamp radius +0x27c.
        if (action.name == "GetCourage") {
            needs_.threat = needs_.combined_attack;
        } else if (action.name == "ObstAttackSize1") {
            needs_.threat_radius = needs_.obstacle_radius;
        }
    }
    ++finished_count_;
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
        // The food axis is gauge-driven when a phenotype weight is present
        // (`FUN_00571370`); synthetic fixtures keep the configured decay.
        if (i == 0 && needs_.max_food > 0.0f) {
            decay_accum_[i] = 0.0;
            continue;
        }
        // Violence is the exact 10-point 10 s pass below, not a tuned rate.
        if (i == 1) {
            decay_accum_[i] = 0.0;
            continue;
        }
        // REPOS is the rest gauge (`FUN_00571540`), not a tuned rate.
        if (i == 3 && needs_.max_rest > 0.0f) {
            decay_accum_[i] = 0.0;
            continue;
        }
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
    // 5 s pass (`FUN_005869e0` -> `FUN_005725e0`): heal `regen` while the
    // food and rest gauges are positive and the dino is not dead. Retail
    // runs this before the 10 s pass in the same frame.
    health_timer_ += dt;
    while (health_timer_ >= 5.0) {
        health_timer_ -= 5.0;
        if (needs_.food_gauge > 0.01f && needs_.rest_gauge > 0.01f &&
            needs_.health_state != 4 && needs_.regen > 0) {
            apply_health(-needs_.regen);
        }
    }
    // 10 s pass (`FUN_005869e0`): violence decays by 10 points
    // (`FUN_00571410`), then the food gauge/need when a phenotype weight is
    // present. A gauge below the consumed amount deals starvation damage
    // `max(1, int((amount - gauge) * 0.1))` (`0x5713ba`) and clamps to 0.
    food_gauge_timer_ += dt;
    while (food_gauge_timer_ >= 10.0) {
        food_gauge_timer_ -= 10.0;
        if (needs_.value[1] > 0) {
            needs_.value[1] = std::max(0, needs_.value[1] - 10);
        }
        if (needs_.max_food > 0.0f) {
            const float amount = 10.0f * needs_.food_consumption;
            float gauge = needs_.food_gauge;
            if (gauge < amount) {
                const int damage = std::max(
                    1, static_cast<int>((amount - gauge) * 0.1f));
                apply_health(damage);
                gauge = 0.0f;
            } else {
                gauge -= amount;
            }
            needs_.food_gauge = gauge;
            needs_.value[0] = std::clamp(
                static_cast<int>(gauge * 99.0f / needs_.max_food), 0, 99);
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

std::string dino_clip_for_action(const std::string& action) {
    if (action == "Sleep") {
        return "Sleep";
    }
    if (action == "Eat" || action == "EatForSmallies" ||
        action.rfind("EatBait", 0) == 0) {
        return "Chew";
    }
    if (action == "Smell") {
        return "Sniff";
    }
    if (action == "Exploration" || action == "ReachWaypoint") {
        return "Walk";
    }
    if (action == "RunAway") {
        return "Run";
    }
    if (action == "Victory") {
        return "Winner";
    }
    if (action == "Danse") {
        return "Dance";
    }
    return "Neutral";
}

float eat_arrival_radius(float field_18, int mode, int body_length_cm,
                         int size_radius) {
    // `0x0056c52b` mode 2 uses the target's height; `0x0056c51f` mode 1 the
    // body length cache. Neither mode occurs in the four Eat actions (all
    // `.bai` files checked), so only the two proven modes are scaled here.
    switch (mode) {
        case 3:  // 0x0056c537: r = base * (target+0xf4 * 0.01)
            return field_18 * static_cast<float>(body_length_cm) * 0.01f;
        case 4:  // 0x0056c545: r = base * ((float)target+0x60 + 1.0)
            return field_18 * (static_cast<float>(size_radius) + 1.0f);
        default:
            return field_18;
    }
}

}  // namespace dino
