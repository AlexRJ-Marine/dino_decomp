#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../formats/bbi.h"
#include "../formats/mesh.h"
#include "../formats/tga.h"
#include "terrain.h"

namespace dino {

// World-unit rectangle from a retail `.wld`/`.sav` road or fence record.
struct WorldRect {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

// Retail `spRoadNetwork` / `stWall` occupancy grid (`0x5849a0`): one u16 per
// world unit; the high nibble is the occupant class. Roads stamp
// `DAT_00675a60`, walls class 7 (`0x584200` -> `0x5849a0`), buildings class 1
// (`0x586b80`). The road validator accepts a road over a road and rejects
// every other occupant; the construction cell scan rejects any non-zero
// high nibble and also applies the 0.01 height-slope test. Coordinates are
// engine map units (centred world + size/2).
struct OccupancyGrid {
    // High-nibble occupant class. Retail values: buildings stamp 1, walls 7
    // (`DAT_006757b4`), roads `DAT_00675a60` (a runtime class; any distinct
    // value works because only the road-over-road comparison is special).
    // 0xf is the port's non-buildable `.raw` terrain class.
    static constexpr std::uint16_t kTypeMask = 0xf000;
    static constexpr std::uint16_t kBuilding = 0x1000;  // 0x586b80
    static constexpr std::uint16_t kRoad = 0x2000;      // DAT_00675a60 class
    static constexpr std::uint16_t kWall = 0x7000;      // 0x5849a0 (7)
    static constexpr std::uint16_t kBlocked = 0xf000;   // non-buildable .raw
    int width = 0;
    int height = 0;
    std::vector<std::uint16_t> cells;

    void init(int w, int h);
    // `mask` is the runtime `.raw` occupation array (row 0 = map bottom, as
    // `Terrain::occupation`): every non-zero byte becomes kBlocked.
    void init_from_mask(const std::vector<std::uint8_t>& mask, int w, int h);
    bool in_bounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }
    std::uint16_t at(int x, int y) const;
    // Inclusive map-unit rectangle; stamps/clears only the high nibble.
    void stamp(int x0, int y0, int x1, int y1, std::uint16_t type);
    void clear(int x0, int y0, int x1, int y1, std::uint16_t type);
    bool free_rect(int x0, int y0, int x1, int y1) const;
    bool road_free(int x0, int y0, int x1, int y1) const;
};

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
    // spRoadNetwork segment list (+0x08..+0x0c): start/end points in world
    // units (0..1024), x0 <= x1 and y0 <= y1, kept sorted by x0. A segment
    // covers [x0, x1+3] x [y0, y1+3] (0x594810). Filled by add_road.
    std::vector<WorldRect> roads;
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
    // Retail `helper_road` snap (`0x5f8c70` -> `0x5faaa0`): the cursor is
    // snapped to the NEAREST cell anchor (multiples of `cell_size`), not
    // floored. Exact half-cell distances keep the lower coordinate (the
    // nearest-corner loop uses a strict `<`), i.e. round-half-down. This
    // decides where the road axis-lock flips between the two directions.
    bool road_cells_for_world(float wx, float wy, int& x, int& y) const;
};

struct BuildMeshes {
    Mesh paths;
    Mesh fences;
    int path_tiles = 0;
    int fence_segments = 0;
};

// Imports retail park data into the 4-unit grid. Road rectangles become path
// cells; fence rectangles become canonical south/east edges.
void import_roads(BuildGrid& grid, const std::vector<WorldRect>& roads);

// spRoadNetwork::AddRoad (0x58d110): merges the new segment with collinear
// segments that overlap or touch within 4 units (0x58d890), extends it or a
// perpendicular segment by 4 so T-junctions overlap (0x58d9d0), inserts it
// sorted by x0 (0x58dbd0) and marks its cells as path. Endpoints are world
// units; they are ordered first (retail only builds +x/+y runs, see
// notes/road-network-recon.md).
void add_road(BuildGrid& grid, WorldRect road);

// spBuilding door rect in engine units (FUN_00586b80, jump table 0x586d0c):
// the .bbi door segment rotated by the orientation state around the
// footprint at (cell_x, cell_y); values are truncated like __ftol.
WorldRect building_door(const BuildingDef& def, int cell_x, int cell_y,
                        int state);

