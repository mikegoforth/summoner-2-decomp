#ifndef GAME_VIF_MESH_H
#define GAME_VIF_MESH_H

#include "types.h"
#include "game/matrix.h"

// Oldest .vim version the game loads.
#define VIM_MIN_VERSION 11

#define VIM_MAX_TEXTURES        6
#define VIM_TEXTURE_NAME_LENGTH 65

// VifMesh::flags
#define VIM_FLAG_ALLOCATED 0x8  // the file was read into memory from new[] (see func_0019BD60)
#define VIM_FLAG_RELOCATED 0x10 // offsets have been converted to pointers

// A named point on the mesh.
struct VimDummy {
    /* 0x00 */ char* name;
    /* 0x04 */ quaternion orient;
    /* 0x14 */ vector3 pos;
};

struct VimBatch {
    /* 0x00 */ u8 unknown_00[0xC];
    /* 0x0C */ void* data[8];
};

struct VimObjectRef {
    /* 0x00 */ void* data;
    /* 0x04 */ int unknown_04;
};

struct VimObject {
    /* 0x00 */ u16 unknown_00;
    /* 0x02 */ u16 num_batches;
    /* 0x04 */ u16 num_refs;
    /* 0x06 */ u16 unknown_06;
    /* 0x08 */ VimBatch* batches;
    /* 0x0C */ VimObjectRef* refs;
};

struct VimEntry70 {
    /* 0x00 */ void* data;
    /* 0x04 */ u8 unknown_04[0x6C];
};

struct VimEntry38 {
    /* 0x00 */ void* data;
    /* 0x04 */ u8 unknown_04[0x34];
};

// A mesh in the .vim format. The file is loaded whole, on a 128-byte boundary, and used in place:
// every pointer below is stored in the file as an offset from its start (0 for none) until
// func_0019B730 relocates it.
struct VifMesh {
    /* 0x000 */ int version;
    /* 0x004 */ u32 flags;
    /* 0x008 */ u8 unknown_008[0x14];
    /* 0x01C */ int num_textures;
    /* 0x020 */ char texture_names[VIM_MAX_TEXTURES][VIM_TEXTURE_NAME_LENGTH];
    /* 0x1A8 */ int texture_handles[VIM_MAX_TEXTURES];
    /* 0x1C0 */ int num_objects;
    /* 0x1C4 */ VimObject* objects;
    /* 0x1C8 */ int num_entries70;
    /* 0x1CC */ VimEntry70* entries70;
    /* 0x1D0 */ int unknown_1D0;
    /* 0x1D4 */ int num_entries38;
    /* 0x1D8 */ VimEntry38* entries38;
    /* 0x1DC */ int unknown_1DC;
    /* 0x1E0 */ int num_dummies;
    /* 0x1E4 */ VimDummy* dummies;
    // VIF codes to overwrite with an MSCAL of the mesh microprogram.
    /* 0x1E8 */ u32** mscal_codes;
    /* 0x1EC */ int num_mscal_codes;
    /* 0x1F0 */ int unknown_1F0;
};

extern "C" {
// Checks a .vim file loaded at `data` (it must be 128-byte aligned and at least version
// VIM_MIN_VERSION), looks up its textures in texture group `group` (-1: any) and, once, relocates
// its offsets. Returns the mesh, or NULL if the file is unusable.
VifMesh* func_0019B730(void* data, int group);
// Loads `name` with the extension ".vim": in place if a loaded conditional packfile holds it, or else
// read through SysFile into a new buffer. Returns NULL on failure.
VifMesh* func_0019BBB0(const char* name, int group);
// Frees a mesh read into its own buffer.
void func_0019BD60(VifMesh* mesh);
// Returns the index of the dummy called `name`, or -1.
int func_0019BD90(VifMesh* mesh, const char* name);
// Copies the position of dummy `index` to `pos` and its orientation, as a matrix, to `orient`.
void func_0019BE30(VifMesh* mesh, int index, vector3* pos, matrix3* orient);
// Adds `offset` to the load address of every MPG code in the VIF packet `code`, `qwc` quadwords
// long, so a microprogram can be loaded anywhere in VU1 micro memory.
void func_0019BEA0(u32* code, u32 qwc, u16 offset);
// Copies a matrix to three quadwords, transposed: each quadword holds one column (the x, y or z
// components of the rows) and a leftover matrix value in its fourth word. The matrix must be
// 16-byte aligned; it is read as three quadwords, 12 bytes past its end.
void func_0019BF10(const matrix3* matrix, void* columns);
}

#endif // GAME_VIF_MESH_H
