#ifndef GAME_LEVEL_SCRIPTS_H
#define GAME_LEVEL_SCRIPTS_H

#include "types.h"
#include "game/matrix.h"
#include "game/memory.h"
#include "game/unit_00196B10.h"

struct Actor;
struct NavNode;

// A 2D line segment given by its end points.
struct segment2 {
    /* 0x00 */ vector2* start;
    /* 0x04 */ vector2* end;
    /* 0x08 */ u8 pad08[0x08];
};

// Navpoint types, the "$Type:" of a "$Navpoint:" entry (names in D_002A9F60).
#define NAVPOINT_LEVEL 0
#define NAVPOINT_CUTSCENE 1
#define NAVPOINT_CAMERA 2
#define NAVPOINT_AMBIENT_SOUND 3
#define NUM_NAVPOINT_TYPES 4

// "$Navpoint:" entry: a named point that player starts, splines, spheres and region quads refer
// to.
struct Navpoint {
    /* 0x00 */ char name[0x10];
    /* 0x10 */ matrix3 orientation;
    /* 0x34 */ vector3 position;
    /* 0x40 */ int layer;
    /* 0x44 */ int field_44;
    /* 0x48 */ int type; // NAVPOINT_*
    /* 0x4C */ u8 pad4C[0x04];
};

// A navpoint as it is stored in the level data.
struct NavpointDef {
    /* 0x00 */ const char* name;
    /* 0x04 */ matrix3 orientation;
    /* 0x28 */ vector3 position;
};

// What a script command refers to; type selects the meaning of the union.
struct ScriptTarget {
    /* 0x00 */ int type;
    /* 0x04 */ u8 pad04[0x0C];
    /* 0x10 */ union {
        Navpoint* navpoint; // type 0
        Actor* actor;       // type 1
        void* object;       // type 2
        int id;             // type 3: object type id, looked up in the object lists
    };
    /* 0x14 */ int team; // types 0 and 2
};

// Item table entry ("level_items.tbl").
struct LevelItem {
    /* 0x00 */ const char* name;
    /* 0x04 */ u8 pad04[0x0C];
};

// "$Ambient:" entry: a looping sound played at a navpoint.
struct AmbientSound {
    /* 0x00 */ int handle;
    /* 0x04 */ int mask;
    /* 0x08 */ float radius;
    /* 0x0C */ Navpoint* source;
};

// "$Constrain:" entry.
struct Constraint {
    /* 0x00 */ const char* name;
    /* 0x04 */ u8 pad04[0x10];
};

// "$Click:" entry.
struct ClickSpot {
    /* 0x00 */ const char* name;
    /* 0x04 */ u8 pad04[0x28];
};

// "$Load spline:" entry: crossing the line between the two navpoints loads another level, or the
// world map.
struct LoadSpline {
    /* 0x00 */ const char* name;
    /* 0x04 */ Navpoint* left;
    /* 0x08 */ Navpoint* right;
    /* 0x0C */ int level;     // index into the level table
    /* 0x10 */ u8 start_nav;  // "$Start nav:"
    /* 0x11 */ bool enabled;
    /* 0x12 */ bool worldmap; // "+Worldmap"
};

// "$Trigger Spline:" entry: crossing the line between the two navpoints runs a sequence, unless
// one of the disable flags is set.
struct TriggerSpline {
    /* 0x00 */ const char* name;
    /* 0x04 */ Navpoint* left;
    /* 0x08 */ Navpoint* right;
    /* 0x0C */ void* sequence;
    /* 0x10 */ int avoidance; // avoidance spline for "+Impassable", or -1
    /* 0x14 */ u8 num_disable_flags;
    /* 0x18 */ int* disable_flags;
};

// "$Trigger Sphere:" entry: entering the sphere runs a sequence, unless one of the disable flags
// is set.
struct TriggerSphere {
    /* 0x00 */ const char* name;
    /* 0x04 */ Navpoint* navpoint;
    /* 0x08 */ float radius;
    /* 0x0C */ void* sequence;
    /* 0x10 */ u8 num_disable_flags;
    /* 0x14 */ int* disable_flags;
};

// "$Shopkeeper:" entry.
struct Shopkeeper {
    /* 0x00 */ void* creature;
    /* 0x04 */ int num_items;
    /* 0x08 */ void* items;
    /* 0x0C */ u8 pad0C[0x04];
};

struct ShopItem {
    /* 0x00 */ u8 pad00[0x08];
};

// "$Region:" entry: a box, refined by up to four quads ("+Quad:") of navpoints, that runs its
// camera sequence ("+Camera Sequence:") while the player is inside.
struct Region {
    /* 0x00 */ const char* name;
    /* 0x04 */ vector3 min;
    /* 0x10 */ vector3 max;
    /* 0x1C */ int num_quads;
    /* 0x20 */ Navpoint* quads[4][4];
    /* 0x60 */ void* camera_sequence;
};

// Per-node script data, parallel to the navigation mesh nodes.
struct NavNodeScript {
    NavNodeScript() { id = -1; }

    /* 0x00 */ int id;
    /* 0x04 */ u8 pad04[0x54];
};

struct SoundSlot {
    /* 0x00 */ int handle;
    /* 0x04 */ int sound;
};

// A Mempool that the static initializer sets up over its buffer. Mempool itself has no
// constructors, so pools that are set up later (file_packfile.cpp's) need no static initializer.
class StaticMempool : public Mempool {
public:
    StaticMempool(char* buffer, u32 size, const char* name, u8 alignment) {
        Copy((u8*)buffer, size, name, alignment);
    }
};

