#!/usr/bin/env python3
"""Generate build.ninja and objdiff.json for the Summoner 2 (PS2) decompilation.

    python3 configure.py            # split with splat, then write build files
    python3 configure.py --no-split # reuse the existing split
    ninja                           # build and verify the ELF matches
    ninja report                    # objdiff progress report (build/report.json)

Each translation unit (TU) is a set of splat subsegments sharing a name. Its
"target" object is assembled from the original code/data; its "base" object is
compiled from src/<name>.cpp when that file exists. objdiff compares the two.
Units marked Matching below are linked from source instead of assembly, and the
build fails if the result is not byte-identical.
"""
import argparse
import json
import re
import shlex
import subprocess
import sys
from collections import OrderedDict
from pathlib import Path

from ninja import ninja_syntax

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools"))
import download_tool  # noqa: E402

GAME = "SLUS_204.48"
SPLAT_CONFIG = Path("config") / f"{GAME}.yaml"
SPLAT_LD = Path("build") / f"{GAME}.ld"
ELF = Path("iso") / GAME
ROM = Path("iso") / f"{GAME}.rom"
BUILD = Path("build")
SYMBOL_ADDRS = Path("config") / "symbol_addrs.txt"

BINUTILS = Path("tools/binutils/mips-ps2-decompals")
WIBO = Path("tools/bin/wibo")
OBJDIFF = Path("tools/bin/objdiff-cli")
MWCC = Path("tools/compilers") / download_tool.COMPILER / "mwccps2.exe"
# Run on every compiled object so that GNU ld places it where the original linker did
# (see Quirks in docs/compiler.md).
FIX_ALIGNMENT = Path("tools/fix_alignment.py")
DROP_INLINE_COPIES = Path("tools/drop_inline_copies.py")

ASFLAGS = "-EL -march=r5900 -mabi=eabi -G 0 -no-pad-sections -mno-pdr -I include"
# fast_fptosi makes (int)float compile to an inline trunc.w.s instead of a call to fptosi;
# the game was built with it on (see docs/compiler.md).
CFLAGS = [
    "-O4,p",
    "-str", "readonly",
    "-Cpp_exceptions", "off",
    "-pragma", "fast_fptosi on",
    "-gccinc",
    "-Iinclude",
    "-Isrc",
]

# --------------------------------------------------------------------------
# Translation-unit status. Units not listed here are NonMatching if
# src/<name>.cpp exists, or assembly-only otherwise.
#   Matching    - linked from source; must stay byte-identical
#   NonMatching - compiled for objdiff only; the original assembly is linked
# --------------------------------------------------------------------------
Matching = "Matching"
NonMatching = "NonMatching"

UNIT_STATUS = {
    "bmpman_ngps": NonMatching,  # static initializer still in the shared .init assembly
    # console.cpp: its static initializer is still in the shared .init assembly, which refers to
    # this file's string literals by address, so it can only be linked once .init is split.
    "console": NonMatching,
    "debug": Matching,
    "file_packfile": Matching,
    "frametime": Matching,
    "gr": NonMatching,  # static initializer still in the shared .init assembly
    "gr_font": NonMatching,  # 11/12 functions match
    "gr_ngps_texture": NonMatching,  # 40/43 functions match
    "level_save": NonMatching,  # static initializer still in the shared .init assembly
    "level_scripts": NonMatching,  # static initializer still in the shared .init assembly
    "matrix": NonMatching,  # static initializer still in the shared .init assembly
    "memcard": Matching,
    "memcard_find": Matching,
    "memory": Matching,
    "ngps_dvd": Matching,
    "ngps_file": NonMatching,  # static initializer still in the shared .init assembly
    "norlsallocpool": Matching,
    "parse": Matching,
    "plainstring": Matching,
    "platform_ngps": Matching,
    "stringpool": Matching,
    "timer": Matching,
    "timestamp": Matching,
    "unit_0017A830": Matching,
    "unit_0017CEB0": NonMatching,  # static initializer still in the shared .init assembly
    "unit_00180370": NonMatching,  # static initializer still in the shared .init assembly
    "unit_00180B40": NonMatching,  # static initializer still in the shared .init assembly
    "unit_00180B70": Matching,
    "unit_00182180": Matching,
    "unit_001843A0": NonMatching,  # static initializer still in the shared .init assembly
    "unit_00187F80": NonMatching,  # static initializer still in the shared .init assembly
    "unit_0018DB80": NonMatching,  # static initializer still in the shared .init assembly
    "unit_0018E1D0": NonMatching,  # static initializer still in the shared .init assembly
    "unit_0018F070": NonMatching,  # static initializer still in the shared .init assembly
    "unit_00190720": Matching,
    "unit_00194A80": Matching,
    "unit_00195310": NonMatching,  # static initializer still in the shared .init assembly
    "unit_00196480": Matching,
    "unit_00196B10": NonMatching,  # static initializer still in the shared .init assembly
    "unit_00197D00": Matching,
    "vif": Matching,
    "vif_mesh": Matching,
}


