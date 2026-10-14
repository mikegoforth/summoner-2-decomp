// Level saves. The game state is saved as an image of tagged sections laid out in one buffer
// (LevelSave), written to and read from the memory card. Also kept in the save: the named global
// flags and ints, the item and click flags, the automap and the movies seen. Ends with the console
// commands for flags, saves and level meshes.
#include "game/level_save.h"
#include "game/bmpman_ngps.h"
#include "game/console.h"
#include "game/debug.h"
#include "game/gr_ngps_texture.h"
#include "game/matrix.h"
#include "game/memcard.h"
#include "game/parse.h"
#include "game/platform_ngps.h"
#include "game/timestamp.h"
#include "macros.h"
#include "types.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------
// Other units. TODO: these belong to files not split yet; only the members used here are named.

extern "C" {
// Case-insensitive hash of a name.
u32 func_001DA1C0(const char* name);
// Index of the named level, or -1.
s32 func_001DA2A0(const char* name);
// Name of the current level.
const char* func_001DA4B0(void);
}

// Level table entry.
struct LevelInfo {
    /* 0x00 */ const char* name;
    /* 0x04 */ u8 unk_04[0x40];
};

#define NUM_LEVELS 46

extern LevelInfo D_002AD550[NUM_LEVELS];
// Index of the current level in D_002AD550.
extern int D_002D3FAC;

// Unit type table entry (0x2AC bytes).
struct UnitType {
    /* 0x000 */ s32 unk_000;
    /* 0x004 */ u8 unk_004[0x24 - 0x4];
    /* 0x024 */ s16 id;
    /* 0x026 */ u8 unk_026[0x2AC - 0x26];
};

extern UnitType D_003DAF60[];

struct ObjectAnimation {
    /* 0x00 */ u8 unk_00[0x8];
    /* 0x08 */ s16 index;
};

// Animation state of an object.
struct Animator {
    /* 0x000 */ u8 unk_000[0x1B5];
    /* 0x1B5 */ u8 unk_1B5;
    /* 0x1B6 */ u8 unk_1B6;
};

// Mode of an object, tested by func_0013E190 and func_0013E1A0.
struct ObjectMode {
    /* 0x0 */ s8 mode;
    /* 0x1 */ s8 submode;
};

struct Link {
    /* 0x0 */ Link* next;
    /* 0x4 */ Link* prev;
};

// Object in the level. Units are linked into the circular lists headed by D_00381BB0 and
// D_00382180.
struct GameObject {
    /* 0x000 */ Link link;
    /* 0x008 */ s32 type; // 6 for characters
    /* 0x00C */ u8 unk_00C[0x58 - 0xC];
    /* 0x058 */ int handle;
    /* 0x05C */ u32 unk_05C;
    /* 0x060 */ u8 unk_060[0x7C - 0x60];
    /* 0x07C */ char name[0x24];
    /* 0x0A0 */ Animator* animator;
    /* 0x0A4 */ GameObject* next;
    /* 0x0A8 */ GameObject* prev;
    /* 0x0AC */ u8 unk_0AC[0xB4 - 0xAC];
    /* 0x0B4 */ u32 flags;
    /* 0x0B8 */ u8 unk_0B8[0x4];
    /* 0x0BC */ ObjectMode mode;
    /* 0x0BE */ u8 unk_0BE[0x10C - 0xBE];
    /* 0x10C */ int target; // handle
    /* 0x110 */ u8 unk_110[0x144 - 0x110];
    /* 0x144 */ s16 unk_144;
    /* 0x146 */ u8 unk_146[0x260 - 0x146];
    /* 0x260 */ s32 unk_260;
    /* 0x264 */ u8 unk_264[0x2F4 - 0x264];
    /* 0x2F4 */ int unk_2F4;
    /* 0x2F8 */ u8 unk_2F8[0x400 - 0x2F8];
    /* 0x400 */ s16 unk_400;
    /* 0x402 */ s16 unk_402;
    /* 0x404 */ s16 unk_404;
    /* 0x406 */ s16 animation; // index of the animation playing
    /* 0x408 */ s16 unk_408;
    /* 0x40A */ s16 unk_40A;
    /* 0x40C */ u8 unk_40C[0x458 - 0x40C];
    /* 0x458 */ s16 unk_458;
    /* 0x45A */ u8 unk_45A[0x46C - 0x45A];
    /* 0x46C */ UnitType* unit_type;
};

// Heads of the circular unit lists.
extern GameObject D_00381BB0;
extern GameObject D_00382180;
// The character being played, if any.
extern GameObject* D_002D4590;
extern GameObject* D_002D458C;

extern "C" {
// Returns the object a handle refers to, or NULL.
GameObject* func_00132910(int handle);
// Looks up an animation of a unit type by name.
ObjectAnimation* func_0013A4C0(UnitType* type, const char* name);
// Index of the named animation of a unit type, or -1.
int func_0013BFC0(UnitType* type, const char* name);
// Length of an animation in seconds.
float func_0016A880(Animator* animator, int animation);
// Starts an animation; start and end times below 0 mean the animation's own. Returns the channel.
int func_0016A500(Animator* animator, int animation, int flags, float speed, float start, float end);
// Advances the animator by time.
void func_001697D0(Animator* animator, float time, bool unk);
// Sets flag 4 on the object with this handle.
void func_00116940(int handle);
BOOL func_0013E190(ObjectMode* mode);                         // mode is 4
BOOL func_0013E1A0(ObjectMode* mode, int value, int submode); // -1 matches any submode
void func_0013D540(ObjectMode* mode);
void func_0013D750(ObjectMode* mode);
BOOL func_0013A860(int id);
BOOL func_0012DEB0(GameObject* a, GameObject* b);
}

// Objects in the circular list headed by D_002E33C0, attached to a GameObject by handle.
struct LinkedObject {
    /* 0x00 */ u8 unk_00[0x58];
    /* 0x58 */ int handle;
    /* 0x5C */ u8 unk_5C[0xC];
    /* 0x68 */ LinkedObject* next;
    /* 0x6C */ u8 unk_6C[0x8];
    /* 0x74 */ int type;
    /* 0x78 */ u8 unk_78[0xC];
    /* 0x84 */ int owner;
};

extern LinkedObject D_002E33C0;

// Item table entry (0xA4 bytes).
struct Item {
    /* 0x00 */ u8 unk_00[0x68];
    /* 0x68 */ s32 unk_68;
    /* 0x6C */ u8 unk_6C[0xA4 - 0x6C];
};

extern Item D_00335A20[];
extern int D_002D4348; // number of entries in D_00335A20

struct ItemOwner {
    /* 0x0 */ s32 unk_0;
    /* 0x4 */ Item* item;
    /* 0x8 */ s32* unk_8;
};

struct ItemSlot {
    /* 0x0 */ s32 id;
    /* 0x4 */ ItemOwner* owner;
};

extern ItemSlot D_002EB910[];
extern int D_002D4344; // number of entries in D_002EB910

extern "C" {
// First and next index in table of an entry of the given category, or -1.
int func_0011A1B0(int category, ItemSlot* table, int count);
int func_0011A050(int index, int category, ItemSlot* table, int count);
void func_0011AD70(int index, u32 value, u32 kind, int a, int b);
}

// Node of the two party lists, headed by D_007C27C0 and D_007C2820.
struct PartyNode {
    /* 0x00 */ PartyNode* next;
    /* 0x04 */ PartyNode* prev;
    /* 0x08 */ s32 type; // index into D_003DAF60
    /* 0x0C */ int handle;
    /* 0x10 */ u32 flags;
    /* 0x14 */ u8 unk_14[0x8];
    /* 0x1C */ s32 unk_1C;
    /* 0x20 */ s32 unk_20;
    /* 0x24 */ s32 unk_24;
    /* 0x28 */ u8 unk_28;
    /* 0x2C */ union {
        s16 s;
        s32 i;
        Item* item;
    } value;
    /* 0x30 */ u8 unk_30[0x30];
};

extern PartyNode D_007C27C0;
extern PartyNode D_007C2820;
extern PartyNode* D_002D4588;

extern "C" {
PartyNode* func_001602B0(GameObject* unit);
void func_00160E30(PartyNode* node, GameObject* unit);
void func_00161160(PartyNode* node, int value);
BOOL func_00161ED0(void);
}

#define SAVE_GROUP_SIZE 6

// A group of units (D_00782440); func_001529D0 returns the group of an object.
struct UnitGroup {
    /* 0x00 */ s32 member_count;
    /* 0x04 */ GameObject* members[SAVE_GROUP_SIZE];
    /* 0x1C */ s32 unk_count;
    /* 0x20 */ GameObject* unk_20[17];
    /* 0x64 */ GameObject* leader;
    /* 0x68 */ s32 unk_68;
    /* 0x6C */ s32 unk_6C;
};

extern UnitGroup D_00782440[20];

extern "C" {
void func_00152760(void);
UnitGroup* func_001529D0(GameObject* object);
void func_00152AA0(UnitGroup* group, GameObject* object);
}

// A level event (D_00382BC0, in use where D_00383CF0 is set). func_00120B10 starts one.
struct LevelEvent {
    /* 0x00 */ s32 type;
    /* 0x04 */ GameTimer timer1;
    /* 0x08 */ s32 handle;
    /* 0x0C */ s32 target; // object handle, or -1
    /* 0x10 */ s32 owner;  // object handle, or -1
    /* 0x14 */ s16 model; // object type (index into D_00ABC570), read as unsigned when loading
    /* 0x18 */ GameTimer timer2;
    /* 0x1C */ s32 unk_1C;
    /* 0x20 */ f32 unk_20;
    /* 0x24 */ s16 kind;
    /* 0x26 */ s16 unk_26;
    /* 0x28 */ s32 unk_28;
};

#define SAVE_EVENT_COUNT 100

extern LevelEvent D_00382BC0[SAVE_EVENT_COUNT];
extern u8 D_00383CF0[SAVE_EVENT_COUNT];

#define SAVE_TRACKER_COUNT 2
#define SAVE_TRACKER_VALUES 13

struct Tracker {
    /* 0x00 */ u8 active;
    /* 0x04 */ s32 unit; // handle
    /* 0x08 */ s16 values[SAVE_TRACKER_VALUES];
    /* 0x24 */ s32 unk_24;
    /* 0x28 */ s32 unk_28;
};

extern Tracker D_00782380[SAVE_TRACKER_COUNT];

#define SAVE_TIMER_COUNT 20

struct TimerSlot {
    /* 0x0 */ s32 id;
    /* 0x4 */ GameTimer timer;
};

extern TimerSlot D_00335980[SAVE_TIMER_COUNT];

// Object type table: entries are 0x28 bytes, the type data pointer is the last member.
struct ObjectTypeData {
    /* 0x00 */ u8 unk_00[0x24];
    /* 0x24 */ void* effect;
};

struct ObjectTypeEntry {
    /* 0x00 */ const char* name;
    /* 0x04 */ u8 unk_04[0x20];
    /* 0x24 */ ObjectTypeData* data;
};

extern ObjectTypeEntry D_00ABC570[];

struct EffectTemplate {
    /* 0x00 */ u8 unk_00[4];
    /* 0x04 */ u16 index;
};

struct EffectResource {
    /* 0x00 */ u8 data[0x24];
};

struct EffectResourceTable {
    /* 0x00 */ EffectResource* entries;
    /* 0x04 */ s32 count;
    /* 0x08 */ u8 unk_08[8];
};

extern EffectResourceTable D_00831420;
extern s32 D_002D4004;

extern "C" {
LevelEvent* func_00120B10(GameObject* owner, s16 kind);
EffectTemplate* func_001743F0(void* effect);
int func_001732B0(EffectResource* resource, int a, int b, int c);
s32 func_001146F0(void* effect, vector3* position, matrix3* orientation, int flags);
s32 func_001148B0(void* effect, s32 a, int b, int c, int d);
void func_001147D0(s32 handle, int a, int b, int c);
s32 func_001154E0(s32 a, int b);
BOOL func_001FB930(s16 kind);
}

// Clickable object; bit 0 of its state isn't saved and is kept when loading.
struct ClickEntry {
    /* 0x00 */ const char* name;
    /* 0x04 */ u32 state;
    /* 0x08 */ u8 unk_08[0x2C - 0x8];
};

extern ClickEntry D_009ED550[];
extern s32 D_002D4938; // number of entries in D_009ED550

struct CameraState;
extern CameraState D_003CEF80;

extern "C" {
void func_00133880(CameraState* camera, void* data);
void func_001338A0(CameraState* camera, void* data);
s32 func_00263EC0(void);
void func_00263ED0(s32 value);
}

// Render mesh of the level.
struct LevelMesh {
    /* 0x00 */ vector3 position;
    /* 0x0C */ matrix3 rotation;
    /* 0x30 */ u8 unk_30[0x4A - 0x30];
    /* 0x4A */ u16 flags;
    /* 0x4C */ u8 unk_4C[0x8];
    /* 0x54 */ char* name;
    /* 0x58 */ u8 unk_58[0x8];
};

// The only mesh flag kept in a save, one bit per mesh.
#define MESH_HIDDEN 0x40

// A door moves its mesh between two placements.
struct LevelDoor {
    /* 0x00 */ s32 mesh; // index in LevelGeometry::meshes
    /* 0x04 */ u8 unk_04[0x10 - 0x4];
    /* 0x10 */ vector3 open_position;
    /* 0x1C */ quaternion open_rotation;
    /* 0x2C */ vector3 closed_position;
    /* 0x38 */ quaternion closed_rotation;
};

// The loaded level file.
struct LevelGeometry {
    /* 0x00 */ u8 unk_00[0xC];
    /* 0x0C */ s32 mesh_count;
    /* 0x10 */ LevelMesh* meshes;
    /* 0x14 */ u8 unk_14[0x34 - 0x14];
    /* 0x34 */ s32 door_count;
    /* 0x38 */ LevelDoor* doors;
};

extern LevelGeometry D_002E13C0;

extern "C" {
// Index of the named mesh, or -1.
int func_0010EA20(const char* name);
void func_00247110(void);
}

#define MAX_DOORS 64

// Run-time state of a door, parallel to LevelGeometry::doors.
struct Door {
    /* 0x00 */ GameTimer timer;
    /* 0x04 */ u8 open : 1;
    /* 0x04 */ u8 unk1 : 1;
    /* 0x04 */ u8 toggle : 1; // switch to toggle_open when loaded
    /* 0x04 */ u8 toggle_open : 1;
    /* 0x04 */ u8 timed : 1; // the timer is set to expire at once when loaded
    /* 0x04 */ u8 unk5 : 1;
    /* 0x04 */ u8 unk6 : 1;
    /* 0x04 */ u8 unk7 : 1;
    /* 0x05 */ u8 unk8 : 1;
    /* 0x05 */ u8 unk9 : 1;
    /* 0x06 */ u8 unk_06[0x58 - 0x6];
};

extern Door* D_002D492C;
extern s32 D_002D4930; // number of entries in D_002D492C

// Level effect, looked up by name.
struct LevelEffect {
    /* 0x00 */ char name[0x20];
    /* 0x20 */ s32 handle; // -1 if the effect has no object
    /* 0x24 */ s32 unk_24;
};

#define MAX_LEVEL_EFFECTS 64

extern LevelEffect D_00A2FD30[];
extern s32 D_002D4A68; // number of entries in D_00A2FD30

struct EffectAnim {
    /* 0x00 */ u8 unk_00[6];
    /* 0x06 */ s16 unk_06;
    /* 0x08 */ u8 unk_08[8];
    /* 0x10 */ s16 unk_10;
};

struct EffectInstance {
    /* 0x00 */ u8 unk_00[8];
    /* 0x08 */ f32 unk_08;
    /* 0x0C */ u8 unk_0C[8];
    /* 0x14 */ f32 unk_14;
    /* 0x18 */ f32 unk_18;
    /* 0x1C */ f32 unk_1C;
    /* 0x20 */ u8 unk_20[0x50 - 0x20];
    /* 0x50 */ EffectAnim* anim;
};

struct EffectObject {
    /* 0x00 */ u8 unk_00[0x70];
    /* 0x70 */ EffectInstance* instance;
};

extern "C" EffectObject* func_00112A70(s32 handle);

// Pickups: items lying in the level, and the containers that hold several of them.
struct DropTable {
    /* 0x0 */ u8 data[0x10];
};

extern DropTable D_009ECCD0[];

struct LevelPickup {
    /* 0x00 */ u8 unk_00[0x8];
    /* 0x08 */ s32 type; // 0 for a free slot
    /* 0x0C */ vector3 position;
};

struct Pickup : LevelPickup {
    /* 0x18 */ u8 unk_18[0x68 - 0x18];
    /* 0x68 */ ItemOwner* owner;
    /* 0x6C */ s32 unk_6C;
    /* 0x70 */ s32 unk_70;
    /* 0x74 */ u8 unk_74[4];
    /* 0x78 */ GameTimer timer;
    /* 0x7C */ u8 unk_7C[4];
};

#define PICKUP_CONTAINER 10

struct PickupContainer : LevelPickup {
    /* 0x18 */ u8 unk_18[0xA8 - 0x18];
    /* 0xA8 */ s32 count;
    /* 0xAC */ u8 unk_AC[0xB4 - 0xAC];
    /* 0xB4 */ DropTable* table;
    /* 0xB8 */ u8 unk_B8[4];
};

extern PickupContainer D_002E9BB0[40];
extern Pickup D_003C7280[250];

