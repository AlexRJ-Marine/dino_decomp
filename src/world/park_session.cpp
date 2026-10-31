#include "park_session.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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

FoodSource* ParkSession::paddock_food(int paddock, bool carnivore) {    if (paddock < 0 ||
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

Feeder* ParkSession::paddock_feeder(int paddock, bool carnivore, float x,
                                    float y) {
    if (paddock < 0 ||
        paddock >= static_cast<int>(paddocks.size())) {
        return nullptr;
    }
    const WorldRect& r = paddocks[static_cast<std::size_t>(paddock)].rect;
    const float min_x = static_cast<float>(std::min(r.x0, r.x1));
    const float max_x = static_cast<float>(std::max(r.x0, r.x1));
    const float min_y = static_cast<float>(std::min(r.y0, r.y1));
    const float max_y = static_cast<float>(std::max(r.y0, r.y1));
    Feeder* best = nullptr;
    float best_distance = 1e30f;
    for (Feeder& feeder : feeders) {
        if (feeder.quantity <= 0.0f ||
            food_is_carnivore(feeder.kind) != carnivore) {
            continue;
        }
        if (feeder.x < min_x || feeder.x > max_x || feeder.y < min_y ||
            feeder.y > max_y) {
            continue;
        }
        const float dx = feeder.x - x;
        const float dy = feeder.y - y;
        const float distance = dx * dx + dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
            best = &feeder;
        }
    }
    return best;
}

bool paddock_patrol_point(const WorldRect& rect, int random_x, int random_y,
                          int& x, int& y) {
    const int min_x = std::min(rect.x0, rect.x1);
    const int max_x = std::max(rect.x0, rect.x1);
    const int min_y = std::min(rect.y0, rect.y1);
    const int max_y = std::max(rect.y0, rect.y1);
    const int span_x = max_x - min_x - 7;
    const int span_y = max_y - min_y - 7;
    if (span_x <= 0 || span_y <= 0) {
        return false;
    }
    x = min_x + random_x % span_x + 4;
    y = min_y + random_y % span_y + 4;
    return true;
}

ParkSession::DinoActionEvent ParkSession::tick_dino(    ImportedDino& dinosaur, double dt,
    const std::vector<BaiAction>& actions) {
    DinoActionEvent event;
    (void)actions;  // scoring/event application lives in DinoActionLoop
    if (!dinosaur.action_loop) {
        return event;
    }
    // `saDinosaur::Update` (`0x571540`) ticks the mover independently of the
    // behavior update. The paddock rect is `path+0x1c0`; a step outside it is
    // refused by `FUN_0055af70`. The inset approximates the actor footprint
    // (retail `actor+0x13` cells) so the body does not clip the wall.
    // Retail fixes the rect on the path object at creation (`FUN_0055ac30`
    // stores it at `path+0x1c0`); the live position is only tested against
    // it (`FUN_0055af70`). Adopt the spawn paddock once.
    if (dinosaur.paddock < 0) {
        dinosaur.paddock = paddock_at(dinosaur.x, dinosaur.y);
    }
    const int confinement = dinosaur.paddock;
    if (confinement >= 0 &&
        confinement < static_cast<int>(paddocks.size())) {
        const WorldRect& r =
            paddocks[static_cast<std::size_t>(confinement)].rect;
        const float min_x =
            static_cast<float>(std::min(r.x0, r.x1));
        const float max_x =
            static_cast<float>(std::max(r.x0, r.x1));
        const float min_y =
            static_cast<float>(std::min(r.y0, r.y1));
        const float max_y =
            static_cast<float>(std::max(r.y0, r.y1));
        // The mover clamps every accepted step to this rect, so an escape
        // means the dino was placed outside or the clamp was bypassed.
        event.escaped = dinosaur.x < min_x - 0.5f ||
                        dinosaur.x > max_x + 0.5f ||
                        dinosaur.y < min_y - 0.5f ||
                        dinosaur.y > max_y + 0.5f;
        const float length_m = static_cast<float>(dinosaur.needs.size_cm) / 100.0f;
        const float inset = std::clamp(length_m * 0.15f, 1.0f, 3.0f);
        dinosaur.mover.set_bounds(min_x, min_y, max_x, max_y, inset);
    } else {
        dinosaur.mover.clear_bounds();
    }
    const bool was_moving = dinosaur.mover.active();
    if (dinosaur.mover.tick(dt)) {
        dinosaur.x = dinosaur.mover.x();
        dinosaur.y = dinosaur.mover.y();
        dinosaur.angle = dinosaur.mover.angle_deg();
        event.moved = true;
    }
    if (was_moving && !dinosaur.mover.active()) {
        event.move_finished = true;
    }
    if (dinosaur.mover.blocked()) {
        event.move_blocked = true;
    }
    event.x = dinosaur.x;
    event.y = dinosaur.y;
    // The `.bsi` FoodSource is the visitor/employee source; the dinosaur Eat
    // target is a LunchBox/StaticBait item actor (`feeder`), and the meal
    // drains it through `FUN_00576e50` into the food gauge.
    const int pen = confinement;
    Feeder* feeder =
        paddock_feeder(pen, dinosaur.carnivore, dinosaur.x, dinosaur.y);
    dinosaur.action_loop->set_inventory(feeder != nullptr);
    const int finished_before = dinosaur.action_loop->finished_count();
    const std::string previous = dinosaur.action_loop->action_name();
    dinosaur.action_loop->tick(dt);
    auto family_duration = [&](int kind, float fallback) {
        int family = dinosaur.attributes.family;
        if (family < 0 || family > 4) {
            family = 3;
        }
        const float value = action_duration(actions_bad, family, kind);
        return value > 0.0f ? value : fallback;
    };
    if (dinosaur.action_loop->finished_count() != finished_before) {
        const std::string& finished =
            dinosaur.action_loop->last_finished_action();
        if (finished == "Eat" || finished == "EatForSmallies") {
            if (feeder != nullptr && feeder->quantity > 0.0f) {
                const float dx = feeder->x - dinosaur.x;
                const float dy = feeder->y - dinosaur.y;
                const float gate = std::max(0.1f, feeder->radius);
                if (dx * dx + dy * dy <= gate * gate) {
                    // `FUN_00576e50` clamps the amount to the source; the
                    // retail amount (FILD dino+0x288) is above the LunchBox
                    // quantity, so one meal drains it and `apply_food`
                    // clamps the gauge.
                    const float value =
                        feeder_drain(*feeder, feeder->quantity);
                    dinosaur.action_loop->apply_food(value);
                    std::printf(
                        "dino eat: %s <- %s nutrition=%.0f quantity=%.0f "
                        "gauge=%.0f\n",
                        dinosaur.species.c_str(), feeder->name.c_str(),
                        static_cast<double>(value),
                        static_cast<double>(feeder->quantity),
                        static_cast<double>(
                            dinosaur.action_loop->food_gauge()));
                }
            }
        } else if (finished == "Sleep") {
            // `FUN_0056eec0` case 2 (special_action 3): state 3 emits the
            // 0x1c wake gesture at the dino's own position.
            dinosaur.mover.clear();
            dinosaur.mover.start(0x1c, dinosaur.x, dinosaur.y, 2,
                                 family_duration(0x1c, 1.5f));
            std::printf("dino sleep: %s wake kind=0x1c\n",
                        dinosaur.species.c_str());
        }
    }
    const std::string& action = dinosaur.action_loop->action_name();
    if (action != previous) {
        event.action_changed = true;
        event.action = action;
        // A new action replaces a lingering gesture record, but a walk/run
        // marker may finish. Sleep keeps its own 0x1d/0x1e list.
        if (dinosaur.mover.active() && action != "Sleep" &&
            dinosaur.mover.kind() != 1 && dinosaur.mover.kind() != 2 &&
            dinosaur.mover.kind() != 0x34 &&
            dinosaur.mover.kind() != 0x35) {
            dinosaur.mover.clear();
        }
        if (action == "Sleep") {
            // `FUN_0056eec0` case 2 state 0: 0x1d then the 0x1e sentinel at
            // the dino's own position. The 0x1d waits 1.5 s, then the
            // sentinel holds the sleep pose until the action ends.
            dinosaur.mover.clear();
            dinosaur.mover.start(0x1d, dinosaur.x, dinosaur.y, 2,
                                 family_duration(0x1d, 1.5f));
            dinosaur.mover.queue(0x1e, dinosaur.x, dinosaur.y, 2, 3.4e38f);
            std::printf("dino sleep: %s start kind=0x1d/0x1e\n",
                        dinosaur.species.c_str());
        } else if (action == "Exploration") {
            // `FUN_0056eec0` case 0 (special_action 1): a random action
            // point in the paddock rect, then the five-record list:
            // 0x27 self (2.0 s), 0x27 point (X[0x27]+0.5), 0x34 marker,
            // 0x20 point (X[0x20]), 0x20 self (rand()%16+5).
            int px = static_cast<int>(dinosaur.x);
            int py = static_cast<int>(dinosaur.y);
            if (pen >= 0) {
                paddock_patrol_point(
                    paddocks[static_cast<std::size_t>(pen)].rect, std::rand(),
                    std::rand(), px, py);
            }
            const float point_x = static_cast<float>(px);
            const float point_y = static_cast<float>(py);
            const float turn_duration = family_duration(0x27, 2.0f);
            dinosaur.mover.clear();
            dinosaur.mover.start(0x27, dinosaur.x, dinosaur.y, 2,
                                 turn_duration);
            dinosaur.mover.queue(0x27, point_x, point_y, 2,
                                 turn_duration + 0.5f);
            dinosaur.mover.queue(0x34, point_x, point_y, 1);
            dinosaur.mover.queue(0x20, point_x, point_y, 2,
                                 family_duration(0x20, 1.5f));
            dinosaur.mover.queue(
                0x20, dinosaur.x, dinosaur.y, 2,
                5.0f + static_cast<float>(std::rand() % 16));
            std::printf(
                "dino explore: %s point=%.1f,%.1f "
                "records=0x27,0x27,0x34,0x20,0x20\n",
                dinosaur.species.c_str(), static_cast<double>(point_x),
                static_cast<double>(point_y));
        }
        if (action != "idle") {
            event.scored = true;
            event.score = dinosaur.action_loop->last_score();
            const std::array<int, 4>& needs =
                dinosaur.action_loop->last_needs();
            event.needs = needs;
        }
        event.clip =
            action == "idle" ? "Neutral" : dino_clip_for_action(action);
    }
    // `0x56f94a` (actor-target executor): the walk/run marker is re-emitted
    // every frame while the element is outside its target gate; arrival
    // emits the kind-0 stop (`0x56f98a`). The marker kind is
    // `0x34 + (REPOS state != 2)` (`dino+0x2b8`; state 2 = BAD/tired), so an
    // exhausted dino walks and a rested one runs to the food.
    if (action == "Eat" || action == "EatForSmallies") {
        if (feeder == nullptr || feeder->quantity <= 0.0f) {
            // Target gone: `0x56f9fd` aborts the element (mode 0 cancel).
            if (dinosaur.mover.active() &&
                (dinosaur.mover.kind() == 0x34 ||
                 dinosaur.mover.kind() == 0x35)) {
                dinosaur.mover.clear();
            }
        } else {
            const float dx = feeder->x - dinosaur.x;
            const float dy = feeder->y - dinosaur.y;
            const float gate = std::max(0.1f, feeder->radius);
            if (dx * dx + dy * dy <= gate * gate) {
                if (dinosaur.mover.active()) {
                    dinosaur.mover.clear();
                }
            } else {
                const bool walking =
                    dinosaur.mover.active() &&
                    (dinosaur.mover.kind() == 0x34 ||
                     dinosaur.mover.kind() == 0x35) &&
                    dinosaur.mover.target_x() == feeder->x &&
                    dinosaur.mover.target_y() == feeder->y;
                if (!walking) {
                    const int kind =
                        0x34 +
                        (dinosaur.action_loop->needs().state(
                             NeedAxis::Repos) != 2
                             ? 1
                             : 0);
                    dinosaur.mover.start(kind, feeder->x, feeder->y, 1);
                    event.move_started = true;
                    event.move_kind = kind;
                    event.move_target_x = feeder->x;
                    event.move_target_y = feeder->y;
                }
            }
        }
    }
    return event;
}

}  // namespace dino
