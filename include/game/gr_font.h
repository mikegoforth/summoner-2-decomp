#ifndef GAME_GR_FONT_H
#define GAME_GR_FONT_H

#include "types.h"

// Bitmap fonts. A font is a .vf file (header, kerning pairs, character metrics and the position of
// every character in the font bitmap) and a .vbm bitmap of the same name holding the glyphs.
// Functions taking a font_num use the default font (the first one loaded) when it is negative.

#define MAX_FONTS 2

// Newest font file version this code reads.
#define FONT_VERSION 1

// Font::format. Version 0 files have no format field and are always FONT_FORMAT_MONO_4.
#define FONT_FORMAT_MONO_4 0xF

// x passed to gr_string to center each line in the clip area.
#define GR_STRING_CENTER 0x8000

// Extra spacing between two characters.
struct FontKernPair {
    /* 0x0 */ s8 c1; // character codes relative to Font::first_ascii
    /* 0x1 */ s8 c2;
    /* 0x2 */ s8 offset;
}; // size 0x3

struct FontChar {
    /* 0x0 */ int spacing;    // distance to the next character
    /* 0x4 */ int byte_width; // width of the glyph
    /* 0x8 */ int offset;
    /* 0xC */ s16 kerning_entry; // first of this character's kerning pairs, -1 if none
    /* 0xE */ s16 user_data;
}; // size 0x10

struct Font {
    /* 0x00 */ char filename[32];
    /* 0x20 */ int id;
    /* 0x24 */ int format;
    /* 0x28 */ int num_chars;
    /* 0x2C */ int first_ascii;
    /* 0x30 */ int w; // spacing of characters the font does not have
    /* 0x34 */ int h;
    /* 0x38 */ int line_height; // h unless gr_load_font sets it; not used here
    /* 0x3C */ int num_kern_pairs;
    /* 0x40 */ int pixel_data_size;
    /* 0x44 */ FontKernPair* kern_data; // sorted by c1, then c2
    /* 0x48 */ FontChar* char_data;
    /* 0x4C */ u8* pixel_data; // not used: the glyphs are in the bitmap
    /* 0x50 */ int bitmap;     // the .vbm bitmap
    /* 0x54 */ int* bm_u;      // position of each character in the bitmap
    /* 0x58 */ int* bm_v;
}; // size 0x5C

// Space, tab or line break.
int is_white_space(char c);
// Space or tab.
int is_gray_space(char c);
// Reads a .vf font into font and loads its .vbm bitmap. Returns false if the file cannot be opened
// or is of a newer version.
bool gr_read_font(const char* filename, Font* font);
// Returns the new font's number, or -1.
int gr_create_font(const char* filename);
// Selects the font and returns its height (16 if there is no font).
int gr_get_font_height(int font_num);
// Width of the widest line and total height of the first len characters of text (all of it if
// len is negative). Both are 0 if text is NULL.
void gr_get_string_size(int* w, int* h, const char* text, int len, int font_num);
// Loads a font and returns its number, or -1. The first font loaded becomes the default font.
// line_height is stored in the font if it is not negative.
int gr_load_font(const char* filename, int line_height);
// Draws text at (x, y) in the current color. Line breaks start a new line at x; x can be
// GR_STRING_CENTER. Gr_string_x and Gr_string_y are left where the text ends.
void gr_string(int x, int y, const char* text, int font_num);
// Draws one line of text at (x, y), scaled by scale.
void gr_string_scaled(float x, float y, const char* text, int font_num, float scale);
// Splits text into lines narrower than max_w, breaking at line breaks, at the first of a run of
// spaces or after a hyphen. For each line, sets the index of its first character in line_starts and
// its length in line_lengths. From ignore_char to the next white space, characters are not
// measured. Returns the number of lines, at most max_lines.
int gr_split_str(int* line_lengths, int* line_starts, const char* text, int max_w, int max_lines, char ignore_char,
                 int font_num);

extern int Num_fonts;
extern Font* Current_font;
extern Font* Default_font;
extern int Gr_string_x; // where the last gr_string ended
extern int Gr_string_y;
extern Font Fonts[MAX_FONTS];

#endif // GAME_GR_FONT_H
