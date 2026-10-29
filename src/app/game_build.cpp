// Construction: buildings, fences and roads, the store catalog and
// daily settlement.

#include "game.h"

namespace dino::app {

void Game::clear_buildings() {
    for (BuildingDraw& building : building_draws) {
        building.gpu.destroy(glfns);
        if (building.texture != 0) {
            glDeleteTextures(1, &building.texture);
        }
    }
    building_draws.clear();
    building_instances.clear();
    attractions.clear();
    entrance_cell_x = -1;
    entrance_cell_y = -1;
    entrance_road.reset();
    building_center = {1e30f, 1e30f, 1e30f};

}

bool Game::place_building(const dino::BuildingDef& def, float cell_x,
                              float cell_y, int state, float extra_angle_deg,
                              float scale, int kind) {
    if (terrain.world_size <= 0.0f) {
        std::fprintf(stderr, "building: --lnd is required for placement\n");
        return false;
    }
    try {
        BuildingModel model = load_building_model(def);
        const std::string& mesh_rel = model.mesh_rel;
        std::vector<dino::Mesh>& parts = model.parts;
        const std::vector<GLuint>& part_textures = model.part_textures;

        // `FUN_00586b80`: cell_x/cell_y is the footprint min corner; odd
        // orientation states swap the footprint axes. `gpkBuilding`
        // (`FUN_005fecf0`) places the model at the footprint centre.
        const int orientation = ((state % 4) + 4) % 4;
        const float w0 = def.footprint_w > 0 && def.footprint_w < 256
                             ? static_cast<float>(def.footprint_w)
                             : 1.0f;
        const float w1 = def.footprint_h > 0 && def.footprint_h < 256
                             ? static_cast<float>(def.footprint_h)
                             : 1.0f;
        const float span_x = (orientation % 2 == 0 ? w0 : w1) - 1.0f;
        const float span_y = (orientation % 2 == 0 ? w1 : w0) - 1.0f;
        const float centre_x = cell_x + span_x * 0.5f;
        const float centre_y = cell_y + span_y * 0.5f;
        const float half = terrain.world_size * 0.5f;
        const float wx = centre_x - half;
        const float wy = centre_y - half;
        const float wz = terrain.height_at(wx, wy);
        if (building_center.x > 1e29f) {
            building_center = {wx, wy, wz + 3.0f};
        }
        const float angle_deg =
            extra_angle_deg + static_cast<float>(orientation) * 90.0f;
        const float radians = angle_deg * kPi / 180.0f;
        const float cos_a = std::cos(radians);
        const float sin_a = std::sin(radians);
        for (std::size_t part_index = 0; part_index < parts.size();
             ++part_index) {
            dino::Mesh& placed = parts[part_index];
            if (def.model0 == 1) {
                // FUN_0061c510: slope-aligned basis from the terrain normal.
                const dino::TerrainBasis basis =
                    dino::slope_basis(terrain, wx, wy, radians);
                const dino::Vec3& right = basis.right;
                const dino::Vec3& up = basis.up;
                const dino::Vec3& n = basis.normal;
                const dino::Vec3 rx{right.x * scale, right.y * scale,
                                    right.z * scale};
                const dino::Vec3 ux{up.x * scale, up.y * scale, up.z * scale};
                const dino::Vec3 nx{n.x * scale, n.y * scale, n.z * scale};
                for (std::size_t i = 0; i < placed.positions.size(); ++i) {
                    const dino::Vec3 p = placed.positions[i];
                    placed.positions[i] = {
                        wx + p.x * rx.x + p.y * ux.x + p.z * nx.x,
                        wy + p.x * rx.y + p.y * ux.y + p.z * nx.y,
                        wz + p.x * rx.z + p.y * ux.z + p.z * nx.z};
                    if (i < placed.normals.size()) {
                        const dino::Vec3 v = placed.normals[i];
                        placed.normals[i] = dino::normalize(
                            {v.x * right.x + v.y * up.x + v.z * n.x,
                             v.x * right.y + v.y * up.y + v.z * n.y,
                             v.x * right.z + v.y * up.z + v.z * n.z});
                    }
                }
            } else {
                const float cs = cos_a * scale;
                const float sn = sin_a * scale;
                for (std::size_t i = 0; i < placed.positions.size(); ++i) {
                    const dino::Vec3 p = placed.positions[i];
                    placed.positions[i] = {wx + p.x * cs - p.y * sn,
                                           wy + p.x * sn + p.y * cs,
                                           wz + p.z * scale};
                    if (i < placed.normals.size()) {
                        const dino::Vec3 n = placed.normals[i];
                        placed.normals[i] = dino::normalize(
                            {n.x * cs - n.y * sn, n.x * sn + n.y * cs, n.z});
                    }
                }
            }
            building_draws.push_back(
                {upload_mesh(placed, glfns), part_textures[part_index]});
        }
        if (kind >= 0) {
            building_defs[kind] = def;
            building_instances.push_back(
                {kind, static_cast<int>(cell_x), static_cast<int>(cell_y),
                 orientation});
            // spPark building footprint stamp (0x586b80): class 1 over
            // `[cell_x .. cell_x+w-1] x [cell_y .. cell_y+h-1]`, footprint
            // axes swapped for odd orientation states.
            const int fw = orientation % 2 == 0
                               ? static_cast<int>(def.footprint_w)
                               : static_cast<int>(def.footprint_h);
            const int fh = orientation % 2 == 0
                               ? static_cast<int>(def.footprint_h)
                               : static_cast<int>(def.footprint_w);
            park_session.build_grid.occupancy.stamp(
                static_cast<int>(cell_x), static_cast<int>(cell_y),
                static_cast<int>(cell_x) + fw - 1,
                static_cast<int>(cell_y) + fh - 1,
                dino::OccupancyGrid::kBuilding);
        }
        const int centre_cell_x = static_cast<int>(std::floor(
            centre_x / (park_session.build_grid.cell_size > 0.0f
                            ? park_session.build_grid.cell_size
                            : 4.0f)));
        const int centre_cell_y = static_cast<int>(std::floor(
            centre_y / (park_session.build_grid.cell_size > 0.0f
                            ? park_session.build_grid.cell_size
                            : 4.0f)));
        if (def.category == 10) {
            entrance_cell_x = centre_cell_x;
            entrance_cell_y = centre_cell_y;
            entrance_road = dino::entrance_access_road(
                def, static_cast<int>(cell_x), static_cast<int>(cell_y),
                orientation);
        } else if (def.category > 0) {
            dino::Attraction attraction;
            attraction.cell_x = centre_cell_x;
            attraction.cell_y = centre_cell_y;
            attraction.world_x = wx;
            attraction.world_y = wy;
            const float radius = static_cast<float>(def.radius_squared);
            attraction.radius = radius > 0.0f ? radius : 32.0f;
            attraction.appeal_scale = def.appeal_scale;
            attraction.appeal_weights = def.appeal_weights;
            attraction.need_delta1 = static_cast<int>(def.need_delta1);
            attraction.need_delta2 = static_cast<int>(def.need_delta2);
            attraction.price = static_cast<int>(def.price);
            attraction.duration =
                def.duration > 0.1f ? def.duration : 10.0f;
            attraction.category = static_cast<int>(def.category);
            attractions.push_back(attraction);
        }
        std::size_t total_verts = 0;
        std::size_t total_tris = 0;
        const dino::Mesh* last_chunk = nullptr;
        for (const dino::Mesh& part : parts) {
            // Material parts of one chunk share its vertex array.
            if (last_chunk == nullptr || part.name != last_chunk->name) {
                total_verts += part.positions.size();
            }
            last_chunk = &part;
            total_tris += part.triangle_count();
        }
        const std::string texture_name =
            parts.empty() ? std::string() : parts.front().texture_name();
        std::printf(
            "building: %s mesh=%s verts=%zu tris=%zu parts=%zu "
            "cell=%.1f,%.1f centre=%.1f,%.1f z=%.1f state=%d angle=%.0f "
            "texture=%s\n",
            def.name.c_str(), mesh_rel.c_str(), total_verts, total_tris,
            parts.size(), cell_x, cell_y, centre_x, centre_y, wz,
            orientation, angle_deg, texture_name.c_str());
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "building: %s\n", e.what());
        return false;
    }

}

