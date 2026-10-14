#include "game/level_scripts.h"
#include "game/debug.h"
#include "game/gr.h"
#include "game/memory.h"
#include "game/platform_ngps.h"
#include "game/unit_00190720.h"
#include "macros.h"
#include <stdio.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// TODO: types from other units; names and layouts are placeholders.
struct CreatureStats {
    /* 0x00 */ u8 pad00[0x48];
    /* 0x48 */ s16 level;
};

// A creature or other live object.
struct Actor {
    /* 0x000 */ u8 pad000[0x08];
    /* 0x008 */ int type;
    /* 0x00C */ vector3 position;
    /* 0x018 */ vector3 last_position;
    /* 0x024 */ u8 pad024[0x58];
    /* 0x07C */ char name[0x24];
    /* 0x0A0 */ void* model;
    /* 0x0A4 */ Actor* next;
    /* 0x0A8 */ Actor* prev;
    /* 0x0AC */ u8 pad0AC[0x08];
    /* 0x0B4 */ u32 flags;
    /* 0x0B8 */ u8 pad0B8[0x04];
    /* 0x0BC */ u8 state[0x50];
    /* 0x10C */ int target; // handle of the object this creature targets
    /* 0x110 */ u8 pad110[0x33C];
    /* 0x44C */ int hit_points;
    /* 0x450 */ u8 pad450[0x06];
    /* 0x456 */ s16 level;
    /* 0x458 */ u8 pad458[0x14];
    /* 0x46C */ CreatureStats* stats;
    /* 0x470 */ u8 pad470[0x7C];
    /* 0x4EC */ int team;
};

// Entry in the object lists at D_007C27C0 (the party) and D_007C2820. The list heads are sentinel
// entries.
struct ObjectListNode {
    /* 0x00 */ ObjectListNode* next;
    /* 0x04 */ ObjectListNode* prev;
    /* 0x08 */ int type;
    /* 0x0C */ int handle;
    /* 0x10 */ u32 flags;
};

// Object type table entry (D_003DAF60).
struct ObjectType {
    /* 0x000 */ u8 pad000[0x24];
    /* 0x024 */ s16 id;
    /* 0x026 */ u8 pad026[0x286];
};

// Level mesh; navigation layers are looked up per mesh.
struct Mesh {
    /* 0x00 */ u8 pad00[0x60];
};

struct PointList {
    /* 0x00 */ u8 pad00[0x06];
    /* 0x06 */ u16 count;
    /* 0x08 */ vector3* points;
};

// Navigation mesh node.
struct NavNode {
    /* 0x00 */ u8 pad00[0x48];
};

// Level geometry header at D_002E13C0.
struct LevelGeometry {
    /* 0x00 */ u8 pad00[0x10];
    /* 0x10 */ Mesh* meshes;
    /* 0x14 */ u8 pad14[0x14];
    /* 0x28 */ PointList* point_lists;
    /* 0x2C */ u8 pad2C[0x08];
    /* 0x34 */ int num_nodes;
    /* 0x38 */ NavNode* nodes;
};

struct MeshRef {
    /* 0x00 */ u8 pad00[0x04];
    /* 0x04 */ s16 point_list;
};

// The level being played (D_00A27F70).
struct CurrentLevel {
    /* 0x00 */ char name[0x8C];
    /* 0x8C */ Navpoint* player_starts[4];
    /* 0x9C */ int start_nav;
};

// Level table entry (D_002AD550).
struct LevelDef {
    /* 0x00 */ const char* name;
    /* 0x04 */ u8 pad04[0x40];
};

// Ray cast against the level geometry by func_0010C6E0.
struct RayCast {
    RayCast() { field_30 = 0; }

    /* 0x00 */ vector3 origin;
    /* 0x0C */ vector3 direction;
    /* 0x18 */ int field_18;
    /* 0x1C */ int field_1C;
    /* 0x20 */ int field_20;
    /* 0x24 */ int field_24;
    /* 0x28 */ u8 pad28[0x08];
    /* 0x30 */ int field_30;
    /* 0x34 */ u8 pad34[0x38];
};

struct RayHit {
    /* 0x00 */ int hit;
    /* 0x04 */ float t;
    /* 0x08 */ u8 pad08[0x1C];
    /* 0x24 */ int mesh;
    /* 0x28 */ u8 pad28[0x04];
};

struct Camera {
    /* 0x00 */ u8 pad00[0x0C];
    /* 0x0C */ u8 flag0 : 1;
    /* 0x0C */ u8 flag1 : 1;
    /* 0x0C */ u8 flag2 : 1;

    bool IsFlag2() {
        if (flag2) {
            return true;
        }
        return false;
    }
};

