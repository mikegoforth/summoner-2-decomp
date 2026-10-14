// Controller input and vibration on top of libpad: opening the ports, choosing the controller mode,
// reading what the vibration motors can do, and the button and stick state read every frame.
#include "game/unit_0018F070.h"
#include "game/gr.h"
#include "game/gr_font.h"
#include "game/gr_ngps_texture.h"
#include "game/ngps_file.h"
#include "game/timer.h"
#include "game/unit_00187F80.h"
#include <stdio.h>
#include <string.h>

extern "C" {
// libpad
int func_001C79A8(int mode);                                   // scePadInit
int func_001C7C08(int port, int slot, void* dma_buffer);       // scePadPortOpen
int func_001C7DF0(int port, int slot);                         // scePadPortClose
int func_001C7F58(int port, int slot, u8* data);               // scePadRead
int func_001C7FD8(int port, int slot);                         // scePadGetState
int func_001C80F0(int port, int slot);                         // scePadGetReqState
int func_001C8178(int port, int slot, int actuator, int term); // scePadInfoAct
int func_001C8298(int port, int slot, int list, int offset);   // scePadInfoComb
int func_001C83B8(int port, int slot, int term, int offset);   // scePadInfoMode
int func_001C84F0(int port, int slot, int offset, int lock);   // scePadSetMainMode
int func_001C85A8(int port, int slot, const u8* data);         // scePadSetActDirect
int func_001C8668(int port, int slot, const u8* data);         // scePadSetActAlign
// Called around the wait for a controller.
u8 func_001D87C0(void);
void func_001D8250(int a, int b); // a is 1 before the wait for a controller and 0 after it
}

// libpad values
#define PAD_REQ_COMPLETE 0            // scePadReqStateComplete
#define PAD_REQ_FAILED 1              // scePadReqStateFaild
#define PAD_STATE_DISCONNECTED 0      // scePadStateDiscon
#define PAD_STATE_STABLE 6            // scePadStateStable
#define PAD_INFO_MODE_CURRENT_EX_ID 2 // InfoModeCurExID
#define PAD_INFO_MODE_ID_TABLE 4      // InfoModeIdTable
#define PAD_INFO_ACT_FUNC 0           // InfoActFunc
#define PAD_INFO_ACT_SUB 1            // InfoActSub
#define PAD_INFO_ACT_SIZE 2           // InfoActSize
#define PAD_ID_NEGCON 2
#define PAD_ID_DIGITAL 4
#define PAD_ID_JOYSTICK 5
#define PAD_ID_ANALOG 7
#define PAD_MODE_LOCK 3 // scePadSetMainMode lock

u8 Pad_insert_prompt = 1;           // ask for a controller when one is unplugged
u8 Pad_show_insert_message = 1;     // draw the message while asking
u8 Vibration_enabled = 1;
int Pad_port_mode = -1;             // 0: port 1 only, 1: both ports, -1 before initialization
u8 Pad_wait_for_release[PAD_PORTS]; // ignore the buttons until all of them are up
PadPort Pads[PAD_PORTS];

// Waits for the next vertical blank.
static void wait_for_vsync(void) {
    u64 count = Gs_vsync_total;
    while (count == Gs_vsync_total) {
    }
}

void pad_wait_for_request(int port) {
    int state;

    for (;;) {
        state = func_001C80F0(port, 0);
        if (state == PAD_REQ_COMPLETE) {
            break;
        }
        if (state == PAD_REQ_FAILED) {
            Pads[port].flags &= ~PAD_CONNECTED;
            break;
        }
        wait_for_vsync();
    }
}

void pad_wait_until_stable(int port) {
    int state;

    for (;;) {
        state = func_001C7FD8(port, 0);
        if (state == PAD_STATE_STABLE) {
            break;
        }
        if (state == PAD_STATE_DISCONNECTED) {
            Pads[port].flags &= ~PAD_CONNECTED;
            break;
        }
        wait_for_vsync();
    }
}

