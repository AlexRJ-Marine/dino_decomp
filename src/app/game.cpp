// The port's application object: the process shell.
//
// This file holds the constructor, the frame loop and its phases. The
// session state lives in game.h; the phase bodies live in the game_*.cpp
// files next to this one, one domain per file.

#include "game.h"

namespace dino::app {

Game::Game(int argc, char** argv) : opt(parse_args(argc, argv)) {}

Game::~Game() = default;

int Game::run() {
    if (std::optional<int> exit_code = boot()) {
        return *exit_code;
    }

    while (running) {
        handle_input();
        if (!running) {
            break;
        }
        begin_frame();
        if (opt.camera_selftest && terrain.world_size > 0.0f) {
            return camera_selftest(camera_config, terrain);
        }
        if (width <= 0 || height <= 0) {
            SDL_Delay(10);
            continue;
        }
        advance_time();
        update_session();
        update_audio();
        update_camera();
        update_diagnostics();
        set_background();
        update_simulation();
        update_camera_view();
        update_placement();
        update_preview();
        draw_frame();
        present_frame();
    }

    shutdown();
    return 0;
}

// Release every GL/SDL resource and write the end-of-run output (config,
// build save, visitor stats).
void Game::shutdown() {
    if (opt.save_config) {
        int final_width = 0, final_height = 0;
        SDL_GetWindowSize(window, &final_width, &final_height);
        if (!menu.screen_mode_changed && !opt.fullscreen &&
            final_width >= 64 && final_height >= 64) {
            opt.width = final_width;
            opt.height = final_height;
        }
        save_config(opt.config_path, opt);
    }

    if (build_ready && !opt.build_save_path.empty()) {
        try {
            dino::save_build(build_grid, opt.build_save_path);
            std::printf("build saved: %s (%d paths, %d fences)\n",
                        opt.build_save_path.c_str(), build_grid.path_count,
                        build_grid.fence_count);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "build save: %s\n", e.what());
        }
    }

    if (audio != nullptr) {
        SDL_DestroyAudioStream(audio);
    }
    ui_sound.destroy();
    music.destroy();
    for (auto& group : veg_gpu) {
        group.destroy(glfns);
    }
    dino_gpu.destroy(glfns);
    shadow_gpu.destroy(glfns);
    clear_imported_dinos();
    for (auto& entry : imported_species) {
        if (entry.second->texture != 0) {
            glDeleteTextures(1, &entry.second->texture);
            entry.second->texture = 0;
        }
    }
    imported_species.clear();
    for (BuildingDraw& building : building_draws) {
        building.gpu.destroy(glfns);
        if (building.texture != 0) {
            glDeleteTextures(1, &building.texture);
        }
    }
    path_gpu.destroy(glfns);
    road_preview_gpu.destroy(glfns);
    fence_gpu.destroy(glfns);
    water_gpu.destroy(glfns);
    visitor_gpu.destroy(glfns);
    employee_gpu.destroy(glfns);
    if (employee_texture_id != 0) {
        glDeleteTextures(1, &employee_texture_id);
        employee_texture_id = 0;
    }
    hud_gpu.destroy(glfns);
    menu_gpu.destroy(glfns);
    for (auto& entry : ui_textures) {
        if (entry.second.id != 0) {
            glDeleteTextures(1, &entry.second.id);
        }
    }
    for (auto& entry : ui_font_atlases) {
        if (entry.second != 0) {
            glDeleteTextures(1, &entry.second);
        }
    }
    if (hud_font_texture != 0) {
        glDeleteTextures(1, &hud_font_texture);
    }
    if (visitor_texture_id != 0) {
        glDeleteTextures(1, &visitor_texture_id);
    }
    for (int i = 0; i < 6; ++i) {
        skybox.faces[i].destroy(glfns);
        if (skybox.textures[i] != 0) {
            glDeleteTextures(1, &skybox.textures[i]);
        }
    }
    if (water_texture != 0) {
        glDeleteTextures(1, &water_texture);
    }
    if (water_texture2 != 0) {
        glDeleteTextures(1, &water_texture2);
    }
    if (path_texture != 0) {
        glDeleteTextures(1, &path_texture);
    }
    if (fence_texture != 0) {
        glDeleteTextures(1, &fence_texture);
    }
    for (const GLuint id : veg_textures) {
        if (id != 0) {
            glDeleteTextures(1, &id);
        }
    }
    if (dino_texture != 0) {
        glDeleteTextures(1, &dino_texture);
    }
    gpu.destroy(glfns);
    if (texture != 0) {
        glDeleteTextures(1, &texture);
    }
    glfns.DeleteProgram(program);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (opt.visitor_stats && !visitor_system.visitors.empty()) {
        int states[4] = {0, 0, 0, 0};
        double needs[3] = {0.0, 0.0, 0.0};
        for (const dino::Visitor& visitor : visitor_system.visitors) {
            const int state = static_cast<int>(visitor.state);
            if (state >= 0 && state < 4) {
                ++states[state];
            }
            needs[0] += visitor.need0;
            needs[1] += visitor.need1;
            needs[2] += visitor.need2;
        }
        const double count =
            static_cast<double>(visitor_system.visitors.size());
        std::printf(
            "visitor stats: count=%zu states=%d/%d/%d/%d "
            "needs=%.1f/%.1f/%.1f\n",
            visitor_system.visitors.size(), states[0], states[1], states[2],
            states[3], needs[0] / count, needs[1] / count, needs[2] / count);
    }
}

void Game::begin_frame() {
    SDL_GetWindowSizeInPixels(window, &width, &height);
    sync_park_layout();
    if (build_ready && opt.select_paddock >= 0 &&
        opt.select_paddock < static_cast<int>(paddocks.size()) &&
        selected_paddock != opt.select_paddock) {
        selected_paddock = opt.select_paddock;
        rebuild_build_meshes();
        refresh_fence_info_panel();
    }
    if (pending_sell_mode) {
        pending_sell_mode = false;
        if (park.loaded()) {
            park_sell_mode = true;
            park_sell_paddock = -1;
            build_tool = 0;
            fence_corner_x = -1;
            fence_corner_y = -1;
            pending_construct_kind = -1;
            pending_show_index = -1;
            pending_store_index = -1;
            road_has_start = false;
            selected_paddock = -1;
            park.overlay.set_visible(
                "FENCE_CONSTRUCTION_AND_INFO_PANEL", false);
            park.overlay.set_visible("CONSTRUCTION_PANEL", false);
            park.overlay.set_visible("CONSTRUCTION_BUILDINGS_PANEL",
                                     false);
            park.overlay.set_visible("CONSTRUCTION_ROAD_METRO_PANEL",
                                     false);
            park.overlay.set_visible("PREDEFINED_SHOW_PANEL", false);
            rebuild_build_meshes();
            std::printf("sell: mode armed\n");
        }
    }
}

