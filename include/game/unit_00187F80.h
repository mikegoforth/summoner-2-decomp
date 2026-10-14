#ifndef GAME_UNIT_00187F80_H
#define GAME_UNIT_00187F80_H

#include "types.h"
#include "game/gr.h"
#include "game/unit_00196B10.h"

// PS2 back end of the renderer: the GIF packet that drawing code builds in scratchpad memory,
// display setup and buffer flips, bitmap drawing, primitives in the current color, GS render
// state, VRAM copies and depth readback. Coordinates are screen pixels; Gs_state.offset_x and
// offset_y turn them into GS window coordinates (12.4 fixed point).

typedef unsigned int u128 __attribute__((mode(TI)));

// GIF packet built in scratchpad memory and sent to the GS over DMA channel 2 (PATH3). Code all
// over the game adds to it directly through `cur`.
class GsPacket {
public:
    /* 0x00 */ u64* base; // start of the buffer
    /* 0x08 */ u64 madr;  // the buffer's address as the DMA controller sees it
    /* 0x10 */ u64* cur;  // next free doubleword

    GsPacket();
    // Sends the packet as it is and waits for the transfer. The last tag must have EOP set.
    void kick();
    // Ends the packet with an empty EOP tag and sends it, if anything has been added.
    void flush();
    // Ends the packet with an empty EOP tag and copies it to `dest` (quadword aligned) instead
    // of sending it. Returns the size copied in bytes, 0 if the packet was empty.
    int copy(void* dest);

private:
    void send();
};

// Drawing state of the PS2 renderer.
struct GsState {
    /* 0x00 */ int offset_x; // XYOFFSET of the draw buffer, 12.4 fixed point
    /* 0x04 */ int offset_y;
    /* 0x08 */ float fog_scale;
    /* 0x10 */ u64 fog_color;
};

// TEXFLUSH as a ready-made GIF packet: tag (NLOOP 1, EOP, A+D) and the register write.
extern u64 Gs_texflush_packet[4];
extern GsPacket Gs_packet;
extern GsState Gs_state;
// Buffer that gr_ngps_read_depth reads the depth buffer into.
extern u32 Gs_store_buffer[0x200];
extern int Gs_frame;                // frames flipped; bit 0 selects the draw buffer
extern volatile int Gs_vsync_count; // vertical blanks since the last flip
extern volatile u64 Gs_vsync_total; // vertical blanks since the display was set up
extern u8 Gs_wait_vsync;            // nonzero: flips wait for the requested vertical blank

// Vertical blank interrupt handler: counts vertical blanks.
int gr_ngps_vsync_handler(int cause);
// Resets the GS for a 512x448 interlaced NTSC display and installs the vertical blank handler.
void gr_ngps_reset_display(void);
// Installs the vertical blank handler again, after something else has replaced it.
void gr_ngps_restore_vsync_handler(void);
void gr_ngps_init(void);
// Sets SCISSOR_1 to the inclusive rectangle (x0, y0)-(x1, y1).
void gr_ngps_set_scissor(int x0, int y0, int x1, int y1);
// Sets the scissor from the clip rectangle of gr_screen.
void gr_ngps_set_clip(void);
// Clears the clip rectangle of the draw buffer to black and resets the depth buffer.
void gr_ngps_clear(void);
// Waits for drawing to finish, then until at least `min_vsyncs` vertical blanks have passed
// since the last flip (0: no wait), and swaps the display and draw buffers.
void gr_ngps_flip(u32 min_vsyncs);
// Draws a bitmap at its own size, tinted by the current color and blended by its alpha.
void gr_ngps_sprite(int bitmap, int x, int y);
// Draws a bitmap scaled by `scale`, with its point (hot_x, hot_y) at (x, y).
void gr_ngps_sprite_scaled(int bitmap, int x, int y, int hot_x, int hot_y, float scale);
// Draws a bitmap at its own size, untinted and without blending.
void gr_ngps_sprite_opaque(int bitmap, int x, int y);
// Draws a bitmap as a sprite facing the view, centered on the world point `pos`.
void gr_ngps_billboard(int bitmap, const vector3* pos, float radius);

// A bitmap with power-of-two sides is drawn as one quad textured through the texture cache. Any
// other bitmap is drawn in tiles: each tile is uploaded to the VRAM reserved for the bitmap at
// a power-of-two size (at most 256x256) and drawn as its own quad. Bitmaps stored as compressed
// pictures (formats 1 and 2) are decoded a tile at a time on the way.
//
// Draws the sw x sh area at (sx, sy) of a power-of-two bitmap, stretched to w x h at (x, y).
void gr_ngps_bitmap_quad(int bitmap, int x, int y, int w, int h, int sx, int sy, int sw, int sh, gr_mode mode);
// Draws a compressed bitmap whole, at its own size, with its point (sx, sy) at (x, y).
void gr_ngps_bitmap_compressed(int bitmap, int x, int y, int w, int h, int sx, int sy, gr_mode mode);
// Draws the sw x sh area at (sx, sy) of a bitmap, stretched to w x h at (x, y). Only bitmaps with
// power-of-two sides can be stretched or cut. Other bitmaps are drawn in tiles at their own size:
// compressed ones whole, with their point (sx, sy) at (x, y); the rest from row sy to the bottom,
// with the left end of that row at (x, y).
void gr_ngps_bitmap(int bitmap, int x, int y, int w, int h, int sx, int sy, int sw, int sh, gr_mode mode);

// Lines in the current color, alpha blended (gr_ngps_line) or antialiased (gr_ngps_aaline).
void gr_ngps_line(float x1, float y1, float x2, float y2);
void gr_ngps_aaline(float x1, float y1, float x2, float y2);
// Line between two projected vertices, at their depths. The mode is not used.
void gr_ngps_line_3d(vertex* v1, vertex* v2, gr_mode mode);
// Rectangle from (x1, y1) to (x2, y2) in the current color.
void gr_ngps_rect(int x1, int y1, int x2, int y2);
// Covers the whole 512x448 draw buffer with a color (alpha blended), as 16 sprites 32 pixels
// wide.
void gr_ngps_fill(u8 r, u8 g, u8 b, u8 a);
// Triangle fan through projected vertices, textured with the current bitmap when the mode has a
// texture source. Vertex colors and alphas replace the current ones per VF_RGB and VF_ALPHA.
void gr_ngps_tmapper(int nverts, vertex** verts, gr_mode mode);
// Sends FOGCOL and works out the fog scale from gr_screen's fog color and range.
void gr_ngps_set_fog(void);
// Empties the texture cache.
void gr_ngps_flush_textures(void);
// Copies a w x h rectangle of VRAM from block address sbp to dbp (buffer widths in pixels).
void gr_ngps_copy_vram(u32 dbp, u32 sbp, int sbw, int dbw, int w, int h, int psm);
// Block addresses (TBP) of the buffer being drawn and of the buffer on screen.
u32 gr_ngps_back_buffer(void);
u32 gr_ngps_front_buffer(void);
// Depth test: 0 off, 1 test without writing, 4 test and write.
void gr_ngps_set_zbuffer(int mode);
void gr_ngps_set_alpha(u64 alpha);
// CLAMP_1 wrap mode, the same for both texture axes.
void gr_ngps_set_clamp(int mode);
// Sends TEXFLUSH and waits for the transfer.
void gr_ngps_texflush(void);
// Reads the depth buffer at a pixel, as a depth from 0 to 10000.
float gr_ngps_read_depth(int x, int y);
// Sends the packet, flushes the texture cache and waits for the GS and the next vertical blank.
void gr_ngps_sync(void);

#endif // GAME_UNIT_00187F80_H
