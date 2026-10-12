#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void free(void* ptr);
double atof(const char* s);
int atoi(const char* s);
long strtol(const char* s, char** end, int base);

#ifdef __cplusplus
}
#endif

#endif