// Picks the analog mode when the controller has one, otherwise the first mode it lists. NeGcon and
// joystick modes are never used.
void pad_select_mode(int port) {
    int i;
    int count;
    int id;
    int best_id = -1;
    int best = -1;

    count = func_001C83B8(port, 0, PAD_INFO_MODE_ID_TABLE, -1);
    for (i = 0; i < count; i++) {
        id = func_001C83B8(port, 0, PAD_INFO_MODE_ID_TABLE, i);
        if (id != PAD_ID_NEGCON && id != PAD_ID_JOYSTICK && (best_id == -1 || id == PAD_ID_ANALOG)) {
            best_id = id;
            best = i;
        }
    }
    if (best < 0) {
        Pads[port].mode = -1;
    } else {
        Pads[port].mode = best_id;
        func_001C84F0(port, 0, best, PAD_MODE_LOCK);
        pad_wait_for_request(port);
    }
}

// Reads what the motors can do and how they may be combined, then assigns them in order.
void pad_read_actuators(PadPort* pad, int port) {
    int i;
    int j;

    pad->actuators.count = func_001C8178(port, 0, -1, PAD_INFO_ACT_FUNC);
    if (pad->actuators.count > 0) {
        for (i = 0; i < pad->actuators.count; i++) {
            pad->actuators.flags[i] = 0;
            if (func_001C8178(port, 0, i, PAD_INFO_ACT_SUB)) {
                pad->actuators.flags[i] |= 1;
            }
            if (func_001C8178(port, 0, i, PAD_INFO_ACT_SIZE) == 2) {
                pad->actuators.flags[i] |= 2;
            }
        }
        pad->actuators.combo_count = func_001C8298(port, 0, -1, 0);
        for (i = 0; i < pad->actuators.combo_count; i++) {
            pad->actuators.combo_sizes[i] = func_001C8298(port, 0, i, -1);
            for (j = 0; j < pad->actuators.combo_sizes[i]; j++) {
                pad->actuators.combos[i][j] = func_001C8298(port, 0, i, j);
            }
        }
        u8 align[PAD_MAX_ACTUATORS] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        for (i = 0; i < pad->actuators.count; i++) {
            align[i] = i;
        }
        func_001C8668(port, 0, align);
        pad_wait_for_request(port);
    }
}

// Stops the motors once the time given to pad_vibrate runs out.
void pad_update_vibration(int port) {
    int count;
    int i;

    if (Pads[port].actuators.timer.valid() && Pads[port].actuators.timer.elapsed()) {
        Pads[port].actuators.timer = RealtimeTimer();
        count = Pads[port].actuators.count;
        for (i = 0; i < count; i++) {
            pad_set_vibration(port, i, 0);
        }
    }
}

// Returns 1 when a controller in a usable mode is found in the port.
int pad_open(int port) {
    func_001C7DF0(port, 0);
    if (func_001C7C08(port, 0, Pads[port].dma_buffer) == 1) {
        pad_wait_until_stable(port);
        pad_select_mode(port);
        if (Pads[port].mode != -1) {
            Pads[port].flags |= PAD_CONNECTED;
            pad_wait_until_stable(port);
            pad_read_actuators(&Pads[port], port);
            return 1;
        }
    }
    return 0;
}

