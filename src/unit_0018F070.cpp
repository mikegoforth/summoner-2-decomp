#include "game/unit_0018F070.h"
#include "game/timer.h"

// libpad
extern "C" int func_001C7F58(int port, int slot, u8* data);             // scePadRead
extern "C" int func_001C7FD8(int port, int slot);                       // scePadGetState
extern "C" int func_001C83B8(int port, int slot, int term, int offset); // scePadInfoMode
extern "C" int func_001C85A8(int port, int slot, const u8* data);       // scePadSetActDirect

// Earlier in this file.
extern "C" void func_0018E8A0(int port); // stops the vibration once its timer runs out
extern "C" void func_0018EA30(int port); // shows the insert-controller message until a pad is found
extern "C" void func_0018EDF0(void);     // resets the button and stick state of both ports

// libpad values
enum {
    PAD_STATE_DISCONNECTED = 0, // scePadStateDiscon
    PAD_STATE_STABLE = 6,       // scePadStateStable
};
#define PAD_INFO_MODE_CURRENT_EX_ID 2 // InfoModeCurExID
#define PAD_ID_DIGITAL 4
#define PAD_ID_ANALOG 7

u8 Pad_insert_prompt = 1;           // ask for a controller when one is unplugged
u8 Pad_show_insert_message = 1;     // draw the message while asking
u8 Vibration_enabled = 1;
int Pad_port_mode = -1;             // 0: port 1 only, 1: both ports, -1 before initialization
u8 Pad_wait_for_release[PAD_PORTS]; // ignore the buttons until all of them are up
PadPort Pads[PAD_PORTS];

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
                func_0018EA30(port);
            } else {
                func_0018EDF0();
            }
            continue;
        }

        func_0018E8A0(port);
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