// Select the `.bbi` LOD mesh, split materials and upload their textures.
Game::BuildingModel Game::load_building_model(
    const dino::BuildingDef& def) {
    auto ends_with_ci = [](const std::string& text, const char* suffix) {
        const std::size_t n = std::char_traits<char>::length(suffix);
        if (text.size() < n) {
            return false;
        }
        for (std::size_t i = 0; i < n; ++i) {
            const char a = static_cast<char>(std::tolower(
                static_cast<unsigned char>(text[text.size() - n + i])));
            const char b = static_cast<char>(std::tolower(
                static_cast<unsigned char>(suffix[i])));
            if (a != b) {
                return false;
            }
        }
        return true;
    };
    // `.bbi` mesh order: normal LOD1 `.skin`, crash, destroy, then
    // either the static normal LOD1 `.mesh` (Entrance, seats, doors)
    // or the construction model. The static normal LOD1 is in metres
    // and in the placement-pivot space; the `.skin` full mesh is in
    // centimetres and is centred on its own bind origin. The Entrance
    // needs the static mesh so the pier reaches the beach and the
    // boat sits at the water end. Other buildings fall back to the
    // animated `.skin`.
    auto is_static_lod1 = [&](const std::string& path) {
        // Retail abbreviates the construction variants (`construc`)
        // as well as spelling them out. Missing the abbreviation
        // selected `stand_construc_lod1.mesh`, which has no material
        // and rendered as a white model.
        return ends_with_ci(path, ".mesh") &&
               path.find("_lod1") != std::string::npos &&
               path.find("crash") == std::string::npos &&
               path.find("crach") == std::string::npos &&
               path.find("destroy") == std::string::npos &&
               path.find("construc") == std::string::npos;
    };
    std::string mesh_rel;
    if (!def.meshes.empty()) {
        mesh_rel = def.meshes.front();
        if (def.meshes.size() > 3 && is_static_lod1(def.meshes[3])) {
            mesh_rel = def.meshes[3];
        }
        if (opt.building_model_mesh) {
            if (def.meshes.size() > 3 && is_static_lod1(def.meshes[3])) {
                mesh_rel = def.meshes[3];
            } else {
                for (const std::string& candidate : def.meshes) {
                    if (is_static_lod1(candidate)) {
                        mesh_rel = candidate;
                        break;
                    }
                }
            }
        }
    }
    // A static `.mesh` can hold several chunks (entrance_lod1: boat,
    // radar, ponton2 gangway, ponton dock); retail draws them all, so
    // every chunk/material becomes its own textured part.
    std::vector<dino::Mesh> parts;
    if (ends_with_ci(mesh_rel, ".skin")) {
        dino::Skin skinned =
            dino::load_skin_auto(resolve_asset(opt.asset_root, mesh_rel));
        parts.push_back(std::move(skinned.mesh));
        // `.skin` full meshes are stored in centimetres.
        for (dino::Vec3& p : parts.front().positions) {
            p.x *= 0.01f;
            p.y *= 0.01f;
            p.z *= 0.01f;
        }
    } else {
        for (const dino::Mesh& chunk : dino::load_mesh_chunks(
                 resolve_asset(opt.asset_root, mesh_rel))) {
            for (dino::Mesh& part : dino::split_by_material(chunk)) {
                parts.push_back(std::move(part));
            }
        }
    }
    // `tail_strings` mostly holds the building's mesh/skeleton paths
    // (e.g. `DATA\graphics\buildings\souvenirs\souv_construction_
    // lod1.skel`). Only PointOfView declares a real texture directory
    // (`DATA\graphics\TEXTURES\buildings`). Treating every `DATA\`
    // string as a texture directory made most buildings look up their
    // .tga inside a .skel path and render white.
    std::string texture_dir = "Data/Graphics/Textures/buildings";
    for (const auto& text : def.tail_strings) {
        std::string lower = text;
        for (char& c : lower) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
        }
        if (lower.find("textures") != std::string::npos &&
            lower.rfind("data\\", 0) == 0) {
            texture_dir = text;
            break;
        }
    }
    std::vector<GLuint> part_textures;
    for (const dino::Mesh& part : parts) {
        GLuint texture = 0;
        const std::string texture_name = part.texture_name();
        if (!texture_name.empty()) {
            try {
                texture = upload_texture(dino::load_tga(
                    resolve_asset(opt.asset_root,
                                  texture_dir + "/" + texture_name)));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "building texture: %s\n", e.what());
            }
        }
        part_textures.push_back(texture);
    }
    return {std::move(mesh_rel), std::move(parts),
            std::move(part_textures)};
}