void Game::advance_time() {
    const std::uint64_t now = SDL_GetPerformanceCounter();
    frame_dt = static_cast<double>(now - last) / static_cast<double>(freq);
    last = now;
    elapsed += frame_dt;

    // Retail palette time: starts at 0 when the park scene is created and
    // accumulates the park frame dt (FUN_00600550).
    if (park.loaded() && !park_pulse_started) {
        park_pulse_time = 0.0;
        park_pulse_started = true;
    } else if (!park.loaded()) {
        park_pulse_started = false;
        park_pulse_time = 0.0;
    }
    if (opt.pulse_time >= 0.0f) {
        park_pulse_time = opt.pulse_time;
    } else {
        park_pulse_time += frame_dt;
    }
    {
        float lamp_raw[3] = {1.0f, 1.0f, 1.0f};
        dino::fence_pulse_colors(park_pulse_time, lamp_raw,
                                 selected_tint.data());
        // State 7 lights the material (Light Y, AmbientMatSource
        // Material): the mesh header's white ambient dominates the blue
        // diffuse, which is why the retail lamps read near-white with a
        // subtle blue pulse. Scene ambient from dino_additif's lit path.
        lamp_tint[0] = 0.62f + lamp_raw[0];
        lamp_tint[1] = 0.64f + lamp_raw[1];
        lamp_tint[2] = 0.68f + lamp_raw[2];
    }
}

