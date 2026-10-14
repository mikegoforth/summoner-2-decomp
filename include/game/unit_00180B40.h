#ifndef GAME_UNIT_00180B40_H
#define GAME_UNIT_00180B40_H

#include "types.h"

// 0x6C-byte record with an empty constructor. This file keeps a table of 400 of them that no
// code reads or writes.
class Entry6C {
  public:
    /* 0x00 */ u8 data[0x6C];

    Entry6C() {}
};

// Per-channel floor for vertex lighting: the lit colour of a vertex is its material colour times
// the larger of the light intensity and this value.
extern f32 Ambient_light_red;
extern f32 Ambient_light_green;
extern f32 Ambient_light_blue;

void get_ambient_light(f32* red, f32* green, f32* blue);

#endif // GAME_UNIT_00180B40_H
