# Compiler

**Metrowerks CodeWarrior for PS2, `mwccps2` 3.0.3 (build 2002-07-16)**, available as
`mwcps2-3.0.3-020716` in [decompme/compilers](https://github.com/decompme/compilers).

Flags: `-O4,p -str readonly -Cpp_exceptions off`

## How it was identified

The ELF's `.comment` section reads `MW MIPS C Compiler (2.4.1.01)`. Every CodeWarrior PS2 build
from 2.4 (2000-12-13) through 3.0.3 (2002-07-16) embeds that same string, so it only narrows the
search. Test compiles of `memory.cpp` and `frametime.cpp` against the original instructions then
gave:

| Build | `memory.cpp` (11 functions) | `frametime.cpp` (6 functions) |
|---|---|---|
| 2.4-001213 | 6 | 3 |
| 3.0-011126 | 11 | 5 |
| 3.0.1-020123 | 10 | 5 |
| 3.0.3-020716 | 10 | 6 |
| 3.0b22-020926 | 6 | 5 |

The deciding evidence is float-to-int conversion. 3.0.3 compiles `(int)f` to an inline
`trunc.w.s`, which binutils encodes as R5900 `cvt.w.s` (`0x46000024`). Earlier builds call a
runtime helper, `fptosi`, instead. The game's code has about 960 inline conversions and never
calls such a helper, so it was built with 3.0.3 or something very close to it.

## Flags

- `-O4,p`: `,p` (optimize for speed) is required; without it, loop unrolling and scheduling
  differ. `-O3,p` has produced identical code so far.
- `-str readonly`: string literals go to `.rodata`. Without it they land in small data
  (`$gp`-relative), which the game never does for strings.
- `-Cpp_exceptions off`: with exceptions on, functions with exception specifications (such as
  `operator delete`) gain calls to `__unexpected`.

## Quirks

- **Retail asserts.** Hard errors compile to `jal DebugError` followed by `break 0; nop`.
  CodeWarrior's inline assembler treats `break` as the C keyword, so `include/macros.h` spells it
  `asm(".word 0x0000000D"); asm("nop");`.
- **Static initializers** (`__sinit_<file>_cpp`) go to `.init`, with a pointer in `.ctor`. In
  the executable these sit together after `.rodata` (`0x2CF680` and `0x2D39D0`), not next to
  their file's code. They aren't split per file yet, so a variable that only a static
  initializer writes must stay non-`static` for now (see `min_frametime` in `src/frametime.cpp`).
- **Delay slots and `a0`.** 3.0.3 won't move an instruction that writes `$a0` into a branch
  delay slot when the function takes an argument in `$a0`; 3.0-011126 will.
  `allocate_in_commonblock` (`memory.cpp`) has that slot filled in the game and is the only
  known function 3.0.3 doesn't reproduce. Either the source differs in a way not found yet, or
  the game used a build between 3.0.1 and 3.0.3 that isn't archived.

## Memory layout

| Address | Contents |
|---|---|
| `0x00100000` | `.text`: game code, with the Sony SDK 2.4 libraries (built with ee-gcc) in between |
| `0x002A3380` | VU1 microcode |
| `0x002A8300` | `.data`, then `.rodata` (start estimated at `0x002B3100`) |
| `0x002CF680` | `.init`: static initializers |
| `0x002D39D0` | `.ctor`: static initializer table, 135 entries |
| `0x002D3D00` | `.sdata`; `_gp` = `0x002DBCF0` |
| `0x002D4200` | `.sbss`, then `.bss` up to `_end` = `0x018E0300` |
