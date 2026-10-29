// Park lifecycle: island load, campaign park load, and the port state
// round-trip (.psav save/load).

#include "game.h"

namespace dino::app {

void Game::load_island(const std::string& island, bool reset_view) {
    entrance_road.reset();
    if (visitor_profiles.empty()) {
        try {
            visitor_profiles = load_easy_visitor_profiles(opt.asset_root);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "visitor profiles: %s\n", e.what());
        }
    }
    clear_buildings();
    const std::string lnd_path = resolve_asset(
        opt.asset_root,
        "Data/Design/Landscape/" + island + "/" + island + ".lnd");
    dino::Lnd new_lnd = dino::load_lnd(lnd_path);
    int hm_width = 0, hm_height = 0;
    const auto heights = dino::load_raw_u8(
        resolve_asset(opt.asset_root, new_lnd.heightmap), hm_width, hm_height,
        true);
    dino::Terrain new_terrain = dino::build_terrain(
        heights, hm_width, hm_height, opt.terrain_size, opt.height_scale);
    try {
        int occ_w = 0, occ_h = 0;
        new_terrain.occupation = dino::load_raw_u8(
            resolve_asset(opt.asset_root, new_lnd.occupation), occ_w, occ_h,
            true);
        new_terrain.occupation_width = occ_w;
        new_terrain.occupation_height = occ_h;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "occupation mask: %s\n", e.what());
    }
    dino::Vegetation new_vegetation;
    if (!opt.no_vegetation) {
        new_vegetation =
            dino::load_vegetation(opt.asset_root, new_lnd, new_terrain);
    }
    dino::Image new_image;
    if (opt.texture_path.empty() && !new_lnd.basemap.empty()) {
        new_image =
            dino::load_tga(resolve_asset(opt.asset_root, new_lnd.basemap));
    }
    dino::Image new_detail;
    if (!opt.terrain_lit && !new_lnd.detailmap.empty()) {
        try {
            new_detail = dino::load_tga(
                resolve_asset(opt.asset_root, new_lnd.detailmap));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "detail map: %s\n", e.what());
        }
    }
    std::array<dino::Image, 6> new_sky{};
    const std::string sky_dir =
        opt.skybox_dir.empty() ? resolve_asset(opt.asset_root, new_lnd.skybox)
                               : opt.skybox_dir;
    static const char* kFaces[6] = {"front", "back", "left",
                                    "right", "top", "bottom"};
    for (int i = 0; i < 6; ++i) {
        try {
            new_sky[i] =
                dino::load_image(sky_dir + "/" + kFaces[i] + ".PNG");
        } catch (const std::exception& e) {
            std::fprintf(stderr, "skybox face %s: %s\n", kFaces[i], e.what());
        }
    }
    dino::Image new_water;
    dino::Image new_water2;
    const float new_water_z =
        opt.water_level >= 0.0f ? opt.water_level : new_lnd.unknown_float;
    if (opt.water && !new_lnd.water.empty()) {
        try {
            new_water = dino::load_image(
                resolve_asset(opt.asset_root, new_lnd.water));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "water texture: %s\n", e.what());
        }
    }
    if (opt.water && opt.water2 && !new_lnd.water2.empty()) {
        try {
            new_water2 = dino::load_image(
                resolve_asset(opt.asset_root, new_lnd.water2));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "water2 texture: %s\n", e.what());
        }
    }

    // Release the previous island's GPU resources before replacing them.
    gpu.destroy(glfns);
    release_vegetation();
    for (int i = 0; i < 6; ++i) {
        skybox.faces[i].destroy(glfns);
        if (skybox.textures[i] != 0) {
            glDeleteTextures(1, &skybox.textures[i]);
        }
        skybox.textures[i] = 0;
    }
    skybox.ready = false;
    water_gpu.destroy(glfns);
    if (water_texture != 0) {
        glDeleteTextures(1, &water_texture);
        water_texture = 0;
    }
    if (water_texture2 != 0) {
        glDeleteTextures(1, &water_texture2);
        water_texture2 = 0;
    }
    visitor_gpu.destroy(glfns);
    if (visitor_texture_id != 0) {
        glDeleteTextures(1, &visitor_texture_id);
        visitor_texture_id = 0;
    }
    dino_gpu.destroy(glfns);
    shadow_gpu.destroy(glfns);
    clear_imported_dinos();
    if (opt.texture_path.empty()) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
            texture = 0;
        }
        image = std::move(new_image);
        detail_image = std::move(new_detail);
    }

    lnd = std::move(new_lnd);
    terrain = std::move(new_terrain);
    vegetation = std::move(new_vegetation);
    sky_images = std::move(new_sky);
    water_image = std::move(new_water);
    water2_image = std::move(new_water2);
    water_z = new_water_z;

    int loaded_faces = upload_island_graphics();

    build_island_park();

    place_island_entrance();
    rebuild_extra_items();

    compute_island_center();
    reset_island_camera(reset_view);

    std::printf(
        "island: %s lnd=%s terrain=%dx%d vegetation=%zu groups water=%.1f "
        "sky=%d\n",
        island.c_str(), lnd_path.c_str(), terrain.width, terrain.height,
        vegetation.groups.size(), water_z, loaded_faces);
    if (music.enabled && !lnd.music.empty()) {
        const auto music_it = park_sound_files.find(lnd.music);
        if (music_it != park_sound_files.end()) {
            music.play(music_it->second);
        }
    }
}

