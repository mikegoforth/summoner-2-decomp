// Texture cache of the PS2 renderer. A bitmap is sent to GS local memory (VRAM) the first time it
// is drawn and stays there until the allocator, which hands out VRAM like a ring buffer, comes
// round to its space again. The file ends with the builders for the DMA tags, GIF tags and A+D
// register writes of an image transfer, the kick for a GIF DMA chain, and the converters that turn
// bitmap pixels into the GS 16-bit and 32-bit formats.
#include "game/gr_ngps_texture.h"
#include "game/bmpman_ngps.h"
#include "game/gr.h"
#include "game/unit_00187F80.h"
#include "macros.h"

// EE registers
#define D2_CHCR (*(volatile u32*)0x1000A000)
#define D2_QWC  (*(volatile u32*)0x1000A020)
#define D2_TADR (*(volatile u32*)0x1000A030)
#define D_STAT  (*(volatile u32*)0x1000E010)
#define D_PCR   (*(volatile u32*)0x1000E020)

#define D_CHCR_STR 0x100 // the channel is transferring
// Start a chain transfer from memory (DIR | MOD chain | STR).
#define D_CHCR_SEND_CHAIN 0x105

// Single uploads are put together in scratchpad memory.
#define SPR ((u64*)0x70000000)

// GS registers written through A+D
#define GS_TEX0_1    0x06
#define GS_TEX1_1    0x14
#define GS_MIPTBP1_1 0x34
#define GS_ALPHA_1   0x42
#define GS_BITBLTBUF 0x50
#define GS_TRXPOS    0x51
#define GS_TRXREG    0x52
#define GS_TRXDIR    0x53

// GS pixel storage modes
#define GS_PSMCT32  0x00
#define GS_PSMCT16  0x02
#define GS_PSMCT16S 0x0A
#define GS_PSMT8    0x13
#define GS_PSMT4    0x14

// Writes an A+D pair: data to GS register reg.
static inline void gs_ad(u64* p, u32 reg, u64 data) {
    ((u32*)p)[2] = reg;
    p[0] = data;
}

// Leading sign bits of x, less one: 30 - lzc(x) is log2(x) for a power of two.
static inline u32 lzc(int x) {
    u32 count;
    asm volatile("plzcw %0, %1" : "=r"(count) : "r"(x));
    return count;
}

// GS pixel storage mode of each bitmap_entry::format.
u8 Bm_format_psm[10] = {GS_PSMCT32, GS_PSMCT16, GS_PSMCT32, GS_PSMCT16,  GS_PSMT8,
                        GS_PSMT4,   GS_PSMCT32, GS_PSMCT32, GS_PSMCT16S, GS_PSMCT16};

u32 Vram_cursor; // next block the cache hands out
u32 Vram_start;  // VRAM the cache manages, in words
u32 Vram_end;    // end of the cache, where the fixed textures start
u32 Vram_limit;  // end of the fixed textures

VramBlock Vram_blocks[NUM_VRAM_BLOCKS];
VramBlock Vram_block_pool; // list of the unused entries of Vram_blocks
VramBlock Vram_used_list;  // runs holding a bitmap, newest first
VramBlock Vram_free_list;  // runs from the cursor up, in address order
u64 Upload_chain[0x2000];  // DMA chain built by gr_ngps_upload_tiles
VramBlock Fixed_blocks[NUM_FIXED_BLOCKS];

// The two allocator functions are static and come before their callers, which keep values live
// across the calls in the registers these leave alone.

// Puts every entry back in the pool and makes all of the cache's VRAM one free run.
static void gr_ngps_vram_reset(void) {
    int i;
    VramBlock* block;

    Vram_block_pool.next = NULL;
    for (i = 0; i < NUM_VRAM_BLOCKS; i++) {
        Vram_blocks[i].next = Vram_block_pool.next;
        Vram_block_pool.next = &Vram_blocks[i];
    }
    Vram_used_list.next = NULL;
    Vram_free_list.next = NULL;
    Vram_cursor = Vram_start >> 6;
    block = Vram_block_pool.next;
    Vram_block_pool.next = block->next;
    block->size = (Vram_end - Vram_start) >> 6;
    block->owner = -1;
    block->next = Vram_free_list.next;
    Vram_free_list.next = block;
}