// Input to func_0025EC60: up to eight segments in the XZ plane, each with a side and a layer.
struct AvoidanceSplineDef {
    /* 0x00 */ int num_segments;
    /* 0x04 */ int sides[8];
    /* 0x24 */ int layers[8];
    /* 0x44 */ vector2 starts[8];
    /* 0x84 */ vector2 ends[8];
};

extern CurrentLevel D_00A27F70;
extern Actor D_00381BB0;  // sentinel of the creature list linked through Actor::next
extern Actor* D_002D458C; // the player, leader of the party
extern ObjectListNode D_007C27C0;
extern ObjectListNode D_007C2820;
extern ObjectType D_003DAF60[];
extern LevelGeometry D_002E13C0;
extern LevelDef D_002AD550[];
extern Camera D_003CEF80;
extern u8 D_009B6B50[]; // game flags, tested with func_001ADA90
extern u8 D_00B3D5A0[];
extern u32 D_002D4534;
extern int D_002D3FAC;
extern bool D_002D3F74;

extern "C" void func_0010C6E0(RayCast* ray, RayHit* hit);
extern "C" void* func_0010CCF0(void* object);
extern "C" int func_0010E8C0(Mesh* mesh, int index);
extern "C" int func_0010E8D0(Mesh* mesh, vector3* point, int a2);
extern "C" void func_00120E00(Actor* actor);
extern "C" void func_00121AA0(Actor* actor, int index);
extern "C" float func_00123C10(Actor* actor);
extern "C" void func_00127830(Actor* actor);
extern "C" BOOL func_0012DEB0(Actor* a, Actor* b);
extern "C" Actor* func_00132910(int handle);
extern "C" void func_00133FD0(Camera* camera, Actor* actor);
extern "C" void func_00134EE0(Camera* camera, int a, int b);
extern "C" BOOL func_0013E1A0(void* state, int a, int b);
extern "C" void func_0013E9E0(void* state);
extern "C" void func_0014B380(void* state);
extern "C" void func_00157690(vector2* v, const vector3* source);
extern "C" int func_001576E0(const vector2* point, const segment2* line, int unused);
extern "C" int func_00157EC0(const segment2* a, const segment2* b, vector2* intersection);
extern "C" u8 func_001587D0(vector3* point, int a1, int layer);
extern "C" void func_0015DF40(int a0);
extern "C" void func_0015F640(int a);
extern "C" void func_001697D0(void* model, int a1, float f12);
extern "C" void func_0016A8F0(void* model);
extern "C" void func_0016A940(void* model);
extern "C" int func_001ADA90(void* flags, int flag);
extern "C" int func_001D7C50(int sound, const vector3* position, const vector3* velocity, int mask, float radius);
extern "C" void func_001D7F80(int handle, const vector3* position, const vector3* velocity);
extern "C" void func_001D9C80(const char* level, int a);
extern "C" int func_001DA050(int event, const char* name, const char* argument);
extern "C" char* func_001DA4B0(void);
extern "C" u8 func_001DA510(void);
extern "C" void func_001EBCA0(void);
extern "C" void func_001EBCF0(void);
extern "C" void func_001EBDF0(void* sequence, Actor* actor);
extern "C" void func_001EC2A0(void);
extern "C" int func_001EC8A0(void);
extern "C" void func_001F99A0(void* level, int a);
extern "C" void func_001F9F70(void* level);
extern "C" void func_001FEF60(int a, int b);
extern "C" int func_0025EBC0(int id);
extern "C" int func_0025EC60(AvoidanceSplineDef* spline);
extern "C" void func_00260900(void* level);
extern "C" void func_00260A70(void);
extern "C" Mempool* func_00283660(void* a);

const char* D_002A9F60[NUM_NAVPOINT_TYPES] = {"level", "cutscene", "camera", "ambient sound"};

int D_002D491C;
int D_002D4920;
int D_002D4924;
int D_002D4928;
NavNodeScript* D_002D492C;
int D_002D4930;
int D_002D4934;
int D_002D4938;
int D_002D493C;
int D_002D4940;
int D_002D4944;
int D_002D4948;
int D_002D494C;
int D_002D4950;
int D_002D4954;
const char* D_002D4958;
int D_002D495C;
int D_002D4960;
bool D_002D4964;
bool D_002D4968;
int D_002D496C;
int D_002D4970;
int D_002D4974;
int D_002D4978;
Actor* D_002D497C;
int D_002D4980;