int Game::place_world_buildings(const dino::WldWorld& world) {
    const std::vector<std::string> names = dino::load_building_list(
        resolve_asset(opt.asset_root,
                      "Data/Design/buildings/BuildingList.ini"));
    int placed = 0;
    for (const dino::WorldBuilding& b : world.buildings) {
        if (b.kind >= names.size()) {
            std::fprintf(stderr, "wld: unknown building kind %u\n", b.kind);
            continue;
        }
        try {
            const dino::BuildingDef def = dino::load_bbi(resolve_asset(
                opt.asset_root,
                "Data/Design/buildings/" + names[b.kind] + ".bbi"));
            if (place_building(def, static_cast<float>(b.cell_x),
                               static_cast<float>(b.cell_y),
                               static_cast<int>(b.state), 0.0f, 1.0f,
                               static_cast<int>(b.kind))) {
                ++placed;
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "wld building %u: %s\n", b.kind,
                         e.what());
        }
    }
    std::printf("wld: placed %d/%zu buildings\n", placed,
                world.buildings.size());
    return placed;

}

int Game::construction_kind_for_widget(const std::string& widget) {
    ensure_construction_catalog();
    for (std::size_t i = 0; i < construction_defs.size(); ++i) {
        for (const std::string& ui : construction_defs[i].tail_strings) {
            if (ui == widget) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;

}

int Game::construction_kind_for_name(const std::string& name) {
    ensure_construction_catalog();
    if (name.rfind("CONSTRUC_", 0) == 0) {
        return construction_kind_for_widget(name);
    }
    for (std::size_t i = 0; i < construction_names.size(); ++i) {
        if (construction_names[i] == name) {
            return static_cast<int>(i);
        }
    }
    return -1;

}

bool Game::construction_overlaps(int kind, int cell_x, int cell_y,
                                     int state) {
    const dino::BuildingDef& def =
        construction_defs[static_cast<std::size_t>(kind)];
    const int w = state % 2 == 0 ? static_cast<int>(def.footprint_w)
                                 : static_cast<int>(def.footprint_h);
    const int h = state % 2 == 0 ? static_cast<int>(def.footprint_h)
                                 : static_cast<int>(def.footprint_w);
    for (const dino::PortBuildingState& other : building_instances) {
        if (other.kind < 0 ||
            other.kind >= static_cast<int>(construction_defs.size())) {
            continue;
        }
        const dino::BuildingDef& od =
            construction_defs[static_cast<std::size_t>(other.kind)];
        const int ow = other.state % 2 == 0
                           ? static_cast<int>(od.footprint_w)
                           : static_cast<int>(od.footprint_h);
        const int oh = other.state % 2 == 0
                           ? static_cast<int>(od.footprint_h)
                           : static_cast<int>(od.footprint_w);
        if (cell_x < other.cell_x + ow && other.cell_x < cell_x + w &&
            cell_y < other.cell_y + oh && other.cell_y < cell_y + h) {
            return true;
        }
    }
    return false;

}

bool Game::try_construct(int kind, int cell_x, int cell_y,
                             int state) {
    if (kind < 0 || kind >= static_cast<int>(construction_defs.size())) {
        std::printf("construction: unknown building\n");
        return false;
    }
    const dino::BuildingDef& def =
        construction_defs[static_cast<std::size_t>(kind)];
    const int w = state % 2 == 0 ? static_cast<int>(def.footprint_w)
                                 : static_cast<int>(def.footprint_h);
    const int h = state % 2 == 0 ? static_cast<int>(def.footprint_h)
                                 : static_cast<int>(def.footprint_w);
    // Retail gates the budget first (0x5306e1 -> FUN_0052bed0), then the
    // placement/ground checks.
    if (park_session.budget < static_cast<int>(def.construction_cost)) {
        std::printf(
            "construction: %s needs %u, budget %d (WARNING_NOT_ENOUGH_MONEY)\n",
            def.name.c_str(), def.construction_cost, park_session.budget);
        return false;
    }
    const int world = static_cast<int>(terrain.world_size);
    if (cell_x < 0 || cell_y < 0 || cell_x + w > world ||
        cell_y + h > world) {
        std::printf("construction: %s outside the island\n",
                    def.name.c_str());
        return false;
    }
    if (construction_overlaps(kind, cell_x, cell_y, state)) {
        std::printf("construction: %s overlaps another building\n",
                    def.name.c_str());
        return false;
    }
    // Occupancy scan (0x58a390): any non-zero high nibble is busy. The
    // grid is seeded from the .raw mask and carries buildings/roads/
    // walls, so this replaces the old centre-sample buildable() test.
    if (park_session.build_grid.occupancy.width > 0 &&
        !park_session.build_grid.occupancy.free_rect(cell_x, cell_y, cell_x + w - 1,
                                        cell_y + h - 1)) {
        std::printf("construction: %s on blocked ground\n",
                    def.name.c_str());
        return false;
    }
    if (!place_building(def, static_cast<float>(cell_x),
                        static_cast<float>(cell_y), state, 0.0f, 1.0f,
                        kind)) {
        return false;
    }
    park_session.budget -= static_cast<int>(def.construction_cost);
    park_session.ledger.spend_construction(
        static_cast<int>(def.construction_cost));
    rebuild_extra_items();
    std::printf("construction: %s cost=%u budget=%d cell=%d,%d\n",
                def.name.c_str(), def.construction_cost, park_session.budget,
                cell_x, cell_y);
    return true;

}

void Game::ensure_construction_catalog() {
    if (!construction_defs.empty()) {
        return;
    }
    construction_names = dino::load_building_list(
        resolve_asset(opt.asset_root,
                      "Data/Design/buildings/BuildingList.ini"));
    construction_defs.reserve(construction_names.size());
    for (const std::string& name : construction_names) {
        try {
            construction_defs.push_back(dino::load_bbi(resolve_asset(
                opt.asset_root,
                "Data/Design/buildings/" + name + ".bbi")));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "building %s: %s\n", name.c_str(),
                         e.what());
            construction_defs.push_back({});
        }
    }

}

std::uint32_t Game::ensure_road_price() {
    if (!road_price_loaded) {
        road_price_loaded = true;
        try {
            road_price = dino::load_general_params(
                             resolve_asset(opt.asset_root,
                                           "Data/Design/generalparams/"
                                           "GeneralParams.bgd"))
                             .road_price();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "general params: %s\n", e.what());
        }
    }
    return road_price;

}