// Takes the next `blocks` blocks of VRAM for the bitmap, unloading whatever is there. Returns the
// address in words.
static u32 gr_ngps_vram_alloc(short handle, u32 blocks) {
    u32 total;
    VramBlock* block;
    u32 addr;
    bitmap_entry* bitmaps;

    block = Vram_free_list.next;
    total = 0;
    while (block != NULL) {
        total += block->size;
        if (total >= blocks) {
            break;
        }
        block = block->next;
    }
    if (block == NULL) {
        // Not enough room before the end: unload everything and start again from the bottom.
        for (block = Vram_free_list.next; block != NULL; block = block->next) {
            if (block->owner != -1) {
                bm_bitmaps[block->owner].vram = NULL;
                block->owner = -1;
            }
        }
        while ((block = Vram_used_list.next) != NULL) {
            Vram_used_list.next = block->next;
            block->next = Vram_free_list.next;
            Vram_free_list.next = block;
        }
        Vram_cursor = Vram_start >> 6;
    }
    bitmaps = bm_bitmaps;
    total = 0;
    do {
        block = Vram_free_list.next;
        if (block->owner != -1) {
            bitmaps[block->owner].vram = NULL;
        }
        total += block->size;
        if (total <= blocks) {
            block = Vram_free_list.next;
            Vram_free_list.next = block->next;
            block->next = Vram_block_pool.next;
            Vram_block_pool.next = block;
        } else {
            block->owner = -1;
            block->size = total - blocks;
            break;
        }
    } while (total < blocks);
    block = Vram_block_pool.next;
    Vram_block_pool.next = block->next;
    block->size = blocks;
    block->owner = handle;
    block->next = Vram_used_list.next;
    Vram_used_list.next = block;
    bitmaps[handle].vram = block;
    addr = Vram_cursor;
    Vram_cursor = addr + blocks;
    return addr << 6;
}

// Sets up the fixed part of the image transfer packet in scratchpad memory: a CNT DMA tag, then a
// GIF tag with BITBLTBUF, TRXPOS, TRXREG and TRXDIR as A+D. gr_ngps_upload_setup fills in the rest.
void gr_ngps_upload_init(void) {
    SPR[0] = 0x10000006;         // CNT, 6 quadwords
    SPR[2] = 0x4000000000000001; // NLOOP 1, NREG 4
    SPR[3] = 0xEEEE;             // A+D
    SPR[5] = GS_BITBLTBUF;
    SPR[7] = GS_TRXPOS;
    SPR[6] = 0;
    SPR[9] = GS_TRXREG;
    SPR[11] = GS_TRXDIR;
    SPR[10] = 0; // host to local
    D2_QWC = 0;
}

// Fills in the transfer packet for a w x h image going to vram, sets *qwc to its size in
// quadwords and returns where the DMA tag pointing at the image goes.
u64* gr_ngps_upload_setup(u32 vram, int psm, int w, int h, int bw, u32* qwc) {
    int size;
    int dbw;

    size = w * h;
    if (psm == GS_PSMCT16) {
        size *= 2;
    } else if (psm == GS_PSMT4) {
        size /= 2;
    } else if (psm == GS_PSMCT32) {
        size *= 4;
    }
    *qwc = (size + 15) / 16;
    dbw = (bw + 63) / 64;
    if (psm == GS_PSMT8 || psm == GS_PSMT4) {
        dbw = (dbw + 1) & ~1;
    }
    SPR[12] = (u64)*qwc | 0x8000 | ((u64)2 << 58); // EOP, IMAGE
    SPR[4] = ((u64)(vram >> 6) << 32) | ((u64)dbw << 48) | ((u64)psm << 56);
    SPR[8] = (u64)w | ((u64)h << 32);
    return &SPR[14];
}

