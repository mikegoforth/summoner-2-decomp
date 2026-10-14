#ifndef GAME_LEVEL_SAVE_H
#define GAME_LEVEL_SAVE_H

#include "types.h"
#include <string.h>

class GameTimer;
struct GameObject;
struct EffectRecord;

// Size of the buffer the save sections are laid out in.
#define LEVEL_SAVE_BUFFER_SIZE 0x28000

// Format version this executable writes. Older saves are brought up to date while loading.
#define LEVEL_SAVE_VERSION 2

// Number of movies whose "seen" flags are kept in the save.
#define LEVEL_SAVE_MOVIES 18

// Section tags are 32-bit values whose bytes read in order in memory, so 'ITEM' shows up as "ITEM"
// in a dump of the save buffer.
#define SAVE_TAG(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

// One section of the save buffer. Every section starts with its 4-byte tag and its 4-byte size; the
// members the section types add are the offsets of the section's blocks from its start. Values in
// the buffer are not aligned, so they are copied in and out with memcpy.
struct SaveSection {
    // Writes the section's tag and size at its start.
    void WriteHeader(u32 tag) {
        u32 n;
        u32 value;

        value = tag;
        memcpy(data, &value, sizeof(value));
        n = size;
        memcpy(data + 4, &n, sizeof(n));
    }

    void Write(int value, const int& offset) { memcpy(data + offset, &value, sizeof(value)); }
    void Write(u32 value, const int& offset) { memcpy(data + offset, &value, sizeof(value)); }
    void Write(f32 value, const int& offset) { memcpy(data + offset, &value, sizeof(value)); }
    void Write(const void* src, u32 size, const int& offset) { memcpy(data + offset, src, size); }

    // The same with the offset first.
    void WriteAt(u32 offset, const void* src, u32 n) { memcpy(data + offset, src, n); }
    void WriteIntAt(s32 offset, s32 value) {
        s32 v = value;
        memcpy(data + offset, &v, sizeof(v));
    }

    /* 0x0 */ u8* data;
    /* 0x4 */ int size;
};

// "S2SG": the format version and the description shown when choosing a save to load.
struct SaveHeaderSection : SaveSection {
    // The version follows the tag and size.
    void WriteVersion(u32 version) { memcpy(data + 8, &version, sizeof(version)); }

    /* 0x08 */ int version;
    /* 0x0C */ int info; // info block, see LevelSave
};

// "THMB": the 176x150 picture shown with the save.
struct SaveThumbnailSection : SaveSection {
    /* 0x08 */ int image;
};

// "GLBL": the global flags (one bit each) and ints, found by the hashes of their names.
struct SaveGlobalsSection : SaveSection {
    /* 0x08 */ int flag_count;
    /* 0x0C */ int flag_names; // one u32 name hash per flag
    /* 0x10 */ int flag_bits;
    /* 0x14 */ int int_count;
    /* 0x18 */ int int_names; // one u32 name hash per int
    /* 0x1C */ int int_values;
};

// "LEPS": the party and the units of the level, and its groups, events, trackers and timers.
struct SaveLevelSection : SaveSection {
    /* 0x08 */ int party; // SaveParty
    /* 0x0C */ int unit_count;
    /* 0x10 */ int units;  // SaveUnit records
    /* 0x14 */ int groups; // count, then SaveGroup records
    /* 0x18 */ int slots;  // one word per item slot
    /* 0x1C */ int members; // SaveMember records
    /* 0x20 */ int member_count;
    /* 0x24 */ int events;   // SaveEvent records
    /* 0x28 */ int trackers; // SaveTracker records
    /* 0x2C */ int timers;   // SaveTimer records
};

// "CLIK" and "ITEM": a count, a table of name hashes and bits indexed by table position.
struct SaveFlagSection : SaveSection {
    /* 0x08 */ u32 count;
    /* 0x0C */ u32 names;
    /* 0x10 */ u32 bits;
};

// "AMAP": one word of bits per level, then one more word.
struct SaveAutomapSection : SaveSection {
    /* 0x08 */ int levels;
    /* 0x0C */ int value;
};

// A section holding one block.
struct SaveBlockSection : SaveSection {
    /* 0x08 */ int block;
};

// A section holding a record count and the records.
struct SaveListSection : SaveSection {
    /* 0x08 */ int count;
    /* 0x0C */ int records;
};

// "SPLN"
struct SaveSplineSection : SaveSection {
    /* 0x08 */ int removed; // one byte per spline
    /* 0x0C */ int states;
};

