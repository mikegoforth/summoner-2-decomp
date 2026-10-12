// PS2 memory card saves: loads the card IOP modules (MCMAN.IRX, MCSERV.IRX), keeps the save's
// directory name and file list, and wraps the libmc requests the game uses.

// The file headers declare bool parameters, so they are read before bool is disabled.
#include "game/file_packfile.h"
#include "game/ngps_file.h"

// This unit is built with bool disabled: its comparisons are int-valued, as in C, and its flags are u8.
#pragma bool off

#include "game/memcard.h"
#include "game/platform_ngps.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

// Japan Standard Time clock in BCD (libcdvd's sceCdCLOCK).
struct sceCdCLOCK {
    /* 0x0 */ u8 stat;
    /* 0x1 */ u8 second;
    /* 0x2 */ u8 minute;
    /* 0x3 */ u8 hour;
    /* 0x4 */ u8 pad;
    /* 0x5 */ u8 day;
    /* 0x6 */ u8 month;
    /* 0x7 */ u8 year;
};

extern "C" {
// Sony SDK 2.4 libmc.
int func_001BE690(void);                                                  // sceMcInit
int func_001BF1C8(int port, int slot, int* type, int* free, int* format); // sceMcGetInfo
int func_001BF090(int mode, int* cmd, int* result);                       // sceMcSync
int func_001BF680(int port, int slot);                                    // sceMcFormat
int func_001BEB28(int port, int slot, const char* name);                  // sceMcMkdir
int func_001BE9F0(int port, int slot, const char* name, int mode);        // sceMcOpen
int func_001BEB60(int fd);                                                // sceMcClose
int func_001BED80(int fd, void* buffer, int size);                        // sceMcRead
int func_001BEEA0(int fd, const void* buffer, int size);                  // sceMcWrite
int func_001BF748(int port, int slot, const char* name);                  // sceMcDelete
int func_001BF350(int port, int slot, const char* name, unsigned int mode, int maxent,
                  sceMcTblGetDir* table); // sceMcGetDir
// Sony SDK 2.4 libscf: converts a Japan Standard Time clock to the console's local time.
int func_001C9300(sceCdCLOCK* clock); // sceScfGetLocalTimefromRTC
// newlib strdup.
char* func_0024FCA8(const char* string);
// Searches the card directory `path` for the next directory (`directories` set) or file entry;
// `next` is 0 for the first search. Copies the entry's name to `name` and, if `entry` is not
// NULL, the whole entry to `entry`. Returns FALSE when there are no more entries.
BOOL func_0019F530(char* name, int port, const char* path, int next, int directories, sceMcTblGetDir* entry);
}

// libmc request codes, as returned through sceMcSync's `cmd`.
#define MC_FUNC_CARD_INFO 1
#define MC_FUNC_OPEN 2
#define MC_FUNC_CLOSE 3
#define MC_FUNC_READ 5
#define MC_FUNC_WRITE 6
#define MC_FUNC_MKDIR 11
#define MC_FUNC_GET_DIR 13
#define MC_FUNC_DELETE 15
#define MC_FUNC_FORMAT 16

// sceMcSync modes.
#define MC_WAIT 0
#define MC_NO_WAIT 1

// sceMcGetInfo results of -10 and below mean the slot is empty.
#define MC_RESULT_NO_CARD -10

// Sizes icon.sys and the icon file (named after the save directory) must have.
#define ICON_SYS_SIZE 964
#define ICON_SIZE 1024

// Passed to SysFile::Open as its size, which it ignores.
#define DISC_FILE_TIMEOUT 9999999

// Explicitly initialized: the original buffer is in .data, not .bss.
char Memcard_directory[128] = {0};
char* Memcard_files[MEMCARD_MAX_FILES];
int Memcard_file_sizes[MEMCARD_MAX_FILES];
int Memcard_save_size;

// Binary <-> BCD for the sceCdCLOCK fields.
#define TO_BCD(n) ((n) / 10 * 16 + (n) % 10)
#define FROM_BCD(n) ((n) / 16 * 10 + (n) % 16)

