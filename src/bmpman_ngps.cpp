// Bitmap manager: loading and unloading PEGs, the bitmap table and its name hash table, and
// ccrunch.arr, the list of which bitmaps each PEG holds.
#include "game/bmpman_ngps.h"
#include "game/debug.h"
#include "game/file_packfile.h"
#include "game/frametime.h"
#include "game/gr_ngps_texture.h"
#include "game/memory.h"
#include "game/ngps_file.h"
#include "game/parse.h"
#include "game/plainstring.h"
#include "game/platform_ngps.h"
#include "game/stringpool.h"
#include "game/unit_00190720.h"
#include "macros.h"
#include <stdio.h>
#include <string.h>

extern "C" char* func_00251270(char* string, const char* delimiters); // strtok

int bm_default_handle = -1;
int bm_level_start = -1;

int bm_next_handle;
bool bm_level_mode;
int ccrunch_num_pegs;
int ccrunch_loaded;

bitmap_entry bm_bitmaps[BM_MAX_BITMAPS];
s16 bm_hash_table[BM_HASH_TABLE_SIZE];
peg_entry peg_entries[BM_MAX_PEGS];
peg_entry peg_free_list;
peg_entry peg_permanent_list;
peg_entry peg_level_list;
char ccrunch_peg_names[CCRUNCH_MAX_PEGS][CCRUNCH_NAME_LENGTH];
ccrunch_peg_sizes ccrunch_sizes[CCRUNCH_MAX_PEGS];
u32 ccrunch_bitmap_crcs[CCRUNCH_MAX_PEGS][CCRUNCH_MAX_BITMAPS];
char bm_name_pool_buffer[0x3000];
StringPool bm_name_pool(bm_name_pool_buffer, sizeof(bm_name_pool_buffer), false);
char bm_level_name_pool_buffer[0x4000];
StringPool bm_level_name_pool(bm_level_name_pool_buffer, sizeof(bm_level_name_pool_buffer), false);

// Entries a bitmap takes: 1, or the animation's own entry and one per frame.
static inline int bm_num_entries(const bitmap_entry* bm) {
    return bm->num_frames == 1 ? 1 : bm->num_frames + 1;
}

static inline void peg_list_init(peg_entry* list) {
    list->next = list;
    list->prev = list;
}

static inline void peg_list_remove(peg_entry* peg) {
    peg->prev->next = peg->next;
    peg->next->prev = peg->prev;
    peg->next = NULL;
    peg->prev = NULL;
}

static inline void peg_list_append(peg_entry* list, peg_entry* peg) {
    peg->prev = list->prev;
    peg->next = list;
    list->prev->next = peg;
    list->prev = peg;
}

// bm_update_pow2_flag, bm_hash_add, bm_hash_remove and bm_find_free_entries are static and come
// before their callers: CodeWarrior keeps a caller's values live across a call in the registers
// that an already compiled static function leaves alone, and peg_unload, peg_setup_bitmaps,
// bm_create and bm_release rely on that.

// Sets BM_FLAG_NOT_POW2 for formats 1 and 2 and for sides that are not a power of two.
static void bm_update_pow2_flag(bitmap_entry* bm) {
    if (bm->format == BM_FORMAT_SIZED_2 || bm->format == BM_FORMAT_SIZED_1) {
        bm->flags |= BM_FLAG_NOT_POW2;
        return;
    }
    int bits = 0;
    for (int i = 0; i < 16; i++) {
        bits += (bm->width & (1 << i)) ? 1 : 0;
    }
    if (bits > 1) {
        bm->flags |= BM_FLAG_NOT_POW2;
        return;
    }
    bits = 0;
    for (int j = 0; j < 16; j++) {
        bits += (bm->height & (1 << j)) ? 1 : 0;
    }
    if (bits > 1) {
        bm->flags |= BM_FLAG_NOT_POW2;
    }
}

