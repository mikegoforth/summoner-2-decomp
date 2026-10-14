#ifndef GAME_VIF_H
#define GAME_VIF_H

#include "types.h"

// How to UNPACK one element format to VU memory. Indexed by the format argument of
// func_0019B5B0.
struct VifUnpackFormat {
    /* 0x00 */ int vifcode;        // UNPACK command (0x60 | vn << 2 | vl)
    /* 0x04 */ int components;     // components per element (1 to 4)
    /* 0x08 */ int component_size; // bytes per component
    /* 0x0C */ int granularity;    // elements that fill a whole number of quadwords
    /* 0x10 */ int max_count;      // most elements per UNPACK: a multiple of granularity below 256
};

#define VIF_UNPACK_FORMAT_COUNT 13

extern VifUnpackFormat Vif_unpack_formats[VIF_UNPACK_FORMAT_COUNT];

extern "C" {
// Masks the VIF1 DMAtag mismatch error, so DMA tags sent with their transfer can pass through VIF1.
void func_0019B490(void);
// Start a source-chain DMA at the tag `chain` on the VIF1 (or VIF0) channel and wait until the
// channel and the VIF are idle.
void func_0019B4B0(void* chain);
void func_0019B530(void* chain);
// Uploads `count` components of format `format`, read from main memory at `data`, to VU1 memory
// starting at quadword `vu_addr`. The data goes as one DMA chain of REF tags, each carrying a
// VIF UNPACK, built in scratchpad memory. `spr` is ORed into each tag's address. Waits for the
// transfer and returns 0.
int func_0019B5B0(u32 data, int count, int vu_addr, int format, u32 spr);
}

#endif // GAME_VIF_H
