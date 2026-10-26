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

int arg_int(const CampaignObjective& objective, std::size_t index,
            int fallback = 0) {
    if (index >= objective.args.size()) {
        return fallback;
    }
    try {
        return std::stoi(objective.args[index]);
    } catch (...) {
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
        case 16: {  // employees profile count
            const int required = arg_int(objective, 1);
            return state.employee_count(arg_str(objective, 0)) >= required
                       ? ObjectiveVerdict::Success
                       : ObjectiveVerdict::Pending;
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
        case 32:  // bankrupt
            return state.bankrupt ? ObjectiveVerdict::Failure
                                  : ObjectiveVerdict::Pending;
        default:
            // Shows, connected buildings, isolation tags, hungry/carnivore
            // variants and the unused verbs need systems the port does not
            // model yet; keep them pending instead of guessing.
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
