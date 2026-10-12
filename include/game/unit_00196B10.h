#ifndef GAME_UNIT_00196B10_H
#define GAME_UNIT_00196B10_H

#include "types.h"

// sqrtf: a single sqrt.s.
extern "C" float func_00191580(float x);

class matrix3;

class vector2 {
public:
    float x;
    float y;

    vector2() {}
    vector2(float _x, float _y) : x(_x), y(_y) {}

    vector2 operator+(const vector2& v) const { return vector2(x + v.x, y + v.y); }
    vector2 operator-(const vector2& v) const { return vector2(x - v.x, y - v.y); }
    vector2 operator*(float s) const { return vector2(x * s, y * s); }
    float dot(const vector2& v) const { return x * v.x + y * v.y; }

    float length() const;
    float length_squared() const;
    // Returns the length before normalizing. A zero vector becomes (1, 0) and returns 1.
    float normalize();
    void negate();
};

class vector3 {
public:
    float x;
    float y;
    float z;

    vector3() {}
    vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    vector3 operator+(const vector3& v) const { return vector3(x + v.x, y + v.y, z + v.z); }
    vector3 operator-(const vector3& v) const { return vector3(x - v.x, y - v.y, z - v.z); }
    vector3 operator*(float s) const { return vector3(x * s, y * s, z * s); }
    vector3& operator+=(const vector3& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    float dot(const vector3& v) const { return x * v.x + y * v.y + z * v.z; }
    vector3 cross(const vector3& v) const { return vector3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x); }
    float length_squared() const { return x * x + y * y + z * z; }
    float length() const { return func_00191580(length_squared()); }

    // Approximate length: largest component plus 3/8 of the middle and 3/16 of the smallest.
    float length_quick() const;
    BOOL is_zero() const;
    vector3 normalized() const;
    vector3 operator-() const;
    // Dot products with the matrix rows (world to local).
    vector3 rotated(const matrix3& m) const;
    // Sum of the matrix rows scaled by the components (local to world).
    vector3 unrotated(const matrix3& m) const;
    // True if every component differs from v's by no more than tolerance.
    BOOL equals(const vector3& v, const float& tolerance) const;
    void normalize();
    // Returns the length before normalizing.
    float normalize_length();
    // A zero vector becomes (1, 0, 0).
    void normalize_safe();
    // Returns the length before normalizing; a zero vector becomes (1, 0, 0) and returns 1.
    float normalize_safe_length();
    // In place: rotated and unrotated.
    void rotate(const matrix3& m);
    void unrotate(const matrix3& m);
};

extern vector2 Zero_vector2;
extern vector3 Zero_vector3;

// Uniformly distributed point on the unit sphere.
void random_direction(vector3* direction);
void random_unit_vector(vector3* v);

#endif // GAME_UNIT_00196B10_H
