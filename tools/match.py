#!/usr/bin/env python3
"""Compile one source file and compare its functions with the original executable.

    .venv/bin/python tools/match.py src/timer.cpp timer_get__Fi=0x19AD90
    .venv/bin/python tools/match.py -v src/timer.cpp timer_get__Fi=0x19AD90

Each argument after the source file pairs a (mangled) symbol in the compiled object with the
address of the original function. Instruction words are compared directly, ignoring the fields
that relocations fill in, so this works before a unit's data has been split out in config/.
objdiff stays the final word once the unit is in the build. -v prints both listings side by side.
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
os.chdir(ROOT)
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools"))

import rabbitizer  # noqa: E402

import configure  # noqa: E402

ROM = ROOT / "iso" / "SLUS_204.48.rom"
VRAM = 0x00100000
OBJDUMP = f"{configure.BINUTILS}-objdump"
FUNC_HEADER = re.compile(r"^nonmatching (\S+), (0x[0-9A-F]+)\n\n(?:/\*.*\*/\n)?glabel \S+\n\s+/\* [0-9A-F]+ ([0-9A-F]{8}) ", re.M)


def target_sizes() -> dict[int, int]:
    """Size of every function splat found, keyed by address."""
    sizes = {}
    for asm in (ROOT / "asm").glob("*.s"):
        for m in FUNC_HEADER.finditer(asm.read_text()):
            sizes[int(m.group(3), 16)] = int(m.group(2), 16)
    return sizes


def compile_source(src: Path, out: Path) -> None:
    env = dict(os.environ, MWCIncludes="include")
    cmd = [str(configure.WIBO), str(configure.MWCC), *configure.CFLAGS.split(), "-c", "-o", str(out), str(src)]
    if sys.platform == "win32":
        cmd = cmd[1:]
    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    messages = "\n".join(
        line for line in (result.stdout + result.stderr).splitlines() if "MWCIncludes" not in line and "Usage Warning" not in line
    ).strip()
    if result.returncode != 0 or not out.exists():
        sys.exit(messages or "compile failed")
    if messages:
        print(messages)


def base_functions(obj: Path) -> dict[str, list[tuple[int, bool, str]]]:
    """{symbol: [(word, has_relocation, text), ...]} from the compiled object."""
    out = subprocess.run([OBJDUMP, "-d", "-r", "-z", str(obj)], capture_output=True, text=True, check=True).stdout
    funcs: dict[str, list] = {}
    current = None
    for line in out.splitlines():
        header = re.match(r"^[0-9a-f]+ <(.+)>:", line)
        if header:
            current = funcs.setdefault(header.group(1), [])
            continue
        ins = re.match(r"^\s+[0-9a-f]+:\s+([0-9a-f]{8})\s+(.*)", line)
        if ins and current is not None:
            current.append([int(ins.group(1), 16), False, re.sub(r"\s+", " ", ins.group(2)).strip()])
            continue
        reloc = re.match(r"^\s+[0-9a-f]+: (R_MIPS_\w+)\s+(\S+)", line)
        if reloc and current:
            current[-1][1] = True
            current[-1][2] += f"  <{reloc.group(2)}>"
    return funcs


def target_words(rom: bytes, addr: int, size: int) -> list[int]:
    return list(struct.unpack_from(f"<{size // 4}I", rom, addr - VRAM))


def disassemble(word: int, addr: int) -> str:
    ins = rabbitizer.Instruction(word, vram=addr, category=rabbitizer.InstrCategory.R5900)
    return re.sub(r"\s+", " ", ins.disassemble()).replace("$", "")


def same(target: int, base: int, relocated: bool) -> bool:
    if not relocated:
        return target == base
    mask = 0xFC000000 if (target >> 26) in (2, 3) else 0xFFFF0000
    return target & mask == base & mask


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-v", "--verbose", action="store_true", help="print side-by-side listings")
    parser.add_argument("source", type=Path)
    parser.add_argument("pairs", nargs="+", metavar="symbol=0xADDRESS")
    args = parser.parse_args()

    rom = ROM.read_bytes()
    sizes = target_sizes()
    with tempfile.TemporaryDirectory() as tmp:
        obj = Path(tmp) / "match.o"
        compile_source(args.source, obj)
        funcs = base_functions(obj)

    matched = 0
    for pair in args.pairs:
        symbol, _, addr_text = pair.partition("=")
        addr = int(addr_text, 16)
        base = funcs.get(symbol)
        if base is None:
            print(f"{symbol}: not in the compiled object (have: {', '.join(sorted(funcs)) or 'nothing'})")
            continue
        size = sizes.get(addr)
        if size is None:
            print(f"{symbol}: no function starts at {addr:#x} in asm/")
            continue
        target = target_words(rom, addr, size)
        # splat counts trailing alignment nops as part of the function
        while len(target) > len(base) and target[-1] == 0:
            target.pop()
        diffs = sum(
            1
            for i in range(max(len(target), len(base)))
            if i >= len(target) or i >= len(base) or not same(target[i], base[i][0], base[i][1])
        )
        status = "MATCH" if diffs == 0 else f"{diffs} differences ({len(base)} vs {len(target)} instructions)"
        print(f"{symbol:48s} {addr:#010x}  {status}")
        matched += diffs == 0
        if args.verbose and diffs:
            for i in range(max(len(target), len(base))):
                t = disassemble(target[i], addr + i * 4) if i < len(target) else ""
                b = base[i][2] if i < len(base) else ""
                ok = i < len(target) and i < len(base) and same(target[i], base[i][0], base[i][1])
                print(f"  {i * 4:4X} {'  ' if ok else '!!'} {t[:46]:<46} | {b}")
    print(f"{matched}/{len(args.pairs)} functions match")


if __name__ == "__main__":
    main()
