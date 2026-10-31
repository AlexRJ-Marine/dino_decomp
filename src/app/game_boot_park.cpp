// Startup, part 2: park scene resources, park and title objects,
// dinosaurs, food, employees, camera, tools and the final state restore.

#include "game.h"

#include <cctype>

namespace dino::app {

std::optional<int> Game::boot_scene_resources() {
    vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::printf("SDL %d.%d.%d | GL %s | %s | %s\n",
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
                version ? version : "?", renderer ? renderer : "?", vendor ? vendor : "?");

    skinned = !skin.bones.empty();
    if (skinned) {
        build_skin_mapping(skin, skeleton, vertex_bone, bone_skel);
    }

    if (mesh.positions.empty() && !skinned && opt.lnd_path.empty()) {
        dino::Mesh fallback;
        fallback.positions = {{-0.62f, -0.52f, 0.0f}, {0.02f, 0.68f, 0.0f}, {0.68f, -0.44f, 0.0f}};
        fallback.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
        fallback.colors = {{246, 140, 41, 255}, {46, 199, 245, 255}, {235, 61, 112, 255}};
        fallback.indices = {0, 1, 2};
        mesh = std::move(fallback);
    }

    texture = image.width > 0 ? upload_texture(image) : 0;
    detail_texture = detail_image.width > 0 ? upload_detail_texture(detail_image, glfns) : 0;
    dino_texture = dino_image.width > 0 ? upload_texture(dino_image) : 0;
    hud_font_texture = (opt.hud && hud_font_ready) ? upload_font_atlas(hud_font) : 0;
    hud_visible = true;

    if (!opt.video_play_path.empty()) {
        if (!play_video(opt.video_play_path, opt.video_play_start,
                        opt.video_play_frames)) {
            return 1;
        }
        return 0;
    }
    play_pending_cinematic();
    place_dino = skinned && !opt.lnd_path.empty();
    spawn_x = opt.spawn_x >= 0.0f ? opt.spawn_x : terrain.world_size * 0.5f;
    spawn_y = opt.spawn_y >= 0.0f ? opt.spawn_y : terrain.world_size * 0.5f;
    if (!opt.lnd_path.empty()) {
        display = terrain.mesh;
    } else if (skinned) {
        display = build_skinned_mesh(skin.mesh, skin, skeleton,
                                     anim.bones.empty() ? nullptr : &anim,
                                     opt.anim_time >= 0.0 ? opt.anim_time : 0.0,
                                     vertex_bone, bone_skel,
                                     opt.skin_bind_from_skin);
        if (mesh.colors.size() == display.positions.size()) {
            display.colors = mesh.colors;
        }
    } else {
        display = mesh;
    }
    gpu = upload_mesh(display, glfns);
    if (place_dino) {
        dino_gpu = upload_mesh(pose_dino(opt.anim_time >= 0.0 ? opt.anim_time : 0.0),
                               glfns);
        const float half = terrain.world_size * 0.5f;
        shadow_gpu = upload_mesh(
            make_contact_shadow(spawn_x - half, spawn_y - half,
                                terrain.height_at(spawn_x - half, spawn_y - half) + 0.06f,
                                2.6f),
            glfns);
        std::printf("spawn: x=%.1f y=%.1f z=%.2f angle=%.1f\n", spawn_x, spawn_y,
                    terrain.height_at(spawn_x - terrain.world_size * 0.5f,
                                      spawn_y - terrain.world_size * 0.5f),
                    opt.dino_angle);
    }
    danger_timer = 1.0;  // rebuild on the first frame;
    dino_initial_needs = {50, 99, 99, 99};
    if (std::sscanf(opt.dino_needs.c_str(), "%d,%d,%d,%d",
                    &dino_initial_needs[0], &dino_initial_needs[1],
                    &dino_initial_needs[2], &dino_initial_needs[3]) == 4) {
        for (int& value : dino_initial_needs) {
            value = std::clamp(value, 0, 99);
        }
    }
    if (opt.dino_action_loop) {
        try {
            dino_action_table = dino::load_action_table(opt.asset_root);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "dino action table: %s\n", e.what());
        }
    }
    // Per-family turn/gesture durations: the last 51 floats of each
    // Actions.bad block. Loaded even without --dino-action-loop so the mover
    // uses the species duration.
    try {
        park_session.actions_bad = dino::load_actions_bad(
            resolve_asset(opt.asset_root, "Data/Design/Actions.bad"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "actions bad: %s\n", e.what());
    }
    for (const auto& p : display.positions) {
        bounds.add(p);
    }
    if (place_dino) {
        for (const auto& p : pose_dino(0.0).positions) {
            bounds.add(p);
        }
    }
    upload_vegetation();
    for (const auto& group : vegetation.groups) {
        if (group.lod != 0) {
            continue;
        }
        for (const auto& p : group.mesh.positions) {
            bounds.add(p);
        }
    }
    road_preview_ok = true;
    road_preview_texture = 0;  // focus/road.tga x material colour;
    path_texture = 0;
    fence_texture = 0;
    fence_laser_texture = 0;
    paddock_dirt_texture = 0;
    paddock_occup_texture = 0;
    park_pulse_time = 0.0;
    park_pulse_started = false;
    lamp_tint = {1.0f, 1.0f, 1.0f};
    selected_tint = {1.0f, 1.0f, 1.0f};
    selected_paddock = -1;
    if (build_ready) {
        path_texture = path_image.width > 0 ? upload_texture(path_image) : 0;
        fence_texture = fence_image.width > 0 ? upload_texture(fence_image) : 0;
        fence_laser_texture =
            fence_laser_image.width > 0 ? upload_texture(fence_laser_image) : 0;
        // dino_attractionground.ini sets Wrap U/V; stage 1 samples the next
        // 128x128 quadrant, which needs wrapping at 1.0.
        paddock_dirt_texture =
            paddock_dirt_image.width > 0
                ? upload_texture_wrapped(paddock_dirt_image, GL_REPEAT)
                : 0;
        paddock_occup_texture =
            paddock_occup_image.width > 0
                ? upload_texture_wrapped(paddock_occup_image, GL_REPEAT)
                : 0;
        const dino::BuildMeshes meshes =
            dino::build_placement_meshes(park_session.build_grid, terrain, fence_source,
                                         selected_paddock_rect());
        path_gpu = upload_mesh(meshes.paths, glfns);
        fence_gpu = upload_mesh(meshes.fences, glfns);
        fence_selected_gpu = upload_mesh(meshes.fences_selected, glfns);
        fence_beams_gpu = upload_mesh(meshes.fence_beams, glfns);
        fence_ground_gpu =
            upload_mesh(dino::fence_ground_mesh(park_session.paddocks, terrain), glfns);
    }
    fence_preview_visible = false;
    fence_preview_ok = true;
    fence_preview_cell_x = -1;
    fence_preview_cell_y = -1;
    fence_preview_cost = -1;  // &MESSAGE_FENCE_PERIMETER_COST value;
    fence_template_visible = false;
    fence_template_ok = true;
    fence_template_key = {-1, -1, -1, -1, -1, -1};
    fence_preview_ok_texture = 0;
    fence_preview_bad_texture = 0;
    fence_template_ok_texture = 0;
    fence_template_bad_texture = 0;
    {
        auto solid_texture = [](std::uint8_t r, std::uint8_t g,
                                std::uint8_t b) {
            dino::Image image;
            image.width = 1;
            image.height = 1;
            image.rgba = {r, g, b, 255};
            return upload_texture(image);
        };
        // Retail captures: start disc blue (38,38,136) / maroon (80,38,38);
        // the pending grid is (38,38,151) / (148,38,38).
        fence_preview_ok_texture = solid_texture(38, 38, 136);
        fence_preview_bad_texture = solid_texture(80, 38, 38);
        fence_template_ok_texture = solid_texture(38, 38, 151);
        fence_template_bad_texture = solid_texture(148, 38, 38);
    }
    sky_faces = 0;
    for (int i = 0; i < 6; ++i) {
        if (sky_images[i].width <= 0) {
            continue;
        }
        skybox.faces[i] = upload_mesh(make_sky_face(i, kSkyboxHalfSize), glfns);
        skybox.textures[i] = upload_texture(sky_images[i]);
        if (skybox.faces[i].vao != 0 && skybox.textures[i] != 0) {
            ++sky_faces;
        }
    }
    skybox.ready = sky_faces == 6;

    water_texture = 0;
    water_texture2 = 0;
    if (water_image.width > 0 && terrain.world_size > 0.0f) {
        water_texture = upload_texture_wrapped(water_image, GL_REPEAT);
        if (water2_image.width > 0) {
            water_texture2 =
                upload_texture_wrapped(water2_image, GL_REPEAT);
        }
        water_gpu = upload_mesh(
            make_water_plane(terrain.world_size * 4.75f, water_z, 38.0f), glfns);
    }
    visitor_texture_id = 0;
    employee_texture_id = 0;
    if (!park_session.visitor_system.visitors.empty()) {
        visitor_texture_id = upload_texture(visitor_image);
        visitor_gpu = upload_mesh(
            park_session.visitor_system.build_mesh(park_session.build_grid, terrain, visitor_pose1,
                                      visitor_pose2),
            glfns);
    }
    return std::nullopt;
}

std::optional<int> Game::boot_park_objects() {
    sav_ready = false;
    park_session.entrance_cell_x = -1;
    park_session.entrance_cell_y = -1;
    park_session.building_center = {1e30f, 1e30f, 1e30f};

    if (!opt.building_path.empty()) {
        try {
            const dino::BuildingDef def = dino::load_bbi(opt.building_path);
            int kind = -1;
            try {
                const std::vector<std::string> names =
                    dino::load_building_list(resolve_asset(
                        opt.asset_root,
                        "Data/Design/buildings/BuildingList.ini"));
                for (std::size_t i = 0; i < names.size(); ++i) {
                    if (names[i] == def.name) {
                        kind = static_cast<int>(i);
                        break;
                    }
                }
            } catch (const std::exception&) {
            }
            place_building(def, opt.building_x, opt.building_y,
                           opt.building_state, opt.building_angle,
                           opt.building_scale, kind);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "building: %s\n", e.what());
        }
    }