// Sends the image at data (main memory) to vram and waits for the transfer.
void gr_ngps_upload_image(u32 data, u32 vram, int psm, int w, int h) {
    u32 qwc;
    u64* tag;

    tag = gr_ngps_upload_setup(vram, psm, w, h, w, &qwc);
    *tag = ((u64)data << 32) | qwc; // REFE
    D_PCR = 4;                      // CPCOND: channel 2
    D_STAT = 4;
    D2_TADR = 0xF0000000; // the packet, in scratchpad memory
    D2_CHCR = 0x104;      // chain mode, start
    // Wait on COP0's condition line, which follows channel 2.
    asm {
        sync
        sync.p
    @1:
        bc0f @1
        nop
    }
}

// Sends a w x h image to vram as 16 x 16 tiles, bytes_per_pixel at a time, and waits for it.
void gr_ngps_upload_tiles(u32 data, int bytes_per_pixel, int w, int h, int bw, int vram, int psm) {
    gr_ngps_build_tile_chain(Upload_chain, data, bytes_per_pixel, w, h, bw, vram, psm);
    start_gif_dma_chain(Upload_chain);
    while (D2_CHCR & D_CHCR_STR) {
    }
}

// TEX0 for a w x h texture (powers of two) at tbp with its CLUT at cbp, both in words.
u64 gr_ngps_make_tex0(int tbp, int psm, int w, int h, int cbp, int cpsm) {
    u32 tw;
    u32 th;
    u32 tbw;

    tw = 30 - lzc(w);
    th = 30 - lzc(h);
    tbw = (w + 63) / 64;
    if (psm == GS_PSMT8 || psm == GS_PSMT4) {
        tbw = (tbw + 1) & ~1;
    }
    // TCC 1 (RGBA), CLD 2 (load the CLUT when CBP changes)
    return (u64)(tbp / 64) | ((u64)tbw << 14) | ((u64)psm << 20) | ((u64)tw << 26) | ((u64)th << 30) |
           ((u64)1 << 34) | ((u64)(cbp / 64) << 37) | ((u64)cpsm << 51) | ((u64)2 << 61);
}

// Sets TEX0 and TEX1 of a bitmap that has a VRAM run.
void gr_ngps_set_vram_regs(int handle, int w, int h, int tbp, int cbp) {
    bitmap_entry* bm;
    VramBlock* block;
    int psm;
    int cpsm;

    bm = &bm_bitmaps[handle];
    psm = Bm_format_psm[bm->format];
    block = bm_bitmaps[handle].vram;
    cpsm = bm->palette_format == BM_PALETTE_16BIT ? GS_PSMCT16 : GS_PSMCT32;
    block->tex0 = gr_ngps_make_tex0(tbp, psm, w, h, cbp, cpsm);
    // MXL mipmap levels - 1, MMAG linear, MMIN linear mipmap linear, L 1, K from the bitmap
    block->tex1 = (u64)0x80160 | ((u64)(bm->num_mip_levels - 1) << 2) | ((u64)bm->unknown_1E << 32);
}

