#include "game/unit_00180B40.h"

f32 Ambient_light_red = 1.0f;
f32 Ambient_light_green = 1.0f;
f32 Ambient_light_blue = 1.0f;

// Only this file's static initializer touches it.
Entry6C D_008928D0[400];

void get_ambient_light(f32* red, f32* green, f32* blue) {
    *red = Ambient_light_red;
    *green = Ambient_light_green;
    *blue = Ambient_light_blue;
}
