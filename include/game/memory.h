#ifndef GAME_MEMORY_H
#define GAME_MEMORY_H

#include "types.h"
#include <stddef.h>

#define COMMON_BLOCK_SIZE 0x47000
#define MAX_POOL_NAME_LENGTH 32

class Mempool {
public:
    void Copy(u8* _destination, u32 _size, const char* _name, u8 _alignment);
    void ResetOffset(void) { offset = 0; }
    u32 getPoolSpaceLeft(void) { return size - offset; }

    /* 0x00 */ u8* destination;
    /* 0x04 */ u32 size;
    /* 0x08 */ u32 offset;
    /* 0x0C */ u8 alignment;
    /* 0x0D */ char pool_name[MAX_POOL_NAME_LENGTH];
};

void* operator new(size_t amount, const char* file, int line);
void* allocate_in_mempool(Mempool* mempool, u32 size);
void set_current_mempool(Mempool* pool);
Mempool* get_current_mempool(void);
BOOL is_mempool_active(void);
u32* allocate_in_commonblock(u32 size);
void deallocate_from_commonblock(u32* p);

extern int memory1, memory2, memory3, memory4, memory5, memory6, memory7, memory8;
extern bool gHeapAlloc;

#endif // GAME_MEMORY_H
