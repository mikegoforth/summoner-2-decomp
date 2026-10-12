// Game file access: path helpers, FileStream (reads from a block of memory or from a file) and
// SysFile (reads files out of the packfiles through a small table of open file slots).
#include "game/ngps_file.h"
#include "game/file_packfile.h"
#include "game/matrix.h"
#include "game/platform_ngps.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

// State of an open SysFile.
struct FileSlot {
    /* 0x00 */ int mode; // 0 when the slot is free
    /* 0x04 */ int unknown_04;
    /* 0x08 */ int version;
    /* 0x0C */ int error;
    /* 0x10 */ int position;
    /* 0x14 */ int read_count;
    /* 0x18 */ int bytes_read;
    /* 0x1C */ int size;
    /* 0x20 */ int buffer_start; // file position of the buffered data, -1 if none
    /* 0x24 */ int buffer_offset;
    /* 0x28 */ int buffer_length;
    /* 0x2C */ SysFile* owner;
};

struct FileDirectory {
    /* 0x00 */ const char* name;
    /* 0x04 */ int unknown_04;
};

// DVD reader structures (ngps_dvd.cpp).
struct DVDFile {
    /* 0x0000 */ u8 buffer[0x8000];
    /* 0x8000 */ int buffered;
    /* 0x8004 */ int buffer_lsn;
    /* 0x8008 */ int buffer_size;
    /* 0x800C */ int sector;
    /* 0x8010 */ u16 sector_offset;
    /* 0x8012 */ u8 in_use;
};

struct DVDRequest {
    /* 0x00 */ int copy_size;
    /* 0x04 */ int remaining;
    /* 0x08 */ int pending;
    /* 0x0C */ u8* destination;
    /* 0x10 */ DVDRequest* next;
    /* 0x14 */ DVDRequest* prev;
};

extern "C" {
// C library.
char* func_00250230(const char* string, int c); // strrchr
// Sony SDK 2.4 libkernel.
int func_001BD3E8(const char* filename, int args, const char* argp); // sceSifLoadModule
// TODO: name once their units are split.
void func_001A2D70(void);          // DVD reader setup
void func_00195820(quaternion* q); // set to identity
}

char Base_path[256];
FileDirectory Directories[512];
FileSlot File_slots[SYSFILE_MAX_OPEN];
// Values returned by reads that are skipped.
vector3 Default_vector;
quaternion Default_quaternion;
matrix3 Default_matrix;
BOOL Use_host_files = TRUE;

char* make_path(int directory, const char* name, char* path) {
    strcpy(path, Base_path);
    if (directory) {
        ngps_strcat(path, Directories[directory].name);
        ngps_strcat(path, "/");
    }
    if (name) {
        ngps_strcat(path, name);
    }
    return path;
}

void file_system_initialize(int unused, bool load_packfiles) {
    Base_path[0] = '\0';
    func_001A2D70();
    Default_vector.x = 0.0f;
    Default_vector.y = 0.0f;
    Default_vector.z = 0.0f;
    func_00195820(&Default_quaternion);
    Default_matrix.make_identity();
    if (load_packfiles) {
        packfile_init();
    }
}

void change_extension(char* dst, const char* src, const char* extension) {
    strip_extension(dst, src);
    ngps_strcat(dst, extension);
}

void strip_extension(char* dst, const char* src) {
    char* dot = func_00250230(src, '.');
    int length = dot - src;

    if (dot == NULL) {
        strcpy(dst, src);
    } else {
        memcpy(dst, src, length);
        dst[length] = '\0';
    }
}

void strip_path(char* dst, const char* src) {
    char* slash = func_00250230(src, '/');

    if (slash == NULL) {
        strcpy(dst, src);
    } else {
        strlen(slash + 1);
        strcpy(dst, slash + 1);
    }
}

const char* get_extension(const char* filename) {
    char* dot = func_00250230(filename, '.');

    if (dot) {
        return dot;
    }
    return "";
}

