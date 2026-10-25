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
#include "../world/terrain.h"
#include "../world/vegetation.h"
#include "../world/build.h"
#include "../world/visitors.h"
#include "../ui/menu.h"
#include "keymap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
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
    bool park_hud = true;
    std::string skybox_dir;
    bool water = true;
    float water_level = -1.0f;
    int visitors = 0;
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
        } else if (arg == "--no-park-hud") {
            opt.park_hud = false;
        } else if (arg == "--no-water") {
            opt.water = false;
        } else if (arg == "--water-level") {
            opt.water_level = static_cast<float>(std::atof(next("--water-level")));
        } else if (arg == "--skybox") {
            opt.skybox_dir = next("--skybox");
        } else if (arg == "--visitors") {
            opt.visitors = std::atoi(next("--visitors"));
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
                "  --no-park-hud        skip the ParkInterface.Ini HUD\n"
                "  --no-water           skip the water plane\n"
                "  --water-level N      override the .lnd water level\n"
                "  --skybox DIR         skybox face directory (front/back/left/right/top/bottom.PNG)\n"
                "  --visitors N         spawn N visitors on the placed path network\n"
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
    PFNGLUNIFORM1FPROC Uniform1f = nullptr;
    PFNGLUNIFORM1IPROC Uniform1i = nullptr;
    PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;

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
        LOAD(Uniform1f, "glUniform1f")
        LOAD(Uniform1i, "glUniform1i")
        LOAD(ActiveTexture, "glActiveTexture")
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
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    v_normal = a_normal;
    v_uv = a_uv;
    v_color = a_color;
}
)GLSL";
    static const char* kFragment = R"GLSL(