void Game::road_message(const char* id) {
    if (park.loaded()) {
        park.show_message(dino::ui::resolve_loc_text(id, park.loc));
    }

}

bool Game::road_cell_busy(int x, int y) {
    const float cs = park_session.build_grid.cell_size;
    if (!terrain.buildable(park_session.build_grid.origin_x + (x + 0.5f) * cs,
                           park_session.build_grid.origin_y + (y + 0.5f) * cs)) {
        return true;
    }
    ensure_construction_catalog();
    const int wx = static_cast<int>(std::lround(
        park_session.build_grid.origin_x + x * cs + terrain.world_size * 0.5f));
    const int wy = static_cast<int>(std::lround(
        park_session.build_grid.origin_y + y * cs + terrain.world_size * 0.5f));
    const int wc = static_cast<int>(cs);
    for (const dino::PortBuildingState& b : building_instances) {
        if (b.kind < 0 || b.kind >= static_cast<int>(construction_defs.size())) {
            continue;
        }
        const dino::BuildingDef& d =
            construction_defs[static_cast<std::size_t>(b.kind)];
        const int bw = static_cast<int>(b.state % 2 == 0 ? d.footprint_w : d.footprint_h);
        const int bh = static_cast<int>(b.state % 2 == 0 ? d.footprint_h : d.footprint_w);
        if (wx < b.cell_x + bw && b.cell_x < wx + wc && wy < b.cell_y + bh &&
            b.cell_y < wy + wc) {
            return true;
        }
    }
    return false;

}

