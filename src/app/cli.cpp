// Command line parsing and config persistence (see cli.h).
//
// Moved verbatim out of src/app/main.cpp during the app-shell split. Keeping
// the help text next to the parser is deliberate: adding a flag means editing
// one file.

#include "cli.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

void load_config(const std::string& path, Options& opt) {
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const int value = std::atoi(line.c_str() + eq + 1);
        if (key == "width" && value >= 64) {
            opt.width = value;
        } else if (key == "height" && value >= 64) {
            opt.height = value;
        } else if (key == "fullscreen") {
            opt.fullscreen = value != 0;
        } else if (key == "borderless") {
            opt.borderless = value != 0;
        } else if (key == "vsync") {
            opt.vsync = value != 0;
        } else if (key == "music_volume") {
            opt.music_volume = static_cast<float>(value);
        } else if (key == "sfx_volume") {
            opt.sfx_volume = static_cast<float>(value);
        } else if (key == "game_speed") {
            opt.game_speed = value;
        } else if (key == "lod_quality") {
            opt.menu_lod_quality = value;
        } else if (key == "texture_quality") {
            opt.texture_quality = value;
        } else if (key == "texture_depth") {
            opt.texture_depth = value == 32 ? 32 : 16;
        } else if (key == "hardware_tnl") {
            opt.hardware_tnl = value != 0;
        } else if (key == "hardware_sound") {
            opt.hardware_sound = value != 0;
        }
    }
}

void save_config(const std::string& path, const Options& opt) {
    std::ofstream out(path);
    if (!out) {
        return;
    }
    out << "width=" << opt.width << "\n";
    out << "height=" << opt.height << "\n";
    out << "fullscreen=" << (opt.fullscreen ? 1 : 0) << "\n";
    out << "borderless=" << (opt.borderless ? 1 : 0) << "\n";
    out << "vsync=" << (opt.vsync ? 1 : 0) << "\n";
    out << "music_volume=" << static_cast<int>(opt.music_volume) << "\n";
    out << "sfx_volume=" << static_cast<int>(opt.sfx_volume) << "\n";
    out << "game_speed=" << opt.game_speed << "\n";
    out << "lod_quality=" << opt.menu_lod_quality << "\n";
    out << "texture_quality=" << opt.texture_quality << "\n";
    out << "texture_depth=" << opt.texture_depth << "\n";
    out << "hardware_tnl=" << opt.hardware_tnl << "\n";
    out << "hardware_sound=" << opt.hardware_sound << "\n";
}

