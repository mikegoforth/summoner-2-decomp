#ifndef GAME_MATRIX_H
#define GAME_MATRIX_H

#include "types.h"
#include "game/unit_00195310.h"
#include "game/unit_00196B10.h"

// Rotation matrix stored as three rows: right, up and forward. Vectors are row vectors: a vector
// times the matrix is v.x * rvec + v.y * uvec + v.z * fvec.
class matrix3 {
public:
    vector3 rvec;
    vector3 uvec;
    vector3 fvec;

    matrix3() {}
    matrix3(const matrix3& m);
    matrix3(const vector3& r, const vector3& u, const vector3& f);
    // Rotation from angles in radians; see set_rotation().
    matrix3(float x, float z, float y);

    // Rotation from the sines and cosines of the angles about X, Z and Y.
    void set_rotation_sincos(float sx, float cx, float sz, float cz, float sy, float cy);
    // Rotation by x about the X axis, z about the Z axis and y about the Y axis (radians).
    void set_rotation(float x, float z, float y);
    // Rotation by angle (radians) about axis, which should be unit length.
    void set_axis_angle(float angle, const vector3& axis);
    // Inverse of set_rotation(): recovers the three angles.
    void get_rotation(float& x, float& z, float& y) const;
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

// Row vector times matrix.
inline vector3 operator*(const vector3& v, const matrix3& m) {
    return vector3(v.x * m.rvec.x + v.y * m.uvec.x + v.z * m.fvec.x, v.x * m.rvec.y + v.y * m.uvec.y + v.z * m.fvec.y,
                   v.x * m.rvec.z + v.y * m.uvec.z + v.z * m.fvec.z);
}

matrix3 operator*(const matrix3& a, const matrix3& b);

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