// Gives the bitmap a VRAM run and sends its CLUT, image and mipmaps there.
VramBlock* gr_ngps_upload_bitmap(int handle) {
    bitmap_entry* bm;
    int w;
    int h;
    int mips;
    int format;
    u32 data;
    u32 palette;
    u32 vram;
    u32 tbp;
    int cpsm;
    u64 tbw; // buffer width of a mipmap level
    u64 miptbp;
    u64 shift;
    VramBlock* block;

    bm = &bm_bitmaps[handle];
    mips = bm->num_mip_levels - 1;
    w = bm->width;
    h = bm->height;
    format = bm->format;
    data = (u32)bm->data;
    palette = (u32)bm->palette;
    vram = gr_ngps_vram_alloc(handle, bm->vram_size);
    miptbp = 0;
    shift = 0;
    switch (format) {
    case BM_FORMAT_8BIT:
        // The CLUT takes the first page, the image the rest. A PSMT8 page is 128 x 64.
        cpsm = bm->palette_format == BM_PALETTE_16BIT ? GS_PSMCT16 : GS_PSMCT32;
        tbp = vram + 0x800;
        gr_ngps_upload_image(palette, vram, cpsm, 16, 16);
        gr_ngps_upload_image(data, tbp, GS_PSMT8, w, h);
        gr_ngps_set_vram_regs(handle, w, h, tbp, vram);
        if (mips > 0) {
            tbw = (w + 127) / 128 * 2;
            tbp += ((h + 63) & ~63) * (tbw * 16);
            do {
                data += w * h;
                w /= 2;
                h /= 2;
                gr_ngps_upload_image(data, tbp, GS_PSMT8, w, h);
                tbw = (w + 127) / 128 * 2;
                miptbp |= ((tbw << 14) | (u64)(tbp >> 6)) << shift;
                shift += 20;
                tbp += ((h + 63) & ~63) * (tbw * 16);
            } while (--mips != 0);
        }
        break;
    case BM_FORMAT_4BIT:
        // A PSMT4 page is 128 x 128.
        cpsm = bm->palette_format == BM_PALETTE_16BIT ? GS_PSMCT16 : GS_PSMCT32;
        tbp = vram + 0x800;
        gr_ngps_upload_image(palette, vram, cpsm, 8, 2);
        gr_ngps_upload_image(data, tbp, GS_PSMT4, w, h);
        gr_ngps_set_vram_regs(handle, w, h, tbp, vram);
        if (mips > 0) {
            tbw = (w + 127) / 128 * 2;
            tbp += ((h + 127) & ~127) * (tbw * 8);
            do {
                data += w / 2 * h;
                w /= 2;
                h /= 2;
                gr_ngps_upload_image(data, tbp, GS_PSMT4, w, h);
                tbw = (w + 127) / 128 * 2;
                miptbp |= ((tbw << 14) | (u64)(tbp >> 6)) << shift;
                shift += 20;
                tbp += ((h + 127) & ~127) * (tbw * 8);
            } while (--mips != 0);
        }
        break;
    case BM_FORMAT_16BIT:
    case BM_FORMAT_CREATED_16S:
    case BM_FORMAT_CREATED_16:
        // A PSMCT16 page is 64 x 64.
        gr_ngps_upload_image(data, vram, GS_PSMCT16, w, h);
        gr_ngps_set_vram_regs(handle, w, h, vram, 0);
        if (mips > 0) {
            tbw = (w + 63) / 64;
            vram += ((h + 63) & ~63) * (tbw * 32);
            do {
                data += w * h * 2;
                w /= 2;
                h /= 2;
                gr_ngps_upload_image(data, vram, GS_PSMCT16, w, h);
                tbw = (w + 63) / 64;
                miptbp |= ((tbw << 14) | (u64)(vram >> 6)) << shift;
                shift += 20;
                vram += ((h + 63) & ~63) * (tbw * 32);
            } while (--mips != 0);
        }
        break;
    case BM_FORMAT_32BIT:
    case BM_FORMAT_PEG_32BIT:
        gr_ngps_upload_image(data, vram, GS_PSMCT32, w, h);
        gr_ngps_set_vram_regs(handle, w, h, vram, 0);
        break;
    }
    block = bm->vram;
    block->miptbp1 = miptbp;
    return block;
}

// Forgets every bitmap in VRAM.
void gr_ngps_tcache_flush(void) {
    int i;

    for (i = 0; i < BM_MAX_BITMAPS; i++) {
        bm_bitmaps[i].vram = NULL;
    }
    gr_ngps_vram_reset();
}

// The cache gets VRAM from vram_start to vram_end and the fixed textures from there to vram_limit,
// all in words.
void gr_ngps_tcache_init(u32 vram_start, u32 vram_end, u32 vram_limit) {
    Vram_start = vram_start;
    Vram_end = vram_end;
    Vram_limit = vram_limit;
    gr_ngps_tcache_flush();
}

