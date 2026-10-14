// Bitmap fonts: loading .vf fonts, measuring and drawing strings as GS sprites, and splitting text
// into lines.
#include "game/gr_font.h"
#include "game/bmpman_ngps.h"
#include "game/debug.h"
#include "game/file_packfile.h"
#include "game/gr.h"
#include "game/gr_ngps_texture.h"
#include "game/ngps_file.h"
#include "game/unit_00187F80.h"
#include "macros.h"
#include <string.h>

// A GIF tag's first doubleword.
#define GIF_TAG(nloop, eop, pre, prim, flg, nreg)                                                          \
    ((u64)(nloop) | ((u64)(eop) << 15) | ((u64)(pre) << 46) | ((u64)(prim) << 47) | ((u64)(flg) << 58) | \
     ((u64)(nreg) << 60))
#define GIF_FLG_PACKED  0
#define GIF_FLG_REGLIST 1

// Register descriptors for a GIF tag's REGS field.
#define GIF_REG_RGBAQ 0x1
#define GIF_REG_UV    0x3
#define GIF_REG_XYZ2  0x5
#define GIF_REG_AD    0xE

// GS registers written with A+D.
#define GS_PRIM    0x00
#define GS_PRMODE  0x1B
#define GS_ALPHA_1 0x42

// PRIM and PRMODE fields.
#define GS_PRIM_SPRITE 0x6
#define GS_PRIM_TME    0x10  // textured
#define GS_PRIM_ABE    0x40  // alpha blended
#define GS_PRIM_FST    0x100 // texture coordinates given as UV

// ALPHA: (A - B) * C + D, each picked from the source (0) or the frame buffer (1).
#define GS_ALPHA_SET(a, b, c, d) ((a) | ((b) << 2) | ((c) << 4) | ((d) << 6))

// Depth of the text sprites: the largest 24-bit value.
#define FONT_Z 0xFFFFFF

// A register written with the A+D format.
struct GsAD {
    /* 0x0 */ u64 data;
    /* 0x8 */ u32 addr;
    /* 0xC */ u32 pad;
};

// The settings sent before a string's sprites.
struct FontPacketHeader {
    /* 0x00 */ u64 tag[2];
    /* 0x10 */ GsAD prim;
    /* 0x20 */ GsAD prmode;
    /* 0x30 */ GsAD alpha;
    /* 0x40 */ u32 r, g, b, a; // RGBAQ
};

// One character: a sprite given by two corners, each a UV and an XYZ2 register (REGLIST format).
// Coordinates are 12.4 fixed point.
struct FontSprite {
    /* 0x00 */ u16 u0, v0;
    /* 0x04 */ u32 pad04;
    /* 0x08 */ u16 x0, y0;
    /* 0x0C */ u32 z0;
    /* 0x10 */ u16 u1, v1;
    /* 0x14 */ u32 pad14;
    /* 0x18 */ u16 x1, y1;
    /* 0x1C */ u32 z1;
};

int Num_fonts;
Font* Current_font;
Font* Default_font;
int Gr_string_x;
int Gr_string_y;
Font Fonts[MAX_FONTS];

static inline void set_font(int font_num) {
    if (font_num < 0) {
        Current_font = Default_font;
    } else {
        Current_font = &Fonts[font_num];
    }
}

// Sets *width and *spacing for character c1 followed by c2 (spacing includes the kerning between
// them) and returns c1's index in the font, or -1 if the font does not have it.
static int get_char_width(u8 c1, u8 c2, int* width, int* spacing) {
    int i;
    int letter;
    FontChar* ch;

    letter = c1 - Current_font->first_ascii;
    if (letter < 0 || letter >= Current_font->num_chars) {
        *width = 0;
        *spacing = Current_font->w;
        return -1;
    }

    ch = &Current_font->char_data[letter];
    *width = ch->byte_width;
    *spacing = ch->spacing;

    i = ch->kerning_entry;
    if (i > -1) {
        if (!(c2 == 0 || c2 == '\n')) {
            int letter2;

            letter2 = c2 - Current_font->first_ascii;
            if (letter2 >= 0 && letter2 < Current_font->num_chars) {
                FontKernPair* k = &Current_font->kern_data[i];
                int num_kern_pairs = Current_font->num_kern_pairs;
                while (k->c1 == letter && k->c2 < letter2 && i < num_kern_pairs) {
                    i++;
                    k++;
                }
                if (k->c2 == letter2) {
                    *spacing += k->offset;
                }
            }
        }
    }
    return letter;
}

// Left edge that centers the line starting at s.
static int get_centered_x(const char* s) {
    int w;
    int w2;
    int s2;

    for (w = 0; *s != 0 && *s != '\n'; s++) {
        get_char_width(s[0], s[1], &w2, &s2);
        w += s2;
    }
    return (gr_screen.clip_width - w) / 2;
}

