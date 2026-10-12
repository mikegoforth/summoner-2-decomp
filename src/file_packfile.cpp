// Packfiles: the .vpp archives the game's files are read from, on the disc, the hard disk or the host.
// Only the asserts name this file; the function names are descriptive guesses.
#include "game/file_packfile.h"
#include "game/debug.h"
#include "game/memory.h"
#include "game/ngps_file.h"
#include "game/platform_ngps.h"
#include "macros.h"
#include <stdio.h>
#include <string.h>

extern "C" {
// TODO: name these once their units are split.
// ngps_dvd.cpp: files on the disc, opened by sector.
void* func_001A3460(int sector, int size); // open
int func_001A3570(void* file, int offset); // seek
int func_001A36C0(void* file, u8* buffer, int size); // read
int func_001A3780(void* file, u8* buffer, int size); // read and decompress
int func_001A3A40(void* file, u8* buffer, int size); // start an asynchronous read
void func_001A3B10(void* file); // close
int func_001A2E50(void* file); // status of the last request, 0 while busy
// Reads size bytes of compressed data from a host file and decompresses them into buffer.
void func_001A3BD0(u8* buffer, int size, NGPS_FILE* file);
// Hard disk files (pfs0:).
void* func_0025DD90(const char* filename, const char* mode); // open
int func_0025DF40(void* file, int offset, int origin); // seek
int func_0025E050(void* file, u8* buffer, int size); // read
int func_0025E0F0(void* file, u8* buffer, int size); // start an asynchronous read
void func_0025E180(void* file); // close
int func_0025E290(void* file, u8* buffer, int size); // read and decompress
// Earlier in this file.
u32 func_001A3E40(const char* name); // name hash, case-insensitive
void func_001A3F00(void* destination, int size, NGPS_FILE* file); // read size bytes in whole sectors
void func_001A3FA0(void* destination, int size, void* file); // same, from the hard disk
void func_001A4040(void* destination, int size, void* file); // same, from the disc
void func_001A41D0(Packfile* packfile); // fixes up the directory and fills Packfile_lookup
void func_001A42B0(const char* filename, char* name, char* path); // splits off the directory
int func_001A4370(const char* filename, int* sector); // PACKFILE_LOCATION_*, and the disc sector
BOOL func_001A4500(Packfile* packfile, u8* header); // checks the header sector and copies it in
}

PackfileLookup Packfile_lookup[MAX_PACKFILE_ENTRIES];
Packfile Packfiles[MAX_PACKFILES];
Packfile Cond_packfiles[MAX_COND_PACKFILES];
int Cond_packfile_state[MAX_COND_PACKFILES];
char Packfile_pool_buffer[PACKFILE_POOL_SIZE];
char Cond_packfile_pool_buffer[COND_PACKFILE_POOL_SIZE];
Mempool Packfile_pool;      // directories and names of the regular packfiles
Mempool Cond_packfile_pool; // directories and names of the conditional packfiles

int Num_packfile_entries;
int Num_packfiles;
bool Summoner_vpp_loaded;
int Num_cond_packfiles;
BOOL Packfiles_locked;
bool Cond_packfiles_in_main_pool;

// Allocates the directory and names blocks. Returns the names block, or NULL.
void* packfile_alloc_directory(Packfile* packfile) {
    Mempool* pool;
    Mempool* old_pool;

    if ((packfile->flags & PACKFILE_CONDITIONAL) && !Cond_packfiles_in_main_pool) {
        if (Cond_packfile_state[Num_cond_packfiles - 1] != COND_PACKFILE_ADDED) {
            return NULL;
        }
        pool = &Cond_packfile_pool;
    } else {
        pool = &Packfile_pool;
    }

    if ((int)pool->getPoolSpaceLeft() < packfile->directory_size + packfile->names_size + 32) {
        sprintf(string_buffer, "Out of space in %s. size: %d\n", pool->pool_name, pool->size);
        DEBUGERRORLINE(938, string_buffer);
        return NULL;
    }

    old_pool = get_current_mempool();
    set_current_mempool(pool);
    packfile->entries = (PackfileEntry*)new u8[packfile->directory_size];
    packfile->names = new char[packfile->names_size];
    set_current_mempool(old_pool);
    return packfile->names;
}

