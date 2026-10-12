#include "game/timer.h"
#include "macros.h"

// Sony libkernel
extern "C" int AddIntcHandler(int cause, int (*handler)(int), int next);
extern "C" int EnableIntc(int cause);

#define INTC_TIM0 9

// EE timer 0 registers.
#define T0_COUNT ((volatile u32*)0x10000000)
#define T0_MODE ((volatile u32*)0x10000010)
#define T0_COMP ((volatile u32*)0x10000020)

bool Timer_initialized;
volatile u64 Timer_ticks;

int timer_interrupt_handler(int cause) {
    *T0_MODE = 0x582;
    Timer_ticks += 0xFFFF;
    return -1;
}

void timer_initialize(void) {
    if (!Timer_initialized) {
        *T0_MODE = 0;
        AddIntcHandler(INTC_TIM0, timer_interrupt_handler, 0);
        EnableIntc(INTC_TIM0);
        *T0_COUNT = 0;
        *T0_COMP = 0xFFFF;
        *T0_MODE = 0x182;
        Timer_initialized = true;
    }
}

// Differs by one delay slot under 3.0.3: the game fills the `frequency == 1` branch's slot with
// the a0 = ticks move from the case body (the same limitation as allocate_in_commonblock).
int timer_get(int frequency) {
    u64 ticks = Timer_ticks + *T0_COUNT;
    u64 time;

    switch (frequency) {
    case 1:
        time = ticks / 576 / 1000;
        break;
    case 1000:
        time = ticks / 576;
        break;
    case 1000000:
        time = ticks * 1000 / 576;
        break;
    default:
        DEBUGERRORLINE(682, "Unknown frequency passed to timer_get.");
    }
    return time;
}

u64 timer_get_ticks(void) { return Timer_ticks; }

float timer_get_elapsed(s64 start_ticks, s64 end_ticks, int frequency) {
    u64 elapsed;
    float time;

    if (start_ticks <= end_ticks) {
        elapsed = end_ticks - start_ticks;
    } else {
        elapsed = end_ticks + (0xFFFFFFFFFFFFFFFF - start_ticks);
    }
    switch (frequency) {
    case 1:
        time = elapsed / 576000.0f;
        break;
    case 1000:
        time = (elapsed * 1000) / 576000.0f;
        break;
    case 1000000:
        time = (elapsed * 1000000) / 576000.0f;
        break;
    default:
        DEBUGERRORLINE(730, "Unknown frequency passed to timer_get.");
    }
    return time;
}

// Differs by one nop under 3.0.3: the game's compiler pads short loops to six instructions
// before the branch, 3.0.3 to five.
void timer_wait(int milliseconds) {
    int now = timer_get(1000);
    int end = now + milliseconds;

    if (now < end) {
        while (now < end) {
            now = timer_get(1000);
        }
    }
}