void Game::update_session() {
    if (menu_loaded && opt.use_menu && menu.loading) {
        menu.loading_timer -= static_cast<float>(frame_dt);
        if (menu.loading_timer <= 0.0f) {
            menu.loading = false;
            menu.active = false;
            start_screen = false;
            const bool campaign_start = menu.start_campaign;
            menu.start_campaign = false;
            const dino::Campaign* campaign =
                campaign_start ? menu.selected_campaign() : nullptr;
            const dino::CampaignMission* mission =
                campaign_start ? menu.selected_mission() : nullptr;
            if (campaign != nullptr && mission != nullptr &&
                menu.campaign_unlocked(menu.campaign)) {
                try {
                    std::string island_name = campaign->island;
                    std::transform(
                        island_name.begin(), island_name.end(),
                        island_name.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                    load_island(island_name, true);
                    current_island = island_name;
                    minimap_rendered_scale = -1.0f;
                    const std::string scenario_root = resolve_asset(
                        opt.asset_root, "Data/Design/Scenario");
                    const std::string world_path =
                        (std::filesystem::path(scenario_root) /
                         campaign->name / campaign->world_file)
                            .string();
                    load_campaign_park(world_path);
                    int budget = campaign->budget;
                    const auto delta =
                        mission->values.find("CONTEXT.BUDGET");
                    if (delta != mission->values.end()) {
                        budget += std::atoi(delta->second.c_str());
                    }
                    park_budget = std::max(0, budget);
                    park_sellback = static_cast<float>(campaign->sell_back);
                    std::printf(
                        "start: campaign=%s mission=%s island=%s "
                        "budget=%d sellback=%.2f\n",
                        campaign->name.c_str(), mission->name.c_str(),
                        campaign->island.c_str(), park_budget,
                        static_cast<double>(park_sellback));
                    start_campaign_mission(*mission);
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "campaign start: %s\n", e.what());
                }
            } else if (!campaign_start && !island_names.empty()) {
                const int island_index =
                    std::clamp(menu.island, 0,
                               static_cast<int>(island_names.size()) - 1);
                const int difficulty = std::clamp(menu.difficulty, 0, 2);
                try {
                    load_island(island_names[static_cast<std::size_t>(island_index)],
                                true);
                    current_island =
                        island_names[static_cast<std::size_t>(island_index)];
                    minimap_rendered_scale = -1.0f;
                    if (entrance_road) {
                        ensure_build_ready();
                        dino::import_roads(build_grid, {*entrance_road});
                        rebuild_build_meshes();
                        std::printf(
                            "entrance road: %d,%d-%d,%d path_cells=%d\n",
                            entrance_road->x0, entrance_road->y0,
                            entrance_road->x1, entrance_road->y1,
                            build_grid.path_count);
                    }
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "island load: %s\n", e.what());
                }
                park_budget = free_game_budgets[difficulty];
                park_sellback = free_game_sellback[difficulty];
                // Free game lists its own [DINOSTORE] subset (20 entries)
                // in FreeGame.ini; campaign missions override this with
                // their mission block in start_campaign_mission.
                try {
                    apply_store_catalog(
                        dino::load_market(resolve_asset(
                            opt.asset_root,
                            "Data/Design/Scenario/FreeGame.ini")),
                        "FreeGame.ini");
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "store catalog: %s\n", e.what());
                }
                std::printf(
                    "start: island=%s difficulty=%d budget=%d\n",
                    island_names[static_cast<std::size_t>(island_index)].c_str(),
                    difficulty, park_budget);
            }
        }
    }

    if (campaign_runtime && campaign_runtime->active()) {
        apply_show_specs();
    }
    if (park.loaded() && !start_screen) {
        park.update(frame_dt);
        park_clock.advance(frame_dt);
        if (campaign_runtime && campaign_runtime->active()) {
            const int hour_index = static_cast<int>(
                park_clock.elapsed * park_clock.hours_per_second());
            show_runtime.tick(
                hour_index, park_clock.day, park_clock.end_of_day, frame_dt,
                park_clock.time_scale, [&](dino::ParkShow& show) {
                    fill_show_scores(show.record, show.participants);
                    std::printf(
                        "show: %s finished round %d/%d at %02d:%02d\n",
                        show.entry.stem.c_str(), show.done,
                        show.iterations, park_clock.hour(),
                        park_clock.minute());
                });
        }
        if (opt.show_status && !show_status_printed) {
            show_status_printed = true;
            for (int slot = 1; slot <= 10; ++slot) {
                std::string text;
                int iterations = 0;
                int locked = 0;
                attraction_row_text(slot, text, iterations, locked);
                std::printf(
                    "attraction list: slot=%d name=%s iterations=%d "
                    "locked=%d\n",
                    slot, text.c_str(), iterations, locked);
            }
        }
        if (park_clock.new_hour && !employee_system.employees.empty()) {
            const int assigned = employee_system.start_hour(
                employee_profiles, food_sources, food_defs);
            if (assigned > 0) {
                std::printf("feed: hour=%d assigned=%d\n",
                            park_clock.hour(), assigned);
            }
        }
        employee_system.update(frame_dt, employee_profiles, food_sources,
                               food_defs);
        if (employee_gpu.vao != 0 && !employee_pose.positions.empty()) {
            update_mesh(employee_gpu,
                        employee_system.build_mesh(terrain, employee_pose),
                        glfns);
        }
        if (park_clock.end_of_day) {
            run_daily_settlement();
            park_clock.next_day();
        }
    }
    if (campaign_runtime && campaign_runtime->active()) {
        dino::CampaignState mission_state;
        mission_state.treasury = park_budget;
        for (std::size_t i = 1; i < budget_ledger.shop_revenue.size(); ++i) {
            mission_state.merchandise_income +=
                budget_ledger.shop_revenue[i];
        }
        // The port has no death counter; deaths stay zero (dyingdinos
        // passes). Hired staff feed the `employees` objective.
        mission_state.deaths = 0;
        mission_state.camera_altitude = static_cast<int>(
            (free_camera ? free_pos : orbit_eye()).z);
        mission_state.clock_minutes = park_clock.minutes();
        mission_state.store_empty = store_catalog.empty();
        mission_state.bankrupt = park_budget < 0;
        // VisitorList.ini personality order 0..9; mission objective
        // profile strings use these names.
        static const char* const kProfileNames[10] = {
            "surfer",  "holligan", "parent", "blase",    "retraite",
            "riche",   "scientific", "amoureux", "spd", "enfant"};
        for (const dino::Visitor& visitor : visitor_system.visitors) {
            dino::CampaignVisitorState state_visitor;
            const int profile = visitor.global_profile;
            state_visitor.profile =
                profile >= 0 && profile < 10
                    ? kProfileNames[profile]
                    : std::to_string(visitor.profile);
            state_visitor.satisfaction = visitor.need0;
            mission_state.visitors.push_back(state_visitor);
        }
        // Hired staff are the live `employees` source. The objective
        // profile strings match the `.bei` role names
        // (Nutritionist/Guard); "all" matches any role.
        for (const dino::Employee& employee : employee_system.employees) {
            if (employee.profile >= 0 &&
                employee.profile <
                    static_cast<int>(employee_profiles.size())) {
                mission_state.employee_roles.push_back(
                    employee_profiles[static_cast<std::size_t>(
                                          employee.profile)]
                        .name);
            }
        }
        // Connected-buildings objective: a building is connected when a
        // path cell touches its footprint perimeter. `+0x34` is the retail
        // flag; the port derives it from the live build grid.
        for (const dino::PortBuildingState& building :
             building_instances) {
            const auto def_it = building_defs.find(building.kind);
            if (def_it == building_defs.end()) {
                continue;
            }
            const dino::BuildingDef& def = def_it->second;
            const bool rotated = building.state % 2 != 0;
            const int width = static_cast<int>(
                rotated ? def.footprint_h : def.footprint_w);
            const int height = static_cast<int>(
                rotated ? def.footprint_w : def.footprint_h);
            // `.wld` roads/fences and building cells are uncentered world
            // units; `import_roads` maps them with world/cell_size, so the
            // connectivity test uses the same convention.
            const float cell = build_grid.cell_size > 0.0f
                                   ? build_grid.cell_size
                                   : 4.0f;
            const int gx0 = static_cast<int>(
                std::floor(static_cast<float>(building.cell_x) / cell));
            const int gy0 = static_cast<int>(
                std::floor(static_cast<float>(building.cell_y) / cell));
            const int gx1 = static_cast<int>(std::floor(
                static_cast<float>(building.cell_x + width - 1) / cell));
            const int gy1 = static_cast<int>(std::floor(
                static_cast<float>(building.cell_y + height - 1) / cell));
            bool connected = false;
            for (int y = gy0; y <= gy1 && !connected; ++y) {
                connected = build_grid.path_at(gx0 - 1, y) ||
                            build_grid.path_at(gx1 + 1, y);
            }
            for (int x = gx0; x <= gx1 && !connected; ++x) {
                connected = build_grid.path_at(x, gy0 - 1) ||
                            build_grid.path_at(x, gy1 + 1);
            }
            if (std::getenv("DINO_DEBUG_BUILDINGS") != nullptr) {
                std::fprintf(
                    stderr,
                    "building kind=%d cat=%d cell=%d,%d wh=%dx%d "
                    "grid=%d,%d..%d,%d connected=%d\n",
                    building.kind, static_cast<int>(def.category),
                    building.cell_x, building.cell_y, width, height, gx0,
                    gy0, gx1, gy1, connected ? 1 : 0);
            }
            mission_state.buildings.push_back(
                {static_cast<int>(def.category), connected});
        }
        mission_state.fence_count = static_cast<int>(paddocks.size());
        // Show arenas: `--show NAME[:I,J]` is the immediate-score test
        // hook; `--place-show` feeds the scheduled `ShowRuntime` whose
        // per-performance scores arrive on the hour. Both use the retail
        // judge pipeline in `fill_show_scores`.
        for (const PortShowSpec& spec : port_shows) {
            dino::CampaignShowState show;
            show.type = spec.type;
            fill_show_scores(show, spec.participants);
            if (std::getenv("DINO_DEBUG_SHOWS") != nullptr) {
                std::fprintf(stderr,
                             "show %s type=%d participants=%d profiles=%zu\n",
                             spec.name.c_str(), show.type,
                             show.participants,
                             show.profile_scores.size());
                for (const auto& entry : show.profile_scores) {
                    std::fprintf(stderr, "  profile=%d score=%.2f\n",
                                 entry.personality, entry.score);
                }
            }
            mission_state.shows.push_back(std::move(show));
        }
        for (const dino::CampaignShowState& show :
             show_runtime.campaign_states()) {
            mission_state.shows.push_back(show);
        }
        // Live trait snapshot. The objective slots are actor fields
        // (+0x90/+0xbc/+0x1e4/+0x1e8/+0x244/+0x98/int(+0x7c)/+0xc0/
        // +0x1e0/+0x8c); the port's phenotype exposes the related
        // beauty/aggression/stamina/agility/intelligence and a body
        // length. Hybrid/scarcity/strangeness/ornaments are not ported
        // yet, so those verbs stay pending.
        for (const ImportedDino& dinosaur : imported_dinos) {
            dino::DinoTraitState trait;
            trait.species = dinosaur.species;
            trait.cuteness = dinosaur.attributes.beauty;
            trait.aggression = dinosaur.attributes.aggression;
            const dino::DinoNeeds& needs =
                dinosaur.action_loop ? dinosaur.action_loop->needs()
                                     : dinosaur.needs;
            trait.food = needs.value[0];
            // Actor +0x98 is agility; +0x1e0 is the phenotype size class
            // (0..3), not the body length.
            trait.agility = dinosaur.attributes.agility;
            trait.size_class = dinosaur.attributes.size_class;
            trait.strangeness =
                dinosaur.attributes.conventional_appeal;
            trait.ornament_count =
                dinosaur.attributes.ornament_count;
            trait.carnivore = dinosaur.carnivore;
            trait.biped = !dinosaur.attributes.quadruped;
            trait.age = 0;  // imported dinos are spawned at park start
            trait.speed = static_cast<int>(
                dinosaur.attributes.walk_speed * 100.0f);
            trait.stamina = dinosaur.attributes.stamina;
            trait.intelligence = dinosaur.attributes.intelligence;
            trait.beauty = dinosaur.attributes.beauty;
            // `+0x1e4` is hybrid/strangeness. The store's ORIGINALITY /
            // Strangeness gauge is `100 - conventional_appeal`; use that
            // phenotype value until the actor writer is traced.
            trait.hybrid = dinosaur.attributes.originality;
            // Isolation: the dino is inside a paddock rectangle and no
            // other imported dino shares that rectangle.
            for (const dino::Paddock& paddock : paddocks) {
                const float x0 =
                    static_cast<float>(std::min(paddock.rect.x0,
                                                paddock.rect.x1));
                const float x1 =
                    static_cast<float>(std::max(paddock.rect.x0,
                                                paddock.rect.x1));
                const float y0 =
                    static_cast<float>(std::min(paddock.rect.y0,
                                                paddock.rect.y1));
                const float y1 =
                    static_cast<float>(std::max(paddock.rect.y0,
                                                paddock.rect.y1));
                if (dinosaur.x < x0 || dinosaur.x > x1 ||
                    dinosaur.y < y0 || dinosaur.y > y1) {
                    continue;
                }
                int occupants = 0;
                for (const ImportedDino& other : imported_dinos) {
                    if (other.x >= x0 && other.x <= x1 && other.y >= y0 &&
                        other.y <= y1) {
                        ++occupants;
                    }
                }
                trait.isolated = occupants <= 1;
                break;
            }
            mission_state.dinos.push_back(trait);
        }
        campaign_runtime->tick(frame_dt, mission_state);
        const auto& verdicts = campaign_runtime->verdicts();
        if (verdicts != last_mission_verdicts) {
            std::vector<int> values;
            values.reserve(verdicts.size());
            for (dino::ObjectiveVerdict verdict : verdicts) {
                values.push_back(static_cast<int>(verdict));
            }
            park.set_mission_verdicts(values);
            for (std::size_t i = 0; i < verdicts.size(); ++i) {
                if (i < last_mission_verdicts.size() &&
                    verdicts[i] == last_mission_verdicts[i]) {
                    continue;
                }
                std::printf(
                    "mission objective %zu %s -> %s\n", i,
                    campaign_runtime->mission()
                        .objectives[i]
                        .verb.c_str(),
                    dino::verdict_name(verdicts[i]));
            }
            last_mission_verdicts = verdicts;
        }
        if (campaign_runtime->finished()) {
            const bool success = campaign_runtime->overall() ==
                                 dino::ObjectiveVerdict::Success;
            const dino::CampaignMission finished =
                campaign_runtime->mission();
            std::printf("mission %s: %s\n", finished.name.c_str(),
                        success ? "success" : "failure");
            std::string end_text =
                success ? "Mission complete." : "Mission failed.";
            pending_next_mission.reset();
            if (success) {
                pending_next_mission =
                    dino::next_campaign_mission(finished);
                if (!pending_next_mission) {
                    const std::optional<dino::Campaign> campaign =
                        dino::load_parent_campaign(finished);
                    if (campaign) {
                        const std::string ending =
                            dino::ui::resolve_loc_text(
                                campaign->ending_text_id, park.loc);
                        if (!ending.empty()) {
                            end_text = ending;
                        }
                        std::printf(
                            "campaign complete: %s ending=%s video=%s\n",
                            campaign->name.c_str(), ending.c_str(),
                            campaign->ending_video.c_str());
                        if (!campaign->ending_video.empty()) {
                            pending_ending_video = resolve_asset(
                                opt.asset_root,
                                "Data/Video/" + campaign->ending_video);
                        }
                        const unsigned number =
                            dino::campaign_number(campaign->name);
                        std::uint32_t next_value = campaign_unlock;
                        if (number > 0) {
                            next_value = std::max(next_value, number + 1);
                        }
                        if (next_value != campaign_unlock) {
                            campaign_unlock = next_value;
                            try {
                                dino::write_campaign_unlock(
                                    campaign_dino_dat, campaign_unlock);
                                std::printf(
                                    "campaign unlock: value=%u\n",
                                    static_cast<unsigned>(
                                        campaign_unlock));
                            } catch (const std::exception& e) {
                                std::fprintf(stderr,
                                             "campaign unlock: %s\n",
                                             e.what());
                            }
                        }
                        campaign_progress.erase(campaign->name);
                        try {
                            dino::save_campaign_progress(
                                campaign_progress,
                                campaign_progress_path);
                            std::printf(
                                "campaign progress: %s complete\n",
                                campaign->name.c_str());
                        } catch (const std::exception& e) {
                            std::fprintf(stderr,
                                         "campaign progress: %s\n",
                                         e.what());
                        }
                    }
                }
            }
            park.show_campaign_end(end_text);
            campaign_runtime->stop();
            if (opt.auto_continue) {
                park.close_dialog();
                advance_campaign();
            }
        }
    }
}