Options parse_args(int argc, char** argv) {
    Options opt;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--config") {
            opt.config_path = argv[i + 1];
        }
    }
    load_config(opt.config_path, opt);
    // Pre-rename config name: read the old file once; the next save writes
    // `dino_port.cfg`. Keeps an existing local setup (window, volumes, LOD).
    if (opt.config_path == "dino_port.cfg") {
        std::ifstream current(opt.config_path);
        if (!current) {
            load_config("dino_viewer.cfg", opt);
        }
    }
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s requires a value\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--width") {
            opt.width = std::atoi(next("--width"));
        } else if (arg == "--height") {
            opt.height = std::atoi(next("--height"));
        } else if (arg == "--frames") {
            opt.frames = std::atoi(next("--frames"));
        } else if (arg == "--screenshot") {
            opt.screenshot = next("--screenshot");
        } else if (arg == "--capture-dir") {
            opt.capture_dir = next("--capture-dir");
        } else if (arg == "--capture-prefix") {
            opt.capture_prefix = next("--capture-prefix");
        } else if (arg == "--title") {
            opt.title = next("--title");
        } else if (arg == "--mesh") {
            opt.mesh_path = next("--mesh");
        } else if (arg == "--texture") {
            opt.texture_path = next("--texture");
        } else if (arg == "--skel") {
            opt.skel_path = next("--skel");
        } else if (arg == "--skin") {
            opt.skin_path = next("--skin");
        } else if (arg == "--anim") {
            opt.anim_path = next("--anim");
        } else if (arg == "--wav") {
            opt.wav_path = next("--wav");
        } else if (arg == "--lnd") {
            opt.lnd_path = next("--lnd");
        } else if (arg == "--dino-texture") {
            opt.dino_texture_path = next("--dino-texture");
        } else if (arg == "--dino-pos") {
            const std::string value = next("--dino-pos");
            const auto comma = value.find(',');
            if (comma == std::string::npos) {
                std::fprintf(stderr, "--dino-pos expects x,y\n");
                std::exit(2);
            }
            opt.spawn_x = static_cast<float>(std::atof(value.substr(0, comma).c_str()));
            opt.spawn_y = static_cast<float>(std::atof(value.substr(comma + 1).c_str()));
        } else if (arg == "--dino-angle") {
            opt.dino_angle = static_cast<float>(std::atof(next("--dino-angle")));
        } else if (arg == "--asset-root") {
            opt.asset_root = next("--asset-root");
        } else if (arg == "--terrain-size") {
            opt.terrain_size = static_cast<float>(std::atof(next("--terrain-size")));
        } else if (arg == "--height-scale") {
            opt.height_scale = static_cast<float>(std::atof(next("--height-scale")));
        } else if (arg == "--no-vegetation") {
            opt.no_vegetation = true;
        } else if (arg == "--lod-quality") {
            opt.lod_quality = next("--lod-quality");
        } else if (arg == "--no-lod") {
            opt.no_lod = true;
        } else if (arg == "--no-cull") {
            opt.no_cull = true;
        } else if (arg == "--entrance-road") {
            opt.entrance_road = true;
        } else if (arg == "--terrain-lit") {
            opt.terrain_lit = true;
        } else if (arg == "--skin-bind") {
            const std::string value = next("--skin-bind");
            if (value == "skin") {
                opt.skin_bind_from_skin = true;
            } else if (value == "mesh") {
                opt.skin_bind_from_skin = false;
            } else {
                std::fprintf(stderr, "--skin-bind expects mesh or skin\n");
                std::exit(2);
            }
        } else if (arg == "--free-camera") {
            opt.free_camera = true;
        } else if (arg == "--build") {
            opt.build_mode = true;
        } else if (arg == "--no-build") {
            // --wld-park arms the path tool; tests that click the scene
            // (dino selection) clear it with this flag.
            opt.build_mode = false;
        } else if (arg == "--build-cell") {
            opt.build_cell = static_cast<float>(std::atof(next("--build-cell")));
        } else if (arg == "--path-texture") {
            opt.path_texture_path = next("--path-texture");
        } else if (arg == "--fence-mesh") {
            opt.fence_mesh_path = next("--fence-mesh");
        } else if (arg == "--fence-texture") {
            opt.fence_texture_path = next("--fence-texture");
        } else if (arg == "--build-load") {
            opt.build_load_path = next("--build-load");
            opt.build_mode = true;
        } else if (arg == "--build-save") {
            opt.build_save_path = next("--build-save");
            opt.build_mode = true;
        } else if (arg == "--save-port") {
            opt.save_port_path = next("--save-port");
            opt.build_mode = true;
        } else if (arg == "--load-port") {
            opt.load_port_path = next("--load-port");
            opt.build_mode = true;
        } else if (arg == "--save-name") {
            opt.save_name = next("--save-name");
        } else if (arg == "--building") {
            opt.building_path = next("--building");
        } else if (arg == "--wld") {
            opt.wld_path = next("--wld");
        } else if (arg == "--wld-park") {
            opt.wld_path = next("--wld-park");
            opt.wld_park = true;
            opt.build_mode = true;
        } else if (arg == "--sav-park") {
            opt.sav_park_path = next("--sav-park");
            opt.wld_path = opt.sav_park_path;
            opt.build_mode = true;
        } else if (arg == "--sav-roundtrip") {
            opt.sav_roundtrip_path = next("--sav-roundtrip");
        } else if (arg == "--sav-info") {
            opt.sav_info_path = next("--sav-info");
        } else if (arg == "--sav-write") {
            opt.sav_roundtrip_path = next("--sav-write");
            opt.sav_write_path = next("--sav-write output");
        } else if (arg == "--sav-name") {
            opt.sav_name = next("--sav-name");
        } else if (arg == "--sav-slot") {
            opt.sav_slot = std::atoi(next("--sav-slot"));
        } else if (arg == "--sav-budget") {
            opt.sav_budget = std::atoi(next("--sav-budget"));
        } else if (arg == "--sav-force") {
            opt.sav_force = true;
        } else if (arg == "--wld-info") {
            opt.wld_info = true;
        } else if (arg == "--no-import-dinos") {
            opt.import_dinos = false;
        } else if (arg == "--dino-clip") {
            opt.dino_clip = next("--dino-clip");
        } else if (arg == "--bai-info") {
            opt.bai_info = true;
        } else if (arg == "--actions-bad-info") {
            opt.actions_bad_info = next("--actions-bad-info");
        } else if (arg == "--market-info") {
            opt.market_info = true;
        } else if (arg == "--store-info") {
            opt.store_ini_path = next("--store-info");
        } else if (arg == "--food-info") {
            opt.food_info_dir = next("--food-info");
        } else if (arg == "--campaign-info") {
            opt.campaign_info_dir = next("--campaign-info");
        } else if (arg == "--campaign-coverage") {
            opt.campaign_coverage_dir = next("--campaign-coverage");
        } else if (arg == "--video-info") {
            opt.video_info_path = next("--video-info");
        } else if (arg == "--video-frame") {
            opt.video_frame_path = next("--video-frame");
        } else if (arg == "--video-frame-index") {
            opt.video_frame_index = std::atoi(next("--video-frame-index"));
        } else if (arg == "--video-out") {
            opt.video_out_path = next("--video-out");
        } else if (arg == "--play-video") {
            opt.video_play_path = next("--play-video");
        } else if (arg == "--video-play-frames") {
            opt.video_play_frames =
                std::atoi(next("--video-play-frames"));
        } else if (arg == "--video-play-start") {
            opt.video_play_start =
                std::atoi(next("--video-play-start"));
        } else if (arg == "--campaign-mission") {
            opt.campaign_mission_path = next("--campaign-mission");
        } else if (arg == "--dino-dat") {
            opt.dino_dat_path = next("--dino-dat");
        } else if (arg == "--campaign-progress") {
            opt.campaign_progress_path = next("--campaign-progress");
        } else if (arg == "--auto-continue") {
            opt.auto_continue = true;
        } else if (arg == "--start-mission") {
            opt.start_mission_path = next("--start-mission");
        } else if (arg == "--place-food") {
            opt.place_food = next("--place-food");
        } else if (arg == "--food-pos") {
            opt.food_pos = next("--food-pos");
        } else if (arg == "--place-lunchbox") {
            opt.place_lunchbox = true;
        } else if (arg == "--fence-lunchbox") {
            // Test hook for the paddock panel's SWITCH_FENCE_LUNCHBOX:
            // arm feature mode; the next scene click places the dish.
            opt.fence_lunchbox = true;
        } else if (arg == "--lunchbox-prefill") {
            // Test hook: place N dishes in a 5-unit grid in the first
            // paddock so the 16-dish cap can be exercised.
            opt.lunchbox_prefill = std::atoi(next("--lunchbox-prefill"));
        } else if (arg == "--lunchbox-pos") {
            opt.lunchbox_pos = next("--lunchbox-pos");
        } else if (arg == "--place-bait") {
            // Test hook: place a StaticBait feature (for example
            // StaticBaitFlower) through the generic feature path.
            opt.place_bait = next("--place-bait");
        } else if (arg == "--bait-pos") {
            opt.bait_pos = next("--bait-pos");
        } else if (arg == "--hire-nutritionist" ||
                   arg == "--hire-nutritionists") {
            opt.hire_nutritionists = std::atoi(next("--hire-nutritionist"));
        } else if (arg == "--fire-nutritionist" ||
                   arg == "--fire-nutritionists") {
            opt.fire_nutritionists = std::atoi(next("--fire-nutritionist"));
        } else if (arg == "--feed-now") {
            opt.feed_now = true;
        } else if (arg == "--employee-info") {
            opt.employee_info = true;
        } else if (arg == "--bwi-info") {
            opt.bwi_info = true;
        } else if (arg == "--show-info") {
            opt.show_info_path = next("--show-info");
        } else if (arg == "--show") {
            opt.show_specs.push_back(next("--show"));
        } else if (arg == "--place-show") {
            opt.place_show_specs.push_back(next("--place-show"));
        } else if (arg == "--show-place-pos") {
            opt.show_place_positions.push_back(next("--show-place-pos"));
        } else if (arg == "--show-participants") {
            opt.show_participant_specs.push_back(
                next("--show-participants"));
        } else if (arg == "--show-hour") {
            opt.show_hour_specs.push_back(next("--show-hour"));
        } else if (arg == "--show-iterations") {
            opt.show_iteration_specs.push_back(next("--show-iterations"));
        } else if (arg == "--show-duration") {
            opt.show_duration_minutes = static_cast<float>(
                std::atof(next("--show-duration")));
        } else if (arg == "--show-locked") {
            opt.show_locked = true;
        } else if (arg == "--show-status") {
            opt.show_status = true;
        } else if (arg == "--camera-target") {
            opt.camera_target = next("--camera-target");
        } else if (arg == "--camera-retail") {
            opt.camera_retail = true;
        } else if (arg == "--camera-orbit") {
            opt.camera_orbit = true;
        } else if (arg == "--camera-selftest") {
            opt.camera_selftest = true;
        } else if (arg == "--camera-hz") {
            opt.camera_hz = static_cast<float>(std::atof(next("--camera-hz")));
        } else if (arg == "--edge-scroll") {
            opt.edge_scroll = std::atoi(next("--edge-scroll"));
        } else if (arg == "--budget-info") {
            opt.budget_info = true;
        } else if (arg == "--dino-actions") {
            opt.dino_actions_info = true;
        } else if (arg == "--dino-action-loop") {
            opt.dino_action_loop = true;
        } else if (arg == "--dino-needs") {
            opt.dino_needs = next("--dino-needs");
        } else if (arg == "--dino-action-speed") {
            opt.dino_action_speed = static_cast<float>(
                std::atof(next("--dino-action-speed")));
        } else if (arg == "--dino-move-speed") {
            opt.dino_move_speed = static_cast<float>(
                std::atof(next("--dino-move-speed")));
        } else if (arg == "--fixed-dt") {
            opt.fixed_dt = static_cast<float>(
                std::atof(next("--fixed-dt")));
        } else if (arg == "--dino-threat") {
            opt.dino_threat = std::atoi(next("--dino-threat"));
        } else if (arg == "--dino-radius") {
            opt.dino_radius = std::atoi(next("--dino-radius"));
        } else if (arg == "--dino-health") {
            opt.dino_health_override = true;
            std::sscanf(next("--dino-health"), "%d,%d", &opt.dino_health,
                        &opt.dino_max_health);
        } else if (arg == "--dino-food") {
            opt.dino_food = static_cast<float>(
                std::atof(next("--dino-food")));
        } else if (arg == "--sell-dino") {
            opt.sell_dino = next("--sell-dino");
        } else if (arg == "--dino-info") {
            opt.dino_info = next("--dino-info");
        } else if (arg == "--dino-panel") {
            opt.dino_panel = next("--dino-panel");
        } else if (arg == "--secu-probe") {
            opt.secu_probe = true;
        } else if (arg == "--rest-probe") {
            opt.rest_probe = true;
        } else if (arg == "--building-pos") {
            std::sscanf(next("--building-pos"), "%f,%f",
                        &opt.building_x, &opt.building_y);
        } else if (arg == "--building-angle") {
            opt.building_angle = static_cast<float>(
                std::atof(next("--building-angle")));
        } else if (arg == "--building-state") {
            opt.building_state = std::atoi(next("--building-state"));
        } else if (arg == "--building-scale") {
            opt.building_scale = static_cast<float>(
                std::atof(next("--building-scale")));
        } else if (arg == "--building-model") {
            opt.building_model_mesh =
                std::string(next("--building-model")) == "mesh";
        } else if (arg == "--focus-building") {
            opt.focus_building = true;
        } else if (arg == "--focus-dinos") {
            opt.focus_dinos = true;
        } else if (arg == "--focus-dino") {
            opt.focus_dinos = true;
            opt.focus_dino = std::atoi(next("--focus-dino"));
        } else if (arg == "--no-entrance") {
            opt.place_entrance = false;
        } else if (arg == "--construct") {
            opt.construct_name = next("--construct");
            opt.build_mode = true;
        } else if (arg == "--construct-pos") {
            opt.construct_positions.push_back(next("--construct-pos"));
            opt.build_mode = true;
        } else if (arg == "--road-click") {
            opt.road_clicks.push_back(next("--road-click"));
            opt.build_mode = true;
        } else if (arg == "--park-budget") {
            opt.park_budget_override = std::atoi(next("--park-budget"));
        } else if (arg == "--buy-dino") {
            opt.buy_dino = next("--buy-dino");
        } else if (arg == "--buy-paddock") {
            opt.buy_paddock = std::atoi(next("--buy-paddock"));
        } else if (arg == "--select-paddock") {
            opt.select_paddock = std::atoi(next("--select-paddock"));
        } else if (arg == "--settle-day") {
            opt.settle_day = true;
        } else if (arg == "--type-text") {
            opt.type_text = next("--type-text");
        } else if (arg == "--place-path") {
            opt.place_paths.push_back(next("--place-path"));
            opt.build_mode = true;
        } else if (arg == "--place-fence") {
            opt.place_fences.push_back(next("--place-fence"));
            opt.build_mode = true;
        } else if (arg == "--pick-test") {
            opt.pick_test = next("--pick-test");
        } else if (arg == "--no-input") {
            opt.no_input = true;
        } else if (arg == "--selftest") {
            opt.selftest = true;
        } else if (arg == "--no-hud") {
            opt.hud = false;
        } else if (arg == "--hud-font") {
            opt.hud_font_path = next("--hud-font");
        } else if (arg == "--no-menu") {
            opt.no_menu = true;
            opt.use_menu = false;
        } else if (arg == "--placeholder-menu") {
            opt.placeholder_menu = true;
            opt.use_menu = false;
        } else if (arg == "--menu") {
            opt.force_menu = true;
        } else if (arg == "--help-panel") {
            opt.force_help = true;
        } else if (arg == "--menu-page") {
            opt.menu_page = std::atoi(next("--menu-page"));
            opt.force_menu = true;
        } else if (arg == "--menu-click") {
            opt.menu_clicks.push_back(next("--menu-click"));
            opt.force_menu = true;
        } else if (arg == "--menu-loading-time") {
            opt.menu_loading_time = static_cast<float>(std::atof(next("--menu-loading-time")));
            opt.force_menu = true;
        } else if (arg == "--menu-hover") {
            opt.menu_hover = next("--menu-hover");
            opt.force_menu = true;
        } else if (arg == "--minimap-scale") {
            opt.minimap_scale = static_cast<float>(
                std::atof(next("--minimap-scale")));
        } else if (arg == "--park-time") {
            opt.park_time = next("--park-time");
        } else if (arg == "--park-time-scale") {
            opt.park_time_scale = static_cast<float>(
                std::atof(next("--park-time-scale")));
        } else if (arg == "--park-advance") {
            opt.park_advance = static_cast<float>(
                std::atof(next("--park-advance")));
        } else if (arg == "--start-dino") {
            opt.start_dino = true;
        } else if (arg == "--ui-sounds") {
            opt.ui_sounds = true;
        } else if (arg == "--no-sounds") {
            opt.ui_sounds = false;
        } else if (arg == "--list-sounds") {
            opt.list_sounds = true;
        } else if (arg == "--play-sound") {
            opt.play_sound = next("--play-sound");
        } else if (arg == "--music") {
            opt.music = true;
            opt.music_forced = true;
        } else if (arg == "--no-music") {
            opt.music = false;
            opt.music_forced = true;
        } else if (arg == "--music-volume") {
            opt.music_volume = static_cast<float>(
                std::atof(next("--music-volume")));
        } else if (arg == "--warning") {
            opt.warning_text = next("--warning");
        } else if (arg == "--request") {
            opt.request_text = next("--request");
        } else if (arg == "--campaign-end") {
            opt.campaign_end_text = next("--campaign-end");
        } else if (arg == "--no-park-hud") {
            opt.park_hud = false;
        } else if (arg == "--park-hover") {
            opt.park_hover = next("--park-hover");
        } else if (arg == "--park-click") {
            opt.park_clicks.push_back(next("--park-click"));
        } else if (arg == "--park-window-click") {
            opt.park_window_clicks.push_back(next("--park-window-click"));
        } else if (arg == "--road-cursor") {
            opt.road_cursor = next("--road-cursor");
        } else if (arg == "--fence-cursor") {
            opt.fence_cursor = next("--fence-cursor");
        } else if (arg == "--inject-click") {
            opt.inject_clicks.push_back(next("--inject-click"));
        } else if (arg == "--inject-key") {
            opt.inject_keys.push_back(next("--inject-key"));
        } else if (arg == "--ground-click") {
            opt.ground_click = next("--ground-click");
        } else if (arg == "--no-water") {
            opt.water = false;
        } else if (arg == "--no-water2") {
            opt.water2 = false;
        } else if (arg == "--water-time") {
            opt.water_time = static_cast<float>(std::atof(next("--water-time")));
        } else if (arg == "--pulse-time") {
            opt.pulse_time = static_cast<float>(std::atof(next("--pulse-time")));
        } else if (arg == "--water-level") {
            opt.water_level = static_cast<float>(std::atof(next("--water-level")));
        } else if (arg == "--skybox") {
            opt.skybox_dir = next("--skybox");
        } else if (arg == "--visitors") {
            opt.visitors = std::atoi(next("--visitors"));
        } else if (arg == "--visitor-satisfaction") {
            opt.visitor_satisfaction =
                std::atoi(next("--visitor-satisfaction"));
        } else if (arg == "--visitor-stats") {
            opt.visitor_stats = true;
        } else if (arg == "--visitor-pose1") {
            opt.visitor_pose1 = next("--visitor-pose1");
        } else if (arg == "--visitor-pose2") {
            opt.visitor_pose2 = next("--visitor-pose2");
        } else if (arg == "--visitor-texture") {
            opt.visitor_texture = next("--visitor-texture");
        } else if (arg == "--once") {
            opt.loop_audio = false;
        } else if (arg == "--mute") {
            opt.muted = true;
        } else if (arg == "--time") {
            opt.anim_time = std::atof(next("--time"));
        } else if (arg == "--speed") {
            opt.anim_speed = std::atof(next("--speed"));
        } else if (arg == "--yaw") {
            opt.yaw = static_cast<float>(std::atof(next("--yaw")));
        } else if (arg == "--pitch") {
            opt.pitch = static_cast<float>(std::atof(next("--pitch")));
        } else if (arg == "--distance") {
            opt.distance = static_cast<float>(std::atof(next("--distance")));
        } else if (arg == "--validate") {
            opt.validate = true;
        } else if (arg == "--fullscreen") {
            opt.fullscreen = true;
        } else if (arg == "--borderless") {
            opt.borderless = true;
        } else if (arg == "--windowed") {
            opt.fullscreen = false;
            opt.borderless = false;
        } else if (arg == "--config") {
            opt.config_path = next("--config");
        } else if (arg == "--keys") {
            opt.keys_path = next("--keys");
        } else if (arg == "--rebind") {
            opt.rebinds.emplace_back(next("--rebind"));
        } else if (arg == "--list-keys") {
            opt.list_keys = true;
        } else if (arg == "--no-save-keys") {
            opt.no_save_keys = true;
        } else if (arg == "--no-save-config") {
            opt.save_config = false;
        } else if (arg == "--vsync") {
            opt.vsync = true;
        } else if (arg == "--no-vsync") {
            opt.vsync = false;
        } else if (arg == "--help" || arg == "-h") {
            std::printf(
                "usage: dino_port [options]\n"
                "  --mesh FILE          retail .mesh to draw\n"
                "  --texture FILE       retail .tga texture\n"
                "  --skel FILE          retail .skel skeleton\n"
                "  --skin FILE          retail .skin (requires --mesh for the name)\n"
                "  --anim FILE          retail .anim clip\n"
                "  --wav FILE           retail .wav to play\n"
                "  --lnd FILE           island .lnd (terrain + basemap)\n"
                "  --dino-texture FILE  texture for the skinned mesh when --lnd is used\n"
                "  --dino-pos X,Y       place the skinned mesh on the terrain (0..1024)\n"
                "  --dino-angle DEG     Z rotation for the placed skinned mesh\n"
                "  --asset-root DIR     root for paths inside .lnd (default game/%%MAINDIR%%)\n"
                "  --terrain-size N     terrain world size (default 1024)\n"
                "  --height-scale N     heightmap multiplier (default 1)\n"
                "  --no-vegetation      skip the .imp/.blf placed objects\n"
                "  --lod-quality Q      HIGH/MEDIUM/LOW/AUTO vegetation LOD range\n"
                "                       (default: Dino.ini [GRAPHIC] LODQuality)\n"
                "  --no-lod             draw all vegetation at LOD1, no distance cut\n"
                "  --no-cull            draw vegetation batches outside the view too\n"
                "  --entrance-road      with --build, lay the retail start road from a\n"
                "                       --building Entrance (Free Game start does it)\n"
                "  --terrain-lit        legacy lit terrain without the retail detail map\n"
                "  --skin-bind MODE     mesh (default, .mesh rest pose) or skin\n"
                "  --free-camera        start in free-fly camera mode (C toggles)\n"
                "  --build              path/fence placement (1=path, 2=fence, 0=off, U=undo)\n"
                "  --build-cell N       grid cell size in world units (default 4)\n"
                "  --path-texture FILE  texture for path tiles (default ground.tga)\n"
                "  --fence-mesh FILE    fence segment mesh (default paddock fence)\n"
                "  --fence-texture FILE fence texture (default from the mesh material)\n"
                "  --build-load FILE    load placements\n"
                "  --build-save FILE    save placements on exit\n"
                "  --building FILE      place a .bbi building (static LOD1)\n"
                "  --building-pos X,Y   .imp-space position (default 520,376)\n"
                "  --building-angle DEG Z rotation (default 0)\n"
                "  --building-state N   orientation 0..3 (90-degree steps)\n"
                "  --building-scale N   scale (default 1)\n"
                "  --focus-building     frame the placed building\n"
                "  --focus-dinos        frame the first imported dinosaur\n"
                "  --focus-dino N       frame the Nth imported dinosaur\n"
                "  --no-entrance        do not auto-place the free-game Entrance\n"
                "  --construct NAME     select a building (kind name or CONSTRUC_*_BUTTON)\n"
                "  --construct-pos X,Y  place the selected building immediately (repeatable)\n"
                "  --show-place-pos X,Y place the armed show immediately (repeatable;\n"
                "                       arm it with PREDEFINED_SHOW_VALIDATE or --place-show)\n"
                "  --inject-click F,X,Y[,right]  push a real mouse click (window pixels)\n"
                "                       into the event queue at frame F (repeatable)\n"
                "  --inject-key F,KEY    push a key down/up pair at frame F (SDL keycode)\n"
                "  --road-click X,Y     road-tool ground click at a build cell (repeatable;\n"
                "                       arm it with --construct CONSTRUC_ROAD_BUTTON or the\n"
                "                       panel's Road button)\n"
                "  --road-cursor X,Y    pin the road preview cursor (window pixels; tests)\n"
                "  --fence-cursor X,Y   pin the enclosure preview cursor (window pixels; tests)\n"
                "  --park-budget N      override the starting park budget (tests/debug)\n"
                "  --buy-dino NAME      buy a Market.ini dinosaur into a paddock\n"
                "  --buy-paddock N      paddock index for --buy-dino (default first with room)\n"
                "  --select-paddock N   select paddock N (Dino_SelectedWall pulse)\n"
                "  --settle-day         run one daily settlement at startup (tests/debug)\n"
                "  --type-text TEXT     append TEXT to the Options save name (tests)\n"
                "  --wld FILE           read a .wld/.sav world block\n"
                "  --wld-park FILE      import roads/fences/buildings from a .wld\n"
                "  --sav-park FILE      import roads/fences/buildings/visitors/budget/clock from a .sav\n"
                "  --sav-info FILE      print a .sav header/world/simulation summary\n"
                "  --sav-roundtrip FILE rebuild a .sav and compare the whole file\n"
                "  --sav-write IN OUT   rebuild a .sav and write it to OUT\n"
                "  --sav-name NAME      save name for --sav-write (ASCII)\n"
                "  --sav-slot N         save slot for --sav-write\n"
                "  --sav-budget N       park budget for --sav-write\n"
                "  --sav-force          write even if the rebuild differs\n"
                "  --wld-info           print the world block and exit\n"
                "  --no-import-dinos    do not spawn the imported dinosaur genotypes\n"
                "  --dino-clip NAME     imported dinosaur clip (default Walk)\n"
                "  --bai-info           print the 64 action definitions and exit\n"
                "  --actions-bad-info FILE  print the Actions.bad family durations and exit\n"
                "  --market-info        print the dinosaur store catalog and exit\n"
                "  --store-info FILE    print a [DINOSTORE] INI (e.g. FreeGame.ini)\n"
                "  --food-info DIR      print Data/Design/foodsource and exit\n"
                "  --campaign-info DIR  scan Data/Design/Scenario campaigns/missions and exit\n"
                "  --campaign-coverage DIR  report implemented/pending objective verbs\n"
                "  --video-info FILE    probe one MPEG ending video and exit\n"
                "  --video-frame FILE   decode a video frame (with --video-frame-index/--video-out)\n"
                "  --play-video FILE    play an MPEG ending video in the window\n"
                "  --video-play-frames N  stop after N frames (test hook)\n"
                "  --video-play-start N  skip N frames before playback\n"
                "  --campaign-mission F print one mission's objectives and verb ids\n"
                "  --start-mission FILE run a campaign mission in the loaded park\n"
                "  --dino-dat FILE      campaign unlock count (default run/save/dino.dat)\n"
                "  --campaign-progress FILE  current mission per campaign\n"
                "  --auto-continue      advance past CPGEND without a click (test hook)\n"
                "  --place-food NAME    place a .bsi food source in the park\n"
                "  --food-pos X,Y       food source cell (default: first paddock)\n"
                "  --place-lunchbox     place a LunchBox item actor (herbivore)\n"
                "  --fence-lunchbox     arm the paddock Feeding Dish switch (next scene click places)\n"
                "  --lunchbox-prefill N place N test dishes in a 5-unit grid (cap test)\n"
                "  --lunchbox-pos X,Y   LunchBox cell (default: first paddock)\n"
                "  --place-bait NAME    place a StaticBait feature (generic .bfi path)\n"
                "  --bait-pos X,Y       StaticBait cell (default: first paddock)\n"
                "  --hire-nutritionist N hire N nutritionists (500 each)\n"
                "  --fire-nutritionist N dismiss N nutritionists (no refund)\n"
                "  --feed-now           run one hourly feeding pass at startup\n"
                "  --employee-info      print the .bei employee profiles and exit\n"
                "  --bwi-info           print the .bwi wall definitions and exit\n"
                "  --show-info PATH     print .shw show summaries and exit\n"
                "  --show NAME[:I,J]    place a show arena with dino participants\n"
                "  --place-show NAME[:X,Y] place a scheduled show (mission [SHOWS] catalog)\n"
                "  --show-participants NAME:I,J assign imported dinos to a placed show\n"
                "  --show-hour NAME:SLOT schedule a show at hour slot 1..10 (10:00..19:00)\n"
                "  --show-iterations NAME:N performances per day for a placed show\n"
                "  --show-duration MIN  performance length in park minutes (default 50)\n"
                "  --show-locked        place shows closed to the public\n"
                "  --show-status        print the attraction list rows after the first tick\n"
                "  --camera-target X,Y  screen-shot camera target in world units\n"
                "  --camera-retail      force the retail park camera (default)\n"
                "  --camera-orbit       use the legacy orbit camera\n"
                "  --camera-hz N        retail camera tick rate (default 60)\n"
                "  --edge-scroll N      mouse edge band in client px (default 2)\n"
                "  --budget-info        print the budget ledger (current/yesterday/totals)\n"
                "  --dino-actions       print the dinosaur action chooser and exit\n"
                "  --dino-action-loop   drive imported dinosaur clips from the chooser\n"
                "  --dino-needs F,V,S,R imported dinosaur FOOD,VIOLENCE,SECU,REPOS (default 50,99,99,99)\n"
                "  --dino-action-speed N action duration scale (default 1)\n"
                "  --dino-move-speed N  mover speed scale for tests (default 1)\n"
                "  --fixed-dt SECONDS   fixed simulation delta (headless tests)\n"
                "  --dino-threat N      danger-grid stamp value for every imported dinosaur\n"
                "  --dino-radius N      danger-grid stamp radius (world units)\n"
                "  --dino-health CUR,MAX health pair for the SECU/health tests (default phenotype)\n"
                "  --dino-food N        initial food gauge for imported dinosaurs (test hook)\n"
                "  --sell-dino NAME     sell the last imported dinosaur of NAME (test hook)\n"
                "  --no-build           clear the default build tool (scene picking tests)\n"
                "  --dino-info NAME     print one imported dinosaur's live panel values\n"
                "  --dino-panel NAME    open DINO_INFO_PANEL for the first NAME (or click a dinosaur)\n"
                "  --secu-probe         print each dinosaur's grid cell and SECU per rebuild\n"
                "  --rest-probe         print each dinosaur's rest gauge, REPOS, health and state per second\n"
                "  --save-port FILE     write the port park state\n"
                "  --load-port FILE     load a port park state at startup\n"
                "  --save-name NAME     Options Save target name (default quicksave)\n"
                "  --place-path X,Y     add a path cell (repeatable)\n"
                "  --place-fence X,Y,E  add a fence edge, E=0 south / 1 east (repeatable)\n"
                "  --pick-test X,Y      print the terrain cell under a pixel (first frame)\n"
                "  --no-input           ignore all input (benchmark/stability runs)\n"
                "  --selftest           run grid/visitor checks and exit\n"
                "  --no-hud             hide the on-screen HUD\n"
                "  --hud-font FILE      bitmap font for the HUD (default arial12.fnt)\n"
                "  --no-menu            skip the start screen\n"
                "  --menu               force the start screen (for screenshots)\n"
                "  --placeholder-menu   use the old placeholder start screen\n"
                "  --help-panel         show the controls panel at startup\n"
                "  --menu-page N        start the retail menu on page 0..8\n"
                "  --menu-click X,Y     click design-space point(s) before the first frame\n"
                "  --menu-loading-time N  seconds to show the loading page (default 0.8)\n"
                "  --menu-hover X,Y     set the hovered design-space point\n"
                "  --minimap-scale N    detail-map scale 0.1..2.0 (default 0.25)\n"
                "  --park-time HH:MM    set the park clock for screenshots/tests\n"
                "  --park-time-scale N  override the clock time scale (default 40)\n"
                "  --park-advance N     advance the clock N real seconds at startup\n"
                "  --start-dino         place an allosaurus at the menu-started park\n"
                "  --ui-sounds          enable menu/park click sounds in screenshots\n"
                "  --no-sounds          disable UI click sounds\n"
                "  --list-sounds        print the menu sound DB and exit\n"
                "  --play-sound NAME    play one MenuSoundDB entry at startup\n"
                "  --music              play the island theme (interactive default)\n"
                "  --no-music           disable island music\n"
                "  --music-volume N     music gain 0..100 (default 97)\n"
                "  --warning TEXT       show the retail warning dialog\n"
                "  --request TEXT       show the retail request dialog\n"
                "  --campaign-end TEXT  show the retail campaign-end dialog\n"
                "  --park-hover X,Y     set the park HUD hovered point\n"
                "  --park-click X,Y     click a park HUD point in design coordinates\n"
                "  --park-window-click X,Y  click a park HUD point in window coordinates\n"
                "  --no-park-hud        skip the ParkInterface.Ini HUD\n"
                "  --no-water           skip the water plane\n"
                "  --no-water2          skip the WaterSurface Modulate2x stage 1\n"
                "  --water-time SEC     initial water scroll time (debug/A-B)\n"
                "  --pulse-time SEC     freeze the fence/selection pulse (debug)\n"
                "  --water-level N      override the .lnd water level\n"
                "  --skybox DIR         skybox face directory (front/back/left/right/top/bottom.PNG)\n"
                "  --visitors N         spawn N visitors on the placed path network\n"
                "  --visitor-satisfaction N  initial visitor need0 (test/debug)\n"
                "  --visitor-stats      print visitor state/need summary at exit\n"
                "  --visitor-pose1 FILE walk pose A (default woman_walk1)\n"
                "  --visitor-pose2 FILE walk pose B (default woman_walk2)\n"
                "  --visitor-texture FILE visitor texture (default wom.tga)\n"
                "  --once               play the WAV once (default loops)\n"
                "  --mute               start muted\n"
                "  --time N             fixed animation time (default: animate)\n"
                "  --speed N            animation speed multiplier (default 1)\n"
                "  --width N --height N window size (default 1280x720)\n"
                "  --frames N           exit after N frames (0 = interactive)\n"
                "  --screenshot FILE.bmp save the last frame\n"
                "  --capture-dir DIR    save every frame (fixed animation step)\n"
                "  --capture-prefix P   frame filename prefix (default frame)\n"
                "  --yaw DEG --pitch DEG --distance N  camera\n"
                "  --fullscreen         start fullscreen\n"
                "  --borderless         borderless window\n"
                "  --windowed           force windowed mode\n"
                "  --config FILE        config file (default dino_port.cfg)\n"
                "  --keys FILE          key map (default dino_keys.cfg)\n"
                "  --rebind A=KEY       rebind one action (repeatable)\n"
                "  --list-keys          print the key map and exit\n"
                "  --no-save-keys       do not persist rebinds\n"
                "  --no-save-config     do not persist window settings\n"
                "  --validate           parse inputs and print counts, no window\n"
                "  --vsync / --no-vsync enable/disable vsync\n");
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
            std::exit(2);
        }
    }
    if (opt.width < 64 || opt.height < 64) {
        std::fprintf(stderr, "window must be at least 64x64\n");
        std::exit(2);
    }
    opt.pitch = std::clamp(opt.pitch, -89.0f, 89.0f);
    if (opt.distance > 0.0f) {
        opt.distance = std::clamp(opt.distance, 1.0f, 10000.0f);
    }
    return opt;
}

