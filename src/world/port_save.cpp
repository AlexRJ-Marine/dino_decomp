#include "port_save.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

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
            << visitor.prev_y << "\n";
    }
    for (const PortBuildingState& building : state.buildings) {
        out << "building " << building.kind << " " << building.cell_x << " "
            << building.cell_y << " " << building.state << "\n";
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
            state.visitors.push_back(visitor);
        } else if (key == "building") {
            PortBuildingState building;
            parts >> building.kind >> building.cell_x >> building.cell_y >>
                building.state;
            require(parts, key, path);
            state.buildings.push_back(building);
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
