// Bounding boxes, CRC-32, the random number table, table-based sine and cosine, and collision and
// intersection tests. The file has no assert strings, so its name is unknown.
#include "game/unit_00190720.h"
#include "game/matrix.h"

// Real-time clock reading, as filled in by func_001B4038.
struct sceCdCLOCK {
    u8 stat;
    u8 second;
    u8 minute;
    u8 hour;
    u8 pad;
    u8 day;
    u8 month;
    u8 year;
};

extern "C" float func_001CC200(float x);         // cosf
extern "C" float func_001CC4B8(float x);         // sinf
extern "C" int func_001B4038(sceCdCLOCK* clock); // sceCdReadClock
extern "C" float func_001CC300(float x);         // floorf
extern "C" float func_001CBE30(float x);         // atanf

// abs.s through inline assembly, so it stays next to its operand when scheduled.
inline float abs_float(float x) {
    float result;
    asm("abs.s %0, %1" : "=f"(result) : "f"(x));
    return result;
}

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

// Grows the box min..max to cover its sweep along v.
void sweep_box(vector3& min, vector3& max, const vector3& v) {
    if (v.x < 0.0f) {
        min.x += v.x;
    } else {
        max.x += v.x;
    }
    if (v.y < 0.0f) {
        min.y += v.y;
    } else {
        max.y += v.y;
    }
    if (v.z < 0.0f) {
        min.z += v.z;
    } else {
        max.z += v.z;
    }
}

// Corners 0-3 lie on the min.z face and 4-7 on the max.z face.
void get_box_corner(vector3& out, int index, const vector3& min, const vector3& max) {
    float x;
    float y;
    float z;

    switch (index) {
    case 0:
        x = max.x;
        y = max.y;
        z = min.z;
        break;
    case 1:
        x = max.x;
        y = min.y;
        z = min.z;
        break;
    case 2:
        x = min.x;
        y = min.y;
        z = min.z;
        break;
    case 3:
        x = min.x;
        y = max.y;
        z = min.z;
        break;
    case 4:
        x = max.x;
        y = max.y;
        z = max.z;
        break;
    case 5:
        x = max.x;
        y = min.y;
        z = max.z;
        break;
    case 6:
        x = min.x;
        y = min.y;
        z = max.z;
        break;
    case 7:
        x = min.x;
        y = max.y;
        z = max.z;
        break;
    }
    out.x = x;
    out.y = y;
    out.z = z;
}

// Indices (as used by get_box_corner) of the corners that lie least and furthest along dir.
void get_box_corners_along(const vector3& dir, int* min_corner, int* max_corner) {
    if (dir.x > 0.0f) {
        if (dir.y > 0.0f) {
            if (dir.z > 0.0f) {
                *max_corner = 4;
                *min_corner = 2;
            } else {
                *max_corner = 0;
                *min_corner = 6;
            }
        } else {
            if (dir.z > 0.0f) {
                *max_corner = 5;
                *min_corner = 3;
            } else {
                *max_corner = 1;
                *min_corner = 7;
            }
        }
    } else {
        if (dir.y > 0.0f) {
            if (dir.z > 0.0f) {
                *max_corner = 7;
                *min_corner = 1;
            } else {
                *max_corner = 3;
                *min_corner = 5;
            }
        } else {
            if (dir.z > 0.0f) {
                *max_corner = 6;
                *min_corner = 0;
            } else {
                *max_corner = 2;
                *min_corner = 4;
            }
        }
    }
}

// Cosine and sine of 0..2pi in 100 steps; entry 100 repeats entry 0.
#define TABLE_STEPS 100
#define RANDOM_TABLE_SIZE 1240

// Mersenne Twister (MT19937) parameters.
#define MT_N 624
#define MT_M 397
#define MT_MATRIX_A 0x9908B0DF
#define MT_UPPER_MASK 0x80000000
#define MT_LOWER_MASK 0x7FFFFFFF
#define MT_TEMPERING_MASK_B 0x9D2C5680
#define MT_TEMPERING_MASK_C 0xEFC60000

static u32 crc_table[256];
static float cos_table[TABLE_STEPS + 1];
static float sin_table[TABLE_STEPS + 1];
static u32 mt[MT_N];
static int mti = MT_N + 1;
// Precomputed random numbers, handed out in order and refilled by refill_random_table.
u32 random_table[RANDOM_TABLE_SIZE];
static int random_index;

