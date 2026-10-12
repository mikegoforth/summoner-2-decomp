#ifndef GAME_UNIT_0018F070_H
#define GAME_UNIT_0018F070_H

#include "types.h"
#include "game/timestamp.h"

// Controller input and vibration on top of libpad.

#define PAD_PORTS 2
#define PAD_BUTTONS 16
#define PAD_STICKS 2
#define PAD_DIRECTIONS 4
#define PAD_MAX_ACTUATORS 6

// PadPort::flags
#define PAD_CONNECTED 0x01

// Sticks, as passed to pad_get_stick and friends.
enum {
    PAD_STICK_LEFT = 0,
    PAD_STICK_RIGHT = 1,
};

// Stick directions, as indexes into PadPort::taps and PadPort::repeats.
enum {
    PAD_DIRECTION_UP = 0,
    PAD_DIRECTION_DOWN = 1,
    PAD_DIRECTION_LEFT = 2,
    PAD_DIRECTION_RIGHT = 3,
};

// One per bit of the libpad button word. Times are timer_get(1000) values.
struct PadButton {
    /* 0x00 */ int press_time;   // when the button went down, -1 while it is up
    /* 0x04 */ int triggered;    // set for one frame on the press and on every auto-repeat
    /* 0x08 */ int trigger_time; // when triggered was last set, -1 if never
    /* 0x0C */ int just_pressed; // set for the first frame the button is down
    /* 0x10 */ int repeat_delay; // auto-repeat interval in milliseconds
};

// A stick direction treated as a button.
struct PadDirection {
    /* 0x00 */ int time; // when the stick was pushed (taps) or last repeated (repeats), -1 if idle
    /* 0x04 */ u8 state;
};

// What scePadInfoAct and scePadInfoComb report, plus the vibration state.
struct PadActuators {
    /* 0x000 */ int count;
    /* 0x004 */ int unk_004[16];
    /* 0x044 */ u32 flags[16];
    /* 0x084 */ int combo_count;
    /* 0x088 */ int combos[16][16];
    /* 0x488 */ int combo_sizes[16];
    /* 0x4C8 */ RealtimeTimer timer; // stops the vibration when it runs out
    /* 0x4CC */ int vibrating;
    /* 0x4D0 */ int paused_time;     // time left when the vibration was paused, -1 if not paused
};

// The array is 64-byte aligned and each entry is padded to a multiple of 64 bytes, since the first
// member is the DMA buffer passed to scePadPortOpen.
struct PadPort {
    /* 0x000 */ u8 dma_buffer[0x100];
    /* 0x100 */ u8 flags;
    /* 0x104 */ PadActuators actuators;
    /* 0x5D8 */ PadButton buttons[PAD_BUTTONS];
    /* 0x718 */ u8 analog[4]; // right x, right y, left x, left y; 0x80 is centred
    /* 0x71C */ PadDirection taps[PAD_STICKS][PAD_DIRECTIONS];    // set when the stick is flicked and released
    /* 0x75C */ PadDirection repeats[PAD_STICKS][PAD_DIRECTIONS]; // set on the push, then every direction_repeat ms
    /* 0x79C */ int deadzone[PAD_STICKS];
    /* 0x7A4 */ int direction_repeat;
    /* 0x7A8 */ int tap_time; // longest push that still counts as a tap
    /* 0x7AC */ int mode;     // libpad controller ID: 4 digital, 7 analog, -1 if unsupported
    /* 0x7B0 */ u8 pad_7B0[0x10];
};

extern u8 Pad_insert_prompt;
extern u8 Pad_show_insert_message;
extern u8 Vibration_enabled;
extern int Pad_port_mode;
extern u8 Pad_wait_for_release[PAD_PORTS];
extern PadPort Pads[PAD_PORTS];

int pad_button_triggered(int port, int button);
int pad_button_just_pressed(int port, int button);
void pad_get_stick(int port, int stick, float* x, float* y, int raw);
int pad_stick_centered(int port, int stick);
void pad_set_deadzone(int port, int stick, u8 deadzone);
u8 pad_stick_direction(int port, int stick, int direction);
void pad_set_direction_repeat(int port, int milliseconds);
int pad_get_direction_repeat(int port);
void pad_set_repeat_delay(int port, int button, int milliseconds);
bool pad_is_idle(int port);
void pad_update(void);
void pad_set_vibration(int port, int actuator, int on);
void pad_vibrate(int port, int actuator, int milliseconds);
void pad_stop_vibration(void);
void pad_pause_vibration(int pause);

#endif // GAME_UNIT_0018F070_H
