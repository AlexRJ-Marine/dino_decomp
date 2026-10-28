// Mesh and texture upload (see mesh_gpu.h).

#include "mesh_gpu.h"

#include <cstdio>
#include <vector>

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

// Terrain detail map: repeat-wrapped and mipmapped. Retail asks for no mip
// maps (TextureParam_DetailMap.txt), but the world/16 tiling then aliases
// into noise at park-camera distances; the dgVoodoo2 retail captures show a
// filtered detail layer, which mipmaps reproduce.
GLuint upload_detail_texture(const dino::Image& image, const GlFns& gl) {
    const GLuint id = upload_texture_wrapped(image, GL_REPEAT);
    if (gl.GenerateMipmap != nullptr) {
        gl.GenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
    }
    return id;
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