FileStream::FileStream() {
    type = FILESTREAM_CLOSED;
    buffer = NULL;
}

void FileStream::OpenMemory(void* data, int data_size) {
    type = FILESTREAM_MEMORY;
    buffer = (u8*)data;
    size = data_size;
    position = 0;
    strcpy(name, "");
}

void FileStream::OpenFile(SysFile* sys_file) {
    type = FILESTREAM_FILE;
    file = sys_file;
    buffer = NULL;
    size = -1;
    position = 0;
}

int FileStream::GetVersion() {
    if (type != FILESTREAM_FILE) {
        return version;
    }
    return file->GetVersion();
}

void FileStream::SetVersion(int new_version) {
    if (type != FILESTREAM_FILE) {
        version = new_version;
    } else {
        file->SetVersion(new_version);
    }
}

bool FileStream::VersionAtLeast(int min_version) { return GetVersion() >= min_version; }

void FileStream::SetName(const char* new_name) {
    if (type != FILESTREAM_FILE) {
        strcpy(name, new_name);
    }
}

const char* FileStream::GetName() {
    if (type != FILESTREAM_FILE) {
        return name;
    }
    return file->GetName();
}

// In memory, SEEK_SET and SEEK_CUR only accept positions above 0, and SEEK_END moves back from
// the current position rather than from the end.
int FileStream::Seek(int offset, int origin) {
    int new_position;

    if (type != FILESTREAM_FILE) {
        switch (origin) {
        case SEEK_SET:
            if (offset > 0 && offset < size) {
                position = offset;
                return 0;
            }
            break;
        case SEEK_CUR:
            new_position = offset + position;
            if (new_position > 0 && new_position < size) {
                position = new_position;
                return 0;
            }
            break;
        case SEEK_END:
            if (offset > 0) {
                new_position = position - offset;
                if (new_position >= 0) {
                    position = new_position;
                    return 0;
                }
            }
            break;
        }
        return -1;
    }
    return file->Seek(offset, origin);
}

bool FileStream::EndOfFile() { return type != FILESTREAM_FILE ? position >= size : file->EndOfFile(); }

int FileStream::Close() {
    int result;

    if (type == FILESTREAM_FILE) {
        result = file->Close();
    } else {
        result = 0;
    }
    type = FILESTREAM_CLOSED;
    return result;
}

int FileStream::ReadLine(char* line, int line_size) {
    int count;
    char c;

    if (type != FILESTREAM_FILE) {
        line[0] = '\0';
        count = 0;
        c = ReadChar();
        while (c && c != '\r' && c != '\n') {
            line[count] = c;
            c = ReadChar();
            count++;
        }
        line[count] = '\0';
        while (c == '\r' || c == '\n') {
            c = ReadChar();
        }
        if (c) {
            Seek(-1, SEEK_CUR);
        }
        return count;
    }
    return file->ReadLine(line, line_size, 0);
}

int FileStream::Read(void* data, int count) {
    if (type != FILESTREAM_FILE) {
        if (position + count > size) {
            return 0;
        }
        memcpy(data, buffer + position, count);
        position += count;
        return count;
    }
    return file->Read(data, count, 0, false);
}

int FileStream::ReadInt() {
    int value;

    if (type != FILESTREAM_FILE) {
        if (position + 4 > size) {
            return 0;
        }
        memcpy(&value, buffer + position, 4);
        position += 4;
        return value;
    }
    return file->ReadInt(0, 0);
}

u32 FileStream::ReadUInt() {
    u32 value;

    if (type != FILESTREAM_FILE) {
        if (position + 4 > size) {
            return 0;
        }
        memcpy(&value, buffer + position, 4);
        position += 4;
        return value;
    }
    return file->ReadUInt(0, 0);
}

bool FileStream::ReadBool() {
    bool value;

    if (type != FILESTREAM_FILE) {
        if (position + 1 > size) {
            return false;
        }
        memcpy(&value, buffer + position, 1);
        position += 1;
        return value;
    }
    return file->ReadBool(0, true);
}

