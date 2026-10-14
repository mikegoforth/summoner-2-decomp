#ifndef GAME_UNIT_00182180_H
#define GAME_UNIT_00182180_H

#include "types.h"
#include "game/unit_00196B10.h"
#include "game/gr.h"

// Camera-facing ribbons and wireframe spheres, drawn with the primitive builder (prim_begin to
// prim_end) like the primitives of unit_00180B70.

// Division by a scalar, which vector3 does not declare.
inline vector3 operator/(const vector3& v, float s) { return vector3(v.x / s, v.y / s, v.z / s); }

// Camera-facing strip of quads through count points (at most 21), width wide, drawn with the
// current bitmap and colour. colors and alphas give each point's colour and alpha and may be
// null. Each quad's texture coordinates run from 0 to 1.
int draw_ribbon(int count, const vector3* points, const color* colors, const u8* alphas, float width, gr_mode mode);

// Wireframe sphere: a circle of the given radius around center in the plane across each of the
// X, Y and Z axes, each with a line through the center along its axis. The circles have between
// 5 and 32 segments depending on their size on screen.
int draw_wire_sphere(const vector3& center, float radius, gr_mode mode);

#endif // GAME_UNIT_00182180_H
