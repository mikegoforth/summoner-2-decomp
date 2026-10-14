// VIF helpers: DMA chains sent to VIF0 and VIF1 (waiting for them to finish), and uploads of
// main memory data to VU1 memory through UNPACK packets built in scratchpad memory.
#include "game/vif.h"

// EE registers.
#define VIF0_STAT (*(volatile u32*)0x10003800)
#define VIF1_STAT (*(volatile u32*)0x10003C00)
#define VIF1_ERR  (*(volatile u32*)0x10003C20)
#define D0_CHCR   (*(volatile u32*)0x10008000)
#define D0_QWC    (*(volatile u32*)0x10008020)
#define D0_TADR   (*(volatile u32*)0x10008030)
#define D1_CHCR   (*(volatile u32*)0x10009000)
#define D1_QWC    (*(volatile u32*)0x10009020)
#define D1_TADR   (*(volatile u32*)0x10009030)
#define D_STAT    (*(volatile u32*)0x1000E010)

#define VIF_ERR_ME0  0x2        // mask the DMAtag mismatch error
#define VIF_STAT_VPS 0x3        // VIF packet status: 0 when idle
#define D_STAT_CIS0  0x1        // writing 1 clears the channel 0 interrupt status
#define D_STAT_CIS1  0x2        // writing 1 clears the channel 1 interrupt status
#define D_CHCR_STR   0x100      // set while the channel is transferring
#define D_TADR_SPR   0x80000000 // the tag address is in scratchpad memory

// Start a chain transfer from memory, sending each tag to the VIF along with its data
// (DIR | MOD chain | TTE | STR).
#define D_CHCR_SEND_CHAIN 0x145

#define DMA_TAG_REFE 0x00000000 // transfer the data at ADDR, then end
#define DMA_TAG_REF  0x30000000 // transfer the data at ADDR, then the next tag

// The UNPACK chain is built at this scratchpad address.
#define VIF_UNPACK_CHAIN ((u32*)0x70002000)

// VIF UNPACK code: write `num` elements to VU memory starting at quadword `vu_addr`.
#define VIF_UNPACK(vu_addr, num, cmd) ((vu_addr) | ((num) << 16) | (((cmd) | 0x60) << 24))

VifUnpackFormat Vif_unpack_formats[VIF_UNPACK_FORMAT_COUNT] = {
    {0x60, 1, 4, 4, 252},  // S-32
    {0x61, 1, 2, 8, 248},  // S-16
    {0x62, 1, 1, 16, 240}, // S-8
    {0x64, 2, 4, 2, 254},  // V2-32
    {0x65, 2, 2, 4, 252},  // V2-16
    {0x66, 2, 1, 8, 248},  // V2-8
    {0x68, 3, 4, 4, 252},  // V3-32
    {0x69, 3, 2, 8, 248},  // V3-16
    {0x6A, 3, 1, 16, 240}, // V3-8
    {0x6C, 4, 4, 1, 255},  // V4-32
    {0x6D, 4, 2, 2, 254},  // V4-16
    {0x6E, 4, 1, 4, 252},  // V4-8
    {0x6F, 4, 2, 2, 254},  // V4-5
};

void func_0019B490(void) { VIF1_ERR |= VIF_ERR_ME0; }

void func_0019B4B0(void* chain) {
    D1_QWC = 0;
    D1_TADR = (u32)chain & 0x0FFFFFFF;
    D_STAT = D_STAT_CIS1;
    D1_CHCR = D_CHCR_SEND_CHAIN;
    while (D1_CHCR & D_CHCR_STR) {
    }
    while (VIF1_STAT & VIF_STAT_VPS) {
    }
}

void func_0019B530(void* chain) {
    D0_QWC = 0;
    D0_TADR = (u32)chain & 0x0FFFFFFF;
    D_STAT = D_STAT_CIS0;
    D0_CHCR = D_CHCR_SEND_CHAIN;
    while (D0_CHCR & D_CHCR_STR) {
    }
    while (VIF0_STAT & VIF_STAT_VPS) {
    }
}

int func_0019B5B0(u32 data, int count, int vu_addr, int format, u32 spr) {
    int total_qwc;
    u32* tag;
    int remaining;
    int done;
    int qwc;
    int num;
    u32 id;

    total_qwc = (count * Vif_unpack_formats[format].component_size + 15) / 16;
    remaining = (count + (Vif_unpack_formats[format].components - 1)) / Vif_unpack_formats[format].components;
    tag = VIF_UNPACK_CHAIN;
    done = 0;
    while (remaining != 0) {
        if (remaining > 255) {
            num = Vif_unpack_formats[format].max_count;
            qwc = Vif_unpack_formats[format].component_size * (num * Vif_unpack_formats[format].components);
            id = DMA_TAG_REF;
            qwc >>= 4;
        } else {
            num = remaining;
            qwc = total_qwc - done;
            id = DMA_TAG_REFE;
        }
        tag[0] = id | qwc;
        remaining -= num;
        tag[1] = spr | (data + done * 16);
        tag[2] = 0;
        tag[3] = VIF_UNPACK(vu_addr, num, Vif_unpack_formats[format].vifcode);
        vu_addr += num;
        tag += 4;
        done += qwc;
    }
    D1_TADR = D_TADR_SPR | (u32)VIF_UNPACK_CHAIN;
    D1_QWC = 0;
    D1_CHCR = D_CHCR_SEND_CHAIN;
    while (D1_CHCR & D_CHCR_STR) {
    }
    while (VIF1_STAT & VIF_STAT_VPS) {
    }
    return remaining;
}
