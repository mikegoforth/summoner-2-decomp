#ifndef GAME_STRINGPOOL_H
#define GAME_STRINGPOOL_H

#include "types.h"

// A block of NUL-terminated strings packed back to back.
class StringPool {
public:
    StringPool();
    StringPool(char* pool, int max_length, bool allocated) { Init(pool, max_length, allocated); }
    ~StringPool() { Release(); }

    void Init(char* pool, int max_length, bool allocated);
    void Reset();
    void Release();
    char* Add(const char* string);
    char* AddUnique(const char* string);
    char* AddQuoted(void* parser);

    /* 0x00 */ char* pool;
    /* 0x04 */ bool allocated;
    /* 0x08 */ int max_length;
    /* 0x0C */ int length;
    /* 0x10 */ int count;
};

#endif // GAME_STRINGPOOL_H
