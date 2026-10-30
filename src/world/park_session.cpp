#include "park_session.h"

#include <algorithm>

namespace dino {

void ParkSession::advance_clock(double dt) {
    clock.advance(dt);
}

int ParkSession::update_staff(double dt) {
    int assigned = 0;
    if (clock.new_hour && !employee_system.employees.empty()) {
        assigned = employee_system.start_hour(employee_profiles, food_sources,
                                              food_defs);
    }
    employee_system.update(dt, employee_profiles, food_sources, food_defs);
    return assigned;
}

ParkSession::DaySettlement ParkSession::settle_day(int ticket_price) {
    DaySettlement result;
    result.visitors = static_cast<int>(visitor_system.visitors.size());
    const std::vector<VisitorProfile>& profiles =
        all_visitor_profiles.empty() ? visitor_profiles
                                     : all_visitor_profiles;
    for (const Visitor& visitor : visitor_system.visitors) {
        int index = visitor.global_profile;
        if (index < 0 || index >= static_cast<int>(profiles.size())) {
            index = visitor.profile;
        }
        const float scale =
            (index >= 0 && index < static_cast<int>(profiles.size()))
                ? profiles[static_cast<std::size_t>(index)].price_scale
                : 1.0f;
        result.tickets +=
            static_cast<int>(static_cast<float>(ticket_price) * scale);
    }
    result.wages = employee_system.daily_wages(employee_profiles);
    for (const Employee& employee : employee_system.employees) {
        if (employee.profile < 0 ||
            employee.profile >=
                static_cast<int>(employee_profiles.size())) {
            continue;
        }
        const EmployeeProfile& profile =
            employee_profiles[static_cast<std::size_t>(employee.profile)];
        ledger.spend_staff(profile.kind, profile.wage);
    }
    budget += result.tickets - result.wages;
    ledger.add_ticket(result.tickets, result.visitors);
    ledger.settle();
    return result;
}

int ParkSession::paddock_at(float x, float y) const {
    for (std::size_t p = 0; p < paddocks.size(); ++p) {
        const WorldRect& r = paddocks[p].rect;
        if (x >= static_cast<float>(std::min(r.x0, r.x1)) &&
            x <= static_cast<float>(std::max(r.x0, r.x1)) &&
            y >= static_cast<float>(std::min(r.y0, r.y1)) &&
            y <= static_cast<float>(std::max(r.y0, r.y1))) {
            return static_cast<int>(p);
        }
    }
    return -1;
}

FoodSource* ParkSession::paddock_food(int paddock, bool carnivore) {
    if (paddock < 0 ||
        paddock >= static_cast<int>(paddocks.size())) {
        return nullptr;
    }
    const WorldRect& r = paddocks[static_cast<std::size_t>(paddock)].rect;
    for (FoodSource& source : food_sources) {
        if (source.remaining <= 0.0f || source.def < 0 ||
            source.def >= static_cast<int>(food_defs.size())) {
            continue;
        }
        if (food_is_carnivore(
                food_defs[static_cast<std::size_t>(source.def)].kind) !=
            carnivore) {
            continue;
        }
        if (source.x >= static_cast<float>(std::min(r.x0, r.x1)) &&
            source.x <= static_cast<float>(std::max(r.x0, r.x1)) &&
            source.y >= static_cast<float>(std::min(r.y0, r.y1)) &&
            source.y <= static_cast<float>(std::max(r.y0, r.y1))) {
            return &source;
        }
    }
    return nullptr;
}

ParkSession::DinoActionEvent ParkSession::tick_dino(
    ImportedDino& dinosaur, double dt,
    const std::vector<BaiAction>& actions) {
    DinoActionEvent event;
    if (!dinosaur.action_loop) {
        return event;
    }
    // The `.bai` consume gate: Eat is only available when the dinosaur's
    // paddock holds a matching food source.
    const int pen = paddock_at(dinosaur.x, dinosaur.y);
    dinosaur.action_loop->set_inventory(
        paddock_food(pen, dinosaur.carnivore) != nullptr);
    const int finished_before = dinosaur.action_loop->finished_count();
    const std::string previous = dinosaur.action_loop->action_name();
    dinosaur.action_loop->tick(dt);
    if (dinosaur.action_loop->finished_count() != finished_before) {
        const std::string& finished =
            dinosaur.action_loop->last_finished_action();
        if (finished == "Eat" || finished == "EatForSmallies") {
            FoodSource* source = paddock_food(pen, dinosaur.carnivore);
            if (source != nullptr) {
                source->remaining = std::max(0.0f, source->remaining - 1.0f);
                event.ate = true;
                event.food_name =
                    food_defs[static_cast<std::size_t>(source->def)].name;
                event.remaining = source->remaining;
                event.food_depleted = source->remaining <= 0.0f;
            }
        }
    }
    const std::string& action = dinosaur.action_loop->action_name();
    if (action != previous) {
        event.action_changed = true;
        event.action = action;
        if (action != "idle") {
            const BaiAction& definition =
                actions[static_cast<std::size_t>(
                    dinosaur.action_loop->action_index())];
            event.scored = true;
            event.score = score_action(definition, dinosaur.action_loop->needs());
            const DinoNeeds& needs = dinosaur.action_loop->needs();
            event.needs = {needs.value[0], needs.value[1], needs.value[2],
                           needs.value[3]};
        }
        event.clip =
            action == "idle" ? "Neutral" : dino_clip_for_action(action);
    }
    return event;
}

}  // namespace dino