Navpoint D_009DEF10[MAX_NAVPOINTS];
ScriptTarget D_009EC9D0[MAX_SCRIPT_TARGETS];
LevelItem D_009ECCD0[MAX_LEVEL_ITEMS];
AmbientSound D_009ECED0[MAX_AMBIENT_SOUNDS];
Constraint D_009ED2D0[MAX_CONSTRAINTS];
ClickSpot D_009ED550[MAX_CLICK_SPOTS];
LoadSpline D_009EE050[MAX_LOAD_SPLINES];
TriggerSpline D_009EE190[MAX_TRIGGER_SPLINES];
TriggerSphere D_009EE890[MAX_TRIGGER_SPHERES];
Shopkeeper D_009EEB90[MAX_SHOPKEEPERS];
ShopItem D_009EEC80[MAX_SHOP_ITEMS];
Navpoint D_009F0BC0[MAX_EXTRA_NAVPOINTS];
char D_009F15C0[LEVEL_SCRIPT_POOL_SIZE];
StaticMempool D_00A015C0(D_009F15C0, LEVEL_SCRIPT_POOL_SIZE, "level scripts", 4);
char D_00A015E0[LEVEL_ITEM_POOL_SIZE];
StaticMempool D_00A025E0(D_00A015E0, LEVEL_ITEM_POOL_SIZE, "level itmes", 4);
Region D_00A02600[MAX_REGIONS];
vector3 D_00A029E8;
SoundSlot D_00A02A00[MAX_SOUND_SLOTS];

// Finds the player start navpoints for the current level.
void func_001AFE00(void) {
    char name[64];
    int i;

    if (D_00A27F70.start_nav < 0) {
        D_00A27F70.start_nav = 1;
    }
    for (i = 0; i < 4; i++) {
        sprintf(name, "$player%d-%02d", D_00A27F70.start_nav, i + 1);
        D_00A27F70.player_starts[i] = func_001B1330(name);
        if (D_00A27F70.player_starts[i] == NULL) {
            sprintf(name, "$player%02d", i + 1);
            D_00A27F70.player_starts[i] = func_001B1330(name);
        }
    }
    if (D_00A27F70.player_starts[0] == NULL) {
        sprintf(string_buffer, "Unable to locate player start for level %s.", func_001DA4B0());
        DEBUGERRORLINE(0x284, string_buffer);
    }
}

void func_001AFF10(void) {
    int i;

    for (i = 0; i < MAX_SOUND_SLOTS; i++) {
        D_00A02A00[i].handle = -1;
    }
}

void func_001AFFA0(void) {
    int i;

    for (i = 0; i < MAX_SOUND_SLOTS; i++) {
        SoundSlot* slot = &D_00A02A00[i];

        if (slot->handle != -1) {
            func_0025EBC0(slot->sound);
            slot->handle = -1;
        }
    }
}

void func_001B0010(void) {
    int i;

    for (i = 0; i < D_002D4928; i++) {
        AmbientSound* sound = &D_009ECED0[i];

        func_001D7F80(sound->handle, &sound->source->position, &Zero_vector3);
    }
}

// Returns TRUE if actor crossed the line between left and right, in one direction, since the
// last frame.
BOOL func_001B0080(Actor* actor, Navpoint* left, Navpoint* right) {
    segment2 path;
    segment2 line;
    vector2 from;
    vector2 to;
    vector2 line_start;
    vector2 line_end;
    vector2 crossing;

    if (actor == NULL) {
        return FALSE;
    }
    if (left == NULL || right == NULL) {
        return FALSE;
    }
    if (left->position == right->position) {
        return FALSE;
    }
    if (actor->position == actor->last_position) {
        return FALSE;
    }

    func_00157690(&from, &actor->last_position);
    func_00157690(&to, &actor->position);
    path.start = &from;
    path.end = &to;
    func_00157690(&line_start, &left->position);
    func_00157690(&line_end, &right->position);
    line.start = &line_start;
    line.end = &line_end;
    if (func_00157EC0(&path, &line, &crossing) != 1) {
        return FALSE;
    }
    if (func_001576E0(&to, &line, 0)) {
        return FALSE;
    }

    // The crossing must also be at about the same height on both lines.
    vector3 path_delta = actor->position - actor->last_position;
    vector3 line_delta = right->position - left->position;
    float path_y = actor->last_position.y +
                   path_delta.y * ((crossing.x + crossing.y - actor->last_position.x - actor->last_position.z) /
                                   (path_delta.x + path_delta.z));
    float line_y = left->position.y +
                   line_delta.y * ((crossing.x + crossing.y - left->position.x - left->position.z) /
                                   (line_delta.x + line_delta.z));
    if (__s_abs(path_y - line_y) <= 2.0f) {
        return TRUE;
    }
    return FALSE;
}