// Blocks until a controller is found in the port, showing a message on the screen meanwhile.
void pad_wait_for_controller(int port) {
    int back;
    int front;
    int height;
    int y;
    char message[64];

    if (func_001D87C0()) {
        func_001D8250(1, 1);
    }
    if (!pad_open(port)) {
        Gs_packet.flush();
        back = gr_ngps_back_buffer();
        front = gr_ngps_front_buffer();
        if (Pad_show_insert_message) {
            gr_ngps_tcache_flush();
            gr_ngps_clear();
            gr_set_color_white();
            // 0xAF is the registered trademark sign in the game's font.
            sprintf(message, "Please insert an analog controller (DUALSHOCK%s2)", "\xAF");
            height = gr_get_font_height(-1);
            y = 224 - height; // two lines around the middle of the 448-line screen
            gr_string(GR_STRING_CENTER, y, message, -1);
            sprintf(message, "into controller port %d.", port + 1);
            gr_string(GR_STRING_CENTER, y + height, message, -1);
            // Show the message at once: copy the buffer it was drawn into to the one on screen.
            gr_ngps_copy_vram(front, back, 0x200, 0x200, 0x200, 0x1C0, 0);
        } else {
            // Keep the picture: copy the buffer on screen to the one drawn into.
            gr_ngps_copy_vram(back, front, 0x200, 0x200, 0x200, 0x1C0, 0);
        }
        while (!pad_open(port)) {
        }
        if (Pad_show_insert_message) {
            gr_ngps_clear();
            gr_ngps_flip(0);
            gr_ngps_clear();
        }
    }
    pad_stop_vibration();
    if (func_001D87C0()) {
        func_001D8250(0, 1);
    }
}

// Opens the ports that Pad_port_mode asks for.
int pad_open_ports(void) {
    switch (Pad_port_mode) {
    case PAD_PORT_MODE_ONE:
        pad_open(0);
        Pad_port_mode = PAD_PORT_MODE_ONE;
        break;
    case PAD_PORT_MODE_BOTH:
        pad_open(0);
        pad_open(1);
        Pad_port_mode = PAD_PORT_MODE_BOTH;
        break;
    default:
        return 0;
    }
    pad_reset();
    return 1;
}

// Loads the controller modules and opens the ports. Returns 0 if libpad fails to start.
int pad_initialize(int port_mode) {
    int i;
    int port;

    load_module("SIO2MAN.IRX");
    load_module("PADMAN.IRX");
    for (port = 0; port < PAD_PORTS; port++) {
        Pads[port].mode = -1;
        Pads[port].flags = 0;
        Pads[port].deadzone[PAD_STICK_LEFT] = 75;
        Pads[port].deadzone[PAD_STICK_RIGHT] = 75;
        Pads[port].direction_repeat = 100;
        Pads[port].tap_time = 300;
        Pads[port].actuators.count = 0;
        Pads[port].actuators.timer = RealtimeTimer();
        Pads[port].actuators.vibrating = 0;
        Pads[port].actuators.paused_time = -1;
        for (i = 0; i < PAD_BUTTONS; i++) {
            Pads[port].buttons[i].repeat_delay = 300;
        }
    }
    Pad_port_mode = port_mode;
    if (func_001C79A8(0) != 1) {
        return 0;
    }
    pad_open_ports();
    for (port = 0; port < PAD_PORTS; port++) {
        pad_set_repeat_delay(port, PAD_UP, 200);
        pad_set_repeat_delay(port, PAD_DOWN, 200);
        pad_set_repeat_delay(port, PAD_LEFT, 200);
        pad_set_repeat_delay(port, PAD_RIGHT, 200);
        pad_set_repeat_delay(port, PAD_CROSS, 400);
    }
    memset(Pad_wait_for_release, 0, sizeof(Pad_wait_for_release));
    return 1;
}

