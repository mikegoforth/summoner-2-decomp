#ifndef GAME_DEBUG_H
#define GAME_DEBUG_H

#include "types.h"

int DebugError(const char* File, u32 Line, const char* Error);

extern char string_buffer[];

#endif // GAME_DEBUG_H
