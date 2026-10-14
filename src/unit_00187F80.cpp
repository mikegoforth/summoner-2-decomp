// PS2 back end of the renderer: the GIF packet, display setup and buffer flips, bitmap drawing,
// primitives in the current color, GS render state, VRAM copies and depth readback. Bitmaps with
// power-of-two sides go through the texture cache as one quad; other bitmaps are uploaded and drawn
// a tile at a time.
#include "game/unit_00187F80.h"
#include "game/bmpman_ngps.h"
#include "game/gr.h"
#include "game/gr_ngps_texture.h"
#include "game/unit_0017CEB0.h"

// EE registers: DMA channel 2 (GIF).
#define D2_CHCR (*(volatile u32*)0x1000A000)
#define D2_MADR (*(volatile u32*)0x1000A010)
#define D2_QWC  (*(volatile u32*)0x1000A020)
#define D2_TADR (*(volatile u32*)0x1000A030)

#define D_CHCR_STR   0x100 // set while the channel is transferring
#define D_CHCR_CHAIN 0x104 // MOD chain | STR: follow the tags from TADR
#define D_TADR_SPR   0x80000000

#define DMA_TAG_REFE 0x00000000 // transfer the data at ADDR, then end
#define DMA_TAG_REF  0x30000000 // transfer the data at ADDR, then the next tag

// GS privileged registers.
#define GS_PMODE    (*(volatile u64*)0x12000000)
#define GS_SMODE2   (*(volatile u64*)0x12000020)
#define GS_DISPFB1  (*(volatile u64*)0x12000070)
#define GS_DISPLAY1 (*(volatile u64*)0x12000080)
#define GS_DISPFB2  (*(volatile u64*)0x12000090)
#define GS_DISPLAY2 (*(volatile u64*)0x120000A0)
#define GS_BGCOLOR  (*(volatile u64*)0x120000E0)
#define GS_CSR      (*(volatile u64*)0x12001000)

#define GS_CSR_FINISH 0x2 // a FINISH write has been processed; writing 1 clears it

#define GS_SET_PMODE(en1, en2, mmod, amod, slbg, alp)                                             \
    ((u64)(en1) | ((u64)(en2) << 1) | ((u64)1 << 2) | ((u64)(mmod) << 5) | ((u64)(amod) << 6) |   \
     ((u64)(slbg) << 7) | ((u64)(alp) << 8))
#define GS_SET_SMODE2(intl, ffmd, dpms) ((u64)(intl) | ((u64)(ffmd) << 1) | ((u64)(dpms) << 2))
#define GS_SET_DISPFB(fbp, fbw, psm, dbx, dby)                                                    \
    ((u64)(fbp) | ((u64)(fbw) << 9) | ((u64)(psm) << 15) | ((u64)(dbx) << 32) | ((u64)(dby) << 43))
#define GS_SET_DISPLAY(dx, dy, magh, magv, dw, dh)                                                \
    ((u64)(dx) | ((u64)(dy) << 12) | ((u64)(magh) << 23) | ((u64)(magv) << 27) | ((u64)(dw) << 32) | \
     ((u64)(dh) << 44))

// GS registers written through A+D, and their values.
#define GS_CLAMP_1    0x08
#define GS_CLAMP_2    0x09
#define GS_TEX1_1     0x14
#define GS_TEX1_2     0x15
#define GS_XYOFFSET_1 0x18
#define GS_XYOFFSET_2 0x19
#define GS_PRMODECONT 0x1A
#define GS_PRMODE     0x1B
#define GS_SCANMSK    0x22
#define GS_TEXA       0x3B
#define GS_FOGCOL     0x3D
#define GS_SCISSOR_1  0x40
#define GS_ALPHA_1    0x42
#define GS_ALPHA_2    0x43
#define GS_COLCLAMP   0x46
#define GS_TEST_1     0x47
#define GS_PABE       0x49
#define GS_FRAME_1    0x4C
#define GS_FRAME_2    0x4D
#define GS_ZBUF_1     0x4E
#define GS_BITBLTBUF  0x50
#define GS_TRXPOS     0x51
#define GS_TRXREG     0x52
#define GS_TRXDIR     0x53
#define GS_FINISH     0x61

#define GS_SET_TEX1(lcm, mxl, mmag, mmin, mtba, l, k)                                             \
    ((u64)(lcm) | ((u64)(mxl) << 2) | ((u64)(mmag) << 5) | ((u64)(mmin) << 6) | ((u64)(mtba) << 9) | \
     ((u64)(l) << 19) | ((u64)(k) << 32))
#define GS_SET_CLAMP(wms, wmt, minu, maxu, minv, maxv)                                            \
    ((u64)(wms) | ((u64)(wmt) << 2) | ((u64)(minu) << 4) | ((u64)(maxu) << 14) | ((u64)(minv) << 24) | \
     ((u64)(maxv) << 34))
#define GS_SET_XYOFFSET(ofx, ofy) ((u64)(ofx) | ((u64)(ofy) << 32))
#define GS_SET_PRMODE(iip, tme, fge, abe, aa1, fst, ctxt, fix)                                    \
    (((u64)(iip) << 3) | ((u64)(tme) << 4) | ((u64)(fge) << 5) | ((u64)(abe) << 6) | ((u64)(aa1) << 7) | \
     ((u64)(fst) << 8) | ((u64)(ctxt) << 9) | ((u64)(fix) << 10))
#define GS_SET_TEXA(ta0, aem, ta1) ((u64)(ta0) | ((u64)(aem) << 15) | ((u64)(ta1) << 32))
#define GS_SET_SCISSOR(scax0, scax1, scay0, scay1)                                                \
    ((u64)(scax0) | ((u64)(scax1) << 16) | ((u64)(scay0) << 32) | ((u64)(scay1) << 48))
#define GS_SET_ALPHA(a, b, c, d, fix)                                                              \
    ((u64)(a) | ((u64)(b) << 2) | ((u64)(c) << 4) | ((u64)(d) << 6) | ((u64)(fix) << 32))
#define GS_SET_TEST(ate, atst, aref, afail, date, datm, zte, ztst)                                \
    ((u64)(ate) | ((u64)(atst) << 1) | ((u64)(aref) << 4) | ((u64)(afail) << 12) | ((u64)(date) << 14) | \
     ((u64)(datm) << 15) | ((u64)(zte) << 16) | ((u64)(ztst) << 17))
#define GS_SET_FRAME(fbp, fbw, psm, fbmsk)                                                        \
    ((u64)(fbp) | ((u64)(fbw) << 16) | ((u64)(psm) << 24) | ((u64)(fbmsk) << 32))