extern "C" {
s32 func_0011EC90(const char* name);
s32 func_0011EA10(s32 type);
Pickup* func_00131AF0(s32 type, vector3* position, matrix3* orientation, s32 unk, s32 a, s32 b);
void func_00115FE0(LevelPickup* pickup);
PickupContainer* func_00119DC0(vector3* position, f32 radius);
void func_00132290(PickupContainer* container, Pickup* pickup);
Pickup* func_001324C0(PickupContainer* container, int index);
BOOL func_00131E90(Pickup* pickup);
}

// Scripted sequences: a circular list of sequences, each with a circular list of steps.
struct SequenceStep {
    /* 0x0 */ SequenceStep* next;
};

struct Sequence {
    /* 0x00 */ Sequence* next;
    /* 0x04 */ u8 unk_04[8];
    /* 0x0C */ u32 flags;
    /* 0x10 */ u8 unk_10[8];
    /* 0x18 */ SequenceStep* steps;
    /* 0x1C */ SequenceStep* current;
};

#define SEQUENCE_RESTORED 0x2000000
#define SEQUENCE_DONE 0x8000000
#define MAX_SEQUENCE_RECORDS 100

extern Sequence* D_002D4A8C;

// Glyphs: up to five, each with its object and an optional effect.
struct LevelGlyph {
    /* 0x00 */ s32 object; // handle, or -1
    /* 0x04 */ s32 effect; // -1 if none
    /* 0x08 */ s32 unk_08;
    /* 0x0C */ vector3 position;
    /* 0x18 */ GameTimer timer;
};

#define NUM_GLYPHS 5

extern LevelGlyph D_00782200[NUM_GLYPHS];

// Type whose effect marks an active glyph.
#define OBJECT_TYPE_GLYPH 150

// Changed objects: up to twenty.
struct LevelChange {
    /* 0x0 */ s32 object; // handle, or -1 if unused
    /* 0x4 */ s16 unk_4;
    /* 0x6 */ s16 unk_6;
    /* 0x8 */ GameTimer timer;
};

#define NUM_CHANGES 20

extern LevelChange D_00782290[NUM_CHANGES];

// Splines that can be removed while playing; the save keeps one byte per spline.
struct LevelSpline {
    /* 0x00 */ u8 unk_00[4];
    /* 0x04 */ s32 unk_04;
    /* 0x08 */ s32 unk_08;
    /* 0x0C */ u8 unk_0C[4];
    /* 0x10 */ s32 handle; // -1 if the spline doesn't exist
    /* 0x14 */ u8 unk_14[0x1C - 0x14];
};

#define MAX_SPLINES 64
#define NUM_SPLINE_STATES 16

extern LevelSpline D_009EE190[];
extern s32 D_002D4940; // number of entries in D_009EE190

struct LevelSplineState {
    /* 0x00 */ u8 unk_00[0x11];
    /* 0x11 */ u8 state;
    /* 0x12 */ u8 unk_12[0x14 - 0x12];
};

extern LevelSplineState D_009EE050[];
extern s32 D_002D493C; // number of entries in D_009EE050

extern "C" {
void func_001B26C0(s32 handle);
s32 func_001B2160(s32 a, s32 b);
}

struct OptionsState {
    /* 0x00 */ u8 data[0x18];
};

extern OptionsState D_0078DC60;

extern "C" void func_002894C0(void);

// Party state.
extern u32 D_002D4534;
extern s32 D_002D4340;

extern "C" {
s32 func_0025DA70(void);
void func_0025DA60(GameObject* object);
void func_0025DA80(void);
void func_0025DA90(void);
void func_0015DD20(void);
void func_0010FED0(s16* list);
s32 func_0013A150(s32 type);
GameObject* func_0012DF20(int a, s32 type, vector3* position, matrix3* orientation, int b, int c, int d, int e);
void func_001218A0(GameObject* unit, s32 value);
}

// Movement animation names: "stand", "norm_walk", "norm_run".
extern const char* D_002A88F0[3];
extern f32 D_002D4514;
// While set, the game can always be saved, and LevelSave::Save stores only the header and the
// thumbnail.
extern u8 D_002D5850;
// The captured screen image.
extern u8 D_01890A90[];

extern "C" {
void func_00226C40(void);
void func_0011FD70(void);
void func_00293720(void);
// Called around storing the game state; both only act in level 0x23.
void func_002936E0(void);
void func_00293760(void);
void func_0015F8D0(const char* level_name, int a1, int a2);
int func_001DA710(void);
BOOL func_0022A2B0(void);
BOOL func_001DA0D0(void);
}

// ---------------------------------------------------------------------------------------------
// Records in the save buffer.

#define SAVE_THUMBNAIL_SIZE 0xCE40

// The header section's info block: the description shown when choosing a save to load.
struct SaveHeaderInfo {
    /* 0x00 */ char name[0x2C];
    /* 0x2C */ s32 unk_2C;
    /* 0x30 */ s32 unk_30;
    /* 0x34 */ f32 unk_34;
    /* 0x38 */ u32 unk_38;
    /* 0x3C */ u8 unk_3C[0x40];
    /* 0x7C */ u8 movies_seen[LEVEL_SAVE_MOVIES];
};

#define SAVE_PARTY_SLOTS 25
#define SAVE_SLOT_COUNT 1000
#define SAVE_MEMBER_COUNT 13
#define SAVE_MAX_UNITS 180

// LEPS: party state. Each unit type maps to a slot (func_001A5E30); the masks have one bit per
// slot. mask1 to mask3 sort the units of the second party list: type 6 units that are not linked
// into a unit list go to mask1 if their node has flag 0x10 set and to mask2 otherwise; everything
// else goes to mask3.
struct SaveParty {
    /* 0x00 */ u32 members; // slots of the first party list
    /* 0x04 */ u32 mask1;
    /* 0x08 */ u32 mask2;
    /* 0x0C */ u32 mask3;
    /* 0x10 */ s32 slot_values[SAVE_PARTY_SLOTS];
    /* 0x74 */ s32 member_types[SAVE_PARTY_SLOTS]; // in party order, ended by -1
    /* 0xD8 */ u8 unk_D8;
    /* 0xD9 */ u8 unk_D9;
    /* 0xDC */ s32 player; // type of the character being played, or -1
    /* 0xE0 */ s32 time;
    /* 0xE4 */ s32 leader;
    /* 0xE8 */ s32 unk_E8;
};

// LEPS: a party member.
struct SaveMember {
    /* 0x0 */ s16 type;
    /* 0x4 */ s32 unk_04;
    /* 0x8 */ u8 unk_08;
    /* 0xC */ union {
        s16 s;
        s32 i;
    } value;
};

// LEPS: an event. Times are remaining milliseconds; objects are name hashes.
struct SaveEvent {
    /* 0x00 */ s32 type;
    /* 0x04 */ s32 time1;
    /* 0x08 */ u32 target;
    /* 0x0C */ s16 model;
    /* 0x10 */ s32 time2;
    /* 0x14 */ s32 unk_14;
    /* 0x18 */ f32 unk_18;
    /* 0x1C */ s16 kind;
    /* 0x1E */ s16 unk_1E;
    /* 0x20 */ u32 owner;
    /* 0x24 */ u8 used;
};

// LEPS: a tracker.
struct SaveTracker {
    /* 0x00 */ u8 active;
    /* 0x04 */ u32 unit; // name hash
    /* 0x08 */ s16 values[SAVE_TRACKER_VALUES];
    /* 0x24 */ s32 unk_24;
    /* 0x28 */ s32 unk_28;
};

// LEPS: a timer.
struct SaveTimer {
    /* 0x0 */ s32 id;
    /* 0x4 */ s32 time; // remaining milliseconds
};

// LEPS: a group: name hashes of its units.
struct SaveGroup {
    /* 0x00 */ u32 leader;
    /* 0x04 */ s32 unk_04;
    /* 0x08 */ u8 member_count;
    /* 0x0C */ u32 members[SAVE_GROUP_SIZE];
    /* 0x24 */ u8 unk_count;
    /* 0x28 */ u32 unk_28[SAVE_GROUP_SIZE];
};

struct UnitSlot {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s16 unk8;
    /* 0xA */ s16 unkA;
    /* 0xC */ f32 unkC;
};

#define NUM_UNIT_SLOTS 10
#define UNIT_NAME_LENGTH 44
#define NUM_UNIT_STATS 13

// LEPS: a unit. Most of it is filled in by the unit itself (func_001269E0) and read back by
// func_00126E30.
struct SaveUnit {
    /* 0x000 */ UnitSlot slots[NUM_UNIT_SLOTS];
    /* 0x0A0 */ f32 x;
    /* 0x0A4 */ f32 y;
    /* 0x0A8 */ f32 z;
    /* 0x0AC */ u32 name; // hash
    /* 0x0B0 */ s32 unk0B0;
    /* 0x0B4 */ u32 flags;
    /* 0x0B8 */ s32 unk0B8;
    /* 0x0BC */ s32 unk0BC;
    /* 0x0C0 */ s32 unk0C0;
    /* 0x0C4 */ s32 unk0C4;
    /* 0x0C8 */ s32 unk0C8;
    /* 0x0CC */ s32 unk0CC;
    /* 0x0D0 */ f32 unk0D0;
    /* 0x0D4 */ f32 unk0D4;
    /* 0x0D8 */ s16 stats[NUM_UNIT_STATS];
    /* 0x0F2 */ s16 unk0F2;
    /* 0x0F4 */ s16 unk0F4;
    /* 0x0F6 */ s16 unk0F6;
    /* 0x0F8 */ s16 unk0F8;
    /* 0x0FA */ s16 unk0FA;
    /* 0x0FC */ s16 unk0FC;
    /* 0x0FE */ s16 unk0FE;
    /* 0x100 */ s16 unk100;
    /* 0x102 */ s16 unk102;
    /* 0x104 */ s8 flag0 : 1;
    /* 0x104 */ s8 flag1 : 1;
    /* 0x104 */ s8 flag2 : 1;
    /* 0x104 */ s8 flag3 : 1;
    /* 0x104 */ s8 standing : 1; // playing its "stand" animation
    /* 0x104 */ s8 flag5 : 1;
    /* 0x104 */ s8 flag6 : 1;
    /* 0x104 */ s8 flag7 : 1;
    /* 0x105 */ s8 flag8 : 1;
    /* 0x106 */ u8 unit_name[UNIT_NAME_LENGTH];
};

// LEPS: a unit as written by format version 1. Same size as version 2, so it is upgraded in place.
struct SaveUnitV1 {
    /* 0x000 */ s32 unk000;
    /* 0x004 */ s16 unk004;
    /* 0x008 */ f32 x;
    /* 0x00C */ f32 y;
    /* 0x010 */ f32 z;
    /* 0x014 */ f32 unk014;
    /* 0x018 */ s16 unk018;
    /* 0x01C */ s32 unk01C;
    /* 0x020 */ s32 unk020;
    /* 0x024 */ s32 unk024;
    /* 0x028 */ s32 unk028;
    /* 0x02C */ UnitSlot slots[NUM_UNIT_SLOTS];
    /* 0x0CC */ s8 flag0 : 1;
    /* 0x0CC */ s8 flag1 : 1;
    /* 0x0CC */ s8 flag2 : 1;
    /* 0x0CC */ s8 flag3 : 1;
    /* 0x0CC */ s8 flag4 : 1;
    /* 0x0CC */ s8 flag5 : 1;
    /* 0x0CC */ s8 flag6 : 1;
    /* 0x0CC */ s8 flag7 : 1;
    /* 0x0CD */ s8 flag8 : 1;
    /* 0x0D0 */ f32 unk0D0;
    /* 0x0D4 */ u8 unit_name[UNIT_NAME_LENGTH];
    /* 0x100 */ s16 stats[NUM_UNIT_STATS];
    /* 0x11C */ s32 unk11C;
    /* 0x120 */ s32 unk120;
    /* 0x124 */ s32 unk124;
    /* 0x128 */ s16 unk128;
    /* 0x12A */ s16 unk12A;
    /* 0x12C */ s16 unk12C;
    /* 0x12E */ s16 unk12E;
    /* 0x130 */ s8 unk130;
    /* 0x131 */ u8 unk131;
};

extern "C" {
// Fills a save record from a unit, and the unit from a record. Nonzero means failure.
int func_001269E0(GameObject* unit, SaveUnit* record);
void func_00126E30(GameObject* unit, SaveUnit* record);
}

// DOOR: the bits of a Door.
struct DoorRecord {
    /* 0x0 */ u8 open : 1;
    /* 0x0 */ u8 unk1 : 1;
    /* 0x0 */ u8 toggle : 1;
    /* 0x0 */ u8 toggle_open : 1;
    /* 0x0 */ u8 timed : 1;
    /* 0x0 */ u8 unk5 : 1;
    /* 0x0 */ u8 unk6 : 1;
    /* 0x0 */ u8 unk7 : 1;
    /* 0x1 */ u8 unk8 : 1;
    /* 0x1 */ u8 unk9 : 1;
    /* 0x2 */ u8 unk_2[2];
};

// LVFX
struct EffectRecord {
    /* 0x00 */ f32 unk00;
    /* 0x04 */ f32 unk04;
    /* 0x08 */ f32 unk08;
    /* 0x0C */ f32 unk0C;
    /* 0x10 */ char name[0x20];
};

// DROP: one record per pickup, in groups of count records. A group is either a single loose piece
// of gold or a container holding the group's pickups.
struct DropRecord {
    DropRecord() {}

    /* 0x00 */ vector3 position;
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s32 table; // index in D_009ECCD0, -1 if none
    /* 0x14 */ s32 count;
    /* 0x18 */ s32 type; // -1 for gold
    /* 0x1C */ s32 time;
};

#define MAX_DROP_RECORDS 256
#define MAX_DROP_ITEMS 19

// LSEQ: the position of a sequence in the sequence list and of its current step.
struct SequenceRecord {
    /* 0x0 */ s32 sequence;
    /* 0x4 */ s32 step;
};

// GLYF
struct GlyphRecord {
    /* 0x00 */ u32 object; // name hash
    /* 0x04 */ s32 unk04;
    /* 0x08 */ f32 x;
    /* 0x0C */ f32 y;
    /* 0x10 */ f32 z;
    /* 0x14 */ s32 time;
    /* 0x18 */ s8 active : 1;
    /* 0x18 */ s8 has_effect : 1;
};

// CHNG
struct ChangeRecord {
    /* 0x0 */ u32 object; // name hash
    /* 0x4 */ s16 unk4;   // -1 if unused
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s32 time;
};

// ---------------------------------------------------------------------------------------------

SaveVariableRef D_002A9D60[34] = {
    { "galdyr_war", -1 },
    { "mgb_reappear_done", -1 },
    { "mgb_vanish_done", -1 },
    { "pb_summon_played", -1 },
    { "ub-summon-played", -1 },
    { "indubal_summon_played", -1 },
    { "sepulchre-summon-played", -1 },
    { "super-summon-unlocked", -1 },
    { "tree_ghost_1_captured", -1 },
    { "tree_ghost_2_captured", -1 },
    { "tree_ghost_3_captured", -1 },
    { "tree_ghost_4_captured", -1 },
    { "overlook_attack", -1 },
    { "spiketrap_off", -1 },
    { "icetrap_off", -1 },
    { "firetrap_off", -1 },
    { "krobelus_boss_fight", -1 },
    { "climbing_minigame_done", -1 },
    { "made_multi_colored_gem", -1 },
    { "demo_completed", -1 },
    { "disable_skills_and_spells", -1 },
    { "disable_blood_summon", -1 },
    { "disable_eye_summon", -1 },
    { "disable_sand_summon", -1 },
    { "disable_tree_summon", -1 },
    { "disable_inventory_items", -1 },
    { "automap_disabled", -1 },
    { "automap_was_displayed", -1 },
    { "from_save_game", -1 },
    { "twilight_reached", -1 },
    { "teleport_pending", -1 },
    { "allow_teleport", -1 },
    { "teleport_complete", -1 },
    { "reserved_2", -1 },
};

SaveVariableRef D_002A9E70[20] = {
    { "summon_eye_level", -1 },
    { "summon_sand_level", -1 },
    { "summon_tree_level", -1 },
    { "summon_blood_level", -1 },
    { "soulc_redemption", -1 },
    { "soulc_soul_search", -1 },
    { "soulc_soul_vitalize", -1 },
    { "soulc_soul_haste", -1 },
    { "soulc_soul_transfer", -1 },
    { "soulc_soul_inferno", -1 },
    { "soulc_soul_omen", -1 },
    { "soulc_urath_prayer", -1 },
    { "soulc_beguile", -1 },
    { "gladiator_games_played", -1 },
    { "gladiator_game_running", -1 },
    { "shop_last_xp_count", -1 },
    { "fools_game_result", -1 },
    { "poker_game_result", -1 },
    { "shell_game_result", -1 },
    { "fools_gold", -1 },
};

const char* D_002A9F10[LEVEL_SAVE_MOVIES + 1] = {
    "m03", "m07", "m09", "m10", "m12", "m13", "m15", "m17", "m19", "m20",
    "ebd.pss", "m25", "m26", "m27", "m30", "m32", "m38", "m40", NULL,
};

LevelSave D_009B6B50;
u8 D_009DEDE0[0x40];
u8 D_009DEE20[LEVEL_SAVE_MOVIES];
s16 D_009DEE40[16];
char D_009DEE60[0x40];

int D_002D4910;
int D_002D4914;
u8 D_002D4918;

// Object type ids handled by open_flagged_doors.
#define OBJECT_TYPE_DOOR_A0 0xA0
#define OBJECT_TYPE_DOOR_A1 0xA1

