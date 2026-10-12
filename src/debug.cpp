#include "game/debug.h"
#include <stdio.h>

// TODO: graphics functions from other units (initialized check, text extents and drawing, ...).
extern "C" int func_0017CE70(void);
extern "C" void func_00183810(int* width, int* height, const char* text, int length, int flags);
extern "C" void func_0017BBE0(void);
extern "C" void func_0018B210(int mode);
extern "C" void func_0017BD90(int r, int g, int b, int a);
extern "C" void func_0017C450(int x, int y, int width, int height, int z);
extern "C" void func_001839D0(int x, int y, const char* text, int flags);
extern "C" void func_00188520(int mode);
extern "C" void func_00188230(void);

// TODO: owned by another unit; set by a static initializer.
extern int D_002D4674;

// Shows the message centered on a dark blue box.
void DebugPrint(char* buffer) {
    int width;
    int height;

    if (func_0017CE70()) {
        func_00183810(&width, &height, buffer, -1, 0);
        func_0017BBE0();
        func_0018B210(0x48);
        func_0017BD90(0x20, 0x20, 0x80, 0xFF);
        func_0017C450(0, 0, 512, 448, D_002D4674);
        func_0017BD90(0xFF, 0xFF, 0xFF, 0xFF);
        func_001839D0(256 - width / 2, 224 - height / 2, buffer, 0);
        func_00188520(0);
        func_00188230();
    }
}

int DebugError(const char* File, u32 Line, const char* Error) {
    char charBuffer[1052];

    sprintf(charBuffer, "Error: %s\nFile: %s - Line: %d\n\n", Error, File, Line);
    DebugPrint(charBuffer);
    return 0;
}
