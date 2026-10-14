#ifndef GAME_UNIT_0017A830_H
#define GAME_UNIT_0017A830_H

#include "types.h"
#include "game/matrix.h"

// Keyframed effect emitters: the tracks that animate an emitter definition and the per-instance
// state they drive.

// Track::value() returns this once the track has ended.
#define TRACK_DONE 4000

// Points the pointer at base plus its offset, unless it is null.
extern "C" void func_0016C850(u8** pointer, int base);

// A keyframed value, one entry per span of frames. An entry whose first byte has bit 0 clear is a
// one-frame key: its first two bytes, low byte first, are the value. An entry whose first byte
// has bit 0 set lasts as many frames as its second byte says; a first byte of 1 holds the value
// in the bytes after that, any other odd first byte ends the track, as does a zero length. Two
// cursors (pos and time) let two nearby frames be looked up without rescanning from the start.
struct Track {
    /* 0x00 */ u8* data;
    /* 0x04 */ u16 done : 1;
    /* 0x04 */ u16 short_values : 1; // values take one byte instead of two
    /* 0x04 */ u16 size : 14;
    /* 0x06 */ s16 base; // added to every value
    /* 0x08 */ u16 pos[2];
    /* 0x0C */ u16 time[2];

    s16 value(u16 time);

    // Rewinds the cursors and points data into the loaded file.
    void relocate(int file_base) {
        pos[0] = 0;
        time[0] = 0;
        pos[1] = 0;
        time[1] = 0;
        done = 0;
        func_0016C850(&data, file_base);
    }
};

// Out of line in another file; the same as Track::relocate.
extern "C" void func_0016C3E0(Track* track, int file_base);

// Track values are 8.8 fixed point.
inline float fixed8_to_float(s16 v) { return (1.0f / 256) * v; }

inline float track_lerp(float a, float b, float t, float inv_t) { return inv_t * a + t * b; }
inline float track_lerp_fixed8(s16 a, s16 b, float t, float inv_t) {
    return inv_t * ((1.0f / 256) * a) + t * ((1.0f / 256) * b);
}

struct VectorTrack {
    /* 0x00 */ Track x;
    /* 0x10 */ Track y;
    /* 0x20 */ Track z;

    vector3 get(int frame) {
        return vector3(fixed8_to_float(x.value(frame)), fixed8_to_float(y.value(frame)),
                       fixed8_to_float(z.value(frame)));
    }
};

// Out of line copy of the packed quaternion constructor (four 16-bit components).
extern "C" s16* func_0016C740(s16* packed, s16 x, s16 y, s16 z, s16 w);

// Components are fractions of 16383.
struct QuatTrack {
    /* 0x00 */ Track x;
    /* 0x10 */ Track y;
    /* 0x20 */ Track z;
    /* 0x30 */ Track w;

    void get(quaternion* q, u16 frame) {
        s16 packed[4];

        func_0016C740(packed, x.value(frame), y.value(frame), z.value(frame), w.value(frame));
        q->x = (1.0f / 16383) * packed[0];
        q->y = (1.0f / 16383) * packed[1];
        q->z = (1.0f / 16383) * packed[2];
        q->w = (1.0f / 16383) * packed[3];
    }
};

// Replaces v with v transformed by m.
inline void transform_in_place(vector3* v, const matrix43& m) {
    vector3 result;
    transform_point(&result, m, *v);
    *v = result;
}

struct EmitterDef {
    /* 0x00 */ int unk00; // relocated against base2
    /* 0x04 */ int unk04; // relocated against base2
    /* 0x08 */ int unk08; // relocated against the file base
    /* 0x0C */ int unk0C;
    /* 0x10 */ u32 type : 8;
    /* 0x10 */ s32 num_frames : 24;
    /* 0x14 */ VectorTrack pos;
    /* 0x44 */ QuatTrack orient;
    /* 0x84 */ Track value18; // EmitterState::value18, scaled by 2000
    /* 0x94 */ Track value1C; // the rest are 8.8 fixed point
    /* 0xA4 */ Track value20;
    /* 0xB4 */ Track value24;
    /* 0xC4 */ Track value28;

    vector3 get_pos(int frame) { return pos.get(frame); }
    void relocate(int file_base, int base2);
};

// Emitter state interpolated between two frames, handed to the particle code.
struct EmitterState {
    /* 0x00 */ vector3 pos;
    /* 0x0C */ vector3 dir; // the emitter's up axis
    /* 0x18 */ f32 value18;
    /* 0x1C */ f32 value1C;
    /* 0x20 */ f32 value20;
    /* 0x24 */ f32 value24;
    /* 0x28 */ f32 value28;
};

struct EmitterInstance {
    /* 0x00 */ int parent; // passed with parent_index to get the matrix to attach to
    /* 0x04 */ u32 parent_index : 29;
    /* 0x04 */ u32 frozen : 1;   // keep the current position and orientation
    /* 0x04 */ u32 attached : 1; // the tracks are relative to the parent's matrix
    /* 0x04 */ u32 flag31 : 1;
    /* 0x08 */ f32 time;
    /* 0x0C */ vector3 pos;
    /* 0x18 */ quaternion orient;
    /* 0x28 */ vector3 prev_pos;
    /* 0x34 */ quaternion prev_orient;

    void reset();
    void update(EmitterDef* def, float time);
    void get_state(EmitterDef* def, EmitterState* state, float time);
};

#endif // GAME_UNIT_0017A830_H