// Doors whose item flag is set in the save are shown open: their "open up" animation is played to
// the end, and the objects linked to them get flag 4.
void open_flagged_doors(void) {
    GameObject* object;
    ObjectAnimation* animation;
    LinkedObject* linked;
    float length;

    for (object = D_00381BB0.next; object != &D_00381BB0; object = object->next) {
        if (object->unit_type->id == OBJECT_TYPE_DOOR_A1 || object->unit_type->id == OBJECT_TYPE_DOOR_A0) {
            if (D_009B6B50.GetItemFlag(object->name)) {
                animation = func_0013A4C0(object->unit_type, "open up");
                if (animation != NULL) {
                    length = func_0016A880(object->animator, animation->index);
                    func_0016A500(object->animator, animation->index, 1, 1.0f, -1.0f, -1.0f);
                    func_001697D0(object->animator, length, false);
                } else {
                    object->unk_05C &= ~1;
                }
                for (linked = D_002E33C0.next; linked != &D_002E33C0; linked = linked->next) {
                    if (linked->owner == object->handle) {
                        func_00116940(linked->handle);
                    }
                }
            }
        }
    }
}

int func_001A5E30(s16 type) {
    if (type < 22) {
        return type;
    }
    if (type >= 64 && type < 68) {
        return type - 42;
    }
    return -1;
}

int func_001A5E70(int index) {
    if (index < 22) {
        return (s16)index;
    }
    index += 42;
    if (index >= 64 && index < 68) {
        return (s16)index;
    }
    return -1;
}

// Searches both unit lists.
GameObject* func_001A5EB0(u32 hash) {
    GameObject* object;
    for (object = D_00381BB0.next; object != &D_00381BB0; object = object->next) {
        if (func_001DA1C0(object->name) == hash) {
            return object;
        }
    }
    for (object = D_00382180.next; object != &D_00382180; object = object->next) {
        if (func_001DA1C0(object->name) == hash) {
            return object;
        }
    }
    return NULL;
}

// Searches the objects of the second party list.
GameObject* func_001A5F70(u32 hash) {
    PartyNode* node;
    for (node = D_007C2820.next; node != &D_007C2820; node = node->next) {
        GameObject* object = func_00132910(node->handle);
        if (func_001DA1C0(object->name) == hash) {
            return object;
        }
    }
    return NULL;
}

// Restores a timer from a saved remaining time; out-of-range values use the fallback instead.
// Returns TRUE if the fallback was used.
BOOL func_001A6000(GameTimer* timer, int time, int fallback) {
    if (time >= MAX_TIME / 2 || time < -(MAX_TIME / 2)) {
        timer->SetTimeout(fallback);
        return TRUE;
    }
    timer->SetTimeout(time);
    return FALSE;
}

// Section layout helpers. A section starts at the end of the used part of the buffer (all but the
// first two on a 16-byte boundary, at least 16 bytes past the previous section) and reserves its
// blocks in order, recording the offsets of the blocks that are read and written later.
#define SECTION_BEGIN(section)                     \
    section.data = save->buffer + save->used;      \
    section.size = 0
#define SECTION_ALIGNED_BEGIN(section)             \
    save->used = (save->used + 0x1F) & ~0xF;       \
    SECTION_BEGIN(section)
#define SECTION_SKIP(section, bytes)               \
    save->used += bytes;                           \
    section.size += bytes
#define SECTION_BLOCK(section, offset, bytes)      \
    section.offset = section.size;                 \
    SECTION_SKIP(section, bytes)

// Header: tag, size, version, then the info block.
void func_001A6060(LevelSave* save) {
    SECTION_BEGIN(save->header);
    SECTION_SKIP(save->header, 4);
    SECTION_SKIP(save->header, 4);
    SECTION_BLOCK(save->header, version, 4);
    SECTION_BLOCK(save->header, info, 0x2C);
    SECTION_SKIP(save->header, 4);
    SECTION_SKIP(save->header, 4);
    SECTION_SKIP(save->header, 4);
    SECTION_SKIP(save->header, 4);
    SECTION_SKIP(save->header, 0x40);
    SECTION_SKIP(save->header, 0x12);
}

// Thumbnail: tag, size, then the image on a 16-byte boundary.
void func_001A6230(LevelSave* save) {
    int padding;
    int start = save->used;
    save->thumbnail.data = save->buffer + start;
    save->used += 4;
    save->used += 4;
    padding = save->used & 0xF;
    padding = padding == 0 ? 0 : 16 - padding;
    save->used += padding;
    save->thumbnail.image = save->used - start;
    save->used += SAVE_THUMBNAIL_SIZE;
    save->thumbnail.size = save->used - start;
}

// Globals: the named flags and ints, see func_001A74A0.
void func_001A6310(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->globals);
    SECTION_SKIP(save->globals, 4);
    SECTION_SKIP(save->globals, 4);
    SECTION_BLOCK(save->globals, flag_count, 4);
    SECTION_BLOCK(save->globals, int_count, 4);
    SECTION_BLOCK(save->globals, flag_names, 0x2000);
    SECTION_BLOCK(save->globals, int_names, 0x800);
    SECTION_BLOCK(save->globals, flag_bits, 0x100);
    SECTION_BLOCK(save->globals, int_values, 0x800);
}

void func_001A64D0(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->level);
    SECTION_SKIP(save->level, 4);
    SECTION_SKIP(save->level, 4);
    SECTION_BLOCK(save->level, party, sizeof(SaveParty));
    SECTION_BLOCK(save->level, unit_count, 4);
    SECTION_BLOCK(save->level, units, SAVE_MAX_UNITS * sizeof(SaveUnit));
    SECTION_BLOCK(save->level, groups, 4);
    SECTION_SKIP(save->level, 20 * sizeof(SaveGroup));
    SECTION_BLOCK(save->level, slots, SAVE_SLOT_COUNT * 4);
    SECTION_BLOCK(save->level, members, SAVE_MEMBER_COUNT * sizeof(SaveMember));
    SECTION_BLOCK(save->level, member_count, 4);
    SECTION_BLOCK(save->level, events, SAVE_EVENT_COUNT * sizeof(SaveEvent));
    SECTION_BLOCK(save->level, trackers, SAVE_TRACKER_COUNT * sizeof(SaveTracker));
    SECTION_BLOCK(save->level, timers, SAVE_TIMER_COUNT * sizeof(SaveTimer));
}

void func_001A6780(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->clicks);
    SECTION_SKIP(save->clicks, 4);
    SECTION_SKIP(save->clicks, 4);
    SECTION_BLOCK(save->clicks, count, 4);
    SECTION_BLOCK(save->clicks, names, 0x2E00);
    SECTION_BLOCK(save->clicks, bits, 0x2E0);
}

void func_001A68B0(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->items);
    SECTION_SKIP(save->items, 4);
    SECTION_SKIP(save->items, 4);
    SECTION_BLOCK(save->items, count, 4);
    SECTION_BLOCK(save->items, names, 0x800);
    SECTION_BLOCK(save->items, bits, 0x40);
}

void func_001A69E0(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->automap);
    SECTION_SKIP(save->automap, 4);
    SECTION_SKIP(save->automap, 4);
    SECTION_BLOCK(save->automap, levels, NUM_LEVELS * 4);
    SECTION_BLOCK(save->automap, value, 4);
}

void func_001A6AE0(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->camera);
    SECTION_SKIP(save->camera, 4);
    SECTION_SKIP(save->camera, 4);
    SECTION_BLOCK(save->camera, block, 8);
}

void func_001A6BB0(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->meshes);
    SECTION_SKIP(save->meshes, 4);
    SECTION_SKIP(save->meshes, 4);
    SECTION_BLOCK(save->meshes, block, 0xFB);
}

void func_001A6C80(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->doors);
    SECTION_SKIP(save->doors, 4);
    SECTION_SKIP(save->doors, 4);
    SECTION_BLOCK(save->doors, block, MAX_DOORS * sizeof(DoorRecord));
}

void func_001A6D50(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->effects);
    SECTION_SKIP(save->effects, 4);
    SECTION_SKIP(save->effects, 4);
    SECTION_BLOCK(save->effects, records, MAX_LEVEL_EFFECTS * sizeof(EffectRecord));
    SECTION_BLOCK(save->effects, count, 4);
}

void func_001A6E50(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->drops);
    SECTION_SKIP(save->drops, 4);
    SECTION_SKIP(save->drops, 4);
    SECTION_BLOCK(save->drops, count, 4);
    SECTION_BLOCK(save->drops, records, MAX_DROP_RECORDS * sizeof(DropRecord));
}

void func_001A6F50(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->sequences);
    SECTION_SKIP(save->sequences, 4);
    SECTION_SKIP(save->sequences, 4);
    SECTION_BLOCK(save->sequences, count, 4);
    SECTION_BLOCK(save->sequences, records, MAX_SEQUENCE_RECORDS * sizeof(SequenceRecord));
}

void func_001A7050(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->glyphs);
    SECTION_SKIP(save->glyphs, 4);
    SECTION_SKIP(save->glyphs, 4);
    SECTION_BLOCK(save->glyphs, block, NUM_GLYPHS * sizeof(GlyphRecord));
}

void func_001A7120(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->changes);
    SECTION_SKIP(save->changes, 4);
    SECTION_SKIP(save->changes, 4);
    SECTION_BLOCK(save->changes, block, NUM_CHANGES * sizeof(ChangeRecord));
}

void func_001A71F0(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->splines);
    SECTION_SKIP(save->splines, 4);
    SECTION_SKIP(save->splines, 4);
    SECTION_BLOCK(save->splines, removed, MAX_SPLINES);
    SECTION_BLOCK(save->splines, states, NUM_SPLINE_STATES);
}

void func_001A72F0(LevelSave* save) {
    SECTION_ALIGNED_BEGIN(save->options);
    SECTION_SKIP(save->options, 4);
    SECTION_SKIP(save->options, 4);
    SECTION_BLOCK(save->options, block, sizeof(OptionsState));
}

// Header tag, size and version.
void func_001A73C0(LevelSave* save) {
    u32 tag = SAVE_TAG('S', '2', 'S', 'G');
    memcpy(save->header.data, &tag, 4);
    s32 size = save->header.size;
    memcpy(save->header.data + 4, &size, 4);
    s32 version = LEVEL_SAVE_VERSION;
    memcpy(save->header.data + 8, &version, 4);
}

// Thumbnail tag and size.
void func_001A7440(LevelSave* save) {
    u32 tag = SAVE_TAG('T', 'H', 'M', 'B');
    memcpy(save->thumbnail.data, &tag, 4);
    s32 size = save->thumbnail.size;
    memcpy(save->thumbnail.data + 4, &size, 4);
}

#define NUM_FLAG_REFS 34
#define NUM_INT_REFS 20

// Fills in the globals section: the name hashes of the flags and ints listed in events.tbl,
// followed by one "<level>_load_int" int per level, then caches the indices of the flags and ints
// that game code refers to by name.
void func_001A74A0(LevelSave* save) {
    TextFile file;
    char name[64];
    int offset;
    int flag_count;
    int int_count;
    int i;
    int j;

    if (file.Open("events.tbl", 9999999)) {
        return;
    }
    ((u32*)save->globals.data)[0] = SAVE_TAG('G', 'L', 'B', 'L');
    ((u32*)save->globals.data)[1] = save->globals.size;
    flag_count = 0;
    int_count = 0;
    while (!file.Accept("#End")) {
        if (file.Accept("$Level:")) {
            file.ParseString(name, sizeof(name), '"', '"');
        } else if (file.Accept("+Flag:")) {
            file.ParseString(name, sizeof(name), '"', '"');
            *(u32*)(save->globals.data + (save->globals.flag_names + flag_count * 4)) = func_001DA1C0(name);
            flag_count++;
        } else if (file.Accept("+Int:")) {
            file.ParseString(name, sizeof(name), '"', '"');
            *(u32*)(save->globals.data + (save->globals.int_names + int_count * 4)) = func_001DA1C0(name);
            int_count++;
        }
    }
    file.Close();

    offset = save->globals.int_names + int_count * 4;
    for (i = 0; i < NUM_LEVELS; i++) {
        sprintf(name, "%s_load_int", D_002AD550[i].name);
        *(u32*)(save->globals.data + offset) = func_001DA1C0(name);
        int_count++;
        offset += 4;
    }
    *(s32*)(save->globals.data + (u32)save->globals.flag_count) = flag_count;
    *(s32*)(save->globals.data + save->globals.int_count) = int_count;

    for (j = 0; j < NUM_FLAG_REFS; j++) {
        D_002A9D60[j].index = D_009B6B50.FindGlobalFlag(D_002A9D60[j].name);
    }
    for (j = 0; j < NUM_INT_REFS; j++) {
        D_002A9E70[j].index = D_009B6B50.FindGlobalInt(D_002A9E70[j].name);
    }
    func_001AF210(0, 1);
    func_001AF210(1, 1);
    func_001AF210(2, 1);
    func_001AF210(3, 1);
}

void func_001A7790(LevelSave* save) {
    save->level.WriteHeader(SAVE_TAG('L', 'E', 'P', 'S'));
}

void func_001A77F0(LevelSave* save) {
    save->clicks.WriteHeader(SAVE_TAG('C', 'L', 'I', 'K'));
}

// Level item table: for each level, the hash of its item list name.
void func_001A7850(LevelSave* save) {
    TextFile file;
    int count;
    u32 hash;

    if (file.Open("level_items.tbl", 9999999)) {
        return;
    }
    save->items.WriteHeader(SAVE_TAG('I', 'T', 'E', 'M'));
    count = 0;
    while (!file.Accept("#End")) {
        if (file.Accept("#Level:")) {
            file.ParseString(string_buffer, 0x200, '"', '"');
        } else if (file.Accept("$Items:")) {
            file.ParseString(string_buffer, 0x200, '"', '"');
            hash = func_001DA1C0(string_buffer);
            save->items.Write(hash, save->items.names + count * 4);
            file.SkipPast("$EndItems", NULL);
            count++;
        }
    }
    file.Close();
    save->items.Write(count, save->items.count);
}

void func_001A7A10(LevelSave* save) {
    save->automap.WriteHeader(SAVE_TAG('A', 'M', 'A', 'P'));
}

void func_001A7A70(LevelSave* save) {
    save->camera.WriteHeader(SAVE_TAG('C', 'A', 'M', 'R'));
}

void func_001A7AD0(LevelSave* save) {
    save->meshes.WriteHeader(SAVE_TAG('M', 'E', 'S', 'H'));
}

void func_001A7B30(LevelSave* save) {
    save->doors.WriteHeader(SAVE_TAG('D', 'O', 'O', 'R'));
}

void func_001A7B90(LevelSave* save) {
    save->effects.WriteHeader(SAVE_TAG('L', 'V', 'F', 'X'));
}

void func_001A7BF0(LevelSave* save) {
    save->drops.WriteHeader(SAVE_TAG('D', 'R', 'O', 'P'));
}

void func_001A7C50(LevelSave* save) {
    save->sequences.WriteHeader(SAVE_TAG('L', 'S', 'E', 'Q'));
}

void func_001A7CB0(LevelSave* save) {
    save->glyphs.WriteHeader(SAVE_TAG('G', 'L', 'Y', 'F'));
}

void func_001A7D10(LevelSave* save) {
    save->changes.WriteHeader(SAVE_TAG('C', 'H', 'N', 'G'));
}

void func_001A7D70(LevelSave* save) {
    save->splines.WriteHeader(SAVE_TAG('S', 'P', 'L', 'N'));
}

void func_001A7DD0(LevelSave* save) {
    save->options.WriteHeader(SAVE_TAG('O', 'P', 'T', 'S'));
}

// Fills the header section: tag, size and version, then the description shown when choosing a
// save to load.
int func_001A7E30(LevelSave* save) {
    u32 info;
    const char* name;
    s32 value;

    save->header.WriteHeader(SAVE_TAG('S', '2', 'S', 'G'));
    save->header.WriteVersion(LEVEL_SAVE_VERSION);
    info = save->header.info;
    memset(save->header.data + info, 0, sizeof(((SaveHeaderInfo*)0)->name));
    name = func_001DA4B0();
    save->header.Write(name, strlen(func_001DA4B0()), info);
    save->header.Write(D_002D4914, info + 0x2C);
    value = func_001AF170(0x1D);
    save->header.Write(value, info + 0x30);
    save->header.Write(D_002D4514, info + 0x34);
    save->header.Write(D_002D5850, info + 0x38);
    save->header.Write(D_009DEDE0, sizeof(D_009DEDE0), info + 0x3C);
    save->header.Write(D_009DEE20, LEVEL_SAVE_MOVIES, info + 0x7C);
    return 0;
}

// Fills the thumbnail section from the captured screen image.
int func_001A7FC0(LevelSave* save) {
    save->thumbnail.WriteHeader(SAVE_TAG('T', 'H', 'M', 'B'));
    save->thumbnail.Write(D_01890A90, SAVE_THUMBNAIL_SIZE, save->thumbnail.image);
    return 0;
}

// The globals section is filled elsewhere.
int func_001A8040(LevelSave* save) { return 0; }

// Appends one SaveUnit record to the level section at *offset. Returns nonzero on failure.
int func_001A8050(LevelSave* save, int* offset, GameObject* unit, u8 flag0, u8 flag1, u8 flag2,
                             u8 flag3) {
    SaveUnit record;

    if (func_001269E0(unit, &record)) {
        return 1;
    }
    if (unit->type == 6) {
        int index = func_0013BFC0(unit->unit_type, D_002A88F0[0]);
        record.standing = (index >= 0 && unit->animation == index) ? 1 : 0;
    } else {
        record.standing = 0;
    }
    record.flag0 = flag0 ? 1 : 0;
    record.flag1 = flag1 ? 1 : 0;
    record.flag2 = flag2 ? 1 : 0;
    record.flag3 = flag3 ? 1 : 0;
    save->level.Write(&record, sizeof(record), *offset);
    *offset += sizeof(record);
    return 0;
}

