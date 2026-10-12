#ifndef GAME_MEMCARD_H
#define GAME_MEMCARD_H

#include "types.h"

// Memory card saves through Sony's libmc. Only slot 0 of ports 0 and 1 is used.
//
// Each card request comes in three parts: a _start function that queues the request (it returns
// FALSE if libmc refuses it), a _poll function that checks it with a non-blocking sceMcSync
// (0 while it is still running, 1 when it succeeded, -1 when it failed), and a blocking version
// that queues the request and spins on the poll.
//
// Names are relative to the save directory ("<directory>/<name>") unless `absolute` is set.

#define MEMCARD_PORTS 2
#define MEMCARD_MAX_FILES 3

// libmc card types (sceMcGetInfo).
#define MEMCARD_TYPE_PS2 2

// libmc open modes (sceMcOpen).
#define MEMCARD_RDONLY 0x0001
#define MEMCARD_WRONLY 0x0002
#define MEMCARD_CREAT 0x0200

// libmc file attribute: the entry is a directory.
#define MEMCARD_ATTR_SUBDIR 0x0020

// Date and time of a card directory entry (libmc's sceMcStDateTime), in Japan Standard Time.
struct sceMcStDateTime {
    /* 0x0 */ u8 Resv2;
    /* 0x1 */ u8 Sec;
    /* 0x2 */ u8 Min;
    /* 0x3 */ u8 Hour;
    /* 0x4 */ u8 Day;
    /* 0x5 */ u8 Month;
    /* 0x6 */ u16 Year;
};

// A card directory entry as returned by sceMcGetDir (libmc's sceMcTblGetDir, which libmc.h
// declares 64-byte aligned).
struct sceMcTblGetDir {
    /* 0x00 */ sceMcStDateTime _Create;
    /* 0x08 */ sceMcStDateTime _Modify;
    /* 0x10 */ u32 FileSizeByte;
    /* 0x14 */ u16 AttrFile;
    /* 0x16 */ u16 Reserve1;
    /* 0x18 */ u32 Reserve2;
    /* 0x1C */ u32 PdaAplNo;
    /* 0x20 */ u8 EntryName[32];
} __attribute__((aligned(64)));

// Save directory name, e.g. "BASLUS-20448vol".
extern char Memcard_directory[128];
// The save's data files (copies of the names given to memcard_initialize, duplicates dropped).
extern char* Memcard_files[MEMCARD_MAX_FILES];
// Size in bytes of each data file, taken from the default copy on the disc.
extern int Memcard_file_sizes[MEMCARD_MAX_FILES];
// Card space the whole save needs, in kilobytes.
extern int Memcard_save_size;

// Loads the card modules, initializes libmc and works out the save's file list and size.
void memcard_initialize(const char* directory, const char** files);

// sceMcGetInfo on slot 0 of `port`; `free` is converted from kilobytes to bytes. Returns TRUE if
// there is a card in the slot.
u8 memcard_get_info(int port, int* type, int* free, int* format);
// TRUE if the slot holds a PS2 memory card.
u8 memcard_is_ps2_card(int port);
// sceMcGetInfo's format flag: nonzero if the card is formatted.
int memcard_get_format(int port);
// Free space on the card, in bytes.
int memcard_get_free_space(int port);

// Search the card directory matching `pattern` for directories or for files. On success the
// entry's name goes to `name` (and the whole entry to `entry`, if not NULL) and they return TRUE.
BOOL memcard_find_first_directory(char* name, int port, const char* pattern, int absolute, sceMcTblGetDir* entry);
BOOL memcard_find_next_directory(char* name, int port, const char* pattern, int absolute, sceMcTblGetDir* entry);
BOOL memcard_find_first_file(char* name, int port, const char* pattern, int absolute, sceMcTblGetDir* entry);
BOOL memcard_find_next_file(char* name, int port, const char* pattern, int absolute, sceMcTblGetDir* entry);

// Writes the default copy of a save file, read from the disc, to the save directory.
BOOL memcard_copy_file(int port, const char* name);
// Creates whatever the save is missing on the card: the save directory, icon.sys, the icon
// (a file named after the directory) and the data files. Entries of the wrong size are replaced.
BOOL memcard_create_save(int port);
// TRUE if icon.sys and every data file can be opened.
BOOL memcard_save_exists(int port);
// Memcard_save_size.
int memcard_get_save_size(void);

BOOL memcard_format(int port);
BOOL memcard_format_start(int port);
int memcard_format_poll(void);

BOOL memcard_mkdir(int port, const char* name);
BOOL memcard_mkdir_start(int port, const char* name);
int memcard_mkdir_poll(void);

// Returns the card file descriptor, -1 if the card refused, or 0 for a bad port.
int memcard_open(int port, const char* name, int mode, int absolute);
BOOL memcard_open_start(int port, const char* name, int mode, int absolute);
// Stores the descriptor (or -1) in *fd once the request is done.
int memcard_open_poll(int* fd);

void memcard_close(int fd);
BOOL memcard_close_start(int fd);
int memcard_close_poll(void);

BOOL memcard_delete(int port, const char* name, int absolute);
BOOL memcard_delete_start(int port, const char* name, int absolute);
int memcard_delete_poll(void);

// size * count bytes.
BOOL memcard_read(void* buffer, int size, int count, int fd);
BOOL memcard_read_start(void* buffer, int size, int count, int fd);
int memcard_read_poll(void);

BOOL memcard_write(void* buffer, int size, int count, int fd);
BOOL memcard_write_start(void* buffer, int size, int count, int fd);
int memcard_write_poll(void);

// Reads the directory entry of one file. Its modification time is converted to local time.
BOOL memcard_get_file_info(sceMcTblGetDir* entry, int port, const char* name, int absolute);

#endif // GAME_MEMCARD_H
