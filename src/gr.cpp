#include "game/gr.h"
#include "game/bmpman_ngps.h"
#include "game/unit_00187F80.h"
#include "game/unit_001843A0.h"
#include "game/frametime.h"
#include "macros.h"
#include <string.h>

// powf, sinf and cosf.
extern "C" float func_001CC678(float x, float y);
extern "C" float func_001CC4B8(float x);
extern "C" float func_001CC200(float x);

// Render modes, set up by the static initializer.
gr_mode D_002D4660(0x06018C43);
gr_mode D_002D4664(0x06018021);
gr_mode D_002D4668(0x06018C41);
gr_mode D_002D466C(0x06010021);
gr_mode D_002D4670(0x06018000);
gr_mode D_002D4674(0x06018000);
gr_mode D_002D4678(0x06018000);
gr_mode D_002D467C(0x00418000);
gr_mode D_002D4680(0x00418000);
gr_mode D_002D4684(0x00018821);
gr_mode D_002D4688(0x00018C41);
gr_mode D_002D468C(0x00118C42);
gr_mode D_002D4690(0x06110C42);
gr_mode D_002D4694(0x04428821);
gr_mode D_002D4698(0x00110000);
gr_mode D_002D469C = D_002D4690;
gr_mode D_002D46A0 = D_002D4690;
u8 Gr_inited;
float Gr_fade_time;
float Gr_fade_duration;
int Gr_fade_state;
int Gr_fade_paused;

// How far the screen is faded to black: 1 is black, below 0 is no fade at all.
float Gr_fade = -1.0f;
float Gr_gamma = 1.8f;

screen gr_screen;
int Gr_gamma_lookup[256];
float Gr_gamma_lookup_float[256];

extern "C" void func_0017B550(void) {}

int gr_init(int max_w, int max_h, int mode, int window_mode, int flip, u8 unk80) {
    memset(&gr_screen, 0, sizeof(gr_screen));
    gr_screen.mode = mode;
    gr_screen.window_mode = window_mode;
    gr_screen.max_w = max_w;
    gr_screen.max_h = max_h;
    gr_screen.aspect = 1.0f;
    gr_screen.offset_x = 0;
    gr_screen.offset_y = 0;
    gr_screen.clip_left = 0;
    gr_screen.clip_top = 0;
    gr_screen.current_bitmap = -1;
    gr_screen.current_section = -1;
    gr_screen.fog_near = 0.0f;
    gr_screen.fog_far = 60.0f;
    gr_screen.unk80 = unk80;
    gr_screen.unk84 = 0;
    gr_screen.clip_width = gr_screen.max_w;
    gr_screen.clip_right = gr_screen.max_w - 1;
    gr_screen.clip_bottom = gr_screen.max_h - 1;
    gr_screen.clip_height = gr_screen.max_h;
    func_0017B550();

    switch (gr_screen.window_mode) {
    case GR_WINDOW_MODE_C8:
    case GR_WINDOW_MODE_C9:
    case GR_WINDOW_MODE_CA:
        break;
    default:
        DEBUGERRORLINE(602, "Invalid window_mode passed to gr_init\n");
    }

    gr_ngps_init();
    gr_set_fog(1, 0, 0, 0, 0.0f, 60.0f);
    gr_set_fog(0, 0, 0, 0, -1.0f, -1.0f);
    gr_set_gamma(Gr_gamma);
    Gr_inited = 1;
    gr_reset_clip();
    gr_clear();
    if (flip) {
        gr_flip();
    }
    return 0;
}

void gr_set_unk80(u8 value) { gr_screen.unk80 = value; }

extern "C" void func_0017B770(void) {}

void gr_fade_pause(int paused) { Gr_fade_paused = paused; }

// Fades the screen to black.
void gr_fade_out(int milliseconds) {
    if (milliseconds <= 0) {
        Gr_fade_state = GR_FADE_NONE;
        Gr_fade = 1.0f;
    } else {
        Gr_fade_time = 0.0f;
        Gr_fade_state = GR_FADE_OUT;
        Gr_fade_duration = milliseconds / 1000.0f;
    }
}

// Fades the screen back in from black.
void gr_fade_in(int milliseconds) {
    if (milliseconds <= 0) {
        Gr_fade_state = GR_FADE_NONE;
        Gr_fade = -1.0f;
    } else {
        Gr_fade_time = 0.0f;
        Gr_fade_state = GR_FADE_IN;
        Gr_fade_duration = milliseconds / 1000.0f;
    }
}