void memcard_initialize(const char* directory, const char** files) {
    SysFile file;
    int size;
    int i;
    int found;
    int j;
    int count;

    strcpy(Memcard_directory, directory);
    load_module("MCMAN.IRX");
    load_module("MCSERV.IRX");
    func_001BE690();

    Memcard_files[0] = NULL;
    Memcard_files[1] = NULL;
    Memcard_files[2] = NULL;
    for (i = 0; i < MEMCARD_MAX_FILES; i++) {
        found = FALSE;
        if (files[i] != NULL) {
            for (j = 0; j < MEMCARD_MAX_FILES; j++) {
                if (Memcard_files[j] != NULL && ngps_stricmp(Memcard_files[j], files[i]) == 0) {
                    found = TRUE;
                }
            }
            if (!found) {
                Memcard_files[i] = func_0024FCA8(files[i]);
            }
        }
    }

    // In kilobytes: one each for icon.sys and the icon, each data file rounded up, half a
    // kilobyte per directory entry (count is the number of files in the save directory), and
    // two more for the directory itself.
    Memcard_save_size = 0;
    count = 2;
    Memcard_save_size++;
    Memcard_save_size++;
    memset(Memcard_file_sizes, 0, sizeof(Memcard_file_sizes));
    for (i = 0; i < MEMCARD_MAX_FILES; i++) {
        if (Memcard_files[i] != NULL) {
            if (!packfile_find_cond(Memcard_files[i], &size)) {
                if (file.Open(Memcard_files[i], SYSFILE_MODE_READ, DISC_FILE_TIMEOUT) == 0) {
                    size = file.GetSize(NULL);
                } else {
                    size = 0;
                }
                file.Close();
            }
            Memcard_file_sizes[i] = size;
            Memcard_save_size += (size + 1023) / 1024;
            count++;
        }
    }
    Memcard_save_size += (count + 1) / 2;
    Memcard_save_size += 2;
}

u8 memcard_get_info(int port, int* type, int* free, int* format) {
    int result;
    int cmd = MC_FUNC_CARD_INFO;

    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    func_001BF1C8(port, 0, type, free, format);
    func_001BF090(MC_WAIT, &cmd, &result);
    *free <<= 10;
    return result > MC_RESULT_NO_CARD;
}

u8 memcard_is_ps2_card(int port) {
    int result;
    int type;
    int cmd = MC_FUNC_CARD_INFO;

    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    func_001BF1C8(port, 0, &type, NULL, NULL);
    func_001BF090(MC_WAIT, &cmd, &result);
    return result > MC_RESULT_NO_CARD && type == MEMCARD_TYPE_PS2;
}

int memcard_get_format(int port) {
    int format;
    int result;
    int cmd = MC_FUNC_CARD_INFO;

    if (port < 0 || port >= MEMCARD_PORTS) {
        return 0;
    }
    func_001BF1C8(port, 0, NULL, NULL, &format);
    func_001BF090(MC_WAIT, &cmd, &result);
    return format;
}

int memcard_get_free_space(int port) {
    int type;
    int free;
    int format;
    int result;
    int cmd = MC_FUNC_CARD_INFO;

    if (port < 0 || port >= MEMCARD_PORTS) {
        return 0;
    }
    func_001BF1C8(port, 0, &type, &free, &format);
    func_001BF090(MC_WAIT, &cmd, &result);
    return free << 10;
}

BOOL memcard_find_first_directory(char* name, int port, const char* pattern, int absolute, sceMcTblGetDir* entry) {
    char path[128];

    if (absolute) {
        strcpy(path, pattern);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, pattern);
    }
    return func_0019F530(name, port, path, 0, TRUE, entry);
}

BOOL memcard_find_next_directory(char* name, int port, const char* pattern, int absolute, sceMcTblGetDir* entry) {
    char path[128];

    if (absolute) {
        strcpy(path, pattern);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, pattern);
    }
    return func_0019F530(name, port, path, 1, TRUE, entry);
}