// Texture/detail/terrain/vegetation/sky/water GPU state for the new island.
int Game::upload_island_graphics() {
    if (opt.texture_path.empty() && image.width > 0) {
        texture = upload_texture(image);
    }
    if (detail_texture != 0) {
        glDeleteTextures(1, &detail_texture);
        detail_texture = 0;
    }
    if (detail_image.width > 0) {
        detail_texture = upload_detail_texture(detail_image, glfns);
    }
    display = terrain.mesh;
    gpu = upload_mesh(display, glfns);

    upload_vegetation();

    int loaded_faces = 0;
    for (int i = 0; i < 6; ++i) {
        if (sky_images[i].width <= 0) {
            continue;
        }
        skybox.faces[i] = upload_mesh(make_sky_face(i, kSkyboxHalfSize), glfns);
        skybox.textures[i] = upload_texture(sky_images[i]);
        if (skybox.faces[i].vao != 0 && skybox.textures[i] != 0) {
            ++loaded_faces;
        }
    }
    skybox.ready = loaded_faces == 6;

    if (water_image.width > 0 && terrain.world_size > 0.0f) {
        water_texture = upload_texture_wrapped(water_image, GL_REPEAT);
        if (water2_image.width > 0) {
            water_texture2 =
                upload_texture_wrapped(water2_image, GL_REPEAT);
        }
        water_gpu = upload_mesh(
            make_water_plane(terrain.world_size * 4.75f, water_z, 38.0f),
            glfns);
    }
    return loaded_faces;
}

// Build grid, --place-path/--place-fence cells and the visitor actors.
void Game::build_island_park() {
    if (build_ready) {
        build_grid.init(opt.build_cell, terrain.world_size);
        build_grid.occupancy.init_from_mask(
            terrain.occupation, terrain.occupation_width,
            terrain.occupation_height);
        for (const auto& value : opt.place_paths) {
            int x = 0, y = 0, edge = 0;
            if (std::sscanf(value.c_str(), "%d,%d,%d", &x, &y, &edge) >= 2) {
                const float cs = build_grid.cell_size;
                if (!terrain.buildable(
                        build_grid.origin_x +
                            (static_cast<float>(x) + 0.5f) * cs,
                        build_grid.origin_y +
                            (static_cast<float>(y) + 0.5f) * cs)) {
                    std::fprintf(stderr, "place-path: blocked cell %d,%d\n",
                                 x, y);
                    continue;
                }
                build_grid.set_path(x, y, true);
            }
        }
        for (const auto& value : opt.place_fences) {
            int x = 0, y = 0, edge = 0;
            if (std::sscanf(value.c_str(), "%d,%d,%d", &x, &y, &edge) == 3) {
                const float cs = build_grid.cell_size;
                auto free_cell = [&](int gx, int gy) {
                    return terrain.buildable(
                        build_grid.origin_x +
                            (static_cast<float>(gx) + 0.5f) * cs,
                        build_grid.origin_y +
                            (static_cast<float>(gy) + 0.5f) * cs);
                };
                auto edge_ok = [&](int gx, int gy, int e) {
                    if (!free_cell(gx, gy)) {
                        return false;
                    }
                    if (e == 1) {
                        return gx + 1 >= build_grid.width ||
                               free_cell(gx + 1, gy);
                    }
                    return gy <= 0 || free_cell(gx, gy - 1);
                };
                if (!edge_ok(x, y, edge)) {
                    std::fprintf(stderr,
                                 "place-fence: blocked cell %d,%d\n", x, y);
                    continue;
                }
                build_grid.set_fence(x, y, edge, true);
            }
        }
        build_grid.undo_stack.clear();
        visitor_system.visitors.clear();
        if (opt.visitors > 0) {
            const int spawned = visitor_profiles.empty()
                                    ? visitor_system.spawn(build_grid,
                                                           opt.visitors)
                                    : visitor_system.spawn(
                                          build_grid, opt.visitors,
                                          visitor_profiles,
                                          entrance_cell_x, entrance_cell_y);
            std::printf("visitors: %d spawned on %d path cells\n", spawned,
                        build_grid.path_count);
            if (opt.visitor_satisfaction >= 0) {
                for (dino::Visitor& visitor : visitor_system.visitors) {
                    visitor.need0 =
                        std::clamp(opt.visitor_satisfaction, 0, 99);
                }
            }
        }
        if (!visitor_system.visitors.empty()) {
            visitor_texture_id = upload_texture(visitor_image);
            visitor_gpu = upload_mesh(
                visitor_system.build_mesh(build_grid, terrain, visitor_pose1,
                                          visitor_pose2),
                glfns);
        }
        rebuild_build_meshes();
    }
}

void Game::place_island_entrance() {
    // Free Game handoff: the engine's `FUN_0051dad0` spawns the access
    // road from the Entrance (category 10) at the island's .lnd
    // width/height. Place the same Entrance boat at that cell.
    if (opt.place_entrance) {
        clear_buildings();
        try {
            const dino::BuildingDef entrance = dino::load_bbi(
                resolve_asset(opt.asset_root,
                              "Data/Design/buildings/Entrance.bbi"));
            const int state =
                std::clamp(static_cast<int>(lnd.unknown_int), 0, 3);
            // place_building records the access road; the Free Game
            // start lays it once the build grid exists.
            place_building(entrance, static_cast<float>(lnd.width),
                           static_cast<float>(lnd.height), state, 0.0f,
                           1.0f, 9);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "entrance: %s\n", e.what());
        }
    }
}