int is_white_space(char c) { return c == ' ' || c == '\t' || c == '\n'; }

int is_gray_space(char c) { return c == ' ' || c == '\t'; }

bool gr_read_font(const char* filename, Font* font) {
    FileStream stream;
    SysFile file;
    char bitmap_name[64];
    int size;
    u8* data;
    int version;
    int i;
    int j;
    int num_chars;

    data = packfile_find_cond(filename, &size);
    if (data) {
        stream.OpenMemory(data, size);
    } else {
        if (file.Open(filename, SYSFILE_MODE_READ, 9999999) < 0) {
            return false;
        }
        stream.OpenFile(&file);
    }

    strcpy(font->filename, filename);
    font->id = stream.ReadInt();
    version = stream.ReadInt();
    if (version > FONT_VERSION) {
        DEBUGERRORLINE(396, "Trying to read a newer font version than supported!!\n");
        return false;
    }

    if (version <= 0) {
        font->format = FONT_FORMAT_MONO_4;
    } else {
        font->format = stream.ReadUInt();
    }
    font->num_chars = stream.ReadInt();
    font->first_ascii = stream.ReadInt();
    font->w = stream.ReadInt();
    font->h = stream.ReadInt();
    font->line_height = font->h;
    font->num_kern_pairs = stream.ReadInt();
    if (version <= 0) {
        // kerning and character data sizes
        stream.ReadInt();
        stream.ReadInt();
    }
    font->pixel_data_size = stream.ReadInt();

    if (font->num_kern_pairs) {
        font->kern_data = new FontKernPair[font->num_kern_pairs];
        for (i = 0; i < font->num_kern_pairs; i++) {
            font->kern_data[i].c1 = stream.ReadChar();
            font->kern_data[i].c2 = stream.ReadChar();
            stream.Read(&font->kern_data[i].offset, 1);
        }
    } else {
        font->kern_data = NULL;
    }

    font->char_data = new FontChar[font->num_chars];
    for (j = 0; j < font->num_chars; j++) {
        font->char_data[j].spacing = stream.ReadInt();
        font->char_data[j].byte_width = stream.ReadInt();
        font->char_data[j].offset = stream.ReadInt();
        font->char_data[j].kerning_entry = stream.ReadShort();
        font->char_data[j].user_data = stream.ReadShort();
    }

    num_chars = font->num_chars;
    font->bm_u = new int[num_chars];
    font->bm_v = new int[num_chars];
    stream.Read(font->bm_u, num_chars * sizeof(int));
    stream.Read(font->bm_v, num_chars * sizeof(int));
    stream.Close();

    change_extension(bitmap_name, filename, ".vbm");
    font->bitmap = bm_load(bitmap_name, -1, -1);
    return true;
}

int gr_create_font(const char* filename) {
    int n = Num_fonts++;

    if (!gr_read_font(filename, &Fonts[n])) {
        Num_fonts--;
        return -1;
    }
    return n;
}

int gr_get_font_height(int font_num) {
    set_font(font_num);
    return Current_font ? Current_font->h : 16;
}

void gr_get_string_size(int* w, int* h, const char* text, int len, int font_num) {
    int width;
    int spacing;
    int x;
    int height;
    int longest_width;
    char c;

    if (text == NULL) {
        *w = 0;
        *h = 0;
        return;
    }

    set_font(font_num);
    if (len < 0) {
        len = strlen(text);
    }

    x = 0;
    longest_width = 0;
    height = 0;
    height += Current_font->h;
    while ((c = *text) != 0 && len > 0) {
        text++;
        len--;
        if (c == '\n') {
            x = 0;
            if (*text) {
                height += Current_font->h;
            }
            continue;
        }
        get_char_width(c, *text, &width, &spacing);
        x += spacing;
        if (x > longest_width) {
            longest_width = x;
        }
    }
    *h = height;
    *w = longest_width;
}

int gr_load_font(const char* filename, int line_height) {
    int n = gr_create_font(filename);

    if (n < 0) {
        return -1;
    }
    if (Default_font == NULL) {
        Default_font = &Fonts[n];
    }
    if (line_height > -1) {
        Fonts[n].line_height = line_height;
    }
    return n;
}

