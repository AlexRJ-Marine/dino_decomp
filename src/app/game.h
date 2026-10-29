// The port's application object: one process, one window, one park.
//
// This header lists the session state the app shell owns. The data
// mirrors the retail application globals (spPark, spSimuManager, the
// display/view lists) grouped by subsystem; the methods are the
// sections of the old 11k-line main(), each named after what it does.
//
// Keep this file a map: no logic here beyond inline data helpers.

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

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

#include "../formats/anim.h"
#include "../formats/bai.h"
#include "../formats/bbi.h"
#include "../formats/bei.h"
#include "../formats/bfi.h"
#include "../formats/bsi.h"
#include "../formats/bvi.h"
#include "../formats/campaign.h"
#include "../formats/dino_data.h"
#include "../formats/fnt.h"
#include "../formats/general_params.h"
#include "../formats/image.h"
#include "../formats/imp.h"
#include "../formats/lnd.h"
#include "../formats/loc.h"
#include "../formats/market.h"
#include "../formats/mesh.h"
#include "../formats/mpg.h"
#include "../formats/sav.h"
#include "../formats/skel.h"
#include "../formats/skin.h"
#include "../formats/shw.h"
#include "../formats/tga.h"
#include "../formats/wav.h"
#include "../formats/wld.h"
#include "../render/gl_api.h"
#include "../render/mesh_gpu.h"
#include "../render/scene.h"
#include "../render/shaders.h"
#include "../render/skinning.h"
#include "../render/text.h"
#include "../audio/music.h"
#include "../audio/ui_sound.h"
#include "../ui/ini.h"
#include "../ui/menu.h"
#include "../ui/park.h"
#include "../world/build.h"
#include "../world/campaign_objectives.h"
#include "../world/campaign_runtime.h"
#include "../world/danger_grid.h"
#include "../world/dino_actions.h"
#include "../world/economy.h"
#include "../world/employees.h"
#include "../world/food.h"
#include "../world/park_clock.h"
#include "../world/port_save.h"
#include "../world/show_runtime.h"
#include "../world/terrain.h"
#include "../world/terrain_query.h"
#include "../world/vegetation.h"
#include "../world/visitors.h"
#include "asset_paths.h"
#include "camera_config.h"
#include "capture.h"
#include "cli.h"
#include "keymap.h"
#include "park_camera.h"
#include "ui_geometry.h"


namespace dino::app {

class Game {
public:
    Game(int argc, char** argv);
    ~Game();

    // Boot -> frame loop -> shutdown. Returns the process exit code.
    int run();

private:
    // --- nested types --------------------------------------------------
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

    struct StorePreview {
            GpuMesh gpu;
            GLuint texture = 0;
            dino::Vec3 center{};
            float size = 1.0f;
            bool ready = false;
        };

    struct BuildingDraw {
            GpuMesh gpu;
            GLuint texture = 0;
        };

    struct ShowFeatureDraw {
            GpuMesh gpu;
            GLuint texture = 0;
        };

