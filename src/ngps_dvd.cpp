// Streaming file access on the DVD through Sony's libcdvd (sceCd* calls), compressed reads from the
// disc and the host, and the helpers that read and index packfile directories.
#include "game/ngps_dvd.h"
#include "game/file_packfile.h"
#include "game/platform_ngps.h"
#include "game/timer.h"
#include "types.h"
#include "macros.h"
#include <stdio.h>
#include <string.h>

extern "C" {
// Sony SDK 2.4 libcdvd.
int func_001B3B00(int mode);                                                    // sceCdDiskReady
int func_001B35A0(int mode);                                                    // sceCdSync
int func_001B3DC0(u32 lsn, u32 sectors, void* buffer, sceCdRMode* mode);       // sceCdRead
int func_001B3FA0(void);                                                        // sceCdGetError
int func_001B3098(sceCdlFILE* file, const char* name);                         // sceCdSearchFile
// C library.
char* func_00251820(char* string);                                              // strupr
char* func_00250230(const char* string, int c);                                 // strrchr
// zlib stream helpers: begin, inflate one block (reporting the bytes written and consumed), end.
void func_002838B0(void);
int func_00283940(u8* destination, int destination_size, int* written, u8* source, int source_size, int* consumed);
void func_002839D0(void);
// Screen output used by the retry message.
int func_0018B0F0(void);
int func_0018B110(void);
void func_0018AF60(int a, int b, int c, int d, int e, int f, int g);
void func_00183810(int* width, int* height, const char* text, int a, int b);
int func_001837B0(int font);
void func_001839D0(int x, int y, const char* text, int color);
void func_002002E0(int x, int y, int width, int height, int a);
void func_0017BD90(int r, int g, int b, int a);
void func_0017BC10(void);
}

#define SCECdComplete 2
#define SCECdNotReady 6

#define SCECdErNO 0

#define SCECdSpinNom 1
#define SCECdSecS2048 0

// Output limits passed to the inflate helper.
#define INFLATE_UNLIMITED  0x80000000
#define INFLATE_HOST_LIMIT 0x6400000

// Where packfile_locate found a packfile that has not been added yet.
#define FOUND_NOWHERE 0
#define FOUND_DVD     1
#define FOUND_HOST    2
#define FOUND_HDD     3

int Dvd_show_retry_message;
sceCdRMode Dvd_read_mode;
int Dvd_retry_dot_time;
int Dvd_retry_dots;

DVDFile Dvd_files[DVD_MAX_OPEN_FILES];
DVDRequest Dvd_requests[DVD_MAX_OPEN_FILES];
DVDRequest Dvd_request_list;

static int dvd_start_read(DVDFile* file, u8* destination, int size, DVDRequest* request);

// Appends the request slot of file_index to the queue. Each file has one slot.
DVDRequest* dvd_add_request(int file_index) {
    DVDRequest* request;

    if (Dvd_requests[file_index].destination) {
        DEBUGERRORLINE(260, "ARGH! DVD trying to put multiple requests to the same file!");
        return NULL;
    }
    request = &Dvd_requests[file_index];
    request->prev = Dvd_request_list.prev;
    request->next = &Dvd_request_list;
    Dvd_request_list.prev->next = request;
    Dvd_request_list.prev = request;
    return request;
}

void dvd_remove_request(DVDRequest* request) {
    request->prev->next = request->next;
    request->next->prev = request->prev;
    request->next = NULL;
    request->prev = NULL;
    dvd_reset_request(request);
}