void Game::update_audio() {
    ui_sound.update(frame_dt);
    music.update();

    if (minimap_zoom_in) {
        minimap_scale =
            std::max(0.1f, minimap_scale - static_cast<float>(frame_dt) * 0.15f);
    }
    if (minimap_zoom_out) {
        minimap_scale =
            std::min(2.0f, minimap_scale + static_cast<float>(frame_dt) * 0.15f);
    }
}

void Game::update_camera() {
    if (free_camera && !opt.no_input) {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto held = [&](dino::KeyAction action) {
            const SDL_Scancode scancode =
                SDL_GetScancodeFromKey(keymap[action], nullptr);
            return scancode != SDL_SCANCODE_UNKNOWN && keys[scancode];
        };
        const float yaw_rad = free_yaw * 3.14159265f / 180.0f;
        const float pitch_rad = free_pitch * 3.14159265f / 180.0f;
        const dino::Vec3 forward{std::cos(pitch_rad) * std::cos(yaw_rad),
                                 std::cos(pitch_rad) * std::sin(yaw_rad),
                                 std::sin(pitch_rad)};
        const dino::Vec3 right{std::sin(yaw_rad), -std::cos(yaw_rad), 0.0f};
        const float speed = held(dino::KeyAction::Sprint) ? 140.0f : 40.0f;
        const float step = speed * static_cast<float>(frame_dt);
        if (held(dino::KeyAction::MoveForward)) {
            free_pos.x += forward.x * step;
            free_pos.y += forward.y * step;
            free_pos.z += forward.z * step;
        }
        if (held(dino::KeyAction::MoveBack)) {
            free_pos.x -= forward.x * step;
            free_pos.y -= forward.y * step;
            free_pos.z -= forward.z * step;
        }
        if (held(dino::KeyAction::MoveRight)) {
            free_pos.x += right.x * step;
            free_pos.y += right.y * step;
        }
        if (held(dino::KeyAction::MoveLeft)) {
            free_pos.x -= right.x * step;
            free_pos.y -= right.y * step;
        }
        if (held(dino::KeyAction::MoveUp)) {
            free_pos.z += step;
        }
        if (held(dino::KeyAction::MoveDown)) {
            free_pos.z -= step;
        }
        if (terrain.world_size > 0.0f) {
            const float half = terrain.world_size * 0.5f;
            if (free_pos.x >= -half && free_pos.x <= half &&
                free_pos.y >= -half && free_pos.y <= half) {
                const float min_z = terrain.height_at(free_pos.x, free_pos.y) + 1.6f;
                free_pos.z = std::max(free_pos.z, min_z);
            }
        }
    } else if (!opt.no_input && !start_screen && !park.name_edit_active()) {
        // Retail park camera (camera.ini / KeyMapper.ini):
        //   arrows       pan the look-at target (CameraKey*)
        //   Shift+arrows rotate around it (CameraRotate*, pi/2 rad/s)
        //   + / -        zoom (NAVIGATION_ZOOM/DEZOOM)
        // The drag rotation below is a port-only mouse convenience.
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto held = [&](dino::KeyAction action) {
            const SDL_Scancode scancode =
                SDL_GetScancodeFromKey(keymap[action], nullptr);
            return scancode != SDL_SCANCODE_UNKNOWN && keys[scancode];
        };
        const bool shift = keys[SDL_SCANCODE_LSHIFT] ||
                           keys[SDL_SCANCODE_RSHIFT];
        const float dtf = static_cast<float>(frame_dt);
        if (camera_retail_model) {
            // Eight action bytes consumed by 0x005e5ff0:
            //   0..3 pan (arrows / GOTOP..GORIGHT),
            //   4/5 yaw (Shift+Left/Right, Del/PgDn),
            //   6/7 progress (Shift+Up/Down, Home/End, ZOOM/DEZOOM).
            bool flags[8] = {};
            flags[0] = nav_pan_forward ||
                       (!shift && held(dino::KeyAction::OrbitUp));
            flags[1] = nav_pan_back ||
                       (!shift && held(dino::KeyAction::OrbitDown));
            flags[2] = nav_pan_left ||
                       (!shift && held(dino::KeyAction::OrbitLeft));
            flags[3] = nav_pan_right ||
                       (!shift && held(dino::KeyAction::OrbitRight));
            flags[4] = nav_turn_left ||
                       (shift && held(dino::KeyAction::OrbitLeft)) ||
                       keys[SDL_SCANCODE_DELETE];
            flags[5] = nav_turn_right ||
                       (shift && held(dino::KeyAction::OrbitRight)) ||
                       keys[SDL_SCANCODE_PAGEDOWN];
            flags[6] = nav_zoom_in ||
                       (shift && held(dino::KeyAction::OrbitUp)) ||
                       keys[SDL_SCANCODE_HOME];
            flags[7] = nav_zoom_out ||
                       (shift && held(dino::KeyAction::OrbitDown)) ||
                       keys[SDL_SCANCODE_END];
            // Retail edge scroll: 2 client px bands, OR-ed into the same
            // pan flags, disabled while the rotate modifier (Shift) is
            // held. The position persists on focus loss.
            const int edge_band = std::max(1, opt.edge_scroll);
            if (!shift && cam_mouse_valid) {
                flags[2] = flags[2] || cam_mouse_x < edge_band;
                flags[3] = flags[3] ||
                           cam_mouse_x > width - edge_band;
                flags[0] = flags[0] || cam_mouse_y < edge_band;
                flags[1] = flags[1] ||
                           cam_mouse_y > height - edge_band;
            }
            // The binary applies the pan step once per camera Update.
            // Run the retail camera at a fixed tick so a 1000 Hz render
            // loop does not pan 16x faster than a 60 Hz one.
            const float camera_step =
                1.0f / std::clamp(opt.camera_hz, 10.0f, 240.0f);
            camera_accum =
                std::min(camera_accum + dtf, camera_step * 4.0f);
            int camera_steps = 0;
            while (camera_accum >= camera_step && camera_steps < 4) {
                retail_camera.update(camera_config, terrain, camera_step,
                                     flags);
                camera_accum -= camera_step;
                ++camera_steps;
            }
            camera_progress = retail_camera.progress;
            center = retail_camera.target_cur;
            yaw = retail_camera.yaw_cur * 57.2957795f;
            camera_yaw_goal = yaw;
            const float radius =
                camera_config.distance(camera_progress);
            const float height =
                camera_config.elevation(camera_progress);
            pitch = std::atan2(height, radius) * 57.2957795f;
            distance = std::sqrt(radius * radius + height * height);
        } else {
            // Legacy spherical orbit for --camera-orbit and explicit
            // screenshot views. Speed/limits still come from camera.ini.
            const float pitch_rad = pitch * 3.14159265f / 180.0f;
            const float horizontal =
                std::max(camera_config.distance_min,
                         distance * std::cos(pitch_rad));
            const float progress = std::clamp(
                (horizontal - camera_config.distance_min) /
                    std::max(1.0f, camera_config.distance_max -
                                       camera_config.distance_min),
                0.0f, 1.0f);
            const float rotate =
                camera_config.rotation_speed(progress) * 57.2957795f *
                dtf;
            if (nav_turn_left ||
                (shift && held(dino::KeyAction::OrbitLeft))) {
                yaw -= rotate;
            }
            if (nav_turn_right ||
                (shift && held(dino::KeyAction::OrbitRight))) {
                yaw += rotate;
            }
            const float min_pitch =
                std::atan2(camera_config.elevation_min,
                           std::max(1e-3f, distance)) *
                57.2957795f;
            const float max_pitch =
                std::atan2(camera_config.elevation_max,
                           std::max(1e-3f, distance)) *
                57.2957795f;
            if (shift && held(dino::KeyAction::OrbitUp)) {
                pitch = std::min(max_pitch, pitch + rotate);
            }
            if (shift && held(dino::KeyAction::OrbitDown)) {
                pitch = std::max(min_pitch, pitch - rotate);
            }
            // `yaw` points from the target to the eye; the view direction
            // is the opposite. Move the target in the camera's ground
            // plane at the camera.ini MoveSpeed (4-unit cells/second).
            const float view_yaw =
                yaw * 3.14159265f / 180.0f + 3.14159265f;
            const dino::Vec3 forward{std::cos(view_yaw),
                                     std::sin(view_yaw), 0.0f};
            const dino::Vec3 right{std::sin(view_yaw),
                                   -std::cos(view_yaw), 0.0f};
            const float step =
                camera_config.move_speed(progress) * 4.0f * dtf;
            dino::Vec3 delta{0.0f, 0.0f, 0.0f};
            if (nav_pan_forward ||
                (!shift && held(dino::KeyAction::OrbitUp))) {
                delta.x += forward.x * step;
                delta.y += forward.y * step;
            }
            if (nav_pan_back ||
                (!shift && held(dino::KeyAction::OrbitDown))) {
                delta.x -= forward.x * step;
                delta.y -= forward.y * step;
            }
            if (nav_pan_right ||
                (!shift && held(dino::KeyAction::OrbitRight))) {
                delta.x += right.x * step;
                delta.y += right.y * step;
            }
            if (nav_pan_left ||
                (!shift && held(dino::KeyAction::OrbitLeft))) {
                delta.x -= right.x * step;
                delta.y -= right.y * step;
            }
            if (delta.x != 0.0f || delta.y != 0.0f) {
                center.x += delta.x;
                center.y += delta.y;
                if (terrain.world_size > 0.0f) {
                    const float half = terrain.world_size * 0.5f;
                    center.x = std::clamp(center.x, -half, half);
                    center.y = std::clamp(center.y, -half, half);
                    const float target_z =
                        terrain.height_at(center.x, center.y) + 2.5f;
                    const float alpha = std::min(
                        1.0f,
                        camera_config.position_hardness(progress) * dtf);
                    center.z += (target_z - center.z) * alpha;
                }
            }
            // ProgressSpeed is a fraction of the full distance range per
            // second: 0.05..0.2 over 290 units = 14.5..58 units/s.
            const float zoom_step =
                camera_config.progress_speed(progress) *
                (camera_config.distance_max -
                 camera_config.distance_min) *
                dtf;
            if (nav_zoom_in || held(dino::KeyAction::ZoomIn)) {
                distance = std::max(camera_config.distance_min,
                                    distance - zoom_step);
            }
            if (nav_zoom_out || held(dino::KeyAction::ZoomOut)) {
                distance = std::min(camera_config.distance_max,
                                    distance + zoom_step);
            }
        }
    }
}