float gr_get_fade(void) {
    if (Gr_fade < 0.0f) {
        return 0.0f;
    }
    return Gr_fade;
}

// Advances the fade and darkens the whole screen by it.
void gr_fade_frame(void) {
    if (Gr_fade_paused) {
        return;
    }
    if (Gr_fade_state) {
        Gr_fade_time += reported_frametime;
        if (Gr_fade_time <= Gr_fade_duration) {
            Gr_fade = Gr_fade_time / Gr_fade_duration;
            if (Gr_fade_state == GR_FADE_IN) {
                Gr_fade = 1.0f - Gr_fade;
            }
        } else {
            if (Gr_fade_state == GR_FADE_IN) {
                Gr_fade = -1.0f;
            } else {
                Gr_fade = 1.0f;
            }
            Gr_fade_state = GR_FADE_NONE;
        }
    }
    if (Gr_fade > 0.0f) {
        gr_set_color(0, 0, 0, (int)(255.0f * Gr_fade));
        gr_ngps_set_alpha(0x44);
        gr_ngps_set_zbuffer(0);
        gr_ngps_set_scissor(0, 0, 516, 452);
        gr_ngps_rect(-2, -2, 514, 450);
    }
}

bool gr_is_fading(void) { return Gr_fade_state && !Gr_fade_paused; }

color::color(int red, int green, int blue, int alpha) {
    r = red;
    g = green;
    b = blue;
    a = alpha;
}

void color::set(int red, int green, int blue, int alpha) {
    r = red;
    g = green;
    b = blue;
    a = alpha;
}

void gr_set_clip(int x, int y, int w, int h) {
    gr_screen.clip_right = w - 1;
    gr_screen.clip_bottom = h - 1;
    gr_screen.offset_x = x;
    gr_screen.offset_y = y;
    gr_screen.clip_left = 0;
    gr_screen.clip_top = 0;

    // Keep the clip rectangle on the screen.
    if (gr_screen.clip_left + x < 0) {
        gr_screen.clip_left = -x;
    } else if (gr_screen.clip_left + x > gr_screen.max_w - 1) {
        gr_screen.clip_left = gr_screen.max_w - 1 - x;
    }
    if (gr_screen.clip_right + x < 0) {
        gr_screen.clip_right = -x;
    } else if (gr_screen.clip_right + x >= gr_screen.max_w - 1) {
        gr_screen.clip_right = gr_screen.max_w - 1 - x;
    }
    if (gr_screen.clip_top + y < 0) {
        gr_screen.clip_top = -y;
    } else if (gr_screen.clip_top + y > gr_screen.max_h - 1) {
        gr_screen.clip_top = gr_screen.max_h - 1 - y;
    }
    if (gr_screen.clip_bottom + y < 0) {
        gr_screen.clip_bottom = -y;
    } else if (gr_screen.clip_bottom + y > gr_screen.max_h - 1) {
        gr_screen.clip_bottom = gr_screen.max_h - 1 - y;
    }

    gr_screen.clip_width = gr_screen.clip_right - gr_screen.clip_left + 1;
    gr_screen.clip_height = gr_screen.clip_bottom - gr_screen.clip_top + 1;
    gr_ngps_set_clip();
}

float gr_get_aspect(void) { return gr_screen.aspect; }

void gr_set_aspect(float aspect) { gr_screen.aspect = aspect; }

void gr_get_clip(int* x, int* y, int* w, int* h) {
    *x = gr_screen.offset_x;
    *y = gr_screen.offset_y;
    *w = gr_screen.clip_width;
    *h = gr_screen.clip_height;
}

void gr_reset_clip(void) { gr_set_clip(0, 0, gr_screen.max_w, gr_screen.max_h); }

void gr_clear(void) { gr_ngps_clear(); }

void gr_flip(void) {
    prim_flush();
    gr_reset_clip();
    gr_fade_frame();
    gr_ngps_flip(2);
}