// Scene bounds and the orbit/close-up centre.
void Game::compute_island_center() {
    bounds = Bounds{};
    for (const auto& p : display.positions) {
        bounds.add(p);
    }
    for (const auto& group : vegetation.groups) {
        if (group.lod != 0) {
            continue;
        }
        for (const auto& p : group.mesh.positions) {
            bounds.add(p);
        }
    }

    place_dino = skinned;
    if (place_dino && opt.camera_target.empty() &&
        opt.distance <= 0.0f && !opt.camera_retail) {
        // --start-dino uses the demo close-up; the retail park camera
        // would fly to the island centre instead.
        camera_retail_model = false;
    }
    if (place_dino) {
        spawn_x = opt.spawn_x >= 0.0f ? opt.spawn_x
                                      : terrain.world_size * 0.5f;
        spawn_y = opt.spawn_y >= 0.0f ? opt.spawn_y
                                      : terrain.world_size * 0.5f;
        dino_gpu = upload_mesh(
            pose_dino(opt.anim_time >= 0.0 ? opt.anim_time : 0.0), glfns);
        const float half = terrain.world_size * 0.5f;
        shadow_gpu = upload_mesh(
            make_contact_shadow(
                spawn_x - half, spawn_y - half,
                terrain.height_at(spawn_x - half, spawn_y - half) + 0.06f,
                2.6f),
            glfns);
        for (const auto& p : pose_dino(0.0).positions) {
            bounds.add(p);
        }
        std::printf("spawn: x=%.1f y=%.1f z=%.2f angle=%.1f\n", spawn_x,
                    spawn_y,
                    terrain.height_at(spawn_x - half, spawn_y - half),
                    opt.dino_angle);
    }
    // The dino/shadow may have just replaced their GPU meshes; the draw
    // list holds pointers, so rebuild it after the uploads.
    rebuild_extra_items();

    const float half = terrain.world_size * 0.5f;
    center = place_dino
        ? dino::Vec3{spawn_x - half, spawn_y - half,
                     terrain.height_at(spawn_x - half, spawn_y - half) + 3.0f}
        : bounds.center();
    auto_distance = place_dino ? 22.0f
                               : std::max(2.0f, bounds.extent() * 1.7f);
}

// Initial camera: demo close-up or the retail camSwitcher state.
void Game::reset_island_camera(bool reset_view) {
    if (reset_view) {
        if (place_dino) {
            // Demo close-up for --start-dino; the original free game has
            // no dinosaur placed at the centre.
            center = dino::Vec3{
                spawn_x - terrain.world_size * 0.5f,
                spawn_y - terrain.world_size * 0.5f,
                terrain.height_at(spawn_x - terrain.world_size * 0.5f,
                                  spawn_y - terrain.world_size * 0.5f) +
                    3.0f};
            distance = opt.distance > 0.0f ? opt.distance : 22.0f;
            yaw = opt.yaw;
            pitch = opt.pitch;
        } else {
            // Retail camSwitcher ctor: target (512,512,50), eye
            // (128,128,250) in record coordinates, yaw 0, progress 0.1.
            // The first update glues the target to the terrain and the
            // eye flies in with the 6..2 XY filter.
            retail_camera.reset(terrain);
            if (camera_retail_model) {
                center = retail_camera.target_cur;
                yaw = 0.0f;
                const float radius =
                    camera_config.distance(retail_camera.progress);
                const float elevation =
                    camera_config.elevation(retail_camera.progress);
                distance = std::sqrt(radius * radius +
                                     elevation * elevation);
                pitch = std::atan2(elevation, radius) *
                        180.0f / 3.14159265f;
            } else {
                // Legacy orbit start used by --camera-orbit.
                center = {0.0f, 0.0f, terrain.height_at(0.0f, 0.0f)};
                const float horizontal =
                    camera_config.distance(camera_config.progress);
                const float elevation =
                    camera_config.elevation(camera_config.progress);
                distance = std::sqrt(horizontal * horizontal +
                                     elevation * elevation);
                yaw = 0.0f;
                pitch = std::asin(std::clamp(elevation / distance,
                                             -1.0f, 1.0f)) *
                        180.0f / 3.14159265f;
            }
        }
        free_pos = orbit_eye();
        if (free_camera) {
            enter_free_camera();
        }
    }
}

