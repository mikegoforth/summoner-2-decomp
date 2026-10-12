#include "game/norlsallocpool.h"

NoRlsAllocPool::NoRlsAllocPool() {
    start = NULL;
    owns_blocks = 0;
    can_grow = 0;
    blocks = NULL;
    last_allocation = NULL;
}

NoRlsAllocPool::~NoRlsAllocPool() { Release(); }

void NoRlsAllocPool::Release() {
    if (owns_blocks) {
        u8* block = blocks;
        while (block) {
            u8* next = *(u8**)block;
            delete[] block;
            block = next;
        }
        blocks = NULL;
        start = NULL;
        owns_blocks = 0;
    }
}

int NoRlsAllocPool::AddBlock() {
    if (!owns_blocks) {
        return 1;
    }
    u8* block = new u8[block_size + 8];
    if (!block) {
        return 1;
    }
    *(u8**)block = blocks;
    blocks = block;
    start = blocks + 4;
    Reset();
    return 0;
}

void NoRlsAllocPool::Init(u8* buffer, u32 size) {
    start = buffer;
    block_size = size;
    owns_blocks = 0;
    can_grow = 0;
    Reset();
}

void NoRlsAllocPool::Reset() {
    if (start) {
        current = start;
        Align();
    }
}

void* NoRlsAllocPool::Alloc(u32 size) {
    if (remaining < size) {
        if (!can_grow || AddBlock()) {
            return NULL;
        }
        if (remaining < size) {
            return NULL;
        }
    }
    u8* allocation = current;
    current = allocation + size;
    Align();
    last_allocation = allocation;
    return allocation;
}

void NoRlsAllocPool::Align() {
    u32 misalignment = (u32)current & 3;
    if (misalignment) {
        current += 4 - misalignment;
    }
    int left = block_size - (current - start);
    if (left < 0) {
        left = 0;
    }
    remaining = left;
}
