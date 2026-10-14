// 3D view: the view matrix, the instance stack and the planes of the view volume; moving points
// into view space and projecting them; culling spheres and boxes against the view.
#include "game/unit_0017CEB0.h"
#include "game/unit_001843A0.h"
#include "game/unit_00187F80.h"

#define PI 3.1415927f

// atanf and tanf (libm).
extern "C" float func_001CBE30(float x);
extern "C" float func_001CC5A0(float x);

// Float max and min with the R5900 FPU's max.s and min.s, each followed by a nop.
inline float float_max(register float a, register float b) {
    register float result;
    asm {
        max.s result, a, b
        nop
    }
    return result;
}

inline float float_min(register float a, register float b) {
    register float result;
    asm {
        min.s result, a, b
        nop
    }
    return result;
}

f32 D_002D3ED8 = 1000.0f;
u8 Clip_codes_enabled = 0xFF;

f32 D_002D46B8;
int Canvas_width;
int Canvas_height;
f32 Canvas_w2;
f32 Canvas_h2;
f32 D_002D46CC;
f32 D_002D46D0;
f32 D_002D46D4;
f32 D_002D46D8;
int D_002D46DC;
u8 D_002D46E0;
u8 D_002D46E4;
f32 D_002D46E8;
f32 D_002D46EC;
f32 D_002D46F0;
f32 D_002D46F4;
f32 D_002D46F8;
int Instance_depth;
int Num_clip_planes;

matrix3 View_matrix;
matrix3 Unscaled_matrix;
vector3 View_position __attribute__((aligned(16)));
matrix3 Object_matrix;
vector3 Object_position;
matrix3 Eye_matrix;
vector3 Eye_position;
vector3 Matrix_scale;
clip_plane D_00885B60[MAX_CLIP_PLANES];
vertex* D_00885C40[28];
vertex* D_00885CB0[28];
view_instance Instance_stack[MAX_INSTANCE_DEPTH];
plane Guard_band_planes[5];
plane Cull_planes[5];
clip_plane Clip_planes[MAX_CLIP_PLANES];
view_state Saved_view_state;
f32 View_matrix_vu[3][4];

// The side planes go through the eye and the corners of the view at CLIP_PLANE_DISTANCE; the
// near plane goes through the eye.
void update_clip_planes(void) {
    float dist;
    vector3 eye;
    float right;
    float left;
    float top;
    float bottom;
    vector3 center;
    vector3 top_right;
    vector3 top_left;
    vector3 bottom_left;
    vector3 bottom_right;

    dist = CLIP_PLANE_DISTANCE;
    D_002D46EC = Matrix_scale.z * D_002D46E8;
    D_002D46F4 = Matrix_scale.z * D_002D46F0;
    eye = View_position;
    right = dist * Matrix_scale.z / Matrix_scale.x;
    left = -right;
    top = dist * Matrix_scale.z / Matrix_scale.y;
    bottom = -top;
    center = Unscaled_matrix.fvec * dist + View_position;
    top_right = Unscaled_matrix.rvec * right + Unscaled_matrix.uvec * top + center;
    top_left = Unscaled_matrix.rvec * left + Unscaled_matrix.uvec * top + center;
    bottom_left = Unscaled_matrix.rvec * left + Unscaled_matrix.uvec * bottom + center;
    bottom_right = Unscaled_matrix.rvec * right + Unscaled_matrix.uvec * bottom + center;

    Num_clip_planes = 0;
    Clip_planes[Num_clip_planes++].set(CC_OFF_LEFT, eye, bottom_left, top_left);
    Clip_planes[Num_clip_planes++].set(CC_OFF_RIGHT, eye, top_right, bottom_right);
    Clip_planes[Num_clip_planes++].set(CC_OFF_BOT, eye, bottom_right, bottom_left);
    Clip_planes[Num_clip_planes++].set(CC_OFF_TOP, eye, top_left, top_right);
    Clip_planes[Num_clip_planes++].set(CC_BEHIND, Unscaled_matrix.fvec * -1.0f, View_position);
    if (D_002D46E4) {
        Clip_planes[Num_clip_planes++].set(CC_OFF_FAR, Unscaled_matrix.fvec,
                                           View_position + Unscaled_matrix.fvec * D_002D46E8);
    }
}

