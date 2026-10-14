// 2D and 3D vectors: lengths, normalizing, rotation by a matrix3, random directions and small
// geometry helpers.
#include "game/unit_00196B10.h"
#include "game/matrix.h"
#include "game/unit_00190720.h"

#define PI 3.1415927f

// Set up by this file's static initializer.
vector2 Zero_vector2(0.0f, 0.0f);
vector3 Zero_vector3(0.0f, 0.0f, 0.0f);

extern "C" float func_001CC200(float x); // cosf
extern "C" float func_001CC4B8(float x); // sinf
extern "C" float func_001CC618(float x); // acosf

inline float float_abs(float f) {
    if (f < 0.0f) {
        return -f;
    }
    return f;
}

inline bool within_tolerance(float a, float b, const float& tolerance) {
    if (float_abs(a - b) > tolerance) {
        return false;
    }
    return true;
}

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

float vector2::length() const { return sqrt_float(x * x + y * y); }

float vector2::length_squared() const { return x * x + y * y; }

float vector2::normalize() {
    float len = length();
    if (len == 0.0f) {
        x = 1.0f;
        y = 0.0f;
        return 1.0f;
    }
    x /= len;
    y /= len;
    return len;
}

void vector2::negate() {
    x = -x;
    y = -y;
}

float vector3::length_quick() const {
    float b;
    float a;
    float c;
    float bc;
    float t;

    a = x;
    if (a < 0.0f) {
        a = -a;
    }
    b = y;
    if (b < 0.0f) {
        b = -b;
    }
    c = z;
    if (c < 0.0f) {
        c = -c;
    }
    if (a < b) {
        t = a;
        a = b;
        b = t;
    }
    if (b < c) {
        t = b;
        b = c;
        c = t;
        if (a < b) {
            t = a;
            a = b;
            b = t;
        }
    }
    bc = b / 4.0f + c / 8.0f;
    return a + bc + bc / 2.0f;
}

BOOL vector3::is_zero() const { return x == 0.0f && y == 0.0f && z == 0.0f; }

vector3 vector3::normalized() const {
    float inv = 1.0f / length();
    return vector3(x * inv, y * inv, z * inv);
}

vector3 vector3::operator-() const { return vector3(-x, -y, -z); }

vector3 vector3::rotated(const matrix3& m) const {
    return vector3(x * m.rvec.x + y * m.rvec.y + z * m.rvec.z, x * m.uvec.x + y * m.uvec.y + z * m.uvec.z,
                   x * m.fvec.x + y * m.fvec.y + z * m.fvec.z);
}

vector3 vector3::unrotated(const matrix3& m) const {
    return vector3(x * m.rvec.x + y * m.uvec.x + z * m.fvec.x, x * m.rvec.y + y * m.uvec.y + z * m.fvec.y,
                   x * m.rvec.z + y * m.uvec.z + z * m.fvec.z);
}

BOOL vector3::equals(const vector3& v, const float& tolerance) const {
    if (!within_tolerance(x, v.x, tolerance)) {
        return FALSE;
    }
    if (!within_tolerance(y, v.y, tolerance)) {
        return FALSE;
    }
    if (!within_tolerance(z, v.z, tolerance)) {
        return FALSE;
    }
    return TRUE;
}

void vector3::normalize() {
    float inv = 1.0f / length();
    x *= inv;
    y *= inv;
    z *= inv;
}

float vector3::normalize_length() {
    float len = length();
    float inv = 1.0f / len;
    x *= inv;
    y *= inv;
    z *= inv;
    return len;
}

void vector3::normalize_safe() {
    float len = length();
    if (len <= 0.0f) {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return;
    }
    float inv = 1.0f / len;
    x *= inv;
    y *= inv;
    z *= inv;
}

float vector3::normalize_safe_length() {
    float len = length();
    if (len <= 0.0f) {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return 1.0f;
    }
    float inv = 1.0f / len;
    x *= inv;
    y *= inv;
    z *= inv;
    return len;
}

void vector3::rotate(const matrix3& m) {
    float vx = x;
    float vy = y;
    float vz = z;
    x = vx * m.rvec.x + vy * m.rvec.y + vz * m.rvec.z;
    y = vx * m.uvec.x + vy * m.uvec.y + vz * m.uvec.z;
    z = vx * m.fvec.x + vy * m.fvec.y + vz * m.fvec.z;
}

void vector3::unrotate(const matrix3& m) {
    float vx = x;
    float vy = y;
    float vz = z;
    x = vx * m.rvec.x + vy * m.uvec.x + vz * m.fvec.x;
    y = vx * m.rvec.y + vy * m.uvec.y + vz * m.fvec.y;
    z = vx * m.rvec.z + vy * m.uvec.z + vz * m.fvec.z;
}

void random_direction(vector3* direction) {
    float min_z = -1.0f;
    direction->z = random_range(min_z, 1.0f);
    float angle = 2.0f * PI * random_float();
    float radius = sqrt_float(1.0f - direction->z * direction->z);
    direction->x = radius * func_001CC200(angle);
    direction->y = radius * func_001CC4B8(angle);
}