void Game::load_campaign_park(const std::string& world_path) {
    clear_imported_dinos();
    food_sources.clear();
    employee_system.employees.clear();
    wld_roads.clear();
    wld_fences.clear();
    wld_dinosaurs.clear();
    paddocks.clear();
    ensure_build_ready();
    try {
        const dino::WldWorld world = dino::read_wld(world_path);
        place_world_buildings(world);
        // Campaign show arenas: each drsShow waypoint is one placed .bfi
        // feature (index, rotation, cell offset). Draw them from the
        // FeatureList order used by the waypoints.
        for (ShowFeatureDraw& feature : show_feature_draws) {
            feature.gpu.destroy(glfns);
        }
        show_feature_draws.clear();
        std::vector<dino::BfiFeature> show_features;
        try {
            show_features = dino::load_bfi_feature_list(resolve_asset(
                opt.asset_root, "Data/Design/attractions/features"));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "campaign show features: %s\n", e.what());
        }
        auto ends_with = [](const std::string& text, const char* suffix) {
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
        std::size_t show_features_spawned = 0;
        std::size_t show_waypoints = 0;
        const float half_world = terrain.world_size * 0.5f;
        for (const dino::SavWorldPlaced& show : world.records.shows) {
            for (const dino::SavWorldWaypoint& wp : show.waypoints) {
                ++show_waypoints;
                if (wp.x >= show_features.size()) {
                    continue;
                }
                const dino::BfiFeature& feature =
                    show_features[static_cast<std::size_t>(wp.x)];
                std::string mesh_rel;
                for (const std::string& slot : feature.mesh_slots) {
                    if (ends_with(slot, ".mesh")) {
                        mesh_rel = slot;
                        break;
                    }
                }
                if (mesh_rel.empty()) {
                    continue;
                }
                try {
                    dino::Mesh mesh = dino::load_mesh(
                        resolve_asset(opt.asset_root, mesh_rel));
                    const std::string texture_name = mesh.texture_name();
                    GLuint texture = 0;
                    if (!texture_name.empty()) {
                        try {
                            texture = upload_texture(dino::load_tga(
                                resolve_asset(opt.asset_root,
                                              feature.texture_dir + "/" +
                                                  texture_name)));
                        } catch (const std::exception& e) {
                            std::fprintf(stderr,
                                         "campaign show texture: %s\n",
                                         e.what());
                        }
                    }
                    const float wx =
                        static_cast<float>(show.start_x + wp.z) - half_world;
                    const float wy =
                        static_cast<float>(show.start_y + wp.extra) - half_world;
                    mesh = place_on_terrain(
                        std::move(mesh), terrain, wx, wy,
                        static_cast<float>(wp.y) * 90.0f);
                    show_feature_draws.push_back(
                        {upload_mesh(mesh, glfns), texture});
                    ++show_features_spawned;
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "campaign show feature: %s\n",
                                 e.what());
                }
            }
        }
        std::printf("campaign show features: spawned=%zu/%zu features=%zu\n",
                    show_features_spawned, show_waypoints,
                    show_features.size());
        if (!opt.camera_target.empty()) {
            // Scripted campaign screenshots need a view independent of
            // the park's default start camera.
            float tx = 0.0f;
            float ty = 0.0f;
            if (std::sscanf(opt.camera_target.c_str(), "%f,%f", &tx,
                            &ty) == 2) {
                center = {tx, ty, terrain.height_at(tx, ty) + 2.5f};
                distance = opt.distance > 0.0f ? opt.distance : 12.0f;
                yaw = opt.yaw;
                pitch = opt.pitch;
                camera_retail_model = false;
            }
        }
        rebuild_extra_items();
        wld_dinosaurs = world.dinosaurs;
        for (const dino::RoadRect& road : world.roads) {
            wld_roads.push_back({road.x0, road.y0, road.x1, road.y1});
        }
        for (const dino::WorldFence& fence : world.fences) {
            wld_fences.push_back({fence.start_x, fence.start_y,
                                  fence.end_x, fence.end_y});
        }
        import_park_layout();
        load_dino_family_tables();
        spawn_world_dinos();
        // Campaign employees: the .wld simulation list stores the live
        // positions; the world record only carries the actor id. Retail
        // assigns profile index 0 (Nutritionist) because drsEmployee's
        // +0x20 profile field is never written on the load path.
        int spawned_employees = 0;
        const float half = terrain.world_size * 0.5f;
        if (!employee_profiles.empty()) {
            for (const dino::WldEmployeeState& saved :
                 world.employees_sim) {
                if (employee_system.hire(employee_profiles, 0,
                                         saved.x - half,
                                         saved.y - half)) {
                    ++spawned_employees;
                }
            }
        }
        std::printf(
            "campaign employees: spawned=%d/%zu profile=Nutritionist\n",
            spawned_employees, world.employees_sim.size());
        if (spawned_employees > 0 && !employee_pose.positions.empty()) {
            if (employee_image.width > 0 && employee_texture_id == 0) {
                employee_texture_id = upload_texture(employee_image);
            }
            employee_gpu = upload_mesh(
                employee_system.build_mesh(terrain, employee_pose), glfns);
            rebuild_extra_items();
            std::printf("campaign employees: rendered=%zu\n",
                        employee_system.employees.size());
        }
        // Food sources: list1 stores the live position and the profile's
        // nutrition constant (+0x12). Match that constant against the
        // .bsi nutrition to recover the FoodSourceList index.
        if (food_defs.empty()) {
            try {
                food_defs = dino::load_food_defs(
                    opt.asset_root,
                    resolve_asset(opt.asset_root,
                                  "Data/Design/foodsource"));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "campaign food defs: %s\n", e.what());
            }
        }
        int spawned_food = 0;
        for (const dino::WldFoodSource& saved : world.food_sources) {
            int def_index = -1;
            for (std::size_t i = 0; i < food_defs.size(); ++i) {
                if (std::fabs(food_defs[i].nutrition -
                              static_cast<float>(saved.field_12)) < 0.5f) {
                    def_index = static_cast<int>(i);
                    break;
                }
            }
            if (def_index < 0) {
                std::fprintf(stderr,
                             "campaign food: no def for nutrition %u\n",
                             saved.field_12);
                continue;
            }
            const float fx = saved.x - half;
            const float fy = saved.y - half;
            food_sources.push_back(
                {def_index, fx, fy, food_defs[def_index].quantity});
            ++spawned_food;
        }
        std::printf("campaign food sources: spawned=%d/%zu\n",
                    spawned_food, world.food_sources.size());
        if (spawned_food > 0 && food_gpu.vao == 0 &&
            !food_sources.empty()) {
            // The renderer has one food draw item, so merge every
            // campaign source into a single mesh at its saved position.
            dino::Mesh combined;
            auto append = [&](const dino::Mesh& src) {
                const auto base = static_cast<std::uint32_t>(
                    combined.positions.size());
                combined.positions.insert(combined.positions.end(),
                                          src.positions.begin(),
                                          src.positions.end());
                if (src.normals.size() == src.positions.size()) {
                    combined.normals.insert(combined.normals.end(),
                                            src.normals.begin(),
                                            src.normals.end());
                }
                if (src.uvs.size() == src.positions.size()) {
                    combined.uvs.insert(combined.uvs.end(),
                                        src.uvs.begin(), src.uvs.end());
                }
                for (const std::uint32_t index : src.indices) {
                    combined.indices.push_back(base + index);
                }
            };
            try {
                for (const dino::FoodSource& source : food_sources) {
                    const dino::FoodDef& def =
                        food_defs[static_cast<std::size_t>(source.def)];
                    dino::Mesh mesh = dino::load_mesh(
                        resolve_asset(opt.asset_root, def.mesh));
                    mesh = place_on_terrain(std::move(mesh), terrain,
                                            source.x, source.y, 0.0f);
                    append(mesh);
                    if (food_texture == 0 && !def.texture.empty()) {
                        food_texture = upload_texture(dino::load_tga(
                            resolve_asset(opt.asset_root, def.texture)));
                    }
                }
                food_gpu = upload_mesh(combined, glfns);
                rebuild_extra_items();
                std::printf(
                    "campaign food sources: rendered=%zu vertices=%zu\n",
                    food_sources.size(), combined.positions.size());
            } catch (const std::exception& e) {
                std::fprintf(stderr, "campaign food mesh: %s\n", e.what());
            }
        }
        print_paddock_totals();
        std::printf(
            "campaign park: %s buildings=%zu roads=%zu fences=%zu "
            "dinos=%zu shows=%zu animals=%zu employees=%zu\n",
            world_path.c_str(), world.buildings.size(), wld_roads.size(),
            wld_fences.size(), imported_dinos.size(),
            world.records.shows.size(), world.records.animals.size(),
            world.records.employees.size());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "campaign park: %s\n", e.what());
    }

}

