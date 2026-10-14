// Quaternions and 16-bit packed quaternions: conversion from and to rotation matrices, slerp and
// squad.
#include "game/unit_00195310.h"
#include "game/matrix.h"

extern "C" float func_001CC618(float x); // acosf

// Quadword holding the start address of the VU0 micro program that returns the sines of vf1's x, y
// and z in vf7.
extern u32 D_009265E0[4];

#define QUATERNION_EPSILON 1e-6f
#define HALF_PI 1.5707964f

packed_quaternion Identity_packed_quaternion(0, 0, 0, PACKED_QUATERNION_ONE);
quaternion Identity_quaternion(0.0f, 0.0f, 0.0f, 1.0f);

quaternion::quaternion(const matrix43& m) {
    float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];
    float s;
    int i;

    if (trace >= 0.0f) {
        s = sqrt_float(trace + 1.0f);
        w = 0.5f * s;
        s = 0.5f / s;
        x = s * (m.m[2][1] - m.m[1][2]);
        y = s * (m.m[0][2] - m.m[2][0]);
        z = s * (m.m[1][0] - m.m[0][1]);
    } else {
        i = 0;
        if (m.m[1][1] > m.m[0][0]) {
            i = 1;
        }
        if (m.m[2][2] > m.m[i][i]) {
            i = 2;
        }
        switch (i) {
        case 0:
            s = sqrt_float(1.0f + (m.m[0][0] - (m.m[1][1] + m.m[2][2])));
            x = 0.5f * s;
            s = 0.5f / s;
            y = s * (m.m[0][1] + m.m[1][0]);
            z = s * (m.m[2][0] + m.m[0][2]);
            w = s * (m.m[2][1] - m.m[1][2]);
            break;
        case 1:
            s = sqrt_float(1.0f + (m.m[1][1] - (m.m[2][2] + m.m[0][0])));
            y = 0.5f * s;
            s = 0.5f / s;
            z = s * (m.m[1][2] + m.m[2][1]);
            x = s * (m.m[0][1] + m.m[1][0]);
            w = s * (m.m[0][2] - m.m[2][0]);
            break;
        case 2:
            s = sqrt_float(1.0f + (m.m[2][2] - (m.m[0][0] + m.m[1][1])));
            z = 0.5f * s;
            s = 0.5f / s;
            x = s * (m.m[2][0] + m.m[0][2]);
            y = s * (m.m[1][2] + m.m[2][1]);
            w = s * (m.m[1][0] - m.m[0][1]);
            break;
        }
    }
}

void quaternion::get_matrix(matrix43* m) const {
    m->m[0][0] = 1.0f - 2.0f * y * y - 2.0f * z * z;
    m->m[0][1] = 2.0f * x * y - 2.0f * z * w;
    m->m[0][2] = 2.0f * z * x + 2.0f * y * w;
    m->m[1][0] = 2.0f * x * y + 2.0f * z * w;
    m->m[1][1] = 1.0f - 2.0f * x * x - 2.0f * z * z;
    m->m[1][2] = 2.0f * z * y - 2.0f * x * w;
    m->m[2][0] = 2.0f * x * z - 2.0f * y * w;
    m->m[2][1] = 2.0f * z * y + 2.0f * x * w;
    m->m[2][2] = 1.0f - 2.0f * x * x - 2.0f * y * y;
    m->m[3][0] = 0.0f;
    m->m[3][1] = 0.0f;
    m->m[3][2] = 0.0f;
}

void quaternion::get_matrix(matrix3* m) const {
    m->rvec.x = 1.0f - 2.0f * y * y - 2.0f * z * z;
    m->rvec.y = 2.0f * x * y - 2.0f * z * w;
    m->rvec.z = 2.0f * z * x + 2.0f * y * w;
    m->uvec.x = 2.0f * x * y + 2.0f * z * w;
    m->uvec.y = 1.0f - 2.0f * x * x - 2.0f * z * z;
    m->uvec.z = 2.0f * z * y - 2.0f * x * w;
    m->fvec.x = 2.0f * x * z - 2.0f * y * w;
    m->fvec.y = 2.0f * z * y + 2.0f * x * w;
    m->fvec.z = 1.0f - 2.0f * x * x - 2.0f * y * y;
}

