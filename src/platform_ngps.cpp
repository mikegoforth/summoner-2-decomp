// PS2 platform layer: file handles over the SDK's sifdev file I/O, plus the string helpers the
// game uses in place of the C library's. Only ngps_fopen, ngps_fseek and find_free_handle are named
// by the retail asserts; the other names are guesses after the C library functions they mirror.
#include "game/platform_ngps.h"
#include "types.h"
#include "macros.h"
#include <string.h>

extern "C" {
// Sony SDK 2.4 libkernel (sifdev.h). Identified from their sif RPC request codes
// (open 0, close 1, read 2, lseek 4) and sceOpen's variadic prologue.
int sceOpen(const char* filename, int flag, ...);
int sceClose(int fd);
int sceRead(int fd, void* buf, int nbyte);
int sceLseek(int fd, int offset, int where);
// newlib's C strlwr (0x24FE70, uses the _ctype_ table), not plainstring.cpp's C++ strlwr.
char* strlwr(char* s);
// TODO: name once their units are split. Both return a global flag; ngps_fopen opens files on the
// hard disk ("pfs0:/") when the first is clear and the second is set, otherwise on the host.
int func_001A1BD0(void);
int func_0025DD80(void);
}

#define SCE_RDONLY 0x0001
#define SCE_WRONLY 0x0002
#define SCE_APPEND 0x0100
#define SCE_CREAT  0x0200

#define SCE_SEEK_SET 0
#define SCE_SEEK_CUR 1
#define SCE_SEEK_END 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

// Explicitly initialized: the original table is in .data, not .bss.
NGPS_FILE Ngps_file_handles[NGPS_MAX_OPEN_FILES] = {0};

NGPS_FILE* find_free_handle(void) {
    int i;
    for (i = 0; i < NGPS_MAX_OPEN_FILES; i++) {
        if (Ngps_file_handles[i].fd == 0) {
            return &Ngps_file_handles[i];
        }
    }
    DEBUGERRORLINE(199, "find_free_handle in platform_ngps.cpp failed.");
    return NULL;
}

char ngps_tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        c = c + 'a' - 'A';
    }
    return c;
}

char ngps_toupper(char c) {
    if (c >= 'a' && c <= 'z') {
        c = c + 'A' - 'a';
    }
    return c;
}

char* ngps_strcat(char* dest, const char* src) {
    char* p = dest + strlen(dest);
    while (*src) {
        *p++ = *src++;
    }
    *p = '\0';
    return dest;
}

int ngps_strncmp(const char* s1, const char* s2, int n) {
    if (s1 == NULL || s2 == NULL) {
        return 1;
    }
    while (n--) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 == '\0' && c2 == '\0') {
            return 0;
        }
        if (c1 > c2) {
            return 1;
        }
        if (c1 < c2) {
            return -1;
        }
        s1++;
        s2++;
    }
    return 0;
}

// Same as ngps_strncmp, comparing ASCII-lowercased characters (written out; ngps_tolower isn't inlined).
int ngps_strnicmp(const char* s1, const char* s2, int n) {
    if (s1 == NULL || s2 == NULL) {
        return -1;
    }
    while (n--) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 = c1 + 'a' - 'A';
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 = c2 + 'a' - 'A';
        }
        if (c1 == '\0' && c2 == '\0') {
            return 0;
        }
        if (c1 > c2) {
            return 1;
        }
        if (c1 < c2) {
            return -1;
        }
        s1++;
        s2++;
    }
    return 0;
}

int ngps_strcmp(const char* s1, const char* s2) { return ngps_strncmp(s1, s2, NGPS_MAX_STRING_LENGTH); }

int ngps_stricmp(const char* s1, const char* s2) { return ngps_strnicmp(s1, s2, NGPS_MAX_STRING_LENGTH); }

char* ngps_strchr(const char* s, int c) {
    while (*s) {
        if (*s == c) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}

NGPS_FILE* ngps_fopen(const char* filename, const char* mode) {
    int permissions;
    NGPS_FILE* file;
    int flags;
    int fd;
    char path[256];

    file = find_free_handle();
    if (!func_001A1BD0() && func_0025DD80()) {
        strcpy(path, "pfs0:/");
        permissions = 0666;
    } else {
        strcpy(path, "host:");
        permissions = 0;
    }
    ngps_strcat(path, filename);
    strlwr(path);

    flags = 0;
    while (*mode) {
        switch (*mode++) {
        case 'r':
            flags |= SCE_RDONLY;
            break;
        case 'w':
            flags |= SCE_CREAT | SCE_WRONLY;
            break;
        case '+':
            flags |= SCE_APPEND;
            break;
        case 'b':
            break;
        case 't':
            break;
        default:
            DEBUGERRORLINE(393, "Unknown mode character in ngps_fopen.");
            break;
        }
    }

    fd = sceOpen(path, flags, permissions);
    if (fd < 0) {
        return NULL;
    }
    file->fd = fd;
    return file;
}

int ngps_fclose(NGPS_FILE* file) {
    if (file->fd >= 0) {
        sceClose(file->fd);
        file->fd = 0;
        return 0;
    }
    return 1;
}

int ngps_fread(void* buffer, int size, int count, NGPS_FILE* file) { return sceRead(file->fd, buffer, size * count); }

int ngps_fseek(NGPS_FILE* file, int offset, int origin) {
    int where = SCE_SEEK_SET;
    switch (origin) {
    case SEEK_SET:
        where = SCE_SEEK_SET;
        break;
    case SEEK_CUR:
        where = SCE_SEEK_CUR;
        break;
    case SEEK_END:
        where = SCE_SEEK_END;
        break;
    default:
        DEBUGERRORLINE(496, "Unknown positioning type in ngps_fseek.");
        break;
    }
    sceLseek(file->fd, offset, where);
    return 0;
}

// True for the conversion characters of a printf format.
BOOL is_printf_conversion(char c) {
    if (c == 'd' || c == 'i' || c == 'c' || c == 'u' || c == 'x' || c == 'X' || c == 'E' || c == 'e' || c == 'f' ||
        c == 'g' || c == 's' || c == 'o' || c == 'p' || c == 'n') {
        return TRUE;
    }
    return FALSE;
}
