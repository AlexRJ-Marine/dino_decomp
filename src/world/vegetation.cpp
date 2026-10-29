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
#include "../formats/path.h"

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

int landscape_sector(float engine_x, float engine_y) {
    const int col = static_cast<int>(engine_x) >> 6;
    const int row = static_cast<int>(engine_y) >> 6;
    return std::clamp(row * kLandscapeSectorsPerRow + col, 0,
                      kLandscapeSectorCount - 1);
}

int landscape_lod(float sector_distance, float quality) {
    for (int i = 0; i < 2; ++i) {
        if (sector_distance <= quality * kLandscapeLodDistance[i]) {
            return i;
        }
    }
    return -1;
}

float landscape_lod_quality(const std::string& setting) {
    const std::string value = lower(setting);
    if (value == "high") {
        return 2.0f;
    }
    if (value == "low") {
        return 0.5f;
    }
    return 1.0f;  // MEDIUM, AUTO and unknown values
}

Vegetation load_vegetation(const std::string& asset_root, const Lnd& lnd,
                           const Terrain& terrain) {
    Vegetation out;
    const float half = terrain.world_size * 0.5f;
    out.sector_centres.reserve(kLandscapeSectorCount);
    for (int row = 0; row < kLandscapeSectorsPerRow; ++row) {
        for (int col = 0; col < kLandscapeSectorsPerRow; ++col) {
            const float x = 32.0f + kLandscapeSectorSize * col - half;
            const float y = 32.0f + kLandscapeSectorSize * row - half;
            out.sector_centres.push_back({x, y, terrain.height_at(x, y)});
        }
    }
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
            std::vector<Mesh> lods;
            std::vector<std::string> textures;
            for (std::size_t i = 0; i < blf.lods.size() && i < 2; ++i) {
                Mesh mesh = load_mesh(asset_path(asset_root, blf.lods[i]));
                if (mesh.positions.empty()) {
                    out.warnings.push_back(entry + ": empty lod" +
                                           std::to_string(i + 1) + " mesh");
                    break;
                }
                std::string texture;
                if (!mesh.texture_name().empty() && !blf.texture_dir.empty()) {
                    texture = asset_path(asset_root, blf.texture_dir + "\\" +
                                                         mesh.texture_name());
                }
                lods.push_back(std::move(mesh));
                textures.push_back(std::move(texture));
            }
            if (lods.empty()) {
                continue;
            }
            std::vector<std::vector<ImpInstance>> by_sector(kLandscapeSectorCount);
            for (const auto& instance : imp.instances) {
                by_sector[landscape_sector(instance.x, instance.y)].push_back(instance);
            }
            for (int sector = 0; sector < kLandscapeSectorCount; ++sector) {
                const auto& instances = by_sector[sector];
                if (instances.empty()) {
                    continue;
                }
                for (std::size_t lod = 0; lod < lods.size(); ++lod) {
                    VegetationGroup group;
                    group.instances = instances.size();
                    group.sector = sector;
                    group.lod = static_cast<int>(lod);
                    group.texture_path = textures[lod];
                    group.mesh = bake_instances(lods[lod], instances, terrain);
                    (lod == 0 ? out.triangles : out.lod2_triangles) +=
                        group.mesh.triangle_count();
                    out.groups.push_back(std::move(group));
                }
            }
            ++out.entries;
            out.instances += imp.instances.size();
        } catch (const std::exception& e) {
            out.warnings.push_back(std::string(entry) + ": " + e.what());
        }
    }
    return out;
}

}  // namespace dino
