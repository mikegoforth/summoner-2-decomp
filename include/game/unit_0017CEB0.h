#ifndef GAME_UNIT_0017CEB0_H
#define GAME_UNIT_0017CEB0_H

#include "types.h"
#include "game/unit_00196B10.h"
#include "game/matrix.h"
#include "game/gr.h"
#include "game/unit_00180370.h"
#include "game/unit_00190720.h"

// 3D view: the view matrix and position, the instance stack for local coordinate systems, the
// planes of the view volume and the codes that say which of them a point is outside of, moving
// points into view space and projecting them onto the screen, and culling against the view.

// sphere_visibility() result bits.
#define SPHERE_CLIPPED 0x1 // reaches past the guard band planes, so its polygons need clipping
#define SPHERE_VISIBLE 0x2 // at least partly inside the cull planes

#define MAX_CLIP_PLANES    8
#define MAX_INSTANCE_DEPTH 5

// Distance in front of the eye at which update_clip_planes measures the edges of the view.
#define CLIP_PLANE_DISTANCE 1.0f

// How far out the guard band planes are, in multiples of the view's half width and half height.
#define GUARD_BAND_X 4.0f
#define GUARD_BAND_Y (32.0f / 7.0f)

// -v, expanded inline (vector3::operator-() is out of line).
inline vector3 negated(const vector3& v) { return vector3(-v.x, -v.y, -v.z); }

// plane (unit_00190720.h) holds the points p with normal.dot(p) + d == 0. The normals of the view
// planes face out of the view.

// The plane through three points, with the normal (p1 - p0) x (p2 - p1).
inline void set_plane(plane* pl, const vector3& p0, const vector3& p1, const vector3& p2) {
    triangle_normal(&pl->normal, &p0, &p1, &p2);
    pl->normal.normalize();
    pl->d = -1.0f * pl->normal.dot(p0);
}

// The plane with the given normal through point.
inline void set_plane(plane* pl, const vector3& normal, const vector3& point) {
    pl->normal = normal;
    pl->d = -1.0f * normal.dot(point);
}

// Turns the plane to face the other way.
inline void flip_plane(plane* pl) {
    pl->normal = negated(pl->normal);
    pl->d = -pl->d;
}

// Fill in a new plane through three points, or with a normal through a point. init_plane reaches
// set_plane through construct_plane, two calls deep, as a constructor of plane would.
inline void construct_plane(plane* pl, const vector3& p0, const vector3& p1, const vector3& p2) {
    set_plane(pl, p0, p1, p2);
}

inline void construct_plane(plane* pl, const vector3& normal, const vector3& point) { set_plane(pl, normal, point); }

inline void init_plane(plane& pl, const vector3& p0, const vector3& p1, const vector3& p2) {
    construct_plane(&pl, p0, p1, p2);
}

inline void init_plane(plane& pl, const vector3& normal, const vector3& point) { construct_plane(&pl, normal, point); }

// A view plane and the corners (as numbered by get_box_corner) of a box that lie least and
// furthest along its normal.
struct clip_plane {
    /* 0x00 */ u8 code; // vertex::codes bit (CC_*) for points outside this plane
    /* 0x04 */ plane p;
    /* 0x14 */ int near_corner;
    /* 0x18 */ int far_corner;

    // The plane through three points.
    void set(u8 c, vector3 p0, vector3 p1, vector3 p2) {
        plane temp;
        code = c;
        init_plane(temp, p0, p1, p2);
        p = temp;
        get_box_corners_along(p.normal, &near_corner, &far_corner);
    }
    // The plane with the given normal through point.
    void set(u8 c, vector3 normal, vector3 point) {
        plane temp;
        code = c;
        init_plane(temp, normal, point);
        p = temp;
        get_box_corners_along(p.normal, &near_corner, &far_corner);
    }
};

// The view position and orientation from before an instance (a local coordinate system) was
// entered.
struct view_instance {
    /* 0x00 */ vector3 position;
    /* 0x0C */ matrix3 orient;
};