// Checks the player against the load splines.
void func_001B02E0(void) {
    vector3 position = D_002D458C->last_position;
    int i;

    for (i = 0; i < D_002D493C; i++) {
        LoadSpline* spline = &D_009EE050[i];

        if (func_001B0080(D_002D458C, spline->left, spline->right)) {
            if (spline->enabled) {
                D_002D4958 = D_002AD550[spline->level].name;
                D_002D495C = 0;
                D_002D4960 = spline->start_nav;
                D_002D4964 = spline->worldmap;
                D_002D496C = get_party_gather_state();
                D_00A029E8 = position;
                D_002D4968 = false;
            } else {
                func_001DA050(7, spline->name, D_002AD550[spline->level].name);
            }
            break;
        }
    }
}

// Checks the party against the trigger splines.
void func_001B0420(void) {
    Actor* actors[4];
    int count = 0;
    ObjectListNode* node;
    int i;
    int j;

    for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        if (!(node->flags & 2)) {
            Actor* actor = func_00132910(node->handle);

            if (actor != NULL && !(actor->flags & 0x800088) && actor->hit_points > 0) {
                if (count >= 4) {
                    break;
                }
                actors[count++] = actor;
            }
        }
    }

    for (i = 0; i < D_002D4940; i++) {
        TriggerSpline* spline = &D_009EE190[i];

        for (j = 0; j < spline->num_disable_flags; j++) {
            if (func_001ADA90(D_009B6B50, spline->disable_flags[j])) {
                break;
            }
        }
        if (j >= spline->num_disable_flags) {
            for (j = 0; j < count; j++) {
                Actor* actor = actors[j];

                if (func_001B0080(actor, spline->left, spline->right)) {
                    if (spline->sequence) {
                        D_002D497C = actor;
                        func_001EBDF0(spline->sequence, actor);
                    }
                    func_001DA050(4, spline->name, NULL);
                    break;
                }
            }
        }
    }
}

// Checks the party against the trigger spheres.
void func_001B05F0(void) {
    Actor* actors[4];
    int count = 0;
    ObjectListNode* node;
    int i;
    int j;

    for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        if (!(node->flags & 2)) {
            Actor* actor = func_00132910(node->handle);

            if (actor != NULL && !(actor->flags & 0x800088) && actor->hit_points > 0) {
                if (count >= 4) {
                    break;
                }
                actors[count++] = actor;
            }
        }
    }

    for (i = 0; i < D_002D4944; i++) {
        TriggerSphere* sphere = &D_009EE890[i];

        for (j = 0; j < sphere->num_disable_flags; j++) {
            if (func_001ADA90(D_009B6B50, sphere->disable_flags[j])) {
                break;
            }
        }
        if (j >= sphere->num_disable_flags) {
            for (j = 0; j < count; j++) {
                Actor* actor = actors[j];

                if ((actor->last_position - sphere->navpoint->position).length_squared() <=
                    sphere->radius * sphere->radius) {
                    continue;
                }
                if ((actor->position - sphere->navpoint->position).length_squared() <=
                    sphere->radius * sphere->radius) {
                    if (sphere->sequence) {
                        func_001EBDF0(sphere->sequence, actor);
                    }
                    func_001DA050(5, sphere->name, NULL);
                    break;
                }
            }
        }
    }
}

void func_001B0870(void) {
    D_002D491C = 0;
    D_002D4920 = 0;
    D_002D4924 = 0;
    D_002D4928 = 0;
    D_002D4938 = 0;
    D_002D493C = 0;
    D_002D4940 = 0;
    D_002D4944 = 0;
    D_002D4948 = 0;
    D_002D494C = 0;
    D_002D4954 = 0;
    D_002D4934 = 0;
}

void func_001B08B0(void* level) {
    if (D_002E13C0.num_nodes > 0) {
        set_current_mempool(func_00283660(D_00B3D5A0));
        D_002D492C = new NavNodeScript[D_002E13C0.num_nodes];
        D_002D4930 = D_002E13C0.num_nodes;
        memset(D_002D492C, 0, D_002D4930 * sizeof(NavNodeScript));
        set_current_mempool(NULL);
    } else {
        D_002D492C = NULL;
        D_002D4930 = 0;
    }
    func_001F9F70(level);
    func_001F99A0(level, 1);
    func_00260900(level);
    func_001AFE00();
    func_001AFF10();
}

