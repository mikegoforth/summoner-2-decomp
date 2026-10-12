#include "game/unit_00197D00.h"

BOOL BitArray::IsSet(int bit) const { return (bits[bit >> 3] & (1 << (bit & 7))) ? TRUE : FALSE; }