// Reads a conditional packfile's header; data_size gets the size of the buffer it needs.
BOOL packfile_read_cond_header_host(Packfile* packfile, int* data_size) {
    NGPS_FILE* file;
    char filename[128];
    u8 header[PACKFILE_SECTOR_SIZE];

    if (Cond_packfile_state[Num_cond_packfiles]) {
        return FALSE;
    }
    sprintf(filename, "%s%s", packfile->path, packfile->name);
    file = ngps_fopen(filename, "rb");
    if (file == NULL) {
        return FALSE;
    }
    packfile->flags |= PACKFILE_HOST;
    ngps_fread(header, PACKFILE_SECTOR_SIZE, 1, file);
    if (!func_001A4500(packfile, header)) {
        ngps_fclose(file);
        return FALSE;
    }
    ngps_fclose(file);
    *data_size = packfile->data_size;
    return TRUE;
}

BOOL packfile_read_cond_header_hdd(Packfile* packfile, int* data_size) {
    void* file;
    char filename[128];
    u8 header[PACKFILE_SECTOR_SIZE];

    if (Cond_packfile_state[Num_cond_packfiles]) {
        return FALSE;
    }
    sprintf(filename, "%s%s", packfile->path, packfile->name);
    file = func_0025DD90(filename, "rb");
    if (file == NULL) {
        return FALSE;
    }
    packfile->flags |= PACKFILE_HDD;
    func_0025E050(file, header, PACKFILE_SECTOR_SIZE);
    if (!func_001A4500(packfile, header)) {
        func_0025E180(file);
        return FALSE;
    }
    func_0025E180(file);
    *data_size = packfile->data_size;
    return TRUE;
}

BOOL packfile_read_cond_header_dvd(Packfile* packfile, int* data_size) {
    void* file;
    u8 header[PACKFILE_SECTOR_SIZE];

    if (Cond_packfile_state[Num_cond_packfiles]) {
        return FALSE;
    }
    if (packfile->sector == -1) {
        return FALSE;
    }
    file = func_001A3460(packfile->sector, -1);
    if (file == NULL) {
        return FALSE;
    }
    packfile->flags |= PACKFILE_DVD;
    func_001A36C0(file, header, PACKFILE_SECTOR_SIZE);
    if (!func_001A4500(packfile, header)) {
        func_001A3B10(file);
        return FALSE;
    }
    func_001A3B10(file);
    *data_size = packfile->data_size;
    return TRUE;
}

// Reads a regular packfile's header, directory and names.
BOOL packfile_load_dvd(Packfile* packfile) {
    void* file;
    u8 header[PACKFILE_SECTOR_SIZE];

    if (packfile->sector == -1) {
        return FALSE;
    }
    file = func_001A3460(packfile->sector, -1);
    if (file == NULL) {
        return FALSE;
    }
    packfile->flags |= PACKFILE_DVD;
    func_001A36C0(file, header, PACKFILE_SECTOR_SIZE);
    if (!func_001A4500(packfile, header)) {
        func_001A3B10(file);
        return FALSE;
    }
    if (!packfile_alloc_directory(packfile)) {
        func_001A3B10(file);
        DEBUGERRORLINE(1115, "Cannot allocate enough space for all files in the game!\n");
        return FALSE;
    }
    func_001A4040(packfile->entries, packfile->directory_size, file);
    func_001A4040(packfile->names, packfile->names_size, file);
    func_001A41D0(packfile);
    func_001A3B10(file);
    return TRUE;
}