// Shown while libcdvd retries a failed read, with a dot added every second.
void dvd_show_retry_message(void) {
    int width;
    int height;
    int dots_width;
    int dots_height;
    int i;
    int screen;
    int box_width;
    int box_height;
    int box_top;
    int box_left;

    if (!Dvd_show_retry_message) {
        return;
    }

    screen = func_0018B0F0();
    func_0018AF60(func_0018B110(), screen, 0x200, 0x200, 0x200, 0x1C0, 0);
    char message[64] = "DVD retrying read";
    char dots[32];
    func_00183810(&width, &height, message, -1, -1);
    box_width = width + 20;
    box_height = func_001837B0(-1) * 2 + 10;

    if (timer_get(1000) - Dvd_retry_dot_time > 1000) {
        Dvd_retry_dots = Dvd_retry_dots >= 9 ? 0 : Dvd_retry_dots + 1;
        Dvd_retry_dot_time = timer_get(1000);
    }
    strcpy(dots, ".");
    for (i = 0; i < Dvd_retry_dots; i++) {
        ngps_strcat(dots, ".");
    }
    func_00183810(&dots_width, &dots_height, dots, -1, -1);

    box_left = (0x200 - box_width) / 2;
    box_top = (0x1C0 - box_height) / 2;

    func_002002E0(box_left, box_top, box_width, box_height, 0);
    func_0017BD90(0xFF, 0xFF, 0x9E, 0xFF);
    func_001839D0(0x100 - width / 2, 0xE0 - func_001837B0(-1), message, -1);
    func_001839D0(0x100 - dots_width / 2, 0xE0, dots, -1);
    func_0017BC10();

    func_002002E0(box_left, box_top, box_width, box_height, 0);
    func_0017BD90(0xFF, 0xFF, 0x9E, 0xFF);
    func_001839D0(0x100 - width / 2, 0xE0 - func_001837B0(-1), message, -1);
    func_001839D0(0x100 - dots_width / 2, 0xE0, dots, -1);
    func_0017BC10();
}

// Waits for the disc to be ready, then starts reading sectors into buffer.
int dvd_issue_read(u32 lsn, u32 sectors, void* buffer) {
    while (func_001B3B00(1) == SCECdNotReady) {
        dvd_show_retry_message();
    }
    return func_001B3DC0(lsn, sectors, buffer, &Dvd_read_mode);
}

void dvd_initialize(void) {
    int i;

    for (i = 0; i < DVD_MAX_OPEN_FILES; i++) {
        dvd_reset_file(&Dvd_files[i]);
    }
    for (i = 0; i < DVD_MAX_OPEN_FILES; i++) {
        dvd_reset_request(&Dvd_requests[i]);
    }
    Dvd_request_list.next = &Dvd_request_list;
    Dvd_request_list.prev = &Dvd_request_list;
    Dvd_read_mode.trycount = 0;
    Dvd_read_mode.spindlctrl = SCECdSpinNom;
    Dvd_read_mode.datapattern = SCECdSecS2048;
    dvd_disk_ready(true);
}

bool dvd_disk_ready(bool wait) { return func_001B3B00(!wait) == SCECdComplete; }

BOOL dvd_file_idle(DVDFile* file) {
    dvd_update();
    return Dvd_requests[file - Dvd_files].destination == NULL;
}

// Serves a read from the file's buffer if it holds enough, otherwise takes what it has and starts
// reading the next stretch of the file into the buffer for dvd_update to copy out.
static int dvd_start_read(DVDFile* file, u8* destination, int size, DVDRequest* request) {
    int position;
    int remainder;

    if (file->buffered > 0 && file->buffered >= size) {
        memcpy(destination, file->buffer + (file->buffer_size - file->buffered), size);
        position = file->sector_offset + file->sector * DVD_SECTOR_SIZE;
        position += size;
        file->sector = position / DVD_SECTOR_SIZE;
        file->sector_offset = position - file->sector * DVD_SECTOR_SIZE;
        file->buffered -= size;
        dvd_remove_request(request);
        return 1;
    }

    if (file->buffered > 0) {
        memcpy(destination, file->buffer + (file->buffer_size - file->buffered), size);
        position = file->sector_offset + file->sector * DVD_SECTOR_SIZE;
        position += file->buffered;
        file->sector = position / DVD_SECTOR_SIZE;
        file->sector_offset = position - file->sector * DVD_SECTOR_SIZE;
        destination += file->buffered;
        size -= file->buffered;
        file->buffered = 0;
    }

    request->copy_size = size > DVD_BUFFER_SIZE ? DVD_BUFFER_SIZE : size;
    if (request->copy_size + file->sector_offset > DVD_BUFFER_SIZE) {
        request->copy_size -= file->sector_offset;
    }
    if (request->copy_size < 0) {
        return 0;
    }
    request->remaining = size - request->copy_size;
    request->destination = destination;

    file->buffer_lsn = file->start_lsn + file->sector;
    if (file->size > 0 && file->size < file->sector * DVD_SECTOR_SIZE + DVD_BUFFER_SIZE) {
        file->buffer_size = file->size - file->sector * DVD_SECTOR_SIZE;
        remainder = file->buffer_size % DVD_SECTOR_SIZE;
        file->buffer_size += remainder == 0 ? 0 : DVD_SECTOR_SIZE - remainder;
    } else {
        file->buffer_size = DVD_BUFFER_SIZE;
    }
    if (request->copy_size > file->buffer_size) {
        request->copy_size = file->buffer_size;
    }
    return dvd_issue_read(file->start_lsn + file->sector, file->buffer_size / DVD_SECTOR_SIZE, file->buffer);
}

