// Primitive builder: polygons given point by point with any number of layers, clipped (with the
// values of every layer) to the view volume and drawn, or recorded into a command buffer and
// replayed later.
#include "game/unit_001843A0.h"
#include "game/unit_0017CEB0.h"

vertex Prim_clip_vertices[PRIM_MAX_CLIP_POINTS];
PolyPoint Prim_clip_points[PRIM_MAX_CLIP_POINTS];
PolyPoint* Prim_free_clip_points[PRIM_MAX_CLIP_POINTS];
PolyPoint Prim_points[PRIM_MAX_POINTS];
PolyLayer Prim_layers[POLY_MAX_LAYERS];
u8 Prim_buffer[PRIM_BUFFER_SIZE];
u8* Prim_lists[PRIM_MAX_LISTS];

u8* Prim_buffer_limit = Prim_buffer + PRIM_BUFFER_SIZE - PRIM_BUFFER_SLACK;
u8* Prim_buffer_ptr = Prim_buffer;

int Prim_num_clip_points;
u8 Prim_clipper_initialized;
u8 Prim_active;
int Prim_num_points;
int Prim_num_layers;
u8 Prim_use_depth;
f32 Prim_depth;
PolyLayer* Prim_cur_layer;
int Prim_cur_layer_index;
PolyPoint* Prim_uv_point;
PolyPoint* Prim_rgb_point;
PolyPoint* Prim_alpha_point;
u8 Prim_recording;
int Prim_num_lists;

static inline f32 lerp(f32 from, f32 to, f32 t) { return from + t * (to - from); }

// The next free point for prim_clip_edge. prim_draw frees them all once the polygon is drawn.
static inline PolyPoint* get_temp_point() { return Prim_free_clip_points[Prim_num_clip_points++]; }

PolyPoint* prim_clip_edge(int plane, PolyPoint* on_pnt, PolyPoint* off_pnt) {
    f32 ratio;
    PolyPoint* tmp;
    vertex* on;
    vertex* off;
    vertex* v;
    int i;

    tmp = get_temp_point();
    tmp->vert->proj_flags = 0;
    on = on_pnt->vert;
    v = tmp->vert;
    off = off_pnt->vert;
    if (plane & CC_OFF_FAR) {
        f32 d = D_002D46EC - on->pos.z;
        f32 dz = off->pos.z - on->pos.z;
        if (dz == 0.0f) {
            ratio = 1.0f;
        } else {
            ratio = d / dz;
        }
        v->pos.x = lerp(on->pos.x, off->pos.x, ratio);
        v->pos.y = lerp(on->pos.y, off->pos.y, ratio);
        v->pos.z = D_002D46EC;
    } else if (plane & CC_OFF_NEAR) {
        f32 d = D_002D46F4 - on->pos.z;
        f32 dz = off->pos.z - on->pos.z;
        if (dz == 0.0f) {
            ratio = 1.0f;
        } else {
            ratio = d / dz;
        }
        v->pos.x = lerp(on->pos.x, off->pos.x, ratio);
        v->pos.y = lerp(on->pos.y, off->pos.y, ratio);
        v->pos.z = D_002D46F4;
    } else {
        // The side planes are x = z, x = -z, y = z and y = -z.
        f32 a, b, kn, kd;

        if (plane & (CC_OFF_LEFT | CC_OFF_RIGHT)) {
            a = on->pos.x;
            b = off->pos.x;
        } else {
            a = on->pos.y;
            b = off->pos.y;
        }
        if (plane & (CC_OFF_LEFT | CC_OFF_BOT)) {
            a = -a;
            b = -b;
        }
        kn = a - on->pos.z;
        kd = kn - b + off->pos.z;
        ratio = kn / kd;
        v->pos.x = lerp(on->pos.x, off->pos.x, ratio);
        v->pos.y = lerp(on->pos.y, off->pos.y, ratio);
        if (plane & (CC_OFF_BOT | CC_OFF_TOP)) {
            v->pos.z = v->pos.y;
        } else {
            v->pos.z = v->pos.x;
        }
        if (plane & (CC_OFF_LEFT | CC_OFF_BOT)) {
            v->pos.z = -v->pos.z;
        }
    }
    code_vertex(v);

    for (i = 0; i < Prim_num_layers; i++) {
        PolyLayer* layer = &Prim_layers[i];
        if (layer->has_uv) {
            tmp->layers[i].u = lerp(on_pnt->layers[i].u, off_pnt->layers[i].u, ratio);
            tmp->layers[i].v = lerp(on_pnt->layers[i].v, off_pnt->layers[i].v, ratio);
        }
        if (layer->has_rgb) {
            f32 on_r, on_b, on_g;
            f32 off_r, off_b, off_g;

            on_r = on_pnt->layers[i].r;
            off_r = off_pnt->layers[i].r;
            on_g = on_pnt->layers[i].g;
            off_g = off_pnt->layers[i].g;
            on_b = on_pnt->layers[i].b;
            off_b = off_pnt->layers[i].b;
            tmp->layers[i].r = (int)lerp(on_r, off_r, ratio);
            tmp->layers[i].g = (int)lerp(on_g, off_g, ratio);
            tmp->layers[i].b = (int)lerp(on_b, off_b, ratio);
        }
        if (layer->has_alpha) {
            f32 on_a, off_a;

            on_a = on_pnt->layers[i].a;
            off_a = off_pnt->layers[i].a;
            if (D_002D46E0 && (plane & CC_OFF_FAR) && off_a == 0.0f) {
                tmp->layers[i].a = 0;
            } else if (plane & CC_OFF_NEAR) {
                // The near plane keeps the larger alpha.
                tmp->layers[i].a = (int)(on_a > off_a ? on_a : off_a);
            } else {
                tmp->layers[i].a = (int)lerp(on_a, off_a, ratio);
            }
        }
    }
    return tmp;
}

