// Meshes in the .vim format, drawn through VIF1 by the VU1 mesh microprograms: loading and
// relocating the files, dummy lookups, and microprogram helpers.
#include "game/vif_mesh.h"
#include "game/bmpman_ngps.h"
#include "game/debug.h"
#include "game/file_packfile.h"
#include "game/memory.h"
#include "game/ngps_file.h"
#include "game/platform_ngps.h"
#include "macros.h"
#include <stdio.h>

// VU1 micro memory address of the mesh microprogram, set when the microprograms are loaded.
extern u32 D_00926590[4];

#define VIF_MSCAL 0x14000000

// Converts an offset from the start of the file to a pointer, leaving 0 as NULL.
#define VIM_RELOCATE(type, field, base) ((field) = (type)((u32)(field) + ((field) ? (u32)(base) : 0)))

VifMesh* func_0019B730(void* data, int group) {
    int i;
    VifMesh* mesh;
    int handle;
    int j;
    VimObject* object;
    VimBatch* batch;

    if ((u32)data & 0x7F) {
        return NULL;
    }
    mesh = (VifMesh*)data;
    if (mesh->version < VIM_MIN_VERSION) {
        return NULL;
    }
    for (i = 0; i < mesh->num_textures; i++) {
        handle = bm_load(mesh->texture_names[i], -1, group);
        mesh->texture_handles[i] = handle;
        if (handle <= 0) {
            sprintf(string_buffer, "Can't find texture for vim:\n%s", mesh->texture_names[i]);
            DEBUGERRORLINE(95, string_buffer);
        }
    }
    if (mesh->flags & VIM_FLAG_RELOCATED) {
        return mesh;
    }
    VIM_RELOCATE(VimObject*, mesh->objects, data);
    for (i = 0; i < mesh->num_objects; i++) {
        object = &mesh->objects[i];
        VIM_RELOCATE(VimBatch*, object->batches, data);
        for (j = 0; j < object->num_batches; j++) {
            batch = &object->batches[j];
            VIM_RELOCATE(void*, batch->data[0], data);
            VIM_RELOCATE(void*, batch->data[1], data);
            VIM_RELOCATE(void*, batch->data[2], data);
            VIM_RELOCATE(void*, batch->data[3], data);
            VIM_RELOCATE(void*, batch->data[4], data);
            VIM_RELOCATE(void*, batch->data[5], data);
            VIM_RELOCATE(void*, batch->data[6], data);
            VIM_RELOCATE(void*, batch->data[7], data);
        }
        VIM_RELOCATE(VimObjectRef*, object->refs, data);
        for (j = 0; j < object->num_refs; j++) {
            VIM_RELOCATE(void*, object->refs[j].data, data);
        }
    }
    VIM_RELOCATE(u32**, mesh->mscal_codes, data);
    for (i = 0; i < mesh->num_mscal_codes; i++) {
        VIM_RELOCATE(u32*, mesh->mscal_codes[i], data);
        *mesh->mscal_codes[i] = D_00926590[0] | VIF_MSCAL;
    }
    VIM_RELOCATE(VimEntry70*, mesh->entries70, data);
    VIM_RELOCATE(VimEntry38*, mesh->entries38, data);
    VIM_RELOCATE(VimDummy*, mesh->dummies, data);
    for (i = 0; i < mesh->num_entries70; i++) {
        VIM_RELOCATE(void*, mesh->entries70[i].data, data);
    }
    for (i = 0; i < mesh->num_entries38; i++) {
        VIM_RELOCATE(void*, mesh->entries38[i].data, data);
    }
    for (i = 0; i < mesh->num_dummies; i++) {
        VIM_RELOCATE(char*, mesh->dummies[i].name, data);
    }
    mesh->flags |= VIM_FLAG_RELOCATED;
    return mesh;
}

VifMesh* func_0019BBB0(const char* name, int group) {
    char filename[48];
    SysFile file;
    int size;
    VifMesh* mesh;
    bool failed;
    u8* buffer;
    u8* packed;

    mesh = NULL;
    failed = false;
    buffer = NULL;
    change_extension(filename, name, ".vim");
    packed = packfile_find_cond(filename, &size);
    if (packed != NULL) {
        buffer = packed;
    } else {
        if (file.Open(filename, SYSFILE_MODE_READ, SYSFILE_ANY_SIZE)) {
            failed = true;
            goto done;
        }
        // Room to align the buffer to 128 bytes.
        size = file.GetSize(NULL) + 0x80;
        buffer = new u8[size];
        if (buffer == NULL) {
            failed = true;
            goto done;
        }
        buffer = (u8*)(((u32)buffer + 0x7F) & ~0x7F);
        file.Read(buffer, file.GetSize(NULL), 0, false);
    }
    mesh = func_0019B730(buffer, group);
    if (mesh == NULL) {
        failed = true;
    } else if (!is_mempool_active() && packed == NULL) {
        mesh->flags |= VIM_FLAG_ALLOCATED;
    }
done:
    if (failed) {
        if (buffer != NULL && !is_mempool_active() && packed == NULL) {
            delete[] buffer;
        }
    }
    if (file.IsOpen()) {
        file.Close();
    }
    return mesh;
}

void func_0019BD60(VifMesh* mesh) {
    if (mesh != NULL && (mesh->flags & VIM_FLAG_ALLOCATED)) {
        delete[] (u8*)mesh;
    }
}

int func_0019BD90(VifMesh* mesh, const char* name) {
    int i;

    if (mesh == NULL) {
        return -1;
    }
    for (i = 0; i < mesh->num_dummies; i++) {
        if (ngps_stricmp(name, mesh->dummies[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

void func_0019BE30(VifMesh* mesh, int index, vector3* pos, matrix3* orient) {
    if (mesh == NULL || index < 0 || index >= mesh->num_dummies) {
        return;
    }
    *pos = mesh->dummies[index].pos;
    mesh->dummies[index].orient.get_matrix(orient);
}

// VIF code command field, and the MPG command (load a microprogram at the address in its low 16
// bits).
#define VIF_CMD_MASK 0xFF000000
#define VIF_MPG      0x4A000000

void func_0019BEA0(u32* code, u32 qwc, u16 offset) {
    u32 i;
    u32 addr;

    for (i = 0; i < qwc * 4; i++) {
        if ((*code & VIF_CMD_MASK) == VIF_MPG) {
            addr = *code & 0xFFFF;
            addr += offset;
            *code &= 0xFFFF0000;
            *code |= addr;
        }
        code++;
    }
}

asm void func_0019BF10(const matrix3* matrix, void* columns) {
    // Rows 0 to 2 of the matrix, each starting at word 0: t4, t5 and t6.
    lq t4, 0x0(a0)
    lq t1, 0x10(a0)
    lq t2, 0x20(a0)
    pcpyud t5, t4, t4
    pcpyld t5, t1, t5
    mtsah zero, 2
    qfsrv t5, t5, t5
    pcpyud t6, t1, t1
    pcpyld t6, t2, t6
    // Column 0, then rotate each row's first three words and repeat for columns 1 and 2.
    pextlw t1, t5, t4
    pcpyld t1, t6, t1
    prot3w t4, t4
    prot3w t5, t5
    prot3w t6, t6
    pextlw t2, t5, t4
    pcpyld t2, t6, t2
    prot3w t4, t4
    prot3w t5, t5
    prot3w t6, t6
    pextlw t3, t5, t4
    pcpyld t3, t6, t3
    sq t1, 0x0(a1)
    sq t2, 0x10(a1)
    sq t3, 0x20(a1)
    jr ra
    nop
}
