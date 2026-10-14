// 3D primitives: lines, polygons, camera-facing bitmaps and rods.
// The only data of the file are the two 16-byte zero blocks in .bss at 0x89D190 and 0x89D1A0 that
// CodeWarrior emits for the vertex list initializers in the two draw_bitmap functions that take a
// view-space point: the block is copied into the list, then the addresses are stored.
#include "game/unit_00180B70.h"
#include "game/unit_0017CEB0.h"
#include "game/unit_001843A0.h"
#include "game/unit_00187F80.h"
#include "game/bmpman_ngps.h"

extern "C" float func_001CC200(float x); // cosf
extern "C" float func_001CC4B8(float x); // sinf

u8 draw_line(vertex* p0, vertex* p1, gr_mode mode) {
    vertex* a = p0;
    vertex* b = p1;
    u8 and_codes = a->codes & b->codes;
    if (and_codes) {
        return 0;
    }
    u8 clipped = 0;
    u8 visible = 1;
    u8 or_codes = a->codes | b->codes;
    if (or_codes) {
        clip_line(&a, &b, or_codes);
        clipped = 1;
        or_codes = a->codes | b->codes;
        if (or_codes) {
            visible = 0;
        }
    }
    if (visible) {
        if (!(a->proj_flags & PF_PROJECTED)) {
            project_vertex(a);
        }
        if (!(b->proj_flags & PF_PROJECTED)) {
            project_vertex(b);
        }
        gr_ngps_line_3d(a, b, mode);
    }
    if (clipped) {
        Num_clip_vertices = 0;
    }
    return visible;
}

u8 draw_line(const vector3& a, const vector3& b, gr_mode mode) {
    vertex va;
    vertex vb;
    va.flags = 0;
    vb.flags = 0;
    rotate_vertex(&va, a);
    rotate_vertex(&vb, b);
    return draw_line(&va, &vb, mode);
}

u8 draw_polygon(int count, vertex** verts, gr_mode mode, int fixed_depth, float depth) {
    int i;
    int blend;
    u8 translucent = 0;
    blend = (mode.value >> 10) & 0x1F; // blend mode, bits 10-14 of the render state
    for (i = 0; !translucent && i < count; i++) {
        if (verts[i]->flags & VF_ALPHA) {
            if (verts[i]->a != 0xFF) {
                translucent = 1;
            }
        } else if (gr_screen.current_color.a != 0xFF) {
            translucent = 1;
        }
    }
    if (translucent || blend == 3 || blend == 1) {
        float max_z = verts[0]->pos.z;
        for (i = 1; i < count; i++) {
            if (verts[i]->pos.z > max_z) {
                max_z = verts[i]->pos.z;
            }
        }
        prim_begin(1, max_z, fixed_depth, depth);
    } else {
        prim_begin(0, 0.0f, fixed_depth, depth);
    }
    prim_layer(mode, gr_screen.current_bitmap, gr_screen.current_color.r, gr_screen.current_color.g,
               gr_screen.current_color.b, gr_screen.current_color.a, 0);
    for (i = 0; i < count; i++) {
        vertex* v = verts[i];
        int flags = v->flags;
        prim_vertex(v);
        if (flags & VF_UV) {
            prim_uv(v->u, v->v);
        }
        if (flags & VF_RGB) {
            prim_rgb(v->r, v->g, v->b);
        }
        if (flags & VF_ALPHA) {
            prim_alpha(v->a);
        }
    }
    return prim_end();
}

u8 draw_polygon_immediate(int count, vertex** verts, gr_mode mode) {
    int i;
    prim_begin(0, 0.0f, 0, 0.0f);
    prim_layer(mode, gr_screen.current_bitmap, gr_screen.current_color.r, gr_screen.current_color.g,
               gr_screen.current_color.b, gr_screen.current_color.a, 0);
    for (i = 0; i < count; i++) {
        vertex* v = verts[i];
        int flags = v->flags;
        prim_vertex(v);
        if (flags & VF_UV) {
            prim_uv(v->u, v->v);
        }
        if (flags & VF_RGB) {
            prim_rgb(v->r, v->g, v->b);
        }
        if (flags & VF_ALPHA) {
            prim_alpha(v->a);
        }
    }
    return prim_end();
}

vertex** clip_polygon_to_view(int* count, vertex** verts) {
    int i;
    vertex** list = D_00885C40;
    u8 or_codes = 0;
    u8 and_codes = 0xFF;
    for (i = 0; i < *count; i++) {
        vertex* v = list[i] = verts[i];
        and_codes &= v->codes;
        or_codes |= v->codes;
    }
    if (and_codes) {
        *count = 0;
        return list;
    }
    u8 outcodes[2];
    outcodes[0] = or_codes;
    outcodes[1] = and_codes;
    u8 clipped = 0;
    if (or_codes) {
        clipped = 1;
        list = clip_polygon(D_00885C40, D_00885CB0, count, outcodes);
        if (*count == 0 || outcodes[1]) {
            *count = 0;
        }
    }
    if (clipped) {
        Num_clip_vertices = 0;
    }
    return list;
}

