#include "game/unit_0018E1D0.h"
#include "game/unit_0018F070.h"

u8 Cursor_initialized;
int Cursor_width;
int Cursor_height;
int Cursor_x;
int Cursor_y;
int Cursor_z;
int Cursor_flags;
int Cursor_dx;
int Cursor_dy;
int Cursor_dz;
int Cursor_unused_z;
CursorButton Cursor_buttons[CURSOR_BUTTONS];
CursorButton Cursor_button_timers[CURSOR_BUTTONS];

// The stick position maps straight onto a 512x448 screen.
void cursor_update(void) {
    static int last_x = 0;
    static int last_y = 0;
    float x;
    float y;
    int new_x;
    int new_y;

    Cursor_dx = Cursor_dy = Cursor_dz = 0;
    if (Cursor_initialized) {
        pad_get_stick(0, PAD_STICK_LEFT, &x, &y, 0);
        new_x = 512.0f * (0.5f * (1.0f + x));
        new_y = 448.0f * (0.5f * (1.0f + y));
        Cursor_x = new_x;
        Cursor_dx = new_x - last_x;
        last_x = new_x;
        Cursor_y = new_y;
        Cursor_dy = new_y - last_y;
        last_y = new_y;
    }
}

void cursor_initialize(int width, int height) {
    if (!Cursor_initialized) {
        Cursor_initialized = 1;
        Cursor_width = width;
        Cursor_height = height;
        Cursor_x = width / 2;
        Cursor_y = height / 2;
        Cursor_flags = 0;
        cursor_update();
        cursor_reset();
    }
}

void cursor_reset(void) {
    int i;

    if (!Cursor_initialized) {
        return;
    }
    cursor_update();
    Cursor_dx = Cursor_dy = Cursor_dz = 0;
    for (i = 0; i < CURSOR_BUTTONS; i++) {
        Cursor_buttons[i].down = Cursor_buttons[i].unk_08 = Cursor_buttons[i].unk_01 = 0;
        Cursor_button_timers[i].timer.SetTimeout(0);
        Cursor_button_timers[i].unk_00 = 0;
    }
}

int cursor_get_position(int* x, int* y, int* z) {
    if (!Cursor_initialized) {
        *x = *y = *z = 0;
        return -1;
    }
    *x = Cursor_x;
    *y = Cursor_y;
    *z = Cursor_z;
    return 0;
}

int cursor_button_down(int button) {
    if (!Cursor_initialized) {
        return 0;
    }
    return Cursor_buttons[button].down > 0;
}
