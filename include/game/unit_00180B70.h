#ifndef GAME_UNIT_00180B70_H
#define GAME_UNIT_00180B70_H

#include "types.h"
#include "game/unit_00196B10.h"
#include "game/matrix.h"
#include "game/gr.h"
#include "game/unit_00180370.h"

// 3D primitives built from view-space vertices: lines, polygons, camera-facing bitmaps and rods.
// They are clipped to the view volume and handed to the primitive builder (prim_begin to
// prim_end) in gr_screen's current colour and bitmap.

// Draws the line a-b, clipping it first if needed. Returns 0 if nothing was left to draw.
u8 draw_line(vertex* a, vertex* b, gr_mode mode);
// The same for two world-space points.
u8 draw_line(const vector3& a, const vector3& b, gr_mode mode);

// Hands a polygon to the primitive builder, which clips and draws it. Translucent polygons (from
// the vertex alphas or the current alpha) and blend modes 1 and 3 are recorded and drawn later.
// With fixed_depth set, depth replaces the z of every clipped point.
u8 draw_polygon(int count, vertex** verts, gr_mode mode, int fixed_depth, float depth);
// The same, always drawn straight away.
u8 draw_polygon_immediate(int count, vertex** verts, gr_mode mode);
// Clips a polygon to the view volume. Returns the list holding the result and sets count to 0
// when nothing is left. The list may point at clip vertices, whose allocator has been reset.
vertex** clip_polygon_to_view(int* count, vertex** verts);

// Camera-facing square of the current bitmap centred on a view-space point, rotated by angle
// (radians) and stretched to the bitmap's aspect ratio. The whole square is drawn at one depth,
// the point's less the scaled radius when that stays in front of the camera, so it is not cut by
// what lies just behind the point. Returns 0 if nothing was drawn.
u8 draw_bitmap(vertex* center, float angle, float radius, gr_mode mode);
// A width by height rectangle whose texture coordinates are offset by u and v.
u8 draw_bitmap(vertex* center, float angle, float width, float height, float u, float v, gr_mode mode);
// The same for world-space points.
u8 draw_bitmap(const vector3& pos, float angle, float radius, gr_mode mode);
u8 draw_bitmap(const vector3& pos, float angle, float width, float height, float u, float v, gr_mode mode);

// Strip of the current bitmap, width wide, from start to end, turned about its axis to face the
// camera. Always returns 1.
u8 draw_rod(const vector3& start, const vector3& end, float width, gr_mode mode);
// The same with the texture coordinates offset by u and v.
u8 draw_rod(const vector3& start, const vector3& end, float width, float u, float v, gr_mode mode);

#endif // GAME_UNIT_00180B70_H