BOOL packfile_load_hdd(Packfile* packfile) {
    void* file;
    u8 header[PACKFILE_SECTOR_SIZE];

    // The directory part of the path is not used on the hard disk.
    file = func_0025DD90(packfile->name, "rb");
    if (file == NULL) {
        return FALSE;
    }
    packfile->flags |= PACKFILE_HDD;
    func_0025E050(file, header, PACKFILE_SECTOR_SIZE);
    if (!func_001A4500(packfile, header)) {
        func_0025E180(file);
        return FALSE;
    }
    if (!packfile_alloc_directory(packfile)) {
        func_0025E180(file);
        DEBUGERRORLINE(1171, "Cannot allocate enough space for all files in the game!\n");
        return FALSE;
    }
    func_001A3FA0(packfile->entries, packfile->directory_size, file);
    func_001A3FA0(packfile->names, packfile->names_size, file);
    func_001A41D0(packfile);
    func_0025E180(file);
    return TRUE;
}

BOOL packfile_load_host(Packfile* packfile) {
    NGPS_FILE* file;
    char filename[128];
    u8 header[PACKFILE_SECTOR_SIZE];

    sprintf(filename, "%s%s", packfile->path, packfile->name);
    file = ngps_fopen(filename, "rb");
    if (file == NULL) {
        return FALSE;
    }
    packfile->flags = PACKFILE_HOST;
    ngps_fread(header, PACKFILE_SECTOR_SIZE, 1, file);
    if (!func_001A4500(packfile, header)) {
        ngps_fclose(file);
        return FALSE;
    }
    if (!packfile_alloc_directory(packfile)) {
        ngps_fclose(file);
        return FALSE;
    }
    func_001A3F00(packfile->entries, packfile->directory_size, file);
    func_001A3F00(packfile->names, packfile->names_size, file);
    func_001A41D0(packfile);
    ngps_fclose(file);
    return TRUE;
}

void packfile_init(void) {
    int i;

    Num_packfiles = 0;
    Num_packfile_entries = 0;
    Packfiles_locked = FALSE;
    Summoner_vpp_loaded = false;
    Packfile_pool.Copy((u8*)Packfile_pool_buffer, PACKFILE_POOL_SIZE, "Packfile", 16);
    Cond_packfile_pool.Copy((u8*)Cond_packfile_pool_buffer, COND_PACKFILE_POOL_SIZE, "Cond. Packfile", 16);
    if (packfile_add("s2assets\\summoner.vpp")) {
        Summoner_vpp_loaded = true;
    }
    packfile_add("s2assets\\tables.vpp");
    packfile_add("s2assets\\chars.vpp");
    packfile_add("s2assets\\items.vpp");
    packfile_add("s2assets\\effects.vpp");
    packfile_add("s2assets\\sounds.vpp");
    packfile_add("s2assets\\music.vpp");
    Num_cond_packfiles = 0;
    for (i = 0; i < MAX_COND_PACKFILES; i++) {
        Cond_packfile_state[i] = COND_PACKFILE_FREE;
    }
}

bool packfile_summoner_vpp_loaded(void) { return Summoner_vpp_loaded; }

// Finds a file of the regular packfiles by name; the directory part of filename is ignored.

PackfileEntry* packfile_find(const char* filename) {
    PackfileLookup* lookup;
    int i;
    u32 hash;
    char name[64];

    strip_path(name, filename);
    hash = func_001A3E40(name);
    for (i = Num_packfile_entries - 1; i >= 0; i--) {
        lookup = &Packfile_lookup[i];
        if (hash == lookup->hash) {
            if (lookup->entry->name == NULL) {
                return NULL;
            }
            if (ngps_stricmp(name, lookup->entry->name) == 0) {
                return lookup->entry;
            }
        }
    }
    return NULL;
}

BOOL packfile_add(const char* filename) {
    int location;
    Packfile* packfile;
    char name[32];
    char path[44];
    int sector;

    if (Packfiles_locked) {
        return FALSE;
    }
    if (Num_packfiles >= MAX_PACKFILES) {
        return FALSE;
    }
    location = func_001A4370(filename, &sector);
    if (location == PACKFILE_LOCATION_NONE) {
        return FALSE;
    }
    func_001A42B0(filename, name, path);
    packfile = &Packfiles[Num_packfiles++];
    strncpy(packfile->name, name, 12);
    strncpy(packfile->path, path, 30);
    packfile->sector = sector;
    packfile->num_files = 0;
    packfile->entries = NULL;
    if (location == PACKFILE_LOCATION_HOST) {
        return packfile_load_host(packfile);
    } else if (location == PACKFILE_LOCATION_HDD) {
        return packfile_load_hdd(packfile);
    } else {
        return packfile_load_dvd(packfile);
    }
}