void make_crc_table(void) {
    for (int n = 0; n < 256; n++) {
        u32 c = n;
        for (int k = 8; k > 0; k--) {
            if (c & 1) {
                c = (c >> 1) ^ 0xEDB88320;
            } else {
                c >>= 1;
            }
        }
        crc_table[n] = c;
    }
}

u32 crc32(const u8* buffer, int length, u32 crc) {
    while (length--) {
        crc = ((crc >> 8) & 0x00FFFFFFL) ^ crc_table[(crc ^ *buffer++) & 0xFF];
    }
    return crc;
}

u32 crc32_string(const char* string, u32 crc) {
    for (;;) {
        u8 c = *string++;
        if (c == 0) {
            break;
        }
        crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_table[(crc ^ c) & 0xFF];
    }
    return crc;
}

// The generator is static, so the compiler knows which registers sgenrand and genrand use, and
// init_math_tables and refill_random_table keep values in caller-saved registers across the calls.
static void sgenrand(u32 seed) {
    mt[0] = seed;
    for (mti = 1; mti < MT_N; mti++) {
        mt[mti] = 69069 * mt[mti - 1];
    }
}

static u32 genrand(void) {
    u32 y;
    static u32 mag01[2] = {0x0, MT_MATRIX_A};

    if (mti >= MT_N) {
        int kk;

        if (mti == MT_N + 1) {
            sgenrand(4357);
        }
        for (kk = 0; kk < MT_N - MT_M; kk++) {
            y = (mt[kk] & MT_UPPER_MASK) | (mt[kk + 1] & MT_LOWER_MASK);
            mt[kk] = mt[kk + MT_M] ^ (y >> 1) ^ mag01[y & 0x1];
        }
        for (; kk < MT_N - 1; kk++) {
            y = (mt[kk] & MT_UPPER_MASK) | (mt[kk + 1] & MT_LOWER_MASK);
            mt[kk] = mt[kk + (MT_M - MT_N)] ^ (y >> 1) ^ mag01[y & 0x1];
        }
        y = (mt[MT_N - 1] & MT_UPPER_MASK) | (mt[0] & MT_LOWER_MASK);
        mt[MT_N - 1] = mt[MT_M - 1] ^ (y >> 1) ^ mag01[y & 0x1];
        mti = 0;
    }

    y = mt[mti++];
    y ^= y >> 11;
    y ^= (y << 7) & MT_TEMPERING_MASK_B;
    y ^= (y << 15) & MT_TEMPERING_MASK_C;
    y ^= y >> 18;
    return y;
}

// Fills the cosine and sine tables and seeds the random table from the clock.
void init_math_tables(void) {
    int i;
    sceCdCLOCK clock;
    int hours;

    for (i = 0; i < TABLE_STEPS + 1; i++) {
        float angle = (float)i / 100.0f;
        angle *= 6.2831855f;
        cos_table[i] = func_001CC200(angle);
        sin_table[i] = func_001CC4B8(angle);
    }
    func_001B4038(&clock);
    hours = clock.hour + clock.day * 24;
    sgenrand(clock.second + (hours * 3600 + clock.minute * 60));
    random_index = 0;
    for (i = 0; i < RANDOM_TABLE_SIZE; i++) {
        random_table[i] = genrand();
    }
}

u32 random_u32(void) {
    u32 r = random_table[random_index++];
    if (random_index == RANDOM_TABLE_SIZE) {
        random_index = 0;
    }
    return r;
}

float random_float(void) {
    u32 r = random_table[random_index++];
    if (random_index == RANDOM_TABLE_SIZE) {
        random_index = 0;
    }
    return (1.0f / 4294967296.0f) * r;
}

void refill_random_table(void) {
    random_index = 0;
    for (int i = 0; i < RANDOM_TABLE_SIZE; i++) {
        random_table[i] = genrand();
    }
}

float random_range(float min, float max) {
    float r = random_float();
    return min + r * (max - min);
}

float arctan2(float y, float x) {
    if (x == 0.0f) {
        return y == 0.0f ? 0.0f : (y > 0.0f ? 1.5707964f : -1.5707964f);
    }
    float angle = func_001CBE30(y / x);
    if (x < 0.0f) {
        angle += 3.1415927f;
    }
    return angle;
}