std::vector<dino::WorldRect> Game::road_perimeters() {
    std::vector<dino::WorldRect> rects;
    rects.reserve(park_session.paddocks.size());
    for (const dino::Paddock& p : park_session.paddocks) {
        rects.push_back(p.rect);
    }
    return rects;

}

void Game::arm_road_tool() {
    build_tool = 4;
    pending_construct_kind = -1;
    road_has_start = false;
    std::printf("road: tool armed, price=%u per cell\n", ensure_road_price());

}

void Game::road_click(int cx, int cy) {
    const std::vector<dino::WorldRect> perimeters = road_perimeters();
    if (!road_has_start) {
        const dino::RoadCheck check = dino::check_road_start(
            park_session.build_grid, cx, cy, perimeters, [this](int x, int y) { return road_cell_busy(x, y); });
        if (check != dino::RoadCheck::Ok) {
            std::printf("road: start %d,%d rejected (%s)\n", cx, cy,
                        dino::road_warning(check) + 1);
            road_message(dino::road_warning(check));
            return;
        }
        road_has_start = true;
        road_start_x = cx;
        road_start_y = cy;
        std::printf("road: start %d,%d\n", cx, cy);
        return;
    }
    const dino::RoadSegment road =
        dino::road_segment(road_start_x, road_start_y, cx, cy);
    const dino::RoadCheck check =
        dino::check_road(park_session.build_grid, road, perimeters, [this](int x, int y) { return road_cell_busy(x, y); });
    if (check != dino::RoadCheck::Ok) {
        std::printf("road: %d,%d -> %d,%d rejected (%s)\n", road.x0, road.y0,
                    road.x1, road.y1, dino::road_warning(check) + 1);
        road_message(dino::road_warning(check));
        return;
    }
    const int cost = dino::road_cost(road, ensure_road_price());
    if (park_session.budget < cost) {
        std::printf("road: needs %d, budget %d (WARNING_NOT_ENOUGH_MONEY)\n",
                    cost, park_session.budget);
        road_message("&WARNING_NOT_ENOUGH_MONEY");
        return;
    }
    const int added = dino::place_road(park_session.build_grid, road);
    park_session.budget -= cost;
    park_session.ledger.spend_construction(cost);
    rebuild_build_meshes();
    std::printf(
        "road: built %d,%d -> %d,%d cells=%d new=%d cost=%d budget=%d "
        "construction=%d/%d\n",
        road.x0, road.y0, road.x1, road.y1, road.cells(), added, cost,
        park_session.budget, park_session.ledger.construction_count,
        park_session.ledger.construction_total);
    // FUN_005f8830: chain from the run's endpoint nearest the click, in
    // the direction the run was dragged. `road_segment` stores x0<=x1 /
    // y0<=y1, so using the ordered max corner restarts at the wrong end
    // for a -x/-y drag and re-commits an overlapping segment.
    if (road.y0 == road.y1) {  // horizontal run
        road_start_x = cx > road_start_x ? road.x1 : road.x0;
        road_start_y = road.y0;
    } else {  // vertical run
        road_start_y = cy > road_start_y ? road.y1 : road.y0;
        road_start_x = road.x0;
    }

}

void Game::handle_store_buy(int index) {
    if (index < 0 || index >= static_cast<int>(store_catalog.size())) {
        return;
    }
    const dino::MarketEntry& entry =
        store_catalog[static_cast<std::size_t>(index)];
    if (park_session.budget < entry.price) {
        std::printf(
            "store: %s price=%d budget=%d "
            "(WARNING_NOT_ENOUGH_MONEY)\n",
            entry.name.c_str(), entry.price, park_session.budget);
        park.show_message(dino::ui::resolve_loc_text(
            "&WARNING_NOT_ENOUGH_MONEY", park.loc));
        return;
    }
    pending_store_index = index;
    pending_store_price = entry.price;
    park.overlay.set_visible("DINO_ACHAT_SAVE_PANEL", false);
    std::printf("store: %s price=%d selected, click a paddock\n",
                entry.name.c_str(), entry.price);

}