    // WORLD-2: every `drsBuilding` in a campaign .wld / save park. The record
    // stores kind (BuildingList.ini index), cell_x/cell_y and a state byte;
    // the exact cell origin/Z/rotation mapping is still being reverse
    // engineered, so the transform matches the single-building path above.
    if (!opt.wld_path.empty() && terrain.world_size > 0.0f) {
        try {
            std::string lower = opt.wld_path;
            for (char& c : lower) {
                c = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }
            const bool is_save =
                lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".sav") == 0;
            const dino::WldWorld world =
                is_save ? dino::read_sav_world(opt.wld_path)
                        : dino::read_wld(opt.wld_path);
            place_world_buildings(world);
            if (!opt.sav_park_path.empty()) {
                try {
                    sav_simulation =
                        dino::read_sav_simulation(opt.sav_park_path);
                    sav_fences = world.fences;
                    sav_buildings = world.buildings;
                    sav_world_visitors = world.visitors;
                    wld_dinosaurs = world.dinosaurs;
                    sav_ready = true;
                    wld_roads.clear();
                    wld_fences.clear();
                    for (const dino::RoadRect& road : sav_simulation.roads) {
                        wld_roads.push_back(
                            {road.x0, road.y0, road.x1, road.y1});
                    }
                    for (const dino::WorldFence& fence : sav_fences) {
                        wld_fences.push_back({fence.start_x, fence.start_y,
                                              fence.end_x, fence.end_y});
                    }
                    std::printf(
                        "sav park: visitors=%zu roads=%zu budget=%d day=%d "
                        "elapsed=%.3f fences=%zu buildings=%zu\n",
                        sav_simulation.visitors.size(),
                        sav_simulation.roads.size(), sav_simulation.budget,
                        sav_simulation.day, sav_simulation.elapsed,
                        sav_fences.size(), sav_buildings.size());
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "sav park: %s\n", e.what());
                }
            }
            if (opt.wld_park) {
                wld_roads.clear();
                wld_fences.clear();
                wld_dinosaurs = world.dinosaurs;
                for (const dino::RoadRect& road : world.roads) {
                    wld_roads.push_back(
                        {road.x0, road.y0, road.x1, road.y1});
                }
                for (const dino::WorldFence& fence : world.fences) {
                    wld_fences.push_back({fence.start_x, fence.start_y,
                                          fence.end_x, fence.end_y});
                }
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "wld: %s\n", e.what());
        }
    }
    return std::nullopt;
}