    // --- session state -------------------------------------------------
    Options opt;
    dino::KeyMap keymap;
    std::vector<std::string> island_names;
    std::array<int, 3> free_game_budgets;
    std::array<float, 3> free_game_sellback;
    ParkClock park_clock;
    std::optional<dino::CampaignRuntime> campaign_runtime;
    std::vector<dino::ObjectiveVerdict> last_mission_verdicts;
    std::optional<dino::CampaignMission> pending_next_mission;
    std::string campaign_progress_path;
    std::map<std::string, std::string> campaign_progress;
    std::string mission_panel_title;
    std::string pending_ending_video;
    std::unordered_map<std::string, std::string> menu_sound_files;
    std::vector<std::string> menu_sound_names;
    std::unordered_map<std::string, std::string> park_sound_files;
    std::vector<std::pair<std::string, std::string>> save_slots;
    std::vector<std::pair<std::string, std::string>> menu_save_slots;
    dino::ui::Menu menu;
    bool menu_loaded;
    bool park_return_to_menu;
    bool pending_sell_mode;
    bool park_sell_mode;
    int park_sell_paddock;
    dino::ui::ParkHud park;
    dino::ShowRuntime show_runtime;
    std::vector<dino::ShowCatalogEntry> show_catalog;
    bool show_placements_applied;
    int pending_show_index;
    int attraction_selected_slot;
    bool show_status_printed;
    std::string campaign_dino_dat;
    std::uint32_t campaign_unlock;
    dino::DinoPhenotypeTable phenotype_table;
    std::vector<dino::MarketEntry> store_catalog;
    int pending_store_index;
    int pending_store_price;
    int pending_minimap_zoom;
    bool pending_port_save;
    bool pending_port_load;
    std::string pending_menu_load_path;
    std::string pending_menu_cinematic;
    std::string pending_construct_widget;
    bool pending_fence_tool;
    int fence_corner_x;
    int fence_corner_y;
    std::string pending_store_action;
    std::string pending_show_action;
    std::vector<std::string> pending_attraction_actions;
    dino::Mesh mesh;
    dino::Image image;
    dino::Image detail_image;
    dino::Image dino_image;
    dino::Skeleton skeleton;
    dino::Skin skin;
    dino::Animation anim;
    dino::Wav wav;
    dino::Lnd lnd;
    dino::Terrain terrain;
    dino::Vegetation vegetation;
    float lod_quality;
    bool veg_stats;
    dino::BuildGrid build_grid;
    dino::Mesh fence_source;
    dino::Image path_image;
    dino::Image fence_image;
    dino::Image fence_laser_image;
    dino::Image paddock_dirt_image;
    dino::Image paddock_occup_image;
    std::array<dino::Image, 6> sky_images;
    dino::Image water_image;
    dino::Image water2_image;
    float water_z;
    float water_scroll;
    dino::VisitorSystem visitor_system;
    std::vector<dino::VisitorProfile> visitor_profiles;
    std::vector<dino::VisitorProfile> all_visitor_profiles;
    std::vector<PortShowSpec> port_shows;
    dino::Mesh visitor_pose1;
    dino::Mesh visitor_pose2;
    dino::Image visitor_image;
    dino::Fnt hud_font;
    bool hud_font_ready;
    bool build_ready;
    Uint32 init_flags;
    UiSoundPlayer ui_sound;
    MusicPlayer music;
    SDL_WindowFlags window_flags;
    SDL_Window* window;
    SDL_GLContext context;
    GlFns glfns;
    GLuint program;
    SDL_AudioStream* audio;
    const char* vendor;
    const char* renderer;
    const char* version;
    bool skinned;
    std::vector<int> vertex_bone;
    std::vector<int> bone_skel;
    GLuint texture;
    GLuint detail_texture;
    GLuint dino_texture;
    GLuint hud_font_texture;
    bool hud_visible;
    GpuMesh hud_gpu;
    GpuMesh menu_gpu;
    std::unordered_map<std::string, UiTexture> ui_textures;
    std::unordered_map<std::string, dino::Fnt> ui_fonts;
    std::unordered_map<std::string, GLuint> ui_font_atlases;
    bool place_dino;
    float spawn_x;
    float spawn_y;
    dino::Mesh display;
    GpuMesh gpu;
    GpuMesh dino_gpu;
    GpuMesh shadow_gpu;
    std::map<std::string, std::shared_ptr<ImportedSpecies>> imported_species;
    std::vector<ImportedDino> imported_dinos;
    dino::DangerGrid danger_grid;
    double danger_timer;
    std::vector<dino::FoodSource> food_sources;
    std::vector<dino::Paddock> park_paddocks;
    std::vector<dino::Paddock> paddocks;
    dino::DinoFamilyTable family_table;
    std::map<int, std::string> head_species;
    std::map<std::string, int> species_size_cm;
    std::function<int(const std::string&)> size_for_species;
    std::vector<dino::BaiAction> dino_action_table;
    std::array<int, 4> dino_initial_needs;
    std::map<std::string, StorePreview> store_previews;
    Bounds bounds;
    std::vector<GpuMesh> veg_gpu;
    std::vector<GLuint> veg_textures;
    std::vector<unsigned char> veg_visible;
    std::vector<std::array<float, 4>> veg_bounds;
    GpuMesh path_gpu;
    GpuMesh fence_gpu;
    GpuMesh road_preview_gpu;
    bool road_preview_ok;
    GLuint road_preview_texture;
    dino::Image road_helper_image;
    dino::Mesh road_helper_box;
    GLuint path_texture;
    GLuint fence_texture;
    GLuint fence_laser_texture;
    GLuint paddock_dirt_texture;
    GLuint paddock_occup_texture;
    GpuMesh fence_beams_gpu;
    GpuMesh fence_ground_gpu;
    GpuMesh fence_selected_gpu;
    double park_pulse_time;
    bool park_pulse_started;
    std::array<float, 3> lamp_tint;
    std::array<float, 3> selected_tint;
    int selected_paddock;
    GpuMesh fence_preview_gpu;
    bool fence_preview_visible;
    bool fence_preview_ok;
    int fence_preview_cell_x;
    int fence_preview_cell_y;
    int fence_preview_cost;
    GpuMesh fence_template_gpu;
    dino::Mesh fence_sphere_source;
    bool fence_template_visible;
    bool fence_template_ok;
    std::array<int, 6> fence_template_key;
    GLuint fence_preview_ok_texture;
    GLuint fence_preview_bad_texture;
    GLuint fence_template_ok_texture;
    GLuint fence_template_bad_texture;
    SkyboxGL skybox;
    int sky_faces;
    GLuint water_texture;
    GLuint water_texture2;
    GpuMesh water_gpu;
    GpuMesh visitor_gpu;
    GLuint visitor_texture_id;
    GpuMesh employee_gpu;
    GLuint employee_texture_id;
    dino::Mesh employee_pose;
    dino::Image employee_image;
    std::vector<BuildingDraw> building_draws;
    std::vector<dino::PortBuildingState> building_instances;
    std::map<int, dino::BuildingDef> building_defs;
    std::vector<dino::WorldRect> wld_roads;
    std::vector<dino::WorldRect> wld_fences;
    std::vector<dino::WorldDinosaur> wld_dinosaurs;
    dino::SavSimulation sav_simulation;
    std::vector<dino::WorldFence> sav_fences;
    std::vector<dino::WorldBuilding> sav_buildings;
    std::vector<dino::WorldVisitor> sav_world_visitors;
    bool sav_ready;
    std::vector<dino::Attraction> attractions;
    int entrance_cell_x;
    int entrance_cell_y;
    std::optional<dino::WorldRect> entrance_road;
    dino::Vec3 building_center;
    MinimapTarget minimap;
    bool minimap_ready;
    float minimap_scale;
    bool minimap_zoom_in;
    bool minimap_zoom_out;
    bool nav_pan_forward;
    bool nav_pan_back;
    bool nav_pan_left;
    bool nav_pan_right;
    bool nav_turn_left;
    bool nav_turn_right;
    bool nav_zoom_in;
    bool nav_zoom_out;
    std::vector<DrawItem> extra_items;
    std::vector<dino::FoodDef> food_defs;
    GpuMesh food_gpu;
    GLuint food_texture;
    std::vector<ShowFeatureDraw> show_feature_draws;
    std::vector<dino::EmployeeProfile> employee_profiles;
    dino::CameraConfig camera_config;
    dino::BudgetLedger budget_ledger;
    int pending_buy_price;
    static constexpr float kPi = 3.14159265f;
    int build_tool;
    int pending_construct_kind;
    bool pending_click;
    float click_x;
    float click_y;
    float drag_distance;
    dino::Vec3 center;
    float auto_distance;
    float distance;
    float yaw;
    float pitch;
    float camera_progress;
    float camera_yaw_goal;
    float camera_accum;
    int cam_mouse_x;
    int cam_mouse_y;
    bool cam_mouse_valid;
    RetailParkCamera retail_camera;
    bool explicit_camera_view;
    bool camera_retail_model;
    bool free_camera;
    dino::Vec3 free_pos;
    float free_yaw;
    float free_pitch;
    int park_hover_x;
    int park_hover_y;
    int park_active_index;
    int park_budget;
    float park_sellback;
    dino::EmployeeSystem employee_system;
    std::vector<std::string> construction_names;
    std::vector<dino::BuildingDef> construction_defs;
    std::uint32_t road_price;
    bool road_price_loaded;
    bool road_has_start;
    int road_start_x;
    int road_start_y;
    std::array<int, 4> road_preview_key;
    int road_preview_cost;
    std::string selected_construct;
    int park_layout_w;
    int park_layout_h;
    dino::Vec3 minimap_focus;
    float minimap_heading;
    float minimap_rendered_scale;
    std::string current_island;
    bool running;
    bool start_screen;
    bool show_help;
    bool dragging;
    bool ui_click_consumed;
    float last_mx;
    float last_my;
    int frame;
    std::uint64_t freq;
    std::uint64_t last;
    double elapsed;
    // Frame delta in seconds; the retail scheduler's per-frame tick.
    double frame_dt;
    int width;
    int height;
    int logical_width;
    int logical_height;
    bool menu_hover_forced;
    bool park_hover_forced;

