#include "game/stringpool.h"
#include "game/platform_ngps.h"
#include "macros.h"
#include <string.h>

// TODO: name these once their units are split.
// Parser: reads the next string between `open` and `close` into `buffer` (parse.cpp).
extern "C" void func_0019EE90(void* parser, char* buffer, int buffer_size, char open, char close);

void StringPool::Init(char* _pool, int _max_length, bool _allocated) {
    allocated = _allocated;
    max_length = _max_length;
    length = 0;
    pool = _pool;
}

void StringPool::Reset() {
    length = 0;
    count = 0;
}

void StringPool::Release() {
    if (pool != NULL) {
        if (allocated) {
            delete pool;
        }
        pool = NULL;
        allocated = false;
        length = 0;
        max_length = 0;
        count = 0;
    }
}

char* StringPool::Add(const char* string) {
    int string_length;
    char* destination;

    if (string == NULL) {
        return NULL;
    }
    string_length = strlen(string);
    destination = pool + length;
    if (length + string_length + 1 > max_length) {
        DEBUGERRORLINE(152, "Length of string pool exceeded.");
    }
    strcpy(destination, string);
    length += string_length + 1;
    count++;
    return destination;
}

char* StringPool::AddUnique(const char* string) {
    int index;
    char* entry;

    if (string == NULL) {
        return NULL;
    }
    entry = pool;
    for (index = 0; index < count; index++) {
        if (ngps_stricmp(entry, string) == 0) {
            break;
        }
        entry += strlen(entry) + 1;
    }
    if (index != count) {
        return entry;
    }
    return Add(string);
}

char* StringPool::AddQuoted(void* parser) {
    char buffer[256];

    func_0019EE90(parser, buffer, sizeof(buffer), '"', '"');
    return AddUnique(buffer);
}

StringPool::StringPool() {
    allocated = false;
    length = 0;
    max_length = 0;
    pool = NULL;
    count = 0;
}
