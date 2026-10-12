#!/usr/bin/env python3
"""Raise the alignment of small .rodata sections in a compiled object to 8.

    python3 tools/fix_alignment.py build/src/plainstring.o

CodeWarrior marks string literals of 4 bytes or fewer as 4-byte aligned, but the game has every
string literal on an 8-byte boundary (plainstring.cpp's "%d", console.cpp's "+", "-", "/*"). The
original linker must have imposed that minimum; this does the same so GNU ld places them where the
game has them. Only the section headers change, never the code or data.
"""
import struct
import sys
from pathlib import Path

MIN_ALIGN = 8
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
        if name == ".rodata" and 0 < h[8] < MIN_ALIGN:
            h[8] = MIN_ALIGN
            SHDR.pack_into(data, e_shoff + i * e_shentsize, *h)
    path.write_bytes(data)


if __name__ == "__main__":
    main()