// Forgets the VRAM copy of the bitmap's current frame.
void gr_ngps_tcache_unload(int bitmap) {
    int handle;

    handle = bm_get_frame(bitmap);
    if (handle >= 0 && bm_bitmaps[handle].vram != NULL) {
        bm_bitmaps[handle].vram->owner = -1;
        bm_bitmaps[handle].vram = NULL;
    }
}

// Selects the bitmap's current frame as the texture, uploading it first if needed. tfx goes to
// TEX0's TFX field.
void gr_ngps_tcache_set(int bitmap, u32 tfx) {
    int handle;
    VramBlock* block;
    u64* p;

    handle = bm_get_frame(bitmap);
    block = bm_bitmaps[handle].vram;
    if (block == NULL) {
        Gs_packet.flush();
        gr_ngps_upload_init();
        block = gr_ngps_upload_bitmap(handle);
        gr_ngps_texflush();
    }
    p = Gs_packet.cur;
    Gs_packet.cur[0] = 0x1000000000000003; // NLOOP 3, NREG 1
    Gs_packet.cur[1] = 0xE;                // A+D
    Gs_packet.cur += 8;
    gs_ad(p + 2, GS_TEX0_1, block->tex0 | ((u64)tfx << 35));
    gs_ad(p + 4, GS_TEX1_1, block->tex1);
    gs_ad(p + 6, GS_MIPTBP1_1, block->miptbp1);
}

// Writes the TEX0 (ORed with tex0_flags), TEX1 and MIPTBP1 values of the bitmap's current frame
// to the data halves of three A+D pairs, uploading the bitmap first if needed.
void gr_ngps_get_texture_regs(int bitmap, u64 tex0_flags, u64* regs) {
    int handle;
    VramBlock* block;

    handle = bm_get_frame(bitmap);
    block = bm_bitmaps[handle].vram;
    if (block == NULL) {
        gr_ngps_upload_init();
        block = gr_ngps_upload_bitmap(handle);
    }
    regs[0] = block->tex0 | tex0_flags;
    regs[2] = block->tex1;
    regs[4] = block->miptbp1;
}

// The same, flushing the GS texture cache after an upload.
void gr_ngps_get_texture_regs_flush(int bitmap, u64 tex0_flags, u64* regs) {
    int handle;
    VramBlock* block;

    handle = bm_get_frame(bitmap);
    block = bm_bitmaps[handle].vram;
    if (block == NULL) {
        gr_ngps_upload_init();
        block = gr_ngps_upload_bitmap(handle);
        gr_ngps_texflush();
    }
    regs[0] = block->tex0 | tex0_flags;
    regs[2] = block->tex1;
    regs[4] = block->miptbp1;
}

// Selects a frame buffer as the texture.
void gr_ngps_set_framebuffer_texture(u32 fbp) {
    u64* p;

    p = Gs_packet.cur;
    Gs_packet.cur[0] = 0x1000000000000003; // NLOOP 3, NREG 1
    Gs_packet.cur[1] = 0xE;                // A+D
    Gs_packet.cur += 8;
    // 512 x 512 PSMCT24 at fbp, buffer width 512
    gs_ad(p + 2, GS_TEX0_1, (u64)fbp | 0x264120000);
    gs_ad(p + 4, GS_TEX1_1, 0);
    gs_ad(p + 6, GS_ALPHA_1, 0x44);
}

void gr_ngps_set_front_buffer_texture(void) { gr_ngps_set_framebuffer_texture(gr_ngps_front_buffer()); }

void gr_ngps_set_back_buffer_texture(void) { gr_ngps_set_framebuffer_texture(gr_ngps_back_buffer()); }