    // Per-frame camera rig: eye and look-at target shared by terrain
    // picking, the minimap and the scene draw.
    dino::Vec3 eye;
    dino::Vec3 view_center;

    // --- lifecycle -----------------------------------------------------
    std::optional<int> boot();
    // Release the GL/SDL resources and write the end-of-run output.
    void shutdown();

    // Boot phases, in the order boot() calls them.
    std::optional<int> boot_command_line();
    std::optional<int> boot_budgets_and_clock();
    std::optional<int> boot_campaign_and_saves();
    std::optional<int> boot_menu_state();
    std::optional<int> boot_menu_overlay();
    std::optional<int> boot_demo_defaults();
    std::optional<int> boot_load_assets();
    std::optional<int> boot_build_textures();
    std::optional<int> boot_print_modes();
    std::optional<int> boot_platform();
    std::optional<int> boot_scene_resources();
    std::optional<int> boot_park_objects();
    std::optional<int> boot_minimap_and_visitors();
    std::optional<int> boot_employee_profiles();
    std::optional<int> boot_import_dinos();
    std::optional<int> boot_food_definitions();
    std::optional<int> boot_camera_setup();
    std::optional<int> boot_park_session();
    std::optional<int> boot_employee_session();
    std::optional<int> boot_build_tools();
    std::optional<int> boot_finalize();