// Appends the group count and a SaveGroup record for each group that has units in it.
int func_001A8240(LevelSave* save, int* offset) {
    int count_offset;
    SaveGroup group;
    int count;
    int i;
    int j;

    count = 0;
    count_offset = *offset;
    save->level.Write(0, *offset);
    *offset += 4;
    for (i = 0; i < 20; i++) {
        UnitGroup* g = &D_00782440[i];
        if (g->unk_count > 0) {
            group.leader = func_001DA1C0(g->leader->name);
            group.unk_04 = g->unk_68;
            group.member_count = 0;
            for (j = 0; j < g->member_count; j++) {
                group.members[group.member_count++] = func_001DA1C0(g->members[j]->name);
            }
            group.unk_count = 0;
            for (j = 0; j < g->unk_count; j++) {
                group.unk_28[group.unk_count++] = func_001DA1C0(g->unk_20[j]->name);
            }
            save->level.Write(&group, sizeof(group), *offset);
            *offset += sizeof(group);
            count++;
        }
    }
    save->level.Write(count, count_offset);
    return 0;
}

// Whether the party slot of the unit's type is set in mask.
static inline u8 in_party_mask(const u32& mask, GameObject* unit) {
    if (func_001A5E30(unit->unit_type->id) != -1 && (mask & (1 << func_001A5E30(unit->unit_type->id)))) {
        return 1;
    }
    return 0;
}

// Fills the level section: party state, item slots, party members, events, trackers, timers,
// one record per unit and the groups. Returns true on failure.
bool func_001A8420(LevelSave* save) {
    SaveParty party;
    SaveMember members[SAVE_MEMBER_COUNT];
    SaveEvent events[SAVE_EVENT_COUNT];
    SaveTracker trackers[SAVE_TRACKER_COUNT];
    SaveTimer timers[SAVE_TIMER_COUNT];
    int offset;
    int i;
    int j;
    int slot;
    int count;
    int n;
    PartyNode* node;
    s16 type;
    GameObject* unit;
    GameTimer* timer;

    for (i = 0; i < SAVE_PARTY_SLOTS; i++) {
        party.slot_values[i] = -1;
        party.member_types[i] = -1;
    }
    n = 0;
    party.members = 0;
    party.mask1 = 0;
    party.mask2 = 0;
    party.mask3 = 0;

    for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        type = D_003DAF60[node->type].id;
        party.member_types[n] = type;
        n++;
        slot = func_001A5E30(type);
        if (slot == -1) {
            return true;
        }
        party.members |= 1 << slot;
        party.slot_values[slot] = node->unk_1C;
    }

    for (node = D_007C2820.next; node != &D_007C2820; node = node->next) {
        type = D_003DAF60[node->type].id;
        unit = func_00132910(node->handle);
        slot = func_001A5E30(type);
        if (slot == -1) {
            return true;
        }
        if (unit->type == 6 && unit->next == NULL && unit->prev == NULL) {
            if (node->flags & 0x10) {
                party.mask1 |= 1 << slot;
            } else {
                party.mask2 |= 1 << slot;
            }
        } else {
            party.mask3 |= 1 << slot;
        }
        party.slot_values[slot] = node->unk_1C;
    }

    party.player = (s16)(D_002D4590 ? D_002D4590->unit_type->id : -1);
    party.leader = D_002D458C->unit_type->id;
    party.unk_D8 = (D_002D4534 & 0x400) ? 1 : 0;
    party.unk_D9 = (D_002D4534 & 0x800) ? 1 : 0;
    party.unk_E8 = D_002D4340;
    if (party.player != -1) {
        party.time = func_0025DA70();
    } else {
        party.time = -1;
    }

    offset = save->level.slots;
    for (i = 0; i < SAVE_SLOT_COUNT; i++) {
        save->level.Write(0, offset);
        offset += 4;
    }
    for (i = 0; i < 6; i++) {
        for (slot = func_0011A1B0(i, D_002EB910, D_002D4344); slot >= 0;
             slot = func_0011A050(slot, i, D_002EB910, D_002D4344)) {
            ItemSlot* entry = &D_002EB910[slot];
            ItemOwner* owner = entry->owner;
            int index = owner->item - D_00335A20;
            u32 value;
            offset = save->level.slots;
            offset += index * 4;
            value = entry->id;
            value |= (owner->item->unk_68 ? *owner->unk_8 : 0) << 28;
            save->level.Write(value, offset);
            offset += 4;
        }
    }

    count = 0;
    for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        members[count].type = D_003DAF60[node->type].id;
        members[count].unk_04 = node->unk_24;
        members[count].unk_08 = node->unk_28;
        if (node->unk_24 < 13) {
            members[count].value.s = node->value.s;
        } else if (node->unk_28) {
            members[count].value.i = node->value.i;
        } else {
            members[count].value.i = -1;
            for (j = 0; j < D_002D4348; j++) {
                if (&D_00335A20[j] == node->value.item) {
                    members[count].value.i = j;
                    break;
                }
            }
            if (members[count].value.i == -1) {
                return true;
            }
        }
        count++;
    }
    offset = save->level.members;
    save->level.Write(members, sizeof(members), offset);
    offset += sizeof(members);
    offset = save->level.member_count;
    save->level.Write(count, offset);
    offset += 4;

    for (i = 0; i < SAVE_EVENT_COUNT; i++) {
        if (!D_00383CF0[i]) {
            events[i].used = 0;
        } else {
            events[i].used = 1;
            events[i].type = D_00382BC0[i].type;
            events[i].model = D_00382BC0[i].model;
            events[i].unk_14 = D_00382BC0[i].unk_1C;
            events[i].unk_18 = D_00382BC0[i].unk_20;
            events[i].kind = D_00382BC0[i].kind;
            events[i].unk_1E = D_00382BC0[i].unk_26;
            timer = &D_00382BC0[i].timer1;
            if (timer->valid()) {
                events[i].time1 = timer->GetRemainingTime();
            } else {
                events[i].time1 = 10000000;
            }
            timer = &D_00382BC0[i].timer2;
            if (timer->valid()) {
                events[i].time2 = timer->GetRemainingTime();
            } else {
                events[i].time2 = 10000000;
            }
            if (D_00382BC0[i].target != -1) {
                unit = func_00132910(D_00382BC0[i].target);
                if (unit == NULL) {
                    events[i].target = 0;
                } else {
                    events[i].target = func_001DA1C0(unit->name);
                }
            } else {
                events[i].target = 0;
            }
            if (D_00382BC0[i].owner != -1) {
                unit = func_00132910(D_00382BC0[i].owner);
                if (unit == NULL) {
                    return true;
                }
                events[i].owner = func_001DA1C0(unit->name);
            } else {
                events[i].owner = 0;
            }
        }
    }
    offset = save->level.events;
    save->level.Write(events, sizeof(events), offset);
    offset += sizeof(events);

    for (int t = 0; t < SAVE_TRACKER_COUNT; t++) {
        trackers[t].active = D_00782380[t].active;
        if (trackers[t].active) {
            trackers[t].unit = func_001DA1C0(func_00132910(D_00782380[t].unit)->name);
            trackers[t].unk_24 = D_00782380[t].unk_24;
            trackers[t].unk_28 = D_00782380[t].unk_28;
            for (int k = 0; k < SAVE_TRACKER_VALUES; k++) {
                trackers[t].values[k] = D_00782380[t].values[k];
            }
        }
    }
    offset = save->level.trackers;
    save->level.Write(trackers, sizeof(trackers), offset);
    offset += sizeof(trackers);

    for (int k = 0; k < SAVE_TIMER_COUNT; k++) {
        timers[k].id = D_00335980[k].id;
        if (timers[k].id != -1) {
            timers[k].time = D_00335980[k].timer.GetRemainingTime();
        }
    }
    offset = save->level.timers;
    save->level.Write(timers, sizeof(timers), offset);
    offset += sizeof(timers);

    offset = save->level.party;
    save->level.Write(&party, sizeof(party), offset);
    offset += sizeof(party);

    i = 0;
    offset = save->level.units;
    for (unit = D_00381BB0.next; unit != &D_00381BB0; unit = unit->next) {
        if (func_001A8050(save, &offset, unit, 0, in_party_mask(party.mask3, unit), 0, 0)) {
            return true;
        }
        i++;
    }
    for (unit = D_00382180.next; unit != &D_00382180; unit = unit->next) {
        if (func_001A8050(save, &offset, unit, 0, in_party_mask(party.mask3, unit), 1, 0)) {
            return true;
        }
        i++;
    }
    for (node = D_007C2820.next; node != &D_007C2820; node = node->next) {
        u8 in_mask2;
        GameObject* u;
        u8 in_mask3;

        u = func_00132910(node->handle);
        in_mask2 = 0;
        in_mask3 = 0;
        if (func_001A5E30(u->unit_type->id) != -1 && (party.mask3 & (1 << func_001A5E30(u->unit_type->id)))) {
            continue;
        }
        if (func_001A5E30(u->unit_type->id) != -1 && (party.mask2 & (1 << func_001A5E30(u->unit_type->id)))) {
            in_mask2 = 1;
        } else {
            in_mask3 = 0;
        }
        if (in_mask2) {
            if (func_001A8050(save, &offset, u, 0, in_mask3, 0, in_mask2)) {
                return true;
            }
        } else {
            if (func_001A8050(save, &offset, u, 1, in_mask3, 0, in_mask2)) {
                return true;
            }
        }
        i++;
    }
    if (i > SAVE_MAX_UNITS) {
        return true;
    }
    offset = save->level.unit_count;
    save->level.Write(i, offset);
    offset += 4;
    offset = save->level.groups;
    return func_001A8240(save, &offset);
}

// Stores the click state of every clickable object.
int func_001A9060(LevelSave* save) {
    int i;
    ClickEntry* entry;
    for (i = 0; i < D_002D4938; i++) {
        entry = &D_009ED550[i];
        save->SetClickState(entry->name, entry->state);
    }
    return 0;
}

int func_001A90E0(LevelSave* save) { return 0; }

int func_001A90F0(LevelSave* save) {
    save->automap.WriteIntAt(save->automap.value, func_00263EC0());
    return 0;
}

int func_001A9140(LevelSave* save) {
    u8 buffer[8];
    func_00133880(&D_003CEF80, buffer);
    memcpy(save->camera.data + save->camera.block, buffer, 8);
    return 0;
}

// One bit per mesh: set when the mesh is hidden.
int func_001A9190(LevelSave* save) {
    LevelMesh* mesh = D_002E13C0.meshes;
    int i;
    for (i = 0; i < D_002E13C0.mesh_count; i++) {
        int byte = i >> 3;
        u8 mask = 1 << (i & 7);
        if (mesh->flags & MESH_HIDDEN) {
            (save->meshes.data + save->meshes.block)[byte] |= mask;
        } else {
            (save->meshes.data + save->meshes.block)[byte] &= ~(char)mask;
        }
        mesh++;
    }
    return 0;
}

// Copies ten bits of every door; fails if either table is over 64 entries or they differ.
int func_001A9240(LevelSave* save) {
    int i;
    Door* src;
    DoorRecord* dst;
    if (D_002D4930 > MAX_DOORS) {
        return 1;
    }
    if (D_002E13C0.door_count > MAX_DOORS) {
        return 1;
    }
    if (D_002E13C0.door_count != D_002D4930) {
        return 1;
    }
    dst = (DoorRecord*)(save->doors.data + save->doors.block);
    for (i = 0; i < D_002D4930; i++) {
        src = &D_002D492C[i];
        dst->open = src->open;
        dst->unk1 = src->unk1;
        dst->toggle = src->toggle;
        dst->toggle_open = src->toggle_open;
        dst->timed = src->timed;
        dst->unk5 = src->unk5;
        dst->unk6 = src->unk6;
        dst->unk7 = src->unk7;
        dst->unk8 = src->unk8;
        dst->unk9 = src->unk9;
        dst++;
    }
    return 0;
}

// Name and placement of the level effects that have an object, at most 64.
int func_001A9440(LevelSave* save) {
    int i;
    int count;
    EffectRecord* rec;
    count = 0;
    rec = (EffectRecord*)(save->effects.data + save->effects.records);
    for (i = 0; i < D_002D4A68; i++) {
        LevelEffect* effect = &D_00A2FD30[i];
        EffectObject* object;
        if (effect->handle == -1) {
            continue;
        }
        object = func_00112A70(effect->handle);
        if (object == NULL) {
            continue;
        }
        if (object->instance == NULL) {
            continue;
        }
        if (object->instance->anim->unk_06 == 0 && object->instance->anim->unk_10 == 0) {
            continue;
        }
        strcpy(rec->name, effect->name);
        count++;
        rec->unk0C = object->instance->unk_08;
        rec->unk04 = object->instance->unk_18;
        rec->unk08 = object->instance->unk_1C;
        rec->unk00 = object->instance->unk_14;
        rec++;
        if (count == MAX_LEVEL_EFFECTS) {
            return 1;
        }
    }
    save->effects.WriteIntAt(save->effects.count, count);
    return 0;
}

// Up to 256 pickup records: the pickups in every container, then the loose pickups.
int func_001A9590(LevelSave* save) {
    DropRecord recs[MAX_DROP_RECORDS];
    int i;
    int j;
    Pickup* item;
    int count;
    PickupContainer* container;
    count = 0;
    for (i = 0; i < 40; i++) {
        container = &D_002E9BB0[i];
        if (container->type != PICKUP_CONTAINER) {
            continue;
        }
        if (container->table != NULL) {
            recs[count].table = container->table - D_009ECCD0;
        } else {
            recs[count].table = -1;
        }
        for (j = 0; j < container->count; j++) {
            item = func_001324C0(container, j);
            recs[count].count = container->count;
            if (func_00131E90(item)) {
                recs[count].type = -1;
            } else {
                recs[count].type = item->owner->item - D_00335A20;
            }
            recs[count].position.x = item->position.x;
            recs[count].position.y = item->position.y;
            recs[count].position.z = item->position.z;
            recs[count].unk0C = item->unk_6C;
            recs[count].time = -1;
            count++;
            if (count == MAX_DROP_RECORDS) {
                return 1;
            }
        }
    }
    for (i = 0; i < 250; i++) {
        if (D_003C7280[i].type == 0) {
            continue;
        }
        if (!func_00131E90(&D_003C7280[i])) {
            continue;
        }
        if (D_003C7280[i].unk_70 != 0) {
            continue;
        }
        recs[count].count = 1;
        recs[count].type = -1;
        recs[count].position.x = D_003C7280[i].position.x;
        recs[count].position.y = D_003C7280[i].position.y;
        recs[count].position.z = D_003C7280[i].position.z;
        recs[count].unk0C = D_003C7280[i].unk_6C;
        recs[count].table = -1;
        recs[count].time = D_003C7280[i].timer.GetElapsedTime();
        count++;
        if (count == MAX_DROP_RECORDS) {
            return 1;
        }
    }
    save->drops.WriteAt(save->drops.records, recs, sizeof(recs));
    save->drops.WriteIntAt(save->drops.count, count);
    return 0;
}

// Position in the sequence list of every running sequence and the position of its current step.
int func_001A9870(LevelSave* save) {
    SequenceRecord records[MAX_SEQUENCE_RECORDS];
    int index;
    int count;
    Sequence* sequence;
    SequenceStep* step;
    int j;
    index = 0;
    count = 0;
    for (sequence = D_002D4A8C; sequence != NULL;) {
        if ((sequence->flags & SEQUENCE_RESTORED) && !(sequence->flags & SEQUENCE_DONE)) {
            records[count].sequence = index;
            for (step = sequence->steps, j = 0; step != NULL;) {
                if (sequence->current == step) {
                    records[count].step = j;
                }
                j++;
                step = (step->next == sequence->steps) ? NULL : step->next;
            }
            count++;
        }
        index++;
        sequence = (sequence->next == D_002D4A8C) ? NULL : sequence->next;
    }
    save->sequences.WriteAt(save->sequences.records, records, sizeof(records));
    save->sequences.WriteIntAt(save->sequences.count, count);
    return 0;
}

int func_001A9970(LevelSave* save) {
    GlyphRecord records[NUM_GLYPHS];
    int i;
    for (i = 0; i < NUM_GLYPHS; i++) {
        if (D_00782200[i].object != -1) {
            GameObject* object = func_00132910(D_00782200[i].object);
            records[i].active = 1;
            records[i].x = D_00782200[i].position.x;
            records[i].y = D_00782200[i].position.y;
            records[i].z = D_00782200[i].position.z;
            records[i].unk04 = D_00782200[i].unk_08;
            records[i].time = D_00782200[i].timer.GetRemainingTime();
            records[i].object = func_001DA1C0(object->name);
            if (D_00782200[i].effect != -1) {
                records[i].has_effect = 1;
            } else {
                records[i].has_effect = 0;
            }
        } else {
            records[i].active = 0;
        }
    }
    memcpy(save->glyphs.data + save->glyphs.block, records, sizeof(records));
    return 0;
}

int func_001A9AF0(LevelSave* save) {
    ChangeRecord records[NUM_CHANGES];
    int i;
    for (i = 0; i < NUM_CHANGES; i++) {
        if (D_00782290[i].object == -1 || D_00782290[i].unk_4 == -1) {
            records[i].object = 0;
            records[i].unk4 = -1;
        } else {
            GameObject* object = func_00132910(D_00782290[i].object);
            if (object == NULL) {
                return 1;
            }
            records[i].object = func_001DA1C0(object->name);
            records[i].unk6 = D_00782290[i].unk_6;
            records[i].time = D_00782290[i].timer.GetElapsedTime();
            records[i].unk4 = D_00782290[i].unk_4;
        }
    }
    memcpy(save->changes.data + save->changes.block, records, sizeof(records));
    return 0;
}

