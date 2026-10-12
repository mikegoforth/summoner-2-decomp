#ifndef GAME_PLAINSTRING_H
#define GAME_PLAINSTRING_H

#include "types.h"

// The formatting functions take the destination's size, but only
// int_to_string_with_commas checks it.

class vector3;

bool is_blank(const char* string);
void trim_whitespace(char* destination, const char* source);
char* int_to_string_with_commas(char* destination, int size, int value);
char* stristr(const char* string, const char* substring);
char* strlwr(char* string);
char* strupr(char* string);
char* int_to_string(char* destination, int size, int value);
char* vector3_to_string(char* destination, int size, const vector3& v, int precision);

#endif // GAME_PLAINSTRING_H
