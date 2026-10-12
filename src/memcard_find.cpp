// Memory card directory search, used by memcard_find_first_directory, memcard_find_next_directory,
// memcard_find_first_file and memcard_find_next_file. It is the first function of the memory card
// unit (memcard.cpp).
#include "game/memcard.h"
#include "types.h"
#include <stddef.h>
#include <string.h>

extern "C" {
// Sony SDK 2.4 libmc.
int func_001BF090(int mode, int* cmd, int* result); // sceMcSync
int func_001BF350(int port, int slot, const char* name, unsigned int mode, int maxent,
                  sceMcTblGetDir* table); // sceMcGetDir
}

// libmc request code of sceMcGetDir, as returned through sceMcSync's `cmd`.
#define MC_FUNC_GET_DIR 13
// sceMcSync mode: wait for the request to finish.
#define MC_WAIT 0

// Searches the card directory `path` for the next directory (`directories` set) or file entry.
// `next` is 0 for the first search. Copies the name to `name` and the entry to `entry`.
extern "C" BOOL func_0019F530(char* name, int port, const char* path, int next, int directories,
                              sceMcTblGetDir* entry) {
    // libmc.h declares sceMcTblGetDir 64-byte aligned.
    sceMcTblGetDir table __attribute__((aligned(64)));
    int cmd;
    int result;

    do {
        memset(&table, 0, sizeof(table));
        if (func_001BF350(port, 0, path, next, 1, &table) < 0) {
            return FALSE;
        }
        cmd = MC_FUNC_GET_DIR;
        func_001BF090(MC_WAIT, &cmd, &result);
        if (result > 0) {
            if ((directories && (table.AttrFile & MEMCARD_ATTR_SUBDIR)) ||
                (!directories && !(table.AttrFile & MEMCARD_ATTR_SUBDIR))) {
                strcpy(name, (char*)table.EntryName);
                if (entry != NULL) {
                    memcpy(entry, &table, sizeof(table));
                }
                return TRUE;
            }
        }
        next = 1;
    } while (result > 0);
    return FALSE;
}
