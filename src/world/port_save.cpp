#include "port_save.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dino {
namespace {

constexpr int kVersion = 1;

void require(std::istringstream& in, const std::string& key,
             const std::string& path) {
    if (!in) {
        throw std::runtime_error("port save: bad " + key + " in " + path);
    }
}

}  // namespace

void save_port_state(const PortState& state, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write port save " + path);
    }
    out << std::setprecision(9);
    out << "DINOPSAV " << kVersion << "\n";
    out << "island " << (state.island.empty() ? "-" : state.island) << "\n";
    out << "difficulty " << state.difficulty << "\n";
    out << "budget " << state.budget << "\n";
    out << "clock " << state.day << " " << state.elapsed << " "
        << state.time_scale << "\n";
    out << "camera " << state.target.x << " " << state.target.y << " "
        << state.target.z << " " << state.yaw << " " << state.pitch << " "
        << state.distance << " " << (state.free_camera ? 1 : 0) << "\n";
    out << "free " << state.free_pos.x << " " << state.free_pos.y << " "
        << state.free_pos.z << " " << state.free_yaw << " "
        << state.free_pitch << "\n";
    out << "grid " << state.cell_size << " " << state.origin_x << " "
        << state.origin_y << " " << state.width << " " << state.height << "\n";
    for (const auto& cell : state.paths) {
        out << "path " << cell.first << " " << cell.second << "\n";
    }
    for (const PortFenceState& fence : state.fences) {
        out << "fence " << fence.x << " " << fence.y << " " << fence.edge
            << "\n";
    }
    for (const PortVisitorState& visitor : state.visitors) {
        out << "visitor " << visitor.x << " " << visitor.y << " "
            << visitor.angle << " " << visitor.phase << " " << visitor.cell_x
            << " " << visitor.cell_y << " " << visitor.target_x << " "
            << visitor.target_y << " " << visitor.prev_x << " "
            << visitor.prev_y << " " << visitor.profile << " "
            << visitor.needs[0] << " " << visitor.needs[1] << " "
            << visitor.needs[2] << " " << visitor.global_profile << " "
            << visitor.need0_at_use << " " << visitor.state << " "
            << visitor.target_attraction << " " << visitor.route_index << " "
            << visitor.use_timer << " " << visitor.decay0 << " "
            << visitor.decay1 << " " << visitor.decay2 << " "
            << visitor.rng << " " << (visitor.done ? 1 : 0) << " "
            << visitor.route.size();
        for (const auto& [x, y] : visitor.route) {
            out << " " << x << " " << y;
        }
        out << "\n";
    }
    for (const PortBuildingState& building : state.buildings) {
        out << "building " << building.kind << " " << building.cell_x << " "
            << building.cell_y << " " << building.state << "\n";
    }
    for (const PortDinoState& dinosaur : state.dinos) {
        out << "dino " << dinosaur.species << " " << dinosaur.family << " "
            << dinosaur.clip << " " << dinosaur.x << " " << dinosaur.y << " "
            << dinosaur.angle << " " << dinosaur.needs[0] << " "
            << dinosaur.needs[1] << " " << dinosaur.needs[2] << " "
            << dinosaur.needs[3] << " " << dinosaur.action_id << " "
            << dinosaur.action_left << " " << dinosaur.size_cm << " "
            << (dinosaur.carnivore ? 1 : 0) << " " << dinosaur.phase
            << "\n";
    }
    for (const PortPaddockState& paddock : state.paddocks) {
        out << "paddock " << paddock.x0 << " " << paddock.y0 << " "
            << paddock.x1 << " " << paddock.y1 << " " << paddock.capacity
            << " " << paddock.used << " " << paddock.occupants.size();
        for (const int occupant : paddock.occupants) {
            out << " " << occupant;
        }
        out << "\n";
    }
    for (const PortFoodState& food : state.food) {
        out << "food " << food.name << " " << food.x << " " << food.y << " "
            << food.remaining << "\n";
    }
    for (const PortEmployeeState& employee : state.employees) {
        out << "employee " << employee.profile << " " << employee.x << " "
            << employee.y << "\n";
    }
    if (!out) {
        throw std::runtime_error("short write to port save " + path);
    }
}