void quaternion::make_identity() {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    w = 1.0f;
}

float dot(const quaternion& a, const quaternion& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

float dot(const packed_quaternion& a, const packed_quaternion& b) {
    return (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w) *
           (1.0f / ((float)PACKED_QUATERNION_ONE * (float)PACKED_QUATERNION_ONE));
}

// The sines come from a VU0 micro program: the angles are written to the scratchpad, loaded into vf1,
// and the results are read back from vf7. The stores and the micro program call are kept in order.
void slerp(quaternion* result, const quaternion& a, const quaternion& b, float t) {
    float* sin_out = (float*)0x70003CC0;
    float* sin_in = (float*)0x70003C00;
    float cosom;
    float omega;
    float scale0;
    float scale1;
    u32* program;

    while (t < 0.0f) {
        t += 1.0f;
    }
    while (t > 1.0f) {
        t -= 1.0f;
    }

    // Use whichever of b and -b is closer to a.
    quaternion diff = a - b;
    float diff_length = dot(diff, diff);
    quaternion sum = a + b;
    float sum_length = dot(sum, sum);
    quaternion target;
    if (diff_length < sum_length) {
        target = b;
    } else {
        target = -b;
    }

    cosom = dot(a, target);
    if (1.0f + cosom > QUATERNION_EPSILON) {
        if (1.0f - cosom <= QUATERNION_EPSILON) {
            scale0 = 0.0f;
            scale1 = 1.0f;
        } else {
            if (cosom >= 0.9f) {
                if (cosom >= 0.9999f) {
                    omega = 0.01f;
                } else {
                    omega = -31.5546f + 71.1423f * cosom - 39.5588f * cosom * cosom;
                }
            } else {
                omega = func_001CC618(cosom);
            }
            scale1 = t * omega;
            asm { .set noreorder }
            sin_in[0] = omega;
            sin_in[1] = (1.0f - t) * omega;
            sin_in[2] = scale1;
            asm {
                la t0, D_009265E0
                lq t0, 0(t0)
                ctc2 t0, vi27
                lqc2 vf1, 0(sin_in)
                vnop
                vcallmsr vi27
                qmfc2.i t0, vf7
                sq t0, 0(sin_out)
                .set reorder
            }
            float inv_sinom = 1.0f / sin_out[0];
            scale0 = sin_out[1] * inv_sinom;
            scale1 = sin_out[2] * inv_sinom;
        }
        result->x = scale0 * a.x + scale1 * target.x;
        result->y = scale0 * a.y + scale1 * target.y;
        result->z = scale0 * a.z + scale1 * target.z;
        result->w = scale0 * a.w + scale1 * target.w;
    } else {
        // Nearly opposite: rotate through a quaternion perpendicular to the target.
        asm { .set noreorder }
        program = D_009265E0;
        sin_in[0] = HALF_PI * (1.0f - t);
        sin_in[1] = HALF_PI * t;
        asm {
            lq t0, 0(program)
            ctc2 t0, vi27
            lqc2 vf1, 0(sin_in)
            vnop
            vcallmsr vi27
            qmfc2.i t0, vf7
            sq t0, 0(sin_out)
            .set reorder
        }
        scale0 = sin_out[0];
        scale1 = sin_out[1];
        result->x = scale0 * a.x + scale1 * -target.y;
        result->y = scale0 * a.y + scale1 * target.x;
        result->z = scale0 * a.z + scale1 * -target.w;
        result->w = scale0 * a.w + scale1 * target.z;
    }
    if (result->w == 0.0f) {
        result->w = QUATERNION_EPSILON;
    }
}

void slerp(packed_quaternion* result, const packed_quaternion& a, const packed_quaternion& b, float t) {
    float* sin_out = (float*)0x70003CC0;
    float* sin_in = (float*)0x70003C00;
    float cosom;
    float omega;
    float scale0;
    float scale1;
    u32* program;

    while (t < 0.0f) {
        t += 1.0f;
    }
    while (t > 1.0f) {
        t -= 1.0f;
    }

    packed_quaternion target;
    packed_quaternion diff = a - b;
    float diff_length = dot(diff, diff);
    packed_quaternion sum = a + b;
    float sum_length = dot(sum, sum);
    if (diff_length < sum_length) {
        target = b;
    } else {
        target = -b;
    }

    cosom = dot(a, target);
    if (1.0f + cosom > QUATERNION_EPSILON) {
        if (1.0f - cosom <= QUATERNION_EPSILON) {
            scale0 = 0.0f;
            scale1 = 1.0f;
        } else {
            if (cosom >= 0.9f) {
                if (cosom >= 0.9999f) {
                    omega = 0.01f;
                } else {
                    omega = -31.5546f + 71.1423f * cosom - 39.5588f * cosom * cosom;
                }
            } else {
                omega = func_001CC618(cosom);
            }
            scale1 = t * omega;
            asm { .set noreorder }
            sin_in[0] = omega;
            sin_in[1] = (1.0f - t) * omega;
            sin_in[2] = scale1;
            asm {
                la t0, D_009265E0
                lq t0, 0(t0)
                ctc2 t0, vi27
                lqc2 vf1, 0(sin_in)
                vnop
                vcallmsr vi27
                qmfc2.i t0, vf7
                sq t0, 0(sin_out)
                .set reorder
            }
            float inv_sinom = 1.0f / sin_out[0];
            scale0 = sin_out[1] * inv_sinom;
            scale1 = sin_out[2] * inv_sinom;
        }
        result->x = scale0 * a.x + scale1 * target.x;
        result->y = scale0 * a.y + scale1 * target.y;
        result->z = scale0 * a.z + scale1 * target.z;
        result->w = scale0 * a.w + scale1 * target.w;
    } else {
        asm { .set noreorder }
        program = D_009265E0;
        sin_in[0] = HALF_PI * (1.0f - t);
        sin_in[1] = HALF_PI * t;
        asm {
            lq t0, 0(program)
            ctc2 t0, vi27
            lqc2 vf1, 0(sin_in)
            vnop
            vcallmsr vi27
            qmfc2.i t0, vf7
            sq t0, 0(sin_out)
            .set reorder
        }
        scale0 = sin_out[0];
        scale1 = sin_out[1];
        result->x = scale0 * a.x + scale1 * -target.y;
        result->y = scale0 * a.y + scale1 * target.x;
        result->z = scale0 * a.z + scale1 * -target.w;
        result->w = scale0 * a.w + scale1 * target.z;
    }
    if (result->w == 0) {
        result->w = 1;
    }
}

void squad(quaternion* result, const packed_quaternion& p, const packed_quaternion& a, const packed_quaternion& b,
           const packed_quaternion& q, float t) {
    packed_quaternion pq;
    packed_quaternion ab;
    packed_quaternion r;

    slerp(&pq, p, q, t);
    slerp(&ab, a, b, t);
    slerp(&r, pq, ab, 2.0f * (1.0f - t) * t);
    result->x = (1.0f / PACKED_QUATERNION_ONE) * r.x;
    result->y = (1.0f / PACKED_QUATERNION_ONE) * r.y;
    result->z = (1.0f / PACKED_QUATERNION_ONE) * r.z;
    if (r.w == 0) {
        result->w = QUATERNION_EPSILON;
    } else {
        result->w = (1.0f / PACKED_QUATERNION_ONE) * r.w;
    }
}

void packed_quaternion::set(const quaternion& q) {
    x = (float)PACKED_QUATERNION_ONE * q.x;
    y = (float)PACKED_QUATERNION_ONE * q.y;
    z = (float)PACKED_QUATERNION_ONE * q.z;
    w = (float)PACKED_QUATERNION_ONE * q.w;
}