void Game::update_diagnostics() {
    if (std::getenv("DINO_DEBUG_CAMERA") != nullptr && (frame % 30) == 0) {
        std::fprintf(stderr,
                     "cam free=%d target=%.1f,%.1f,%.1f yaw=%.1f "
                     "pitch=%.1f dist=%.1f p=%.3f r=%.1f h=%.1f "
                     "yaw_goal=%.1f up=0,0,1\n",
                     free_camera ? 1 : 0, center.x, center.y, center.z,
                     yaw, pitch, distance,
                     camera_retail_model ? camera_progress : -1.0f,
                     camera_retail_model ? camera_radius() : -1.0f,
                     camera_retail_model ? camera_height() : -1.0f,
                     camera_yaw_goal);
    }

    if (audio != nullptr && opt.loop_audio && !wav.data.empty()) {
        const int queued = SDL_GetAudioStreamQueued(audio);
        if (queued < static_cast<int>(wav.data.size() / 2)) {
            SDL_PutAudioStreamData(audio, wav.data.data(),
                                   static_cast<int>(wav.data.size()));
        }
    }
}

void Game::set_background() {
    const float pulse = 0.5f + 0.5f * SDL_sinf(static_cast<float>(elapsed) * 1.5f);
    glClearColor(0.055f + 0.02f * pulse, 0.09f + 0.03f * pulse,
                 0.14f + 0.04f * pulse, 1.0f);
}

