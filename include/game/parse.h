#ifndef GAME_PARSE_H
#define GAME_PARSE_H

#include "types.h"

class vector3;
class matrix3;

// A saved read position, see TextFile::GetPosition and TextFile::SetPosition.
struct TextFilePosition {
    /* 0x00 */ int line;
    /* 0x04 */ char* cursor;
};

// An entry of the tables passed to TextFile::ParseTypeString's second form: a name and a value.
struct TextFileType {
    /* 0x00 */ const char* name;
    /* 0x04 */ int value;
};

// Called by ParseIntList for list elements written as quoted strings. Stores the element's value and
// returns true if the string was recognised.
typedef bool (*TextFileListCallback)(int* element, int index, const char* string);

// A text file held in memory as one NUL-terminated buffer, read through a cursor. Lines end in "\r\n"
// and line numbers count the '\r's. "//" and "/* */" comments count as whitespace. Matching of
// keywords is case-insensitive. The Parse functions report malformed input through Error.
class TextFile {
public:
    TextFile();
    ~TextFile();

    // Skips to the end of a "/* */" comment whose opening "/*" has been read.
    void SkipBlockComment();
    // Skips whitespace and comments.
    void SkipWhitespace();
    // Skips to the next whitespace.
    void SkipToken();
    // Returns the first token from start on that begins with string, or NULL. The position is kept.
    char* Find(char* start, const char* string);
    // Moves the cursor to the next token that begins with string, unless stop (if not NULL) comes
    // first. Returns FALSE, without moving, if it isn't found.
    BOOL SkipTo(const char* string, const char* stop);
    // SkipTo, then reads past string.
    BOOL SkipPast(const char* string, const char* stop);
    void GetPosition(TextFilePosition* position);
    void SetPosition(const TextFilePosition* position);
    // Loads the whole file, from a packfile if one holds it, otherwise from disc. Returns 0, or the
    // negative error from opening the file.
    int Open(const char* name, int max_size);
    void Close();
    // Moves back to the start of the text.
    void Rewind();
    // Reports a parse error with the text around the cursor. Does not return.
    void Error(const char* expected);
    // Copies the next line (without its "\r\n") to dest. Returns FALSE at the end of the text or if the
    // line does not fit.
    BOOL ReadLine(char* dest, int dest_size);
    // True if the next token begins with string. The position is kept.
    bool Peek(const char* string);
    // Reads past string if the next token begins with it; returns whether it did.
    BOOL Accept(const char* string);
    // Accept, reporting an error if string isn't there.
    void Expect(const char* string);
    char GetChar();
    // Decimal, or hexadecimal after "0x".
    int ParseInt();
    u32 ParseUInt();
    float ParseFloat();
    // "<x, y, z>"
    void ParseVector(vector3& v);
    // "[I]" (identity), "[<x, y, z>, <x, y, z>, <x, y, z>]" (rows), or "[angle]" (a rotation).
    void ParseMatrix(matrix3& m);
    // Reads the text between open and close (on one line) into dest. Returns its length.
    int ParseString(char* dest, int dest_size, char open, char close);
    // Reads letters, digits and underscores into dest. Returns the length.
    int ParseIdentifier(char* dest, int dest_size);
    // "(a b c ...)": reads integers into list and returns how many there were. Quoted elements are
    // passed to callback. max_count is not checked.
    int ParseIntList(int* list, int max_count, TextFileListCallback callback);
    // "(a b c ...)": reads floats into list and returns how many there were. max_count is not checked.
    int ParseFloatList(float* list, int max_count);
    // Reads a quoted string and returns its index in types, which ends at count entries or at a NULL
    // name.
    int ParseTypeString(const char** types, int count);
    int ParseTypeString(const TextFileType* types, int count);
    // CRC-32 of the whole text.
    u32 Checksum();
    // True at the end of the text.
    BOOL AtEnd();

    /* 0x00 */ char* buffer;
    /* 0x04 */ char* cursor;
    /* 0x08 */ char filename[64];
    /* 0x48 */ int line;
    /* 0x4C */ int size;
    // Set when buffer was allocated in the common block rather than pointing into a packfile.
    /* 0x50 */ bool allocated;
};

#endif // GAME_PARSE_H
