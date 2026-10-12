#!/usr/bin/env python3
"""Compare a built ROM image against the original; report the first difference."""
import hashlib
import sys
from pathlib import Path

VRAM = 0x00100000  # load address of the ROM image's first byte


def main() -> None:
    built, expected = (Path(p).read_bytes() for p in sys.argv[1:3])
    if built == expected:
        print(f"OK  {sys.argv[1]}  sha1 {hashlib.sha1(built).hexdigest()}")
        return
    print(f"MISMATCH  {sys.argv[1]} ({len(built):#x} bytes) vs {sys.argv[2]} ({len(expected):#x} bytes)")
    for i, (a, b) in enumerate(zip(built, expected)):
        if a != b:
            print(f"  first difference at rom offset {i:#x} (vram {VRAM + i:#010x})")
            break
    sys.exit(1)


if __name__ == "__main__":
    main()