BOOL packfile_add_cond(const char* filename, int* data_size) {
    int location;
    Packfile* packfile;
    char name[32];
    char path[44];
    int sector;

    // Never true: the range check uses && where || was meant.
    if (Num_cond_packfiles < 0 && Num_cond_packfiles >= MAX_COND_PACKFILES) {
        return FALSE;
    }
    if (Cond_packfile_state[Num_cond_packfiles]) {
        return FALSE;
    }
    location = func_001A4370(filename, &sector);
    if (location == PACKFILE_LOCATION_NONE) {
        return FALSE;
    }
    func_001A42B0(filename, name, path);
    packfile = &Cond_packfiles[Num_cond_packfiles];
    strncpy(packfile->name, name, 12);
    strncpy(packfile->path, path, 30);
    packfile->sector = sector;
    packfile->num_files = 0;
    packfile->entries = NULL;
    if (location == PACKFILE_LOCATION_HOST) {
        if (!packfile_read_cond_header_host(packfile, data_size)) {
            return FALSE;
        }
    } else if (location == PACKFILE_LOCATION_HDD) {
        if (!packfile_read_cond_header_hdd(packfile, data_size)) {
            return FALSE;
        }
    } else {
        if (!packfile_read_cond_header_dvd(packfile, data_size)) {
            return FALSE;
        }
    }
    Cond_packfile_state[Num_cond_packfiles] = COND_PACKFILE_ADDED;
    Num_cond_packfiles++;
    return TRUE;
}

// File access within a packfile. Offsets are relative to the start of the entry.
BOOL packfile_fopen(PackfileEntry* entry) {
    char filename[128];

    if (entry == NULL || entry->packfile == NULL) {
        return FALSE;
    }
    if (entry->handle != NULL || entry->file != NULL) {
        return FALSE;
    }
    if (entry->packfile->flags & PACKFILE_DVD) {
        entry->handle = func_001A3460(entry->offset + entry->packfile->sector, entry->size);
        return entry->handle != NULL;
    }
    if (entry->packfile->flags & PACKFILE_HDD) {
        sprintf(filename, "%s%s", entry->packfile->path, entry->packfile->name);
        entry->handle = func_0025DD90(filename, "rb");
        if (entry->handle == NULL) {
            return FALSE;
        }
        func_0025DF40(entry->handle, entry->offset * PACKFILE_SECTOR_SIZE, 0);
        return TRUE;
    }
    sprintf(filename, "%s%s", entry->packfile->path, entry->packfile->name);
    entry->file = ngps_fopen(filename, "rb");
    if (entry->file == NULL) {
        return FALSE;
    }
    ngps_fseek(entry->file, entry->offset * PACKFILE_SECTOR_SIZE, 0);
    return TRUE;
}

void packfile_fclose(PackfileEntry* entry) {
    if (entry == NULL || entry->packfile == NULL) {
        return;
    }
    if (entry->packfile->flags & PACKFILE_DVD) {
        func_001A3B10(entry->handle);
        entry->handle = NULL;
    } else if (entry->packfile->flags & PACKFILE_HDD) {
        func_0025E180(entry->handle);
        entry->handle = NULL;
    } else if (entry->file != NULL) {
        ngps_fclose(entry->file);
        entry->file = NULL;
    }
}

