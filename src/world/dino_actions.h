#pragma once

#include <array>
#include <cstdint>
#include <functional>
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

// Minimal per-dinosaur need/action loop. Decay rates are port tuning (the raw
// gauge constants are not calibrated yet); the chooser itself is the retail
// algorithm. The `.bai` need deltas are applied when an action completes so
// the loop closes; retail's feeding interaction is the open part.
struct DinoActionLoopConfig {
    std::vector<std::string> node_names{"Exploration", "Eat", "EatForSmallies",
                                        "Sleep"};
    double decision_interval = 0.5;
    double food_decay = 0.5;      // need points per second
    double violence_decay = 0.2;
    double secu_decay = 0.0;
    double repos_decay = 0.5;
    double action_scale = 1.0;    // multiplies the `.bai` field_2e duration
    // The `.bai` consume/item gate: retail only offers Eat when the target
    // food source exists. The viewer sets this per dinosaur from the food
    // sources in its paddock.
    bool inventory = false;
};

class DinoActionLoop {
public:
    DinoActionLoop(const std::vector<BaiAction>* actions, DinoNeeds needs,
                   DinoActionLoopConfig config = {});

    // Advance decay, action countdown and the decision timer by `dt` seconds.
    void tick(double dt);

    bool busy() const { return action_index_ >= 0; }
    int action_index() const { return action_index_; }   // ActionList index
    int action_id() const;                               // 0x1f when idle
    const std::string& action_name() const;
    double action_progress() const;                      // 0..1
    double remaining() const;                            // seconds left
    const DinoNeeds& needs() const { return needs_; }
    DinoNeeds& needs() { return needs_; }

    // Restore a saved state (needs, ActionList index or 0x1f, seconds left).
    // The loop resumes without re-scoring; a valid id with 0 left finishes on
    // the next tick.
    void restore(const DinoNeeds& needs, int action_id, double action_left);

    // Consume/item gate used by the next decision.
    void set_inventory(bool available) { config_.inventory = available; }
    bool inventory() const { return config_.inventory; }

    // Number of completed actions and the name of the last one (the viewer
    // consumes a food portion when an Eat/EatForSmallies finishes).
    int finished_count() const { return finished_count_; }
    const std::string& last_finished_action() const {
        return last_finished_;
    }

private:
    void decide();
    void finish_action();

    const std::vector<BaiAction>* actions_ = nullptr;
    DinoNeeds needs_;
    DinoActionLoopConfig config_;
    std::array<double, 4> decay_accum_{};
    int action_index_ = -1;
    double elapsed_ = 0.0;
    double duration_ = 0.0;
    double decision_ = 0.0;
    int finished_count_ = 0;
    std::string last_finished_;
};

// FUN_0056e670: highest score >= 1 among the candidate nodes. An empty
// `node_names` means "all actions"; retail scores only the nodes present in
// the dinosaur's behavior graph, so callers should pass one.
std::optional<ActionChoice> choose_action(
    const std::vector<BaiAction>& actions, const DinoNeeds& needs,
    const std::vector<std::string>& node_names = {},
    bool inventory = false);

}  // namespace dino