int func_001A9C00(LevelSave* save) {
    u8 removed[MAX_SPLINES];
    u8 states[NUM_SPLINE_STATES];
    int i;
    for (i = 0; i < D_002D4940; i++) {
        if (D_009EE190[i].handle == -1) {
            removed[i] = 1;
        } else {
            removed[i] = 0;
        }
    }
    for (i = 0; i < D_002D493C; i++) {
        states[i] = D_009EE050[i].state;
    }
    memcpy(save->splines.data + save->splines.removed, removed, sizeof(removed));
    memcpy(save->splines.data + save->splines.states, states, sizeof(states));
    return 0;
}

int func_001A9D70(LevelSave* save) {
    memcpy(save->options.data + save->options.block, &D_0078DC60, sizeof(OptionsState));
    return 0;
}

// Checks the header section and copies its info block out.
int func_001A9DB0(LevelSave* save) {
    u32 tag;
    s32 size;
    s32 offset;
    memcpy(&tag, save->header.data, 4);
    if (tag != SAVE_TAG('S', '2', 'S', 'G')) {
        return 1;
    }
    memcpy(&size, save->header.data + 4, 4);
    if (size != save->header.size) {
        return 1;
    }
    offset = save->header.info;
    strcpy(save->level_name, (char*)save->header.data + offset);
    if (func_001DA2A0(save->level_name) == -1) {
        return 1;
    }
    memcpy(&save->unk_28194, save->header.data + (offset + 0x2C), 4);
    memcpy(&save->unk_28198, save->header.data + (offset + 0x30), 4);
    memcpy(&save->unk_281A0, save->header.data + (offset + 0x34), 4);
    memcpy(&save->unk_2819C, save->header.data + (offset + 0x38), 4);
    memcpy(save->unk_281A4, save->header.data + (offset + 0x3C), 0x40);
    memcpy(save->movies_seen, save->header.data + (offset + 0x7C), LEVEL_SAVE_MOVIES);
    return 0;
}

int func_001A9F30(LevelSave* save) { return 0; }

// Checks the globals section's tag and size.
int func_001A9F40(LevelSave* save) {
    s32 offset = 0;
    u32 tag;
    s32 size;
    tag = *(u32*)(save->globals.data + offset);
    offset += 4;
    if (tag != SAVE_TAG('G', 'L', 'B', 'L')) {
        return 1;
    }
    size = *(s32*)(save->globals.data + offset);
    offset += 4;
    if (size != save->globals.size) {
        return 1;
    }
    return 0;
}

// Reads one unit record from the level section and applies it to the unit with that name.
int func_001A9F80(LevelSave* save, int* offset) {
    SaveUnit rec;
    PartyNode* node;
    GameObject* unit;
    memcpy(&rec, save->level.data + *offset, sizeof(rec));
    *offset += sizeof(rec);
    unit = NULL;
    if (D_002D4590 != NULL && rec.name == func_001DA1C0(D_002D4590->name)) {
        unit = D_002D4590;
    }
    if (unit == NULL) {
        for (unit = D_00381BB0.next; unit != &D_00381BB0; unit = unit->next) {
            if (rec.name == func_001DA1C0(unit->name)) {
                break;
            }
        }
        if (unit == &D_00381BB0) {
            return 1;
        }
    }
    if (unit->type == 6) {
        node = func_001602B0(unit);
        if (node == NULL) {
            return 1;
        }
        func_00160E30(node, unit);
        if (rec.flag0 || rec.flag3) {
            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->next = NULL;
            node->prev = NULL;
            node->prev = D_007C2820.prev;
            node->next = &D_007C2820;
            D_007C2820.prev->next = node;
            D_007C2820.prev = node;
            func_0013D750(&unit->mode);
            if (unit->link.next != NULL && unit->link.prev != NULL) {
                unit->link.prev->next = unit->link.next;
                unit->link.next->prev = unit->link.prev;
                unit->link.next = NULL;
                unit->link.prev = NULL;
            }
            if (unit->next != NULL && unit->prev != NULL) {
                unit->prev->next = unit->next;
                unit->next->prev = unit->prev;
                unit->next = NULL;
                unit->prev = NULL;
            }
            unit->flags |= 0x800000;
            if (!rec.flag3) {
                node->flags |= 0x10;
            }
            node->flags &= ~0xE0;
            if (!rec.flag1) {
                rec.flags &= ~0x80;
            }
        } else if (rec.flag1) {
            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->next = NULL;
            node->prev = NULL;
            node->prev = D_007C2820.prev;
            node->next = &D_007C2820;
            D_007C2820.prev->next = node;
            D_007C2820.prev = node;
            node->flags &= ~0xE0;
        }
    }
    func_00126E30(unit, &rec);
    if (rec.standing) {
        unit->animation = unit->unk_400;
        unit->unk_408 = unit->unk_402;
        unit->unk_40A = unit->unk_404;
    }
    return 0;
}

// Reads the group records from the level section and rebuilds each unit's group.
int func_001AA280(LevelSave* save, int* offset) {
    SaveGroup rec;
    s32 count;
    int i;
    UnitGroup* group;
    int j;
    GameObject* object;
    GameObject* unit;
    func_00152760();
    for (unit = D_00381BB0.next; unit != &D_00381BB0; unit = unit->next) {
        unit->unk_260 = 0;
    }
    memcpy(&count, save->level.data + *offset, sizeof(count));
    *offset += sizeof(count);
    for (i = 0; i < count; i++) {
        memcpy(&rec, save->level.data + *offset, sizeof(rec));
        *offset += sizeof(rec);
        object = func_001A5EB0(rec.leader);
        if (object == NULL) {
            object = func_001A5F70(rec.leader);
        }
        if (object == NULL) {
            continue;
        }
        group = func_001529D0(object);
        if (group == NULL) {
            return 1;
        }
        for (j = 0; j < rec.member_count; j++) {
            if (rec.members[j] == rec.leader) {
                break;
            }
        }
        if (j == rec.member_count) {
            if (group->member_count == 0) {
                return 1;
            }
            group->members[group->member_count] = NULL;
            group->member_count--;
        }
        group->unk_68 = rec.unk_04;
        for (j = 0; j < rec.member_count; j++) {
            object = func_001A5EB0(rec.members[j]);
            if (object == NULL) {
                object = func_001A5F70(rec.members[j]);
            }
            if (object != NULL) {
                bool found = false;
                int k;
                for (k = 0; k < group->member_count; k++) {
                    if (object == group->members[k]) {
                        found = true;
                    }
                }
                if (!found) {
                    if (group->member_count == SAVE_GROUP_SIZE) {
                        return 1;
                    }
                    group->members[group->member_count++] = object;
                }
            }
        }
        for (j = 0; j < rec.unk_count; j++) {
            object = func_001A5EB0(rec.unk_28[j]);
            if (object == NULL) {
                object = func_001A5F70(rec.unk_28[j]);
            }
            if (object != NULL) {
                func_00152AA0(group, object);
            }
        }
    }
    return 0;
}

// Reads the level section ("LEPS") and rebuilds the party, the units, the events, the trackers
// and the timers from it.
int func_001AA560(LevelSave* save) {
    SaveParty party;
    SaveMember slots[SAVE_MEMBER_COUNT];
    SaveMember kinds[SAVE_MEMBER_COUNT];
    SaveEvent events[SAVE_EVENT_COUNT];
    SaveTracker trackers[SAVE_TRACKER_COUNT];
    SaveTimer timers[SAVE_TIMER_COUNT];
    s32 offset;
    u32 tag;
    s32 count;
    s32 size;
    s32 slot_count;
    u32 word;
    s32 kind_count;
    int i;
    s32 party_offset;
    GameObject* unit;
    int k;
    int j;
    int n;
    int m;
    int e;
    int h;
    int p;
    int c;
    s32 id;
    PartyNode* node;
    bool found;
    GameObject* owner;
    LevelEvent* event;
    GameObject* target;

    offset = 0;
    memcpy(&tag, save->level.data + offset, 4);
    offset += 4;
    if (tag != SAVE_TAG('L', 'E', 'P', 'S')) {
        return 1;
    }
    memcpy(&size, save->level.data + offset, 4);
    offset += 4;
    if (size != save->level.size) {
        return 1;
    }
    party_offset = offset;
    offset = save->level.members;
    memcpy(slots, save->level.data + offset, sizeof(slots));
    offset += sizeof(slots);
    offset = save->level.member_count;
    memcpy(&slot_count, save->level.data + offset, 4);
    offset += 4;
    n = 0;
    for (m = 0; m < slot_count; m++) {
        switch (slots[m].unk_04) {
        case 0:
            D_009DEE40[n] = slots[m].value.s;
            n++;
            break;
        case -1:
            break;
        }
    }
    func_0010FED0(D_009DEE40);
    offset = party_offset;
    memcpy(&party, save->level.data + offset, sizeof(party));
    offset += sizeof(party);
    for (i = 0; i < SAVE_PARTY_SLOTS; i++) {
        id = func_001A5E70(i);
        if ((party.members & (1 << i)) || (party.mask3 & (1 << i))) {
            s32 type = func_0013A150(id);
            GameObject* created = func_0012DF20(6, type, &Zero_vector3, &identity_matrix, 1, 0, 0, 0);
            if (created == NULL) {
                return 1;
            }
            if (i < 9 || i > 21) {
                func_001218A0(created, D_003DAF60[type].unk_000);
            }
            if (i == party.player) {
                D_002D4590 = created;
            }
            func_0013D540(&created->mode);
        } else if (party.mask1 & (1 << i)) {
            s32 type = func_0013A150(id);
            GameObject* created = func_0012DF20(6, type, &Zero_vector3, &identity_matrix, 0, 0, 1, 1);
            if (created == NULL) {
                return 1;
            }
            func_001218A0(created, D_003DAF60[type].unk_000);
            func_0013D540(&created->mode);
        } else if (party.mask2 & (1 << i)) {
            s32 type = func_0013A150(id);
            GameObject* created = func_0012DF20(6, type, &Zero_vector3, &identity_matrix, 0, 0, 1, 1);
            if (created == NULL) {
                return 1;
            }
            func_001218A0(created, D_003DAF60[type].unk_000);
            func_0013D540(&created->mode);
        }
    }
    memcpy(&count, save->level.data + offset, 4);
    offset += 4;
    offset = save->level.units;
    for (i = 0; i < count; i++) {
        if (func_001A9F80(save, &offset)) {
            return 1;
        }
    }
    offset = save->level.groups;
    if (func_001AA280(save, &offset)) {
        return 1;
    }
    for (j = 0; party.member_types[j] != -1; j++) {
        for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
            if (party.member_types[j] == D_003DAF60[node->type].id) {
                node->prev->next = node->next;
                node->next->prev = node->prev;
                node->next = NULL;
                node->prev = NULL;
                node->prev = D_007C27C0.prev;
                node->next = &D_007C27C0;
                D_007C27C0.prev->next = node;
                D_007C27C0.prev = node;
                break;
            }
        }
    }
    found = false;
    for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        n = func_001A5E30(D_003DAF60[node->type].id);
        if (party.slot_values[n] == -1) {
            return 1;
        }
        node->unk_1C = party.slot_values[n];
        if (party.leader == D_003DAF60[node->type].id) {
            func_00161160(node, 1);
            found = true;
        }
    }
    if (!found) {
        return 1;
    }
    for (node = D_007C2820.next; node != &D_007C2820; node = node->next) {
        n = func_001A5E30(D_003DAF60[node->type].id);
        if (party.slot_values[n] == -1) {
            return 1;
        }
        node->unk_1C = party.slot_values[n];
    }
    for (unit = D_00381BB0.next; unit != &D_00381BB0; unit = unit->next) {
        unit->animator->unk_1B6 = unit->animator->unk_1B5;
    }
    for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        if (D_003DAF60[node->type].id >= 9 && D_003DAF60[node->type].id < 22) {
            D_002D4588 = node;
            break;
        }
    }
    if (D_002D4588 != NULL) {
        for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
            if (D_003DAF60[node->type].id == 0) {
                node->flags |= 2;
                func_0025DA60(func_00132910(node->handle));
                break;
            }
        }
        if (party.time == -1 || party.time >= MAX_TIME / 2 || party.time < -(MAX_TIME / 2)) {
            func_0025DA90();
        } else {
            func_0025DA80();
        }
    }
    if (party.unk_D8) {
        func_0015DD20();
    }
    if (party.unk_D9) {
        D_002D4534 |= 0x800;
    }
    offset = save->level.slots;
    for (i = 0; i < SAVE_SLOT_COUNT; i++) {
        u32 value;
        u32 type;
        memcpy(&word, save->level.data + offset, 4);
        offset += 4;
        value = word & 0x0FFFFFFF;
        type = word & 0xF0000000;
        if (value) {
            func_0011AD70(i, value, type >> 28, 1, 0);
        }
    }
    offset = save->level.members;
    memcpy(kinds, save->level.data + offset, sizeof(kinds));
    offset += sizeof(kinds);
    offset = save->level.member_count;
    memcpy(&kind_count, save->level.data + offset, 4);
    offset += 4;
    for (node = D_007C27C0.next; node != &D_007C27C0; node = node->next) {
        for (k = 0; k < kind_count; k++) {
            if (kinds[k].type == D_003DAF60[node->type].id) {
                node->unk_24 = kinds[k].unk_04;
                node->unk_28 = kinds[k].unk_08;
                if (node->unk_24 < 13) {
                    node->value.s = kinds[k].value.s;
                } else if (node->unk_28) {
                    node->value.i = kinds[k].value.i;
                } else {
                    node->value.item = &D_00335A20[kinds[k].value.i];
                }
            }
        }
    }
    for (owner = D_00381BB0.next; owner != &D_00381BB0; owner = owner->next) {
        if (func_001FB930(owner->unk_458)) {
            void* model = NULL;
            int entry = 0x101;
            switch (owner->unk_458) {
            case 4:
                entry = 0x58;
                break;
            case 5:
                entry = 0x59;
                break;
            case 6:
                entry = 0x5B;
                break;
            }
            if (entry != 0x101) {
                if (D_00ABC570[entry].data != NULL && D_00ABC570[entry].data->effect != NULL) {
                    model = D_00ABC570[entry].data->effect;
                }
                if (model != NULL) {
                    D_002D4004 = func_001148B0(model, owner->handle, -1, 1, 0);
                }
            }
        }
    }
    offset = save->level.events;
    memcpy(events, save->level.data + offset, sizeof(events));
    offset += sizeof(events);
    for (e = 0; e < SAVE_EVENT_COUNT; e++) {
        if (!events[e].used) {
            continue;
        }
        if (events[e].owner != 0) {
            owner = func_001A5EB0(events[e].owner);
            if (owner == NULL) {
                if (func_001A5F70(events[e].owner)) {
                    continue;
                }
                return 1;
            }
        } else {
            return 1;
        }
        target = NULL;
        if (events[e].target != 0) {
            target = func_001A5EB0(events[e].target);
            if (target == NULL) {
                if (func_001A5F70(events[e].target)) {
                    continue;
                }
                return 1;
            }
        }
        event = func_00120B10(owner, events[e].kind);
        if (event == NULL) {
            return 1;
        }
        event->handle = -1;
        event->type = events[e].type;
        event->model = events[e].model;
        event->unk_1C = events[e].unk_14;
        event->unk_20 = events[e].unk_18;
        event->kind = events[e].kind;
        event->unk_26 = events[e].unk_1E;
        func_001A6000(&event->timer1, events[e].time1, 1000);
        func_001A6000(&event->timer2, events[e].time2, 1000);
        if (event->type == 0 || event->type == 2) {
            if (event->kind == 13) {
                if ((u16)event->model != 0x101 && D_00ABC570[(u16)event->model].data != NULL &&
                    D_00ABC570[(u16)event->model].data->effect != NULL) {
                    EffectTemplate* info = func_001743F0(D_00ABC570[(u16)event->model].data->effect);
                    if (func_001732B0(&D_00831420.entries[info->index], 1, 0, 0)) {
                        return 1;
                    }
                    event->handle = func_001148B0(D_00ABC570[(u16)event->model].data->effect, event->owner, -1, 1, 0);
                    func_001147D0(event->handle, -1, 0, 1);
                }
            } else if (event->kind == 7) {
                event->handle = func_001154E0(event->owner, 8);
            }
        }
        if (target != NULL) {
            event->target = target->handle;
        } else {
            event->target = -1;
        }
    }
    offset = save->level.trackers;
    memcpy(trackers, save->level.data + offset, sizeof(trackers));
    offset += sizeof(trackers);
    for (h = 0; h < SAVE_TRACKER_COUNT; h++) {
        GameObject* object;
        D_00782380[h].active = trackers[h].active;
        if (!trackers[h].active) {
            continue;
        }
        object = func_001A5EB0(trackers[h].unit);
        if (object == NULL) {
            object = func_001A5F70(trackers[h].unit);
        }
        if (object == NULL) {
            return 1;
        }
        D_00782380[h].unit = object->handle;
        D_00782380[h].unk_24 = trackers[h].unk_24;
        D_00782380[h].unk_28 = trackers[h].unk_28;
        for (c = 0; c < SAVE_TRACKER_VALUES; c++) {
            D_00782380[h].values[c] = trackers[h].values[c];
        }
    }
    offset = save->level.timers;
    memcpy(timers, save->level.data + offset, sizeof(timers));
    offset += sizeof(timers);
    for (p = 0; p < SAVE_TIMER_COUNT; p++) {
        D_00335980[p].id = timers[p].id;
        if (timers[p].id != -1) {
            func_001A6000(&D_00335980[p].timer, timers[p].time, 1000);
        }
    }
    D_002D4340 = party.unk_E8;
    return 0;
}

