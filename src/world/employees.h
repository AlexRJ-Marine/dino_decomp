#pragma once

#include <cstddef>
#include <vector>

#include "../formats/bei.h"
#include "../formats/mesh.h"
#include "food.h"
#include "terrain.h"

namespace dino {

// One live employee. Positions are raw world units (0..world_size), the same
// convention as FoodSource/Paddock.
struct Employee {
    int profile = 0;   // index into EmployeeProfile list
    float x = 0.0f;
    float y = 0.0f;
    int state = 0;     // 0 idle, 1 travel to the food source, 2 work
    int target = -1;   // FoodSource index
    float work_left = 0.0f;
};

// One `EmployeeSystem::update` step: completed feed jobs and the FOOD price
// charged by `FUN_00576d50` (`0x585c90`).
struct FeedResult {
    int completed = 0;
    int food_spent = 0;
};

// Port of the decoded employee loop (`apEmployeeBehavior::Update`,
// 0x5220e0) at the first-slice level: every new hour each idle nutritionist
// takes one placed food source that is below capacity, walks to it, plays the
// profile work duration, and refills it. Guards are a wage/no-job role in the
// retail exe (only Nutritionists are hired), so they never take jobs here.
struct EmployeeSystem {
    std::vector<Employee> employees;
    int serviced = 0;  // completed feed actions (diagnostics)

    bool hire(const std::vector<EmployeeProfile>& profiles, int profile_index,
              float x, float y);

    int count_role(const std::vector<EmployeeProfile>& profiles, int kind) const;
    int daily_wages(const std::vector<EmployeeProfile>& profiles) const;

    // Assigns one needy food source per idle nutritionist (neediest first).
    // `instant` teleports the employee to the source (test/debug path).
    int start_hour(const std::vector<EmployeeProfile>& profiles,
                   const std::vector<FoodSource>& sources,
                   const std::vector<FoodDef>& defs, bool instant = false);

    // Moves/works; refills a source with `set_food_level(level 0)` on
    // completion and returns the completed count plus the charged FOOD
    // price (`FUN_00576d50`). Level 0 is exact: `FUN_00580e30` is the only
    // slot writer and all three call sites pass 0.
    FeedResult update(float dt, const std::vector<EmployeeProfile>& profiles,
                      std::vector<FoodSource>& sources,
                      const std::vector<FoodDef>& defs);

    int active() const;

    // Bakes every employee at its current position (one `pose` for all).
    Mesh build_mesh(const Terrain& terrain, const Mesh& pose) const;
};

}  // namespace dino