// Disc files ignore origin.
int packfile_fseek(PackfileEntry* entry, int offset, int origin) {
    if (entry == NULL || entry->packfile == NULL) {
        return 0;
    }
    if (entry->packfile->flags & PACKFILE_DVD) {
        return func_001A3570(entry->handle, offset);
    }
    if (entry->packfile->flags & PACKFILE_HDD) {
        return func_0025DF40(entry->handle, offset + entry->offset * PACKFILE_SECTOR_SIZE, origin);
    }
    if (entry->file == NULL) {
        return 0;
    }
    return !ngps_fseek(entry->file, offset + entry->offset * PACKFILE_SECTOR_SIZE, origin);
}

int packfile_fread(void* buffer, int size, int count, PackfileEntry* entry, bool async) {
    if (entry == NULL || entry->packfile == NULL) {
        return 0;
    }
    if (entry->packfile->flags & PACKFILE_DVD) {
        if (async) {
            return func_001A3A40(entry->handle, (u8*)buffer, size * count);
        } else {
            return func_001A36C0(entry->handle, (u8*)buffer, size * count);
        }
    }
    if (entry->packfile->flags & PACKFILE_HDD) {
        if (async) {
            return func_0025E0F0(entry->handle, (u8*)buffer, size * count);
        } else {
            return func_0025E050(entry->handle, (u8*)buffer, size * count);
        }
    }
    if (entry->file != NULL) {
        return ngps_fread(buffer, size, count, entry->file);
    }
    return 0;
}

int packfile_fstatus(PackfileEntry* entry) {
    if (entry->handle != NULL) {
        return func_001A2E50(entry->handle);
    }
    return -1;
}

// Sets the buffer the last added conditional packfile is loaded into.
void packfile_set_cond_buffer(u8* buffer) { Cond_packfiles[Num_cond_packfiles - 1].data = buffer; }

// Reads the last added conditional packfile's directory, names and data.
BOOL packfile_load_cond_host(Packfile* packfile) {
    NGPS_FILE* file;
    char filename[128];

    if (Cond_packfile_state[Num_cond_packfiles - 1] != COND_PACKFILE_ADDED) {
        return FALSE;
    }
    sprintf(filename, "%s%s", packfile->path, packfile->name);
    file = ngps_fopen(filename, "rb");
    if (file == NULL) {
        return FALSE;
    }
    ngps_fseek(file, PACKFILE_SECTOR_SIZE, 0);
    if (!packfile_alloc_directory(packfile)) {
        ngps_fclose(file);
        DEBUGERRORLINE(1793, "Cannot allocate enough space for all files in the game!\n");
        return FALSE;
    }
    func_001A3F00(packfile->entries, packfile->directory_size, file);
    func_001A3F00(packfile->names, packfile->names_size, file);
    func_001A41D0(packfile);
    if (packfile->compressed_size != -1) {
        func_001A3BD0(packfile->data, packfile->compressed_size, file);
    } else {
        ngps_fread(packfile->data, packfile->data_size, 1, file);
    }
    ngps_fclose(file);
    Cond_packfile_state[Num_cond_packfiles - 1] = COND_PACKFILE_LOADED;
    return TRUE;
}

BOOL packfile_load_cond_hdd(Packfile* packfile) {
    void* file;
    char filename[128];

    sprintf(filename, "%s%s", packfile->path, packfile->name);
    file = func_0025DD90(filename, "rb");
    if (file == NULL) {
        return FALSE;
    }
    func_0025DF40(file, PACKFILE_SECTOR_SIZE, 0);
    if (!packfile_alloc_directory(packfile)) {
        func_0025E180(file);
        DEBUGERRORLINE(1841, "Cannot allocate enough space for all files in the game!\n");
        return FALSE;
    }
    func_001A3FA0(packfile->entries, packfile->directory_size, file);
    func_001A3FA0(packfile->names, packfile->names_size, file);
    func_001A41D0(packfile);
    if (packfile->compressed_size != -1) {
        func_0025E290(file, packfile->data, packfile->compressed_size);
    } else {
        func_0025E050(file, packfile->data, packfile->data_size);
    }
    func_0025E180(file);
    Cond_packfile_state[Num_cond_packfiles - 1] = COND_PACKFILE_LOADED;
    return TRUE;
}