// Makes the bitmap the current one and gives it a w x h VRAM run to render into, if it has none.
void gr_ngps_set_render_texture(int bitmap, int w, int h) {
    int handle;
    int format;
    u32 vram;
    u32 tbp;
    u32 cbp;

    handle = bm_get_frame(bitmap);
    gr_screen.current_bitmap = bitmap;
    if (bm_bitmaps[handle].vram == NULL) {
        format = bm_get_format(bitmap);
        vram = gr_ngps_vram_alloc(handle, bm_get_vram_size(format, w, h, 1));
        if (format == BM_FORMAT_8BIT) {
            tbp = vram + 0x800;
            cbp = vram;
        } else {
            tbp = vram;
            cbp = 0;
        }
        gr_ngps_set_vram_regs(handle, w, h, tbp, cbp);
        bm_bitmaps[handle].vram->miptbp1 = 0;
    }
}

// TEX0 PSM of the bitmap's current frame, which must be in VRAM.
int gr_ngps_get_texture_psm(int bitmap) {
    int handle;

    handle = bm_get_frame(bitmap);
    return (bm_bitmaps[handle].vram->tex0 & 0x3F00000) >> 20;
}

// CLUT and texture block addresses of the bitmap's current frame, which must be in VRAM.
void gr_ngps_get_texture_vram(int bitmap, int* cbp, int* tbp) {
    int handle;
    u64 tex0;

    handle = bm_get_frame(bitmap);
    tex0 = bm_bitmaps[handle].vram->tex0;
    *cbp = (tex0 >> 37) & 0x3FFF;
    *tbp = tex0 & 0x3FFF;
}

// Puts the bitmap in fixed VRAM slot `slot`, after the cache. Returns its block address, or 0
// if the slot is past the end.
int gr_ngps_alloc_fixed_texture(int handle, int slot) {
    bitmap_entry* bm;
    u32 vram;
    VramBlock* block;

    bm = &bm_bitmaps[handle];
    vram = Vram_end + (bm->vram_size << 6) * slot;
    if (vram >= Vram_limit) {
        return 0;
    }
    block = &Fixed_blocks[slot];
    bm->vram = block;
    block->owner = handle;
    gr_ngps_set_vram_regs(handle, bm->width, bm->height, vram, 0);
    return block->tex0 & 0x3FFF;
}

// Builds a DMA chain that sends a w x h image to VRAM as 16 x 16 tiles, one transfer per tile. The
// chain is written through the uncached mapping.
void gr_ngps_build_tile_chain(u64* chain, u32 data, int bytes_per_pixel, int w, int h, int bw, int vram,
                              int psm) {
    u32* p;
    int rows;
    int cols;
    int col;
    int row;
    int eop;

    rows = h >> 4;
    cols = w >> 4;
    p = (u32*)((u32)chain | 0x20000000);
    set_dma_tag((u64*)p, 0, 0, 0, 1, 0, 3); // CNT
    set_gif_tag(p + 4, 0xE, 1, 0, 0, 0, 0, 2);
    set_bitbltbuf(p + 8, vram / 64, (bw + 63) / 64 * 64 / 64, psm);
    set_trxreg(p + 12, 16, 16);
    p += 16;
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            eop = (row == rows - 1 && col == cols - 1) ? 1 : 0;
            set_dma_tag((u64*)p, 0, 0, 0, 1, 0, 4); // CNT
            set_gif_tag(p + 4, 0xE, 1, 0, 0, 0, 0, 2);
            set_trxpos(p + 8, 0, col * 16, row * 16);
            set_trxdir(p + 12, 0);
            set_gif_tag(p + 16, 0, 0, 2, 0, 0, eop, (bytes_per_pixel * 256) / 16); // IMAGE
            set_dma_tag((u64*)(p + 20), 0, data, 0, 3, 0, (bytes_per_pixel * 256) / 16); // REF
            data += bytes_per_pixel * 256;
            p += 24;
        }
    }
    set_dma_tag((u64*)p, 0, 0, 0, 7, 0, 0); // END
}

void set_dma_tag(u64* tag, u32 spr, u32 addr, u32 irq, u32 id, u32 pce, u32 qwc) {
    *tag = ((u64)spr << 63) | ((u64)(addr & ~0xF) << 32) | ((u64)irq << 31) | ((u64)id << 28) | ((u64)pce << 26) |
           (u64)qwc;
}

