#include "game/unit_00190720.h"

// Sorts each coordinate of a and b into min and max.
void box_from_points(vector3& min, vector3& max, const vector3& a, const vector3& b) {
    if (a.x < b.x) {
        min.x = a.x;
        max.x = b.x;
    } else {
        min.x = b.x;
        max.x = a.x;
    }
    if (a.y < b.y) {
        min.y = a.y;
        max.y = b.y;
    } else {
        min.y = b.y;
        max.y = a.y;
    }
    if (a.z < b.z) {
        min.z = a.z;
        max.z = b.z;
    } else {
        min.z = b.z;
        max.z = a.z;
    }
}

// Boxes that only touch count as overlapping.
bool boxes_overlap(const vector3& min1, const vector3& max1, const vector3& min2, const vector3& max2) {
    if (min1.x > max2.x || min1.y > max2.y || min1.z > max2.z || max1.x < min2.x || max1.y < min2.y ||
        max1.z < min2.z) {
        return false;
    }
    return true;
}