void set_view_matrix(const matrix3* orient, const vector3* position, float zoom) {
    float aspect;
    float far_distance;
    float scale;

    Instance_depth = 0;
    Num_clip_vertices = 0;
    Canvas_width = gr_screen.clip_width;
    Canvas_height = gr_screen.clip_height;
    Canvas_w2 = 0.5f * (float)Canvas_width;
    Canvas_h2 = 0.5f * (float)Canvas_height;
    aspect = gr_screen.aspect * (float)Canvas_height / (float)Canvas_width;

    // The primitive builder's clip points are linked up the first time a view is set.
    if (!Prim_clipper_initialized) {
        Prim_clipper_initialized = 1;
        prim_init();
    }
    // A field of view in degrees becomes the tangent of half of it.
    zoom = zoom < 2.0f ? zoom : func_001CC5A0(0.5f * (PI * zoom / 180.0f));

    View_position = *position;
    View_matrix = *orient;
    Eye_matrix = View_matrix;
    Eye_position = View_position;
    Object_matrix.make_identity();
    Object_position = vector3(0.0f, 0.0f, 0.0f);
    // View z is scaled so that the far distance (250 to 1000) lands on 0.98.
    D_002D46F8 = 0.98f;
    far_distance = float_min(float_max(250.0f, D_002D46E8), 1000.0f);
    scale = D_002D46F8 / far_distance;
    Matrix_scale.z = scale;
    Matrix_scale.x = scale / zoom;
    Matrix_scale.y = scale / (aspect * zoom);
    D_002D3ED8 = far_distance;
    D_002D46B8 = 1.0f / Matrix_scale.z;

    Unscaled_matrix = View_matrix;
    View_matrix.rvec.x *= Matrix_scale.x;
    View_matrix.rvec.y *= Matrix_scale.x;
    View_matrix.rvec.z *= Matrix_scale.x;
    View_matrix.uvec.x *= Matrix_scale.y;
    View_matrix.uvec.y *= Matrix_scale.y;
    View_matrix.uvec.z *= Matrix_scale.y;
    View_matrix.fvec.x *= Matrix_scale.z;
    View_matrix.fvec.y *= Matrix_scale.z;
    View_matrix.fvec.z *= Matrix_scale.z;
    // The screen centre and half size in GS coordinates (12.4 fixed point).
    D_002D46CC = (float)Gs_state.offset_x + 16.0f * (Canvas_w2 + (float)gr_screen.offset_x);
    D_002D46D0 = (float)Gs_state.offset_y + 16.0f * (Canvas_h2 + (float)gr_screen.offset_y);
    D_002D46D4 = 16.0f * Canvas_w2;
    D_002D46D8 = 16.0f * -Canvas_h2;
    // The rows again in the layout VU0 code loads.
    View_matrix_vu[0][0] = View_matrix.rvec.x;
    View_matrix_vu[0][1] = View_matrix.rvec.y;
    View_matrix_vu[0][2] = View_matrix.rvec.z;
    View_matrix_vu[1][0] = View_matrix.uvec.x;
    View_matrix_vu[1][1] = View_matrix.uvec.y;
    View_matrix_vu[1][2] = View_matrix.uvec.z;
    View_matrix_vu[2][0] = View_matrix.fvec.x;
    View_matrix_vu[2][1] = View_matrix.fvec.y;
    View_matrix_vu[2][2] = View_matrix.fvec.z;
    update_clip_planes();
}

void get_view_fov(float* fov_x, float* fov_y) {
    *fov_x = 180.0f * (2.0f * func_001CBE30(Matrix_scale.z / Matrix_scale.x)) / PI;
    *fov_y = 180.0f * (2.0f * func_001CBE30(Matrix_scale.z / Matrix_scale.y)) / PI;
}

void start_instance(const vector3* position, const matrix3* orient) {
    view_instance* inst;

    Clip_codes_enabled = 0xFF;
    inst = &Instance_stack[Instance_depth];
    inst->position = View_position;
    inst->orient = View_matrix;
    View_position.x -= position->x;
    View_position.y -= position->y;
    View_position.z -= position->z;
    View_position.rotate(*orient);
    View_matrix = View_matrix * orient->transpose();
    Instance_depth++;
}

void done_instance(void) {
    Instance_depth--;
    View_position = Instance_stack[Instance_depth].position;
    View_matrix = Instance_stack[Instance_depth].orient;
}