#define GS_SET_ZBUF(zbp, psm, zmsk) ((u64)(zbp) | ((u64)(psm) << 24) | ((u64)(zmsk) << 32))
#define GS_SET_BITBLTBUF(sbp, sbw, spsm, dbp, dbw, dpsm)                                          \
    ((u64)(sbp) | ((u64)(sbw) << 16) | ((u64)(spsm) << 24) | ((u64)(dbp) << 32) | ((u64)(dbw) << 48) | \
     ((u64)(dpsm) << 56))
#define GS_SET_TRXPOS(ssax, ssay, dsax, dsay, dir)                                                \
    ((u64)(ssax) | ((u64)(ssay) << 16) | ((u64)(dsax) << 32) | ((u64)(dsay) << 48) | ((u64)(dir) << 59))
#define GS_SET_TRXREG(rrw, rrh) ((u64)(rrw) | ((u64)(rrh) << 32))

// GS pixel storage modes.
#define GS_PSMCT32 0x00
#define GS_PSMCT16 0x02
#define GS_PSMZ24  0x31

// TEST: ATST and ZTST.
#define GS_ALWAYS  1
#define GS_GEQUAL  2
#define GS_GREATER 6

// TRXDIR: local to local.
#define GS_TRX_LOCAL 2

// PRIM: primitive type and attributes.
#define GS_PRIM_LINE   1
#define GS_PRIM_TRIFAN 5
#define GS_PRIM_SPRITE 6
#define GS_PRIM_IIP    0x8   // Gouraud shaded
#define GS_PRIM_TME    0x10  // textured
#define GS_PRIM_ABE    0x40  // alpha blended
#define GS_PRIM_AA1    0x80  // antialiased
#define GS_PRIM_FST    0x100 // texture coordinates in UV rather than STQ

#define GIF_TAG(nloop, eop, pre, prim, flg, nreg)                                                 \
    ((u64)(nloop) | ((u64)(eop) << 15) | ((u64)(pre) << 46) | ((u64)(prim) << 47) |               \
     ((u64)(flg) << 58) | ((u64)(nreg) << 60))
#define GIF_AD      0xE    // REGS of a tag of A+D register writes
#define GIF_EOP_TAG 0x8000 // NLOOP 0 with EOP: ends the packet

// REGS of tags in the PACKED format: one register per nibble, first in the lowest.
#define GIF_REGS_RGBAQ          0x1   // RGBAQ
#define GIF_REGS_XYZ2_XYZ2      0x55  // XYZ2, XYZ2
#define GIF_REGS_RGBAQ_XYZ2_X2  0x551 // RGBAQ, XYZ2, XYZ2
#define GIF_REGS_STQ_RGBAQ_XYZ2 0x512 // STQ, RGBAQ, XYZ2

// A primitive is only added once the packet has room: past this many doublewords it is sent first.
#define GS_PACKET_LIMIT 0x300

// The draw buffers: 512 pixels wide (FBW 8), 448 lines, 32-bit, at frame pages 0 and 0x70, and the
// 24-bit depth buffer at page 0xE0. A frame page is 32 of the 64-word blocks that TBP and
// BITBLTBUF count in.
#define GS_FBW         8
#define GS_FRAME_FBP   0x70
#define GS_ZBUF_ZBP    0xE0
#define GS_PAGE_BLOCKS 32

// The draw buffer is centered on the GS's 4096x4096 coordinate space, in 12.4 fixed point.
#define GS_OFFSET_X ((2048 - 512 / 2) << 4)
#define GS_OFFSET_Y ((2048 - 448 / 2) << 4)

// Depth values run from 0 to 10000, scaled to the 24-bit depth buffer.
#define GS_DEPTH_RANGE 0.0001f
#define GS_DEPTH_MAX   16777215.0f
#define GS_DEPTH_SCALE (GS_DEPTH_MAX * GS_DEPTH_RANGE)

// The upload chain that gr_ngps_upload_setup starts in scratchpad memory.
#define UPLOAD_CHAIN 0x70000000

// Depth given to screen-space points.
#define BITMAP_DEPTH 1000000000.0f

// Size of the decoder object that unpacks compressed bitmaps (constructed by func_001659E0).
#define BM_DECODER_SIZE 0x20

#define MIN(a, b) ((a) < (b) ? (a) : (b))
// What to add to n to round it up to a multiple of 16.
#define PAD16(n) ((n) % 16 == 0 ? 0 : 16 - (n) % 16)

// sceGsStoreImage set up in scratchpad memory by gr_ngps_read_depth.
#define DEPTH_STORE_IMAGE ((void*)0x70003C00)

// gr_mode bits that pick the texture source: zero for an untextured primitive.
#define GR_MODE_TEXTURE_MASK 0x1F

// Libraries.
extern "C" void func_001B4628(void);                                              // sceDevVu0Reset
extern "C" void func_001B4A20(void);                                              // sceDevVif0Reset
extern "C" void func_001CC690(short mode, short inter, short omode, short ffmode); // sceGsResetGraph
extern "C" void func_001CC830(void);                                              // sceGsResetPath
extern "C" int func_001CDEE0(int (*handler)(int));                                // sceGsSyncVCallback
extern "C" int func_001CE068(int mode);                                           // sceDmaReset
extern "C" int func_001CD360(int mode);                                           // sceGsSyncV
extern "C" int func_001CD3F8(int mode, int timeout);                              // sceGsSyncPath
extern "C" int func_001CD850(void* sp, void* dst);                                // sceGsExecStoreImage
// sceGsSetDefStoreImage
extern "C" int func_001CD710(void* sp, short sbp, short sbw, short spsm, short x, short y, short w, short h);

// Decoder of compressed bitmaps (BM_DECODER_SIZE bytes). The destructor takes CodeWarrior's
// hidden delete flag (-1 for an object on the stack).
extern "C" void* func_001659E0(void* decoder);
extern "C" void* func_00165A30(void* decoder, short flags);
// Decodes a picture of w x h pixels (multiples of 16) at `bits` bits a pixel from the size bytes
// at data. Returns the decoded pixels.
extern "C" u8* func_00166530(void* decoder, u8* data, int w, int h, int size, int bits);

u64 Gs_texflush_packet[4] = {GIF_TAG(1, 1, 0, 0, 0, 1), GIF_AD, 0, 0x3F};
GsPacket Gs_packet;
GsState Gs_state;
u32 Gs_store_buffer[0x200];
int Gs_frame;
volatile int Gs_vsync_count;
volatile u64 Gs_vsync_total;
u8 Gs_wait_vsync = 1;

// Quadwords of a GIF packet in the PACKED format.
inline void gs_ad(u64* p, u32 reg, u64 data) {
    ((u32*)p)[2] = reg;
    p[0] = data;
}