// Advances the read queue: starts the request at its head, or once the drive has finished a
// read, copies the data out of the file's buffer and continues or retires the request.
void dvd_update(void) {
    DVDFile* file;
    DVDRequest* request;
    BOOL done;
    int position;

    request = Dvd_request_list.next;
    if (request == &Dvd_request_list) {
        return;
    }
    file = &Dvd_files[request - Dvd_requests];

    if (request->pending) {
        request->pending = 0;
        dvd_start_read(file, request->destination, request->remaining, request);
        return;
    }

    done = !func_001B35A0(1);
    if (done) {
        switch (func_001B3FA0()) {
        default:
            dvd_show_retry_message();
            dvd_issue_read(file->start_lsn + file->sector, file->buffer_size / DVD_SECTOR_SIZE, file->buffer);
            break;
        case SCECdErNO:
            file->buffered = file->buffer_size - file->sector_offset;
            if (request->copy_size > 0) {
                memcpy(request->destination, file->buffer + (file->buffer_size - file->buffered), request->copy_size);
                file->buffered -= request->copy_size;
                position = file->sector_offset + file->sector * DVD_SECTOR_SIZE;
                position += request->copy_size;
                file->sector = position / DVD_SECTOR_SIZE;
                file->sector_offset = position - file->sector * DVD_SECTOR_SIZE;
                if (request->remaining > 0) {
                    dvd_start_read(file, request->destination + request->copy_size, request->remaining, request);
                } else {
                    dvd_remove_request(request);
                }
            }
            break;
        }
    }
}

DVDFile* dvd_open(int start_lsn, int size) {
    DVDFile* file;
    int i;
    int index = -1;

    for (i = 0; i < DVD_MAX_OPEN_FILES; i++) {
        if (!Dvd_files[i].in_use) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        DEBUGERRORLINE(647, "ARGH! Out of DVD file handles!");
        return NULL;
    }
    file = &Dvd_files[index];
    dvd_reset_file(file);
    file->start_lsn = start_lsn;
    file->size = size;
    file->sector_offset = 0;
    file->sector = 0;
    file->in_use = 1;
    return file;
}

int dvd_seek(DVDFile* file, int position) {
    int buffer_start;

    if (!dvd_file_idle(file)) {
        DEBUGERRORLINE(681, "ARGH! Trying to seek in a file that is currently busy!");
        return -1;
    }
    if (file->buffer_lsn >= 0) {
        buffer_start = (file->buffer_lsn - file->start_lsn) * DVD_SECTOR_SIZE;
        if (position >= buffer_start && position < buffer_start + file->buffer_size) {
            file->buffered = file->buffer_size - (position - buffer_start);
            file->sector = position / DVD_SECTOR_SIZE;
            file->sector_offset = position - file->sector * DVD_SECTOR_SIZE;
            return 1;
        }
    }
    file->buffered = 0;
    file->sector = position / DVD_SECTOR_SIZE;
    file->sector_offset = position - file->sector * DVD_SECTOR_SIZE;
    return 1;
}

int dvd_read(DVDFile* file, u8* destination, int size) {
    BOOL idle;

    if (!dvd_file_idle(file)) {
        DEBUGERRORLINE(731, "ARGH! Trying to read in a file that is currently busy!");
        return -1;
    }
    if (!dvd_read_async(file, destination, size)) {
        return 0;
    }
    do {
        idle = dvd_file_idle(file);
    } while (idle == FALSE);
    return idle == TRUE ? 1 : 0;
}