void Game::run_daily_settlement() {
    ensure_construction_catalog();
    int ticket_price = 50;
    for (const dino::BuildingDef& def : construction_defs) {
        if (def.category == 10) {
            ticket_price = static_cast<int>(def.price);
            break;
        }
    }
    const int visitors = static_cast<int>(park_session.visitor_system.visitors.size());
    if (park_session.all_visitor_profiles.empty()) {
        try {
            park_session.all_visitor_profiles =
                load_all_visitor_profiles(opt.asset_root);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "all visitor profiles: %s\n", e.what());
        }
    }
    const std::vector<dino::VisitorProfile>& ticket_profiles =
        park_session.all_visitor_profiles.empty() ? park_session.visitor_profiles
                                     : park_session.all_visitor_profiles;
    int tickets = 0;
    for (const dino::Visitor& visitor : park_session.visitor_system.visitors) {
        int index = visitor.global_profile;
        if (index < 0 ||
            index >= static_cast<int>(ticket_profiles.size())) {
            index = visitor.profile;
        }
        const float scale =
            (index >= 0 &&
             index < static_cast<int>(ticket_profiles.size()))
                ? ticket_profiles[static_cast<std::size_t>(index)]
                      .price_scale
                : 1.0f;
        tickets += static_cast<int>(
            static_cast<float>(ticket_price) * scale);
    }
    const int wages = park_session.employee_system.daily_wages(park_session.employee_profiles);
    for (const dino::Employee& employee : park_session.employee_system.employees) {
        if (employee.profile < 0 ||
            employee.profile >= static_cast<int>(park_session.employee_profiles.size())) {
            continue;
        }
        const dino::EmployeeProfile& profile =
            park_session.employee_profiles[static_cast<std::size_t>(employee.profile)];
        park_session.ledger.spend_staff(profile.kind, profile.wage);
    }
    park_session.budget += tickets - wages;
    park_session.ledger.add_ticket(tickets, visitors);
    park_session.ledger.settle();
    std::printf(
        "daily settlement: day=%d visitors=%d ticket=%d income=%d "
        "wages=%d budget=%d\n",
        park_session.clock.day, visitors, ticket_price, tickets, wages,
        park_session.budget);

}

std::array<int, 11> Game::store_row_attributes(const dino::MarketEntry& entry) {
    // Gauge order: ORIGINALITY/STAMINA/COURAGE/INTELLIGENCE/AGILITY/
    // SPEED/ARMOR/AGGRESSIVENESS/ATTACK/BEAUTY/PERCEPTION.
    std::array<int, 11> values{};
    if (!phenotype_table.valid) {
        return values;
    }
    try {
        const dino::DinoGenotype genotype = dino::read_gen_genotype(
            resolve_asset(opt.asset_root,
                          "Data/Design/Scenario/Genotype/" +
                              entry.genotype));
        const dino::DinoAttributes attrs =
            phenotype_table.attributes(genotype);
        values = {attrs.originality,
                  attrs.stamina,
                  attrs.courage,
                  attrs.intelligence,
                  attrs.agility,
                  static_cast<int>(attrs.walk_speed),
                  attrs.armor,
                  attrs.aggression,
                  // AttributeRange.ini caps Attack at 120; the combined
                  // five-part total is 179..345, so the store gauge is
                  // the jaw attack (+0x4c).
                  attrs.attack_head,
                  attrs.beauty,
                  attrs.perception};
    } catch (const std::exception& e) {
        std::fprintf(stderr, "store genotype %s: %s\n",
                     entry.genotype.c_str(), e.what());
    }
    return values;

}

void Game::refresh_store_rows() {
    std::vector<dino::ui::StoreRow> rows;
    rows.reserve(store_catalog.size());
    for (const dino::MarketEntry& entry : store_catalog) {
        std::string name = entry.name;
        std::replace(name.begin(), name.end(), '_', ' ');
        rows.push_back(
            {name, entry.price, store_row_attributes(entry),
             entry.genotype});
    }
    park.set_store_rows(std::move(rows));

}

void Game::apply_store_catalog(dino::Market market,
                                   const std::string& source) {
    store_catalog = std::move(market.entries);
    // An armed insertion belongs to the previous catalog. Never resolve
    // its row against the replacement vector (mission change or load).
    pending_store_index = -1;
    pending_store_price = 0;
    // The retail vector is rebuilt when the state activates; the port
    // refreshes the six-row viewport in place.
    refresh_store_rows();
    std::printf("store catalog: %s entries=%zu\n", source.c_str(),
                store_catalog.size());

}