BOOL packfile_load_cond_dvd(Packfile* packfile) {
    void* file;

    file = func_001A3460(packfile->sector + 1, -1);
    if (file == NULL) {
        return FALSE;
    }
    if (!packfile_alloc_directory(packfile)) {
        func_001A3B10(file);
        DEBUGERRORLINE(1884, "Cannot allocate enough space for all files in the game!\n");
        return FALSE;
    }
    func_001A4040(packfile->entries, packfile->directory_size, file);
    func_001A4040(packfile->names, packfile->names_size, file);
    func_001A41D0(packfile);
    if (packfile->compressed_size != -1) {
        func_001A3780(file, packfile->data, packfile->compressed_size);
    } else {
        func_001A36C0(file, packfile->data, packfile->data_size);
    }
    func_001A3B10(file);
    Cond_packfile_state[Num_cond_packfiles - 1] = COND_PACKFILE_LOADED;
    return TRUE;
}

BOOL packfile_load_cond(const char* filename) {
    Packfile* packfile;
    char name[32];
    char path[32];

    if (Cond_packfile_state[Num_cond_packfiles - 1] != COND_PACKFILE_ADDED) {
        return FALSE;
    }
    packfile = &Cond_packfiles[Num_cond_packfiles - 1];
    // The split name and path are not used.
    if (filename) {
        func_001A42B0(filename, name, path);
    }
    if (packfile->flags & PACKFILE_DVD) {
        return packfile_load_cond_dvd(packfile);
    } else if (packfile->flags & PACKFILE_HDD) {
        return packfile_load_cond_hdd(packfile);
    } else {
        return packfile_load_cond_host(packfile);
    }
}

// Adds a conditional packfile and loads it into a new buffer.
BOOL packfile_add_and_load_cond(const char* filename) {
    int data_size;

    if (packfile_add_cond(filename, &data_size)) {
        packfile_set_cond_buffer(new u8[data_size]);
        if (packfile_load_cond(filename)) {
            return TRUE;
        }
    }
    return FALSE;
}

// Forgets all conditional packfiles. Their data buffers are not freed here.
void packfile_release_cond(void) {
    int i;

    Num_cond_packfiles = 0;
    for (i = 0; i < MAX_COND_PACKFILES; i++) {
        Cond_packfile_state[i] = COND_PACKFILE_FREE;
        Cond_packfiles[i].data = NULL;
    }
    Cond_packfile_pool.ResetOffset();
}

// Finds a file in the loaded conditional packfiles; returns its data and size.
u8* packfile_find_cond(const char* filename, int* size) {
    int i;
    int j;
    u32 hash;

    if (Cond_packfile_state[0] != COND_PACKFILE_LOADED) {
        return NULL;
    }
    hash = func_001A3E40(filename);
    for (i = 0; i < Num_cond_packfiles; i++) {
        for (j = 0; j < Cond_packfiles[i].num_files; j++) {
            if (hash == Cond_packfiles[i].entries[j].hash && ngps_stricmp(filename, Cond_packfiles[i].entries[j].name) == 0) {
                *size = Cond_packfiles[i].entries[j].size;
                return Cond_packfiles[i].data + Cond_packfiles[i].entries[j].offset;
            }
        }
    }
    return NULL;
}

// Points a conditional packfile's file at data within the packfile's buffer.
void packfile_set_cond_data(const char* filename, u8* data) {
    int i;
    int j;
    u32 hash;

    hash = func_001A3E40(filename);
    for (i = 0; i < Num_cond_packfiles; i++) {
        for (j = 0; j < Cond_packfiles[i].num_files; j++) {
            if (hash == Cond_packfiles[i].entries[j].hash && ngps_stricmp(filename, Cond_packfiles[i].entries[j].name) == 0) {
                Cond_packfiles[i].entries[j].offset = data - Cond_packfiles[i].data;
                return;
            }
        }
    }
}

BOOL packfile_cond_loaded(void) { return Cond_packfile_state[0] == COND_PACKFILE_LOADED; }