void Game::apply_port_state(const dino::PortState& state) {
    if (!state.island.empty() && state.island != current_island) {
        load_island(state.island, false);
        current_island = state.island;
    }
    ensure_build_ready();
    park_budget = state.budget;
    park_clock.day = state.day;
    park_clock.elapsed = state.elapsed;
    park_clock.time_scale = state.time_scale > 0.0 ? state.time_scale : 60.0;
    center = state.target;
    yaw = state.yaw;
    pitch = std::clamp(state.pitch, -89.0f, 89.0f);
    distance = std::max(1.0f, state.distance);
    free_camera = state.free_camera;
    free_pos = state.free_pos;
    free_yaw = state.free_yaw;
    free_pitch = state.free_pitch;
    // The port state stores the orbit form (target/yaw/pitch/distance).
    // Rebuild the retail camera's eye/target/goals from it so a loaded
    // session keeps its view instead of snapping back to the start state.
    if (camera_retail_model && terrain.world_size > 0.0f) {
        const float yaw_rad = state.yaw * 3.14159265f / 180.0f;
        const float pitch_rad = pitch * 3.14159265f / 180.0f;
        const dino::Vec3 eye{
            center.x + distance * std::cos(pitch_rad) * std::cos(yaw_rad),
            center.y + distance * std::cos(pitch_rad) * std::sin(yaw_rad),
            center.z + distance * std::sin(pitch_rad)};
        const float height = std::max(0.0f, eye.z - center.z);
        const float progress = std::clamp(
            (height - camera_config.elevation_min) /
                std::max(1e-3f, camera_config.elevation_max -
                                   camera_config.elevation_min),
            0.0f, 1.0f);
        retail_camera.set_saved(eye, center, progress);
        camera_progress = progress;
    }

    apply_port_build_grid(state);

    apply_port_visitors(state);

    apply_port_buildings(state);

    apply_port_dinos(state);

    rebuild_build_meshes();
    if (!visitor_system.visitors.empty() &&
        !visitor_pose1.positions.empty()) {
        if (visitor_texture_id == 0 && visitor_image.width > 0) {
            visitor_texture_id = upload_texture(visitor_image);
        }
        const dino::Mesh visitor_mesh = visitor_system.build_mesh(
            build_grid, terrain, visitor_pose1, visitor_pose2);
        if (visitor_gpu.vao == 0) {
            visitor_gpu = upload_mesh(visitor_mesh, glfns);
        } else {
            update_mesh(visitor_gpu, visitor_mesh, glfns);
        }
    } else if (visitor_gpu.vao != 0) {
        visitor_gpu.destroy(glfns);
    }

    apply_port_paddocks(state);

    apply_port_food_and_employees(state);

    rebuild_extra_items();
    minimap_rendered_scale = -1.0f;
    minimap_focus = {1e30f, 1e30f, 1e30f};
    minimap_heading = 1e30f;
    if (opt.dino_action_loop) {
        log_dino_food();
    }
    std::printf("port load: island=%s budget=%d day=%d visitors=%zu "
                "paths=%d fences=%d buildings=%zu dinos=%zu "
                "paddocks=%zu food=%zu\n",
                current_island.c_str(), park_budget, park_clock.day,
                visitor_system.visitors.size(), build_grid.path_count,
                build_grid.fence_count, building_instances.size(),
                imported_dinos.size(), paddocks.size(),
                food_sources.size());

    resume_port_campaign(state);
}