void func_001B09D0(BOOL scale_levels) {
    Actor* actor;

    D_002D4978 = 0;
    D_002D4974 = -1;
    D_002D4980 = -1;
    if (scale_levels) {
        if (D_002D458C) {
            // Bring creatures at or below the player's level to about the player's level.
            for (actor = D_00381BB0.next; actor != &D_00381BB0; actor = actor->next) {
                if (actor->type != 6 && actor->level <= D_002D458C->level) {
                    int max_level;

                    actor->level = D_002D458C->level * random_range(0.8f, 1.2f) + 0.5f;
                    actor->level = actor->level <= 0 ? 1 : actor->level;
                    max_level = 100;
                    switch (D_002D3FAC) {
                    case 4:
                        max_level = 3;
                        break;
                    case 0x1B:
                        max_level = 14;
                        break;
                    }
                    actor->level = actor->level > (s16)max_level ? (s16)max_level : actor->level;
                    func_00120E00(actor);
                }
            }
        } else {
            for (actor = D_00381BB0.next; actor != &D_00381BB0; actor = actor->next) {
                if (actor->type != 6) {
                    actor->level = actor->stats->level;
                    func_00120E00(actor);
                }
            }
        }
    }
    func_001EBCA0();
}

// Returns TRUE if actor is inside region index.
BOOL func_001B0B90(int index, Actor* actor) {
    Region* region = &D_00A02600[index];
    vector3 position;
    vector3* corners[4];
    int i;

    if (region == NULL) {
        return FALSE;
    }
    position = actor->position;
    if (position.x < region->min.x) {
        return FALSE;
    }
    if (position.x > region->max.x) {
        return FALSE;
    }
    if (position.z < region->min.z) {
        return FALSE;
    }
    if (position.z > region->max.z) {
        return FALSE;
    }
    if (position.y + func_00123C10(actor) < region->min.y) {
        return FALSE;
    }
    if (position.y - func_00123C10(actor) > region->max.y) {
        return FALSE;
    }
    for (i = 0; i < region->num_quads; i++) {
        corners[0] = &region->quads[i][0]->position;
        corners[1] = &region->quads[i][1]->position;
        corners[2] = &region->quads[i][2]->position;
        corners[3] = &region->quads[i][3]->position;
        if (point_in_polygon_xz(position, 4, corners)) {
            return TRUE;
        }
    }
    return FALSE;
}

// Runs the camera sequence of the region the player has moved into, or gives the camera back
// to the player when they leave all regions.
void func_001B0D60(void) {
    int i;

    D_002D4980 = -1;
    for (i = 0; i < D_002D4954; i++) {
        if (D_00A02600[i].camera_sequence && func_001B0B90(i, D_002D458C)) {
            D_002D4980 = i;
        }
    }
    if (D_002D4980 != D_002D4974) {
        D_002D4974 = D_002D4980;
        if (D_002D4980 == -1) {
            if (D_003CEF80.IsFlag2()) {
                func_00134EE0(&D_003CEF80, 1, 1);
            }
            func_00133FD0(&D_003CEF80, D_002D458C);
        } else {
            func_001EBDF0(D_00A02600[D_002D4980].camera_sequence, NULL);
        }
    }
}

void func_001B0E70(void) {
    func_00260A70();
    func_001B0010();
    if (D_002D4958) {
        // A load spline was crossed; once the fade has finished, go to the new level or the world map.
        if (D_002D4968) {
            if (gr_get_fade() == 1.0f) {
                D_002D4534 &= ~0x4000;
                if (D_002D4964) {
                    func_001FEF60(0x11, 0);
                    if (D_002D4970) {
                        func_0015F640(4);
                    }
                } else {
                    func_001D9C80(D_002D4958, D_002D495C);
                    if (D_002D4970) {
                        func_0015F640(4);
                    } else {
                        func_0015F640(3);
                    }
                }
            } else if (gr_is_fading()) {
                D_002D4534 |= 0x4000;
            } else {
                D_002D4534 |= 0x4000;
                gr_fade_out(1500);
            }
        }
        return;
    }

    func_001B0D60();
    func_001EBCF0();
    if (!(D_002D4534 & 0x10) && !func_001EC8A0()) {
        func_001B02E0();
        if (D_002D3F74) {
            func_001B0420();
            func_001B05F0();
        }
    }
}

void func_001B0FE0(void) {
    func_001EC2A0();
    func_001AFFA0();
}

Actor* func_001B1010(const char* name) {
    Actor* actor;

    for (actor = D_00381BB0.next; actor != &D_00381BB0; actor = actor->next) {
        if (ngps_stricmp(name, actor->name) == 0) {
            return actor;
        }
    }
    return NULL;
}