Game::StorePreview* Game::store_preview_for(const dino::ui::StoreRow& row) {
    if (row.genotype.empty()) {
        return nullptr;
    }
    const auto found = store_previews.find(row.genotype);
    if (found != store_previews.end()) {
        return found->second.ready ? &found->second : nullptr;
    }
    StorePreview preview;
    try {
        const std::string path = resolve_asset(
            opt.asset_root,
            "Data/Design/Scenario/Genotype/" + row.genotype);
        const int head = dino::read_gen_head_type(path);
        const auto match = head_species.find(head);
        const std::string species =
            match != head_species.end() ? match->second : "allo";
        std::string family = family_table.family_of(species);
        if (family.empty()) {
            family = "LightBiped";
        }
        const auto assets =
            load_imported_species(species, family, opt.dino_clip);
        if (assets) {
            const dino::Animation* anim =
                assets->anim.bones.empty() ? nullptr : &assets->anim;
            dino::Mesh pose = build_skinned_mesh(
                assets->skin.mesh, assets->skin, assets->skeleton, anim,
                0.0, assets->vertex_bone, assets->bone_skel, false);
            if (assets->mesh.colors.size() == pose.positions.size()) {
                pose.colors = assets->mesh.colors;
            }
            if (!pose.positions.empty()) {
                dino::Vec3 lo{1e30f, 1e30f, 1e30f};
                dino::Vec3 hi{-1e30f, -1e30f, -1e30f};
                for (const dino::Vec3& p : pose.positions) {
                    lo.x = std::min(lo.x, p.x);
                    lo.y = std::min(lo.y, p.y);
                    lo.z = std::min(lo.z, p.z);
                    hi.x = std::max(hi.x, p.x);
                    hi.y = std::max(hi.y, p.y);
                    hi.z = std::max(hi.z, p.z);
                }
                preview.center = {(lo.x + hi.x) * 0.5f,
                                  (lo.y + hi.y) * 0.5f,
                                  (lo.z + hi.z) * 0.5f};
                preview.size = std::max(
                    {hi.x - lo.x, hi.y - lo.y, hi.z - lo.z});
                preview.texture = assets->texture;
                preview.gpu = upload_mesh(pose, glfns);
                preview.ready =
                    preview.gpu.vao != 0 && preview.size > 0.001f;
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "store preview: %s\n", e.what());
    }
    const std::string key = row.genotype;
    const bool ready = preview.ready;
    if (std::getenv("DINO_DEBUG_STORE_PREVIEW") != nullptr) {
        std::fprintf(stderr,
                     "store preview: %s genotype=%s verts=%d "
                     "size=%.2f texture=%u ready=%d\n",
                     row.name.c_str(), key.c_str(),
                     preview.gpu.index_count,
                     static_cast<double>(preview.size),
                     static_cast<unsigned>(preview.texture),
                     ready ? 1 : 0);
    }
    store_previews.emplace(key, std::move(preview));
    if (!ready) {
        return nullptr;
    }
    return &store_previews[key];

}

void Game::ensure_build_ready() {
    if (terrain.world_size <= 0.0f) {
        return;
    }
    if (!build_ready) {
        park_session.build_grid.init(opt.build_cell > 0.0f ? opt.build_cell : 4.0f,
                        terrain.world_size);
        park_session.build_grid.occupancy.init_from_mask(
            terrain.occupation, terrain.occupation_width,
            terrain.occupation_height);
        try {
            const std::string fence_mesh =
                opt.fence_mesh_path.empty()
                    ? resolve_asset(opt.asset_root,
                                    "Data/Graphics/paddock/fences/"
                                    "fence_paddock_lod1.mesh")
                    : opt.fence_mesh_path;
            // Chunk loader keeps the material table and per-triangle
            // material ids: material 1 is the laser2 lamp billboards.
            const std::vector<dino::Mesh> fence_chunks =
                dino::load_mesh_chunks(fence_mesh);
            if (fence_chunks.empty()) {
                throw std::runtime_error("fence mesh has no chunks");
            }
            fence_source = fence_chunks.front();
            build_ready = true;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "build enable: %s\n", e.what());
        }
        if (path_image.width == 0) {
            const std::string path_texture =
                opt.path_texture_path.empty()
                    ? resolve_asset(opt.asset_root,
                                    "Data/Graphics/Textures/buildings/"
                                    "ground.tga")
                    : opt.path_texture_path;
            try {
                path_image = dino::load_tga(path_texture);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "path texture: %s\n", e.what());
            }
        }
        if (fence_image.width == 0 &&
            !fence_source.texture_name().empty()) {
            const std::string fence_texture =
                opt.fence_texture_path.empty()
                    ? resolve_asset(
                          opt.asset_root,
                          "Data/Graphics/Textures/buildings/" +
                              fence_source.texture_name())
                    : opt.fence_texture_path;
            try {
                fence_image = dino::load_tga(fence_texture);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "fence texture: %s\n", e.what());
            }
        }
        if (path_texture == 0 && path_image.width > 0) {
            path_texture = upload_texture(path_image);
        }
        if (fence_texture == 0 && fence_image.width > 0) {
            fence_texture = upload_texture(fence_image);
        }
        // Fence.bwi material 1 and the gatAttraction ground atlases.
        if (fence_laser_image.width == 0) {
            load_asset_tga(opt.asset_root,
                           "Data/Graphics/Textures/buildings/laser2.tga",
                           fence_laser_image, "fence laser texture");
        }
        if (paddock_dirt_image.width == 0) {
            load_asset_tga(opt.asset_root,
                           "Data/Graphics/Textures/buildings/boue_pad.tga",
                           paddock_dirt_image, "paddock ground texture");
        }
        if (paddock_occup_image.width == 0) {
            load_asset_tga(
                opt.asset_root,
                "Data/Graphics/Textures/buildings/ground_occup_pad.tga",
                paddock_occup_image, "paddock occupancy texture");
        }
        if (fence_laser_texture == 0 && fence_laser_image.width > 0) {
            fence_laser_texture = upload_texture(fence_laser_image);
        }
        if (paddock_dirt_texture == 0 && paddock_dirt_image.width > 0) {
            paddock_dirt_texture =
                upload_texture_wrapped(paddock_dirt_image, GL_REPEAT);
        }
        if (paddock_occup_texture == 0 && paddock_occup_image.width > 0) {
            paddock_occup_texture =
                upload_texture_wrapped(paddock_occup_image, GL_REPEAT);
        }
    }
    if (park_session.visitor_profiles.empty()) {
        try {
            park_session.visitor_profiles = load_easy_visitor_profiles(opt.asset_root);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "visitor profiles: %s\n", e.what());
        }
    }
    if (visitor_pose1.positions.empty()) {
        const std::string pose1 =
            opt.visitor_pose1.empty()
                ? resolve_asset(opt.asset_root,
                                "Data/Graphics/Actors/people/customers/"
                                "wom/woman_walk1_LOD1.mesh")
                : opt.visitor_pose1;
        const std::string pose2 =
            opt.visitor_pose2.empty()
                ? resolve_asset(opt.asset_root,
                                "Data/Graphics/Actors/people/customers/"
                                "wom/woman_walk2_LOD1.mesh")
                : opt.visitor_pose2;
        const std::string texture =
            opt.visitor_texture.empty()
                ? resolve_asset(opt.asset_root,
                                "Data/Graphics/Textures/actors/people/"
                                "customers/wom.tga")
                : opt.visitor_texture;
        try {
            visitor_pose1 = dino::load_mesh(pose1);
            visitor_pose2 = dino::load_mesh(pose2);
            visitor_image = dino::load_image(texture);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "visitor assets: %s\n", e.what());
        }
    }

}

