#include "game/debug.h"
#include "game/gr.h"
#include "game/gr_font.h"
#include "game/unit_00187F80.h"
#include <stdio.h>

// Shows the message centered on a dark blue box.
void DebugPrint(char* buffer) {
    int width;
    int height;

    if (gr_is_inited()) {
        gr_get_string_size(&width, &height, buffer, -1, 0);
        gr_reset_clip();
        gr_ngps_set_alpha(0x48);
        gr_set_color(0x20, 0x20, 0x80, 0xFF);
        gr_rect(0, 0, 512, 448, D_002D4674);
        gr_set_color(0xFF, 0xFF, 0xFF, 0xFF);
        gr_string(256 - width / 2, 224 - height / 2, buffer, 0);
        gr_ngps_flip(0);
        gr_ngps_restore_vsync_handler();
    }
}

int DebugError(const char* File, u32 Line, const char* Error) {
    char charBuffer[1052];

    sprintf(charBuffer, "Error: %s\nFile: %s - Line: %d\n\n", Error, File, Line);
    DebugPrint(charBuffer);
    return 0;
}