void func_001B1080(NavpointDef* def) {
    RayCast ray;
    RayHit hit;
    float angle_x;
    float angle_z;
    float angle_y;
    int n = D_002D491C++;

    strcpy(D_009DEF10[n].name, def->name);
    def->orientation.get_rotation(angle_x, angle_z, angle_y);
    D_009DEF10[n].position = def->position;
    D_009DEF10[n].orientation = def->orientation;
    D_009DEF10[n].field_44 = 0;
    D_009DEF10[n].layer = 0;

    // Drop the navpoint onto the ground below it.
    ray.origin = def->position;
    ray.origin.y += 1.0f;
    ray.direction.x = 0.0f;
    ray.direction.y = -1000.0f;
    ray.direction.z = 0.0f;
    ray.field_24 = 0x100;
    ray.field_18 = 0;
    ray.field_1C = 0;
    ray.field_20 = 0;
    func_0010C6E0(&ray, &hit);
    if (hit.hit) {
        int layer;

        vector3 point = ray.direction * hit.t + ray.origin;
        layer = func_0010E8C0(&D_002E13C0.meshes[hit.mesh], 0);
        if (layer < 0) {
            sprintf(string_buffer,
                    "Layer for navpoint %s is invalid -- assuming no layer set exists and setting layer to 0\n",
                    def->name);
            layer = 0;
        }
        D_009DEF10[n].position = point;
        D_009DEF10[n].layer = layer;
    }
}

Navpoint* func_001B1330(const char* name) {
    int i;

    for (i = 0; i < D_002D491C; i++) {
        if (ngps_stricmp(D_009DEF10[i].name, name) == 0) {
            return &D_009DEF10[i];
        }
    }
    for (i = 0; i < D_002D4950; i++) {
        if (ngps_stricmp(D_009F0BC0[i].name, name) == 0) {
            return &D_009F0BC0[i];
        }
    }
    return NULL;
}

Navpoint* func_001B1420(const vector3* position) {
    int i;

    for (i = 0; i < D_002D491C; i++) {
        if (D_009DEF10[i].position == *position) {
            return &D_009DEF10[i];
        }
    }
    for (i = 0; i < D_002D4950; i++) {
        if (D_009F0BC0[i].position == *position) {
            return &D_009F0BC0[i];
        }
    }
    return NULL;
}

vector3 func_001B1590(ScriptTarget* target) {
    switch (target->type) {
    case 0: {
        Navpoint* navpoint = target->navpoint;

        if (navpoint) {
            return navpoint->position;
        }
        break;
    }
    case 1: {
        Actor* actor = target->actor;

        if (actor) {
            return actor->position;
        }
        break;
    }
    case 2:
        if (target->object) {
            MeshRef* ref = (MeshRef*)func_0010CCF0(target->object);

            if (ref == NULL) {
                return *(vector3*)target->object;
            } else {
                // The centre of the mesh's points.
                PointList* list = &D_002E13C0.point_lists[ref->point_list];
                vector3 result;
                float count;
                float scale;
                int i;

                result.x = 0.0f;
                result.y = 0.0f;
                result.z = 0.0f;
                for (i = 0; i < list->count; i++) {
                    result.x += list->points[i].x;
                    result.y += list->points[i].y;
                    result.z += list->points[i].z;
                }
                count = list->count;
                scale = 1.0f / count;
                result.x *= scale;
                result.y *= scale;
                result.z *= scale;
                return result;
            }
        }
        break;
    case 3:
        if (target->id != -1) {
            Actor* actor = NULL;
            ObjectListNode* node;

            for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
                if (target->id == D_003DAF60[node->type].id) {
                    actor = func_00132910(node->handle);
                    break;
                }
            }
            if (actor == NULL) {
                for (node = D_007C2820.next; node != &D_007C2820; node = node->next) {
                    if (target->id == D_003DAF60[node->type].id) {
                        actor = func_00132910(node->handle);
                        break;
                    }
                }
            }
            if (actor && actor->next && actor->prev) {
                return actor->position;
            }
        }
        break;
    }
    return vector3(0.0f, 0.0f, 0.0f);
}

// Types 0 and 2 carry the team directly; type 1 refers to an actor and type 3 to an object id
// looked up in the object lists. Returns -1 if there is no live actor.
int get_script_target_team(ScriptTarget* target) {
    switch (target->type) {
    case 0:
    case 2:
        return target->team;
    case 1:
        if (target->actor != NULL) {
            return target->actor->team;
        }
        break;
    case 3: {
        if (target->id == -1) {
            break;
        }
        Actor* actor = NULL;
        ObjectListNode* node;
        for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
            if (target->id == D_003DAF60[node->type].id) {
                actor = func_00132910(node->handle);
                break;
            }
        }
        if (actor == NULL) {
            for (node = D_007C2820.next; node != &D_007C2820; node = node->next) {
                if (target->id == D_003DAF60[node->type].id) {
                    actor = func_00132910(node->handle);
                    break;
                }
            }
        }
        if (actor != NULL && actor->next != NULL && actor->prev != NULL) {
            return actor->team;
        }
        break;
    }
    }
    return -1;
}

