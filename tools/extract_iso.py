#!/usr/bin/env python3
"""Extract SLUS_204.48 from your copy of the game and verify it.

Usage: python3 tools/extract_iso.py <game image> [outdir]

Reads the image directly in Python, so no mounting or extra tools are needed.
"""
import hashlib
import struct
import sys
from pathlib import Path

SECTOR = 2048
WANTED = {
    "SLUS_204.48": "a5e367fabef46215a66eebd281dfe42e0afac91c",
    "SYSTEM.CNF": None,
}


def read_dir(image, lba, size):
    image.seek(lba * SECTOR)
    data = image.read(size)
    pos = 0
    while pos < len(data):
        rec_len = data[pos]
        if rec_len == 0:
            # records never straddle sectors; skip to the next one
            pos = (pos // SECTOR + 1) * SECTOR
            continue
        ext_lba, ext_size = struct.unpack_from("<I4xI", data, pos + 2)
        flags = data[pos + 25]
        name_len = data[pos + 32]
        name = data[pos + 33 : pos + 33 + name_len].decode("ascii", "replace")
        yield name.split(";")[0], ext_lba, ext_size, flags
        pos += rec_len


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    image_path = Path(sys.argv[1])
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("iso")
    out_dir.mkdir(parents=True, exist_ok=True)

    with image_path.open("rb") as image:
        image.seek(16 * SECTOR)
        pvd = image.read(SECTOR)
        if pvd[1:6] != b"CD001":
            sys.exit("not a supported image")
        root_lba, root_size = struct.unpack_from("<I4xI", pvd, 156 + 2)

        found = {}
        for name, lba, size, flags in read_dir(image, root_lba, root_size):
            if name in WANTED and not flags & 2:
                image.seek(lba * SECTOR)
                found[name] = image.read(size)

    ok = True
    for name, expected in WANTED.items():
        if name not in found:
            print(f"error: {name} not found in {image_path}")
            ok = False
            continue
        data = found[name]
        sha1 = hashlib.sha1(data).hexdigest()
        if expected and sha1 != expected:
            print(f"error: {name} sha1 {sha1} != expected {expected}")
            ok = False
        (out_dir / name).write_bytes(data)
        print(f"extracted {out_dir / name} ({len(data)} bytes, sha1 {sha1})")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