void set_far_clip(u8 value, float distance) {
    D_002D46E0 = value;
    if (distance > 1.0f) {
        D_002D46E4 = 1;
        D_002D46E8 = distance;
    } else {
        D_002D46E4 = 0;
        D_002D46E8 = 0.0f;
    }
    update_clip_planes();
}

void enable_clip_codes(void) { Clip_codes_enabled = 0xFF; }

u8 code_vertex(vertex* p) {
    u8 codes = 0;
    if (Clip_codes_enabled) {
        if (p->pos.x > p->pos.z) {
            codes |= CC_OFF_RIGHT;
        }
        if (p->pos.y > p->pos.z) {
            codes |= CC_OFF_TOP;
        }
        if (p->pos.x < -p->pos.z) {
            codes |= CC_OFF_LEFT;
        }
        if (p->pos.y < -p->pos.z) {
            codes |= CC_OFF_BOT;
        }
        if (p->pos.z <= 0.0f) {
            codes |= CC_BEHIND;
        }
        if (D_002D46E4 && p->pos.z > D_002D46EC) {
            codes |= CC_OFF_FAR;
        }
    }
    return p->codes = codes;
}

u8 rotate_vertex(vertex* dest, const vector3& src) {
    vector3 rel = src - View_position;
    dest->pos = rel.rotated(View_matrix);
    dest->proj_flags = 0;
    return code_vertex(dest);
}

void rotate_vertices(vertex* dest, const vector3* src, int count) {
    while (count--) {
        vector3 rel = *src - View_position;
        dest->pos = rel.rotated(View_matrix);
        dest->proj_flags = 0;
        code_vertex(dest);
        src++;
        dest++;
    }
}

void project_vertex(vertex* p) {
    if (p->proj_flags & (PF_PROJECTED | PF_OVERFLOW)) {
        return;
    }
    if (p->pos.z <= 0.0f) {
        p->proj_flags |= PF_OVERFLOW;
    } else {
        p->proj_flags |= PF_PROJECTED;
        float w = 1.0f / p->pos.z;
        p->sw = w;
        float x = 1.0f + p->pos.x * w;
        float y = 1.0f - p->pos.y * w;
        p->sx = gr_screen.offset_x + x * Canvas_w2;
        p->sy = gr_screen.offset_y + y * Canvas_h2;
    }
}

BOOL check_normal_facing(const vector3* normal, const vector3* point) {
    if ((View_position - *point).dot(*normal) > 0.0f) {
        return TRUE;
    }
    return FALSE;
}

void screen_point_to_ray(ray* r, float sx, float sy) {
    vector3 v;
    v.x = (sx - Canvas_w2) / Canvas_w2;
    v.y = -(sy - Canvas_h2) / Canvas_h2;
    v.z = 1.0f;
    v.x = v.x * Matrix_scale.z / Matrix_scale.x;
    v.y = v.y * Matrix_scale.z / Matrix_scale.y;
    v.normalize();
    v.unrotate(Unscaled_matrix);
    r->origin = Eye_position;
    r->dir = v;
}

float get_screen_size(const vertex* p, const float& size) { return Canvas_w2 * (size * Matrix_scale.x) / p->pos.z; }

float scale_view_depth(float size) { return size * Matrix_scale.z; }

// The view setup always adds the side and near planes, so the plane loops below don't check for
// an empty list.
BOOL sphere_outside_view(const vector3* center, float radius) {
    int count = Num_clip_planes;
    int i = 0;
    do {
        clip_plane* cp = &Clip_planes[i];
        if (cp->p.d + center->dot(cp->p.normal) > radius) {
            return TRUE;
        }
        i++;
    } while (i < count);
    return FALSE;
}

int get_clip_planes(plane* planes, u8* codes) {
    for (int i = 0; i < Num_clip_planes; i++) {
        planes[i] = Clip_planes[i].p;
    }
    if (codes) {
        for (int i = 0; i < Num_clip_planes; i++) {
            codes[i] = Clip_planes[i].code;
        }
    }
    return Num_clip_planes;
}