// The save image: the section table, the buffer the sections point into, the values unpacked from
// the header section and the state of the last file read from the memory card.
//
// Info block layout (header.data + header.info):
//   0x00 char name[0x2C]   0x2C int   0x30 int   0x34 f32   0x38 u32
//   0x3C u8[0x40]          0x7C u8[LEVEL_SAVE_MOVIES]
class LevelSave {
public:
    // Only the first word is cleared: sizeof(this) is the size of a pointer.
    LevelSave() { memset(this, 0, sizeof(this)); }

    // Each Upgrade step checks one section of a save written by an older version and returns
    // true on failure.
    bool UpgradeCAMR(int* size, int version);
    bool UpgradeMESH(int* size, int version);
    bool UpgradeDOOR(int* size, int version);
    bool UpgradeLVFX(int* size, int version);
    bool UpgradeDROP(int* size, int version);
    bool UpgradeLSEQ(int* size, int version);
    bool UpgradeGLYF(int* size, int version);
    bool UpgradeCHNG(int* size, int version);
    bool UpgradeSPLN(int* size, int version);
    bool UpgradeOPTS(int* size, int version);
    // Brings a save written by an older version up to date: runs every Upgrade step, then checks
    // that size matches the current layout. Returns true on failure.
    bool Upgrade(int size);

    // Click states are stored under the hash of the current level's name followed by the object's
    // name.
    u32 GetClickState(const char* name);
    void SetClickState(const char* name, u32 state);

    // Lays out the sections. file_buffer receives saves read from the memory card.
    void Init(void* file_buffer, int file_buffer_size);
    // Clears the buffer and writes every section with its starting contents.
    void Reset(void);
    u8* GetThumbnail(void);

    // Global flags and ints can be looked up by name, by the name's hash or by their index in the
    // section. The name passed along with a hash or an index is not used. FindGlobalFlag returns -1
    // for a missing flag, but FindGlobalInt returns 0, the first int's index.
    int FindGlobalFlag(const char* name);
    bool GetGlobalFlag(int index);
    BOOL GetGlobalFlag(const char* name, int index);
    bool GetGlobalFlag(u32 hash, const char* name);
    bool GetGlobalFlag(const char* name);
    void SetGlobalFlag(const char* name, int index, bool value);
    void SetGlobalFlag(u32 hash, bool value, const char* name);
    void SetGlobalFlag(const char* name, bool value);
    int FindGlobalInt(const char* name);
    int GetGlobalInt(int index);
    int GetGlobalInt(const char* name, int index);
    int GetGlobalInt(u32 hash, const char* name);
    int GetGlobalInt(const char* name);
    void SetGlobalInt(const char* name, int index, int value);
    void SetGlobalInt(u32 hash, int value, const char* name);
    void SetGlobalInt(const char* name, int value);

    bool GetItemFlag(u32 hash);
    bool GetItemFlag(const char* name);
    // Item flags can only be set; value is ignored.
    void SetItemFlag(u32 hash, bool value);
    void SetItemFlag(const char* name, bool value);

    u32 GetAutomap(int level);
    void SetAutomapBit(int level, int bit);

    // Stores the game state and writes the buffer to the named file on the memory card in port.
    // Returns true on failure.
    bool Save(const char* name, int port);

