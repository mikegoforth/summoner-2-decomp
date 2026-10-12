// Text file parsing: reads integers, floats, vectors, matrices, quoted strings, lists and type names
// from a text file loaded whole into memory.
#include "game/parse.h"
#include "game/debug.h"
#include "game/file_packfile.h"
#include "game/matrix.h"
#include "game/memory.h"
#include "game/ngps_file.h"
#include "game/platform_ngps.h"
#include "macros.h"
#include <stdio.h>
#include <string.h>

// This file's name for error reports. It is an array rather than a string literal, which puts it at
// the start of this file's read-only data.
static const char THIS_FILE[] = __FILE__;

// newlib's character class table (ctype.h): _U | _L | _N marks letters and digits.
extern "C" const char _ctype_[];
#define isalnum(c) ((_ctype_ + 1)[(unsigned)(c)] & 7)

extern "C" {
// TODO: name these once their units are split.
// CRC-32 of a block, continuing from crc.
u32 func_00190B70(const void* data, int size, u32 crc);
// Builds the rotation matrix for three angles (radians).
void func_00193640(matrix3* m, float x, float y, float z);
}

TextFile::TextFile() {
    buffer = NULL;
    cursor = NULL;
    filename[0] = '\0';
}

TextFile::~TextFile() {
    Close();
}

void TextFile::SkipBlockComment() {
    while (*cursor && !(*cursor == '*' && cursor[1] == '/')) {
        if (*cursor == '\r') {
            line++;
        }
        cursor++;
    }
    if (*cursor) {
        cursor += 2;
    }
}

void TextFile::SkipWhitespace() {
    for (;;) {
        if (!*cursor) {
            return;
        }
        if (*cursor == ' ' || (*cursor >= '\t' && *cursor <= '\f')) {
            cursor++;
        } else if (*cursor == '\r') {
            cursor++;
            line++;
        } else if (*cursor == '/' && cursor[1] == '/') {
            while (*cursor && *cursor != '\r') {
                cursor++;
            }
        } else if (*cursor == '/' && cursor[1] == '*') {
            cursor += 2;
            SkipBlockComment();
        } else {
            return;
        }
    }
}

void TextFile::SkipToken() {
    for (;;) {
        if (!*cursor) {
            return;
        }
        if (*cursor == ' ' || (*cursor >= '\t' && *cursor <= '\f')) {
            return;
        } else if (*cursor == '\r') {
            cursor++;
            line++;
        } else if (*cursor == '/' && cursor[1] == '/') {
            while (*cursor && *cursor != '\r') {
                cursor++;
            }
        } else if (*cursor == '/' && cursor[1] == '*') {
            cursor += 2;
            SkipBlockComment();
        } else {
            cursor++;
        }
    }
}

char* TextFile::Find(char* start, const char* string) {
    TextFilePosition position;
    int length;

    GetPosition(&position);
    cursor = start;
    if (*cursor == '\0') {
        return NULL;
    }
    length = strlen(string);
    for (;;) {
        SkipWhitespace();
        if (*cursor == '\0') {
            break;
        }
        if (ngps_strnicmp(cursor, string, length) == 0) {
            char* found = cursor;
            SetPosition(&position);
            return found;
        }
        SkipToken();
    }
    SetPosition(&position);
    return NULL;
}

BOOL TextFile::SkipTo(const char* string, const char* stop) {
    char* found;

    if (cursor - buffer > size) {
        return FALSE;
    }
    found = Find(cursor, string);
    if (found == NULL) {
        return FALSE;
    }
    if (stop != NULL) {
        char* stop_found = Find(cursor, stop);
        if (stop_found != NULL && stop_found < found) {
            return FALSE;
        }
    }
    while (cursor != found) {
        if (*cursor++ == '\r') {
            line++;
        }
    }
    return TRUE;
}

BOOL TextFile::SkipPast(const char* string, const char* stop) {
    if (SkipTo(string, stop)) {
        Expect(string);
        return TRUE;
    }
    return FALSE;
}

void TextFile::GetPosition(TextFilePosition* position) {
    position->line = line;
    position->cursor = cursor;
}

void TextFile::SetPosition(const TextFilePosition* position) {
    line = position->line;
    cursor = position->cursor;
}

int TextFile::Open(const char* name, int max_size) {
    SysFile file;
    int result;

    strcpy(filename, name);
    buffer = NULL;
    allocated = true;
    if (packfile_cond_loaded()) {
        buffer = (char*)packfile_find_cond(filename, &size);
        allocated = buffer == NULL;
    }
    if (buffer == NULL) {
        result = file.Open(name, SYSFILE_MODE_READ, max_size);
        if (result < 0) {
            return result;
        }
        size = file.GetSize(NULL);
        buffer = (char*)allocate_in_commonblock(size + 1);
        buffer[size] = '\0';
        file.Read(buffer, size, 0, false);
        file.Close();
    }
    Rewind();
    return 0;
}

void TextFile::Close() {
    if (buffer != NULL && allocated) {
        deallocate_from_commonblock((u32*)buffer);
    }
    buffer = NULL;
}