void gr_string(int x, int y, const char* text, int font_num) {
    int width;
    int spacing;
    int sx;
    int h;
    int base_x;
    int sy;
    int count;
    int letter;
    int r, g, b, a;
    FontPacketHeader* hdr;
    FontSprite* sprite;
    FontChar* ch;
    int u;
    int v;
    int px;
    int w;
    int th;

    set_font(font_num);
    if (text == NULL || *text == 0) {
        Gr_string_x = x;
        Gr_string_y = y;
        return;
    }

    gr_ngps_tcache_set(Current_font->bitmap, 0);
    Gs_packet.flush();

    // The header is followed by the sprites' GIF tag, which is filled in once they are counted.
    sprite = (FontSprite*)Gs_packet.cur;
    hdr = (FontPacketHeader*)sprite;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 0, 0, GIF_FLG_PACKED, 4);
    Gs_packet.cur[1] = GIF_REG_AD | (GIF_REG_AD << 4) | (GIF_REG_AD << 8) | (GIF_REG_RGBAQ << 12);
    Gs_packet.cur += sizeof(FontPacketHeader) / sizeof(u64);
    hdr->prim.addr = GS_PRIM;
    hdr->prim.data = GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST;
    hdr->prmode.addr = GS_PRMODE;
    hdr->prmode.data = GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST;
    hdr->alpha.addr = GS_ALPHA_1;
    hdr->alpha.data = GS_ALPHA_SET(0, 1, 0, 1);
    // The GS takes 0x80 as full intensity.
    r = (gr_screen.current_color.r + 1) >> 1;
    g = (gr_screen.current_color.g + 1) >> 1;
    b = (gr_screen.current_color.b + 1) >> 1;
    a = (gr_screen.current_color.a + 1) >> 1;
    hdr->r = r;
    hdr->g = g;
    hdr->b = b;
    hdr->a = a;

    h = Current_font->h;
    sx = x;
    if (x == GR_STRING_CENTER) {
        sx = get_centered_x(text);
    }
    sprite += 3;
    count = 0;
    base_x = (gr_screen.offset_x << 4) + Gs_state.offset_x;
    sy = (y << 4) + ((gr_screen.offset_y << 4) + Gs_state.offset_y);
    while (*text) {
        u8 c = *text;
        text++;
        if (c < ' ') {
            if (c == '\n') {
                y += h;
                if (x == GR_STRING_CENTER) {
                    sx = get_centered_x(text);
                } else {
                    sx = x;
                }
                sy += h << 4;
            }
            continue;
        }

        letter = get_char_width(c, *text, &width, &spacing);
        ch = &Current_font->char_data[letter];
        u = Current_font->bm_u[letter] << 4;
        v = Current_font->bm_v[letter] << 4;
        px = (sx << 4) + base_x;
        th = h << 4;
        count++;
        // Texture coordinates start just under half a texel in, at the texel centers.
        sprite->u0 = u + 7;
        sprite->v0 = v + 7;
        sprite->x0 = px;
        sprite->y0 = sy;
        sprite->z0 = FONT_Z;
        w = ch->byte_width << 4;
        sprite->u1 = u + w;
        sprite->v1 = v + th;
        sprite->x1 = px + w;
        sprite->y1 = sy + th;
        sprite->z1 = FONT_Z;
        sprite++;
        sx += spacing;
    }

    Gs_packet.cur[0] = GIF_TAG(count, 0, 0, 0, GIF_FLG_REGLIST, 4);
    Gs_packet.cur[1] = GIF_REG_UV | (GIF_REG_XYZ2 << 4) | (GIF_REG_UV << 8) | (GIF_REG_XYZ2 << 12);
    Gs_packet.cur += count * 4 + 2;
    Gs_packet.flush();
    Gr_string_x = sx;
    Gr_string_y = y;
}