u8 draw_bitmap(vertex* center, float angle, float radius, gr_mode mode) {
    vertex v[4];
    vertex* list[4] = {&v[3], &v[2], &v[1], &v[0]};
    float sa;
    float ca;
    float width;
    float height;
    u8 and_codes;

    if (!(center->proj_flags & PF_PROJECTED)) {
        project_vertex(center);
    }
    // (!flags) & 1: only true when no flag is set, which projecting never leaves.
    if (!center->proj_flags & PF_PROJECTED) {
        return 0;
    }
    if (angle == 0.0f) {
        sa = 0.0f;
        ca = 1.0f;
    } else {
        sa = func_001CC4B8(angle);
        ca = func_001CC200(angle);
    }
    if (gr_screen.current_bitmap > -1) {
        int bw;
        int bh;
        bm_get_dimensions(gr_screen.current_bitmap, &bw, &bh);
        if (!bw) {
            return 0;
        }
        if (bw < bh) {
            height = radius * bh / bw;
            width = radius;
        } else if (bw > bh) {
            width = radius * bw / bh;
            height = radius;
        } else {
            width = height = radius;
        }
    } else {
        width = height = radius;
    }

    and_codes = 0xFF;
    v[0].pos.x = center->pos.x + Matrix_scale.x * (-width * ca + height * sa);
    v[0].pos.y = center->pos.y + Matrix_scale.y * (-width * sa - height * ca);
    v[0].pos.z = center->pos.z;
    v[0].u = 0.0f;
    v[0].v = 1.0f;
    v[0].flags = VF_UV;

    v[1].pos.x = center->pos.x + Matrix_scale.x * (width * ca + height * sa);
    v[1].pos.y = center->pos.y + Matrix_scale.y * (width * sa - height * ca);
    v[1].pos.z = center->pos.z;
    v[1].u = 1.0f;
    v[1].v = 1.0f;
    v[1].flags = VF_UV;

    v[2].pos.x = center->pos.x + Matrix_scale.x * (width * ca - height * sa);
    v[2].pos.y = center->pos.y + Matrix_scale.y * (width * sa + height * ca);
    v[2].pos.z = center->pos.z;
    v[2].u = 1.0f;
    v[2].v = 0.0f;
    v[2].flags = VF_UV;

    v[3].pos.x = center->pos.x + Matrix_scale.x * (-width * ca - height * sa);
    v[3].pos.y = center->pos.y + Matrix_scale.y * (-width * sa + height * ca);
    v[3].pos.z = center->pos.z;
    v[3].u = 0.0f;
    v[3].v = 0.0f;
    v[3].flags = VF_UV;

    float offset = scale_view_depth(radius);
    float z = center->pos.z;
    if (offset < z) {
        z -= offset;
    }

    for (int i = 0; i < 4; i++) {
        and_codes &= code_vertex(&v[i]);
        v[i].proj_flags = 0;
    }
    if (and_codes) {
        return 0;
    }
    return draw_polygon(4, list, mode, 1, z);
}

u8 draw_bitmap(vertex* center, float angle, float width, float height, float u, float v, gr_mode mode) {
    vertex pts[4];
    vertex* list[4] = {&pts[3], &pts[2], &pts[1], &pts[0]};
    float sa;
    float ca;
    u8 and_codes;

    if (!(center->proj_flags & PF_PROJECTED)) {
        project_vertex(center);
    }
    // (!flags) & 1: only true when no flag is set, which projecting never leaves.
    if (!center->proj_flags & PF_PROJECTED) {
        return 0;
    }
    if (angle == 0.0f) {
        sa = 0.0f;
        ca = 1.0f;
    } else {
        sa = func_001CC4B8(angle);
        ca = func_001CC200(angle);
    }
    and_codes = 0xFF;
    width /= 2.0f;
    height /= 2.0f;

    pts[0].pos.x = center->pos.x + Matrix_scale.x * (-width * ca + height * sa);
    pts[0].pos.y = center->pos.y + Matrix_scale.y * (-width * sa - height * ca);
    pts[0].pos.z = center->pos.z;
    pts[0].u = u;
    pts[0].v = 1.0f + v;
    pts[0].flags = VF_UV;

    pts[1].pos.x = center->pos.x + Matrix_scale.x * (width * ca + height * sa);
    pts[1].pos.y = center->pos.y + Matrix_scale.y * (width * sa - height * ca);
    pts[1].pos.z = center->pos.z;
    pts[1].u = 1.0f + u;
    pts[1].v = 1.0f + v;
    pts[1].flags = VF_UV;

    pts[2].pos.x = center->pos.x + Matrix_scale.x * (width * ca - height * sa);
    pts[2].pos.y = center->pos.y + Matrix_scale.y * (width * sa + height * ca);
    pts[2].pos.z = center->pos.z;
    pts[2].u = 1.0f + u;
    pts[2].v = v;
    pts[2].flags = VF_UV;

    pts[3].pos.x = center->pos.x + Matrix_scale.x * (-width * ca - height * sa);
    pts[3].pos.y = center->pos.y + Matrix_scale.y * (-width * sa + height * ca);
    pts[3].pos.z = center->pos.z;
    pts[3].u = u;
    pts[3].v = v;
    pts[3].flags = VF_UV;

    float offset = scale_view_depth(width);
    float z = center->pos.z;
    if (offset < z) {
        z -= offset;
    }

    for (int i = 0; i < 4; i++) {
        and_codes &= code_vertex(&pts[i]);
        pts[i].proj_flags = 0;
    }
    if (and_codes) {
        return 0;
    }
    return draw_polygon(4, list, mode, 1, z);
}