void set_gif_tag(u32* tag, u64 regs, u32 nreg, u32 flg, u32 prim, u32 pre, u32 eop, u32 nloop) {
    tag[0] = (eop << 15) | nloop;
    tag[1] = (pre << 14) | (prim << 15) | (flg << 26) | (nreg << 28);
    tag[2] = regs & 0xFFFFFFFF;
    tag[3] = regs >> 32;
}

// One A+D quadword: data goes to GS register reg. Static and before its four callers, which keep
// their values in the registers it leaves alone.
static void set_gif_ad(u32* p, u32 reg, u64 data) {
    p[0] = data & 0xFFFFFFFF;
    p[1] = data >> 32;
    p[2] = reg;
    p[3] = 0;
}

void set_bitbltbuf(u32* p, u32 dbp, u32 dbw, u32 dpsm) {
    set_gif_ad(p, GS_BITBLTBUF, ((u64)dpsm << 56) | ((u64)dbw << 48) | ((u64)dbp << 32));
}

void set_trxpos(u32* p, u32 dir, u32 dsax, u32 dsay) {
    set_gif_ad(p, GS_TRXPOS, ((u64)dir << 59) | ((u64)dsay << 48) | ((u64)dsax << 32));
}

void set_trxreg(u32* p, u32 rrw, u32 rrh) { set_gif_ad(p, GS_TRXREG, ((u64)rrh << 32) | (u64)rrw); }

void set_trxdir(u32* p, u32 xdir) { set_gif_ad(p, GS_TRXDIR, (u64)xdir); }

void start_gif_dma_chain(void* tag) {
    D2_TADR = (u32)tag;
    D2_QWC = 0;
    D2_CHCR = D_CHCR_SEND_CHAIN;
}

// The 16-bit converters write the GS 16-bit layout: red in bits 0-4, green 5-9, blue 10-14 and
// the alpha bit 15. The 16-bit sources only move their fields: 1555 loses its transparency bit,
// 565 puts all six green bits at 5-10 (the lowest blue bit gets ORed with the top green bit) and
// 4444 puts each channel in the low four bits of its field, always opaque. For the 24-bit and
// 32-bit sources an opaque pixel's alpha is 0xFF shifted up to bit 15; the bits above 15 fall off
// when the pixel is stored.

void convert_1555_to_16(u16* dst, u16* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        u16 c = src[i];
        dst[i] = ((c >> 10) & 0x1F) | (((c >> 5) & 0x1F) << 5) | ((c & 0x1F) << 10);
    }
}

void convert_565_to_16(u16* dst, u16* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        u16 c = src[i];
        dst[i] = ((c >> 11) & 0x1F) | (((c >> 5) & 0x3F) << 5) | ((c & 0x1F) << 10);
    }
}

void convert_4444_to_16(u16* dst, u16* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        u16 c = src[i];
        dst[i] = ((c >> 8) & 0xF) | (((c >> 4) & 0xF) << 5) | ((c & 0xF) << 10) | 0x8000;
    }
}

// Pure green (blue 0, green 255, red 0) is the color key: those pixels come out transparent.
void convert_888_to_16(u16* dst, u8* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        int b = src[0] >> 3;
        int g = src[1] >> 3;
        int r = src[2] >> 3;
        int a = (src[0] == 0 && src[1] == 0xFF && src[2] == 0) ? 0 : 0xFF;
        src += 3;
        dst[i] = r | (g << 5) | (b << 10) | ((a & 0xFFFF) << 15);
    }
}

void convert_8888_to_16(u16* dst, u8* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        int b = src[0] >> 3;
        int g = src[1] >> 3;
        int r = src[2] >> 3;
        int a = src[3] ? 0xFF : 0;
        src += 4;
        dst[i] = r | (g << 5) | (b << 10) | ((a & 0xFFFF) << 15);
    }
}

