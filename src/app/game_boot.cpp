// Startup, part 1: process options, keymap, budgets, campaign and save
// discovery, menu state, asset loading, print modes and the SDL window.
// Called in order by Game::boot().

#include "game.h"

#include "reports.h"

#include "../formats/ini.h"

namespace dino::app {

std::optional<int> Game::boot() {
    if (std::optional<int> exit_code = boot_command_line()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_budgets_and_clock()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_campaign_and_saves()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_menu_state()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_menu_overlay()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_demo_defaults()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_load_assets()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_build_textures()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_print_modes()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_platform()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_scene_resources()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_park_objects()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_minimap_and_visitors()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_employee_profiles()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_import_dinos()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_food_definitions()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_camera_setup()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_park_session()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_employee_session()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_build_tools()) {
        return exit_code;
    }
    if (std::optional<int> exit_code = boot_finalize()) {
        return exit_code;
    }
    return std::nullopt;
}

std::optional<int> Game::boot_command_line() {
    if (std::optional<int> report = dino::app::run_cli_reports(opt)) {
        return *report;
    }
    dino::load_keymap(opt.keys_path, keymap);
    for (const auto& rebind : opt.rebinds) {
        std::string error;
        if (!dino::rebind_key(keymap, rebind, error)) {
            std::fprintf(stderr, "rebind error: %s\n", error.c_str());
            return 2;
        }
    }
    if (!opt.rebinds.empty() && !opt.no_save_keys) {
        dino::save_keymap(opt.keys_path, keymap);
    }
    if (!opt.music_forced) {
        opt.music = opt.frames == 0 && opt.screenshot.empty();
    }
    if (opt.list_keys) {
        dino::print_keymap(keymap);
        return 0;
    }
    return std::nullopt;
}

std::optional<int> Game::boot_budgets_and_clock() {
    free_game_budgets = {75000, 50000, 25000};
    free_game_sellback = {1.0f, 0.5f, 0.2f};
    try {
        island_names = read_island_list(resolve_asset(
            opt.asset_root, "Data/Design/landscape/IslandList.ini"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "island list: %s\n", e.what());
    }
    try {
        const dino::IniFile free_game = dino::load_ini(
            resolve_asset(opt.asset_root, "Data/Design/Scenario/FreeGame.ini"));
        if (const dino::IniSection* context = free_game.find("CONTEXT")) {
            auto budget = [&](const char* key, int fallback) {
                const std::string value = context->get(key);
                return value.empty() ? fallback : std::atoi(value.c_str());
            };
            free_game_budgets[0] = budget("Budget", 75000);
            free_game_budgets[1] = budget("BudgetMedium", 50000);
            free_game_budgets[2] = budget("BudgetHard", 25000);
            auto sellback = [&](const char* key, float fallback) {
                const std::string value = context->get(key);
                return value.empty()
                           ? fallback
                           : static_cast<float>(std::atof(value.c_str()));
            };
            free_game_sellback[0] = sellback("SellBackFactor", 1.0f);
            free_game_sellback[1] = sellback("SellBackFactorMedium", 0.5f);
            free_game_sellback[2] = sellback("SellBackFactorHard", 0.2f);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "free game scenario: %s\n", e.what());
    }
    park_clock.time_scale =
        opt.park_time_scale > 0.0f ? opt.park_time_scale : 60.0;
    if (!opt.park_time.empty()) {
        int hour = 0;
        int minute = 0;
        if (std::sscanf(opt.park_time.c_str(), "%d:%d", &hour, &minute) == 2) {
            const double target =
                (hour - park_clock.base_hour) * 60.0 + minute;
            park_clock.elapsed =
                target > 0.0 ? target / park_clock.minutes_per_second() : 0.0;
        }
    }
    if (opt.park_advance > 0.0f) {
        park_clock.advance(opt.park_advance);
        if (park_clock.end_of_day) {
            park_clock.next_day();
        }
    }
    std::printf("park clock: scale=%.1f base=%d time=%s day=%s\n",
                park_clock.time_scale, park_clock.base_hour,
                park_clock.time_text(false).c_str(),
                park_clock.day_text().c_str());
    return std::nullopt;
}

std::optional<int> Game::boot_campaign_and_saves() {
    campaign_progress_path = opt.campaign_progress_path.empty()
            ? "run/save/campaign-progress.txt"
            : opt.campaign_progress_path;
    campaign_progress = dino::load_campaign_progress(campaign_progress_path);
    try {
        const dino::IniFile sound_db = dino::load_ini(
            resolve_asset(opt.asset_root, "Data/Sound/MenuSoundDB.ini"));
        for (const auto& section : sound_db.sections) {
            const std::string filename = section.get("FILENAME");
            if (!filename.empty()) {
                menu_sound_files[section.name] = filename;
                menu_sound_names.push_back(section.name);
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "menu sounds: %s\n", e.what());
    }
    try {
        const dino::IniFile sound_db = dino::load_ini(
            resolve_asset(opt.asset_root, "Data/Sound/ParkSoundDB.ini"));
        for (const auto& section : sound_db.sections) {
            const std::string filename = section.get("FILENAME");
            if (!filename.empty()) {
                park_sound_files[section.name] = filename;
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "park sounds: %s\n", e.what());
    }
    try {
        const std::filesystem::path save_dir("run/save");
        if (std::filesystem::is_directory(save_dir)) {
            for (const auto& entry :
                 std::filesystem::directory_iterator(save_dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                if (entry.path().extension() == ".psav") {
                    save_slots.emplace_back(entry.path().string(),
                                            "[port] " + entry.path().stem().string());
                    continue;
                }
                if (entry.path().extension() != ".sav") {
                    continue;
                }
                try {
                    const dino::SavHeader header =
                        dino::read_sav_header(entry.path().string());
                    save_slots.emplace_back(entry.path().string(), header.name);
                    std::printf("save %s: name=%s version=%u slot=%u\n",
                                entry.path().filename().string().c_str(),
                                header.name.c_str(), header.version,
                                header.slot);
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "save %s: %s\n",
                                 entry.path().filename().string().c_str(),
                                 e.what());
                }
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "save scan: %s\n", e.what());
    }
    std::sort(save_slots.begin(), save_slots.end());
    for (const auto& slot : save_slots) {
        if (std::filesystem::path(slot.first).extension() == ".psav") {
            menu_save_slots.push_back(slot);
        }
    }
    if (opt.list_sounds) {
        for (const auto& name : menu_sound_names) {
            std::printf("sound %s: %s\n", name.c_str(),
                        menu_sound_files[name].c_str());
        }
        return 0;
    }
    return std::nullopt;
}

std::optional<int> Game::boot_menu_state() {
    menu_loaded = false;
    park_return_to_menu = false;
    pending_sell_mode = false;
    park_sell_mode = false;
    park_sell_paddock = -1;  // pending fence sale shown in REQUEST_PANEL;
    show_placements_applied = false;
    pending_show_index = -1;
    attraction_selected_slot = -1;
    show_status_printed = false;
    campaign_dino_dat = opt.dino_dat_path.empty() ? "run/save/dino.dat" : opt.dino_dat_path;
    campaign_unlock = dino::read_campaign_unlock(campaign_dino_dat);
    if (opt.use_menu) {
        menu_loaded = menu.load(opt.asset_root);
        if (!menu_loaded) {
            std::fprintf(stderr, "menu: %s (falling back to placeholder)\n",
                         menu.error.c_str());
        } else if (menu.load_campaigns(
                       resolve_asset(opt.asset_root, "Data/Design/Scenario"),
                       campaign_unlock)) {
            menu.campaign_progress = campaign_progress;
            std::printf(
                "menu campaigns: %zu unlocked=%u (%s)\n",
                menu.campaigns.size(),
                static_cast<unsigned>(campaign_unlock),
                campaign_dino_dat.c_str());
        } else {
            std::fprintf(stderr, "menu campaigns: %s\n",
                         menu.campaign_error.c_str());
        }
    }

    if (menu_loaded) {
        // Main-menu Options state (retail page 5 / Dino.ini defaults).
        menu.music_volume = std::clamp(
            static_cast<int>(std::lround(opt.music_volume)), 0, 100);
        menu.sfx_volume = std::clamp(
            static_cast<int>(std::lround(opt.sfx_volume)), 0, 100);
        menu.game_speed = std::clamp(opt.game_speed, 0, 97);
        menu.lod_quality = std::clamp(opt.menu_lod_quality, 0, 3);
        menu.texture_quality = std::clamp(opt.texture_quality, 0, 3);
        menu.texture_depth = opt.texture_depth == 32 ? 32 : 16;
        menu.hardware_tnl = opt.hardware_tnl != 0 ? 1 : 0;
        menu.hardware_sound = opt.hardware_sound != 0 ? 1 : 0;
        menu.screen_modes = {"800x600x16", "1024x768x16",
                             "1280x800x16", "1280x1024x32",
                             "1600x900x32"};
        char current_mode[32];
        std::snprintf(current_mode, sizeof(current_mode), "%dx%d",
                      opt.width, opt.height);
        for (std::size_t i = 0; i < menu.screen_modes.size(); ++i) {
            if (menu.screen_modes[i].rfind(current_mode, 0) == 0) {
                menu.screen_mode = static_cast<int>(i);
                break;
            }
        }
        menu.options_sync_positions();
        menu.game_speed_dirty = menu.game_speed != 10;
        // The menu Options LOD quality is the same retail setting as
        // Dino.ini [GRAPHIC] LODQuality (R-2 vegetation range). An explicit
        // `--lod-quality NAME` still wins.
        if (opt.lod_quality.empty()) {
            static const char* const kLodNames[] = {"HIGH", "MEDIUM", "LOW",
                                                    "AUTO"};
            opt.lod_quality =
                kLodNames[std::clamp(opt.menu_lod_quality, 0, 3)];
        }
    }
    try {
        phenotype_table = dino::read_dino_phenotype(
            resolve_asset(opt.asset_root,
                          "Data/Design/Genotype/proportion.dat"),
            resolve_asset(opt.asset_root,
                          "Data/Design/Genotype/simu_trans.dic"),
            resolve_asset(opt.asset_root,
                          "Data/Design/Genotype/TranslatorConstants.ini"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "dino phenotype table: %s\n", e.what());
    }
    pending_store_index = -1;
    pending_store_price = 0;

    pending_minimap_zoom = 0;
    pending_port_save = false;
    pending_port_load = false;
    pending_fence_tool = false;
    fence_corner_x = -1;
    fence_corner_y = -1;
    return std::nullopt;
}

std::optional<int> Game::boot_menu_overlay() {
    if (opt.park_hud) {
        std::string park_error;
        const bool loaded = park.load(
            resolve_asset(opt.asset_root, "Data/Overlay/ParkInterface.Ini"),
            resolve_asset(opt.asset_root, "Data/Overlay/ParkGraphicDB.Ini"),
            resolve_asset(opt.asset_root, "Data/Overlay/ParkEng.loc"),
            park_error);
        if (loaded) {
            park.set_save_name(opt.save_name);
            if (!opt.type_text.empty()) {
                park.set_save_name("");
                for (char c : opt.type_text) {
                    park.append_name_char(c);
                }
                opt.save_name = park.save_name();
            }
            try {
                apply_store_catalog(
                    dino::load_market(resolve_asset(
                        opt.asset_root,
                        "Data/Design/Scenario/Market.ini")),
                    "Market.ini");
            } catch (const std::exception& e) {
                std::fprintf(stderr, "store catalog: %s\n", e.what());
            }
            if (!opt.start_mission_path.empty()) {
                try {
                    start_campaign_mission(
                        dino::load_campaign_mission(opt.start_mission_path));
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "start mission: %s\n", e.what());
                }
            }
            auto dialog_text = [&](const std::string& value) {
                return !value.empty() && value[0] == '&'
                           ? dino::ui::resolve_loc_text(value, park.loc)
                           : value;
            };
            if (!opt.warning_text.empty()) {
                const std::string text = dialog_text(opt.warning_text);
                park.show_warning(text);
                std::printf("park warning: %s\n", text.c_str());
            } else if (!opt.request_text.empty()) {
                const std::string text = dialog_text(opt.request_text);
                park.show_request(text);
                std::printf("park request: %s\n", text.c_str());
            } else if (!opt.campaign_end_text.empty()) {
                const std::string text = dialog_text(opt.campaign_end_text);
                park.show_campaign_end(text);
                std::printf("park campaign end: %s\n", text.c_str());
            }
            // Apply scripted HUD clicks early so --validate can report them;
            // minimap zoom actions are replayed once the render target exists.
            auto apply_park_action = [&](int x, int y, const std::string& action,
                                         const char* label) {
                if (action == "minimap zoom in") {
                    --pending_minimap_zoom;
                } else if (action == "minimap zoom out") {
                    ++pending_minimap_zoom;
                } else if (action == "save port") {
                    pending_port_save = true;
                } else if (action == "load port") {
                    pending_port_load = true;
                } else if (action.rfind("construct ", 0) == 0) {
                    pending_construct_widget = action.substr(10);
                } else if (action ==
                           "open FENCE_CONSTRUCTION_AND_INFO_PANEL") {
                    pending_fence_tool = true;
                } else if (action == "sell mode") {
                    pending_sell_mode = true;
                } else if (action.rfind("store buy ", 0) == 0) {
                    pending_store_action = action;
                } else if (action.rfind("show place ", 0) == 0) {
                    pending_show_action = action;
                } else if (action.rfind("attraction ", 0) == 0) {
                    // Attraction rows need the placed show, which exists only
                    // after the pre-loop VALIDATE/`--show-place-pos` pass.
                    pending_attraction_actions.push_back(action);
                }
                std::printf("%s %d,%d -> %s\n", label, x, y,
                            action.empty() ? "consumed" : action.c_str());
            };
            for (const auto& value : opt.park_clicks) {
                int x = 0;
                int y = 0;
                if (std::sscanf(value.c_str(), "%d,%d", &x, &y) == 2) {
                    std::string action;
                    park.click(x, y, action);
                    if (park.return_to_menu_requested()) {
                        park.clear_return_to_menu_request();
                        park_return_to_menu = true;
                    }
                    apply_park_action(x, y, action, "park click");
                }
            }
            if (!opt.park_window_clicks.empty()) {
                // Window-coordinate clicks use the same mapping as SDL events:
                // the park HUD is laid out to the full logical window.
                park.overlay.layout_for_size(opt.width, opt.height);
                for (const auto& value : opt.park_window_clicks) {
                    int wx = 0;
                    int wy = 0;
                    if (std::sscanf(value.c_str(), "%d,%d", &wx, &wy) == 2) {
                        int x = 0;
                        int y = 0;
                        map_park_pointer(static_cast<float>(wx),
                                         static_cast<float>(wy), x, y);
                        std::string action;
                        park.click(x, y, action);
                        if (park.return_to_menu_requested()) {
                            park.clear_return_to_menu_request();
                            park_return_to_menu = true;
                        }
                        apply_park_action(x, y, action, "park window click");
                    }
                }
            }
        } else {
            std::fprintf(stderr, "park hud: %s\n", park_error.c_str());
        }
    }
    if (menu_loaded) {
        menu.loading_time = opt.menu_loading_time;
        menu.save_count = static_cast<int>(menu_save_slots.size());
        if (opt.menu_page >= 0) {
            menu.page = std::clamp(opt.menu_page, 0, 8);
        }
        update_campaign_info_lines();
        for (const auto& value : opt.menu_clicks) {
            int x = 0;
            int y = 0;
            if (std::sscanf(value.c_str(), "%d,%d", &x, &y) == 2) {
                menu.click(x, y);
                update_campaign_info_lines();
                queue_menu_load();
                queue_menu_cinematic();
            }
        }
        if (!opt.menu_hover.empty()) {
            int x = 0;
            int y = 0;
            if (std::sscanf(opt.menu_hover.c_str(), "%d,%d", &x, &y) == 2) {
                menu.move(x, y);
            }
        }
    }
    return std::nullopt;
}

std::optional<int> Game::boot_demo_defaults() {
    if (opt.start_dino) {
        // Demo path: an allosaurus at the centre of the menu-started island,
        // using the same validated assets as the vertical slice.
        auto set_default = [&](std::string& path, const char* relative) {
            if (path.empty()) {
                path = resolve_asset(opt.asset_root, relative);
            }
        };
        set_default(opt.mesh_path,
                    "Data/Graphics/Actors/Dinos/allo/allo_lod1.mesh");
        set_default(opt.skel_path,
                    "Data/Graphics/Actors/Dinos/allo/allo.skel");
        set_default(opt.skin_path,
                    "Data/Graphics/Actors/Dinos/allo/allo_lod1.skin");
        set_default(opt.anim_path,
                    "Data/Graphics/Actors/Dinos/animations/HeavyBiped/"
                    "HeavyBiped_Walk.anim");
        set_default(opt.dino_texture_path,
                    "Data/Graphics/Textures/actors/dinos/allo.tga");
    }
    lod_quality = 1.0f;
    {
        std::string setting = opt.lod_quality;
        if (setting.empty()) {
            const std::string ini_path =
                resolve_asset(opt.asset_root, "Data/Config/Dino.ini");
            if (std::filesystem::exists(ini_path)) {
                try {
                    const auto ini = dino::load_ini(ini_path);
                    if (const auto* graphic = ini.find("GRAPHIC")) {
                        setting = graphic->get("LODQuality");
                    }
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "Dino.ini: %s\n", e.what());
                }
            }
        }
        lod_quality = dino::landscape_lod_quality(setting);
    }
    veg_stats = true;  // print the first frame's LOD split;
    water_z = 0.0f;
    water_scroll = opt.water_time > 0.0f ? opt.water_time : 0.0f;
    port_shows = parse_show_specs(opt.show_specs, opt.asset_root);
    hud_font_ready = false;
    build_ready = false;
    return std::nullopt;
}

std::optional<int> Game::boot_load_assets() {
    try {
        if (!opt.mesh_path.empty()) {
            mesh = dino::load_mesh(opt.mesh_path);
        }
        if (!opt.lnd_path.empty()) {
            lnd = dino::load_lnd(opt.lnd_path);
            int hm_width = 0, hm_height = 0;
            const auto heights = dino::load_raw_u8(
                resolve_asset(opt.asset_root, lnd.heightmap), hm_width, hm_height,
                true);
            terrain = dino::build_terrain(heights, hm_width, hm_height,
                                          opt.terrain_size, opt.height_scale);
            try {
                int occ_w = 0, occ_h = 0;
                terrain.occupation = dino::load_raw_u8(
                    resolve_asset(opt.asset_root, lnd.occupation), occ_w, occ_h,
                    true);
                terrain.occupation_width = occ_w;
                terrain.occupation_height = occ_h;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "occupation mask: %s\n", e.what());
            }
            if (opt.texture_path.empty() && !lnd.basemap.empty()) {
                image = dino::load_tga(resolve_asset(opt.asset_root, lnd.basemap));
            }
            if (!opt.terrain_lit && !lnd.detailmap.empty()) {
                try {
                    detail_image = dino::load_tga(
                        resolve_asset(opt.asset_root, lnd.detailmap));
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "detail map: %s\n", e.what());
                }
            }
            if (!opt.no_vegetation) {
                vegetation = dino::load_vegetation(opt.asset_root, lnd, terrain);
            }
            if (opt.build_mode) {
                build_grid.init(opt.build_cell, terrain.world_size);
                build_grid.occupancy.init_from_mask(
                    terrain.occupation, terrain.occupation_width,
                    terrain.occupation_height);
                if (!opt.build_load_path.empty()) {
                    dino::load_build(build_grid, opt.build_load_path, terrain.world_size);
                }
                for (const auto& value : opt.place_paths) {
                    int x = 0, y = 0, edge = 0;
                    if (std::sscanf(value.c_str(), "%d,%d,%d", &x, &y, &edge) >= 2) {
                        const float cs = build_grid.cell_size;
                        if (!terrain.buildable(
                                build_grid.origin_x +
                                    (static_cast<float>(x) + 0.5f) * cs,
                                build_grid.origin_y +
                                    (static_cast<float>(y) + 0.5f) * cs)) {
                            std::fprintf(stderr,
                                         "place-path: blocked cell %d,%d\n", x,
                                         y);
                            continue;
                        }
                        build_grid.set_path(x, y, true);
                    } else {
                        std::fprintf(stderr, "bad --place-path %s\n", value.c_str());
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
                        // Edge 0 is the south edge (shared with y-1); edge 1
                        // is the east edge (shared with x+1). Out-of-bounds
                        // neighbours are allowed so a boundary edge can still
                        // be placed.
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
                                         "place-fence: blocked cell %d,%d\n", x,
                                         y);
                            continue;
                        }
                        build_grid.set_fence(x, y, edge, true);
                    } else {
                        std::fprintf(stderr, "bad --place-fence %s\n", value.c_str());
                    }
                }
                build_grid.undo_stack.clear();
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

                if (opt.visitors > 0 || !opt.load_port_path.empty()) {
                    if (visitor_profiles.empty()) {
                        try {
                            visitor_profiles =
                                load_easy_visitor_profiles(opt.asset_root);
                        } catch (const std::exception& e) {
                            std::fprintf(stderr, "visitor profiles: %s\n",
                                         e.what());
                        }
                    }
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
                    visitor_pose1 = dino::load_mesh(pose1);
                    visitor_pose2 = dino::load_mesh(pose2);
                    visitor_image = dino::load_image(texture);
                    const bool defer_visitors =
                        opt.wld_park || !opt.sav_park_path.empty();
                    if (!defer_visitors) {
                        const int spawned =
                            visitor_profiles.empty()
                                ? visitor_system.spawn(build_grid,
                                                       opt.visitors)
                                : visitor_system.spawn(
                                      build_grid, opt.visitors,
                                      visitor_profiles, -1, -1);
                        std::printf(
                            "visitors: %d spawned on %d path cells\n",
                            spawned, build_grid.path_count);
                        if (opt.visitor_satisfaction >= 0) {
                            for (dino::Visitor& visitor :
                                 visitor_system.visitors) {
                                visitor.need0 = std::clamp(
                                    opt.visitor_satisfaction, 0, 99);
                            }
                        }
                    }
                }
            }
            const std::string sky_dir =
                opt.skybox_dir.empty()
                    ? resolve_asset(opt.asset_root, lnd.skybox)
                    : opt.skybox_dir;
            static const char* kFaceNames[6] = {"front", "back", "left",
                                                "right", "top", "bottom"};
            for (int i = 0; i < 6; ++i) {
                try {
                    sky_images[i] = dino::load_image(
                        sky_dir + "/" + kFaceNames[i] + ".PNG");
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "skybox face %s: %s\n", kFaceNames[i],
                                 e.what());
                }
            }
            water_z = opt.water_level >= 0.0f ? opt.water_level : lnd.unknown_float;
            if (opt.water && !lnd.water.empty()) {
                try {
                    water_image = dino::load_image(resolve_asset(opt.asset_root, lnd.water));
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "water texture: %s\n", e.what());
                }
            }
            if (opt.water && opt.water2 && !lnd.water2.empty()) {
                try {
                    water2_image = dino::load_image(
                        resolve_asset(opt.asset_root, lnd.water2));
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "water2 texture: %s\n", e.what());
                }
            }
        }
        if (!opt.texture_path.empty()) {
            image = dino::load_tga(opt.texture_path);
        }
        if (!opt.dino_texture_path.empty()) {
            dino_image = dino::load_tga(opt.dino_texture_path);
        }
        if (!opt.skel_path.empty()) {
            skeleton = dino::load_skel(opt.skel_path);
        }
        if (!opt.skin_path.empty()) {
            if (opt.mesh_path.empty()) {
                throw std::runtime_error("--skin requires --mesh so the full-mesh block name is known");
            }
            if (opt.skel_path.empty()) {
                throw std::runtime_error("--skin requires --skel");
            }
            skin = dino::load_skin(opt.skin_path, mesh.name);
            // The .skin full mesh is stored in a crouched bind pose. The
            // inverse-bind matrices are inverse(skeleton bind), so any geometry
            // can be skinned with them; using the .mesh restores the standing
            // pose while keeping the same weights.
            if (!opt.skin_bind_from_skin && !mesh.positions.empty()) {
                if (skin.mesh.positions.size() == mesh.positions.size() &&
                    skin.mesh.indices == mesh.indices) {
                    for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
                        skin.mesh.positions[i] = {mesh.positions[i].x * 100.0f,
                                                  mesh.positions[i].y * 100.0f,
                                                  mesh.positions[i].z * 100.0f};
                    }
                    if (mesh.uvs.size() == mesh.positions.size()) {
                        skin.mesh.uvs = mesh.uvs;
                    }
                    if (mesh.normals.size() == mesh.positions.size()) {
                        skin.mesh.normals = mesh.normals;
                    }
                    if (mesh.colors.size() == mesh.positions.size()) {
                        skin.mesh.colors = mesh.colors;
                    }
                } else {
                    std::fprintf(stderr,
                                 "warning: .skin topology differs from .mesh; "
                                 "using the .skin bind pose\n");
                }
            }
        }
        if (!opt.anim_path.empty()) {
            if (opt.skel_path.empty()) {
                throw std::runtime_error("--anim requires --skel");
            }
            anim = dino::load_anim(opt.anim_path);
        }
        if (!opt.wav_path.empty()) {
            wav = dino::load_wav(opt.wav_path);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "load error: %s\n", e.what());
        return 1;
    }
    return std::nullopt;
}