float fast_cos(float angle) {
    angle *= 0.15915494f;
    angle -= (int)func_001CC300(angle);
    angle *= 100.0f;
    int i = (int)func_001CC300(angle);
    angle -= i;
    return (1.0f - angle) * cos_table[i] + angle * cos_table[i + 1];
}

float fast_sin(float angle) {
    angle *= 0.15915494f;
    angle -= (int)func_001CC300(angle);
    angle *= 100.0f;
    int i = (int)func_001CC300(angle);
    angle -= i;
    return (1.0f - angle) * sin_table[i] + angle * sin_table[i + 1];
}

// An asm function: the compiler's square root intrinsic would put nops after the sqrt.s.
asm float sqrt_float(float x) {
    sqrt.s $f0, $f12
    jr $ra
    nop
}

// Cohen-Sutherland style outcode of p against the box min-max:
// 1/2 = beyond max.x/min.x, 4/8 = beyond max.y/min.y, 0x20/0x10 = beyond max.z/min.z.
// clip_segment_box() keeps its arguments in registers across the calls, which the compiler only
// does for a static function it has already compiled.
static int outcode(const vector3& min, const vector3& max, const vector3& p) {
    int code = 0;

    if (p.x > max.x) {
        code |= 1;
    } else if (p.x < min.x) {
        code |= 2;
    }
    if (p.y > max.y) {
        code |= 4;
    } else if (p.y < min.y) {
        code |= 8;
    }
    if (p.z > max.z) {
        code |= 0x20;
    } else if (p.z < min.z) {
        code |= 0x10;
    }
    return code;
}

// Intersects the segment start..start + delta with the plane through point.
// out receives the hit point and the fraction of delta.
int intersect_segment_plane(vector4* out, const vector3& start, const vector3& delta, const vector3& point,
                            const vector3& normal) {
    static vector3 offset;

    float denom = -normal.dot(delta);
    if (denom == 0.0f) {
        return 0;
    }
    offset = start - point;
    float t = normal.dot(offset) / denom;
    if (t < 0.0f) {
        return 0;
    }
    if (t > 1.0f) {
        return 0;
    }
    vector3 hit = start + delta * t;
    out->x = hit.x;
    out->y = hit.y;
    out->z = hit.z;
    out->w = t;
    return 1;
}

// Fraction of delta at which a point moving from start crosses the plane,
// for a point in front of it that reaches it within delta.
int segment_crosses_plane(const vector3& start, const vector3& delta, const vector4& plane, float* t) {
    float dist = start.x * plane.x + start.y * plane.y + start.z * plane.z + plane.w;
    if (dist < 0.0f) {
        return 0;
    }
    float speed = -(delta.x * plane.x + delta.y * plane.y + delta.z * plane.z);
    if (speed < dist) {
        return 0;
    }
    *t = dist / speed;
    return 1;
}

// First point where the segment start..end enters the sphere.
bool intersect_segment_sphere(vector3* out, const vector3& start, const vector3& end, const vector3& center,
                              float radius) {
    float dx, dy, dz, cx, cy, cz;

    dz = end.z - start.z;
    dy = end.y - start.y;
    dx = end.x - start.x;
    cz = center.z - start.z;
    cy = center.y - start.y;
    cx = center.x - start.x;
    float length = sqrt_float(dx * dx + dy * dy + dz * dz);
    if (length <= 0.0f) {
        float dist = sqrt_float(cx * cx + cy * cy + cz * cz);
        *out = start;
        return dist < radius ? 1 : 0;
    }
    float inv = 1.0f / length;
    vector3 dir(dx * inv, dy * inv, dz * inv);
    vector3 to_center(cx, cy, cz);
    float proj = dir.dot(to_center);
    if (proj < -radius) {
        return false;
    }
    if (proj > length + radius) {
        return false;
    }
    vector3 closest = start + dir * proj;
    float dist = distance(closest, center);
    if (dist < radius) {
        float t = proj - sqrt_float(radius * radius - dist * dist);
        if (t > length || t < 0.0f) {
            *out = start;
            return true;
        }
        *out = start + dir * t;
        return true;
    }
    return false;
}

#define EDGE_EPSILON 0.0001f

