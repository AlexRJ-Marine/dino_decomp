#pragma once

#include <vector>

#include "build.h"
#include "economy.h"
#include "park_clock.h"
#include "visitors.h"

namespace dino {

// Live park state. `Game` still runs the update order; this type is the
// home the ARCH-2 migration moves the remaining subsystems into, one at a
// time. Defaults are safe; the boot phases assign the retail values.
struct ParkSession {
    ParkClock clock;
    int budget = 0;
    float sellback = 1.0f;
    BudgetLedger ledger;
    BuildGrid build_grid;
    std::vector<Paddock> paddocks;
    VisitorSystem visitor_system;
    std::vector<VisitorProfile> visitor_profiles;
    std::vector<VisitorProfile> all_visitor_profiles;
};

}  // namespace dino