// Small files are read in two halves, the second while the first inflates; larger ones stream
// through two 32 KB buffers in turn.
int dvd_read_compressed(DVDFile* file, u8* destination, int size) {
    u8 buffer[2][DVD_BUFFER_SIZE];
    int consumed;
    int written;
    int offset;
    int first_size;
    int second_size;
    int chunk;
    int previous_size;
    int current;
    int previous;

    func_002838B0();
    if (size < 0x10000) {
        if (size / 0x1000 == 0) {
            dvd_read(file, buffer[0], size);
            func_00283940(destination, INFLATE_UNLIMITED, &written, buffer[0], size, &consumed);
        } else {
            first_size = (size / 0x1000 / 2 + 1) * 0x1000;
            second_size = size - first_size;
            dvd_read(file, buffer[0], first_size);
            dvd_read_async(file, buffer[1], second_size);
            func_00283940(destination, first_size * first_size, &written, buffer[0], first_size, &consumed);
            while (!dvd_file_idle(file)) {
                continue;
            }
            func_00283940(destination + written, INFLATE_UNLIMITED, &written, buffer[1], second_size, &consumed);
        }
    } else {
        offset = 0;
        timer_get(1000000);
        dvd_read(file, buffer[0], DVD_BUFFER_SIZE);
        timer_get(1000000);
        size -= DVD_BUFFER_SIZE;
        previous_size = DVD_BUFFER_SIZE;
        current = 1;
        previous = 0;
        do {
            chunk = size > DVD_BUFFER_SIZE ? DVD_BUFFER_SIZE : size;
            dvd_read_async(file, buffer[current], chunk);
            timer_get(1000000);
            func_00283940(destination + offset, INFLATE_UNLIMITED, &written, buffer[previous], previous_size,
                          &consumed);
            timer_get(1000000);
            offset += written;
            previous = current;
            current = 1 - current;
            previous_size = chunk;
            size -= chunk;
            while (!dvd_file_idle(file)) {
                continue;
            }
        } while (size > 0);
        func_00283940(destination + offset, INFLATE_UNLIMITED, &written, buffer[previous], chunk, &consumed);
    }
    func_002839D0();
    return 1;
}

// Queues a read; it starts on the next dvd_update.
int dvd_read_async(DVDFile* file, u8* destination, int size) {
    DVDRequest* request;

    if (size <= 0) {
        return 1;
    }
    request = dvd_add_request(file - Dvd_files);
    if (!request) {
        return 0;
    }
    if (Dvd_request_list.next == &Dvd_request_list) {
        request->pending = 0;
        return dvd_start_read(file, destination, size, request);
    }
    request->destination = destination;
    request->remaining = size;
    request->copy_size = 0;
    request->pending = 1;
    return 1;
}

void dvd_close(DVDFile* file) {
    if (file) {
        // Let any read in flight finish first.
        while (!dvd_file_idle(file)) {
            continue;
        }
        dvd_reset_file(file);
    }
}

// Looks the file up in the disc's root directory (ISO 9660 names are upper case with a ";1"
// version suffix).
int dvd_find_file(const char* filename) {
    char name[64];
    char path[64];
    sceCdlFILE file;

    strcpy(name, filename);
    func_00251820(name);
    sprintf(path, "\\%s;1", name);
    if (!func_001B3098(&file, path)) {
        return -1;
    }
    return file.lsn;
}

