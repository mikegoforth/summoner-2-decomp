#include "game/plainstring.h"
#include "game/platform_ngps.h"
#include "game/unit_00196B10.h"
#include <stdio.h>
#include <string.h>

bool is_blank(const char* string) {
    int length = strlen(string);
    if (length <= 0) {
        return true;
    }
    for (int i = 0; i < length; i++) {
        if (string[i] != ' ' && string[i] != '\t' && string[i] != '\n') {
            return false;
        }
    }
    return true;
}

void trim_whitespace(char* destination, const char* source, int max_length) {
    int length = strlen(source);
    int first;
    int last;
    int count;

    for (first = 0; first < length; first++) {
        if (source[first] != ' ' && source[first] != '\t' && source[first] != '\n') {
            break;
        }
    }
    for (last = length - 1; last >= 0; last--) {
        if (source[last] != ' ' && source[last] != '\t' && source[last] != '\n') {
            break;
        }
    }
    count = last + 1 - first;
    if (count <= 0) {
        destination[0] = '\0';
    } else {
        strncpy(destination, source + first, count);
        destination[count] = '\0';
    }
}

char* int_to_string_with_commas(char* destination, int size, int value) {
    char buffer[32];
    int negative;
    int length;
    int digits;
    int commas;
    int from;
    int to;

    sprintf(buffer, "%d", value);
    length = strlen(buffer);
    negative = value < 0 ? 1 : 0;
    digits = length - negative;
    commas = (digits - 1) / 3;
    if (length + commas >= size) {
        return NULL;
    }
    if (commas == 0) {
        strcpy(destination, buffer);
        return destination;
    }
    if (negative) {
        from = 1;
    } else {
        from = 0;
    }
    from += digits % 3;
    if (from == 0) {
        from = 3;
    }
    strncpy(destination, buffer, from);
    for (to = from; from < length; from += 3, to += 4) {
        destination[to] = ',';
        strncpy(&destination[to + 1], &buffer[from], 3);
    }
    destination[to] = '\0';
    return destination;
}

char* stristr(const char* string, const char* substring) {
    char lower_string[512];
    char lower_substring[512];
    int string_length = strlen(string);
    int substring_length = strlen(substring);
    int i;
    char* found;

    for (i = 0; i < string_length; i++) {
        lower_string[i] = ngps_tolower(string[i]);
    }
    lower_string[i] = '\0';
    for (i = 0; i < substring_length; i++) {
        lower_substring[i] = ngps_tolower(substring[i]);
    }
    lower_substring[i] = '\0';
    found = strstr(lower_string, lower_substring);
    if (found == NULL) {
        return found;
    }
    return (char*)&string[found - lower_string];
}

char* strlwr(char* string) {
    if (string) {
        char* p = string;
        char c;
        do {
            c = ngps_tolower(*p);
            *p++ = c;
        } while (c);
    }
    return string;
}

char* strupr(char* string) {
    if (string) {
        char* p = string;
        char c;
        do {
            c = ngps_toupper(*p);
            *p++ = c;
        } while (c);
    }
    return string;
}

char* int_to_string(char* destination, int size, int value) {
    char buffer[32];
    sprintf(buffer, "%d", value);
    strcpy(destination, buffer);
    return destination;
}

char* vector3_to_string(char* destination, int size, const vector3& v, int precision) {
    char buffer[256];
    sprintf(buffer, "< %.*f, %.*f, %.*f >", precision, v.x, precision, v.y, precision, v.z);
    strcpy(destination, buffer);
    return destination;
}