inline void gs_rgbaq(u64* p, int r, int g, int b, int a) {
    ((u32*)p)[0] = r;
    ((u32*)p)[1] = g;
    ((u32*)p)[2] = b;
    ((u32*)p)[3] = a;
}

inline void gs_stq(u64* p, float s, float t, float q) {
    ((f32*)p)[0] = s;
    ((f32*)p)[1] = t;
    ((f32*)p)[2] = q;
}

inline void gs_uv(u64* p, int u, int v) {
    ((s16*)p)[0] = u;
    ((s16*)p)[2] = v;
}

// The depth is unsigned: the buffer holds 24 bits.
inline void gs_xyz2(u64* p, int x, int y, u32 z) {
    ((s16*)p)[0] = x;
    ((s16*)p)[2] = y;
    p[1] = z;
}

// Sends the packet first if it is getting full.
inline void gs_reserve(void) {
    if (Gs_packet.cur - Gs_packet.base >= GS_PACKET_LIMIT) {
        *Gs_packet.cur = GIF_EOP_TAG;
        Gs_packet.cur += 2;
        Gs_packet.kick();
    }
}

GsPacket::GsPacket() {
    base = (u64*)0x70000000;
    madr = 0xF0000000; // scratchpad flag | 0x70000000
    cur = base;
}

// Starts the DMA transfer of the packet to the GIF, waits for it and empties the packet.
inline void GsPacket::send() {
    D2_MADR = madr;
    D2_QWC = (cur - base) / 2;
    D2_CHCR = D_CHCR_STR;
    while (D2_CHCR & D_CHCR_STR) {
    }
    cur = base;
}

void GsPacket::kick() { send(); }

void GsPacket::flush() {
    if (base != cur) {
        *cur = GIF_EOP_TAG;
        cur += 2;
        send();
    }
}

int GsPacket::copy(void* dest) {
    if (base == cur) {
        return 0;
    }
    *cur = GIF_EOP_TAG;
    cur += 2;
    int qwc = (cur - base) / 2;
    int size = qwc * 16;
    u128* src = (u128*)base;
    u128* dst = (u128*)dest;
    do {
        *dst++ = *src++;
    } while (--qwc);
    cur = base;
    return size;
}

int gr_ngps_vsync_handler(int cause) {
    Gs_vsync_count++;
    Gs_vsync_total++;
    return 1;
}

void gr_ngps_reset_display(void) {
    Gs_packet.cur = Gs_packet.base;
    gr_screen.max_h = 448;
    func_001CC690(0, 1, 2, 0); // reset, interlaced, NTSC, field mode
    gr_screen.max_w = 512;
    // 512x448 pixels shown at 4:3.
    gr_screen.aspect = 6.0f / 7.0f;
    Gs_state.offset_x = GS_OFFSET_X;
    Gs_state.offset_y = GS_OFFSET_Y;
    Gs_vsync_count = 0;
    func_001CDEE0(gr_ngps_vsync_handler);
    // Textures go in the VRAM after the two frame buffers and the depth buffer.
    gr_ngps_tcache_init(0xA8000, 0xF8000, 0x100000);
    Gs_packet.flush();
}

void gr_ngps_restore_vsync_handler(void) {
    Gs_vsync_count = 0;
    func_001CDEE0(gr_ngps_vsync_handler);
}

void gr_ngps_init(void) {
    func_001B4A20();
    func_001B4628();
    func_001CC830();
    func_001CE068(1);
    gr_screen.mode = 0x69;
    Gs_frame = 0;
    Gs_packet.cur = Gs_packet.base;
    gr_ngps_reset_display();
    gr_ngps_set_scissor(0, 0, 512, 448);
    GS_BGCOLOR = 0;
    gr_ngps_clear();
    gr_ngps_flip(0);
    gr_ngps_clear();
    gr_ngps_flip(0);
}

void gr_ngps_set_scissor(int x0, int y0, int x1, int y1) {
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_packet.cur += 4;
    gs_ad(p + 2, GS_SCISSOR_1, GS_SET_SCISSOR(x0, x1, y0, y1));
}

void gr_ngps_set_clip(void) {
    int x0 = gr_screen.clip_left + gr_screen.offset_x;
    int x1 = gr_screen.clip_right + gr_screen.offset_x;
    int y0 = gr_screen.clip_top + gr_screen.offset_y;
    int y1 = gr_screen.clip_bottom + gr_screen.offset_y;
    // One letterboxed clip rectangle loses its bottom line.
    if (y0 == 74 && y1 == 371) {
        y1--;
    }
    gr_ngps_set_scissor(x0, y0, x1, y1);
}

void gr_ngps_clear(void) {
    u64* p = Gs_packet.cur;
    // A+D x4, RGBAQ, XYZ2, XYZ2.
    Gs_packet.cur[0] = GIF_TAG(1, 0, 1, GS_PRIM_SPRITE | GS_PRIM_ABE, 0, 7);
    Gs_packet.cur[1] = 0x0551EEEE;
    Gs_packet.cur += 16;
    gs_ad(p + 2, GS_TEST_1, GS_SET_TEST(0, GS_ALWAYS, 0, 0, 0, 0, 1, GS_ALWAYS));
    gs_ad(p + 4, GS_ZBUF_1, GS_SET_ZBUF(GS_ZBUF_ZBP, GS_PSMZ24, 0));
    gs_ad(p + 6, GS_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, GS_ALWAYS));
    // Source color times 0x80 / 0x80: no blending.
    gs_ad(p + 8, GS_ALPHA_1, GS_SET_ALPHA(0, 2, 2, 2, 0x80));
    gs_rgbaq(p + 10, 0, 0, 0, 0x80);
    gs_xyz2(p + 12, Gs_state.offset_x + (gr_screen.clip_left - 2) * 16,
            Gs_state.offset_y + (gr_screen.clip_top - 2) * 16, 0);
    gs_xyz2(p + 14, Gs_state.offset_x + (gr_screen.clip_right + 2) * 16,
            Gs_state.offset_y + (gr_screen.clip_bottom + 2) * 16, 0);
    gr_ngps_set_zbuffer(4);
    gr_ngps_set_alpha(GS_SET_ALPHA(0, 1, 0, 1, 0));
    Gs_packet.flush();
}