    /* 0x00000 */ SaveHeaderSection header;       // "S2SG"
    /* 0x00010 */ SaveThumbnailSection thumbnail; // "THMB"
    /* 0x0001C */ SaveGlobalsSection globals;     // "GLBL"
    /* 0x0003C */ SaveLevelSection level;         // "LEPS"
    /* 0x0006C */ SaveFlagSection clicks;         // "CLIK"
    /* 0x00080 */ SaveFlagSection items;          // "ITEM"
    /* 0x00094 */ SaveAutomapSection automap;     // "AMAP"
    /* 0x000A4 */ SaveBlockSection camera;        // "CAMR"
    /* 0x000B0 */ SaveBlockSection meshes;        // "MESH"
    /* 0x000BC */ SaveBlockSection doors;         // "DOOR"
    /* 0x000C8 */ SaveListSection effects;        // "LVFX"
    /* 0x000D8 */ SaveListSection drops;          // "DROP"
    /* 0x000E8 */ SaveListSection sequences;      // "LSEQ"
    /* 0x000F8 */ SaveBlockSection glyphs;        // "GLYF"
    /* 0x00104 */ SaveBlockSection changes;       // "CHNG"
    /* 0x00110 */ SaveSplineSection splines;      // "SPLN"
    /* 0x00120 */ SaveBlockSection options;       // "OPTS"
    /* 0x0012C */ int unk12C;
    /* 0x00130 */ u8 buffer[LEVEL_SAVE_BUFFER_SIZE];
    /* 0x28130 */ int used; // bytes of the buffer the sections take
    /* 0x28134 */ bool valid; // set once a save has been read and unpacked
    /* 0x28138 */ int version;
    // From the header section's info block.
    /* 0x2813C */ char level_name[0x58];
    /* 0x28194 */ int unk_28194; // info 0x2C
    /* 0x28198 */ int unk_28198; // info 0x30
    /* 0x2819C */ u32 unk_2819C; // info 0x38
    /* 0x281A0 */ f32 unk_281A0; // info 0x34
    /* 0x281A4 */ u8 unk_281A4[0x40];
    /* 0x281E4 */ u8 movies_seen[LEVEL_SAVE_MOVIES];
    // When the file was last written, from its memory card directory entry.
    /* 0x281F6 */ u8 sec;
    /* 0x281F7 */ u8 min;
    /* 0x281F8 */ u8 hour;
    /* 0x281F9 */ u8 day;
    /* 0x281FA */ u8 month;
    /* 0x281FC */ u16 year;
    /* 0x281FE */ u8 unk_281FE[2];
    // File read into file_buffer by func_001AE820, unpacked later by func_001AE9D0.
    /* 0x28200 */ char filename[0x80];
    /* 0x28280 */ u32 file_size;
    /* 0x28284 */ u8* file_buffer;
    /* 0x28288 */ u32 file_buffer_size;
};

// A flag or int that game code refers to by name; index caches its slot in the globals section.
struct SaveVariableRef {
    /* 0x0 */ const char* name;
    /* 0x4 */ s32 index;
};

// Why func_001AED70 refuses a save.
enum {
    SAVE_ALLOWED = 0,
    SAVE_NOT_IN_THIS_LEVEL = 1,
    SAVE_IN_COMBAT = 2,
    SAVE_IN_MINIGAME = 3,
    SAVE_NO_CLEAR_REASON = 4,
    SAVE_SPELLCASTING = 5,
    SAVE_EFFECT_PLAYING = 6,
    SAVE_FLINCHING = 7,
    SAVE_SUMMON_TRANSITION = 8,
    SAVE_INHIBITED_BY_SCRIPT = 9,
};

// Global flags and ints kept in the save, looked up by name.
extern SaveVariableRef D_002A9D60[34];
extern SaveVariableRef D_002A9E70[20];
// Movie names (without the .pss extension, except "ebd.pss").
extern const char* D_002A9F10[LEVEL_SAVE_MOVIES + 1];

// The game's save image.
extern LevelSave D_009B6B50;
extern u8 D_009DEDE0[0x40];
extern u8 D_009DEE20[LEVEL_SAVE_MOVIES];
extern s16 D_009DEE40[16];
extern char D_009DEE60[0x40];

// Bitmap made from the save's thumbnail section.
extern int D_002D4910;
extern int D_002D4914;
// Set while a script forbids saving.
extern u8 D_002D4918;

// Plays the "open up" animation of the doors whose flag is set in the save.
void open_flagged_doors(void);