void gr_string_scaled(float x, float y, const char* text, int font_num, float scale) {
    int width;
    int spacing;
    int h;
    int count;
    int letter;
    int r, g, b, a;
    int sy0;
    int sy1;
    float base_x;
    float sy;
    float sx;
    FontPacketHeader* hdr;
    FontSprite* sprite;
    FontChar* ch;
    int u;
    int v;

    set_font(font_num);
    if (text == NULL || *text == 0) {
        return;
    }

    gr_ngps_tcache_set(Current_font->bitmap, 0);
    Gs_packet.flush();

    // Same header as gr_string's.
    sprite = (FontSprite*)Gs_packet.cur;
    hdr = (FontPacketHeader*)sprite;
    Gs_packet.cur[0] = GIF_TAG(1, 0, 0, 0, GIF_FLG_PACKED, 4);
    Gs_packet.cur[1] = GIF_REG_AD | (GIF_REG_AD << 4) | (GIF_REG_AD << 8) | (GIF_REG_RGBAQ << 12);
    Gs_packet.cur += sizeof(FontPacketHeader) / sizeof(u64);
    hdr->prim.addr = GS_PRIM;
    hdr->prim.data = GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST;
    hdr->prmode.addr = GS_PRMODE;
    hdr->prmode.data = GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST;
    hdr->alpha.addr = GS_ALPHA_1;
    hdr->alpha.data = GS_ALPHA_SET(0, 1, 0, 1);
    r = (gr_screen.current_color.r + 1) >> 1;
    g = (gr_screen.current_color.g + 1) >> 1;
    b = (gr_screen.current_color.b + 1) >> 1;
    a = (gr_screen.current_color.a + 1) >> 1;
    hdr->r = r;
    hdr->g = g;
    hdr->b = b;
    hdr->a = a;

    count = 0;
    h = Current_font->h;
    base_x = gr_screen.offset_x + (Gs_state.offset_x >> 4);
    sy = y + (gr_screen.offset_y + (Gs_state.offset_y >> 4));
    sy0 = 16.0f * sy;
    sy1 = 16.0f * (sy + h * scale);
    sprite += 3;
    while (*text) {
        u8 c = *text;
        text++;
        letter = get_char_width(c, *text, &width, &spacing);
        count++;
        sx = x + base_x;
        ch = &Current_font->char_data[letter];
        u = Current_font->bm_u[letter] << 4;
        v = Current_font->bm_v[letter] << 4;
        sprite->u0 = u + 7;
        sprite->v0 = v + 7;
        sprite->x0 = (int)(16.0f * sx);
        sprite->y0 = sy0;
        sprite->z0 = FONT_Z;
        // The texture size of the glyph is not scaled.
        sprite->u1 = u + ((ch->byte_width << 4) & 0xFFFF);
        sprite->v1 = v + ((h << 4) & 0xFFFFL);
        sprite->x1 = (int)(16.0f * (sx + ch->byte_width * scale));
        sprite->y1 = sy1;
        sprite->z1 = FONT_Z;
        sprite++;
        x += spacing * scale;
    }

    Gs_packet.cur[0] = GIF_TAG(count, 0, 0, 0, GIF_FLG_REGLIST, 4);
    Gs_packet.cur[1] = GIF_REG_UV | (GIF_REG_XYZ2 << 4) | (GIF_REG_UV << 8) | (GIF_REG_XYZ2 << 12);
    Gs_packet.cur += count * 4 + 2;
    Gs_packet.flush();
}

int gr_split_str(int* line_lengths, int* line_starts, const char* text, int max_w, int max_lines, char ignore_char,
                 int font_num) {
    char buffer[512];
    int sw;
    int sh;
    int breakpoint = -1;
    int new_line;
    int last_was_white = 0;
    int ignore_until_whitespace;
    int i;
    int line_num = 0;
    int buf_index;

    if (text == NULL) {
        return 0;
    }

    memset(buffer, 0, sizeof(buffer));
    buf_index = 0;
    ignore_until_whitespace = 0;

    // Skip leading whitespace.
    i = 0;
    while (is_white_space(text[i])) {
        i++;
    }

    new_line = 1;
    line_starts[0] = -1;
    for (; i < (int)strlen(text); i++) {
        if (line_num >= max_lines) {
            return line_num;
        }

        if (new_line) {
            line_starts[line_num] = -1;
            if (is_gray_space(text[i])) {
                continue;
            }
            line_starts[line_num] = i;
            breakpoint = -1;
            new_line = 0;
        }

        if (ignore_until_whitespace) {
            if (is_white_space(text[i])) {
                ignore_until_whitespace = 0;
            }
            continue;
        }

        if (text[i] == '\n') {
            line_lengths[line_num] = i - line_starts[line_num];
            line_num++;
            line_starts[line_num] = -1;
            new_line = 1;
            memset(buffer, 0, sizeof(buffer));
            buf_index = 0;
            continue;
        }

        if (text[i] == ignore_char) {
            ignore_until_whitespace = 1;
            continue;
        }

        // Lines can break after a hyphen or at the first of a run of spaces.
        if (i > 0 && text[i - 1] == '-') {
            breakpoint = i;
        }
        if (is_gray_space(text[i])) {
            if (!last_was_white) {
                breakpoint = i;
            }
            last_was_white = 1;
        } else {
            last_was_white = 0;
        }

        buffer[buf_index] = text[i];
        buf_index++;
        buffer[buf_index] = 0;
        gr_get_string_size(&sw, &sh, buffer, -1, font_num);
        if (sw >= max_w) {
            int end;

            if (breakpoint >= 0) {
                end = breakpoint;
                if (breakpoint > 0 && text[breakpoint - 1] == '-') {
                    i = breakpoint - 1;
                } else {
                    i = breakpoint;
                }
            } else {
                // No place to break: split here and start the next line with this character.
                end = i;
                i--;
            }
            line_lengths[line_num] = end - line_starts[line_num];
            line_num++;
            line_starts[line_num] = -1;
            new_line = 1;
            memset(buffer, 0, sizeof(buffer));
            buf_index = 0;
        }
    }

    if (line_starts[line_num] >= 0) {
        line_lengths[line_num] = i - line_starts[line_num];
        line_num++;
    }
    return line_num;
}