// Save rows: cells, roads and fences.
void Game::apply_port_build_grid(const dino::PortState& state) {
    build_grid.init(state.cell_size > 0.0f ? state.cell_size : 4.0f,
                    terrain.world_size);
    build_grid.occupancy.init_from_mask(terrain.occupation,
                                        terrain.occupation_width,
                                        terrain.occupation_height);
    for (const auto& cell : state.paths) {
        build_grid.set_path(cell.first, cell.second, true);
    }
    for (const auto& road : state.roads) {
        dino::add_road(build_grid, {road[0], road[1], road[2], road[3]});
    }
    for (const dino::PortFenceState& fence : state.fences) {
        build_grid.set_fence(fence.x, fence.y, fence.edge, true);
    }
    build_grid.undo_stack.clear();
}

// Save rows: visitor actors and their state machine.
void Game::apply_port_visitors(const dino::PortState& state) {
    visitor_system.visitors.clear();
    for (const dino::PortVisitorState& saved : state.visitors) {
        dino::Visitor visitor;
        visitor.x = saved.x;
        visitor.y = saved.y;
        visitor.angle = saved.angle;
        visitor.phase = saved.phase;
        visitor.cell_x = saved.cell_x;
        visitor.cell_y = saved.cell_y;
        visitor.target_x = saved.target_x;
        visitor.target_y = saved.target_y;
        visitor.prev_x = saved.prev_x;
        visitor.prev_y = saved.prev_y;
        visitor.profile = visitor_profiles.empty()
                              ? 0
                              : std::clamp(
                                    saved.profile, 0,
                                    static_cast<int>(
                                        visitor_profiles.size()) -
                                        1);
        visitor.need0 = saved.needs[0];
        visitor.need1 = saved.needs[1];
        visitor.need2 = saved.needs[2];
        visitor.global_profile = saved.global_profile;
        visitor.need0_at_use = saved.need0_at_use;
        visitor.state = static_cast<dino::VisitorState>(
            std::clamp(saved.state, 0, 3));
        visitor.target_attraction = saved.target_attraction;
        visitor.route = saved.route;
        visitor.route_index = static_cast<std::size_t>(std::clamp(
            saved.route_index, 0, static_cast<int>(visitor.route.size())));
        visitor.use_timer = saved.use_timer;
        visitor.decay0 = saved.decay0;
        visitor.decay1 = saved.decay1;
        visitor.decay2 = saved.decay2;
        visitor.done = saved.done;
        visitor.rng = saved.rng != 0
                          ? saved.rng
                          : 0x51ce5eedu ^
                                static_cast<unsigned>(
                                    visitor_system.visitors.size() + 1);
        visitor_system.visitors.push_back(visitor);
    }
}

// Save rows: placed buildings (kind, cell, orientation).
void Game::apply_port_buildings(const dino::PortState& state) {
    if (!state.buildings.empty()) {
        clear_buildings();
        try {
            const std::vector<std::string> names = dino::load_building_list(
                resolve_asset(opt.asset_root,
                              "Data/Design/buildings/BuildingList.ini"));
            for (const dino::PortBuildingState& saved : state.buildings) {
                if (saved.kind < 0 ||
                    saved.kind >= static_cast<int>(names.size())) {
                    continue;
                }
                try {
                    const dino::BuildingDef def = dino::load_bbi(
                        resolve_asset(opt.asset_root,
                                      "Data/Design/buildings/" +
                                          names[static_cast<std::size_t>(
                                              saved.kind)] +
                                          ".bbi"));
                    place_building(def,
                                   static_cast<float>(saved.cell_x),
                                   static_cast<float>(saved.cell_y),
                                   saved.state, 0.0f, 1.0f, saved.kind);
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "port building %d: %s\n",
                                 saved.kind, e.what());
                }
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "port buildings: %s\n", e.what());
        }
    }
}

// Save rows: imported dinosaurs, needs and the live action loop.
void Game::apply_port_dinos(const dino::PortState& state) {
    if (!state.dinos.empty()) {
        clear_imported_dinos();
        for (const dino::PortDinoState& saved : state.dinos) {
            try {
                if (!add_imported_dino(saved.species, saved.family,
                                       saved.clip, saved.x, saved.y,
                                       saved.angle)) {
                    continue;
                }
                ImportedDino& dinosaur = imported_dinos.back();
                dino::DinoNeeds restored;
                restored.value = saved.needs;
                restored.size_cm = saved.size_cm;
                restored.threat =
                    opt.dino_threat > 0 ? opt.dino_threat : saved.threat;
                restored.threat_radius =
                    opt.dino_radius > 0 ? opt.dino_radius
                                        : saved.threat_radius;
                restored.combined_attack = saved.combined_attack;
                restored.obstacle_radius = saved.obstacle_radius;
                restored.health = std::max(0, opt.dino_health);
                restored.max_health = std::max(0, opt.dino_max_health);
                dinosaur.needs = restored;
                dinosaur.carnivore = saved.carnivore;
                dinosaur.phase = saved.phase;
                if (dinosaur.action_loop) {
                    dinosaur.action_loop->restore(
                        restored, saved.action_id, saved.action_left);
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "port dino %s: %s\n",
                             saved.species.c_str(), e.what());
            }
        }
        rebuild_extra_items();
    }
}

void Game::apply_port_paddocks(const dino::PortState& state) {
    // Restore the live paddock model and placed food sources. Older port
    // saves omit these records.
    if (!state.paddocks.empty()) {
        paddocks.clear();
        for (const dino::PortPaddockState& saved : state.paddocks) {
            dino::Paddock paddock;
            paddock.rect = {saved.x0, saved.y0, saved.x1, saved.y1};
            paddock.capacity = saved.capacity;
            paddock.used = saved.used;
            paddock.occupants = saved.occupants;
            // Retail rebuilds stFence on load, so the ground quadrant is
            // drawn again from rand() & 3 and the name regenerated.
            paddock.ground_variant = std::rand() & 3;
            paddock.name =
                "Enclosure" + std::to_string(paddocks.size() + 1);
            paddocks.push_back(std::move(paddock));
        }
        park_paddocks = paddocks;
    }
}

