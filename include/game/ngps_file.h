#ifndef GAME_NGPS_FILE_H
#define GAME_NGPS_FILE_H

#include "types.h"

// Number of files that can be open through SysFile at once.
#define SYSFILE_MAX_OPEN 4

// SysFile error codes.
#define SYSFILE_ERROR_OPEN_NOT_FOUND -1
#define SYSFILE_ERROR_SIZE_NOT_FOUND -4
#define SYSFILE_ERROR_NO_SLOT -6
#define SYSFILE_ERROR_PACKFILE -99999

// SysFile open mode flags.
#define SYSFILE_MODE_READ 0x1

// Size passed to SysFile::Open by callers that set no limit (Open ignores the size).
#define SYSFILE_ANY_SIZE 9999999

class vector3;
class quaternion;
struct PackfileEntry;

// A file inside the packfiles, read through one of SYSFILE_MAX_OPEN slots. Every read takes a
// version: the data is read only if the file's version is at least that version, otherwise the
// read returns its default value. This lets newer code read older files.
class SysFile {
  public:
    SysFile();
    ~SysFile();

    int GetModeFlags(int mask);
    bool VersionAtLeast(int version);
    int GetVersion();
    void SetVersion(int version);
    // The file name without its directory.
    const char* GetName();
    bool Find(const char* name);
    int OpenSlot(int mode);
    // Returns 0 or a SYSFILE_ERROR code. size is unused.
    int Open(const char* name, int mode, int size);
    // Frees the slot and returns its error flag.
    int Close();
    // 1 unless a read of the file from the disc is still in progress.
    int IsReady();
    // Size of the open file, or of the file called name if name is not NULL.
    int GetSize(const char* name);
    int Seek(int offset, int origin);
    bool EndOfFile();
    int GetError();
    bool IsOpen();
    bool ReadBool(int version, bool default_value);
    s8 ReadChar(int version, s8 default_value);
    s16 ReadShort(int version, s16 default_value);
    u16 ReadUShort(int version, u16 default_value);
    int ReadInt(int version, int default_value);
    u32 ReadUInt(int version, u32 default_value);
    f32 ReadFloat(int version, f32 default_value);
    void ReadVector3(vector3* v, int version, const vector3* default_value);
    void ReadQuaternion(quaternion* q, int version, const quaternion* default_value);
    // Reads up to the end of the line and skips the line break. size is unused.
    int ReadLine(char* line, int size, int version);
    // Returns the number of bytes read. With async the read is only started.
    int Read(void* buffer, int size, int version, bool async);
    // Writing is not supported; does nothing.
    void Write(const char* text, int length);

    /* 0x000 */ int unknown_000;
    /* 0x004 */ bool found;
    /* 0x008 */ int unknown_008;
    /* 0x00C */ char filename[256];
    /* 0x10C */ PackfileEntry* entry;
    /* 0x110 */ int unknown_110;
    /* 0x114 */ int slot; // -1 when the file is not open
}; // size 0x118

// FileStream::type
enum {
    FILESTREAM_CLOSED = -1,
    FILESTREAM_FILE = 0,
    FILESTREAM_MEMORY = 1,
};

// Reads binary or text data either from a block of memory or from an open SysFile.
class FileStream {
  public:
    FileStream();

    void OpenMemory(void* data, int data_size);
    void OpenFile(SysFile* sys_file);
    int GetVersion();
    void SetVersion(int new_version);
    bool VersionAtLeast(int min_version);
    void SetName(const char* new_name);
    const char* GetName();
    // Returns 0, or -1 if the new position is out of range.
    int Seek(int offset, int origin);
    bool EndOfFile();
    int Close();
    // Reads up to the end of the line and skips the line break. line_size is unused.
    int ReadLine(char* line, int line_size);
    int Read(void* data, int count);
    int ReadInt();
    u32 ReadUInt();
    bool ReadBool();
    f32 ReadFloat();
    s16 ReadShort();
    u16 ReadUShort();
    s8 ReadChar();
    // Reads a string stored as a 16-bit length and its characters, cut to fit max_length.
    void ReadString(char* string, int max_length);
    void ReadVector3(vector3& v);
    void ReadQuaternion(quaternion& q);

    /* 0x00 */ int type;
    /* 0x04 */ SysFile* file;
    /* 0x08 */ u8* buffer;
    /* 0x0C */ int position;
    /* 0x10 */ int size;
    /* 0x14 */ int version;
    /* 0x18 */ char name[36];
};

// Writes Base_path, then the directory's name and a slash (not for directory 0), then name.
char* make_path(int directory, const char* name, char* path);
void file_system_initialize(int unused, bool load_packfiles);
// Copies src to dst with its extension replaced by extension (which includes the dot).
void change_extension(char* dst, const char* src, const char* extension);
void strip_extension(char* dst, const char* src);
void strip_path(char* dst, const char* src);
// The extension of filename including the dot, or "" if it has none.
const char* get_extension(const char* filename);
// Loads an IOP module from the disc, or from the host if that fails, retrying until one works.
void load_module(const char* name);
BOOL use_host_files(void);
int find_free_file_slot(void);

extern char Base_path[256];
extern BOOL Use_host_files;

#endif // GAME_NGPS_FILE_H
