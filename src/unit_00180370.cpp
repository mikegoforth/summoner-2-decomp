#include "game/unit_00180370.h"
#include "game/unit_0017CEB0.h"

vertex Clip_vertices[MAX_CLIP_VERTICES];
int Num_clip_vertices;

inline f32 lerp(f32 from, f32 to, f32 t) { return from + t * (to - from); }

// New vertex where the edge from a (inside the plane) to b (outside) crosses the plane. The
// values both ends have are interpolated.
vertex* clip_edge(int plane, vertex* a, vertex* b) {
    vertex* v = &Clip_vertices[Num_clip_vertices++];
    f32 t;
    v->proj_flags = PF_TEMP_POINT;
    if (plane & CC_OFF_FAR) {
        f32 d = D_002D46EC - a->pos.z;
        f32 dz = b->pos.z - a->pos.z;
        if (dz == 0.0f) {
            t = 1.0f;
        } else {
            t = d / dz;
        }
        v->pos.x = lerp(a->pos.x, b->pos.x, t);
        v->pos.y = lerp(a->pos.y, b->pos.y, t);
        v->pos.z = D_002D46EC;
    } else if (plane & CC_OFF_NEAR) {
        f32 d = D_002D46F4 - a->pos.z;
        f32 dz = b->pos.z - a->pos.z;
        if (dz == 0.0f) {
            t = 1.0f;
        } else {
            t = d / dz;
        }
        v->pos.x = lerp(a->pos.x, b->pos.x, t);
        v->pos.y = lerp(a->pos.y, b->pos.y, t);
        v->pos.z = D_002D46F4;
    } else {
        // The side planes are x = z, x = -z, y = z and y = -z.
        f32 pa;
        f32 pb;
        if (plane & (CC_OFF_LEFT | CC_OFF_RIGHT)) {
            pa = a->pos.x;
            pb = b->pos.x;
        } else {
            pa = a->pos.y;
            pb = b->pos.y;
        }
        if (plane & (CC_OFF_LEFT | CC_OFF_BOT)) {
            pa = -pa;
            pb = -pb;
        }
        f32 da = pa - a->pos.z;
        t = da / (da - pb + b->pos.z);
        v->pos.x = lerp(a->pos.x, b->pos.x, t);
        v->pos.y = lerp(a->pos.y, b->pos.y, t);
        if (plane & (CC_OFF_BOT | CC_OFF_TOP)) {
            v->pos.z = v->pos.y;
        } else {
            v->pos.z = v->pos.x;
        }
        if (plane & (CC_OFF_LEFT | CC_OFF_BOT)) {
            v->pos.z = -v->pos.z;
        }
    }

    int flags = a->flags & b->flags;
    v->flags = flags;
    if (flags & VF_UV) {
        v->u = lerp(a->u, b->u, t);
        v->v = lerp(a->v, b->v, t);
    }
    if (flags & VF_RGB) {
        f32 ar = a->r;
        f32 br = b->r;
        f32 ag = a->g;
        f32 bg = b->g;
        f32 ab = a->b;
        f32 bb = b->b;
        v->r = (int)lerp(ar, br, t);
        v->g = (int)lerp(ag, bg, t);
        v->b = (int)lerp(ab, bb, t);
    }
    if (flags & VF_ALPHA) {
        if (D_002D46E0 && b->a == 0 && (plane & CC_OFF_FAR)) {
            v->a = 0;
        } else if (plane & CC_OFF_NEAR) {
            v->a = a->a > b->a ? a->a : b->a;
        } else {
            v->a = (int)lerp(a->a, b->a, t);
        }
    }
    code_vertex(v);
    return v;
}

// Clips the line a-b against each plane in outcodes (the OR of the ends' outcodes), replacing
// the end outside the plane with a new vertex.
void clip_line(vertex** a, vertex** b, u8 outcodes) {
    int plane;
    for (plane = 1; plane < CLIP_PLANES + 1; plane <<= 1) {
        if (outcodes & plane) {
            if ((*a)->codes & plane) {
                vertex* tmp = *a;
                *a = *b;
                *b = tmp;
            }
            *b = clip_edge(plane, *a, *b);
            outcodes = (*a)->codes | (*b)->codes;
        }
    }
}

// Clips a polygon against one plane. in holds *count vertices with room for two more (the first
// two are repeated after the last); the clipped polygon goes to out and its vertex count is
// returned. outcodes[0] and outcodes[1] get the OR and the AND of the new vertices' outcodes.
int clip_polygon_plane(int plane, vertex** in, vertex** out, int* count, u8* outcodes) {
    vertex** start = out;
    int and_codes = 0xFF;
    int or_codes = 0;
    int i;
    in[*count] = in[0];
    in[*count + 1] = in[1];
    for (i = 1; i <= *count; i++) {
        if (plane & in[i]->codes) {
            if (!(plane & in[i - 1]->codes)) {
                *out = clip_edge(plane, in[i - 1], in[i]);
                or_codes |= (*out)->codes;
                and_codes &= (*out)->codes;
                out++;
            }
            if (!(plane & in[i + 1]->codes)) {
                *out = clip_edge(plane, in[i + 1], in[i]);
                or_codes |= (*out)->codes;
                and_codes &= (*out)->codes;
                out++;
            }
        } else {
            *out++ = in[i];
            or_codes |= in[i]->codes;
            and_codes &= in[i]->codes;
        }
    }
    outcodes[0] = or_codes;
    outcodes[1] = and_codes;
    return out - start;
}

// Clips a polygon against each plane in outcodes[0], going back and forth between the two
// vertex lists, and returns the list that holds the result. Stops early once every vertex is
// outside one plane (outcodes[1] nonzero).
vertex** clip_polygon(vertex** in, vertex** out, int* count, u8* outcodes) {
    int plane;
    for (plane = 1; plane < CLIP_PLANES + 1; plane <<= 1) {
        if (outcodes[0] & plane) {
            *count = clip_polygon_plane(plane, in, out, count, outcodes);
            if (outcodes[1]) {
                return out;
            }
            vertex** tmp = in;
            in = out;
            out = tmp;
        }
    }
    return in;
}