// Save rows: food sources and employees, with their GPU actors.
void Game::apply_port_food_and_employees(const dino::PortState& state) {
    if (!state.food.empty() || !state.employees.empty()) {
        if (food_defs.empty()) {
            try {
                food_defs = dino::load_food_defs(
                    opt.asset_root,
                    resolve_asset(opt.asset_root,
                                  "Data/Design/foodsource"));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "port food defs: %s\n", e.what());
            }
        }
        food_sources.clear();
        for (const dino::PortFoodState& saved : state.food) {
            int def_index = -1;
            for (std::size_t i = 0; i < food_defs.size(); ++i) {
                if (food_defs[i].name == saved.name) {
                    def_index = static_cast<int>(i);
                    break;
                }
            }
            if (def_index >= 0) {
                food_sources.push_back(
                    {def_index, saved.x, saved.y, saved.remaining});
            }
        }
        if (!state.employees.empty()) {
            employee_system.employees.clear();
            for (const dino::PortEmployeeState& saved : state.employees) {
                int profile_index = -1;
                for (std::size_t i = 0; i < employee_profiles.size(); ++i) {
                    if (employee_profiles[i].name == saved.profile) {
                        profile_index = static_cast<int>(i);
                        break;
                    }
                }
                if (profile_index >= 0) {
                    employee_system.hire(employee_profiles, profile_index,
                                         saved.x, saved.y);
                }
            }
            std::printf("port employees: %zu\n",
                        employee_system.employees.size());
            if (employee_gpu.vao == 0 &&
                !employee_pose.positions.empty()) {
                if (employee_image.width > 0 &&
                    employee_texture_id == 0) {
                    employee_texture_id = upload_texture(employee_image);
                }
                employee_gpu = upload_mesh(
                    employee_system.build_mesh(terrain, employee_pose),
                    glfns);
                rebuild_extra_items();
            }
        }
        if (!food_sources.empty()) {
            const dino::FoodDef& def =
                food_defs[static_cast<std::size_t>(
                    food_sources.front().def)];
            try {
                if (food_gpu.vao != 0) {
                    food_gpu.destroy(glfns);
                    food_gpu = GpuMesh{};
                }
                dino::Mesh mesh = dino::load_mesh(
                    resolve_asset(opt.asset_root, def.mesh));
                mesh = place_on_terrain(
                    std::move(mesh), terrain, food_sources.front().x,
                    food_sources.front().y, 0.0f);
                food_gpu = upload_mesh(mesh, glfns);
                if (!def.texture.empty()) {
                    food_texture = upload_texture(dino::load_tga(
                        resolve_asset(opt.asset_root, def.texture)));
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "port food mesh: %s\n", e.what());
            }
        }
    }
}

void Game::resume_port_campaign(const dino::PortState& state) {
    // Resume the campaign mission stored in the port save. The objective
    // verdicts are derived again by the first runtime tick, so only the
    // mission id is persisted.
    if (!state.campaign_mission.empty()) {
        const std::size_t separator =
            state.campaign_mission.find("_Mission");
        const std::string campaign_name =
            separator == std::string::npos
                ? std::string()
                : state.campaign_mission.substr(0, separator);
        if (!campaign_name.empty()) {
            const std::string mission_path =
                (std::filesystem::path(resolve_asset(
                     opt.asset_root, "Data/Design/Scenario")) /
                 campaign_name / (state.campaign_mission + ".ini"))
                    .string();
            try {
                start_campaign_mission(
                    dino::load_campaign_mission(mission_path));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "port mission: %s\n", e.what());
            }
        }
    }
}