int func_001AB3D0(LevelSave* save) {
    int tag;
    int size;
    int i;
    u32 kept;
    ClickEntry* entry;

    memcpy(&tag, save->clicks.data, 4);
    if (tag != SAVE_TAG('C', 'L', 'I', 'K')) {
        return 1;
    }
    memcpy(&size, save->clicks.data + 4, 4);
    if (size != save->clicks.size) {
        return 1;
    }
    for (i = 0; i < D_002D4938; i++) {
        entry = &D_009ED550[i];
        kept = entry->state & 1;
        entry->state = kept | save->GetClickState(entry->name);
    }
    return 0;
}

int func_001AB4C0(LevelSave* save) {
    int tag;
    int size;

    memcpy(&tag, save->items.data, 4);
    if (tag != SAVE_TAG('I', 'T', 'E', 'M')) {
        return 1;
    }
    memcpy(&size, save->items.data + 4, 4);
    return size != save->items.size;
}

int func_001AB530(LevelSave* save) {
    int tag;
    int size;
    s32 value;

    memcpy(&tag, save->automap.data, 4);
    if (tag != SAVE_TAG('A', 'M', 'A', 'P')) {
        return 1;
    }
    memcpy(&size, save->automap.data + 4, 4);
    if (size != save->automap.size) {
        return 1;
    }
    memcpy(&value, save->automap.data + save->automap.value, 4);
    func_00263ED0(value);
    return 0;
}

int func_001AB5D0(LevelSave* save) {
    u8 data[8];
    int tag;
    int size;

    memcpy(&tag, save->camera.data, 4);
    if (tag != SAVE_TAG('C', 'A', 'M', 'R')) {
        return 1;
    }
    memcpy(&size, save->camera.data + 4, 4);
    if (size != save->camera.size) {
        return 1;
    }
    memcpy(data, save->camera.data + save->camera.block, 8);
    func_001338A0(&D_003CEF80, data);
    return 0;
}

int func_001AB680(LevelSave* save) {
    int tag;
    int size;
    LevelMesh* mesh;
    int i;

    memcpy(&tag, save->meshes.data, 4);
    if (tag != SAVE_TAG('M', 'E', 'S', 'H')) {
        return 1;
    }
    memcpy(&size, save->meshes.data + 4, 4);
    if (size != save->meshes.size) {
        return 1;
    }
    mesh = D_002E13C0.meshes;
    for (i = 0; i < D_002E13C0.mesh_count; mesh++, i++) {
        if ((save->meshes.data + save->meshes.block)[i >> 3] & (u8)(1 << (i & 7))) {
            mesh->flags |= MESH_HIDDEN;
        } else {
            mesh->flags &= ~MESH_HIDDEN;
        }
    }
    func_00247110();
    return 0;
}

// The DOOR section has no tag check; it is only accepted if the door counts agree.
int func_001AB780(LevelSave* save) {
    int i;
    Door* door;
    DoorRecord* record;
    LevelDoor* level_door;
    LevelMesh* mesh;

    if (D_002D4930 > MAX_DOORS) {
        return 1;
    }
    if (D_002E13C0.door_count > MAX_DOORS) {
        return 1;
    }
    if (D_002E13C0.door_count != D_002D4930) {
        return 1;
    }
    record = (DoorRecord*)(save->doors.data + save->doors.block);
    for (i = 0; i < D_002D4930; record++, i++) {
        door = &D_002D492C[i];
        level_door = &D_002E13C0.doors[i];
        door->open = record->open;
        door->unk1 = record->unk1;
        door->toggle = record->toggle;
        door->toggle_open = record->toggle_open;
        door->timed = record->timed;
        door->unk5 = record->unk5;
        door->unk6 = record->unk6;
        door->unk7 = record->unk7;
        door->unk8 = record->unk8;
        door->unk9 = record->unk9;
        mesh = &D_002E13C0.meshes[level_door->mesh];
        if (door->toggle) {
            if (door->toggle_open) {
                door->open = 1;
            } else {
                door->open = 0;
            }
        }
        door->toggle = 0;
        if (door->open) {
            mesh->position = level_door->open_position;
            level_door->open_rotation.get_matrix(&mesh->rotation);
        } else {
            mesh->position = level_door->closed_position;
            level_door->closed_rotation.get_matrix(&mesh->rotation);
        }
        if (door->timed) {
            door->timer.SetTimeout(0);
        }
    }
    return 0;
}

// Applies one saved effect record to the level effect with the same name. Returns nonzero (and
// leaves a message in string_buffer) if there is no such effect.
int func_001ABAC0(LevelSave* save, EffectRecord* record) {
    int i;
    LevelEffect* effect;
    EffectObject* object;

    for (i = 0; i < D_002D4A68; i++) {
        effect = &D_00A2FD30[i];
        if (ngps_stricmp(effect->name, record->name) == 0 && effect->handle != -1) {
            object = func_00112A70(effect->handle);
            if (object != NULL && object->instance != NULL &&
                (object->instance->anim->unk_06 != 0 || object->instance->anim->unk_10 != 0)) {
                object->instance->unk_08 = record->unk0C;
                object->instance->unk_18 = record->unk04;
                object->instance->unk_1C = record->unk08;
                object->instance->unk_14 = record->unk00;
                return 0;
            }
        }
    }
    sprintf(string_buffer, "Cannot find effect '%s'\n", record->name);
    return 1;
}

int func_001ABBE0(LevelSave* save) {
    int count;
    int i;
    EffectRecord* record;

    memcpy(&count, save->effects.data + save->effects.count, 4);
    record = (EffectRecord*)(save->effects.data + save->effects.records);
    for (i = 0; i < count; record++, i++) {
        if (func_001ABAC0(save, record)) {
            return 1;
        }
    }
    return 0;
}

int func_001ABC80(LevelSave* save) {
    DropRecord records[MAX_DROP_RECORDS];
    Pickup* pickups[MAX_DROP_ITEMS];
    int count;
    int i;
    int j;
    int n;
    PickupContainer* container;
    Pickup* first;

    memcpy(records, save->drops.data + save->drops.records, sizeof(records));
    memcpy(&count, save->drops.data + save->drops.count, 4);
    for (i = 0; i < count; i += n) {
        n = records[i].count;
        if (n == 1 && records[i].type == -1) {
            pickups[0] = func_00131AF0(func_0011EC90("Gold"), &records[i].position, &identity_matrix,
                                       records[i].unk0C, -1, -1);
            func_001A6000(&pickups[0]->timer, records[i].time, 1000);
            func_00115FE0(pickups[0]);
        } else {
            container = func_00119DC0(&records[i].position, 0.0f);
            if (container == NULL) {
                return 1;
            }
            if (records[i].table != -1) {
                container->table = &D_009ECCD0[records[i].table];
            }
            for (j = 0; j < n; j++) {
                if (records[i + j].type == -1) {
                    pickups[j] = func_00131AF0(func_0011EC90("Gold"), &records[i + j].position, &identity_matrix,
                                               records[i + j].unk0C, -1, -1);
                } else {
                    pickups[j] = func_00131AF0(func_0011EA10(records[i + j].type), &records[i + j].position,
                                               &identity_matrix, records[i + j].unk0C, -1, -1);
                }
                func_00132290(container, pickups[j]);
            }
            first = func_001324C0(container, 0);
            if (first != NULL) {
                container->position = first->position;
                container->position.y += 0.2f;
            }
            func_00115FE0(container);
        }
    }
    return 0;
}

int func_001ABF10(LevelSave* save) {
    SequenceRecord records[MAX_SEQUENCE_RECORDS];
    int count;
    int i;
    bool found_sequence;
    bool found_step;
    int sequence_index;
    Sequence* sequence;
    SequenceStep* step;
    int step_index;

    memcpy(records, save->sequences.data + save->sequences.records, sizeof(records));
    memcpy(&count, save->sequences.data + save->sequences.count, 4);
    for (i = 0; i < count; i++) {
        sequence_index = 0;
        found_sequence = false;
        found_step = false;
        for (sequence = D_002D4A8C; sequence != NULL;
             sequence = (sequence->next == D_002D4A8C) ? NULL : sequence->next) {
            if (sequence_index == records[i].sequence) {
                found_sequence = true;
                step_index = 0;
                for (step = sequence->steps; step != NULL;
                     step = (step->next == sequence->steps) ? NULL : step->next) {
                    if (step_index == records[i].step) {
                        sequence->current = step;
                        found_step = true;
                        sequence->flags |= SEQUENCE_RESTORED;
                        break;
                    }
                    step_index++;
                }
                break;
            }
            sequence_index++;
        }
        if (!found_sequence) {
            return 1;
        }
        if (!found_step) {
            return 1;
        }
    }
    return 0;
}

int func_001AC050(LevelSave* save) {
    GlyphRecord records[NUM_GLYPHS];
    int i;
    GameObject* object;

    memcpy(records, save->glyphs.data + save->glyphs.block, sizeof(records));
    for (i = 0; i < NUM_GLYPHS; i++) {
        if (records[i].active) {
            object = func_001A5EB0(records[i].object);
            if (object == NULL) {
                object = func_001A5F70(records[i].object);
            }
            if (object == NULL) {
                return 1;
            }
            D_00782200[i].object = object->handle;
            D_00782200[i].position.x = records[i].x;
            D_00782200[i].position.y = records[i].y;
            D_00782200[i].position.z = records[i].z;
            D_00782200[i].unk_08 = records[i].unk04;
            func_001A6000(&D_00782200[i].timer, records[i].time, 1000);
            if (records[i].has_effect) {
                EffectTemplate* effect = func_001743F0(D_00ABC570[OBJECT_TYPE_GLYPH].data->effect);
                if (func_001732B0(&D_00831420.entries[effect->index], 1, 0, 0)) {
                    return 1;
                }
                D_00782200[i].effect = func_001146F0(D_00ABC570[OBJECT_TYPE_GLYPH].data->effect,
                                                     &D_00782200[i].position, &identity_matrix, 1);
            } else {
                D_00782200[i].effect = -1;
            }
        } else {
            D_00782200[i].object = -1;
            D_00782200[i].effect = -1;
        }
    }
    return 0;
}

int func_001AC200(LevelSave* save) {
    ChangeRecord records[NUM_CHANGES];
    int i;
    GameObject* object;

    memcpy(records, save->changes.data + save->changes.block, sizeof(records));
    for (i = 0; i < NUM_CHANGES; i++) {
        if (records[i].unk4 == -1) {
            D_00782290[i].object = -1;
            D_00782290[i].unk_4 = -1;
        } else {
            object = func_001A5EB0(records[i].object);
            if (object == NULL) {
                object = func_001A5F70(records[i].object);
            }
            if (object == NULL) {
                return 1;
            }
            D_00782290[i].unk_4 = records[i].unk4;
            D_00782290[i].unk_6 = records[i].unk6;
            func_001A6000(&D_00782290[i].timer, records[i].time, 600000);
            D_00782290[i].object = object->handle;
        }
    }
    return 0;
}

int func_001AC310(LevelSave* save) {
    u8 removed[MAX_SPLINES];
    u8 states[NUM_SPLINE_STATES];
    int i;

    memcpy(removed, save->splines.data + save->splines.removed, sizeof(removed));
    memcpy(states, save->splines.data + save->splines.states, sizeof(states));
    for (i = 0; i < D_002D4940; i++) {
        if (D_009EE190[i].handle != -1 && removed[i]) {
            func_001B26C0(D_009EE190[i].handle);
            D_009EE190[i].handle = -1;
        } else if (D_009EE190[i].handle == -1 && !removed[i]) {
            D_009EE190[i].handle = func_001B2160(D_009EE190[i].unk_04, D_009EE190[i].unk_08);
        }
    }
    for (i = 0; i < D_002D493C; i++) {
        D_009EE050[i].state = states[i];
    }
    return 0;
}

int func_001AC4D0(LevelSave* save) {
    memcpy(&D_0078DC60, save->options.data + save->options.block, sizeof(OptionsState));
    func_002894C0();
    return 0;
}

int func_001AC510(LevelSave* save, int* size, int version) {
    int tag;
    int header_size;

    if (version >= LEVEL_SAVE_VERSION) {
        return 0;
    }
    memcpy(&tag, save->header.data, 4);
    if (tag != SAVE_TAG('S', '2', 'S', 'G')) {
        return 1;
    }
    memcpy(&header_size, save->header.data + 4, 4);
    return 0;
}

int func_001AC590(LevelSave* save, int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return 0;
    }
    return ((u32*)save->globals.data)[0] != SAVE_TAG('G', 'L', 'B', 'L');
}

// Version 1 unit records are rewritten in place as version 2 records.
int func_001AC5D0(LevelSave* save, int* size, int version) {
    SaveUnitV1 old_unit;
    SaveUnit unit;
    int tag;
    int section_size;
    int count;
    int from;
    int offset;
    int unit_offset;
    int i;
    int j;

    if (version >= LEVEL_SAVE_VERSION) {
        return 0;
    }
    memcpy(&tag, save->level.data, 4);
    if (tag != SAVE_TAG('L', 'E', 'P', 'S')) {
        return 1;
    }
    memcpy(&section_size, save->level.data + 4, 4);
    for (from = save->version; from != LEVEL_SAVE_VERSION; from++) {
        switch (from) {
        case 1:
            memcpy(&count, save->level.data + save->level.unit_count, 4);
            offset = save->level.units;
            for (i = 0; i < count; i++) {
                unit_offset = offset;
                memcpy(&old_unit, save->level.data + offset, sizeof(SaveUnitV1));
                offset += sizeof(SaveUnitV1);
                unit.name = old_unit.unk000;
                unit.unk100 = old_unit.unk004;
                unit.x = old_unit.x;
                unit.y = old_unit.y;
                unit.z = old_unit.z;
                unit.unk0D4 = old_unit.unk014;
                unit.unk102 = old_unit.unk018;
                unit.unk0B0 = old_unit.unk01C;
                unit.flags = old_unit.unk020;
                unit.unk0B8 = old_unit.unk024;
                unit.unk0BC = old_unit.unk028;
                for (j = 0; j < NUM_UNIT_SLOTS; j++) {
                    unit.slots[j] = old_unit.slots[j];
                }
                unit.flag0 = old_unit.flag0;
                unit.flag1 = old_unit.flag1;
                unit.flag2 = old_unit.flag2;
                unit.flag3 = old_unit.flag3;
                unit.standing = old_unit.flag4;
                unit.flag5 = old_unit.flag5;
                unit.flag6 = old_unit.flag6;
                unit.flag7 = old_unit.flag7;
                unit.flag8 = old_unit.flag8;
                unit.unk0D0 = old_unit.unk0D0;
                unit.unk0FE = 0;
                for (j = 0; j < UNIT_NAME_LENGTH; j++) {
                    unit.unit_name[j] = old_unit.unit_name[j];
                }
                for (j = 0; j < NUM_UNIT_STATS; j++) {
                    unit.stats[j] = old_unit.stats[j];
                }
                unit.unk0C0 = old_unit.unk11C;
                unit.unk0C4 = old_unit.unk120;
                unit.unk0C8 = old_unit.unk124;
                unit.unk0F2 = old_unit.unk128;
                unit.unk0F4 = old_unit.unk12A;
                unit.unk0F6 = old_unit.unk12C;
                unit.unk0F8 = old_unit.unk12E;
                unit.unk0FA = old_unit.unk130;
                unit.unk0FC = old_unit.unk131;
                unit.unk0CC = 0;
                memcpy(save->level.data + unit_offset, &unit, sizeof(SaveUnit));
            }
            break;
        case 2:
            // Current version: nothing to do.
            break;
        }
    }
    return 0;
}

int func_001ACAA0(LevelSave* save, int* size, int version) {
    int tag;
    int section_size;

    if (version >= LEVEL_SAVE_VERSION) {
        return 0;
    }
    memcpy(&tag, save->clicks.data, 4);
    if (tag != SAVE_TAG('C', 'L', 'I', 'K')) {
        return 1;
    }
    memcpy(&section_size, save->clicks.data + 4, 4);
    return 0;
}

int func_001ACB20(LevelSave* save, int* size, int version) {
    int tag;
    int section_size;

    if (version >= LEVEL_SAVE_VERSION) {
        return 0;
    }
    memcpy(&tag, save->items.data, 4);
    if (tag != SAVE_TAG('I', 'T', 'E', 'M')) {
        return 1;
    }
    memcpy(&section_size, save->items.data + 4, 4);
    return 0;
}

int func_001ACBA0(LevelSave* save, int* size, int version) {
    int tag;
    int section_size;

    if (version >= LEVEL_SAVE_VERSION) {
        return 0;
    }
    memcpy(&tag, save->automap.data, 4);
    if (tag != SAVE_TAG('A', 'M', 'A', 'P')) {
        return 1;
    }
    memcpy(&section_size, save->automap.data + 4, 4);
    return 0;
}

#define MAX_CLICK_STATES 0xB80

// The two bits of a click state, as returned by GetClickState and taken by SetClickState. The CLIK
// section keeps them in the low bits of one byte per object.
#define CLICK_STATE_BIT0 0x40000000
#define CLICK_STATE_BIT1 0x80000000