BOOL memcard_find_first_file(char* name, int port, const char* pattern, int absolute, sceMcTblGetDir* entry) {
    char path[128];

    if (absolute) {
        strcpy(path, pattern);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, pattern);
    }
    return func_0019F530(name, port, path, 0, FALSE, entry);
}

BOOL memcard_find_next_file(char* name, int port, const char* pattern, int absolute, sceMcTblGetDir* entry) {
    char path[128];

    if (absolute) {
        strcpy(path, pattern);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, pattern);
    }
    return func_0019F530(name, port, path, 1, FALSE, entry);
}

BOOL memcard_copy_file(int port, const char* name) {
    char buffer[256];
    SysFile file;
    int remaining;
    int fd;
    int chunk;

    fd = memcard_open(port, name, MEMCARD_CREAT, FALSE);
    memcard_close(fd);
    if (file.Open(name, SYSFILE_MODE_READ, DISC_FILE_TIMEOUT) != 0) {
        return FALSE;
    }
    remaining = file.GetSize(NULL);
    fd = memcard_open(port, name, MEMCARD_WRONLY, FALSE);
    if (fd == -1) {
        file.Close();
        return FALSE;
    }
    while (remaining != 0) {
        chunk = remaining > 256 ? 256 : remaining;
        remaining -= chunk;
        file.Read(buffer, chunk, 0, FALSE);
        memcard_write(buffer, chunk, 1, fd);
    }
    file.Close();
    memcard_close(fd);
    return TRUE;
}