u8 draw_bitmap(const vector3& pos, float angle, float radius, gr_mode mode) {
    vertex v;
    rotate_vertex(&v, pos);
    project_vertex(&v);
    if (v.proj_flags & PF_PROJECTED) {
        return draw_bitmap(&v, angle, radius, mode);
    }
    return 0;
}

u8 draw_bitmap(const vector3& pos, float angle, float width, float height, float u, float v, gr_mode mode) {
    vertex p;
    rotate_vertex(&p, pos);
    project_vertex(&p);
    if (p.proj_flags & PF_PROJECTED) {
        return draw_bitmap(&p, angle, width, height, u, v, mode);
    }
    return 0;
}

u8 draw_rod(const vector3& start, const vector3& end, float width, gr_mode mode) {
    vertex p0;
    vertex p1;
    vertex p2;
    vertex p3;
    vector3 fvec;
    vector3 rvec;
    vector3 uvec;
    vector3 v0;
    vector3 v1;
    vector3 v2;
    vector3 v3;

    // fvec along the rod, rvec towards the camera, uvec across the rod.
    fvec = start - end;
    fvec.normalize_safe();
    vector3 mid = (start + end) * 0.5f;
    rvec = View_position - mid;
    rvec.normalize_safe();
    uvec = fvec.cross(rvec);
    uvec.normalize_safe();

    float half = 0.5f * width;
    v0 = start + uvec * half;
    rotate_vertex(&p0, v0);
    v1 = start - uvec * half;
    rotate_vertex(&p1, v1);
    v2 = end + uvec * half;
    rotate_vertex(&p2, v2);
    v3 = end - uvec * half;
    rotate_vertex(&p3, v3);

    // Recorded with the view-space depth of the midpoint as its depth.
    prim_begin(1, (start * 0.5f + end * 0.5f - View_position).rotated(View_matrix).z, 0, 0.0f);
    prim_vertex(&p0);
    prim_vertex(&p2);
    prim_vertex(&p3);
    prim_vertex(&p1);
    prim_layer(mode, gr_screen.current_bitmap, gr_screen.current_color.r, gr_screen.current_color.g,
               gr_screen.current_color.b, gr_screen.current_color.a, 0);
    prim_uv(0.0f, 0.0f);
    prim_uv(0.0f, 1.0f);
    prim_uv(1.0f, 1.0f);
    prim_uv(1.0f, 0.0f);
    prim_end();
    return 1;
}

u8 draw_rod(const vector3& start, const vector3& end, float width, float u, float v, gr_mode mode) {
    vertex p0;
    vertex p1;
    vertex p2;
    vertex p3;
    vector3 fvec;
    vector3 rvec;
    vector3 uvec;
    vector3 v0;
    vector3 v1;
    vector3 v2;
    vector3 v3;

    fvec = start - end;
    fvec.normalize_safe();
    vector3 mid = (start + end) * 0.5f;
    rvec = View_position - mid;
    rvec.normalize_safe();
    uvec = fvec.cross(rvec);
    uvec.normalize_safe();

    float half = 0.5f * width;
    v0 = start + uvec * half;
    rotate_vertex(&p0, v0);
    v1 = start - uvec * half;
    rotate_vertex(&p1, v1);
    v2 = end + uvec * half;
    rotate_vertex(&p2, v2);
    v3 = end - uvec * half;
    rotate_vertex(&p3, v3);

    prim_begin(1, (start * 0.5f + end * 0.5f - View_position).rotated(View_matrix).z, 0, 0.0f);
    prim_vertex(&p0);
    prim_vertex(&p2);
    prim_vertex(&p3);
    prim_vertex(&p1);
    prim_layer(mode, gr_screen.current_bitmap, gr_screen.current_color.r, gr_screen.current_color.g,
               gr_screen.current_color.b, gr_screen.current_color.a, 0);
    prim_uv(u, v);
    prim_uv(u, 1.0f + v);
    prim_uv(1.0f + u, 1.0f + v);
    prim_uv(1.0f + u, v);
    prim_end();
    return 1;
}
