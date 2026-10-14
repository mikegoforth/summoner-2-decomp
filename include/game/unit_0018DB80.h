#ifndef GAME_UNIT_0018DB80_H
#define GAME_UNIT_0018DB80_H

#include "types.h"
#include "game/matrix.h"

// A free-flying camera driven by the second controller: the left stick moves forward, back and
// sideways, triangle and cross move up and down, the right stick pitches and turns, circle and
// square roll, and L1 makes everything five times faster while Free_camera_fast_enabled is set.
// The velocities ease toward what the controls ask for with exponential damping.

extern u8 Free_camera_fast_enabled;
extern float Free_camera_move_damping; // time constants, in seconds
extern float Free_camera_turn_damping;
extern u8 Free_camera_initialized;
extern vector3 Free_camera_move_speed; // per axis, at full deflection
extern vector3 Free_camera_turn_speed; // radians per second
extern vector3 Free_camera_velocity;   // world space
extern vector3 Free_camera_spin;       // radians per second about the camera's own X, Y and Z axes
extern vector3 Free_camera_target_velocity;
extern vector3 Free_camera_target_spin;

void free_camera_initialize(float move_speed, float turn_speed, float move_damping, float turn_damping);
// Moves and turns the camera for one frame of frametime seconds.
void free_camera_update(vector3* position, matrix3* orient, float frametime);

#endif // GAME_UNIT_0018DB80_H
