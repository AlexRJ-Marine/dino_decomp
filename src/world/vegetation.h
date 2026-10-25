#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../formats/lnd.h"
#include "../formats/mesh.h"
#include "terrain.h"

namespace dino {

struct VegetationGroup {
    Mesh mesh;                 // all instances baked into world space
    std::string texture_path;  // resolved from the .blf texture dir + material
    std::size_t instances = 0;
};

struct Vegetation {
    std::vector<VegetationGroup> groups;
    std::size_t instances = 0;
    std::size_t triangles = 0;
    std::vector<std::string> warnings;  // missing/malformed entries, non-fatal
};

// Joins a retail path (backslashes) onto an asset root.
std::string asset_path(const std::string& root, const std::string& path);

// Loads every non-"None" .imp listed by the .lnd, resolves its .blf and lod1
// .mesh, and bakes the placements into one mesh per vegetation entry.
Vegetation load_vegetation(const std::string& asset_root, const Lnd& lnd,
                           const Terrain& terrain);

}  // namespace dino
