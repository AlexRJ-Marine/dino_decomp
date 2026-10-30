#include "park_session.h"

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

}  // namespace dino