std::optional<int> Game::boot_minimap_and_visitors() {
    minimap_ready = minimap.create(glfns, 256);
    minimap_scale = std::clamp(opt.minimap_scale, 0.1f, 2.0f);
    minimap_zoom_in = false;
    minimap_zoom_out = false;
    nav_pan_forward = false;
    nav_pan_back = false;
    nav_pan_left = false;
    nav_pan_right = false;
    nav_turn_left = false;
    nav_turn_right = false;
    nav_zoom_in = false;
    nav_zoom_out = false;

    if (pending_minimap_zoom != 0) {
        minimap_click_zoom(pending_minimap_zoom);
    }
    food_texture = 0;
    rebuild_extra_items();

    if (opt.wld_park || sav_ready) {
        import_park_layout();
    } else if (opt.entrance_road && park_session.entrance_road && build_ready) {
        // Opt-in: treat a --building Entrance at startup as a fresh park.
        dino::import_roads(park_session.build_grid, {*park_session.entrance_road});
        rebuild_build_meshes();
        std::printf("entrance road: %d,%d-%d,%d path_cells=%d\n",
                    park_session.entrance_road->x0, park_session.entrance_road->y0, park_session.entrance_road->x1,
                    park_session.entrance_road->y1, park_session.build_grid.path_count);
    }
    // `--visitors` is handled before the `.wld` roads are imported, so a
    // campaign park would spawn on zero path cells. Spawn once more after
    // the roads exist.
    if (opt.visitors > 0 && park_session.visitor_system.visitors.empty() &&
        park_session.build_grid.path_count > 0) {
        const int spawned =
            park_session.visitor_profiles.empty()
                ? park_session.visitor_system.spawn(park_session.build_grid, opt.visitors)
                : park_session.visitor_system.spawn(park_session.build_grid, opt.visitors,
                                       park_session.visitor_profiles, park_session.entrance_cell_x,
                                       park_session.entrance_cell_y);
        std::printf("visitors: %d spawned on %d path cells\n", spawned,
                    park_session.build_grid.path_count);
        if (opt.visitor_satisfaction >= 0) {
            for (dino::Visitor& visitor : park_session.visitor_system.visitors) {
                visitor.need0 =
                    std::clamp(opt.visitor_satisfaction, 0, 99);
            }
        }
        if (!park_session.visitor_system.visitors.empty()) {
            if (visitor_texture_id == 0 && visitor_image.width > 0) {
                visitor_texture_id = upload_texture(visitor_image);
            }
            visitor_gpu = upload_mesh(
                park_session.visitor_system.build_mesh(park_session.build_grid, terrain, visitor_pose1,
                                          visitor_pose2),
                glfns);
        }
    }
    return std::nullopt;
}

