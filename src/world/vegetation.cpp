#include "vegetation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "../formats/blf.h"
#include "../formats/imp.h"
#include "../formats/math.h"

namespace dino {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool is_none(const std::string& entry) {
    return entry.empty() || lower(entry) == "none";
}

Mesh bake_instances(const Mesh& src, const std::vector<ImpInstance>& instances,
                    const Terrain& terrain) {
    Mesh out;
    out.name = src.name;
    out.hash = src.hash;
    const bool has_normals = src.normals.size() == src.positions.size();
    const bool has_uvs = src.uvs.size() == src.positions.size();
    const float half = terrain.world_size * 0.5f;

    out.positions.reserve(src.positions.size() * instances.size());
    out.normals.reserve(has_normals ? src.normals.size() * instances.size() : 0);
    out.uvs.reserve(has_uvs ? src.uvs.size() * instances.size() : 0);
    out.indices.reserve(src.indices.size() * instances.size());

    for (const auto& instance : instances) {
        const float base_z = terrain.height_at(instance.x - half, instance.y - half);
        const float cs = std::cos(instance.angle);
        const float sn = std::sin(instance.angle);
        const float sc = instance.scale;
        const auto offset = static_cast<std::uint32_t>(out.positions.size());
        for (std::size_t i = 0; i < src.positions.size(); ++i) {
            const Vec3& p = src.positions[i];
            out.positions.push_back({
                instance.x - half + (cs * p.x - sn * p.y) * sc,
                instance.y - half + (sn * p.x + cs * p.y) * sc,
                base_z + p.z * sc,
            });
            if (has_normals) {
                const Vec3& n = src.normals[i];
                out.normals.push_back(normalize({cs * n.x - sn * n.y,
                                                 sn * n.x + cs * n.y,
                                                 n.z}));
            }
            if (has_uvs) {
                out.uvs.push_back(src.uvs[i]);
            }
        }
        for (const std::uint32_t index : src.indices) {
            out.indices.push_back(offset + index);
        }
    }
    return out;
}

}  // namespace

std::string asset_path(const std::string& root, const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const std::filesystem::path candidate(normalized);
    if (candidate.is_absolute()) {
        return candidate.string();
    }
    return (std::filesystem::path(root) / candidate).string();
}

Vegetation load_vegetation(const std::string& asset_root, const Lnd& lnd,
                           const Terrain& terrain) {
    Vegetation out;
    for (const auto& entry : lnd.vegetation) {
        if (is_none(entry)) {
            continue;
        }
        try {
            const Imp imp = load_imp(asset_path(asset_root, entry));
            const Blf blf = load_blf(asset_path(asset_root, imp.model));
            if (blf.lods.empty()) {
                out.warnings.push_back(entry + ": .blf has no LODs");
                continue;
            }
            const Mesh source = load_mesh(asset_path(asset_root, blf.lods.front()));
            if (source.positions.empty()) {
                out.warnings.push_back(entry + ": empty lod1 mesh");
                continue;
            }
            VegetationGroup group;
            group.instances = imp.instances.size();
            if (!source.texture_name().empty() && !blf.texture_dir.empty()) {
                group.texture_path = asset_path(
                    asset_root, blf.texture_dir + "\\" + source.texture_name());
            }
            group.mesh = bake_instances(source, imp.instances, terrain);
            out.triangles += group.mesh.triangle_count();
            out.instances += group.instances;
            out.groups.push_back(std::move(group));
        } catch (const std::exception& e) {
            out.warnings.push_back(std::string(entry) + ": " + e.what());
        }
    }
    return out;
}

}  // namespace dino