void Game::update_simulation() {
    if (skinned) {
        double anim_time = opt.anim_time >= 0.0 ? opt.anim_time : 0.0;
        if (opt.anim_time < 0.0 && anim.duration > 0) {
            if (!opt.capture_dir.empty() && opt.frames > 0) {
                anim_time = std::fmod(static_cast<double>(frame) *
                                          static_cast<double>(anim.duration) /
                                          static_cast<double>(opt.frames),
                                      static_cast<double>(anim.duration));
            } else {
                anim_time = std::fmod(elapsed * 4800.0 * opt.anim_speed,
                                      static_cast<double>(anim.duration));
            }
        }
        const dino::Animation* anim_ptr = anim.bones.empty() ? nullptr : &anim;
        dino::Mesh deformed = build_skinned_mesh(skin.mesh, skin, skeleton,
                                                 anim_ptr, anim_time,
                                                 vertex_bone, bone_skel,
                                                 opt.skin_bind_from_skin);
        if (mesh.colors.size() == deformed.positions.size()) {
            deformed.colors = mesh.colors;
        }
        if (place_dino) {
            deformed = place_on_terrain(std::move(deformed), terrain,
                                        spawn_x, spawn_y, opt.dino_angle);
            update_mesh(dino_gpu, deformed, glfns);
        } else {
            update_mesh(gpu, deformed, glfns);
        }
    }

    // `FUN_00589CD0`: rebuild the 8-unit danger grid once per second, then
    // recompute each dinosaur's SECU from the cell under its position.
    if (!imported_dinos.empty() && terrain.world_size > 0.0f) {
        danger_timer += frame_dt;
        if (danger_timer >= 1.0) {
            danger_timer = 0.0;
            danger_grid.clear();
            for (const ImportedDino& dinosaur : imported_dinos) {
                const dino::DinoNeeds& needs =
                    dinosaur.action_loop ? dinosaur.action_loop->needs()
                                         : dinosaur.needs;
                // ImportedDino::x/y are raw `.wld`/`.raw` units (the
                // terrain and placement code subtract world_size/2).
                danger_grid.stamp(dinosaur.x, dinosaur.y,
                                  needs.threat_radius, needs.threat);
            }
            // The grid is "live" when any dinosaur carries danger
            // attributes. With every cache at 0 (the import default) the
            // retail grid would pin SECU to 99, so the port keeps the
            // configured value and `--dino-needs` fixtures stay useful.
            bool any_live = false;
            for (const ImportedDino& dinosaur : imported_dinos) {
                const dino::DinoNeeds& needs =
                    dinosaur.action_loop ? dinosaur.action_loop->needs()
                                         : dinosaur.needs;
                if (needs.threat != 0 || needs.threat_radius != 0 ||
                    needs.health != needs.max_health) {
                    any_live = true;
                    break;
                }
            }
            for (ImportedDino& dinosaur : imported_dinos) {
                dino::DinoNeeds& needs =
                    dinosaur.action_loop ? dinosaur.action_loop->needs()
                                         : dinosaur.needs;
                const float raw_x = dinosaur.x;
                const float raw_y = dinosaur.y;
                const int cell = danger_grid.at(raw_x, raw_y);
                const int secu = dino::secu_from_grid(
                    danger_grid, raw_x, raw_y, needs.threat, needs.health,
                    needs.max_health);
                // The port's need range is 0..99; retail stores the raw
                // value but never exceeds 99 for in-grid, undamaged
                // dinosaurs.
                const int applied = std::max(0, std::min(99, secu));
                if (any_live) {
                    needs.value[2] = applied;
                    if (dinosaur.action_loop) {
                        dinosaur.action_loop->set_secu(applied);
                    }
                }
                if (opt.secu_probe) {
                    std::printf(
                        "secu: %s x=%.1f y=%.1f threat=%d radius=%d "
                        "cell=%d health=%d/%d secu=%d\n",
                        dinosaur.species.c_str(), dinosaur.x, dinosaur.y,
                        needs.threat, needs.threat_radius, cell,
                        needs.health, needs.max_health, applied);
                }
            }
        }
    }

    for (ImportedDino& dinosaur : imported_dinos) {
        if (dinosaur.action_loop) {
            // The `.bai` consume gate: Eat is only available when the
            // dinosaur's paddock holds a matching food source.
            const int pen = paddock_at(dinosaur.x, dinosaur.y);
            dinosaur.action_loop->set_inventory(
                paddock_food(pen, dinosaur.carnivore) != nullptr);
            const int finished_before =
                dinosaur.action_loop->finished_count();
            const std::string previous =
                dinosaur.action_loop->action_name();
            dinosaur.action_loop->tick(frame_dt);
            if (dinosaur.action_loop->finished_count() !=
                finished_before) {
                const std::string& finished =
                    dinosaur.action_loop->last_finished_action();
                if (finished == "Eat" || finished == "EatForSmallies") {
                    dino::FoodSource* source =
                        paddock_food(pen, dinosaur.carnivore);
                    if (source != nullptr) {
                        source->remaining =
                            std::max(0.0f, source->remaining - 1.0f);
                        const std::string& food_name =
                            food_defs[static_cast<std::size_t>(
                                          source->def)]
                                .name;
                        std::printf(
                            "dino eat: %s <- %s remaining=%.2f\n",
                            dinosaur.species.c_str(), food_name.c_str(),
                            source->remaining);
                        if (source->remaining <= 0.0f) {
                            std::printf("food: %s depleted\n",
                                        food_name.c_str());
                            rebuild_extra_items();
                        }
                    }
                }
            }
            const std::string& action =
                dinosaur.action_loop->action_name();
            if (action != previous) {
                if (action != "idle") {
                    const dino::BaiAction& definition =
                        dino_action_table[static_cast<std::size_t>(
                            dinosaur.action_loop->action_index())];
                    const int score = dino::score_action(
                        definition, dinosaur.action_loop->needs());
                    const dino::DinoNeeds& needs =
                        dinosaur.action_loop->needs();
                    std::printf(
                        "dino action: %s %s score=%d "
                        "needs=%d/%d/%d/%d\n",
                        dinosaur.species.c_str(), action.c_str(), score,
                        needs.value[0], needs.value[1], needs.value[2],
                        needs.value[3]);
                }
                const std::string clip =
                    action == "idle" ? "Neutral"
                                     : dino_clip_for_action(action);
                if (clip != dinosaur.clip) {
                    try {
                        dinosaur.assets = load_imported_species(
                            dinosaur.species, dinosaur.family, clip);
                        dinosaur.clip = clip;
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "dino clip %s: %s\n",
                                     clip.c_str(), e.what());
                    }
                }
            }
        }
        const ImportedSpecies& assets = *dinosaur.assets;
        if (assets.anim.bones.empty() || assets.anim.duration <= 0.0) {
            continue;
        }
        double anim_time = opt.anim_time;
        if (opt.anim_time < 0.0) {
            anim_time = std::fmod(
                elapsed * 4800.0 * opt.anim_speed + dinosaur.phase,
                static_cast<double>(assets.anim.duration));
        }
        dino::Mesh deformed = build_skinned_mesh(
            assets.skin.mesh, assets.skin, assets.skeleton, &assets.anim,
            anim_time, assets.vertex_bone, assets.bone_skel, false);
        if (assets.mesh.colors.size() == deformed.positions.size()) {
            deformed.colors = assets.mesh.colors;
        }
        deformed = place_on_terrain(std::move(deformed), terrain,
                                    dinosaur.x, dinosaur.y,
                                    dinosaur.angle);
        update_mesh(dinosaur.gpu, deformed, glfns);
    }

    if (!visitor_system.visitors.empty()) {
        visitor_system.update(build_grid, terrain,
                              static_cast<float>(frame_dt), attractions,
                              entrance_cell_x, entrance_cell_y,
                              visitor_profiles);
        update_mesh(visitor_gpu,
                    visitor_system.build_mesh(build_grid, terrain,
                                              visitor_pose1, visitor_pose2),
                    glfns);
    }
}