std::optional<int> Game::boot_employee_profiles() {
    try {
        park_session.employee_profiles = dino::load_employee_list(
            resolve_asset(opt.asset_root, "Data/Design/employees"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "employee profiles: %s\n", e.what());
    }
    try {
        camera_config = dino::load_camera_config(
            resolve_asset(opt.asset_root, "Data/Config/camera.ini"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "camera config: %s\n", e.what());
    }
    return std::nullopt;
}

std::optional<int> Game::boot_import_dinos() {
    pending_buy_price = 0;

    if (opt.import_dinos && !wld_dinosaurs.empty() &&
        terrain.world_size > 0.0f) {
        load_dino_family_tables();
        spawn_world_dinos();
        if (!opt.buy_dino.empty()) {
            const int buy_budget = opt.park_budget_override >= 0
                                       ? opt.park_budget_override
                                       : (sav_ready ? sav_simulation.budget
                                                    : free_game_budgets[0]);
            try {
                const dino::Market market = dino::load_market(resolve_asset(
                    opt.asset_root, "Data/Design/Scenario/Market.ini"));
                const dino::MarketEntry* entry = nullptr;
                for (const dino::MarketEntry& candidate : market.entries) {
                    if (candidate.name == opt.buy_dino) {
                        entry = &candidate;
                        break;
                    }
                }
                if (entry == nullptr) {
                    std::printf("buy: no store entry named %s\n",
                                opt.buy_dino.c_str());
                } else {
                    const int head = dino::read_gen_head_type(resolve_asset(
                        opt.asset_root,
                        "Data/Design/Scenario/Genotype/" + entry->genotype));
                    dino::DinoGenotype buy_genotype{};
                    bool have_genotype = false;
                    try {
                        buy_genotype = dino::read_gen_genotype(resolve_asset(
                            opt.asset_root,
                            "Data/Design/Scenario/Genotype/" +
                                entry->genotype));
                        have_genotype = true;
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "buy genotype: %s\n", e.what());
                    }
                    const auto match = head_species.find(head);
                    const std::string species =
                        match != head_species.end() ? match->second : "allo";
                    std::string family = family_table.family_of(species);
                    if (family.empty()) {
                        family = "LightBiped";
                    }
                    const int cost_class =
                        (have_genotype && phenotype_table.valid)
                            ? phenotype_table.size_class(buy_genotype)
                            : dino::paddock_size_class(
                                  size_for_species(species));
                    const int dino_id =
                        park_session.next_imported_dino_id;
                    int paddock_index = -1;
                    if (opt.buy_paddock >= 0 &&
                        opt.buy_paddock < static_cast<int>(park_session.paddocks.size()) &&
                        dino::paddock_can_add(
                            park_session.paddocks[static_cast<std::size_t>(
                                opt.buy_paddock)],
                            dino_id, cost_class)) {
                        paddock_index = opt.buy_paddock;
                    } else if (opt.buy_paddock < 0) {
                        for (std::size_t p = 0; p < park_session.paddocks.size(); ++p) {
                            if (dino::paddock_can_add(park_session.paddocks[p], dino_id,
                                                      cost_class)) {
                                paddock_index = static_cast<int>(p);
                                break;
                            }
                        }
                    }
                    if (paddock_index < 0) {
                        std::printf(
                            "buy: %s no paddock room "
                            "(WARNING_NO_ROOM_IN_FENCE) budget=%d\n",
                            entry->name.c_str(), buy_budget);
                        if (opt.park_hud && park.loaded()) {
                            park.show_message(dino::ui::resolve_loc_text(
                                "&WARNING_NO_ROOM_IN_FENCE", park.loc));
                        }
                    } else if (buy_budget < entry->price) {
                        std::printf(
                            "buy: %s price=%d budget=%d "
                            "(WARNING_NOT_ENOUGH_MONEY)\n",
                            entry->name.c_str(), entry->price, buy_budget);
                        if (opt.park_hud && park.loaded()) {
                            park.show_message(dino::ui::resolve_loc_text(
                                "&WARNING_NOT_ENOUGH_MONEY", park.loc));
                        }
                    } else {
                        dino::Paddock& paddock =
                            park_session.paddocks[static_cast<std::size_t>(
                                paddock_index)];
                        const dino::WorldRect& pen = paddock.rect;
                        const float x0 = static_cast<float>(
                            std::min(pen.x0, pen.x1));
                        const float x1 = static_cast<float>(
                            std::max(pen.x0, pen.x1));
                        const float y0 = static_cast<float>(
                            std::min(pen.y0, pen.y1));
                        const float y1 = static_cast<float>(
                            std::max(pen.y0, pen.y1));
                        const float radius = std::max(
                            1.0f, std::min(x1 - x0, y1 - y0) * 0.28f);
                        const float theta =
                            static_cast<float>(paddock.occupants.size()) *
                                2.39996323f +
                            static_cast<float>(paddock_index) * 0.7f;
                        const float x =
                            (x0 + x1) * 0.5f + std::cos(theta) * radius;
                        const float y =
                            (y0 + y1) * 0.5f + std::sin(theta) * radius;
                        const bool have_attributes =
                            have_genotype && phenotype_table.valid;
                        const dino::DinoAttributes buy_attrs =
                            have_attributes
                                ? phenotype_table.attributes(buy_genotype)
                                : dino::DinoAttributes{};
                        if (add_imported_dino(
                                species, family, opt.dino_clip, x, y,
                                theta * 180.0f / kPi + 90.0f,
                                have_attributes ? &buy_attrs : nullptr,
                                dino_id)) {
                            park_session.imported_dinos.back().carnivore =
                                have_genotype
                                    ? dino::genotype_is_carnivore(buy_genotype)
                                    : true;
                            dino::paddock_add(paddock, dino_id, cost_class);
                            park_session.ledger.spend_dinoachat(entry->price);
                            pending_buy_price = entry->price;
                            std::printf(
                                "buy: %s price=%d budget=%d paddock=%d "
                                "dinos=%zu\n",
                                entry->name.c_str(), entry->price,
                                buy_budget - entry->price, paddock_index,
                                park_session.imported_dinos.size());
                            rebuild_extra_items();
                        } else {
                            std::printf("buy: %s model load failed\n",
                                        entry->name.c_str());
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "buy: %s\n", e.what());
            }
        }
        print_paddock_totals();

        std::printf("park list: dinos=%zu paddocks=%zu\n",
                    park_session.imported_dinos.size(), park_paddocks.size());
    }
    return std::nullopt;
}

std::optional<int> Game::boot_food_definitions() {
    // Food sources (`--place-food`): the `.bsi` definition supplies the model,
    // texture and portion count; the position is a build-grid cell and
    // defaults to the first paddock centre.
    if (park_session.food_defs.empty() &&
        (!opt.place_food.empty() || opt.hire_nutritionists > 0 ||
         opt.feed_now)) {
        try {
            park_session.food_defs = dino::load_food_defs(
                opt.asset_root,
                resolve_asset(opt.asset_root, "Data/Design/foodsource"));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "food defs: %s\n", e.what());
        }
    }
    if (!opt.place_food.empty()) {
        try {
            park_session.food_defs = dino::load_food_defs(
                opt.asset_root,
                resolve_asset(opt.asset_root, "Data/Design/foodsource"));
            int def_index = -1;
            for (std::size_t i = 0; i < park_session.food_defs.size(); ++i) {
                if (park_session.food_defs[i].name == opt.place_food) {
                    def_index = static_cast<int>(i);
                    break;
                }
            }
            if (def_index < 0) {
                const int kind = std::atoi(opt.place_food.c_str());
                for (std::size_t i = 0; i < park_session.food_defs.size(); ++i) {
                    if (park_session.food_defs[i].kind == kind) {
                        def_index = static_cast<int>(i);
                        break;
                    }
                }
            }
            if (def_index < 0) {
                std::printf("food: unknown source %s\n",
                            opt.place_food.c_str());
            } else {
                float fx = 0.0f;
                float fy = 0.0f;
                bool have_position = false;
                int cx = 0;
                int cy = 0;
                if (std::sscanf(opt.food_pos.c_str(), "%d,%d", &cx, &cy) == 2) {
                    fx = (static_cast<float>(cx) + 0.5f) *
                         park_session.build_grid.cell_size;
                    fy = (static_cast<float>(cy) + 0.5f) *
                         park_session.build_grid.cell_size;
                    have_position = true;
                } else if (!park_session.paddocks.empty()) {
                    const dino::WorldRect& rect = park_session.paddocks.front().rect;
                    fx = (static_cast<float>(std::min(rect.x0, rect.x1)) +
                          static_cast<float>(std::max(rect.x0, rect.x1)) +
                          1.0f) *
                         0.5f;
                    fy = (static_cast<float>(std::min(rect.y0, rect.y1)) +
                          static_cast<float>(std::max(rect.y0, rect.y1)) +
                          1.0f) *
                         0.5f;
                    have_position = true;
                }
                if (!have_position) {
                    std::printf("food: no position (use --food-pos X,Y)\n");
                } else {
                    const dino::FoodDef& def =
                        park_session.food_defs[static_cast<std::size_t>(def_index)];
                    park_session.food_sources.push_back({def_index, fx, fy, def.quantity});
                    try {
                        dino::Mesh mesh = dino::load_mesh(resolve_asset(
                            opt.asset_root, def.mesh));
                        std::printf("food mesh: %s verts=%zu tris=%zu\n",
                                    def.mesh.c_str(), mesh.positions.size(),
                                    mesh.indices.size() / 3);
                        mesh = place_on_terrain(std::move(mesh), terrain, fx,
                                                fy, 0.0f);
                        food_gpu = upload_mesh(mesh, glfns);
                        if (!def.texture.empty()) {
                            food_texture = upload_texture(dino::load_tga(
                                resolve_asset(opt.asset_root, def.texture)));
                        }
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "food mesh: %s\n", e.what());
                    }
                    rebuild_extra_items();
                    std::printf(
                        "food: %s kind=%d at %.1f,%.1f portions=%.2f "
                        "texture=%s\n",
                        def.name.c_str(), def.kind, fx, fy, def.quantity,
                        def.texture.empty() ? "none" : def.texture.c_str());
                }
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "food: %s\n", e.what());
        }
    }
    if (opt.place_lunchbox) {
        try {
            const dino::BfiFeature bfi = dino::load_bfi(resolve_asset(
                opt.asset_root,
                "Data/Design/attractions/features/LunchBox.bfi"));
            float fx = 0.0f;
            float fy = 0.0f;
            bool have_position = false;
            int cx = 0;
            int cy = 0;
            if (std::sscanf(opt.lunchbox_pos.c_str(), "%d,%d", &cx, &cy) ==
                2) {
                fx = (static_cast<float>(cx) + 0.5f) *
                     park_session.build_grid.cell_size;
                fy = (static_cast<float>(cy) + 0.5f) *
                     park_session.build_grid.cell_size;
                have_position = true;
            } else if (!park_session.paddocks.empty()) {
                const dino::WorldRect& rect =
                    park_session.paddocks.front().rect;
                fx = (static_cast<float>(std::min(rect.x0, rect.x1)) +
                      static_cast<float>(std::max(rect.x0, rect.x1)) +
                      1.0f) *
                     0.5f;
                fy = (static_cast<float>(std::min(rect.y0, rect.y1)) +
                      static_cast<float>(std::max(rect.y0, rect.y1)) +
                      1.0f) *
                     0.5f;
                have_position = true;
            }
            if (!have_position) {
                std::printf("lunchbox: no position (use --lunchbox-pos)\n");
            } else {
                dino::Feeder feeder;
                feeder.name = bfi.name;
                feeder.kind = bfi.food_kind;
                feeder.quantity = static_cast<float>(bfi.food_quantity);
                feeder.max_quantity = feeder.quantity;
                feeder.nutrition = static_cast<float>(bfi.food_nutrition);
                feeder.radius =
                    std::max(1.0f, static_cast<float>(
                                       std::max(bfi.width, bfi.height)) *
                                       0.5f);
                feeder.x = fx;
                feeder.y = fy;
                park_session.feeders.push_back(feeder);
                std::string mesh_rel;
                for (const std::string& slot : bfi.mesh_slots) {
                    if (slot.ends_with(".mesh")) {
                        mesh_rel = slot;
                        break;
                    }
                }
                if (!mesh_rel.empty()) {
                    dino::Mesh mesh = dino::load_mesh(
                        resolve_asset(opt.asset_root, mesh_rel));
                    const std::string texture_name = mesh.texture_name();
                    mesh = place_on_terrain(std::move(mesh), terrain, fx,
                                            fy, 0.0f);
                    ShowFeatureDraw draw;
                    draw.gpu = upload_mesh(mesh, glfns);
                    if (!texture_name.empty()) {
                        draw.texture = upload_texture(dino::load_tga(
                            resolve_asset(opt.asset_root,
                                          bfi.texture_dir + "/" +
                                              texture_name)));
                    }
                    show_feature_draws.push_back(std::move(draw));
                }
                rebuild_extra_items();
                std::printf(
                    "lunchbox: %s kind=%d at %.1f,%.1f quantity=%.0f "
                    "nutrition=%.0f radius=%.1f\n",
                    feeder.name.c_str(), feeder.kind,
                    static_cast<double>(fx), static_cast<double>(fy),
                    static_cast<double>(feeder.quantity),
                    static_cast<double>(feeder.nutrition),
                    static_cast<double>(feeder.radius));
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "lunchbox: %s\n", e.what());
        }
    }
    return std::nullopt;
}

std::optional<int> Game::boot_camera_setup() {
    if (opt.dino_action_loop) {
        log_dino_food();
    }

    build_tool = opt.build_mode ? 1 : 0;
    pending_construct_kind = -1;
    pending_click = false;
    click_x = 0.0f;
    click_y = 0.0f;
    drag_distance = 0.0f;
    if (!opt.ground_click.empty()) {
        int gx = 0;
        int gy = 0;
        if (std::sscanf(opt.ground_click.c_str(), "%d,%d", &gx, &gy) == 2) {
            pending_click = true;
            click_x = static_cast<float>(gx);
            click_y = static_cast<float>(gy);
        }
    }

    center = place_dino
        ? dino::Vec3{spawn_x - terrain.world_size * 0.5f,
                     spawn_y - terrain.world_size * 0.5f,
                     terrain.height_at(spawn_x - terrain.world_size * 0.5f,
                                       spawn_y - terrain.world_size * 0.5f) + 3.0f}
        : bounds.center();

    auto_distance = place_dino ? 22.0f
        : std::max(2.0f, bounds.extent() * 1.7f);
    distance = opt.distance > 0.0f ? opt.distance : auto_distance;
    yaw = opt.yaw;
    pitch = opt.pitch;
    camera_progress = 0.1f;
    camera_yaw_goal = yaw;
    camera_accum = 0.0f;  // fixed-step camera tick accumulator;
    cam_mouse_x = 0;        // last client-pixel mouse position (retail;
    cam_mouse_y = 0;        // edge scroll; persists on focus loss);
    cam_mouse_valid = false;
    explicit_camera_view = !opt.camera_target.empty() || opt.distance > 0.0f ||
        opt.focus_building || opt.focus_dinos || place_dino;
    camera_retail_model = !opt.camera_orbit && (opt.camera_retail || !explicit_camera_view);
    if (opt.focus_building && park_session.building_center.x < 1e29f) {
        center = park_session.building_center;
        distance = 60.0f;
    }
    if (opt.focus_dinos && !park_session.imported_dinos.empty()) {
        const std::size_t focus_index = static_cast<std::size_t>(std::clamp(
            opt.focus_dino, 0, static_cast<int>(park_session.imported_dinos.size()) - 1));
        const ImportedDino& dinosaur = park_session.imported_dinos[focus_index];
        const float half = terrain.world_size * 0.5f;
        center = {dinosaur.x - half, dinosaur.y - half,
                  terrain.height_at(dinosaur.x - half,
                                    dinosaur.y - half) + 2.5f};
        distance = opt.distance > 0.0f ? opt.distance : 26.0f;
        yaw = opt.yaw;
        pitch = opt.pitch;
    }
    if (!opt.camera_target.empty()) {
        float tx = 0.0f;
        float ty = 0.0f;
        if (std::sscanf(opt.camera_target.c_str(), "%f,%f", &tx, &ty) == 2) {
            center = {tx, ty, terrain.height_at(tx, ty) + 2.5f};
            distance = opt.distance > 0.0f ? opt.distance : 12.0f;
            yaw = opt.yaw;
            pitch = opt.pitch;
        }
    }

    free_camera = opt.free_camera;
    free_pos = orbit_eye();
    free_yaw = 0.0f;
    free_pitch = 0.0f;
    if (free_camera) {
        enter_free_camera();
    }
    return std::nullopt;
}

std::optional<int> Game::boot_park_session() {
    park_hover_x = -1;
    park_hover_y = -1;
    park_active_index = -1;
    park_session.budget = free_game_budgets[0];
    park_session.sellback = free_game_sellback[0];
    if (sav_ready) {
        park_session.budget = sav_simulation.budget;
        park_session.clock.day = sav_simulation.day;
        park_session.clock.elapsed = sav_simulation.elapsed;
        // Camera tail: eye[0..2], target[3..5], progress[6], reserved[7..9].
        // The port's save reader retained it; apply it unless the CLI asked
        // for an explicit camera.
        if (opt.camera_target.empty() && opt.distance <= 0.0f) {
            const auto& cam = sav_simulation.camera;
            const float dx = cam[0] - cam[3];
            const float dy = cam[1] - cam[4];
            const float dz = cam[2] - cam[5];
            const float horizontal = std::sqrt(dx * dx + dy * dy);
            const float length =
                std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length > 1e-4f) {
                // The save tail stores retail record coordinates (isl01:
                // target 512,512,32). Convert to the viewer's centred world
                // before using it.
                const float half = terrain.world_size * 0.5f;
                const dino::Vec3 saved_eye{cam[0] - half, cam[1] - half,
                                           cam[2]};
                const dino::Vec3 saved_target{cam[3] - half, cam[4] - half,
                                              cam[5]};
                if (camera_retail_model) {
                    retail_camera.set_saved(saved_eye, saved_target, cam[6]);
                    center = retail_camera.target_cur;
                    yaw = retail_camera.yaw_cur * 180.0f / kPi;
                    distance = length;
                    pitch = std::asin(std::clamp(dz / length, -1.0f, 1.0f)) *
                            180.0f / kPi;
                } else {
                    center = saved_target;
                    distance = length;
                    if (horizontal > 1e-4f) {
                        yaw = std::atan2(dy, dx) * 180.0f / kPi;
                    }
                    pitch = std::asin(std::clamp(dz / length, -1.0f, 1.0f)) *
                            180.0f / kPi;
                }
                // The tail's progress is the retail orbit parameter; using it
                // preserves the saved eye exactly (radius/height are derived).
                camera_progress = std::clamp(cam[6], 0.0f, 1.0f);
                camera_yaw_goal = yaw;
                if (free_camera) {
                    enter_free_camera();
                }
                std::printf(
                    "sav camera: eye=%.2f,%.2f,%.2f target=%.2f,%.2f,%.2f "
                    "dist=%.2f yaw=%.2f pitch=%.2f\n",
                    cam[0], cam[1], cam[2], cam[3], cam[4], cam[5],
                    distance, yaw, pitch);
            }
        }
        // The budget panel reads the yesterday block (0x510430). Import it
        // from the save so a settled save shows its recorded rows; the
        // 0xBAADF00D fill from an un-settled save maps to 0.
        {
            constexpr std::uint32_t kPoison = 0xBAADF00Du;
            auto sane = [](int value) {
                return static_cast<std::uint32_t>(value) == kPoison ? 0
                                                                    : value;
            };
            const dino::SavEconomy& economy = sav_simulation.economy;
            park_session.ledger.y_food_total = sane(economy.yesterday_food[0]);
            park_session.ledger.y_food_count = sane(economy.yesterday_food[1]);
            park_session.ledger.y_staff_food_total =
                sane(economy.yesterday_staff[0]);
            park_session.ledger.y_secu_total = sane(economy.yesterday_secu[0]);
            park_session.ledger.y_secu_count = sane(economy.yesterday_secu[1]);
            park_session.ledger.y_dinoachat_count =
                sane(economy.yesterday_totals[5]);
            park_session.ledger.y_dinoachat_total =
                sane(economy.yesterday_totals[6]);
            park_session.ledger.y_construction_count =
                sane(economy.yesterday_totals[7]);
            park_session.ledger.y_construction_total =
                sane(economy.yesterday_totals[8]);
            park_session.ledger.y_laboaction_count =
                sane(economy.yesterday_totals[9]);
            park_session.ledger.y_laboaction_total =
                sane(economy.yesterday_totals[10]);
            park_session.ledger.y_ticket_total =
                sane(economy.yesterday_totals[0]);
            park_session.ledger.y_ticket_count =
                sane(economy.yesterday_totals[1]);
            park_session.ledger.y_ventes_total =
                sane(economy.yesterday_totals[12]);
            park_session.ledger.y_ventes_count =
                sane(economy.yesterday_totals[11]);
            for (std::size_t i = 1;
                 i < park_session.ledger.y_shop_revenue.size(); ++i) {
                park_session.ledger.y_shop_revenue[i] =
                    sane(economy.yesterday_revenue[i]);
                park_session.ledger.y_shop_sales[i] =
                    sane(economy.yesterday_sales[i]);
            }
        }
        if (park_session.visitor_profiles.empty()) {
            try {
                park_session.visitor_profiles = load_easy_visitor_profiles(opt.asset_root);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "visitor profiles: %s\n", e.what());
            }
        }
        if (!sav_simulation.visitors.empty()) {
            park_session.visitor_system.visitors.clear();
            for (const dino::SavVisitor& saved : sav_simulation.visitors) {
                dino::Visitor visitor;
                visitor.x = static_cast<float>(saved.x) - 512.0f;
                visitor.y = static_cast<float>(saved.y) - 512.0f;
                visitor.cell_x = saved.x / 4;
                visitor.cell_y = saved.y / 4;
                visitor.target_x = visitor.cell_x;
                visitor.target_y = visitor.cell_y;
                int global_profile = 0;
                for (const dino::WorldVisitor& wv : sav_world_visitors) {
                    if (wv.field_1c == saved.object_id) {
                        global_profile = static_cast<int>(wv.field_20);
                        break;
                    }
                }
                visitor.profile = 0;
                visitor.global_profile = global_profile;
                visitor.need0 = static_cast<int>(saved.needs[0]);
                visitor.need1 = static_cast<int>(saved.needs[1]);
                visitor.need2 = static_cast<int>(saved.needs[2]);
                visitor.state = dino::VisitorState::Choose;
                visitor.angle = std::atan2(saved.dy, saved.dx);
                visitor.rng = 0x51ce5eedu ^
                              static_cast<unsigned>(
                                  park_session.visitor_system.visitors.size() + 1);
                park_session.visitor_system.visitors.push_back(visitor);
            }
            std::printf("sav visitors: %zu restored\n",
                        park_session.visitor_system.visitors.size());
        }
        std::printf("sav park budget=%d day=%d elapsed=%.3f roads=%zu\n",
                    park_session.budget, park_session.clock.day, park_session.clock.elapsed,
                    wld_roads.size());
    }
    return std::nullopt;
}