// Releases every button and centres the sticks on both ports. Buttons are then ignored until all of
// them are up.
void pad_reset(void) {
    int port;
    int i;

    for (port = 0; port < PAD_PORTS; port++) {
        for (i = 0; i < PAD_BUTTONS; i++) {
            Pads[port].buttons[i].press_time = -1;
            Pads[port].buttons[i].trigger_time = -1;
            Pads[port].buttons[i].triggered = 0;
            Pads[port].buttons[i].just_pressed = 0;
        }
        for (i = 0; i < 4; i++) {
            Pads[port].analog[i] = 0x80;
        }
        for (i = 0; i < PAD_DIRECTIONS; i++) {
            Pads[port].taps[PAD_STICK_LEFT][i].state = 0;
            Pads[port].taps[PAD_STICK_RIGHT][i].state = 0;
            Pads[port].taps[PAD_STICK_LEFT][i].time = -1;
            Pads[port].taps[PAD_STICK_RIGHT][i].time = -1;
        }
        for (i = 0; i < PAD_DIRECTIONS; i++) {
            Pads[port].repeats[PAD_STICK_LEFT][i].state = 0;
            Pads[port].repeats[PAD_STICK_RIGHT][i].state = 0;
            Pads[port].repeats[PAD_STICK_LEFT][i].time = -1;
            Pads[port].repeats[PAD_STICK_RIGHT][i].time = -1;
        }
        Pad_wait_for_release[port] = 1;
    }
}

int pad_is_connected(int port) {
    return Pads[port].flags & PAD_CONNECTED;
}

int pad_button_down(int port, int button, int* held_time) {
    int press_time;
    int now;

    if (held_time) {
        *held_time = 0;
    }
    if (!(Pads[port].flags & PAD_CONNECTED)) {
        return 0;
    }
    press_time = Pads[port].buttons[button].press_time;
    if (press_time < 0) {
        return 0;
    }
    if (held_time) {
        now = timer_get(1000);
        if (now < press_time) { // the millisecond count wrapped around
            *held_time = -1 - press_time + now;
        } else {
            *held_time = now - press_time;
        }
    }
    return 1;
}

int pad_button_triggered(int port, int button) {
    if (Pads[port].flags & PAD_CONNECTED) {
        return Pads[port].buttons[button].triggered;
    }
    return 0;
}

int pad_button_just_pressed(int port, int button) {
    if (Pads[port].flags & PAD_CONNECTED) {
        return Pads[port].buttons[button].just_pressed;
    }
    return 0;
}

// Each axis comes back in [-1, 1). Unless raw is set, values within the stick's dead zone read as 0.
void pad_get_stick(int port, int stick, float* x, float* y, int raw) {
    int value;

    if (Pads[port].flags & PAD_CONNECTED) {
        switch (stick) {
        case PAD_STICK_LEFT:
            if (x) {
                value = Pads[port].analog[2] - 128;
                if (!raw && __abs(value) <= Pads[port].deadzone[PAD_STICK_LEFT]) {
                    value = 0;
                }
                *x = value / 128.0f;
            }
            if (y) {
                value = Pads[port].analog[3] - 128;
                if (!raw && __abs(value) <= Pads[port].deadzone[PAD_STICK_LEFT]) {
                    value = 0;
                }
                *y = value / 128.0f;
            }
            break;
        case PAD_STICK_RIGHT:
            if (x) {
                value = Pads[port].analog[0] - 128;
                if (!raw && __abs(value) <= Pads[port].deadzone[PAD_STICK_RIGHT]) {
                    value = 0;
                }
                *x = value / 128.0f;
            }
            if (y) {
                value = Pads[port].analog[1] - 128;
                if (!raw && __abs(value) <= Pads[port].deadzone[PAD_STICK_RIGHT]) {
                    value = 0;
                }
                *y = value / 128.0f;
            }
            break;
        }
    }
}

int pad_stick_centered(int port, int stick) {
    int x;
    int y;
    int deadzone;
    bool centered;

    if (!(Pads[port].flags & PAD_CONNECTED)) {
        return false;
    }
    switch (stick) {
    case PAD_STICK_LEFT:
        x = Pads[port].analog[2] - 128;
        y = Pads[port].analog[3] - 128;
        deadzone = Pads[port].deadzone[PAD_STICK_LEFT];
        break;
    case PAD_STICK_RIGHT:
        x = Pads[port].analog[0] - 128;
        y = Pads[port].analog[1] - 128;
        deadzone = Pads[port].deadzone[PAD_STICK_RIGHT];
        break;
    default:
        return false;
    }
    centered = __abs(x) <= deadzone && __abs(y) <= deadzone;
    return centered;
}