std::optional<int> Game::boot_build_textures() {
    if (build_ready) {
        const std::string path_texture =
            opt.path_texture_path.empty()
                ? resolve_asset(opt.asset_root,
                                "Data/Graphics/Textures/buildings/ground.tga")
                : opt.path_texture_path;
        try {
            path_image = dino::load_tga(path_texture);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "path texture: %s\n", e.what());
        }
        std::string fence_texture = opt.fence_texture_path;
        if (fence_texture.empty() && !fence_source.texture_name().empty()) {
            fence_texture = resolve_asset(
                opt.asset_root,
                "Data/Graphics/Textures/buildings/" + fence_source.texture_name());
        }
        if (!fence_texture.empty()) {
            try {
                fence_image = dino::load_tga(fence_texture);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "fence texture: %s\n", e.what());
            }
        }
        // Fence.bwi texture slots (verified in the .bwi string table):
        // material 0 wall textures, mesh slot 1 fence_paddock_lod1.mesh,
        // material 1 laser2.tga, ground slot 1 boue_pad.tga and slot 2
        // ground_occup_pad.tga (0x53d0b0/0x5ec7e0 bind them as stages 0/1).
        load_asset_tga(opt.asset_root,
                       "Data/Graphics/Textures/buildings/laser2.tga",
                       fence_laser_image, "fence laser texture");
        load_asset_tga(opt.asset_root,
                       "Data/Graphics/Textures/buildings/boue_pad.tga",
                       paddock_dirt_image, "paddock ground texture");
        load_asset_tga(opt.asset_root,
                       "Data/Graphics/Textures/buildings/ground_occup_pad.tga",
                       paddock_occup_image, "paddock occupancy texture");
    }

    if (opt.hud) {
        const std::string font_path =
            opt.hud_font_path.empty()
                ? resolve_asset(opt.asset_root, "Data/Fonte/arial12.fnt")
                : opt.hud_font_path;
        try {
            hud_font = dino::load_fnt(font_path);
            hud_font_ready = true;
            std::printf("hud font %s: %zu glyphs\n", font_path.c_str(),
                        hud_font.glyphs.size());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "hud font: %s\n", e.what());
        }
    }
    return std::nullopt;
}