void gr_ngps_flip(u32 min_vsyncs) {
    u64* p;
    u64* p2;
    u64* p3;
    u64 dispfb1;
    u32 fbp;
    u64 dispfb2;

    Gs_packet.flush();
    p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_packet.cur += 4;
    gs_ad(p + 2, GS_FINISH, 1);
    Gs_packet.flush();
    while (!(GS_CSR & GS_CSR_FINISH)) {
    }
    GS_CSR = GS_CSR | GS_CSR_FINISH;
    if (Gs_wait_vsync && min_vsyncs) {
        int count = Gs_vsync_count;
        while (Gs_vsync_count == count) {
        }
        while (Gs_vsync_count < min_vsyncs) {
        }
    }
    Gs_vsync_count = 0;
    // Show the buffer just drawn and draw into the other. Both read circuits show it, the second
    // one line down, and PMODE blends them half and half against interlace flicker.
    if (Gs_frame & 1) {
        dispfb1 = GS_SET_DISPFB(GS_FRAME_FBP, GS_FBW, 0, 0, 0);
        dispfb2 = GS_SET_DISPFB(GS_FRAME_FBP, GS_FBW, 0, 0, 1);
        fbp = 0;
    } else {
        dispfb1 = GS_SET_DISPFB(0, GS_FBW, 0, 0, 0);
        dispfb2 = GS_SET_DISPFB(0, GS_FBW, 0, 0, 1);
        fbp = GS_FRAME_FBP;
    }
    GS_DISPFB1 = dispfb1;
    GS_DISPFB2 = dispfb2;
    GS_DISPLAY1 = GS_SET_DISPLAY(620, 50, 4, 0, 2559, 446);
    GS_DISPLAY2 = GS_SET_DISPLAY(620, 50, 4, 0, 2559, 446);
    GS_PMODE = GS_SET_PMODE(1, 1, 1, 1, 0, 0x80);
    GS_SMODE2 = GS_SET_SMODE2(1, 0, 0);
    p2 = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(8, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_packet.cur += 18;
    gs_ad(p2 + 2, GS_XYOFFSET_1, GS_SET_XYOFFSET(GS_OFFSET_X, GS_OFFSET_Y));
    gs_ad(p2 + 4, GS_XYOFFSET_2, GS_SET_XYOFFSET(GS_OFFSET_X, GS_OFFSET_Y));
    gs_ad(p2 + 6, GS_FRAME_1, GS_SET_FRAME(fbp, GS_FBW, 0, 0));
    gs_ad(p2 + 8, GS_FRAME_2, GS_SET_FRAME(fbp, GS_FBW, 0, 0));
    gs_ad(p2 + 10, GS_PRMODE, GS_SET_PRMODE(1, 1, 1, 1, 0, 0, 0, 0));
    gs_ad(p2 + 12, GS_PRMODECONT, 1);
    gs_ad(p2 + 14, GS_COLCLAMP, 1);
    gs_ad(p2 + 16, GS_SCANMSK, 0);
    p3 = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(7, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_packet.cur += 16;
    gs_ad(p3 + 2, GS_TEX1_1, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
    gs_ad(p3 + 4, GS_TEX1_2, GS_SET_TEX1(0, 0, 1, 1, 0, 0, 0));
    gs_ad(p3 + 6, GS_PABE, 0);
    gs_ad(p3 + 8, GS_FOGCOL, 0);
    gs_ad(p3 + 10, GS_ALPHA_2, GS_SET_ALPHA(1, 2, 0, 2, 0));
    gs_ad(p3 + 12, GS_CLAMP_2, GS_SET_CLAMP(1, 1, 0, 0, 0, 0));
    gs_ad(p3 + 14, GS_TEXA, GS_SET_TEXA(0, 1, 0x80));
    gr_screen.clip_left = 0;
    gr_screen.clip_right = 511;
    gr_screen.clip_top = 0;
    gr_screen.clip_bottom = 447;
    Gs_frame++;
    gr_ngps_set_clip();
    gr_ngps_set_alpha(GS_SET_ALPHA(0, 1, 0, 1, 0));
    gr_ngps_set_zbuffer(4);
    gr_ngps_set_clamp(0);
}

void gr_ngps_sprite(int bitmap, int x, int y) {
    int w;
    int h;

    bm_get_dimensions(bitmap, &w, &h);
    if (x >= 512 || y >= 448 || x + w <= 0 || y + h <= 0) {
        return;
    }
    gs_reserve();
    gr_ngps_tcache_set(bitmap, 0);
    u64* p = Gs_packet.cur;
    // RGBAQ, UV, XYZ2, UV, XYZ2.
    Gs_packet.cur[0] = GIF_TAG(1, 0, 1, GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST, 0, 5);
    Gs_packet.cur[1] = 0x53531;
    Gs_packet.cur += 12;
    int sx = Gs_state.offset_x + x * 16;
    int sy = Gs_state.offset_y + y * 16;
    gs_rgbaq(p + 2, gr_screen.current_color.r >> 1, gr_screen.current_color.g >> 1, gr_screen.current_color.b >> 1,
             (gr_screen.current_color.a + 1) >> 1);
    gs_uv(p + 4, 7, 7);
    gs_xyz2(p + 6, sx, sy, 0xFFFFFF);
    gs_uv(p + 8, w * 16, h * 16);
    gs_xyz2(p + 10, sx + w * 16, sy + h * 16, 0xFFFFFF);
}

void gr_ngps_sprite_scaled(int bitmap, int x, int y, int hot_x, int hot_y, float scale) {
    int w;
    int h;

    bm_get_dimensions(bitmap, &w, &h);
    if (x >= 512 || y >= 448 || x + w <= 0 || y + h <= 0) {
        return;
    }
    int sw = w * scale;
    int sh = h * scale;
    hot_x = hot_x * scale;
    hot_y = hot_y * scale;
    x -= hot_x;
    y -= hot_y;
    gs_reserve();
    gr_ngps_tcache_set(bitmap, 0);
    u64* p = Gs_packet.cur;
    // RGBAQ, UV, XYZ2, UV, XYZ2.
    Gs_packet.cur[0] = GIF_TAG(1, 0, 1, GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST, 0, 5);
    Gs_packet.cur[1] = 0x53531;
    Gs_packet.cur += 12;
    int sx = Gs_state.offset_x + x * 16;
    int sy = Gs_state.offset_y + y * 16;
    gs_rgbaq(p + 2, gr_screen.current_color.r >> 1, gr_screen.current_color.g >> 1, gr_screen.current_color.b >> 1,
             (gr_screen.current_color.a + 1) >> 1);
    gs_uv(p + 4, 7, 7);
    gs_xyz2(p + 6, sx, sy, 0xFFFFFF);
    gs_uv(p + 8, w * 16, h * 16);
    gs_xyz2(p + 10, sx + sw * 16, sy + sh * 16, 0xFFFFFF);
}

void gr_ngps_sprite_opaque(int bitmap, int x, int y) {
    int w;
    int h;

    bm_get_dimensions(bitmap, &w, &h);
    if (x >= 512 || y >= 448 || x + w <= 0 || y + h <= 0) {
        return;
    }
    gs_reserve();
    gr_ngps_tcache_set(bitmap, 1);
    u64* p = Gs_packet.cur;
    // UV, XYZ2, UV, XYZ2.
    Gs_packet.cur[0] = GIF_TAG(1, 0, 1, GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_FST, 0, 4);
    Gs_packet.cur[1] = 0x5353;
    Gs_packet.cur += 10;
    int sx = Gs_state.offset_x + x * 16;
    int sy = Gs_state.offset_y + y * 16;
    gs_uv(p + 2, 7, 7);
    gs_xyz2(p + 4, sx, sy, 0xFFFFFF);
    gs_uv(p + 6, w * 16, h * 16);
    gs_xyz2(p + 8, sx + w * 16, sy + h * 16, 0xFFFFFF);
}

void gr_ngps_billboard(int bitmap, const vector3* pos, float radius) {
    vertex v;
    vertex v1;
    vertex v2;

    v.proj_flags = 0;
    v.flags = 0;
    rotate_vertex(&v, *pos);
    if (v.pos.z <= 0.0f) {
        return;
    }
    float rx = radius * Matrix_scale.x;
    float ry = radius * Matrix_scale.y;
    v1.flags = 0;
    v1.proj_flags = 0;
    v2.flags = 0;
    v2.proj_flags = 0;
    v1.pos.x = v.pos.x - rx;
    v1.pos.y = v.pos.y + ry;
    v1.pos.z = v.pos.z;
    v2.pos.x = v.pos.x + rx;
    v2.pos.y = v.pos.y - ry;
    v2.pos.z = v.pos.z;
    project_vertex(&v1);
    if (v1.sx >= 512.0f || v1.sy >= 448.0f) {
        return;
    }
    project_vertex(&v2);
    if (v2.sx <= 0.0f || v2.sy <= 0.0f) {
        return;
    }
    gs_reserve();
    gr_ngps_tcache_set(bitmap, 1);
    float q = v1.sw;
    u32 z = (int)(16777215.0f * (0.0001f * q));
    u64* p = Gs_packet.cur;
    // (STQ, RGBAQ, XYZ2) x2. The colors are left as they are.
    Gs_packet.cur[0] = GIF_TAG(2, 0, 1, GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_ABE, 0, 3);
    Gs_packet.cur[1] = 0x512;
    Gs_packet.cur += 14;
    gs_stq(p + 2, 0.0f, 0.0f, q);
    gs_xyz2(p + 6, Gs_state.offset_x + 16.0f * v1.sx, Gs_state.offset_y + 16.0f * v1.sy, z);
    gs_stq(p + 8, q, q, q);
    gs_xyz2(p + 12, Gs_state.offset_x + 16.0f * v2.sx, Gs_state.offset_y + 16.0f * v2.sy, z);
}

// The size of a texture holding n pixels of a side: the next power of two, at most 256.
static inline int tile_size(int n) {
    return (n == 256 || n > 128) ? 256
         : (n == 128 || n > 64)  ? 128
         : (n == 64 || n > 32)   ? 64
         : (n == 32 || n > 16)   ? 32
         : (n == 16 || n > 8)    ? 16
         : (n == 8 || n > 4)     ? 8
         : (n == 4 || n > 2)     ? 4
         : (n == 2 || n > 1)     ? 2
                                 : 1;
}

void gr_ngps_bitmap_quad(int bitmap, int x, int y, int w, int h, int sx, int sy, int sw, int sh, gr_mode mode) {
    int bw, bh;
    vertex p0, p1, p2, p3;

    bm_get_dimensions(bitmap, &bw, &bh);
    float u_scale = 1.0f / bw;
    float v_scale = 1.0f / bh;
    float u0 = (sx + 0.5f) * u_scale;
    float v0 = (sy + 0.5f) * v_scale;
    float u1 = ((sx + sw) - 0.5f) * u_scale;
    float v1 = ((sy + sh) - 0.5f) * v_scale;
    int x1 = x + gr_screen.offset_x;
    int y1 = y + gr_screen.offset_y;
    int x2 = x + w + gr_screen.offset_x;
    int y2 = y + h + gr_screen.offset_y;
    u8 r = gr_screen.current_color.r;
    u8 g = gr_screen.current_color.g;
    u8 b = gr_screen.current_color.b;
    u8 a = gr_screen.current_color.a;
    vertex* list[4] = {&p0, &p1, &p2, &p3};

    gr_set_bitmap(bitmap, -1);

    p0.sw = BITMAP_DEPTH;
    p0.flags = VF_UV | VF_RGB | VF_ALPHA;
    p0.pos.z = 0.0f;
    p0.r = r;
    p0.g = g;
    p0.b = b;
    p0.a = a;
    p0.sx = x1;
    p0.sy = y1;
    p0.u = u0;
    p0.v = v0;
    p1.sw = BITMAP_DEPTH;
    p1.flags = VF_UV | VF_RGB | VF_ALPHA;
    p1.pos.z = 0.0f;
    p1.r = r;
    p1.g = g;
    p1.b = b;
    p1.a = a;
    p1.sx = x2;
    p1.sy = y1;
    p1.u = u1;
    p1.v = v0;
    p2.sw = BITMAP_DEPTH;
    p2.flags = VF_UV | VF_RGB | VF_ALPHA;
    p2.pos.z = 0.0f;
    p2.r = r;
    p2.g = g;
    p2.b = b;
    p2.a = a;
    p2.sx = x2;
    p2.sy = y2;
    p2.u = u1;
    p2.v = v1;
    p3.sw = BITMAP_DEPTH;
    p3.flags = VF_UV | VF_RGB | VF_ALPHA;
    p3.pos.z = 0.0f;
    p3.r = r;
    p3.g = g;
    p3.b = b;
    p3.a = a;
    p3.sx = x1;
    p3.sy = y2;
    p3.u = u0;
    p3.v = v1;
    gr_ngps_tmapper(4, list, mode);
}

// The data of a compressed bitmap starts with its size in bytes. Then, for each tile from left to
// right and top to bottom, comes the size of the tile's picture and the picture, each padded to
// 16 bytes. A picture covers its tile rounded up to multiples of 16 pixels.
void gr_ngps_bitmap_compressed(int bitmap, int x, int y, int w, int h, int sx, int sy, gr_mode mode) {
    int bw, bh;
    int tile_w, tile_h;
    int tbp, cbp;
    u8* data;
    u8* palette;
    u8 decoder[BM_DECODER_SIZE];
    vertex p0, p1, p2, p3;
    int tx;
    int ty;
    int size;
    int format;
    int px;
    int py;
    int th;
    int tw;

    bm_get_dimensions(bitmap, &bw, &bh);
    tile_h = tile_size(bh);
    tile_w = tile_size(bw);
    x -= sx;
    y -= sy;
    gr_ngps_set_render_texture(bitmap, tile_w, tile_h);
    gr_ngps_get_texture_vram(bitmap, &cbp, &tbp);
    format = bm_get_format(bitmap);
    bm_get_data(bitmap, &data, &palette);
    int offset = 4;
    u8 r = gr_screen.current_color.r;
    u8 g = gr_screen.current_color.g;
    u8 b = gr_screen.current_color.b;
    u8 a = gr_screen.current_color.a;
    func_001659E0(decoder);
    for (py = 0, ty = 0; ty < bh; ty += th, py += th) {
        th = MIN(tile_h, bh - ty);
        for (tx = 0, px = 0; tx < bw; tx += tw, px += tw) {
            tw = MIN(tile_w, bw - tx);
            size = *(int*)(data + offset);
            offset += 4;
            offset += PAD16(offset);
            Gs_packet.flush();
            gr_ngps_upload_init();
            u8* pixels = func_00166530(decoder, data + offset, tw + PAD16(tw), th + PAD16(th), size,
                                       format == BM_FORMAT_SIZED_2 ? 32 : 16);
            gr_ngps_upload_tiles((u32)pixels, format == BM_FORMAT_SIZED_2 ? 4 : 2, tw + PAD16(tw), th + PAD16(th),
                                 tile_w, tbp << 6, format == BM_FORMAT_SIZED_2 ? GS_PSMCT32 : GS_PSMCT16);
            gr_ngps_texflush();
            offset += size;
            offset += PAD16(offset);

            vertex* list[4] = {&p0, &p1, &p2, &p3};
            float u0 = 1.25f / bw;
            float v0 = 1.25f / bh;
            float u1 = (float)tw / (float)tile_w;
            float v1 = (float)th / (float)tile_h;
            int x1 = gr_screen.offset_x + x + px;
            int x2 = x1 + tw;
            int y1 = gr_screen.offset_y + y + py;
            int y2 = y1 + th;
            p0.sw = BITMAP_DEPTH;
            p0.flags = VF_UV | VF_RGB | VF_ALPHA;
            p0.pos.z = 0.0f;
            p0.r = r;
            p0.g = g;
            p0.b = b;
            p0.a = a;
            p0.sx = x1;
            p0.sy = y1;
            p0.u = u0;
            p0.v = v0;
            p1.sw = BITMAP_DEPTH;
            p1.flags = VF_UV | VF_RGB | VF_ALPHA;
            p1.pos.z = 0.0f;
            p1.r = r;
            p1.g = g;
            p1.b = b;
            p1.a = a;
            p1.sx = x2;
            p1.sy = y1;
            p1.u = u1;
            p1.v = v0;
            p2.sw = BITMAP_DEPTH;
            p2.flags = VF_UV | VF_RGB | VF_ALPHA;
            p2.pos.z = 0.0f;
            p2.r = r;
            p2.g = g;
            p2.b = b;
            p2.a = a;
            p2.sx = x2;
            p2.sy = y2;
            p2.u = u1;
            p2.v = v1;
            p3.sw = BITMAP_DEPTH;
            p3.flags = VF_UV | VF_RGB | VF_ALPHA;
            p3.pos.z = 0.0f;
            p3.r = r;
            p3.g = g;
            p3.b = b;
            p3.a = a;
            p3.sx = x1;
            p3.sy = y2;
            p3.u = u0;
            p3.v = v1;
            gr_ngps_tmapper(4, list, mode);
        }
    }
    func_00165A30(decoder, -1);
}

void gr_ngps_bitmap(int bitmap, int x, int y, int w, int h, int sx, int sy, int sw, int sh, gr_mode mode) {
    int tbp, cbp;
    int bw, bh;
    u32 qwc;
    u8* data;
    u8* palette;
    vertex p0, p1, p2, p3;
    int px;
    int py;
    int psm;
    int bytes_per_pixel;
    int tile_h;
    int tile_w;
    int th;
    int tw;
    int tx;
    int stride;
    int format;
    int i;

    if (bm_is_pow2(bitmap)) {
        gr_ngps_bitmap_quad(bitmap, x, y, w, h, sx, sy, sw, sh, mode);
        return;
    }
    format = bm_get_format(bitmap);
    if (format == BM_FORMAT_SIZED_2 || format == BM_FORMAT_SIZED_1) {
        gr_ngps_bitmap_compressed(bitmap, x, y, w, h, sx, sy, mode);
        return;
    }
    Gs_packet.flush();
    u8 r = gr_screen.current_color.r;
    u8 g = gr_screen.current_color.g;
    u8 b = gr_screen.current_color.b;
    u8 a = gr_screen.current_color.a;
    bm_get_dimensions(bitmap, &bw, &bh);
    tile_h = tile_size(bh);
    tile_w = tile_size(bw);
    gr_ngps_set_render_texture(bitmap, tile_w, tile_h);
    gr_ngps_get_texture_vram(bitmap, &cbp, &tbp);
    psm = gr_ngps_get_texture_psm(bitmap);
    bm_get_data(bitmap, &data, &palette);
    bytes_per_pixel = 4;
    if (format == BM_FORMAT_8BIT) {
        Gs_packet.flush();
        gr_ngps_upload_init();
        int palette_format = bm_get_palette_format(bitmap);
        gr_ngps_upload_image((u32)palette, cbp << 6, palette_format == BM_PALETTE_16BIT ? GS_PSMCT16 : GS_PSMCT32, 16,
                             16);
        bytes_per_pixel = 1;
    } else if (format == BM_FORMAT_16BIT || format == BM_FORMAT_CREATED_16) {
        bytes_per_pixel = 2;
    }
    stride = bw * bytes_per_pixel;
    for (py = 0; sy < bh; sy += th, py += th) {
        th = MIN(tile_h, bh - sy);
        for (tx = 0, px = 0; tx < bw; tx += tw, px += tw) {
            tw = MIN(tile_w, bw - tx);
            Gs_packet.flush();
            gr_ngps_upload_init();
            // The upload chain: the GIF setup, then a tag sending each row of the tile from the
            // bitmap's pixels.
            u64* tag = gr_ngps_upload_setup(tbp << 6, psm, tw, th, tile_w, &qwc);
            u32 row_qwc = (u32)(tw * bytes_per_pixel) >> 4;
            u8* src = data + tx * bytes_per_pixel + sy * stride;
            for (i = 0; i < th - 1; i++) {
                *tag = ((u64)(u32)src << 32) | DMA_TAG_REF | row_qwc;
                tag += 2;
                src += stride;
            }
            *tag = ((u64)(u32)src << 32) | DMA_TAG_REFE | row_qwc;
            D2_QWC = 0;
            D2_TADR = UPLOAD_CHAIN | D_TADR_SPR;
            D2_CHCR = D_CHCR_CHAIN;
            while (D2_CHCR & D_CHCR_STR) {
            }
            gr_ngps_texflush();

            vertex* list[4] = {&p0, &p1, &p2, &p3};
            float u0 = 1.25f / bw;
            float v0 = 1.25f / bh;
            float u1 = (float)tw / (float)tile_w;
            float v1 = (float)th / (float)tile_h;
            int x1 = gr_screen.offset_x + x + px;
            int x2 = x1 + tw;
            int y1 = gr_screen.offset_y + y + py;
            int y2 = y1 + th;
            p0.sw = BITMAP_DEPTH;
            p0.flags = VF_UV | VF_RGB | VF_ALPHA;
            p0.pos.z = 0.0f;
            p0.r = r;
            p0.g = g;
            p0.b = b;
            p0.a = a;
            p0.sx = x1;
            p0.sy = y1;
            p0.u = u0;
            p0.v = v0;
            p1.sw = BITMAP_DEPTH;
            p1.flags = VF_UV | VF_RGB | VF_ALPHA;
            p1.pos.z = 0.0f;
            p1.r = r;
            p1.g = g;
            p1.b = b;
            p1.a = a;
            p1.sx = x2;
            p1.sy = y1;
            p1.u = u1;
            p1.v = v0;
            p2.sw = BITMAP_DEPTH;
            p2.flags = VF_UV | VF_RGB | VF_ALPHA;
            p2.pos.z = 0.0f;
            p2.r = r;
            p2.g = g;
            p2.b = b;
            p2.a = a;
            p2.sx = x2;
            p2.sy = y2;
            p2.u = u1;
            p2.v = v1;
            p3.sw = BITMAP_DEPTH;
            p3.flags = VF_UV | VF_RGB | VF_ALPHA;
            p3.pos.z = 0.0f;
            p3.r = r;
            p3.g = g;
            p3.b = b;
            p3.a = a;
            p3.sx = x1;
            p3.sy = y2;
            p3.u = u0;
            p3.v = v1;
            gr_ngps_tmapper(4, list, mode);
        }
    }
}

void gr_ngps_line(float x1, float y1, float x2, float y2) {
    gs_reserve();
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 1, GS_PRIM_LINE | GS_PRIM_ABE, 0, 3);
    Gs_packet.cur[1] = GIF_REGS_RGBAQ_XYZ2_X2;
    Gs_packet.cur += 8;
    gs_rgbaq(p + 2, gr_screen.current_color.r, gr_screen.current_color.g, gr_screen.current_color.b,
             (gr_screen.current_color.a + 1) >> 1);
    gs_xyz2(p + 4, Gs_state.offset_x + x1 * 16.0f, Gs_state.offset_y + y1 * 16.0f, 0xFFFFFF);
    gs_xyz2(p + 6, Gs_state.offset_x + x2 * 16.0f, Gs_state.offset_y + y2 * 16.0f, 0xFFFFFF);
}

void gr_ngps_aaline(float x1, float y1, float x2, float y2) {
    GsPacket& packet = Gs_packet;
    if (packet.cur - packet.base >= GS_PACKET_LIMIT) {
        *packet.cur = GIF_EOP_TAG;
        packet.cur += 2;
        packet.kick();
    }
    u64* p = packet.cur;
    packet.cur[0] = GIF_TAG(1, 0, 1, GS_PRIM_LINE | GS_PRIM_AA1, 0, 3);
    packet.cur[1] = GIF_REGS_RGBAQ_XYZ2_X2;
    packet.cur += 8;
    color& c = gr_screen.current_color;
    gs_rgbaq(p + 2, c.r, c.g, c.b, (c.a + 1) >> 1);
    gs_xyz2(p + 4, Gs_state.offset_x + x1 * 16.0f, Gs_state.offset_y + y1 * 16.0f, 0xFFFFFF);
    gs_xyz2(p + 6, Gs_state.offset_x + x2 * 16.0f, Gs_state.offset_y + y2 * 16.0f, 0xFFFFFF);
}

void gr_ngps_line_3d(vertex* v1, vertex* v2, gr_mode mode) {
    gs_reserve();
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 1, GS_PRIM_LINE | GS_PRIM_ABE, 0, 3);
    Gs_packet.cur[1] = GIF_REGS_RGBAQ_XYZ2_X2;
    Gs_packet.cur += 8;
    gs_rgbaq(p + 2, gr_screen.current_color.r, gr_screen.current_color.g, gr_screen.current_color.b,
             (gr_screen.current_color.a + 1) >> 1);
    gs_xyz2(p + 4, Gs_state.offset_x + v1->sx * 16.0f, Gs_state.offset_y + v1->sy * 16.0f,
            (int)(v1->sw * GS_DEPTH_SCALE));
    gs_xyz2(p + 6, Gs_state.offset_x + v2->sx * 16.0f, Gs_state.offset_y + v2->sy * 16.0f,
            (int)(v2->sw * GS_DEPTH_SCALE));
}

void gr_ngps_rect(int x1, int y1, int x2, int y2) {
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 1, GS_PRIM_SPRITE | GS_PRIM_ABE, 0, 3);
    Gs_packet.cur[1] = GIF_REGS_RGBAQ_XYZ2_X2;
    Gs_packet.cur += 8;
    gs_rgbaq(p + 2, gr_screen.current_color.r, gr_screen.current_color.g, gr_screen.current_color.b,
             (gr_screen.current_color.a + 1) >> 1);
    gs_xyz2(p + 4, Gs_state.offset_x + x1 * 16, Gs_state.offset_y + y1 * 16, 0xFFFFFF);
    gs_xyz2(p + 6, Gs_state.offset_x + x2 * 16, Gs_state.offset_y + y2 * 16, 0xFFFFFF);
}