def run_splat() -> None:
    cmd = [sys.executable, "-m", "splat", "split", SPLAT_CONFIG.name]
    print("running splat:", " ".join(cmd))
    subprocess.run(cmd, cwd=ROOT / "config", check=True)


def make_rom() -> None:
    if not ELF.exists():
        sys.exit(f"missing {ELF}: copy your SLUS_204.48 there (tools/extract_iso.py can pull it from your copy of the game)")
    if not ROM.exists() or ROM.stat().st_mtime < ELF.stat().st_mtime:
        subprocess.run([f"{BINUTILS}-objcopy", "-O", "binary", "--gap-fill=0x00", str(ELF), str(ROM)], check=True)


def source_for_object(obj: Path) -> Path:
    """Map a splat object path back to the file it is built from."""
    rel = obj.relative_to(BUILD)
    if rel.parts[0] == "assets":
        return rel.with_suffix(".bin")
    return rel.with_suffix(".s")


SECTION_FOR_SUFFIX = {"data": ".data", "rodata": ".rodata", "sdata": ".sdata", "sbss": ".sbss", "bss": ".bss"}
VRAM_COMMENT = re.compile(r"/\* (?:[0-9A-F]+ )?([0-9A-F]{8})\b")


def start_address(src: Path) -> int | None:
    """Original address of the first thing in a splat .s file."""
    with open(src) as f:
        for line in f:
            m = VRAM_COMMENT.search(line)
            if m:
                return int(m.group(1), 16)
    return None


def section_and_alignment(src: Path) -> tuple[str, int]:
    """Section name and natural alignment of a splat .s file.

    splat doesn't record the original section alignment, so when a compiled
    object ends earlier than the assembly it replaced (dropping trailing
    padding), the next assembly object would slide down. Giving each one the
    largest power-of-two alignment (up to MWCC's usual 16) that divides its
    original start address puts it back where it belongs.
    """
    parts = src.name.split(".")
    section = SECTION_FOR_SUFFIX.get(parts[-2], ".text") if len(parts) > 2 else ".text"
    vram = start_address(src)
    if vram is None:
        return section, 1
    return section, min(16, vram & -vram) if vram else 16


def unit_ranges(units) -> dict[str, list[tuple[int, int]]]:
    """Original address ranges of each unit's splat subsegments.

    The subsegments cover the whole executable, so each one runs up to the
    start of the next.
    """
    starts = []
    for name, objs in units.items():
        for obj in objs:
            src = source_for_object(obj)
            vram = start_address(src) if src.suffix == ".s" else None
            if vram is not None:
                starts.append((vram, name))
    starts.sort()
    ranges = {}
    for (start, name), (end, _) in zip(starts, starts[1:] + [(1 << 32, None)]):
        unit = ranges.setdefault(name, [])
        if unit and unit[-1][1] == start:
            unit[-1] = (unit[-1][0], end)
        else:
            unit.append((start, end))
    return ranges