// The host file version of dvd_read_compressed; reads do not overlap inflating.
int ngps_read_compressed(u8* destination, int size, NGPS_FILE* file) {
    u8 buffer[2][DVD_BUFFER_SIZE];
    int consumed;
    int written;
    int offset;
    int first_size;
    int second_size;
    int current;
    int previous;
    int chunk;
    int previous_size;

    func_002838B0();
    if (size < 0x10000) {
        if (size / 0x1000 == 0) {
            ngps_fread(buffer[0], size, 1, file);
            func_00283940(destination, INFLATE_HOST_LIMIT, &written, buffer[0], size, &consumed);
        } else {
            first_size = (size / 0x1000 / 2 + 1) * 0x1000;
            second_size = size - first_size;
            ngps_fread(buffer[0], first_size, 1, file);
            ngps_fread(buffer[1], second_size, 1, file);
            func_00283940(destination, first_size * first_size, &written, buffer[0], first_size, &consumed);
            func_00283940(destination + written, INFLATE_HOST_LIMIT, &written, buffer[1], second_size, &consumed);
        }
    } else {
        offset = 0;
        timer_get(1000000);
        ngps_fread(buffer[0], DVD_BUFFER_SIZE, 1, file);
        timer_get(1000000);
        current = 1;
        size -= (previous_size = DVD_BUFFER_SIZE);
        previous = 0;
        do {
            if (size > DVD_BUFFER_SIZE) {
                chunk = DVD_BUFFER_SIZE;
            } else {
                chunk = size;
            }
            ngps_fread(buffer[current], chunk, 1, file);
            timer_get(1000000);
            func_00283940(destination + offset, INFLATE_HOST_LIMIT, &written, buffer[previous], previous_size,
                          &consumed);
            timer_get(1000000);
            offset += written;
            previous = current;
            size -= chunk;
            previous_size = chunk;
            current = 1 - current;
        } while (size > 0);
        func_00283940(destination + offset, INFLATE_HOST_LIMIT, &written, buffer[previous], chunk, &consumed);
    }
    func_002839D0();
    return 1;
}

// Packfile directory helpers.

// Case-insensitive name hash used by Packfile_lookup.
u32 packfile_hash(const char* name) {
    u32 hash;
    const char* p;
    char c;

    if (!name) {
        return -1;
    }
    hash = 0;
    for (p = name; *p; p++) {
        c = ngps_tolower(*p);
        hash = (hash << 6 | hash >> 26) ^ c;
    }
    return hash;
}

// Turns the directory's name offsets into pointers into the names block.
void packfile_fix_names(Packfile* packfile) {
    int i;

    for (i = 0; i < packfile->num_files; i++) {
        packfile->entries[i].name = (const char*)((int)packfile->entries[i].name + (int)packfile->names);
    }
}

// The packfile readers read whole sectors; the last partial sector goes through a buffer.
void packfile_read_host(void* destination, int size, NGPS_FILE* file) {
    u8 buffer[0x2000];
    int sectors = size / PACKFILE_SECTOR_SIZE;
    int remainder = size % PACKFILE_SECTOR_SIZE;
    u8* dest = (u8*)destination;

    ngps_fread(dest, PACKFILE_SECTOR_SIZE, sectors, file);
    if (remainder) {
        ngps_fread(buffer, PACKFILE_SECTOR_SIZE, 1, file);
        memcpy(dest + sectors * PACKFILE_SECTOR_SIZE, buffer, remainder);
    }
}

void packfile_read_hdd(void* destination, int size, void* file) {
    u8 buffer[0x2000];
    int remainder = size % PACKFILE_SECTOR_SIZE;
    u8* dest = (u8*)destination;
    int whole = size / PACKFILE_SECTOR_SIZE * PACKFILE_SECTOR_SIZE;

    func_0025E050(file, dest, whole);
    if (remainder) {
        func_0025E050(file, buffer, PACKFILE_SECTOR_SIZE);
        memcpy(dest + whole, buffer, remainder);
    }
}

void packfile_read_dvd(void* destination, int size, DVDFile* file) {
    u8 buffer[0x2000];
    int remainder = size % PACKFILE_SECTOR_SIZE;
    u8* dest = (u8*)destination;
    int whole = size / PACKFILE_SECTOR_SIZE * PACKFILE_SECTOR_SIZE;

    dvd_read(file, dest, whole);
    if (remainder) {
        dvd_read(file, buffer, PACKFILE_SECTOR_SIZE);
        memcpy(dest + whole, buffer, remainder);
    }
}

// First sector of the file data: after the header sector, the directory and the names.
int packfile_data_start(Packfile* packfile) {
    int start = 1;

    start += (packfile->directory_size + PACKFILE_SECTOR_SIZE - 1) / PACKFILE_SECTOR_SIZE;
    start += (packfile->names_size + PACKFILE_SECTOR_SIZE - 1) / PACKFILE_SECTOR_SIZE;
    return start;
}