BOOL memcard_create_save(int port) {
    sceMcTblGetDir entry;
    char name[128];
    u8 found;
    int fd;
    int i;

    if (!memcard_is_ps2_card(port)) {
        return FALSE;
    }

    // The save directory, in the card's root.
    found = FALSE;
    if (memcard_find_first_directory(name, port, "/*", TRUE, NULL)) {
        do {
            if (ngps_stricmp(name, Memcard_directory) == 0) {
                found = TRUE;
                break;
            }
        } while (memcard_find_next_directory(name, port, "/*", TRUE, NULL));
    }
    if (!found) {
        memcard_mkdir(port, Memcard_directory);
    }

    // icon.sys, copied from the disc.
    found = FALSE;
    if (memcard_find_first_file(name, port, "icon.sys", FALSE, &entry)) {
        do {
            if (ngps_stricmp(name, "icon.sys") == 0) {
                if (entry.FileSizeByte == ICON_SYS_SIZE) {
                    found = TRUE;
                } else {
                    memcard_delete(port, "icon.sys", FALSE);
                    found = FALSE;
                }
                break;
            }
        } while (memcard_find_next_file(name, port, "icon.sys", FALSE, &entry));
    }
    if (!found && !memcard_copy_file(port, "icon.sys")) {
        return FALSE;
    }

    // The icon file, created as 1KB of zeroes.
    found = FALSE;
    if (memcard_find_first_file(name, port, Memcard_directory, FALSE, &entry)) {
        do {
            if (ngps_stricmp(name, Memcard_directory) == 0) {
                if (entry.FileSizeByte == ICON_SIZE) {
                    found = TRUE;
                } else {
                    memcard_delete(port, Memcard_directory, FALSE);
                    found = FALSE;
                }
                break;
            }
        } while (memcard_find_next_file(name, port, Memcard_directory, FALSE, &entry));
    }
    if (!found) {
        fd = memcard_open(port, Memcard_directory, MEMCARD_CREAT, FALSE);
        memcard_close(fd);
        fd = memcard_open(port, Memcard_directory, MEMCARD_WRONLY, FALSE);
        if (fd == -1) {
            return FALSE;
        }
        memset(name, 0, sizeof(name));
        memcard_write(name, sizeof(name), ICON_SIZE / sizeof(name), fd);
        memcard_close(fd);
    }

    // The data files, copied from the disc.
    for (i = 0; i < MEMCARD_MAX_FILES; i++) {
        if (Memcard_files[i] != NULL) {
            found = FALSE;
            if (memcard_find_first_file(name, port, Memcard_files[i], FALSE, &entry)) {
                do {
                    if (ngps_stricmp(name, Memcard_files[i]) == 0) {
                        if (entry.FileSizeByte == Memcard_file_sizes[i]) {
                            found = TRUE;
                        } else {
                            memcard_delete(port, Memcard_files[i], FALSE);
                            found = FALSE;
                        }
                        break;
                    }
                } while (memcard_find_next_file(name, port, Memcard_files[i], FALSE, &entry));
            }
            if (!found && !memcard_copy_file(port, Memcard_files[i])) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

BOOL memcard_save_exists(int port) {
    int fd;
    int i;

    fd = memcard_open(port, "icon.sys", MEMCARD_RDONLY, FALSE);
    if (fd == -1) {
        return FALSE;
    }
    memcard_close(fd);
    for (i = 0; i < MEMCARD_MAX_FILES; i++) {
        if (Memcard_files[i] != NULL) {
            fd = memcard_open(port, Memcard_files[i], MEMCARD_RDONLY, FALSE);
            if (fd == -1) {
                return FALSE;
            }
            memcard_close(fd);
        }
    }
    return TRUE;
}

int memcard_get_save_size(void) { return Memcard_save_size; }

BOOL memcard_format(int port) {
    int result;

    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    if (!memcard_format_start(port)) {
        return FALSE;
    }
    while ((result = memcard_format_poll()) == 0) {
    }
    return result < 0 ? FALSE : TRUE;
}

BOOL memcard_format_start(int port) {
    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    return func_001BF680(port, 0) >= 0;
}

int memcard_format_poll(void) {
    int cmd = MC_FUNC_FORMAT;
    int result;

    if (func_001BF090(MC_NO_WAIT, &cmd, &result)) {
        return result < 0 ? -1 : 1;
    }
    return 0;
}

BOOL memcard_mkdir(int port, const char* name) {
    int result;

    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    if (!memcard_mkdir_start(port, name)) {
        return FALSE;
    }
    while ((result = memcard_mkdir_poll()) == 0) {
    }
    return result < 0 ? FALSE : TRUE;
}

BOOL memcard_mkdir_start(int port, const char* name) {
    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    return func_001BEB28(port, 0, name) >= 0;
}

int memcard_mkdir_poll(void) {
    int cmd = MC_FUNC_MKDIR;
    int result;

    if (func_001BF090(MC_NO_WAIT, &cmd, &result)) {
        return result < 0 ? -1 : 1;
    }
    return 0;
}

int memcard_open(int port, const char* name, int mode, int absolute) {
    char path[128];
    int fd;

    if (port < 0 || port >= MEMCARD_PORTS) {
        return 0;
    }
    if (absolute) {
        strcpy(path, name);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, name);
    }
    if (!memcard_open_start(port, path, mode, TRUE)) {
        return -1;
    }
    while (!memcard_open_poll(&fd)) {
    }
    return fd;
}

BOOL memcard_open_start(int port, const char* name, int mode, int absolute) {
    char path[64];

    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    if (absolute) {
        strcpy(path, name);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, name);
    }
    return func_001BE9F0(port, 0, path, mode) >= 0;
}

int memcard_open_poll(int* fd) {
    int cmd = MC_FUNC_OPEN;
    int result;

    if (func_001BF090(MC_NO_WAIT, &cmd, &result)) {
        if (result < 0) {
            *fd = -1;
            return -1;
        }
        *fd = result;
        return 1;
    }
    return 0;
}

void memcard_close(int fd) {
    if (!memcard_close_start(fd)) {
        return;
    }
    while (!memcard_close_poll()) {
    }
}

BOOL memcard_close_start(int fd) { return func_001BEB60(fd) >= 0; }

int memcard_close_poll(void) {
    int cmd = MC_FUNC_CLOSE;
    int result;

    if (func_001BF090(MC_NO_WAIT, &cmd, &result)) {
        return result < 0 ? -1 : 1;
    }
    return 0;
}

BOOL memcard_delete(int port, const char* name, int absolute) {
    char path[128];
    int result;

    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    if (absolute) {
        strcpy(path, name);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, name);
    }
    if (!memcard_delete_start(port, path, TRUE)) {
        return FALSE;
    }
    while ((result = memcard_delete_poll()) == 0) {
    }
    return result < 0 ? FALSE : TRUE;
}

