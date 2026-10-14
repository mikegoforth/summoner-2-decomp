#ifndef GAME_UNIT_00196480_H
#define GAME_UNIT_00196480_H

#include "game/unit_00196B10.h"

// Catmull-Rom spline through an array of points. The array is not owned.
class spline {
public:
    /* 0x00 */ vector3* points;
    /* 0x04 */ int count;

    spline(int count, vector3* points);
    void set(int count, vector3* points);
    // Point at t: the integer part selects the segment that starts at that point and the fraction is
    // the position along it. The missing neighbours at the ends are extended by a tenth of the first
    // and last segments. Returns the zero vector with no points and the last point once t reaches it.
    void evaluate(float t, vector3* result) const;
};

#endif // GAME_UNIT_00196480_H
