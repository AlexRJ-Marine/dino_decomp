#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../formats/bbi.h"
#include "../formats/actions_bad.h"
#include "../formats/math.h"
#include "../formats/wld.h"
#include "build.h"
#include "danger_grid.h"
#include "economy.h"
#include "employees.h"
#include "food.h"
#include "imported_dino.h"
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
    // Per-family record durations (`Actions.bad` f[411..461]); the mover's
    // turn kind 3/4/5 duration comes from here. Session-owned config.
    ActionsBad actions_bad;
    int budget = 0;
    float sellback = 1.0f;
    BudgetLedger ledger;
    BuildGrid build_grid;
    std::vector<Paddock> paddocks;
    std::vector<ImportedDino> imported_dinos;
    // Campaign `.wld` list0 live state (positions/needs/action), matched to
    // the genotype bank at spawn. Empty for free game/imports.
    std::vector<WldDinoState> campaign_dinos;
    // Next stable `ImportedDino::id`; `clear_imported_dinos` resets it.
    int next_imported_dino_id = 1;
    VisitorSystem visitor_system;
    std::vector<VisitorProfile> visitor_profiles;
    std::vector<VisitorProfile> all_visitor_profiles;
    std::vector<FoodSource> food_sources;
    std::vector<FoodDef> food_defs;
    // `.bfi` LunchBox/StaticBait item actors (`saFoodSource` created by the
    // feature); the dinosaur Eat target and `FUN_00576e50` source.
    std::vector<Feeder> feeders;
    // Construction-kit feature selected by `FUN_0052bbb0` (`SWITCH_BAIT_*`
    // or a feature tile); empty = the fence panel's LunchBox default.
    std::string selected_feature;
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

    // Clock and staff update order. Show ticking and HUD/GPU work stay with
    // the application; these two steps only touch session state.
    void advance_clock(double dt);
    int update_staff(double dt);

    // Day-end economy. The caller resolves the ticket price and loads the
    // profile tables; this method only mutates session state and returns the
    // figures for the log line.
    struct DaySettlement {
        int visitors = 0;
        int tickets = 0;
        int wages = 0;
    };
    DaySettlement settle_day(int ticket_price);

    // Paddock/food queries used by the dino tick and the HUD food log.
    int paddock_at(float x, float y) const;
    FoodSource* paddock_food(int paddock, bool carnivore);
    // Nearest non-empty `.bfi` feeder in the paddock whose kind matches the
    // diet (`food_is_carnivore`); null when none. `x/y` is the dinosaur.
    Feeder* paddock_feeder(int paddock, bool carnivore, float x, float y);
    // One dinosaur behavior step: inventory gate, action-loop tick, Eat
    // consumption and clip selection. `Game` prints the returned events,
    // loads the clip assets and uploads the posed mesh.
    struct DinoActionEvent {
        bool action_changed = false;
        bool scored = false;
        std::string action;
        int score = 0;
        std::array<int, 4> needs{};
        std::string clip;
        // Mover (`saDinosaur+0x2c8`) state for the frame.
        bool moved = false;
        bool move_started = false;
        bool move_finished = false;
        bool move_blocked = false;
        float x = 0.0f;
        float y = 0.0f;
        float move_target_x = 0.0f;
        float move_target_y = 0.0f;
        int move_kind = 0;
        // True when a dino with a fixed paddock is outside its rect (a
        // confinement failure; the soak test asserts this never fires).
        bool escaped = false;
    };
    DinoActionEvent tick_dino(ImportedDino& dinosaur, double dt,
                              const std::vector<BaiAction>& actions,
                              const DangerGrid* danger_grid = nullptr);
};

// `FUN_0056eec0` case 0 (special_action 1, Exploration): a random action
// point inside the paddock rect:
//   x = minX + rand() % (maxX - minX - 7) + 4   (same for y)
// The caller passes two `std::rand()` samples so the formula is testable.
// Returns false when the rect is too small (span <= 0); the caller then keeps
// the dino's current point.
bool paddock_patrol_point(const WorldRect& rect, int random_x, int random_y,
                          int& x, int& y);

// `FUN_00577570` (RunAway case 1): the strict-minimum danger-grid cell in the
// paddock rect, converted to its 8-unit centre and clamped four units inside
// the rect. `runaway_move` applies the retail current-cell comparison:
// `cur = grid[dino] - threat`; if the steering point is inside the dino's
// `threat_radius` box, `chosen -= cur`; move only when `chosen < cur`.
// Returns false when the dino should hold with the `0x19` element.
bool runaway_move(const DangerGrid& grid, const WorldRect& rect, float dino_x,
                  float dino_y, int threat, int threat_radius,
                  float& target_x, float& target_y);

}  // namespace dino