// What save_view_state copies away and restore_view_state puts back. view_position is aligned
// like View_position.
struct view_state {
    /* 0x000 */ matrix3 view_matrix;
    /* 0x024 */ matrix3 unscaled_matrix;
    /* 0x050 */ vector3 view_position __attribute__((aligned(16)));
    /* 0x05C */ matrix3 eye_matrix;
    /* 0x080 */ vector3 eye_position;
    /* 0x08C */ vector3 matrix_scale;
    /* 0x098 */ f32 unk98;
    /* 0x09C */ int canvas_width;
    /* 0x0A0 */ int canvas_height;
    /* 0x0A4 */ f32 canvas_w2;
    /* 0x0A8 */ f32 canvas_h2;
    /* 0x0AC */ f32 unkAC;
    /* 0x0B0 */ f32 unkB0;
    /* 0x0B4 */ f32 unkB4;
    /* 0x0B8 */ f32 unkB8;
    /* 0x0BC */ clip_plane unk_planes[MAX_CLIP_PLANES];
    /* 0x19C */ int num_unk_planes;
    /* 0x1A0 */ u8 unk1A0;
    /* 0x1A1 */ u8 unk1A1;
    /* 0x1A4 */ f32 unk1A4;
    /* 0x1A8 */ f32 unk1A8;
    /* 0x1AC */ f32 unk1AC;
    /* 0x1B0 */ f32 unk1B0;
    /* 0x1B4 */ view_instance instance_stack[MAX_INSTANCE_DEPTH];
    /* 0x2A4 */ int instance_depth;
    /* 0x2A8 */ clip_plane clip_planes[MAX_CLIP_PLANES];
    /* 0x388 */ int num_clip_planes;
};

// One entry of a depth-sorted draw list.
struct depth_entry {
    /* 0x00 */ f32 z; // view space depth
    /* 0x04 */ u16 index;
};

// How to draw a model. The constructor fills in the defaults and the caller changes what it needs.
struct DrawParams {
    /* 0x00 */ u32 flags;
    /* 0x04 */ int unk04;
    /* 0x08 */ int alpha; // 0 to 255
    /* 0x0C */ u8 unk0C[4];
    /* 0x10 */ int unk10;
    /* 0x14 */ f32 brightness;
    /* 0x18 */ u8 unk18[3];
    /* 0x1C */ int unk1C;
    /* 0x20 */ f32 scroll_u; // texture offset
    /* 0x24 */ f32 scroll_v;
    /* 0x28 */ f32 unk28;

    DrawParams();
};

// The file's data.
extern f32 D_002D3ED8;          // the far distance the view z scale was set up for, 250 to 1000
extern u8 Clip_codes_enabled;   // 0x2D3EDC: 0 turns code_vertex off
extern f32 D_002D46B8;          // 1 / Matrix_scale.z
extern int Canvas_width;        // 0x2D46BC: gr_screen.clip_width when the view was set
extern int Canvas_height;       // 0x2D46C0
extern f32 Canvas_w2;           // 0x2D46C4: half of Canvas_width
extern f32 Canvas_h2;           // 0x2D46C8
extern f32 D_002D46CC;          // screen centre x in GS coordinates (12.4 fixed point)
extern f32 D_002D46D0;          // screen centre y in GS coordinates
extern f32 D_002D46D4;          // Canvas_w2 in GS units
extern f32 D_002D46D8;          // -Canvas_h2 in GS units
extern int D_002D46DC;          // entries used in D_00885B60
extern u8 D_002D46E0;           // when set, clipping at the far plane passes a zero alpha on
extern u8 D_002D46E4;           // the far plane is on
extern f32 D_002D46E8;          // distance to the far plane
extern f32 D_002D46EC;          // D_002D46E8 scaled like the view z axis: the clipper's far z
extern f32 D_002D46F0;          // near clipping distance (nothing sets it, so 0)
extern f32 D_002D46F4;          // D_002D46F0 scaled like the view z axis: the clipper's near z
extern f32 D_002D46F8;          // scaled view z of D_002D3ED8 (0.98)
extern int Instance_depth;      // 0x2D46FC
extern int Num_clip_planes;     // 0x2D4700
extern matrix3 View_matrix;     // 0x885A60: world to view, scaled for the field of view
extern matrix3 Unscaled_matrix; // 0x885A90: View_matrix before scaling
// 0x885AC0. VU0 code loads it with lqc2, so it is 16-byte aligned.
extern vector3 View_position __attribute__((aligned(16)));
extern matrix3 Object_matrix;   // 0x885AD0: identity once the view is set
extern vector3 Object_position; // 0x885AF8: zero once the view is set
extern matrix3 Eye_matrix;      // 0x885B10: the view as set, before any instance
extern vector3 Eye_position;    // 0x885B38
extern vector3 Matrix_scale;    // 0x885B48: how far View_matrix's rows were scaled
extern clip_plane D_00885B60[MAX_CLIP_PLANES];
// 0x885C40 and 0x885CB0: vertex lists that polygon clipping works in.
extern vertex* D_00885C40[28];
extern vertex* D_00885CB0[28];
extern view_instance Instance_stack[MAX_INSTANCE_DEPTH]; // 0x885D20
// 0x885E10 and 0x885E60. Both sets are in the order near, top, bottom, left, right, with the
// normals facing out, and share the near plane, which lies one unit behind the eye.
extern plane Guard_band_planes[5]; // edges GUARD_BAND_X and GUARD_BAND_Y times as far out as the view's
extern plane Cull_planes[5];       // edges from the clip planes
extern clip_plane Clip_planes[MAX_CLIP_PLANES]; // 0x885EB0: the view volume
extern view_state Saved_view_state;             // 0x885F90
extern f32 View_matrix_vu[3][4];                // 0x886320: View_matrix's rows for VU0

