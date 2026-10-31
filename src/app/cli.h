// Command line, config file and runtime options for the port.
//
// These used to live inside main.cpp. Parsing is mechanical and every layer of
// the shell reads the same `Options`, so it is its own unit. No SDL, no GL:
// the options describe the process, not the window.
//
// Precedence: defaults, then `dino_port.cfg`, then argv. `--config FILE` is
// scanned first so the named file still wins over the default one.

#ifndef DINO_APP_CLI_H
#define DINO_APP_CLI_H

#include <string>
#include <vector>

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
    std::string config_path = "dino_port.cfg";
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
    // Dino.ini [GRAPHIC] LODQuality; empty = read it from the asset root.
    std::string lod_quality;
    bool no_lod = false;  // draw every vegetation sector at LOD1 (A/B)
    bool no_cull = false;  // disable the port-only frustum test (A/B)
    // Lay the FUN_0051dad0 access road for a --building Entrance at startup
    // (the Free Game start always lays it).
    bool entrance_road = false;
    // Legacy terrain look: lit, fogged basemap without the detail map.
    bool terrain_lit = false;
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
    std::string actions_bad_info;
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
    std::string dino_dat_path;  // campaign unlock count (default run/save/dino.dat)
    std::string campaign_progress_path;  // current mission per campaign
    bool auto_continue = false;  // test hook: advance past CPGEND automatically
    std::string start_mission_path;
    std::string place_food;
    std::string food_pos;
    bool place_lunchbox = false;
    std::string lunchbox_pos;
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
    // Fixed simulation delta for deterministic headless runs. 0 = wall
    // clock (retail lpScheduler behaviour).
    float fixed_dt = 0.0f;
    // Test accelerator for the mover: multiplies the phenotype walk/run
    // speeds. Retail speed is 1.0; headless frames advance only ~1 ms of
    // simulation each, so a movement test needs a multiplier.
    float dino_move_speed = 1.0f;
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
    int select_paddock = -1;
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
    float sfx_volume = 97.0f;
    // Main-menu Options (Dino.ini settings). Defaults from the retail config:
    // 800x600x16, depth 16, quality AUTO, software TNL/sound, GameSpeed 10.
    int game_speed = 10;
    int menu_lod_quality = 3;  // Main-menu Options: 0 High, 1 Medium, 2 Low, 3 Auto
    int texture_quality = 3;
    int texture_depth = 16;
    int hardware_tnl = 0;
    int hardware_sound = 0;
    std::string warning_text;
    std::string request_text;
    std::string campaign_end_text;
    bool park_hud = true;
    std::string park_hover;
    std::vector<std::string> park_clicks;
    std::vector<std::string> park_window_clicks;
    std::string ground_click;  // scripted scene click, "x,y" window pixels
    std::vector<std::string> inject_clicks;  // "frame,x,y[,right]"
    std::vector<std::string> inject_keys;    // "frame,keycode" (tests)
    std::string road_cursor;  // fixed road preview cursor "x,y" (tests)
    std::string fence_cursor;  // fixed enclosure preview cursor "x,y" (tests)
    std::string skybox_dir;
    bool water = true;
    bool water2 = true;  // WaterSurface stage 1 Modulate2x
    float water_level = -1.0f;
    float water_time = -1.0f;  // initial scroll time (debug/A-B)
    float pulse_time = -1.0f;  // freeze the gscMaterialBank pulse (debug)
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
    bool camera_retail = false;  // force the retail park camera
    bool camera_orbit = false;   // force the legacy orbit camera
    bool camera_selftest = false;
    float camera_hz = 60.0f;     // retail camera update rate (pan is per update)
    int edge_scroll = 2;         // client-pixel edge band (retail = 2)
};


// Reads `key=value` lines. A missing file is not an error (the defaults stay);
// unknown keys and malformed lines are ignored.
void load_config(const std::string& path, Options& opt);

// Writes the subset that survives a restart: window, volumes, quality.
void save_config(const std::string& path, const Options& opt);

// Parses argv: scan --config, load that file, then apply every flag. --help
// prints the usage block and exits 0; a missing value, an unknown argument or
// a window below 64x64 exits 2. Clamps pitch and distance.
Options parse_args(int argc, char** argv);

#endif  // DINO_APP_CLI_H