void TextFile::Rewind() {
    if (buffer != NULL) {
        line = 1;
        cursor = buffer;
    }
}

void TextFile::Error(const char* expected) {
    char prior_text[41];
    char found_text[41];
    char expected_text[512];
    int i;
    char* p;

    // Up to 40 characters on each side of the cursor, with control characters shown as 0x16.
    strcpy(expected_text, expected);
    for (i = 0; i < 40; i++) {
        if (cursor[i] && cursor[i] < ' ') {
            found_text[i] = 0x16;
        } else {
            found_text[i] = cursor[i];
        }
    }
    found_text[i] = '\0';

    p = cursor - 40;
    if (p < buffer) {
        p = buffer;
    }
    for (i = 0; i < 40; i++, p++) {
        if (p == cursor) {
            break;
        }
        if (*p < ' ') {
            prior_text[i] = 0x16;
        } else {
            prior_text[i] = *p;
        }
    }
    prior_text[i] = '\0';

    sprintf(string_buffer,
            "Parsing file \"%s\", at line #%d\n"
            "Expected %s but found this text:\n\n[%s]\n\n"
            "Prior text was this:\n\n[%s]\n",
            filename, line, expected_text, found_text, prior_text);
    DebugError(THIS_FILE, 617, string_buffer);
    BREAKPOINT();
}

BOOL TextFile::ReadLine(char* dest, int dest_size) {
    int length;

    if (*cursor == '\0') {
        return FALSE;
    }
    for (length = 0; cursor[length] && cursor[length] != '\r'; length++) {
    }
    if (length >= dest_size) {
        return FALSE;
    }
    strncpy(dest, cursor, length);
    dest[length] = '\0';
    cursor += length;
    if (*cursor == '\r') {
        line++;
        cursor++;
        if (*cursor == '\n') {
            cursor++;
        }
    }
    return TRUE;
}

bool TextFile::Peek(const char* string) {
    int length = strlen(string);
    TextFilePosition position;
    bool result;

    GetPosition(&position);
    SkipWhitespace();
    if (!*cursor) {
        result = false;
    } else {
        result = ngps_strnicmp(cursor, string, length) == 0;
    }
    SetPosition(&position);
    return result;
}

BOOL TextFile::Accept(const char* string) {
    int length = strlen(string);
    BOOL result;

    SkipWhitespace();
    if (*cursor == '\0') {
        return FALSE;
    }
    result = ngps_strnicmp(cursor, string, length) == 0;
    if (result) {
        cursor += length;
    }
    return result;
}

void TextFile::Expect(const char* string) {
    if (!Accept(string)) {
        sprintf(string_buffer, "\"%s\"", string);
        Error(string_buffer);
    }
}

char TextFile::GetChar() {
    return *cursor++;
}

int TextFile::ParseInt() {
    bool negative = false;
    int value = 0;
    int digits = 0;

    SkipWhitespace();
    if (*cursor == '-') {
        negative = true;
        cursor++;
    }
    if (*cursor == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        cursor += 2;
        while (ngps_strchr("0123456789abcdefABCDEF", *cursor)) {
            char c = *cursor;
            digits++;
            if (c >= '0' && c <= '9') {
                value = value * 16 + *cursor++ - '0';
            } else if (c >= 'a' && c <= 'f') {
                value = value * 16 + *cursor++ - 'a' + 10;
            } else {
                value = value * 16 + *cursor++ - 'A' + 10;
            }
        }
    } else {
        while (*cursor >= '0' && *cursor <= '9') {
            value = value * 10 + *cursor++ - '0';
            digits++;
        }
    }
    if (negative) {
        value = -value;
    }
    if (digits == 0) {
        Error("an integer");
    }
    return value;
}

u32 TextFile::ParseUInt() {
    int digits = 0;
    u32 value = 0;

    SkipWhitespace();
    if (*cursor == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        cursor += 2;
        while (ngps_strchr("0123456789abcdefABCDEF", *cursor)) {
            char c = *cursor;
            digits++;
            if (c >= '0' && c <= '9') {
                value = value * 16 + *cursor++ - '0';
            } else if (c >= 'a' && c <= 'f') {
                value = value * 16 + *cursor++ - 'a' + 10;
            } else {
                value = value * 16 + *cursor++ - 'A' + 10;
            }
        }
    } else {
        while (*cursor >= '0' && *cursor <= '9') {
            value = value * 10 + *cursor++ - '0';
            digits++;
        }
    }
    if (digits == 0) {
        Error("a uint");
    }
    return value;
}

float TextFile::ParseFloat() {
    bool negative;
    int whole;
    float scale;
    float fraction;

    SkipWhitespace();
    negative = false;
    if (*cursor == '-') {
        negative = true;
        cursor++;
    }
    if (*cursor == '.') {
        whole = 0;
    } else {
        whole = ParseInt();
        if (*cursor != '.') {
            if (negative) {
                whole = -whole;
            }
            return (float)whole;
        }
    }
    cursor++;
    scale = 0.1f;
    fraction = 0.0f;
    while (*cursor >= '0' && *cursor <= '9') {
        fraction += scale * (*cursor - '0');
        cursor++;
        scale *= 0.1f;
    }
    if (negative) {
        whole = -whole;
        fraction = -fraction;
    }
    return fraction + (float)whole;
}

