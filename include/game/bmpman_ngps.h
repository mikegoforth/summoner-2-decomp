#ifndef GAME_BMPMAN_NGPS_H
#define GAME_BMPMAN_NGPS_H

#include "types.h"

// Bitmap manager. Bitmaps come from PEG files: a header, a directory of bitmap headers, then the
// pixels. Every bitmap takes one bitmap_entry and an animation takes one more per frame, right
// after its own; a bitmap handle is the index in bm_bitmaps. Bitmaps are found by name through
// bm_hash_table, which holds handles.

class Mempool;
class StringPool;
struct VramBlock;

#define BM_MAX_BITMAPS 2500
#define BM_HASH_TABLE_SIZE 2501 // open addressing on bitmap_entry::hash
#define BM_MAX_PEGS 300

// ccrunch.arr lists which bitmaps (by the CRC of their lowercase names) each PEG holds, one PEG
// a line: "peg: bitmap, bitmap, ...".
#define CCRUNCH_MAX_PEGS 1200
#define CCRUNCH_MAX_BITMAPS 16 // per PEG
#define CCRUNCH_NAME_LENGTH 48

// bitmap_entry::format, as the pixels are stored in a PEG.
#define BM_FORMAT_NONE 0
#define BM_FORMAT_SIZED_1 1 // the data starts with its own size in bytes
#define BM_FORMAT_SIZED_2 2
#define BM_FORMAT_16BIT 3     // two bytes a pixel
#define BM_FORMAT_8BIT 4      // a 256-colour palette, then a byte a pixel
#define BM_FORMAT_4BIT 5      // a 16-colour palette, then half a byte a pixel
#define BM_FORMAT_PEG_32BIT 7 // four bytes a pixel

// bitmap_entry::format of the bitmaps bm_create makes. Only these give their entry back in
// bm_release.
#define BM_FORMAT_32BIT 6       // four bytes a pixel
#define BM_FORMAT_CREATED_16S 8 // two bytes a pixel, sent to the GS as PSMCT16S
#define BM_FORMAT_CREATED_16 9  // two bytes a pixel

// bitmap_entry::palette_format, the size of a palette colour.
#define BM_PALETTE_16BIT 1
#define BM_PALETTE_32BIT 2

// bitmap_entry::flags
#define BM_FLAG_NOT_POW2 0x04 // formats 1 and 2, or a side is not a power of two
#define BM_FLAG_USED 0x08

// peg_entry::flags
#define PEG_FLAG_MEMPOOL 0x01      // data lives in a mempool and is not freed
#define PEG_FLAG_PACKFILE 0x02     // data points into a loaded packfile and is not freed
#define PEG_FLAG_UNREGISTERED 0x04 // bitmaps are not in the hash table; skipped by name lookups

struct bitmap_entry {
    /* 0x00 */ char* name; // in one of the two string pools; NULL for animation frames
    /* 0x04 */ u32 hash;   // hash of the name, -1 while not in bm_hash_table
    /* 0x08 */ u8* data;
    /* 0x0C */ u8* palette;
    /* 0x10 */ int data_size;
    /* 0x14 */ int num_pixels; // over all mip levels
    /* 0x18 */ u16 vram_size;  // in 256-byte GS blocks
    /* 0x1A */ u16 width;
    /* 0x1C */ u16 height;
    /* 0x1E */ s16 unknown_1E; // TEX1's K (mipmap level bias): 150 plus the value in the PEG
    /* 0x20 */ u8 num_frames;  // 1 unless animated; the frames follow the entry
    /* 0x21 */ u8 fps;
    /* 0x22 */ u8 num_mip_levels;
    /* 0x23 */ u8 format;
    /* 0x24 */ u8 palette_format;
    /* 0x25 */ u8 flags;
    /* 0x26 */ s16 parent;    // for an animation frame, the handle of the bitmap; otherwise -1
    /* 0x28 */ VramBlock* vram; // the texture cache's VRAM run holding the bitmap, NULL if none
}; // size 0x2C

// A loaded PEG. The entries are kept in doubly linked lists whose heads are spare entries.
struct peg_entry {
    /* 0x00 */ peg_entry* next;
    /* 0x04 */ peg_entry* prev;
    /* 0x08 */ char name[40];
    /* 0x30 */ u8* data;
    /* 0x34 */ int size;
    /* 0x38 */ Mempool* mempool;
    /* 0x3C */ s16 first_bitmap; // handle of its first bitmap
    /* 0x3E */ s16 num_bitmaps;
    /* 0x40 */ s16 num_entries;  // bitmap entries used, animation frames included
    /* 0x42 */ u16 flags;
}; // size 0x44

// The start of a PEG file.
struct peg_header {
    /* 0x00 */ int signature;
    /* 0x04 */ int version;
    /* 0x08 */ int header_size;
    /* 0x0C */ int data_size;
    /* 0x10 */ u32 num_bitmaps;
    /* 0x14 */ u32 unknown_14;
    /* 0x18 */ u32 num_entries; // bitmap entries needed, animation frames included
    /* 0x1C */ int alignment;   // of each bitmap's data
}; // size 0x20

