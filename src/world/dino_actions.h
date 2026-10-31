#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../formats/bai.h"
#include "danger_grid.h"

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
    // Food gauge (`dino+0x25c`): max = weight (`+0x1dc`), initial value
    // max*0.5, decay `10 * food_consumption` every 10 s, and the FOOD need is
    // `ftol(gauge * 99 / max)` (`FUN_00571370`). `food_consumption` is
    // `dino+0x6c` = weight * 0.001/s. A zero `max_food` keeps the old
    // fixture decay for synthetic tests.
    float max_food = 0.0f;
    float food_gauge = 0.0f;
    float food_consumption = 0.0f;
    // Rest gauge (`dino+0x258`, max `+0x1d8` = `def+0x188`): it starts at
    // max and each mover tick applies `gauge -= progress * scale[kind]`
    // (`FUN_00571540`, `Actions.bad` f[105+kind]); REPOS is
    // `ftol(gauge*99/max_rest)`.
    float max_rest = 0.0f;
    float rest_gauge = 0.0f;
    int size_cm = 300;        // attribute 4, actor+0x50
    int size_class = 3;       // attribute 12, actor+0x98
    int aggression = 50;      // attribute 18, actor+0xbc
    int stamina = 50;         // attribute 19, actor+0xc0
    int agility = 50;         // attribute 29, actor+0x1e0
    // Danger-grid caches (actor fields, not needs). `FUN_00571440` reads
    // them when it recomputes SECU from the grid:
    int health = 100;         // int16 +0x16
    int max_health = 100;     // int16 +0x1d4
    // `+0x1d6` regen (int16) and `+0x2bc` health state (0 good, 1 wounded,
    // 2 critical, 4 dead). `FUN_005725e0` heals `regen` every 5 s when food
    // and rest are > 0.01 and state != 4; `FUN_005724d0` applies signed
    // damage and the 67/34/0 % bands.
    int regen = 0;
    int health_state = 0;
    int threat = 0;           // +0x2a0, the value stamped into the grid
    int threat_radius = 0;    // +0x27c, the stamp radius in world units
    // Phenotype sources for the completion writers (`FUN_00571190`):
    // GetCourage copies combined_attack into threat; ObstAttackSize1 copies
    // obstacle_radius into threat_radius. These are not the caches.
    int combined_attack = 0;  // phenotype +0x44
    int obstacle_radius = 0;  // phenotype +0x80
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

// The retail action node's target policy (`node+0x3c`; vtable slot +0x14 is
// `__thiscall void GetTarget(int *x, int *y)`, +0x18 the radius, +0x28 the
// inventory index, +0x2c the amount). `x == -1` means "no target" and makes
// the chooser skip the threat term. See notes/dino-target-vtable-recon.md.
struct ActionTarget {
    enum class Kind { None, Actor, Box };
    Kind kind = Kind::None;
    int x = -1;
    int y = -1;
    int radius = 0;
    int item_index = 0;
    float amount = 0.0f;
};

// `FUN_00572dd0` + the `0x56e4d1` threat term: the target cell's danger, plus
// the actor's cell danger minus its `+0x2a0` cache when the target is inside
// the actor's `+0x27c` threat-radius box (the retail `|tx-x| <= r` test).
// `score_action` divides this by 10 before the SECU `need_term`.
int chooser_threat_score(const ActionTarget& target, int actor_x, int actor_y,
                         int actor_threat, int actor_radius,
                         const DangerGrid& grid);

using ActionThreatProvider = std::function<int(const BaiAction&)>;

// FUN_0056e410: sum the action's need deltas, add the optional node rest and
// threat terms, apply the node percentage, then add the `.bai` base score.
// Action -> animation clip. The retail link is still open (the `.bai`
// files carry no clip name); these are the name-based candidates
// documented in `notes/dino-action-chooser-recon.md` (inferred).
std::string dino_clip_for_action(const std::string& action);