void gr_set_gamma(float gamma) {
    int i;
    int v;
    float f;

    Gr_gamma = gamma;
    for (i = 0; i < 256; i++) {
        v = (int)(255.0f * func_001CC678(i / 255.0f, 1.0f / Gr_gamma));
        if (v > 255) {
            v = 255;
        } else if (v < 0) {
            v = 0;
        }
        Gr_gamma_lookup[i] = v;
    }
    for (i = 0; i < 256; i++) {
        f = i;
        if (f > 255.0f) {
            f = 255.0f;
        } else if (f < 0.0f) {
            f = 0.0f;
        }
        Gr_gamma_lookup_float[i] = f;
    }
}

void gr_set_color(int r, int g, int b, int a) {
    gr_screen.current_color.r = r;
    gr_screen.current_color.g = g;
    gr_screen.current_color.b = b;
    gr_screen.current_color.a = a;
}

void gr_set_color_fast(color* c) {
    gr_screen.current_color.r = c->r;
    gr_screen.current_color.g = c->g;
    gr_screen.current_color.b = c->b;
    gr_screen.current_color.a = c->a;
}

void gr_set_color_white(void) {
    gr_screen.current_color.r = gr_screen.current_color.g = gr_screen.current_color.b = gr_screen.current_color.a = 255;
}

void gr_set_alpha(int a) { gr_screen.current_color.a = a; }

void gr_set_bitmap(int bitmap, int section) {
    gr_screen.current_bitmap = bitmap;
    gr_screen.current_section = section;
}

// Draws the sw x sh area at (sx, sy) of a bitmap stretched to w x h at (x, y).
void gr_bitmap_ex(int bitmap, int x, int y, int w, int h, int sx, int sy, int sw, int sh, gr_mode mode) {
    int type = bm_get_format(bitmap);
    if (type == 2 || type == 1) {
        return;
    }
    gr_ngps_bitmap(bitmap, x, y, w, h, sx, sy, sw, sh, mode);
}

void gr_bitmap(int bitmap, int x, int y, gr_mode mode) {
    int w;
    int h;

    bm_get_dimensions(bitmap, &w, &h);
    if (w) {
        gr_ngps_bitmap(bitmap, x, y, w, h, 0, 0, w, h, mode);
    }
}

// Clips the line to the clip rectangle before drawing it. The renderer's line drawing
// doesn't take a mode.
void gr_line(float x1, float y1, float x2, float y2, gr_mode mode) {
    float temp;
    float clip_left = gr_screen.clip_left;
    float clip_top = gr_screen.clip_top;
    float clip_right = gr_screen.clip_right;
    float clip_bottom = gr_screen.clip_bottom;

    if (y1 > y2) {
        temp = y1;
        y1 = y2;
        y2 = temp;
        temp = x1;
        x1 = x2;
        x2 = temp;
    }
    if (y2 < clip_top || y1 > clip_bottom) {
        return;
    }
    if (x1 < x2) {
        if (x2 < clip_left || x1 > clip_right) {
            return;
        }
        if (x1 < clip_left) {
            temp = (y2 - y1) * (clip_left - x1) / (x2 - x1);
            if ((y1 += temp) > clip_bottom) {
                return;
            }
            x1 = clip_left;
        }
        if (x2 > clip_right) {
            temp = (y2 - y1) * (x2 - clip_right) / (x2 - x1);
            if ((y2 -= temp) < clip_top) {
                return;
            }
            x2 = clip_right;
        }
        if (y1 < clip_top) {
            temp = (x2 - x1) * (clip_top - y1) / (y2 - y1);
            x1 += temp;
            y1 = clip_top;
        }
        if (y2 > clip_bottom) {
            temp = (x2 - x1) * (y2 - clip_bottom) / (y2 - y1);
            x2 -= temp;
            y2 = clip_bottom;
        }
    } else {
        if (x1 < clip_left || x2 > clip_right) {
            return;
        }
        if (x1 > clip_right) {
            temp = (y2 - y1) * (x1 - clip_right) / (x1 - x2);
            if ((y1 += temp) > clip_bottom) {
                return;
            }
            x1 = clip_right;
        }
        if (x2 < clip_left) {
            temp = (y2 - y1) * (clip_left - x2) / (x1 - x2);
            if ((y2 -= temp) < clip_top) {
                return;
            }
            x2 = clip_left;
        }
        if (y1 < clip_top) {
            temp = (x1 - x2) * (clip_top - y1) / (y2 - y1);
            x1 -= temp;
            y1 = clip_top;
        }
        if (y2 > clip_bottom) {
            temp = (x1 - x2) * (y2 - clip_bottom) / (y2 - y1);
            x2 += temp;
            y2 = clip_bottom;
        }
    }
    gr_ngps_line(x1, y1, x2, y2);
}