BOOL memcard_delete_start(int port, const char* name, int absolute) {
    char path[128];

    if (port < 0 || port >= MEMCARD_PORTS) {
        return FALSE;
    }
    if (absolute) {
        strcpy(path, name);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, name);
    }
    return func_001BF748(port, 0, path) >= 0;
}

int memcard_delete_poll(void) {
    int cmd = MC_FUNC_DELETE;
    int result;

    if (func_001BF090(MC_NO_WAIT, &cmd, &result)) {
        return result < 0 ? -1 : 1;
    }
    return 0;
}

BOOL memcard_read(void* buffer, int size, int count, int fd) {
    int result;

    if (!memcard_read_start(buffer, size, count, fd)) {
        return FALSE;
    }
    while ((result = memcard_read_poll()) == 0) {
    }
    return result < 0 ? FALSE : TRUE;
}

BOOL memcard_read_start(void* buffer, int size, int count, int fd) { return func_001BED80(fd, buffer, size * count) >= 0; }

int memcard_read_poll(void) {
    int cmd = MC_FUNC_READ;
    int result;

    if (func_001BF090(MC_NO_WAIT, &cmd, &result)) {
        return result < 0 ? -1 : 1;
    }
    return 0;
}

BOOL memcard_write(void* buffer, int size, int count, int fd) {
    int result;

    if (!memcard_write_start(buffer, size, count, fd)) {
        return FALSE;
    }
    while ((result = memcard_write_poll()) == 0) {
    }
    return result < 0 ? FALSE : TRUE;
}

BOOL memcard_write_start(void* buffer, int size, int count, int fd) { return func_001BEEA0(fd, buffer, size * count) >= 0; }

int memcard_write_poll(void) {
    int cmd = MC_FUNC_WRITE;
    int result;

    if (func_001BF090(MC_NO_WAIT, &cmd, &result)) {
        return result < 0 ? -1 : 1;
    }
    return 0;
}

BOOL memcard_get_file_info(sceMcTblGetDir* entry, int port, const char* name, int absolute) {
    char path[128];
    sceCdCLOCK clock;
    int cmd;
    int result;

    if (absolute) {
        strcpy(path, name);
    } else {
        sprintf(path, "%s/%s", Memcard_directory, name);
    }
    memset(entry, 0, sizeof(sceMcTblGetDir));
    if (func_001BF350(port, 0, path, 0, 1, entry) < 0) {
        return FALSE;
    }
    cmd = MC_FUNC_GET_DIR;
    func_001BF090(MC_WAIT, &cmd, &result);

    // Card timestamps are Japan Standard Time.
    clock.second = TO_BCD(entry->_Modify.Sec);
    clock.minute = TO_BCD(entry->_Modify.Min);
    clock.hour = TO_BCD(entry->_Modify.Hour);
    clock.day = TO_BCD(entry->_Modify.Day);
    clock.month = TO_BCD(entry->_Modify.Month);
    clock.year = TO_BCD(entry->_Modify.Year - 2000);
    func_001C9300(&clock);
    entry->_Modify.Sec = FROM_BCD(clock.second);
    entry->_Modify.Min = FROM_BCD(clock.minute);
    entry->_Modify.Hour = FROM_BCD(clock.hour);
    entry->_Modify.Day = FROM_BCD(clock.day);
    entry->_Modify.Month = FROM_BCD(clock.month);
    entry->_Modify.Year = FROM_BCD(clock.year) + 2000;
    return TRUE;
}