void Game::rebuild_build_meshes() {
    if (!build_ready) {
        return;
    }
    refresh_terrain_texture();
    const dino::BuildMeshes meshes =
        dino::build_placement_meshes(park_session.build_grid, terrain, fence_source,
                                     selected_paddock_rect());
    path_gpu.destroy(glfns);
    fence_gpu.destroy(glfns);
    fence_selected_gpu.destroy(glfns);
    fence_beams_gpu.destroy(glfns);
    fence_ground_gpu.destroy(glfns);
    path_gpu = upload_mesh(meshes.paths, glfns);
    fence_gpu = upload_mesh(meshes.fences, glfns);
    fence_selected_gpu = upload_mesh(meshes.fences_selected, glfns);
    fence_beams_gpu = upload_mesh(meshes.fence_beams, glfns);
    const dino::Mesh ground = dino::fence_ground_mesh(park_session.paddocks, terrain);
    if (std::getenv("DINO_DEBUG_GROUND") != nullptr) {
        std::printf("ground: %zu paddocks, %zu verts\n", park_session.paddocks.size(),
                    ground.positions.size());
    }
    fence_ground_gpu = upload_mesh(ground, glfns);
    rebuild_extra_items();
    std::printf("build: %d path cells, %d fences\n", park_session.build_grid.path_count,
                park_session.build_grid.fence_count);

}

void Game::sell_paddock_now(int index) {
    if (index < 0 || index >= static_cast<int>(park_session.paddocks.size())) {
        park_sell_paddock = -1;
        return;
    }
    const dino::Paddock pen = park_session.paddocks[static_cast<std::size_t>(index)];
    if (!pen.occupants.empty() || pen.used > 0.0f) {
        park.show_warning(dino::ui::resolve_loc_text(
            "&WARNING_SELLING_NON_EMPTY_FENCE", park.loc));
        park_sell_paddock = -1;
        std::printf("sell: enclosure %d refused (occupied)\n", index);
        return;
    }
    const int value = dino::fence_sell_value(
        pen.rect, park_session.build_grid.cell_size, 30, 4, park_session.sellback);
    const int x0 = std::min(pen.rect.x0, pen.rect.x1);
    const int x1 = std::max(pen.rect.x0, pen.rect.x1);
    const int y0 = std::min(pen.rect.y0, pen.rect.y1);
    const int y1 = std::max(pen.rect.y0, pen.rect.y1);
    const int cell = std::max(1, static_cast<int>(park_session.build_grid.cell_size));
    const int cx0 = x0 / cell;
    const int cx1 = x1 / cell;
    const int cy0 = y0 / cell;
    const int cy1 = y1 / cell;
    for (int x = cx0; x <= cx1; ++x) {
        park_session.build_grid.set_fence(x, cy0, 0, false);
        park_session.build_grid.set_fence(x, cy1 + 1, 0, false);
    }
    for (int y = cy0; y <= cy1; ++y) {
        park_session.build_grid.set_fence(cx0 - 1, y, 1, false);
        park_session.build_grid.set_fence(cx1, y, 1, false);
    }
    park_session.build_grid.occupancy.stamp_wall_rect(x0, y0, x1, y1, false);
    park_session.paddocks.erase(park_session.paddocks.begin() + index);
    if (selected_paddock == index) {
        selected_paddock = -1;
    } else if (selected_paddock > index) {
        --selected_paddock;
    }
    park_paddocks = park_session.paddocks;
    park_session.budget += value;
    park_sell_paddock = -1;
    rebuild_build_meshes();
    refresh_fence_info_panel();
    std::printf(
        "sell: sold enclosure %d refund=%d budget=%d sellback=%.2f\n",
        index, value, park_session.budget,
        static_cast<double>(park_session.sellback));

}

}  // namespace dino::app