    // --- frame loop ----------------------------------------------------
    // One call per phase, in frame order; see run().
    void handle_input();
    void begin_frame();
    void advance_time();
    void update_session();
    void update_audio();
    void update_camera();
    void update_diagnostics();
    void set_background();
    void update_simulation();
    void update_camera_view();
    void update_placement();
    void update_preview();
    void draw_frame();
    void present_frame();

    // Frame-phase details (game.cpp): called from the phase above at the
    // same point in the frame, so the phase name says what runs.
    void update_minimap_zoom();
    void refill_looping_stream();

    // --- UI drawing (game_ui.cpp) --------------------------------------
    UiTexture* ui_texture(const std::string& filename);
    const dino::Fnt* ui_font(const std::string& path, int text_height,
                             float& scale_out, GLuint& atlas_out);
    void draw_ui_mesh(GpuMesh& target, const dino::Mesh& geometry,
                      GLuint texture_id, float alpha, int unlit, int view_w,
                      int view_h);
    void draw_menu(int view_w, int view_h, int logical_w, int logical_h);
    void draw_menu_sprite(const std::string& name, float x, float y, float w,
                          float h, int sprite_frame, int view_w, int view_h);
    void attraction_row_text(int slot, std::string& name, int& iterations,
                             int& locked);
    void refresh_fence_info_panel();
    void draw_park_hud(int view_w, int view_h, int logical_w, int logical_h);
    void activate_main_menu();
    void park_design(float window_x, float window_y, int& design_x,
                     int& design_y);
    void sync_park_layout();