def unit_name_for_object(obj: Path) -> str:
    rel = obj.relative_to(BUILD)
    if rel.parts[0] == "asm" and len(rel.parts) > 1 and rel.parts[1] == "data":
        # build/asm/data/<unit>.<section>.o
        return str(Path(*rel.parts[2:])).split(".")[0]
    return str(Path(*rel.parts[1:]).with_suffix(""))


def collect_units(ld_text: str):
    """Return {unit: [object paths]} in link order, from splat's linker script."""
    units = OrderedDict()
    for m in re.finditer(r"^\s*(build/\S+\.o)\(", ld_text, re.M):
        obj = Path(m.group(1))
        name = unit_name_for_object(obj)
        objs = units.setdefault(name, [])
        if obj not in objs:
            objs.append(obj)
    return units


def write_linker_script(ld_text: str, units) -> str:
    """Swap Matching units' assembly objects for their compiled objects."""
    for name, objs in units.items():
        if UNIT_STATUS.get(name) != Matching:
            continue
        base = BUILD / "src" / f"{name}.o"
        for obj in objs:
            ld_text = ld_text.replace(f"{obj}(", f"{base}(")
    return ld_text


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--no-split", action="store_true", help="skip running splat")
    args = parser.parse_args()

    download_tool.ensure()
    make_rom()
    if not args.no_split or not SPLAT_LD.exists():
        run_splat()

    ld_text = SPLAT_LD.read_text()
    units = collect_units(ld_text)

    for name in UNIT_STATUS:
        if name not in units:
            sys.exit(f"UNIT_STATUS lists {name!r}, which is not a unit in {SPLAT_CONFIG}")
        if not (Path("src") / f"{name}.cpp").exists():
            sys.exit(f"UNIT_STATUS lists {name!r} but src/{name}.cpp does not exist")

    final_ld = BUILD / f"{GAME}.final.ld"
    final_ld.write_text(write_linker_script(ld_text, units))

    out = open("build.ninja", "w")
    n = ninja_syntax.Writer(out, width=120)
    n.comment("Generated by configure.py -- do not edit")
    n.variable("ninja_required_version", "1.10")
    n.variable("binutils", str(BINUTILS))
    n.variable("asflags", ASFLAGS)
    n.variable("cflags", shlex.join(CFLAGS))
    n.variable("wibo", str(WIBO) if sys.platform != "win32" else "")
    n.variable("mwcc", str(MWCC))
    n.newline()

    n.rule(
        "as",
        "${binutils}-as $asflags -o $out $in && ${binutils}-objcopy --set-section-alignment $section=$align $out",
        description="AS $in",
    )
    n.rule(
        "bin2o",
        "${binutils}-objcopy -I binary -O elf32-littlemips -B mips:5900 "
        "--rename-section .data=.data,alloc,load,data,contents $in $out",
        description="BIN $in",
    )
    n.rule(
        "mwcc",
        f"MWCIncludes=include $wibo $mwcc $cflags -c -o $out $in"
        f" && {sys.executable} {FIX_ALIGNMENT} $out"
        f" && {sys.executable} {DROP_INLINE_COPIES} $out {SYMBOL_ADDRS} $unit_ranges",
        description="MWCC $in",
    )
    n.rule("ld_r", "${binutils}-ld -EL -r -o $out $in", description="LD -r $out")
    n.rule(
        "link",
        "${binutils}-ld -EL -T $ldscript -T config/undefined_syms_auto.txt -T config/undefined_funcs_auto.txt "
        "-Map $map --no-check-sections -o $out $objs",
        description="LINK $out",
    )
    n.rule("objcopy", "${binutils}-objcopy -O binary --gap-fill=0x00 $in $out", description="OBJCOPY $out")
    n.rule("sha1", f"{sys.executable} tools/check_sha1.py $in $expected && touch $out", description="CHECK $in")
    n.rule("report", f"{OBJDIFF} report generate -o $out", description="REPORT $out")
    n.rule("progress", f"{sys.executable} tools/progress.py $in", description="PROGRESS")
    n.rule(
        "configure",
        f"{sys.executable} configure.py --no-split",
        description="CONFIGURE",
        generator=True,
    )
    n.newline()

    link_objs = []
    objdiff_units = []
    base_objs = []
    ranges = unit_ranges(units)
    for name, objs in units.items():
        for obj in objs:
            src = source_for_object(obj)
            if src.suffix == ".bin":
                n.build(str(obj), "bin2o", str(src))
            else:
                section, align = section_and_alignment(src)
                n.build(
                    str(obj),
                    "as",
                    str(src),
                    implicit=["include/macro.inc"],
                    variables={"section": section, "align": str(align)},
                )

        target = BUILD / "target" / f"{name}.o"
        n.build(str(target), "ld_r", [str(o) for o in objs])

        status = UNIT_STATUS.get(name)
        src_cpp = Path("src") / f"{name}.cpp"
        base = None
        if src_cpp.exists():
            base = BUILD / "src" / f"{name}.o"
            n.build(
                str(base),
                "mwcc",
                str(src_cpp),
                implicit=[str(FIX_ALIGNMENT), str(DROP_INLINE_COPIES), str(SYMBOL_ADDRS)],
                variables={"unit_ranges": " ".join(f"0x{s:X}-0x{e:X}" for s, e in ranges.get(name, []))},
            )
            base_objs.append(str(base))
            status = status or NonMatching

        link_objs.extend([str(base)] if status == Matching else [str(o) for o in objs])

        unit = {"name": name, "target_path": str(target)}
        if base:
            unit["base_path"] = str(base)
        unit["metadata"] = {"auto_generated": base is None, "complete": status == Matching}
        if base:
            unit["metadata"]["source_path"] = str(src_cpp)
        objdiff_units.append(unit)

    link_objs = list(OrderedDict.fromkeys(link_objs))
    elf = BUILD / f"{GAME}.elf"
    rom = BUILD / f"{GAME}.rom"
    n.build(
        str(elf),
        "link",
        link_objs,
        implicit=[str(final_ld), "config/undefined_syms_auto.txt", "config/undefined_funcs_auto.txt"],
        variables={"ldscript": str(final_ld), "map": str(BUILD / f"{GAME}.map"), "objs": " ".join(link_objs)},
    )
    n.build(str(rom), "objcopy", str(elf))
    n.build(str(BUILD / f"{GAME}.ok"), "sha1", str(rom), variables={"expected": str(ROM)})
    n.newline()

    targets = [str(BUILD / "target" / f"{name}.o") for name in units]
    n.build(str(BUILD / "report.json"), "report", implicit=targets + base_objs + ["objdiff.json"])
    n.build("report", "phony", str(BUILD / "report.json"))
    n.build("progress", "progress", str(BUILD / "report.json"))
    n.build("build.ninja", "configure", implicit=["configure.py", str(SPLAT_LD)])
    n.newline()
    n.default(str(BUILD / f"{GAME}.ok"))
    out.close()

    objdiff = {
        "$schema": "https://raw.githubusercontent.com/encounter/objdiff/main/config.schema.json",
        "custom_make": "ninja",
        "build_target": False,
        "build_base": True,
        "watch_patterns": ["*.cpp", "*.c", "*.h", "*.hpp", "*.s", "*.inc"],
        "units": objdiff_units,
    }
    Path("objdiff.json").write_text(json.dumps(objdiff, indent=2) + "\n")
    matching = sum(1 for s in UNIT_STATUS.values() if s == Matching)
    print(f"wrote build.ninja and objdiff.json: {len(units)} units, {len(base_objs)} with source, {matching} matching")


if __name__ == "__main__":
    main()
