#ifndef GAME_GR_NGPS_TEXTURE_H
#define GAME_GR_NGPS_TEXTURE_H

#include "types.h"

// Texture cache of the PS2 renderer: bitmaps resident in GS local memory (VRAM), the transfers that
// put them there, and the GS register values that select them. Also the converters that turn
// bitmap pixels into the GS 16-bit and 32-bit formats.
//
// VRAM addresses are in words (a GS block address times 64) unless a name says otherwise. Functions
// taking `bitmap` take a bitmap handle and work on its current animation frame (bm_get_frame);
// those taking `handle` take the frame's handle itself.

// A run of VRAM, in 64-word blocks. Runs from the allocation cursor up sit in Vram_free_list in
// address order (one there may still hold a bitmap until the cursor reaches it); runs given out
// since the cursor last wrapped sit in Vram_used_list, newest first.
struct VramBlock {
    /* 0x00 */ u64 tex0;    // TEX0_1 value that selects the bitmap
    /* 0x08 */ u64 tex1;    // TEX1_1
    /* 0x10 */ u64 miptbp1; // MIPTBP1_1
    /* 0x18 */ VramBlock* next;
    /* 0x1C */ u16 size;    // in blocks
    /* 0x1E */ s16 owner;   // handle of the bitmap in the run, -1 if none
};

#define NUM_VRAM_BLOCKS 256
#define NUM_FIXED_BLOCKS 64

// Pixel formats the texture converters read (bm_create's source_format). Channels are listed from
// the high bits down; the 24-bit and 32-bit formats are stored as bytes blue, green, red (, alpha).
#define BM_SOURCE_565  3 // R5G6B5
#define BM_SOURCE_4444 4 // A4R4G4B4
#define BM_SOURCE_1555 5 // X1R5G5B5, the top bit set on transparent pixels
#define BM_SOURCE_888  6 // R8G8B8
#define BM_SOURCE_8888 7 // A8R8G8B8

extern u8 Bm_format_psm[10];
extern u32 Vram_cursor;
extern u32 Vram_start;
extern u32 Vram_end;
extern u32 Vram_limit;
extern VramBlock Vram_blocks[NUM_VRAM_BLOCKS];
extern VramBlock Vram_block_pool; // only next is used: the unused entries of Vram_blocks
extern VramBlock Vram_used_list;  // only next is used
extern VramBlock Vram_free_list;  // only next is used
extern u64 Upload_chain[0x2000];
extern VramBlock Fixed_blocks[NUM_FIXED_BLOCKS];

// Image transfers through scratchpad memory, one at a time.
void gr_ngps_upload_init(void);
u64* gr_ngps_upload_setup(u32 vram, int psm, int w, int h, int bw, u32* qwc);
void gr_ngps_upload_image(u32 data, u32 vram, int psm, int w, int h);
// Image transfer in 16 x 16 tiles, as one DMA chain.
void gr_ngps_upload_tiles(u32 data, int bytes_per_pixel, int w, int h, int bw, int vram, int psm);

u64 gr_ngps_make_tex0(int tbp, int psm, int w, int h, int cbp, int cpsm);
void gr_ngps_set_vram_regs(int handle, int w, int h, int tbp, int cbp);
VramBlock* gr_ngps_upload_bitmap(int handle);

void gr_ngps_tcache_flush(void);
void gr_ngps_tcache_init(u32 vram_start, u32 vram_end, u32 vram_limit);
void gr_ngps_tcache_unload(int bitmap);
void gr_ngps_tcache_set(int bitmap, u32 tfx);
// regs points at three A+D pairs; only their data halves are written.
void gr_ngps_get_texture_regs(int bitmap, u64 tex0_flags, u64* regs);
void gr_ngps_get_texture_regs_flush(int bitmap, u64 tex0_flags, u64* regs);
void gr_ngps_set_framebuffer_texture(u32 fbp);
void gr_ngps_set_front_buffer_texture(void);
void gr_ngps_set_back_buffer_texture(void);
void gr_ngps_set_render_texture(int bitmap, int w, int h);
int gr_ngps_get_texture_psm(int bitmap);
void gr_ngps_get_texture_vram(int bitmap, int* cbp, int* tbp);
int gr_ngps_alloc_fixed_texture(int handle, int slot);
void gr_ngps_build_tile_chain(u64* chain, u32 data, int bytes_per_pixel, int w, int h, int bw, int vram,
                              int psm);

// Writes a DMA tag. addr loses its low 4 bits (it must be quadword aligned).
void set_dma_tag(u64* tag, u32 spr, u32 addr, u32 irq, u32 id, u32 pce, u32 qwc);
// Writes a GIF tag (a quadword) as four words.
void set_gif_tag(u32* tag, u64 regs, u32 nreg, u32 flg, u32 prim, u32 pre, u32 eop, u32 nloop);
// Each writes one A+D quadword setting its GS register. The source side of BITBLTBUF and TRXPOS
// is left 0.
void set_bitbltbuf(u32* p, u32 dbp, u32 dbw, u32 dpsm);
void set_trxpos(u32* p, u32 dir, u32 dsax, u32 dsay);
void set_trxreg(u32* p, u32 rrw, u32 rrh);
void set_trxdir(u32* p, u32 xdir);
// Starts a GIF (channel 2) DMA chain at tag. Does not wait for it.
void start_gif_dma_chain(void* tag);

// Each converter turns count pixels at src into the GS format at dst.
void convert_1555_to_16(u16* dst, u16* src, int count);
void convert_565_to_16(u16* dst, u16* src, int count);
void convert_4444_to_16(u16* dst, u16* src, int count);
void convert_888_to_16(u16* dst, u8* src, int count);
void convert_8888_to_16(u16* dst, u8* src, int count);
void convert_1555_to_32(u32* dst, u16* src, int count);
void convert_565_to_32(u32* dst, u16* src, int count);
void convert_4444_to_32(u32* dst, u16* src, int count);
void convert_888_to_32(u32* dst, u8* src, int count);
void convert_8888_to_32(u32* dst, u8* src, int count);

// Convert count pixels of the given BM_SOURCE_ format to 16-bit or 32-bit GS pixels, into dst, or
// into a new array when dst is NULL. Return the pixels, or NULL (after a hard error) for a format
// they cannot convert.
u16* convert_texture_to_16(void* src, int format, int count, u16* dst);
u32* convert_texture_to_32(void* src, int format, int count, u32* dst);

#endif // GAME_GR_NGPS_TEXTURE_H