void Game::update_camera_view() {
    dino::Vec3 eye;
    dino::Vec3 view_center;
    if (free_camera) {
        const float yaw_rad = free_yaw * 3.14159265f / 180.0f;
        const float pitch_rad = free_pitch * 3.14159265f / 180.0f;
        const dino::Vec3 forward{std::cos(pitch_rad) * std::cos(yaw_rad),
                                 std::cos(pitch_rad) * std::sin(yaw_rad),
                                 std::sin(pitch_rad)};
        eye = free_pos;
        view_center = {free_pos.x + forward.x * 10.0f,
                       free_pos.y + forward.y * 10.0f,
                       free_pos.z + forward.z * 10.0f};
    } else {
        eye = orbit_eye();
        view_center = center;
    }
}

void Game::draw_frame() {
    // WaterSurface animation (FUN_00602fd0): +0x10 += dt*0.03,
    // +0x14 += dt*0.01. Stage 0 uses (-u14,-u10), stage 1 (+u10,+u14),
    // so the two layers drift in opposite directions.
    water_scroll += std::min(static_cast<float>(frame_dt), 0.1f);
    for (DrawItem& item : extra_items) {
        if (item.modulate2x && item.texture2 != 0) {
            item.uv_offset[0] = -0.01f * water_scroll;
            item.uv_offset[1] = -0.03f * water_scroll;
            item.uv2_offset[0] =
                0.25f / 30.0f + 0.03f * water_scroll;
            item.uv2_offset[1] = 0.01f * water_scroll;
        }
    }
    // gpkLandscapeSector 0x609970: one camera distance per sector,
    // then every object in it picks LOD1/LOD2/none (0x508c10).
    for (std::size_t i = 0; i < veg_visible.size(); ++i) {
        const auto& group = vegetation.groups[i];
        if (opt.no_lod) {
            veg_visible[i] = group.lod == 0 ? 1 : 0;
            continue;
        }
        const dino::Vec3& c = vegetation.sector_centres[group.sector];
        const float dx = c.x - eye.x;
        const float dy = c.y - eye.y;
        const float dz = c.z - eye.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        veg_visible[i] =
            dino::landscape_lod(distance, lod_quality) == group.lod ? 1 : 0;
    }
    const int culled =
        draw_scene(gpu, texture, extra_items, skybox, program, glfns, eye,
                   view_center, width, height,
                   terrain.world_size > 0.0f ? 80.0f : 33.0f, !opt.no_cull,
                   opt.texture_path.empty() ? detail_texture : 0);
    if (veg_stats) {
        std::size_t drawn[2] = {0, 0};
        std::size_t batches = 0;
        for (std::size_t i = 0; i < veg_visible.size(); ++i) {
            if (veg_visible[i] != 0) {
                drawn[vegetation.groups[i].lod] += vegetation.groups[i].instances;
                ++batches;
            }
        }
        std::printf("vegetation frame: lod1 %zu lod2 %zu hidden %zu, "
                    "batches %zu culled %d\n",
                    drawn[0], drawn[1],
                    vegetation.instances - drawn[0] - drawn[1], batches,
                    culled);
        veg_stats = false;
    }

    auto draw_2d = [&](const dino::Mesh& mesh, GLuint texture_id, float alpha,
                       int unlit) {
        draw_ui_mesh(hud_gpu, mesh, texture_id, alpha, unlit, width, height);
    };

    if (start_screen) {
        if (menu_loaded && opt.use_menu) {
            draw_menu(width, height, logical_width, logical_height);
        } else if (hud_font_texture != 0) {
            draw_2d(build_screen_rect(0.0f, 0.0f, static_cast<float>(width),
                                      static_cast<float>(height),
                                      {8, 12, 18, 255}),
                    0, 0.55f, 1);
            const std::string title = "DINO ISLAND";
            const float title_scale = 4.0f;
            const float title_width = text_width(hud_font, title, title_scale);
            draw_2d(build_text_mesh(hud_font, title,
                                    (static_cast<float>(width) - title_width) * 0.5f,
                                    static_cast<float>(height) * 0.30f,
                                    title_scale, {240, 236, 220, 255}),
                    hud_font_texture, 1.0f, 2);
            const std::string subtitle = "Press ENTER to begin";
            const float subtitle_scale = 1.6f;
            const float subtitle_width =
                text_width(hud_font, subtitle, subtitle_scale);
            draw_2d(build_text_mesh(
                        hud_font, subtitle,
                        (static_cast<float>(width) - subtitle_width) * 0.5f,
                        static_cast<float>(height) * 0.52f, subtitle_scale,
                        {220, 228, 236, 255}),
                    hud_font_texture, 1.0f, 2);
            const std::string hint = "Esc quits    Tab help";
            const float hint_width = text_width(hud_font, hint, 1.2f);
            draw_2d(build_text_mesh(
                        hud_font, hint,
                        (static_cast<float>(width) - hint_width) * 0.5f,
                        static_cast<float>(height) * 0.66f, 1.2f,
                        {170, 185, 200, 255}),
                    hud_font_texture, 1.0f, 2);
        }
    } else {
        if (park.loaded()) {
            draw_park_hud(width, height, logical_width, logical_height);
            if (minimap_ready && !park.hud_hidden()) {
                const float dpi =
                    logical_width > 0
                        ? static_cast<float>(width) /
                              static_cast<float>(logical_width)
                        : 1.0f;
                const dino::ui::Widget* map_widget =
                    park.overlay.get("MAP_RENDERVIEW");
                const float map_x =
                    static_cast<float>(map_widget ? map_widget->abs_x : 652) *
                    dpi;
                const float map_y =
                    static_cast<float>(map_widget ? map_widget->abs_y : 451) *
                    dpi;
                const float map_w =
                    static_cast<float>(map_widget ? map_widget->width : 136) *
                    dpi;
                const float map_h =
                    static_cast<float>(map_widget ? map_widget->height : 136) *
                    dpi;
                // MAP_RENDERVIEW is a bottom-up FBO texture: flip V.
                draw_ui_mesh(
                    menu_gpu,
                    build_ui_quad(map_x, map_y, map_w, map_h, 0.0f, 1.0f,
                                  1.0f, 0.0f, {255, 255, 255, 255}),
                    minimap.color, 1.0f, 1, width, height);

                // Retail map marker: red block at the camera position
                // with a short heading tick (FUN_005fb120 maps the camera
                // position into the rotated, target-centred quad).
                const float half = terrain.world_size * 2.0f * minimap_scale;
                if (half > 0.5f) {
                    const float angle = std::atan2(view_center.y - eye.y,
                                                   view_center.x - eye.x);
                    const float dx = eye.x - view_center.x;
                    const float dy = eye.y - view_center.y;
                    const float sx =
                        dx * std::sin(angle) - dy * std::cos(angle);
                    const float sy =
                        dx * std::cos(angle) + dy * std::sin(angle);
                    const float centre_x = map_x + map_w * 0.5f;
                    const float centre_y = map_y + map_h * 0.5f;
                    const float half_px = map_w * 0.5f;
                    const float marker_x =
                        centre_x + (sx / half) * half_px;
                    const float marker_y =
                        centre_y - (sy / half) * half_px;
                    const float inset = 4.0f * dpi;
                    if (marker_x > map_x + inset &&
                        marker_x < map_x + map_w - inset &&
                        marker_y > map_y + inset &&
                        marker_y < map_y + map_h - inset) {
                        draw_ui_mesh(
                            menu_gpu,
                            build_screen_rect(marker_x - 2.5f * dpi,
                                              marker_y - 2.5f * dpi,
                                              5.0f * dpi, 5.0f * dpi,
                                              {220, 32, 32, 255}),
                            0, 1.0f, 1, width, height);
                        draw_ui_mesh(
                            menu_gpu,
                            build_screen_rect(marker_x - 0.75f * dpi,
                                              marker_y - 7.0f * dpi,
                                              1.5f * dpi, 4.5f * dpi,
                                              {250, 224, 64, 255}),
                            0, 1.0f, 1, width, height);
                    }
                }
            }
        }
        if (hud_visible && hud_font_texture != 0) {
            char line1[128];
            char line2[192];
            std::snprintf(line1, sizeof(line1), "Dino Island   FPS %.0f",
                          frame_dt > 0.0 ? 1.0 / frame_dt : 0.0);
            std::snprintf(line2, sizeof(line2),
                          "visitors %zu   paths %d   fences %d",
                          visitor_system.visitors.size(), build_grid.path_count,
                          build_grid.fence_count);
            std::string text = std::string(line1) + "\n" + line2;
            if (opt.frames == 0 && elapsed < 12.0) {
                text += "\nArrows pan   Shift+Arrows rotate   "
                        "+/- or wheel zoom   C free-fly";
            }
            draw_2d(build_text_mesh(hud_font, text, 12.0f, 8.0f, 1.0f,
                                    {235, 240, 245, 255}),
                    hud_font_texture, 0.95f, 2);
        }
        if (show_help && hud_font_texture != 0) {
            const float panel_w =
                std::min(640.0f, static_cast<float>(width) - 80.0f);
            draw_2d(build_screen_rect(40.0f, 60.0f, panel_w, 230.0f,
                                      {10, 14, 20, 255}),
                    0, 0.80f, 1);
            draw_2d(build_text_mesh(
                        hud_font,
                        "CONTROLS\n\n"
                        "Arrows pan target   Shift+Arrows rotate\n"
                        "+/- or wheel zoom   C free-fly   R reset\n"
                        "1 path    2 fence    0 off    U undo    click place\n"
                        "F1 HUD    Tab close    Esc quit",
                        64.0f, 78.0f, 1.4f, {235, 240, 245, 255}),
                    hud_font_texture, 1.0f, 2);
        }
    }
}

void Game::present_frame() {
    if (!opt.capture_dir.empty()) {
        std::filesystem::create_directories(opt.capture_dir);
        char path[1024];
        std::snprintf(path, sizeof(path), "%s/%s_%04d.bmp",
                      opt.capture_dir.c_str(), opt.capture_prefix.c_str(), frame);
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        write_bmp(path, width, height, pixels);
    }

    if (!opt.screenshot.empty() &&
        (opt.frames <= 0 ? false : frame + 1 >= opt.frames)) {
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        write_bmp(opt.screenshot, width, height, pixels);
    }

    SDL_GL_SwapWindow(window);
    ++frame;
    if (opt.frames > 0 && frame >= opt.frames) {
        running = false;
    }
    if (frame % 120 == 0) {
        std::printf("frame %d | %.1f FPS | %dx%d | yaw %.1f pitch %.1f dist %.2f\n",
                    frame, 1.0 / (frame_dt > 0.0 ? frame_dt : 1.0), width, height,
                    yaw, pitch, distance);
    }
}

}  // namespace dino::app