// `0x0056c500..0x0056c575` (object/box Eat executor): the proximity radius
// for the arrive band. `field_18` is the `.bai` element radius and `mode`
// its `field_1c`. The four Eat actions use mode 3 (`Eat`, `EatBait`:
// `r = field_18 * body_length_cm * 0.01`) or mode 4 (`EatForSmallies`,
// `EatBaitForSmallies`: `r = field_18 * (size_radius + 1)`). The arrival test
// is `0.8*r^2 <= d^2 <= 1.2*r^2`. Other modes return the base radius; they
// do not occur in the Eat family.
float eat_arrival_radius(float field_18, int mode, int body_length_cm,
                         int size_radius);

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
    // Retail park graph: Exploration/RunAway/Sleep. Victory is class-6
    // gated and unavailable without a show cast, so it is excluded here.
    // Eat/EatForSmallies/EatBait* are the food-item behavior's action list;
    // the port models them on the same chooser (the item behavior's separate
    // arbitration is not ported yet).
    std::vector<std::string> node_names{
        "Exploration", "RunAway", "Sleep", "Eat", "EatForSmallies",
        "EatBait", "EatBaitForSmallies"};
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
    // Optional per-action threat score from the target policy. Retail
    // evaluates `GetTarget` once per distinct target per availability pass;
    // the callback must return 0 for `x == -1` (no target).
    ActionThreatProvider threat_provider;
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
    // The score and needs snapshot from the last successful decision (before
    // the action's element events ran). `tick_dino` logs these as the retail
    // selection values instead of rescoring after the events.
    int last_score() const { return last_score_; }
    const std::array<int, 4>& last_needs() const { return last_needs_; }
    const DinoNeeds& needs() const { return needs_; }
    DinoNeeds& needs() { return needs_; }

    // Restore a saved state (needs, ActionList index or 0x1f, seconds left).
    // The loop resumes without re-scoring; a valid id with 0 left finishes on
    // the next tick.
    void restore(const DinoNeeds& needs, int action_id, double action_left);

    // `FUN_00571540` (`0x5718e2`): apply one mover tick's rest-gauge change.
    // `kind` is the mover activity type (`+0x4c0`), `progress` its
    // progress-at-last-accepted-move (`+0x498`), and `scale` the
    // `Actions.bad` f[105+kind] value. kind 0 means no active record.
    void update_rest(int kind, float progress, float scale);
    float rest_gauge() const { return needs_.rest_gauge; }
    float max_rest() const { return needs_.max_rest; }

    // `FUN_005724d0(short delta)`: positive damages, negative heals. The
    // health state is 0/1/2 at >= 67 / 34..67 / < 34 %, and 4 at 0.
    void apply_health(int delta);
    int health_state() const { return needs_.health_state; }

    // Consume/item gate used by the next decision.
    void set_inventory(bool available) { config_.inventory = available; }
    bool inventory() const { return config_.inventory; }

    // Target-policy threat provider (`node+0x3c` vtable +0x14).
    void set_threat_provider(ActionThreatProvider provider) {
        config_.threat_provider = std::move(provider);
    }

    // `FUN_00571540`: the park-wide danger grid owns the SECU need. The
    // viewer pushes the recomputed value once per grid rebuild (once per
    // second in retail). Clamped to the 0..99 need range.
    void set_secu(int value) {
        needs_.value[2] = std::max(0, std::min(99, value));
    }

    // `FUN_005727b0` event 0x17: gauge = value + percent*gauge/100, clamped
    // to max_food. The FOOD need itself updates on the next 10 s pass, as in
    // retail. Used by the LunchBox/StaticBait meal (value = amount*nutrition,
    // percent = 100).
    void apply_food(float value, float percent = 100.0f) {
        if (needs_.max_food <= 0.0f) {
            return;
        }
        const float next = value + percent * needs_.food_gauge * 0.01f;
        needs_.food_gauge = std::clamp(next, 0.0f, needs_.max_food);
    }
    float food_gauge() const { return needs_.food_gauge; }

    // Number of completed actions and the name of the last one (the viewer
    // applies the Eat element once the dino reaches the feeder, before the
    // action cap, not on finish).
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
    double food_gauge_timer_ = 0.0;  // 10 s `FUN_00571370` pass
    double health_timer_ = 0.0;      // 5 s `FUN_005725e0` pass
    int action_index_ = -1;
    double elapsed_ = 0.0;
    double duration_ = 0.0;
    double decision_ = 0.0;
    int finished_count_ = 0;
    std::string last_finished_;
    int last_score_ = 0;
    std::array<int, 4> last_needs_{};
};

// FUN_0056e670: highest score >= 1 among the candidate nodes. An empty
// `node_names` means "all actions"; retail scores only the nodes present in
// the dinosaur's behavior graph, so callers should pass one.
std::optional<ActionChoice> choose_action(
    const std::vector<BaiAction>& actions, const DinoNeeds& needs,
    const std::vector<std::string>& node_names = {},
    bool inventory = false,
    const ActionThreatProvider& threat_provider = {});

// `FUN_00569A00` + `FUN_005727B0`: apply the selected action's element
// events. Retail runs this when the element executes, not on completion;
// the `.bai` need-delta table is never applied (whole-image writer census,
// notes/dino-ai-gap-audit.md §4.2). Ported kinds: 1 = VIOLENCE delta,
// 25 = REPOS from the rest-gauge percent. Other kinds write phenotype/actor
// stats (courage/perception/health) that DinoNeeds does not model yet.
void apply_action_events(const BaiAction& action, DinoNeeds& needs);

}  // namespace dino