// A bitmap's header in a PEG file. They follow the peg_header.
struct peg_bitmap {
    /* 0x00 */ u16 width;
    /* 0x02 */ u16 height;
    /* 0x04 */ u8 format;
    /* 0x05 */ u8 palette_format;
    /* 0x06 */ u8 flags;
    /* 0x07 */ u8 num_frames;
    /* 0x08 */ u8 fps;
    /* 0x09 */ u8 num_mip_levels;
    /* 0x0A */ u16 unknown_0A;
    /* 0x0C */ char name[48];
    /* 0x3C */ int unknown_3C;
}; // size 0x40

// A PEG that is already in memory, passed to bm_load_peg instead of reading the file.
struct peg_memory {
    /* 0x00 */ u8* data;
    /* 0x04 */ u8* unknown_04;
    /* 0x08 */ u8* unknown_08;
    /* 0x0C */ int size_1;
    /* 0x10 */ int unknown_10;
    /* 0x14 */ int size_2;
    /* 0x18 */ int unknown_18;
    /* 0x1C */ int size_3;
    /* 0x20 */ Mempool* mempool; // owner of data, or NULL
};

// Section sizes of a PEG, as recorded by ccrunch_set_peg_sizes.
struct ccrunch_peg_sizes {
    /* 0x00 */ int size_1;
    /* 0x04 */ int size_2;
};

extern bitmap_entry bm_bitmaps[BM_MAX_BITMAPS];
extern s16 bm_hash_table[BM_HASH_TABLE_SIZE];
extern peg_entry peg_entries[BM_MAX_PEGS];
extern peg_entry peg_free_list;
extern peg_entry peg_permanent_list;
extern peg_entry peg_level_list; // unloaded by bm_level_unload
extern char ccrunch_peg_names[CCRUNCH_MAX_PEGS][CCRUNCH_NAME_LENGTH];
extern ccrunch_peg_sizes ccrunch_sizes[CCRUNCH_MAX_PEGS];
extern u32 ccrunch_bitmap_crcs[CCRUNCH_MAX_PEGS][CCRUNCH_MAX_BITMAPS];
extern StringPool bm_name_pool;
extern StringPool bm_level_name_pool; // names of the bitmaps loaded in level mode

extern int bm_default_handle; // misc-static.tga, returned when a bitmap is not found
extern int bm_level_start;    // bm_next_handle when the level started
extern int bm_next_handle;    // where the search for free entries starts
extern bool bm_level_mode;    // between bm_level_start_mark and bm_level_unload
extern int ccrunch_num_pegs;
extern int ccrunch_loaded;

u32 bm_hash_name(const char* name);
peg_entry* peg_find(const char* name);
peg_entry* peg_find_any(const char* name);
void* bm_alloc_data(int size);
void bm_free_data(void* data);
void peg_unload(peg_entry* peg);
void ccrunch_load(void);
void peg_read_header(peg_header* header, peg_entry* peg, int* header_size, int* data_size, int* alignment,
                     int* version);
peg_bitmap* peg_read_bitmap(peg_bitmap* header, peg_entry* peg, bitmap_entry* bm, int* count, int version,
                            BOOL add_frames);
void peg_setup_bitmaps(peg_entry* peg, u8* data, int alignment, int first, int* count, BOOL unregistered);
int peg_load(const char* name, BOOL unregistered, BOOL level, peg_memory* memory);
void bm_init(void);
void bm_set_default(void);
int bm_load(const char* name, int unused, int peg);
int bm_create(int source_format, int width, int height, u8* data, u8* unused, int format, bool convert);
void bm_release(int handle);
void bm_get_data(int handle, u8** data, u8** palette);
int bm_get_frame(int handle);
int bm_find(const char* name);
void bm_get_dimensions(int handle, int* width, int* height);
void bm_get_info(int handle, int* width, int* height, int* num_pixels, int* num_mip_levels);
int bm_get_num_frames(int handle, float* duration);
int bm_get_format(int handle);
int bm_get_palette_format(int handle);
void bm_get_peg_data(const char* name, u8** data, bool any);
BOOL ccrunch_find_peg(char* bitmap_name, char* peg_name);
BOOL ccrunch_find_peg_name(char* bitmap_name, char** peg_name);
BOOL ccrunch_load_peg(char* bitmap_name);
BOOL ccrunch_unload_peg(char* bitmap_name);
void ccrunch_set_peg_sizes(const char* peg_name, int size_1, int size_2);
BOOL ccrunch_get_peg_sizes(const char* peg_name, int* size_1, int* size_2);
void bm_reload_peg(const char* peg_name, const char* filename);
BOOL bm_peg_loaded(const char* name);
int bm_load_peg(const char* name, peg_memory* memory);
void bm_unload_peg(const char* name);
int bm_load_peg_unregistered(const char* name, peg_memory* memory);
void bm_unload_peg_index(int index);
void bm_level_start_mark(void);
void bm_level_unload(void);
BOOL bm_is_pow2(int handle);
int bm_get_mip_pixels(int width, int height, int num_mip_levels);
u32 bm_get_vram_size(int format, int width, int height, int num_mip_levels);

#endif // GAME_BMPMAN_NGPS_H