void gr_ngps_fill(u8 r, u8 g, u8 b, u8 a) {
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_REGS_RGBAQ;
    Gs_packet.cur += 4;
    gs_rgbaq(p + 2, r, g, b, a);
    int strips = 16;
    p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(strips, 0, 1, GS_PRIM_SPRITE | GS_PRIM_ABE, 0, 2);
    Gs_packet.cur[1] = GIF_REGS_XYZ2_XYZ2;
    Gs_packet.cur += strips * 4 + 2;
    int x = Gs_state.offset_x;
    int y = Gs_state.offset_y;
    for (int i = strips; i > 0; i--) {
        gs_xyz2(p + 2, x, y, 0xFFFFFF);
        x += 32 << 4;
        gs_xyz2(p + 4, x, y + (448 << 4), 0xFFFFFF);
        p += 4;
    }
}

void gr_ngps_tmapper(int nverts, vertex** verts, gr_mode mode) {
    gs_reserve();
    // Usual blending, (Cs - Cd) * As + Cd, unless the mode is one of two special ones: Cd * As
    // (darkening) or Cs * As + Cd (additive).
    u64 alpha = GS_SET_ALPHA(0, 1, 0, 1, 0);
    if (D_002D4694.value == mode.value) {
        alpha = GS_SET_ALPHA(1, 2, 0, 2, 0);
    } else if (D_002D4690.value == mode.value) {
        alpha = GS_SET_ALPHA(0, 2, 0, 1, 0);
    }
    gr_ngps_set_alpha(alpha);
    u64 prim;
    if (mode.value & GR_MODE_TEXTURE_MASK) {
        if (gr_screen.current_bitmap == -1) {
            return;
        }
        gr_ngps_tcache_set(gr_screen.current_bitmap, 0);
        prim = GS_PRIM_TRIFAN | GS_PRIM_IIP | GS_PRIM_TME | GS_PRIM_ABE;
    } else {
        prim = GS_PRIM_TRIFAN | GS_PRIM_IIP | GS_PRIM_ABE;
    }
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(nverts, 0, 1, prim, 0, 3);
    Gs_packet.cur[1] = GIF_REGS_STQ_RGBAQ_XYZ2;
    Gs_packet.cur += nverts * 6 + 2;
    int i = 0;
    do {
        vertex* v = verts[i];
        float q = v->sw;
        float depth = q * GS_DEPTH_RANGE;
        int z = depth * GS_DEPTH_MAX;
        gs_stq(p + 2, v->u * q, v->v * q, q);
        int a = (v->flags & VF_ALPHA) ? v->a : gr_screen.current_color.a;
        int r, g, b;
        if (v->flags & VF_RGB) {
            r = v->r;
            g = v->g;
            b = v->b;
        } else {
            r = gr_screen.current_color.r;
            g = gr_screen.current_color.g;
            b = gr_screen.current_color.b;
        }
        a = (a + 1) >> 1;
        gs_rgbaq(p + 4, r >> 1, g >> 1, b >> 1, a);
        gs_xyz2(p + 6, Gs_state.offset_x + v->sx * 16.0f, Gs_state.offset_y + v->sy * 16.0f, z);
        p += 6;
        i++;
    } while (i < nverts);
}