// Tests p against the triangle fan verts[0], verts[i - 1], verts[i] in the XZ plane.
bool point_in_polygon_xz(const vector3& p, int count, vector3** verts) {
    const vector3* origin = verts[0];
    float px = p.x - origin->x;
    float pz = p.z - origin->z;
    bool inside = false;
    int i = 2;

    do {
        const vector3* a = verts[i - 1];
        const vector3* b = verts[i];
        float e1x, e2x, e1z, e2z;
        e1x = a->x - origin->x;
        e1z = a->z - origin->z;
        e2x = b->x - origin->x;
        e2z = b->z - origin->z;

        float s;
        float t;
        if (e1x > -EDGE_EPSILON && e1x < EDGE_EPSILON) {
            s = px / e2x;
            if (s >= 0.0f && s <= 1.0f) {
                t = (pz - s * e2z) / e1z;
                inside = t >= 0.0f && t + s <= 1.0f;
            }
        } else {
            s = (pz * e1x - px * e1z) / (e2z * e1x - e2x * e1z);
            if (s >= 0.0f && s <= 1.0f) {
                t = (px - s * e2x) / e1x;
                inside = t >= 0.0f && t + s <= 1.0f;
            }
        }
    } while (!inside && ++i < count);
    return inside;
}

// Coordinate pairs to project onto for each major axis of the normal.
static int projection_axes[3][2] = {{2, 1}, {0, 2}, {1, 0}};

// As point_in_polygon_xz, in the plane of the two axes the normal is least aligned with.
bool point_in_polygon(const vector3& p, int count, vector3** verts, const vector3& normal) {
    float nx = abs_float(normal.x);
    float ny = abs_float(normal.y);
    float nz = abs_float(normal.z);
    int axis;
    if (nx > ny) {
        if (nx > nz) {
            axis = 0;
        } else {
            axis = 2;
        }
    } else if (ny > nz) {
        axis = 1;
    } else {
        axis = 2;
    }

    int u;
    int v;
    if (((const float*)&normal)[axis] > 0.0f) {
        u = projection_axes[axis][0];
        v = projection_axes[axis][1];
    } else {
        u = projection_axes[axis][1];
        v = projection_axes[axis][0];
    }

    bool inside = false;
    int i = 2;
    const float* origin = (const float*)verts[0];
    float pu = ((const float*)&p)[u] - origin[u];
    float pv = ((const float*)&p)[v] - origin[v];

    do {
        const float* a = (const float*)verts[i - 1];
        const float* b = (const float*)verts[i];
        float e1u, e2u, e1v, e2v;
        e1u = a[u] - origin[u];
        e1v = a[v] - origin[v];
        e2u = b[u] - origin[u];
        e2v = b[v] - origin[v];

        float s;
        float t;
        if (e1u > -EDGE_EPSILON && e1u < EDGE_EPSILON) {
            s = pu / e2u;
            if (s >= 0.0f && s <= 1.0f) {
                t = (pv - s * e2v) / e1v;
                inside = t >= 0.0f && t + s <= 1.0f;
            }
        } else {
            s = (pv * e1u - pu * e1v) / (e2v * e1u - e2u * e1v);
            if (s >= 0.0f && s <= 1.0f) {
                t = (pu - s * e2u) / e1u;
                inside = t >= 0.0f && t + s <= 1.0f;
            }
        }
    } while (!inside && ++i < count);
    return inside;
}

bool spheres_overlap(const vector3& a, const vector3& b, float radius_a, float radius_b) {
    return distance_squared(a, b) < (radius_a + radius_b) * (radius_a + radius_b);
}

// Intersects the segment start..start + delta with the slab of half-width radius around the plane
// through point. t_enter receives the entry fraction and t_span the fraction spent inside; when the
// entry lies on the segment, out receives it projected onto the plane.
bool intersect_segment_slab(vector3* out, const vector3& start, const vector3& delta, const vector3& normal,
                            const vector3& point, float* t_enter, float* t_span, float radius) {
    float d = -normal.dot(point);
    float dist = normal.dot(start);
    float denom = normal.dot(delta);
    float t0;
    float t1;

    if (abs_float(denom) > 1.0e-30f) {
        float base = -d - dist;
        t0 = (radius + base) / denom;
        t1 = (base - radius) / denom;
    } else {
        return false;
    }
    if (t1 < t0) {
        float swap = t0;
        t0 = t1;
        t1 = swap;
    }
    *t_enter = t0;
    if (t0 > 0.0f && t0 < 1.0f) {
        static vector3 hit;
        hit = start + delta * t0;
        project_onto_plane(out, hit, normal, point);
    }
    *t_span = t1 - t0;
    return t0 < 1.0f && t1 > 0.0f;
}