// Case-insensitive; -1 for NULL.
u32 bm_hash_name(const char* name) {
    u32 hash;
    const char* p;

    if (name == NULL) {
        return (u32)-1;
    }
    p = name;
    hash = 0;
    while (*p) {
        hash = ((hash << 6) | (hash >> 26)) ^ ngps_tolower(*p);
        p++;
    }
    return hash;
}

// Linear probing from hash % BM_HASH_TABLE_SIZE.
static void bm_hash_add(bitmap_entry* bm) {
    u32 key = bm->hash;
    u32 slot = key % BM_HASH_TABLE_SIZE;

    while (bm_hash_table[slot] != -1) {
        key++;
        slot = key % BM_HASH_TABLE_SIZE;
    }
    bm_hash_table[slot] = bm - bm_bitmaps;
}

static void bm_hash_remove(bitmap_entry* bm) {
    u32 key = bm->hash;
    u32 slot = key % BM_HASH_TABLE_SIZE;
    int index = bm - bm_bitmaps;

    while (index != bm_hash_table[slot]) {
        key++;
        slot = key % BM_HASH_TABLE_SIZE;
    }
    bm_hash_table[slot] = -1;
}

// A registered PEG by name, or NULL. Level PEGs are searched first.
peg_entry* peg_find(const char* name) {
    peg_entry* peg;

    for (peg = peg_level_list.next; peg != &peg_level_list; peg = peg->next) {
        if (!(peg->flags & PEG_FLAG_UNREGISTERED) && ngps_stricmp(name, peg->name) == 0) {
            return peg;
        }
    }
    for (peg = peg_permanent_list.next; peg != &peg_permanent_list; peg = peg->next) {
        if (!(peg->flags & PEG_FLAG_UNREGISTERED) && ngps_stricmp(name, peg->name) == 0) {
            return peg;
        }
    }
    return NULL;
}

// Any PEG by name, unregistered ones included, or NULL.
peg_entry* peg_find_any(const char* name) {
    peg_entry* peg;

    for (peg = peg_level_list.next; peg != &peg_level_list; peg = peg->next) {
        if (ngps_stricmp(name, peg->name) == 0) {
            return peg;
        }
    }
    for (peg = peg_permanent_list.next; peg != &peg_permanent_list; peg = peg->next) {
        if (ngps_stricmp(name, peg->name) == 0) {
            return peg;
        }
    }
    return NULL;
}

void* bm_alloc_data(int size) {
    return new u8[size];
}

void bm_free_data(void* data) {
    if (data != NULL) {
        delete[] (u8*)data;
    }
}

// Frees a PEG's data (unless a mempool or a packfile owns it) and its bitmap entries, and puts it
// back on the free list.
void peg_unload(peg_entry* peg) {
    int n;
    int i;
    int j;
    int count;
    bitmap_entry* bm;

    if (peg->data != NULL && !(peg->flags & PEG_FLAG_MEMPOOL) && !(peg->flags & PEG_FLAG_PACKFILE)) {
        bm_free_data(peg->data);
    }
    n = peg->first_bitmap;
    for (i = 0; i < peg->num_bitmaps; i++) {
        bm = &bm_bitmaps[n];
        if (bm->hash != -1) {
            bm_hash_remove(bm);
        }
        bm->flags &= ~BM_FLAG_USED;
        count = bm_num_entries(bm);
        for (j = 0; j < count; j++) {
            bm_bitmaps[n].format = BM_FORMAT_NONE;
            bm_bitmaps[n].hash = -1;
            bm_bitmaps[n].flags &= ~BM_FLAG_USED;
            n++;
        }
    }
    peg->data = NULL;
    peg->mempool = NULL;
    peg->flags = 0;
    peg_list_remove(peg);
    peg_list_append(&peg_free_list, peg);
}

