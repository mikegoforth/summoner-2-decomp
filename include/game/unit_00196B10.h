#ifndef GAME_UNIT_00196B10_H
#define GAME_UNIT_00196B10_H

#include "types.h"

class matrix3;

// A single sqrt.s (defined in unit_00190720.cpp).
float sqrt_float(float x);

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

// 3D vector used throughout the game code.
//
// The constructors and arithmetic operators are inline. The compiler does not always expand the
// three-float constructor (for example when an operator's result is passed straight on to another
// operator, as in `a + b * t`), so an out-of-line copy is emitted and called; the linked game keeps
// a single copy of it at 0x1026F0. Constructor arguments are evaluated right to left, which is why
// results come out z first. The empty default constructor is what gives function-local static
// vectors their guard flag.
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
    // Add or subtract the same amount on every axis.
    vector3 operator+(float s) const { return vector3(x + s, y + s, z + s); }
    vector3 operator-(float s) const { return vector3(x - s, y - s, z - s); }
    vector3& operator+=(const vector3& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    // Exact comparison of every component.
    bool operator==(const vector3& v) const {
        if (x == v.x && y == v.y && z == v.z) {
            return true;
        }
        return false;
    }
    float dot(const vector3& v) const { return x * v.x + y * v.y + z * v.z; }
    vector3 cross(const vector3& v) const { return vector3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x); }
    float length_squared() const { return x * x + y * y + z * z; }
    float length() const { return sqrt_float(length_squared()); }

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

#endif // GAME_UNIT_00196B10_H