bool sweep_sphere_edge(vector3& hit, const vector3& origin, const vector3& dir, float radius, const vector3& a,
                       const vector3& b, float* t_out, float max_t) {
    static vector3 contact;
    static vector3 center;
    int vertex = false;
    vector3 edge = b - a;
    vector3 w = origin - a;
    float we = w.dot(edge);
    float wd = w.dot(dir);
    float ww = w.dot(w);
    float ed = edge.dot(dir);
    float ee = edge.dot(edge);
    float dd = dir.dot(dir);
    float t = 0.0f;
    // Distance from the moving centre to the edge's line equals radius:
    // qa t^2 + qb t + qc = 0.
    float qa = ed * ed - ee * dd;
    float qb = 2.0f * (we * ed - wd * ee);
    float qc = we * we + ee * (radius * radius) - ww * ee;
    float rr = radius * radius;
    if (qa == 0.0f) {
        // Moving parallel to the edge.
        vertex = true;
    } else {
        float disc = qb * qb - 4.0f * qa * qc;
        if (disc > 0.0f) {
            float s = sqrt_float(disc);
            t = (-qb + s) / (2.0f * qa);
            float t2 = (-qb - s) / (2.0f * qa);
            if (t2 < t) {
                float tmp = t;
                t = t2;
                t2 = tmp;
            }
            // Treat a contact just behind the start as touching at the start.
            if (t >= -0.05f && t < 0.0f) {
                t = 1e-6f;
            }
            if (t < 0.0f || t > 1.0f) {
                if (t > 1.0f || t2 < 0.0f) {
                    return false;
                }
                // Already overlapping the line at the start.
                vertex = true;
            } else if (t >= max_t) {
                return false;
            }
        } else {
            return false;
        }
        if (!vertex) {
            center = origin + dir * t;
            float u = (center - a).dot(edge) / ee;
            if (u < 0.0f || u > 1.0f) {
                // The line is hit outside the edge.
                vertex = true;
            } else {
                contact = a + edge * u;
            }
        }
    }
    if (vertex) {
        // Sphere against the end point a.
        float b2 = 2.0f * wd;
        float disc = b2 * b2 - 4.0f * dd * (ww - rr);
        if (disc > 0.0f) {
            float s = sqrt_float(disc);
            t = (-b2 + s) / (2.0f * dd);
            float t2 = (-b2 - s) / (2.0f * dd);
            if (t > t2) {
                t = t2;
            }
            if (t < 0.0f || t > 1.0f || t >= max_t) {
                return false;
            }
            center = origin + dir * t;
            contact = a;
        } else {
            return false;
        }
    }
    *t_out = t;
    hit = contact;
    return true;
}

bool sweep_sphere_polygon(vector3& hit, const vector3& origin, const vector3& dir, float radius, int count,
                          vector3** verts, float* t_out) {
    static vector3 a;
    static vector3 b;
    float best = 3.4028235e38f; // FLT_MAX
    for (int i = 0; i < count; i++) {
        a = *verts[i];
        if (i + 1 == count) {
            b = *verts[0];
        } else {
            b = *verts[i + 1];
        }
        static vector3 point;
        static vector3 lo;
        static vector3 hi;
        // Box around the whole sweep; skip edges that miss it.
        vector3 min = origin - radius;
        vector3 max = origin + radius;
        box_from_points(lo, hi, min, max);
        sweep_box(lo, hi, dir);
        if (clip_segment_box(lo, hi, a, b, point, 0)) {
            float t;
            bool found = sweep_sphere_edge(point, origin, dir, radius, a, b, &t, best);
            if (found && t < best) {
                best = t;
                hit = point;
            }
        }
    }
    if (best <= 1.0f) {
        *t_out = best;
        return true;
    }
    return false;
}

bool intersect_ray_plane(const ray& r, const plane& p, vector3& hit) {
    float denom = r.dir.dot(p.normal);
    if (denom == 0.0f) {
        return false;
    }
    float t = -(p.d + r.origin.dot(p.normal)) / denom;
    hit = r.origin + r.dir * t;
    return !(t < 0.0f);
}