f32 FileStream::ReadFloat() {
    f32 value;

    if (type != FILESTREAM_FILE) {
        if (position + 4 > size) {
            return 0.0f;
        }
        memcpy(&value, buffer + position, 4);
        position += 4;
        return value;
    }
    return file->ReadFloat(0, 0.0f);
}

s16 FileStream::ReadShort() {
    s16 value;

    if (type != FILESTREAM_FILE) {
        if (position + 2 > size) {
            return 0;
        }
        memcpy(&value, buffer + position, 2);
        position += 2;
        return value;
    }
    return file->ReadShort(0, 0);
}

u16 FileStream::ReadUShort() {
    u16 value;

    if (type != FILESTREAM_FILE) {
        if (position + 2 > size) {
            return 0;
        }
        memcpy(&value, buffer + position, 2);
        position += 2;
        return value;
    }
    return file->ReadUShort(0, 0);
}

s8 FileStream::ReadChar() {
    s8 value;

    if (type != FILESTREAM_FILE) {
        if (position + 1 > size) {
            return 0;
        }
        memcpy(&value, buffer + position, 1);
        position += 1;
        return value;
    }
    return file->ReadChar(0, 0);
}

void FileStream::ReadString(char* string, int max_length) {
    int length;
    int i;

    length = ReadUShort();
    if (length != 0) {
        if (length < max_length) {
            Read(string, length);
            string[length] = '\0';
        } else {
            Read(string, max_length - 1);
            string[max_length - 1] = '\0';
            for (i = 0; i < length - (max_length - 1); i++) {
                ReadChar();
            }
        }
    } else {
        string[0] = '\0';
    }
}

void FileStream::ReadVector3(vector3& v) {
    if (type != FILESTREAM_FILE) {
        if (position + 12 > size) {
            return;
        }
        memcpy(&v.x, buffer + position, 4);
        position += 4;
        memcpy(&v.y, buffer + position, 4);
        position += 4;
        memcpy(&v.z, buffer + position, 4);
        position += 4;
        return;
    }
    file->ReadVector3(&v, 0, &Default_vector);
}

void FileStream::ReadQuaternion(quaternion& q) {
    if (type != FILESTREAM_FILE) {
        if (position + 16 > size) {
            return;
        }
        memcpy(&q.x, buffer + position, 4);
        position += 4;
        memcpy(&q.y, buffer + position, 4);
        position += 4;
        memcpy(&q.z, buffer + position, 4);
        position += 4;
        memcpy(&q.w, buffer + position, 4);
        position += 4;
        return;
    }
    file->ReadQuaternion(&q, 0, &Default_quaternion);
}

void load_module(const char* name) {
    char path[64];

    sprintf(path, "cdrom0:\\S2ASSETS\\%s;1", name);
    while (func_001BD3E8(path, 0, NULL) < 0) {
        sprintf(path, "host0:S2ASSETS\\%s", name);
    }
}

BOOL use_host_files(void) { return Use_host_files; }

int find_free_file_slot(void) {
    int i;

    for (i = 0; i < SYSFILE_MAX_OPEN; i++) {
        if (File_slots[i].mode == 0) {
            return i;
        }
    }
    return -1;
}

SysFile::SysFile() {
    filename[0] = '\0';
    entry = NULL;
    slot = -1;
    found = false;
}

SysFile::~SysFile() {
    if (slot >= 0) {
        Close();
    }
}

int SysFile::GetModeFlags(int mask) { return mask & File_slots[slot].mode; }

bool SysFile::VersionAtLeast(int version) { return File_slots[slot].version >= version; }

int SysFile::GetVersion() { return File_slots[slot].version; }

void SysFile::SetVersion(int version) { File_slots[slot].version = version; }

const char* SysFile::GetName() {
    char* separator = func_00250230(filename, '\\');

    if (separator == NULL) {
        separator = func_00250230(filename, '/');
        if (separator == NULL) {
            return filename;
        }
    }
    return separator + 1;
}

