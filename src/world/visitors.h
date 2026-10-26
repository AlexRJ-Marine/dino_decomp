#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../formats/bvi.h"
#include "../formats/mesh.h"
#include "build.h"
#include "terrain.h"

namespace dino {

// A sim attraction distilled from a placed `.bbi` building. The appeal value
// is the retail dot product of the building's five weights with the visitor
// profile's five weights; the scorer uses the same formula as
// `apVisitorBehavior::Choose` (`0x00522a10`).
struct Attraction {
    int cell_x = 0;
    int cell_y = 0;
    float world_x = 0.0f;
    float world_y = 0.0f;
    float radius = 16.0f;
    float appeal_scale = 1.0f;
    std::array<float, 5> appeal_weights{};
    int need_delta1 = 0;  // def+0x50, added to need1
    int need_delta2 = 0;  // def+0x54, added to need2
    int price = 0;        // def+0x1c, charged per use
    float duration = 10.0f;
    int category = 0;
    bool enabled = true;
};

enum class VisitorState : int {
    Choose = 0,
    Travel = 1,
    Use = 2,
    Leave = 3,
};

// A visitor walks cell-to-cell over the placed path network. Retail people are
// not skinned: they ship one mesh per pose (standing, walk1, walk2, ...), so
// the walk cycle is a two-pose flip book.
struct Visitor {
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    int cell_x = 0;
    int cell_y = 0;
    int target_x = 0;
    int target_y = 0;
    int prev_x = -1;
    int prev_y = -1;
    float phase = 0.0f;

    // SIM-2b state.
    int profile = 0;
    int need0 = 39;  // GAUGE / satisfaction, can go negative (index 0)
    int need1 = 99;  // FOOD satisfaction
    int need2 = 99;  // REST satisfaction
    int need0_at_use = 99;  // actor+0x58 snapshot taken on arrival
    VisitorState state = VisitorState::Choose;
    int target_attraction = -1;
    std::vector<std::pair<int, int>> route;
    std::size_t route_index = 0;
    float use_timer = 0.0f;
    float decay0 = 0.0f;  // 12.5 s tick
    float decay1 = 0.0f;  // 10 s tick
    float decay2 = 0.0f;  // 7.5 s tick
    unsigned rng = 1;
    bool done = false;
};

// Retail score from `apVisitorBehavior::Choose` (`0x00522a10`). `random_base`
// is `rand() % 6000 - 3000`; `distance` is the Manhattan cell distance.
long long attraction_score(const Attraction& attraction,
                           const VisitorProfile& profile, int need0,
                           int need1, int need2, int distance,
                           int random_base);

struct VisitorSystem {
    std::vector<Visitor> visitors;
    float speed = 1.6f;  // world units per second (fallback when no profile)

    // Wander mode (no profiles/attractions): keeps the old SIM-2 behaviour.
    int spawn(const BuildGrid& grid, int count, unsigned seed = 0x51ce5eedu);

    // SIM-2b mode: spawn using the free-game profile weights and enter the
    // choose -> travel -> use -> leave state machine.
    int spawn(const BuildGrid& grid, int count,
              const std::vector<VisitorProfile>& profiles, int exit_x,
              int exit_y, unsigned seed = 0x51ce5eedu);

    void update(const BuildGrid& grid, const Terrain& terrain, float dt,
                const std::vector<Attraction>& attractions = {},
                int exit_x = -1, int exit_y = -1,
                const std::vector<VisitorProfile>& profiles = {});

    // Bakes every visitor with pose1/pose2 into one world-space mesh.
    Mesh build_mesh(const BuildGrid& grid, const Terrain& terrain,
                    const Mesh& pose1, const Mesh& pose2) const;
};

}  // namespace dino
