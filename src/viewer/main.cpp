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
#include "keymap.h"

#include <algorithm>
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
out vec4 frag_color;
void main() {
    vec3 n = normalize(v_normal);
    if (!gl_FrontFacing) n = -n;
    float key = max(dot(n, u_light_dir), 0.0);
    float fill = max(dot(n, -u_light_dir), 0.0) * 0.25;
    vec3 base = v_color.rgb;
    if (u_use_texture != 0) {
        base *= texture(u_texture, v_uv).rgb;
    }
    vec3 color = base * (u_ambient + u_light_color * (key + fill));
    color = pow(clamp(color, 0.0, 1.0), vec3(0.85));
    frag_color = vec4(color, 1.0);
}
)GLSL";
    const GLuint vs = compile_shader(GL_VERTEX_SHADER, kVertex, gl);
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kFragment, gl);
    if (!vs || !fs) {
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

GLuint upload_texture(const dino::Image& image) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    return id;
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

void draw_scene(const GpuMesh& mesh, GLuint texture, GLuint program,
                const GlFns& gl, const Bounds& bounds, float yaw, float pitch,
                float distance, int width, int height) {
    const dino::Vec3 center = bounds.center();
    const float cp = std::cos(pitch * 3.14159265f / 180.0f);
    const float sp = std::sin(pitch * 3.14159265f / 180.0f);
    const float cy = std::cos(yaw * 3.14159265f / 180.0f);
    const float sy = std::sin(yaw * 3.14159265f / 180.0f);
    const dino::Vec3 eye{center.x + distance * cp * cy,
                         center.y + distance * cp * sy,
                         center.z + distance * sp};

    const dino::Mat4 proj = perspective(33.0f,
                                        static_cast<float>(width) / std::max(1, height),
                                        0.1f, 2000.0f);
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

    if (texture != 0) {
        gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    gl.BindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
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

}  // namespace

int main(int argc, char** argv) {
    Options opt = parse_args(argc, argv);

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

    dino::Mesh mesh;
    dino::Image image;
    dino::Skeleton skeleton;
    dino::Skin skin;
    dino::Animation anim;
    try {
        if (!opt.mesh_path.empty()) {
            mesh = dino::load_mesh(opt.mesh_path);
        }
        if (!opt.texture_path.empty()) {
            image = dino::load_tga(opt.texture_path);
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
        }
        if (!opt.anim_path.empty()) {
            if (opt.skel_path.empty()) {
                throw std::runtime_error("--anim requires --skel");
            }
            anim = dino::load_anim(opt.anim_path);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "load error: %s\n", e.what());
        return 1;
    }

    if (opt.validate) {
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
        if (!opt.skel_path.empty()) {
            std::printf("skel %s: %zu bones\n", opt.skel_path.c_str(), skeleton.bones.size());
        }
        if (!opt.skin_path.empty()) {
            std::size_t assigned = 0;
            for (const auto& bone : skin.bones) {
                assigned += bone.vertices.size();
            }
            std::printf("skin %s: %zu vertices, %zu bones, %zu assignments\n",
                        opt.skin_path.c_str(), skin.mesh.positions.size(),
                        skin.bones.size(), assigned);
        }
        if (!opt.anim_path.empty()) {
            std::size_t keys = 0;
            for (const auto& bone : anim.bones) {
                keys += bone.times.size();
            }
            std::printf("anim %s: %zu bones, %u duration, %zu keys\n",
                        opt.anim_path.c_str(), anim.bones.size(), anim.duration, keys);
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
        return 0;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
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

    if (mesh.positions.empty() && !skinned) {
        dino::Mesh fallback;
        fallback.positions = {{-0.62f, -0.52f, 0.0f}, {0.02f, 0.68f, 0.0f}, {0.68f, -0.44f, 0.0f}};
        fallback.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
        fallback.colors = {{246, 140, 41, 255}, {46, 199, 245, 255}, {235, 61, 112, 255}};
        fallback.indices = {0, 1, 2};
        mesh = std::move(fallback);
    }

    const GLuint texture = image.width > 0 ? upload_texture(image) : 0;
    dino::Mesh display;
    if (skinned) {
        const dino::Animation* anim_ptr = anim.bones.empty() ? nullptr : &anim;
        const double initial_time = opt.anim_time >= 0.0 ? opt.anim_time : 0.0;
        display = build_skinned_mesh(skin.mesh, skin, skeleton, anim_ptr, initial_time,
                                     vertex_bone, bone_skel);
        if (mesh.colors.size() == display.positions.size()) {
            display.colors = mesh.colors;
        }
    } else {
        display = mesh;
    }
    GpuMesh gpu = upload_mesh(display, glfns);
    Bounds bounds;
    for (const auto& p : display.positions) {
        bounds.add(p);
    }
    const float auto_distance = std::max(2.0f, bounds.extent() * 1.7f);
    float distance = opt.distance > 0.0f ? opt.distance : auto_distance;
    float yaw = opt.yaw;
    float pitch = opt.pitch;

    bool running = true;
    bool dragging = false;
    float last_mx = 0.0f, last_my = 0.0f;
    int frame = 0;
    const std::uint64_t freq = SDL_GetPerformanceFrequency();
    std::uint64_t last = SDL_GetPerformanceCounter();
    double elapsed = 0.0;
    int width = opt.width;
    int height = opt.height;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
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
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                       event.button.button == SDL_BUTTON_LEFT) {
                dragging = true;
                last_mx = event.button.x;
                last_my = event.button.y;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                       event.button.button == SDL_BUTTON_LEFT) {
                dragging = false;
            } else if (event.type == SDL_EVENT_MOUSE_MOTION && dragging) {
                yaw += (event.motion.x - last_mx) * 0.4f;
                pitch = std::clamp(pitch + (event.motion.y - last_my) * 0.4f, -89.0f, 89.0f);
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
        if (width <= 0 || height <= 0) {
            SDL_Delay(10);
            continue;
        }

        const std::uint64_t now = SDL_GetPerformanceCounter();
        const double dt = static_cast<double>(now - last) / static_cast<double>(freq);
        last = now;
        elapsed += dt;

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
            update_mesh(gpu, deformed, glfns);
        }

        draw_scene(gpu, texture, program, glfns, bounds, yaw, pitch, distance,
                   width, height);

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
