#include "game/unit_0017A830.h"

// floorf
extern "C" float func_001CC300(float x);
// The matrix an attached emitter follows.
extern "C" matrix43 func_0016AE00(int parent, int index);
// Relocates the fields shared by all effect definitions.
extern "C" void func_0016C2A0(EmitterDef* def, int file_base, int base2);

s16 Track::value(u16 t) {
    int i;
    int best;
    int min_time;
    int later;
    int diff;
    u8* bytes;
    u8 op;
    u32 duration;
    u32 step;
    u8* entry;
    u8* value;
    u16 start;

    if (done) {
        return TRACK_DONE;
    }

    // Continue from the cursor that is at t, else from the earlier of those before t. If both are
    // past t, rewind them.
    best = 0;
    min_time = 0x10000;
    later = 0;
    for (i = 0; i < 2; i++) {
        diff = t - time[i];
        if (diff == 0) {
            break;
        }
        if (diff > 0) {
            if (time[i] < min_time) {
                min_time = time[i];
                best = i;
            }
        } else {
            later++;
        }
    }
    if (later == 2) {
        pos[0] = 0;
        i = 0;
        time[0] = 0;
        pos[1] = 0;
        time[1] = 0;
    } else if (i == 2) {
        i = best;
    }
    if (t < time[i]) {
        time[i] = 0;
        pos[i] = 0;
    }

    while (pos[i] < size) {
        bytes = data;
        op = bytes[pos[i]];
        if (op & 1) {
            duration = bytes[pos[i] + 1];
            if (duration == 0) {
                done = 1;
                return TRACK_DONE;
            }
            start = time[i];
            if (start <= t && t < start + duration) {
                entry = &bytes[pos[i]];
                value = entry + 2;
                switch (*entry) {
                case 1:
                    if (short_values) {
                        return base + (s16)value[0];
                    }
                    return base + (s16)(value[0] | ((s16)value[1] << 8));
                default:
                    return 0;
                }
            }
            switch (op) {
            case 1:
                step = 4;
                break;
            default:
                done = 1;
                return TRACK_DONE;
            }
            if (short_values) {
                step--;
            }
            time[i] = start + duration;
        } else {
            if (t == time[i]) {
                return base + (bytes[pos[i]] + ((s16)bytes[pos[i] + 1] << 8));
            }
            time[i]++;
            if (!short_values) {
                step = 2;
            } else {
                step = 1;
            }
        }
        pos[i] += step;
    }
    return 0;
}

void EmitterInstance::reset() {
    attached = 0;
    frozen = 0;
    flag31 = 0;
    time = 0.0f;
}

void EmitterInstance::update(EmitterDef* def, float t) {
    int frame;
    int next;
    float frac;
    float inv;
    quaternion q1;
    quaternion q2;

    if (time > 0.0f) {
        prev_pos = pos;
        prev_orient = orient;
    }
    if (!frozen) {
        frame = (int)func_001CC300(t);
        next = frame + 1;
        if (next >= def->num_frames) {
            next = frame;
        }
        frac = t - (int)func_001CC300(t);
        inv = 1.0f - frac;
        pos = def->get_pos(frame) * inv + def->get_pos(next) * frac;
        def->orient.get(&q1, frame);
        def->orient.get(&q2, next);
        slerp(&orient, q1, q2, frac);
        if (attached) {
            matrix3 rot;
            matrix43 m3;
            matrix43 m;

            m = func_0016AE00(parent, parent_index);
            m.get_rotation(&rot);
            vector3 p(m.m[3][0], m.m[3][1], m.m[3][2]);
            matrix43 m2(p, rot);
            transform_in_place(&pos, m2);
            m3.set(orient, pos);
            multiply_4x4(&m3, m2, m3);
            orient = quaternion(m3);
        }
    }
    if (time <= 0.0f) {
        prev_pos = pos;
        prev_orient = orient;
    }
    if (t >= def->num_frames) {
        time = def->num_frames - 1;
    } else {
        time = t;
    }
}

void EmitterInstance::get_state(EmitterDef* def, EmitterState* state, float t) {
    int next;
    int frame;
    float frac;
    float inv;
    quaternion q;
    matrix3 m;

    frame = (int)func_001CC300(t);
    next = frame + 1;
    if (next >= def->num_frames) {
        next = frame;
    }
    frac = t - (int)func_001CC300(t);
    inv = 1.0f - frac;
    state->pos = prev_pos * inv + pos * frac;
    slerp(&q, prev_orient, orient, frac);
    q.get_matrix(&m);
    state->dir = vector3(0.0f, 1.0f, 0.0f);
    state->dir.unrotate(m);
    state->dir.normalize_safe();
    state->value18 = track_lerp(2000.0f * def->value18.value(frame), 2000.0f * def->value18.value(next), frac, inv);
    state->value1C =
        track_lerp(fixed8_to_float(def->value1C.value(frame)), fixed8_to_float(def->value1C.value(next)), frac, inv);
    state->value20 = track_lerp_fixed8(def->value20.value(frame), def->value20.value(next), frac, inv);
    state->value24 = track_lerp_fixed8(def->value24.value(frame), def->value24.value(next), frac, inv);
    state->value28 = track_lerp_fixed8(def->value28.value(frame), def->value28.value(next), frac, inv);
}

void EmitterDef::relocate(int file_base, int base2) {
    func_0016C2A0(this, file_base, base2);
    func_0016C3E0(&pos.x, file_base);
    func_0016C3E0(&pos.y, file_base);
    func_0016C3E0(&pos.z, file_base);
    func_0016C3E0(&orient.x, file_base);
    func_0016C3E0(&orient.y, file_base);
    func_0016C3E0(&orient.z, file_base);
    func_0016C3E0(&orient.w, file_base);
    value18.relocate(file_base);
    value1C.relocate(file_base);
    value20.relocate(file_base);
    value24.relocate(file_base);
    value28.relocate(file_base);
}