void pad_set_deadzone(int port, int stick, u8 deadzone) {
    if (Pads[port].flags & PAD_CONNECTED) {
        switch (stick) {
        case PAD_STICK_LEFT:
            Pads[port].deadzone[PAD_STICK_LEFT] = deadzone;
            break;
        case PAD_STICK_RIGHT:
            Pads[port].deadzone[PAD_STICK_RIGHT] = deadzone;
            break;
        }
    }
}

u8 pad_stick_direction(int port, int stick, int direction) {
    if (Pads[port].flags & PAD_CONNECTED) {
        return Pads[port].repeats[stick][direction].state;
    }
    return 0;
}

void pad_set_direction_repeat(int port, int milliseconds) {
    if (Pads[port].flags & PAD_CONNECTED) {
        Pads[port].direction_repeat = milliseconds;
    }
}

int pad_get_direction_repeat(int port) {
    if (Pads[port].flags & PAD_CONNECTED) {
        return Pads[port].direction_repeat;
    }
    return 0;
}

void pad_set_repeat_delay(int port, int button, int milliseconds) {
    if (Pads[port].flags & PAD_CONNECTED) {
        Pads[port].buttons[button].repeat_delay = milliseconds;
    }
}

// True when no button triggered this frame and both sticks are centred.
bool pad_is_idle(int port) {
    int i;

    for (i = 0; i < PAD_BUTTONS; i++) {
        if (Pads[port].buttons[i].triggered) {
            return false;
        }
    }
    if (!pad_stick_centered(port, PAD_STICK_LEFT)) {
        return false;
    }
    if (!pad_stick_centered(port, PAD_STICK_RIGHT)) {
        return false;
    }
    return true;
}

// A push of at most tap_time milliseconds sets the direction's state when the stick is released.
#define UPDATE_TAP(direction, push)                                                   \
    do {                                                                              \
        if ((push) >= 0.5f) {                                                         \
            if (direction.time < 0) {                                                 \
                direction.time = now;                                                 \
            }                                                                         \
        } else {                                                                      \
            if (direction.time >= 0) {                                                \
                direction.state = __abs(now - direction.time) <= Pads[port].tap_time; \
                if (direction.state) {                                                \
                    direction.time = -1;                                              \
                }                                                                     \
            }                                                                         \
            direction.time = -1;                                                      \
        }                                                                             \
    } while (0)

// Sets the direction's state every direction_repeat milliseconds while the stick is pushed. The
// first branch never runs, since !time is 0 or 1.
#define UPDATE_REPEAT(direction, push)                                                         \
    do {                                                                                       \
        if ((push) >= 0.5f) {                                                                  \
            if (!direction.time < 0) {                                                         \
                direction.time = now;                                                          \
            } else {                                                                           \
                direction.state = __abs(now - direction.time) >= Pads[port].direction_repeat; \
                if (direction.state) {                                                         \
                    direction.state = 1;                                                       \
                    direction.time = now;                                                      \
                }                                                                              \
            }                                                                                  \
        } else {                                                                               \
            direction.state = 0;                                                               \
            direction.time = -1;                                                               \
        }                                                                                      \
    } while (0)