void ccrunch_load(void) {
    char line[0x300];
    char bitmap_name[0x40];
    TextFile file;
    SysFile unused;
    char* token;
    int count;

    ccrunch_num_pegs = 0;
    for (int i = 0; i < CCRUNCH_MAX_PEGS; i++) {
        ccrunch_sizes[i].size_1 = -1;
        ccrunch_sizes[i].size_2 = -1;
    }
    if (file.Open("ccrunch.arr", 9999999) != 0) {
        return;
    }
    while (!file.AtEnd()) {
        file.ReadLine(line, sizeof(line) - 1);
        if (ccrunch_num_pegs < CCRUNCH_MAX_PEGS) {
            token = func_00251270(line, ":");
            if (token != NULL) {
                trim_whitespace(ccrunch_peg_names[ccrunch_num_pegs], token, -1);
                count = 0;
                while (token != NULL) {
                    token = func_00251270(NULL, ",");
                    if (token != NULL && count < CCRUNCH_MAX_BITMAPS) {
                        trim_whitespace(bitmap_name, token, -1);
                        strlwr(bitmap_name);
                        ccrunch_bitmap_crcs[ccrunch_num_pegs][count] = crc32_string(bitmap_name, 0);
                        count++;
                    }
                }
                ccrunch_num_pegs++;
            }
        }
    }
    file.Close();
    ccrunch_loaded = TRUE;
}

// First of count consecutive free entries, searching from bm_next_handle to the end and then from
// the start (from bm_level_start in level mode) up to bm_next_handle, or -1.
static int bm_find_free_entries(int count) {
    int i;
    int start = -1;
    int found = 0;
    int first;

    for (i = bm_next_handle; i < BM_MAX_BITMAPS; i++) {
        if (bm_bitmaps[i].flags & BM_FLAG_USED) {
            start = -1;
            found = 0;
        } else {
            if (start < 0) {
                start = i;
            }
            found++;
        }
        if (found == count) {
            return start;
        }
    }
    first = 0;
    if (bm_level_mode == 1) {
        first = bm_level_start - 1;
    }
    start = -1;
    found = 0;
    for (i = first; i <= bm_next_handle; i++) {
        if (bm_bitmaps[i].flags & BM_FLAG_USED) {
            start = -1;
            found = 0;
        } else {
            if (start < 0) {
                start = i;
            }
            found++;
        }
        if (found == count) {
            return start;
        }
    }
    return -1;
}

// Copies what the loader needs out of the PEG header.
void peg_read_header(peg_header* header, peg_entry* peg, int* header_size, int* data_size, int* alignment,
                     int* version) {
    *version = header->version;
    *header_size = header->header_size;
    *data_size = header->data_size;
    peg->num_bitmaps = header->num_bitmaps;
    peg->num_entries = header->num_entries;
    *alignment = header->alignment;
}

// Fills in bm, and its animation frames if add_frames, from a bitmap header and adds the entries
// used to count. Returns the next header.
peg_bitmap* peg_read_bitmap(peg_bitmap* header, peg_entry* peg, bitmap_entry* bm, int* count, int version,
                            BOOL add_frames) {
    char name[49];
    int num_entries;
    int i;
    bitmap_entry* frame;

    bm->width = header->width;
    bm->height = header->height;
    bm->format = header->format;
    bm->palette_format = header->palette_format;
    bm->flags = header->flags;
    num_entries = header->num_frames;
    bm->num_frames = num_entries;
    bm->fps = header->fps;
    bm->num_mip_levels = header->num_mip_levels;
    bm->unknown_1E = header->unknown_0A;
    bm->unknown_1E += 150;
    bm->num_pixels = bm_get_mip_pixels(bm->width, bm->height, bm->num_mip_levels);
    memset(name, 0, sizeof(name));
    memcpy(name, header->name, sizeof(header->name));
    if (bm_level_mode == 1) {
        bm->name = bm_level_name_pool.AddUnique(name);
    } else {
        bm->name = bm_name_pool.AddUnique(name);
    }
    bm->vram_size = bm_get_vram_size(bm->format, bm->width, bm->height, bm->num_mip_levels);
    bm->parent = -1;
    bm->hash = -1;
    bm->flags |= BM_FLAG_USED;
    bm_update_pow2_flag(bm);
    if (num_entries > 1) {
        num_entries++;
    }
    if (add_frames) {
        for (i = 1; i < num_entries; i++) {
            frame = &bm_bitmaps[i + (bm_next_handle + *count)];
            frame->width = bm->width;
            frame->height = bm->height;
            frame->format = bm->format;
            frame->palette_format = bm->palette_format;
            frame->flags = bm->flags;
            frame->num_frames = 1;
            frame->fps = bm->fps;
            frame->num_mip_levels = bm->num_mip_levels;
            frame->unknown_1E = bm->unknown_1E;
            frame->num_pixels = bm->num_pixels;
            frame->vram_size = bm->vram_size;
            frame->parent = bm_next_handle + *count;
            frame->name = NULL;
        }
    }
    header = (peg_bitmap*)(((u32)(header + 1) + 3) & ~3);
    *count += num_entries;
    return header;
}