bool LevelSave::UpgradeCAMR(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, camera.data, 4);
    if (tag != SAVE_TAG('C', 'A', 'M', 'R')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, camera.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeMESH(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, meshes.data, 4);
    if (tag != SAVE_TAG('M', 'E', 'S', 'H')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, meshes.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeDOOR(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, doors.data, 4);
    if (tag != SAVE_TAG('D', 'O', 'O', 'R')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, doors.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeLVFX(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, effects.data, 4);
    if (tag != SAVE_TAG('L', 'V', 'F', 'X')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, effects.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeDROP(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, drops.data, 4);
    if (tag != SAVE_TAG('D', 'R', 'O', 'P')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, drops.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeLSEQ(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, sequences.data, 4);
    if (tag != SAVE_TAG('L', 'S', 'E', 'Q')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, sequences.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeGLYF(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, glyphs.data, 4);
    if (tag != SAVE_TAG('G', 'L', 'Y', 'F')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, glyphs.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeCHNG(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, changes.data, 4);
    if (tag != SAVE_TAG('C', 'H', 'N', 'G')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, changes.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeSPLN(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, splines.data, 4);
    if (tag != SAVE_TAG('S', 'P', 'L', 'N')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, splines.data + 4, 4);
    return false;
}

bool LevelSave::UpgradeOPTS(int* size, int version) {
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    u32 tag;
    memcpy(&tag, options.data, 4);
    if (tag != SAVE_TAG('O', 'P', 'T', 'S')) {
        return true;
    }
    int section_size;
    memcpy(&section_size, options.data + 4, 4);
    return false;
}

bool LevelSave::Upgrade(int size) {
    int version;

    memcpy(&version, header.data + header.version, 4);
    if (version >= LEVEL_SAVE_VERSION) {
        return false;
    }
    if (func_001AC510(this, &size, version)) {
        return true;
    }
    if (func_001AC590(this, &size, version)) {
        return true;
    }
    if (func_001AC5D0(this, &size, version)) {
        return true;
    }
    if (func_001ACAA0(this, &size, version)) {
        return true;
    }
    if (func_001ACB20(this, &size, version)) {
        return true;
    }
    if (func_001ACBA0(this, &size, version)) {
        return true;
    }
    if (UpgradeCAMR(&size, version)) {
        return true;
    }
    if (UpgradeMESH(&size, version)) {
        return true;
    }
    if (UpgradeDOOR(&size, version)) {
        return true;
    }
    if (UpgradeLVFX(&size, version)) {
        return true;
    }
    if (UpgradeDROP(&size, version)) {
        return true;
    }
    if (UpgradeLSEQ(&size, version)) {
        return true;
    }
    if (UpgradeGLYF(&size, version)) {
        return true;
    }
    if (UpgradeCHNG(&size, version)) {
        return true;
    }
    if (UpgradeSPLN(&size, version)) {
        return true;
    }
    if (UpgradeOPTS(&size, version)) {
        return true;
    }
    return size != used;
}

u32 LevelSave::GetClickState(const char* name) {
    int count;
    int offset;
    int i;

    memcpy(&count, clicks.data + clicks.count, 4);
    sprintf(string_buffer, "%s%s", D_002AD550[D_002D3FAC].name, name);
    u32 hash = func_001DA1C0(string_buffer);
    offset = clicks.names;
    for (i = 0; i < count; i++) {
        u32 entry;
        memcpy(&entry, clicks.data + offset, 4);
        offset += 4;
        if (hash == entry) {
            break;
        }
    }
    if (i >= count) {
        return 0;
    }

    u32 state = 0;
    for (int bit = 0; bit < 2; bit++) {
        int index = (i << 3) + bit;
        if ((clicks.data + clicks.bits)[index >> 3] & (u8)(1 << (index & 7))) {
            switch (bit) {
            case 0:
                state |= CLICK_STATE_BIT0;
                break;
            case 1:
                state |= CLICK_STATE_BIT1;
                break;
            }
        }
    }
    return state;
}

void LevelSave::SetClickState(const char* name, u32 state) {
    int count;
    int offset;
    int i;

    memcpy(&count, clicks.data + clicks.count, 4);
    sprintf(string_buffer, "%s%s", D_002AD550[D_002D3FAC].name, name);
    u32 hash = func_001DA1C0(string_buffer);
    offset = clicks.names;
    for (i = 0; i < count; i++) {
        u32 entry;
        memcpy(&entry, clicks.data + offset, 4);
        offset += 4;
        if (hash == entry) {
            break;
        }
    }
    if (i >= count && count >= MAX_CLICK_STATES) {
        return;
    }

    if (i == count) {
        u32 new_hash = hash;
        memcpy(clicks.data + offset, &new_hash, 4);
    }
    for (int bit = 0; bit < 2; bit++) {
        int index = (i << 3) + bit;
        int byte = clicks.bits + (index >> 3);
        u8 mask = 1 << (index & 7);
        bool set;
        switch (bit) {
        case 0:
            set = (state & CLICK_STATE_BIT0) != 0;
            break;
        case 1:
            set = (state & CLICK_STATE_BIT1) != 0;
            break;
        default:
            return;
        }
        if (set) {
            clicks.data[byte] |= mask;
        } else {
            clicks.data[byte] &= ~mask;
        }
    }
    if (i >= count) {
        count++;
        int count_offset = clicks.count;
        int new_count = count;
        memcpy(clicks.data + count_offset, &new_count, 4);
    }
}

void LevelSave::Init(void* file_buffer, int file_buffer_size) {
    valid = false;
    this->file_buffer = (u8*)file_buffer;
    this->file_buffer_size = file_buffer_size;
    used = 0;
    func_001A6060(this);
    func_001A6230(this);
    func_001A6310(this);
    func_001A64D0(this);
    func_001A6780(this);
    func_001A68B0(this);
    func_001A69E0(this);
    func_001A6AE0(this);
    func_001A6BB0(this);
    func_001A6C80(this);
    func_001A6D50(this);
    func_001A6E50(this);
    func_001A6F50(this);
    func_001A7050(this);
    func_001A7120(this);
    func_001A71F0(this);
    func_001A72F0(this);
}

void LevelSave::Reset(void) {
    valid = false;
    memset(buffer, 0, LEVEL_SAVE_BUFFER_SIZE);
    func_001A73C0(this);
    func_001A7440(this);
    func_001A74A0(this);
    func_001A7790(this);
    func_001A77F0(this);
    func_001A7850(this);
    func_001A7A10(this);
    func_001A7A70(this);
    func_001A7AD0(this);
    func_001A7B30(this);
    func_001A7B90(this);
    func_001A7BF0(this);
    func_001A7C50(this);
    func_001A7CB0(this);
    func_001A7D10(this);
    func_001A7D70(this);
    func_001A7DD0(this);
}

u8* LevelSave::GetThumbnail(void) {
    return thumbnail.data + thumbnail.image;
}

int LevelSave::FindGlobalFlag(const char* name) {
    u32 hash = func_001DA1C0(name);
    int offset = globals.flag_names;
    int count = *(int*)(globals.data + globals.flag_count);
    int i;

    for (i = 0; i < count; i++) {
        u32 entry = *(u32*)(globals.data + offset);
        offset += 4;
        if (entry == hash) {
            break;
        }
    }
    if (i < count) {
        return i;
    }
    return -1;
}

bool LevelSave::GetGlobalFlag(int index) {
    if (index == -1) {
        return false;
    }
    return ((globals.data + globals.flag_bits)[index >> 3] & (u8)(1 << (index & 7))) ? true : false;
}

BOOL LevelSave::GetGlobalFlag(const char* name, int index) {
    return ((globals.data + globals.flag_bits)[index >> 3] & (u8)(1 << (index & 7))) ? TRUE : FALSE;
}

bool LevelSave::GetGlobalFlag(u32 hash, const char* name) {
    int offset = globals.flag_names;
    int count = *(int*)(globals.data + globals.flag_count);
    int i;

    for (i = 0; i < count; i++) {
        u32 entry = *(u32*)(globals.data + offset);
        offset += 4;
        if (entry == hash) {
            break;
        }
    }
    if (i >= count) {
        return false;
    }
    return ((globals.data + globals.flag_bits)[i >> 3] & (u8)(1 << (i & 7))) ? true : false;
}

bool LevelSave::GetGlobalFlag(const char* name) {
    return GetGlobalFlag(func_001DA1C0(name), name);
}

void LevelSave::SetGlobalFlag(const char* name, int index, bool value) {
    int byte = index >> 3;
    u8 mask = 1 << (index & 7);
    if (value) {
        (globals.data + globals.flag_bits)[byte] |= mask;
    } else {
        (globals.data + globals.flag_bits)[byte] &= ~mask;
    }
}

void LevelSave::SetGlobalFlag(u32 hash, bool value, const char* name) {
    int offset = globals.flag_names;
    int count = *(int*)(globals.data + globals.flag_count);
    int i;

    for (i = 0; i < count; i++) {
        u32 entry = *(u32*)(globals.data + offset);
        offset += 4;
        if (entry == hash) {
            break;
        }
    }
    if (i < count) {
        int byte = i >> 3;
        u8 mask = 1 << (i & 7);
        if (value) {
            (globals.data + globals.flag_bits)[byte] |= mask;
        } else {
            (globals.data + globals.flag_bits)[byte] &= ~mask;
        }
    }
}

void LevelSave::SetGlobalFlag(const char* name, bool value) {
    SetGlobalFlag(func_001DA1C0(name), value, name);
}

int LevelSave::FindGlobalInt(const char* name) {
    u32 hash = func_001DA1C0(name);
    int offset = globals.int_names;
    int count = *(int*)(globals.data + globals.int_count);
    int i;

    for (i = 0; i < count; i++) {
        u32 entry = *(u32*)(globals.data + offset);
        offset += 4;
        if (entry == hash) {
            break;
        }
    }
    if (i < count) {
        return i;
    }
    return 0;
}

int LevelSave::GetGlobalInt(int index) {
    if (index != -1) {
        return *(int*)(globals.data + (globals.int_values + index * 4));
    }
    return 0;
}

int LevelSave::GetGlobalInt(const char* name, int index) {
    return *(int*)(globals.data + (globals.int_values + index * 4));
}

int LevelSave::GetGlobalInt(u32 hash, const char* name) {
    int offset = globals.int_names;
    int count = *(int*)(globals.data + globals.int_count);
    int i;

    for (i = 0; i < count; i++) {
        u32 entry = *(u32*)(globals.data + offset);
        offset += 4;
        if (entry == hash) {
            break;
        }
    }
    if (i < count) {
        return *(int*)(globals.data + (globals.int_values + i * 4));
    }
    return 0;
}

int LevelSave::GetGlobalInt(const char* name) {
    return GetGlobalInt(func_001DA1C0(name), name);
}

void LevelSave::SetGlobalInt(const char* name, int index, int value) {
    *(int*)(globals.data + (globals.int_values + index * 4)) = value;
}

void LevelSave::SetGlobalInt(u32 hash, int value, const char* name) {
    int offset = globals.int_names;
    int count = *(int*)(globals.data + globals.int_count);
    int i;

    for (i = 0; i < count; i++) {
        u32 entry = *(u32*)(globals.data + offset);
        offset += 4;
        if (entry == hash) {
            break;
        }
    }
    if (i < count) {
        *(int*)(globals.data + (globals.int_values + i * 4)) = value;
    }
}

void LevelSave::SetGlobalInt(const char* name, int value) {
    SetGlobalInt(func_001DA1C0(name), value, name);
}

bool LevelSave::GetItemFlag(u32 hash) {
    int count;

    memcpy(&count, items.data + items.count, 4);
    int offset = items.names;
    int i;
    for (i = 0; i < count; i++) {
        u32 entry;
        memcpy(&entry, items.data + offset, 4);
        offset += 4;
        if (entry == hash) {
            break;
        }
    }
    if (i >= count) {
        return false;
    }
    return ((items.data + items.bits)[i >> 3] & (u8)(1 << (i & 7))) ? true : false;
}

bool LevelSave::GetItemFlag(const char* name) {
    return GetItemFlag(func_001DA1C0(name));
}

void LevelSave::SetItemFlag(u32 hash, bool value) {
    int count;

    memcpy(&count, items.data + items.count, 4);
    int offset = items.names;
    int i;
    for (i = 0; i < count; i++) {
        u32 entry;
        memcpy(&entry, items.data + offset, 4);
        offset += 4;
        if (entry == hash) {
            break;
        }
    }
    if (i < count) {
        (items.data + items.bits)[i >> 3] |= (u8)(1 << (i & 7));
    }
}

void LevelSave::SetItemFlag(const char* name, bool value) {
    SetItemFlag(func_001DA1C0(name), value);
}

u32 LevelSave::GetAutomap(int level) {
    u32 value;
    memcpy(&value, automap.data + (automap.levels + level * 4), 4);
    return value;
}

void LevelSave::SetAutomapBit(int level, int bit) {
    if (bit < 0 || bit >= 32) {
        return;
    }
    u32 value;
    memcpy(&value, automap.data + (automap.levels + level * 4), 4);
    int offset = automap.levels + level * 4;
    value |= 1 << bit;
    u32 new_value = value;
    memcpy(automap.data + offset, &new_value, 4);
}

bool LevelSave::Save(const char* name, int port) {
    valid = false;
    if (func_001A7E30(this)) {
        return true;
    }
    if (func_001A7FC0(this)) {
        return true;
    }
    func_002936E0();
    if (!D_002D5850) {
        if (func_001A8040(this)) {
            return true;
        }
        if (func_001A8420(this)) {
            return true;
        }
        if (func_001A9060(this)) {
            return true;
        }
        if (func_001A90E0(this)) {
            return true;
        }
        if (func_001A90F0(this)) {
            return true;
        }
        if (func_001A9140(this)) {
            return true;
        }
        if (func_001A9190(this)) {
            return true;
        }
        if (func_001A9240(this)) {
            return true;
        }
        if (func_001A9440(this)) {
            return true;
        }
        if (func_001A9590(this)) {
            return true;
        }
        if (func_001A9870(this)) {
            return true;
        }
        if (func_001A9970(this)) {
            return true;
        }
        if (func_001A9AF0(this)) {
            return true;
        }
        if (func_001A9C00(this)) {
            return true;
        }
        if (func_001A9D70(this)) {
            return true;
        }
    }
    func_00293760();

    if (strlen(name) >= 32) {
        return true;
    }
    if (!memcard_create_save(port)) {
        return true;
    }
    memcard_delete(port, name, FALSE);
    int fd = memcard_open(port, name, MEMCARD_CREAT, FALSE);
    if (fd == -1) {
        return true;
    }
    memcard_close(fd);
    fd = memcard_open(port, name, MEMCARD_WRONLY, FALSE);
    if (fd == -1) {
        return true;
    }
    BOOL written = memcard_write(buffer, used, 1, fd);
    memcard_close(fd);
    return !written;
}

// Reads the header section of a save file into the buffer and unpacks its info block.
// Returns 0 on success, 1 on failure.
int func_001AE4D0(LevelSave* save, const char* name, int port) {
    int fd;
    sceMcTblGetDir entry;
    int version;
    int size;
    BOOL ok;
    u32 info;

    save->valid = false;
    fd = memcard_open(port, name, MEMCARD_RDONLY, 0);
    if (fd == -1) {
        return 1;
    }
    if (!memcard_get_file_info(&entry, port, name, 0)) {
        memcard_close(fd);
        return 1;
    }
    size = save->header.size;
    if ((u32)size > LEVEL_SAVE_BUFFER_SIZE) {
        memcard_close(fd);
        return 1;
    }
    ok = memcard_read(save->buffer, size, 1, fd);
    memcard_close(fd);
    if (!ok) {
        return 1;
    }
    save->sec = entry._Modify.Sec;
    save->min = entry._Modify.Min;
    save->hour = entry._Modify.Hour;
    save->day = entry._Modify.Day;
    save->month = entry._Modify.Month;
    save->year = entry._Modify.Year;
    memcpy(&version, save->header.data + save->header.version, sizeof(version));
    if (version < LEVEL_SAVE_VERSION) {
        if (func_001AC510(save, &size, version)) {
            return 1;
        }
    } else if (version > LEVEL_SAVE_VERSION) {
        DEBUGERRORLINE(0x18C4, "Savegame file is newer than executable.\nGet new code and rebuild the executable.");
        return 1;
    }
    save->version = version;
    info = save->header.info;
    strcpy(save->level_name, (char*)save->header.data + info);
    memcpy(&save->unk_28194, &save->header.data[info + 0x2C], 4);
    memcpy(&save->unk_28198, &save->header.data[info + 0x30], 4);
    memcpy(&save->unk_281A0, &save->header.data[info + 0x34], 4);
    memcpy(&save->unk_2819C, &save->header.data[info + 0x38], 4);
    memcpy(save->unk_281A4, &save->header.data[info + 0x3C], sizeof(save->unk_281A4));
    memcpy(save->movies_seen, &save->header.data[info + 0x7C], sizeof(save->movies_seen));
    save->valid = true;
    return 0;
}

// Reads just the header and thumbnail sections of a save file and refreshes the thumbnail image.
void func_001AE7A0(LevelSave* save, const char* name, int port) {
    int fd = memcard_open(port, name, MEMCARD_RDONLY, 0);
    if (fd != -1) {
        memcard_read(save->buffer, save->header.size + save->thumbnail.size, 1, fd);
        memcard_close(fd);
        gr_ngps_tcache_unload(D_002D4910);
    }
}

// Reads a whole save file into file_buffer, to be unpacked by func_001AE9D0.
// Returns 0 on success, 1 on failure.
int func_001AE820(LevelSave* save, const char* name, int port) {
    int result = 0;
    int fd;
    sceMcTblGetDir entry;

    strcpy(save->filename, name);
    fd = memcard_open(port, save->filename, MEMCARD_RDONLY, 0);
    if (fd == -1) {
        result = 1;
    } else if (!memcard_get_file_info(&entry, port, save->filename, 0)) {
        memcard_close(fd);
        result = 1;
    } else {
        save->file_size = entry.FileSizeByte;
        if (save->file_size > LEVEL_SAVE_BUFFER_SIZE) {
            memcard_close(fd);
            result = 1;
        } else {
            BOOL ok = memcard_read(save->file_buffer, save->file_size, 1, fd);
            memcard_close(fd);
            if (!ok) {
                result = 1;
            } else {
                save->min = entry._Modify.Min;
                save->hour = entry._Modify.Hour;
                save->day = entry._Modify.Day;
                save->month = entry._Modify.Month;
                save->year = entry._Modify.Year;
            }
        }
    }
    return result;
}

// True while a file read by func_001AE820 is waiting to be unpacked.
BOOL func_001AE9A0(LevelSave* save) {
    return strlen(save->filename) != 0;
}

// Copies the file read by func_001AE820 into the buffer and restores the game from every section.
// Returns 0 on success, 1 on failure.
int func_001AE9D0(LevelSave* save) {
    int result = 0;
    int version;

    save->valid = false;
    memcpy(save->buffer, save->file_buffer, LEVEL_SAVE_BUFFER_SIZE);
    memcpy(&version, save->header.data + save->header.version, sizeof(version));
    if (version < LEVEL_SAVE_VERSION) {
        if (save->Upgrade(save->file_size)) {
            result = 1;
            goto done;
        }
    } else if (version > LEVEL_SAVE_VERSION) {
        DEBUGERRORLINE(0x1A02, "Savegame file is newer than executable.\nGet new code and rebuild the executable.");
        result = 1;
        goto done;
    }
    if (func_001A9DB0(save)) {
        result = 1;
    } else if (func_001A9F30(save)) {
        result = 1;
    } else if (save->unk_2819C) {
        result = 1;
    } else if (func_001A9F40(save)) {
        result = 1;
    } else if (func_001AA560(save)) {
        result = 1;
    } else if (func_001AB3D0(save)) {
        result = 1;
    } else if (func_001AB4C0(save)) {
        result = 1;
    } else if (func_001AB530(save)) {
        result = 1;
    } else if (func_001AB5D0(save)) {
        result = 1;
    } else if (func_001AB680(save)) {
        result = 1;
    } else if (func_001AB780(save)) {
        result = 1;
    } else if (func_001ABBE0(save)) {
        result = 1;
    } else if (func_001ABC80(save)) {
        result = 1;
    } else if (func_001ABF10(save)) {
        result = 1;
    } else if (func_001AC050(save)) {
        result = 1;
    } else if (func_001AC200(save)) {
        result = 1;
    } else if (func_001AC310(save)) {
        result = 1;
    } else if (func_001AC4D0(save)) {
        result = 1;
    } else {
        func_00226C40();
        open_flagged_doors();
        func_0011FD70();
        func_00293720();
        save->valid = true;
    }
done:
    save->filename[0] = '\0';
    return result;
}

// Sets up the save image (file_buffer receives whole save files) and makes the thumbnail image.
void func_001AECE0(u8* file_buffer, u32 file_buffer_size) {
    D_009B6B50.Init(file_buffer, file_buffer_size);
    D_009B6B50.Reset();
    D_002D4910 = bm_create(BM_SOURCE_1555, 176, 150, D_009B6B50.GetThumbnail(), NULL, BM_FORMAT_CREATED_16, false);
}

void func_001AED50(void) {
    D_002D4918 = 0;
}

void func_001AED60(void) {
    D_009B6B50.Reset();
}

// Returns SAVE_ALLOWED, or the reason the game can't be saved right now.
int func_001AED70(void) {
    if (D_002D5850) {
        return SAVE_ALLOWED;
    }
    if (func_001DA710() != -1) {
        return SAVE_IN_MINIGAME;
    }
    if (D_002D4918) {
        return SAVE_INHIBITED_BY_SCRIPT;
    }
    if (D_002D4590) {
        if (func_0025DA70() < 500) {
            return SAVE_SUMMON_TRANSITION;
        }
        if (func_0013E1A0(&D_002D4590->mode, 7, -1)) {
            return SAVE_SUMMON_TRANSITION;
        }
        if (D_002D4590->flags & 0x20000) {
            return SAVE_SUMMON_TRANSITION;
        }
    } else {
        PartyNode* member;
        for (member = D_007C27C0.next; member != &D_007C27C0; member = member->next) {
            if (!(member->flags & 2) && D_003DAF60[member->type].id == 0) {
                GameObject* object = func_00132910(member->handle);
                if (func_0013E190(&object->mode) && func_0013A860(object->unk_144)) {
                    return SAVE_SUMMON_TRANSITION;
                }
                break;
            }
        }
    }

    GameObject* object;
    for (object = D_00381BB0.next; object != &D_00381BB0; object = object->next) {
        if (func_0013E190(&object->mode)) {
            if (object->type == 6) {
                return SAVE_SPELLCASTING;
            }
            GameObject* target = func_00132910(object->target);
            if (target && target->type == 6) {
                return SAVE_SPELLCASTING;
            }
        }
    }

    if (!func_00161ED0()) {
        return SAVE_NO_CLEAR_REASON;
    }

    LinkedObject* linked;
    for (linked = D_002E33C0.next; linked != &D_002E33C0; linked = linked->next) {
        if (linked->type == 0x13) {
            return SAVE_EFFECT_PLAYING;
        }
    }

    if (func_0022A2B0()) {
        return SAVE_NO_CLEAR_REASON;
    }

    PartyNode* member;
    for (member = D_007C27C0.next; member != &D_007C27C0; member = member->next) {
        GameObject* enemy;
        GameObject* character = func_00132910(member->handle);
        if (!(character->flags & 0x80)) {
            if (character->unk_2F4 == 5) {
                return SAVE_FLINCHING;
            }
            for (enemy = D_00381BB0.next; enemy != &D_00381BB0; enemy = enemy->next) {
                if (enemy != character && func_0012DEB0(character, enemy) && func_0013E1A0(&enemy->mode, 3, -1) &&
                    enemy->target == character->handle) {
                    return SAVE_IN_COMBAT;
                }
            }
        }
    }

    if (func_001DA0D0()) {
        if (D_002D3FAC == 0x2B) {
            return SAVE_NOT_IN_THIS_LEVEL;
        }
        return SAVE_IN_COMBAT;
    }
    return SAVE_ALLOWED;
}

void func_001AF0B0(int value) {
    D_002D4914 = value;
}

// Value of the current level's "<level>_load_int" global.
int func_001AF0C0(void) {
    return D_009B6B50.GetGlobalInt(func_001AF0F0(-1));
}

// Name of a level's "<level>_load_int" global; -1 means the current level.
const char* func_001AF0F0(int level) {
    if (level == -1) {
        sprintf(D_009DEE60, "%s_load_int", func_001DA4B0());
    } else {
        sprintf(D_009DEE60, "%s_load_int", D_002AD550[level].name);
    }
    return D_009DEE60;
}

BOOL func_001AF170(int flag) {
    return D_009B6B50.GetGlobalFlag(D_002A9D60[flag].name, D_002A9D60[flag].index);
}

void func_001AF1A0(int flag, bool value) {
    D_009B6B50.SetGlobalFlag(D_002A9D60[flag].name, D_002A9D60[flag].index, value);
}

int func_001AF1E0(int var) {
    return D_009B6B50.GetGlobalInt(D_002A9E70[var].name, D_002A9E70[var].index);
}

void func_001AF210(int var, int value) {
    D_009B6B50.SetGlobalInt(D_002A9E70[var].name, D_002A9E70[var].index, value);
}

void func_001AF250(void) {
    int i;
    for (i = 0; i < 0x40; i++) {
        D_009DEDE0[i] = 0;
    }
}

void func_001AF2A0(void) {
    int i;
    for (i = 0; i < 0x40; i++) {
        D_009DEDE0[i] = D_009B6B50.unk_281A4[i];
    }
}

void func_001AF320(int index) {
    D_009DEDE0[index] = 1;
}

void func_001AF340(void) {
    int i;
    for (i = 0; i < LEVEL_SAVE_MOVIES; i++) {
        D_009DEE20[i] = 0;
    }
}

void func_001AF3C0(void) {
    int i;
    for (i = 0; i < LEVEL_SAVE_MOVIES; i++) {
        D_009DEE20[i] = D_009B6B50.movies_seen[i];
    }
}

// Marks the movie file at `path` as seen. It is looked up without its directory and, except for
// "ebd.pss", without its extension.
void func_001AF480(const char* path) {
    char name[64];
    const char* p;
    int index;

    p = ngps_strchr(path, '\\');
    if (p == NULL) {
        p = path;
    } else {
        p++;
    }
    strcpy(name, p);
    if (ngps_stricmp(name, "ebd.pss") != 0) {
        char* extension = ngps_strchr(name, '.');
        if (extension) {
            *extension = '\0';
        }
    }
    index = func_001AF520(name);
    if (index != -1) {
        D_009DEE20[index] = 1;
    }
}

// Index of a movie in D_002A9F10, or -1.
int func_001AF520(const char* name) {
    int i;
    for (i = 0; i < LEVEL_SAVE_MOVIES; i++) {
        if (ngps_stricmp(name, D_002A9F10[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// Console command callbacks.
extern "C" {
int func_001AF5A0(void);
int func_001AF600(void);
int func_001AF660(void);
int func_001AF7F0(void);
int func_001AF880(void);
int func_001AFAA0(void);
int func_001AFC50(void);
}

int func_001AF5A0(void) {
    if (doing_help_for_comand) {
        print_to_console("Usage: set_flag <flag_name>", false);
    } else if (calling_a_command_function) {
        process_command(ARG_TYPE_STRING);
        D_009B6B50.SetGlobalFlag(next_arg, true);
    }
}

int func_001AF600(void) {
    if (doing_help_for_comand) {
        print_to_console("Usage: clear_flag <flag_name>", false);
    } else if (calling_a_command_function) {
        process_command(ARG_TYPE_STRING);
        D_009B6B50.SetGlobalFlag(next_arg, false);
    }
}

int func_001AF660(void) {
    if (doing_help_for_comand) {
        print_to_console("Usage: save_game <save_name>", false);
    } else if (calling_a_command_function) {
        const char* name;

        process_command(ARG_TYPE_STRING);
        name = next_arg;
        switch (func_001AED70()) {
        case SAVE_ALLOWED:
            D_009B6B50.Save(name, 0);
            break;
        case SAVE_NOT_IN_THIS_LEVEL:
            print_to_console("Save not allowed\nin this level.\n", false);
            break;
        case SAVE_IN_COMBAT:
            print_to_console("Save not allowed\nin combat\n", false);
            break;
        case SAVE_IN_MINIGAME:
            print_to_console("Save not allowed\nin a minigame\n", false);
            break;
        case SAVE_NO_CLEAR_REASON:
            print_to_console("Save not allowed\nno clear reason\n", false);
            break;
        case SAVE_SUMMON_TRANSITION:
            print_to_console("Save not allowed\nsummon transition active\n", false);
            break;
        case SAVE_SPELLCASTING:
            print_to_console("Save not allowed\nsomebody is spellcasting\n", false);
            break;
        case SAVE_EFFECT_PLAYING:
            print_to_console("Save not allowed\npotion or scroll effect playing\n", false);
            break;
        case SAVE_FLINCHING:
            print_to_console("Save not allowed\nsomebody is flinching\n", false);
            break;
        case SAVE_INHIBITED_BY_SCRIPT:
            print_to_console("Save not allowed\nInhibited by script\n", false);
            break;
        default:
            print_to_console("Save not allowed\nswitch-case not updated!\n", false);
            break;
        }
    }
}

int func_001AF7F0(void) {
    if (doing_help_for_comand) {
        print_to_console("Usage: load_game <load_name>", false);
    } else if (calling_a_command_function) {
        process_command(ARG_TYPE_STRING);
        func_001AE4D0(&D_009B6B50, next_arg, 0);
        func_001AE820(&D_009B6B50, next_arg, 0);
        func_0015F8D0(D_009B6B50.level_name, 0, 0);
    }
}

// A trailing '*' in the mesh commands matches every mesh whose name starts with what comes before
// it. Note that toggle_mesh reports the opposite of what it did for wildcard matches.
int func_001AF880(void) {
    if (doing_help_for_comand) {
        print_to_console("Usage: toggle_mesh <mesh_name*> ", false);
    } else if (calling_a_command_function) {
        char* name;
        char* wildcard;
        char prefix[48];

        process_command(ARG_TYPE_STRING);
        name = next_arg;
        wildcard = ngps_strchr(name, '*');
        if (wildcard) {
            int length = wildcard - name;
            int count;
            int i;
            LevelMesh* mesh;

            strncpy(prefix, name, length);
            prefix[length] = '\0';
            count = D_002E13C0.mesh_count;
            mesh = D_002E13C0.meshes;
            for (i = 0; i < count; i++, mesh++) {
                if (ngps_strnicmp(mesh->name, prefix, length) == 0) {
                    if (mesh->flags & MESH_HIDDEN) {
                        mesh->flags &= ~MESH_HIDDEN;
                        sprintf(string_buffer, "hide '%s'", mesh->name);
                    } else {
                        mesh->flags |= MESH_HIDDEN;
                        sprintf(string_buffer, "show '%s'", mesh->name);
                    }
                    print_to_console(string_buffer, false);
                }
            }
        } else {
            int index = func_0010EA20(name);
            if (index < 0) {
                sprintf(string_buffer, "cannot find '%s'", name);
                print_to_console(string_buffer, false);
            } else {
                if (D_002E13C0.meshes[index].flags & MESH_HIDDEN) {
                    D_002E13C0.meshes[index].flags &= ~MESH_HIDDEN;
                    sprintf(string_buffer, "show '%s'", name);
                } else {
                    sprintf(string_buffer, "hide '%s'", name);
                    D_002E13C0.meshes[index].flags |= MESH_HIDDEN;
                }
                print_to_console(string_buffer, false);
            }
        }
    }
}

int func_001AFAA0(void) {
    if (doing_help_for_comand) {
        print_to_console("Usage: mesh_hide <mesh_name*> ", false);
    } else if (calling_a_command_function) {
        char* name;
        char* wildcard;
        char prefix[48];

        process_command(ARG_TYPE_STRING);
        name = next_arg;
        wildcard = ngps_strchr(name, '*');
        if (wildcard) {
            int length = wildcard - name;
            int count;
            int i;
            LevelMesh* mesh;

            strncpy(prefix, name, length);
            prefix[length] = '\0';
            count = D_002E13C0.mesh_count;
            mesh = D_002E13C0.meshes;
            for (i = 0; i < count; i++, mesh++) {
                if (ngps_strnicmp(mesh->name, prefix, length) == 0) {
                    mesh->flags |= MESH_HIDDEN;
                    sprintf(string_buffer, "hide '%s'", mesh->name);
                    print_to_console(string_buffer, false);
                }
            }
        } else {
            int index = func_0010EA20(name);
            if (index < 0) {
                sprintf(string_buffer, "cannot find '%s'", name);
                print_to_console(string_buffer, false);
            } else {
                sprintf(string_buffer, "hide '%s'", name);
                D_002E13C0.meshes[index].flags |= MESH_HIDDEN;
                print_to_console(string_buffer, false);
            }
        }
    }
}

int func_001AFC50(void) {
    if (doing_help_for_comand) {
        print_to_console("Usage: mesh_show <mesh_name*> ", false);
    } else if (calling_a_command_function) {
        char* name;
        char* wildcard;
        char prefix[48];

        process_command(ARG_TYPE_STRING);
        name = next_arg;
        wildcard = ngps_strchr(name, '*');
        if (wildcard) {
            int length = wildcard - name;
            int count;
            int i;
            LevelMesh* mesh;

            strncpy(prefix, name, length);
            prefix[length] = '\0';
            count = D_002E13C0.mesh_count;
            mesh = D_002E13C0.meshes;
            for (i = 0; i < count; i++, mesh++) {
                if (ngps_strnicmp(mesh->name, prefix, length) == 0) {
                    mesh->flags &= ~MESH_HIDDEN;
                    sprintf(string_buffer, "show '%s'", mesh->name);
                    print_to_console(string_buffer, false);
                }
            }
        } else {
            int index = func_0010EA20(name);
            if (index < 0) {
                sprintf(string_buffer, "cannot find '%s'", name);
                print_to_console(string_buffer, false);
            } else {
                D_002E13C0.meshes[index].flags &= ~MESH_HIDDEN;
                sprintf(string_buffer, "show '%s'", name);
                print_to_console(string_buffer, false);
            }
        }
    }
}

console_command D_009DEEA0("set_flag", "Set a global flag", CALL, func_001AF5A0);
console_command D_009DEEB0("clear_flag", "Clear a global flag", CALL, func_001AF600);
console_command D_009DEEC0("save_game", "Save out game status", CALL, func_001AF660);
console_command D_009DEED0("load_game", "Load in game status", CALL, func_001AF7F0);
console_command D_009DEEE0("toggle_mesh", "toggle mesh render flags", CALL, func_001AF880);
console_command D_009DEEF0("mesh_hide", "deactivate mesh render flags", CALL, func_001AFAA0);
console_command D_009DEF00("mesh_show", "activate mesh render flags", CALL, func_001AFC50);
