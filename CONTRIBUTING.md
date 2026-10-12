# Contributing

Thanks for helping. This page covers how a function goes from assembly to matching C++. All code is
written from scratch by reading the compiled game; never use original or leaked source.
Build setup is in the [README](README.md); compiler details are in [docs/compiler.md](docs/compiler.md).

## How the build fits together

- `config/SLUS_204.48.yaml` tells splat how to cut the executable into **units**. A unit is one
  translation unit (one `.cpp` file in the original project), made of a `.text` piece plus the
  `.rodata`, `.data`, `.sdata`, `.sbss` and `.bss` pieces that belong to it, all sharing a name.
  Units named like `text_00199BB0` are stretches of the binary that haven't been split into files yet.
- For every unit, the build assembles the original code and data into a **target** object
  (`build/target/<unit>.o`). If `src/<unit>.cpp` exists it is compiled to a **base** object
  (`build/src/<unit>.o`), and objdiff compares the two function by function.
- Units listed as `Matching` in `UNIT_STATUS` in `configure.py` are linked from source instead of
  assembly. `ninja` fails if the rebuilt executable differs from the original by even one byte.

## Workflow

1. **Find a unit.** Retail asserts still embed `__FILE__`, so strings such as `"memory.cpp"` mark
   which functions belong to which file. Within a unit, CodeWarrior keeps functions in source
   order and lays out each unit's data in the same order as its code, so the strings and
   variables a group of functions uses tell you where the unit's data starts and ends.
2. **Split it.** Add the unit's pieces to `config/SLUS_204.48.yaml`, splitting the neighbouring
   placeholder units around them. Keep every piece in address order. `.rodata` and `.sdata`
   pieces need `linker_section_order: .data`, and `.sbss` pieces need
   `linker_section_order: .bss`; without these splat regroups sections by type and the layout
   breaks.
3. **Name symbols** in `config/symbol_addrs.txt`. Functions use their CodeWarrior-mangled
   names (`allocate_in_mempool__FP7MempoolUi`); global variables are not mangled. Functions
   whose real name is still unknown keep splat's `func_XXXXXXXX` name and are declared
   `extern "C"` in C++ until someone names them.
4. **Write the C++** in `src/<unit>.cpp`, then run `.venv/bin/python configure.py` and
   `.venv/bin/ninja`. Use the objdiff GUI (open this folder) to see per-instruction differences.
5. **Mark it `Matching`** in `configure.py` once every function matches and `ninja` still
   passes with the unit linked from source.

## Conventions

- Use the typedefs in `include/types.h` (`u8`, `s32`, `f32`, ...).
- Hard errors use `DEBUGERRORLINE(line, message)` from `include/macros.h`, which emits the
  `DebugError` call followed by `break 0; nop` like the retail build. The line numbers are
  the PS2 build's, read from the call's argument.
- Format with the repository's `.clang-format`.
- Never commit anything extracted from the game: the executable, `asm/`, `assets/` or `build/`.
  All of these are already in `.gitignore`.

## objdiff GUI

Open this folder in the [objdiff](https://github.com/encounter/objdiff/releases) GUI to compare each
function in `src/` against the original, instruction by instruction. It rebuilds the unit when you
save.

## CI and decomp.dev

The [Build workflow](.github/workflows/build.yml) builds on every push, checks the result matches,
and uploads `build/report.json` as the `SLUS_204.48_report` artifact that decomp.dev reads.
The executable can't be committed, so CI downloads it from a private URL in the repository secret
`ELF_URL`. Until that secret exists, CI uploads `progress/report.json` instead, so refresh it in
any pull request that changes matching code:

```sh
.venv/bin/ninja report && cp build/report.json progress/report.json
```

## Tips

- If a function is identical except for register allocation or instruction order, try
  reordering independent statements or declaring locals in a different order before
  restructuring the logic.
- `frametime_initialize` only matched once its stores were reordered to match the original
  scheduling. The same habits show up across files.
