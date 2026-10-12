#ifndef GAME_MATRIX_H
#define GAME_MATRIX_H

#include "types.h"
#include "game/unit_00196B10.h"

class quaternion {
public:
    float x;
    float y;
    float z;
    float w;

    quaternion() {}
};

// Rotation matrix stored as three rows: right, up and forward.
class matrix3 {
public:
    vector3 rvec;
    vector3 uvec;
    vector3 fvec;

    matrix3() {}
    matrix3(const vector3& r, const vector3& u, const vector3& f);

    void get_axis_angle(vector3* axis, float* angle) const;
    matrix3 transpose() const;
    void orthogonalize();
    void make_identity();
    // Builds an orthonormal basis around forward; set_from_unit_forward expects it normalized already.
    void set_from_forward(const vector3& forward);
    void set_from_unit_forward(const vector3& forward);
    void set_from_forward_up(const vector3& forward, const vector3& up);
    bool equals(const matrix3& m, const float& tolerance) const;
};

// Rotation rows 0-2 and translation row 3 of a 4x4 matrix whose last column is 0, 0, 0, 1.
class matrix43 {
public:
    float m[4][3];

    matrix43() {}
    matrix43(const matrix3& rotation);
    matrix43(const vector3& r, const vector3& u, const vector3& f, const vector3& position);
    matrix43(const vector3& position, const matrix3& rotation);

    matrix43 inverse() const;
    void set(const quaternion& q, const vector3& position);
    void get_rotation(matrix3* rotation) const;
};

void multiply(matrix43* result, const matrix43& a, const matrix43& b);
void transform_point(vector3* result, const matrix43& m, const vector3& v);
matrix43 interpolate(const matrix43& from, const matrix43& to, float t);
void multiply_4x4(matrix43* result, const matrix43& a, const matrix43& b);

inline vector3 operator*(const vector3& v, const matrix43& m) {
    vector3 result;
    transform_point(&result, m, v);
    return result;
}

extern matrix3 identity_matrix;
extern matrix43 identity_matrix43;

#endif // GAME_MATRIX_H