PortState load_port_state(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open port save " + path);
    }
    PortState state;
    std::string line;
    bool magic = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        std::istringstream parts(line);
        std::string key;
        parts >> key;
        if (key == "DINOPSAV") {
            int version = 0;
            parts >> version;
            if (version != kVersion) {
                throw std::runtime_error("port save: unsupported version in " +
                                         path);
            }
            magic = true;
        } else if (key == "island") {
            parts >> state.island;
            if (state.island == "-") {
                state.island.clear();
            }
        } else if (key == "difficulty") {
            parts >> state.difficulty;
            require(parts, key, path);
        } else if (key == "budget") {
            parts >> state.budget;
            require(parts, key, path);
        } else if (key == "clock") {
            parts >> state.day >> state.elapsed >> state.time_scale;
            require(parts, key, path);
        } else if (key == "camera") {
            int free = 0;
            parts >> state.target.x >> state.target.y >> state.target.z >>
                state.yaw >> state.pitch >> state.distance >> free;
            state.free_camera = free != 0;
            require(parts, key, path);
        } else if (key == "free") {
            parts >> state.free_pos.x >> state.free_pos.y >> state.free_pos.z >>
                state.free_yaw >> state.free_pitch;
            require(parts, key, path);
        } else if (key == "grid") {
            parts >> state.cell_size >> state.origin_x >> state.origin_y >>
                state.width >> state.height;
            require(parts, key, path);
        } else if (key == "path") {
            int x = 0;
            int y = 0;
            parts >> x >> y;
            require(parts, key, path);
            state.paths.emplace_back(x, y);
        } else if (key == "fence") {
            PortFenceState fence;
            parts >> fence.x >> fence.y >> fence.edge;
            require(parts, key, path);
            state.fences.push_back(fence);
        } else if (key == "visitor") {
            PortVisitorState visitor;
            parts >> visitor.x >> visitor.y >> visitor.angle >>
                visitor.phase >> visitor.cell_x >> visitor.cell_y >>
                visitor.target_x >> visitor.target_y >> visitor.prev_x >>
                visitor.prev_y;
            require(parts, key, path);
            int profile = 0;
            std::array<int, 3> needs{39, 99, 99};
            if (parts >> profile >> needs[0] >> needs[1] >> needs[2]) {
                visitor.profile = std::max(0, profile);
                for (int& need : needs) {
                    need = std::clamp(need, 0, 99);
                }
                visitor.needs = needs;
            }
            // SIM-2b travel state; older saves end after the needs.
            int global_profile = 0;
            int need0_at_use = 99;
            int state_id = 0;
            int target_attraction = -1;
            int route_index = 0;
            float use_timer = 0.0f;
            float decay0 = 0.0f;
            float decay1 = 0.0f;
            float decay2 = 0.0f;
            unsigned rng = 1;
            int done = 0;
            int route_count = 0;
            if (parts >> global_profile >> need0_at_use >> state_id >>
                target_attraction >> route_index >> use_timer >> decay0 >>
                decay1 >> decay2 >> rng >> done >> route_count) {
                visitor.global_profile = std::max(0, global_profile);
                visitor.need0_at_use = std::clamp(need0_at_use, 0, 99);
                visitor.state = std::clamp(state_id, 0, 3);
                visitor.target_attraction = target_attraction;
                visitor.use_timer = use_timer;
                visitor.decay0 = decay0;
                visitor.decay1 = decay1;
                visitor.decay2 = decay2;
                visitor.rng = rng;
                visitor.done = done != 0;
                if (route_count < 0 || route_count > 100000) {
                    throw std::runtime_error(
                        "port save: bad visitor route in " + path);
                }
                visitor.route.reserve(static_cast<std::size_t>(route_count));
                for (int i = 0; i < route_count; ++i) {
                    int x = 0;
                    int y = 0;
                    parts >> x >> y;
                    require(parts, key, path);
                    visitor.route.emplace_back(x, y);
                }
                visitor.route_index = std::clamp(
                    route_index, 0,
                    static_cast<int>(visitor.route.size()));
            }
            state.visitors.push_back(visitor);
        } else if (key == "building") {
            PortBuildingState building;
            parts >> building.kind >> building.cell_x >> building.cell_y >>
                building.state;
            require(parts, key, path);
            state.buildings.push_back(building);
        } else if (key == "dino") {
            PortDinoState dinosaur;
            parts >> dinosaur.species >> dinosaur.family >> dinosaur.clip >>
                dinosaur.x >> dinosaur.y >> dinosaur.angle >>
                dinosaur.needs[0] >> dinosaur.needs[1] >> dinosaur.needs[2] >>
                dinosaur.needs[3] >> dinosaur.action_id >>
                dinosaur.action_left >> dinosaur.size_cm;
            require(parts, key, path);
            for (int& need : dinosaur.needs) {
                need = std::clamp(need, 0, 99);
            }
            int carnivore = 1;
            if (parts >> carnivore) {
                dinosaur.carnivore = carnivore != 0;
            }
            double phase = 0.0;
            if (parts >> phase) {
                dinosaur.phase = phase;
            }
            if (dinosaur.species.empty() || dinosaur.family.empty() ||
                dinosaur.clip.empty()) {
                throw std::runtime_error("port save: incomplete dino in " +
                                         path);
            }
            state.dinos.push_back(dinosaur);
        } else if (key == "paddock") {
            PortPaddockState paddock;
            int count = 0;
            parts >> paddock.x0 >> paddock.y0 >> paddock.x1 >> paddock.y1 >>
                paddock.capacity >> paddock.used >> count;
            require(parts, key, path);
            if (count < 0 || count > 100000) {
                throw std::runtime_error("port save: bad paddock occupancy in " +
                                         path);
            }
            for (int i = 0; i < count; ++i) {
                int occupant = 0;
                parts >> occupant;
                require(parts, key, path);
                paddock.occupants.push_back(occupant);
            }
            state.paddocks.push_back(std::move(paddock));
        } else if (key == "food") {
            PortFoodState food;
            parts >> food.name >> food.x >> food.y >> food.remaining;
            require(parts, key, path);
            if (!food.name.empty()) {
                state.food.push_back(std::move(food));
            }
        } else if (key == "employee") {
            PortEmployeeState employee;
            parts >> employee.profile >> employee.x >> employee.y;
            require(parts, key, path);
            if (!employee.profile.empty()) {
                state.employees.push_back(std::move(employee));
            }
        }
    }
    if (!magic) {
        throw std::runtime_error("port save: not a DINOPSAV file: " + path);
    }
    if (state.width < 0 || state.height < 0 || state.width > 4096 ||
        state.height > 4096) {
        throw std::runtime_error("port save: bad grid size in " + path);
    }
    return state;
}

}  // namespace dino
