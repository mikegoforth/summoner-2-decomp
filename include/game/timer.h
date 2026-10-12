#ifndef GAME_TIMER_H
#define GAME_TIMER_H

#include "types.h"

// EE timer 0 counts BUSCLK/256 = 576000 ticks per second (576 per millisecond); Timer_ticks
// accumulates 0xFFFF per compare interrupt.

int timer_interrupt_handler(int cause);
void timer_initialize(void);
int timer_get(int frequency);
u64 timer_get_ticks(void);
float timer_get_elapsed(s64 start_ticks, s64 end_ticks, int frequency);
void timer_wait(int milliseconds);

extern bool Timer_initialized;
extern volatile u64 Timer_ticks;

#endif // GAME_TIMER_H
