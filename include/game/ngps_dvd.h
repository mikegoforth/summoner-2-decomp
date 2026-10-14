#ifndef GAME_NGPS_DVD_H
#define GAME_NGPS_DVD_H

#include "types.h"
#include "game/platform_ngps.h"

// Streaming file access on the DVD through Sony's libcdvd. Each open file owns a 32 KB sector
// buffer. Reads go through a queue with one request slot per file: dvd_update() starts queued
// requests and copies finished sectors out of the buffer.

#define DVD_SECTOR_SIZE 2048
#define DVD_BUFFER_SIZE 0x8000
#define DVD_MAX_OPEN_FILES 4

// libcdvd's read mode (sceCdRMode).
struct sceCdRMode {
    /* 0x0 */ u8 trycount;
    /* 0x1 */ u8 spindlctrl;
    /* 0x2 */ u8 datapattern;
    /* 0x3 */ u8 pad;
};

// libcdvd's directory entry (sceCdlFILE).
struct sceCdlFILE {
    /* 0x00 */ u32 lsn;
    /* 0x04 */ u32 size;
    /* 0x08 */ char name[16];
    /* 0x18 */ u8 date[8];
    /* 0x20 */ u32 flag;
};

// The buffer is the destination of sceCdRead, which needs 64-byte alignment.
struct DVDFile {
    /* 0x0000 */ u8 buffer[DVD_BUFFER_SIZE];
    /* 0x8000 */ int buffered;      // bytes of the buffer not yet consumed
    /* 0x8004 */ int buffer_lsn;    // first sector in the buffer, -1 if none
    /* 0x8008 */ int buffer_size;   // bytes read into the buffer, -1 if none
    /* 0x800C */ u32 sector;        // read position: sector within the file...
    /* 0x8010 */ u16 sector_offset; // ...and byte within that sector
    /* 0x8012 */ u8 in_use;
    /* 0x8014 */ u32 start_lsn;     // first sector of the file on the disc
    /* 0x8018 */ int size;          // file size in bytes, 0 or less if unknown
} __attribute__((aligned(64)));

// A read in progress. Requests sit in a circular list headed by Dvd_request_list.
struct DVDRequest {
    /* 0x00 */ int copy_size;   // bytes to take from the buffer once its read completes
    /* 0x04 */ int remaining;   // bytes still to read after that
    /* 0x08 */ int pending;     // queued but not started
    /* 0x0C */ u8* destination; // NULL when the slot is free
    /* 0x10 */ DVDRequest* next;
    /* 0x14 */ DVDRequest* prev;
};

// Set once the screen can show the "DVD retrying read" message.
extern int Dvd_show_retry_message;
extern sceCdRMode Dvd_read_mode;
extern int Dvd_retry_dot_time;
extern int Dvd_retry_dots;
extern DVDFile Dvd_files[DVD_MAX_OPEN_FILES];
extern DVDRequest Dvd_requests[DVD_MAX_OPEN_FILES];
extern DVDRequest Dvd_request_list;

// Defined at the end of ngps_file.cpp. A reset file has no buffered data; a reset request is
// unlinked (it points at itself) and free.
void dvd_reset_file(DVDFile* file);
void dvd_reset_request(DVDRequest* request);

DVDRequest* dvd_add_request(int file_index);
void dvd_remove_request(DVDRequest* request);
void dvd_show_retry_message(void);
int dvd_issue_read(u32 lsn, u32 sectors, void* buffer);
void dvd_initialize(void);
bool dvd_disk_ready(bool wait);
// True when the file has no read in flight. Also advances the read queue.
BOOL dvd_file_idle(DVDFile* file);
void dvd_update(void);
// start_lsn: first sector of the file; size: its length in bytes (0 or less if unknown).
DVDFile* dvd_open(int start_lsn, int size);
int dvd_seek(DVDFile* file, int position);
// Blocking read. Returns 1 on success, 0 on failure, -1 if the file is busy.
int dvd_read(DVDFile* file, u8* destination, int size);
// Reads size bytes of zlib-compressed data and inflates them to destination.
int dvd_read_compressed(DVDFile* file, u8* destination, int size);
// Queues a read; poll dvd_file_idle() for completion. Returns 0 if the file already has one.
int dvd_read_async(DVDFile* file, u8* destination, int size);
void dvd_close(DVDFile* file);
// Returns the first sector of a file in the disc's root directory, or -1.
int dvd_find_file(const char* filename);
// The host file version of dvd_read_compressed.
int ngps_read_compressed(u8* destination, int size, NGPS_FILE* file);

// The packfile directory helpers that follow in ngps_dvd.cpp are declared in game/file_packfile.h.

#endif // GAME_NGPS_DVD_H
