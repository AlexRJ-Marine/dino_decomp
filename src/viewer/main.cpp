// Dino Island portable viewer - Phase 2.
//
// VIEW-1: SDL3 window + OpenGL context + input/timing/screenshot.
// VIEW-2: .mesh/.tga loaders, GLSL lit pipeline, orbit camera.

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "../formats/mesh.h"
#include "../formats/tga.h"
#include "../formats/skel.h"
#include "../formats/skin.h"
#include "../formats/anim.h"
#include "../formats/wav.h"
#include "../formats/lnd.h"
#include "../formats/image.h"
#include "../formats/fnt.h"
#include "../formats/sav.h"
#include "../formats/bbi.h"
#include "../formats/bvi.h"
#include "../formats/dino_data.h"
#include "../formats/campaign.h"
#include "../formats/wld.h"
#include "../formats/bai.h"
#include "../formats/market.h"
#include "../formats/bsi.h"
#include "../formats/bei.h"
#include "../formats/shw.h"
#include "../formats/mpg.h"
#include "../formats/bfi.h"
#include "../formats/general_params.h"
#include "camera_config.h"
#include "../world/terrain.h"
#include "../world/vegetation.h"
#include "../world/build.h"
#include "../world/food.h"
#include "../world/employees.h"
#include "../world/visitors.h"
#include "../world/dino_actions.h"
#include "../world/danger_grid.h"
#include "../world/campaign_objectives.h"
#include "../world/campaign_runtime.h"
#include "../world/show_runtime.h"
#include "../world/port_save.h"
#include "../world/economy.h"
#include "../ui/menu.h"
#include "../ui/park.h"
#include "../ui/ini.h"
#include "keymap.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct Options {
    int width = 1280;
    int height = 720;
    int frames = 0;
    bool vsync = true;
    bool validate = false;
    std::string title = "Dino Island - portable viewer";
    std::string screenshot;
    std::string capture_dir;
    std::string capture_prefix = "frame";
    std::string config_path = "dino_viewer.cfg";
    std::string keys_path = "dino_keys.cfg";
    std::vector<std::string> rebinds;
    bool list_keys = false;
    bool no_save_keys = false;
    bool fullscreen = false;
    bool borderless = false;
    bool save_config = true;
    std::string mesh_path;
    std::string texture_path;
    std::string skel_path;
    std::string skin_path;
    std::string anim_path;
    std::string wav_path;
    std::string lnd_path;
    std::string dino_texture_path;
    float spawn_x = -1.0f;
    float spawn_y = -1.0f;
    float dino_angle = 0.0f;
    std::string asset_root = "game/%MAINDIR%";
    float terrain_size = 1024.0f;
    float height_scale = 1.0f;
    bool loop_audio = true;
    bool muted = false;
    bool no_vegetation = false;
    bool skin_bind_from_skin = false;
    bool free_camera = false;
    bool build_mode = false;
    float build_cell = 4.0f;
    std::string path_texture_path;
    std::string fence_mesh_path;
    std::string fence_texture_path;
    std::string build_load_path;
    std::string build_save_path;
    std::string save_port_path;
    std::string load_port_path;
    std::string save_name = "quicksave";
    std::string building_path;
    std::string wld_path;
    std::string sav_park_path;
    std::string sav_roundtrip_path;
    std::string sav_info_path;
    std::string sav_write_path;
    std::string sav_name;
    int sav_slot = -1;
    int sav_budget = -1;
    bool sav_force = false;
    bool wld_info = false;
    bool wld_park = false;
    bool import_dinos = true;
    std::string dino_clip = "Walk";
    bool bai_info = false;
    bool market_info = false;
    std::string store_ini_path;
    std::string food_info_dir;
    std::string campaign_info_dir;
    std::string campaign_coverage_dir;
    std::string video_info_path;
    std::string video_frame_path;
    int video_frame_index = 0;
    std::string video_out_path;
    std::string video_play_path;
    int video_play_frames = 0;
    int video_play_start = 0;
    std::string campaign_mission_path;
    std::string dino_dat_path;  // campaign unlock mask (default run/save/dino.dat)
    std::string campaign_progress_path;  // current mission per campaign
    bool auto_continue = false;  // test hook: advance past CPGEND automatically
    std::string start_mission_path;
    std::string place_food;
    std::string food_pos;
    int hire_nutritionists = 0;
    int fire_nutritionists = 0;
    bool feed_now = false;
    bool employee_info = false;
    std::string show_info_path;
    std::vector<std::string> show_specs;  // --show NAME[:IDX,IDX]
    std::vector<std::string> place_show_specs;  // --place-show NAME[:X,Y]
    std::vector<std::string> show_place_positions;  // --show-place-pos X,Y
    std::vector<std::string> show_participant_specs;  // NAME:I,J
    std::vector<std::string> show_hour_specs;         // NAME:SLOT
    std::vector<std::string> show_iteration_specs;    // NAME:N
    float show_duration_minutes = 30.0f;              // --show-duration
    bool show_locked = false;                         // --show-locked
    bool show_status = false;                         // --show-status
    std::string camera_target;
    bool budget_info = false;
    bool dino_actions_info = false;
    bool dino_action_loop = false;
    std::string dino_needs = "50,99,99,99";
    float dino_action_speed = 1.0f;
    int dino_threat = 0;
    int dino_radius = 0;
    int dino_health = 100;
    int dino_max_health = 100;
    bool secu_probe = false;
    float building_x = 520.0f;
    float building_y = 376.0f;
    int building_state = 0;
    float building_angle = 0.0f;
    float building_scale = 1.0f;
    bool focus_building = false;
    bool building_model_mesh = false;
    bool focus_dinos = false;
    int focus_dino = 0;
    bool place_entrance = true;
    std::string construct_name;
    std::vector<std::string> construct_positions;
    std::vector<std::string> road_clicks;
    std::string buy_dino;
    int buy_paddock = -1;
    int park_budget_override = -1;
    bool settle_day = false;
    std::string type_text;
    std::vector<std::string> place_paths;
    std::vector<std::string> place_fences;
    std::string pick_test;
    bool no_input = false;
    bool selftest = false;
    bool hud = true;
    std::string hud_font_path;
    bool no_menu = false;
    bool force_menu = false;
    bool force_help = false;
    bool placeholder_menu = false;
    bool use_menu = true;
    int menu_page = -1;
    std::vector<std::string> menu_clicks;
    std::string menu_hover;
    float menu_loading_time = 0.8f;
    float minimap_scale = 0.25f;
    float park_time_scale = 0.0f;
    std::string park_time;
    float park_advance = 0.0f;
    bool start_dino = false;
    bool ui_sounds = true;
    bool list_sounds = false;
    std::string play_sound;
    bool music = false;
    bool music_forced = false;
    float music_volume = 97.0f;
    std::string warning_text;
    std::string request_text;
    std::string campaign_end_text;
    bool park_hud = true;
    std::string park_hover;
    std::vector<std::string> park_clicks;
    std::vector<std::string> park_window_clicks;
    std::string ground_click;  // scripted scene click, "x,y" window pixels
    std::vector<std::string> inject_clicks;  // "frame,x,y[,right]"
    std::string road_cursor;  // fixed road preview cursor "x,y" (tests)
    std::string fence_cursor;  // fixed enclosure preview cursor "x,y" (tests)
    std::string skybox_dir;
    bool water = true;
    bool water2 = true;  // WaterSurface stage 1 Modulate2x
    float water_level = -1.0f;
    float water_time = -1.0f;  // initial scroll time (debug/A-B)
    int visitors = 0;
    int visitor_satisfaction = -1;  // test/debug initial need0 override
    bool visitor_stats = false;
    std::string visitor_pose1;
    std::string visitor_pose2;
    std::string visitor_texture;
    double anim_time = -1.0;   // < 0 = animate with elapsed time
    double anim_speed = 1.0;   // 1.0 = 4800 time units per second
    float yaw = 35.0f;
    float pitch = 20.0f;
    float distance = 0.0f;
};

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
}

Options parse_args(int argc, char** argv) {
    Options opt;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--config") {
            opt.config_path = argv[i + 1];
        }
    }
    load_config(opt.config_path, opt);
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
        } else if (arg == "--dino-threat") {
            opt.dino_threat = std::atoi(next("--dino-threat"));
        } else if (arg == "--dino-radius") {
            opt.dino_radius = std::atoi(next("--dino-radius"));
        } else if (arg == "--dino-health") {
            std::sscanf(next("--dino-health"), "%d,%d", &opt.dino_health,
                        &opt.dino_max_health);
        } else if (arg == "--secu-probe") {
            opt.secu_probe = true;
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
        } else if (arg == "--ground-click") {
            opt.ground_click = next("--ground-click");
        } else if (arg == "--no-water") {
            opt.water = false;
        } else if (arg == "--no-water2") {
            opt.water2 = false;
        } else if (arg == "--water-time") {
            opt.water_time = static_cast<float>(std::atof(next("--water-time")));
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
                "usage: dino_viewer [options]\n"
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
                "  --road-click X,Y     road-tool ground click at a build cell (repeatable;\n"
                "                       arm it with --construct CONSTRUC_ROAD_BUTTON or the\n"
                "                       panel's Road button)\n"
                "  --road-cursor X,Y    pin the road preview cursor (window pixels; tests)\n"
                "  --fence-cursor X,Y   pin the enclosure preview cursor (window pixels; tests)\n"
                "  --park-budget N      override the starting park budget (tests/debug)\n"
                "  --buy-dino NAME      buy a Market.ini dinosaur into a paddock\n"
                "  --buy-paddock N      paddock index for --buy-dino (default first with room)\n"
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
                "  --dino-dat FILE      campaign unlock mask (default run/save/dino.dat)\n"
                "  --campaign-progress FILE  current mission per campaign\n"
                "  --auto-continue      advance past CPGEND without a click (test hook)\n"
                "  --place-food NAME    place a .bsi food source in the park\n"
                "  --food-pos X,Y       food source cell (default: first paddock)\n"
                "  --hire-nutritionist N hire N nutritionists (500 each)\n"
                "  --fire-nutritionist N dismiss N nutritionists (no refund)\n"
                "  --feed-now           run one hourly feeding pass at startup\n"
                "  --employee-info      print the .bei employee profiles and exit\n"
                "  --show-info PATH     print .shw show summaries and exit\n"
                "  --show NAME[:I,J]    place a show arena with dino participants\n"
                "  --place-show NAME[:X,Y] place a scheduled show (mission [SHOWS] catalog)\n"
                "  --show-participants NAME:I,J assign imported dinos to a placed show\n"
                "  --show-hour NAME:SLOT schedule a show at hour slot 1..10 (10:00..19:00)\n"
                "  --show-iterations NAME:N performances per day for a placed show\n"
                "  --show-duration MIN  performance length in park minutes (default 30)\n"
                "  --show-locked        place shows closed to the public\n"
                "  --show-status        print the attraction list rows after the first tick\n"
                "  --camera-target X,Y  screen-shot camera target in world units\n"
                "  --budget-info        print the budget ledger (current/yesterday/totals)\n"
                "  --dino-actions       print the dinosaur action chooser and exit\n"
                "  --dino-action-loop   drive imported dinosaur clips from the chooser\n"
                "  --dino-needs F,V,S,R imported dinosaur FOOD,VIOLENCE,SECU,REPOS (default 50,99,99,99)\n"
                "  --dino-action-speed N action duration scale (default 1)\n"
                "  --dino-threat N      danger-grid stamp value for every imported dinosaur\n"
                "  --dino-radius N      danger-grid stamp radius (world units)\n"
                "  --dino-health CUR,MAX health pair for the SECU formula (default 100,100)\n"
                "  --secu-probe         print each dinosaur's grid cell and SECU per rebuild\n"
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
                "  --park-time-scale N  override the clock time scale (default 60)\n"
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
                "  --config FILE        config file (default dino_viewer.cfg)\n"
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

void write_bmp(const std::string& path, int width, int height,
               const std::vector<std::uint8_t>& rgba) {
    const int row = width * 3;
    const int padding = (4 - (row % 4)) % 4;
    const std::uint32_t image_size =
        static_cast<std::uint32_t>((row + padding) * height);
    const std::uint32_t file_size = 14 + 40 + image_size;

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open screenshot %s\n", path.c_str());
        return;
    }
    auto u16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
    auto u32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };

    out.put('B');
    out.put('M');
    u32(file_size);
    u16(0);
    u16(0);
    u32(14 + 40);
    u32(40);
    u32(static_cast<std::uint32_t>(width));
    u32(static_cast<std::uint32_t>(height));
    u16(1);
    u16(24);
    u32(0);
    u32(image_size);
    u32(2835);
    u32(2835);
    u32(0);
    u32(0);

    const std::uint8_t pad[3] = {0, 0, 0};
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* src = rgba.data() + static_cast<std::size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            out.put(static_cast<char>(src[x * 4 + 2]));
            out.put(static_cast<char>(src[x * 4 + 1]));
            out.put(static_cast<char>(src[x * 4 + 0]));
        }
        out.write(reinterpret_cast<const char*>(pad), padding);
    }
    std::printf("wrote screenshot %s (%dx%d)\n", path.c_str(), width, height);
}

struct Bounds {
    dino::Vec3 min{1e30f, 1e30f, 1e30f};
    dino::Vec3 max{-1e30f, -1e30f, -1e30f};

    void add(const dino::Vec3& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    dino::Vec3 center() const {
        return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
    }

    float extent() const {
        return std::max({max.x - min.x, max.y - min.y, max.z - min.z});
    }
};

// Park clock: mirrors spScheduleManager's time manager (0x0058c1c0 /
// 0x0058c2f0 / 0x0058c350). A day is 12 hours; the 11th hour ends it.
struct ParkClock {
    int base_hour = 9;  // hardcoded PUSH 9 in spScheduleManager ctor
    int day = 0;
    double elapsed = 0.0;
    // GeneralParams.bgd/clock object: 60.0 next to the 10 at +0x1a0;
    // GameSpeed (Dino.ini) drives the scheduler timer, not this clock.
    double time_scale = 60.0;
    bool new_hour = false;
    bool end_of_day = false;

    double hours_per_second() const { return time_scale / 3600.0; }
    double minutes_per_second() const { return time_scale / 60.0; }

    int hour() const {
        return static_cast<int>(elapsed * hours_per_second()) + base_hour;
    }
    int minute() const {
        return static_cast<int>(elapsed * minutes_per_second()) % 60;
    }
    // Absolute park minute, used by campaign objectives (`delaylimit`).
    int minutes() const { return day * 720 + hour() * 60 + minute(); }

    void advance(double dt) {
        new_hour = false;
        end_of_day = false;
        const double next = elapsed + dt;
        if (static_cast<int>(next * hours_per_second()) >= 11) {
            end_of_day = true;
            return;
        }
        if (static_cast<int>(next * hours_per_second()) !=
            static_cast<int>(elapsed * hours_per_second())) {
            new_hour = true;
        }
        elapsed = next;
    }

    void next_day() {
        ++day;
        new_hour = false;
        end_of_day = false;
        elapsed = 0.0;
    }

    std::string time_text(bool ampm) const {
        int h = hour();
        const int m = minute();
        const char* suffix = "";
        if (ampm) {
            suffix = h < 12 ? "AM" : "PM";
            h %= 12;
            if (h == 0) {
                h = 12;
            }
        }
        char text[16];
        std::snprintf(text, sizeof(text), "%02d:%02d%s", h, m, suffix);
        return text;
    }

    std::string day_text() const { return std::to_string(day + 1); }
};

// ---------------------------------------------------------------------------
// Menu/park UI click sounds (MenuSoundDB.ini). A click cuts the previous
// sound; SDL3 makes one short device stream per click.
// ---------------------------------------------------------------------------
struct UiSoundPlayer {
    std::string asset_root;
    std::unordered_map<std::string, dino::Wav> cache;
    SDL_AudioStream* stream = nullptr;
    double remaining = 0.0;
    bool enabled = true;
    bool muted = false;

    void play(const std::string& relative) {
        if (!enabled || relative.empty()) {
            return;
        }
        std::string path;
        try {
            path = dino::asset_path(asset_root, relative);
        } catch (const std::exception&) {
            return;
        }
        auto it = cache.find(path);
        if (it == cache.end()) {
            try {
                it = cache.emplace(path, dino::load_wav(path)).first;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "sound load %s: %s\n", path.c_str(), e.what());
                return;
            }
        }
        const dino::Wav& wav = it->second;
        if (stream != nullptr) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
        SDL_AudioSpec spec{};
        spec.format = wav.bits_per_sample == 16 ? SDL_AUDIO_S16 : SDL_AUDIO_U8;
        spec.channels = wav.channels;
        spec.freq = static_cast<int>(wav.sample_rate);
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                           &spec, nullptr, nullptr);
        if (stream == nullptr) {
            std::fprintf(stderr, "sound device: %s\n", SDL_GetError());
            enabled = false;
            return;
        }
        SDL_SetAudioStreamGain(stream, muted ? 0.0f : 1.0f);
        SDL_ResumeAudioStreamDevice(stream);
        SDL_PutAudioStreamData(stream, wav.data.data(),
                               static_cast<int>(wav.data.size()));
        remaining = wav.duration;
        std::printf("sound: %s\n", path.c_str());
    }

    void set_muted(bool value) {
        muted = value;
        if (stream != nullptr) {
            SDL_SetAudioStreamGain(stream, muted ? 0.0f : 1.0f);
        }
    }

    void update(double dt) {
        if (stream == nullptr) {
            return;
        }
        remaining -= dt;
        if (remaining <= 0.0) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
    }

    void destroy() {
        if (stream != nullptr) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
    }
};

// Island music: streams a 26-28 MB theme in 64 KB chunks, looping at the end.
struct MusicPlayer {
    std::string asset_root;
    SDL_AudioStream* stream = nullptr;
    dino::Wav wav;
    std::size_t offset = 0;
    bool looping = true;
    bool enabled = false;
    float gain = 0.97f;
    std::string current;

    void stop() {
        if (stream != nullptr) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
        wav = dino::Wav{};
        offset = 0;
        current.clear();
    }

    void play(const std::string& relative) {
        if (!enabled || relative.empty()) {
            return;
        }
        stop();
        std::string path;
        try {
            path = dino::asset_path(asset_root, relative);
        } catch (const std::exception&) {
            return;
        }
        try {
            wav = dino::load_wav(path);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "music load %s: %s\n", path.c_str(), e.what());
            return;
        }
        if (wav.data.empty() || wav.sample_rate == 0 || wav.channels == 0) {
            return;
        }
        SDL_AudioSpec spec{};
        spec.format = wav.bits_per_sample == 16 ? SDL_AUDIO_S16 : SDL_AUDIO_U8;
        spec.channels = wav.channels;
        spec.freq = static_cast<int>(wav.sample_rate);
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                           &spec, nullptr, nullptr);
        if (stream == nullptr) {
            std::fprintf(stderr, "music device: %s\n", SDL_GetError());
            enabled = false;
            return;
        }
        SDL_SetAudioStreamGain(stream, gain);
        SDL_ResumeAudioStreamDevice(stream);
        current = path;
        offset = 0;
        feed();
        std::printf("music: %s, %u Hz, %u ch, %.1fs (loop)\n", path.c_str(),
                    wav.sample_rate, wav.channels, wav.duration);
    }

    void feed() {
        if (stream == nullptr || wav.data.empty()) {
            return;
        }
        const int bytes_per_second =
            static_cast<int>(wav.sample_rate) * wav.channels *
            (wav.bits_per_sample / 8);
        if (bytes_per_second <= 0) {
            return;
        }
        while (SDL_GetAudioStreamQueued(stream) < bytes_per_second) {
            if (offset >= wav.data.size()) {
                if (!looping) {
                    break;
                }
                offset = 0;
            }
            const std::size_t chunk =
                std::min<std::size_t>(65536, wav.data.size() - offset);
            SDL_PutAudioStreamData(stream, wav.data.data() + offset,
                                   static_cast<int>(chunk));
            offset += chunk;
        }
    }

    void update() {
        if (stream != nullptr) {
            feed();
        }
    }

    void set_gain(float value) {
        gain = value;
        if (stream != nullptr) {
            SDL_SetAudioStreamGain(stream, gain);
        }
    }

    void destroy() {
        stop();
        enabled = false;
    }
};

// ---------------------------------------------------------------------------
// Minimal GL 3.3 function loader (Windows opengl32 only exports GL 1.1).
// ---------------------------------------------------------------------------

struct GlFns {
    PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC GenBuffers = nullptr;
    PFNGLBINDBUFFERPROC BindBuffer = nullptr;
    PFNGLBUFFERDATAPROC BufferData = nullptr;
    PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;
    PFNGLCREATESHADERPROC CreateShader = nullptr;
    PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC CompileShader = nullptr;
    PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC DeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
    PFNGLATTACHSHADERPROC AttachShader = nullptr;
    PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
    PFNGLUSEPROGRAMPROC UseProgram = nullptr;
    PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
    PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv = nullptr;
    PFNGLUNIFORM3FVPROC Uniform3fv = nullptr;
    PFNGLUNIFORM4FPROC Uniform4f = nullptr;
    PFNGLUNIFORM2FPROC Uniform2f = nullptr;
    PFNGLUNIFORM1FPROC Uniform1f = nullptr;
    PFNGLUNIFORM1IPROC Uniform1i = nullptr;
    PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
    PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr;
    PFNGLGENRENDERBUFFERSPROC GenRenderbuffers = nullptr;
    PFNGLBINDRENDERBUFFERPROC BindRenderbuffer = nullptr;
    PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage = nullptr;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer = nullptr;
    PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers = nullptr;

    bool load() {
        auto get = [](const char* name) {
            return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
        };
#define LOAD(field, name) \
    field = reinterpret_cast<decltype(field)>(get(name)); \
    if (!field) { std::fprintf(stderr, "missing GL function %s\n", name); return false; }
        LOAD(GenVertexArrays, "glGenVertexArrays")
        LOAD(BindVertexArray, "glBindVertexArray")
        LOAD(DeleteVertexArrays, "glDeleteVertexArrays")
        LOAD(GenBuffers, "glGenBuffers")
        LOAD(BindBuffer, "glBindBuffer")
        LOAD(BufferData, "glBufferData")
        LOAD(DeleteBuffers, "glDeleteBuffers")
        LOAD(CreateShader, "glCreateShader")
        LOAD(ShaderSource, "glShaderSource")
        LOAD(CompileShader, "glCompileShader")
        LOAD(GetShaderiv, "glGetShaderiv")
        LOAD(GetShaderInfoLog, "glGetShaderInfoLog")
        LOAD(DeleteShader, "glDeleteShader")
        LOAD(CreateProgram, "glCreateProgram")
        LOAD(AttachShader, "glAttachShader")
        LOAD(LinkProgram, "glLinkProgram")
        LOAD(GetProgramiv, "glGetProgramiv")
        LOAD(GetProgramInfoLog, "glGetProgramInfoLog")
        LOAD(UseProgram, "glUseProgram")
        LOAD(DeleteProgram, "glDeleteProgram")
        LOAD(EnableVertexAttribArray, "glEnableVertexAttribArray")
        LOAD(VertexAttribPointer, "glVertexAttribPointer")
        LOAD(GetUniformLocation, "glGetUniformLocation")
        LOAD(UniformMatrix4fv, "glUniformMatrix4fv")
        LOAD(Uniform3fv, "glUniform3fv")
        LOAD(Uniform4f, "glUniform4f")
        LOAD(Uniform2f, "glUniform2f")
        LOAD(Uniform1f, "glUniform1f")
        LOAD(Uniform1i, "glUniform1i")
        LOAD(ActiveTexture, "glActiveTexture")
        LOAD(GenFramebuffers, "glGenFramebuffers")
        LOAD(BindFramebuffer, "glBindFramebuffer")
        LOAD(FramebufferTexture2D, "glFramebufferTexture2D")
        LOAD(CheckFramebufferStatus, "glCheckFramebufferStatus")
        LOAD(DeleteFramebuffers, "glDeleteFramebuffers")
        LOAD(GenRenderbuffers, "glGenRenderbuffers")
        LOAD(BindRenderbuffer, "glBindRenderbuffer")
        LOAD(RenderbufferStorage, "glRenderbufferStorage")
        LOAD(FramebufferRenderbuffer, "glFramebufferRenderbuffer")
        LOAD(DeleteRenderbuffers, "glDeleteRenderbuffers")
#undef LOAD
        return true;
    }
};

GLuint compile_shader(GLenum type, const char* source, const GlFns& gl) {
    const GLuint shader = gl.CreateShader(type);
    gl.ShaderSource(shader, 1, &source, nullptr);
    gl.CompileShader(shader);
    GLint ok = GL_FALSE;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        GLsizei len = 0;
        gl.GetShaderInfoLog(shader, sizeof(log), &len, log);
        std::fprintf(stderr, "shader compile failed: %.*s\n", static_cast<int>(len), log);
        gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint build_program(const GlFns& gl) {
    static const char* kVertex = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_color;
uniform mat4 u_mvp;
uniform vec4 u_uv2_transform;
uniform vec2 u_uv_offset;
out vec3 v_normal;
out vec2 v_uv;
out vec2 v_uv2;
out vec4 v_color;
out vec3 v_world;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    v_normal = a_normal;
    // WaterSurface stage 0: the retail builder scrolls this layer with
    // -offset (u from +0x14, v from +0x10). Other materials pass 0.
    v_uv = a_uv + u_uv_offset;
    // WaterSurface stage 1 uses texture coordinate set 1: the same water
    // texture with a slightly different tiling and a small offset.
    v_uv2 = a_uv * u_uv2_transform.xy + u_uv2_transform.zw;
    v_color = a_color;
    v_world = a_pos;
}
)GLSL";
    static const char* kFragment = R"GLSL(
#version 330 core
in vec3 v_normal;
in vec2 v_uv;
in vec2 v_uv2;
in vec4 v_color;
in vec3 v_world;
uniform sampler2D u_texture;
uniform sampler2D u_texture2;
uniform int u_use_texture;
uniform int u_use_texture2;
uniform int u_modulate2x;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform float u_alpha;
uniform int u_unlit;
uniform vec3 u_camera;
uniform vec3 u_fog_color;
uniform float u_fog_near;
uniform float u_fog_far;
uniform int u_fog_enable;
out vec4 frag_color;
void main() {
    vec3 n = normalize(v_normal);
    if (!gl_FrontFacing) n = -n;
    float key = max(dot(n, u_light_dir), 0.0);
    float fill = max(dot(n, -u_light_dir), 0.0) * 0.25;
    vec3 base = v_color.rgb;
    vec4 texel = vec4(1.0);
    if (u_use_texture != 0) {
        texel = texture(u_texture, v_uv);
        base *= texel.rgb;
    }
    // WaterSurface.ini: stage 1 Modulate2x (Texture x Current, doubled),
    // alpha SelectArg1(Texture), texture coordinate set 1.
    if (u_use_texture2 != 0) {
        vec4 texel2 = texture(u_texture2, v_uv2);
        if (u_modulate2x != 0) {
            texel2.rgb = clamp(texel2.rgb * 2.0, 0.0, 1.0);
        }
        base *= texel2.rgb;
        texel.a *= texel2.a;
    }
    if (u_unlit == 2) {
        frag_color = vec4(v_color.rgb, texel.r * u_alpha * v_color.a);
        return;
    }
    vec3 color = base;
    if (u_unlit == 0) {
        color = base * (u_ambient + u_light_color * (key + fill));
        color = pow(clamp(color, 0.0, 1.0), vec3(0.85));
        if (u_fog_enable != 0) {
            float dist = length(v_world - u_camera);
            float fog = clamp((dist - u_fog_near) /
                                  max(1.0, u_fog_far - u_fog_near),
                              0.0, 1.0);
            color = mix(color, u_fog_color, fog);
        }
    }
    float out_alpha = u_alpha * v_color.a;
    if (u_use_texture != 0) {
        out_alpha *= texel.a;
    }
    frag_color = vec4(color, out_alpha);
}
)GLSL";
    const GLuint vs = compile_shader(GL_VERTEX_SHADER, kVertex, gl);
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kFragment, gl);
    if (!vs || !fs) {
        if (vs) {
            gl.DeleteShader(vs);
        }
        if (fs) {
            gl.DeleteShader(fs);
        }
        return 0;
    }
    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, vs);
    gl.AttachShader(program, fs);
    gl.LinkProgram(program);
    gl.DeleteShader(vs);
    gl.DeleteShader(fs);
    GLint ok = GL_FALSE;
    gl.GetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        GLsizei len = 0;
        gl.GetProgramInfoLog(program, sizeof(log), &len, log);
        std::fprintf(stderr, "program link failed: %.*s\n", static_cast<int>(len), log);
        gl.DeleteProgram(program);
        return 0;
    }
    return program;
}

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    std::uint8_t r, g, b, a;
};
static_assert(sizeof(Vertex) == 36, "unexpected vertex stride");

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei index_count = 0;

    void destroy(const GlFns& gl) {
        if (ebo) gl.DeleteBuffers(1, &ebo);
        if (vbo) gl.DeleteBuffers(1, &vbo);
        if (vao) gl.DeleteVertexArrays(1, &vao);
        ebo = vbo = vao = 0;
        index_count = 0;
    }
};

struct SkyboxGL {
    std::array<GpuMesh, 6> faces;
    std::array<GLuint, 6> textures{};
    bool ready = false;
};

std::vector<Vertex> build_vertices(const dino::Mesh& mesh) {
    const std::size_t count = mesh.positions.size();
    std::vector<Vertex> vertices;
    vertices.reserve(count);
    const bool has_n = mesh.normals.size() == count;
    const bool has_uv = mesh.uvs.size() == count;
    const bool has_c = mesh.colors.size() == count;
    for (std::size_t i = 0; i < count; ++i) {
        Vertex v{};
        const auto& p = mesh.positions[i];
        v.px = p.x; v.py = p.y; v.pz = p.z;
        if (has_n) {
            v.nx = mesh.normals[i].x; v.ny = mesh.normals[i].y; v.nz = mesh.normals[i].z;
        } else {
            v.nz = 1.0f;
        }
        if (has_uv) {
            v.u = mesh.uvs[i].u;
            v.v = 1.0f + mesh.uvs[i].v;  // engine UVs are bottom-left
        }
        if (has_c) {
            const auto& c = mesh.colors[i];
            v.r = c.r; v.g = c.g; v.b = c.b; v.a = c.a;
        } else {
            v.r = v.g = v.b = 220; v.a = 255;
        }
        vertices.push_back(v);
    }
    return vertices;
}

GpuMesh upload_mesh(const dino::Mesh& mesh, const GlFns& gl) {
    GpuMesh gpu;
    const std::size_t count = mesh.positions.size();
    if (count == 0) {
        return gpu;
    }
    std::vector<Vertex> vertices = build_vertices(mesh);
    std::vector<std::uint32_t> indices = mesh.indices;
    if (indices.empty()) {
        indices.resize(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            indices[i] = i;
        }
    }
    gpu.index_count = static_cast<GLsizei>(indices.size());

    gl.GenVertexArrays(1, &gpu.vao);
    gl.BindVertexArray(gpu.vao);
    gl.GenBuffers(1, &gpu.vbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    gl.BufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                  vertices.data(), GL_STATIC_DRAW);
    gl.GenBuffers(1, &gpu.ebo);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                  indices.data(), GL_STATIC_DRAW);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                           reinterpret_cast<const void*>(offsetof(Vertex, px)));
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                           reinterpret_cast<const void*>(offsetof(Vertex, nx)));
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                           reinterpret_cast<const void*>(offsetof(Vertex, u)));
    gl.EnableVertexAttribArray(3);
    gl.VertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                           reinterpret_cast<const void*>(offsetof(Vertex, r)));
    gl.BindVertexArray(0);
    return gpu;
}

void update_mesh(GpuMesh& gpu, const dino::Mesh& mesh, const GlFns& gl) {
    if (gpu.vbo == 0) {
        return;
    }
    const std::vector<Vertex> vertices = build_vertices(mesh);
    gl.BindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    gl.BufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                  vertices.data(), GL_DYNAMIC_DRAW);
    gl.BindBuffer(GL_ARRAY_BUFFER, 0);
}

GLuint upload_texture_wrapped(const dino::Image& image, GLint wrap) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    return id;
}

GLuint upload_texture(const dino::Image& image) {
    return upload_texture_wrapped(image, GL_CLAMP_TO_EDGE);
}

dino::Mesh make_water_plane(float half_size, float z, float tiles) {
    dino::Mesh mesh;
    mesh.name = "water";
    mesh.positions = {{-half_size, -half_size, z},
                      {half_size, -half_size, z},
                      {half_size, half_size, z},
                      {-half_size, half_size, z}};
    mesh.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    // build_vertices adds +1 to v; final UVs are 0..tiles.
    mesh.uvs = {{0.0f, -1.0f}, {tiles, -1.0f}, {tiles, tiles - 1.0f},
                {0.0f, tiles - 1.0f}};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

dino::Mesh make_sky_face(int index, float half_size) {
    const float h = half_size;
    dino::Mesh mesh;
    mesh.name = "skybox";
    dino::Vec3 normal{0, 0, 1};
    // Retail face mapping (skybox geometry builder 0x00600f80, draw
    // 0x00600df0): front = -X, left = +Y, back = +X, right = -Y,
    // top = +Z, bottom = -Z. Each face's vertices are in UV order
    // (0,0), (1,0), (0,1), (1,1); v=0 is the PNG top row. The shader adds
    // 1 to v, so the stored values are (0,-1), (1,-1), (0,0), (1,0).
    switch (index) {
        case 0:  // front (-X)
            mesh.positions = {{-h, -h, h}, {-h, h, h}, {-h, -h, -h}, {-h, h, -h}};
            normal = {1, 0, 0};
            break;
        case 1:  // back (+X)
            mesh.positions = {{h, h, h}, {h, -h, h}, {h, h, -h}, {h, -h, -h}};
            normal = {-1, 0, 0};
            break;
        case 2:  // left (+Y)
            mesh.positions = {{-h, h, h}, {h, h, h}, {-h, h, -h}, {h, h, -h}};
            normal = {0, -1, 0};
            break;
        case 3:  // right (-Y)
            mesh.positions = {{h, -h, h}, {-h, -h, h}, {h, -h, -h}, {-h, -h, -h}};
            normal = {0, 1, 0};
            break;
        case 4:  // top (+Z)
            mesh.positions = {{h, -h, h}, {h, h, h}, {-h, -h, h}, {-h, h, h}};
            normal = {0, 0, -1};
            break;
        default:  // bottom (-Z)
            mesh.positions = {{-h, -h, -h}, {-h, h, -h}, {h, -h, -h}, {h, h, -h}};
            normal = {0, 0, 1};
            break;
    }
    mesh.normals.assign(4, normal);
    mesh.uvs = {{0.0f, -1.0f}, {1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}};
    mesh.indices = {0, 1, 2, 2, 1, 3};
    return mesh;
}

GLuint upload_font_atlas(const dino::Fnt& font) {
    std::vector<std::uint8_t> alpha(
        static_cast<std::size_t>(font.atlas_width) * font.atlas_height);
    for (std::size_t i = 0; i < alpha.size(); ++i) {
        alpha[i] = font.atlas[i * 2 + 1];
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, font.atlas_width, font.atlas_height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, alpha.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return id;
}

dino::Mesh build_text_mesh(const dino::Fnt& font, const std::string& text,
                           float x, float y, float scale, dino::Color color) {
    dino::Mesh mesh;
    mesh.name = "hud";
    const float inv_w = 1.0f / static_cast<float>(font.atlas_width);
    const float inv_h = 1.0f / static_cast<float>(font.atlas_height);
    float pen_x = x;
    float pen_y = y;
    for (const unsigned char ch : text) {
        if (ch == '\n') {
            pen_x = x;
            pen_y += static_cast<float>(font.line_height) * scale;
            continue;
        }
        const dino::FntGlyph* glyph = font.glyph(ch);
        if (glyph == nullptr) {
            pen_x += static_cast<float>(font.font_size) * 0.5f * scale;
            continue;
        }
        const float w = static_cast<float>(glyph->width()) * scale;
        const float h = static_cast<float>(glyph->height()) * scale;
        const float u0 = static_cast<float>(glyph->x0) * inv_w;
        const float u1 = static_cast<float>(glyph->x1 + 1) * inv_w;
        const float v0 = static_cast<float>(glyph->y0) * inv_h;
        const float v1 = static_cast<float>(glyph->y1 + 1) * inv_h;
        const auto base = static_cast<std::uint32_t>(mesh.positions.size());
        mesh.positions.push_back({pen_x, pen_y, 0.0f});
        mesh.positions.push_back({pen_x + w, pen_y, 0.0f});
        mesh.positions.push_back({pen_x + w, pen_y + h, 0.0f});
        mesh.positions.push_back({pen_x, pen_y + h, 0.0f});
        mesh.normals.insert(mesh.normals.end(), 4, {0.0f, 0.0f, 1.0f});
        // build_vertices adds +1 to v, so pass v-1 to get the atlas coordinate.
        mesh.uvs.push_back({u0, v0 - 1.0f});
        mesh.uvs.push_back({u1, v0 - 1.0f});
        mesh.uvs.push_back({u1, v1 - 1.0f});
        mesh.uvs.push_back({u0, v1 - 1.0f});
        mesh.colors.insert(mesh.colors.end(), 4, color);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
        pen_x += w;
    }
    return mesh;
}

float text_width(const dino::Fnt& font, const std::string& text, float scale) {
    float width = 0.0f;
    for (const unsigned char ch : text) {
        if (ch == '\n') {
            continue;
        }
        const dino::FntGlyph* glyph = font.glyph(ch);
        width += glyph != nullptr ? static_cast<float>(glyph->width()) * scale
                                  : static_cast<float>(font.font_size) * 0.5f * scale;
    }
    return width;
}

dino::Mesh build_screen_rect(float x, float y, float w, float h,
                             dino::Color color) {
    dino::Mesh mesh;
    mesh.name = "screen_rect";
    mesh.positions = {{x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}};
    mesh.normals.assign(4, {0.0f, 0.0f, 1.0f});
    mesh.colors.assign(4, color);
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

struct UiTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

struct UiSourceRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool valid = true;
};

// Park HUD widgets are re-anchored to the full logical window by
// Overlay::layout_for_size(), so SDL window points are already park design
// points. The menu is the exception: it is drawn as an 800x600 rect centred
// in the window and subtracts the letterbox origin separately.
void map_park_pointer(float window_x, float window_y, int& design_x,
                      int& design_y) {
    design_x = static_cast<int>(window_x);
    design_y = static_cast<int>(window_y);
}

UiSourceRect ui_frame_rect(const UiTexture& texture, const dino::ui::Graphic& graphic,
                           int logical_frame) {
    UiSourceRect rect{graphic.rect.x, graphic.rect.y, graphic.rect.width,
                      graphic.rect.height};
    std::vector<int> used;
    for (int frame = 0; frame < graphic.objects; ++frame) {
        if (std::find(graphic.skip.begin(), graphic.skip.end(), frame) ==
            graphic.skip.end()) {
            used.push_back(frame);
        }
    }
    const auto it = std::find(used.begin(), used.end(), logical_frame);
    if (it == used.end() || graphic.rect.width <= 0 || graphic.rect.height <= 0) {
        rect.valid = false;
        return rect;
    }
    const int slot = static_cast<int>(it - used.begin());
    const bool horizontal =
        graphic.rect.x + static_cast<int>(used.size()) * graphic.rect.width <=
        texture.width;
    rect.x = horizontal ? graphic.rect.x + slot * graphic.rect.width
                        : graphic.rect.x;
    rect.y = horizontal ? graphic.rect.y
                        : graphic.rect.y + slot * graphic.rect.height;
    if (rect.x + rect.w > texture.width || rect.y + rect.h > texture.height) {
        rect.valid = false;
    }
    return rect;
}

dino::Mesh build_ui_quad(float x, float y, float w, float h, float u0, float v0,
                         float u1, float v1, dino::Color color) {
    dino::Mesh mesh;
    mesh.name = "ui_quad";
    mesh.positions = {{x, y, 0}, {x + w, y, 0}, {x + w, y + h, 0}, {x, y + h, 0}};
    mesh.normals.assign(4, {0.0f, 0.0f, 1.0f});
    // build_vertices adds +1 to v.
    mesh.uvs = {{u0, v0 - 1.0f}, {u1, v0 - 1.0f}, {u1, v1 - 1.0f}, {u0, v1 - 1.0f}};
    mesh.colors.assign(4, color);
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

void append_mesh(dino::Mesh& dst, const dino::Mesh& src) {
    const auto base = static_cast<std::uint32_t>(dst.positions.size());
    dst.positions.insert(dst.positions.end(), src.positions.begin(), src.positions.end());
    dst.normals.insert(dst.normals.end(), src.normals.begin(), src.normals.end());
    dst.uvs.insert(dst.uvs.end(), src.uvs.begin(), src.uvs.end());
    dst.colors.insert(dst.colors.end(), src.colors.begin(), src.colors.end());
    for (const std::uint32_t index : src.indices) {
        dst.indices.push_back(base + index);
    }
}

dino::Mesh build_text_block(const dino::Fnt& font, const std::string& text,
                            float x, float y, float scale, dino::Color color,
                            const std::string& justify, float box_w, float box_h) {
    dino::Mesh out;
    out.name = "ui_text";
    std::vector<std::string> lines;
    std::string line;
    for (const char ch : text) {
        if (ch == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line.push_back(ch);
        }
    }
    lines.push_back(line);
    const float line_height = static_cast<float>(font.line_height) * scale;
    const float total_height = line_height * static_cast<float>(lines.size());
    float start_y = y;
    if (justify.find("DOWN") != std::string::npos) {
        start_y = y + box_h - total_height;
    } else if (justify.find("CENTER") != std::string::npos ||
               justify.find("Y") != std::string::npos) {
        start_y = y + (box_h - total_height) * 0.5f;
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const float width = text_width(font, lines[i], scale);
        float line_x = x;
        if (justify.find("RIGHT") != std::string::npos) {
            line_x = x + box_w - width;
        } else if (justify.find("CENTER") != std::string::npos ||
                   justify.find("X") != std::string::npos) {
            line_x = x + (box_w - width) * 0.5f;
        }
        const dino::Mesh glyphs = build_text_mesh(
            font, lines[i], line_x, start_y + static_cast<float>(i) * line_height,
            scale, color);
        append_mesh(out, glyphs);
    }
    return out;
}

std::string wrap_text(const std::string& text, std::size_t width) {
    std::string out;
    std::string word;
    std::size_t line = 0;
    auto flush = [&]() {
        if (word.empty()) {
            return;
        }
        if (line != 0 && line + 1 + word.size() > width) {
            out += '\n';
            line = 0;
        } else if (line != 0) {
            out += ' ';
            ++line;
        }
        out += word;
        line += word.size();
        word.clear();
    };
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const char ch = i < text.size() ? text[i] : ' ';
        if (ch == '\\' && i + 1 < text.size() && text[i + 1] == 'n') {
            flush();
            out += '\n';
            line = 0;
            ++i;
        } else if (ch == '\n') {
            flush();
            out += '\n';
            line = 0;
        } else if (ch == ' ' || ch == '\t' || ch == '\r') {
            flush();
        } else {
            word.push_back(ch);
        }
    }
    flush();
    return out;
}

std::string limit_lines(const std::string& text, std::size_t max_lines) {
    std::size_t line = 1;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            ++line;
            if (line > max_lines) {
                return text.substr(0, i);
            }
        }
    }
    return text;
}

dino::Mat4 perspective(float fov_deg, float aspect, float near_plane, float far_plane) {
    dino::Mat4 out;
    const float f = 1.0f / std::tan(fov_deg * 3.14159265f / 360.0f);
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    out.m[14] = -1.0f;
    out.m[11] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    out.m[15] = 0.0f;
    return out;
}
dino::Vec3 cross(const dino::Vec3& a, const dino::Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float dot(const dino::Vec3& a, const dino::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

dino::Mat4 look_at(const dino::Vec3& eye, const dino::Vec3& target, const dino::Vec3& up) {
    const dino::Vec3 f = dino::normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
    const dino::Vec3 s = dino::normalize(cross(f, up));
    const dino::Vec3 u = cross(s, f);
    dino::Mat4 out;
    out.m[0] = s.x;  out.m[1] = s.y;  out.m[2] = s.z;  out.m[3] = -dot(s, eye);
    out.m[4] = u.x;  out.m[5] = u.y;  out.m[6] = u.z;  out.m[7] = -dot(u, eye);
    out.m[8] = -f.x; out.m[9] = -f.y; out.m[10] = -f.z; out.m[11] = dot(f, eye);
    out.m[12] = 0.0f; out.m[13] = 0.0f; out.m[14] = 0.0f; out.m[15] = 1.0f;
    return out;
}

struct DrawItem {
    const GpuMesh* mesh = nullptr;
    GLuint texture = 0;
    float alpha = 1.0f;
    // Water: Data/States/WaterSurface.ini uses src One / dest One / Add.
    bool additive = false;
    // Additive items normally leave the depth buffer alone; a solid preview
    // box needs the nearest face to win so back/side faces do not add a
    // second layer and saturate to white.
    bool depth_write = false;
    // Skip lighting for flat preview quads whose sampled colour must match
    // retail exactly (enclosure disc).
    bool unlit = false;
    // 0 = scene fog (FogParam.txt), 1 = water fog (FogParam_WaterSurface.txt).
    int fog = 0;
    // WaterSurface stage 1: same texture on coordinate set 1, Modulate2x.
    GLuint texture2 = 0;
    bool modulate2x = false;
    float uv_offset[2] = {0.0f, 0.0f};
    float uv2_scale[2] = {1.0f, 1.0f};
    float uv2_offset[2] = {0.0f, 0.0f};
};

dino::Mat4 ortho(float left, float right, float bottom, float top,
                 float near_plane, float far_plane) {
    // Row-vector convention, matching look_at() and the transposed upload
    // used by the renderer: translation lives in row 3 (m[3], m[7], m[11]).
    dino::Mat4 out;
    out.m[0] = 2.0f / (right - left);
    out.m[5] = 2.0f / (top - bottom);
    out.m[10] = -2.0f / (far_plane - near_plane);
    out.m[3] = -(right + left) / (right - left);
    out.m[7] = -(top + bottom) / (top - bottom);
    out.m[11] = -(far_plane + near_plane) / (far_plane - near_plane);
    out.m[15] = 1.0f;
    return out;
}

// Offscreen colour+depth target for MAP_RENDERVIEW (136x136 in the retail
// HUD). The minimap is a render target, not a crop of the basemap.
struct MinimapTarget {
    GLuint fbo = 0;
    GLuint color = 0;
    GLuint depth = 0;
    int size = 0;

    bool create(GlFns& gl, int new_size) {
        destroy(gl);
        size = new_size;
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gl.GenFramebuffers(1, &fbo);
        gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, color, 0);
        gl.GenRenderbuffers(1, &depth);
        gl.BindRenderbuffer(GL_RENDERBUFFER, depth);
        gl.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, size, size);
        gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_RENDERBUFFER, depth);
        const GLenum status = gl.CheckFramebufferStatus(GL_FRAMEBUFFER);
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "minimap FBO incomplete: 0x%x\n", status);
            destroy(gl);
            return false;
        }
        return true;
    }

    void destroy(GlFns& gl) {
        if (fbo != 0) {
            gl.DeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
        if (depth != 0) {
            gl.DeleteRenderbuffers(1, &depth);
            depth = 0;
        }
        if (color != 0) {
            glDeleteTextures(1, &color);
            color = 0;
        }
        size = 0;
    }
};

void draw_scene(const GpuMesh& mesh, GLuint texture,
                const std::vector<DrawItem>& extras,
                const SkyboxGL& skybox,
                GLuint program,
                const GlFns& gl, const dino::Vec3& eye, const dino::Vec3& center,
                int width, int height, float fov_deg = 33.0f) {
    const dino::Mat4 proj = perspective(fov_deg,
                                        static_cast<float>(width) / std::max(1, height),
                                        0.1f, 20000.0f);
    const dino::Mat4 view = look_at(eye, center, {0.0f, 0.0f, 1.0f});
    const dino::Mat4 mvp = dino::multiply(proj, view);

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    gl.UseProgram(program);
    gl.UniformMatrix4fv(gl.GetUniformLocation(program, "u_mvp"), 1, GL_TRUE, mvp.m);
    const float light[3] = {-0.35f, -0.68f, 0.72f};
    const float light_color[3] = {0.92f, 0.93f, 0.95f};
    const float ambient[3] = {0.62f, 0.64f, 0.68f};
    gl.Uniform3fv(gl.GetUniformLocation(program, "u_light_dir"), 1, light);
    gl.Uniform3fv(gl.GetUniformLocation(program, "u_light_color"), 1, light_color);
    gl.Uniform3fv(gl.GetUniformLocation(program, "u_ambient"), 1, ambient);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture"), texture != 0 ? 1 : 0);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_texture"), 0);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_texture2"), 1);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture2"), 0);
    gl.Uniform1i(gl.GetUniformLocation(program, "u_modulate2x"), 0);
    gl.Uniform4f(gl.GetUniformLocation(program, "u_uv2_transform"), 1.0f, 1.0f,
                 0.0f, 0.0f);
    gl.Uniform2f(gl.GetUniformLocation(program, "u_uv_offset"), 0.0f, 0.0f);
    gl.Uniform1f(gl.GetUniformLocation(program, "u_alpha"), 1.0f);

    // Data/States/FogParam.txt: linear 1500..2000, colour (255,220,165).
    const GLint camera_loc = gl.GetUniformLocation(program, "u_camera");
    const GLint fog_color_loc = gl.GetUniformLocation(program, "u_fog_color");
    const GLint fog_near_loc = gl.GetUniformLocation(program, "u_fog_near");
    const GLint fog_far_loc = gl.GetUniformLocation(program, "u_fog_far");
    const GLint fog_enable_loc = gl.GetUniformLocation(program, "u_fog_enable");
    const float scene_fog_color[3] = {1.0f, 0.862745f, 0.647059f};
    const float camera_pos[3] = {eye.x, eye.y, eye.z};
    gl.Uniform3fv(camera_loc, 1, camera_pos);
    gl.Uniform1i(fog_enable_loc, 1);
    gl.Uniform3fv(fog_color_loc, 1, scene_fog_color);
    gl.Uniform1f(fog_near_loc, 1500.0f);
    gl.Uniform1f(fog_far_loc, 2000.0f);

    const GLint unlit_loc = gl.GetUniformLocation(program, "u_unlit");
    if (skybox.ready) {
        gl.Uniform1i(unlit_loc, 1);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture"), 1);
        for (int i = 0; i < 6; ++i) {
            if (skybox.faces[i].vao == 0 || skybox.textures[i] == 0) {
                continue;
            }
            glBindTexture(GL_TEXTURE_2D, skybox.textures[i]);
            gl.BindVertexArray(skybox.faces[i].vao);
            glDrawElements(GL_TRIANGLES, skybox.faces[i].index_count,
                           GL_UNSIGNED_INT, nullptr);
        }
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        gl.Uniform1i(gl.GetUniformLocation(program, "u_use_texture"),
                     texture != 0 ? 1 : 0);
    }
    gl.Uniform1i(unlit_loc, 0);

    if (texture != 0) {
        gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    gl.BindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);

    const GLint use_texture_loc = gl.GetUniformLocation(program, "u_use_texture");
    const GLint alpha_loc = gl.GetUniformLocation(program, "u_alpha");
    const GLint use_texture2_loc =
        gl.GetUniformLocation(program, "u_use_texture2");
    const GLint modulate2x_loc =
        gl.GetUniformLocation(program, "u_modulate2x");
    const GLint uv2_loc = gl.GetUniformLocation(program, "u_uv2_transform");
    const GLint uv_offset_loc = gl.GetUniformLocation(program, "u_uv_offset");
    for (const DrawItem& item : extras) {
        if (item.mesh == nullptr || item.mesh->vao == 0) {
            continue;
        }
        gl.Uniform1i(use_texture_loc, item.texture != 0 ? 1 : 0);
        gl.Uniform1i(unlit_loc, item.unlit ? 1 : 0);
        gl.Uniform1f(alpha_loc, item.alpha);
        gl.Uniform1i(use_texture2_loc, item.texture2 != 0 ? 1 : 0);
        gl.Uniform1i(modulate2x_loc, item.modulate2x ? 1 : 0);
        gl.Uniform4f(uv2_loc, item.uv2_scale[0], item.uv2_scale[1],
                     item.uv2_offset[0], item.uv2_offset[1]);
        gl.Uniform2f(uv_offset_loc, item.uv_offset[0], item.uv_offset[1]);
        if (item.fog == 1) {
            // Data/States/FogParam_WaterSurface.txt: 100..700, black.
            const float water_fog[3] = {0.0f, 0.0f, 0.0f};
            gl.Uniform3fv(fog_color_loc, 1, water_fog);
            gl.Uniform1f(fog_near_loc, 100.0f);
            gl.Uniform1f(fog_far_loc, 700.0f);
        } else {
            gl.Uniform3fv(fog_color_loc, 1, scene_fog_color);
            gl.Uniform1f(fog_near_loc, 1500.0f);
            gl.Uniform1f(fog_far_loc, 2000.0f);
        }
        const bool blend = item.alpha < 1.0f || item.additive;
        if (blend) {
            glEnable(GL_BLEND);
            if (item.additive) {
                glBlendFunc(GL_ONE, GL_ONE);
            } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            glDepthMask(item.depth_write ? GL_TRUE : GL_FALSE);
        }
        if (item.texture != 0) {
            glBindTexture(GL_TEXTURE_2D, item.texture);
        }
        if (item.texture2 != 0) {
            gl.ActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, item.texture2);
            gl.ActiveTexture(GL_TEXTURE0);
        }
        gl.BindVertexArray(item.mesh->vao);
        glDrawElements(GL_TRIANGLES, item.mesh->index_count, GL_UNSIGNED_INT, nullptr);
        if (blend) {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
    }
    gl.BindVertexArray(0);
    gl.Uniform1i(use_texture2_loc, 0);
    gl.Uniform1i(modulate2x_loc, 0);
    gl.Uniform2f(uv_offset_loc, 0.0f, 0.0f);
    gl.UseProgram(0);
}

void build_skin_mapping(const dino::Skin& skin, const dino::Skeleton& skeleton,
                        std::vector<int>& vertex_bone, std::vector<int>& bone_skel) {
    vertex_bone.assign(skin.mesh.positions.size(), -1);
    bone_skel.assign(skin.bones.size(), -1);
    for (std::size_t b = 0; b < skin.bones.size(); ++b) {
        const int index = skeleton.index_of(skin.bones[b].name);
        if (index < 0) {
            throw std::runtime_error("skin bone '" + skin.bones[b].name +
                                     "' is not in the skeleton");
        }
        bone_skel[b] = index;
        for (const std::uint32_t v : skin.bones[b].vertices) {
            vertex_bone[v] = static_cast<int>(b);
        }
    }
}

dino::Mesh build_skinned_mesh(const dino::Mesh& bind, const dino::Skin& skin,
                              const dino::Skeleton& skeleton,
                              const dino::Animation* anim, double time,
                              const std::vector<int>& vertex_bone,
                              const std::vector<int>& bone_skel,
                              bool bind_from_skin) {
    std::vector<dino::Mat4> locals;
    locals.reserve(skeleton.bones.size());
    for (const auto& bone : skeleton.bones) {
        locals.push_back(bone.local);
    }
    if (anim != nullptr) {
        for (const auto& bone : anim->bones) {
            const int index = skeleton.index_of(bone.name);
            if (index >= 0) {
                locals[static_cast<std::size_t>(index)] = bone.sample(time);
            }
        }
    }
    const auto world = skeleton.world_matrices(locals);
    std::vector<dino::Mat4> raw;
    raw.reserve(world.size());
    for (const auto& matrix : world) {
        raw.push_back(dino::raw_centimetres(matrix));
    }

    // `mesh` bind: use the skeleton rest inverse so the standing .mesh is
    // exact at rest for every LOD (pachy LOD2/LOD3 store a bent-neck bind).
    // `skin` bind: the source mesh is in the skin's bind pose, so use the
    // stored inverse_bind.
    std::vector<dino::Mat4> rest_inverse;
    if (!bind_from_skin) {
        const auto rest_world = skeleton.world_matrices();
        rest_inverse.reserve(rest_world.size());
        for (const auto& matrix : rest_world) {
            rest_inverse.push_back(dino::inverse(dino::raw_centimetres(matrix)));
        }
    }

    std::vector<dino::Mat4> skin_matrices(skin.bones.size());
    for (std::size_t i = 0; i < skin.bones.size(); ++i) {
        const int skel_index = bone_skel[i];
        if (skel_index < 0) {
            skin_matrices[i] = dino::identity();
            continue;
        }
        const dino::Mat4& bind_matrix =
            bind_from_skin
                ? skin.bones[i].inverse_bind
                : rest_inverse[static_cast<std::size_t>(skel_index)];
        skin_matrices[i] = dino::multiply(
            bind_matrix, raw[static_cast<std::size_t>(skel_index)]);
    }

    dino::Mesh out = bind;
    const bool has_normals = bind.normals.size() == bind.positions.size();
    for (std::size_t v = 0; v < bind.positions.size(); ++v) {
        const int bone = vertex_bone[v];
        dino::Vec3 p = bind.positions[v];
        dino::Vec3 n = has_normals ? bind.normals[v] : dino::Vec3{0, 0, 1};
        if (bone >= 0) {
            const dino::Mat4& m = skin_matrices[static_cast<std::size_t>(bone)];
            p = dino::transform_point(m, p);
            n = dino::normalize(dino::transform_direction(m, n));
        }
        out.positions[v] = {p.x * 0.01f, p.y * 0.01f, p.z * 0.01f};
        if (has_normals) {
            out.normals[v] = n;
        }
    }
    return out;
}

std::string resolve_asset(const std::string& root, const std::string& path) {
    return dino::asset_path(root, path);
}

// Action -> animation clip. The retail link is still open (the `.bai` files
// carry no clip name); these are the name-based candidates documented in
// `notes/dino-action-chooser-recon.md` and are marked inferred, not verified.
std::string dino_clip_for_action(const std::string& action) {
    if (action == "Sleep") {
        return "Sleep";
    }
    if (action == "Eat" || action == "EatForSmallies" ||
        action.rfind("EatBait", 0) == 0) {
        return "Chew";
    }
    if (action == "Smell") {
        return "Sniff";
    }
    if (action == "Exploration" || action == "ReachWaypoint") {
        return "Walk";
    }
    if (action == "RunAway") {
        return "Run";
    }
    if (action == "Victory") {
        return "Winner";
    }
    if (action == "Danse") {
        return "Dance";
    }
    return "Neutral";
}

// One `--show NAME[:I,J]` test hook. The retail `.shw` "Judge Kind" is the
// show-type id read by `FUN_00567190` (1 race, 2 obstacle, 3 fight,
// 4 destruction, 5 team, 6 capture, 7 fence). `participants` are indices into
// the imported-dino vector.
struct PortShowSpec {
    std::string name;
    int type = 0;
    std::vector<int> participants;
};

std::vector<PortShowSpec> parse_show_specs(
    const std::vector<std::string>& specs, const std::string& asset_root) {    std::vector<PortShowSpec> shows;
    for (const std::string& spec : specs) {
        const std::size_t colon = spec.find(':');
        PortShowSpec show;
        show.name = spec.substr(0, colon);
        if (colon != std::string::npos) {
            const char* cursor = spec.c_str() + colon + 1;
            while (*cursor != '\0') {
                char* next = nullptr;
                const long value = std::strtol(cursor, &next, 10);
                if (next == cursor) {
                    ++cursor;
                    continue;
                }
                show.participants.push_back(static_cast<int>(value));
                cursor = next;
            }
        }
        std::string path = show.name;
        if (!std::filesystem::is_regular_file(path)) {
            path = resolve_asset(asset_root,
                                 "Data/Design/Scenario/Show/" + show.name +
                                     ".shw");
        }
        try {
            show.type = dino::load_shw(path).judge_kind;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "show %s: %s\n", show.name.c_str(), e.what());
        }
        shows.push_back(std::move(show));
    }
    return shows;
}

// `NAME:V1,V2` test-hook argument used by --place-show / --show-participants
// / --show-hour / --show-iterations.
struct PortNameSpec {
    std::string name;
    std::vector<int> values;
};

PortNameSpec parse_name_spec(const std::string& spec) {
    PortNameSpec out;
    const std::size_t colon = spec.find(':');
    out.name = spec.substr(0, colon);
    if (colon == std::string::npos) {
        return out;
    }
    const char* cursor = spec.c_str() + colon + 1;
    while (*cursor != '\0') {
        char* next = nullptr;
        const long value = std::strtol(cursor, &next, 10);
        if (next == cursor) {
            ++cursor;
            continue;
        }
        out.values.push_back(static_cast<int>(value));
        cursor = next;
    }
    return out;
}

// Calendar label for a show hour slot: 1 -> 10:00 AM, 10 -> 07:00 PM.
// The retail day starts at 9:00 (spScheduleManager ctor), and slot 0 is the
// 9:00 hour, which has no show (the show window starts one hour later).
std::string show_slot_text(int slot) {
    if (slot < 1 || slot > 10) {
        return "--:--";
    }
    const int hour = 9 + slot;
    int display = hour % 12;
    if (display == 0) {
        display = 12;
    }
    char text[16];
    std::snprintf(text, sizeof(text), "%02d:00 %s", display,
                  hour < 12 ? "AM" : "PM");
    return text;
}

// Free-game easy profile table (`FreeGame.ini` `[PROFILES]`):
// retraite, surfer, parent, enfant, holligan, scientific - 30 each.
std::vector<dino::VisitorProfile> load_easy_visitor_profiles(
    const std::string& asset_root) {
    static const char* kFiles[6] = {
        "OldMan.bvi",     "SurferMan.bvi", "ParentMan.bvi",
        "KidMan.bvi",     "HooliganMan.bvi", "ScientistMan.bvi"};
    std::vector<dino::VisitorProfile> profiles;
    profiles.reserve(6);
    for (const char* file : kFiles) {
        dino::VisitorProfile profile = dino::load_bvi(resolve_asset(
            asset_root, std::string("Data/Design/visitors/") + file));
        profile.spawn_weight = 30;
        profile.spawn_cap = 100;
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

// Full 10-profile `.bvi` table, indexed by the global personality id
// (`VisitorList.ini` order 0..9). Used for the per-visitor ticket scale of
// saved visitors, whose `drsVisitor.field_20` is this global id.
std::vector<dino::VisitorProfile> load_all_visitor_profiles(
    const std::string& asset_root) {
    static const char* kFiles[10] = {
        "SurferMan.bvi",   "HooliganMan.bvi", "ParentMan.bvi",
        "BlaseMan.bvi",    "OldMan.bvi",      "RicheMan.bvi",
        "ScientistMan.bvi", "FanMan.bvi",     "SpdMan.bvi",
        "KidMan.bvi"};
    std::vector<dino::VisitorProfile> profiles;
    profiles.reserve(10);
    for (const char* file : kFiles) {
        profiles.push_back(dino::load_bvi(resolve_asset(
            asset_root, std::string("Data/Design/visitors/") + file)));
    }
    return profiles;
}

// `Data/Design/landscape/IslandList.ini`: count then one folder name per line.
// The original reads this in gdtIslandDataBase (FUN_00506530) and builds
// `Data\Design\Landscape\<name>\<name>.lnd` from the selected record.
std::vector<std::string> read_island_list(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open island list: " + path);
    }
    std::vector<std::string> names;
    int count = -1;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (count < 0) {
            count = std::atoi(line.c_str());
            continue;
        }
        names.push_back(line);
        if (static_cast<int>(names.size()) == count) {
            break;
        }
    }
    if (count <= 0 || static_cast<int>(names.size()) != count) {
        throw std::runtime_error("bad island list: " + path);
    }
    return names;
}

// Ray-marches the heightfield from the camera through a screen pixel.
bool pick_terrain(const dino::Terrain& terrain, const dino::Vec3& eye,
                  const dino::Vec3& target, float mouse_x, float mouse_y,
                  int width, int height, dino::Vec3& hit) {
    if (terrain.world_size <= 0.0f) {
        return false;
    }
    const dino::Vec3 forward = dino::normalize({target.x - eye.x, target.y - eye.y,
                                                target.z - eye.z});
    const dino::Vec3 right = dino::normalize(cross(forward, {0.0f, 0.0f, 1.0f}));
    const dino::Vec3 up = cross(right, forward);
    const float tan_half = std::tan(33.0f * 3.14159265f / 360.0f);
    const float aspect = static_cast<float>(width) / static_cast<float>(std::max(1, height));
    const float ndc_x = (2.0f * mouse_x / static_cast<float>(width) - 1.0f) *
                        tan_half * aspect;
    const float ndc_y = (1.0f - 2.0f * mouse_y / static_cast<float>(height)) * tan_half;
    const dino::Vec3 dir = dino::normalize({forward.x + right.x * ndc_x + up.x * ndc_y,
                                            forward.y + right.y * ndc_x + up.y * ndc_y,
                                            forward.z + right.z * ndc_x + up.z * ndc_y});

    auto at = [&](float t) {
        return dino::Vec3{eye.x + dir.x * t, eye.y + dir.y * t, eye.z + dir.z * t};
    };
    auto above = [&](const dino::Vec3& p) {
        return p.z - terrain.height_at(p.x, p.y);
    };
    float previous_t = 0.5f;
    for (float t = 1.0f; t < 2500.0f; t += 2.0f) {
        const dino::Vec3 p = at(t);
        if (above(p) <= 0.0f) {
            float lo = previous_t;
            float hi = t;
            for (int i = 0; i < 24; ++i) {
                const float mid = (lo + hi) * 0.5f;
                if (above(at(mid)) <= 0.0f) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            hit = at(hi);
            const float half = terrain.world_size * 0.5f;
            return hit.x >= -half && hit.x <= half && hit.y >= -half && hit.y <= half;
        }
        previous_t = t;
    }
    return false;
}

// Moves a mesh (metres) onto the terrain (world units) at an .imp-style
// position: x/y in 0..1024, Z rotation in degrees.
// A disc at ground level so a placed model reads as standing on the terrain.
dino::Mesh make_contact_shadow(float x, float y, float z, float radius) {
    constexpr int kSegments = 40;
    dino::Mesh mesh;
    mesh.name = "contact_shadow";
    const dino::Color color{26, 36, 22, 255};
    mesh.positions.push_back({x, y, z});
    mesh.normals.push_back({0.0f, 0.0f, 1.0f});
    mesh.uvs.push_back({0.0f, 0.0f});
    mesh.colors.push_back(color);
    for (int i = 0; i < kSegments; ++i) {
        const float angle = 2.0f * 3.14159265f * static_cast<float>(i) /
                            static_cast<float>(kSegments);
        mesh.positions.push_back({x + std::cos(angle) * radius,
                                  y + std::sin(angle) * radius, z});
        mesh.normals.push_back({0.0f, 0.0f, 1.0f});
        mesh.uvs.push_back({0.0f, 0.0f});
        mesh.colors.push_back(color);
    }
    for (int i = 0; i < kSegments; ++i) {
        mesh.indices.push_back(0);
        mesh.indices.push_back(static_cast<std::uint32_t>(1 + i));
        mesh.indices.push_back(static_cast<std::uint32_t>(1 + (i + 1) % kSegments));
    }
    return mesh;
}

dino::Mesh place_on_terrain(dino::Mesh mesh, const dino::Terrain& terrain,
                            float x, float y, float angle_degrees) {
    const float half = terrain.world_size * 0.5f;
    const float base_z = terrain.height_at(x - half, y - half);
    const float radians = angle_degrees * 3.14159265f / 180.0f;
    const float cs = std::cos(radians);
    const float sn = std::sin(radians);
    const bool has_normals = mesh.normals.size() == mesh.positions.size();
    for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
        const dino::Vec3 p = mesh.positions[i];
        mesh.positions[i] = {x - half + cs * p.x - sn * p.y,
                             y - half + sn * p.x + cs * p.y,
                             base_z + p.z};
        if (has_normals) {
            const dino::Vec3 n = mesh.normals[i];
            mesh.normals[i] = dino::normalize({cs * n.x - sn * n.y,
                                               sn * n.x + cs * n.y,
                                               n.z});
        }
    }
    return mesh;
}

int run_selftest() {
    int checks = 0;
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        ++checks;
        if (!ok) {
            std::printf("selftest: FAIL %s\n", what);
            ++failures;
        }
    };

    dino::BuildGrid grid;
    grid.init(4.0f, 1024.0f);
    check(grid.width == 256 && grid.height == 256, "grid size");
    check(grid.origin_x == -512.0f && grid.origin_y == -512.0f, "grid origin");
    check(!grid.path_at(10, 10), "grid empty");

    grid.set_path(10, 10, true);
    check(grid.path_count == 1 && grid.path_at(10, 10), "path set");
    grid.set_path(10, 10, true);
    check(grid.path_count == 1, "path idempotent");
    grid.set_fence(10, 10, 0, true);
    check(grid.fence_count == 1 && grid.fence_at(10, 10, 0), "fence south");
    grid.set_fence(10, 10, 1, true);
    check(grid.fence_count == 2 && grid.fence_at(10, 10, 1), "fence east");
    grid.set_fence(10, 10, 0, false);
    check(grid.fence_count == 1 && !grid.fence_at(10, 10, 0), "fence clear");
    check(grid.undo() && grid.fence_count == 2 && grid.fence_at(10, 10, 0),
          "undo restores south fence");
    check(grid.undo() && grid.fence_count == 1, "undo restores east fence");

    int cx = 0;
    int cy = 0;
    check(grid.cells_for_world(-512.0f + 10.5f * 4.0f,
                               -512.0f + 10.5f * 4.0f, cx, cy) &&
              cx == 10 && cy == 10, "world to cell");

    // Retail road tool (notes/road-construction-recon.md).
    {
        const dino::RoadSegment h = dino::road_segment(20, 20, 26, 22);
        check(h.x1 == 26 && h.y1 == 20 && h.cells() == 7, "road axis lock x");
        const dino::RoadSegment v = dino::road_segment(20, 20, 22, 22);
        check(v.x1 == 20 && v.y1 == 22, "road axis tie goes vertical");
        check(dino::road_cost(h, 10) == 60, "road cost 10 per extra cell");
        check(dino::road_cost(dino::road_segment(26, 20, 20, 20), 10) == 60,
              "road cost toward -x");
        dino::BuildGrid roads;
        roads.init(4.0f, 1024.0f);
        const std::vector<dino::WorldRect> pens = {{100, 100, 140, 140}};
        auto none = [](int, int) { return false; };
        check(dino::check_road(roads, dino::road_segment(5, 5, 5, 5), {}, none) ==
                  dino::RoadCheck::TooShort,
              "road too short");
        check(dino::check_road(roads, dino::road_segment(27, 27, 30, 27), pens,
                               none) == dino::RoadCheck::InsidePerimeter,
              "road inside perimeter");
        check(dino::check_road_start(roads, 30, 30, pens, none) ==
                  dino::RoadCheck::InsidePerimeter,
              "road start inside perimeter");
        auto blocked = [](int x, int y) { return x == 42 && y == 40; };
        check(dino::check_road(roads, dino::road_segment(40, 40, 45, 40), {},
                               blocked) == dino::RoadCheck::BusySpace,
              "road busy cell");
        roads.set_fence(42, 41, 0, true);  // edge between (42,40) and (42,41)
        check(dino::check_road(roads, dino::road_segment(42, 38, 42, 44), {},
                               none) == dino::RoadCheck::BusySpace,
              "road crosses fence");
        const dino::RoadSegment ok = dino::road_segment(60, 60, 60, 64);
        check(dino::check_road(roads, ok, pens, none) == dino::RoadCheck::Ok &&
                  dino::place_road(roads, ok) == 5 && roads.path_count == 5,
              "road placed");
        auto everything_busy = [](int, int) { return true; };
        check(dino::check_road(roads, dino::road_segment(60, 60, 60, 64), {},
                               everything_busy) == dino::RoadCheck::Ok,
              "road may run over roads");

        // Road layer 0x602380: cells (10,10)+(11,10) = units x 40..47,
        // y 40..43 -> top-down rows 980..983.
        dino::BuildGrid painted;
        painted.init(4.0f, 1024.0f);
        painted.set_path(10, 10, true);
        painted.set_path(11, 10, true);
        dino::Image map;
        map.width = map.height = 1024;
        map.rgba.assign(1024u * 1024u * 4u, 0);
        dino::paint_roads(map, painted, 1024.0f);
        auto rgb = [&map](int x, int y) {
            const std::uint8_t* p = &map.rgba[(static_cast<std::size_t>(y) * 1024 + x) * 4];
            return std::array<int, 3>{p[0], p[1], p[2]};
        };
        const std::array<int, 3> fill = {253, 198, 137};
        const std::array<int, 3> edge = {170, 130, 90};
        check(rgb(41, 981) == fill && rgb(43, 981) == fill && rgb(44, 981) == fill,
              "road paint fill joins cells");
        check(rgb(40, 981) == edge && rgb(47, 981) == edge && rgb(41, 980) == edge &&
                  rgb(41, 983) == edge,
              "road paint border on open sides");
        check(rgb(41, 979) == std::array<int, 3>{0, 0, 0} &&
                  rgb(41, 41) == std::array<int, 3>{0, 0, 0},
              "road paint y flipped");
        dino::Image small;
        small.width = small.height = 512;
        small.rgba.assign(512u * 512u * 4u, 0);
        dino::paint_roads(small, painted, 1024.0f);
        const std::uint8_t* sp = &small.rgba[(static_cast<std::size_t>(490) * 512 + 20) * 4];
        check(sp[0] == 253 && sp[1] == 198 && sp[2] == 137, "road paint small map fill only");
    }

    dino::Paddock paddock;
    paddock.rect = {0, 0, 31, 31};
    paddock.capacity = dino::paddock_capacity(paddock.rect, 4.0f);
    check(paddock.capacity == 64.0f, "paddock capacity");
    check(dino::paddock_cost(0) == 8.0f && dino::paddock_cost(1) == 16.0f &&
              dino::paddock_cost(3) == 64.0f, "paddock cost table");
    check(dino::paddock_size_class(200) == 0 &&
              dino::paddock_size_class(620) == 1 &&
              dino::paddock_size_class(900) == 2 &&
              dino::paddock_size_class(901) == 3, "paddock size class");
    check(dino::paddock_add(paddock, 1, 0) && paddock.used == 8.0f,
          "paddock add");
    check(!dino::paddock_add(paddock, 1, 0), "paddock duplicate");
    for (int id = 2; id <= 8; ++id) {
        dino::paddock_add(paddock, id, 0);
    }
    check(paddock.used == 64.0f && paddock.fill() == 1.0f, "paddock full");
    check(!dino::paddock_add(paddock, 9, 0), "paddock no room");

    dino::Terrain basis_terrain;
    basis_terrain.width = 16;
    basis_terrain.height = 16;
    basis_terrain.world_size = 1024.0f;
    basis_terrain.height_scale = 1.0f;
    basis_terrain.heights.assign(16 * 16, 0);
    const dino::TerrainBasis flat_basis =
        dino::slope_basis(basis_terrain, 0.0f, 0.0f, 0.0f);
    check(flat_basis.normal.z > 0.999f && flat_basis.right.x > 0.999f &&
              flat_basis.up.y > 0.999f, "slope basis flat");
    dino::Terrain sloped = basis_terrain;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            sloped.heights[static_cast<std::size_t>(y) * 16 + x] =
                static_cast<std::uint8_t>(x * 8);
        }
    }
    const dino::TerrainBasis sloped_basis =
        dino::slope_basis(sloped, 0.0f, 0.0f, 0.0f);
    check(sloped_basis.normal.x < -0.05f &&
              sloped_basis.normal.z > 0.9f, "slope basis tilted");

    dino::Terrain terrain;
    terrain.width = 16;
    terrain.height = 16;
    terrain.world_size = 1024.0f;
    terrain.height_scale = 1.0f;
    terrain.heights.assign(16 * 16, 0);
    terrain.occupation_width = 1024;
    terrain.occupation_height = 1024;
    terrain.occupation.assign(1024 * 1024, 255);
    terrain.occupation[512 * 1024 + 512] = 0;  // world (0, 0)
    terrain.occupation[100 * 1024 + 100] = 0;  // world (-412, -412)
    check(terrain.buildable(0.0f, 0.0f) &&
              terrain.buildable(-412.0f, -412.0f) &&
              !terrain.buildable(10.0f, 0.0f) &&
              !terrain.buildable(-1000.0f, 0.0f),
          "occupation mask");

    for (int i = 100; i <= 110; ++i) {
        grid.set_path(i, 100, true);
        grid.set_path(i, 110, true);
        grid.set_path(100, i, true);
        grid.set_path(110, i, true);
    }
    check(grid.path_count == 41, "loop cells (40 + one earlier)");

    dino::VisitorSystem visitors;
    check(visitors.spawn(grid, 12) == 12, "visitor spawn count");
    const float start_x = visitors.visitors.empty() ? 0.0f : visitors.visitors[0].x;
    const float start_y = visitors.visitors.empty() ? 0.0f : visitors.visitors[0].y;
    for (int i = 0; i < 900; ++i) {
        visitors.update(grid, terrain, 1.0f / 60.0f);
    }
    bool moved = false;
    bool on_path = true;
    for (const auto& visitor : visitors.visitors) {
        if (std::fabs(visitor.x - start_x) > 0.1f ||
            std::fabs(visitor.y - start_y) > 0.1f) {
            moved = true;
        }
        if (!grid.path_at(visitor.cell_x, visitor.cell_y)) {
            on_path = false;
        }
    }
    check(moved, "visitors moved");
    check(on_path, "visitors stay on path cells");

    // SIM-2b: exact retail score and the choose -> travel -> use loop.
    dino::VisitorProfile ai_profile;
    ai_profile.weights = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    ai_profile.walk_speed = 10.0f;
    dino::Attraction ai_attraction;
    ai_attraction.world_x = -512.0f + 105.5f * 4.0f;
    ai_attraction.world_y = -512.0f + 100.5f * 4.0f;
    ai_attraction.radius = 1000.0f;
    ai_attraction.appeal_scale = 1.0f;
    ai_attraction.appeal_weights = {10.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    ai_attraction.need_delta1 = 10;
    ai_attraction.duration = 0.1f;
    ai_attraction.category = 1;
    check(dino::attraction_score(ai_attraction, ai_profile, 99, 50, 99, 10,
                                 0) == 21010,
          "attraction score");
    dino::BuildGrid ai_grid;
    ai_grid.init(4.0f, 1024.0f);
    for (int x = 100; x <= 110; ++x) {
        ai_grid.set_path(x, 100, true);
    }
    dino::VisitorSystem ai_visitors;
    check(ai_visitors.spawn(ai_grid, 1, {ai_profile}, -1, -1, 7) == 1,
          "ai visitor spawn");
    dino::Visitor& ai_visitor = ai_visitors.visitors[0];
    ai_visitor.cell_x = 100;
    ai_visitor.cell_y = 100;
    ai_visitor.target_x = 100;
    ai_visitor.target_y = 100;
    ai_visitor.x = -512.0f + 100.5f * 4.0f;
    ai_visitor.y = -512.0f + 100.5f * 4.0f;
    ai_visitor.need0 = 50;
    ai_visitor.need1 = 50;
    ai_visitor.need2 = 99;
    ai_visitor.state = dino::VisitorState::Choose;
    ai_visitor.rng = 1;
    for (int i = 0; i < 30; ++i) {
        ai_visitors.update(ai_grid, terrain, 0.1f, {ai_attraction}, -1, -1,
                           {ai_profile});
    }
    check(!ai_visitors.visitors.empty(), "ai visitor survives");
    check(ai_visitors.visitors[0].need1 > 50, "ai need1 gained");
    check(ai_visitors.visitors[0].need0 <= 50, "ai need0 adjusted");

    const dino::Mesh empty_pose;
    check(visitors.build_mesh(grid, terrain, empty_pose, empty_pose)
              .positions.empty(), "empty pose builds empty mesh");

    dino::Mesh fence_source;
    fence_source.positions = {{0, 0, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}};
    fence_source.indices = {0, 1, 2, 0, 2, 3};
    grid.set_fence(10, 10, 0, true);
    const dino::BuildMeshes meshes =
        dino::build_placement_meshes(grid, terrain, fence_source);
    check(meshes.path_tiles == grid.path_count, "path mesh tiles");
    check(meshes.fence_segments == grid.fence_count, "fence mesh segments");

    // SECU danger grid (`FUN_005774F0` / `FUN_00571B50` / `FUN_00571440`).
    dino::DangerGrid danger;
    danger.clear();
    danger.stamp(100.0f, 100.0f, 4, 50);
    check(danger.at(100.0f, 100.0f) == 50, "danger grid stamp");
    check(danger.at(108.0f, 100.0f) == 50, "danger grid radius");
    check(danger.at(112.0f, 100.0f) == 0, "danger grid edge");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 50, 100, 100) == 99,
          "secu full");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 100, 100) == 94,
          "secu threat");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 50, 100) == 44,
          "secu health");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 25, 100) == 19,
          "secu damage");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 0, 100) == 0,
          "secu clamp");
    check(dino::secu_from_grid(danger, 100.0f, 100.0f, 0, 0, 0) == 94,
          "secu no max health");

    // Campaign objective evaluator (`snoObjective` 0x5654f0 / verb 0x5658d0).
    auto objective = [](const char* command) {
        return dino::parse_objective_command(command);
    };
    dino::CampaignState objective_state;
    objective_state.treasury = 25000;
    check(dino::evaluate_objective(objective("treasury 25000 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective treasury success");
    objective_state.treasury = 100;
    check(dino::evaluate_objective(objective("treasury 25000 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective treasury pending");
    check(dino::evaluate_objective(objective("treasury 25000 0"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Failure,
          "objective treasury failure");
    objective_state.merchandise_income = 50001;
    check(dino::evaluate_objective(objective("marchandizing 50000"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective marchandizing");
    objective_state.deaths = 3;
    check(dino::evaluate_objective(objective("dyingdinos 5"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective dyingdinos success");
    objective_state.deaths = 6;
    check(dino::evaluate_objective(objective("dyingdinos 5"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Failure,
          "objective dyingdinos failure");

    dino::CampaignVisitorState kid;
    kid.profile = "kid";
    kid.satisfaction = 80;
    dino::CampaignVisitorState adult;
    adult.profile = "adult";
    adult.satisfaction = 90;
    objective_state.visitors = {kid, adult};
    check(dino::evaluate_objective(objective("visitors all 2"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective visitors all");
    check(dino::evaluate_objective(objective("visitors kid 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective visitors profile");
    check(dino::evaluate_objective(objective("visitors all 3"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective visitors pending");
    check(dino::evaluate_objective(objective("happyvisitors all 80"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective happy visitors");
    objective_state.employee_roles = {"nutritionist", "nutritionist"};
    check(dino::evaluate_objective(objective("employees all 2"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective employees");
    objective_state.camera_altitude = 120;
    check(dino::evaluate_objective(objective("cameraaltitude 120"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective camera altitude");
    objective_state.clock_minutes = 5;
    objective_state.load_minutes = 0;
    check(dino::evaluate_objective(objective("delaylimit 10"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective delay limit pending");
    objective_state.clock_minutes = 11;
    check(dino::evaluate_objective(objective("delaylimit 10"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Failure,
          "objective delay limit failure");
    objective_state.store_empty = true;
    check(dino::evaluate_objective(objective("buyalldinos"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective buy all dinos");
    objective_state.bankrupt = true;
    check(dino::evaluate_objective(objective("bankrupt"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Failure,
          "objective bankrupt");

    dino::DinoTraitState trait;
    trait.species = "Hypsilophodon";
    trait.cuteness = 60;
    trait.aggression = 90;
    trait.hybrid = 95;
    trait.speed = 30;
    trait.stamina = 70;
    trait.agility = 50;
    trait.intelligence = 80;
    trait.size_class = 2;
    trait.food = 70;
    trait.beauty = 80;
    trait.strangeness = 10;
    trait.ornament_count = 1;
    trait.isolated = true;
    trait.carnivore = true;
    trait.biped = true;
    trait.age = 10;
    dino::DinoTraitState hungry = trait;
    hungry.food = 50;
    hungry.aggression = 0;
    hungry.ornament_count = 0;
    hungry.hybrid = 0;
    objective_state.dinos = {trait, hungry};
    check(dino::evaluate_objective(objective("aggressivedinos 1 80"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective aggressive dinos");
    check(dino::evaluate_objective(objective("aggressivedinos 2 80"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective aggressive count");
    check(dino::evaluate_objective(objective("cutedinos 1 50"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective cute dinos");
    check(dino::evaluate_objective(objective("hybriddinos 1 90"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective hybrid dinos");
    check(dino::evaluate_objective(objective("hybriddinos 2 90"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective hybrid count");
    check(dino::evaluate_objective(objective("speeddinos 1 25"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective speed dinos");
    check(dino::evaluate_objective(objective("staminadinos 1 60"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective stamina dinos");
    check(dino::evaluate_objective(objective("cleverdinos 1 70"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective clever dinos");
    check(dino::evaluate_objective(objective("hungrydinos 1 60"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective hungry one");
    check(dino::evaluate_objective(objective("hungrydinos -1 60"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective hungry all");
    objective_state.buildings = {{8, true}, {11, false}, {6, true}};
    objective_state.fence_count = 2;
    check(dino::evaluate_objective(objective("connectedbuildings farm 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective connected farm");
    check(dino::evaluate_objective(
              objective("connectedbuildings lunchdoor 1"), objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective connected lunchdoor");
    check(dino::evaluate_objective(
              objective("notconnectedbuildings lunchdoor 1"),
              objective_state) == dino::ObjectiveVerdict::Success,
          "objective notconnected lunchdoor");
    check(dino::evaluate_objective(objective("connectedbuildings buildings 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective connected buildings");
    check(dino::evaluate_objective(objective("connectedbuildings buildings 2"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective connected buildings count");
    check(dino::evaluate_objective(objective("connectedbuildings show 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective connected show");
    check(dino::evaluate_objective(objective("connectedbuildings fence 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective connected fence");
    check(dino::evaluate_objective(objective("isolateddinos 71"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective isolated dinos");
    dino::CampaignState crowded = objective_state;
    crowded.dinos[0].isolated = false;
    check(dino::evaluate_objective(objective("isolateddinos 71"), crowded) ==
              dino::ObjectiveVerdict::Pending,
          "objective isolated crowded");
    check(dino::evaluate_objective(objective("sizedagilitydinos 1 40"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective size agility");
    check(dino::evaluate_objective(objective("sizedagilitydinos 3 40"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective size class threshold");
    check(dino::evaluate_objective(objective("franckstrangedinos 20 30"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective franck strange");
    check(dino::evaluate_objective(objective("franckstrangedinos 5 30"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective franck age");
    check(dino::evaluate_objective(objective("ornateddinos 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective ornated dinos");
    check(dino::evaluate_objective(objective("ornateddinos 2"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Pending,
          "objective ornated count");
    check(dino::evaluate_objective(objective("prettystrangedino 70 20"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective pretty strange");
    check(dino::evaluate_objective(objective("genotypedinos hypsilo 1"),
                                   objective_state) ==
              dino::ObjectiveVerdict::Success,
          "objective genotype species");

    // `FUN_00618020` five show needs from the retail constants.
    {
        dino::DinoPhenotypeTable needs_table;
        dino::DinoAttributes attrs;
        attrs.beauty = 50;
        attrs.attack_head = 40;
        attrs.aggression = 20;
        attrs.attack_total = 100;
        attrs.length_cm = 1000;
        attrs.width_cm = 500;
        attrs.height_cm = 200;
        attrs.conventional_appeal = 60;
        attrs.intelligence = 50;
        const std::array<float, 5> needs =
            dino::dino_show_needs(attrs, needs_table);
        const auto close_to = [](float value, float expected) {
            return std::fabs(value - expected) < 0.05f;
        };
        check(close_to(needs[0], 30.0f), "show need beauty");
        check(close_to(needs[1], 13.5f), "show need danger");
        check(close_to(needs[2], 20.69f), "show need spectacle");
        check(close_to(needs[3], 30.0f), "show need conventional");
        check(close_to(needs[4], 34.5f), "show need taming");
    }

    // Show objectives (`0x567190`, `0x5678b0`, `0x5692a0`).
    {
        dino::CampaignShowState show;
        show.type = 1;
        show.participants = 3;
        show.add_score(0, 65.0f);
        show.add_score(1, 70.0f);
        show.add_score(2, 55.0f);
        show.participant_ids = {0, 1, 2};
        show.participant_hybrid = {50, 90, 91};
        dino::CampaignState show_state;
        show_state.shows = {show};
        check(dino::evaluate_objective(objective("happyshow one 60"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow one");
        check(dino::evaluate_objective(objective("happyshow all 60"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective happyshow all");
        check(dino::evaluate_objective(objective("happyshow all 50"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow all pass");
        check(dino::evaluate_objective(objective("happyshow holligan 65"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow holligan");
        check(dino::evaluate_objective(objective("happyshow hooligan 65"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow hooligan alias");
        check(dino::evaluate_objective(objective("happyshow unknown 65"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happyshow unknown surfer");
        check(dino::evaluate_objective(objective("happyshow one 71"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective happyshow threshold");
        check(dino::evaluate_objective(objective("happydinoshow 50 2"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Success,
              "objective happydinoshow");
        check(dino::evaluate_objective(objective("happydinoshow 60 2"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective happydinoshow score");
        check(dino::evaluate_objective(objective("happydinoshow 50 4"),
                                       show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective happydinoshow count");
        check(dino::evaluate_objective(objective("fightshow 65"), show_state) ==
                  dino::ObjectiveVerdict::Pending,
              "objective fightshow type");
        dino::CampaignState fight = show_state;
        fight.shows[0].type = 3;
        check(dino::evaluate_objective(objective("fightshow 65"), fight) ==
                  dino::ObjectiveVerdict::Success,
              "objective fightshow");
        dino::CampaignState scientific = show_state;
        scientific.shows[0].add_score(6, 65.0f);
        scientific.shows[0].participant_hybrid = {50, 90};
        check(dino::evaluate_objective(objective("scientificshow 60 90"),
                                       scientific) ==
                  dino::ObjectiveVerdict::Success,
              "objective scientificshow");
        scientific.shows[0].participant_hybrid = {50, 91};
        check(dino::evaluate_objective(objective("scientificshow 60 90"),
                                       scientific) ==
                  dino::ObjectiveVerdict::Pending,
              "objective scientificshow cap");
        scientific.shows[0].participant_hybrid = {50, 90};
        scientific.shows[0].profile_scores.back().score = 60.0f;
        check(dino::evaluate_objective(objective("scientificshow 60 90"),
                                       scientific) ==
                  dino::ObjectiveVerdict::Pending,
              "objective scientificshow strict");
        dino::CampaignState exclusive = show_state;
        exclusive.shows.push_back(show);
        check(dino::evaluate_objective(objective("dinoexclusiveshow"),
                                       exclusive) ==
                  dino::ObjectiveVerdict::Pending,
              "objective exclusive shared");
        dino::CampaignState exclusive_ok = show_state;
        dino::CampaignShowState other;
        other.participants = 1;
        other.participant_ids = {5};
        exclusive_ok.shows.push_back(other);
        check(dino::evaluate_objective(objective("dinoexclusiveshow"),
                                       exclusive_ok) ==
                  dino::ObjectiveVerdict::Success,
              "objective exclusive disjoint");
        dino::CampaignState exclusive_dup = show_state;
        exclusive_dup.shows[0].participant_ids = {0, 0};
        check(dino::evaluate_objective(objective("dinoexclusiveshow"),
                                       exclusive_dup) ==
                  dino::ObjectiveVerdict::Success,
              "objective exclusive same show");
        dino::CampaignState exclusive_empty;
        check(dino::evaluate_objective(objective("dinoexclusiveshow"),
                                       exclusive_empty) ==
                  dino::ObjectiveVerdict::Success,
              "objective exclusive empty");
    }

    // Show runtime (`spScheduleManager` hour slots + show state machine).
    {
        dino::ShowCatalogEntry entry;
        entry.stem = "Cpg4_Derby_1";
        entry.name_id = "&cpg4_derby_1";
        entry.judge_kind = 4;
        entry.width = 71;
        entry.height = 35;

        std::vector<dino::ShowCatalogEntry> catalog = {entry};
        check(dino::find_show_catalog_index(catalog, "Cpg4_Derby_1") == 0,
              "show catalog stem");
        check(dino::find_show_catalog_index(catalog, "cpg4_derby_1") == 0,
              "show catalog name id bare");
        check(dino::find_show_catalog_index(catalog, "&CPG4_DERBY_1") == 0,
              "show catalog name id");
        check(dino::find_show_catalog_index(catalog, "missing") == -1,
              "show catalog miss");
        check(dino::show_catalog_entry_matches(entry, "&cpg4_derby_1"),
              "show catalog matches");

        dino::ShowRuntime runtime;
        const std::vector<int> profile_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        const int first = runtime.place(entry, 10, 20, profile_ids);
        check(first == 0, "show place index");
        check(runtime.count() == 1, "show count");
        const dino::ParkShow* placed = runtime.at(first);
        check(placed != nullptr && placed->slot == 1, "show first free slot");
        check(placed != nullptr && placed->cell_x == 10 &&
                  placed->cell_y == 20,
              "show placement cell");
        check(placed != nullptr && placed->record.profile_scores.size() == 10,
              "show zero-filled score vector");
        check(placed != nullptr && placed->record.profile_scores[0].score ==
                                       0.0f,
              "show zero-filled score value");
        check(placed != nullptr && placed->record.type == 4,
              "show record type");

        const int second = runtime.place(entry, 40, 20, profile_ids);
        check(second == 1 && runtime.at(second)->slot == 2,
              "show second slot");

        check(runtime.set_slot(0, 5), "show set slot");
        check(runtime.at(0)->slot == 5, "show slot value");
        check(!runtime.set_slot(0, 11), "show slot range");
        check(!runtime.set_iterations(0, 0), "show iteration range");
        check(runtime.set_iterations(0, 2), "show set iterations");
        check(runtime.set_participants(0, {3, 4}),
              "show set participants");
        check(runtime.at(0)->record.participants == 2,
              "show participant count");

        int judged = 0;
        int completed = 0;
        auto judge = [&](dino::ParkShow&) { ++judged; };
        runtime.set_duration_minutes(5.0f);
        // Hour 0 (9:00) has no show; hour 5 is the first scheduled one.
        completed = runtime.tick(0, 0, false, 0.0, 60.0, judge);
        check(completed == 0 && runtime.at(0)->state == 0,
              "show not started before slot");
        completed = runtime.tick(5, 0, false, 0.0, 60.0, judge);
        check(completed == 0 && runtime.at(0)->state == 2,
              "show started on slot hour");
        check(judged == 0, "show judge waits for performance");
        check(runtime.at(1)->state == 0, "show other slot idle");
        // 2.5 park minutes at time_scale 60 -> half the 5-minute performance.
        completed = runtime.tick(5, 0, false, 2.5, 60.0, judge);
        check(completed == 0 && judged == 0, "show performance in progress");
        completed = runtime.tick(5, 0, false, 2.5, 60.0, judge);
        check(completed == 1 && judged == 1, "show performance judged");
        check(runtime.at(0)->done == 1 && runtime.at(0)->state == 2,
              "show first of two rounds");
        completed = runtime.tick(5, 0, false, 5.0, 60.0, judge);
        check(completed == 1 && runtime.at(0)->done == 2 &&
                  runtime.at(0)->state == 0,
              "show iterations complete");
        // Next day: the schedule restarts from zero.
        runtime.tick(5, 0, true, 0.0, 60.0, judge);
        check(runtime.at(0)->done == 0 && runtime.at(0)->state == 0,
              "show day reset");
        completed = runtime.tick(5, 1, false, 0.0, 60.0, judge);
        check(completed == 0 && runtime.at(0)->state == 2,
              "show next day start");

        dino::ShowRuntime full;
        for (int i = 0; i < 10; ++i) {
            check(full.place(entry, i, 0, profile_ids) == i,
                  "show fill ten slots");
        }
        check(full.place(entry, 0, 0, profile_ids) == -1,
              "show slots exhausted");
        full.clear();
        check(full.count() == 0, "show clear");

        runtime.set_iterations(0, 1);
        const std::vector<dino::CampaignShowState> states =
            runtime.campaign_states();
        check(states.size() == 2, "show campaign states");
        check(states[0].participants == 2, "show campaign participants");
    }

    check(dino::aggregate_verdict({dino::ObjectiveVerdict::Success,
                                   dino::ObjectiveVerdict::Success}) ==
              dino::ObjectiveVerdict::Success,
          "objective aggregate success");
    check(dino::aggregate_verdict({dino::ObjectiveVerdict::Success,
                                   dino::ObjectiveVerdict::Failure}) ==
              dino::ObjectiveVerdict::Failure,
          "objective aggregate failure");

    if (failures == 0) {
        std::printf("selftest: PASS (%d checks)\n", checks);
    }
    return failures == 0 ? 0 : 1;
}

std::string join_args(const std::vector<std::string>& args) {
    std::string text;
    for (const std::string& arg : args) {
        if (!text.empty()) {
            text += " ";
        }
        text += arg;
    }
    return text.empty() ? "-" : text;
}

int run_campaign_info(const std::string& scenario_root) {
    try {
        const std::vector<dino::Campaign> campaigns =
            dino::load_campaigns(scenario_root);
        int missions = 0;
        int objectives = 0;
        int unknown = 0;
        for (const dino::Campaign& campaign : campaigns) {
            missions += static_cast<int>(campaign.missions.size());
            for (const dino::CampaignMission& mission : campaign.missions) {
                objectives += static_cast<int>(mission.objectives.size());
                for (const dino::CampaignObjective& objective :
                     mission.objectives) {
                    if (objective.objective_id < 0) {
                        ++unknown;
                    }
                }
            }
        }
        std::printf(
            "campaigns: %zu missions: %d objectives: %d unknown: %d verbs: %d\n",
            campaigns.size(), missions, objectives, unknown,
            dino::objective_verb_count());
        for (const dino::Campaign& campaign : campaigns) {
            std::printf(
                "campaign %s island=%s budget=%d sellback=%.2f missions=%zu "
                "world=%s title=%s\n",
                campaign.name.c_str(), campaign.island.c_str(),
                campaign.budget, campaign.sell_back, campaign.missions.size(),
                campaign.world_file.c_str(), campaign.title_id.c_str());
            for (const dino::CampaignMission& mission : campaign.missions) {
                std::printf("  mission %s objectives=%zu title=%s\n",
                            mission.name.c_str(), mission.objectives.size(),
                            mission.title_id.c_str());
                for (const dino::CampaignObjective& objective :
                     mission.objectives) {
                    std::printf("    objective id=%d verb=%s args=%s desc=%s\n",
                                objective.objective_id,
                                objective.verb.c_str(),
                                join_args(objective.args).c_str(),
                                objective.description_id.c_str());
                }
            }
        }
        return unknown == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "campaign info: %s\n", e.what());
        return 1;
    }
}

int run_campaign_coverage(const std::string& scenario_root) {
    try {
        const std::vector<dino::Campaign> campaigns =
            dino::load_campaigns(scenario_root);
        std::map<int, int> used;
        int total = 0;
        int implemented = 0;
        int pending = 0;
        int unknown = 0;
        for (const dino::Campaign& campaign : campaigns) {
            for (const dino::CampaignMission& mission : campaign.missions) {
                for (const dino::CampaignObjective& objective :
                     mission.objectives) {
                    ++total;
                    if (objective.objective_id < 0) {
                        ++unknown;
                        continue;
                    }
                    ++used[objective.objective_id];
                    if (dino::objective_verb_implemented(
                            objective.objective_id)) {
                        ++implemented;
                    } else {
                        ++pending;
                    }
                }
            }
        }
        std::printf(
            "campaign coverage: %d objectives, %d implemented, %d pending, "
            "%d unknown\n",
            total, implemented, pending, unknown);
        for (int id = 0; id < dino::objective_verb_count(); ++id) {
            const auto it = used.find(id);
            const int count = it == used.end() ? 0 : it->second;
            const bool impl = dino::objective_verb_implemented(id);
            const char* status =
                impl ? "implemented" : (count > 0 ? "pending" : "unused");
            std::printf("verb id=%d %s objectives=%d %s\n", id,
                        dino::objective_verb_name(id), count, status);
        }
        return unknown == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "campaign coverage: %s\n", e.what());
        return 1;
    }
}

int run_campaign_mission_info(const std::string& mission_path) {
    try {
        const dino::CampaignMission mission =
            dino::load_campaign_mission(mission_path);
        std::printf("mission %s objectives=%zu title=%s\n",
                    mission.name.c_str(), mission.objectives.size(),
                    mission.title_id.c_str());
        for (const dino::CampaignObjective& objective : mission.objectives) {
            std::printf("objective id=%d verb=%s args=%s desc=%s\n",
                        objective.objective_id, objective.verb.c_str(),
                        join_args(objective.args).c_str(),
                        objective.description_id.c_str());
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "campaign mission: %s\n", e.what());
        return 1;
    }
}

}  // namespace

int main(int argc, char** argv) {
    Options opt = parse_args(argc, argv);

    if (opt.selftest) {
        return run_selftest();
    }
    if (!opt.video_info_path.empty()) {
        try {
            const dino::MpgInfo info = dino::probe_mpg(opt.video_info_path);
            std::printf(
                "video: %s width=%d height=%d duration=%.3f fps=%.3f\n",
                opt.video_info_path.c_str(), info.width, info.height,
                info.duration, info.frame_rate);
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "video info: %s\n", e.what());
            return 1;
        }
    }
    if (!opt.video_frame_path.empty()) {
        try {
            const dino::MpgFrame frame = dino::decode_mpg_frame(
                opt.video_frame_path, opt.video_frame_index);
            if (opt.video_out_path.empty()) {
                std::fprintf(stderr, "video frame: --video-out is required\n");
                return 2;
            }
            std::vector<std::uint8_t> rgba(
                static_cast<std::size_t>(frame.width) * frame.height * 4, 255);
            for (int y = 0; y < frame.height; ++y) {
                const std::uint8_t* src =
                    frame.rgb.data() +
                    static_cast<std::size_t>(frame.height - 1 - y) *
                        frame.width * 3;
                std::uint8_t* dst =
                    rgba.data() +
                    static_cast<std::size_t>(y) * frame.width * 4;
                for (int x = 0; x < frame.width; ++x) {
                    dst[x * 4 + 0] = src[x * 3 + 0];
                    dst[x * 4 + 1] = src[x * 3 + 1];
                    dst[x * 4 + 2] = src[x * 3 + 2];
                }
            }
            write_bmp(opt.video_out_path, frame.width, frame.height, rgba);
            std::printf("video frame: %s index=%d time=%.3f %dx%d\n",
                        opt.video_frame_path.c_str(), frame.index, frame.time,
                        frame.width, frame.height);
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "video frame: %s\n", e.what());
            return 1;
        }
    }
    if (!opt.campaign_coverage_dir.empty()) {
        return run_campaign_coverage(opt.campaign_coverage_dir);
    }
    if (!opt.campaign_info_dir.empty()) {
        return run_campaign_info(opt.campaign_info_dir);
    }
    if (!opt.campaign_mission_path.empty()) {
        return run_campaign_mission_info(opt.campaign_mission_path);
    }

    if (!opt.sav_info_path.empty()) {
        try {
            const dino::WldWorld world =
                dino::read_sav_world(opt.sav_info_path);
            const dino::SavSimulation sim =
                dino::read_sav_simulation(opt.sav_info_path);
            std::string name;
            for (std::size_t i = 0; i + 1 < sim.name.size(); i += 2) {
                const unsigned char lo = sim.name[i];
                const unsigned char hi = sim.name[i + 1];
                if (lo == 0 && hi == 0) {
                    break;
                }
                name.push_back(hi == 0 && lo >= 0x20
                                   ? static_cast<char>(lo)
                                   : '?');
            }
            std::printf("sav: version=%u slot=%u name='%s'\n", world.version,
                        sim.slot, name.c_str());
            const dino::SavWorldRecords& records = world.records;
            std::printf("sav world: campaign=%u mission=%u objectives=%zu\n",
                        records.campaign_id, records.mission_id,
                        records.objectives.size());
            std::printf(
                "sav objects: fences=%zu shows=%zu animals=%zu employees=%zu "
                "buildings=%zu visitors=%zu dinosaurs=%zu roads=%zu\n",
                records.fences.size(), records.shows.size(),
                records.animals.size(), records.employees.size(),
                records.buildings.size(), records.visitors.size(),
                records.dinosaurs.size(), sim.roads.size());
            std::printf(
                "sav flags: feature=%zu building=%zu food=%zu misc=%zu\n",
                records.feature_flags.size(), records.building_flags.size(),
                records.food_flags.size(), records.misc_flags.size());
            std::printf("sav catalog: %zu\n", sim.catalog.size());
            std::printf("sav park: budget=%d day=%d elapsed=%.3f\n",
                        sim.budget, sim.day, sim.elapsed);
            std::printf(
                "sav sim: visitors=%zu stores=%zu labs=%zu "
                "model=%d,%d,%d,%d,%d ai=%u,%u,%u,%u,%u "
                "camera=%.2f,%.2f,%.2f,%.2f\n",
                sim.visitors.size(), sim.dino_store.size(), sim.labs.size(),
                sim.visitor_model[0], sim.visitor_model[1],
                sim.visitor_model[2], sim.visitor_model[3],
                sim.visitor_model[4], sim.ai_counts[0], sim.ai_counts[1],
                sim.ai_counts[2], sim.ai_counts[3], sim.ai_counts[4],
                sim.camera[0], sim.camera[1], sim.camera[2], sim.camera[3]);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "sav info failed: %s\n", e.what());
            return 2;
        }
        return 0;
    }

    if (!opt.sav_roundtrip_path.empty()) {
        std::string error;
        bool ok = false;
        try {
            ok = dino::sav_park_roundtrip(opt.sav_roundtrip_path, &error);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "sav read failed: %s\n", e.what());
            return 2;
        }
        std::printf("sav roundtrip: %s%s\n",
                    ok ? "whole file identical" : "sav MISMATCH",
                    error.c_str());
        if (!opt.sav_write_path.empty()) {
            if (!ok && !opt.sav_force) {
                std::fprintf(stderr,
                             "sav write refused: rebuild differs (--sav-force "
                             "to write anyway)\n");
                return 1;
            }
            try {
                const std::string* new_name =
                    opt.sav_name.empty() ? nullptr : &opt.sav_name;
                const std::vector<std::uint8_t> rebuilt =
                    dino::rebuild_sav(opt.sav_roundtrip_path, new_name,
                                      opt.sav_slot, opt.sav_budget);
                std::ofstream out(opt.sav_write_path, std::ios::binary);
                if (!out) {
                    std::fprintf(stderr, "cannot open %s\n",
                                 opt.sav_write_path.c_str());
                    return 2;
                }
                out.write(reinterpret_cast<const char*>(rebuilt.data()),
                          static_cast<std::streamsize>(rebuilt.size()));
                if (!out) {
                    std::fprintf(stderr, "cannot write %s\n",
                                 opt.sav_write_path.c_str());
                    return 2;
                }
                std::printf("sav written: %zu bytes -> %s\n", rebuilt.size(),
                            opt.sav_write_path.c_str());
            } catch (const std::exception& e) {
                std::fprintf(stderr, "sav write failed: %s\n", e.what());
                return 2;
            }
        }
        return ok ? 0 : 1;
    }
    if (opt.wld_info) {
        if (opt.wld_path.empty()) {
            std::fprintf(stderr, "--wld-info requires --wld FILE\n");
            return 2;
        }
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
            std::vector<std::string> building_names;
            try {
                building_names = dino::load_building_list(dino::asset_path(
                    opt.asset_root, "Data/Design/buildings/BuildingList.ini"));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "building list: %s\n", e.what());
            }
            std::printf(
                "wld: %s version=%u mode=%u fences=%u shows=%u animals=%u "
                "employees=%u buildings=%zu visitors=%u dinosaurs=%u roads=%zu "
                "camera=%d\n",
                opt.wld_path.c_str(), world.version, world.mode,
                world.fence_count, world.show_count, world.animal_count,
                world.employee_count, world.buildings.size(),
                world.visitor_count, world.dinosaur_count, world.roads.size(),
                world.has_camera ? 1 : 0);
            for (const dino::WorldBuilding& b : world.buildings) {
                const char* name =
                    b.kind < building_names.size()
                        ? building_names[b.kind].c_str()
                        : "?";
                std::printf(
                    "building: kind=%u name=%s x=%u y=%u state=%u obj=%u "
                    "linked=%u link=%u class=%u\n",
                    b.kind, name, b.cell_x, b.cell_y, b.state, b.object_id,
                    b.linked_id, b.link, b.class_tag);
            }
            for (const dino::RoadRect& road : world.roads) {
                std::printf("road: %d,%d - %d,%d\n", road.x0, road.y0, road.x1,
                            road.y1);
            }
            for (const dino::SavWorldPlaced& show : world.records.shows) {
                std::printf(
                    "show: obj=%u start=%u,%u end=%u,%u class=%u field44=%u "
                    "field48=%u waypoints=%zu\n",
                    show.object_id, show.start_x, show.start_y, show.end_x,
                    show.end_y, show.class_tag, show.field_44, show.field_48,
                    show.waypoints.size());
            }
            for (const dino::SavWorldAnimal& animal : world.records.animals) {
                std::printf("animal: obj=%u field1c=%u field20=%u\n",
                            animal.object_id, animal.field_1c, animal.field_20);
            }
            for (const dino::SavWorldEmployee& employee :
                 world.records.employees) {
                std::printf("employee: obj=%u field1c=%u\n", employee.object_id,
                            employee.field_1c);
            }
            if (world.has_simulation_lists) {
                std::printf("sim lists: food_sources=%zu employees=%zu\n",
                            world.food_sources.size(),
                            world.employees_sim.size());
                for (const dino::WldFoodSource& source : world.food_sources) {
                    std::printf(
                        "food source: id=%u cell=%u,%u pos=%.3f,%.3f "
                        "dir=%.3f,%.3f\n",
                        source.id, source.cell_x, source.cell_y,
                        static_cast<double>(source.x),
                        static_cast<double>(source.y),
                        static_cast<double>(source.dx),
                        static_cast<double>(source.dy));
                }
                for (const dino::WldEmployeeState& employee :
                     world.employees_sim) {
                    std::printf(
                        "sim employee: id=%u cell=%u,%u pos=%.3f,%.3f "
                        "dir=%.3f,%.3f state=%u\n",
                        employee.id, employee.cell_x, employee.cell_y,
                        static_cast<double>(employee.x),
                        static_cast<double>(employee.y),
                        static_cast<double>(employee.dx),
                        static_cast<double>(employee.dy), employee.state);
                }
            }
            if (world.has_camera) {
                std::printf("camera:");
                for (const float value : world.camera) {
                    std::printf(" %.3f", value);
                }
                std::printf("\n");
            }
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "wld-info: %s\n", e.what());
            return 1;
        }
    }

    if (opt.bai_info) {
        try {
            const std::vector<dino::BaiAction> actions =
                dino::load_action_table(opt.asset_root);
            std::size_t pre = 0;
            std::size_t weights = 0;
            std::size_t needs = 0;
            std::size_t grants = 0;
            std::size_t elements = 0;
            std::size_t events = 0;
            std::array<std::size_t, 7> groups{};
            std::array<std::size_t, 6> classes{};
            for (const dino::BaiAction& action : actions) {
                pre += action.preconditions.size();
                weights += action.effect_weights.size();
                needs += action.need_deltas.size();
                grants += action.stat_grants.size();
                elements += action.elements.size();
                for (const dino::BaiElement& element : action.elements) {
                    events += element.events.size();
                }
                if (action.special_action < groups.size()) {
                    ++groups[action.special_action];
                }
                if (action.effect_class < classes.size()) {
                    ++classes[action.effect_class];
                }
            }
            std::printf(
                "actions: %zu preconditions=%zu weights=%zu need_deltas=%zu "
                "grants=%zu elements=%zu events=%zu\n",
                actions.size(), pre, weights, needs, grants, elements, events);
            std::printf("groups:");
            for (std::size_t i = 0; i < groups.size(); ++i) {
                std::printf(" %zu=%zu", i, groups[i]);
            }
            std::printf("\nclasses:");
            for (std::size_t i = 0; i < classes.size(); ++i) {
                std::printf(" %zu=%zu", i, classes[i]);
            }
            std::printf("\n");
            if (!actions.empty()) {
                std::printf("first=%s last=%s\n", actions.front().name.c_str(),
                            actions.back().name.c_str());
            }
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "bai-info: %s\n", e.what());
            return 1;
        }
    }

    if (opt.market_info || !opt.store_ini_path.empty()) {
        try {
            const bool explicit_store = !opt.store_ini_path.empty();
            const dino::Market market = dino::load_market(resolve_asset(
                opt.asset_root,
                explicit_store ? opt.store_ini_path
                               : "Data/Design/Scenario/Market.ini"));
            const char* label = explicit_store ? "store" : "market";
            int min_price = market.entries.front().price;
            int max_price = min_price;
            for (const dino::MarketEntry& entry : market.entries) {
                min_price = std::min(min_price, entry.price);
                max_price = std::max(max_price, entry.price);
            }
            std::printf("%s: entries=%zu stores=%zu prices=%d..%d\n", label,
                        market.entries.size(), market.stores.size(),
                        min_price, max_price);
            std::printf("%s stores:", label);
            for (const int store : market.stores) {
                std::size_t count = 0;
                for (const dino::MarketEntry& entry : market.entries) {
                    if (entry.store == store) {
                        ++count;
                    }
                }
                std::printf(" %d=%zu", store, count);
            }
            std::printf("\n");
            const dino::MarketEntry& first = market.entries.front();
            const dino::MarketEntry& last = market.entries.back();
            std::printf("first=%s %d %s last=%s %d %s\n", first.name.c_str(),
                        first.price, first.genotype.c_str(), last.name.c_str(),
                        last.price, last.genotype.c_str());
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "market-info: %s\n", e.what());
            return 1;
        }
    }

    if (!opt.food_info_dir.empty()) {
        try {
            const std::string dir =
                resolve_asset(opt.asset_root, opt.food_info_dir);
            const std::vector<std::string> names =
                dino::load_food_source_list(dir + "/FoodSourceList.ini");
            for (const std::string& name : names) {
                const dino::FoodSourceDef source =
                    dino::load_bsi(dir + "/" + name + ".bsi");
                std::printf(
                    "food: %s nutrition=%.3g quantity=%.3g price=%d kind=%d "
                    "flags=%d feeder=%s ui=%s\n",
                    source.name.c_str(), source.nutrition, source.quantity,
                    source.price, source.kind_id, source.flags,
                    source.feeder_mesh().c_str(), source.ui_name().c_str());
            }
            std::printf("food sources: %zu\n", names.size());
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "food-info: %s\n", e.what());
            return 1;
        }
    }

    if (opt.employee_info) {
        try {
            const std::vector<dino::EmployeeProfile> profiles =
                dino::load_employee_list(
                    resolve_asset(opt.asset_root, "Data/Design/employees"));
            for (const dino::EmployeeProfile& profile : profiles) {
                std::printf(
                    "employee: %s kind=%d wage=%d height=%.2f walk=%.2f "
                    "action=%.2f work=%.2f meshes=%zu sounds=%zu\n",
                    profile.name.c_str(), profile.kind, profile.wage,
                    profile.height, profile.walk_speed, profile.action_speed,
                    profile.work_duration, profile.meshes.size(),
                    profile.sounds.size());
            }
            std::printf("employee profiles: %zu\n", profiles.size());
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "employee-info: %s\n", e.what());
            return 1;
        }
    }

    if (!opt.show_info_path.empty()) {
        try {
            std::vector<std::string> paths;
            const std::filesystem::path input(opt.show_info_path);
            if (std::filesystem::is_directory(input)) {
                for (const auto& entry :
                     std::filesystem::directory_iterator(input)) {
                    if (entry.is_regular_file() &&
                        entry.path().extension() == ".shw") {
                        paths.push_back(entry.path().string());
                    }
                }
                std::sort(paths.begin(), paths.end());
            } else {
                paths.push_back(input.string());
            }
            const std::vector<std::string> feature_names =
                dino::load_feature_list(resolve_asset(
                    opt.asset_root, "Data/Design/attractions/features"));
            std::vector<dino::BfiFeature> bfi_features;
            try {
                bfi_features = dino::load_bfi_dir(resolve_asset(
                    opt.asset_root, "Data/Design/attractions/features"));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "show-info .bfi: %s\n", e.what());
            }
            std::printf("bfi features: %zu\n", bfi_features.size());
            const auto find_bfi = [&](const std::string& name)
                -> const dino::BfiFeature* {
                for (const dino::BfiFeature& bfi : bfi_features) {
                    if (bfi.name.size() != name.size()) {
                        continue;
                    }
                    bool same = true;
                    for (std::size_t i = 0; i < name.size(); ++i) {
                        if (std::tolower(static_cast<unsigned char>(bfi.name[i])) !=
                            std::tolower(static_cast<unsigned char>(name[i]))) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        return &bfi;
                    }
                }
                return nullptr;
            };
            const bool list_features = paths.size() == 1;
            for (const std::string& path : paths) {
                const dino::ShwShow show = dino::load_shw(path);
                std::printf(
                    "show: %s name=%s cost=%d judge=%d dims=%dx%d wall=%d "
                    "cages=%d,%d,%d,%d features=%zu teams=%zu\n",
                    std::filesystem::path(path).stem().string().c_str(),
                    show.name.c_str(), show.cost, show.judge_kind, show.width,
                    show.height, show.wall_type, show.cage_size[0],
                    show.cage_size[1], show.cage_size[2], show.cage_size[3],
                    show.features.size(), show.teams.size());
                std::vector<int> cells(
                    static_cast<std::size_t>(show.width) * show.height, 0);
                int overlaps = 0;
                for (const auto& feature : show.features) {
                    const std::string feature_name =
                        feature[0] >= 0 &&
                                feature[0] <
                                    static_cast<int>(feature_names.size())
                            ? feature_names[static_cast<std::size_t>(
                                  feature[0])]
                            : std::string("?");
                    const dino::BfiFeature* bfi = find_bfi(feature_name);
                    int fw = bfi != nullptr ? bfi->width : 1;
                    int fh = bfi != nullptr ? bfi->height : 1;
                    if ((feature[1] & 1) != 0) {
                        std::swap(fw, fh);
                    }
                    for (int dy = 0; dy < fh; ++dy) {
                        for (int dx = 0; dx < fw; ++dx) {
                            const int cx = feature[2] + dx;
                            const int cy = feature[3] + dy;
                            if (cx >= 0 && cy >= 0 && cx < show.width &&
                                cy < show.height) {
                                if (cells[static_cast<std::size_t>(cy) *
                                              show.width +
                                          cx]++ > 0) {
                                    ++overlaps;
                                }
                            }
                        }
                    }
                    if (list_features) {
                        std::printf(
                            "show feature: idx=%d rot=%d x=%d y=%d name=%s "
                            "w=%d h=%d\n",
                            feature[0], feature[1], feature[2], feature[3],
                            feature_name.c_str(), fw, fh);
                    }
                }
                int occupied = 0;
                for (const int cell : cells) {
                    if (cell > 0) {
                        ++occupied;
                    }
                }
                std::printf("show tiling: %s cells=%d overlaps=%d\n",
                            std::filesystem::path(path)
                                .stem()
                                .string()
                                .c_str(),
                            occupied, overlaps);
                if (list_features) {
                    for (const auto& team : show.teams) {
                        std::printf("show team: cage=%d team=%d\n", team.first,
                                    team.second);
                    }
                }
            }
            std::printf("shows: %zu\n", paths.size());
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "show-info: %s\n", e.what());
            return 1;
        }
    }

    if (opt.dino_actions_info) {
        try {
            const std::vector<dino::BaiAction> actions =
                dino::load_action_table(opt.asset_root);
            if (actions.size() != 64) {
                std::fprintf(stderr, "dino-actions: expected 64 actions, got %zu\n",
                             actions.size());
                return 1;
            }
            // The retail behavior graph only contains the self/special actions
            // for these needs; the other 60 need a perceived world target.
            const std::vector<std::string> nodes = {
                "Exploration", "Eat", "EatForSmallies", "Sleep"};
            const auto make_needs = [](int food, int violence, int secu,
                                       int repos, int size) {
                dino::DinoNeeds needs;
                needs.value = {food, violence, secu, repos};
                needs.size_cm = size;
                return needs;
            };
            struct Scenario {
                const char* name;
                dino::DinoNeeds needs;
            };
            const Scenario scenarios[] = {
                {"starving", make_needs(20, 90, 50, 90, 250)},
                {"tired", make_needs(70, 90, 90, 20, 250)},
                {"both", make_needs(20, 90, 50, 20, 250)},
                {"full", make_needs(99, 99, 99, 99, 250)},
                {"small", make_needs(20, 90, 50, 90, 150)},
            };
            std::printf("dino actions: %zu nodes=%zu\n", actions.size(),
                        nodes.size());
            for (const Scenario& scenario : scenarios) {
                const dino::DinoNeeds& needs = scenario.needs;
                std::printf("dino states %s: %d %d %d %d\n", scenario.name,
                            dino::need_state(needs.value[0]),
                            dino::need_state(needs.value[1]),
                            dino::need_state(needs.value[2]),
                            dino::need_state(needs.value[3]));
                const auto choice =
                    dino::choose_action(actions, needs, nodes, true);
                if (choice) {
                    std::printf("dino choose %s: %s %d\n", scenario.name,
                                choice->name.c_str(), choice->score);
                } else {
                    std::printf("dino choose %s: idle\n", scenario.name);
                }
            }
            dino::DinoActionLoopConfig loop_config;
            loop_config.action_scale = 0.1;  // 3 s per action in the probe
            dino::DinoActionLoop loop(&actions,
                                      make_needs(20, 90, 50, 20, 250),
                                      loop_config);
            std::printf("dino timeline:\n");
            std::string previous;
            double t = 0.0;
            for (int step = 0; step < 400; ++step) {
                loop.tick(0.1);
                t += 0.1;
                const std::string& name = loop.action_name();
                if (name != previous) {
                    const dino::DinoNeeds& n = loop.needs();
                    std::printf(
                        "dino timeline %.1f: %s food=%d violence=%d secu=%d "
                        "repos=%d\n",
                        t, name.c_str(), n.value[0], n.value[1], n.value[2],
                        n.value[3]);
                    previous = name;
                }
            }
            // Action-target policy (`node+0x3c` vtable +0x14): the target
            // position feeds the chooser's danger-grid threat term.
            {
                dino::DangerGrid grid;
                grid.cell(12, 12) = 500;  // target cell (100, 100)
                grid.cell(25, 25) = 300;  // actor cell  (200, 200)
                dino::ActionTarget box;
                box.kind = dino::ActionTarget::Kind::Box;
                box.x = 100;
                box.y = 100;
                std::printf(
                    "dino target box: %d\n",
                    dino::chooser_threat_score(box, 200, 200, 40, 50, grid));
                dino::ActionTarget actor;
                actor.kind = dino::ActionTarget::Kind::Actor;
                actor.x = 200;
                actor.y = 200;
                std::printf(
                    "dino target actor: %d\n",
                    dino::chooser_threat_score(actor, 200, 200, 40, 50, grid));
                dino::ActionTarget none;
                std::printf(
                    "dino target none: %d\n",
                    dino::chooser_threat_score(none, 200, 200, 40, 50, grid));
                const auto threat_choice = dino::choose_action(
                    actions, make_needs(20, 90, 50, 90, 250), nodes, true,
                    [&](const dino::BaiAction& action) {
                        return action.name == "Eat"
                                   ? dino::chooser_threat_score(
                                         box, 200, 200, 40, 50, grid)
                                   : 0;
                    });
                if (threat_choice) {
                    std::printf("dino choose threatbox: %s %d\n",
                                threat_choice->name.c_str(),
                                threat_choice->score);
                }
                dino::DinoActionLoop threat_loop(
                    &actions, make_needs(20, 90, 50, 90, 250), loop_config);
                threat_loop.set_threat_provider(
                    [](const dino::BaiAction& action) {
                        return action.name == "Eat" ? -100000 : 0;
                    });
                threat_loop.tick(0.1);
                std::printf("dino loop threat: %s\n",
                            threat_loop.action_name().c_str());
            }
            // Completion writers `FUN_00571190`: case 0xd copies phenotype
            // +0x44 into the grid threat cache; case 0x1a copies +0x80 into
            // the stamp radius. Drive both through the public loop API.
            {
                std::vector<dino::BaiAction> cache_actions(2);
                cache_actions[0].name = "GetCourage";
                cache_actions[1].name = "ObstAttackSize1";
                dino::DinoNeeds cache_needs;
                cache_needs.combined_attack = 54;
                cache_needs.obstacle_radius = 7;
                dino::DinoActionLoop cache_loop(&cache_actions, cache_needs);
                cache_loop.restore(cache_needs, 0, 0.01);
                cache_loop.tick(0.02);
                std::printf("dino cache getcourage: threat=%d\n",
                            cache_loop.needs().threat);
                cache_loop.restore(cache_needs, 1, 0.01);
                cache_loop.tick(0.02);
                std::printf("dino cache obst: radius=%d\n",
                            cache_loop.needs().threat_radius);
            }
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "dino-actions: %s\n", e.what());
            return 1;
        }
    }

    dino::KeyMap keymap;
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

    std::vector<std::string> island_names;
    int free_game_budgets[3] = {75000, 50000, 25000};
    try {
        island_names = read_island_list(resolve_asset(
            opt.asset_root, "Data/Design/landscape/IslandList.ini"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "island list: %s\n", e.what());
    }
    try {
        const dino::ui::IniFile free_game = dino::ui::load_ini(
            resolve_asset(opt.asset_root, "Data/Design/Scenario/FreeGame.ini"));
        if (const dino::ui::IniSection* context = free_game.find("CONTEXT")) {
            auto budget = [&](const char* key, int fallback) {
                const std::string value = context->get(key);
                return value.empty() ? fallback : std::atoi(value.c_str());
            };
            free_game_budgets[0] = budget("Budget", 75000);
            free_game_budgets[1] = budget("BudgetMedium", 50000);
            free_game_budgets[2] = budget("BudgetHard", 25000);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "free game scenario: %s\n", e.what());
    }

    ParkClock park_clock;
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

    std::optional<dino::CampaignRuntime> campaign_runtime;
    std::vector<dino::ObjectiveVerdict> last_mission_verdicts;
    std::optional<dino::CampaignMission> pending_next_mission;
    const std::string campaign_progress_path =
        opt.campaign_progress_path.empty()
            ? "run/save/campaign-progress.txt"
            : opt.campaign_progress_path;
    std::map<std::string, std::string> campaign_progress =
        dino::load_campaign_progress(campaign_progress_path);
    std::string mission_panel_title;
    std::string pending_ending_video;

    std::unordered_map<std::string, std::string> menu_sound_files;
    std::vector<std::string> menu_sound_names;
    try {
        const dino::ui::IniFile sound_db = dino::ui::load_ini(
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
    std::unordered_map<std::string, std::string> park_sound_files;    try {
        const dino::ui::IniFile sound_db = dino::ui::load_ini(
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
    std::vector<std::pair<std::string, std::string>> save_slots;
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
    if (opt.list_sounds) {
        for (const auto& name : menu_sound_names) {
            std::printf("sound %s: %s\n", name.c_str(),
                        menu_sound_files[name].c_str());
        }
        return 0;
    }

    dino::ui::Menu menu;
    bool menu_loaded = false;
    dino::ui::ParkHud park;
    dino::ShowRuntime show_runtime;
    std::vector<dino::ShowCatalogEntry> show_catalog;
    bool show_placements_applied = false;
    int pending_show_index = -1;
    int attraction_selected_slot = -1;
    bool show_status_printed = false;
    const std::string campaign_dino_dat =
        opt.dino_dat_path.empty() ? "run/save/dino.dat" : opt.dino_dat_path;
    std::uint32_t campaign_unlock =
        dino::read_campaign_unlock(campaign_dino_dat);
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
                "menu campaigns: %zu unlocked=0x%X (%s)\n",
                menu.campaigns.size(),
                static_cast<unsigned>(campaign_unlock),
                campaign_dino_dat.c_str());
        } else {
            std::fprintf(stderr, "menu campaigns: %s\n",
                         menu.campaign_error.c_str());
        }
    }

    // Phenotype table, needed by the store attribute gauges as well as the
    // imported/bought dinosaurs. Loading it here keeps `refresh_store_rows`
    // able to fill every row's attribute panel.
    dino::DinoPhenotypeTable phenotype_table;
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

    // Dinosaur store panel: the retail state lists one store vector in
    // six-row pages (0x00535460). The vector source depends on the mode:
    // Market.ini for the plain/debug park, FreeGame.ini `[DINOSTORE]` for
    // free game, and the mission `[DINOSTORE]` block for campaigns. The
    // selected mode replaces the catalog; row order is the file order.
    std::vector<dino::MarketEntry> store_catalog;
    int pending_store_index = -1;
    int pending_store_price = 0;
    auto store_row_attributes = [&](const dino::MarketEntry& entry) {
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
    };
    auto refresh_store_rows = [&]() {
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
    };
    auto apply_store_catalog = [&](dino::Market market,
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
    };

    auto start_campaign_mission = [&](const dino::CampaignMission& mission) {
        campaign_runtime.emplace();
        campaign_runtime->start(mission, park_clock.minutes());
        show_runtime.clear();
        show_runtime.set_duration_minutes(opt.show_duration_minutes);
        show_catalog.clear();
        show_placements_applied = false;
        pending_show_index = -1;
        try {
            show_catalog = dino::load_show_catalog(opt.asset_root, mission,
                                                   park.loc);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "show catalog: %s\n", e.what());
        }
        if (!show_catalog.empty()) {
            std::printf("show catalog: %zu entries\n", show_catalog.size());
        }
        std::vector<dino::ui::ShowRow> show_rows;
        show_rows.reserve(show_catalog.size());
        for (const dino::ShowCatalogEntry& entry : show_catalog) {
            dino::ui::ShowRow row;
            row.title = entry.title;
            row.stem = entry.stem;
            row.judge_kind = entry.judge_kind;
            row.price = entry.cost;
            row.cage_size = entry.cage_size;
            show_rows.push_back(std::move(row));
            std::printf(
                "show catalog: %s title=%s judge=%d cost=%d cage=%d%d%d%d\n",
                entry.stem.c_str(), entry.title.c_str(), entry.judge_kind,
                entry.cost, entry.cage_size[0], entry.cage_size[1],
                entry.cage_size[2], entry.cage_size[3]);
        }
        park.set_show_rows(std::move(show_rows));
        last_mission_verdicts.assign(mission.objectives.size(),
                                     dino::ObjectiveVerdict::Pending);
        park.set_mission_verdicts(
            std::vector<int>(mission.objectives.size(), 0));
        park.show_mission_panel();
        std::printf("mission: %s objectives=%zu\n", mission.name.c_str(),
                    mission.objectives.size());
        for (std::size_t i = 0; i < mission.objectives.size(); ++i) {
            const dino::CampaignObjective& objective = mission.objectives[i];
            std::printf("mission objective %zu %s: %s\n", i,
                        objective.verb.c_str(),
                        dino::ui::resolve_loc_text(objective.description_id,
                                                   park.loc)
                            .c_str());
        }
        const std::string campaign_name =
            std::filesystem::path(mission.path)
                .parent_path()
                .filename()
                .string();
        if (!campaign_name.empty()) {
            campaign_progress[campaign_name] = mission.name;
            try {
                dino::save_campaign_progress(campaign_progress,
                                             campaign_progress_path);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "campaign progress: %s\n", e.what());
            }
        }
        mission_panel_title.clear();
        const std::optional<dino::Campaign> parent =
            dino::load_parent_campaign(mission);
        if (parent) {
            for (std::size_t i = 0; i < parent->missions.size(); ++i) {
                if (parent->missions[i].name == mission.name) {
                    mission_panel_title =
                        std::to_string(i + 1) + "/" +
                        std::to_string(parent->missions.size());
                    break;
                }
            }
        }
        if (!mission_panel_title.empty()) {
            std::printf("mission title: %s\n",
                        mission_panel_title.c_str());
        }
        // Campaign store: the mission's own [DINOSTORE] block. Empty blocks
        // (Cpg04_Mission4) produce an empty catalog, matching the retail
        // per-mission store vector.
        apply_store_catalog(dino::market_from_values(mission.values),
                            mission.name);
    };

    int pending_minimap_zoom = 0;
    bool pending_port_save = false;
    bool pending_port_load = false;
    std::string pending_construct_widget;
    bool pending_fence_tool = false;
    int fence_corner_x = -1;
    int fence_corner_y = -1;
    std::string pending_store_action;
    std::string pending_show_action;
    std::vector<std::string> pending_attraction_actions;

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
        if (opt.menu_page >= 0) {
            menu.page = std::clamp(opt.menu_page, 0, 8);
        }
        for (const auto& value : opt.menu_clicks) {
            int x = 0;
            int y = 0;
            if (std::sscanf(value.c_str(), "%d,%d", &x, &y) == 2) {
                menu.click(x, y);
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

    dino::Mesh mesh;
    dino::Image image;
    dino::Image dino_image;
    dino::Skeleton skeleton;
    dino::Skin skin;
    dino::Animation anim;
    dino::Wav wav;
    dino::Lnd lnd;
    dino::Terrain terrain;
    dino::Vegetation vegetation;
    dino::BuildGrid build_grid;
    dino::Mesh fence_source;
    dino::Image path_image;
    dino::Image fence_image;
    std::array<dino::Image, 6> sky_images;
    dino::Image water_image;
    dino::Image water2_image;
    float water_z = 0.0f;
    // WaterSurface scroll time (FUN_00602fd0: +0x10 0.03/s, +0x14 0.01/s).
    float water_scroll = opt.water_time > 0.0f ? opt.water_time : 0.0f;
    dino::VisitorSystem visitor_system;
    std::vector<dino::VisitorProfile> visitor_profiles;
    std::vector<dino::VisitorProfile> all_visitor_profiles;
    std::vector<PortShowSpec> port_shows =
        parse_show_specs(opt.show_specs, opt.asset_root);
    dino::Mesh visitor_pose1;
    dino::Mesh visitor_pose2;
    dino::Image visitor_image;
    dino::Fnt hud_font;
    bool hud_font_ready = false;
    bool build_ready = false;
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
            if (!opt.no_vegetation) {
                vegetation = dino::load_vegetation(opt.asset_root, lnd, terrain);
            }
            if (opt.build_mode) {
                build_grid.init(opt.build_cell, terrain.world_size);
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
                fence_source = dino::load_mesh(fence_mesh);
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
                            vegetation.groups.size(), vegetation.instances,
                            vegetation.triangles);
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

    Uint32 init_flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    if (!opt.wav_path.empty() || opt.ui_sounds) {
        init_flags |= SDL_INIT_AUDIO;
    }
    if (!SDL_Init(init_flags)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    UiSoundPlayer ui_sound;
    ui_sound.asset_root = opt.asset_root;
    ui_sound.enabled = opt.ui_sounds;
    ui_sound.muted = opt.muted;
    if (!opt.play_sound.empty()) {
        const auto sound_it = menu_sound_files.find(opt.play_sound);
        if (sound_it != menu_sound_files.end()) {
            ui_sound.play(sound_it->second);
        } else {
            std::fprintf(stderr, "unknown sound: %s\n", opt.play_sound.c_str());
        }
    }
    MusicPlayer music;
    music.asset_root = opt.asset_root;
    music.enabled = opt.music;
    music.set_gain(std::clamp(opt.music_volume / 100.0f, 0.0f, 1.0f));
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

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (opt.fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (opt.borderless) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }
    SDL_Window* window = SDL_CreateWindow(
        opt.title.c_str(), opt.width, opt.height, window_flags);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_StartTextInput(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(opt.vsync ? 1 : 0);

    GlFns glfns;
    if (!glfns.load()) {
        std::fprintf(stderr, "required OpenGL 3.3 functions are missing\n");
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    const GLuint program = build_program(glfns);
    if (!program) {
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_AudioStream* audio = nullptr;
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

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::printf("SDL %d.%d.%d | GL %s | %s | %s\n",
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
                version ? version : "?", renderer ? renderer : "?", vendor ? vendor : "?");

    const bool skinned = !skin.bones.empty();
    std::vector<int> vertex_bone;
    std::vector<int> bone_skel;
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

    GLuint texture = image.width > 0 ? upload_texture(image) : 0;
    const GLuint dino_texture = dino_image.width > 0 ? upload_texture(dino_image) : 0;
    const GLuint hud_font_texture =
        (opt.hud && hud_font_ready) ? upload_font_atlas(hud_font) : 0;
    bool hud_visible = true;
    GpuMesh hud_gpu;
    GpuMesh menu_gpu;
    std::unordered_map<std::string, UiTexture> ui_textures;
    std::unordered_map<std::string, dino::Fnt> ui_fonts;
    std::unordered_map<std::string, GLuint> ui_font_atlases;

    auto ui_texture = [&](const std::string& filename) -> UiTexture* {
        if (filename.empty()) {
            return nullptr;
        }
        auto it = ui_textures.find(filename);
        if (it != ui_textures.end()) {
            return &it->second;
        }
        try {
            dino::Image img = dino::load_image(resolve_asset(opt.asset_root, filename));
            UiTexture texture;
            texture.id = upload_texture(img);
            texture.width = img.width;
            texture.height = img.height;
            const auto result = ui_textures.emplace(filename, std::move(texture));
            return &result.first->second;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "menu texture: %s\n", e.what());
            return nullptr;
        }
    };

    auto ui_font = [&](const std::string& path, int text_height,
                       float& scale_out, GLuint& atlas_out) -> const dino::Fnt* {
        const std::string font_path =
            path.empty() ? resolve_asset(opt.asset_root, "Data/Fonte/arial12.fnt")
                         : resolve_asset(opt.asset_root, path);
        auto it = ui_fonts.find(font_path);
        if (it == ui_fonts.end()) {
            try {
                dino::Fnt font = dino::load_fnt(font_path);
                it = ui_fonts.emplace(font_path, std::move(font)).first;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "menu font: %s\n", e.what());
                return nullptr;
            }
        }
        const dino::Fnt& font = it->second;
        scale_out = text_height > 0 && font.font_size > 0
                        ? static_cast<float>(text_height) /
                              static_cast<float>(font.font_size)
                        : 1.0f;
        auto at = ui_font_atlases.find(font_path);
        if (at == ui_font_atlases.end()) {
            at = ui_font_atlases.emplace(font_path, upload_font_atlas(font)).first;
        }
        atlas_out = at->second;
        return &font;
    };

    auto draw_ui_mesh = [&](GpuMesh& target, const dino::Mesh& mesh, GLuint texture_id,
                            float alpha, int unlit, int view_w, int view_h) {
        target.destroy(glfns);
        target = upload_mesh(mesh, glfns);
        if (target.vao == 0) {
            return;
        }
        dino::Mat4 ortho;
        ortho.m[0] = 2.0f / static_cast<float>(view_w);
        ortho.m[5] = -2.0f / static_cast<float>(view_h);
        ortho.m[3] = -1.0f;
        ortho.m[7] = 1.0f;
        glfns.UseProgram(program);
        glfns.UniformMatrix4fv(glfns.GetUniformLocation(program, "u_mvp"), 1, GL_TRUE,
                               ortho.m);
        glfns.Uniform1i(glfns.GetUniformLocation(program, "u_unlit"), unlit);
        glfns.Uniform1i(glfns.GetUniformLocation(program, "u_use_texture"),
                        texture_id != 0 ? 1 : 0);
        glfns.Uniform1i(
            glfns.GetUniformLocation(program, "u_use_texture2"), 0);
        glfns.Uniform1i(
            glfns.GetUniformLocation(program, "u_modulate2x"), 0);
        glfns.Uniform1f(glfns.GetUniformLocation(program, "u_alpha"), alpha);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (texture_id != 0) {
            glfns.ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture_id);
        }
        glfns.BindVertexArray(target.vao);
        glDrawElements(GL_TRIANGLES, target.index_count, GL_UNSIGNED_INT, nullptr);
        glfns.BindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glfns.UseProgram(0);
    };

    auto play_video = [&](const std::string& path, int start_frame,
                          int max_frames) -> bool {
        dino::MpgStream video;
        std::string video_error;
        if (!video.open(path, video_error)) {
            std::fprintf(stderr, "play video: %s\n", video_error.c_str());
            return false;
        }
        std::printf("video play: %s %dx%d %.3f fps\n", path.c_str(),
                    video.width(), video.height(), video.frame_rate());
        for (int i = 0; i < start_frame; ++i) {
            dino::MpgFrame skipped;
            if (!video.next(skipped)) {
                break;
            }
        }
        bool video_running = true;
        int played = 0;
        dino::MpgFrame last_frame;
        dino::Mesh video_quad_mesh;
        GpuMesh video_quad;
        GLuint video_texture = 0;
        while (video_running) {
            dino::MpgFrame frame;
            if (!video.next(frame)) {
                break;
            }
            last_frame = frame;
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_KEY_DOWN &&
                     event.key.key == SDLK_ESCAPE)) {
                    video_running = false;
                }
            }
            if (!video_running) {
                break;
            }
            int view_w = 0;
            int view_h = 0;
            SDL_GetWindowSizeInPixels(window, &view_w, &view_h);
            if (view_w <= 0 || view_h <= 0) {
                break;
            }
            dino::Image image;
            image.width = frame.width;
            image.height = frame.height;
            image.rgba.resize(
                static_cast<std::size_t>(frame.width) * frame.height * 4);
            for (std::size_t i = 0;
                 i < static_cast<std::size_t>(frame.width) * frame.height;
                 ++i) {
                image.rgba[i * 4 + 0] = frame.rgb[i * 3 + 0];
                image.rgba[i * 4 + 1] = frame.rgb[i * 3 + 1];
                image.rgba[i * 4 + 2] = frame.rgb[i * 3 + 2];
                image.rgba[i * 4 + 3] = 255;
            }
            const GLuint texture = upload_texture(image);
            if (video_texture != 0) {
                glDeleteTextures(1, &video_texture);
            }
            video_texture = texture;
            const float scale = std::min(
                static_cast<float>(view_w) / frame.width,
                static_cast<float>(view_h) / frame.height);
            const float quad_w = frame.width * scale;
            const float quad_h = frame.height * scale;
            video_quad_mesh = build_ui_quad(
                (view_w - quad_w) * 0.5f, (view_h - quad_h) * 0.5f, quad_w,
                quad_h, 0.0f, 0.0f, 1.0f, 1.0f, {255, 255, 255, 255});
            glViewport(0, 0, view_w, view_h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            draw_ui_mesh(video_quad, video_quad_mesh, video_texture, 1.0f, 1,
                         view_w, view_h);
            SDL_GL_SwapWindow(window);
            ++played;
            if (max_frames > 0 && played >= max_frames) {
                break;
            }
            const double rate =
                video.frame_rate() > 0.0 ? video.frame_rate() : 24.0;
            SDL_Delay(static_cast<Uint32>(std::max(1.0, 1000.0 / rate)));
        }
        if (!opt.screenshot.empty() && last_frame.width > 0) {
            std::vector<std::uint8_t> rgba(
                static_cast<std::size_t>(last_frame.width) *
                    last_frame.height * 4,
                255);
            for (int y = 0; y < last_frame.height; ++y) {
                const std::uint8_t* src =
                    last_frame.rgb.data() +
                    static_cast<std::size_t>(last_frame.height - 1 - y) *
                        last_frame.width * 3;
                std::uint8_t* dst =
                    rgba.data() +
                    static_cast<std::size_t>(y) * last_frame.width * 4;
                for (int x = 0; x < last_frame.width; ++x) {
                    dst[x * 4 + 0] = src[x * 3 + 0];
                    dst[x * 4 + 1] = src[x * 3 + 1];
                    dst[x * 4 + 2] = src[x * 3 + 2];
                }
            }
            write_bmp(opt.screenshot, last_frame.width, last_frame.height,
                      rgba);
        }
        std::printf("video played: %d frames\n", played);
        video_quad.destroy(glfns);
        if (video_texture != 0) {
            glDeleteTextures(1, &video_texture);
        }
        return true;
    };

    if (!opt.video_play_path.empty()) {
        if (!play_video(opt.video_play_path, opt.video_play_start,
                        opt.video_play_frames)) {
            return 1;
        }
        return 0;
    }

    auto draw_menu = [&](int view_w, int view_h, int logical_w, int logical_h) {
        (void)logical_h;
        if (!menu_loaded) {
            return;
        }
        const float dpi = logical_w > 0
                              ? static_cast<float>(view_w) / static_cast<float>(logical_w)
                              : 1.0f;
        const float ox = (static_cast<float>(view_w) - 800.0f * dpi) * 0.5f;
        const float oy = (static_cast<float>(view_h) - 600.0f * dpi) * 0.5f;
        auto px = [&](int value) { return ox + static_cast<float>(value) * dpi; };
        auto py = [&](int value) { return oy + static_cast<float>(value) * dpi; };

        if (const dino::ui::Graphic* background =
                menu.overlay.graphic(menu.background())) {
            if (UiTexture* texture = ui_texture(background->filename)) {
                draw_ui_mesh(menu_gpu,
                             build_ui_quad(ox, oy, 800.0f * dpi, 600.0f * dpi, 0.0f,
                                           0.0f, 1.0f, 1.0f, {255, 255, 255, 255}),
                             texture->id, 1.0f, 1, view_w, view_h);
            }
        }

        auto draw_sprite = [&](const char* name, float x, float y, float w, float h,
                               int frame) {
            const dino::ui::Graphic* graphic = menu.overlay.graphic(name);
            if (graphic == nullptr) {
                return;
            }
            UiTexture* texture = ui_texture(graphic->filename);
            if (texture == nullptr) {
                return;
            }
            const UiSourceRect source = ui_frame_rect(*texture, *graphic, frame);
            if (!source.valid) {
                return;
            }
            const float u0 = static_cast<float>(source.x) / texture->width;
            const float v0 = static_cast<float>(source.y) / texture->height;
            const float u1 = static_cast<float>(source.x + source.w) / texture->width;
            const float v1 = static_cast<float>(source.y + source.h) / texture->height;
            draw_ui_mesh(menu_gpu,
                         build_ui_quad(x, y, w, h, u0, v0, u1, v1,
                                       {255, 255, 255, 255}),
                         texture->id, 1.0f, 1, view_w, view_h);
        };

        for (int i = 0; i < static_cast<int>(menu.overlay.widgets.size()); ++i) {
            const dino::ui::Widget& widget =
                menu.overlay.widgets[static_cast<std::size_t>(i)];
            bool visible = menu.widget_visible(i);
            const std::string island_prefix = "FREE_GAME_ISLAND_PIC_isl";
            if (widget.name.rfind(island_prefix, 0) == 0) {
                const int index = std::atoi(widget.name.c_str() + island_prefix.size());
                visible = menu.page == 1 &&
                          menu.overlay.page_root(i) ==
                              static_cast<int>(menu.overlay.pages[1]) &&
                          index == menu.island + 1;
            }
            const std::string info_island_prefix =
                "CAMPAIGN_INFO_ISLAND_PIC_isl";
            if (widget.name.rfind(info_island_prefix, 0) == 0) {
                const int index = std::atoi(widget.name.c_str() +
                                            info_island_prefix.size());
                visible = false;
                const dino::Campaign* campaign = menu.selected_campaign();
                if (campaign != nullptr && menu.page == 3 &&
                    menu.overlay.page_root(i) ==
                        static_cast<int>(menu.overlay.pages[3])) {
                    char island_name[16];
                    std::snprintf(island_name, sizeof(island_name), "Isl%02d",
                                  index);
                    visible = campaign->island == island_name;
                }
            }
            if (!visible) {
                continue;
            }

            int frame = i == menu.hover ? 5 : 4;
            if ((widget.name == "FREE_EASY_BUTTON" && menu.difficulty == 0) ||
                (widget.name == "FREE_MIDDLE_BUTTON" && menu.difficulty == 1) ||
                (widget.name == "FREE_HARD_BUTTON" && menu.difficulty == 2)) {
                frame = 7;
            }

            if (!widget.texture.empty()) {
                if (const dino::ui::Graphic* graphic =
                        menu.overlay.graphic(widget.texture)) {
                    if (UiTexture* texture = ui_texture(graphic->filename)) {
                        const int sprite_frame =
                            graphic->objects <= 1 ? 0 : frame;
                        const UiSourceRect source =
                            ui_frame_rect(*texture, *graphic, sprite_frame);
                        if (source.valid) {
                            const float u0 = static_cast<float>(source.x) / texture->width;
                            const float v0 = static_cast<float>(source.y) / texture->height;
                            const float u1 = static_cast<float>(source.x + source.w) /
                                             texture->width;
                            const float v1 = static_cast<float>(source.y + source.h) /
                                             texture->height;
                            draw_ui_mesh(
                                menu_gpu,
                                build_ui_quad(px(widget.abs_x), py(widget.abs_y),
                                              static_cast<float>(widget.width) * dpi,
                                              static_cast<float>(widget.height) * dpi,
                                              u0, v0, u1, v1, {255, 255, 255, 255}),
                                texture->id, 1.0f, 1, view_w, view_h);
                        }
                    }
                }
            } else if (widget.has_back_color) {
                draw_ui_mesh(menu_gpu,
                             build_screen_rect(px(widget.abs_x), py(widget.abs_y),
                                               static_cast<float>(widget.width) * dpi,
                                               static_cast<float>(widget.height) * dpi,
                                               widget.back_color),
                             0, 1.0f, 1, view_w, view_h);
            }

            if (widget.type == "LIST") {
                const int line_h =
                    widget.list_line_height > 0 ? widget.list_line_height : 34;
                const int row_w =
                    widget.list_column0_width > 0
                        ? widget.list_column0_width
                        : widget.width - widget.list_scroll_width;
                const int visible_rows =
                    line_h > 0 ? widget.height / line_h : 0;
                const int rows = std::min(10, visible_rows);
                const bool campaign_list = widget.name == "CAMPAIGN_LIST";
                const bool campaign_info =
                    widget.name == "CAMPAIGN_INFO_DESCRIPTION";
                int selected_row = menu.island;
                int scroll_rows = 10;
                if (campaign_list) {
                    selected_row = menu.campaign;
                    scroll_rows = static_cast<int>(menu.campaigns.size());
                } else if (campaign_info) {
                    selected_row = 0;
                    scroll_rows = 1;
                }
                const int row_count =
                    campaign_list
                        ? std::min(rows,
                                   static_cast<int>(menu.campaigns.size()))
                        : rows;
                const dino::ui::Graphic* row_graphic =
                    menu.overlay.graphic("ISLAND_LIST_BUTTON");
                UiTexture* row_texture =
                    row_graphic != nullptr ? ui_texture(row_graphic->filename)
                                           : nullptr;
                const int selected_frame = 7;
                const int normal_frame = 4;
                float list_scale = 1.0f;
                GLuint list_atlas = 0;
                const dino::Fnt* list_font = ui_font(
                    widget.font, widget.text_height, list_scale, list_atlas);
                const std::string list_justify =
                    widget.justify.empty() ? "CENTER" : widget.justify;
                if (campaign_info) {
                    // Page 3 is the campaign briefing: the LIST holds wrapped
                    // paragraphs (literal `\n` separators in the .loc record),
                    // not one row per item like the island/campaign lists.
                    const dino::Campaign* selected = menu.selected_campaign();
                    if (selected != nullptr && list_font != nullptr) {
                        const std::string briefing = limit_lines(
                            wrap_text(dino::ui::resolve_loc_text(
                                          selected->description_id, menu.loc),
                                      34),
                            static_cast<std::size_t>(
                                std::max(1, visible_rows)));
                        const dino::Mesh block = build_text_block(
                            *list_font, briefing,
                            px(widget.abs_x) + 4.0f * dpi,
                            py(widget.abs_y) + 2.0f * dpi, list_scale * dpi,
                            widget.text_color, "LEFT",
                            static_cast<float>(row_w - 4) * dpi,
                            static_cast<float>(widget.height) * dpi);
                        draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f, 2,
                                     view_w, view_h);
                    }
                } else {
                    for (int row = 0; row < row_count; ++row) {
                        const float row_y = py(widget.abs_y) +
                                            static_cast<float>(row) * line_h * dpi;
                        const bool locked =
                            campaign_list && !menu.campaign_unlocked(row);
                        if (row_texture != nullptr && row_graphic != nullptr) {
                            const int frame =
                                locked ? 0
                                       : (row == selected_row ? selected_frame
                                                              : normal_frame);
                            const UiSourceRect source =
                                ui_frame_rect(*row_texture, *row_graphic, frame);
                            if (source.valid) {
                                const float u0 =
                                    static_cast<float>(source.x) /
                                    row_texture->width;
                                const float v0 =
                                    static_cast<float>(source.y) /
                                    row_texture->height;
                                const float u1 =
                                    static_cast<float>(source.x + source.w) /
                                    row_texture->width;
                                const float v1 =
                                    static_cast<float>(source.y + source.h) /
                                    row_texture->height;
                                draw_ui_mesh(
                                    menu_gpu,
                                    build_ui_quad(
                                        px(widget.abs_x), row_y,
                                        static_cast<float>(row_w) * dpi,
                                        static_cast<float>(line_h) * dpi, u0,
                                        v0, u1, v1, {255, 255, 255, 255}),
                                    row_texture->id, 1.0f, 1, view_w, view_h);
                            }
                        }
                        if (list_font != nullptr) {
                            std::string name;
                            if (campaign_list) {
                                if (row < static_cast<int>(
                                              menu.campaigns.size())) {
                                    name = dino::ui::resolve_loc_text(
                                        menu.campaigns[static_cast<std::size_t>(
                                                           row)]
                                            .title_id,
                                        menu.loc);
                                }
                            } else {
                                char field[32];
                                std::snprintf(field, sizeof(field),
                                              "&Isl%02d_Name", row + 1);
                                name = dino::ui::resolve_loc_text(field,
                                                                 menu.loc);
                            }
                            if (!name.empty()) {
                                const dino::Color text_color =
                                    locked ? dino::Color{120, 120, 120, 255}
                                           : widget.text_color;
                                const dino::Mesh block = build_text_block(
                                    *list_font, name, px(widget.abs_x), row_y,
                                    list_scale * dpi, text_color,
                                    list_justify,
                                    static_cast<float>(row_w) * dpi,
                                    static_cast<float>(line_h) * dpi);
                                draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f,
                                             2, view_w, view_h);
                            }
                        }
                    }
                }
                if (widget.list_scroll_width > 0) {
                    const float scroll_x = px(widget.abs_x + row_w);
                    const float scroll_y = py(widget.abs_y);
                    const float scroll_w =
                        static_cast<float>(widget.list_scroll_width) * dpi;
                    const float scroll_h = static_cast<float>(widget.height) * dpi;
                    draw_sprite("CAMPAIGN_TEXT_LIFT", scroll_x, scroll_y, scroll_w,
                                scroll_h, 0);
                    draw_sprite("CAMPAIGN_TEXT_SCROLLUP", scroll_x, scroll_y,
                                scroll_w, 22.0f * dpi, 4);
                    draw_sprite("CAMPAIGN_TEXT_SCROLLDOWN", scroll_x,
                                scroll_y + scroll_h - 22.0f * dpi, scroll_w,
                                22.0f * dpi, 4);
                    const float free_height =
                        std::max(0.0f, scroll_h - 76.0f * dpi);
                    const float thumb_y =
                        scroll_y + 22.0f * dpi +
                        free_height *
                            (static_cast<float>(selected_row) /
                             static_cast<float>(std::max(1, scroll_rows - 1)));
                    draw_sprite("CAMPAIGN_TEXT_THUMB", scroll_x, thumb_y, scroll_w,
                                32.0f * dpi, 4);
                }
            }

            const std::string text = menu.text_for(widget);
            if (!text.empty()) {
                float scale = 1.0f;
                GLuint atlas = 0;
                if (const dino::Fnt* font =
                        ui_font(widget.font, widget.text_height, scale, atlas)) {
                    const dino::Mesh block = build_text_block(
                        *font, text, px(widget.abs_x), py(widget.abs_y), scale * dpi,
                        widget.text_color, widget.justify,
                        static_cast<float>(widget.width) * dpi,
                        static_cast<float>(widget.height) * dpi);
                    draw_ui_mesh(menu_gpu, block, atlas, 1.0f, 2, view_w, view_h);
                }
            }
        }
    };
    bool place_dino = skinned && !opt.lnd_path.empty();
    float spawn_x = opt.spawn_x >= 0.0f ? opt.spawn_x : terrain.world_size * 0.5f;
    float spawn_y = opt.spawn_y >= 0.0f ? opt.spawn_y : terrain.world_size * 0.5f;

    auto pose_dino = [&](double time) {
        const dino::Animation* anim_ptr = anim.bones.empty() ? nullptr : &anim;
        dino::Mesh pose = build_skinned_mesh(skin.mesh, skin, skeleton, anim_ptr, time,
                                             vertex_bone, bone_skel,
                                             opt.skin_bind_from_skin);
        if (mesh.colors.size() == pose.positions.size()) {
            pose.colors = mesh.colors;
        }
        return place_on_terrain(std::move(pose), terrain, spawn_x, spawn_y,
                                opt.dino_angle);
    };

    dino::Mesh display;
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
    GpuMesh gpu = upload_mesh(display, glfns);
    GpuMesh dino_gpu;
    GpuMesh shadow_gpu;
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

    // P2: retail dinosaurs from a `.wld`/`.sav`. The world record carries the
    // 18-gene genotype; the dominant head-type gene (min of allele A/B) picks
    // one of the 20 base species 1:1, and the family table chooses its
    // animation folder. There is no per-dinosaur position in the file, so the
    // port distributes them inside the saved fence rectangles (paddocks).
    struct ImportedSpecies {
        dino::Mesh mesh;
        dino::Skeleton skeleton;
        dino::Skin skin;
        dino::Animation anim;
        std::vector<int> vertex_bone;
        std::vector<int> bone_skel;
        GLuint texture = 0;
    };
    struct ImportedDino {
        std::shared_ptr<ImportedSpecies> assets;
        GpuMesh gpu;
        GpuMesh shadow;
        std::string species;
        std::string family;
        std::string clip;
        bool carnivore = true;  // diet gene 14; drives the food source match
        dino::DinoNeeds needs;
        dino::DinoAttributes attributes;  // phenotype snapshot for objectives
        std::optional<dino::DinoActionLoop> action_loop;
        float x = 0.0f;
        float y = 0.0f;
        float angle = 0.0f;
        double phase = 0.0;
    };
    std::map<std::string, std::shared_ptr<ImportedSpecies>> imported_species;
    std::vector<ImportedDino> imported_dinos;
    // Judge pipeline shared by `--show` and the placed-show scheduler:
    // FUN_00618020 five needs, dot with the active `.bvi` profile weights,
    // clamp [-100,+100] per participant, max over participants, then
    // [-130,+130] per profile.
    auto sync_show_identity = [&](dino::CampaignShowState& show,
                                  const std::vector<int>& participants) {
        show.participant_ids.clear();
        show.participant_hybrid.clear();
        for (int index : participants) {
            if (index < 0 ||
                index >= static_cast<int>(imported_dinos.size())) {
                continue;
            }
            show.participant_ids.push_back(index);
            show.participant_hybrid.push_back(
                imported_dinos[static_cast<std::size_t>(index)]
                    .attributes.originality);
        }
        show.participants =
            static_cast<int>(show.participant_ids.size());
    };
    auto fill_show_scores = [&](dino::CampaignShowState& show,
                                const std::vector<int>& participants) {
        sync_show_identity(show, participants);
        if (all_visitor_profiles.empty()) {
            try {
                all_visitor_profiles =
                    load_all_visitor_profiles(opt.asset_root);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "all visitor profiles: %s\n", e.what());
            }
        }
        const std::vector<dino::VisitorProfile>& show_profiles =
            all_visitor_profiles.empty() ? visitor_profiles
                                         : all_visitor_profiles;
        for (const dino::VisitorProfile& profile : show_profiles) {
            float best = -1.0e9f;
            for (int index : show.participant_ids) {
                const auto needs = dino::dino_show_needs(
                    imported_dinos[static_cast<std::size_t>(index)]
                        .attributes,
                    phenotype_table);
                float dot = 0.0f;
                for (std::size_t i = 0; i < needs.size(); ++i) {
                    dot += needs[i] * profile.weights[i];
                }
                best = std::max(best, std::clamp(dot, -100.0f, 100.0f));
            }
            if (best > -1.0e8f) {
                show.set_score(profile.personality_id,
                               std::clamp(best, -130.0f, 130.0f));
            }
        }
    };
    // Personality ids of the active visitor table, used to zero-fill a
    // placed show's objective score vector (`FUN_00580f00` -> `FUN_00576860`).
    auto show_profile_ids = [&]() {
        if (all_visitor_profiles.empty()) {
            try {
                all_visitor_profiles =
                    load_all_visitor_profiles(opt.asset_root);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "all visitor profiles: %s\n", e.what());
            }
        }
        const std::vector<dino::VisitorProfile>& profiles =
            all_visitor_profiles.empty() ? visitor_profiles
                                         : all_visitor_profiles;
        std::vector<int> ids;
        ids.reserve(profiles.size());
        for (const dino::VisitorProfile& profile : profiles) {
            ids.push_back(profile.personality_id);
        }
        return ids;
    };
    // Applies the `--place-show` / `--show-participants` / `--show-hour` /
    // `--show-iterations` hooks once, after the mission catalog is loaded and
    // the imported dinosaurs exist. Interactive placement uses the same
    // ShowRuntime entry points.
    auto apply_show_specs = [&]() {
        if (show_placements_applied) {
            return;
        }
        show_placements_applied = true;
        if (show_catalog.empty() && !opt.place_show_specs.empty()) {
            std::printf("show: mission catalog is empty\n");
            return;
        }
        const std::vector<int> profile_ids = show_profile_ids();
        for (const std::string& raw : opt.place_show_specs) {
            const PortNameSpec spec = parse_name_spec(raw);
            const int catalog_index =
                dino::find_show_catalog_index(show_catalog, spec.name);
            if (catalog_index < 0) {
                std::printf("show: %s not in the mission catalog\n",
                            spec.name.c_str());
                continue;
            }
            int x = 64 + 8 * show_runtime.count();
            int y = 64;
            if (spec.values.size() >= 2) {
                x = spec.values[0];
                y = spec.values[1];
            }
            const int index = show_runtime.place(
                show_catalog[static_cast<std::size_t>(catalog_index)], x, y,
                profile_ids);
            if (index < 0) {
                std::printf("show: no free hour slot for %s\n",
                            spec.name.c_str());
                continue;
            }
            if (opt.show_locked) {
                show_runtime.set_locked(index, true);
            }
            const dino::ParkShow* show = show_runtime.at(index);
            std::printf(
                "show: placed %s at %d,%d slot=%d (%s) size=%dx%d\n",
                show->entry.stem.c_str(), show->cell_x, show->cell_y,
                show->slot, show_slot_text(show->slot).c_str(),
                show->entry.width, show->entry.height);
        }
        for (const std::string& raw : opt.show_participant_specs) {
            const PortNameSpec spec = parse_name_spec(raw);
            bool matched = false;
            for (int i = 0; i < show_runtime.count(); ++i) {
                dino::ParkShow* show = show_runtime.at(i);
                if (!dino::show_catalog_entry_matches(show->entry, spec.name)) {
                    continue;
                }
                matched = true;
                show_runtime.set_participants(i, spec.values);
                sync_show_identity(show->record, spec.values);
                std::printf("show: %s participants=%zu\n",
                            show->entry.stem.c_str(),
                            show->participants.size());
            }
            if (!matched) {
                std::printf("show: %s is not placed\n", spec.name.c_str());
            }
        }
        for (const std::string& raw : opt.show_hour_specs) {
            const PortNameSpec spec = parse_name_spec(raw);
            if (spec.values.empty()) {
                continue;
            }
            for (int i = 0; i < show_runtime.count(); ++i) {
                dino::ParkShow* show = show_runtime.at(i);
                if (!dino::show_catalog_entry_matches(show->entry, spec.name)) {
                    continue;
                }
                if (show_runtime.set_slot(i, spec.values[0])) {
                    std::printf("show: %s slot=%d (%s)\n",
                                show->entry.stem.c_str(), show->slot,
                                show_slot_text(show->slot).c_str());
                }
            }
        }
        for (const std::string& raw : opt.show_iteration_specs) {
            const PortNameSpec spec = parse_name_spec(raw);
            if (spec.values.empty()) {
                continue;
            }
            for (int i = 0; i < show_runtime.count(); ++i) {
                dino::ParkShow* show = show_runtime.at(i);
                if (!dino::show_catalog_entry_matches(show->entry, spec.name)) {
                    continue;
                }
                if (show_runtime.set_iterations(i, spec.values[0])) {
                    std::printf("show: %s iterations=%d\n",
                                show->entry.stem.c_str(), show->iterations);
                }
            }
        }
        for (int i = 0; i < show_runtime.count(); ++i) {
            const dino::ParkShow* show = show_runtime.at(i);
            std::printf(
                "show schedule: %s slot=%d (%s) iterations=%d "
                "participants=%zu locked=%d\n",
                show->entry.stem.c_str(), show->slot,
                show_slot_text(show->slot).c_str(), show->iterations,
                show->participants.size(), show->locked ? 1 : 0);
        }
    };
    // Retail assigns show participants through the cage list. The port has no
    // cage-list panel, so a freshly placed arena fills its cages with the
    // first unassigned imported dinosaurs of each size class
    // (`DinoPhenotypeTable::size_category_max`, `saDinosaur+0x1e0`).
    auto auto_assign_show_participants = [&](int show_index) {
        dino::ParkShow* show = show_runtime.at(show_index);
        if (show == nullptr) {
            return;
        }
        std::array<int, 4> remaining = show->entry.cage_size;
        std::vector<int> participants;
        for (int i = 0; i < static_cast<int>(imported_dinos.size()); ++i) {
            const int length = imported_dinos[static_cast<std::size_t>(i)]
                                   .needs.size_cm;
            int size_class = 3;
            for (int category = 0; category < 4; ++category) {
                if (length <=
                    phenotype_table.size_category_max[static_cast<std::size_t>(
                        category)]) {
                    size_class = category;
                    break;
                }
            }
            if (remaining[static_cast<std::size_t>(size_class)] <= 0) {
                continue;
            }
            --remaining[static_cast<std::size_t>(size_class)];
            participants.push_back(i);
        }
        if (participants.empty()) {
            return;
        }
        show_runtime.set_participants(show_index, participants);
        sync_show_identity(show->record, participants);
        std::printf("show: %s participants=%zu (auto-assigned by cage size)\n",
                    show->entry.stem.c_str(), show->participants.size());
    };
    // One row of the retail attraction list (`ATTRACTION_LIST_PANEL`).
    // `FUN_00516dd0` writes the show name into `ATTRACTION_LIST_NAME<n>`;
    // an empty slot is cleared to the zero-filled scratch string
    // `DAT_006743c8`, not the INI template text. The hour labels are the
    // separate `ATTRACTION_LIST_TIME1..10` widgets (`&Schedule10AM` ..).
    // `iterations` is `show+0x8c`, `locked` is `show+0xd3`.
    auto attraction_row_text = [&](int slot, std::string& name, int& iterations,
                                   int& locked) {
        name.clear();
        iterations = 0;
        locked = 0;
        for (const dino::ParkShow& show : show_runtime.shows()) {
            if (show.slot != slot) {
                continue;
            }
            name = show.entry.title.empty() ? show.entry.stem
                                            : show.entry.title;
            iterations = show.iterations;
            locked = show.locked ? 1 : 0;
            break;
        }
    };
    // Retail danger grid (`FUN_00589CD0`): cleared/stamped/recomputed once
    // per second from the live dinosaurs. The SECU need of every dinosaur
    // comes from the cell under its position (`FUN_00571440`).
    dino::DangerGrid danger_grid;
    double danger_timer = 1.0;  // rebuild on the first frame
    // Food instances from the import/placement paths; the chooser target
    // provider reads them for the Eat action (see add_imported_dino).
    std::vector<dino::FoodSource> food_sources;
    // Fence rectangles from a `.wld`/`.sav` import, kept for the park HUD's
    // paddock list after the import block ends.
    std::vector<dino::Paddock> park_paddocks;
    // Live paddocks (capacity/occupancy) for the interactive store purchase.
    std::vector<dino::Paddock> paddocks;
    // Shared by the world import, --buy-dino and the interactive store.
    dino::DinoFamilyTable family_table;
    std::map<int, std::string> head_species;
    std::map<std::string, int> species_size_cm;
    std::function<int(const std::string&)> size_for_species;
    std::vector<dino::BaiAction> dino_action_table;
    std::array<int, 4> dino_initial_needs{50, 99, 99, 99};
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

    auto load_imported_species = [&](const std::string& species,
                                     const std::string& family,
                                     const std::string& clip) {
        const std::string key = species + "|" + family + "|" + clip;
        const auto found = imported_species.find(key);
        if (found != imported_species.end()) {
            return found->second;
        }
        auto assets = std::make_shared<ImportedSpecies>();
        const std::string dir =
            "Data/Graphics/Actors/Dinos/" + species + "/" + species;
        assets->mesh = dino::load_mesh(
            resolve_asset(opt.asset_root, dir + "_lod1.mesh"));
        assets->skeleton = dino::load_skel(
            resolve_asset(opt.asset_root, dir + ".skel"));
        assets->skin = dino::load_skin(
            resolve_asset(opt.asset_root, dir + "_lod1.skin"),
            assets->mesh.name);
        // Same bind rule as the single-dinosaur path: the `.skin` mesh is a
        // crouched bind pose, so rebuild it from the standing `.mesh` while
        // keeping the skin weights.
        if (assets->skin.mesh.positions.size() ==
                assets->mesh.positions.size() &&
            assets->skin.mesh.indices == assets->mesh.indices) {
            for (std::size_t i = 0; i < assets->mesh.positions.size(); ++i) {
                assets->skin.mesh.positions[i] = {
                    assets->mesh.positions[i].x * 100.0f,
                    assets->mesh.positions[i].y * 100.0f,
                    assets->mesh.positions[i].z * 100.0f};
            }
            if (assets->mesh.uvs.size() == assets->mesh.positions.size()) {
                assets->skin.mesh.uvs = assets->mesh.uvs;
            }
            if (assets->mesh.normals.size() == assets->mesh.positions.size()) {
                assets->skin.mesh.normals = assets->mesh.normals;
            }
            if (assets->mesh.colors.size() == assets->mesh.positions.size()) {
                assets->skin.mesh.colors = assets->mesh.colors;
            }
        }
        build_skin_mapping(assets->skin, assets->skeleton, assets->vertex_bone,
                           assets->bone_skel);
        if (!clip.empty() && !family.empty()) {
            assets->anim = dino::load_anim(resolve_asset(
                opt.asset_root, "Data/Graphics/Actors/Dinos/animations/" +
                                    family + "/" + family + "_" + clip +
                                    ".anim"));
        }
        try {
            assets->texture = upload_texture(dino::load_tga(resolve_asset(
                opt.asset_root,
                "Data/Graphics/Textures/actors/dinos/" + species + ".tga")));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "dino texture %s: %s\n", species.c_str(),
                         e.what());
        }
        imported_species[key] = assets;
        return assets;
    };

    // Store `DINO_ACHAT_SAVE_RENDER_VIEW`: one static LOD1 mesh per selected
    // store genotype, fitted into the preview well at draw time.
    struct StorePreview {
        GpuMesh gpu;
        GLuint texture = 0;
        dino::Vec3 center{};
        float size = 1.0f;
        bool ready = false;
    };
    std::map<std::string, StorePreview> store_previews;
    auto store_preview_for =
        [&](const dino::ui::StoreRow& row) -> StorePreview* {
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
    };

    auto add_imported_dino = [&](const std::string& species,
                                 const std::string& family,
                                 const std::string& clip, float x, float y,
                                 float angle,
                                 const dino::DinoAttributes* attributes =
                                     nullptr) -> bool {
        auto assets = load_imported_species(species, family, clip);
        if (!assets) {
            return false;
        }
        ImportedDino dinosaur;
        dinosaur.assets = assets;
        dinosaur.species = species;
        dinosaur.family = family;
        dinosaur.clip = clip;
        dinosaur.x = x;
        dinosaur.y = y;
        dinosaur.angle = angle;
        dinosaur.phase =
            static_cast<double>(imported_dinos.size()) * 617.0;
        dinosaur.needs.value = dino_initial_needs;
        dinosaur.needs.threat = std::max(0, opt.dino_threat);
        dinosaur.needs.threat_radius = std::max(0, opt.dino_radius);
        dinosaur.needs.health = std::max(0, opt.dino_health);
        dinosaur.needs.max_health = std::max(0, opt.dino_max_health);
        // The `.bai` Eat gate reads attribute 4 (size in cm); derive a rough
        // body length from the rest mesh so quadrupeds/bipeds pick the right
        // eater action.
        dino::Vec3 lo{1e30f, 1e30f, 1e30f};
        dino::Vec3 hi{-1e30f, -1e30f, -1e30f};
        for (const dino::Vec3& p : assets->mesh.positions) {
            lo.x = std::min(lo.x, p.x);
            lo.y = std::min(lo.y, p.y);
            lo.z = std::min(lo.z, p.z);
            hi.x = std::max(hi.x, p.x);
            hi.y = std::max(hi.y, p.y);
            hi.z = std::max(hi.z, p.z);
        }
        if (hi.x > lo.x) {
            dinosaur.needs.size_cm = static_cast<int>(
                std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) * 100.0f);
        }
        if (attributes != nullptr && attributes->length_cm > 0) {
            dinosaur.attributes = *attributes;
            // Retail attribute 4 (`actor+0x50`) is the phenotype length, not
            // the rest-mesh bbox; the other values feed the `.bai` gates and
            // the GetCourage/ObstAttackSize1 completion writers.
            dinosaur.needs.size_cm = attributes->length_cm;
            dinosaur.needs.aggression = attributes->aggression;
            dinosaur.needs.stamina = attributes->stamina;
            dinosaur.needs.agility = attributes->agility;
            dinosaur.needs.combined_attack = attributes->combined_attack;
            dinosaur.needs.obstacle_radius = attributes->obstacle_radius;
        }
        if (opt.dino_action_loop && !dino_action_table.empty()) {
            dino::DinoActionLoopConfig config;
            config.action_scale =
                1.0 / std::max(0.05f, opt.dino_action_speed);
            dinosaur.action_loop.emplace(&dino_action_table, dinosaur.needs,
                                         config);
        }
        const dino::Animation* anim_ptr =
            assets->anim.bones.empty() ? nullptr : &assets->anim;
        dino::Mesh pose = build_skinned_mesh(
            assets->skin.mesh, assets->skin, assets->skeleton, anim_ptr, 0.0,
            assets->vertex_bone, assets->bone_skel, false);
        if (assets->mesh.colors.size() == pose.positions.size()) {
            pose.colors = assets->mesh.colors;
        }
        pose = place_on_terrain(std::move(pose), terrain, x, y, angle);
        dinosaur.gpu = upload_mesh(pose, glfns);
        const float half = terrain.world_size * 0.5f;
        dinosaur.shadow = upload_mesh(
            make_contact_shadow(x - half, y - half,
                                terrain.height_at(x - half, y - half) + 0.06f,
                                2.2f),
            glfns);
        imported_dinos.push_back(std::move(dinosaur));
        const std::size_t dino_index = imported_dinos.size() - 1;
        if (imported_dinos[dino_index].action_loop) {
            // `node+0x3c` target policy (`GetTarget` +0x14). The park target
            // factory is not decoded yet, so Eat/EatForSmallies are modelled
            // as the nearest stocked food source (box target); every other
            // action has no target (-1, threat term skipped).
            imported_dinos[dino_index].action_loop->set_threat_provider(
                [&danger_grid, &imported_dinos, &food_sources,
                 dino_index](const dino::BaiAction& action) {
                    const ImportedDino& self = imported_dinos[dino_index];
                    if (action.name != "Eat" &&
                        action.name != "EatForSmallies") {
                        return 0;
                    }
                    const dino::FoodSource* nearest = nullptr;
                    float best = 0.0f;
                    for (const dino::FoodSource& source : food_sources) {
                        if (source.remaining <= 0.0f) {
                            continue;
                        }
                        const float dx = source.x - self.x;
                        const float dy = source.y - self.y;
                        const float d2 = dx * dx + dy * dy;
                        if (nearest == nullptr || d2 < best) {
                            nearest = &source;
                            best = d2;
                        }
                    }
                    if (nearest == nullptr) {
                        return 0;
                    }
                    dino::ActionTarget target;
                    target.kind = dino::ActionTarget::Kind::Box;
                    target.x = static_cast<int>(nearest->x);
                    target.y = static_cast<int>(nearest->y);
                    // The loop owns the live needs; the outer copy is only
                    // the spawn snapshot.
                    const dino::DinoNeeds& needs =
                        self.action_loop ? self.action_loop->needs()
                                         : self.needs;
                    return dino::chooser_threat_score(
                        target, static_cast<int>(self.x),
                        static_cast<int>(self.y), needs.threat,
                        needs.threat_radius, danger_grid);
                });
        }
        return true;
    };

    auto clear_imported_dinos = [&]() {
        for (ImportedDino& dinosaur : imported_dinos) {
            dinosaur.gpu.destroy(glfns);
            dinosaur.shadow.destroy(glfns);
        }
        imported_dinos.clear();
    };

    Bounds bounds;
    for (const auto& p : display.positions) {
        bounds.add(p);
    }
    if (place_dino) {
        for (const auto& p : pose_dino(0.0).positions) {
            bounds.add(p);
        }
    }

    std::vector<GpuMesh> veg_gpu;
    std::vector<GLuint> veg_textures;
    veg_gpu.reserve(vegetation.groups.size());
    veg_textures.reserve(vegetation.groups.size());
    for (const auto& group : vegetation.groups) {
        veg_gpu.push_back(upload_mesh(group.mesh, glfns));
        GLuint group_texture = 0;
        if (!group.texture_path.empty() && std::filesystem::exists(group.texture_path)) {
            try {
                group_texture = upload_texture(dino::load_tga(group.texture_path));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "vegetation texture: %s\n", e.what());
            }
        }
        veg_textures.push_back(group_texture);
        for (const auto& p : group.mesh.positions) {
            bounds.add(p);
        }
    }

    GpuMesh path_gpu;
    GpuMesh fence_gpu;
    GpuMesh road_preview_gpu;
    bool road_preview_ok = true;
    GLuint road_preview_texture = 0;  // focus/road.tga x material colour
    dino::Image road_helper_image;
    dino::Mesh road_helper_box;       // focus/helper_road.mesh
    GLuint path_texture = 0;
    GLuint fence_texture = 0;
    if (build_ready) {
        path_texture = path_image.width > 0 ? upload_texture(path_image) : 0;
        fence_texture = fence_image.width > 0 ? upload_texture(fence_image) : 0;
        const dino::BuildMeshes meshes =
            dino::build_placement_meshes(build_grid, terrain, fence_source);
        path_gpu = upload_mesh(meshes.paths, glfns);
        fence_gpu = upload_mesh(meshes.fences, glfns);
    }
    GpuMesh fence_preview_gpu;
    bool fence_preview_visible = false;
    bool fence_preview_ok = true;
    int fence_preview_cell_x = -1;
    int fence_preview_cell_y = -1;
    GLuint fence_preview_ok_texture = 0;
    GLuint fence_preview_bad_texture = 0;
    {
        auto solid_texture = [](std::uint8_t r, std::uint8_t g,
                                std::uint8_t b) {
            dino::Image image;
            image.width = 1;
            image.height = 1;
            image.rgba = {r, g, b, 255};
            return upload_texture(image);
        };
        // Retail enclosure disc samples: blue (38,38,136), red (80,38,38).
        fence_preview_ok_texture = solid_texture(38, 38, 136);
        fence_preview_bad_texture = solid_texture(80, 38, 38);
    }

    SkyboxGL skybox;
    int sky_faces = 0;
    for (int i = 0; i < 6; ++i) {
        if (sky_images[i].width <= 0) {
            continue;
        }
        skybox.faces[i] = upload_mesh(make_sky_face(i, 8000.0f), glfns);
        skybox.textures[i] = upload_texture(sky_images[i]);
        if (skybox.faces[i].vao != 0 && skybox.textures[i] != 0) {
            ++sky_faces;
        }
    }
    skybox.ready = sky_faces == 6;

    GLuint water_texture = 0;
    GLuint water_texture2 = 0;
    GpuMesh water_gpu;
    if (water_image.width > 0 && terrain.world_size > 0.0f) {
        water_texture = upload_texture_wrapped(water_image, GL_REPEAT);
        if (water2_image.width > 0) {
            water_texture2 =
                upload_texture_wrapped(water2_image, GL_REPEAT);
        }
        water_gpu = upload_mesh(
            make_water_plane(terrain.world_size * 4.75f, water_z, 38.0f), glfns);
    }

    GpuMesh visitor_gpu;
    GLuint visitor_texture_id = 0;
    GpuMesh employee_gpu;
    GLuint employee_texture_id = 0;
    dino::Mesh employee_pose;
    dino::Image employee_image;
    if (!visitor_system.visitors.empty()) {
        visitor_texture_id = upload_texture(visitor_image);
        visitor_gpu = upload_mesh(
            visitor_system.build_mesh(build_grid, terrain, visitor_pose1,
                                      visitor_pose2),
            glfns);
    }

    // `.bbi` building placement (static LOD1) for world-object bring-up.
    struct BuildingDraw {
        GpuMesh gpu;
        GLuint texture = 0;
    };
    std::vector<BuildingDraw> building_draws;
    std::vector<dino::PortBuildingState> building_instances;
    // kind -> .bbi definition, filled on placement for the campaign
    // connected-buildings objective (category/footprint).
    std::map<int, dino::BuildingDef> building_defs;
    std::vector<dino::WorldRect> wld_roads;
    std::vector<dino::WorldRect> wld_fences;
    std::vector<dino::WorldDinosaur> wld_dinosaurs;
    dino::SavSimulation sav_simulation;
    std::vector<dino::WorldFence> sav_fences;
    std::vector<dino::WorldBuilding> sav_buildings;
    std::vector<dino::WorldVisitor> sav_world_visitors;
    bool sav_ready = false;
    std::vector<dino::Attraction> attractions;
    int entrance_cell_x = -1;
    int entrance_cell_y = -1;
    dino::Vec3 building_center{1e30f, 1e30f, 1e30f};

    auto clear_buildings = [&]() {
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
        building_center = {1e30f, 1e30f, 1e30f};
    };

    auto place_building = [&](const dino::BuildingDef& def, float cell_x,
                              float cell_y, int state, float extra_angle_deg,
                              float scale, int kind) {
        if (terrain.world_size <= 0.0f) {
            std::fprintf(stderr, "building: --lnd is required for placement\n");
            return false;
        }
        try {
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
            dino::Mesh placed;
            if (ends_with_ci(mesh_rel, ".skin")) {
                dino::Skin skinned =
                    dino::load_skin_auto(resolve_asset(opt.asset_root, mesh_rel));
                placed = std::move(skinned.mesh);
                // `.skin` full meshes are stored in centimetres.
                for (dino::Vec3& p : placed.positions) {
                    p.x *= 0.01f;
                    p.y *= 0.01f;
                    p.z *= 0.01f;
                }
            } else {
                placed = dino::load_mesh(resolve_asset(opt.asset_root, mesh_rel));
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
            GLuint texture = 0;
            const std::string texture_name = placed.texture_name();
            if (!texture_name.empty()) {
                try {
                    texture = upload_texture(dino::load_tga(
                        resolve_asset(opt.asset_root,
                                      texture_dir + "/" + texture_name)));
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "building texture: %s\n", e.what());
                }
            }
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
            const float radians = angle_deg * 3.14159265f / 180.0f;
            const float cos_a = std::cos(radians);
            const float sin_a = std::sin(radians);
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
            building_draws.push_back({upload_mesh(placed, glfns), texture});
            if (kind >= 0) {
                building_defs[kind] = def;
                building_instances.push_back(
                    {kind, static_cast<int>(cell_x), static_cast<int>(cell_y),
                     orientation});
            }
            const int centre_cell_x = static_cast<int>(std::floor(
                centre_x / (build_grid.cell_size > 0.0f
                                ? build_grid.cell_size
                                : 4.0f)));
            const int centre_cell_y = static_cast<int>(std::floor(
                centre_y / (build_grid.cell_size > 0.0f
                                ? build_grid.cell_size
                                : 4.0f)));
            if (def.category == 10) {
                entrance_cell_x = centre_cell_x;
                entrance_cell_y = centre_cell_y;
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
            std::printf(
                "building: %s mesh=%s verts=%zu tris=%zu cell=%.1f,%.1f "
                "centre=%.1f,%.1f z=%.1f state=%d angle=%.0f texture=%s\n",
                def.name.c_str(), mesh_rel.c_str(), placed.positions.size(),
                placed.triangle_count(), cell_x, cell_y, centre_x, centre_y, wz,
                orientation, angle_deg, texture_name.c_str());
            return true;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "building: %s\n", e.what());
            return false;
        }
    };

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

    auto place_world_buildings = [&](const dino::WldWorld& world) {
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
    };

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

    // MAP_RENDERVIEW: 256x256 detail-map target displayed in the 136x136
    // widget. Recovered from FUN_00529880 / FUN_005fb7e0 / FUN_005fb120:
    // - map scale at +0x38, default 0.25, clamped to 0.1..2.0;
    // - held zoom buttons change it by 0.15 per second (in decreases);
    // - the map is centred on the camera target and rotated by the camera
    //   heading + 90 degrees, so it turns with the park camera.
    MinimapTarget minimap;
    const bool minimap_ready = minimap.create(glfns, 256);
    float minimap_scale = std::clamp(opt.minimap_scale, 0.1f, 2.0f);
    bool minimap_zoom_in = false;
    bool minimap_zoom_out = false;
    // NAVIGATION_PANEL hold actions (camera, not minimap).
    bool nav_pan_forward = false;
    bool nav_pan_back = false;
    bool nav_pan_left = false;
    bool nav_pan_right = false;
    bool nav_turn_left = false;
    bool nav_turn_right = false;
    bool nav_zoom_in = false;
    bool nav_zoom_out = false;

    auto minimap_click_zoom = [&](int delta) {
        minimap_scale =
            std::clamp(minimap_scale + 0.05f * static_cast<float>(delta),
                       0.1f, 2.0f);
        std::printf("minimap scale=%.2f\n", minimap_scale);
    };

    if (pending_minimap_zoom != 0) {
        minimap_click_zoom(pending_minimap_zoom);
    }

    auto render_minimap = [&](const dino::Vec3& focus, float heading_rad) {
        if (!minimap_ready || terrain.world_size <= 0.0f) {
            return;
        }
        const float half = terrain.world_size * 2.0f * minimap_scale;
        const dino::Vec3 eye{focus.x, focus.y, 4000.0f};
        // Put the camera's forward direction at the top of the map so the
        // island rotates with the park camera (FUN_005fb120).
        const dino::Vec3 up{std::cos(heading_rad), std::sin(heading_rad), 0.0f};

        glfns.BindFramebuffer(GL_FRAMEBUFFER, minimap.fbo);
        glViewport(0, 0, minimap.size, minimap.size);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glClearColor(0.10f, 0.29f, 0.43f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const dino::Mat4 view =
            look_at(eye, {focus.x, focus.y, 0.0f}, up);
        const dino::Mat4 proj =
            ortho(-half, half, -half, half, 0.1f, 8000.0f);
        const dino::Mat4 mvp = dino::multiply(proj, view);

        glfns.UseProgram(program);
        glfns.UniformMatrix4fv(glfns.GetUniformLocation(program, "u_mvp"), 1,
                               GL_TRUE, mvp.m);
        glfns.Uniform1i(glfns.GetUniformLocation(program, "u_unlit"), 1);
        glfns.Uniform1i(glfns.GetUniformLocation(program, "u_use_texture"),
                        texture != 0 ? 1 : 0);
        glfns.Uniform1i(
            glfns.GetUniformLocation(program, "u_use_texture2"), 0);
        glfns.Uniform1i(
            glfns.GetUniformLocation(program, "u_modulate2x"), 0);
        glfns.Uniform1f(glfns.GetUniformLocation(program, "u_alpha"), 1.0f);
        if (texture != 0) {
            glfns.ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
        }
        glfns.BindVertexArray(gpu.vao);
        glDrawElements(GL_TRIANGLES, gpu.index_count, GL_UNSIGNED_INT, nullptr);

        glfns.BindFramebuffer(GL_FRAMEBUFFER, 0);
    };

    std::vector<DrawItem> extra_items;
    std::vector<dino::FoodDef> food_defs;
    GpuMesh food_gpu;
    GLuint food_texture = 0;
    struct ShowFeatureDraw {
        GpuMesh gpu;
        GLuint texture = 0;
    };
    std::vector<ShowFeatureDraw> show_feature_draws;
    auto rebuild_extra_items = [&]() {
        extra_items.clear();
        for (std::size_t i = 0; i < veg_gpu.size(); ++i) {
            extra_items.push_back({&veg_gpu[i], veg_textures[i]});
        }
        // Roads are painted into the terrain texture when the island has a
        // basemap; the tile mesh is only the fallback for bare terrain.
        if (path_gpu.vao != 0 && (image.width <= 0 || !opt.texture_path.empty())) {
            extra_items.push_back({&path_gpu, path_texture});
        }
        if (road_preview_gpu.vao != 0 && road_preview_texture != 0) {
            // Retail helper (Data/States/Helper.ini): focus/road.tga times
            // the pulsing gscMaterialBank colour, blended One/One.
            DrawItem item{&road_preview_gpu, road_preview_texture};
            // The captured retail fill is exactly the pulsing material
            // colour (64,64,135 at the captured phase), not a blend with the
            // grass. Draw the tinted helper unlit and opaque, with depth so
            // only the nearest box face contributes.
            item.unlit = true;
            item.additive = false;
            item.alpha = 1.0f;
            item.depth_write = true;
            extra_items.push_back(item);
        }
        if (fence_preview_visible && fence_preview_gpu.vao != 0) {
            DrawItem item{&fence_preview_gpu,
                          fence_preview_ok ? fence_preview_ok_texture
                                           : fence_preview_bad_texture};
            item.unlit = true;       // exact retail disc colour
            item.depth_write = true; // occluded by fence posts
            extra_items.push_back(item);
        }
        if (fence_gpu.vao != 0) {
            extra_items.push_back({&fence_gpu, fence_texture});
        }
        if (visitor_gpu.vao != 0) {
            extra_items.push_back({&visitor_gpu, visitor_texture_id});
        }
        if (employee_gpu.vao != 0) {
            extra_items.push_back({&employee_gpu, employee_texture_id});
        }
        if (dino_gpu.vao != 0) {
            extra_items.push_back({&dino_gpu, dino_texture});
            if (shadow_gpu.vao != 0) {
                extra_items.push_back({&shadow_gpu, 0, 0.35f});
            }
        }
        for (const ImportedDino& dinosaur : imported_dinos) {
            if (dinosaur.gpu.vao != 0) {
                extra_items.push_back(
                    {&dinosaur.gpu, dinosaur.assets->texture});
                if (dinosaur.shadow.vao != 0) {
                    extra_items.push_back({&dinosaur.shadow, 0, 0.35f});
                }
            }
        }
        if (food_gpu.vao != 0 && !food_sources.empty() &&
            food_sources.front().remaining > 0.0f) {
            extra_items.push_back({&food_gpu, food_texture});
        }
        for (const ShowFeatureDraw& feature : show_feature_draws) {
            if (feature.gpu.vao != 0) {
                extra_items.push_back({&feature.gpu, feature.texture});
            }
        }
        for (const BuildingDraw& building : building_draws) {
            if (building.gpu.vao != 0) {
                extra_items.push_back({&building.gpu, building.texture});
            }
        }
        // Transparent pass: water last so it blends over all opaque actors.
        // WaterSurface.ini is additive (src One, dest One, add) and uses a
        // second texture stage: Modulate2x on coordinate set 1 (same TGA,
        // 28/30 tiling + 0.25 offset, opposite scroll in retail).
        if (water_gpu.vao != 0) {
            DrawItem water;
            water.mesh = &water_gpu;
            water.texture = water_texture;
            water.alpha = 1.0f;
            water.additive = true;
            water.fog = 1;
            if (opt.water2 && water_texture2 != 0) {
                water.texture2 = water_texture2;
                water.modulate2x = true;
                water.uv2_scale[0] = 28.0f / 30.0f;
                water.uv2_scale[1] = 28.0f / 30.0f;
                water.uv2_offset[0] = 0.25f / 30.0f;
            }
            extra_items.push_back(water);
        }
    };
    rebuild_extra_items();

    // Retail paints roads into the island texture (gpkLandscapeTextureManager,
    // re-run on the scene dirty flag); `image` stays the clean basemap.
    auto refresh_terrain_texture = [&]() {
        if (!opt.texture_path.empty() || image.width <= 0) {
            return;
        }
        dino::Image painted = image;
        if (build_ready) {
            dino::paint_roads(painted, build_grid, terrain.world_size);
        }
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }
        texture = upload_texture(painted);
    };
    auto rebuild_build_meshes = [&]() {
        if (!build_ready) {
            return;
        }
        refresh_terrain_texture();
        const dino::BuildMeshes meshes =
            dino::build_placement_meshes(build_grid, terrain, fence_source);
        path_gpu.destroy(glfns);
        fence_gpu.destroy(glfns);
        path_gpu = upload_mesh(meshes.paths, glfns);
        fence_gpu = upload_mesh(meshes.fences, glfns);
        rebuild_extra_items();
        std::printf("build: %d path cells, %d fences\n", build_grid.path_count,
                    build_grid.fence_count);
    };

    auto import_park_layout = [&]() {
        if (wld_roads.empty() && wld_fences.empty()) {
            return;
        }
        dino::import_roads(build_grid, wld_roads);
        dino::import_fences(build_grid, wld_fences);
        rebuild_build_meshes();
        std::printf(
            "park import: roads=%zu path_cells=%d fences=%zu fence_edges=%d\n",
            wld_roads.size(), build_grid.path_count, wld_fences.size(),
            build_grid.fence_count);
    };

    if (opt.wld_park || sav_ready) {
        import_park_layout();
    }
    // `--visitors` is handled before the `.wld` roads are imported, so a
    // campaign park would spawn on zero path cells. Spawn once more after
    // the roads exist.
    if (opt.visitors > 0 && visitor_system.visitors.empty() &&
        build_grid.path_count > 0) {
        const int spawned =
            visitor_profiles.empty()
                ? visitor_system.spawn(build_grid, opt.visitors)
                : visitor_system.spawn(build_grid, opt.visitors,
                                       visitor_profiles, entrance_cell_x,
                                       entrance_cell_y);
        std::printf("visitors: %d spawned on %d path cells\n", spawned,
                    build_grid.path_count);
        if (opt.visitor_satisfaction >= 0) {
            for (dino::Visitor& visitor : visitor_system.visitors) {
                visitor.need0 =
                    std::clamp(opt.visitor_satisfaction, 0, 99);
            }
        }
        if (!visitor_system.visitors.empty()) {
            if (visitor_texture_id == 0 && visitor_image.width > 0) {
                visitor_texture_id = upload_texture(visitor_image);
            }
            visitor_gpu = upload_mesh(
                visitor_system.build_mesh(build_grid, terrain, visitor_pose1,
                                          visitor_pose2),
                glfns);
        }
    }

    std::vector<dino::EmployeeProfile> employee_profiles;
    try {
        employee_profiles = dino::load_employee_list(
            resolve_asset(opt.asset_root, "Data/Design/employees"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "employee profiles: %s\n", e.what());
    }

    dino::CameraConfig camera_config;
    try {
        camera_config = dino::load_camera_config(
            resolve_asset(opt.asset_root, "Data/Config/camera.ini"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "camera config: %s\n", e.what());
    }
    dino::BudgetLedger budget_ledger;
    int pending_buy_price = 0;
    constexpr float kPi = 3.14159265f;

    auto load_dino_family_tables = [&]() {
        if (!family_table.species.empty()) {
            return;
        }
        // family_table is shared with the interactive store path.
        try {
            family_table = dino::read_dino_family_table(
                resolve_asset(opt.asset_root,
                              "Data/Design/Genotype/anim_trans.dic"),
                resolve_asset(opt.asset_root,
                              "Data/Design/Genotype/simu_trans.dic"));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "dino family table: %s\n", e.what());
        }
        // head_species is shared with the interactive store path.
        for (const std::string& species : family_table.species) {
            try {
                const int head = dino::read_gen_head_type(resolve_asset(
                    opt.asset_root,
                    "Data/Design/Scenario/Genotype/" + species + ".gen"));
                if (head >= 0) {
                    head_species[head] = species;
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "genotype %s: %s\n", species.c_str(),
                             e.what());
            }
        }
    };

    // Import the `.wld`/`.sav` dinosaurs into the current terrain. Used at
    // startup (--wld-park/--sav-park) and after a menu campaign start loads
    // the selected campaign's island and save world.
    auto spawn_world_dinos = [&]() {
        if (!opt.import_dinos || wld_dinosaurs.empty() ||
            terrain.world_size <= 0.0f) {
            return;
        }
        int spawned = 0;
        std::map<std::string, int> species_counts;
        // Each `.wld` fence rectangle is a paddock with the retail capacity
        // model (stFence, 0x57dc60/0x57eb50). Imported dinosaurs have no
        // stored position, so they fill paddocks in record order.
        std::vector<dino::WorldRect> paddock_rects = wld_fences;
        if (paddock_rects.empty()) {
            for (const dino::WorldFence& fence : sav_fences) {
                paddock_rects.push_back({fence.start_x, fence.start_y,
                                         fence.end_x, fence.end_y});
            }
        }
        paddocks.clear();
        paddocks.reserve(paddock_rects.size());
        for (const dino::WorldRect& rect : paddock_rects) {
            dino::Paddock paddock;
            paddock.rect = rect;
            paddock.capacity =
                dino::paddock_capacity(rect, build_grid.cell_size);
            paddocks.push_back(paddock);
        }
        // `dinosaur+0x1e0` is a size class (8/16/32/64). The exact class comes
        // from the individual's phenotype; the port derives it from the
        // species rest-mesh bbox until the genotype translator is ported.
        // species_size_cm is shared with the interactive store path.
        size_for_species = [&](const std::string& species) -> int {
            const auto found = species_size_cm.find(species);
            if (found != species_size_cm.end()) {
                return found->second;
            }
            int size_cm = 300;
            try {
                const dino::Mesh rest = dino::load_mesh(resolve_asset(
                    opt.asset_root, "Data/Graphics/Actors/Dinos/" + species +
                                        "/" + species + "_lod1.mesh"));
                dino::Vec3 lo{1e30f, 1e30f, 1e30f};
                dino::Vec3 hi{-1e30f, -1e30f, -1e30f};
                for (const dino::Vec3& p : rest.positions) {
                    lo.x = std::min(lo.x, p.x);
                    lo.y = std::min(lo.y, p.y);
                    lo.z = std::min(lo.z, p.z);
                    hi.x = std::max(hi.x, p.x);
                    hi.y = std::max(hi.y, p.y);
                    hi.z = std::max(hi.z, p.z);
                }
                if (hi.x > lo.x) {
                    size_cm = static_cast<int>(
                        std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) *
                        100.0f);
                }
            } catch (const std::exception&) {
            }
            species_size_cm[species] = size_cm;
            return size_cm;
        };
        for (std::size_t i = 0; i < wld_dinosaurs.size(); ++i) {
            const dino::WorldDinosaur& record = wld_dinosaurs[i];
            const int head = static_cast<int>(std::min(
                record.genotype[0].first, record.genotype[0].second));
            const auto match = head_species.find(head);
            const std::string species =
                match != head_species.end() ? match->second : "allo";
            std::string family = family_table.family_of(species);
            if (family.empty()) {
                family = "LightBiped";
            }
            float x = terrain.world_size * 0.5f;
            float y = terrain.world_size * 0.5f;
            float angle = static_cast<float>(i) * 137.5f;
            const dino::DinoAttributes attrs =
                phenotype_table.valid
                    ? phenotype_table.attributes(record.genotype)
                    : dino::DinoAttributes{};
            const dino::DinoAttributes* attrs_ptr =
                phenotype_table.valid ? &attrs : nullptr;
            if (!paddocks.empty()) {
                const int cost_class =
                    phenotype_table.valid
                        ? phenotype_table.size_class(record.genotype)
                        : dino::paddock_size_class(size_for_species(species));
                if (std::getenv("DINO_DEBUG_PHENO") != nullptr) {
                    std::fprintf(
                        stderr,
                        "pheno %zu g0=(%u,%u) g2=(%u,%u) g3=(%u,%u) len=%d "
                        "class=%d wid=%d hei=%d P=%d I=%d B=%d Agi=%d Arm=%d "
                        "Atk=%d Ag=%d O=%d CA=%d Orn=%d St=%d Co=%d walk=%.3f run=%.3f "
                        "jump=%d wt=%d pl=%d,%d,%d,%d,%d "
                        "Ah=%d Ab=%d Aa=%d Al=%d At=%d food=%.3f "
                        "g15=(%u,%u) g16=(%u,%u) g17=(%u,%u)\n",
                        i, record.genotype[0].first,
                        record.genotype[0].second,
                        record.genotype[2].first,
                        record.genotype[2].second,
                        record.genotype[3].first,
                        record.genotype[3].second,
                        phenotype_table.length_cm(record.genotype), cost_class,
                        attrs.width_cm, attrs.height_cm, attrs.perception,
                        attrs.intelligence, attrs.beauty, attrs.agility,
                        attrs.armor, attrs.attack_total, attrs.aggression,
                        attrs.originality, attrs.conventional_appeal,
                        attrs.ornament_count, attrs.stamina, attrs.courage,
                        attrs.walk_speed,
                        attrs.run_speed, attrs.jump, attrs.weight_kg,
                        attrs.part_length[0], attrs.part_length[1],
                        attrs.part_length[2], attrs.part_length[3],
                        attrs.part_length[4], attrs.attack_head,
                        attrs.attack_body, attrs.attack_arm, attrs.attack_leg,
                        attrs.attack_tail, static_cast<double>(attrs.food_consumption),
                        record.genotype[15].first, record.genotype[15].second,
                        record.genotype[16].first, record.genotype[16].second,
                        record.genotype[17].first, record.genotype[17].second);
                }
                int paddock_index = -1;
                for (std::size_t p = 0; p < paddocks.size(); ++p) {
                    if (dino::paddock_can_add(
                            paddocks[p], static_cast<int>(i), cost_class)) {
                        paddock_index = static_cast<int>(p);
                        break;
                    }
                }
                if (paddock_index >= 0) {
                    dino::Paddock& paddock =
                        paddocks[static_cast<std::size_t>(paddock_index)];
                    dino::paddock_add(paddock, static_cast<int>(i),
                                      cost_class);
                    const dino::WorldRect& pen = paddock.rect;
                    const float x0 =
                        static_cast<float>(std::min(pen.x0, pen.x1));
                    const float x1 =
                        static_cast<float>(std::max(pen.x0, pen.x1));
                    const float y0 =
                        static_cast<float>(std::min(pen.y0, pen.y1));
                    const float y1 =
                        static_cast<float>(std::max(pen.y0, pen.y1));
                    const float radius =
                        std::max(1.0f, std::min(x1 - x0, y1 - y0) * 0.28f);
                    const float theta =
                        static_cast<float>(paddock.occupants.size() - 1) *
                            2.39996323f +
                        static_cast<float>(paddock_index) * 0.7f;
                    x = (x0 + x1) * 0.5f + std::cos(theta) * radius;
                    y = (y0 + y1) * 0.5f + std::sin(theta) * radius;
                    angle = theta * 180.0f / kPi + 90.0f;
                } else {
                    std::printf("dino %u (%s): no paddock capacity\n",
                                record.object_id, species.c_str());
                }
            }
            try {
                if (add_imported_dino(species, family, opt.dino_clip, x, y,
                                      angle, attrs_ptr)) {
                    imported_dinos.back().carnivore =
                        dino::genotype_is_carnivore(record.genotype);
                    ++spawned;
                    ++species_counts[species];
                }
            } catch (const std::exception& e) {
                std::fprintf(stderr, "dino %u (%s): %s\n",
                             record.object_id, species.c_str(), e.what());
            }
        }
        if (spawned > 0) {
            std::printf("dinos: %d/%zu spawned", spawned,
                        wld_dinosaurs.size());
            for (const auto& [species, count] : species_counts) {
                std::printf(" %s=%d", species.c_str(), count);
            }
            std::printf("\n");
            rebuild_extra_items();
        }
        park_paddocks = paddocks;
    };

    // The totals must be printed after `--buy-dino` as well; the buy path
    // adds the new occupant to `paddocks`.
    auto print_paddock_totals = [&]() {
        if (paddocks.empty()) {
            return;
        }
        float capacity = 0.0f;
        float used = 0.0f;
        for (const dino::Paddock& paddock : paddocks) {
            capacity += paddock.capacity;
            used += paddock.used;
        }
        std::printf("paddocks: %zu capacity=%.0f used=%.0f\n",
                    paddocks.size(), capacity, used);
    };

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
                        1000000 + static_cast<int>(imported_dinos.size());
                    int paddock_index = -1;
                    if (opt.buy_paddock >= 0 &&
                        opt.buy_paddock < static_cast<int>(paddocks.size()) &&
                        dino::paddock_can_add(
                            paddocks[static_cast<std::size_t>(
                                opt.buy_paddock)],
                            dino_id, cost_class)) {
                        paddock_index = opt.buy_paddock;
                    } else if (opt.buy_paddock < 0) {
                        for (std::size_t p = 0; p < paddocks.size(); ++p) {
                            if (dino::paddock_can_add(paddocks[p], dino_id,
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
                            paddocks[static_cast<std::size_t>(
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
                                have_attributes ? &buy_attrs : nullptr)) {
                            imported_dinos.back().carnivore =
                                have_genotype
                                    ? dino::genotype_is_carnivore(buy_genotype)
                                    : true;
                            dino::paddock_add(paddock, dino_id, cost_class);
                            budget_ledger.spend_dinoachat(entry->price);
                            pending_buy_price = entry->price;
                            std::printf(
                                "buy: %s price=%d budget=%d paddock=%d "
                                "dinos=%zu\n",
                                entry->name.c_str(), entry->price,
                                buy_budget - entry->price, paddock_index,
                                imported_dinos.size());
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
                    imported_dinos.size(), park_paddocks.size());
    }

    // Food sources (`--place-food`): the `.bsi` definition supplies the model,
    // texture and portion count; the position is a build-grid cell and
    // defaults to the first paddock centre.
    if (food_defs.empty() &&
        (!opt.place_food.empty() || opt.hire_nutritionists > 0 ||
         opt.feed_now)) {
        try {
            food_defs = dino::load_food_defs(
                opt.asset_root,
                resolve_asset(opt.asset_root, "Data/Design/foodsource"));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "food defs: %s\n", e.what());
        }
    }
    if (!opt.place_food.empty()) {
        try {
            food_defs = dino::load_food_defs(
                opt.asset_root,
                resolve_asset(opt.asset_root, "Data/Design/foodsource"));
            int def_index = -1;
            for (std::size_t i = 0; i < food_defs.size(); ++i) {
                if (food_defs[i].name == opt.place_food) {
                    def_index = static_cast<int>(i);
                    break;
                }
            }
            if (def_index < 0) {
                const int kind = std::atoi(opt.place_food.c_str());
                for (std::size_t i = 0; i < food_defs.size(); ++i) {
                    if (food_defs[i].kind == kind) {
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
                         build_grid.cell_size;
                    fy = (static_cast<float>(cy) + 0.5f) *
                         build_grid.cell_size;
                    have_position = true;
                } else if (!paddocks.empty()) {
                    const dino::WorldRect& rect = paddocks.front().rect;
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
                        food_defs[static_cast<std::size_t>(def_index)];
                    food_sources.push_back({def_index, fx, fy, def.quantity});
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

    // Paddock helpers for the feeding gate.
    auto paddock_at = [&](float x, float y) -> int {
        for (std::size_t p = 0; p < paddocks.size(); ++p) {
            const dino::WorldRect& r = paddocks[p].rect;
            if (x >= static_cast<float>(std::min(r.x0, r.x1)) &&
                x <= static_cast<float>(std::max(r.x0, r.x1)) &&
                y >= static_cast<float>(std::min(r.y0, r.y1)) &&
                y <= static_cast<float>(std::max(r.y0, r.y1))) {
                return static_cast<int>(p);
            }
        }
        return -1;
    };
    auto paddock_food = [&](int paddock, bool carnivore)
        -> dino::FoodSource* {
        if (paddock < 0 ||
            paddock >= static_cast<int>(paddocks.size())) {
            return nullptr;
        }
        const dino::WorldRect& r =
            paddocks[static_cast<std::size_t>(paddock)].rect;
        for (dino::FoodSource& source : food_sources) {
            if (source.remaining <= 0.0f || source.def < 0 ||
                source.def >= static_cast<int>(food_defs.size())) {
                continue;
            }
            if (dino::food_is_carnivore(
                    food_defs[static_cast<std::size_t>(source.def)].kind) !=
                carnivore) {
                continue;
            }
            if (source.x >= static_cast<float>(std::min(r.x0, r.x1)) &&
                source.x <= static_cast<float>(std::max(r.x0, r.x1)) &&
                source.y >= static_cast<float>(std::min(r.y0, r.y1)) &&
                source.y <= static_cast<float>(std::max(r.y0, r.y1))) {
                return &source;
            }
        }
        return nullptr;
    };
    auto log_dino_food = [&]() {
        for (const ImportedDino& dinosaur : imported_dinos) {
            const int pen = paddock_at(dinosaur.x, dinosaur.y);
            const dino::FoodSource* source =
                paddock_food(pen, dinosaur.carnivore);
            std::printf("dino food: %s paddock=%d diet=%s source=%s\n",
                        dinosaur.species.c_str(), pen,
                        dinosaur.carnivore ? "carnivore" : "herbivore",
                        source != nullptr
                            ? food_defs[static_cast<std::size_t>(
                                            source->def)]
                                  .name.c_str()
                            : "none");
        }
    };
    if (opt.dino_action_loop) {
        log_dino_food();
    }

    int build_tool = opt.build_mode ? 1 : 0;
    int pending_construct_kind = -1;
    bool pending_click = false;
    float click_x = 0.0f;
    float click_y = 0.0f;
    float drag_distance = 0.0f;
    if (!opt.ground_click.empty()) {
        int gx = 0;
        int gy = 0;
        if (std::sscanf(opt.ground_click.c_str(), "%d,%d", &gx, &gy) == 2) {
            pending_click = true;
            click_x = static_cast<float>(gx);
            click_y = static_cast<float>(gy);
        }
    }

    dino::Vec3 center = place_dino
        ? dino::Vec3{spawn_x - terrain.world_size * 0.5f,
                     spawn_y - terrain.world_size * 0.5f,
                     terrain.height_at(spawn_x - terrain.world_size * 0.5f,
                                       spawn_y - terrain.world_size * 0.5f) + 3.0f}
        : bounds.center();

    float auto_distance = place_dino ? 22.0f
        : std::max(2.0f, bounds.extent() * 1.7f);
    float distance = opt.distance > 0.0f ? opt.distance : auto_distance;
    float yaw = opt.yaw;
    float pitch = opt.pitch;
    // Retail park camera model (camera.ini): one `progress` p in [0,1] drives
    // both the horizontal radius and the height above the target, so zoom and
    // tilt are the same axis. Explicit CLI camera views keep the legacy
    // spherical orbit for deterministic screenshots/tests.
    float camera_progress = 0.1f;
    float camera_yaw_goal = yaw;
    bool camera_retail_model =
        opt.camera_target.empty() && opt.distance <= 0.0f;
    if (opt.focus_building && building_center.x < 1e29f) {
        center = building_center;
        distance = 60.0f;
    }
    if (opt.focus_dinos && !imported_dinos.empty()) {
        const std::size_t focus_index = static_cast<std::size_t>(std::clamp(
            opt.focus_dino, 0, static_cast<int>(imported_dinos.size()) - 1));
        const ImportedDino& dinosaur = imported_dinos[focus_index];
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

    auto camera_radius = [&]() {
        return camera_config.distance_min +
               (camera_config.distance_max - camera_config.distance_min) *
                   camera_progress;
    };
    auto camera_height = [&]() {
        return camera_config.elevation_min +
               (camera_config.elevation_max - camera_config.elevation_min) *
                   camera_progress;
    };
    auto orbit_eye = [&]() {
        const float cy = std::cos(yaw * 3.14159265f / 180.0f);
        const float sy = std::sin(yaw * 3.14159265f / 180.0f);
        if (camera_retail_model) {
            const float radius = camera_radius();
            return dino::Vec3{center.x + radius * cy,
                              center.y + radius * sy,
                              center.z + camera_height()};
        }
        const float cp = std::cos(pitch * 3.14159265f / 180.0f);
        const float sp = std::sin(pitch * 3.14159265f / 180.0f);
        return dino::Vec3{center.x + distance * cp * cy,
                          center.y + distance * cp * sy,
                          center.z + distance * sp};
    };

    bool free_camera = opt.free_camera;
    dino::Vec3 free_pos = orbit_eye();
    float free_yaw = 0.0f;
    float free_pitch = 0.0f;
    auto enter_free_camera = [&]() {
        free_pos = orbit_eye();
        const dino::Vec3 dir = dino::normalize({center.x - free_pos.x,
                                                center.y - free_pos.y,
                                                center.z - free_pos.z});
        free_yaw = std::atan2(dir.y, dir.x) * 180.0f / 3.14159265f;
        free_pitch = std::asin(std::clamp(dir.z, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
    };
    if (free_camera) {
        enter_free_camera();
    }

    // Rebuilds the park from `Data\Design\Landscape\<island>\<island>.lnd`,
    // mirroring FUN_0051dad0/FUN_00403e10. The .lnd carries the basemap,
    // heightmap, skybox, water, music and vegetation list, so only the folder
    // name is selected here. Used by the Free Game start handoff.
    auto load_island = [&](const std::string& island, bool reset_view) {
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
        for (auto& mesh : veg_gpu) {
            mesh.destroy(glfns);
        }
        for (const GLuint id : veg_textures) {
            if (id != 0) {
                glDeleteTextures(1, &id);
            }
        }
        veg_gpu.clear();
        veg_textures.clear();
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
        }

        lnd = std::move(new_lnd);
        terrain = std::move(new_terrain);
        vegetation = std::move(new_vegetation);
        sky_images = std::move(new_sky);
        water_image = std::move(new_water);
        water2_image = std::move(new_water2);
        water_z = new_water_z;

        if (opt.texture_path.empty() && image.width > 0) {
            texture = upload_texture(image);
        }
        display = terrain.mesh;
        gpu = upload_mesh(display, glfns);

        veg_gpu.reserve(vegetation.groups.size());
        veg_textures.reserve(vegetation.groups.size());
        for (const auto& group : vegetation.groups) {
            veg_gpu.push_back(upload_mesh(group.mesh, glfns));
            GLuint group_texture = 0;
            if (!group.texture_path.empty() &&
                std::filesystem::exists(group.texture_path)) {
                try {
                    group_texture =
                        upload_texture(dino::load_tga(group.texture_path));
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "vegetation texture: %s\n", e.what());
                }
            }
            veg_textures.push_back(group_texture);
        }

        int loaded_faces = 0;
        for (int i = 0; i < 6; ++i) {
            if (sky_images[i].width <= 0) {
                continue;
            }
            skybox.faces[i] = upload_mesh(make_sky_face(i, 8000.0f), glfns);
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

        if (build_ready) {
            build_grid.init(opt.build_cell, terrain.world_size);
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

        // Free Game handoff: the engine's `FUN_0051dad0` spawns the access
        // road from the Entrance (category 10) at the island's .lnd
        // width/height. Place the same Entrance boat at that cell.
        if (opt.place_entrance) {
            clear_buildings();
            try {
                const dino::BuildingDef entrance = dino::load_bbi(
                    resolve_asset(opt.asset_root,
                                  "Data/Design/buildings/Entrance.bbi"));
                place_building(entrance, static_cast<float>(lnd.width),
                               static_cast<float>(lnd.height),
                               std::clamp(static_cast<int>(lnd.unknown_int), 0,
                                          3),
                               0.0f, 1.0f, 9);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "entrance: %s\n", e.what());
            }
        }
        rebuild_extra_items();

        bounds = Bounds{};
        for (const auto& p : display.positions) {
            bounds.add(p);
        }
        for (const auto& group : vegetation.groups) {
            for (const auto& p : group.mesh.positions) {
                bounds.add(p);
            }
        }

        place_dino = skinned;
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
                // Free Game start camera (camSwitcher/camera.ini): target is
                // the island centre at terrain height, horizontal distance 39
                // and elevation 52.7 at progress 0.1, yaw 0. Scaled for other
                // world sizes. The retail save stores eye (551,512,84.7),
                // target (512,512,32) on isl01.
                const float scale = terrain.world_size / 1024.0f;
                const float horizontal =
                    camera_config.distance(camera_config.progress) * scale;
                const float elevation =
                    camera_config.elevation(camera_config.progress) * scale;
                center = {0.0f, 0.0f, terrain.height_at(0.0f, 0.0f)};
                distance = std::sqrt(horizontal * horizontal +
                                     elevation * elevation);
                yaw = 0.0f;
                pitch =
                    std::asin(std::clamp(elevation / distance, -1.0f, 1.0f)) *
                    180.0f / 3.14159265f;
            }
            free_pos = orbit_eye();
            if (free_camera) {
                enter_free_camera();
            }
        }

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
    };

    int park_hover_x = -1;
    int park_hover_y = -1;
    int park_active_index = -1;
    int park_budget = free_game_budgets[0];
    auto advance_campaign = [&]() {
        if (!pending_next_mission) {
            if (!pending_ending_video.empty()) {
                const std::string video = pending_ending_video;
                pending_ending_video.clear();
                std::printf("campaign video: %s\n", video.c_str());
                play_video(video, 0, opt.video_play_frames);
            }
            // The campaign is over: the mission panel would otherwise stay
            // open with stale objective text behind the dismissed CPGEND.
            park.clear_mission();
            return;
        }
        const dino::CampaignMission next = *pending_next_mission;
        pending_next_mission.reset();
        const auto delta = next.values.find("CONTEXT.BUDGET");
        if (delta != next.values.end()) {
            park_budget += std::atoi(delta->second.c_str());
        }
        park.close_dialog();
        std::printf("campaign: next mission %s budget=%d\n",
                    next.name.c_str(), park_budget);
        start_campaign_mission(next);
    };
    if (sav_ready) {
        park_budget = sav_simulation.budget;
        park_clock.day = sav_simulation.day;
        park_clock.elapsed = sav_simulation.elapsed;
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
                center = {cam[3], cam[4], cam[5]};
                distance = length;
                if (horizontal > 1e-4f) {
                    yaw = std::atan2(dy, dx) * 180.0f / 3.14159265f;
                }
                pitch = std::asin(std::clamp(dz / length, -1.0f, 1.0f)) *
                        180.0f / 3.14159265f;
                // The tail's progress is the retail orbit parameter; using it
                // preserves the saved eye exactly (radius/height are derived).
                camera_progress = std::clamp(cam[6], 0.0f, 1.0f);
                camera_yaw_goal = yaw;
                camera_retail_model = true;
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
            budget_ledger.y_food_total = sane(economy.yesterday_food[0]);
            budget_ledger.y_food_count = sane(economy.yesterday_food[1]);
            budget_ledger.y_staff_food_total =
                sane(economy.yesterday_staff[0]);
            budget_ledger.y_secu_total = sane(economy.yesterday_secu[0]);
            budget_ledger.y_secu_count = sane(economy.yesterday_secu[1]);
            budget_ledger.y_dinoachat_count =
                sane(economy.yesterday_totals[5]);
            budget_ledger.y_dinoachat_total =
                sane(economy.yesterday_totals[6]);
            budget_ledger.y_construction_count =
                sane(economy.yesterday_totals[7]);
            budget_ledger.y_construction_total =
                sane(economy.yesterday_totals[8]);
            budget_ledger.y_laboaction_count =
                sane(economy.yesterday_totals[9]);
            budget_ledger.y_laboaction_total =
                sane(economy.yesterday_totals[10]);
            budget_ledger.y_ticket_total =
                sane(economy.yesterday_totals[0]);
            budget_ledger.y_ticket_count =
                sane(economy.yesterday_totals[1]);
            budget_ledger.y_ventes_total =
                sane(economy.yesterday_totals[12]);
            budget_ledger.y_ventes_count =
                sane(economy.yesterday_totals[11]);
            for (std::size_t i = 1;
                 i < budget_ledger.y_shop_revenue.size(); ++i) {
                budget_ledger.y_shop_revenue[i] =
                    sane(economy.yesterday_revenue[i]);
                budget_ledger.y_shop_sales[i] =
                    sane(economy.yesterday_sales[i]);
            }
        }
        if (visitor_profiles.empty()) {
            try {
                visitor_profiles = load_easy_visitor_profiles(opt.asset_root);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "visitor profiles: %s\n", e.what());
            }
        }
        if (!sav_simulation.visitors.empty()) {
            visitor_system.visitors.clear();
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
                                  visitor_system.visitors.size() + 1);
                visitor_system.visitors.push_back(visitor);
            }
            std::printf("sav visitors: %zu restored\n",
                        visitor_system.visitors.size());
        }
        std::printf("sav park budget=%d day=%d elapsed=%.3f roads=%zu\n",
                    park_budget, park_clock.day, park_clock.elapsed,
                    wld_roads.size());
    }
    if (opt.park_budget_override >= 0) {
        park_budget = opt.park_budget_override;
    }
    if (pending_buy_price > 0) {
        park_budget -= pending_buy_price;
    }

    // Employees (first slice). Retail hires only Nutritionists (0x5366c0,
    // employee index 0) and charges the `.bei` dword 8 as a generic purchase
    // (0x588600 -> 0x585c00). The same value is the daily wage at settlement
    // (0x585d80, role bucket 0). Guards are a wage bucket only.
    dino::EmployeeSystem employee_system;
    if (opt.hire_nutritionists > 0 && !employee_profiles.empty()) {
        int nutritionist = -1;
        for (std::size_t i = 0; i < employee_profiles.size(); ++i) {
            if (employee_profiles[i].kind == 0) {
                nutritionist = static_cast<int>(i);
                break;
            }
        }
        if (nutritionist < 0) {
            std::printf("employees: no nutritionist profile\n");
        } else {
            const int wage =
                employee_profiles[static_cast<std::size_t>(nutritionist)].wage;
            const float half = terrain.world_size * 0.5f;
            float spawn_x = half;
            float spawn_y = half;
            if (entrance_cell_x >= 0 && entrance_cell_y >= 0) {
                spawn_x = (static_cast<float>(entrance_cell_x) + 0.5f) *
                          build_grid.cell_size;
                spawn_y = (static_cast<float>(entrance_cell_y) + 0.5f) *
                          build_grid.cell_size;
            }
            int hired = 0;
            for (int n = 0; n < opt.hire_nutritionists; ++n) {
                if (park_budget < wage) {
                    std::printf("hire: not enough money wage=%d budget=%d\n",
                                wage, park_budget);
                    break;
                }
                park_budget -= wage;
                employee_system.hire(employee_profiles, nutritionist, spawn_x,
                                     spawn_y);
                ++hired;
            }
            std::printf("employees: hired=%d wage=%d budget=%d\n", hired, wage,
                        park_budget);
        }
    }
    if (opt.fire_nutritionists > 0) {
        const std::size_t before = employee_system.employees.size();
        const std::size_t fired = std::min<std::size_t>(
            before, static_cast<std::size_t>(opt.fire_nutritionists));
        employee_system.employees.resize(before - fired);
        std::printf("employees: fired=%zu active=%zu\n", fired,
                    employee_system.employees.size());
    }
    if (opt.feed_now) {
        const int assigned = employee_system.start_hour(
            employee_profiles, food_sources, food_defs, true);
        int completed = 0;
        for (int step = 0; step < 20; ++step) {
            completed += employee_system.update(
                10.0f, employee_profiles, food_sources, food_defs);
        }
        std::printf(
            "feed: assigned=%d completed=%d serviced=%d sources=%zu\n",
            assigned, completed, employee_system.serviced, food_sources.size());
        for (const dino::FoodSource& source : food_sources) {
            const float capacity =
                source.def >= 0 &&
                        source.def < static_cast<int>(food_defs.size())
                    ? food_defs[static_cast<std::size_t>(source.def)].quantity
                    : 0.0f;
            std::printf("feed: source def=%d remaining=%.3f capacity=%.3f\n",
                        source.def, static_cast<double>(source.remaining),
                        static_cast<double>(capacity));
        }
    }

    // Load the Nutritionist standing pose + texture once; the hourly walk is
    // a straight move, so one pose is enough for the first slice.
    if (!employee_profiles.empty() && employee_pose.positions.empty()) {
        int nutritionist = -1;
        for (std::size_t i = 0; i < employee_profiles.size(); ++i) {
            if (employee_profiles[i].kind == 0) {
                nutritionist = static_cast<int>(i);
                break;
            }
        }
        if (nutritionist >= 0) {
            const dino::EmployeeProfile& profile =
                employee_profiles[static_cast<std::size_t>(nutritionist)];
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
        !employee_system.employees.empty()) {
        if (employee_image.width > 0) {
            employee_texture_id = upload_texture(employee_image);
        }
        employee_gpu = upload_mesh(
            employee_system.build_mesh(terrain, employee_pose), glfns);
        std::printf("employees: rendered=%zu pose_vertices=%zu\n",
                    employee_system.employees.size(),
                    employee_pose.positions.size());
        rebuild_extra_items();
    }

    // P4: construction placement through the retail panel. Each building's
    // `.bbi` tail lists the `CONSTRUC_*_BUTTON` widget that builds it; the
    // charge is `.bbi` file offset 21 (`D+0x18`), proven by
    // `pvwStateBuildingConstruction` (0x5306e1 / 0x5307ac).
    std::vector<std::string> construction_names;
    std::vector<dino::BuildingDef> construction_defs;
    auto ensure_construction_catalog = [&]() {
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
    };
    auto construction_kind_for_widget = [&](const std::string& widget) {
        ensure_construction_catalog();
        for (std::size_t i = 0; i < construction_defs.size(); ++i) {
            for (const std::string& ui : construction_defs[i].tail_strings) {
                if (ui == widget) {
                    return static_cast<int>(i);
                }
            }
        }
        return -1;
    };
    auto construction_kind_for_name = [&](const std::string& name) {
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
    };
    auto construction_overlaps = [&](int kind, int cell_x, int cell_y,
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
    };
    auto try_construct = [&](int kind, int cell_x, int cell_y,
                             int state) -> bool {
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
        // Retail gates the budget first (0x5306e1 -> FUN_0052bed0), then the
        // placement/ground checks.
        if (park_budget < static_cast<int>(def.construction_cost)) {
            std::printf(
                "construction: %s needs %u, budget %d (WARNING_NOT_ENOUGH_MONEY)\n",
                def.name.c_str(), def.construction_cost, park_budget);
            return false;
        }
        const float half = terrain.world_size * 0.5f;
        for (int y = cell_y; y < cell_y + h; ++y) {
            for (int x = cell_x; x < cell_x + w; ++x) {
                if (!terrain.buildable(static_cast<float>(x) - half + 0.5f,
                                       static_cast<float>(y) - half + 0.5f)) {
                    std::printf(
                        "construction: %s on blocked ground\n",
                        def.name.c_str());
                    return false;
                }
            }
        }
        if (!place_building(def, static_cast<float>(cell_x),
                            static_cast<float>(cell_y), state, 0.0f, 1.0f,
                            kind)) {
            return false;
        }
        park_budget -= static_cast<int>(def.construction_cost);
        budget_ledger.spend_construction(
            static_cast<int>(def.construction_cost));
        rebuild_extra_items();
        std::printf("construction: %s cost=%u budget=%d cell=%d,%d\n",
                    def.name.c_str(), def.construction_cost, park_budget,
                    cell_x, cell_y);
        return true;
    };

    // Retail road tool (`ROAD_CONSTRUCTION`, notes/road-construction-recon.md):
    // CONSTRUC_ROAD_BUTTON arms it (build tool 4); a left click places the
    // start, the next commits a straight run, charges CONSTRUCTION and chains
    // from its end; right click cancels the run, a second one disarms.
    std::uint32_t road_price = 0;
    bool road_price_loaded = false;
    auto ensure_road_price = [&]() {
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
    };
    bool road_has_start = false;
    int road_start_x = 0;
    int road_start_y = 0;
    std::array<int, 4> road_preview_key = {-1, -1, -1, -1};
    int road_preview_cost = -1;
    auto road_message = [&](const char* id) {
        if (park.loaded()) {
            park.show_message(dino::ui::resolve_loc_text(id, park.loc));
        }
    };
    // A cell is busy when the ground is blocked (occupation mask) or a
    // building footprint covers it; roads themselves never block a road.
    auto road_cell_busy = [&](int x, int y) {
        const float cs = build_grid.cell_size;
        if (!terrain.buildable(build_grid.origin_x + (x + 0.5f) * cs,
                               build_grid.origin_y + (y + 0.5f) * cs)) {
            return true;
        }
        ensure_construction_catalog();
        const int wx = static_cast<int>(std::lround(
            build_grid.origin_x + x * cs + terrain.world_size * 0.5f));
        const int wy = static_cast<int>(std::lround(
            build_grid.origin_y + y * cs + terrain.world_size * 0.5f));
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
    };
    auto road_perimeters = [&]() {
        std::vector<dino::WorldRect> rects;
        rects.reserve(paddocks.size());
        for (const dino::Paddock& p : paddocks) {
            rects.push_back(p.rect);
        }
        return rects;
    };
    auto arm_road_tool = [&]() {
        build_tool = 4;
        pending_construct_kind = -1;
        road_has_start = false;
        std::printf("road: tool armed, price=%u per cell\n", ensure_road_price());
    };
    auto road_click = [&](int cx, int cy) {
        const std::vector<dino::WorldRect> perimeters = road_perimeters();
        if (!road_has_start) {
            const dino::RoadCheck check = dino::check_road_start(
                build_grid, cx, cy, perimeters, road_cell_busy);
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
            dino::check_road(build_grid, road, perimeters, road_cell_busy);
        if (check != dino::RoadCheck::Ok) {
            std::printf("road: %d,%d -> %d,%d rejected (%s)\n", road.x0, road.y0,
                        road.x1, road.y1, dino::road_warning(check) + 1);
            road_message(dino::road_warning(check));
            return;
        }
        const int cost = dino::road_cost(road, ensure_road_price());
        if (park_budget < cost) {
            std::printf("road: needs %d, budget %d (WARNING_NOT_ENOUGH_MONEY)\n",
                        cost, park_budget);
            road_message("&WARNING_NOT_ENOUGH_MONEY");
            return;
        }
        const int added = dino::place_road(build_grid, road);
        park_budget -= cost;
        budget_ledger.spend_construction(cost);
        rebuild_build_meshes();
        std::printf(
            "road: built %d,%d -> %d,%d cells=%d new=%d cost=%d budget=%d "
            "construction=%d/%d\n",
            road.x0, road.y0, road.x1, road.y1, road.cells(), added, cost,
            park_budget, budget_ledger.construction_count,
            budget_ledger.construction_total);
        road_start_x = road.x1;  // FUN_005f8830: chain from the end point
        road_start_y = road.y1;
    };

    const std::string selected_construct =
        !opt.construct_name.empty() ? opt.construct_name
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

    // Start a store purchase (0x00535600): the budget gate stays in the panel
    // on failure; on success the panel closes and the next ground click is
    // the FENCE_INSERTION target.
    auto handle_store_buy = [&](int index) {
        if (index < 0 || index >= static_cast<int>(store_catalog.size())) {
            return;
        }
        const dino::MarketEntry& entry =
            store_catalog[static_cast<std::size_t>(index)];
        if (park_budget < entry.price) {
            std::printf(
                "store: %s price=%d budget=%d "
                "(WARNING_NOT_ENOUGH_MONEY)\n",
                entry.name.c_str(), entry.price, park_budget);
            park.show_message(dino::ui::resolve_loc_text(
                "&WARNING_NOT_ENOUGH_MONEY", park.loc));
            return;
        }
        pending_store_index = index;
        pending_store_price = entry.price;
        park.overlay.set_visible("DINO_ACHAT_SAVE_PANEL", false);
        std::printf("store: %s price=%d selected, click a paddock\n",
                    entry.name.c_str(), entry.price);
    };
    if (pending_store_action.rfind("store buy ", 0) == 0) {
        handle_store_buy(std::atoi(pending_store_action.c_str() + 10));
        pending_store_action.clear();
    }

    // Daily settlement (0x0058ad00): wages, security, visitor tickets, then
    // the counter roll. The port has no employee/security actors yet, so those
    // terms are zero. Ticket value (0x58ad95): round(entrance_price *
    // visitor_profile+0x30), where the Entrance `.bbi` visitor price is 50
    // and the profile scale is `.bvi` offset 0x34 (1.0..2.5).
    auto run_daily_settlement = [&]() {
        ensure_construction_catalog();
        int ticket_price = 50;
        for (const dino::BuildingDef& def : construction_defs) {
            if (def.category == 10) {
                ticket_price = static_cast<int>(def.price);
                break;
            }
        }
        const int visitors = static_cast<int>(visitor_system.visitors.size());
        if (all_visitor_profiles.empty()) {
            try {
                all_visitor_profiles =
                    load_all_visitor_profiles(opt.asset_root);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "all visitor profiles: %s\n", e.what());
            }
        }
        const std::vector<dino::VisitorProfile>& ticket_profiles =
            all_visitor_profiles.empty() ? visitor_profiles
                                         : all_visitor_profiles;
        int tickets = 0;
        for (const dino::Visitor& visitor : visitor_system.visitors) {
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
        const int wages = employee_system.daily_wages(employee_profiles);
        for (const dino::Employee& employee : employee_system.employees) {
            if (employee.profile < 0 ||
                employee.profile >= static_cast<int>(employee_profiles.size())) {
                continue;
            }
            const dino::EmployeeProfile& profile =
                employee_profiles[static_cast<std::size_t>(employee.profile)];
            budget_ledger.spend_staff(profile.kind, profile.wage);
        }
        park_budget += tickets - wages;
        budget_ledger.add_ticket(tickets, visitors);
        budget_ledger.settle();
        std::printf(
            "daily settlement: day=%d visitors=%d ticket=%d income=%d "
            "wages=%d budget=%d\n",
            park_clock.day, visitors, ticket_price, tickets, wages,
            park_budget);
    };
    if (opt.settle_day) {
        run_daily_settlement();
    }
    if (opt.budget_info) {
        std::printf(
            "budget ledger: dinoachat=%d/%d construction=%d/%d tickets=%d/%d "
            "food=%d/%d secu=%d/%d labo=%d/%d ventes=%d/%d\n",
            budget_ledger.dinoachat_count, budget_ledger.dinoachat_total,
            budget_ledger.construction_count,
            budget_ledger.construction_total, budget_ledger.ticket_count,
            budget_ledger.ticket_total, budget_ledger.food_count,
            budget_ledger.food_total, budget_ledger.secu_count,
            budget_ledger.secu_total, budget_ledger.laboaction_count,
            budget_ledger.laboaction_total, budget_ledger.ventes_count,
            budget_ledger.ventes_total);
        std::printf(
            "budget yesterday: dinoachat=%d/%d construction=%d/%d "
            "tickets=%d/%d\n",
            budget_ledger.y_dinoachat_count, budget_ledger.y_dinoachat_total,
            budget_ledger.y_construction_count,
            budget_ledger.y_construction_total, budget_ledger.y_ticket_count,
            budget_ledger.y_ticket_total);
        std::printf(
            "budget totals: expenses=%d income=%d net=%d\n",
            budget_ledger.expenses(), budget_ledger.income(),
            budget_ledger.net());
    }

    int park_layout_w = 800;
    int park_layout_h = 600;
    dino::Vec3 minimap_focus{1e30f, 1e30f, 1e30f};
    float minimap_heading = 1e30f;
    float minimap_rendered_scale = -1.0f;

    std::string current_island;
    if (!opt.lnd_path.empty()) {
        current_island =
            std::filesystem::path(opt.lnd_path).stem().string();
    } else if (!island_names.empty()) {
        current_island = island_names.front();
    }

    auto ends_with = [](const std::string& text, const std::string& suffix) {
        return text.size() >= suffix.size() &&
               text.compare(text.size() - suffix.size(), suffix.size(),
                            suffix) == 0;
    };

    auto make_port_state = [&]() {
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
    };

    auto ensure_build_ready = [&]() {
        if (terrain.world_size <= 0.0f) {
            return;
        }
        if (!build_ready) {
            build_grid.init(opt.build_cell > 0.0f ? opt.build_cell : 4.0f,
                            terrain.world_size);
            try {
                const std::string fence_mesh =
                    opt.fence_mesh_path.empty()
                        ? resolve_asset(opt.asset_root,
                                        "Data/Graphics/paddock/fences/"
                                        "fence_paddock_lod1.mesh")
                        : opt.fence_mesh_path;
                fence_source = dino::load_mesh(fence_mesh);
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
        }
        if (visitor_profiles.empty()) {
            try {
                visitor_profiles = load_easy_visitor_profiles(opt.asset_root);
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
    };

    // Show construction VALIDATE (`0x0052d1e0`): the selected catalog row
    // arms the placement tool; the next ground click places the arena.
    auto handle_show_place = [&](int catalog_index) {
        if (catalog_index < 0 ||
            catalog_index >= static_cast<int>(show_catalog.size())) {
            std::printf("show: no show selected\n");
            return;
        }
        const dino::ShowCatalogEntry& entry =
            show_catalog[static_cast<std::size_t>(catalog_index)];
        if (park_budget < entry.cost) {
            std::printf(
                "show: %s needs %d, budget %d (WARNING_NOT_ENOUGH_MONEY)\n",
                entry.stem.c_str(), entry.cost, park_budget);
            park.show_message(dino::ui::resolve_loc_text(
                "&WARNING_NOT_ENOUGH_MONEY", park.loc));
            return;
        }
        ensure_build_ready();
        pending_show_index = catalog_index;
        build_tool = 5;
        park.overlay.set_visible("PREDEFINED_SHOW_PANEL", false);
        std::printf(
            "show: %s selected, cost=%d size=%dx%d (click the ground)\n",
            entry.stem.c_str(), entry.cost, entry.width, entry.height);
    };
    if (pending_show_action.rfind("show place ", 0) == 0) {
        handle_show_place(std::atoi(pending_show_action.c_str() + 11));
        pending_show_action.clear();
    }

    // Ground placement for the armed show, shared by the world click and the
    // scripted `--show-place-pos` hook. `x`/`y` are the show rectangle's
    // min-corner record coordinates (0..1024, the same space as `--place-show`
    // and building records). The `.shw` dimensions are world units: the
    // 71x35 derby is 71x35 units, not 284x140, or it could not fit in the
    // Cpg04 park (its fences span only 255x239 world units). `FUN_0052d1e0`
    // adds a `2*[show+0x6c]+1` border; the port checks the arena rectangle
    // itself and charges the budget on success only.
    auto place_show_at = [&](int rx, int ry) {
        if (pending_show_index < 0 ||
            pending_show_index >= static_cast<int>(show_catalog.size())) {
            return;
        }
        const dino::ShowCatalogEntry& entry =
            show_catalog[static_cast<std::size_t>(pending_show_index)];
        const int w = std::max(1, entry.width);
        const int h = std::max(1, entry.height);
        const float cs = build_grid.cell_size;
        // Record coordinate -> build cell: `cell = floor(record / cell_size)`
        // (docs/platform/world-import.md; the grid origin is -world_size/2).
        const int gx0 =
            static_cast<int>(std::floor(static_cast<float>(rx) / cs));
        const int gy0 =
            static_cast<int>(std::floor(static_cast<float>(ry) / cs));
        const int gx1 = static_cast<int>(
            std::floor(static_cast<float>(rx + w - 1) / cs));
        const int gy1 = static_cast<int>(
            std::floor(static_cast<float>(ry + h - 1) / cs));
        auto cell_buildable = [&](int x, int y) {
            return terrain.buildable(
                build_grid.origin_x + (static_cast<float>(x) + 0.5f) * cs,
                build_grid.origin_y + (static_cast<float>(y) + 0.5f) * cs);
        };
        bool blocked = false;
        for (int y = gy0; y <= gy1 && !blocked; ++y) {
            for (int x = gx0; x <= gx1 && !blocked; ++x) {
                if (x < 0 || y < 0 || x >= build_grid.width ||
                    y >= build_grid.height || !cell_buildable(x, y)) {
                    blocked = true;
                }
            }
        }
        if (blocked) {
            std::printf("show: %s footprint blocked\n", entry.stem.c_str());
            return;
        }
        if (park_budget < entry.cost) {
            std::printf(
                "show: %s needs %d, budget %d (WARNING_NOT_ENOUGH_MONEY)\n",
                entry.stem.c_str(), entry.cost, park_budget);
            return;
        }
        const std::vector<int> profile_ids = show_profile_ids();
        const int index = show_runtime.place(entry, rx, ry, profile_ids);
        if (index < 0) {
            std::printf("show: no free hour slot for %s\n",
                        entry.stem.c_str());
            return;
        }
        park_budget -= entry.cost;
        budget_ledger.spend_construction(entry.cost);
        auto_assign_show_participants(index);
        const dino::ParkShow* show = show_runtime.at(index);
        std::printf(
            "show: placed %s at %d,%d slot=%d (%s) size=%dx%d budget=%d\n",
            show->entry.stem.c_str(), show->cell_x, show->cell_y, show->slot,
            show_slot_text(show->slot).c_str(), show->entry.width,
            show->entry.height, park_budget);
        build_tool = 0;
        pending_show_index = -1;
    };
    if (pending_show_index >= 0 && !opt.show_place_positions.empty()) {
        for (const std::string& position : opt.show_place_positions) {
            int x = 0;
            int y = 0;
            if (std::sscanf(position.c_str(), "%d,%d", &x, &y) == 2) {
                place_show_at(x, y);
            }
        }
    }

    // Attraction list actions. Rows are hour slots 1..10; the port's
    // ShowRuntime owns the placed shows. `LOCK`/`UNLOCK` drive the retail
    // "closed to the public" flag `show+0xd3` (FUN_0057b0f0); the iteration
    // buttons edit `show+0x8c` (no recovered writer, so clamp 1..10);
    // PERMUTATION swaps two hour slots.
    auto find_show_by_slot = [&](int slot) -> int {
        for (int i = 0; i < show_runtime.count(); ++i) {
            const dino::ParkShow* show = show_runtime.at(i);
            if (show != nullptr && show->slot == slot) {
                return i;
            }
        }
        return -1;
    };
    auto handle_attraction_action = [&](const std::string& action) {
        const std::size_t last_space = action.find_last_of(' ');
        if (last_space == std::string::npos) {
            return;
        }
        const int slot = std::atoi(action.c_str() + last_space + 1);
        const std::string verb =
            action.substr(std::strlen("attraction "),
                          last_space - std::strlen("attraction "));
        const int index = find_show_by_slot(slot);
        if (verb == "select" || verb == "selection_light") {
            if (index >= 0) {
                const dino::ParkShow* show = show_runtime.at(index);
                attraction_selected_slot = slot;
                std::printf("show: selected %s slot=%d\n",
                            show->entry.stem.c_str(), slot);
            }
            return;
        }
        if (index < 0) {
            std::printf("show: no show in slot %d\n", slot);
            return;
        }
        dino::ParkShow* show = show_runtime.at(index);
        if (verb == "lock") {
            show_runtime.set_locked(index, true);
            show_runtime.set_enabled(index, false);
            std::printf("show: %s slot=%d closed to the public\n",
                        show->entry.stem.c_str(), slot);
        } else if (verb == "unlock") {
            show_runtime.set_locked(index, false);
            show_runtime.set_enabled(index, true);
            std::printf("show: %s slot=%d open to the public\n",
                        show->entry.stem.c_str(), slot);
        } else if (verb == "iteration_up" || verb == "iteration_down") {
            const int delta = verb == "iteration_up" ? 1 : -1;
            const int value = std::clamp(show->iterations + delta, 1, 10);
            if (value != show->iterations) {
                show_runtime.set_iterations(index, value);
                std::printf("show: %s iterations=%d\n",
                            show->entry.stem.c_str(), value);
            }
        } else if (verb == "permutation") {
            const int other_slot = slot % 10 + 1;
            const int other = find_show_by_slot(other_slot);
            show_runtime.set_slot(index, other_slot);
            if (other >= 0) {
                show_runtime.set_slot(other, slot);
            }
            std::printf("show: %s slot=%d (%s) permuted with slot %d\n",
                        show->entry.stem.c_str(), other_slot,
                        show_slot_text(other_slot).c_str(), slot);
        }
    };
    for (const std::string& action : pending_attraction_actions) {
        handle_attraction_action(action);
    }
    pending_attraction_actions.clear();

    // Menu campaign start: the retail flow loads the campaign's island and
    // `CpgNN_Save.wld`, then opens the mission briefing. Reuse the same
    // building/road/dinosaur import path as `--wld-park`.
    auto load_campaign_park = [&](const std::string& world_path) {
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
    };

    auto apply_port_state = [&](const dino::PortState& state) {
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

        build_grid.init(state.cell_size > 0.0f ? state.cell_size : 4.0f,
                        terrain.world_size);
        for (const auto& cell : state.paths) {
            build_grid.set_path(cell.first, cell.second, true);
        }
        for (const dino::PortFenceState& fence : state.fences) {
            build_grid.set_fence(fence.x, fence.y, fence.edge, true);
        }
        build_grid.undo_stack.clear();

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
                paddocks.push_back(std::move(paddock));
            }
            park_paddocks = paddocks;
        }
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
    };

    if (sav_ready) {
        dino::PortState state;
        state.island = current_island;
        state.budget = sav_simulation.budget;
        state.day = sav_simulation.day;
        state.elapsed = sav_simulation.elapsed;
        state.time_scale = park_clock.time_scale;
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
             i < visitor_system.visitors.size();
             ++i) {
            const auto& needs = sav_simulation.visitors[i].needs;
            visitor_system.visitors[i].need0 = static_cast<int>(needs[0]);
            visitor_system.visitors[i].need1 = static_cast<int>(needs[1]);
            visitor_system.visitors[i].need2 = static_cast<int>(needs[2]);
        }
        std::printf(
            "sav import: budget=%d day=%d elapsed=%.3f visitors=%zu "
            "paths=%d fences=%d buildings=%zu\n",
            park_budget, park_clock.day, park_clock.elapsed,
            visitor_system.visitors.size(), build_grid.path_count,
            build_grid.fence_count, building_instances.size());
    }

    auto save_name_sanitized = [&]() {
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
    };

    auto port_save_target = [&](bool for_load) {
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
    };

    auto do_port_save = [&](const std::string& path) {
        try {
            dino::save_port_state(make_port_state(), path);
            std::printf("port save: %s\n", path.c_str());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "port save: %s\n", e.what());
        }
    };
    auto do_port_load = [&](const std::string& path) {
        try {
            apply_port_state(dino::load_port_state(path));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "port load: %s\n", e.what());
        }
    };

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

    auto draw_park_hud = [&](int view_w, int view_h, int logical_w, int logical_h) {
        (void)logical_h;
        if (!park.loaded()) {
            return;
        }
        const int hovered =
            park_hover_x >= 0 ? park.overlay.hit_test(park_hover_x, park_hover_y) : -1;
        auto dialog_zone = [&]() -> const char* {
            switch (park.dialog_kind()) {
                case dino::ui::ParkHud::DialogKind::warning:
                    return "WARNING_TEXT_ZONE";
                case dino::ui::ParkHud::DialogKind::request:
                    return "REQUEST_TEXT_ZONE";
                case dino::ui::ParkHud::DialogKind::campaign_end:
                    return "CPGEND_TEXT_ZONE";
                default:
                    return "";
            }
        };
        auto is_text_zone = [](const std::string& name) {
            static const std::string suffix = "_TEXT_ZONE";
            return name.size() >= suffix.size() &&
                   name.compare(name.size() - suffix.size(), suffix.size(),
                                suffix) == 0;
        };
        const float dpi = logical_w > 0
                              ? static_cast<float>(view_w) / static_cast<float>(logical_w)
                              : 1.0f;
        auto px = [&](int value) { return static_cast<float>(value) * dpi; };
        auto py = [&](int value) { return static_cast<float>(value) * dpi; };

        std::vector<int> order;
        auto draw_less = [&](int a, int b) {            const auto& wa = park.overlay.widgets[static_cast<std::size_t>(a)];
            const auto& wb = park.overlay.widgets[static_cast<std::size_t>(b)];
            if (wa.priority != wb.priority) {
                return wa.priority < wb.priority;
            }
            if (wa.zpriority != wb.zpriority) {
                // Lower ZPRIORITY is nearer the viewer in the retail overlay;
                // draw the far widgets first and the near ones last.
                return wa.zpriority > wb.zpriority;
            }
            return a < b;
        };
        // moWidget::Draw (0x005c5bf0) draws the widget, then its child list in
        // priority order. A flat ZPRIORITY sort loses that nesting: a list
        // slot's snapshot would cover its indicator strip. Walk the tree.
        std::function<void(int)> append_widget = [&](int i) {
            const dino::ui::Widget& widget =
                park.overlay.widgets[static_cast<std::size_t>(i)];
            if (widget.name == "RENDERVIEW" ||
                !park.overlay.visible_in_tree(i)) {
                return;
            }
            order.push_back(i);
            std::vector<int> children = widget.children;
            std::sort(children.begin(), children.end(), draw_less);
            for (int child : children) {
                append_widget(child);
            }
        };
        std::vector<int> roots = park.overlay.roots;
        std::sort(roots.begin(), roots.end(), draw_less);
        for (int root : roots) {
            append_widget(root);
        }
        // Dialog text must draw above its *_TEXT_BACK_ZONE and the wood frame.
        std::stable_partition(order.begin(), order.end(), [&](int i) {
            return !is_text_zone(
                park.overlay.widgets[static_cast<std::size_t>(i)].name);
        });

        auto runtime_text = [&](const std::string& name) -> std::string {
            if (name == "MISSION_PANEL_TITLE" &&
                !mission_panel_title.empty()) {
                return mission_panel_title;
            }
            if (campaign_runtime) {
                const dino::CampaignMission& mission =
                    campaign_runtime->mission();
                static const std::string suffix = "_PANEL_EXPLICATION";
                if (name.rfind("OBJECTIF", 0) == 0 &&
                    name.size() > suffix.size() &&
                    name.compare(name.size() - suffix.size(), suffix.size(),
                                 suffix) == 0) {
                    const int index = std::atoi(name.c_str() + 8) - 1;
                    if (index >= 0 &&
                        index < static_cast<int>(mission.objectives.size())) {
                        return dino::ui::resolve_loc_text(
                            mission.objectives[static_cast<std::size_t>(index)]
                                .description_id,
                            park.loc);
                    }
                }
            }
            if (name == "BUDGET_AMOUNT") {
                return std::to_string(park_budget);
            }
            if (name == "OPTIONS_NAME_SLOT") {
                return park.save_name();
            }
            if (name == "VISITOR_AMOUNT") {
                return std::to_string(visitor_system.visitors.size());
            }
            if (name == "DINO_AMOUNT") {
                const std::size_t count =
                    imported_dinos.size() + (place_dino ? 1 : 0);
                return std::to_string(count);
            }
            if (name == "SHOW_AMOUNT") {
                return std::to_string(show_runtime.count());
            }
            if (name == "PADDOCK_AMOUNT") {
                return std::to_string(park_paddocks.size());
            }
            const auto slot_index = [](const std::string& widget_name,
                                       const char* prefix) -> int {
                const std::size_t prefix_len = std::strlen(prefix);
                if (widget_name.rfind(prefix, 0) != 0) {
                    return -1;
                }
                return std::atoi(widget_name.c_str() + prefix_len) - 1;
            };
            if (name.rfind("DINO_NAME_SLOT", 0) == 0) {
                const int slot = slot_index(name, "DINO_NAME_SLOT");
                if (slot >= 0 && slot < static_cast<int>(imported_dinos.size())) {
                    return imported_dinos[static_cast<std::size_t>(slot)].species;
                }
                return "";
            }
            if (name.rfind("FENCE_NAME_SLOT", 0) == 0) {
                const int slot = slot_index(name, "FENCE_NAME_SLOT");
                if (slot >= 0 && slot < static_cast<int>(park_paddocks.size())) {
                    return "Paddock " + std::to_string(slot + 1);
                }
                return "";
            }
            if (name.rfind("FENCE_LIST_QUANTITY_SLOT", 0) == 0) {
                const int slot = slot_index(name, "FENCE_LIST_QUANTITY_SLOT");
                if (slot >= 0 && slot < static_cast<int>(park_paddocks.size())) {
                    return std::to_string(
                        park_paddocks[static_cast<std::size_t>(slot)]
                            .occupants.size());
                }
                return "";
            }
            if (name.rfind("FENCE_LIST_POURCENT_SLOT", 0) == 0) {
                const int slot = slot_index(name, "FENCE_LIST_POURCENT_SLOT");
                if (slot >= 0 && slot < static_cast<int>(park_paddocks.size())) {
                    const float percent =
                        park_paddocks[static_cast<std::size_t>(slot)].fill() *
                        100.0f;
                    return std::to_string(
                        static_cast<int>(percent + 0.5f));
                }
                return "";
            }
            // Attraction list: one row per hour slot. The retail NAME field
            // defaults to the hour label and holds the show name once a show
            // is scheduled (`FUN_00516dd0`); ITERATION_INDIC is the
            // performances-per-day count `show+0x8c` (`%u`).
            if (name.rfind("ATTRACTION_LIST_NAME", 0) == 0) {
                const int slot = slot_index(name, "ATTRACTION_LIST_NAME") + 1;
                std::string text;
                int iterations = 0;
                int locked = 0;
                attraction_row_text(slot, text, iterations, locked);
                return text;
            }
            if (name.rfind("ATTRACTION_LIST_ITERATION_INDIC", 0) == 0) {
                const int slot =
                    slot_index(name, "ATTRACTION_LIST_ITERATION_INDIC") + 1;
                std::string text;
                int iterations = 0;
                int locked = 0;
                attraction_row_text(slot, text, iterations, locked);
                return iterations > 0 ? std::to_string(iterations) : "";
            }
            if (name == "PARK_CURRENT_TIME") {
                return park_clock.time_text(false);
            }
            if (name == "PARK_CURRENT_DAY") {
                return park_clock.day_text();
            }
            // Budget-panel fields are separate number widgets refreshed by
            // 0x510430. The panel shows the yesterday block (docs/decomp/
            // economy.md); only the categories the port currently changes
            // are non-zero.
            if (name.rfind("BUDGET_", 0) == 0 &&
                name.size() > 7 &&
                name.compare(name.size() - 7, 7, "_NUMBER") == 0) {
                if (name == "BUDGET_DINOACHAT_QUANTITY_NUMBER") {
                    return std::to_string(budget_ledger.y_dinoachat_count);
                }
                if (name == "BUDGET_DINOACHAT_QUANTITY_COST_NUMBER") {
                    return std::to_string(budget_ledger.y_dinoachat_total);
                }
                if (name == "BUDGET_CONSTRUCTION_QUANTITY_NUMBER") {
                    return std::to_string(budget_ledger.y_construction_count);
                }
                if (name == "BUDGET_CONSTRUCTION_QUANTITY_COST_NUMBER") {
                    return std::to_string(budget_ledger.y_construction_total);
                }
                if (name == "BUDGET_LABOACTION_QUANTITY_NUMBER") {
                    return std::to_string(budget_ledger.y_laboaction_count);
                }
                if (name == "BUDGET_LABOACTION_QUANTITY_COST_NUMBER") {
                    return std::to_string(budget_ledger.y_laboaction_total);
                }
                if (name == "BUDGET_ENTREES_QUANTITY_NUMBER") {
                    return std::to_string(budget_ledger.y_ticket_count);
                }
                if (name == "BUDGET_ENTREES_QUANTITY_COST_NUMBER") {
                    return std::to_string(budget_ledger.y_ticket_total);
                }
                if (name == "BUDGET_VENTES_QUANTITY_NUMBER") {
                    return std::to_string(budget_ledger.y_ventes_count);
                }
                if (name == "BUDGET_VENTES_QUANTITY_COST_NUMBER") {
                    return std::to_string(budget_ledger.y_ventes_total);
                }
                if (name == "BUDGET_FOOD_QUANTITY_NUMBER") {
                    return std::to_string(budget_ledger.y_food_count);
                }
                if (name == "BUDGET_FOOD_QUANTITY_COST_NUMBER") {
                    return std::to_string(budget_ledger.y_food_total);
                }
                if (name == "BUDGET_EFFECTIFS_SECU_QUANTITY_NUMBER") {
                    return std::to_string(budget_ledger.y_secu_count);
                }
                if (name == "BUDGET_EFFECTIFS_SECU_QUANTITY_COST_NUMBER") {
                    return std::to_string(budget_ledger.y_secu_total);
                }
                if (name == "BUDGET_EFFECTIFS_FOOD_QUANTITY_NUMBER") {
                    return std::to_string(
                        employee_system.count_role(employee_profiles, 0));
                }
                if (name == "BUDGET_EFFECTIFS_FOOD_QUANTITY_COST_NUMBER") {
                    return std::to_string(budget_ledger.y_staff_food_total);
                }
                const struct ShopRow {
                    const char* prefix;
                    int kind;
                } shop_rows[] = {
                    {"BUDGET_RESTOS_", 1},     {"BUDGET_BARAQUES_", 2},
                    {"BUDGET_SOUVENIRS_", 3},  {"BUDGET_BOITNOIRE_", 4},
                    {"BUDGET_POINTDEVUE_", 5}, {"BUDGET_GRADINS_", 6},
                };
                for (const ShopRow& row : shop_rows) {
                    if (name.rfind(row.prefix, 0) != 0) {
                        continue;
                    }
                    const std::size_t index =
                        static_cast<std::size_t>(row.kind);
                    if (name.find("_COST_NUMBER") != std::string::npos) {
                        return std::to_string(
                            budget_ledger.y_shop_revenue[index]);
                    }
                    return std::to_string(
                        budget_ledger.y_shop_sales[index]);
                }
                if (name == "BUDGET_TOTAL_LEFT_COST_NUMBER") {
                    return std::to_string(budget_ledger.expenses());
                }
                if (name == "BUDGET_TOTAL_RIGHT_COST_NUMBER") {
                    return std::to_string(budget_ledger.income());
                }
                return "0";
            }
            if (name == "BUDGET_TOTAL_PRICE_PANEL_PRICE_ZONE") {
                return std::to_string(budget_ledger.net());
            }
            return "";
        };

        for (const int i : order) {
            const dino::ui::Widget& widget =
                park.overlay.widgets[static_cast<std::size_t>(i)];
            if (!widget.texture.empty()) {
                if (const dino::ui::Graphic* graphic =
                        park.overlay.graphic(widget.texture)) {
                    if (UiTexture* texture = ui_texture(graphic->filename)) {
                        int frame = graphic->objects <= 1 ? 0 : 4;
                        if (widget.type == "BUTTON" && graphic->objects > 1) {
                            int wanted = -1;
                            if (i == park_active_index) {
                                wanted = 6;
                            } else if (i == hovered) {
                                wanted = 5;
                            }
                            if (wanted >= 0 &&
                                ui_frame_rect(*texture, *graphic, wanted).valid) {
                                frame = wanted;
                            }
                        }
                        const UiSourceRect source =
                            ui_frame_rect(*texture, *graphic, frame);
                        if (source.valid) {
                            const float u0 =
                                static_cast<float>(source.x) / texture->width;
                            const float v0 =
                                static_cast<float>(source.y) / texture->height;
                            const float u1 = static_cast<float>(source.x + source.w) /
                                             texture->width;
                            const float v1 = static_cast<float>(source.y + source.h) /
                                             texture->height;
                            draw_ui_mesh(
                                menu_gpu,
                                build_ui_quad(px(widget.abs_x), py(widget.abs_y),
                                              static_cast<float>(widget.width) * dpi,
                                              static_cast<float>(widget.height) * dpi,
                                              u0, v0, u1, v1,
                                              {255, 255, 255, 255}),
                                texture->id, 1.0f, 1, view_w, view_h);
                        }
                    }
                }
            } else if (widget.has_back_color) {
                draw_ui_mesh(menu_gpu,
                             build_screen_rect(px(widget.abs_x), py(widget.abs_y),
                                               static_cast<float>(widget.width) * dpi,
                                               static_cast<float>(widget.height) * dpi,
                                               widget.back_color),
                             0, 1.0f, 1, view_w, view_h);
            }

            // DINO_ACHAT_SAVE_RENDER_VIEW: draw the selected store entry's
            // dinosaur's standing LOD1 mesh directly into the well. The
            // retail widget is a 3D preview; the port fits the mesh to the
            // widget rect with a pixel-space MVP and scissor clipping.
            if (widget.name == "DINO_ACHAT_SAVE_RENDER_VIEW" &&
                !park.store_rows().empty()) {
                const int selected = park.store_selected();
                if (selected >= 0 &&
                    selected < static_cast<int>(park.store_rows().size())) {
                    if (StorePreview* preview =
                            store_preview_for(
                                park.store_rows()[static_cast<std::size_t>(
                                    selected)])) {
                        const float w =
                            static_cast<float>(widget.width) * dpi;
                        const float h =
                            static_cast<float>(widget.height) * dpi;
                        const float size =
                            std::max(0.001f, preview->size);
                        const dino::Vec3 center = preview->center;
                        // Side/three-quarter camera; the scene renderer
                        // already uses this projection convention for the
                        // imported dinosaurs.
                        const dino::Vec3 eye{
                            center.x - size * 1.4f,
                            center.y - size * 0.25f,
                            center.z + size * 0.35f};
                        const dino::Mat4 view =
                            look_at(eye, center, {0.0f, 0.0f, 1.0f});
                        const dino::Mat4 proj = perspective(
                            32.0f, 1.0f, 0.1f, size * 10.0f);
                        const dino::Mat4 mvp =
                            dino::multiply(proj, view);
                        const int vx =
                            static_cast<int>(px(widget.abs_x));
                        const int vy = static_cast<int>(
                            view_h - (py(widget.abs_y) + h));
                        glEnable(GL_SCISSOR_TEST);
                        glScissor(vx, vy, static_cast<int>(w),
                                  static_cast<int>(h));
                        glViewport(vx, vy, static_cast<int>(w),
                                   static_cast<int>(h));
                        glEnable(GL_DEPTH_TEST);
                        glDepthFunc(GL_LEQUAL);
                        glDisable(GL_CULL_FACE);
                        glClear(GL_DEPTH_BUFFER_BIT);
                        glfns.UseProgram(program);
                        glfns.UniformMatrix4fv(
                            glfns.GetUniformLocation(program, "u_mvp"), 1,
                            GL_TRUE, mvp.m);
                        glfns.Uniform1i(
                            glfns.GetUniformLocation(program, "u_unlit"), 1);
                        glfns.Uniform1i(
                            glfns.GetUniformLocation(program,
                                                     "u_use_texture"),
                            preview->texture != 0 ? 1 : 0);
                        glfns.Uniform1i(
                            glfns.GetUniformLocation(program,
                                                     "u_use_texture2"),
                            0);
                        glfns.Uniform1i(
                            glfns.GetUniformLocation(program,
                                                     "u_modulate2x"),
                            0);
                        glfns.Uniform1i(
                            glfns.GetUniformLocation(program, "u_texture"), 0);
                        glfns.Uniform1f(
                            glfns.GetUniformLocation(program, "u_alpha"),
                            1.0f);
                        glfns.Uniform1i(
                            glfns.GetUniformLocation(program,
                                                     "u_fog_enable"),
                            0);
                        const float light[3] = {-0.35f, -0.68f, 0.72f};
                        const float light_color[3] = {0.92f, 0.93f, 0.95f};
                        const float ambient[3] = {0.62f, 0.64f, 0.68f};
                        const float camera[3] = {0.0f, 0.0f, 4.0f};
                        glfns.Uniform3fv(
                            glfns.GetUniformLocation(program,
                                                     "u_light_dir"),
                            1, light);
                        glfns.Uniform3fv(
                            glfns.GetUniformLocation(program,
                                                     "u_light_color"),
                            1, light_color);
                        glfns.Uniform3fv(
                            glfns.GetUniformLocation(program, "u_ambient"),
                            1, ambient);
                        glfns.Uniform3fv(
                            glfns.GetUniformLocation(program, "u_camera"),
                            1, camera);
                        if (preview->texture != 0) {
                            glfns.ActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, preview->texture);
                        }
                        glfns.BindVertexArray(preview->gpu.vao);
                        glDrawElements(GL_TRIANGLES,
                                       preview->gpu.index_count,
                                       GL_UNSIGNED_INT, nullptr);
                        glViewport(0, 0, view_w, view_h);
                        glDisable(GL_SCISSOR_TEST);
                        glDisable(GL_DEPTH_TEST);
                    }
                }
            }

            // PROGRESS widgets: fill the gauge track proportionally. The
            // retail panel texture supplies the empty track; the port draws a
            // solid light-blue fill over it (the exact retail gauge art is a
            // `moProgress` draw we have not ported yet).
            if (widget.type == "PROGRESS" && widget.progress_max > 0) {
                const float fraction = std::clamp(
                    static_cast<float>(widget.progress_current) /
                        static_cast<float>(widget.progress_max),
                    0.0f, 1.0f);
                if (fraction > 0.0f) {
                    draw_ui_mesh(
                        menu_gpu,
                        build_screen_rect(
                            px(widget.abs_x), py(widget.abs_y),
                            static_cast<float>(widget.width) * fraction * dpi,
                            static_cast<float>(widget.height) * dpi,
                            {120, 200, 255, 255}),
                        0, 1.0f, 1, view_w, view_h);
                }
            }

            if (widget.name == dialog_zone() && park.dialog_open()) {
                const int line_h =
                    widget.list_line_height > 0 ? widget.list_line_height : 15;
                float list_scale = 1.0f;
                GLuint list_atlas = 0;
                const dino::Fnt* list_font =
                    ui_font(widget.font, widget.text_height, list_scale,
                            list_atlas);
                if (list_font != nullptr) {
                    const float max_w = static_cast<float>(
                        widget.width - widget.list_scroll_width);
                    std::vector<std::string> lines;
                    std::string current;
                    std::string word;
                    auto flush_line = [&]() {
                        lines.push_back(current);
                        current.clear();
                    };
                    auto flush_word = [&]() {
                        if (word.empty()) {
                            return;
                        }
                        const std::string candidate =
                            current.empty() ? word : current + " " + word;
                        if (!current.empty() &&
                            text_width(*list_font, candidate, list_scale) > max_w) {
                            flush_line();
                            current = word;
                        } else {
                            current = candidate;
                        }
                        word.clear();
                    };
                    for (const char ch : park.dialog_text()) {
                        if (ch == '\n') {
                            flush_word();
                            flush_line();
                        } else if (ch == ' ') {
                            flush_word();
                        } else {
                            word.push_back(ch);
                        }
                    }
                    flush_word();
                    if (!current.empty()) {
                        flush_line();
                    }
                    const int rows = std::min(
                        static_cast<int>(lines.size()),
                        line_h > 0 ? widget.height / line_h : 0);
                    for (int row = 0; row < rows; ++row) {
                        const dino::Mesh block = build_text_block(
                            *list_font, lines[static_cast<std::size_t>(row)],
                            px(widget.abs_x), py(widget.abs_y + row * line_h),
                            list_scale * dpi, widget.text_color, "LEFT",
                            static_cast<float>(max_w) * dpi,
                            static_cast<float>(line_h) * dpi);
                        draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f, 2,
                                     view_w, view_h);
                    }
                }
            }

            if (widget.name == "OPTIONS_SAVE_NAMES_LIST" && !save_slots.empty()) {
                const int line_h =
                    widget.list_line_height > 0 ? widget.list_line_height : 12;
                const int row_w =
                    widget.list_column0_width > 0
                        ? widget.list_column0_width
                        : widget.width - widget.list_scroll_width;
                float list_scale = 1.0f;
                GLuint list_atlas = 0;
                const dino::Fnt* list_font =
                    ui_font(widget.font, widget.text_height, list_scale,
                            list_atlas);
                if (list_font != nullptr) {
                    const int rows = std::min(
                        static_cast<int>(save_slots.size()),
                        line_h > 0 ? widget.height / line_h : 0);
                    for (int row = 0; row < rows; ++row) {
                        const dino::Mesh block = build_text_block(
                            *list_font, save_slots[static_cast<std::size_t>(row)].second,
                            px(widget.abs_x), py(widget.abs_y + row * line_h),
                            list_scale * dpi, widget.text_color, widget.justify,
                            static_cast<float>(row_w) * dpi,
                            static_cast<float>(line_h) * dpi);
                        draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f, 2,
                                     view_w, view_h);
                    }
                }
            }

            if (widget.name == "DINO_ACHAT_SAVE_NAMES_LIST" &&
                !park.store_rows().empty()) {
                const int line_h = widget.list_line_height > 0
                                       ? widget.list_line_height
                                       : 12;
                const int pitch = std::max(
                    1, widget.height /
                           dino::ui::ParkHud::store_visible_rows());
                const int row_w =
                    widget.list_column0_width > 0
                        ? widget.list_column0_width
                        : widget.width - widget.list_scroll_width;
                float list_scale = 1.0f;
                GLuint list_atlas = 0;
                const dino::Fnt* list_font =
                    ui_font(widget.font, widget.text_height, list_scale,
                            list_atlas);
                if (list_font != nullptr) {
                    for (int slot = 0;
                         slot < dino::ui::ParkHud::store_visible_rows();
                         ++slot) {
                        const dino::ui::StoreRow* row =
                            park.store_visible_row(slot);
                        if (row == nullptr) {
                            break;
                        }
                        const bool selected =
                            park.store_scroll() + slot ==
                            park.store_selected();
                        const int row_y = widget.abs_y + slot * pitch;
                        const dino::Mesh name_block = build_text_block(
                            *list_font, row->name, px(widget.abs_x),
                            py(row_y), list_scale * dpi,
                            selected ? dino::Color{255, 255, 0, 255}
                                     : widget.text_color,
                            "LEFT", static_cast<float>(row_w) * dpi,
                            static_cast<float>(line_h) * dpi);
                        draw_ui_mesh(menu_gpu, name_block, list_atlas, 1.0f,
                                     2, view_w, view_h);
                        const dino::Mesh price_block = build_text_block(
                            *list_font, std::to_string(row->price),
                            px(widget.abs_x), py(row_y + line_h),
                            list_scale * dpi,
                            dino::Color{220, 200, 120, 255}, "RIGHT",
                            static_cast<float>(row_w) * dpi,
                            static_cast<float>(line_h) * dpi);
                        draw_ui_mesh(menu_gpu, price_block, list_atlas, 1.0f,
                                     2, view_w, view_h);
                    }
                }
            }

            if (widget.name == "PREDEFINED_SHOW_LIST" &&
                !park.show_rows().empty()) {
                const int pitch = std::max(
                    1, widget.height /
                           dino::ui::ParkHud::show_visible_rows());
                const int column_widths[7] = {
                    widget.list_column0_width > 0
                        ? widget.list_column0_width
                        : 150,
                    widget.list_column1_width > 0
                        ? widget.list_column1_width
                        : 28,
                    widget.list_column2_width > 0
                        ? widget.list_column2_width
                        : 60,
                    widget.list_column3_width > 0
                        ? widget.list_column3_width
                        : 28,
                    widget.list_column4_width > 0
                        ? widget.list_column4_width
                        : 28,
                    widget.list_column5_width > 0
                        ? widget.list_column5_width
                        : 28,
                    widget.list_column6_width > 0
                        ? widget.list_column6_width
                        : 28,
                };
                float list_scale = 1.0f;
                GLuint list_atlas = 0;
                const dino::Fnt* list_font =
                    ui_font(widget.font, widget.text_height, list_scale,
                            list_atlas);
                if (list_font != nullptr) {
                    for (int slot = 0;
                         slot < dino::ui::ParkHud::show_visible_rows();
                         ++slot) {
                        const dino::ui::ShowRow* row =
                            park.show_visible_row(slot);
                        if (row == nullptr) {
                            break;
                        }
                        const bool selected =
                            park.show_scroll() + slot ==
                            park.show_selected();
                        const int row_y = widget.abs_y + slot * pitch;
                        int column_x = widget.abs_x;
                        auto draw_cell = [&](int column,
                                             const std::string& value,
                                             dino::Color color,
                                             const char* justify) {
                            const dino::Mesh block = build_text_block(
                                *list_font, value, px(column_x), py(row_y),
                                list_scale * dpi, color, justify,
                                static_cast<float>(column_widths[column]) *
                                    dpi,
                                static_cast<float>(pitch) * dpi);
                            draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f,
                                         2, view_w, view_h);
                            column_x += column_widths[column];
                        };
                        draw_cell(0, row->title,
                                  selected ? dino::Color{255, 255, 0, 255}
                                           : widget.text_color,
                                  "LEFT");
                        draw_cell(1, std::to_string(row->judge_kind),
                                  dino::Color{220, 220, 220, 255}, "CENTER");
                        draw_cell(2, std::to_string(row->price),
                                  dino::Color{220, 200, 120, 255}, "RIGHT");
                        for (int cage = 0; cage < 4; ++cage) {
                            draw_cell(
                                3 + cage,
                                row->cage_size[static_cast<std::size_t>(cage)] >
                                        0
                                    ? std::to_string(cage + 1)
                                    : std::string(),
                                dino::Color{180, 220, 180, 255}, "CENTER");
                        }
                    }
                }
            }

            std::string text = runtime_text(widget.name);
            if (text.empty()) {
                // `FUN_00516dd0` clears an empty attraction row to the
                // zero-filled `DAT_006743c8`; the INI `TEXT` ("10:00 AM") is
                // only the pre-refresh template. The hour itself is drawn by
                // the separate `ATTRACTION_LIST_TIME<n>` widget.
                if (widget.name.rfind("ATTRACTION_LIST_NAME", 0) == 0 ||
                    widget.name.rfind("ATTRACTION_LIST_ITERATION_INDIC", 0) == 0) {
                    continue;
                }
                text = dino::ui::resolve_loc_text(widget.text, park.loc);
            }
            if (text.empty()) {
                continue;
            }
            float scale = 1.0f;
            GLuint atlas = 0;
            if (const dino::Fnt* font =
                    ui_font(widget.font, widget.text_height, scale, atlas)) {
                const dino::Mesh block = build_text_block(
                    *font, text, px(widget.abs_x), py(widget.abs_y), scale * dpi,
                    widget.text_color, widget.justify,
                    static_cast<float>(widget.width) * dpi,
                    static_cast<float>(widget.height) * dpi);
                draw_ui_mesh(menu_gpu, block, atlas, 1.0f, 2, view_w, view_h);
            }
        }

        // The mission description/tips are LIST widgets whose back area has a
        // higher ZPRIORITY than the list in the overlay data; drawing them
        // per-widget would put the back area over the text. Draw these two
        // after the normal widget pass so they stay readable.
        if (campaign_runtime) {
            const dino::CampaignMission& mission = campaign_runtime->mission();
            auto draw_mission_text = [&](const char* widget_name,
                                         const std::string& text) {
                if (text.empty()) {
                    return;
                }
                const int index = park.overlay.find(widget_name);
                if (index < 0 || !park.overlay.visible_in_tree(index)) {
                    return;
                }
                const dino::ui::Widget& widget =
                    park.overlay.widgets[static_cast<std::size_t>(index)];
                float scale = 1.0f;
                GLuint atlas = 0;
                if (const dino::Fnt* font = ui_font(
                        widget.font, widget.text_height, scale, atlas)) {
                    const dino::Mesh block = build_text_block(
                        *font, text, px(widget.abs_x), py(widget.abs_y),
                        scale * dpi, widget.text_color, widget.justify,
                        static_cast<float>(widget.width) * dpi,
                        static_cast<float>(widget.height) * dpi);
                    draw_ui_mesh(menu_gpu, block, atlas, 1.0f, 2, view_w,
                                 view_h);
                }
            };
            draw_mission_text(
                "MISSION_PANEL_EXPLICATION",
                limit_lines(wrap_text(dino::ui::resolve_loc_text(
                                          mission.description_id, park.loc),
                                      58),
                            6));
            draw_mission_text(
                "MISSION_PANEL_TIPS",
                limit_lines(wrap_text(dino::ui::resolve_loc_text(
                                          mission.hint_id, park.loc),
                                      58),
                            6));
        }
    };

    bool running = true;
    bool start_screen = opt.force_menu ||
        (opt.frames == 0 && opt.screenshot.empty() && !opt.no_menu);
    if (menu_loaded && opt.use_menu && !menu.active) {
        start_screen = false;
    }
    bool show_help = opt.force_help;
    bool dragging = false;
    bool ui_click_consumed = false;
    float last_mx = 0.0f, last_my = 0.0f;
    int frame = 0;
    const std::uint64_t freq = SDL_GetPerformanceFrequency();
    std::uint64_t last = SDL_GetPerformanceCounter();
    double elapsed = 0.0;
    int width = opt.width;
    int height = opt.height;
    int logical_width = opt.width;
    int logical_height = opt.height;

    auto park_design = [](float window_x, float window_y, int& design_x,
                          int& design_y) {
        map_park_pointer(window_x, window_y, design_x, design_y);
    };

    // Keep hit-testing in the same coordinate space as the laid-out park
    // overlay. This also runs before the event loop so the first click after
    // startup or a resize uses the current window size.
    auto sync_park_layout = [&]() {
        SDL_GetWindowSize(window, &logical_width, &logical_height);
        if (park.loaded() &&
            (logical_width != park_layout_w || logical_height != park_layout_h)) {
            park.overlay.layout_for_size(logical_width, logical_height);
            park_layout_w = logical_width;
            park_layout_h = logical_height;
        }
    };
    if (park.loaded()) {
        park.overlay.layout_for_size(logical_width, logical_height);
        park_layout_w = logical_width;
        park_layout_h = logical_height;
    }

    const bool menu_hover_forced = !opt.menu_hover.empty();
    const bool park_hover_forced = !opt.park_hover.empty();

    while (running) {
        sync_park_layout();
        // --inject-click FRAME,X,Y[,right]: a real SDL button down/up pair in
        // window pixels, so tests drive the interactive input path.
        for (const std::string& spec : opt.inject_clicks) {
            int at = 0;
            float ix = 0.0f;
            float iy = 0.0f;
            char side[8] = {};
            const int n = std::sscanf(spec.c_str(), "%d,%f,%f,%7s", &at, &ix, &iy, side);
            if (n < 3 || at != frame) {
                continue;
            }
            const Uint8 button =
                (n == 4 && std::string(side) == "right") ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
            for (const Uint32 type : {SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP}) {
                SDL_Event injected{};
                injected.type = type;
                injected.button.windowID = SDL_GetWindowID(window);
                injected.button.button = button;
                injected.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                injected.button.clicks = 1;
                injected.button.x = ix;
                injected.button.y = iy;
                SDL_PushEvent(&injected);
            }
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (opt.no_input) {
                continue;
            }
            if (start_screen) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (menu_loaded && opt.use_menu) {
                    const float off_x =
                        (static_cast<float>(logical_width) - 800.0f) * 0.5f;
                    const float off_y =
                        (static_cast<float>(logical_height) - 600.0f) * 0.5f;
                    if (event.type == SDL_EVENT_MOUSE_MOTION) {
                        if (!menu_hover_forced) {
                            menu.move(static_cast<int>(event.motion.x - off_x),
                                      static_cast<int>(event.motion.y - off_y));
                        }
                    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                               event.button.button == SDL_BUTTON_LEFT) {
                        const int mx = static_cast<int>(event.button.x - off_x);
                        const int my = static_cast<int>(event.button.y - off_y);
                        const int menu_hit = menu.hit_test(mx, my);
                        if (menu_hit >= 0) {
                            const std::string& name =
                                menu.overlay.widgets[static_cast<std::size_t>(
                                    menu_hit)]
                                    .name;
                            const std::string sound_name =
                                name == "QUIT_BUTTON" ? "exitClic" : "clicDefault";
                            const auto sound_it =
                                menu_sound_files.find(sound_name);
                            if (sound_it != menu_sound_files.end()) {
                                ui_sound.play(sound_it->second);
                            }
                        }
                        menu.click(mx, my);
                        if (menu.quit) {
                            running = false;
                        }
                        if (!menu.active) {
                            start_screen = false;
                        }
                    } else if (event.type == SDL_EVENT_KEY_DOWN &&
                               !event.key.repeat) {
                        if (event.key.key == SDLK_ESCAPE) {
                            menu.escape();
                            if (menu.quit) {
                                running = false;
                            }
                        } else if (event.key.key == SDLK_RETURN && menu.page == 0) {
                            menu.page = 1;
                        }
                    }
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                        start_screen = false;
                    } else if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    }
                }
                continue;
            }
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_TEXT_INPUT) {
                if (park.loaded() && park.name_edit_active() &&
                    park.options_tab() == "save") {
                    for (const char* c = event.text.text; *c != '\0'; ++c) {
                        park.append_name_char(*c);
                    }
                    opt.save_name = park.save_name();
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                const SDL_Keycode key = event.key.key;
                if (park.name_edit_active() && key == SDLK_BACKSPACE) {
                    park.backspace_name();
                    opt.save_name = park.save_name();
                } else if (key == keymap[dino::KeyAction::Quit]) {
                    running = false;
                } else if (key == keymap[dino::KeyAction::Reset]) {
                    yaw = opt.yaw;
                    pitch = opt.pitch;
                    distance = auto_distance;
                } else if (key == keymap[dino::KeyAction::Fullscreen]) {
                    opt.fullscreen = !opt.fullscreen;
                    SDL_SetWindowFullscreen(window, opt.fullscreen);
                } else if (key == keymap[dino::KeyAction::Borderless]) {
                    opt.borderless = !opt.borderless;
                    SDL_SetWindowBordered(window, !opt.borderless);
                } else if (key == keymap[dino::KeyAction::Mute]) {
                    opt.muted = !opt.muted;
                    if (audio != nullptr) {
                        SDL_SetAudioStreamGain(audio, opt.muted ? 0.0f : 1.0f);
                    }
                    ui_sound.set_muted(opt.muted);
                    music.set_gain(opt.muted
                                       ? 0.0f
                                       : std::clamp(opt.music_volume / 100.0f,
                                                    0.0f, 1.0f));
                } else if (key == keymap[dino::KeyAction::ToggleCamera]) {
                    free_camera = !free_camera;
                    if (free_camera) {
                        enter_free_camera();
                    }
                } else if (key == SDLK_F1) {
                    hud_visible = !hud_visible;
                } else if (key == SDLK_TAB) {
                    show_help = !show_help;
                } else if (key == SDLK_1) {
                    build_tool = 1;
                    std::printf("build tool: path\n");
                } else if (key == SDLK_2) {
                    build_tool = 2;
                    std::printf("build tool: fence\n");
                } else if (key == SDLK_0) {
                    build_tool = 0;
                    pending_construct_kind = -1;
                    std::printf("build tool: off\n");
                } else if (key == SDLK_U) {
                    if (build_grid.undo()) {
                        rebuild_build_meshes();
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                       event.button.button == SDL_BUTTON_LEFT) {
                if (park.loaded()) {
                    int dx = 0, dy = 0;
                    park_design(event.button.x, event.button.y, dx, dy);
                    if (!park_hover_forced) {
                        park_hover_x = dx;
                        park_hover_y = dy;
                    }
                    const int hit = park.overlay.hit_test(dx, dy);
                    std::string action;
                    if (park.click(dx, dy, action)) {
                        if (hit >= 0) {
                            park_active_index = hit;
                        }
                        if (action.empty() && std::getenv("DINO_DEBUG_CLICKS") != nullptr) {
                            std::printf("park click %d,%d -> consumed by %s\n", dx, dy,
                                        hit >= 0 ? park.overlay.widgets[static_cast<std::size_t>(hit)]
                                                       .name.c_str()
                                                 : "?");
                        }
                        ui_click_consumed = true;
                        if (action == "minimap zoom in") {
                            minimap_zoom_in = true;
                        } else if (action == "minimap zoom out") {
                            minimap_zoom_out = true;
                        } else if (action == "camera pan forward") {
                            nav_pan_forward = true;
                        } else if (action == "camera pan back") {
                            nav_pan_back = true;
                        } else if (action == "camera pan left") {
                            nav_pan_left = true;
                        } else if (action == "camera pan right") {
                            nav_pan_right = true;
                        } else if (action == "camera turn left") {
                            nav_turn_left = true;
                        } else if (action == "camera turn right") {
                            nav_turn_right = true;
                        } else if (action == "camera zoom in") {
                            nav_zoom_in = true;
                        } else if (action == "camera zoom out") {
                            nav_zoom_out = true;
                        } else if (action == "save port" ||
                                   action == "load port") {
                            const std::string path =
                                port_save_target(action == "load port");
                            if (action == "save port") {
                                do_port_save(path);
                            } else {
                                do_port_load(path);
                            }
                            park.overlay.set_visible("OPTIONS_PANEL", false);
                        } else if (action == "close campaign end") {
                            advance_campaign();
                        }
                        if (!action.empty()) {
                            std::printf("park click %d,%d -> %s\n", dx, dy,
                                        action.c_str());
                            const auto sound_it =
                                menu_sound_files.find("clicDefault");
                            if (sound_it != menu_sound_files.end()) {
                                ui_sound.play(sound_it->second);
                            }
                            if (action.rfind("open DINO_LAB", 0) == 0 ||
                                action.rfind("open LAB_", 0) == 0) {
                                const auto lab_music =
                                    park_sound_files.find("musLabo");
                                if (lab_music != park_sound_files.end()) {
                                    music.play(lab_music->second);
                                }
                            } else if (action == "close lab" &&
                                       !lnd.music.empty()) {
                                const auto island_music =
                                    park_sound_files.find(lnd.music);
                                if (island_music != park_sound_files.end()) {
                                    music.play(island_music->second);
                                }
                            }
                            // A menu-started free game has no build grid yet;
                            // every construction tool needs one for its
                            // ground clicks.
                            if (action.rfind("construct ", 0) == 0) {
                                ensure_build_ready();
                            }
                            if (action ==
                                "open FENCE_CONSTRUCTION_AND_INFO_PANEL") {
                                // Retail arms the enclosure tool when the
                                // Enclosure panel opens; the ground clicks
                                // then add fence edges.
                                ensure_build_ready();
                                build_tool = 6;
                                fence_corner_x = -1;
                                fence_corner_y = -1;
                                pending_construct_kind = -1;
                                std::printf(
                                    "fence: tool armed (click two corners)\n");
                            }
                            if (action == "construct CONSTRUC_ROAD_BUTTON") {
                                arm_road_tool();
                            } else if (action.rfind("construct ", 0) == 0) {
                                const std::string widget = action.substr(10);
                                const int kind =
                                    construction_kind_for_widget(widget);
                                if (kind < 0) {
                                    std::printf(
                                        "construction: no building for %s\n",
                                        widget.c_str());
                                } else {
                                    const dino::BuildingDef& def =
                                        construction_defs[static_cast<
                                            std::size_t>(kind)];
                                    if (park_budget <
                                        static_cast<int>(
                                            def.construction_cost)) {
                                        std::printf(
                                            "construction: %s needs %u, "
                                            "budget %d "
                                            "(WARNING_NOT_ENOUGH_MONEY)\n",
                                            def.name.c_str(),
                                            def.construction_cost,
                                            park_budget);
                                    } else {
                                        build_tool = 3;
                                        pending_construct_kind = kind;
                                        std::printf(
                                            "construction: %s selected, "
                                            "cost=%u (click the ground)\n",
                                            def.name.c_str(),
                                            def.construction_cost);
                                    }
                                }
                            }
                            if (action.rfind("store buy ", 0) == 0) {
                                ensure_build_ready();  // paddock pick click
                                handle_store_buy(
                                    std::atoi(action.c_str() + 10));
                            } else if (action == "store no selection") {
                                std::printf("store: no dinosaur selected\n");
                            } else if (action.rfind("show place ", 0) == 0) {
                                handle_show_place(
                                    std::atoi(action.c_str() + 11));
                            } else if (action == "show no selection") {
                                std::printf("show: no show selected\n");
                            } else if (action.rfind("attraction ", 0) == 0) {
                                handle_attraction_action(action);
                            }
                        }
                        continue;
                    }
                }
                dragging = true;
                drag_distance = 0.0f;
                last_mx = event.button.x;
                last_my = event.button.y;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                       event.button.button == SDL_BUTTON_RIGHT &&
                       build_tool == 4) {
                // 0x52ed6a: cancel the run in progress, else disarm the tool.
                if (road_has_start) {
                    road_has_start = false;
                    std::printf("road: segment cancelled\n");
                } else {
                    build_tool = 0;
                    std::printf("road: tool off\n");
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                       event.button.button == SDL_BUTTON_LEFT) {
                park_active_index = -1;
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
                dragging = false;
                if (ui_click_consumed) {
                    ui_click_consumed = false;
                    continue;
                }
                if (std::getenv("DINO_DEBUG_CLICKS") != nullptr) {
                    std::printf("ground release %.0f,%.0f drag=%.1f tool=%d build_ready=%d\n",
                                event.button.x, event.button.y, drag_distance,
                                build_tool, build_ready ? 1 : 0);
                }
                if ((build_tool != 0 || pending_store_index >= 0) &&
                    drag_distance < 5.0f) {
                    pending_click = true;
                    click_x = event.button.x;
                    click_y = event.button.y;
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                if (park.loaded()) {
                    int dx = 0, dy = 0;
                    park_design(event.motion.x, event.motion.y, dx, dy);
                    if (!park_hover_forced) {
                        park_hover_x = dx;
                        park_hover_y = dy;
                    }
                }
                if (!dragging) {
                    continue;
                }
                const float dx = event.motion.x - last_mx;
                const float dy = event.motion.y - last_my;
                drag_distance += std::abs(dx) + std::abs(dy);
                if (free_camera) {
                    free_yaw += dx * 0.3f;
                    free_pitch = std::clamp(free_pitch - dy * 0.3f, -89.0f, 89.0f);
                } else if (camera_retail_model) {
                    camera_yaw_goal += dx * 0.4f;
                    camera_progress = std::clamp(
                        camera_progress + dy * 0.002f, 0.0f, 1.0f);
                } else {
                    yaw += dx * 0.4f;
                    pitch = std::clamp(pitch + dy * 0.4f, -89.0f, 89.0f);
                }
                last_mx = event.motion.x;
                last_my = event.motion.y;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                if (camera_retail_model) {
                    const float step =
                        event.wheel.y > 0.0f ? -0.05f : 0.05f;
                    camera_progress =
                        std::clamp(camera_progress + step, 0.0f, 1.0f);
                } else {
                    distance = std::clamp(
                        distance * std::pow(1.1f, -event.wheel.y),
                        camera_config.distance_min,
                        camera_config.distance_max);
                }
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                width = event.window.data1;
                height = event.window.data2;
            }
        }
        if (!running) {
            break;
        }

        SDL_GetWindowSizeInPixels(window, &width, &height);
        sync_park_layout();
        if (width <= 0 || height <= 0) {
            SDL_Delay(10);
            continue;
        }

        const std::uint64_t now = SDL_GetPerformanceCounter();
        const double dt = static_cast<double>(now - last) / static_cast<double>(freq);
        last = now;
        elapsed += dt;

        if (menu_loaded && opt.use_menu && menu.loading) {
            menu.loading_timer -= static_cast<float>(dt);
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
                        std::printf(
                            "start: campaign=%s mission=%s island=%s "
                            "budget=%d\n",
                            campaign->name.c_str(), mission->name.c_str(),
                            campaign->island.c_str(), park_budget);
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
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "island load: %s\n", e.what());
                    }
                    park_budget = free_game_budgets[difficulty];
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
            park.update(dt);
            park_clock.advance(dt);
            if (campaign_runtime && campaign_runtime->active()) {
                const int hour_index = static_cast<int>(
                    park_clock.elapsed * park_clock.hours_per_second());
                show_runtime.tick(
                    hour_index, park_clock.day, park_clock.end_of_day, dt,
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
            employee_system.update(dt, employee_profiles, food_sources,
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
            campaign_runtime->tick(dt, mission_state);
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
                            std::uint32_t next_mask = campaign_unlock;
                            if (number > 0 && number <= 32) {
                                next_mask |= 1u << (number - 1);
                                if (number < 32) {
                                    next_mask |= 1u << number;
                                }
                            }
                            if (next_mask != campaign_unlock) {
                                campaign_unlock = next_mask;
                                try {
                                    dino::write_campaign_unlock(
                                        campaign_dino_dat, campaign_unlock);
                                    std::printf(
                                        "campaign unlock: mask=0x%X\n",
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
        ui_sound.update(dt);
        music.update();

        if (minimap_zoom_in) {
            minimap_scale =
                std::max(0.1f, minimap_scale - static_cast<float>(dt) * 0.15f);
        }
        if (minimap_zoom_out) {
            minimap_scale =
                std::min(2.0f, minimap_scale + static_cast<float>(dt) * 0.15f);
        }

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
            const float step = speed * static_cast<float>(dt);
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
            const float dtf = static_cast<float>(dt);
            // camera.ini uses one `progress` p in [0,1] to interpolate every
            // speed/limit. The retail model keeps it as authoritative; the
            // legacy path recovers it from the current eye distance/pitch.
            float progress = camera_progress;
            if (!camera_retail_model) {
                const float pitch_rad = pitch * 3.14159265f / 180.0f;
                const float horizontal =
                    std::max(camera_config.distance_min,
                             distance * std::cos(pitch_rad));
                progress = std::clamp(
                    (horizontal - camera_config.distance_min) /
                        std::max(1.0f, camera_config.distance_max -
                                           camera_config.distance_min),
                    0.0f, 1.0f);
            }
            const float rotate =
                camera_config.rotation_speed(progress) * 57.2957795f * dtf;
            if (camera_retail_model) {
                if (nav_turn_left ||
                    (shift && held(dino::KeyAction::OrbitLeft))) {
                    camera_yaw_goal -= rotate;
                }
                if (nav_turn_right ||
                    (shift && held(dino::KeyAction::OrbitRight))) {
                    camera_yaw_goal += rotate;
                }
                // Retail low-passes the yaw at 10/s, giving the ~9 deg lag
                // that reads as a small lean while turning.
                yaw += (camera_yaw_goal - yaw) *
                       std::min(1.0f, 10.0f * dtf);
            } else {
                if (nav_turn_left ||
                    (shift && held(dino::KeyAction::OrbitLeft))) {
                    yaw -= rotate;
                }
                if (nav_turn_right ||
                    (shift && held(dino::KeyAction::OrbitRight))) {
                    yaw += rotate;
                }
            }
            const float min_pitch =
                std::atan2(camera_config.elevation_min,
                           std::max(1e-3f, distance)) *
                57.2957795f;
            const float max_pitch =
                std::atan2(camera_config.elevation_max,
                           std::max(1e-3f, distance)) *
                57.2957795f;
            if (camera_retail_model) {
                const float dp = camera_config.progress_speed(progress) * dtf;
                if (shift && held(dino::KeyAction::OrbitUp)) {
                    camera_progress =
                        std::clamp(camera_progress - dp, 0.0f, 1.0f);
                }
                if (shift && held(dino::KeyAction::OrbitDown)) {
                    camera_progress =
                        std::clamp(camera_progress + dp, 0.0f, 1.0f);
                }
                progress = camera_progress;
            } else {
                if (shift && held(dino::KeyAction::OrbitUp)) {
                    pitch = std::min(max_pitch, pitch + rotate);
                }
                if (shift && held(dino::KeyAction::OrbitDown)) {
                    pitch = std::max(min_pitch, pitch - rotate);
                }
            }
            // `yaw` points from the target to the eye; the view direction is
            // the opposite. Move the target in the camera's ground plane at
            // the retail pan speed (MoveSpeed is in 4-unit cells/second).
            const float view_yaw =
                yaw * 3.14159265f / 180.0f + 3.14159265f;
            const dino::Vec3 forward{std::cos(view_yaw),
                                     std::sin(view_yaw), 0.0f};
            const dino::Vec3 right{std::sin(view_yaw),
                                   -std::cos(view_yaw), 0.0f};
            const float step =
                camera_config.move_speed(progress) * 4.0f * dtf;
            dino::Vec3 delta{0.0f, 0.0f, 0.0f};
            if (nav_pan_forward || (!shift && held(dino::KeyAction::OrbitUp))) {
                delta.x += forward.x * step;
                delta.y += forward.y * step;
            }
            if (nav_pan_back || (!shift && held(dino::KeyAction::OrbitDown))) {
                delta.x -= forward.x * step;
                delta.y -= forward.y * step;
            }
            if (nav_pan_right || (!shift && held(dino::KeyAction::OrbitRight))) {
                delta.x += right.x * step;
                delta.y += right.y * step;
            }
            if (nav_pan_left || (!shift && held(dino::KeyAction::OrbitLeft))) {
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
                    // FUN_00529d00 smooths the target height; 0.1 per
                    // 60 Hz frame matches its (height-current)*0.1 step.
                    const float target_z =
                        terrain.height_at(center.x, center.y) + 2.5f;
                    const float alpha = std::min(
                        1.0f, camera_config.position_hardness(progress) * dtf);
                    center.z += (target_z - center.z) * alpha;
                }
            }
            if (camera_retail_model) {
                // Retail: zoom in/out and Shift+Up/Down are the same axis;
                // both move progress at ProgressSpeed(p) per second.
                const float dp =
                    camera_config.progress_speed(progress) * dtf;
                if (nav_zoom_in || held(dino::KeyAction::ZoomIn)) {
                    camera_progress =
                        std::clamp(camera_progress - dp, 0.0f, 1.0f);
                }
                if (nav_zoom_out || held(dino::KeyAction::ZoomOut)) {
                    camera_progress =
                        std::clamp(camera_progress + dp, 0.0f, 1.0f);
                }
                const float radius = camera_radius();
                const float height = camera_height();
                pitch = std::atan2(height, radius) *
                        57.2957795f;
                distance = std::sqrt(radius * radius + height * height);
            } else {
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

        const float pulse = 0.5f + 0.5f * SDL_sinf(static_cast<float>(elapsed) * 1.5f);
        glClearColor(0.055f + 0.02f * pulse, 0.09f + 0.03f * pulse,
                     0.14f + 0.04f * pulse, 1.0f);

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
            danger_timer += dt;
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
                dinosaur.action_loop->tick(dt);
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
                                  static_cast<float>(dt), attractions,
                                  entrance_cell_x, entrance_cell_y,
                                  visitor_profiles);
            update_mesh(visitor_gpu,
                        visitor_system.build_mesh(build_grid, terrain,
                                                  visitor_pose1, visitor_pose2),
                        glfns);
        }

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

        if (pending_fence_tool) {
            ensure_build_ready();
            build_tool = 6;
            fence_corner_x = -1;
            fence_corner_y = -1;
            pending_construct_kind = -1;
            pending_fence_tool = false;
            std::printf("fence: tool armed (click two corners)\n");
        }
        if ((build_tool == 2 || build_tool == 6) && build_ready &&
            !opt.no_input) {
            float mxf = 0.0f;
            float myf = 0.0f;
            SDL_GetMouseState(&mxf, &myf);
            if (!opt.fence_cursor.empty()) {
                std::sscanf(opt.fence_cursor.c_str(), "%f,%f", &mxf, &myf);
            }
            dino::Vec3 hit;
            int cx = 0;
            int cy = 0;
            bool visible = false;
            bool ok = false;
            if (pick_terrain(terrain, eye, view_center, mxf, myf,
                             logical_width, logical_height, hit) &&
                build_grid.cells_for_world(hit.x, hit.y, cx, cy)) {
                const float cs = build_grid.cell_size;
                const float wx =
                    build_grid.origin_x + (static_cast<float>(cx) + 0.5f) * cs;
                const float wy =
                    build_grid.origin_y + (static_cast<float>(cy) + 0.5f) * cs;
                ok = terrain.buildable(wx, wy);
                // Retail rejects a fence start strictly inside an existing
                // paddock (`WARNING_INSIDE_PERIMETER`). Paddock rects are in
                // record coordinates (0..world_size); the build cell centre is
                // in centred world coordinates.
                const float half_world = terrain.world_size * 0.5f;
                const float rx = wx + half_world;
                const float ry = wy + half_world;
                for (const dino::WorldRect& rect : road_perimeters()) {
                    if (rx > std::min(rect.x0, rect.x1) &&
                        rx < std::max(rect.x0, rect.x1) &&
                        ry > std::min(rect.y0, rect.y1) &&
                        ry < std::max(rect.y0, rect.y1)) {
                        ok = false;
                        break;
                    }
                }
                visible = true;
                if (cx != fence_preview_cell_x ||
                    cy != fence_preview_cell_y ||
                    ok != fence_preview_ok ||
                    fence_preview_gpu.vao == 0) {
                    fence_preview_cell_x = cx;
                    fence_preview_cell_y = cy;
                    fence_preview_gpu.destroy(glfns);
                    fence_preview_gpu = upload_mesh(
                        dino::fence_preview_disc(
                            wx, wy, terrain.height_at(wx, wy) + 0.15f,
                            cs * 0.75f),
                        glfns);
                    std::printf("fence preview: cell=%d,%d ok=%d\n", cx, cy,
                                ok ? 1 : 0);
                }
            }
            if (visible != fence_preview_visible ||
                ok != fence_preview_ok) {
                fence_preview_visible = visible;
                fence_preview_ok = ok;
                rebuild_extra_items();
            }
        } else if (fence_preview_visible) {
            fence_preview_visible = false;
            fence_preview_cell_x = -1;
            fence_preview_cell_y = -1;
            rebuild_extra_items();
        }
        if (pending_click) {
            pending_click = false;
            if (pending_store_index >= 0 && build_ready) {
                // FENCE_INSERTION: the click picks the paddock; a full paddock
                // keeps the insertion state and shows the retail warning.
                dino::Vec3 hit;
                if (pick_terrain(terrain, eye, view_center, click_x, click_y,
                                 logical_width, logical_height, hit)) {
                    const float half = terrain.world_size * 0.5f;
                    const float wx = hit.x + half;
                    const float wy = hit.y + half;
                    std::printf("store: pick world %.1f %.1f\n", wx, wy);
                    int paddock_index = -1;
                    for (std::size_t p = 0; p < paddocks.size(); ++p) {
                        const dino::WorldRect& r = paddocks[p].rect;
                        if (wx >= static_cast<float>(std::min(r.x0, r.x1)) &&
                            wx <= static_cast<float>(std::max(r.x0, r.x1)) &&
                            wy >= static_cast<float>(std::min(r.y0, r.y1)) &&
                            wy <= static_cast<float>(std::max(r.y0, r.y1))) {
                            paddock_index = static_cast<int>(p);
                            break;
                        }
                    }
                    if (paddock_index < 0) {
                        std::printf("store: click inside a paddock\n");
                    } else if (pending_store_index >=
                               static_cast<int>(store_catalog.size())) {
                        pending_store_index = -1;
                        pending_store_price = 0;
                    } else {
                        const dino::MarketEntry entry =
                            store_catalog[static_cast<std::size_t>(
                                pending_store_index)];
                        std::string species;
                        std::string family;
                        int cost_class = 0;
                        try {
                            const std::string genotype_path = resolve_asset(
                                opt.asset_root,
                                "Data/Design/Scenario/Genotype/" +
                                    entry.genotype);
                            const int head =
                                dino::read_gen_head_type(genotype_path);
                            dino::DinoGenotype genotype{};
                            bool have_genotype = false;
                            try {
                                genotype =
                                    dino::read_gen_genotype(genotype_path);
                                have_genotype = true;
                            } catch (const std::exception&) {
                            }
                            const auto match = head_species.find(head);
                            species = match != head_species.end()
                                          ? match->second
                                          : "allo";
                            family = family_table.family_of(species);
                            if (family.empty()) {
                                family = "LightBiped";
                            }
                            cost_class =
                                (have_genotype && phenotype_table.valid)
                                    ? phenotype_table.size_class(genotype)
                                    : (size_for_species
                                           ? size_for_species(species)
                                           : 1);
                            const int dino_id =
                                1000000 +
                                static_cast<int>(imported_dinos.size());
                            dino::Paddock& paddock =
                                paddocks[static_cast<std::size_t>(
                                    paddock_index)];
                            if (!dino::paddock_can_add(paddock, dino_id,
                                                       cost_class)) {
                                std::printf(
                                    "store: no room in paddock %d "
                                    "(WARNING_NO_ROOM_IN_FENCE)\n",
                                    paddock_index);
                                park.show_message(
                                    dino::ui::resolve_loc_text(
                                        "&WARNING_NO_ROOM_IN_FENCE",
                                        park.loc));
                            } else {
                                const bool have_attributes =
                                    have_genotype && phenotype_table.valid;
                                const dino::DinoAttributes store_attrs =
                                    have_attributes
                                        ? phenotype_table.attributes(genotype)
                                        : dino::DinoAttributes{};
                                if (add_imported_dino(
                                        species, family, opt.dino_clip, wx,
                                        wy, 0.0f,
                                        have_attributes ? &store_attrs
                                                        : nullptr)) {
                                    dino::paddock_add(paddock, dino_id,
                                                      cost_class);
                                    park_budget -= pending_store_price;
                                    budget_ledger.spend_dinoachat(
                                        pending_store_price);
                                    store_catalog.erase(
                                        store_catalog.begin() +
                                        pending_store_index);
                                    refresh_store_rows();
                                    park_paddocks = paddocks;
                                    float total_used = 0.0f;
                                    for (const dino::Paddock& p : paddocks) {
                                        total_used += p.used;
                                    }
                                    std::printf(
                                        "store: bought %s price=%d budget=%d "
                                        "paddock=%d dinos=%zu "
                                        "dinoachat=%d/%d used=%.0f "
                                        "total=%.0f\n",
                                        entry.name.c_str(),
                                        pending_store_price, park_budget,
                                        paddock_index, imported_dinos.size(),
                                        budget_ledger.dinoachat_count,
                                        budget_ledger.dinoachat_total,
                                        paddock.used, total_used);
                                    pending_store_index = -1;
                                    pending_store_price = 0;
                                    rebuild_extra_items();
                                } else {
                                    std::printf(
                                        "store: %s model load failed\n",
                                        entry.name.c_str());
                                }
                            }
                        } catch (const std::exception& e) {
                            std::fprintf(stderr, "store: %s\n", e.what());
                        }
                    }
                }
            } else if (build_ready && build_tool != 0) {                dino::Vec3 hit;
                const bool picked = pick_terrain(terrain, eye, view_center, click_x,
                                                 click_y, logical_width, logical_height, hit);
                if (std::getenv("DINO_DEBUG_CLICKS") != nullptr) {
                    std::printf("ground click %.0f,%.0f picked=%d hit=%.1f,%.1f\n", click_x,
                                click_y, picked ? 1 : 0, hit.x, hit.y);
                }
                if (picked) {
                    int cx = 0;
                    int cy = 0;
                    if (build_grid.cells_for_world(hit.x, hit.y, cx, cy)) {
                        const int before = build_grid.path_count + build_grid.fence_count;
                        const float cs = build_grid.cell_size;
                        auto cell_buildable = [&](int gx, int gy) {
                            return terrain.buildable(
                                build_grid.origin_x +
                                    (static_cast<float>(gx) + 0.5f) * cs,
                                build_grid.origin_y +
                                    (static_cast<float>(gy) + 0.5f) * cs);
                        };
                        if (build_tool == 4) {
                            road_click(cx, cy);  // rebuilds the meshes itself
                        } else if (build_tool == 1) {
                            const bool removing = build_grid.path_at(cx, cy);
                            if (!removing && !cell_buildable(cx, cy)) {
                                std::printf("placement: blocked cell\n");
                            } else {
                                build_grid.set_path(cx, cy, !removing);
                            }
                        } else if (build_tool == 6) {
                            // Retail FENCE_CONSTRUCTION: first click stores a
                            // corner, second click builds the sorted rectangle.
                            if (fence_corner_x < 0) {
                                fence_corner_x = cx;
                                fence_corner_y = cy;
                                std::printf(
                                    "fence: corner %d,%d (click opposite "
                                    "corner)\n",
                                    cx, cy);
                            } else {
                                const int x0 =
                                    std::min(fence_corner_x, cx);
                                const int x1 =
                                    std::max(fence_corner_x, cx);
                                const int y0 =
                                    std::min(fence_corner_y, cy);
                                const int y1 =
                                    std::max(fence_corner_y, cy);
                                if (x1 - x0 < 1 || y1 - y0 < 1) {
                                    std::printf(
                                        "fence: perimeter too small\n");
                                } else {
                                    const int before =
                                        build_grid.fence_count;
                                    for (int x = x0; x <= x1; ++x) {
                                        build_grid.set_fence(x, y0, 0, true);
                                        build_grid.set_fence(x, y1 + 1, 0,
                                                             true);
                                    }
                                    for (int y = y0; y <= y1; ++y) {
                                        build_grid.set_fence(x0 - 1, y, 1,
                                                             true);
                                        build_grid.set_fence(x1, y, 1, true);
                                    }
                                    std::printf(
                                        "fence: built rect %d,%d-%d,%d "
                                        "edges=%d\n",
                                        x0, y0, x1, y1,
                                        build_grid.fence_count - before);
                                    rebuild_build_meshes();
                                }
                                fence_corner_x = -1;
                                fence_corner_y = -1;
                            }
                        } else if (build_tool == 3 &&
                                   pending_construct_kind >= 0) {
                            const float half = terrain.world_size * 0.5f;
                            try_construct(
                                pending_construct_kind,
                                static_cast<int>(std::floor(hit.x + half)),
                                static_cast<int>(std::floor(hit.y + half)), 0);
                        } else if (build_tool == 5 && pending_show_index >= 0) {
                            // Grid cell -> show record coordinate (0..1024).
                            place_show_at(
                                static_cast<int>(std::lround(cx * cs)),
                                static_cast<int>(std::lround(cy * cs)));
                        } else {
                            const float x0 = build_grid.origin_x + static_cast<float>(cx) * cs;
                            const float y0 = build_grid.origin_y + static_cast<float>(cy) * cs;
                            float best = 1e30f;
                            int ex = cx;
                            int ey = cy;
                            int edge = 0;
                            auto consider = [&](float distance, int gx, int gy, int e) {
                                if (distance < best) {
                                    best = distance;
                                    ex = gx;
                                    ey = gy;
                                    edge = e;
                                }
                            };
                            if (cx > 0) {
                                consider(std::fabs(hit.x - x0), cx - 1, cy, 1);
                            }
                            consider(std::fabs(hit.x - (x0 + cs)), cx, cy, 1);
                            consider(std::fabs(hit.y - y0), cx, cy, 0);
                            if (cy + 1 < build_grid.height) {
                                consider(std::fabs(hit.y - (y0 + cs)), cx, cy + 1, 0);
                            }
                            const bool removing =
                                build_grid.fence_at(ex, ey, edge);
                            bool allowed = removing;
                            if (!allowed) {
                                const bool inside = cell_buildable(ex, ey);
                                bool neighbour = true;
                                if (edge == 1) {
                                    if (ex + 1 < build_grid.width) {
                                        neighbour = cell_buildable(ex + 1, ey);
                                    }
                                } else if (ey > 0) {
                                    neighbour = cell_buildable(ex, ey - 1);
                                }
                                allowed = inside && neighbour;
                            }
                            if (!allowed) {
                                std::printf("placement: blocked cell\n");
                            } else {
                                build_grid.set_fence(ex, ey, edge, !removing);
                            }
                        }
                        if (build_tool != 4 &&
                            build_grid.path_count + build_grid.fence_count != before) {
                            rebuild_build_meshes();
                        }
                    }
                }
            }
        }

        if (frame == 0 && !opt.pick_test.empty()) {
            int mx = 0;
            int my = 0;
            if (std::sscanf(opt.pick_test.c_str(), "%d,%d", &mx, &my) == 2) {
                dino::Vec3 hit;
                if (pick_terrain(terrain, eye, view_center, static_cast<float>(mx),
                                 static_cast<float>(my), logical_width,
                                 logical_height, hit)) {
                    const float cs = build_ready ? build_grid.cell_size : 4.0f;
                    const float origin = build_ready ? build_grid.origin_x
                                                     : -terrain.world_size * 0.5f;
                    const int cx = static_cast<int>(std::floor((hit.x - origin) / cs));
                    const int cy = static_cast<int>(std::floor((hit.y - origin) / cs));
                    std::printf("pick: %.2f %.2f %.2f cell %d %d\n", hit.x, hit.y,
                                hit.z, cx, cy);
                } else {
                    std::printf("pick: none\n");
                }
            }
        }

        // Road tool preview: the pending run from the start cell to the
        // axis-locked cursor cell, rebuilt only when it changes (0x52e750).
        if (build_ready && build_tool == 4 && road_has_start) {
            float mxf = 0.0f;
            float myf = 0.0f;
            SDL_GetMouseState(&mxf, &myf);
            if (!opt.road_cursor.empty()) {
                std::sscanf(opt.road_cursor.c_str(), "%f,%f", &mxf, &myf);
            }
            dino::Vec3 hit;
            int cx = 0;
            int cy = 0;
            if (pick_terrain(terrain, eye, view_center, mxf, myf, logical_width,
                             logical_height, hit) &&
                build_grid.cells_for_world(hit.x, hit.y, cx, cy)) {
                const dino::RoadSegment road =
                    dino::road_segment(road_start_x, road_start_y, cx, cy);
                const std::array<int, 4> key = {road.x0, road.y0, road.x1, road.y1};
                if (key != road_preview_key) {
                    road_preview_key = key;
                    if (road_helper_box.positions.empty()) {
                        try {
                            road_helper_box = dino::load_mesh(resolve_asset(
                                opt.asset_root, "Data/Graphics/focus/helper_road.mesh"));
                        } catch (const std::exception& e) {
                            std::fprintf(stderr, "road helper mesh: %s\n", e.what());
                        }
                    }
                    road_preview_gpu.destroy(glfns);
                    road_preview_gpu = upload_mesh(
                        dino::road_helper_mesh(road_helper_box, road, build_grid, terrain),
                        glfns);
                    const int cost = dino::road_cost(road, ensure_road_price());
                    road_preview_ok =
                        dino::check_road(build_grid, road, road_perimeters(),
                                         road_cell_busy) == dino::RoadCheck::Ok &&
                        park_budget >= cost;
                    if (road_preview_texture == 0) {
                        try {
                            road_helper_image = dino::load_tga(resolve_asset(
                                opt.asset_root, "Data/Graphics/Textures/focus/road.tga"));
                        } catch (const std::exception& e) {
                            std::fprintf(stderr, "road helper texture: %s\n", e.what());
                            road_helper_image.width = road_helper_image.height = 1;
                            road_helper_image.rgba = {255, 255, 255, 255};
                        }
                        road_preview_texture = upload_texture(road_helper_image);
                    }
                    rebuild_extra_items();
                    if (cost != road_preview_cost) {
                        road_preview_cost = cost;
                        if (park.loaded()) {
                            park.show_message(dino::ui::resolve_loc_text(
                                                  "&MESSAGE_ROAD_COST", park.loc) +
                                              std::to_string(cost));
                        }
                    }
                }
            }
            // gscMaterialBank (0x600550): x = trunc((sin(6 t) + 1) * 0.5 * 127);
            // material 2 (valid) = RGB(64, 64, 255 - x), material 3 (invalid)
            // = RGB(255 - x, 64, 64).
            if (road_preview_texture != 0 && road_helper_image.width > 0) {
                const double t = static_cast<double>(SDL_GetTicks()) / 1000.0;
                const int x = static_cast<int>((std::sin(6.0 * t) + 1.0) * 0.5 * 127.0);
                const int color[3] = {road_preview_ok ? 64 : 255 - x, 64,
                                      road_preview_ok ? 255 - x : 64};
                dino::Image tinted = road_helper_image;
                for (std::size_t i = 0; i + 3 < tinted.rgba.size(); i += 4) {
                    for (int c = 0; c < 3; ++c) {
                        tinted.rgba[i + c] = static_cast<std::uint8_t>(
                            tinted.rgba[i + c] * color[c] / 255);
                    }
                }
                glBindTexture(GL_TEXTURE_2D, road_preview_texture);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tinted.width, tinted.height,
                                GL_RGBA, GL_UNSIGNED_BYTE, tinted.rgba.data());
            }
        } else if (road_preview_gpu.vao != 0) {
            road_preview_gpu.destroy(glfns);
            road_preview_key = {-1, -1, -1, -1};
            road_preview_cost = -1;
            rebuild_extra_items();
        }

        if (park.loaded() && !park.hud_hidden()) {
            const float heading =
                std::atan2(view_center.y - eye.y, view_center.x - eye.x);
            const bool moved =
                std::abs(view_center.x - minimap_focus.x) > 0.5f ||
                std::abs(view_center.y - minimap_focus.y) > 0.5f ||
                std::abs(heading - minimap_heading) > 0.002f ||
                std::abs(minimap_scale - minimap_rendered_scale) > 0.002f;
            if (moved) {
                render_minimap(view_center, heading);
                minimap_focus = view_center;
                minimap_heading = heading;
                minimap_rendered_scale = minimap_scale;
            }
        }
        // WaterSurface animation (FUN_00602fd0): +0x10 += dt*0.03,
        // +0x14 += dt*0.01. Stage 0 uses (-u14,-u10), stage 1 (+u10,+u14),
        // so the two layers drift in opposite directions.
        water_scroll += std::min(static_cast<float>(dt), 0.1f);
        for (DrawItem& item : extra_items) {
            if (item.modulate2x && item.texture2 != 0) {
                item.uv_offset[0] = -0.01f * water_scroll;
                item.uv_offset[1] = -0.03f * water_scroll;
                item.uv2_offset[0] =
                    0.25f / 30.0f + 0.03f * water_scroll;
                item.uv2_offset[1] = 0.01f * water_scroll;
            }
        }
        draw_scene(gpu, texture, extra_items, skybox, program, glfns, eye,
                   view_center, width, height,
                   terrain.world_size > 0.0f ? 80.0f : 33.0f);

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
                              dt > 0.0 ? 1.0 / dt : 0.0);
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
                        frame, 1.0 / (dt > 0.0 ? dt : 1.0), width, height,
                        yaw, pitch, distance);
        }
    }

    if (opt.save_config) {
        int final_width = 0, final_height = 0;
        SDL_GetWindowSize(window, &final_width, &final_height);
        if (!opt.fullscreen && final_width >= 64 && final_height >= 64) {
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
    return 0;
}