void TextFile::ParseVector(vector3& v) {
    SkipWhitespace();
    if (*cursor != '<') {
        Error("a vector");
    }
    cursor++;
    v.x = ParseFloat();
    Accept(",");
    v.y = ParseFloat();
    Accept(",");
    v.z = ParseFloat();
    SkipWhitespace();
    if (*cursor != '>') {
        Error("a vector terminator");
    }
    cursor++;
}

void TextFile::ParseMatrix(matrix3& m) {
    SkipWhitespace();
    if (*cursor != '[') {
        Error("a matrix");
    }
    cursor++;
    SkipWhitespace();
    if (Accept("I")) {
        m = matrix3(vector3(1.0f, 0.0f, 0.0f), vector3(0.0f, 1.0f, 0.0f), vector3(0.0f, 0.0f, 1.0f));
    } else if (*cursor != '<') {
        float angle = ParseFloat();
        func_00193640(&m, 0.0f, 0.0f, angle);
    } else {
        ParseVector(m.rvec);
        Accept(",");
        ParseVector(m.uvec);
        Accept(",");
        ParseVector(m.fvec);
    }
    SkipWhitespace();
    if (*cursor != ']') {
        Error("a matrix terminator");
    }
    cursor++;
}

int TextFile::ParseString(char* dest, int dest_size, char open, char close) {
    int length;

    SkipWhitespace();
    if (*cursor != open) {
        Error("a string");
    }
    cursor++;
    for (length = 0; cursor[length] && cursor[length] != '\r' && cursor[length] != close; length++) {
    }
    if (cursor[length] != close) {
        Error("a string terminator");
    }
    if (length >= dest_size) {
        sprintf(string_buffer, "Not enough storage provided (%d) for parsed C string (len=%d)", dest_size, length);
        DebugError(THIS_FILE, 1106, string_buffer);
        BREAKPOINT();
    }
    strncpy(dest, cursor, length);
    dest[length] = '\0';
    cursor += length + 1;
    return length;
}

int TextFile::ParseIdentifier(char* dest, int dest_size) {
    int length;

    if (dest_size <= 0) {
        return 0;
    }
    SkipWhitespace();
    for (length = 0; cursor[length]; length++) {
        if (!isalnum(cursor[length]) && cursor[length] != '_') {
            break;
        }
    }
    strncpy(dest, cursor, length);
    dest[length] = '\0';
    cursor += length;
    return length;
}

int TextFile::ParseIntList(int* list, int max_count, TextFileListCallback callback) {
    int count;

    SkipWhitespace();
    if (*cursor != '(') {
        Error("an integer list");
    }
    cursor++;
    SkipWhitespace();
    count = 0;
    while (*cursor && *cursor != ')') {
        if (*cursor == '"') {
            TextFilePosition position;
            char string[256];
            bool ok;

            GetPosition(&position);
            ParseString(string, sizeof(string), '"', '"');
            ok = false;
            if (callback) {
                ok = callback(&list[count], count, string);
            }
            if (!ok) {
                SetPosition(&position);
                Error("an integer list element");
            }
        } else {
            list[count] = ParseInt();
        }
        count++;
        SkipWhitespace();
    }
    if (*cursor != ')') {
        Error("an integer list terminator");
    }
    cursor++;
    return count;
}

int TextFile::ParseFloatList(float* list, int max_count) {
    int count;

    SkipWhitespace();
    if (*cursor != '(') {
        Error("a float list");
    }
    cursor++;
    SkipWhitespace();
    count = 0;
    while (*cursor && *cursor != ')') {
        list[count] = ParseFloat();
        count++;
        SkipWhitespace();
    }
    if (*cursor != ')') {
        Error("a float list terminator");
    }
    cursor++;
    return count;
}

int TextFile::ParseTypeString(const char** types, int count) {
    TextFilePosition position;
    char string[256];
    int i;

    SkipWhitespace();
    GetPosition(&position);
    ParseString(string, sizeof(string), '"', '"');
    for (i = 0; i < count && types[i] != NULL; i++) {
        if (ngps_stricmp(string, types[i]) == 0) {
            return i;
        }
    }
    SetPosition(&position);
    Error("a valid type string");
    return 0;
}

int TextFile::ParseTypeString(const TextFileType* types, int count) {
    TextFilePosition position;
    char string[256];
    int i;

    SkipWhitespace();
    GetPosition(&position);
    ParseString(string, sizeof(string), '"', '"');
    for (i = 0; i < count && types[i].name != NULL; i++) {
        if (ngps_stricmp(string, types[i].name) == 0) {
            return i;
        }
    }
    SetPosition(&position);
    Error("a valid type string");
    return 0;
}

u32 TextFile::Checksum() {
    return func_00190B70(buffer, size, 0);
}

BOOL TextFile::AtEnd() {
    BOOL end = *cursor == '\0';
    if (!end && cursor - buffer >= size - 1) {
        end = TRUE;
    }
    return end;
}
