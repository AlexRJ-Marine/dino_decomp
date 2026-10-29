#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../formats/lnd.h"
#include "../formats/mesh.h"
#include "terrain.h"

namespace dino {

// Retail gpkLandscape (0x5fb8b0 builds the grid, 0x5fc132 files each
// object): landscape objects live in a 15x15 grid of 64-unit sectors whose
// centres are 32 + 64*i on both axes, Z = heightmap at the centre.
constexpr int kLandscapeSectorsPerRow = 15;
constexpr int kLandscapeSectorCount =
    kLandscapeSectorsPerRow * kLandscapeSectorsPerRow;
constexpr float kLandscapeSectorSize = 64.0f;

// gpkLandscapeObj 0x603ad0: LOD1 up to 200 units, LOD2 up to 400 units.
constexpr float kLandscapeLodDistance[2] = {200.0f, 400.0f};

// Sector of an object at engine coordinates (0..1024):
// (int(y) >> 6) * 15 + (int(x) >> 6). Retail does not clamp column 15 (it
// aliases the next row's first sector); the port keeps that and only clamps
// the final index into the grid.
int landscape_sector(float engine_x, float engine_y);

// LOD node select 0x508c10/0x508ba0: the first level whose
// `quality * distance` is >= the sector distance, or -1 (not drawn).
int landscape_lod(float sector_distance, float quality);

// Dino.ini [GRAPHIC] LODQuality (0x5054f0 -> 0x51d1d0): HIGH 2.0,
// MEDIUM 1.0, LOW 0.5, AUTO/anything else 1.0.
float landscape_lod_quality(const std::string& setting);

struct VegetationGroup {
    Mesh mesh;                 // this sector's instances baked into world space
    std::string texture_path;  // resolved from the .blf texture dir + material
    std::size_t instances = 0;
    int sector = 0;            // landscape_sector() of every instance
    int lod = 0;               // 0 = .blf lod1 mesh, 1 = lod2
};

struct Vegetation {
    std::vector<VegetationGroup> groups;
    // Port world coordinates of each sector centre (engine - 512).
    std::vector<Vec3> sector_centres;
    std::size_t entries = 0;    // .imp entries that loaded
    std::size_t instances = 0;
    std::size_t triangles = 0;  // LOD1 triangles over all instances
    std::size_t lod2_triangles = 0;
    std::vector<std::string> warnings;  // missing/malformed entries, non-fatal
};

// Joins a retail path (backslashes) onto an asset root. Defined in
// `formats/path.h` so audio and app code do not depend on vegetation.

// Loads every non-"None" .imp listed by the .lnd, resolves its .blf LOD
// meshes, and bakes the placements into one mesh per (entry, sector, LOD).
Vegetation load_vegetation(const std::string& asset_root, const Lnd& lnd,
                           const Terrain& terrain);

}  // namespace dino
