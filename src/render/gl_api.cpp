// GL entry-point table resolution and scene program linking (see gl_api.h).

#include "gl_api.h"

#include "shaders.h"

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
    const GLuint vs = compile_shader(GL_VERTEX_SHADER, kSceneVertex, gl);
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kSceneFragment, gl);
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

