#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../formats/mesh.h"
#include "terrain.h"

namespace dino {

// Grid-based path/fence placement in the same 1024-unit world as the terrain.
//
// A cell is `cell_size` world units; cell (0,0) starts at -world_size/2. Paths
// own cells; fences own cell edges. The south edge of (x,y) and the east edge
// of (x,y) are canonical, so neighbouring cells never store the same edge.
struct BuildGrid {
    struct Change {
        int x = 0;
        int y = 0;
        int kind = 0;  // 0 = path, 1 = south fence, 2 = east fence
        bool value = false;
    };

    float cell_size = 4.0f;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> path;
    std::vector<std::uint8_t> fences;  // bit 0 = south, bit 1 = east
    std::vector<Change> undo_stack;
    int path_count = 0;
    int fence_count = 0;

    void init(float new_cell_size, float world_size);
    bool in_bounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }
    bool path_at(int x, int y) const;
    bool fence_at(int x, int y, int edge) const;
    void set_path(int x, int y, bool on);
    void set_fence(int x, int y, int edge, bool on);
    bool undo();
    bool cells_for_world(float wx, float wy, int& x, int& y) const;
};

struct BuildMeshes {
    Mesh paths;
    Mesh fences;
    int path_tiles = 0;
    int fence_segments = 0;
};

// World-unit rectangle from a retail `.wld`/`.sav` road or fence record.
struct WorldRect {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

// Imports retail park data into the 4-unit grid. Road rectangles become path
// cells; fence rectangles become canonical south/east edges.
void import_roads(BuildGrid& grid, const std::vector<WorldRect>& roads);
void import_fences(BuildGrid& grid, const std::vector<WorldRect>& fences);

// Retail paddock capacity model (`stFence`, 0x57dc60 / 0x57eb50). A paddock is
// a `.wld` fence rectangle; capacity counts 4-unit cells, and each dinosaur
// consumes a cost from the 8/16/32/64 table.
struct Paddock {
    WorldRect rect;
    float capacity = 0.0f;
    float used = 0.0f;
    std::vector<int> occupants;  // dinosaur ids/indices

    float fill() const {
        return capacity > 0.0f ? used / capacity : 0.0f;
    }
};

float paddock_capacity(const WorldRect& rect, float cell_size);
float paddock_cost(int cost_index);  // 8 << clamp(index, 0..3)
// Size class for `dinosaur+0x1e0` (TranslatorConstants.ini
// SizeCategory1..4MaxLength): 0 = <=200 cm, 1 = 201..620, 2 = 621..900,
// 3 = >900. Pass the phenotype length from `DinoPhenotypeTable::length_cm`.
int paddock_size_class(int size_cm);
bool paddock_can_add(const Paddock& paddock, int dinosaur_id, int cost_index);
bool paddock_add(Paddock& paddock, int dinosaur_id, int cost_index);

// `fence_source` is the retail fence segment (long axis = local +Y).
BuildMeshes build_placement_meshes(const BuildGrid& grid, const Terrain& terrain,
                                   const Mesh& fence_source);

void save_build(const BuildGrid& grid, const std::string& path);
void load_build(BuildGrid& grid, const std::string& path, float world_size);

}  // namespace dino
