#include "game/unit_00196480.h"

extern "C" float func_001CC300(float x); // floorf

spline::spline(int count, vector3* points) { set(count, points); }

void spline::set(int count, vector3* points) {
    this->count = count;
    this->points = points;
}

void spline::evaluate(float t, vector3* result) const {
    int last = (int)t;

    if (count <= 0) {
        *result = Zero_vector3;
        return;
    }
    if (last >= count) {
        *result = points[count - 1];
        return;
    }

    int i = (int)func_001CC300(t);
    t -= i;
    float t2 = t * t;
    float t3 = t2 * t;
    float b0 = -0.5f * t3 + t2 - 0.5f * t;
    float b1 = 1.5f * t3 + -2.5f * t2 + 1.0f;
    float b2 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
    float b3 = 0.5f * t3 - 0.5f * t2;

    if (count == 2) {
        vector3 before = points[0] + (points[0] - points[1]) * 0.1f;
        vector3 after = points[count - 1] + (points[count - 1] - points[count - 2]) * 0.1f;
        result->x = b0 * before.x + b1 * points[0].x + b2 * points[1].x + b3 * after.x;
        result->y = b0 * before.y + b1 * points[0].y + b2 * points[1].y + b3 * after.y;
        result->z = b0 * before.z + b1 * points[0].z + b2 * points[1].z + b3 * after.z;
    } else if (i == 0) {
        vector3 before = points[0] + (points[0] - points[1]) * 0.1f;
        result->x = b0 * before.x + b1 * points[0].x + b2 * points[1].x + b3 * points[2].x;
        result->y = b0 * before.y + b1 * points[0].y + b2 * points[1].y + b3 * points[2].y;
        result->z = b0 * before.z + b1 * points[0].z + b2 * points[1].z + b3 * points[2].z;
    } else if (i >= count - 2) {
        if (i > count - 2) {
            i = count - 2;
        }
        vector3 after = points[count - 1] + (points[count - 1] - points[count - 2]) * 0.1f;
        result->x = b0 * points[i - 1].x + b1 * points[i].x + b2 * points[i + 1].x + b3 * after.x;
        result->y = b0 * points[i - 1].y + b1 * points[i].y + b2 * points[i + 1].y + b3 * after.y;
        result->z = b0 * points[i - 1].z + b1 * points[i].z + b2 * points[i + 1].z + b3 * after.z;
    } else {
        result->x = b0 * points[i - 1].x + b1 * points[i].x + b2 * points[i + 1].x + b3 * points[i + 2].x;
        result->y = b0 * points[i - 1].y + b1 * points[i].y + b2 * points[i + 1].y + b3 * points[i + 2].y;
        result->z = b0 * points[i - 1].z + b1 * points[i].z + b2 * points[i + 1].z + b3 * points[i + 2].z;
    }
}
