#ifndef _SETJMP_H
#define _SETJMP_H

// 0xD0 bytes: s0-s7 and fp as quadwords, f24-f31, then sp and ra as quadwords.
typedef int jmp_buf[52];

#ifdef __cplusplus
extern "C" {
#endif

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int value);

#ifdef __cplusplus
}
#endif

#endif
