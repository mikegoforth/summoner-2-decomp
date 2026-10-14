#ifndef GAME_UNIT_0018E1D0_H
#define GAME_UNIT_0018E1D0_H

#include "types.h"
#include "game/timestamp.h"

// A mouse-style cursor driven by the left analog stick of the first controller. Nothing sets the
// cursor buttons (cursor_reset only clears them), so cursor_button_down always returns 0.

#define CURSOR_BUTTONS 3

struct CursorButton {
    /* 0x0 */ u8 unk_00;
    /* 0x1 */ u8 unk_01;
    /* 0x4 */ int down;
    /* 0x8 */ int unk_08;
    /* 0xC */ RealtimeTimer timer;
};

extern u8 Cursor_initialized;
extern int Cursor_width;
extern int Cursor_height;
extern int Cursor_x;
extern int Cursor_y;
extern int Cursor_z;        // never set
extern int Cursor_flags;    // cleared by cursor_initialize
extern int Cursor_dx;       // movement since the previous cursor_update
extern int Cursor_dy;
extern int Cursor_dz;
extern int Cursor_unused_z; // for callers that do not want the third coordinate
extern CursorButton Cursor_buttons[CURSOR_BUTTONS];
extern CursorButton Cursor_button_timers[CURSOR_BUTTONS]; // only their timers and first bytes are reset

// Called once per frame: moves the cursor to where the stick points and updates the deltas.
void cursor_update(void);
void cursor_initialize(int width, int height);
// Updates the position, then clears the deltas and the button state.
void cursor_reset(void);
// Returns -1, with everything set to 0, before cursor_initialize.
int cursor_get_position(int* x, int* y, int* z = &Cursor_unused_z);
int cursor_button_down(int button);

#endif // GAME_UNIT_0018E1D0_H
