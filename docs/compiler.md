# Compiler

**Metrowerks CodeWarrior for PS2 3.0 (`mwccps2` build 2001-11-26)**, with `#pragma fast_fptosi on`.
It is `mwcps2-3.0-011126` in [decompme/compilers](https://github.com/decompme/compilers).

Flags: `-O4,p -str readonly -Cpp_exceptions off -pragma "fast_fptosi on"`

## How it was identified

The ELF's `.comment` section reads `MW MIPS C Compiler (2.4.1.01)`. Every CodeWarrior PS2 build
from 2.4 (2000-12-13) through 3.0.3 (2002-07-16) embeds that same string, so it only narrows the
search. Test compiles of 45 functions from `memory.cpp`, `frametime.cpp`, `timestamp.cpp`,
`stringpool.cpp` and `timer.cpp` settled it:

| Build | Default | With `fast_fptosi on` |
|---|---|---|
| 3.0-011126 | 44 | **45** |
| 3.0.1-020123 | 42 | 43 |
| 3.0b22-020123 | 42 | 43 |
| 3.0.3-020716 | 42 | 42 |
| 3.0b22-020716 | 42 | 42 |

2.4-001213 and 3.0b22-020926 were ruled out earlier on `memory.cpp` and `frametime.cpp` alone.
Three things separate the builds:

- **Float-to-int conversion.** By default, 3.0 and 3.0.1 compile `(int)f` to a call to the runtime
  helper `fptosi`. With `fast_fptosi` they emit an inline `trunc.w.s`, which binutils encodes as
  R5900 `cvt.w.s` (`0x46000024`). The game has about 960 of those and never calls a helper. 3.0.3
  inlines the conversion either way, which is why it looked like the right build at first.
- **Delay slots.** Later builds won't fill a branch delay slot with an instruction that writes a
  register still live into the branch's block, such as an incoming argument in `$a0`. The game
  fills them (`allocate_in_commonblock`, `timer_get`).
- **Short loops.** The game pads short loops with `nop`s to six instructions before the
  backward branch, a workaround for an R5900 short-loop erratum; 3.0.3 pads to five
  (`timer_wait`). `#pragma padloop` controls this padding.

`tools/match.py --compiler <build> --pragma "<pragma>"` reruns this kind of comparison.

## Flags

- `-O4,p`: `,p` (optimize for speed) is required; without it, loop unrolling and scheduling
  differ. `-O3,p` has produced identical code so far.
- `-str readonly`: string literals go to `.rodata`. Without it they land in small data
  (`$gp`-relative), which the game never does for strings.
- `-Cpp_exceptions off`: with exceptions on, functions with exception specifications (such as
  `operator delete`) gain calls to `__unexpected`.
- `fast_fptosi` has no command-line switch, so it is passed with `-pragma`. The original build
  probably set it in the CodeWarrior project settings or a prefix file.

## Quirks

- **Retail asserts.** Hard errors compile to `jal DebugError` followed by `break 0; nop`.
  CodeWarrior's inline assembler treats `break` as the C keyword, so `include/macros.h` spells it
  `asm(".word 0x0000000D"); asm("nop");`.
- **String alignment when linking.** CodeWarrior gives string literals shorter than 4 bytes
  4-byte alignment, but the game's linker put `plainstring.cpp`'s `"%d"` on an 8-byte boundary.
  Until that rule is understood, `plainstring.cpp` is compared in objdiff but not linked from
  source.
- **Static initializers** (`__sinit_<file>_cpp`) go to `.init`, with a pointer in `.ctor`. In
  the executable these sit together after `.rodata` (`0x2CF680` and `0x2D39D0`), not next to
  their file's code. They aren't split per file yet, so a variable that only a static
  initializer writes must stay non-`static` for now (see `min_frametime` in `src/frametime.cpp`).

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
