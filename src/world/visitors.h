#pragma once

#include <cstdint>
#include <vector>

#include "../formats/mesh.h"
#include "build.h"
#include "terrain.h"

namespace dino {

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
};

struct VisitorSystem {
    std::vector<Visitor> visitors;
    float speed = 1.6f;  // world units per second

    // Spawns up to `count` visitors on random path cells. Returns the number
    // actually spawned (0 when there are no path cells).
    int spawn(const BuildGrid& grid, int count, unsigned seed = 0x51ce5eedu);
    void update(const BuildGrid& grid, const Terrain& terrain, float dt);

    // Bakes every visitor with pose1/pose2 into one world-space mesh.
    Mesh build_mesh(const BuildGrid& grid, const Terrain& terrain,
                    const Mesh& pose1, const Mesh& pose2) const;
};

}  // namespace dino