BOOL box_outside_view(const vector3* min, const vector3* max) {
    int count = Num_clip_planes;
    int i = 0;
    do {
        clip_plane* cp = &Clip_planes[i];
        vector3 corner;
        get_box_corner(corner, cp->near_corner, *min, *max);
        if (cp->p.d + corner.dot(cp->p.normal) > 0.0f) {
            return TRUE;
        }
        i++;
    } while (i < count);
    return FALSE;
}

vector3* copy_vector3(vector3* dest, const vector3* src) {
    *dest = *src;
    return dest;
}

void get_eye(vector3* position, matrix3* orient) {
    *position = Eye_position;
    *orient = Eye_matrix;
}

void save_view_state(void) {
    int i;
    view_state* s = &Saved_view_state;

    s->view_matrix = View_matrix;
    s->unscaled_matrix = Unscaled_matrix;
    s->view_position = View_position;
    s->eye_matrix = Eye_matrix;
    s->eye_position = Eye_position;
    s->matrix_scale = Matrix_scale;
    s->unk98 = D_002D46B8;
    s->canvas_width = Canvas_width;
    s->canvas_height = Canvas_height;
    s->canvas_w2 = Canvas_w2;
    s->canvas_h2 = Canvas_h2;
    s->unkAC = D_002D46CC;
    s->unkB0 = D_002D46D0;
    s->unkB4 = D_002D46D4;
    s->unkB8 = D_002D46D8;
    s->num_unk_planes = D_002D46DC;
    for (i = 0; i < D_002D46DC; i++) {
        s->unk_planes[i] = D_00885B60[i];
    }
    s->unk1A0 = D_002D46E0;
    s->unk1A1 = D_002D46E4;
    s->unk1A4 = D_002D46E8;
    s->unk1A8 = D_002D46EC;
    s->unk1AC = D_002D3ED8;
    s->unk1B0 = D_002D46F8;
    s->instance_depth = Instance_depth;
    for (i = 0; i < Instance_depth; i++) {
        s->instance_stack[i] = Instance_stack[i];
    }
    s->num_clip_planes = Num_clip_planes;
    for (i = 0; i < Num_clip_planes; i++) {
        s->clip_planes[i] = Clip_planes[i];
    }
}

void restore_view_state(void) {
    int i;
    view_state* s = &Saved_view_state;

    View_matrix = s->view_matrix;
    Unscaled_matrix = s->unscaled_matrix;
    View_position = s->view_position;
    Eye_matrix = s->eye_matrix;
    Eye_position = s->eye_position;
    Matrix_scale = s->matrix_scale;
    D_002D46B8 = s->unk98;
    Canvas_width = s->canvas_width;
    Canvas_height = s->canvas_height;
    Canvas_w2 = s->canvas_w2;
    Canvas_h2 = s->canvas_h2;
    D_002D46CC = s->unkAC;
    D_002D46D0 = s->unkB0;
    D_002D46D4 = s->unkB4;
    D_002D46D8 = s->unkB8;
    D_002D46DC = s->num_unk_planes;
    for (i = 0; i < D_002D46DC; i++) {
        D_00885B60[i] = s->unk_planes[i];
    }
    D_002D46E0 = s->unk1A0;
    D_002D46E4 = s->unk1A1;
    D_002D46E8 = s->unk1A4;
    D_002D46EC = s->unk1A8;
    D_002D3ED8 = s->unk1AC;
    D_002D46F8 = s->unk1B0;
    Instance_depth = s->instance_depth;
    for (i = 0; i < Instance_depth; i++) {
        Instance_stack[i] = s->instance_stack[i];
    }
    Num_clip_planes = s->num_clip_planes;
    for (i = 0; i < Num_clip_planes; i++) {
        Clip_planes[i] = s->clip_planes[i];
    }

    View_matrix_vu[0][0] = View_matrix.rvec.x;
    View_matrix_vu[0][1] = View_matrix.rvec.y;
    View_matrix_vu[0][2] = View_matrix.rvec.z;
    View_matrix_vu[1][0] = View_matrix.uvec.x;
    View_matrix_vu[1][1] = View_matrix.uvec.y;
    View_matrix_vu[1][2] = View_matrix.uvec.z;
    View_matrix_vu[2][0] = View_matrix.fvec.x;
    View_matrix_vu[2][1] = View_matrix.fvec.y;
    View_matrix_vu[2][2] = View_matrix.fvec.z;
}

