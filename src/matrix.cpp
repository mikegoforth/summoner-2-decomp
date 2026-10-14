// 3x3 rotation matrices and 4x3 transforms.
#include "game/matrix.h"
#include "game/unit_00190720.h"
#include "macros.h"
#include <string.h>

// C library math.
extern "C" float func_001CC4B8(float x); // sinf
extern "C" float func_001CC200(float x); // cosf
extern "C" float func_001CC618(float x); // acosf

matrix3 identity_matrix(vector3(1.0f, 0.0f, 0.0f), vector3(0.0f, 1.0f, 0.0f), vector3(0.0f, 0.0f, 1.0f));
matrix43 identity_matrix43(vector3(1.0f, 0.0f, 0.0f), vector3(0.0f, 1.0f, 0.0f), vector3(0.0f, 0.0f, 1.0f),
                           vector3(0.0f, 0.0f, 0.0f));

// The implied fourth column of a matrix43.
static float Homogeneous_column[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

matrix3::matrix3(const matrix3& m) {
    memcpy(this, &m, sizeof(matrix3));
}

matrix3::matrix3(const vector3& r, const vector3& u, const vector3& f) : rvec(r), uvec(u), fvec(f) {
}

matrix3::matrix3(float x, float z, float y) {
    set_rotation(x, z, y);
}

matrix3 operator*(const matrix3& a, const matrix3& b) {
    return matrix3(a.rvec * b, a.uvec * b, a.fvec * b);
}

void matrix3::set_rotation_sincos(float sx, float cx, float sz, float cz, float sy, float cy) {
    float szsy = sz * sy;
    float czcy = cz * cy;
    float czsy = cz * sy;
    float szcy = sz * cy;
    rvec.x = czcy + sx * szsy;
    uvec.z = szsy + sx * czcy;
    uvec.x = sx * czsy - szcy;
    rvec.z = sx * szcy - czsy;
    fvec.x = sy * cx;
    rvec.y = sz * cx;
    uvec.y = cz * cx;
    fvec.z = cy * cx;
    fvec.y = -sx;
}

void matrix3::set_rotation(float x, float z, float y) {
    float sx = func_001CC4B8(x);
    float cx = func_001CC200(x);
    float sz = func_001CC4B8(z);
    float cz = func_001CC200(z);
    float sy = func_001CC4B8(y);
    float cy = func_001CC200(y);
    set_rotation_sincos(sx, cx, sz, cz, sy, cy);
}

// Builds the rotation through the equivalent unit quaternion (x, y, z, w).
void matrix3::set_axis_angle(float angle, const vector3& axis) {
    float half = 0.5f * angle;
    float s = func_001CC4B8(half);
    float x = axis.x * s;
    float y = axis.y * s;
    float z = axis.z * s;
    float w = func_001CC200(half);
    rvec.x = 1.0f - 2.0f * (y * y) - 2.0f * (z * z);
    rvec.y = 2.0f * (x * y) + 2.0f * (w * z);
    rvec.z = 2.0f * (x * z) - 2.0f * (w * y);
    uvec.x = 2.0f * (x * y) - 2.0f * (w * z);
    uvec.y = 1.0f - 2.0f * (x * x) - 2.0f * (z * z);
    uvec.z = 2.0f * (y * z) + 2.0f * (w * x);
    fvec.x = 2.0f * (x * z) + 2.0f * (w * y);
    fvec.y = 2.0f * (y * z) - 2.0f * (w * x);
    fvec.z = 1.0f - 2.0f * (x * x) - 2.0f * (y * y);
}

void matrix3::get_rotation(float& x, float& z, float& y) const {
    if (fvec.x == 0.0f && fvec.z == 0.0f) {
        y = 0.0f;
    } else {
        y = arctan2(fvec.x, fvec.z);
    }
    float s = func_001CC4B8(y);
    float c = func_001CC200(y);
    // cos(x), with the Y rotation removed.
    float cx = fvec.z * c + fvec.x * s;
    if (cx == 0.0f && fvec.y == 0.0f) {
        x = 0.0f;
    } else {
        x = arctan2(-fvec.y, cx);
    }
    if (cx == 0.0f) {
        z = 0.0f;
    } else {
        float sz = rvec.y / cx;
        float cz = uvec.y / cx;
        if (sz == 0.0f && cz == 0.0f) {
            z = 0.0f;
        } else {
            z = arctan2(sz, cz);
        }
    }
}

void matrix3::get_axis_angle(vector3* axis, float* angle) const {
    float cos_angle;
    int i;

    cos_angle = 0.5f * ((rvec.x + uvec.y + fvec.z) - 1.0f);
    if (cos_angle > 0.99999988f) {
        *angle = 0.0f;
        axis->x = 1.0f;
        axis->y = 0.0f;
        axis->z = 0.0f;
    } else if (cos_angle > -0.99999988f) {
        *angle = func_001CC618(cos_angle);
        axis->x = uvec.z - fvec.y;
        axis->y = fvec.x - rvec.z;
        axis->z = rvec.y - uvec.x;
        axis->normalize();
    } else {
        // A half turn: recover the axis from the largest diagonal element.
        *angle = 3.1415927f;
        i = 0;
        if (uvec.y > rvec.x) {
            i = 1;
            if (fvec.z > uvec.y) {
                i = 2;
            }
        } else if (fvec.z > rvec.x) {
            i = 2;
        }
        switch (i) {
        case 0:
            axis->x = sqrt_float(1.0f + rvec.x);
            axis->y = rvec.y / axis->x;
            axis->z = rvec.z / axis->x;
            break;
        case 1:
            axis->y = sqrt_float(1.0f + uvec.y);
            axis->x = uvec.x / axis->y;
            axis->z = uvec.z / axis->y;
            break;
        case 2:
            axis->z = sqrt_float(1.0f + fvec.z);
            axis->x = fvec.x / axis->z;
            axis->y = fvec.y / axis->z;
            break;
        default:
            DEBUGERRORLINE(552, "invalid index - this should never happen");
            break;
        }
        axis->normalize();
    }
}

matrix3 matrix3::transpose() const {
    vector3 r;
    vector3 u;
    vector3 f;

    u.x = rvec.y;
    u.y = uvec.y;
    u.z = fvec.y;
    r.x = rvec.x;
    r.y = uvec.x;
    r.z = fvec.x;
    f.x = rvec.z;
    f.y = uvec.z;
    f.z = fvec.z;
    return matrix3(r, u, f);
}

void matrix3::orthogonalize() {
    matrix3 result;
    float up_length;
    float right_length;
    float zero = 0.0f;

    result.fvec = fvec.normalized();
    up_length = uvec.length();
    right_length = rvec.length();
    if (up_length <= 0.0f) {
        if (right_length <= 0.0f) {
            if (result.fvec.x == zero && result.fvec.z == zero && result.fvec.y != zero) {
                result.uvec = vector3(0.0f, 0.0f, 1.0f);
            } else {
                result.uvec = vector3(0.0f, 1.0f, 0.0f);
            }
        } else {
            result.uvec = result.fvec.cross(rvec);
        }
    } else {
        result.uvec = uvec.normalized();
    }
    result.rvec = result.uvec.cross(result.fvec);
    result.uvec = result.fvec.cross(result.rvec);
    *this = result;
}

void matrix3::make_identity() {
    rvec.x = 1.0f;
    rvec.y = 0.0f;
    rvec.z = 0.0f;
    uvec.x = 0.0f;
    uvec.y = 1.0f;
    uvec.z = 0.0f;
    fvec.x = 0.0f;
    fvec.y = 0.0f;
    fvec.z = 1.0f;
}

void matrix3::set_from_forward(const vector3& forward) {
    fvec = forward.normalized();
    if (0.0f == fvec.x && 0.0f == fvec.z) {
        rvec.x = 1.0f;
        uvec.z = (fvec.y < 0.0f) ? 1.0f : -1.0f;
        rvec.y = rvec.z = uvec.x = uvec.y = 0.0f;
    } else {
        rvec.x = fvec.z;
        rvec.y = 0.0f;
        rvec.z = -fvec.x;
        rvec.normalize();
        uvec = fvec.cross(rvec);
    }
}

void matrix3::set_from_unit_forward(const vector3& forward) {
    fvec = forward;
    if (0.0f == fvec.x && 0.0f == fvec.z) {
        rvec.x = 1.0f;
        uvec.z = (fvec.y < 0.0f) ? 1.0f : -1.0f;
        rvec.y = rvec.z = uvec.x = uvec.y = 0.0f;
    } else {
        rvec.x = fvec.z;
        rvec.y = 0.0f;
        rvec.z = -fvec.x;
        rvec.normalize();
        uvec = fvec.cross(rvec);
    }
}

void matrix3::set_from_forward_up(const vector3& forward, const vector3& up) {
    fvec = forward.normalized();
    uvec = up;
    if (uvec.normalize_length() == 0.0f) {
        set_from_forward(forward);
        return;
    }
    rvec = uvec.cross(fvec);
    if (rvec.normalize_length() == 0.0f) {
        set_from_forward(forward);
    } else {
        uvec = fvec.cross(rvec);
    }
}

bool matrix3::equals(const matrix3& m, const float& tolerance) const {
    bool result;

    if (!rvec.equals(m.rvec, tolerance)) {
        return false;
    }
    if (!uvec.equals(m.uvec, tolerance)) {
        return false;
    }
    result = fvec.equals(m.fvec, tolerance);
    return result;
}

matrix43::matrix43(const matrix3& rotation) {
    m[0][0] = rotation.rvec.x;
    m[0][1] = rotation.rvec.y;
    m[0][2] = rotation.rvec.z;
    m[1][0] = rotation.uvec.x;
    m[1][1] = rotation.uvec.y;
    m[1][2] = rotation.uvec.z;
    m[2][0] = rotation.fvec.x;
    m[2][1] = rotation.fvec.y;
    m[2][2] = rotation.fvec.z;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
}

matrix43::matrix43(const vector3& r, const vector3& u, const vector3& f, const vector3& position) {
    m[0][0] = r.x;
    m[0][1] = r.y;
    m[0][2] = r.z;
    m[1][0] = u.x;
    m[1][1] = u.y;
    m[1][2] = u.z;
    m[2][0] = f.x;
    m[2][1] = f.y;
    m[2][2] = f.z;
    m[3][0] = position.x;
    m[3][1] = position.y;
    m[3][2] = position.z;
}

matrix43::matrix43(const vector3& position, const matrix3& rotation) {
    m[0][0] = rotation.rvec.x;
    m[0][1] = rotation.rvec.y;
    m[0][2] = rotation.rvec.z;
    m[1][0] = rotation.uvec.x;
    m[1][1] = rotation.uvec.y;
    m[1][2] = rotation.uvec.z;
    m[2][0] = rotation.fvec.x;
    m[2][1] = rotation.fvec.y;
    m[2][2] = rotation.fvec.z;
    m[3][0] = position.x;
    m[3][1] = position.y;
    m[3][2] = position.z;
}

// result = a * b. Hand-written R5900 assembly; result must not overlap a.
asm void multiply(matrix43* result, const matrix43& a, const matrix43& b) {
    .set noreorder
    addiu t1, a0, 0
    paddub a3, zero, zero
    paddub t0, a1, zero
rotation_row:
    lwc1 $f3, 0(t0)
    lwc1 $f2, 0(a2)
    lwc1 $f4, 4(t0)
    lwc1 $f1, 12(a2)
    lwc1 $f5, 8(t0)
    addiu a3, a3, 1
    lwc1 $f0, 24(a2)
    mul.s $f2, $f3, $f2
    slti v1, a3, 3
    addiu t0, t0, 12
    mul.s $f1, $f4, $f1
    adda.s $f2, $f1
    madd.s $f0, $f5, $f0
    swc1 $f0, 0(t1)
    lwc1 $f2, 4(a2)
    lwc1 $f1, 16(a2)
    lwc1 $f0, 28(a2)
    mul.s $f2, $f3, $f2
    mul.s $f1, $f4, $f1
    adda.s $f2, $f1
    madd.s $f0, $f5, $f0
    swc1 $f0, 4(t1)
    lwc1 $f2, 8(a2)
    lwc1 $f1, 20(a2)
    lwc1 $f0, 32(a2)
    mul.s $f2, $f3, $f2
    mul.s $f1, $f4, $f1
    adda.s $f2, $f1
    madd.s $f0, $f5, $f0
    swc1 $f0, 8(t1)
    bnez v1, rotation_row
    addiu t1, t1, 12

    lwc1 $f4, 36(a1)
    lwc1 $f3, 0(a2)
    lwc1 $f5, 40(a1)
    lwc1 $f2, 12(a2)
    lwc1 $f6, 44(a1)
    lwc1 $f1, 24(a2)
    lwc1 $f0, 36(a2)
    mul.s $f3, $f4, $f3
    mul.s $f2, $f5, $f2
    adda.s $f3, $f2
    madd.s $f1, $f6, $f1
    add.s $f0, $f0, $f1
    swc1 $f0, 36(a0)
    lwc1 $f3, 4(a2)
    lwc1 $f2, 16(a2)
    lwc1 $f1, 28(a2)
    lwc1 $f0, 40(a2)
    mul.s $f3, $f4, $f3
    mul.s $f2, $f5, $f2
    adda.s $f3, $f2
    madd.s $f1, $f6, $f1
    add.s $f0, $f0, $f1
    swc1 $f0, 40(a0)
    lwc1 $f3, 8(a2)
    lwc1 $f2, 20(a2)
    lwc1 $f1, 32(a2)
    lwc1 $f0, 44(a2)
    mul.s $f3, $f4, $f3
    mul.s $f2, $f5, $f2
    adda.s $f3, $f2
    madd.s $f1, $f6, $f1
    add.s $f0, $f0, $f1
    jr ra
    swc1 $f0, 44(a0)
}

void transform_point(vector3* result, const matrix43& m, const vector3& v) {
    float x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
    float y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
    float z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];

    result->x = x;
    result->y = y;
    result->z = z;
}

