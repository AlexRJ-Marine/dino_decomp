#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "math.h"
#include "mesh.h"

namespace dino {

struct SkinBone {
    std::string name;
    std::vector<std::uint32_t> vertices;
    std::vector<float> weights;
    Mat4 inverse_bind;
};

struct Skin {
    std::uint32_t hash = 0;
    Mesh mesh;  // full mesh, centimetres
    std::vector<SkinBone> bones;
};

// `mesh_name` is the .mesh name (e.g. "dino_lod1"); it is used to locate the
// full-mesh block inside the .skin container.
Skin load_skin(const std::string& path, const std::string& mesh_name);

// Scans for the full-mesh block when the block name differs from the file
// stem and no sibling .mesh exists (several building skins). Picks the
// largest block whose tail parses as a material table + skin records.
Skin load_skin_auto(const std::string& path);

}  // namespace dino