#version 330 core
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;
uniform sampler2D u_texture;
uniform int u_use_texture;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform float u_alpha;
uniform int u_unlit;
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
    if (u_unlit == 2) {
        frag_color = vec4(v_color.rgb, texel.r * u_alpha * v_color.a);
        return;
    }
    vec3 color = base;
    if (u_unlit == 0) {
        color = base * (u_ambient + u_light_color * (key + fill));
        color = pow(clamp(color, 0.0, 1.0), vec3(0.85));
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
    switch (index) {
        case 0:  // front (+Y)
            mesh.positions = {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}};
            normal = {0, -1, 0};
            break;
        case 1:  // back (-Y)
            mesh.positions = {{h, -h, h}, {-h, -h, h}, {-h, -h, -h}, {h, -h, -h}};
            normal = {0, 1, 0};
            break;
        case 2:  // left (-X)
            mesh.positions = {{-h, h, h}, {-h, -h, h}, {-h, -h, -h}, {-h, h, -h}};
            normal = {1, 0, 0};
            break;
        case 3:  // right (+X)
            mesh.positions = {{h, -h, h}, {h, h, h}, {h, h, -h}, {h, -h, -h}};
            normal = {-1, 0, 0};
            break;
        case 4:  // top (+Z)
            mesh.positions = {{-h, h, h}, {h, h, h}, {h, -h, h}, {-h, -h, h}};
            normal = {0, 0, -1};
            break;
        default:  // bottom (-Z)
            mesh.positions = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h}};
            normal = {0, 0, 1};
            break;
    }
    mesh.normals.assign(4, normal);
    mesh.uvs = {{0.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    mesh.indices = {0, 1, 2, 0, 2, 3};
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
};

void draw_scene(const GpuMesh& mesh, GLuint texture,
                const std::vector<DrawItem>& extras,
                const SkyboxGL& skybox,
                GLuint program,
                const GlFns& gl, const dino::Vec3& eye, const dino::Vec3& center,
                int width, int height) {
    const dino::Mat4 proj = perspective(33.0f,
                                        static_cast<float>(width) / std::max(1, height),
                                        0.1f, 10000.0f);
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
    gl.Uniform1f(gl.GetUniformLocation(program, "u_alpha"), 1.0f);

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
    for (const DrawItem& item : extras) {
        if (item.mesh == nullptr || item.mesh->vao == 0) {
            continue;
        }
        gl.Uniform1i(use_texture_loc, item.texture != 0 ? 1 : 0);
        gl.Uniform1f(alpha_loc, item.alpha);
        const bool blend = item.alpha < 1.0f;
        if (blend) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }
        if (item.texture != 0) {
            glBindTexture(GL_TEXTURE_2D, item.texture);
        }
        gl.BindVertexArray(item.mesh->vao);
        glDrawElements(GL_TRIANGLES, item.mesh->index_count, GL_UNSIGNED_INT, nullptr);
        if (blend) {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
    }
    gl.BindVertexArray(0);
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
                              const std::vector<int>& bone_skel) {
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
    std::vector<dino::Mat4> skin_matrices(skin.bones.size());
    for (std::size_t i = 0; i < skin.bones.size(); ++i) {
        const int skel_index = bone_skel[i];
        skin_matrices[i] = skel_index >= 0
            ? dino::multiply(skin.bones[i].inverse_bind, raw[static_cast<std::size_t>(skel_index)])
            : dino::identity();
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

    dino::Terrain terrain;
    terrain.width = 16;
    terrain.height = 16;
    terrain.world_size = 1024.0f;
    terrain.height_scale = 1.0f;
    terrain.heights.assign(16 * 16, 0);

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

    if (failures == 0) {
        std::printf("selftest: PASS (%d checks)\n", checks);
    }
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt = parse_args(argc, argv);

    if (opt.selftest) {
        return run_selftest();
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
    if (opt.list_keys) {
        dino::print_keymap(keymap);
        return 0;
    }

    dino::ui::Menu menu;
    bool menu_loaded = false;
    dino::ui::Overlay park_overlay;
    dino::Loc park_loc;
    bool park_loaded = false;
    if (opt.use_menu) {
        menu_loaded = menu.load(opt.asset_root);
        if (!menu_loaded) {
            std::fprintf(stderr, "menu: %s (falling back to placeholder)\n",
                         menu.error.c_str());
        }
    }

    if (opt.park_hud && !opt.lnd_path.empty()) {
        std::string park_error;
        park_loaded = park_overlay.load_park(
            resolve_asset(opt.asset_root, "Data/Overlay/ParkInterface.Ini"),
            resolve_asset(opt.asset_root, "Data/Overlay/ParkGraphicDB.Ini"),
            park_error);
        if (park_loaded) {
            try {
                park_loc = dino::load_loc(
                    resolve_asset(opt.asset_root, "Data/Overlay/ParkEng.loc"));
            } catch (const std::exception& e) {
                std::fprintf(stderr, "park loc: %s\n", e.what());
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
    float water_z = 0.0f;
    dino::VisitorSystem visitor_system;
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
                resolve_asset(opt.asset_root, lnd.heightmap), hm_width, hm_height);
            terrain = dino::build_terrain(heights, hm_width, hm_height,
                                          opt.terrain_size, opt.height_scale);
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
                        build_grid.set_path(x, y, true);
                    } else {
                        std::fprintf(stderr, "bad --place-path %s\n", value.c_str());
                    }
                }
                for (const auto& value : opt.place_fences) {
                    int x = 0, y = 0, edge = 0;
                    if (std::sscanf(value.c_str(), "%d,%d,%d", &x, &y, &edge) == 3) {
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

                if (opt.visitors > 0) {
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
                    const int spawned = visitor_system.spawn(build_grid, opt.visitors);
                    std::printf("visitors: %d spawned on %d path cells\n", spawned,
                                build_grid.path_count);
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
            for (int i = 0; i < static_cast<int>(park_overlay.widgets.size()); ++i) {
                if (park_overlay.widgets[static_cast<std::size_t>(i)].name !=
                        "RENDERVIEW" &&
                    park_overlay.visible_in_tree(i)) {
                    ++visible;
                }
            }
            std::printf("park hud: loaded=%d widgets=%zu roots=%zu visible=%d\n",
                        park_loaded ? 1 : 0, park_overlay.widgets.size(),
                        park_overlay.roots.size(), visible);
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
                                                       nullptr, 0.0, vertex_bone, bone_skel);
            const dino::Mesh pose = build_skinned_mesh(skin.mesh, skin, skeleton,
                                                       anim_ptr, t, vertex_bone, bone_skel);
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
    if (!opt.wav_path.empty()) {
        init_flags |= SDL_INIT_AUDIO;
    }
    if (!SDL_Init(init_flags)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
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

    const GLuint texture = image.width > 0 ? upload_texture(image) : 0;
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
                const int rows = std::min(
                    10, line_h > 0 ? widget.height / line_h : 0);
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
                for (int row = 0; row < rows; ++row) {
                    const float row_y =
                        py(widget.abs_y) + static_cast<float>(row) * line_h * dpi;
                    if (row_texture != nullptr && row_graphic != nullptr) {
                        const int frame =
                            row == menu.island ? selected_frame : normal_frame;
                        const UiSourceRect source =
                            ui_frame_rect(*row_texture, *row_graphic, frame);
                        if (source.valid) {
                            const float u0 =
                                static_cast<float>(source.x) / row_texture->width;
                            const float v0 =
                                static_cast<float>(source.y) / row_texture->height;
                            const float u1 = static_cast<float>(source.x + source.w) /
                                             row_texture->width;
                            const float v1 = static_cast<float>(source.y + source.h) /
                                             row_texture->height;
                            draw_ui_mesh(
                                menu_gpu,
                                build_ui_quad(px(widget.abs_x), row_y,
                                              static_cast<float>(row_w) * dpi,
                                              static_cast<float>(line_h) * dpi, u0,
                                              v0, u1, v1, {255, 255, 255, 255}),
                                row_texture->id, 1.0f, 1, view_w, view_h);
                        }
                    }
                    if (list_font != nullptr) {
                        char field[32];
                        std::snprintf(field, sizeof(field), "&Isl%02d_Name", row + 1);
                        const std::string name =
                            dino::ui::resolve_loc_text(field, menu.loc);
                        const dino::Mesh block = build_text_block(
                            *list_font, name, px(widget.abs_x), row_y,
                            list_scale * dpi, widget.text_color, list_justify,
                            static_cast<float>(row_w) * dpi,
                            static_cast<float>(line_h) * dpi);
                        draw_ui_mesh(menu_gpu, block, list_atlas, 1.0f, 2, view_w,
                                     view_h);
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
                        free_height * (static_cast<float>(menu.island) / 9.0f);
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
    const bool place_dino = skinned && !opt.lnd_path.empty();
    const float spawn_x = opt.spawn_x >= 0.0f ? opt.spawn_x : terrain.world_size * 0.5f;
    const float spawn_y = opt.spawn_y >= 0.0f ? opt.spawn_y : terrain.world_size * 0.5f;

    auto pose_dino = [&](double time) {
        const dino::Animation* anim_ptr = anim.bones.empty() ? nullptr : &anim;
        dino::Mesh pose = build_skinned_mesh(skin.mesh, skin, skeleton, anim_ptr, time,
                                             vertex_bone, bone_skel);
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
                                     vertex_bone, bone_skel);
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

    SkyboxGL skybox;
    int sky_faces = 0;
    for (int i = 0; i < 6; ++i) {
        if (sky_images[i].width <= 0) {
            continue;
        }
        skybox.faces[i] = upload_mesh(make_sky_face(i, 5000.0f), glfns);
        skybox.textures[i] = upload_texture(sky_images[i]);
        if (skybox.faces[i].vao != 0 && skybox.textures[i] != 0) {
            ++sky_faces;
        }
    }
    skybox.ready = sky_faces == 6;

    GLuint water_texture = 0;
    GpuMesh water_gpu;
    if (water_image.width > 0 && terrain.world_size > 0.0f) {
        water_texture = upload_texture_wrapped(water_image, GL_REPEAT);
        water_gpu = upload_mesh(
            make_water_plane(terrain.world_size * 6.0f, water_z, 48.0f), glfns);
    }

    GpuMesh visitor_gpu;
    GLuint visitor_texture_id = 0;
    if (!visitor_system.visitors.empty()) {
        visitor_texture_id = upload_texture(visitor_image);
        visitor_gpu = upload_mesh(
            visitor_system.build_mesh(build_grid, terrain, visitor_pose1,
                                      visitor_pose2),
            glfns);
    }

    std::vector<DrawItem> extra_items;
    auto rebuild_extra_items = [&]() {
        extra_items.clear();
        for (std::size_t i = 0; i < veg_gpu.size(); ++i) {
            extra_items.push_back({&veg_gpu[i], veg_textures[i]});
        }
        if (path_gpu.vao != 0) {
            extra_items.push_back({&path_gpu, path_texture});
        }
        if (fence_gpu.vao != 0) {
            extra_items.push_back({&fence_gpu, fence_texture});
        }
        if (visitor_gpu.vao != 0) {
            extra_items.push_back({&visitor_gpu, visitor_texture_id});
        }
        if (dino_gpu.vao != 0) {
            extra_items.push_back({&dino_gpu, dino_texture});
            if (shadow_gpu.vao != 0) {
                extra_items.push_back({&shadow_gpu, 0, 0.35f});
            }
        }
        // Transparent pass: water last so it blends over all opaque actors.
        if (water_gpu.vao != 0) {
            extra_items.push_back({&water_gpu, water_texture, 0.78f});
        }
    };
    rebuild_extra_items();

    auto rebuild_build_meshes = [&]() {
        if (!build_ready) {
            return;
        }
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

    int build_tool = opt.build_mode ? 1 : 0;
    bool pending_click = false;
    float click_x = 0.0f;
    float click_y = 0.0f;
    float drag_distance = 0.0f;

    const dino::Vec3 center = place_dino
        ? dino::Vec3{spawn_x - terrain.world_size * 0.5f,
                     spawn_y - terrain.world_size * 0.5f,
                     terrain.height_at(spawn_x - terrain.world_size * 0.5f,
                                       spawn_y - terrain.world_size * 0.5f) + 3.0f}
        : bounds.center();

    const float auto_distance = place_dino ? 22.0f
        : std::max(2.0f, bounds.extent() * 1.7f);
    float distance = opt.distance > 0.0f ? opt.distance : auto_distance;
    float yaw = opt.yaw;
    float pitch = opt.pitch;

    auto orbit_eye = [&]() {
        const float cp = std::cos(pitch * 3.14159265f / 180.0f);
        const float sp = std::sin(pitch * 3.14159265f / 180.0f);
        const float cy = std::cos(yaw * 3.14159265f / 180.0f);
        const float sy = std::sin(yaw * 3.14159265f / 180.0f);
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

    auto draw_park_hud = [&](int view_w, int view_h, int logical_w, int logical_h) {
        (void)logical_h;
        if (!park_loaded) {
            return;
        }
        const float dpi = logical_w > 0
                              ? static_cast<float>(view_w) / static_cast<float>(logical_w)
                              : 1.0f;
        const float ox = (static_cast<float>(view_w) - 800.0f * dpi) * 0.5f;
        const float oy = (static_cast<float>(view_h) - 600.0f * dpi) * 0.5f;
        auto px = [&](int value) { return ox + static_cast<float>(value) * dpi; };
        auto py = [&](int value) { return oy + static_cast<float>(value) * dpi; };

        std::vector<int> order;
        for (int i = 0; i < static_cast<int>(park_overlay.widgets.size()); ++i) {
            if (park_overlay.widgets[static_cast<std::size_t>(i)].name ==
                    "RENDERVIEW" ||
                !park_overlay.visible_in_tree(i)) {
                continue;
            }
            order.push_back(i);
        }
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            const auto& wa = park_overlay.widgets[static_cast<std::size_t>(a)];
            const auto& wb = park_overlay.widgets[static_cast<std::size_t>(b)];
            if (wa.priority != wb.priority) {
                return wa.priority < wb.priority;
            }
            if (wa.zpriority != wb.zpriority) {
                return wa.zpriority < wb.zpriority;
            }
            return a < b;
        });

        auto runtime_text = [&](const std::string& name) -> std::string {
            if (name == "BUDGET_AMOUNT") {
                return "76050";
            }
            if (name == "VISITOR_AMOUNT") {
                return std::to_string(visitor_system.visitors.size());
            }
            if (name == "DINO_AMOUNT") {
                return place_dino ? "1" : "0";
            }
            if (name == "SHOW_AMOUNT") {
                return "0";
            }
            if (name == "PADDOCK_AMOUNT") {
                return "0";
            }
            if (name == "PARK_CURRENT_TIME") {
                return "09:44";
            }
            if (name == "PARK_CURRENT_DAY") {
                return "1";
            }
            return "";
        };

        for (const int i : order) {
            const dino::ui::Widget& widget =
                park_overlay.widgets[static_cast<std::size_t>(i)];
            if (!widget.texture.empty()) {
                if (const dino::ui::Graphic* graphic =
                        park_overlay.graphic(widget.texture)) {
                    if (UiTexture* texture = ui_texture(graphic->filename)) {
                        const int frame = graphic->objects <= 1 ? 0 : 4;
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

            std::string text = runtime_text(widget.name);
            if (text.empty()) {
                text = dino::ui::resolve_loc_text(widget.text, park_loc);
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
    };

    bool running = true;
    bool start_screen = opt.force_menu ||
        (opt.frames == 0 && opt.screenshot.empty() && !opt.no_menu);
    if (menu_loaded && opt.use_menu && !menu.active) {
        start_screen = false;
    }
    bool show_help = opt.force_help;
    bool dragging = false;
    float last_mx = 0.0f, last_my = 0.0f;
    int frame = 0;
    const std::uint64_t freq = SDL_GetPerformanceFrequency();
    std::uint64_t last = SDL_GetPerformanceCounter();
    double elapsed = 0.0;
    int width = opt.width;
    int height = opt.height;
    int logical_width = opt.width;
    int logical_height = opt.height;

    while (running) {
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
                        menu.move(static_cast<int>(event.motion.x - off_x),
                                  static_cast<int>(event.motion.y - off_y));
                    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                               event.button.button == SDL_BUTTON_LEFT) {
                        menu.click(static_cast<int>(event.button.x - off_x),
                                   static_cast<int>(event.button.y - off_y));
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
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                const SDL_Keycode key = event.key.key;
                if (key == keymap[dino::KeyAction::Quit]) {
                    running = false;
                } else if (key == keymap[dino::KeyAction::OrbitLeft]) {
                    yaw -= 4.0f;
                } else if (key == keymap[dino::KeyAction::OrbitRight]) {
                    yaw += 4.0f;
                } else if (key == keymap[dino::KeyAction::OrbitUp]) {
                    pitch = std::min(89.0f, pitch + 4.0f);
                } else if (key == keymap[dino::KeyAction::OrbitDown]) {
                    pitch = std::max(-89.0f, pitch - 4.0f);
                } else if (key == keymap[dino::KeyAction::ZoomIn]) {
                    distance = std::max(1.0f, distance * 0.9f);
                } else if (key == keymap[dino::KeyAction::ZoomOut]) {
                    distance = std::min(500.0f, distance * 1.1f);
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
                    std::printf("build tool: off\n");
                } else if (key == SDLK_U) {
                    if (build_grid.undo()) {
                        rebuild_build_meshes();
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                       event.button.button == SDL_BUTTON_LEFT) {
                dragging = true;
                drag_distance = 0.0f;
                last_mx = event.button.x;
                last_my = event.button.y;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                       event.button.button == SDL_BUTTON_LEFT) {
                dragging = false;
                if (build_tool != 0 && drag_distance < 5.0f) {
                    pending_click = true;
                    click_x = event.button.x;
                    click_y = event.button.y;
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION && dragging) {
                const float dx = event.motion.x - last_mx;
                const float dy = event.motion.y - last_my;
                drag_distance += std::abs(dx) + std::abs(dy);
                if (free_camera) {
                    free_yaw += dx * 0.3f;
                    free_pitch = std::clamp(free_pitch - dy * 0.3f, -89.0f, 89.0f);
                } else {
                    yaw += dx * 0.4f;
                    pitch = std::clamp(pitch + dy * 0.4f, -89.0f, 89.0f);
                }
                last_mx = event.motion.x;
                last_my = event.motion.y;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                distance = std::clamp(distance * std::pow(1.1f, -event.wheel.y), 1.0f, 500.0f);
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                width = event.window.data1;
                height = event.window.data2;
            }
        }
        if (!running) {
            break;
        }

        SDL_GetWindowSizeInPixels(window, &width, &height);
        SDL_GetWindowSize(window, &logical_width, &logical_height);
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
            }
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
                                                     vertex_bone, bone_skel);
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

        if (!visitor_system.visitors.empty()) {
            visitor_system.update(build_grid, terrain, static_cast<float>(dt));
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

        if (pending_click) {
            pending_click = false;
            if (build_ready && build_tool != 0) {                dino::Vec3 hit;
                if (pick_terrain(terrain, eye, view_center, click_x, click_y,
                                 logical_width, logical_height, hit)) {
                    int cx = 0;
                    int cy = 0;
                    if (build_grid.cells_for_world(hit.x, hit.y, cx, cy)) {
                        const int before = build_grid.path_count + build_grid.fence_count;
                        if (build_tool == 1) {
                            build_grid.set_path(cx, cy, !build_grid.path_at(cx, cy));
                        } else {
                            const float cs = build_grid.cell_size;
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
                            build_grid.set_fence(ex, ey, edge,
                                                 !build_grid.fence_at(ex, ey, edge));
                        }
                        if (build_grid.path_count + build_grid.fence_count != before) {
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

        draw_scene(gpu, texture, extra_items, skybox, program, glfns, eye,
                   view_center, width, height);

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
            if (park_loaded) {
                draw_park_hud(width, height, logical_width, logical_height);
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
                const std::string text = std::string(line1) + "\n" + line2;
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
                            "WASD move    C camera    Shift sprint\n"
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
    for (auto& group : veg_gpu) {
        group.destroy(glfns);
    }
    dino_gpu.destroy(glfns);
    shadow_gpu.destroy(glfns);
    path_gpu.destroy(glfns);
    fence_gpu.destroy(glfns);
    water_gpu.destroy(glfns);
    visitor_gpu.destroy(glfns);
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
    return 0;
}