// Rebuilds Clip_planes from the view: left, right, bottom, top, near and, if it is on, far.
void update_clip_planes(void);
// Sets the view. zoom is the tangent of half the horizontal field of view, or that field of
// view in degrees when it is 2 or more.
void set_view_matrix(const matrix3* orient, const vector3* position, float zoom);
// The horizontal and vertical field of view, in degrees.
void get_view_fov(float* fov_x, float* fov_y);
// Enters the local coordinate system of an object at position with orientation orient.
void start_instance(const vector3* position, const matrix3* orient);
// Goes back to the coordinate system from before the last start_instance.
void done_instance(void);
// Sets D_002D46E0 and the far plane. A distance of 1 or less turns the far plane off.
void set_far_clip(u8 value, float distance);
void enable_clip_codes(void);
// Sets vertex::codes for the view space position and returns them.
u8 code_vertex(vertex* p);
// Puts src into view space and sets the codes; returns them.
u8 rotate_vertex(vertex* dest, const vector3& src);
void rotate_vertices(vertex* dest, const vector3* src, int count);
// Sets the screen position of a vertex in view space, unless that has been done already.
void project_vertex(vertex* p);
// TRUE if a surface through point with this normal faces the eye.
BOOL check_normal_facing(const vector3* normal, const vector3* point);
// The line from the eye through the screen position sx, sy.
void screen_point_to_ray(ray* r, float sx, float sy);
// size (along the view x axis) in pixels at the depth of a vertex in view space.
float get_screen_size(const vertex* p, const float& size);
// size scaled like the view z axis.
float scale_view_depth(float size);
// TRUE if the sphere is entirely outside one of the view planes.
BOOL sphere_outside_view(const vector3* center, float radius);
// Copies out the view planes and, if codes isn't null, their codes. Returns how many there are.
int get_clip_planes(plane* planes, u8* codes);
// TRUE if the box min..max is entirely outside one of the view planes.
BOOL box_outside_view(const vector3* min, const vector3* max);
vector3* copy_vector3(vector3* dest, const vector3* src);
// The view position and orientation as set, ignoring any instance.
void get_eye(vector3* position, matrix3* orient);
void save_view_state(void);
void restore_view_state(void);
// SPHERE_VISIBLE and SPHERE_CLIPPED bits for a bounding sphere; 0 if it is out of view.
int sphere_visibility(const vector3* center, float radius);
// Builds the guard band planes from the eye and the field of view, and the cull planes from them
// and the clip planes.
void update_cull_planes(void);
// Shell sort, largest z first.
void sort_back_to_front(depth_entry* entries, int count);
// Moves an orientation and position from the local space of an object with the given orientation
// and position into world space.
void local_to_world(const matrix3* orient, const vector3* position, matrix3* local_orient, vector3* local_position);
// Screen position and 1 / z of a world space point. FALSE if the point is outside the view.
BOOL project_point(const vector3* point, float* sx, float* sy, float* sw);

#endif // GAME_UNIT_0017CEB0_H
