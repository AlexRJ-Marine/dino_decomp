#pragma once

#include <array>
#include <string>
#include <vector>

#include "../formats/campaign.h"

namespace dino {

// `snoObjective` verdicts: 0 pending, 1 success, 2 failure.
enum class ObjectiveVerdict : int {
    Pending = 0,
    Success = 1,
    Failure = 2,
};

// The ten trait slots `FUN_00567cb0` reads, plus the two extra fields used by
// `prettystrangedino` and `ornateddinos`.
struct DinoTraitState {
    std::string species;
    int cuteness = 0;      // +0x90
    int aggression = 0;    // +0xbc
    int hybrid = 0;        // +0x1e4
    int scarcity = 0;      // +0x1e8
    int food = 99;         // +0x244
    int agility = 0;       // +0x98 (sizedagilitydinos second arg)
    int speed = 0;         // int(+0x7c)
    int stamina = 0;       // +0xc0
    int size_class = 0;    // +0x1e0 (sizedagilitydinos first arg)
    int intelligence = 0;  // +0x8c
    int beauty = 0;        // +0x40
    int strangeness = 0;   // +0x194
    int ornament_count = 0;  // non-empty slots in the 23-slot +0xE8 array
    // `franckstrangedinos`: actor +0x1b0/+0x1b4/+0x6c.
    bool carnivore = false;
    bool biped = false;
    int age = 0;
    // `isolateddinos`: the aggressive dino is alone in its enclosure.
    bool isolated = false;
};

struct CampaignVisitorState {
    std::string profile;
    int satisfaction = 99;
};

// One active placed building for `connectedbuildings` /
// `notconnectedbuildings`. `category` is the `.bbi` category (0..12).
struct CampaignBuildingState {
    int category = -1;
    bool connected = false;  // a path cell touches the footprint
};

// One show arena with its assigned dinosaurs. `type` is the judge-kind family
// (`FUN_00567190`): 1 race, 2 obstacle, 3 fight, 4 destruction, 5 team,
// 6 capture, 7 fence. `profile_scores` mirrors the retail dynamic
// `[show+0xEC]+0x6C` vector<float> in the active visitor-profile order; each
// entry records the VisitorList.ini personality it belongs to.
struct CampaignShowProfileScore {
    int personality = -1;  // 0 surfer .. 9 enfant
    float score = 0.0f;
};

struct CampaignShowState {
    int type = 0;
    int participants = 0;  // show+0xB4 - show+0xB0 >> 2
    std::vector<CampaignShowProfileScore> profile_scores;
    std::vector<int> participant_ids;     // identity (index in state.dinos)
    std::vector<int> participant_hybrid;  // +0x1E4 per participant

    void add_score(int personality, float score);
    // Replaces the vector with one zero entry per personality, matching the
    // retail zero-filled `[show+0xEC]+0x6C` vector at placement
    // (`FUN_00580f00` -> `FUN_00576860`).
    void zero_scores(const std::vector<int>& personalities);
    // Replaces an existing personality's score or appends a new entry.
    void set_score(int personality, float score);
    bool score(int personality, float& out) const;
    float best_score() const;   // max over the vector (-1e9 when empty)
    float worst_score() const;  // min over the vector (+1e9 when empty)
};

struct CampaignState {
    int treasury = 0;
    int merchandise_income = 0;
    int deaths = 0;
    int camera_altitude = 0;
    int clock_minutes = 0;
    int load_minutes = 0;
    bool store_empty = false;
    bool bankrupt = false;
    std::vector<DinoTraitState> dinos;
    std::vector<CampaignVisitorState> visitors;
    std::vector<std::string> employee_roles;
    std::vector<CampaignBuildingState> buildings;
    std::vector<CampaignShowState> shows;
    int fence_count = 0;  // fence rectangles / paddocks (`fence` type)

    int visitor_count(const std::string& profile) const;
    int employee_count(const std::string& profile) const;
};

// Evaluates one parsed objective. Verbs that need unported systems (shows,
// the show/hybrid fields not yet in the port) return Pending.
ObjectiveVerdict evaluate_objective(const CampaignObjective& objective,
                                    const CampaignState& state);

// Aggregate over a mission: any failure -> failure; all success (and at least
// one objective) -> success; otherwise pending.
ObjectiveVerdict aggregate_verdict(
    const std::vector<ObjectiveVerdict>& verdicts);

// Convenience: evaluate every objective of a mission.
std::vector<ObjectiveVerdict> evaluate_mission(
    const CampaignMission& mission, const CampaignState& state);

// "pending" / "success" / "failure" for diagnostics.
const char* verdict_name(ObjectiveVerdict verdict);

}  // namespace dino
