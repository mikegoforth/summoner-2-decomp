#include "game/memory.h"
#include "game/debug.h"
#include "macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int memory1;
int memory2;
int memory3;
int memory4;
int memory5;
int memory6;
int memory7;
int memory8;
bool gHeapAlloc;
Mempool* Pool;
u32 Common_block_index;
u32 Bytes_used;
u8 COMMON_BLOCK[COMMON_BLOCK_SIZE];
s32 Common_block_allocation_size[2] = { -1, -1 };

void* operator new(size_t size, const char* file, int line) {
    void* address;
    char stringbuf[128];

    if (Pool) {
        return allocate_in_mempool(Pool, size);
    }

    address = malloc(size);
    if (address == NULL) {
        sprintf(stringbuf, "Failed to allocate %d bytes\n", size);
        DEBUGERRORLINE(174, stringbuf);
        return NULL;
    }
    return address;
}

void* operator new(size_t size) { return operator new(size, NULL, 0); }

void operator delete(void* p) { free(p); }

void operator delete[](void* p) { ::operator delete(p); }

void Mempool::Copy(u8* _destination, u32 _size, const char* _name, u8 _alignment) {
    strcpy(pool_name, _name);
    destination = _destination;
    size = _size;
    offset = 0;
    alignment = _alignment;
}

void* allocate_in_mempool(Mempool* mempool, u32 size) {
    u8* destination;
    u32 Alinged_size;

    Alinged_size = ~(mempool->alignment - 1) & (size + mempool->alignment - 1);
    if (mempool->offset + Alinged_size > mempool->size) {
        sprintf(string_buffer, "Failed allocation of\n%d bytes in %s pool\n(max pool size is %d\nspace left is %d)\n", size,
                mempool->pool_name, mempool->size, mempool->getPoolSpaceLeft());
        DEBUGERRORLINE(460, string_buffer);
        return NULL;
    }

    destination = mempool->destination + mempool->offset;
    mempool->offset += Alinged_size;
    return destination;
}

void set_current_mempool(Mempool* pool) { Pool = pool; }

Mempool* get_current_mempool(void) { return Pool; }

BOOL is_mempool_active(void) {
    BOOL pool_active;

    if (Pool) {
        pool_active = TRUE;
    } else {
        pool_active = FALSE;
    }
    return pool_active;
}

u32* allocate_in_commonblock(u32 size) {
    u32* Allocation_position;

    size = (size + 15) & ~15;

    if (Common_block_allocation_size[Common_block_index] != -1) {
        DEBUGERRORLINE(535, "Common block is already locked.");
    }
    if (size > COMMON_BLOCK_SIZE - Bytes_used) {
        DEBUGERRORLINE(538, "Not enough space in common block.\n");
    }

    Common_block_allocation_size[Common_block_index++] = size;
    Allocation_position = (u32*)(COMMON_BLOCK + Bytes_used);
    Bytes_used += size;
    return Allocation_position;
}

void deallocate_from_commonblock(u32* p) {
    int size;

    Common_block_index--;
    size = Common_block_allocation_size[Common_block_index];
    Common_block_allocation_size[Common_block_index] = -1;
    Bytes_used -= size;
}