extern "C" {
// Character type to the 0-25 slot index the save uses (types 0-21 keep their number, types 64-67
// become 22-25), or -1.
int func_001A5E30(s16 type);
// Slot index back to character type, or -1.
int func_001A5E70(int index);
// Find an object by name hash.
GameObject* func_001A5EB0(u32 hash);
GameObject* func_001A5F70(u32 hash);
// Restores a timer from a saved time, using fallback if the saved time is out of range.
BOOL func_001A6000(GameTimer* timer, int time, int fallback);

// Section layout, in buffer order.
void func_001A6060(LevelSave* save);
void func_001A6230(LevelSave* save);
void func_001A6310(LevelSave* save);
void func_001A64D0(LevelSave* save);
void func_001A6780(LevelSave* save);
void func_001A68B0(LevelSave* save);
void func_001A69E0(LevelSave* save);
void func_001A6AE0(LevelSave* save);
void func_001A6BB0(LevelSave* save);
void func_001A6C80(LevelSave* save);
void func_001A6D50(LevelSave* save);
void func_001A6E50(LevelSave* save);
void func_001A6F50(LevelSave* save);
void func_001A7050(LevelSave* save);
void func_001A7120(LevelSave* save);
void func_001A71F0(LevelSave* save);
void func_001A72F0(LevelSave* save);

// Starting contents of each section.
void func_001A73C0(LevelSave* save);
void func_001A7440(LevelSave* save);
void func_001A74A0(LevelSave* save);
void func_001A7790(LevelSave* save);
void func_001A77F0(LevelSave* save);
void func_001A7850(LevelSave* save);
void func_001A7A10(LevelSave* save);
void func_001A7A70(LevelSave* save);
void func_001A7AD0(LevelSave* save);
void func_001A7B30(LevelSave* save);
void func_001A7B90(LevelSave* save);
void func_001A7BF0(LevelSave* save);
void func_001A7C50(LevelSave* save);
void func_001A7CB0(LevelSave* save);
void func_001A7D10(LevelSave* save);
void func_001A7D70(LevelSave* save);
void func_001A7DD0(LevelSave* save);

// Savers: each stores one part of the game state in its section. Nonzero means failure.
int func_001A7E30(LevelSave* save);
int func_001A7FC0(LevelSave* save);
int func_001A8040(LevelSave* save);
int func_001A8050(LevelSave* save, int* offset, GameObject* unit, u8 flag0, u8 flag1, u8 flag2, u8 flag3);
int func_001A8240(LevelSave* save, int* offset);
bool func_001A8420(LevelSave* save);
int func_001A9060(LevelSave* save);
int func_001A90E0(LevelSave* save);
int func_001A90F0(LevelSave* save);
int func_001A9140(LevelSave* save);
int func_001A9190(LevelSave* save);
int func_001A9240(LevelSave* save);
int func_001A9440(LevelSave* save);
int func_001A9590(LevelSave* save);
int func_001A9870(LevelSave* save);
int func_001A9970(LevelSave* save);
int func_001A9AF0(LevelSave* save);
int func_001A9C00(LevelSave* save);
int func_001A9D70(LevelSave* save);

// Loaders: each copies one section of a loaded save back into the game. Nonzero means the section
// doesn't match or something it refers to can't be found.
int func_001A9DB0(LevelSave* save); // S2SG
int func_001A9F30(LevelSave* save);
int func_001A9F40(LevelSave* save); // GLBL
int func_001A9F80(LevelSave* save, int* offset);
int func_001AA280(LevelSave* save, int* offset);
int func_001AA560(LevelSave* save); // LEPS
int func_001AB3D0(LevelSave* save); // CLIK
int func_001AB4C0(LevelSave* save); // ITEM
int func_001AB530(LevelSave* save); // AMAP
int func_001AB5D0(LevelSave* save); // CAMR
int func_001AB680(LevelSave* save); // MESH
int func_001AB780(LevelSave* save); // DOOR
int func_001ABAC0(LevelSave* save, EffectRecord* record);
int func_001ABBE0(LevelSave* save); // LVFX
int func_001ABC80(LevelSave* save); // DROP
int func_001ABF10(LevelSave* save); // LSEQ
int func_001AC050(LevelSave* save); // GLYF
int func_001AC200(LevelSave* save); // CHNG
int func_001AC310(LevelSave* save); // SPLN
int func_001AC4D0(LevelSave* save); // OPTS

// Upgraders for the sections before CAMR (the rest are LevelSave::Upgrade*). Nonzero means the
// section's tag doesn't match.
int func_001AC510(LevelSave* save, int* size, int version); // S2SG
int func_001AC590(LevelSave* save, int* size, int version); // GLBL
int func_001AC5D0(LevelSave* save, int* size, int version); // LEPS
int func_001ACAA0(LevelSave* save, int* size, int version); // CLIK
int func_001ACB20(LevelSave* save, int* size, int version); // ITEM
int func_001ACBA0(LevelSave* save, int* size, int version); // AMAP

// Memory card transfers.
int func_001AE4D0(LevelSave* save, const char* name, int port);
void func_001AE7A0(LevelSave* save, const char* name, int port);
int func_001AE820(LevelSave* save, const char* name, int port);
BOOL func_001AE9A0(LevelSave* save);
int func_001AE9D0(LevelSave* save);

void func_001AECE0(u8* file_buffer, u32 file_buffer_size);
void func_001AED50(void);
void func_001AED60(void);
int func_001AED70(void);
void func_001AF0B0(int value);
int func_001AF0C0(void);
const char* func_001AF0F0(int level);
BOOL func_001AF170(int flag);
void func_001AF1A0(int flag, bool value);
int func_001AF1E0(int var);
void func_001AF210(int var, int value);
void func_001AF250(void);
void func_001AF2A0(void);
void func_001AF320(int index);
void func_001AF340(void);
void func_001AF3C0(void);
void func_001AF480(const char* path);
int func_001AF520(const char* name);
}

#endif // GAME_LEVEL_SAVE_H
