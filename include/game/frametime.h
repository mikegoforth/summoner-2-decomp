#ifndef GAME_FRAMETIME_H
#define GAME_FRAMETIME_H

#include "types.h"

extern float actual_frametime;
extern float reported_frametime;
extern float fps;
extern float frametime_divisor;
extern float min_reported_frametime;
extern float max_reported_frametime;
extern int frames_rendered;
extern u32 milliseconds_unpaused;

void frametime_initialize(void);
void FrameDone(void);
void increase_frametime_pause_counter(void);
void decrease_frametime_pause_counter(void);
void increase_animation_pause_counter(void);
void decrease_animation_pause_counter(void);

#endif // GAME_FRAMETIME_H
