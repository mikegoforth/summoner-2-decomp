#ifndef GAME_GR_H
#define GAME_GR_H

#include "types.h"
#include "game/unit_00196B10.h"

// gr_screen.window_mode values accepted by gr_init.
#define GR_WINDOW_MODE_C8 0xC8
#define GR_WINDOW_MODE_C9 0xC9
#define GR_WINDOW_MODE_CA 0xCA

// gr_fade_in / gr_fade_out progress (Gr_fade_state).
#define GR_FADE_NONE 0
#define GR_FADE_IN   1
#define GR_FADE_OUT  2

// vertex::codes: the sides of the view volume a point is outside of. In view space, before the
// perspective divide, a point is inside when -z <= x <= z and -z <= y <= z. The first six also
// name the planes that clipping works against.
#define CC_OFF_NEAR  0x01 // closer than the near plane
#define CC_OFF_FAR   0x02 // beyond the far plane (only when it is on)
#define CC_OFF_LEFT  0x04 // x < -z
#define CC_OFF_RIGHT 0x08 // x > z
#define CC_OFF_BOT   0x10 // y < -z
#define CC_OFF_TOP   0x20 // y > z
#define CC_BEHIND    0x80 // at or behind the eye

// vertex::proj_flags
#define PF_PROJECTED  0x1 // sx, sy and sw are set
#define PF_OVERFLOW   0x2 // at or behind the eye, so it can't be projected
#define PF_TEMP_POINT 0x4 // made by clipping

// vertex::flags: which per-vertex attributes the renderer should use.
#define VF_UV    0x1 // u and v are set
#define VF_RGB   0x2 // use r, g and b instead of the current color
#define VF_ALPHA 0x4 // use a instead of the current alpha

class color {
public:
    u8 r;
    u8 g;
    u8 b;
    u8 a;

    color() {}
    color(int red, int green, int blue, int alpha);
    void set(int red, int green, int blue, int alpha);
};

// Packed render state (texture source, blending, depth test and so on) that the drawing
// functions take by value.
class gr_mode {
public:
    u32 value;

    gr_mode() {}
    gr_mode(u32 v) { value = v; }
};

// The screen state, cleared by gr_init.
struct screen {
    /* 0x00 */ int signature;
    /* 0x04 */ int max_w;
    /* 0x08 */ int max_h;
    /* 0x0C */ int mode; // set to the platform's id by the PS2 renderer's init
    /* 0x10 */ int window_mode;
    /* 0x14 */ int unk14;
    /* 0x18 */ f32 aspect;
    /* 0x1C */ int unk1C;
    /* 0x20 */ int unk20;
    /* 0x24 */ int unk24;
    /* 0x28 */ int offset_x;
    /* 0x2C */ int offset_y;
    /* 0x30 */ int clip_width;
    /* 0x34 */ int clip_height;
    /* 0x38 */ int unk38;
    /* 0x3C */ int unk3C;
    /* 0x40 */ int clip_left; // relative to offset_x
    /* 0x44 */ int clip_right;
    /* 0x48 */ int clip_top; // relative to offset_y
    /* 0x4C */ int clip_bottom;
    /* 0x50 */ color current_color;
    /* 0x54 */ int current_bitmap;
    /* 0x58 */ int current_section;
    /* 0x5C */ int fog_enabled;
    /* 0x60 */ color fog_color;
    /* 0x64 */ f32 fog_near;
    /* 0x68 */ f32 fog_far;
    /* 0x6C */ int unk6C;
    /* 0x70 */ u8 tint_enabled;
    /* 0x74 */ f32 tint_r;
    /* 0x78 */ f32 tint_g;
    /* 0x7C */ f32 tint_b;
    /* 0x80 */ u8 unk80; // set by gr_init and gr_set_unk80
    /* 0x84 */ int unk84;
};

// A point of the 3D renderer: its position in view space and, once projected, on the screen,
// with the texture coordinates and color the renderer can use for it. Assigning a vertex copies
// pos as one vector3 and then each field in turn. pos also gives vertex a default constructor
// that does nothing but still has to be called, so static arrays of vertices are built by the
// static initializers (the game's copy of it is at 0x180B30).
struct vertex {
    /* 0x00 */ vector3 pos;   // view space
    /* 0x0C */ f32 sx, sy;    // screen position
    /* 0x14 */ f32 sw;        // 1 / z
    /* 0x18 */ u8 codes;      // CC_* bits
    /* 0x19 */ u8 proj_flags; // PF_* bits
    /* 0x1A */ u8 flags;      // VF_* bits
    /* 0x1B */ u8 pad1B;
    /* 0x1C */ f32 u, v;
    /* 0x24 */ u8 r, g, b, a;
};