int sphere_visibility(const vector3* center, float radius) {
    plane* p;
    int i;

    for (p = Cull_planes, i = 5; i != 0; i--, p++) {
        if (p->d + center->dot(p->normal) >= -0.0001f + radius) {
            return 0;
        }
    }
    for (p = Guard_band_planes, i = 5; i != 0; i--, p++) {
        if (p->d + center->dot(p->normal) >= -0.0001f - radius) {
            return SPHERE_VISIBLE | SPHERE_CLIPPED;
        }
    }
    return SPHERE_VISIBLE;
}

void update_cull_planes(void) {
    plane planes[MAX_CLIP_PLANES];
    u8 codes[MAX_CLIP_PLANES];
    int count;
    int i;
    float fov_x;
    float fov_y;

    // The edges of the view come from the clip planes, by their vertex::codes bit.
    count = get_clip_planes(planes, codes);
    for (i = 0; i < count; i++) {
        switch (codes[i]) {
        case CC_OFF_LEFT:
            Cull_planes[3] = planes[i];
            break;
        case CC_OFF_RIGHT:
            Cull_planes[4] = planes[i];
            break;
        case CC_OFF_BOT:
            Cull_planes[2] = planes[i];
            break;
        case CC_OFF_TOP:
            Cull_planes[1] = planes[i];
            break;
        }
    }

    // Half the width and height of the guard band, one unit in front of the eye.
    get_view_fov(&fov_x, &fov_y);
    fov_x = 0.5f * (PI * fov_x / 180.0f);
    fov_y = 0.5f * (PI * fov_y / 180.0f);
    fov_x = GUARD_BAND_X * func_001CC5A0(fov_x);
    fov_y = GUARD_BAND_Y * func_001CC5A0(fov_y);

    vector3 center = Eye_position + Eye_matrix.fvec;
    vector3 right = Eye_position + Eye_matrix.rvec;
    vector3 up = Eye_position + Eye_matrix.uvec;

    set_plane(&Guard_band_planes[1], center + Eye_matrix.uvec * fov_y, right, Eye_position);
    set_plane(&Guard_band_planes[2], center - Eye_matrix.uvec * fov_y, right, Eye_position);
    set_plane(&Guard_band_planes[4], center + Eye_matrix.rvec * fov_x, up, Eye_position);
    set_plane(&Guard_band_planes[3], center - Eye_matrix.rvec * fov_x, up, Eye_position);
    flip_plane(&Guard_band_planes[2]);
    flip_plane(&Guard_band_planes[4]);
    // One unit behind the eye.
    set_plane(&Guard_band_planes[0], Eye_matrix.fvec, Eye_position - Eye_matrix.fvec * 1.0f);
    flip_plane(&Guard_band_planes[0]);

    Cull_planes[0] = Guard_band_planes[0];
}

void sort_back_to_front(depth_entry* entries, int count) {
    int i;
    int j;
    int gap;
    depth_entry temp;

    if (count <= 0) {
        return;
    }
    for (gap = count / 2; gap > 0; gap /= 2) {
        for (i = gap; i < count; i++) {
            for (j = i - gap; j >= 0; j -= gap) {
                if (entries[j].z >= (entries + gap)[j].z) {
                    break;
                }
                temp = entries[j];
                entries[j] = entries[j + gap];
                entries[j + gap] = temp;
            }
        }
    }
}

void local_to_world(const matrix3* orient, const vector3* position, matrix3* local_orient, vector3* local_position) {
    *local_orient = *local_orient * *orient;
    local_position->unrotate(*orient);
    *local_position += *position;
}

DrawParams::DrawParams() {
    flags = 0;
    unk04 = 0;
    alpha = 0xFF;
    unk10 = 0;
    brightness = 1.0f;
    unk1C = 0;
    scroll_u = 0.0f;
    scroll_v = 0.0f;
    unk28 = 1.0f;
}

BOOL project_point(const vector3* point, float* sx, float* sy, float* sw) {
    vertex v;

    if (rotate_vertex(&v, *point) == 0) {
        project_vertex(&v);
        if (v.proj_flags & PF_PROJECTED) {
            *sx = v.sx;
            *sy = v.sy;
            *sw = v.sw;
            return TRUE;
        }
    }
    return FALSE;
}

// The file ends with the implicit constructors of plane, view_instance and clip_plane, in that
// order, which the static initializer needs for the file's arrays of them.