LevelItem* find_level_item(const char* name) {
    for (int i = 0; i < D_002D4924; i++) {
        if (ngps_strcmp(name, D_009ECCD0[i].name) == 0) {
            return &D_009ECCD0[i];
        }
    }
    return NULL;
}

void add_ambient_sound(int sound, Navpoint* source, int mask, float radius) {
    if (D_002D4928 < MAX_AMBIENT_SOUNDS) {
        AmbientSound* entry = &D_009ECED0[D_002D4928];
        entry->handle = func_001D7C50(sound, &source->position, &Zero_vector3, mask, radius);
        entry->source = source;
        entry->mask = mask;
        entry->radius = radius;
        if (entry->handle != -1) {
            D_002D4928++;
        }
    }
}

NavNodeScript* get_nav_node_script(NavNode* node) {
    int index = node - D_002E13C0.nodes;
    if (index < 0 || index >= D_002D4930) {
        return NULL;
    }
    return &D_002D492C[index];
}

Constraint* find_constraint(const char* name) {
    for (int i = 0; i < D_002D4934; i++) {
        if (ngps_stricmp(D_009ED2D0[i].name, name) == 0) {
            return &D_009ED2D0[i];
        }
    }
    return NULL;
}

ClickSpot* find_click_spot(const char* name) {
    for (int i = 0; i < D_002D4938; i++) {
        ClickSpot* spot = &D_009ED550[i];
        if (ngps_strcmp(name, spot->name) == 0) {
            return spot;
        }
    }
    return NULL;
}

// Only the first enabled sphere with this name is tested.
BOOL is_in_trigger_sphere(const char* name, Actor* actor) {
    if (actor == NULL) {
        return FALSE;
    }
    for (int i = 0; i < D_002D4944; i++) {
        int j;
        TriggerSphere* sphere = &D_009EE890[i];
        for (j = 0; j < sphere->num_disable_flags; j++) {
            if (func_001ADA90(D_009B6B50, sphere->disable_flags[j])) {
                break;
            }
        }
        if (j < sphere->num_disable_flags) {
            continue;
        }
        if (ngps_strcmp(name, sphere->name) == 0) {
            if ((actor->position - sphere->navpoint->position).length_squared() <= sphere->radius * sphere->radius) {
                return TRUE;
            }
            return FALSE;
        }
    }
    return FALSE;
}

Shopkeeper* find_shopkeeper(void* creature) {
    for (int i = 0; i < D_002D4948; i++) {
        if (D_009EEB90[i].creature == creature) {
            return &D_009EEB90[i];
        }
    }
    return NULL;
}

// If no region has this name, the last region is tested.
BOOL is_in_region(const char* name, Actor* actor) {
    int i;
    Region* region = NULL;
    for (i = 0; i < D_002D4954; i++) {
        region = &D_00A02600[i];
        if (ngps_stricmp(name, region->name) == 0) {
            break;
        }
    }
    if (region == NULL) {
        return FALSE;
    }
    vector3 position = actor->position;
    if (position.x < region->min.x) {
        return FALSE;
    }
    if (position.x > region->max.x) {
        return FALSE;
    }
    if (position.z < region->min.z) {
        return FALSE;
    }
    if (position.z > region->max.z) {
        return FALSE;
    }
    if (position.y + func_00123C10(actor) < region->min.y) {
        return FALSE;
    }
    if (position.y - func_00123C10(actor) > region->max.y) {
        return FALSE;
    }
    for (i = 0; i < region->num_quads; i++) {
        vector3* corners[4];
        corners[0] = &region->quads[i][0]->position;
        corners[1] = &region->quads[i][1]->position;
        corners[2] = &region->quads[i][2]->position;
        corners[3] = &region->quads[i][3]->position;
        if (point_in_polygon_xz(position, 4, corners)) {
            return TRUE;
        }
    }
    return FALSE;
}