// Points the bitmaps of a PEG at their pixels and palettes, which follow the headers in order, each
// starting on a multiple of alignment, and adds the bitmaps to the hash table. Unregistered PEGs only
// add their animations.
void peg_setup_bitmaps(peg_entry* peg, u8* data, int alignment, int first, int* count, BOOL unregistered) {
    int palette_size = 0;
    int offset = 0;
    int i;
    int j;
    int num_entries;
    bitmap_entry* bm;
    u32 hash;
    int remainder;
    u8* palette;

    *count = 0;
    for (i = 0; i < peg->num_bitmaps; i++) {
        bm = &bm_bitmaps[first + *count];
        hash = -1;
        if (bm_find(bm->name) < 0) {
            hash = bm_hash_name(bm->name);
        }
        num_entries = bm->num_frames;
        j = 0;
        if (num_entries > 1) {
            bm->data = NULL;
            bm->data_size = 0;
            num_entries++;
            bm->palette = NULL;
            j = 1;
            bm->hash = hash;
            bm_hash_add(bm);
            bm++;
        }
        for (; j < num_entries; j++, bm++) {
            bm->palette = NULL;
            bm->data = NULL;
            switch (bm->format) {
            case BM_FORMAT_SIZED_1:
            case BM_FORMAT_SIZED_2:
                bm->data = data + offset;
                bm->data_size = *(int*)bm->data;
                offset += *(int*)bm->data;
                break;
            case BM_FORMAT_PEG_32BIT:
                bm->data = data + offset;
                bm->data_size = bm->num_pixels * 4;
                offset += bm->num_pixels * 4;
                break;
            case BM_FORMAT_16BIT:
                bm->data = data + offset;
                bm->data_size = bm->num_pixels * 2;
                offset += bm->num_pixels * 2;
                break;
            case BM_FORMAT_8BIT:
                palette = data + offset;
                bm->palette = palette;
                if (bm->palette_format == BM_PALETTE_16BIT) {
                    palette_size = 0x200;
                } else if (bm->palette_format == BM_PALETTE_32BIT) {
                    palette_size = 0x400;
                }
                bm->data = palette + palette_size;
                bm->data_size = palette_size + bm->num_pixels;
                offset += palette_size + bm->num_pixels;
                break;
            case BM_FORMAT_4BIT:
                palette = data + offset;
                bm->palette = palette;
                if (bm->palette_format == BM_PALETTE_16BIT) {
                    palette_size = 0x20;
                } else if (bm->palette_format == BM_PALETTE_32BIT) {
                    palette_size = 0x40;
                }
                bm->data = palette + palette_size;
                bm->data_size = palette_size + bm->num_pixels / 2;
                offset += palette_size + bm->num_pixels / 2;
                break;
            }
            if (j == 0 && hash != -1 && !unregistered) {
                bm->hash = hash;
                bm_hash_add(bm);
            }
            remainder = offset % alignment;
            offset += remainder == 0 ? 0 : alignment - remainder;
        }
        *count += num_entries;
    }
}