void random_unit_vector(vector3* v) {
    float min_z = -1.0f;
    v->z = random_range(min_z, 1.0f);
    float angle = 2.0f * PI * random_float();
    float radius = sqrt_float(1.0f - v->z * v->z);
    v->x = radius * func_001CC200(angle);
    v->y = radius * func_001CC4B8(angle);
}

void random_direction_in_cone(vector3* direction, float min_cos) {
    direction->z = random_range(min_cos, 1.0f);
    float angle = 2.0f * PI * random_float();
    float radius = sqrt_float(1.0f - direction->z * direction->z);
    direction->x = radius * func_001CC200(angle);
    direction->y = radius * func_001CC4B8(angle);
}

void random_direction_in_cone(vector3* direction, const vector3& axis, float min_cos) {
    matrix3 orient;
    orient.set_from_unit_forward(axis);
    random_direction_in_cone(direction, min_cos);
    direction->unrotate(orient);
}

float distance(const vector3& a, const vector3& b) { return (a - b).length(); }

float distance_squared(const vector3& a, const vector3& b) { return (a - b).length_squared(); }

// VU0 macro mode. Each vector3 is only 4-byte aligned, so it is gathered into a quadword with three
// lwu and pextlw/pcpyld; the cross product comes back through prot3w one component at a time.
asm void triangle_normal(vector3* normal, const vector3* p0, const vector3* p1, const vector3* p2) {
    daddu t0, a0, zero
    lwu at, 0(a1)
    lwu v0, 4(a1)
    lwu v1, 8(a1)
    pextlw a0, v0, at
    pcpyld a0, v1, a0
    qmtc2.ni a0, vf1
    lwu at, 0(a2)
    lwu v0, 4(a2)
    lwu v1, 8(a2)
    pextlw a1, v0, at
    pcpyld a1, v1, a1
    qmtc2.ni a1, vf2
    lwu at, 0(a3)
    lwu v0, 4(a3)
    lwu v1, 8(a3)
    pextlw a2, v0, at
    pcpyld a2, v1, a2
    qmtc2.ni a2, vf3
    vsub.xyz vf4, vf2, vf1
    vsub.xyz vf5, vf3, vf2
    vopmula.xyz ACC, vf4, vf5
    vopmsub.xyz vf6, vf5, vf4
    qmfc2.ni at, vf6
    sw at, 0(t0)
    prot3w at, at
    sw at, 4(t0)
    prot3w at, at
    jr ra
    sw at, 8(t0)
}

float signed_angle_between(const vector3& a, const vector3& b, const vector3& axis) {
    vector3 na = a.normalized();
    vector3 nb = b.normalized();
    vector3 naxis = axis.normalized();
    float cos_angle = na.dot(nb);
    float angle = func_001CC618(clamp(cos_angle, -1.0f, 1.0f));
    if (a.cross(b).dot(naxis) < 0.0f) {
        angle = -angle;
    }
    return angle;
}

void project_onto_plane(vector3* result, const vector3& point, const vector3& normal, const vector3& plane_point) {
    float plane_d = -plane_point.dot(normal);
    float dist = plane_d + point.dot(normal);
    *result = point;
    *result += normal * -dist;
}

void rotate_around_axis(vector3* result, const vector3& point, const vector3& center, const vector3& axis,
                        float angle) {
    matrix3 orient;
    orient.set_from_forward(axis);
    matrix3 rotation(0.0f, angle, 0.0f);
    vector3 offset = point - center;
    offset.rotate(orient);
    offset.rotate(rotation);
    offset.unrotate(orient);
    *result = offset + center;
}

float closest_point_on_segment(const vector2& point, const vector2& start, const vector2& end, vector2* closest) {
    vector2 dir = end - start;
    vector2 offset = point - start;
    float t = offset.dot(dir) / dir.length_squared();
    t = clamp(t, 0.0f, 1.0f);
    *closest = start + dir * t;
    return (*closest - point).length_squared();
}

BOOL segment_hits_circle(const vector2& start, const vector2& end, const vector2& center, float* t, float radius) {
    vector2 to_start = start - center;
    float dist_squared = to_start.length_squared();
    float radius_squared = radius * radius;
    if (dist_squared <= radius_squared) {
        *t = 0.0f;
        return TRUE;
    }
    vector2 dir = end - start;
    float a = dir.length_squared();
    float b = to_start.dot(dir);
    float discriminant = b * b - a * (dist_squared - radius_squared);
    if (discriminant < 0.0f) {
        return FALSE;
    }
    float root = sqrt_float(discriminant);
    float t0 = (-b + root) / a;
    float t1 = (-b - root) / a;
    if (t1 < t0) {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }
    if (t0 >= 0.0f && t0 <= 1.0f) {
        *t = t0;
        return TRUE;
    }
    if (t1 >= 0.0f && t1 <= 1.0f) {
        *t = t1;
        return TRUE;
    }
    return FALSE;
}

void direction_from_angles(vector3* direction, float pitch, float heading) {
    float sin_pitch = func_001CC4B8(pitch);
    float cos_pitch = func_001CC200(pitch);
    float cos_heading = func_001CC200(heading);
    float sin_heading = func_001CC4B8(heading);
    direction->x = sin_heading * cos_pitch;
    direction->y = -sin_pitch;
    direction->z = cos_heading * cos_pitch;
}
