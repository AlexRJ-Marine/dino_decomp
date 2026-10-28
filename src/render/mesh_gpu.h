// CPU -> GPU mesh upload.
//
// Vertex layout, the VAO/VBO/EBO wrapper and the upload helpers. Split out of
// the app shell so the GL resource lifetime is one concern, not mixed into the
// frame loop.

#ifndef DINO_RENDER_MESH_GPU_H
#define DINO_RENDER_MESH_GPU_H

#include <SDL3/SDL_opengl.h>

#include <cstdint>
#include <vector>

#include "../formats/image.h"
#include "../formats/mesh.h"
#include "gl_api.h"

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


// Expands a retail mesh (positions/normals/uvs/colours) into GL vertices,
// duplicated per index so the index buffer is a plain triangle list.
std::vector<Vertex> build_vertices(const dino::Mesh& mesh);

// Creates a VAO/VBO/EBO and uploads the mesh.
GpuMesh upload_mesh(const dino::Mesh& mesh, const GlFns& gl);

// Rewrites the VBO in place (skinned meshes update every frame).
void update_mesh(GpuMesh& gpu, const dino::Mesh& mesh, const GlFns& gl);

GLuint upload_texture_wrapped(const dino::Image& image, GLint wrap);
GLuint upload_texture(const dino::Image& image);

// Terrain detail map: repeat-wrapped and mipmapped.
GLuint upload_detail_texture(const dino::Image& image, const GlFns& gl);

// Concatenates `src` into `dst`, offsetting indices.
void append_mesh(dino::Mesh& dst, const dino::Mesh& src);

#endif  // DINO_RENDER_MESH_GPU_H
