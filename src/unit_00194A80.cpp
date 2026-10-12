#include "game/unit_00194A80.h"

// sqrtf: a single sqrt.s.
extern "C" float func_00191580(float x);

// TODO: name these once their units are split.
// Next value from the precomputed random table, as an unsigned 32-bit integer.
extern "C" u32 func_00191210(void);
// Next value from the precomputed random table, scaled to [0, 1).
extern "C" float func_00191250(void);

static int Noise_p[NOISE_B + NOISE_B + 2];
static float Noise_g3[NOISE_B + NOISE_B + 2][3];
static float Noise_g2[NOISE_B + NOISE_B + 2][2];
static float Noise_g1[NOISE_B + NOISE_B + 2];
static int Noise_start = 1;

float Noise_table[NOISE_TABLE_SIZE];
bool Noise_table_initialized;

#define s_curve(t) (t * t * (3.0f - 2.0f * t))
#define lerp(t, a, b) (a + t * (b - a))

#define setup(value, b0, b1, r0, r1) \
    t = value + NOISE_N;             \
    b0 = ((int)t) & NOISE_BM;        \
    b1 = (b0 + 1) & NOISE_BM;        \
    r0 = t - (int)t;                 \
    r1 = r0 - 1.0f;

void noise_table_init(void) {
    int i;

    for (i = 0; i < NOISE_TABLE_SIZE - 1; i++) {
        Noise_table[i] = func_00191250();
    }
    Noise_table_initialized = true;
    Noise_table[NOISE_TABLE_SIZE - 1] = Noise_table[0];
}

static void normalize2(float v[2]) {
    float s;

    s = func_00191580(v[0] * v[0] + v[1] * v[1]);
    v[0] = v[0] / s;
    v[1] = v[1] / s;
}

static void normalize3(float v[3]) {
    float s;

    s = func_00191580(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] = v[0] / s;
    v[1] = v[1] / s;
    v[2] = v[2] / s;
}

static void noise_init(void) {
    int i;
    int j;
    int k;

    for (i = 0; i < NOISE_B; i++) {
        Noise_p[i] = i;

        Noise_g1[i] = (float)((func_00191210() % (NOISE_B + NOISE_B)) - NOISE_B) / NOISE_B;

        for (j = 0; j < 2; j++) {
            Noise_g2[i][j] = (float)((func_00191210() % (NOISE_B + NOISE_B)) - NOISE_B) / NOISE_B;
        }
        normalize2(Noise_g2[i]);

        for (j = 0; j < 3; j++) {
            Noise_g3[i][j] = (float)((func_00191210() % (NOISE_B + NOISE_B)) - NOISE_B) / NOISE_B;
        }
        normalize3(Noise_g3[i]);
    }

    while (--i) {
        k = Noise_p[i];
        Noise_p[i] = Noise_p[j = func_00191210() % NOISE_B];
        Noise_p[j] = k;
    }

    for (i = 0; i < NOISE_B + 2; i++) {
        Noise_p[NOISE_B + i] = Noise_p[i];
        Noise_g1[NOISE_B + i] = Noise_g1[i];
        for (j = 0; j < 2; j++) {
            Noise_g2[NOISE_B + i][j] = Noise_g2[i][j];
        }
        for (j = 0; j < 3; j++) {
            Noise_g3[NOISE_B + i][j] = Noise_g3[i][j];
        }
    }
}

float noise3(float vec[3]) {
    int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
    float rx0, rx1, ry0, ry1, rz0, rz1, *q, sy, sz, a, b, c, d, t, u, v;
    int i, j;
    float x = vec[0];
    float y = vec[1];
    float z = vec[2];

    if (Noise_start) {
        Noise_start = 0;
        noise_init();
    }

    setup(x, bx0, bx1, rx0, rx1);
    setup(y, by0, by1, ry0, ry1);
    setup(z, bz0, bz1, rz0, rz1);

    i = Noise_p[bx0];
    j = Noise_p[bx1];

    b00 = Noise_p[i + by0];
    b10 = Noise_p[j + by0];
    b01 = Noise_p[i + by1];
    b11 = Noise_p[j + by1];

    t = s_curve(rx0);
    sy = s_curve(ry0);
    sz = s_curve(rz0);

#define at3(rx, ry, rz) (rx * q[0] + ry * q[1] + rz * q[2])

    q = Noise_g3[b00 + bz0];
    u = at3(rx0, ry0, rz0);
    q = Noise_g3[b10 + bz0];
    v = at3(rx1, ry0, rz0);
    a = lerp(t, u, v);

    q = Noise_g3[b01 + bz0];
    u = at3(rx0, ry1, rz0);
    q = Noise_g3[b11 + bz0];
    v = at3(rx1, ry1, rz0);
    b = lerp(t, u, v);

    c = lerp(sy, a, b);

    q = Noise_g3[b00 + bz1];
    u = at3(rx0, ry0, rz1);
    q = Noise_g3[b10 + bz1];
    v = at3(rx1, ry0, rz1);
    a = lerp(t, u, v);

    q = Noise_g3[b01 + bz1];
    u = at3(rx0, ry1, rz1);
    q = Noise_g3[b11 + bz1];
    v = at3(rx1, ry1, rz1);
    b = lerp(t, u, v);

    d = lerp(sy, a, b);

    return lerp(sz, c, d);
}