std::optional<int> Game::boot_employee_session() {
    if (opt.park_budget_override >= 0) {
        park_session.budget = opt.park_budget_override;
    }
    if (pending_buy_price > 0) {
        park_session.budget -= pending_buy_price;
    }
    if (opt.hire_nutritionists > 0 && !park_session.employee_profiles.empty()) {
        int nutritionist = -1;
        for (std::size_t i = 0; i < park_session.employee_profiles.size(); ++i) {
            if (park_session.employee_profiles[i].kind == 0) {
                nutritionist = static_cast<int>(i);
                break;
            }
        }
        if (nutritionist < 0) {
            std::printf("employees: no nutritionist profile\n");
        } else {
            const int wage =
                park_session.employee_profiles[static_cast<std::size_t>(nutritionist)].wage;
            const float half = terrain.world_size * 0.5f;
            float spawn_x = half;
            float spawn_y = half;
            if (park_session.entrance_cell_x >= 0 && park_session.entrance_cell_y >= 0) {
                spawn_x = (static_cast<float>(park_session.entrance_cell_x) + 0.5f) *
                          park_session.build_grid.cell_size;
                spawn_y = (static_cast<float>(park_session.entrance_cell_y) + 0.5f) *
                          park_session.build_grid.cell_size;
            }
            int hired = 0;
            for (int n = 0; n < opt.hire_nutritionists; ++n) {
                if (park_session.budget < wage) {
                    std::printf("hire: not enough money wage=%d budget=%d\n",
                                wage, park_session.budget);
                    break;
                }
                park_session.budget -= wage;
                park_session.employee_system.hire(park_session.employee_profiles, nutritionist, spawn_x,
                                     spawn_y);
                ++hired;
            }
            std::printf("employees: hired=%d wage=%d budget=%d\n", hired, wage,
                        park_session.budget);
        }
    }
    if (opt.fire_nutritionists > 0) {
        const std::size_t before = park_session.employee_system.employees.size();
        const std::size_t fired = std::min<std::size_t>(
            before, static_cast<std::size_t>(opt.fire_nutritionists));
        park_session.employee_system.employees.resize(before - fired);
        std::printf("employees: fired=%zu active=%zu\n", fired,
                    park_session.employee_system.employees.size());
    }
    if (opt.feed_now) {
        const int assigned = park_session.employee_system.start_hour(
            park_session.employee_profiles, park_session.food_sources, park_session.food_defs, true);
        int completed = 0;
        for (int step = 0; step < 20; ++step) {
            completed += park_session.employee_system.update(
                10.0f, park_session.employee_profiles, park_session.food_sources, park_session.food_defs);
        }
        std::printf(
            "feed: assigned=%d completed=%d serviced=%d sources=%zu\n",
            assigned, completed, park_session.employee_system.serviced, park_session.food_sources.size());
        for (const dino::FoodSource& source : park_session.food_sources) {
            const float capacity =
                source.def >= 0 &&
                        source.def < static_cast<int>(park_session.food_defs.size())
                    ? park_session.food_defs[static_cast<std::size_t>(source.def)].quantity
                    : 0.0f;
            std::printf("feed: source def=%d remaining=%.3f capacity=%.3f\n",
                        source.def, static_cast<double>(source.remaining),
                        static_cast<double>(capacity));
        }
    }

    // Load the Nutritionist standing pose + texture once; the hourly walk is
    // a straight move, so one pose is enough for the first slice.
    if (!park_session.employee_profiles.empty() && employee_pose.positions.empty()) {
        int nutritionist = -1;
        for (std::size_t i = 0; i < park_session.employee_profiles.size(); ++i) {
            if (park_session.employee_profiles[i].kind == 0) {
                nutritionist = static_cast<int>(i);
                break;
            }
        }
        if (nutritionist >= 0) {
            const dino::EmployeeProfile& profile =
                park_session.employee_profiles[static_cast<std::size_t>(nutritionist)];
            try {
                if (!profile.meshes.empty()) {
                    employee_pose = dino::load_mesh(
                        resolve_asset(opt.asset_root, profile.meshes.front()));
                }
                const auto ends_with_tga = [](const std::string& text) {
                    if (text.size() < 4) {
                        return false;
                    }
                    std::string tail = text.substr(text.size() - 4);
                    for (char& c : tail) {
                        c = static_cast<char>(
                            std::tolower(static_cast<unsigned char>(c)));
                    }
                    return tail == ".tga";
                };
                for (const std::string& asset : profile.assets) {
                    if (ends_with_tga(asset)) {
                        employee_image = dino::load_image(
                            resolve_asset(opt.asset_root, asset));
                        break;
                    }
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "employee assets: %s\n", e.what());
            }
        }
    }
    if (employee_gpu.vao == 0 && !employee_pose.positions.empty() &&
        !park_session.employee_system.employees.empty()) {
        if (employee_image.width > 0) {
            employee_texture_id = upload_texture(employee_image);
        }
        employee_gpu = upload_mesh(
            park_session.employee_system.build_mesh(terrain, employee_pose), glfns);
        std::printf("employees: rendered=%zu pose_vertices=%zu\n",
                    park_session.employee_system.employees.size(),
                    employee_pose.positions.size());
        rebuild_extra_items();
    }
    // `--sell-dino` runs after the budget override and the pending buy (so
    // the park-session budget reset cannot wipe the sale credit). The last
    // match is sold so earlier import indices do not shift.
    if (!opt.sell_dino.empty() && !park_session.imported_dinos.empty()) {
        const auto matches = [](const std::string& text,
                                const std::string& want) {
            if (want.size() > text.size()) {
                return false;
            }
            for (std::size_t i = 0; i < want.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(text[i])) !=
                    std::tolower(static_cast<unsigned char>(want[i]))) {
                    return false;
                }
            }
            return true;
        };
        for (int i = static_cast<int>(park_session.imported_dinos.size()) -
                     1;
             i >= 0; --i) {
            if (matches(park_session.imported_dinos[
                            static_cast<std::size_t>(i)]
                            .species,
                        opt.sell_dino)) {
                sell_dino(i);
                break;
            }
        }
    }
    return std::nullopt;
}