// Loads a PEG, from memory if memory is not NULL, and returns its index in peg_entries. A registered
// PEG that is already loaded is not loaded again. Level PEGs go on peg_level_list unless unregistered.
int peg_load(const char* filename, BOOL unregistered, BOOL level, peg_memory* memory) {
    SysFile file;
    int i;
    peg_entry* peg;
    u8* data;
    int header_size;
    int data_size;
    int count;
    int version;
    int alignment;
    int size;
    int first;

    peg = peg_find(filename);
    if (peg != NULL && !unregistered) {
        return peg - peg_entries;
    }
    peg = peg_free_list.next;
    strcpy(peg->name, filename);
    strlwr(peg->name);
    if (memory == NULL) {
        data = packfile_find_cond(filename, &size);
        if (data != NULL) {
            peg->data = data;
            peg->size = size;
            peg->flags |= PEG_FLAG_PACKFILE;
        } else {
            if (file.Open(filename, SYSFILE_MODE_READ, 9999999) < 0) {
                sprintf(string_buffer, "ERROR OPENING PEG (%s)\n", filename);
                DEBUGERRORLINE(1172, string_buffer);
            }
            size = file.GetSize(NULL);
            if (size == 0) {
                sprintf(string_buffer, "%s is empty.\n", filename);
                DEBUGERRORLINE(1178, string_buffer);
            }
            data = (u8*)bm_alloc_data(size);
            peg->data = data;
            file.Read(data, size, 0, false);
            peg->size = size;
            file.Close();
        }
    } else {
        data = memory->data;
        peg->data = data;
        peg->size = memory->size_3 + (memory->size_1 + memory->size_2);
    }
    peg_read_header((peg_header*)data, peg, &header_size, &data_size, &alignment, &version);
    data += sizeof(peg_header);
    first = bm_find_free_entries(peg->num_entries);
    if (first < 0) {
        sprintf(string_buffer, "Could not get enough bitmap entries\nto load peg %s\n", filename);
        DEBUGERRORLINE(1213, string_buffer);
    }
    bm_next_handle = first;
    peg->first_bitmap = first;
    count = 0;
    for (i = 0; i < peg->num_bitmaps; i++) {
        data = (u8*)peg_read_bitmap((peg_bitmap*)data, peg, &bm_bitmaps[bm_next_handle + count], &count, version,
                                    TRUE);
    }
    peg_setup_bitmaps(peg, data, alignment, bm_next_handle, &count, unregistered);
    peg_list_remove(peg);
    if (level && !unregistered) {
        peg_list_append(&peg_level_list, peg);
    } else {
        peg_list_append(&peg_permanent_list, peg);
    }
    bm_next_handle += count;
    peg->mempool = NULL;
    if (memory == NULL) {
        if (is_mempool_active()) {
            peg->flags |= PEG_FLAG_MEMPOOL;
            peg->mempool = get_current_mempool();
        }
    } else if (memory->mempool != NULL) {
        peg->mempool = memory->mempool;
        peg->flags |= PEG_FLAG_MEMPOOL;
    }
    if (unregistered) {
        peg->flags |= PEG_FLAG_UNREGISTERED;
    }
    return peg - peg_entries;
}

void bm_init(void) {
    int i;

    bm_name_pool.Reset();
    bm_level_name_pool.Reset();
    bm_next_handle = 0;
    for (i = 0; i < BM_MAX_BITMAPS; i++) {
        bm_bitmaps[i].hash = -1;
    }
    for (i = 0; i < BM_MAX_BITMAPS; i++) {
        bm_hash_table[i] = -1;
    }
    peg_list_init(&peg_free_list);
    peg_list_init(&peg_level_list);
    peg_list_init(&peg_permanent_list);
    for (i = 0; i < BM_MAX_PEGS; i++) {
        peg_entries[i].flags = 0;
        peg_list_append(&peg_free_list, &peg_entries[i]);
    }
}

void bm_set_default(void) {
    bm_default_handle = bm_find("misc-static.tga");
}