// Draws the circle as 5 to 128 line segments.
void gr_circle(int xc, int yc, int r, gr_mode mode) {
    int i;
    int segments;
    float step;
    float angle;
    float x1;
    float y1;
    float x2;
    float y2;

    angle = 0.0f;
    segments = r * 3;
    clamp(segments, 5, 128);
    step = 6.2831855f / segments;
    for (i = 0; i < segments - 1; i++) {
        x1 = r * func_001CC4B8(angle);
        y1 = r * func_001CC200(angle);
        x2 = r * func_001CC4B8(angle + step);
        y2 = r * func_001CC200(angle + step);
        gr_line(xc + x1, yc + y1, xc + x2, yc + y2, mode);
        angle += step;
    }
    x1 = r * func_001CC4B8(angle);
    y1 = r * func_001CC200(angle);
    x2 = r * func_001CC4B8(0.0f);
    y2 = r * func_001CC200(0.0f);
    gr_line(xc + x1, yc + y1, xc + x2, yc + y2, mode);
}

// The renderer's rectangle drawing doesn't take a mode.
void gr_rect(int x, int y, int w, int h, gr_mode mode) { gr_ngps_rect(x, y, x + w, y + h); }

// Draws the polygon once per layer, loading each layer's texture coordinates and colors into
// the vertices first.
void gr_poly_layers(int nverts, PolyPoint** points, int nlayers, PolyLayer* layers) {
    vertex* verts[32];
    int i;
    int layer;
    PolyLayer* l;
    PolyPoint* p;
    u8 alpha;

    for (i = 0; i < nverts; i++) {
        verts[i] = points[i]->vert;
    }
    for (layer = 0; layer < nlayers; layer++) {
        l = &layers[layer];
        for (i = 0; i < nverts; i++) {
            verts[i]->flags = 0;
        }
        if (l->has_uv) {
            for (i = 0; i < nverts; i++) {
                p = points[i];
                verts[i]->u = p->layers[layer].u;
                verts[i]->v = p->layers[layer].v;
                verts[i]->flags |= VF_UV;
            }
        }
        if (l->has_alpha) {
            for (i = 0; i < nverts; i++) {
                p = points[i];
                verts[i]->a = p->layers[layer].a;
                verts[i]->flags |= VF_ALPHA;
            }
            alpha = 255;
        } else {
            alpha = l->a;
        }
        if (l->has_rgb) {
            for (i = 0; i < nverts; i++) {
                p = points[i];
                verts[i]->r = p->layers[layer].r;
                verts[i]->g = p->layers[layer].g;
                verts[i]->b = p->layers[layer].b;
                verts[i]->flags |= VF_RGB;
            }
            gr_set_color(255, 255, 255, alpha);
        } else {
            gr_set_color(l->r, l->g, l->b, alpha);
        }
        gr_set_bitmap(l->bitmap, -1);
        gr_ngps_tmapper(nverts, verts, l->mode);
    }
}

void gr_set_fog(u8 enable, int r, int g, int b, float fog_near, float fog_far) {
    gr_screen.fog_enabled = enable;
    if (enable) {
        gr_screen.fog_color.set(r, g, b, 255);
        if (fog_near >= 0.0f && fog_far >= fog_near) {
            gr_screen.fog_near = fog_near;
            gr_screen.fog_far = fog_far;
        }
    }
    gr_ngps_set_fog();
}

void gr_set_tint(u8 enable, float r, float g, float b) {
    gr_screen.tint_enabled = enable;
    if (!gr_screen.tint_enabled) {
        b = g = r = 0.0f;
    }
    gr_screen.tint_r = r;
    gr_screen.tint_g = g;
    gr_screen.tint_b = b;
}

extern "C" void func_0017CE10(void) { gr_ngps_flush_textures(); }

// The color's average, moved 60% of the way to white, scaled to 0..1.
float gr_brightness(float r, float g, float b) {
    float average = (r + g + b) / 3.0f;
    return (average + 0.6f * (255.0f - average)) / 255.0f;
}

u8 gr_is_inited(void) { return Gr_inited; }