int prim_clip_plane(int plane, PolyPoint** points, PolyPoint** out, int* count, PrimOutcodes* codes) {
    PolyPoint** start = out;
    int i;

    points[*count] = points[0];
    points[*count + 1] = points[1];
    codes->all = 0xFF;
    codes->any = 0;
    for (i = 1; i <= *count; i++) {
        if (plane & points[i]->vert->codes) {
            if (!(plane & points[i - 1]->vert->codes)) {
                *out = prim_clip_edge(plane, points[i - 1], points[i]);
                codes->any |= (*out)->vert->codes;
                codes->all &= (*out)->vert->codes;
                out++;
            }
            if (!(plane & points[i + 1]->vert->codes)) {
                *out = prim_clip_edge(plane, points[i + 1], points[i]);
                codes->any |= (*out)->vert->codes;
                codes->all &= (*out)->vert->codes;
                out++;
            }
        } else {
            *out++ = points[i];
            codes->any |= points[i]->vert->codes;
            codes->all &= points[i]->vert->codes;
        }
    }
    return out - start;
}

PolyPoint** prim_clip_polygon(PolyPoint** points, PolyPoint** temp, int* count, PrimOutcodes* codes) {
    int plane;

    // Bit 0x40 is not a plane; code_vertex never sets it.
    for (plane = 1; plane <= 0x40; plane <<= 1) {
        if (codes->any & plane) {
            PolyPoint** t;
            *count = prim_clip_plane(plane, points, temp, count, codes);
            if (codes->all) {
                return temp;
            }
            t = points;
            points = temp;
            temp = t;
        }
    }
    return points;
}

int prim_draw(PrimOutcodes* codes, PolyPoint** points, PolyPoint** temp) {
    int count;
    int i;
    PolyPoint** list;
    PolyPoint* p;

    list = points;
    count = Prim_num_points;
    if (Prim_num_layers == 0) {
        return 0;
    }
    if (codes->any) {
        list = prim_clip_polygon(points, temp, &count, codes);
        if (count == 0 || codes->all) {
            goto done;
        }
    }
    for (i = 0; i < count; i++) {
        p = list[i];
        if (!(p->vert->proj_flags & PF_PROJECTED)) {
            project_vertex(p->vert);
        }
        if (p->vert->proj_flags & PF_OVERFLOW) {
            goto done;
        }
        if (Prim_use_depth) {
            p->vert->pos.z = Prim_depth;
            p->vert->sw = 1.0f / Prim_depth;
        }
    }
    gr_poly_layers(count, list, Prim_num_layers, Prim_layers);
done:
    Prim_num_clip_points = 0;
    return 1;
}

void prim_init(void) {
    int i;

    Prim_num_clip_points = 0;
    for (i = 0; i < PRIM_MAX_CLIP_POINTS; i++) {
        Prim_free_clip_points[i] = &Prim_clip_points[i];
        Prim_free_clip_points[i]->vert = &Prim_clip_vertices[i];
    }
}