#define MAX_NAVPOINTS 700
#define MAX_SCRIPT_TARGETS 32
#define MAX_LEVEL_ITEMS 32
#define MAX_AMBIENT_SOUNDS 64
#define MAX_CONSTRAINTS 32
#define MAX_CLICK_SPOTS 64
#define MAX_LOAD_SPLINES 16
#define MAX_TRIGGER_SPLINES 64
#define MAX_TRIGGER_SPHERES 32
#define MAX_SHOPKEEPERS 15
#define MAX_SHOP_ITEMS 1000
#define MAX_EXTRA_NAVPOINTS 32
#define LEVEL_SCRIPT_POOL_SIZE 0x10000
#define LEVEL_ITEM_POOL_SIZE 0x1000
#define MAX_REGIONS 10
#define MAX_SOUND_SLOTS 10

extern const char* D_002A9F60[NUM_NAVPOINT_TYPES]; // navpoint type names

extern int D_002D491C;             // navpoint count
extern int D_002D4920;             // script target count
extern int D_002D4924;             // level item count
extern int D_002D4928;             // ambient sound count
extern NavNodeScript* D_002D492C;  // one per navigation mesh node
extern int D_002D4930;             // NavNodeScript count
extern int D_002D4934;             // constraint count
extern int D_002D4938;             // click spot count
extern int D_002D493C;             // load spline count
extern int D_002D4940;             // trigger spline count
extern int D_002D4944;             // trigger sphere count
extern int D_002D4948;             // shopkeeper count
extern int D_002D494C;             // shop item count
extern int D_002D4950;             // extra navpoint count
extern int D_002D4954;             // region count
extern const char* D_002D4958;     // level a load spline is taking the player to
extern int D_002D495C;
extern int D_002D4960;             // start nav in that level
extern bool D_002D4964;            // going to the world map instead
extern bool D_002D4968;
extern int D_002D496C;             // get_party_gather_state() when the load spline was crossed
extern int D_002D4970;
extern int D_002D4974;             // region the player was in last frame, or -1
extern int D_002D4978;
extern Actor* D_002D497C;          // actor that last crossed a trigger spline
extern int D_002D4980;             // region the player is in, or -1

extern Navpoint D_009DEF10[MAX_NAVPOINTS];
extern ScriptTarget D_009EC9D0[MAX_SCRIPT_TARGETS];
extern LevelItem D_009ECCD0[MAX_LEVEL_ITEMS];
extern AmbientSound D_009ECED0[MAX_AMBIENT_SOUNDS];
extern Constraint D_009ED2D0[MAX_CONSTRAINTS];
extern ClickSpot D_009ED550[MAX_CLICK_SPOTS];
extern LoadSpline D_009EE050[MAX_LOAD_SPLINES];
extern TriggerSpline D_009EE190[MAX_TRIGGER_SPLINES];
extern TriggerSphere D_009EE890[MAX_TRIGGER_SPHERES];
extern Shopkeeper D_009EEB90[MAX_SHOPKEEPERS];
extern ShopItem D_009EEC80[MAX_SHOP_ITEMS];
extern Navpoint D_009F0BC0[MAX_EXTRA_NAVPOINTS];
extern char D_009F15C0[LEVEL_SCRIPT_POOL_SIZE];
extern StaticMempool D_00A015C0;   // "level scripts", over D_009F15C0
extern char D_00A015E0[LEVEL_ITEM_POOL_SIZE];
extern StaticMempool D_00A025E0;   // "level itmes", over D_00A015E0
extern Region D_00A02600[MAX_REGIONS];
extern vector3 D_00A029E8;         // player position when a load spline was crossed
extern SoundSlot D_00A02A00[MAX_SOUND_SLOTS];

void func_001AFE00(void);
void func_001AFF10(void);
void func_001AFFA0(void);
void func_001B0010(void);
BOOL func_001B0080(Actor* actor, Navpoint* left, Navpoint* right);
void func_001B02E0(void);
void func_001B0420(void);
void func_001B05F0(void);
void func_001B0870(void);
void func_001B08B0(void* level);
void func_001B09D0(BOOL scale_levels);
BOOL func_001B0B90(int index, Actor* actor);
void func_001B0D60(void);
void func_001B0E70(void);
void func_001B0FE0(void);
Actor* func_001B1010(const char* name);
void func_001B1080(NavpointDef* def);
Navpoint* func_001B1330(const char* name);
Navpoint* func_001B1420(const vector3* position);
vector3 func_001B1590(ScriptTarget* target);
int get_script_target_team(ScriptTarget* target);
LevelItem* find_level_item(const char* name);
void add_ambient_sound(int sound, Navpoint* source, int mask, float radius);
NavNodeScript* get_nav_node_script(NavNode* node);
Constraint* find_constraint(const char* name);
ClickSpot* find_click_spot(const char* name);
BOOL is_in_trigger_sphere(const char* name, Actor* actor);
Shopkeeper* find_shopkeeper(void* creature);
BOOL is_in_region(const char* name, Actor* actor);
int create_avoidance_spline(Navpoint* a, Navpoint* b);
int destroy_avoidance_spline(int id);
void make_table_filename(char* destination, const char* name, const char* suffix);
int get_party_gather_state(void);
void place_party_at_start(void);

#endif // GAME_LEVEL_SCRIPTS_H
