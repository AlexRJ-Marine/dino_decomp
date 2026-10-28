// GLSL 3.30 sources for the scene/UI program.
//
// The shader text used to sit inside build_program; it is a data constant, not
// logic, so it has its own unit. The program links these with
// `compile_shader` + `build_program` in the app shell.
//
// Attribute locations: 0 position, 1 normal, 2 uv, 3 colour.
// Stages cover the detail map (coordinate set 1, world/16), water stage 0/1,
// signed ambient modulation, alpha reference (fence laser) and fog.

#ifndef DINO_RENDER_SHADERS_H
#define DINO_RENDER_SHADERS_H

extern const char* const kSceneVertex;
extern const char* const kSceneFragment;

#endif  // DINO_RENDER_SHADERS_H
