#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../formats/mesh.h"

namespace dino {

// Port-owned park state (not the retail `.sav` layout). The viewer writes one
// `.psav` file from the Options Save panel and restores it from the Load tab.
struct PortVisitorState {
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    float phase = 0.0f;
    int cell_x = 0;
    int cell_y = 0;
    int target_x = 0;
    int target_y = 0;
    int prev_x = -1;
    int prev_y = -1;
};

struct PortFenceState {
    int x = 0;
    int y = 0;
    int edge = 0;
};

struct PortBuildingState {
    int kind = 0;
    int cell_x = 0;
    int cell_y = 0;
    int state = 0;
};

struct PortState {
    std::string island;
    int difficulty = 0;
    int budget = 75000;

    int day = 0;
    double elapsed = 0.0;
    double time_scale = 60.0;

    // Orbit camera; free-camera pose is stored separately.
    Vec3 target{};
    float yaw = 35.0f;
    float pitch = 25.0f;
    float distance = 39.0f;
    bool free_camera = false;
    Vec3 free_pos{};
    float free_yaw = 0.0f;
    float free_pitch = 0.0f;

    float cell_size = 4.0f;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    int width = 0;
    int height = 0;
    std::vector<std::pair<int, int>> paths;
    std::vector<PortFenceState> fences;
    std::vector<PortVisitorState> visitors;
    std::vector<PortBuildingState> buildings;
};

// Text format: `DINOPSAV 1` followed by one record per line.
void save_port_state(const PortState& state, const std::string& path);
PortState load_port_state(const std::string& path);

}  // namespace dino