u16* convert_texture_to_16(void* src, int format, int count, u16* dst) {
    u16* out;

    if (dst == NULL) {
        out = new u16[count];
    } else {
        out = dst;
    }

    switch (format) {
    case BM_SOURCE_1555:
        convert_1555_to_16(out, (u16*)src, count);
        break;
    case BM_SOURCE_565:
        convert_565_to_16(out, (u16*)src, count);
        break;
    case BM_SOURCE_4444:
        convert_4444_to_16(out, (u16*)src, count);
        break;
    case BM_SOURCE_888:
        convert_888_to_16(out, (u8*)src, count);
        break;
    case BM_SOURCE_8888:
        convert_8888_to_16(out, (u8*)src, count);
        break;
    default:
        DEBUGERRORLINE(1446, "No conversion routine for this texture format!\n");
        if (dst == NULL) {
            delete out;
        }
        out = NULL;
        break;
    }
    return out;
}

// The 32-bit converters write the GS 32-bit layout (red in bits 0-7, alpha in 24-31) from the
// 24-bit and 32-bit sources. From the 16-bit sources they keep the source's channel order, so red
// ends up in bits 16-23, and an n-bit channel is scaled by 255 / 2^n: full intensity comes out a
// little under 255. 565 gets alpha 0; 1555 gets alpha 0 when its transparency bit is set and 0xFF
// otherwise.

void convert_1555_to_32(u32* dst, u16* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        u16 c = src[i];
        u8 r = (255.0f / 32.0f) * ((c >> 10) & 0x1F);
        u8 g = (255.0f / 32.0f) * ((c >> 5) & 0x1F);
        u8 b = (255.0f / 32.0f) * (c & 0x1F);
        u8 a = (c & 0x8000) ? 0 : 0xFF;
        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

void convert_565_to_32(u32* dst, u16* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        u16 c = src[i];
        u8 g = (255.0f / 64.0f) * ((c >> 5) & 0x3F);
        u8 r = (255.0f / 32.0f) * ((c >> 11) & 0x1F);
        u8 b = (255.0f / 32.0f) * (c & 0x1F);
        dst[i] = (g << 8) | (r << 16) | b;
    }
}

void convert_4444_to_32(u32* dst, u16* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        u16 c = src[i];
        u8 a = (255.0f / 16.0f) * ((c >> 12) & 0xF);
        u8 r = (255.0f / 16.0f) * ((c >> 8) & 0xF);
        u8 g = (255.0f / 16.0f) * ((c >> 4) & 0xF);
        u8 b = (255.0f / 16.0f) * (c & 0xF);
        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

// Pure green (blue 0, green 255, red 0) is the color key: those pixels come out transparent.
void convert_888_to_32(u32* dst, u8* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        u8 b = src[0];
        u8 g = src[1];
        u8 r = src[2];
        u8 a = (src[0] == 0 && src[1] == 0xFF && src[2] == 0) ? 0 : 0xFF;
        src += 3;
        dst[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
}

void convert_8888_to_32(u32* dst, u8* src, int count) {
    int i;

    for (i = 0; i < count; i++) {
        u8 b = src[0];
        u8 g = src[1];
        u8 r = src[2];
        u8 a = src[3];
        src += 4;
        dst[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
}

u32* convert_texture_to_32(void* src, int format, int count, u32* dst) {
    u32* out;

    if (dst == NULL) {
        out = new u32[count];
    } else {
        out = dst;
    }

    switch (format) {
    case BM_SOURCE_1555:
        convert_1555_to_32(out, (u16*)src, count);
        break;
    case BM_SOURCE_565:
        convert_565_to_32(out, (u16*)src, count);
        break;
    case BM_SOURCE_4444:
        convert_4444_to_32(out, (u16*)src, count);
        break;
    case BM_SOURCE_888:
        convert_888_to_32(out, (u8*)src, count);
        break;
    case BM_SOURCE_8888:
        convert_8888_to_32(out, (u8*)src, count);
        break;
    default:
        DEBUGERRORLINE(1569, "No conversion routine for this texture format!\n");
        if (dst == NULL) {
            delete out;
        }
        out = NULL;
        break;
    }
    return out;
}