    // --- menu and campaign (game_menu.cpp) -----------------------------
    void start_campaign_mission(const dino::CampaignMission& mission);
    void queue_menu_load();
    void queue_menu_cinematic();
    void update_campaign_info_lines();
    void apply_menu_options();
    bool play_video(const std::string& path, int start_frame, int max_frames);
    void play_pending_cinematic();
    void advance_campaign();

    // --- shows and attractions (game_show.cpp) -------------------------
    void sync_show_identity(dino::CampaignShowState& show,
                            const std::vector<int>& participants);
    void fill_show_scores(dino::CampaignShowState& show,
                          const std::vector<int>& participants);
    std::vector<int> show_profile_ids();
    void apply_show_specs();
    void auto_assign_show_participants(int show_index);
    bool ends_with(const std::string& text, const std::string& suffix);
    void handle_show_place(int catalog_index);
    void place_show_at(int rx, int ry);
    int find_show_by_slot(int slot);
    void handle_attraction_action(const std::string& action);

    // --- park lifecycle and save (game_park.cpp) -----------------------
    void import_park_layout();
    void load_island(const std::string& island, bool reset_view);
    dino::PortState make_port_state();
    void load_campaign_park(const std::string& world_path);
    void apply_port_state(const dino::PortState& state);
    std::string save_name_sanitized();
    std::string port_save_target(bool for_load);
    void do_port_save(const std::string& path);
    void do_port_load(const std::string& path);

    // --- construction, roads and store (game_build.cpp) ----------------
    std::array<int, 11> store_row_attributes(const dino::MarketEntry& entry);
    void refresh_store_rows();
    void apply_store_catalog(dino::Market market, const std::string& source);
    StorePreview* store_preview_for(const dino::ui::StoreRow& row);
    void clear_buildings();
    bool place_building(const dino::BuildingDef& def, float cell_x, float cell_y,
                        int state, float extra_angle_deg, float scale,
                        int kind);
    int place_world_buildings(const dino::WldWorld& world);
    void rebuild_build_meshes();
    void sell_paddock_now(int index);
    void ensure_construction_catalog();
    int construction_kind_for_widget(const std::string& widget);
    int construction_kind_for_name(const std::string& name);
    bool construction_overlaps(int kind, int cell_x, int cell_y, int state);
    bool try_construct(int kind, int cell_x, int cell_y, int state);
    std::uint32_t ensure_road_price();
    void road_message(const char* id);
    bool road_cell_busy(int x, int y);
    std::vector<dino::WorldRect> road_perimeters();
    void arm_road_tool();
    void road_click(int cx, int cy);
    void handle_store_buy(int index);
    void run_daily_settlement();
    void ensure_build_ready();

    // --- scene and park contents (game_scene.cpp) ----------------------
    dino::Mesh pose_dino(double time);
    std::shared_ptr<ImportedSpecies> load_imported_species(const std::string& species,
                                                           const std::string& family,
                                                           const std::string& clip);
    bool add_imported_dino(const std::string& species, const std::string& family,
                           const std::string& clip, float x, float y,
                           float angle,
                           const dino::DinoAttributes* attributes = nullptr);
    void clear_imported_dinos();
    void upload_vegetation();
    void release_vegetation();
    const dino::WorldRect* selected_paddock_rect();
    int paddock_at_map(float mx, float my);
    void minimap_click_zoom(int delta);
    void render_minimap(const dino::Vec3& focus, float heading_rad);
    void rebuild_extra_items();
    void refresh_terrain_texture();
    void load_dino_family_tables();
    void spawn_world_dinos();
    void print_paddock_totals();
    int paddock_at(float x, float y);
    dino::FoodSource* paddock_food(int paddock, bool carnivore);
    void log_dino_food();
    float camera_radius();
    float camera_height();
    dino::Vec3 orbit_eye();
    void enter_free_camera();

};

}  // namespace dino::app