// Handle of the bitmap called name, or bm_default_handle if there is none. With peg >= 0 only that
// PEG's bitmaps are searched.
int bm_load(const char* name, int unused, int peg) {
    peg_entry* entry;
    bitmap_entry* bm;
    int handle;
    int i;

    if (peg >= 0) {
        entry = &peg_entries[peg];
        handle = entry->first_bitmap;
        for (i = 0; i < entry->num_bitmaps; i++) {
            bm = &bm_bitmaps[handle];
            if (ngps_stricmp(name, bm->name) == 0) {
                return handle;
            }
            handle += bm_num_entries(bm);
        }
        return bm_default_handle;
    }
    handle = bm_find(name);
    if (handle < 0) {
        return bm_default_handle;
    }
    return handle;
}

// Returns the new handle, or -1. With convert set the pixels are converted from source_format into
// a new buffer; otherwise the bitmap uses data as it is.
int bm_create(int source_format, int width, int height, u8* data, u8* unused, int format, bool convert) {
    int handle;
    int bytes_per_pixel;
    int num_pixels;
    bitmap_entry* bm;

    handle = bm_find_free_entries(1);
    if (handle < 0) {
        return -1;
    }
    bm_next_handle = handle;
    if (format == BM_FORMAT_32BIT) {
        bytes_per_pixel = 4;
    } else {
        bytes_per_pixel = 2;
    }
    bm = &bm_bitmaps[handle];
    bm->name = NULL;
    num_pixels = width * height;
    bm->hash = -1;
    bm->format = format;
    bm->width = width;
    bm->height = height;
    bm->num_mip_levels = 1;
    bm->unknown_1E = 150;
    bm->num_pixels = num_pixels;
    bm->data_size = bytes_per_pixel * num_pixels;
    bm->fps = 0;
    bm->num_frames = 1;
    bm->palette = NULL;
    bm->flags = BM_FLAG_USED;
    bm->parent = -1;
    bm_update_pow2_flag(bm);
    bm->data = NULL;
    if (data != NULL) {
        if (convert) {
            if (format == BM_FORMAT_32BIT) {
                bm->data = (u8*)convert_texture_to_32(data, source_format, num_pixels, NULL);
            } else {
                bm->data = (u8*)convert_texture_to_16(data, source_format, num_pixels, NULL);
            }
        } else {
            bm->data = data;
        }
        if (bm->data == NULL) {
            return -1;
        }
    }
    bm->vram_size = bm_get_vram_size(bm->format, width, height, bm->num_mip_levels);
    return bm_next_handle++;
}

void bm_release(int handle) {
    bitmap_entry* bm;

    if (handle < 0 || handle >= BM_MAX_BITMAPS) {
        return;
    }
    if (handle == bm_default_handle) {
        return;
    }
    bm = &bm_bitmaps[handle];
    if (bm->hash != (u32)-1) {
        bm_hash_remove(bm);
    }
    // Only bitmaps made by bm_create give their entry back.
    if (bm->format == BM_FORMAT_32BIT || bm->format == BM_FORMAT_CREATED_16S ||
        bm->format == BM_FORMAT_CREATED_16) {
        bm->format = BM_FORMAT_NONE;
        bm->flags &= ~BM_FLAG_USED;
    }
}

void bm_get_data(int handle, u8** data, u8** palette) {
    bitmap_entry* bm = &bm_bitmaps[handle];

    *data = bm->data;
    *palette = bm->palette;
}

// Handle of the animation frame to show now.
int bm_get_frame(int handle) {
    bitmap_entry* bm = &bm_bitmaps[handle];

    if (bm->num_frames == 1) {
        return handle;
    }
    return handle + (u32)(int)(milliseconds_unpaused * (bm->fps * 0.001f)) % bm->num_frames + 1;
}

// Handle of the registered bitmap called name, or -1.
int bm_find(const char* name) {
    u32 hash;
    u32 index;
    int handle;
    bitmap_entry* bm;

    hash = bm_hash_name(name);
    handle = bm_hash_table[hash % BM_HASH_TABLE_SIZE];
    index = hash;
    while (handle != -1) {
        bm = &bm_bitmaps[handle];
        if (bm->hash == hash && ngps_stricmp(bm->name, name) == 0) {
            return handle;
        }
        index++;
        handle = bm_hash_table[index % BM_HASH_TABLE_SIZE];
    }
    return -1;
}