// FUN_0051dad0: when a park starts, retail lays one free road from each
// Entrance (category 10). The door centre is snapped to the 4-unit grid and
// the road runs 8 units back into the building and 52 units out:
// state 0 (c-8,c)..(c+52,c), 1 (c,c-8)..(c,c+52), 2 (c-52)..(c+8),
// 3 (c,c-52)..(c,c+8). isl01 (520,376) state 1 -> (524,392)-(524,452),
// the road stored in every retail free-game save.
WorldRect entrance_access_road(const BuildingDef& def, int cell_x, int cell_y,
                               int state);
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
// Retail road tool (`ROAD_CONSTRUCTION`, state 0x10; see
// notes/road-construction-recon.md). A road is a straight, axis-aligned run
// of 4-unit cells placed with two clicks; the second click commits and the
// tool chains from the end point. Codes are the `FUN_0052c2e0` warnings.
enum class RoadCheck {
    Ok = 0,
    BusySpace = 1,        // &WARNING_BUSY_SPACE
    InsidePerimeter = 3,  // &WARNING_INSIDE_PERIMETER
    TooShort = 5,         // &WARNING_ROAD_TOO_SHORT
};

struct RoadSegment {
    int x0 = 0;  // start cell
    int y0 = 0;
    int x1 = 0;  // end cell; x0 == x1 or y0 == y1
    int y1 = 0;
    int cells() const;
};

// Cell (4-unit) rectangle a `RoadSegment` occupies, in map units:
// `[min(x0,x1)*cell, max(x0,x1)*cell + cell-1]`.
WorldRect road_units(const RoadSegment& road, int cell);

// 0x5fa5b0: keep the longer axis of start -> cursor (ties go vertical).
RoadSegment road_segment(int start_x, int start_y, int cursor_x, int cursor_y);
// 0x52e7c2: (|dx| + |dy|) world units * price >> 2, i.e. price per cell
// beyond the first. `price` is GeneralParams.bgd `+0x1a4` (10).
int road_cost(const RoadSegment& road, std::uint32_t price);
const char* road_warning(RoadCheck check);
// `busy(x, y)`: the cell holds something other than a road (building,
// blocked ground). Fence edges crossed by the run also count as busy.
// `perimeters` are paddock/show fence rectangles in world units.
using RoadBusyFn = std::function<bool(int x, int y)>;
RoadCheck check_road_start(const BuildGrid& grid, int x, int y,
                           const std::vector<WorldRect>& perimeters,
                           const RoadBusyFn& busy);  // 0x58ce80
RoadCheck check_road(const BuildGrid& grid, const RoadSegment& road,
                     const std::vector<WorldRect>& perimeters,
                     const RoadBusyFn& busy);  // 0x58cfd0
// Marks the run as path cells; returns how many cells were new.
int place_road(BuildGrid& grid, const RoadSegment& road);

// Flat validity disc for the enclosure tool, centred at (x, y) on the
// terrain. The caller draws it blue when the cell is buildable and red when
// not. Retail samples: blue (38,38,136), red (80,38,38).
Mesh fence_preview_disc(float x, float y, float z, float radius,
                        int segments = 48);

// Retail placement preview (0x5fa5b0..0x5fa891): one helper_road.mesh (a
// bottomless unit box) per 4-unit cell of the run, scaled 4x4x4 and centred at
// (cell x + 1.5, cell y + 1.5, ground height), so it sits half in the ground.
Mesh road_helper_mesh(const Mesh& helper_box, const RoadSegment& road,
                      const BuildGrid& grid, const Terrain& terrain);

// Retail draws no road geometry: gpkLandscapeTextureManager's road layer
// (0x602380) paints the roads into the island texture. On a 1024x1024 map
// each road is a RGB(253,198,137) fill with a 1-texel RGB(170,130,90) border;
// smaller maps get the fill only. `image` is the top-down basemap (row 0 =
// world y 1024). The port paints cells and borders only the sides that touch
// a non-road cell, so joined roads read as one surface.
// Road layer 0x602380: every segment of grid.roads, in list order, as an
// opaque rectangle [x0, x1+3] x [y0, y1+3] (rows flipped); on a 1024x1024
// texture an RGB(170,130,90) rectangle with an RGB(253,198,137) rectangle
// inset by one texel, otherwise only the fill. Path cells that no complete
// segment covers (legacy --place-path cells, undo) use the per-cell look.
void paint_roads(Image& image, const BuildGrid& grid, float world_size);

// `fence_source` is the retail fence segment (long axis = local +Y).
BuildMeshes build_placement_meshes(const BuildGrid& grid, const Terrain& terrain,
                                   const Mesh& fence_source);

void save_build(const BuildGrid& grid, const std::string& path);
void load_build(BuildGrid& grid, const std::string& path, float world_size);

}  // namespace dino
