#include "campaign_objectives.h"

#include <algorithm>
#include <cctype>
#include <functional>

namespace dino {
namespace {

std::string lower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool profile_matches(const std::string& wanted, const std::string& value) {
    const std::string target = lower(wanted);
    return target.empty() || target == "all" || target == lower(value);
}

// `FUN_00565e60` returns 0 (surfer) for an unknown name.
int personality_index(const std::string& name) {
    static const char* const kNames[10] = {
        "surfer",  "holligan", "parent",   "blase",  "retraite",
        "riche",   "scientific", "amoureux", "spd",  "enfant"};
    const std::string wanted = lower(name);
    if (wanted == "hooligan") {
        return 1;
    }
    for (int i = 0; i < 10; ++i) {
        if (wanted == kNames[i]) {
            return i;
        }
    }
    return 0;
}

// `0x567190`: `all` requires every score >= threshold (an empty vector
// passes), `one` requires any score >= threshold (an empty vector fails),
// a name reads that personality's score.
bool show_score_matches(const CampaignShowState& show,
                        const std::string& profile, int threshold) {
    const std::string key = lower(profile);
    if (key == "all") {
        return show.profile_scores.empty() ||
               show.worst_score() >= static_cast<float>(threshold);
    }
    if (key == "one") {
        return !show.profile_scores.empty() &&
               show.best_score() >= static_cast<float>(threshold);
    }
    float value = 0.0f;
    return show.score(personality_index(key), value) &&
           value >= static_cast<float>(threshold);
}

// `0x566620` type strings map to the `.bbi` category ids. `buildings` skips
// viewpoint/seats (5/6), which the helper handles in their own blocks.
bool building_type_matches(const std::string& wanted, int category) {
    const std::string type = lower(wanted);
    if (type.empty() || type == "all") {
        return true;
    }
    if (type == "buildings") {
        return category >= 0 && category <= 12 && category != 5 &&
               category != 6;
    }
    if (type == "park") return category == 0;
    if (type == "restaurant") return category == 1;
    if (type == "self") return category == 2;
    if (type == "shop") return category == 3;
    if (type == "blackbox") return category == 4;
    if (type == "pointofview") return category == 5;
    if (type == "show" || type == "seats") return category == 6;
    if (type == "security") return category == 7;
    if (type == "farm") return category == 8;
    if (type == "station") return category == 9;
    if (type == "entrance") return category == 10;
    if (type == "lunchdoor") return category == 11;
    if (type == "decoration") return category == 12;
    return false;
}

int count_buildings(const CampaignState& state, const std::string& type,
                    bool require_connected) {
    if (lower(type) == "fence") {
        // The retail `fence` block counts fence objects, not building
        // categories; the port tracks one rectangle per paddock.
        return state.fence_count;
    }
    int count = 0;
    for (const CampaignBuildingState& building : state.buildings) {
        if (!building_type_matches(type, building.category)) {
            continue;
        }
        if (require_connected && !building.connected) {
            continue;
        }
        ++count;
    }
    return count;
}

int arg_int(const CampaignObjective& objective, std::size_t index,
            int fallback = 0) {
    if (index >= objective.args.size()) {
        return fallback;
    }
    try {
        return std::stoi(objective.args[index]);
    } catch (const std::exception&) {
        return fallback;
    }
}

std::string arg_str(const CampaignObjective& objective, std::size_t index) {
    return index < objective.args.size() ? objective.args[index] : "";
}

bool species_matches(const std::string& filter, const std::string& species) {
    const std::string wanted = lower(filter);
    if (wanted.empty() || wanted == "all") {
        return true;
    }
    const std::string full = lower(species);
    // Retail objective filters use short names (hypsilo, coelo, ...); the
    // genotype species are the full names (Hypsilophodon, Coelophysis, ...).
    return full == wanted ||
           full.compare(0, wanted.size(), wanted) == 0;
}

ObjectiveVerdict count_trait(const CampaignState& state, int required,
                             const std::function<bool(const DinoTraitState&)>& match) {
    if (required <= 0) {
        return ObjectiveVerdict::Pending;
    }
    int count = 0;
    for (const DinoTraitState& dino : state.dinos) {
        if (match(dino)) {
            ++count;
        }
    }
    return count >= required ? ObjectiveVerdict::Success
                             : ObjectiveVerdict::Pending;
}

}  // namespace

int CampaignState::visitor_count(const std::string& profile) const {
    int count = 0;
    for (const CampaignVisitorState& visitor : visitors) {
        if (profile_matches(profile, visitor.profile)) {
            ++count;
        }
    }
    return count;
}

int CampaignState::employee_count(const std::string& profile) const {
    int count = 0;
    for (const std::string& role : employee_roles) {
        if (profile_matches(profile, role)) {
            ++count;
        }
    }
    return count;
}

void CampaignShowState::add_score(int personality, float score) {
    profile_scores.push_back({personality, score});
}

void CampaignShowState::zero_scores(const std::vector<int>& personalities) {
    profile_scores.clear();
    profile_scores.reserve(personalities.size());
    for (int personality : personalities) {
        profile_scores.push_back({personality, 0.0f});
    }
}

void CampaignShowState::set_score(int personality, float score) {
    for (CampaignShowProfileScore& entry : profile_scores) {
        if (entry.personality == personality) {
            entry.score = score;
            return;
        }
    }
    profile_scores.push_back({personality, score});
}

bool CampaignShowState::score(int personality, float& out) const {
    for (const CampaignShowProfileScore& entry : profile_scores) {
        if (entry.personality == personality) {
            out = entry.score;
            return true;
        }
    }
    return false;
}

float CampaignShowState::best_score() const {
    float best = -1.0e9f;
    for (const CampaignShowProfileScore& entry : profile_scores) {
        best = std::max(best, entry.score);
    }
    return best;
}

float CampaignShowState::worst_score() const {
    float worst = 1.0e9f;
    for (const CampaignShowProfileScore& entry : profile_scores) {
        worst = std::min(worst, entry.score);
    }
    return worst;
}

const char* verdict_name(ObjectiveVerdict verdict) {
    switch (verdict) {
        case ObjectiveVerdict::Success:
            return "success";
        case ObjectiveVerdict::Failure:
            return "failure";
        case ObjectiveVerdict::Pending:
        default:
            return "pending";
    }
}

ObjectiveVerdict evaluate_objective(const CampaignObjective& objective,
                                    const CampaignState& state) {
    switch (objective.objective_id) {
        case 0: {  // treasury target [deadline]
            const int target = arg_int(objective, 0);
            const int deadline = arg_int(objective, 1, 1);
            if (state.treasury >= target) {
                return ObjectiveVerdict::Success;
            }
            return deadline <= 0 ? ObjectiveVerdict::Failure
                                 : ObjectiveVerdict::Pending;
        }
        case 1:  // marchandizing target
            return state.merchandise_income > arg_int(objective, 0)
                       ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Pending;
        case 2:  // dyingdinos max
            return state.deaths <= arg_int(objective, 0)
                       ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Failure;
        case 3:  // aggressivedinos count percent
            return count_trait(state, arg_int(objective, 0),
                               [&](const DinoTraitState& dino) {
                                   return dino.aggression >= arg_int(objective, 1);
                               });
        case 4: {  // genotypedinos species count
            const std::string filter = arg_str(objective, 0);
            return count_trait(state, arg_int(objective, 1),
                               [&](const DinoTraitState& dino) {
                                   return species_matches(filter, dino.species);
                               });
        }
        case 5:  // cutedinos count percent
            return count_trait(state, arg_int(objective, 0),
                               [&](const DinoTraitState& dino) {
                                   return dino.cuteness >= arg_int(objective, 1);
                               });
        case 6:  // hybriddinos count percent
            return count_trait(state, arg_int(objective, 0),
                               [&](const DinoTraitState& dino) {
                                   return dino.hybrid >= arg_int(objective, 1);
                               });
        case 7:  // notconnectedbuildings type count (mode 0 accepts all)
            return count_buildings(state, arg_str(objective, 0), false) >=
                           arg_int(objective, 1)
                       ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Pending;
        case 8:  // connectedbuildings type count (mode 1 requires +0x34)
            return count_buildings(state, arg_str(objective, 0), true) >=
                           arg_int(objective, 1)
                       ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Pending;
        case 9: {  // visitors profile count
            const int required = arg_int(objective, 1);
            return state.visitor_count(arg_str(objective, 0)) >= required
                       ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Pending;
        }
        case 11:  // speeddinos count speed
            return count_trait(state, arg_int(objective, 0),
                               [&](const DinoTraitState& dino) {
                                   return dino.speed >= arg_int(objective, 1);
                               });
        case 12:  // staminadinos count percent
            return count_trait(state, arg_int(objective, 0),
                               [&](const DinoTraitState& dino) {
                                   return dino.stamina >= arg_int(objective, 1);
                               });
        case 13:  // cleverdinos count percent
            return count_trait(state, arg_int(objective, 0),
                               [&](const DinoTraitState& dino) {
                                   return dino.intelligence >= arg_int(objective, 1);
                               });
        case 14:  // sizedagilitydinos min_size_class min_agility
            // `0x569040`: pattern[8] = arg0 (+0x1e0 size class), pattern[5] =
            // arg1 (+0x98 agility); `0x567cb0` mode 2 requires all pattern
            // fields to be >= thresholds.
            for (const DinoTraitState& dino : state.dinos) {
                if (dino.size_class >= arg_int(objective, 0) &&
                    dino.agility >= arg_int(objective, 1)) {
                    return ObjectiveVerdict::Success;
                }
            }
            return ObjectiveVerdict::Pending;
        case 15: {  // franckstrangedinos max_age min_intelligence
            // `0x569110`: any dino with +0x1b0==1 (carnivore), +0x1b4==0
            // (biped), +0x190>=2 (size class), +0x6c<age argument and
            // +0x3c>intelligence argument. The port starts imported dinos at
            // age 0.
            const int max_age = arg_int(objective, 0);
            const int min_intelligence = arg_int(objective, 1);
            for (const DinoTraitState& dino : state.dinos) {
                if (dino.carnivore && dino.biped &&
                    dino.size_class >= 2 && dino.age < max_age &&
                    dino.intelligence > min_intelligence) {
                    return ObjectiveVerdict::Success;
                }
            }
            return ObjectiveVerdict::Pending;
        }
        case 16: {  // employees profile count
            const int required = arg_int(objective, 1);
            return state.employee_count(arg_str(objective, 0)) >= required
                       ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Pending;
        }
        case 17: {  // hungrydinos count food_threshold (count -1 = all)
            // `0x5689e0`: pattern[4] = +0x244 food, mode 2 (`>=`), count -1
            // means every dinosaur. Cpg01_Mission7 `-1 60` = no dino below
            // 60 food.
            const int required = arg_int(objective, 0, -1);
            const int threshold = arg_int(objective, 1);
            int fed = 0;
            for (const DinoTraitState& dino : state.dinos) {
                if (dino.food >= threshold) {
                    ++fed;
                }
            }
            const int need = required < 0
                                 ? static_cast<int>(state.dinos.size())
                                 : required;
            return fed >= need ? ObjectiveVerdict::Success
                               : ObjectiveVerdict::Pending;
        }
        case 18: {  // isolateddinos aggression_threshold (all aggressive alone)
            // `0x568d20`: pattern[1] = +0xbc aggression, requiredCount -2
            // selects every matching dino, then each is checked for an
            // isolation tag. The port models that as "alone in its paddock".
            const int threshold = arg_int(objective, 0, 99);
            for (const DinoTraitState& dino : state.dinos) {
                if (dino.aggression >= threshold && !dino.isolated) {
                    return ObjectiveVerdict::Pending;
                }
            }
            return ObjectiveVerdict::Success;
        }
        case 19: {  // happyvisitors profile percent
            const int required = arg_int(objective, 1);
            bool any = false;
            for (const CampaignVisitorState& visitor : state.visitors) {
                if (!profile_matches(arg_str(objective, 0), visitor.profile)) {
                    continue;
                }
                any = true;
                if (visitor.satisfaction < required) {
                    return ObjectiveVerdict::Pending;
                }
            }
            return any ? ObjectiveVerdict::Success : ObjectiveVerdict::Pending;
        }
        case 22:  // ornateddinos count
            return count_trait(state, arg_int(objective, 0),
                               [](const DinoTraitState& dino) {
                                   return dino.ornament_count > 0;
                               });
        case 23:  // cameraaltitude height
            return state.camera_altitude >= arg_int(objective, 0)
                       ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Pending;
        case 24: {  // happyshow profile score
            const std::string profile = arg_str(objective, 0);
            const int threshold = arg_int(objective, 1);
            for (const CampaignShowState& show : state.shows) {
                if (show.participants > 0 &&
                    show_score_matches(show, profile, threshold)) {
                    return ObjectiveVerdict::Success;
                }
            }
            return ObjectiveVerdict::Pending;
        }
        case 25: {  // fightshow score (unused by retail)
            const int threshold = arg_int(objective, 0);
            for (const CampaignShowState& show : state.shows) {
                if (show.type == 3 && show.participants > 0 &&
                    show_score_matches(show, "holligan", threshold)) {
                    return ObjectiveVerdict::Success;
                }
            }
            return ObjectiveVerdict::Pending;
        }
        case 26: {  // scientificshow score strangeness
            const int threshold = arg_int(objective, 0);
            const int cap = arg_int(objective, 1);
            for (const CampaignShowState& show : state.shows) {
                float value = 0.0f;
                // The scientific profile entry is hard-coded (`0x5678cd`).
                if (!show.score(6, value) ||
                    !(value > static_cast<float>(threshold))) {
                    continue;
                }
                if (show.participant_hybrid.empty()) {
                    continue;
                }
                // `0x5679e3`: any participant with +0x1E4 > cap rejects it.
                bool all_within_cap = true;
                for (const int hybrid : show.participant_hybrid) {
                    if (hybrid > cap) {
                        all_within_cap = false;
                        break;
                    }
                }
                if (all_within_cap) {
                    return ObjectiveVerdict::Success;
                }
            }
            return ObjectiveVerdict::Pending;
        }
        case 27: {  // happydinoshow score participants
            const int threshold = arg_int(objective, 0);
            const int participants = arg_int(objective, 1);
            for (const CampaignShowState& show : state.shows) {
                if (show.participants >= participants &&
                    show_score_matches(show, "all", threshold)) {
                    return ObjectiveVerdict::Success;
                }
            }
            return ObjectiveVerdict::Pending;
        }
        case 28:  // delaylimit limit (frac: pending while clock <= load + limit)
            return state.clock_minutes > state.load_minutes + arg_int(objective, 0)
                       ? ObjectiveVerdict::Failure
                       : ObjectiveVerdict::Pending;
        case 29:  // buyalldinos
            return state.store_empty ? ObjectiveVerdict::Success
                                     : ObjectiveVerdict::Pending;
        case 31: {  // prettystrangedino beauty strange
            for (const DinoTraitState& dino : state.dinos) {
                if (dino.beauty > arg_int(objective, 0) &&
                    dino.strangeness < arg_int(objective, 1)) {
                    return ObjectiveVerdict::Success;
                }
            }
            return ObjectiveVerdict::Pending;
        }
        case 30: {  // dinoexclusiveshow
            // `0x5692a0`: a dino present in two different shows fails; an
            // empty show list succeeds and duplicates inside one show are
            // ignored (only later shows are checked).
            for (std::size_t i = 0; i < state.shows.size(); ++i) {
                for (std::size_t j = i + 1; j < state.shows.size(); ++j) {
                    for (const int id : state.shows[i].participant_ids) {
                        for (const int other : state.shows[j].participant_ids) {
                            if (id == other) {
                                return ObjectiveVerdict::Pending;
                            }
                        }
                    }
                }
            }
            return ObjectiveVerdict::Success;
        }
        case 32:  // bankrupt
            return state.bankrupt ? ObjectiveVerdict::Failure
                                  : ObjectiveVerdict::Pending;
        default:
            // Verbs that need systems the port does not model yet stay
            // pending instead of guessing.
            return ObjectiveVerdict::Pending;
    }
}

ObjectiveVerdict aggregate_verdict(
    const std::vector<ObjectiveVerdict>& verdicts) {
    if (verdicts.empty()) {
        return ObjectiveVerdict::Pending;
    }
    bool all_success = true;
    for (ObjectiveVerdict verdict : verdicts) {
        if (verdict == ObjectiveVerdict::Failure) {
            return ObjectiveVerdict::Failure;
        }
        if (verdict != ObjectiveVerdict::Success) {
            all_success = false;
        }
    }
    return all_success ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Pending;
}

std::vector<ObjectiveVerdict> evaluate_mission(
    const CampaignMission& mission, const CampaignState& state) {
    std::vector<ObjectiveVerdict> verdicts;
    verdicts.reserve(mission.objectives.size());
    for (const CampaignObjective& objective : mission.objectives) {
        verdicts.push_back(evaluate_objective(objective, state));
    }
    return verdicts;
}

}  // namespace dino
