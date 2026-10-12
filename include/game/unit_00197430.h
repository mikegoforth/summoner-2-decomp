#ifndef GAME_UNIT_00197430_H
#define GAME_UNIT_00197430_H

#include "types.h"
#include "game/unit_00196B10.h"

// Float max and min with the R5900 FPU's max.s and min.s, each followed by a nop.
inline float float_max(register float a, register float b) {
    register float result;
    asm {
        max.s result, a, b
        nop
    }
    return result;
}

inline float float_min(register float a, register float b) {
    register float result;
    asm {
        min.s result, a, b
        nop
    }
    return result;
}

inline float clamp(float value, float low, float high) { return float_min(float_max(value, low), high); }

// Random unit vector inside the cone around +z whose half-angle has cosine min_cos (z is uniform in
// [min_cos, 1], so directions are spread evenly over the cap).
void random_direction_in_cone(vector3* direction, float min_cos);
// The same, around a unit-length axis.
void random_direction_in_cone(vector3* direction, const vector3& axis, float min_cos);
float distance(const vector3& a, const vector3& b);
float distance_squared(const vector3& a, const vector3& b);
// (p1 - p0) x (p2 - p1), not normalized.
void triangle_normal(vector3* normal, const vector3* p0, const vector3* p1, const vector3* p2);
// Angle between a and b in radians, negative when a x b points away from axis.
float signed_angle_between(const vector3& a, const vector3& b, const vector3& axis);
// Closest point to point on the plane through plane_point with unit normal.
void project_onto_plane(vector3* result, const vector3& point, const vector3& normal, const vector3& plane_point);
// Rotates point by angle radians about the line through center along axis.
void rotate_around_axis(vector3* result, const vector3& point, const vector3& center, const vector3& axis, float angle);
// Stores the point of segment start-end closest to point and returns its squared distance.
float closest_point_on_segment(const vector2& point, const vector2& start, const vector2& end, vector2* closest);
// TRUE if segment start-end touches the circle. *t is where it enters, from 0 (start) to 1 (end).
BOOL segment_hits_circle(const vector2& start, const vector2& end, const vector2& center, float* t, float radius);
// Unit vector for a pitch and heading in radians: (0, 0) is +z, heading turns toward +x and positive
// pitch tilts toward -y.
void direction_from_angles(vector3* direction, float pitch, float heading);

#endif // GAME_UNIT_00197430_H
