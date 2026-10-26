#pragma once

#include <array>
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
    // Optional tail added after the first format; older saves omit it.
    int profile = 0;
    std::array<int, 3> needs{39, 99, 99};
    // SIM-2b travel/use state; older saves default to Choose.
    int global_profile = 0;
    int need0_at_use = 99;
    int state = 0;  // VisitorState::Choose
    int target_attraction = -1;
    int route_index = 0;
    float use_timer = 0.0f;
    float decay0 = 0.0f;
    float decay1 = 0.0f;
    float decay2 = 0.0f;
    unsigned rng = 1;
    bool done = false;
    std::vector<std::pair<int, int>> route;
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

// Live paddock model (`stFence`): rectangle, capacity/used and occupants.
// Older saves omit it; the loader then falls back to the fence rectangles.
struct PortPaddockState {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    float capacity = 0.0f;
    float used = 0.0f;
    std::vector<int> occupants;
};

// One placed `.bsi` food source, by FoodSourceList name.
struct PortFoodState {
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    float remaining = 0.0f;
};

// One imported dinosaur, including its need/action-loop state. `action_id` is
// the ActionList index; 0x1f is the idle sentinel.
struct PortDinoState {
    std::string species;
    std::string family;
    std::string clip;
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    std::array<int, 4> needs{50, 99, 99, 99};
    int action_id = 0x1f;
    double action_left = 0.0;
    double phase = 0.0;  // clip playback phase
    int size_cm = 300;
    bool carnivore = true;  // diet gene 14; older saves default to true
    // Danger-grid caches and their phenotype sources. Older saves omit the
    // tail; the caches then start at 0 like retail before GetCourage /
    // ObstAttackSize1 complete.
    int threat = 0;           // +0x2a0
    int threat_radius = 0;    // +0x27c
    int combined_attack = 0;  // phenotype +0x44
    int obstacle_radius = 0;  // phenotype +0x80
};

struct PortEmployeeState {
    std::string profile;  // EmployeeList name, e.g. Nutritionist
    float x = 0.0f;
    float y = 0.0f;
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
    std::vector<PortDinoState> dinos;
    std::vector<PortPaddockState> paddocks;
    std::vector<PortFoodState> food;
    std::vector<PortEmployeeState> employees;
};

// Text format: `DINOPSAV 1` followed by one record per line.
void save_port_state(const PortState& state, const std::string& path);
PortState load_port_state(const std::string& path);

}  // namespace dino