std::optional<int> Game::boot_build_tools() {
    road_price = 0;
    road_price_loaded = false;
    road_has_start = false;
    road_start_x = 0;
    road_start_y = 0;
    road_preview_key = {-1, -1, -1, -1};
    road_preview_cost = -1;

    selected_construct = !opt.construct_name.empty() ? opt.construct_name
                                    : pending_construct_widget;
    if (selected_construct == "CONSTRUC_ROAD_BUTTON") {
        arm_road_tool();
        for (const std::string& value : opt.road_clicks) {
            int x = 0;
            int y = 0;
            if (std::sscanf(value.c_str(), "%d,%d", &x, &y) == 2) {
                road_click(x, y);
            }
        }
    } else if (!selected_construct.empty()) {
        const int kind = construction_kind_for_name(selected_construct);
        if (kind < 0) {
            std::printf("construction: no building named %s\n",
                        selected_construct.c_str());
        } else {
            build_tool = 3;
            pending_construct_kind = kind;
            std::printf("construction: %s selected, cost=%u\n",
                        construction_defs[static_cast<std::size_t>(kind)]
                            .name.c_str(),
                        construction_defs[static_cast<std::size_t>(kind)]
                            .construction_cost);
        }
    }
    if (!selected_construct.empty() && selected_construct != "CONSTRUC_ROAD_BUTTON" &&
        !opt.construct_positions.empty()) {
        const int kind = construction_kind_for_name(selected_construct);
        for (const std::string& position : opt.construct_positions) {
            int x = 0;
            int y = 0;
            if (std::sscanf(position.c_str(), "%d,%d", &x, &y) == 2) {
                try_construct(kind, x, y, 0);
            }
        }
    }
    if (pending_store_action.rfind("store buy ", 0) == 0) {
        handle_store_buy(std::atoi(pending_store_action.c_str() + 10));
        pending_store_action.clear();
    }
    if (opt.settle_day) {
        run_daily_settlement();
    }
    if (opt.budget_info) {
        std::printf(
            "budget ledger: dinoachat=%d/%d construction=%d/%d tickets=%d/%d "
            "food=%d/%d secu=%d/%d labo=%d/%d ventes=%d/%d\n",
            park_session.ledger.dinoachat_count, park_session.ledger.dinoachat_total,
            park_session.ledger.construction_count,
            park_session.ledger.construction_total, park_session.ledger.ticket_count,
            park_session.ledger.ticket_total, park_session.ledger.food_count,
            park_session.ledger.food_total, park_session.ledger.secu_count,
            park_session.ledger.secu_total, park_session.ledger.laboaction_count,
            park_session.ledger.laboaction_total, park_session.ledger.ventes_count,
            park_session.ledger.ventes_total);
        std::printf(
            "budget yesterday: dinoachat=%d/%d construction=%d/%d "
            "tickets=%d/%d\n",
            park_session.ledger.y_dinoachat_count, park_session.ledger.y_dinoachat_total,
            park_session.ledger.y_construction_count,
            park_session.ledger.y_construction_total, park_session.ledger.y_ticket_count,
            park_session.ledger.y_ticket_total);
        std::printf(
            "budget totals: expenses=%d income=%d net=%d\n",
            park_session.ledger.expenses(), park_session.ledger.income(),
            park_session.ledger.net());
    }

    park_layout_w = 800;
    park_layout_h = 600;
    minimap_focus = {1e30f, 1e30f, 1e30f};
    minimap_heading = 1e30f;
    minimap_rendered_scale = -1.0f;
    if (!opt.lnd_path.empty()) {
        current_island =
            std::filesystem::path(opt.lnd_path).stem().string();
    } else if (!island_names.empty()) {
        current_island = island_names.front();
    }
    if (pending_show_action.rfind("show place ", 0) == 0) {
        handle_show_place(std::atoi(pending_show_action.c_str() + 11));
        pending_show_action.clear();
    }
    if (pending_show_index >= 0 && !opt.show_place_positions.empty()) {
        for (const std::string& position : opt.show_place_positions) {
            int x = 0;
            int y = 0;
            if (std::sscanf(position.c_str(), "%d,%d", &x, &y) == 2) {
                place_show_at(x, y);
            }
        }
    }
    for (const std::string& action : pending_attraction_actions) {
        handle_attraction_action(action);
    }
    pending_attraction_actions.clear();
    return std::nullopt;
}

