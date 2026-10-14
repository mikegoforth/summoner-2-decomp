#include "game/unit_0018DB80.h"
#include "game/unit_0018F070.h"

extern "C" float func_001CC660(float x); // expf

u8 Free_camera_fast_enabled = 1; // lets L1 speed the camera up

float Free_camera_move_damping;
float Free_camera_turn_damping;
u8 Free_camera_initialized;

vector3 Free_camera_move_speed;
vector3 Free_camera_turn_speed;
vector3 Free_camera_velocity;
vector3 Free_camera_spin;
vector3 Free_camera_target_velocity;
vector3 Free_camera_target_spin;

// Lets a value decay exponentially from current toward target for dt seconds: stores the new value
// in *value and the distance covered (its integral over the step) in *distance. Either pointer may
// be NULL. A time constant under 0.0001 jumps straight to the target.
static void ease(float time_constant, float target, float current, float dt, float* value, float* distance) {
    float decay;
    float difference;

    if (time_constant < 0.0001f) {
        if (distance) {
            *distance = target * dt;
        }
        if (value) {
            *value = target;
        }
    } else {
        difference = current - target;
        decay = func_001CC660(-dt / time_constant);
        if (distance) {
            *distance = time_constant * ((1.0f - decay) * difference) + target * dt;
        }
        if (value) {
            *value = target + difference * decay;
        }
    }
}

void free_camera_initialize(float move_speed, float turn_speed, float move_damping, float turn_damping) {
    Free_camera_initialized = 1;
    Free_camera_move_speed.x = move_speed;
    Free_camera_move_speed.y = move_speed;
    Free_camera_move_speed.z = move_speed;
    Free_camera_turn_speed.x = turn_speed;
    Free_camera_turn_speed.y = turn_speed;
    Free_camera_turn_speed.z = turn_speed;
    Free_camera_velocity.x = 0.0f;
    Free_camera_velocity.y = 0.0f;
    Free_camera_velocity.z = 0.0f;
    Free_camera_move_damping = move_damping;
    Free_camera_turn_damping = turn_damping;
    Free_camera_target_velocity.x = 0.0f;
    Free_camera_target_velocity.y = 0.0f;
    Free_camera_target_velocity.z = 0.0f;
    Free_camera_spin.x = 0.0f;
    Free_camera_spin.y = 0.0f;
    Free_camera_spin.z = 0.0f;
    Free_camera_target_spin.x = 0.0f;
    Free_camera_target_spin.y = 0.0f;
    Free_camera_target_spin.z = 0.0f;
}

void free_camera_update(vector3* position, matrix3* orient, float frametime) {
    float move_x = Free_camera_move_speed.x;
    float move_y = Free_camera_move_speed.y;
    float move_z = Free_camera_move_speed.z;
    float turn_x = Free_camera_turn_speed.x;
    float turn_y = Free_camera_turn_speed.y;
    float turn_z = Free_camera_turn_speed.z;
    float strafe;
    float forward;
    float pitch;
    float yaw;
    int up;
    int down;
    int vertical;
    int circle;
    int square;
    int roll;
    vector3 distance;

    if (Free_camera_fast_enabled && pad_button_down(1, PAD_L1, NULL)) {
        move_x *= 5.0f;
        move_y *= 5.0f;
        move_z *= 5.0f;
        turn_x *= 5.0f;
        turn_y *= 5.0f;
        turn_z *= 5.0f;
    }

    pad_get_stick(1, PAD_STICK_LEFT, &strafe, NULL, 0);
    move_x = strafe * move_x;
    up = pad_button_down(1, PAD_TRIANGLE, NULL);
    down = pad_button_down(1, PAD_CROSS, NULL);
    if (up) {
        vertical = down ? 0 : 1;
    } else {
        vertical = down ? -1 : 0;
    }
    move_y = vertical * move_y;
    pad_get_stick(1, PAD_STICK_LEFT, NULL, &forward, 0);
    move_z = -forward * move_z;

    Free_camera_target_velocity = orient->rvec * move_x;
    Free_camera_target_velocity += orient->uvec * move_y;
    Free_camera_target_velocity += orient->fvec * move_z;
    ease(Free_camera_move_damping, Free_camera_target_velocity.x, Free_camera_velocity.x, frametime,
         &Free_camera_velocity.x, &distance.x);
    ease(Free_camera_move_damping, Free_camera_target_velocity.y, Free_camera_velocity.y, frametime,
         &Free_camera_velocity.y, &distance.y);
    ease(Free_camera_move_damping, Free_camera_target_velocity.z, Free_camera_velocity.z, frametime,
         &Free_camera_velocity.z, &distance.z);
    *position = *position + distance;

    pad_get_stick(1, PAD_STICK_RIGHT, NULL, &pitch, 0);
    Free_camera_target_spin.x = -pitch * turn_x;
    pad_get_stick(1, PAD_STICK_RIGHT, &yaw, NULL, 0);
    Free_camera_target_spin.y = yaw * turn_y;
    circle = pad_button_down(1, PAD_CIRCLE, NULL);
    square = pad_button_down(1, PAD_SQUARE, NULL);
    if (square) {
        roll = circle ? 0 : 3;
    } else {
        roll = circle ? -3 : 0;
    }
    Free_camera_target_spin.z = 0.75f * roll * turn_z;
    ease(Free_camera_turn_damping, Free_camera_target_spin.x, Free_camera_spin.x, frametime, &Free_camera_spin.x,
         NULL);
    ease(Free_camera_turn_damping, Free_camera_target_spin.y, Free_camera_spin.y, frametime, &Free_camera_spin.y,
         NULL);
    ease(Free_camera_turn_damping, Free_camera_target_spin.z, Free_camera_spin.z, frametime, &Free_camera_spin.z,
         NULL);

    matrix3 rotation(Free_camera_spin.x * frametime, Free_camera_spin.z * frametime, Free_camera_spin.y * frametime);
    *orient = rotation * *orient;
    orient->orthogonalize();
}