void bm_get_dimensions(int handle, int* width, int* height) {
    bitmap_entry* bm = &bm_bitmaps[handle];

    *width = bm->width;
    *height = bm->height;
}

void bm_get_info(int handle, int* width, int* height, int* num_pixels, int* num_mip_levels) {
    bitmap_entry* bm = &bm_bitmaps[handle];

    *width = bm->width;
    *height = bm->height;
    *num_pixels = bm->num_pixels;
    *num_mip_levels = bm->num_mip_levels;
}

// Sets duration to the length of the animation in seconds.
int bm_get_num_frames(int handle, float* duration) {
    bitmap_entry* bm = &bm_bitmaps[handle];

    *duration = (float)bm->num_frames / (float)bm->fps;
    return bm->num_frames;
}

int bm_get_format(int handle) {
    return bm_bitmaps[handle].format;
}

int bm_get_palette_format(int handle) {
    return bm_bitmaps[handle].palette_format;
}

// With any set, PEGs loaded unregistered are found too. The PEG must be loaded.
void bm_get_peg_data(const char* name, u8** data, bool any) {
    peg_entry* peg = peg_find(name);

    if (peg == NULL && any) {
        peg = peg_find_any(name);
    }
    *data = peg->data;
}

// Copies the name of the PEG holding the bitmap to peg_name. Lowercases bitmap_name.
BOOL ccrunch_find_peg(char* bitmap_name, char* peg_name) {
    u32 crc;
    int i;
    int j;

    strlwr(bitmap_name);
    crc = crc32_string(bitmap_name, 0);
    for (i = 0; i < ccrunch_num_pegs; i++) {
        for (j = 0; j < CCRUNCH_MAX_BITMAPS; j++) {
            if (crc == ccrunch_bitmap_crcs[i][j]) {
                strcpy(peg_name, ccrunch_peg_names[i]);
                return TRUE;
            }
        }
    }
    return FALSE;
}

// Like ccrunch_find_peg, but points peg_name at the name.
BOOL ccrunch_find_peg_name(char* bitmap_name, char** peg_name) {
    u32 crc;
    int i;
    int j;

    strlwr(bitmap_name);
    crc = crc32_string(bitmap_name, 0);
    for (i = 0; i < ccrunch_num_pegs; i++) {
        for (j = 0; j < CCRUNCH_MAX_BITMAPS; j++) {
            if (crc == ccrunch_bitmap_crcs[i][j]) {
                *peg_name = ccrunch_peg_names[i];
                return TRUE;
            }
        }
    }
    return FALSE;
}

// Loads the PEG holding the bitmap.
BOOL ccrunch_load_peg(char* bitmap_name) {
    char peg_name[128];

    if (!ccrunch_find_peg(bitmap_name, peg_name)) {
        return FALSE;
    }
    return bm_load_peg(peg_name, NULL) >= 0;
}

BOOL ccrunch_unload_peg(char* bitmap_name) {
    char peg_name[128];

    if (!ccrunch_find_peg(bitmap_name, peg_name)) {
        return FALSE;
    }
    bm_unload_peg(peg_name);
    return TRUE;
}

// Sizes of the two sections after a PEG's header, read from stream.pre.
void ccrunch_set_peg_sizes(const char* peg_name, int size_1, int size_2) {
    int i;

    for (i = 0; i < ccrunch_num_pegs; i++) {
        if (ngps_stricmp(peg_name, ccrunch_peg_names[i]) == 0) {
            ccrunch_sizes[i].size_1 = size_1;
            ccrunch_sizes[i].size_2 = size_2;
        }
    }
}

BOOL ccrunch_get_peg_sizes(const char* peg_name, int* size_1, int* size_2) {
    int i;

    for (i = 0; i < ccrunch_num_pegs; i++) {
        if (ngps_stricmp(peg_name, ccrunch_peg_names[i]) == 0) {
            *size_1 = ccrunch_sizes[i].size_1;
            *size_2 = ccrunch_sizes[i].size_2;
            return TRUE;
        }
    }
    return FALSE;
}

