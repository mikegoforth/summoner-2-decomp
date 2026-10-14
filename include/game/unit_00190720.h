#ifndef GAME_UNIT_00190720_H
#define GAME_UNIT_00190720_H

#include "types.h"
#include "game/unit_00196B10.h"

// Bounding boxes, CRC-32, the random number table, table-based sine and cosine, and collision and
// intersection tests. Parametric results (t) run from 0 at the start of a segment, ray or sweep to
// 1 at its end. sqrt_float() is declared with vector3.

// A point with a parameter, or a plane: normal (x, y, z) and distance (w).
class vector4 {
public:
    float x;
    float y;
    float z;
    float w;
};

struct ray {
    vector3 origin;
    vector3 dir;
};

// Points p with normal.dot(p) + d == 0.
struct plane {
    vector3 normal;
    float d;
};

// Axis-aligned boxes given by their minimum and maximum corners.
void box_from_points(vector3& min, vector3& max, const vector3& a, const vector3& b);
bool boxes_overlap(const vector3& min1, const vector3& max1, const vector3& min2, const vector3& max2);
void sweep_box(vector3& min, vector3& max, const vector3& v);
void get_box_corner(vector3& out, int index, const vector3& min, const vector3& max);
void get_box_corners_along(const vector3& dir, int* min_corner, int* max_corner);

void make_crc_table(void);
u32 crc32(const u8* buffer, int length, u32 crc);
u32 crc32_string(const char* string, u32 crc);

void init_math_tables(void);
u32 random_u32(void);
// Uniform in [0, 1).
float random_float(void);
void refill_random_table(void);
float random_range(float min, float max);

// atan2 with a result in (-pi/2, 3pi/2].
float arctan2(float y, float x);
// Table lookups with linear interpolation, 100 steps per turn.
float fast_cos(float angle);
float fast_sin(float angle);

int intersect_segment_plane(vector4* out, const vector3& start, const vector3& delta, const vector3& point,
                            const vector3& normal);
int segment_crosses_plane(const vector3& start, const vector3& delta, const vector4& plane, float* t);
bool intersect_segment_sphere(vector3* out, const vector3& start, const vector3& end, const vector3& center,
                              float radius);
// Point in a convex polygon given as a fan of vertex pointers.
bool point_in_polygon_xz(const vector3& p, int count, vector3** verts);
bool point_in_polygon(const vector3& p, int count, vector3** verts, const vector3& normal);
bool spheres_overlap(const vector3& a, const vector3& b, float radius_a, float radius_b);
bool intersect_segment_slab(vector3* out, const vector3& start, const vector3& delta, const vector3& normal,
                            const vector3& point, float* t_enter, float* t_span, float radius);

// Sphere of the given radius moving from origin to origin + dir, against the edge a-b: first
// against the edge itself, then against its end point a. On a hit before max_t, stores the contact
// point and t and returns true.
bool sweep_sphere_edge(vector3& hit, const vector3& origin, const vector3& dir, float radius, const vector3& a,
                       const vector3& b, float* t_out, float max_t);
// sweep_sphere_edge() against every edge of a closed polygon; keeps the earliest hit.
bool sweep_sphere_polygon(vector3& hit, const vector3& origin, const vector3& dir, float radius, int count,
                          vector3** verts, float* t_out);
// Hit point of a ray (or line) with a plane; returns false if parallel or behind the origin.
bool intersect_ray_plane(const ray& r, const plane& p, vector3& hit);
// clip_segment_box() against a box of the given size centred on center and rotated by m.
bool clip_segment_obb(const vector3& center, const matrix3& m, const vector3& size, const vector3& p0,
                      const vector3& p1, vector3& out, int ignore_end);
// First point of segment p0-p1 inside the axis-aligned box min-max. p0 itself if it is inside; p1
// if it is inside (unless ignore_end is set); otherwise where the segment enters the box through a
// face on the side of p0.
bool clip_segment_box(const vector3& min, const vector3& max, const vector3& p0, const vector3& p1, vector3& out,
                      int ignore_end);
// Ray (unit dir) against a sphere. Returns the origin if it starts inside; length is unused.
bool intersect_ray_sphere(vector3& hit, float* t_out, const ray& r, float length, const vector3& center, float radius);
// Whether 2D segments a-b and (x0, y0)-(x1, y1) intersect. a and b point to x, y pairs.
bool segments_intersect_2d(const float* a, const float* b, float x0, float y0, float x1, float y1);

#endif // GAME_UNIT_00190720_H
