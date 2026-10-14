#!/usr/bin/env python3
"""Raise the alignment of small .rodata, .sdata and .sbss sections in a compiled object.

    python3 tools/fix_alignment.py build/src/plainstring.o

CodeWarrior gives each string literal and variable its own section, and the game has some of them
on coarser boundaries than those sections ask for, so the original linker must have imposed a
minimum per section. This raises the alignment in the section headers to that minimum so that GNU
ld places them the same way. Only the section headers change, never the code or data.

- .rodata: 8. String literals of 4 bytes or fewer are marked 4-byte aligned, but every string
  literal in the game is on an 8-byte boundary (plainstring.cpp's "%d", console.cpp's "+", "-",
  "/*").
- .sdata and .sbss: 4. Bools, chars and the guard bytes of function-local statics are 1-byte
  aligned, but the game has them 4 bytes apart (guard bytes at 0x2D47EC-0x2D480C, bools at
  0x2D4964 and 0x2D4968).

.data and .bss need no minimum: only variables over 8 bytes go there, and CodeWarrior already
aligns those to 8 or 16, which is where the game has them (the 12-byte Cond_packfile_state is on an
8-byte boundary at 0x957AF8).
"""
import struct
import sys
from pathlib import Path

MIN_ALIGN = {".rodata": 8, ".sdata": 4, ".sbss": 4}
SHDR = struct.Struct("<IIIIIIIIII")  # Elf32_Shdr


def main() -> None:
    path = Path(sys.argv[1])
    data = bytearray(path.read_bytes())
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        sys.exit(f"{path}: not a little-endian ELF32 object")
    e_shoff, = struct.unpack_from("<I", data, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 0x2E)

    def header(i):
        return list(SHDR.unpack_from(data, e_shoff + i * e_shentsize))

    strtab_offset = header(e_shstrndx)[4]
    for i in range(e_shnum):
        h = header(i)
        name_end = data.index(b"\0", strtab_offset + h[0])
        name = data[strtab_offset + h[0] : name_end].decode()
        minimum = MIN_ALIGN.get(name, 0)
        if 0 < h[8] < minimum:
            h[8] = minimum
            SHDR.pack_into(data, e_shoff + i * e_shentsize, *h)
    path.write_bytes(data)


if __name__ == "__main__":
    main()