// Gives each entry of a regular packfile its first sector; the data is stored in directory order.
void packfile_assign_offsets(Packfile* packfile) {
    int i;
    int offset;

    if (packfile->flags & PACKFILE_CONDITIONAL) {
        return;
    }
    offset = packfile_data_start(packfile);
    for (i = 0; i < packfile->num_files; i++) {
        packfile->entries[i].offset = offset;
        offset += ((packfile->flags & PACKFILE_COMPRESSED ? packfile->entries[i].compressed_size
                                                           : packfile->entries[i].size) +
                   PACKFILE_SECTOR_SIZE - 1) /
                  PACKFILE_SECTOR_SIZE;
    }
}

// Fixes up a freshly read directory and, for regular packfiles, adds its files to Packfile_lookup.
void packfile_register(Packfile* packfile) {
    int i;
    PackfileLookup* lookup;

    packfile_fix_names(packfile);
    if (!(packfile->flags & PACKFILE_CONDITIONAL)) {
        packfile_assign_offsets(packfile);
    }
    for (i = 0; i < packfile->num_files; i++) {
        packfile->entries[i].packfile = packfile;
        packfile->entries[i].handle = NULL;
        if (!(packfile->flags & PACKFILE_CONDITIONAL)) {
            lookup = &Packfile_lookup[Num_packfile_entries++];
            lookup->hash = packfile_hash(packfile->entries[i].name);
            lookup->entry = &packfile->entries[i];
        }
    }
}

// Splits filename at its last backslash into name and path (path keeps the backslash).
void split_path(const char* filename, char* name, char* path) {
    const char* separator;
    int length;
    int name_length;

    separator = func_00250230(filename, '\\');
    if (separator) {
        strcpy(name, separator + 1);
    } else {
        strcpy(name, filename);
    }
    length = strlen(filename);
    name_length = strlen(name);
    if (length == name_length) {
        strcpy(path, "");
    } else {
        length -= name_length;
        strncpy(path, filename, length);
        path[length] = '\0';
    }
}

// Finds where a packfile lives: one already added, the disc (also returning its first sector) or
// the hard disk. Returns a PACKFILE_LOCATION.
int packfile_locate(const char* filename, int* sector) {
    char name[32];
    char path[32];
    int i;
    int found;

    found = FOUND_NOWHERE;
    if (filename) {
        split_path(filename, name, path);
    }
    if (!filename || strlen(name) > 12) {
        return PACKFILE_LOCATION_NONE;
    }
    for (i = 0; i < Num_packfiles; i++) {
        if (!ngps_stricmp(name, Packfiles[i].name)) {
            *sector = Packfiles[i].sector;
            if (Packfiles[i].flags & PACKFILE_HOST) {
                return PACKFILE_LOCATION_HOST;
            }
            return Packfiles[i].flags & PACKFILE_HDD ? PACKFILE_LOCATION_HDD : PACKFILE_LOCATION_DVD;
        }
    }
    if ((*sector = dvd_find_file(filename)) != -1) {
        found = FOUND_DVD;
    }
    if (func_0025E550(filename)) {
        found = FOUND_HDD;
    }
    switch (found) {
    case FOUND_HOST:
        return PACKFILE_LOCATION_HOST;
    case FOUND_HDD:
        return PACKFILE_LOCATION_HDD;
    case FOUND_DVD:
        return PACKFILE_LOCATION_DVD;
    }
    return PACKFILE_LOCATION_NONE;
}

// Checks a packfile's header sector and copies its description in, keeping the name, path, flags
// and (for disc packfiles) sector the packfile was added with. Returns the number of files.
int packfile_read_header(Packfile* packfile, u8* header) {
    int sector;
    u32 flags;
    char name[13];
    char path[31];

    u32 magic = ((u32*)header)[0];
    u32 version = ((u32*)header)[1];

    if (magic != VPP_MAGIC || version < 2) {
        return 0;
    }
    sector = packfile->sector;
    flags = packfile->flags;
    strncpy(name, packfile->name, sizeof(name));
    strncpy(path, packfile->path, sizeof(path));
    memcpy(packfile, header + 8, sizeof(Packfile));
    packfile->flags |= flags;
    strncpy(packfile->name, name, sizeof(name));
    strncpy(packfile->path, path, sizeof(path));
    if (flags & PACKFILE_DVD) {
        packfile->sector = sector;
    }
    packfile->entries = NULL;
    packfile->names = NULL;
    packfile->data = NULL;
    return packfile->num_files;
}