// Reads the PEG's data again, from filename, into the same buffer.
void bm_reload_peg(const char* peg_name, const char* filename) {
    peg_entry* peg = peg_find(peg_name);
    SysFile file;

    if (file.Open(filename, SYSFILE_MODE_READ, 9999999)) {
        sprintf(string_buffer, "Could not open %s\n", filename);
        DEBUGERRORLINE(1809, string_buffer);
    }
    file.Read(peg->data, peg->size, 0, false);
    file.Close();
}

BOOL bm_peg_loaded(const char* name) {
    return peg_find(name) ? TRUE : FALSE;
}

// Returns the PEG's index. During a level the PEG goes on the level list, to be unloaded by
// bm_level_unload. memory is NULL unless the PEG is already in memory.
int bm_load_peg(const char* name, peg_memory* memory) {
    return peg_load(name, 0, bm_level_mode == true, memory);
}

void bm_unload_peg(const char* name) {
    peg_entry* peg = peg_find(name);

    if (peg != NULL) {
        peg_unload(peg);
    }
}

// Loads a new copy even if the PEG is loaded already. Its bitmaps are not found by name.
int bm_load_peg_unregistered(const char* name, peg_memory* memory) {
    return peg_load(name, 1, 0, memory);
}

void bm_unload_peg_index(int index) {
    if (index >= 0) {
        peg_unload(&peg_entries[index]);
    }
}

// PEGs and bitmap names loaded from now on belong to the level.
void bm_level_start_mark(void) {
    bm_level_mode = true;
    bm_level_start = bm_next_handle;
}

// Unloads the level's PEGs and names, and reuses its bitmap entries.
void bm_level_unload(void) {
    peg_entry* peg;
    peg_entry* next;

    peg = peg_level_list.next;
    next = peg->next;
    while (peg != &peg_level_list) {
        peg_unload(peg);
        peg = next;
        next = next->next;
    }
    bm_level_mode = false;
    bm_next_handle = bm_level_start;
    bm_level_name_pool.Reset();
}

BOOL bm_is_pow2(int handle) {
    return !(bm_bitmaps[handle].flags & BM_FLAG_NOT_POW2);
}

// Pixels in num_mip_levels levels, each half the size of the one before.
int bm_get_mip_pixels(int width, int height, int num_mip_levels) {
    int total = 0;
    int i;

    for (i = 0; i < num_mip_levels; i++) {
        total += width * height;
        width /= 2;
        height /= 2;
    }
    return total;
}

// In 256-byte GS blocks. Each mip level is padded to whole GS pages (64x32 pixels at 32 bits, 64x64
// at 16, 128x64 at 8 and 128x128 at 4) and a paletted bitmap adds a page for its palette.
u32 bm_get_vram_size(int format, int width, int height, int num_mip_levels) {
    u32 words = 0;
    u32 palette_words = 0;
    int i;
    int rows;
    int row_words;

    for (i = 0; i < num_mip_levels; i++) {
        switch (format) {
        case BM_FORMAT_CREATED_16S:
        case BM_FORMAT_CREATED_16:
        case BM_FORMAT_16BIT:
        case BM_FORMAT_SIZED_1:
            rows = (height + 63) & ~63;
            row_words = (width + 63) & ~63;
            row_words /= 2;
            break;
        case BM_FORMAT_8BIT:
            rows = (height + 63) & ~63;
            row_words = (width + 127) & ~127;
            row_words /= 4;
            palette_words = 0x800;
            break;
        case BM_FORMAT_4BIT:
            rows = (height + 127) & ~127;
            row_words = (width + 127) & ~127;
            row_words /= 8;
            palette_words = 0x800;
            break;
        default:
            rows = (height + 31) & ~31;
            row_words = (width + 63) & ~63;
            break;
        }
        words += row_words * rows;
        width /= 2;
        height /= 2;
    }
    return (palette_words + words) / 64;
}
