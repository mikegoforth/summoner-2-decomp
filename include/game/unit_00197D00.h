#ifndef GAME_UNIT_00197D00_H
#define GAME_UNIT_00197D00_H

#include "types.h"

// Bits packed eight to a byte, lowest bit first: bit n is bit (n & 7) of bits[n >> 3].
class BitArray {
public:
    BOOL IsSet(int bit) const;

    /* 0x00 */ u8* bits;
};

#endif // GAME_UNIT_00197D00_H
