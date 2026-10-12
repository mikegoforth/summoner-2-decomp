# Summoner 2

[![Build](https://github.com/mikegoforth/summoner-2-decomp/actions/workflows/build.yml/badge.svg)](https://github.com/mikegoforth/summoner-2-decomp/actions/workflows/build.yml)

This project rebuilds the PlayStation 2 version of Summoner 2 (2002) from C++ source.
Each function is rewritten until the game's original compiler turns it back into exactly the
bytes on the disc.

## How the code is written

Every function is checked against the original in objdiff before it counts. The compiler and
its quirks are written up in [docs/compiler.md](docs/compiler.md).

## Building

You need a Mac with Apple Silicon or an x86-64 Linux machine, Python 3.11 or newer, and your own
copy of the game. PS2 binutils, the compiler, wibo (which runs the Windows-only compiler) and
objdiff-cli are fetched into `tools/` automatically; Macs also need Rosetta 2.

`configure.py` disassembles the game's executable with [splat](https://github.com/ethteck/splat)
and generates the ninja build. `ninja` compiles `src/`, links every unit marked `Matching` from
source, and stops if a single byte differs from the original. `ninja progress` prints the current
numbers.

Want to help? Start with [CONTRIBUTING.md](CONTRIBUTING.md).
