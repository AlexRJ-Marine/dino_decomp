#pragma once

#include <map>
#include <optional>
#include <vector>

#include "../formats/bbi.h"
#include "../formats/math.h"
#include "build.h"
#include "economy.h"
#include "employees.h"
#include "food.h"
#include "park_clock.h"
#include "port_save.h"
#include "show_runtime.h"
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
    std::vector<FoodSource> food_sources;
    std::vector<FoodDef> food_defs;
    EmployeeSystem employee_system;
    std::vector<EmployeeProfile> employee_profiles;
    ShowRuntime show_runtime;
    std::vector<ShowCatalogEntry> show_catalog;
    std::vector<PortBuildingState> building_instances;
    std::map<int, BuildingDef> building_defs;
    int entrance_cell_x = -1;
    int entrance_cell_y = -1;
    std::optional<WorldRect> entrance_road;
    Vec3 building_center{1e30f, 1e30f, 1e30f};
};

}  // namespace dino