void prim_begin(int deferred, f32 max_z, int use_depth, f32 depth) {
    u8* p;

    if (deferred) {
        if (Prim_buffer_ptr >= Prim_buffer_limit || Prim_num_lists >= PRIM_MAX_LISTS - 1) {
            prim_flush();
        }
        Prim_recording = deferred;
        p = Prim_buffer_ptr;
        Prim_lists[Prim_num_lists] = p;
        Prim_num_lists++;
        if (use_depth) {
            ((PrimCmdBeginDepth*)p)->type = PRIM_CMD_BEGIN_DEPTH;
            ((PrimCmdBeginDepth*)p)->depth = depth;
            p += sizeof(PrimCmdBeginDepth);
        } else {
            ((PrimCmdBegin*)p)->type = PRIM_CMD_BEGIN;
            p += sizeof(PrimCmdBegin);
        }
        Prim_buffer_ptr = p;
    } else {
        Prim_use_depth = use_depth;
        Prim_active = true;
        Prim_depth = depth;
        Prim_num_points = 0;
        Prim_num_layers = 0;
        Prim_alpha_point = Prim_rgb_point = Prim_uv_point = Prim_points;
    }
}

void prim_vertex(vertex* v) {
    if (Prim_recording) {
        PrimCmdVertex* cmd = (PrimCmdVertex*)Prim_buffer_ptr;
        cmd->type = PRIM_CMD_VERTEX;
        cmd->codes = v->codes;
        cmd->x = v->pos.x;
        cmd->y = v->pos.y;
        cmd->z = v->pos.z;
        Prim_buffer_ptr = (u8*)(cmd + 1);
    } else {
        PolyPoint* point = &Prim_points[Prim_num_points++];
        point->vert = v;
    }
}

void prim_layer(gr_mode mode, int bitmap, u8 r, u8 g, u8 b, u8 a, u8 flag) {
    if (Prim_recording) {
        PrimCmdLayer* cmd = (PrimCmdLayer*)Prim_buffer_ptr;
        cmd->type = PRIM_CMD_LAYER;
        cmd->flag = flag;
        cmd->bitmap = bitmap;
        cmd->mode = mode;
        cmd->r = r;
        cmd->g = g;
        cmd->b = b;
        cmd->a = a;
        Prim_buffer_ptr = (u8*)(cmd + 1);
    } else {
        PolyLayer* layer;

        Prim_cur_layer_index = Prim_num_layers;
        layer = &Prim_layers[Prim_num_layers];
        Prim_cur_layer = layer;
        Prim_num_layers++;
        layer->mode = mode;
        layer->bitmap = bitmap;
        layer->has_uv = false;
        layer->has_rgb = false;
        layer->has_alpha = false;
        layer->flag = flag;
        layer->r = r;
        layer->g = g;
        layer->b = b;
        layer->a = a;
        Prim_alpha_point = Prim_rgb_point = Prim_uv_point = Prim_points;
    }
}

void prim_uv(f32 u, f32 v) {
    if (Prim_recording) {
        PrimCmdUV* cmd = (PrimCmdUV*)Prim_buffer_ptr;
        cmd->type = PRIM_CMD_UV;
        cmd->u = u;
        cmd->v = v;
        Prim_buffer_ptr = (u8*)(cmd + 1);
    } else {
        PolyPointLayer* values;

        Prim_cur_layer->has_uv = true;
        values = &Prim_uv_point->layers[Prim_cur_layer_index];
        values->u = u;
        values->v = v;
        Prim_uv_point++;
    }
}

void prim_rgb(u8 r, u8 g, u8 b) {
    if (Prim_recording) {
        PrimCmdRGB* cmd = (PrimCmdRGB*)Prim_buffer_ptr;
        cmd->type = PRIM_CMD_RGB;
        cmd->r = r;
        cmd->g = g;
        cmd->b = b;
        Prim_buffer_ptr = (u8*)(cmd + 1);
    } else {
        PolyPointLayer* values;

        Prim_cur_layer->has_rgb = true;
        values = &Prim_rgb_point->layers[Prim_cur_layer_index];
        values->r = r;
        values->g = g;
        values->b = b;
        Prim_rgb_point++;
    }
}

void prim_alpha(u8 a) {
    if (Prim_recording) {
        PrimCmdAlpha* cmd = (PrimCmdAlpha*)Prim_buffer_ptr;
        cmd->type = PRIM_CMD_ALPHA;
        cmd->a = a;
        Prim_buffer_ptr = (u8*)(cmd + 1);
    } else {
        Prim_cur_layer->has_alpha = true;
        Prim_alpha_point->layers[Prim_cur_layer_index].a = a;
        Prim_alpha_point++;
    }
}