std::optional<int> Game::boot_print_modes() {
    if (opt.validate) {
        if (!island_names.empty()) {
            std::printf("islands: %zu %s..%s\n", island_names.size(),
                        island_names.front().c_str(), island_names.back().c_str());
        }
        std::printf("free game budgets: %d/%d/%d\n", free_game_budgets[0],
                    free_game_budgets[1], free_game_budgets[2]);
        if (opt.use_menu) {
            std::printf("menu: loaded=%d widgets=%zu pages=%zu graphics=%zu\n",
                        menu_loaded ? 1 : 0, menu.overlay.widgets.size(),
                        menu.overlay.pages.size(), menu.overlay.graphics.size());
            if (menu_loaded) {
                std::printf("menu page=%d active=%d island=%d difficulty=%d\n",
                            menu.page, menu.active ? 1 : 0, menu.island,
                            menu.difficulty);
            }
            if (!menu_loaded) {
                std::printf("menu error: %s\n", menu.error.c_str());
            }
        }
        if (opt.park_hud) {
            int visible = 0;
            for (int i = 0; i < static_cast<int>(park.overlay.widgets.size()); ++i) {
                if (park.overlay.widgets[static_cast<std::size_t>(i)].name !=
                        "RENDERVIEW" &&
                    park.overlay.visible_in_tree(i)) {
                    ++visible;
                }
            }
            std::printf("park hud: loaded=%d widgets=%zu roots=%zu visible=%d\n",
                        park.loaded() ? 1 : 0, park.overlay.widgets.size(),
                        park.overlay.roots.size(), visible);
        }
        if (!opt.mesh_path.empty()) {
            std::printf("mesh %s: %zu vertices, %zu indices, %zu triangles, "
                        "%zu uvs, %zu normals, %zu colors\n",
                        mesh.name.c_str(), mesh.positions.size(), mesh.indices.size(),
                        mesh.triangle_count(), mesh.uvs.size(), mesh.normals.size(),
                        mesh.colors.size());
        }
        if (!opt.texture_path.empty()) {
            std::printf("texture %s: %dx%d RGBA\n", opt.texture_path.c_str(),
                        image.width, image.height);
        }
        if (!opt.dino_texture_path.empty()) {
            std::printf("dino texture %s: %dx%d RGBA\n", opt.dino_texture_path.c_str(),
                        dino_image.width, dino_image.height);
        }
        if (!opt.skel_path.empty()) {
            std::printf("skel %s: %zu bones\n", opt.skel_path.c_str(), skeleton.bones.size());
        }
        if (!opt.skin_path.empty()) {
            std::size_t assigned = 0;
            for (const auto& bone : skin.bones) {
                assigned += bone.vertices.size();
            }
            std::printf("skin %s: %zu vertices, %zu bones, %zu assignments (%s bind)\n",
                        opt.skin_path.c_str(), skin.mesh.positions.size(),
                        skin.bones.size(), assigned,
                        opt.skin_bind_from_skin ? ".skin" : ".mesh");
        }
        if (!opt.anim_path.empty()) {
            std::size_t keys = 0;
            for (const auto& bone : anim.bones) {
                keys += bone.times.size();
            }
            std::printf("anim %s: %zu bones, %u duration, %zu keys\n",
                        opt.anim_path.c_str(), anim.bones.size(), anim.duration, keys);
        }
        if (!opt.wav_path.empty()) {
            std::printf("wav %s: %u ch, %u Hz, %u-bit, %u frames, %.3fs\n",
                        opt.wav_path.c_str(), wav.channels, wav.sample_rate,
                        wav.bits_per_sample, wav.frames, wav.duration);
        }
        if (!opt.lnd_path.empty()) {
            std::printf("lnd %s: %s, %ux%u, float=%.1f, music=%s, vegetation=%zu\n",
                        opt.lnd_path.c_str(), lnd.name_key.c_str(), lnd.width, lnd.height,
                        lnd.unknown_float, lnd.music.c_str(), lnd.vegetation.size());
            int sky_faces = 0;
            for (const auto& face : sky_images) {
                if (face.width > 0) {
                    ++sky_faces;
                }
            }
            std::printf("environment: skybox %d/6 faces, water %s at z=%.1f",
                        sky_faces, water_image.width > 0 ? "yes" : "no", water_z);
            if (water_image.width > 0) {
                std::printf(" (%dx%d)", water_image.width, water_image.height);
            }
            if (water2_image.width > 0) {
                std::printf(" water2 %dx%d", water2_image.width,
                            water2_image.height);
            }
            std::printf("\n");
            std::printf("terrain: %dx%d heights %u..%u, %zu vertices, %zu triangles\n",
                        terrain.width, terrain.height, terrain.min_height,
                        terrain.max_height, terrain.mesh.positions.size(),
                        terrain.mesh.triangle_count());
            if (!opt.no_vegetation) {
                std::printf("vegetation: %zu groups, %zu instances, %zu triangles\n",
                            vegetation.entries, vegetation.instances,
                            vegetation.triangles);
                std::printf("vegetation lod: %zu sector batches, lod2 %zu "
                            "triangles, quality %.1f\n",
                            vegetation.groups.size(), vegetation.lod2_triangles,
                            lod_quality);
                for (const auto& warning : vegetation.warnings) {
                    std::printf("vegetation warning: %s\n", warning.c_str());
                }
            }
        }
        if (!opt.skin_path.empty() && !opt.skel_path.empty()) {
            std::vector<int> vertex_bone;
            std::vector<int> bone_skel;
            build_skin_mapping(skin, skeleton, vertex_bone, bone_skel);
            const dino::Animation* anim_ptr = anim.bones.empty() ? nullptr : &anim;
            const double t = opt.anim_time >= 0.0 ? opt.anim_time : 0.0;
            const dino::Mesh bind = build_skinned_mesh(skin.mesh, skin, skeleton,
                                                       nullptr, 0.0, vertex_bone, bone_skel,
                                                       opt.skin_bind_from_skin);
            const dino::Mesh pose = build_skinned_mesh(skin.mesh, skin, skeleton,
                                                       anim_ptr, t, vertex_bone, bone_skel,
                                                       opt.skin_bind_from_skin);
            auto print_bounds = [](const char* label, const dino::Mesh& m) {
                dino::Vec3 lo{1e30f, 1e30f, 1e30f};
                dino::Vec3 hi{-1e30f, -1e30f, -1e30f};
                for (const auto& p : m.positions) {
                    lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
                    hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
                }
                std::printf("%s bounds: (%.3f %.3f %.3f) .. (%.3f %.3f %.3f)\n",
                            label, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
            };
            print_bounds("skin bind", bind);
            print_bounds("skin pose", pose);
            float rest_error = 0.0f;
            for (std::size_t i = 0; i < bind.positions.size(); ++i) {
                const dino::Vec3& a = bind.positions[i];
                const dino::Vec3& b = skin.mesh.positions[i];
                rest_error = std::max(
                    rest_error,
                    std::sqrt((a.x * 100.0f - b.x) * (a.x * 100.0f - b.x) +
                              (a.y * 100.0f - b.y) * (a.y * 100.0f - b.y) +
                              (a.z * 100.0f - b.z) * (a.z * 100.0f - b.z)));
            }
            std::printf("skin rest max error: %.4f cm\n", rest_error);
        }
        if (build_ready) {
            const dino::BuildMeshes meshes =
                dino::build_placement_meshes(build_grid, terrain, fence_source);            std::printf("build: cell %.1f, grid %dx%d, %d path cells, %d fences; "
                        "mesh %d tiles, %d segments\n",
                        build_grid.cell_size, build_grid.width, build_grid.height,
                        build_grid.path_count, build_grid.fence_count,
                        meshes.path_tiles, meshes.fence_segments);
            if (!visitor_system.visitors.empty()) {
                std::printf("visitors: %zu\n", visitor_system.visitors.size());
            }
            if (!opt.build_save_path.empty()) {
                dino::save_build(build_grid, opt.build_save_path);
                std::printf("build saved: %s\n", opt.build_save_path.c_str());
            }
        }
        return 0;
    }
    return std::nullopt;
}

std::optional<int> Game::boot_platform() {
    init_flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    if (!opt.wav_path.empty() || opt.ui_sounds) {
        init_flags |= SDL_INIT_AUDIO;
    }
    if (!SDL_Init(init_flags)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    ui_sound.asset_root = opt.asset_root;
    ui_sound.enabled = opt.ui_sounds;
    ui_sound.muted = opt.muted;
    ui_sound.set_gain(menu_loaded
                          ? std::clamp(menu.sfx_volume / 100.0f, 0.0f, 1.0f)
                          : 1.0f);
    if (!opt.play_sound.empty()) {
        const auto sound_it = menu_sound_files.find(opt.play_sound);
        if (sound_it != menu_sound_files.end()) {
            ui_sound.play(sound_it->second);
        } else {
            std::fprintf(stderr, "unknown sound: %s\n", opt.play_sound.c_str());
        }
    }
    music.asset_root = opt.asset_root;
    music.enabled = opt.music;
    music.set_gain(std::clamp(
        (menu_loaded ? static_cast<float>(menu.music_volume)
                     : opt.music_volume) /
            100.0f,
        0.0f, 1.0f));
    if (opt.music && !opt.lnd_path.empty() && !lnd.music.empty()) {
        const auto music_it = park_sound_files.find(lnd.music);
        if (music_it != park_sound_files.end()) {
            music.play(music_it->second);
        }
    } else if (opt.music && menu_loaded) {
        const auto music_it = menu_sound_files.find("music");
        if (music_it != menu_sound_files.end()) {
            music.play(music_it->second);
        }
    }
    apply_menu_options();

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (opt.fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (opt.borderless) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }
    window = SDL_CreateWindow(
        opt.title.c_str(), opt.width, opt.height, window_flags);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_StartTextInput(window);
    context = SDL_GL_CreateContext(window);
    if (!context) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(opt.vsync ? 1 : 0);
    if (!glfns.load()) {
        std::fprintf(stderr, "required OpenGL 3.3 functions are missing\n");
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    program = build_program(glfns);
    if (!program) {
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // Alpha test is off unless a draw item asks for it (uniforms start at 0).
    glfns.UseProgram(program);
    glfns.Uniform1f(glfns.GetUniformLocation(program, "u_alpha_ref"), -1.0f);
    glfns.UseProgram(0);

    audio = nullptr;
    if (!wav.data.empty()) {
        SDL_AudioSpec spec{};
        spec.format = wav.bits_per_sample == 16 ? SDL_AUDIO_S16 : SDL_AUDIO_U8;
        spec.channels = wav.channels;
        spec.freq = static_cast<int>(wav.sample_rate);
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                          nullptr, nullptr);
        if (audio != nullptr) {
            SDL_SetAudioStreamGain(audio, opt.muted ? 0.0f : 1.0f);
            SDL_ResumeAudioStreamDevice(audio);
            SDL_PutAudioStreamData(audio, wav.data.data(),
                                   static_cast<int>(wav.data.size()));
            std::printf("audio: %s, %u Hz, %u ch, %.3fs%s\n",
                        opt.wav_path.c_str(), wav.sample_rate, wav.channels,
                        wav.duration, opt.loop_audio ? " (loop)" : "");
        } else {
            std::fprintf(stderr, "audio open failed: %s\n", SDL_GetError());
        }
    }
    return std::nullopt;
}

}  // namespace dino::app
