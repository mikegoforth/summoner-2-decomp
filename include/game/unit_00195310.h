#ifndef GAME_UNIT_00195310_H
#define GAME_UNIT_00195310_H

#include "types.h"

class matrix3;
class matrix43;

class quaternion {
public:
    float x;
    float y;
    float z;
    float w;

    quaternion() {}
    quaternion(float _x, float _y, float _z, float _w) {
        x = _x;
        y = _y;
        z = _z;
        w = _w;
    }
    // Rotation held in the upper 3x3 part of m.
    quaternion(const matrix43& m);

    quaternion operator+(const quaternion& q) const { return quaternion(x + q.x, y + q.y, z + q.z, w + q.w); }
    quaternion operator-(const quaternion& q) const { return quaternion(x - q.x, y - q.y, z - q.z, w - q.w); }
    quaternion operator-() const { return quaternion(-x, -y, -z, -w); }

    // Rotation matrix with a zero translation row.
    void get_matrix(matrix43* m) const;
    void get_matrix(matrix3* m) const;
    void make_identity();
};

// Components of a packed quaternion are fractions of this value.
#define PACKED_QUATERNION_ONE 16383

// Quaternion stored as four 16-bit fixed point components, used for animation keys.
class packed_quaternion {
public:
    s16 x;
    s16 y;
    s16 z;
    s16 w;

    packed_quaternion() {}
    packed_quaternion(s16 _x, s16 _y, s16 _z, s16 _w) {
        x = _x;
        y = _y;
        z = _z;
        w = _w;
    }

    packed_quaternion operator+(const packed_quaternion& q) const {
        return packed_quaternion(x + q.x, y + q.y, z + q.z, w + q.w);
    }
    packed_quaternion operator-(const packed_quaternion& q) const {
        return packed_quaternion(x - q.x, y - q.y, z - q.z, w - q.w);
    }
    packed_quaternion operator-() const { return packed_quaternion(-x, -y, -z, -w); }

    void set(const quaternion& q);
};

float dot(const quaternion& a, const quaternion& b);
// Dot product scaled back to the range of the float version.
float dot(const packed_quaternion& a, const packed_quaternion& b);

// Spherical linear interpolation from a to b along the shorter arc. t is wrapped into [0, 1].
// A zero w in the result is replaced with a tiny positive value.
void slerp(quaternion* result, const quaternion& a, const quaternion& b, float t);
void slerp(packed_quaternion* result, const packed_quaternion& a, const packed_quaternion& b, float t);

// Spherical quadrangle interpolation from p to q with the inner control points a and b.
void squad(quaternion* result, const packed_quaternion& p, const packed_quaternion& a, const packed_quaternion& b,
           const packed_quaternion& q, float t);

extern quaternion Identity_quaternion;
extern packed_quaternion Identity_packed_quaternion;

#endif // GAME_UNIT_00195310_H
