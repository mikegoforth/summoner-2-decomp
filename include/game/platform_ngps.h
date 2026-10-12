#ifndef GAME_PLATFORM_NGPS_H
#define GAME_PLATFORM_NGPS_H

#include "types.h"

#define NGPS_MAX_OPEN_FILES 4

// Length limit ngps_strcmp and ngps_stricmp pass to their n-limited versions.
#define NGPS_MAX_STRING_LENGTH 0x2000000

// An open file: the descriptor returned by sceOpen. A descriptor of 0 marks a free slot.
struct NGPS_FILE {
    /* 0x00 */ int fd;
};

extern NGPS_FILE Ngps_file_handles[NGPS_MAX_OPEN_FILES];

NGPS_FILE* find_free_handle(void);

// ASCII-only case conversion.
char ngps_tolower(char c);
char ngps_toupper(char c);

char* ngps_strcat(char* dest, const char* src);
// Return 1 if either string is NULL (ngps_strnicmp: -1), else <0, 0 or >0 like strncmp.
int ngps_strncmp(const char* s1, const char* s2, int n);
int ngps_strnicmp(const char* s1, const char* s2, int n);
int ngps_strcmp(const char* s1, const char* s2);
// Case-insensitive ngps_strcmp.
int ngps_stricmp(const char* s1, const char* s2);
char* ngps_strchr(const char* s, int c);

// mode: 'r', 'w' and '+' map to SCE_RDONLY, SCE_CREAT | SCE_WRONLY and SCE_APPEND; 'b' and 't' are
// ignored. Returns NULL if sceOpen fails.
NGPS_FILE* ngps_fopen(const char* filename, const char* mode);
// Returns 0 on success, 1 if the descriptor is negative.
int ngps_fclose(NGPS_FILE* file);
// Returns sceRead's result (bytes read), not an item count.
int ngps_fread(void* buffer, int size, int count, NGPS_FILE* file);
// origin is SEEK_SET, SEEK_CUR or SEEK_END. Always returns 0.
int ngps_fseek(NGPS_FILE* file, int offset, int origin);

BOOL is_printf_conversion(char c);

#endif // GAME_PLATFORM_NGPS_H
