#ifndef GAME_UNIT_00194A80_H
#define GAME_UNIT_00194A80_H

#include "types.h"

// Gradient (Perlin) noise. The lattice is 256 cells wide and repeats.
#define NOISE_B 0x100
#define NOISE_BM 0xFF
#define NOISE_N 0x1000

// Number of random values in the periodic one-dimensional noise table.
#define NOISE_TABLE_SIZE 100

// Fills Noise_table with random values in [0, 1); the last entry repeats the first so the
// table wraps.
void noise_table_init(void);
float noise3(float vec[3]);

extern float Noise_table[NOISE_TABLE_SIZE];
extern bool Noise_table_initialized;

#endif // GAME_UNIT_00194A80_H