bool SysFile::Find(const char* name) {
    entry = packfile_find(name);
    if (entry) {
        strcpy(filename, name);
        found = true;
        return true;
    }
    found = false;
    return false;
}

int SysFile::OpenSlot(int mode) {
    int index;
    FileSlot* file_slot;

    index = find_free_file_slot();
    if (index < 0) {
        return SYSFILE_ERROR_NO_SLOT;
    }
    file_slot = &File_slots[index];
    if (!packfile_fopen(entry)) {
        return SYSFILE_ERROR_PACKFILE;
    }
    file_slot->size = entry->size;
    slot = index;
    file_slot->mode = mode;
    file_slot->unknown_04 = 0;
    file_slot->error = 0;
    file_slot->version = 0;
    file_slot->position = 0;
    file_slot->owner = this;
    file_slot->bytes_read = 0;
    file_slot->read_count = 0;
    file_slot->buffer_length = 0;
    file_slot->buffer_offset = 0;
    file_slot->buffer_start = -1;
    return 0;
}

int SysFile::Open(const char* name, int mode, int size) {
    bool exists = Find(name);

    if (exists) {
        return OpenSlot(mode);
    }
    return SYSFILE_ERROR_OPEN_NOT_FOUND;
}

int SysFile::Close() {
    FileSlot* file_slot = &File_slots[slot];

    packfile_fclose(entry);
    entry = NULL;
    file_slot->mode = 0;
    slot = -1;
    return file_slot->error;
}

int SysFile::IsReady() {
    FileSlot* file_slot;
    PackfileEntry* packfile_entry;

    if (slot < 0) {
        return 1;
    }
    file_slot = &File_slots[slot];
    packfile_entry = file_slot->owner->entry;
    if (packfile_entry && (packfile_entry->packfile->flags & PACKFILE_DVD)) {
        return packfile_fstatus(packfile_entry);
    }
    return 1;
}

int SysFile::GetSize(const char* name) {
    if (name && !Find(name)) {
        return SYSFILE_ERROR_SIZE_NOT_FOUND;
    }
    return entry->size;
}

int SysFile::Seek(int offset, int origin) {
    FileSlot* file_slot = &File_slots[slot];

    if (origin == SEEK_CUR) {
        offset += file_slot->position;
    } else if (origin == SEEK_END) {
        offset += GetSize(NULL);
    }
    if (GetModeFlags(SYSFILE_MODE_READ) && file_slot->buffer_start != -1 && offset >= file_slot->buffer_start &&
        offset < file_slot->buffer_start + file_slot->buffer_length) {
        file_slot->position = offset;
        file_slot->buffer_offset = offset - file_slot->buffer_start;
        return 0;
    }
    file_slot->position = offset;
    file_slot->buffer_offset = 0;
    file_slot->buffer_length = 0;
    file_slot->buffer_start = -1;
    return packfile_fseek(entry, offset, SEEK_SET) ? 0 : SYSFILE_ERROR_PACKFILE;
}

bool SysFile::EndOfFile() { return File_slots[slot].position >= File_slots[slot].size; }

int SysFile::GetError() {
    if (slot < 0) {
        return 0;
    }
    return File_slots[slot].error;
}

bool SysFile::IsOpen() { return slot >= 0; }

bool SysFile::ReadBool(int version, bool default_value) {
    s8 value;

    if (!VersionAtLeast(version)) {
        return default_value;
    }
    Read(&value, sizeof(value), 0, false);
    if (GetError()) {
        return default_value;
    }
    if (value) {
        return true;
    }
    return false;
}

s8 SysFile::ReadChar(int version, s8 default_value) {
    s8 value;

    if (!VersionAtLeast(version)) {
        return default_value;
    }
    Read(&value, sizeof(value), 0, false);
    if (GetError()) {
        return default_value;
    }
    return value;
}

