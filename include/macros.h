#ifndef MACROS_H
#define MACROS_H

#include "game/debug.h"

// Emits `break 0; nop`. MWCC's inline assembler treats `break` as the C keyword,
// so the instruction is spelled out as a raw word.
#define BREAKPOINT()               \
    do {                           \
        asm(".word 0x0000000D");   \
        asm("nop");                \
    } while (0)

// Release (retail) builds keep hard errors but compile asserts out.
#define DEBUGERRORLINE(line, error)          \
    do {                                     \
        DebugError(__FILE__, line, error);   \
        BREAKPOINT();                        \
    } while (0)

#define DEBUGASSERTLINE(line, cond) (void)0

#endif // MACROS_H