dino::PortState Game::make_port_state() {
    dino::PortState state;
    state.island = current_island;
    if (campaign_runtime) {
        state.campaign_mission = campaign_runtime->mission().name;
    }
    if (state.campaign_mission.empty() &&
        !opt.start_mission_path.empty()) {
        state.campaign_mission =
            std::filesystem::path(opt.start_mission_path)
                .stem()
                .string();
    }
    state.difficulty = std::clamp(menu.difficulty, 0, 2);
    state.budget = park_budget;
    state.day = park_clock.day;
    state.elapsed = park_clock.elapsed;
    state.time_scale = park_clock.time_scale;
    state.target = center;
    state.yaw = yaw;
    state.pitch = pitch;
    state.distance = distance;
    state.free_camera = free_camera;
    state.free_pos = free_pos;
    state.free_yaw = free_yaw;
    state.free_pitch = free_pitch;
    state.cell_size = build_grid.cell_size;
    state.origin_x = build_grid.origin_x;
    state.origin_y = build_grid.origin_y;
    state.width = build_grid.width;
    state.height = build_grid.height;
    for (const dino::WorldRect& road : build_grid.roads) {
        state.roads.push_back({road.x0, road.y0, road.x1, road.y1});
    }
    for (int cy = 0; cy < build_grid.height; ++cy) {
        for (int cx = 0; cx < build_grid.width; ++cx) {
            if (build_grid.path_at(cx, cy)) {
                state.paths.emplace_back(cx, cy);
            }
            for (int edge = 0; edge < 2; ++edge) {
                if (build_grid.fence_at(cx, cy, edge)) {
                    state.fences.push_back({cx, cy, edge});
                }
            }
        }
    }
    for (const dino::Visitor& visitor : visitor_system.visitors) {
        dino::PortVisitorState saved;
        saved.x = visitor.x;
        saved.y = visitor.y;
        saved.angle = visitor.angle;
        saved.phase = visitor.phase;
        saved.cell_x = visitor.cell_x;
        saved.cell_y = visitor.cell_y;
        saved.target_x = visitor.target_x;
        saved.target_y = visitor.target_y;
        saved.prev_x = visitor.prev_x;
        saved.prev_y = visitor.prev_y;
        saved.profile = visitor.profile;
        saved.needs = {visitor.need0, visitor.need1, visitor.need2};
        saved.global_profile = visitor.global_profile;
        saved.need0_at_use = visitor.need0_at_use;
        saved.state = static_cast<int>(visitor.state);
        saved.target_attraction = visitor.target_attraction;
        saved.route_index = static_cast<int>(visitor.route_index);
        saved.use_timer = visitor.use_timer;
        saved.decay0 = visitor.decay0;
        saved.decay1 = visitor.decay1;
        saved.decay2 = visitor.decay2;
        saved.rng = visitor.rng;
        saved.done = visitor.done;
        saved.route = visitor.route;
        state.visitors.push_back(saved);
    }
    state.buildings = building_instances;
    for (const ImportedDino& dinosaur : imported_dinos) {
        dino::PortDinoState saved;
        saved.species = dinosaur.species;
        saved.family = dinosaur.family;
        saved.clip = dinosaur.clip;
        saved.x = dinosaur.x;
        saved.y = dinosaur.y;
        saved.angle = dinosaur.angle;
        saved.carnivore = dinosaur.carnivore;
        saved.phase = dinosaur.phase;
        if (dinosaur.action_loop) {
            saved.needs = dinosaur.action_loop->needs().value;
            saved.size_cm = dinosaur.action_loop->needs().size_cm;
            saved.action_id = dinosaur.action_loop->action_id();
            saved.action_left = dinosaur.action_loop->remaining();
        } else {
            saved.needs = dinosaur.needs.value;
            saved.size_cm = dinosaur.needs.size_cm;
        }
        const dino::DinoNeeds& needs =
            dinosaur.action_loop ? dinosaur.action_loop->needs()
                                 : dinosaur.needs;
        saved.threat = needs.threat;
        saved.threat_radius = needs.threat_radius;
        saved.combined_attack = needs.combined_attack;
        saved.obstacle_radius = needs.obstacle_radius;
        state.dinos.push_back(saved);
    }
    for (const dino::Paddock& paddock : paddocks) {
        dino::PortPaddockState saved;
        saved.x0 = paddock.rect.x0;
        saved.y0 = paddock.rect.y0;
        saved.x1 = paddock.rect.x1;
        saved.y1 = paddock.rect.y1;
        saved.capacity = paddock.capacity;
        saved.used = paddock.used;
        saved.occupants = paddock.occupants;
        state.paddocks.push_back(std::move(saved));
    }
    for (const dino::FoodSource& food : food_sources) {
        if (food.def < 0 ||
            food.def >= static_cast<int>(food_defs.size())) {
            continue;
        }
        dino::PortFoodState saved;
        saved.name =
            food_defs[static_cast<std::size_t>(food.def)].name;
        saved.x = food.x;
        saved.y = food.y;
        saved.remaining = food.remaining;
        state.food.push_back(std::move(saved));
    }
    for (const dino::Employee& employee : employee_system.employees) {
        if (employee.profile < 0 ||
            employee.profile >= static_cast<int>(employee_profiles.size())) {
            continue;
        }
        dino::PortEmployeeState saved;
        saved.profile =
            employee_profiles[static_cast<std::size_t>(employee.profile)]
                .name;
        saved.x = employee.x;
        saved.y = employee.y;
        state.employees.push_back(std::move(saved));
    }
    return state;

}

void Game::do_port_save(const std::string& path) {
    try {
        dino::save_port_state(make_port_state(), path);
        std::printf("port save: %s\n", path.c_str());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "port save: %s\n", e.what());
    }

}

void Game::do_port_load(const std::string& path) {
    try {
        apply_port_state(dino::load_port_state(path));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "port load: %s\n", e.what());
    }

}

std::string Game::save_name_sanitized() {
    std::string name = opt.save_name;
    for (char& c : name) {
        const bool ok = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            c = '_';
        }
    }
    if (name.empty()) {
        name = "quicksave";
    }
    return name;

}

std::string Game::port_save_target(bool for_load) {
    const int selected = park.save_selected();
    if (selected >= 0 &&
        selected < static_cast<int>(save_slots.size())) {
        const std::string& candidate =
            save_slots[static_cast<std::size_t>(selected)].first;
        if (ends_with(candidate, ".psav")) {
            return candidate;
        }
    }
    if (for_load && !opt.load_port_path.empty()) {
        return opt.load_port_path;
    }
    std::filesystem::create_directories("run/save");
    return std::string("run/save/") + save_name_sanitized() + ".psav";

}

void Game::import_park_layout() {
    if (wld_roads.empty() && wld_fences.empty()) {
        return;
    }
    // A new park invalidates the selected paddock index.
    selected_paddock = -1;
    dino::import_roads(build_grid, wld_roads);
    dino::import_fences(build_grid, wld_fences);
    rebuild_build_meshes();
    std::printf(
        "park import: roads=%zu path_cells=%d fences=%zu fence_edges=%d\n",
        wld_roads.size(), build_grid.path_count, wld_fences.size(),
        build_grid.fence_count);

}

}  // namespace dino::app
