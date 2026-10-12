#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char* strcpy(char* dst, const char* src);
size_t strlen(const char* s);

#ifdef __cplusplus
}
#endif

#endif