matrix43 matrix43::inverse() const {
    matrix43 result;
    vector3 position;
    int i;
    int j;

    result = matrix43(vector3(1.0f, 0.0f, 0.0f), vector3(0.0f, 1.0f, 0.0f), vector3(0.0f, 0.0f, 1.0f),
                      vector3(0.0f, 0.0f, 0.0f));
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            result.m[i][j] = m[j][i];
        }
    }
    position = -vector3(m[3][0], m[3][1], m[3][2]);
    position = position * result;
    result.m[3][0] = position.x;
    result.m[3][1] = position.y;
    result.m[3][2] = position.z;
    return result;
}

void matrix43::set(const quaternion& q, const vector3& position) {
    q.get_matrix(this);
    m[3][0] = position.x;
    m[3][1] = position.y;
    m[3][2] = position.z;
}

matrix43 interpolate(const matrix43& from, const matrix43& to, float t) {
    matrix43 result;
    quaternion q_from(from);
    quaternion q_to(to);
    quaternion q;

    slerp(&q, q_from, q_to, t);
    q.get_matrix(&result);
    result.m[3][0] = from.m[3][0] * (1.0f - t) + to.m[3][0] * t;
    result.m[3][1] = from.m[3][1] * (1.0f - t) + to.m[3][1] * t;
    result.m[3][2] = from.m[3][2] * (1.0f - t) + to.m[3][2] * t;
    return result;
}