// Layers a polygon can have (PolyPoint::layers).
#define POLY_MAX_LAYERS 4

// One pass of a layered polygon. Attributes the points don't supply come from r, g, b and a.
struct PolyLayer {
    /* 0x00 */ gr_mode mode;
    /* 0x04 */ s16 bitmap;
    /* 0x06 */ u8 has_uv;
    /* 0x07 */ u8 has_rgb;
    /* 0x08 */ u8 has_alpha;
    /* 0x09 */ u8 flag;
    /* 0x0A */ u8 r;
    /* 0x0B */ u8 g;
    /* 0x0C */ u8 b;
    /* 0x0D */ u8 a;

    PolyLayer() : mode(0xFFFFFFFF) {}
};

// What a polygon point supplies for one layer.
struct PolyPointLayer {
    /* 0x00 */ f32 u;
    /* 0x04 */ f32 v;
    /* 0x08 */ u8 r;
    /* 0x09 */ u8 g;
    /* 0x0A */ u8 b;
    /* 0x0B */ u8 a;
};

// A polygon point: its vertex and its attributes for each layer.
struct PolyPoint {
    /* 0x00 */ vertex* vert;
    /* 0x04 */ PolyPointLayer layers[POLY_MAX_LAYERS];
};

// Not inlined: callers in many files share the copy emitted at the end of gr.cpp.
template <class T> void clamp(T& value, T low, T high) {
    if (value < low) {
        value = low;
    } else if (value > high) {
        value = high;
    }
}

// Render modes, set up by the static initializer of gr.cpp.
extern gr_mode D_002D4660;
extern gr_mode D_002D4664;
extern gr_mode D_002D4668;
extern gr_mode D_002D466C;
extern gr_mode D_002D4670;
extern gr_mode D_002D4674;
extern gr_mode D_002D4678;
extern gr_mode D_002D467C;
extern gr_mode D_002D4680;
extern gr_mode D_002D4684;
extern gr_mode D_002D4688;
extern gr_mode D_002D468C;
extern gr_mode D_002D4690;
extern gr_mode D_002D4694;
extern gr_mode D_002D4698;
extern gr_mode D_002D469C;
extern gr_mode D_002D46A0;

extern screen gr_screen;
extern u8 Gr_inited;
extern float Gr_fade;
extern float Gr_gamma;

extern "C" void func_0017B550(void);
int gr_init(int max_w, int max_h, int mode, int window_mode, int flip, u8 unk80);
void gr_set_unk80(u8 value);
extern "C" void func_0017B770(void);
void gr_fade_pause(int paused);
void gr_fade_out(int milliseconds);
void gr_fade_in(int milliseconds);
float gr_get_fade(void);
void gr_fade_frame(void);
bool gr_is_fading(void);
void gr_set_clip(int x, int y, int w, int h);
float gr_get_aspect(void);
void gr_set_aspect(float aspect);
void gr_get_clip(int* x, int* y, int* w, int* h);
void gr_reset_clip(void);
void gr_clear(void);
void gr_flip(void);
void gr_set_gamma(float gamma);
void gr_set_color(int r, int g, int b, int a);
void gr_set_color_fast(color* c);
void gr_set_color_white(void);
void gr_set_alpha(int a);
void gr_set_bitmap(int bitmap, int section);
void gr_bitmap_ex(int bitmap, int x, int y, int w, int h, int sx, int sy, int sw, int sh, gr_mode mode);
void gr_bitmap(int bitmap, int x, int y, gr_mode mode);
void gr_line(float x1, float y1, float x2, float y2, gr_mode mode);
void gr_circle(int xc, int yc, int r, gr_mode mode);
void gr_rect(int x, int y, int w, int h, gr_mode mode);
void gr_poly_layers(int nverts, PolyPoint** points, int nlayers, PolyLayer* layers);
void gr_set_fog(u8 enable, int r, int g, int b, float fog_near, float fog_far);
void gr_set_tint(u8 enable, float r, float g, float b);
extern "C" void func_0017CE10(void);
float gr_brightness(float r, float g, float b);
u8 gr_is_inited(void);

#endif // GAME_GR_H
