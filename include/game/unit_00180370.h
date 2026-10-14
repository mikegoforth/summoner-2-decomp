#ifndef GAME_UNIT_00180370_H
#define GAME_UNIT_00180370_H

#include "types.h"
#include "game/gr.h"

// Clipping of lines and polygons against the view volume, in view space before the
// perspective divide. Planes are named by their vertex::codes bits (CC_OFF_NEAR to CC_OFF_TOP).

// All the planes clipped against.
#define CLIP_PLANES 0x3F

// Vertices made by clipping, handed out in order and reset by the drawing code once a
// primitive has been sent.
#define MAX_CLIP_VERTICES 64
extern vertex Clip_vertices[MAX_CLIP_VERTICES];
extern int Num_clip_vertices;

vertex* clip_edge(int plane, vertex* a, vertex* b);
void clip_line(vertex** a, vertex** b, u8 outcodes);
int clip_polygon_plane(int plane, vertex** in, vertex** out, int* count, u8* outcodes);
vertex** clip_polygon(vertex** in, vertex** out, int* count, u8* outcodes);

#endif // GAME_UNIT_00180370_H