s16 SysFile::ReadShort(int version, s16 default_value) {
    s16 value;

    if (!VersionAtLeast(version)) {
        return default_value;
    }
    Read(&value, sizeof(value), 0, false);
    if (GetError()) {
        return default_value;
    }
    return value;
}

u16 SysFile::ReadUShort(int version, u16 default_value) {
    u16 value;

    if (!VersionAtLeast(version)) {
        return default_value;
    }
    Read(&value, sizeof(value), 0, false);
    if (GetError()) {
        return default_value;
    }
    return value;
}

int SysFile::ReadInt(int version, int default_value) {
    int value;

    if (!VersionAtLeast(version)) {
        return default_value;
    }
    Read(&value, sizeof(value), 0, false);
    if (GetError()) {
        return default_value;
    }
    return value;
}

u32 SysFile::ReadUInt(int version, u32 default_value) {
    u32 value;

    if (!VersionAtLeast(version)) {
        return default_value;
    }
    Read(&value, sizeof(value), 0, false);
    if (GetError()) {
        return default_value;
    }
    return value;
}

f32 SysFile::ReadFloat(int version, f32 default_value) {
    f32 value;

    if (!VersionAtLeast(version)) {
        return default_value;
    }
    Read(&value, sizeof(value), 0, false);
    if (GetError()) {
        return default_value;
    }
    return value;
}

void SysFile::ReadVector3(vector3* v, int version, const vector3* default_value) {
    if (!VersionAtLeast(version)) {
        *v = *default_value;
    } else {
        Read(v, sizeof(vector3), version, false);
    }
}

// The components are read without a version check.
void SysFile::ReadQuaternion(quaternion* q, int version, const quaternion* default_value) {
    if (!VersionAtLeast(version)) {
        *q = *default_value;
    } else {
        q->x = ReadFloat(0, 0.0f);
        q->y = ReadFloat(0, 0.0f);
        q->z = ReadFloat(0, 0.0f);
        q->w = ReadFloat(0, 0.0f);
    }
}

int SysFile::ReadLine(char* line, int size, int version) {
    int count;
    char c;

    line[0] = '\0';
    if (!VersionAtLeast(version)) {
        return 0;
    }
    count = 0;
    c = ReadChar(0, 0);
    while (c && c != '\r' && c != '\n') {
        line[count] = c;
        c = ReadChar(0, 0);
        count++;
    }
    line[count] = '\0';
    while (c == '\r' || c == '\n') {
        c = ReadChar(0, 0);
    }
    if (c) {
        Seek(-1, SEEK_CUR);
    }
    return count;
}

// Only files in packfiles on the DVD are read; for any other file nothing is read and the slot's
// error flag is set.
int SysFile::Read(void* buffer, int size, int version, bool async) {
    FileSlot* file_slot = &File_slots[slot];
    int bytes_read;
    PackfileEntry* packfile_entry;
    BOOL on_dvd;

    file_slot->read_count++;
    if (file_slot->version < version || file_slot->error) {
        return 0;
    }
    bytes_read = 0;
    packfile_entry = file_slot->owner->entry;
    on_dvd = packfile_entry != NULL && (packfile_entry->packfile->flags & PACKFILE_DVD);
    if (on_dvd) {
        if (!packfile_fread(buffer, size, 1, packfile_entry, async)) {
            return 0;
        }
        bytes_read = size;
    }
    file_slot->position += bytes_read;
    file_slot->bytes_read += bytes_read;
    if (size != bytes_read) {
        file_slot->error = -1;
    }
    return bytes_read;
}

void SysFile::Write(const char* text, int length) {}

void dvd_reset_file(DVDFile* file) {
    file->sector = 0;
    file->sector_offset = 0;
    file->buffered = 0;
    file->buffer_lsn = -1;
    file->buffer_size = -1;
    file->in_use = 0;
}

// An unlinked request points at itself.
void dvd_reset_request(DVDRequest* request) {
    request->copy_size = -1;
    request->remaining = -1;
    request->pending = 0;
    request->destination = NULL;
    request->next = request;
    request->prev = request;
}