// Called once per frame. The second port is only read when both ports are in use.
void pad_update(void) {
    int port;
    int now;
    int i;
    int state;
    u16 buttons;
    u8* wait;
    float y;
    float x;
    u8 data[32];

    now = timer_get(1000);
    for (port = 0; port < PAD_PORTS; port++) {
        if (port == 1 && Pad_port_mode != 1) {
            continue;
        }
        for (i = 0; i < PAD_DIRECTIONS; i++) {
            Pads[port].taps[PAD_STICK_LEFT][i].state = 0;
            Pads[port].taps[PAD_STICK_RIGHT][i].state = 0;
            Pads[port].repeats[PAD_STICK_LEFT][i].state = 0;
            Pads[port].repeats[PAD_STICK_RIGHT][i].state = 0;
        }
        for (i = 0; i < PAD_BUTTONS; i++) {
            Pads[port].buttons[i].triggered = 0;
        }

        state = func_001C7FD8(port, 0);
        if (state == PAD_STATE_STABLE) {
            if (func_001C83B8(port, 0, PAD_INFO_MODE_CURRENT_EX_ID, -1) != PAD_ID_ANALOG) {
                Pads[port].flags &= ~PAD_CONNECTED;
            }
        } else {
            if (state != PAD_STATE_DISCONNECTED) {
                continue;
            }
            Pads[port].flags &= ~PAD_CONNECTED;
        }

        if (!(Pads[port].flags & PAD_CONNECTED)) {
            if (Pad_insert_prompt) {
                pad_wait_for_controller(port);
            } else {
                pad_reset();
            }
            continue;
        }

        pad_update_vibration(port);
        if (func_001C7F58(port, 0, data) == 0) {
            Pads[port].flags &= ~PAD_CONNECTED;
            continue;
        }
        if (data[0] == 0xFF) {
            continue;
        }

        switch (Pads[port].mode) {
        case PAD_ID_ANALOG:
            for (i = 0; i < 4; i++) {
                Pads[port].analog[i] = 0;
            }
            for (i = 0; i < 4; i++) {
                Pads[port].analog[i] = data[4 + i];
            }

            pad_get_stick(port, PAD_STICK_LEFT, &x, &y, 1);
            UPDATE_TAP(Pads[port].taps[PAD_STICK_LEFT][PAD_DIRECTION_UP], -y);
            UPDATE_TAP(Pads[port].taps[PAD_STICK_LEFT][PAD_DIRECTION_DOWN], y);
            UPDATE_TAP(Pads[port].taps[PAD_STICK_LEFT][PAD_DIRECTION_LEFT], -x);
            UPDATE_TAP(Pads[port].taps[PAD_STICK_LEFT][PAD_DIRECTION_RIGHT], x);
            UPDATE_REPEAT(Pads[port].repeats[PAD_STICK_LEFT][PAD_DIRECTION_UP], -y);
            UPDATE_REPEAT(Pads[port].repeats[PAD_STICK_LEFT][PAD_DIRECTION_DOWN], y);
            UPDATE_REPEAT(Pads[port].repeats[PAD_STICK_LEFT][PAD_DIRECTION_LEFT], -x);
            UPDATE_REPEAT(Pads[port].repeats[PAD_STICK_LEFT][PAD_DIRECTION_RIGHT], x);

            pad_get_stick(port, PAD_STICK_RIGHT, &x, &y, 1);
            UPDATE_TAP(Pads[port].taps[PAD_STICK_RIGHT][PAD_DIRECTION_UP], -y);
            UPDATE_TAP(Pads[port].taps[PAD_STICK_RIGHT][PAD_DIRECTION_DOWN], y);
            UPDATE_TAP(Pads[port].taps[PAD_STICK_RIGHT][PAD_DIRECTION_LEFT], -x);
            UPDATE_TAP(Pads[port].taps[PAD_STICK_RIGHT][PAD_DIRECTION_RIGHT], x);
            UPDATE_REPEAT(Pads[port].repeats[PAD_STICK_RIGHT][PAD_DIRECTION_UP], -y);
            UPDATE_REPEAT(Pads[port].repeats[PAD_STICK_RIGHT][PAD_DIRECTION_DOWN], y);
            UPDATE_REPEAT(Pads[port].repeats[PAD_STICK_RIGHT][PAD_DIRECTION_LEFT], -x);
            UPDATE_REPEAT(Pads[port].repeats[PAD_STICK_RIGHT][PAD_DIRECTION_RIGHT], x);
            // fall through
        case PAD_ID_DIGITAL:
            // The button word is active low.
            wait = &Pad_wait_for_release[port];
            if (*wait) {
                buttons = ((u16*)data)[1];
                for (i = 0; i < PAD_BUTTONS; i++) {
                    if (!(buttons & (1 << i))) {
                        break;
                    }
                }
                if (i >= PAD_BUTTONS) {
                    *wait = 0;
                }
            }
            if (!*wait) {
                buttons = ((u16*)data)[1];
                for (i = 0; i < PAD_BUTTONS; i++) {
                    if (!(buttons & (1 << i))) {
                        if (Pads[port].buttons[i].just_pressed == 1) {
                            Pads[port].buttons[i].just_pressed = 0;
                        } else if (Pads[port].buttons[i].press_time < 0) {
                            Pads[port].buttons[i].press_time = now;
                            Pads[port].buttons[i].just_pressed = 1;
                        }
                    } else {
                        Pads[port].buttons[i].press_time = -1;
                        Pads[port].buttons[i].just_pressed = 0;
                        continue;
                    }
                    if (Pads[port].buttons[i].press_time >= 0 &&
                        (Pads[port].buttons[i].trigger_time < 0 ||
                         __abs(now - Pads[port].buttons[i].trigger_time) >= Pads[port].buttons[i].repeat_delay)) {
                        Pads[port].buttons[i].triggered = 1;
                        Pads[port].buttons[i].trigger_time = now;
                    }
                }
            }
            break;
        }
    }
}