bool clip_segment_obb(const vector3& center, const matrix3& m, const vector3& size, const vector3& p0,
                      const vector3& p1, vector3& out, int ignore_end) {
    static vector3 a;
    static vector3 b;
    // Into the box's frame.
    a = p0 - center;
    a.rotate(m);
    b = p1 - center;
    b.rotate(m);
    static vector3 lo;
    static vector3 hi;
    lo = size * -0.5f;
    hi = size * 0.5f;
    bool hit = clip_segment_box(lo, hi, a, b, out, ignore_end);
    // And back to world space.
    out.unrotate(m);
    out += center;
    return hit;
}

bool clip_segment_box(const vector3& min, const vector3& max, const vector3& p0, const vector3& p1, vector3& out,
                      int ignore_end) {
    int c0 = outcode(min, max, p0);
    if (c0 == 0) {
        out = p0;
        return true;
    }
    int c1 = outcode(min, max, p1);
    if (c1 == 0 && !ignore_end) {
        out = p1;
        return true;
    }
    if (c0 & c1) {
        // Both ends beyond the same face.
        return false;
    }
    if (c0 & 3) {
        if (c0 & 1) {
            out.x = max.x;
        } else {
            out.x = min.x;
        }
        float len = p1.x - p0.x;
        float d = out.x - p0.x;
        out.y = p0.y + d * (p1.y - p0.y) / len;
        out.z = p0.z + d * (p1.z - p0.z) / len;
        if (out.y <= max.y && out.y >= min.y && out.z <= max.z && out.z >= min.z) {
            return true;
        }
    }
    if (c0 & 0xC) {
        if (c0 & 4) {
            out.y = max.y;
        } else {
            out.y = min.y;
        }
        float len = p1.y - p0.y;
        float d = out.y - p0.y;
        out.x = p0.x + d * (p1.x - p0.x) / len;
        out.z = p0.z + d * (p1.z - p0.z) / len;
        if (out.x <= max.x && out.x >= min.x && out.z <= max.z && out.z >= min.z) {
            return true;
        }
    }
    if (c0 & 0x30) {
        if (c0 & 0x20) {
            out.z = max.z;
        } else {
            out.z = min.z;
        }
        float len = p1.z - p0.z;
        float d = out.z - p0.z;
        out.x = p0.x + d * (p1.x - p0.x) / len;
        out.y = p0.y + d * (p1.y - p0.y) / len;
        if (out.x <= max.x && out.x >= min.x && out.y <= max.y && out.y >= min.y) {
            return true;
        }
    }
    return false;
}

bool intersect_ray_sphere(vector3& hit, float* t_out, const ray& r, float length, const vector3& center, float radius) {
    float rr = radius * radius;
    vector3 l = center - r.origin;
    float ll = l.dot(l);
    if (ll <= rr) {
        hit = r.origin;
        return true;
    }
    float tca = r.dir.dot(l);
    if (tca < 0.0f) {
        return false;
    }
    float thc2 = rr - (ll - tca * tca);
    if (thc2 < 0.0f) {
        return false;
    }
    *t_out = tca - sqrt_float(thc2);
    hit = r.origin + r.dir * *t_out;
    return true;
}

// r and s are the positions of the crossing along a-b and (x0, y0)-(x1, y1); both must lie in
// [0, 1]. Each is its own numerator over the same denominator, which is zero for parallel segments.
bool segments_intersect_2d(const float* a, const float* b, float x0, float y0, float x1, float y1) {
    float r_num = (a[1] - y0) * (x1 - x0) - (a[0] - x0) * (y1 - y0);
    float r_den = (b[0] - a[0]) * (y1 - y0) - (b[1] - a[1]) * (x1 - x0);
    if (r_den == 0.0f) {
        return false;
    }
    float r = r_num / r_den;
    if (r < 0.0f || r > 1.0f) {
        return false;
    }
    float s_num = (a[1] - y0) * (b[0] - a[0]) - (a[0] - x0) * (b[1] - a[1]);
    float s_den = (b[0] - a[0]) * (y1 - y0) - (b[1] - a[1]) * (x1 - x0);
    if (s_den == 0.0f) {
        return false;
    }
    float s = s_num / s_den;
    if (s < 0.0f || s > 1.0f) {
        return false;
    }
    return true;
}
