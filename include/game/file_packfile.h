#ifndef GAME_FILE_PACKFILE_H
#define GAME_FILE_PACKFILE_H

#include "types.h"

// Packfiles (.vpp) bundle many small files. A packfile is a 2048-byte header sector, then the
// directory, then the names block, then the file data, each part starting on a sector boundary.
// The directory and names are kept in memory and file data is read from the packfile on demand.
// Conditional packfiles are loaded whole into a buffer instead, and are released together.

#define VPP_MAGIC 0x51890ACE

#define PACKFILE_SECTOR_SIZE 0x800

#define MAX_PACKFILES        10
#define MAX_COND_PACKFILES   3
#define MAX_PACKFILE_ENTRIES 7000

#define PACKFILE_POOL_SIZE      0x57800
#define COND_PACKFILE_POOL_SIZE 0x7800

// Packfile::flags
#define PACKFILE_DVD         0x01 // read from the disc by sector
#define PACKFILE_HOST        0x02 // read with ngps_fopen
#define PACKFILE_COMPRESSED  0x04 // entries are stored compressed
#define PACKFILE_CONDITIONAL 0x08
#define PACKFILE_HDD         0x10 // read from the hard disk

// Where a packfile was found.
#define PACKFILE_LOCATION_NONE 0
#define PACKFILE_LOCATION_HOST 1
#define PACKFILE_LOCATION_HDD  2
#define PACKFILE_LOCATION_DVD  3

// Cond_packfile_state values.
#define COND_PACKFILE_FREE   0
#define COND_PACKFILE_ADDED  1 // header read, buffer not loaded yet
#define COND_PACKFILE_LOADED 2

struct DVDFile;
struct NGPS_FILE;
struct Packfile;

// A directory entry, as stored in the packfile; name, packfile and handle are fixed up on load.
struct PackfileEntry {
    /* 0x00 */ const char* name;   // offset into the names block until fixed up
    /* 0x04 */ int offset;         // sectors from the packfile's start; bytes into a conditional packfile's buffer
    /* 0x08 */ u32 hash;
    /* 0x0C */ int size;
    /* 0x10 */ int compressed_size;
    /* 0x14 */ Packfile* packfile;
    /* 0x18 */ union {
        void* handle;      // open hard disk file
        DVDFile* dvd_file; // open disc file
        NGPS_FILE* file;   // open host file
    };
};

// The header sector holds this structure after the magic and a version (2 or more). When it is
// read in, name and path are kept, the flags are combined, the pointers are cleared and, for disc
// packfiles, sector is kept.
struct Packfile {
    /* 0x00 */ char name[13];
    /* 0x0D */ char path[31];
    /* 0x2C */ u32 flags;
    /* 0x30 */ int sector;         // first sector on the disc, -1 if not on it
    /* 0x34 */ int num_files;
    /* 0x38 */ int unk38;
    /* 0x3C */ int directory_size;
    /* 0x40 */ int names_size;
    /* 0x44 */ int compressed_size; // -1 if the data is not compressed
    /* 0x48 */ int data_size;
    /* 0x4C */ PackfileEntry* entries;
    /* 0x50 */ char* names;
    /* 0x54 */ u8* data;            // conditional packfiles only
};

// Every file of the regular packfiles, for lookup by name.
struct PackfileLookup {
    /* 0x00 */ u32 hash;
    /* 0x04 */ PackfileEntry* entry;
};

// Directory helpers, defined at the end of ngps_dvd.cpp.

// Case-insensitive name hash used by Packfile_lookup.
u32 packfile_hash(const char* name);
void packfile_fix_names(Packfile* packfile);
// Read size bytes of a packfile; the reads are made in whole sectors.
void packfile_read_host(void* destination, int size, NGPS_FILE* file);
void packfile_read_hdd(void* destination, int size, void* file);
void packfile_read_dvd(void* destination, int size, DVDFile* file);
int packfile_data_start(Packfile* packfile);
void packfile_assign_offsets(Packfile* packfile);
// Fixes up a freshly read directory and, for regular packfiles, adds its files to Packfile_lookup.
void packfile_register(Packfile* packfile);
// Splits filename at its last backslash into name and path (path keeps the backslash).
void split_path(const char* filename, char* name, char* path);
// Returns a PACKFILE_LOCATION and, for a packfile on the disc, its first sector.
int packfile_locate(const char* filename, int* sector);
// Checks a header sector and copies the description in. Returns the number of files, 0 if invalid.
int packfile_read_header(Packfile* packfile, u8* header);

// Defined in file_packfile.cpp.

void* packfile_alloc_directory(Packfile* packfile);
BOOL packfile_read_cond_header_host(Packfile* packfile, int* data_size);
BOOL packfile_read_cond_header_hdd(Packfile* packfile, int* data_size);
BOOL packfile_read_cond_header_dvd(Packfile* packfile, int* data_size);
BOOL packfile_load_dvd(Packfile* packfile);
BOOL packfile_load_hdd(Packfile* packfile);
BOOL packfile_load_host(Packfile* packfile);
void packfile_init(void);
bool packfile_summoner_vpp_loaded(void);
PackfileEntry* packfile_find(const char* filename);
BOOL packfile_add(const char* filename);
BOOL packfile_add_cond(const char* filename, int* data_size);
BOOL packfile_fopen(PackfileEntry* entry);
void packfile_fclose(PackfileEntry* entry);
int packfile_fseek(PackfileEntry* entry, int offset, int origin);
int packfile_fread(void* buffer, int size, int count, PackfileEntry* entry, bool async);
int packfile_fstatus(PackfileEntry* entry);
void packfile_set_cond_buffer(u8* buffer);
BOOL packfile_load_cond_host(Packfile* packfile);
BOOL packfile_load_cond_hdd(Packfile* packfile);
BOOL packfile_load_cond_dvd(Packfile* packfile);
BOOL packfile_load_cond(const char* filename);
BOOL packfile_add_and_load_cond(const char* filename);
void packfile_release_cond(void);
u8* packfile_find_cond(const char* filename, int* size);
void packfile_set_cond_data(const char* filename, u8* data);
BOOL packfile_cond_loaded(void);

extern PackfileLookup Packfile_lookup[MAX_PACKFILE_ENTRIES];
extern Packfile Packfiles[MAX_PACKFILES];
extern Packfile Cond_packfiles[MAX_COND_PACKFILES];
extern int Cond_packfile_state[MAX_COND_PACKFILES];
extern int Num_packfile_entries;
extern int Num_packfiles;
extern bool Summoner_vpp_loaded;
extern int Num_cond_packfiles;
extern BOOL Packfiles_locked;             // packfile_add fails while set; nothing sets it
extern bool Cond_packfiles_in_main_pool; // read by packfile_alloc_directory; nothing sets it

extern "C" {
// Files on the hard disk (pfs0:).
void* func_0025DD90(const char* filename, const char* mode);   // open
int func_0025DF40(void* file, int offset, int origin);         // seek
int func_0025E050(void* file, u8* buffer, int size);           // read
int func_0025E0F0(void* file, u8* buffer, int size);           // start an asynchronous read
void func_0025E180(void* file);                                // close
int func_0025E290(void* file, u8* buffer, int size);           // read and decompress
BOOL func_0025E550(const char* filename);                      // file exists
}

#endif // GAME_FILE_PACKFILE_H
