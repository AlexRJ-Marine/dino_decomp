// OpenGL 3.3 core entry points for the port.
//
// SDL hands out GL function pointers by name; only the 1.x prototypes are
// linked directly, so the 3.x calls the renderer needs are resolved into this
// table at startup. `compile_shader` and `build_program` live here too: they
// are the only code that knows the shader sources are GLSL 3.30.
//
// This is the one layer allowed to depend on SDL/OpenGL: `world`, `formats`
// and `ui` stay free of them.

#ifndef DINO_RENDER_GL_API_H
#define DINO_RENDER_GL_API_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <cstdio>

struct GlFns {
    PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
    PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr;
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
        LOAD(GenerateMipmap, "glGenerateMipmap")
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


// Compiles one shader stage; logs and returns 0 on failure.
GLuint compile_shader(GLenum type, const char* source, const GlFns& gl);

// Links the scene/UI program from render/shaders and queries its uniforms.
// Returns 0 on failure (the caller exits).
GLuint build_program(const GlFns& gl);

#endif  // DINO_RENDER_GL_API_H
