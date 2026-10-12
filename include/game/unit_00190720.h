#ifndef GAME_UNIT_00190720_H
#define GAME_UNIT_00190720_H

#include "types.h"
#include "game/unit_00196B10.h"

// Axis-aligned boxes given by their minimum and maximum corners.
void box_from_points(vector3& min, vector3& max, const vector3& a, const vector3& b);
bool boxes_overlap(const vector3& min1, const vector3& max1, const vector3& min2, const vector3& max2);

#endif // GAME_UNIT_00190720_H
