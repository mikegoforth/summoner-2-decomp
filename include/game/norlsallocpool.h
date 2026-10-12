#ifndef GAME_NORLSALLOCPOOL_H
#define GAME_NORLSALLOCPOOL_H

#include "types.h"

// Hands out pieces of a block and never frees them one at a time. It works
// either on a buffer it is given (Init) or on a chain of blocks it allocates
// itself (owns_blocks), adding a block when it runs out (can_grow).
class NoRlsAllocPool {
public:
    NoRlsAllocPool();
    ~NoRlsAllocPool();
    void Release();
    int AddBlock();
    void Init(u8* buffer, u32 size);
    void Reset();
    void* Alloc(u32 size);
    void Align();

    /* 0x00 */ u8* blocks; // owned blocks; the first word of each points to the next
    /* 0x04 */ u8* start;
    /* 0x08 */ u8* current;
    /* 0x0C */ u32 block_size;
    /* 0x10 */ u32 remaining;
    /* 0x14 */ void* last_allocation;
    /* 0x18 */ u8 owns_blocks : 1;
    /* 0x18 */ u8 can_grow : 1;
};

#endif // GAME_NORLSALLOCPOOL_H