void pad_set_vibration(int port, int actuator, int on) {
    u8 data[PAD_MAX_ACTUATORS] = { 0, 0, 0, 0, 0, 0 };

    if (Vibration_enabled && (Pads[port].flags & PAD_CONNECTED) && actuator < Pads[port].actuators.count) {
        data[actuator] = on ? 0xFF : 0;
        func_001C85A8(port, 0, data);
        Pads[port].actuators.vibrating = on;
        Pads[port].actuators.timer = RealtimeTimer();
    }
}

void pad_vibrate(int port, int actuator, int milliseconds) {
    u8 data[PAD_MAX_ACTUATORS] = { 0, 0, 0, 0, 0, 0 };

    if (Vibration_enabled && (Pads[port].flags & PAD_CONNECTED) && actuator < Pads[port].actuators.count) {
        data[actuator] = 0xFF;
        func_001C85A8(port, 0, data);
        Pads[port].actuators.timer.SetTimeout(milliseconds);
        Pads[port].actuators.vibrating = 1;
    }
}

void pad_stop_vibration(void) {
    int port;
    int actuator;
    int ports;

    ports = Pad_port_mode == 1 ? 2 : 1;
    for (port = 0; port < ports; port++) {
        Pads[port].actuators.timer = RealtimeTimer();
        Pads[port].actuators.paused_time = -1;
        for (actuator = 0; actuator < Pads[port].actuators.count; actuator++) {
            pad_set_vibration(port, actuator, 0);
        }
    }
}

// Pausing stops the motors and keeps the time left; resuming restarts them for that long.
void pad_pause_vibration(int pause) {
    int port;
    PadActuators* actuators;
    int actuator;
    int ports;

    ports = Pad_port_mode == 1 ? 2 : 1;
    for (port = 0; port < ports; port++) {
        actuators = &Pads[port].actuators;
        if (pause && actuators->vibrating) {
            actuators->paused_time = 0;
            if (actuators->timer.valid()) {
                actuators->paused_time = actuators->timer.GetRemainingTime();
            }
            actuators->timer = RealtimeTimer();
            for (actuator = 0; actuator < actuators->count; actuator++) {
                pad_set_vibration(port, actuator, 0);
            }
        } else if (!pause && !actuators->vibrating && actuators->paused_time >= 0) {
            for (actuator = 0; actuator < actuators->count; actuator++) {
                pad_set_vibration(port, actuator, 1);
            }
            if (actuators->paused_time > 0) {
                actuators->timer.SetTimeout(actuators->paused_time);
            }
            actuators->paused_time = -1;
        }
    }
}