u8 prim_end() {
    PolyPoint* points[PRIM_MAX_POINTS];
    PolyPoint* temp[PRIM_MAX_POINTS];
    PrimOutcodes codes;
    PolyPoint** list;
    int i;
    int count;

    if (Prim_recording) {
        PrimCmdEnd* cmd = (PrimCmdEnd*)Prim_buffer_ptr;
        cmd->type = PRIM_CMD_END;
        Prim_buffer_ptr = (u8*)(cmd + 1);
        Prim_recording = false;
        return 1;
    }
    Prim_active = false;
    codes.any = 0;
    codes.all = 0xFF;
    count = Prim_num_points;
    list = points;
    for (i = 0; i < count; i++) {
        list[i] = &Prim_points[i];
    }
    for (i = 0; i < count; i++) {
        PolyPoint* point = list[i];
        codes.all &= point->vert->codes;
        codes.any |= point->vert->codes;
    }
    if (codes.all) {
        return 0;
    }
    return prim_draw(&codes, points, temp);
}

void prim_flush() {
    int num_verts;
    u8 tint;
    int i;
    u8* p;
    bool done;

    if (Prim_buffer_ptr != Prim_buffer) {
        num_verts = 0;
        tint = gr_screen.tint_enabled;
        gr_screen.tint_enabled = 0;
        vertex verts[PRIM_MAX_REPLAY_VERTS];
        for (i = 0; i < Prim_num_lists; i++) {
            done = false;
            p = Prim_lists[i];
            do {
                switch (*p) {
                case PRIM_CMD_BEGIN: {
                    f32 max_z = ((PrimCmdBegin*)p)->max_z;
                    p += sizeof(PrimCmdBegin);
                    num_verts = 0;
                    prim_begin(false, max_z, false, 0.0f);
                    break;
                }
                case PRIM_CMD_BEGIN_DEPTH: {
                    f32 max_z = ((PrimCmdBeginDepth*)p)->max_z;
                    f32 depth = ((PrimCmdBeginDepth*)p)->depth;
                    p += sizeof(PrimCmdBeginDepth);
                    num_verts = 0;
                    prim_begin(false, max_z, true, depth);
                    break;
                }
                case PRIM_CMD_VERTEX: {
                    vertex* v = &verts[num_verts++];
                    v->codes = ((PrimCmdVertex*)p)->codes;
                    v->pos.x = ((PrimCmdVertex*)p)->x;
                    v->pos.y = ((PrimCmdVertex*)p)->y;
                    v->pos.z = ((PrimCmdVertex*)p)->z;
                    v->proj_flags = 0;
                    p += sizeof(PrimCmdVertex);
                    prim_vertex(v);
                    break;
                }
                case PRIM_CMD_LAYER: {
                    u8 flag = ((PrimCmdLayer*)p)->flag;
                    int bitmap = ((PrimCmdLayer*)p)->bitmap;
                    gr_mode mode = ((PrimCmdLayer*)p)->mode;
                    u8 r = ((PrimCmdLayer*)p)->r;
                    u8 g = ((PrimCmdLayer*)p)->g;
                    u8 b = ((PrimCmdLayer*)p)->b;
                    u8 a = ((PrimCmdLayer*)p)->a;
                    p += sizeof(PrimCmdLayer);
                    prim_layer(mode, bitmap, r, g, b, a, flag);
                    break;
                }
                case PRIM_CMD_UV: {
                    f32 u = ((PrimCmdUV*)p)->u;
                    f32 v = ((PrimCmdUV*)p)->v;
                    p += sizeof(PrimCmdUV);
                    prim_uv(u, v);
                    break;
                }
                case PRIM_CMD_RGB: {
                    u8 r = ((PrimCmdRGB*)p)->r;
                    u8 g = ((PrimCmdRGB*)p)->g;
                    u8 b = ((PrimCmdRGB*)p)->b;
                    p += sizeof(PrimCmdRGB);
                    prim_rgb(r, g, b);
                    break;
                }
                case PRIM_CMD_ALPHA: {
                    u8 a = ((PrimCmdAlpha*)p)->a;
                    p += sizeof(PrimCmdAlpha);
                    prim_alpha(a);
                    break;
                }
                case PRIM_CMD_END:
                    prim_end();
                    done = true;
                    break;
                }
            } while (!done);
        }
        gr_screen.tint_enabled = tint;
        Prim_buffer_ptr = Prim_buffer;
        Prim_num_lists = 0;
    }
}

// The file ends with the constructor of PolyLayer, which the static initializer needs for
// Prim_layers.