// Sets FOGCOL and keeps its value in Gs_state. The components come highest byte first.
inline void gs_set_fogcol(int b, int g, int r) {
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_state.fog_color = (b << 16) | (g << 8) | r;
    Gs_packet.cur += 4;
    gs_ad(p + 2, GS_FOGCOL, Gs_state.fog_color);
}

void gr_ngps_set_fog(void) {
    Gs_state.fog_scale = 255.0f / (gr_screen.fog_far - gr_screen.fog_near);
    gs_set_fogcol(gr_screen.fog_color.b, gr_screen.fog_color.g, gr_screen.fog_color.r);
}

void gr_ngps_flush_textures(void) { gr_ngps_tcache_flush(); }

void gr_ngps_copy_vram(u32 dbp, u32 sbp, int sbw, int dbw, int w, int h, int psm) {
    Gs_packet.flush();
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(4, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_packet.cur += 10;
    gs_ad(p + 2, GS_BITBLTBUF, GS_SET_BITBLTBUF(sbp, sbw / 64, psm, dbp, dbw / 64, psm));
    gs_ad(p + 4, GS_TRXPOS, GS_SET_TRXPOS(0, 0, 0, 0, 0));
    gs_ad(p + 6, GS_TRXREG, GS_SET_TRXREG(w, h));
    gs_ad(p + 8, GS_TRXDIR, GS_TRX_LOCAL);
    Gs_packet.flush();
    gr_ngps_texflush();
}

u32 gr_ngps_back_buffer(void) { return (Gs_frame & 1) ? GS_FRAME_FBP * GS_PAGE_BLOCKS : 0; }

u32 gr_ngps_front_buffer(void) { return (Gs_frame & 1) ? 0 : GS_FRAME_FBP * GS_PAGE_BLOCKS; }

void gr_ngps_set_zbuffer(int mode) {
    u64 test;
    u64 zbuf;

    switch (mode) {
    case 4:
        test = GS_SET_TEST(1, GS_GREATER, 1, 0, 0, 0, 1, GS_GEQUAL);
        zbuf = GS_SET_ZBUF(GS_ZBUF_ZBP, GS_PSMZ24, 0);
        break;
    case 1:
        test = GS_SET_TEST(1, GS_GREATER, 1, 0, 0, 0, 1, GS_GEQUAL);
        zbuf = GS_SET_ZBUF(GS_ZBUF_ZBP, GS_PSMZ24, 1);
        break;
    case 0:
        test = GS_SET_TEST(1, GS_GREATER, 1, 0, 0, 0, 1, GS_ALWAYS);
        zbuf = GS_SET_ZBUF(GS_ZBUF_ZBP, GS_PSMZ24, 1);
        break;
    default:
        return;
    }
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(2, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_packet.cur += 6;
    gs_ad(p + 2, GS_TEST_1, test);
    gs_ad(p + 4, GS_ZBUF_1, zbuf);
}

void gr_ngps_set_alpha(u64 alpha) {
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_packet.cur += 4;
    gs_ad(p + 2, GS_ALPHA_1, alpha);
}

void gr_ngps_set_clamp(int mode) {
    u64* p = Gs_packet.cur;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 0, 0, 0, 1);
    Gs_packet.cur[1] = GIF_AD;
    Gs_packet.cur += 4;
    gs_ad(p + 2, GS_CLAMP_1, GS_SET_CLAMP(mode, mode, 0, 0, 0, 0));
}

void gr_ngps_texflush(void) {
    D2_MADR = (u32)Gs_texflush_packet;
    D2_QWC = 2;
    D2_CHCR = D_CHCR_STR;
    while (D2_CHCR & D_CHCR_STR) {
    }
}

float gr_ngps_read_depth(int x, int y) {
    func_001CD3F8(0, 0);
    func_001CD710(DEPTH_STORE_IMAGE, GS_ZBUF_ZBP * GS_PAGE_BLOCKS, GS_FBW, GS_PSMZ24, x, y, 1, 1);
    while (func_001CD850(DEPTH_STORE_IMAGE, Gs_store_buffer)) {
    }
    func_001CD3F8(0, 0);
    float depth = Gs_store_buffer[0] & 0xFFFFFF;
    depth /= GS_DEPTH_SCALE;
    return depth;
}

void gr_ngps_sync(void) {
    Gs_packet.flush();
    gr_ngps_tcache_flush();
    func_001CD3F8(0, 0);
    func_001CD360(0);
}
