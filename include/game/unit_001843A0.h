#ifndef GAME_UNIT_001843A0_H
#define GAME_UNIT_001843A0_H

#include "types.h"
#include "game/gr.h"

// Primitive builder. A polygon is given between prim_begin and prim_end: its vertices, then one
// or more layers (render mode, bitmap and colour), each followed by per-vertex texture
// coordinates, colours and alphas in vertex order. prim_end clips the polygon (with the values of
// every layer) to the view volume and draws it. A deferred polygon is instead recorded into a
// command buffer, and prim_flush replays the buffer.

#define PRIM_MAX_POINTS 32
#define PRIM_MAX_CLIP_POINTS 64
#define PRIM_BUFFER_SIZE 0x20000
#define PRIM_BUFFER_SLACK 0x200 // prim_begin flushes once fewer bytes than this are left
#define PRIM_MAX_LISTS 0x400
#define PRIM_MAX_REPLAY_VERTS 25 // vertices prim_flush can rebuild for one recorded polygon

// Outcodes of the points of a polygon: the union and the intersection.
struct PrimOutcodes {
    /* 0x00 */ u8 any;
    /* 0x01 */ u8 all;
};

// Recorded commands, each starting with its type.
enum {
    PRIM_CMD_BEGIN = 1,
    PRIM_CMD_BEGIN_DEPTH = 2,
    PRIM_CMD_VERTEX = 3,
    PRIM_CMD_LAYER = 4,
    PRIM_CMD_UV = 5,
    PRIM_CMD_RGB = 6,
    PRIM_CMD_ALPHA = 7,
    PRIM_CMD_END = 8,
};

struct PrimCmdBegin {
    /* 0x00 */ u8 type;
    /* 0x04 */ f32 max_z; // never written
};

struct PrimCmdBeginDepth {
    /* 0x00 */ u8 type;
    /* 0x04 */ f32 max_z; // never written
    /* 0x08 */ f32 depth;
};

struct PrimCmdVertex {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 codes;
    /* 0x04 */ f32 x;
    /* 0x08 */ f32 y;
    /* 0x0C */ f32 z;
};

struct PrimCmdLayer {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 flag;
    /* 0x02 */ s16 bitmap;
    /* 0x04 */ gr_mode mode;
    /* 0x08 */ u8 r;
    /* 0x09 */ u8 g;
    /* 0x0A */ u8 b;
    /* 0x0B */ u8 a;
};

struct PrimCmdUV {
    /* 0x00 */ u8 type;
    /* 0x04 */ f32 u;
    /* 0x08 */ f32 v;
};

struct PrimCmdRGB {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 r;
    /* 0x02 */ u8 g;
    /* 0x03 */ u8 b;
};

struct PrimCmdAlpha {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 a;
    /* 0x02 */ u8 pad02[2];
};

struct PrimCmdEnd {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 pad01[3];
};

// Points made by clipping. Prim_free_clip_points[i] points at Prim_clip_points[i], whose vertex is
// Prim_clip_vertices[i]; prim_clip_edge hands them out in order.
extern vertex Prim_clip_vertices[PRIM_MAX_CLIP_POINTS];
extern PolyPoint Prim_clip_points[PRIM_MAX_CLIP_POINTS];
extern PolyPoint* Prim_free_clip_points[PRIM_MAX_CLIP_POINTS];
extern PolyPoint Prim_points[PRIM_MAX_POINTS]; // the polygon being built
extern PolyLayer Prim_layers[POLY_MAX_LAYERS];
extern u8 Prim_buffer[PRIM_BUFFER_SIZE]; // recorded commands
extern u8* Prim_lists[PRIM_MAX_LISTS];   // start of each recorded polygon
extern u8* Prim_buffer_limit;
extern u8* Prim_buffer_ptr;
extern int Prim_num_clip_points;
extern u8 Prim_clipper_initialized; // set once prim_init has linked up the clip points
extern u8 Prim_active;              // between prim_begin and prim_end of a polygon drawn at once
extern int Prim_num_points;
extern int Prim_num_layers;
extern u8 Prim_use_depth;
extern f32 Prim_depth;
extern PolyLayer* Prim_cur_layer;
extern int Prim_cur_layer_index;
// Next point to take a texture coordinate, colour or alpha for the current layer.
extern PolyPoint* Prim_uv_point;
extern PolyPoint* Prim_rgb_point;
extern PolyPoint* Prim_alpha_point;
extern u8 Prim_recording; // between prim_begin and prim_end of a deferred polygon
extern int Prim_num_lists;

// New point where the edge from on_pnt (inside the plane) to off_pnt (outside) crosses the plane,
// taken from Prim_free_clip_points. The values of every layer are interpolated.
PolyPoint* prim_clip_edge(int plane, PolyPoint* on_pnt, PolyPoint* off_pnt);
// Clips the polygon in points (*count points, with room for two more) against one plane into out.
// Returns the new number of points and sets codes from their outcodes.
int prim_clip_plane(int plane, PolyPoint** points, PolyPoint** out, int* count, PrimOutcodes* codes);
// Clips the polygon against every plane in codes->any, using points and temp in turn. Returns the
// list holding the result; *count is its number of points, 0 if nothing is left.
PolyPoint** prim_clip_polygon(PolyPoint** points, PolyPoint** temp, int* count, PrimOutcodes* codes);
// Clips the polygon if any point is outside the view volume, projects the points and draws each
// layer. Returns 0 if no layer was given, otherwise 1.
int prim_draw(PrimOutcodes* codes, PolyPoint** points, PolyPoint** temp);
// Links each of Prim_free_clip_points to its point and vertex.
void prim_init(void);
// deferred: record the polygon for prim_flush instead of drawing it. Callers pass the farthest z
// of the polygon in max_z, which is not used. use_depth: give every point the depth depth once
// the polygon is clipped. Both flags are ints: draw_polygon passes its int fixed_depth straight
// on as use_depth.
void prim_begin(int deferred, f32 max_z, int use_depth, f32 depth);
void prim_vertex(vertex* v);
// Starts a layer drawn with mode, bitmap and the colour r, g, b, a.
void prim_layer(gr_mode mode, int bitmap, u8 r, u8 g, u8 b, u8 a, u8 flag);
void prim_uv(f32 u, f32 v);
void prim_rgb(u8 r, u8 g, u8 b);
void prim_alpha(u8 a);
// Returns 0 if every point is outside the same side of the view volume, or if no layer was
// given; 1 once the polygon is drawn or recorded.
u8 prim_end();
// Draws the recorded polygons, with the tint off, and empties the command buffer.
void prim_flush();

#endif // GAME_UNIT_001843A0_H
