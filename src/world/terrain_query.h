// Terrain queries used by placement and picking.
//
// Pure CPU: ray-march the heightfield, lay a mesh onto it, build a contact
// shadow. No SDL, no GL - the app shell supplies the camera and the mouse.

#ifndef DINO_WORLD_TERRAIN_QUERY_H
#define DINO_WORLD_TERRAIN_QUERY_H

#include "../formats/mesh.h"
#include "terrain.h"

// The camera ray through a screen pixel, using the same projection as the
// scene pass (80 degrees in the park). `origin` is the eye, `direction` is
// normalized.
void screen_ray(const dino::Vec3& eye, const dino::Vec3& target,
                float mouse_x, float mouse_y, int width, int height,
                float fov_degrees, dino::Vec3& origin, dino::Vec3& direction);

// Ray-marches the heightfield from the camera through a screen pixel.
bool pick_terrain(const dino::Terrain& terrain, const dino::Vec3& eye,
                  const dino::Vec3& target, float mouse_x, float mouse_y,
                  int width, int height, dino::Vec3& hit);

// A disc at ground level so a placed model reads as standing on the terrain.
dino::Mesh make_contact_shadow(float x, float y, float z, float radius);

// Moves a mesh (metres) onto the terrain (world units) at an .imp-style
// position: x/y in 0..1024, Z rotation in degrees.
dino::Mesh place_on_terrain(dino::Mesh mesh, const dino::Terrain& terrain,
                            float x, float y, float angle_degrees);

#endif  // DINO_WORLD_TERRAIN_QUERY_H
