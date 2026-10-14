#!/usr/bin/env python3
"""Drop the copies of inline functions in a compiled object that the game keeps elsewhere.

    python3 tools/drop_inline_copies.py build/src/unit_00196480.o config/symbol_addrs.txt 0x196480-0x196B10

When CodeWarrior doesn't inline a call to an inline function, it emits a copy of the function in
its own .text section, under a symbol with binding 13 (its "multiply defined" binding). Vtables
and the statics of inline functions get the same binding. Every object that needs one carries its
own copy, and the original linker kept just one: the game has a single vector3(float, float, float)
constructor, at 0x1026F0. GNU ld treats binding 13 as an ordinary global, so it either keeps the
extra copy, shifting everything linked after it, or stops on a duplicate definition.

A copy is dropped when symbol_addrs.txt names it at an address outside the object's unit, whose
original address ranges follow as START-END arguments. Its symbol becomes an undefined global, so
references resolve to the game's copy. Its section loses its alloc flags and is renamed with a
.discard prefix (.discard.text), so the generated linker script discards it with its relocations
(its /DISCARD/ rule takes every section the script doesn't place) and objdiff ignores it. A copy
the game keeps inside the unit, or one that symbol_addrs.txt doesn't name, stays: the unit may
hold the game's copy, and naming the game's copy drops the others.
"""
import argparse
import re
import struct
import sys
from pathlib import Path

SHDR = struct.Struct("<IIIIIIIIII")  # Elf32_Shdr
SYM = struct.Struct("<IIIBBH")  # Elf32_Sym: name, value, size, info, other, shndx
SHT_SYMTAB = 2
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR = 0x1, 0x2, 0x4
SHN_UNDEF, SHN_LORESERVE = 0, 0xFF00
STB_GLOBAL, STB_MULTIDEF = 1, 13
STT_NOTYPE, STT_FUNC = 0, 2
SYMBOL_LINE = re.compile(r"\s*([^\s=/]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;")


def read_symbol_addrs(path: Path) -> dict[str, int]:
    names = {}
    for line in path.read_text().splitlines():
        m = SYMBOL_LINE.match(line)
        if m:
            names[m.group(1)] = int(m.group(2), 0)
    return names


def parse_range(text: str) -> tuple[int, int]:
    start, _, end = text.partition("-")
    return int(start, 0), int(end, 0)


class ElfObject:
    def __init__(self, path: Path):
        self.data = bytearray(path.read_bytes())
        if self.data[:4] != b"\x7fELF" or self.data[4] != 1 or self.data[5] != 1:
            sys.exit(f"{path}: not a little-endian ELF32 object")
        self.shoff, = struct.unpack_from("<I", self.data, 0x20)
        self.shentsize, self.shnum, self.shstrndx = struct.unpack_from("<HHH", self.data, 0x2E)

    def section(self, i: int) -> list[int]:
        return list(SHDR.unpack_from(self.data, self.shoff + i * self.shentsize))

    def set_section(self, i: int, h: list[int]) -> None:
        SHDR.pack_into(self.data, self.shoff + i * self.shentsize, *h)

    def string(self, table: int, offset: int) -> str:
        start = self.section(table)[4] + offset
        return self.data[start : self.data.index(b"\0", start)].decode()

    def section_name(self, i: int) -> str:
        return self.string(self.shstrndx, self.section(i)[0])

    def add_section_name(self, name: str) -> int:
        """Offset of name in the section name table. A new name is appended to a copy of the
        table at the end of the file."""
        h = self.section(self.shstrndx)
        table = bytes(self.data[h[4] : h[4] + h[5]])
        found = table.find(b"\0" + name.encode() + b"\0")
        if found != -1:
            return found + 1
        self.data += bytes(-len(self.data) % 4)
        h[4], h[5] = len(self.data), len(table) + len(name) + 1
        self.data += table + name.encode() + b"\0"
        self.set_section(self.shstrndx, h)
        return len(table)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("object", type=Path)
    parser.add_argument("symbol_addrs", type=Path)
    parser.add_argument("own", nargs="*", type=parse_range, metavar="START-END", help="address ranges of the object's unit")
    args = parser.parse_args()

    game_symbols = read_symbol_addrs(args.symbol_addrs)
    obj = ElfObject(args.object)
    symtab_index = next((i for i in range(obj.shnum) if obj.section(i)[1] == SHT_SYMTAB), None)
    if symtab_index is None:
        return
    symtab = obj.section(symtab_index)
    strtab, first_global = symtab[6], symtab[7]
    symbols = [list(SYM.unpack_from(obj.data, symtab[4] + i * SYM.size)) for i in range(symtab[5] // SYM.size)]

    changed = False
    for i, (st_name, _, st_size, st_info, st_other, st_shndx) in enumerate(symbols):
        if st_info >> 4 != STB_MULTIDEF or st_shndx == SHN_UNDEF or st_shndx >= SHN_LORESERVE:
            continue
        name = obj.string(strtab, st_name)
        address = game_symbols.get(name)
        if address is None or any(start <= address < end for start, end in args.own):
            continue
        if i < first_global:
            print(f"{args.object}: keeping {name}: it is among the local symbols", file=sys.stderr)
            continue
        if any(s[5] == st_shndx for j, s in enumerate(symbols) if j != i):
            print(f"{args.object}: keeping {name}: its section holds other symbols", file=sys.stderr)
            continue
        # An undefined global, written the way CodeWarrior refers to another file's symbols.
        st_type = st_info & 0xF
        if st_type == STT_FUNC:
            st_type, st_size = STT_NOTYPE, 0
        symbols[i] = [st_name, 0, st_size, STB_GLOBAL << 4 | st_type, st_other, SHN_UNDEF]
        SYM.pack_into(obj.data, symtab[4] + i * SYM.size, *symbols[i])
        h = obj.section(st_shndx)
        h[0] = obj.add_section_name(".discard" + obj.section_name(st_shndx))
        h[2] &= ~(SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR)
        obj.set_section(st_shndx, h)
        changed = True

    if changed:
        args.object.write_bytes(obj.data)


if __name__ == "__main__":
    main()