std::optional<int> Game::boot_finalize() {
    if (sav_ready) {
        dino::PortState state;
        state.island = current_island;
        state.budget = sav_simulation.budget;
        state.day = sav_simulation.day;
        state.elapsed = sav_simulation.elapsed;
        state.time_scale = park_session.clock.time_scale;
        state.target = center;
        state.yaw = yaw;
        state.pitch = pitch;
        state.distance = distance;
        state.free_camera = free_camera;
        state.free_pos = free_pos;
        state.free_yaw = free_yaw;
        state.free_pitch = free_pitch;

        dino::BuildGrid import_grid;
        import_grid.init(4.0f, terrain.world_size);
        std::vector<dino::WorldRect> roads;
        roads.reserve(sav_simulation.roads.size());
        for (const dino::RoadRect& road : sav_simulation.roads) {
            roads.push_back({road.x0, road.y0, road.x1, road.y1});
        }
        dino::import_roads(import_grid, roads);
        std::vector<dino::WorldRect> fences;
        fences.reserve(sav_fences.size());
        for (const dino::WorldFence& fence : sav_fences) {
            fences.push_back({fence.start_x, fence.start_y, fence.end_x,
                              fence.end_y});
        }
        dino::import_fences(import_grid, fences);

        state.cell_size = import_grid.cell_size;
        state.origin_x = import_grid.origin_x;
        state.origin_y = import_grid.origin_y;
        state.width = import_grid.width;
        state.height = import_grid.height;
        for (const dino::WorldRect& road : import_grid.roads) {
            state.roads.push_back({road.x0, road.y0, road.x1, road.y1});
        }
        for (int y = 0; y < import_grid.height; ++y) {
            for (int x = 0; x < import_grid.width; ++x) {
                if (import_grid.path_at(x, y)) {
                    state.paths.emplace_back(x, y);
                }
                for (int edge = 0; edge < 2; ++edge) {
                    if (import_grid.fence_at(x, y, edge)) {
                        state.fences.push_back({x, y, edge});
                    }
                }
            }
        }
        for (const dino::WorldBuilding& building : sav_buildings) {
            state.buildings.push_back(
                {static_cast<int>(building.kind),
                 static_cast<int>(building.cell_x),
                 static_cast<int>(building.cell_y),
                 static_cast<int>(building.state)});
        }
        const float half = terrain.world_size * 0.5f;
        for (const dino::SavVisitor& saved : sav_simulation.visitors) {
            dino::PortVisitorState visitor;
            visitor.x = static_cast<float>(saved.x) - half;
            visitor.y = static_cast<float>(saved.y) - half;
            int cx = 0;
            int cy = 0;
            if (!import_grid.cells_for_world(visitor.x, visitor.y, cx, cy)) {
                cx = 0;
                cy = 0;
            }
            visitor.cell_x = cx;
            visitor.cell_y = cy;
            visitor.target_x = cx;
            visitor.target_y = cy;
            visitor.prev_x = -1;
            visitor.prev_y = -1;
            visitor.angle = std::atan2(saved.dy, saved.dx);
            visitor.phase = 0.0f;
            state.visitors.push_back(visitor);
        }
        apply_port_state(state);
        for (std::size_t i = 0;
             i < sav_simulation.visitors.size() &&
             i < park_session.visitor_system.visitors.size();
             ++i) {
            const auto& needs = sav_simulation.visitors[i].needs;
            park_session.visitor_system.visitors[i].need0 = static_cast<int>(needs[0]);
            park_session.visitor_system.visitors[i].need1 = static_cast<int>(needs[1]);
            park_session.visitor_system.visitors[i].need2 = static_cast<int>(needs[2]);
        }
        std::printf(
            "sav import: budget=%d day=%d elapsed=%.3f visitors=%zu "
            "paths=%d fences=%d buildings=%zu\n",
            park_session.budget, park_session.clock.day, park_session.clock.elapsed,
            park_session.visitor_system.visitors.size(), park_session.build_grid.path_count,
            park_session.build_grid.fence_count, park_session.building_instances.size());
    }

    if (!opt.load_port_path.empty()) {
        do_port_load(opt.load_port_path);
    }
    if (!opt.save_port_path.empty()) {
        do_port_save(opt.save_port_path);
    }
    if (pending_port_load) {
        do_port_load(port_save_target(true));
        park.overlay.set_visible("OPTIONS_PANEL", false);
        pending_port_load = false;
    }
    if (pending_port_save) {
        do_port_save(port_save_target(false));
        park.overlay.set_visible("OPTIONS_PANEL", false);
        pending_port_save = false;
    }
    if (!opt.park_hover.empty() && park.loaded()) {
        int x = 0, y = 0;
        if (std::sscanf(opt.park_hover.c_str(), "%d,%d", &x, &y) == 2) {
            park_hover_x = x;
            park_hover_y = y;
        }
    }

    running = true;
    start_screen = opt.force_menu ||
        (opt.frames == 0 && opt.screenshot.empty() && !opt.no_menu);
    if (menu_loaded && opt.use_menu && !menu.active) {
        start_screen = false;
    }
    if (park_return_to_menu) {
        activate_main_menu();
    }
    // A pre-loop `--menu-click` on LOAD_LOAD_BUTTON only queued the path:
    // load it now that do_port_load and the menu state exist.
    if (!pending_menu_load_path.empty()) {
        do_port_load(pending_menu_load_path);
        pending_menu_load_path.clear();
        menu.loading = false;
        menu.active = false;
        start_screen = false;
    }
    show_help = opt.force_help;
    dragging = false;
    ui_click_consumed = false;
    last_mx = 0.0f;
    last_my = 0.0f;
    frame = 0;
    freq = SDL_GetPerformanceFrequency();
    last = SDL_GetPerformanceCounter();
    elapsed = 0.0;
    width = opt.width;
    height = opt.height;
    logical_width = opt.width;
    logical_height = opt.height;
    if (park.loaded()) {
        park.overlay.layout_for_size(logical_width, logical_height);
        park_layout_w = logical_width;
        park_layout_h = logical_height;
    }

    menu_hover_forced = !opt.menu_hover.empty();
    park_hover_forced = !opt.park_hover.empty();
    return std::nullopt;
}

}  // namespace dino::app
