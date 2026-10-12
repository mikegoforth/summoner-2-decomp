#include "game/unit_00196B10.h"
#include "game/matrix.h"

#define PI 3.1415927f

// Set up by this file's static initializer.
vector2 Zero_vector2(0.0f, 0.0f);
vector3 Zero_vector3(0.0f, 0.0f, 0.0f);

// Next entry of the random number table as a float in [0, 1).
extern "C" float func_00191250(void);
// min + func_00191250() * (max - min).
extern "C" float func_00191310(float min, float max);
// cosf and sinf.
extern "C" float func_001CC200(float x);
extern "C" float func_001CC4B8(float x);

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

float vector2::length() const { return func_00191580(x * x + y * y); }

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
    direction->z = func_00191310(min_z, 1.0f);
    float angle = 2.0f * PI * func_00191250();
    float radius = func_00191580(1.0f - direction->z * direction->z);
    direction->x = radius * func_001CC200(angle);
    direction->y = radius * func_001CC4B8(angle);
}

void random_unit_vector(vector3* v) {
    float min_z = -1.0f;
    v->z = func_00191310(min_z, 1.0f);
    float angle = 2.0f * PI * func_00191250();
    float radius = func_00191580(1.0f - v->z * v->z);
    v->x = radius * func_001CC200(angle);
    v->y = radius * func_001CC4B8(angle);
}