// Builds a two-sided avoidance spline along the segment from a to b, shifted half a unit to one
// side. Returns the spline id from func_0025EC60, or -1.
int create_avoidance_spline(Navpoint* a, Navpoint* b) {
    RayCast ray;
    RayHit hit;
    AvoidanceSplineDef spline;
    vector3 point;
    vector3 direction;
    vector3 start;
    vector3 end;
    vector2 center;
    vector2 corner;
    u8 a_on_layer;
    u8 b_on_layer;
    int layer;
    int i;

    start = a->position;
    end = b->position;
    a_on_layer = func_001587D0(&start, 0, a->layer);
    b_on_layer = func_001587D0(&end, 0, b->layer);
    layer = -1;
    if (a_on_layer && b_on_layer) {
        if (a->layer != b->layer) {
            DEBUGERRORLINE(2131, "Avoidance splines may not cross portals.");
        }
        layer = a->layer;
    } else if (a_on_layer && !b_on_layer) {
        layer = a->layer;
    } else if (!a_on_layer && b_on_layer) {
        layer = b->layer;
    } else {
        // Neither end qualifies: cast a ray down from above the midpoint and use the mesh it hits.
        memset(&ray, 0, sizeof(ray));
        memset(&hit, 0, sizeof(hit));
        ray.origin = (a->position + b->position) * 0.5f;
        ray.origin.y += 2.0f;
        ray.direction = vector3(0.0f, -1000.0f, 0.0f);
        ray.field_20 = 0x20;
        ray.field_18 = 0;
        ray.field_1C = 0;
        ray.field_24 = 0;
        func_0010C6E0(&ray, &hit);
        if (hit.hit > 0) {
            point = ray.direction * hit.t + ray.origin;
            layer = func_0010E8D0(&D_002E13C0.meshes[hit.mesh], &point, 0);
            if (layer < 0) {
                DEBUGERRORLINE(2165, "Unable to determinate avoidance spline plane.");
            }
        } else {
            DEBUGERRORLINE(2168, "Unable to determinate avoidance spline plane.");
        }
    }

    direction = a->position - b->position;
    direction.y = 0.0f;
    direction.normalize_safe();
    vector3 side(-direction.z, 0.0f, direction.x);
    start = a->position - side * 0.5f;
    end = b->position - side * 0.5f;
    vector3 mid = (start + end) * 0.5f;

    // Outline of the segment (center to each end, in the XZ plane); computed but not used here.
    const vector2* outline_center = &center;
    const vector2* outline_corner = &corner;
    center = vector2(mid.x, mid.z);
    corner.x = start.x;
    corner.y = start.z;
    center = vector2(mid.x, mid.z);
    corner.x = end.x;
    corner.y = end.z;

    int n = 0;
    spline.starts[n].x = start.x;
    spline.starts[n].y = start.z;
    spline.layers[n] = layer;
    spline.sides[n] = 0;
    spline.ends[n].x = end.x;
    spline.ends[n].y = end.z;
    spline.layers[n] = layer;
    spline.sides[n] = 0;
    n++;

    // Repeat every segment for the other side.
    for (i = 0; i < n; i++) {
        spline.starts[n + i] = spline.starts[i];
        spline.ends[n + i] = spline.ends[i];
        spline.layers[n + i] = spline.layers[i];
        spline.sides[n + i] = 1;
    }
    spline.num_segments = n * 2;
    return func_0025EC60(&spline);
}

int destroy_avoidance_spline(int id) { return func_0025EBC0(id); }

// Copies name, shortened as needed so that name + suffix fit in 31 characters, then appends
// suffix.
void make_table_filename(char* destination, const char* name, const char* suffix) {
    u32 length = MIN(31 - strlen(suffix), strlen(name));
    strncpy(destination, name, length);
    strcpy(destination + length, suffix);
}

// Returns 1 if a party member (other than the leader) is more than 10 units from the leader, 2 if
// an alerted hostile creature is within 20 units of the party member it targets, otherwise 0.
int get_party_gather_state(void) {
    ObjectListNode* node;
    Actor* creature;

    if (func_001DA510()) {
        return 0;
    }
    for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        if (node->flags & 2) {
            continue;
        }
        creature = func_00132910(node->handle);
        if (creature == D_002D458C) {
            continue;
        }
        if (creature->flags & 0x80) {
            continue;
        }
        if ((creature->position - D_002D458C->position).length_squared() > 100.0f) {
            return 1;
        }
    }
    for (creature = D_00381BB0.next; creature != &D_00381BB0; creature = creature->next) {
        if (func_0012DEB0(D_002D458C, creature) && func_0013E1A0(creature->state, 3, -1)) {
            for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
                if (node->handle == creature->target) {
                    Actor* member = func_00132910(node->handle);
                    if ((member->position - creature->position).length_squared() <= 400.0f) {
                        return 2;
                    }
                }
            }
        }
    }
    return 0;
}

void place_party_at_start(void) {
    int index = 0;

    func_001AFE00();
    for (ObjectListNode* node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        if (!(node->flags & 2)) {
            Actor* actor = func_00132910(node->handle);
            func_00121AA0(actor, index);
            func_0014B380(actor->state);
            func_0013E9E0(actor->state);
            if (!(actor->flags & 0x80)) {
                func_0016A940(actor->model);
                func_0016A8F0(actor->model);
                func_00127830(actor);
                func_001697D0(actor->model, 0, 0.0f);
            }
            index++;
        }
    }
    func_0015DF40(0);
}