void multiply_4x4(matrix43* result, const matrix43& a, const matrix43& b) {
    matrix43 product;
    int i;

    for (i = 0; i < 4; i++) {
        product.m[i][0] = a.m[i][0] * b.m[0][0] + a.m[i][1] * b.m[1][0] + a.m[i][2] * b.m[2][0] +
                          Homogeneous_column[i] * b.m[3][0];
        product.m[i][1] = a.m[i][0] * b.m[0][1] + a.m[i][1] * b.m[1][1] + a.m[i][2] * b.m[2][1] +
                          Homogeneous_column[i] * b.m[3][1];
        product.m[i][2] = a.m[i][0] * b.m[0][2] + a.m[i][1] * b.m[1][2] + a.m[i][2] * b.m[2][2] +
                          Homogeneous_column[i] * b.m[3][2];
    }
    memcpy(result, &product, sizeof(product));
}

void matrix43::get_rotation(matrix3* rotation) const {
    rotation->rvec.x = m[0][0];
    rotation->rvec.y = m[0][1];
    rotation->rvec.z = m[0][2];
    rotation->uvec.x = m[1][0];
    rotation->uvec.y = m[1][1];
    rotation->uvec.z = m[1][2];
    rotation->fvec.x = m[2][0];
    rotation->fvec.y = m[2][1];
    rotation->fvec.z = m[2][2];
}
